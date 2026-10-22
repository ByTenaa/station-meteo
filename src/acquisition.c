/*
 * Acquisition des capteurs et enregistrement dans un fichier CSV unique
 * sur la carte SD. Chaque appel a Acquisition_Run() ajoute une ligne.
 *
 * Colonnes :
 *   numero ; temps_s ; date ; heure ; source_heure ;
 *   temperature_C ; humidite_pct ; pression_hPa ; gaz_ohm ;
 *   luminosite_brut ; latitude ; longitude ; satellites
 *
 * Une valeur indisponible (capteur absent, pas de fix GPS...) laisse la case vide.
 */
#include "acquisition.h"
#include "acquisition_config.h"
#include "stm32l4xx_hal.h"
#include "cesi_types.h"

#include "grove_bme680.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "ff.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern ADC_HandleTypeDef hadc1;               /* grove_light.c       */
extern UART_HandleTypeDef GPS_UART_HANDLE;     /* grove_gps_air530z.c */
void disk_force_reinit(void);                  /* lib/fatfs/diskio.c  */

/* ------------------------------------------------------------------ */
/* Etat                                                                */
/* ------------------------------------------------------------------ */

static FATFS g_fs;
static bool g_mounted = false;
static bool g_bme_ok = false;
static bool g_rtc_ok = false;
static bool g_rtc_set_by_gps = false;
static uint32_t g_numero = 0;
static uint32_t g_start_tick = 0;

typedef struct
{
    bool time_valid;      /* trame RMC valide : date + heure UTC */
    RTC_DateTime dt;
    bool pos_valid;
    double lat, lon;
    int satellites;       /* -1 = inconnu */
} GpsInfo;

/* ------------------------------------------------------------------ */
/* Formatage (printf flottant non disponible avec newlib-nano)          */
/* ------------------------------------------------------------------ */

/* Ecrit v avec 'dec' decimales et le separateur decimal configure */
static void fmt_fixed(char *out, size_t size, double v, int dec)
{
    long mult = 1;
    for (int i = 0; i < dec; i++) mult *= 10;

    long long x = (long long)(v * mult + (v >= 0 ? 0.5 : -0.5));
    const char *sign = "";
    if (x < 0) { sign = "-"; x = -x; }

    if (dec == 0)
        snprintf(out, size, "%s%lu", sign, (unsigned long)x);
    else
        snprintf(out, size, "%s%lu%c%0*lu", sign, (unsigned long)(x / mult),
                 ACQ_CSV_DECIMAL, dec, (unsigned long)(x % mult));
}

/* ------------------------------------------------------------------ */
/* GPS : lecture des trames RMC (date/heure/position) et GGA (satellites) */
/* ------------------------------------------------------------------ */

static bool nmea_checksum_ok(const char *line)
{
    const char *star = strchr(line, '*');
    if (line[0] != '$' || star == NULL || strlen(star) < 3) return false;

    uint8_t sum = 0;
    for (const char *p = line + 1; p < star; p++) sum ^= (uint8_t)*p;
    return sum == (uint8_t)strtoul(star + 1, NULL, 16);
}

/* Decoupe une trame en champs (les champs vides sont conserves) */
static int nmea_split(char *line, char *fields[], int max)
{
    int n = 0;
    fields[n++] = line;
    for (char *p = line; *p && n < max; p++)
    {
        if (*p == ',' || *p == '*')
        {
            *p = '\0';
            fields[n++] = p + 1;
        }
    }
    return n;
}

static double nmea_coord(const char *v, char hemi)
{
    if (v == NULL || v[0] == '\0') return 0.0;
    double raw = atof(v);
    int deg = (int)(raw / 100.0);
    double d = deg + (raw - deg * 100.0) / 60.0;
    return (hemi == 'S' || hemi == 'W') ? -d : d;
}

static int two_digits(const char *s) { return (s[0] - '0') * 10 + (s[1] - '0'); }

static void gps_read(GpsInfo *g)
{
    char line[96];
    char *f[20];
    bool got_rmc = false, got_gga = false;

    memset(g, 0, sizeof(*g));
    g->satellites = -1;

    __HAL_UART_CLEAR_OREFLAG(&GPS_UART_HANDLE);
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < ACQ_GPS_LISTEN_MS && !(got_rmc && got_gga))
    {
        uint32_t left = ACQ_GPS_LISTEN_MS - (HAL_GetTick() - start);
        if (!GroveGPS_ReadLine(line, sizeof(line), left)) continue;
        if (!nmea_checksum_ok(line)) continue;

        int n = nmea_split(line, f, 20);

        /* $xxRMC,hhmmss.ss,A,lat,N,lon,E,vitesse,cap,ddmmyy,... */
        if (n >= 10 && strlen(f[0]) == 6 && strcmp(f[0] + 3, "RMC") == 0)
        {
            got_rmc = true;
            if (f[2][0] == 'A' && strlen(f[1]) >= 6 && strlen(f[9]) == 6)
            {
                g->time_valid   = true;
                g->dt.hours     = (uint8_t)two_digits(f[1]);
                g->dt.minutes   = (uint8_t)two_digits(f[1] + 2);
                g->dt.seconds   = (uint8_t)two_digits(f[1] + 4);
                g->dt.day       = (uint8_t)two_digits(f[9]);
                g->dt.month     = (uint8_t)two_digits(f[9] + 2);
                g->dt.year      = (uint16_t)(2000 + two_digits(f[9] + 4));
                g->dt.dayOfWeek = 1;

                g->pos_valid = true;
                g->lat = nmea_coord(f[3], f[4][0]);
                g->lon = nmea_coord(f[5], f[6][0]);
            }
        }
        /* $xxGGA,hhmmss.ss,lat,N,lon,E,fix,sats,... */
        else if (n >= 8 && strlen(f[0]) == 6 && strcmp(f[0] + 3, "GGA") == 0)
        {
            got_gga = true;
            if (f[7][0] != '\0') g->satellites = atoi(f[7]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Carte SD                                                            */
/* ------------------------------------------------------------------ */

static void sd_unmount(void)
{
    f_mount(NULL, "", 0);
    g_mounted = false;
    disk_force_reinit();
}

static bool sd_append_line(const char *line)
{
    static const char header_fmt[] =
        "numero%ctemps_s%cdate%cheure%csource_heure%ctemperature_C%chumidite_pct%c"
        "pression_hPa%cgaz_ohm%cluminosite_brut%clatitude%clongitude%csatellites\r\n";
    FIL fil;
    UINT written;
    FRESULT fr;

    if (!g_mounted)
    {
        fr = f_mount(&g_fs, "", 1);
        if (fr != FR_OK)
        {
            printf("[SD] montage impossible (erreur FatFs %d)\r\n", fr);
            sd_unmount();
            return false;
        }
        g_mounted = true;
        printf("[SD] carte montee\r\n");
    }

    fr = f_open(&fil, ACQ_CSV_FILENAME, FA_WRITE | FA_OPEN_APPEND);
    if (fr != FR_OK)
    {
        printf("[SD] ouverture de %s impossible (erreur %d)\r\n", ACQ_CSV_FILENAME, fr);
        sd_unmount();
        return false;
    }

    /* Fichier neuf : on ecrit d'abord la ligne d'en-tete */
    if (f_size(&fil) == 0)
    {
        char header[200];
        const char s = ACQ_CSV_SEPARATOR;
        int len = snprintf(header, sizeof(header), header_fmt, s, s, s, s, s, s, s, s, s, s, s, s);
        fr = f_write(&fil, header, (UINT)len, &written);
    }

    if (fr == FR_OK)
        fr = f_write(&fil, line, (UINT)strlen(line), &written);

    /* Fermeture a chaque ligne : les donnees sont sur la carte meme si on la retire */
    FRESULT fr_close = f_close(&fil);

    if (fr != FR_OK || fr_close != FR_OK)
    {
        printf("[SD] erreur d'ecriture (%d / %d)\r\n", fr, fr_close);
        sd_unmount();
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

void Acquisition_Init(void)
{
    g_start_tick = HAL_GetTick();

    GroveLight_Init();
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    printf("  Luminosite : ok\r\n");

    g_bme_ok = GroveBME680_Init();
    printf("  BME680     : %s\r\n", g_bme_ok ? "ok" : "ABSENT");

    g_rtc_ok = GroveRTC_Init();
    printf("  RTC        : %s\r\n", g_rtc_ok ? "ok" : "ABSENTE (heure prise sur le GPS)");

        GroveGPS_Init();
    printf("  GPS        : initialise\r\n");

    /* Montage de la carte des le demarrage pour signaler un probleme tot */
    if (!g_mounted && f_mount(&g_fs, "", 1) == FR_OK)
    {
        g_mounted = true;
        printf("  Carte SD   : ok -> fichier %s\r\n", ACQ_CSV_FILENAME);
    }
    else if (!g_mounted)
    {
        sd_unmount();
        printf("  Carte SD   : ERREUR (nouvel essai a chaque mesure)\r\n");
    }
}

bool Acquisition_Run(void)
{
    char sep = ACQ_CSV_SEPARATOR;
    char temp[16] = "", hum[16] = "", press[16] = "", gas[16] = "";
    char lum[8] = "", lat[16] = "", lon[16] = "", sats[12] = "";
    char date[16] = "", heure[16] = "";
    const char *source = "aucune";

    g_numero++;
    uint32_t t_s = (HAL_GetTick() - g_start_tick) / 1000U;

    /* --- BME680 --- */
    Env_Data env;
    if (!g_bme_ok) g_bme_ok = GroveBME680_Init(); /* capteur rebranche ? */
    if (g_bme_ok && GroveBME680_ReadEnvironment(&env))
    {
        fmt_fixed(temp,  sizeof(temp),  env.temperature_c, 2);
        fmt_fixed(hum,   sizeof(hum),   env.humidity_percent, 1);
        fmt_fixed(press, sizeof(press), env.pressure_hpa, 2);
        if (env.gas_resistance_ohm >= 0.0f)
            fmt_fixed(gas, sizeof(gas), env.gas_resistance_ohm, 0);
    }
    else
    {
        g_bme_ok = false;
    }

    /* --- Luminosite --- */
    uint16_t raw = GroveLight_ReadRaw();
    if (raw != 0xFFFF) snprintf(lum, sizeof(lum), "%u", raw);

    /* --- GPS --- */
    GpsInfo gps;
    gps_read(&gps);
    if (gps.pos_valid)
    {
        fmt_fixed(lat, sizeof(lat), gps.lat, 6);
        fmt_fixed(lon, sizeof(lon), gps.lon, 6);
    }
    if (gps.satellites >= 0) snprintf(sats, sizeof(sats), "%d", gps.satellites);

    /* --- Date / heure : RTC en priorite, sinon GPS (UTC) --- */
    if (!g_rtc_ok) g_rtc_ok = GroveRTC_Init();

#if ACQ_SET_RTC_FROM_GPS
    if (g_rtc_ok && gps.time_valid && !g_rtc_set_by_gps)
    {
        if (GroveRTC_SetDateTime(&gps.dt))
        {
            g_rtc_set_by_gps = true;
            printf("[RTC] mise a l'heure depuis le GPS (UTC)\r\n");
        }
    }
#endif

    RTC_DateTime dt;
    if (g_rtc_ok && GroveRTC_GetDateTime(&dt) && dt.month >= 1 && dt.month <= 12)
        source = "RTC";
    else if (gps.time_valid)
    {
        dt = gps.dt;
        source = "GPS";
    }

    if (strcmp(source, "aucune") != 0)
    {
        snprintf(date, sizeof(date), "%02u/%02u/%04u", dt.day, dt.month, dt.year);
        snprintf(heure, sizeof(heure), "%02u:%02u:%02u", dt.hours, dt.minutes, dt.seconds);
    }

    /* --- Ligne CSV --- */
    char line[200];
    snprintf(line, sizeof(line),
             "%lu%c%lu%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s\r\n",
             (unsigned long)g_numero, sep, (unsigned long)t_s, sep,
             date, sep, heure, sep, source, sep,
             temp, sep, hum, sep, press, sep, gas, sep,
             lum, sep, lat, sep, lon, sep, sats);

    printf("%s", line); /* copie sur le moniteur serie */

    return sd_append_line(line);
}
