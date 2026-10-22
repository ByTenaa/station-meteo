#include "test_rtc.h"
#include "grove_rtc_ds1307.h"
#include "board_pins.h"
#include "tests_config.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef GROVE_I2C_HANDLE; /* defini dans setup.c */

#define DS1307_I2C_ADDR 0x68

/* Liste tous les peripheriques presents sur le bus I2C (utile pour le diagnostic) */
static bool i2c_scan(void)
{
    bool rtc_found = false;
    int count = 0;

    printf("  Scan du bus I2C (PB8=SCL, PB9=SDA) :");
    for (uint8_t addr = 0x08; addr < 0x78; addr++)
    {
        if (HAL_I2C_IsDeviceReady(&GROVE_I2C_HANDLE, (uint16_t)(addr << 1), 1, 5) == HAL_OK)
        {
            printf(" 0x%02X", addr);
            if (addr == DS1307_I2C_ADDR) printf("(RTC)");
            if (addr == 0x76 || addr == 0x77) printf("(BME680?)");
            if (addr == DS1307_I2C_ADDR) rtc_found = true;
            count++;
        }
    }
    if (count == 0) printf(" aucun peripherique");
    printf("\r\n");
    return rtc_found;
}

static void print_datetime(const char *label, const RTC_DateTime *dt)
{
    printf("  %s %02u/%02u/%04u %02u:%02u:%02u\r\n", label,
           (unsigned)dt->day, (unsigned)dt->month, (unsigned)dt->year,
           (unsigned)dt->hours, (unsigned)dt->minutes, (unsigned)dt->seconds);
}

static bool datetime_is_valid(const RTC_DateTime *dt)
{
    return dt->seconds < 60 && dt->minutes < 60 && dt->hours < 24 &&
           dt->day >= 1 && dt->day <= 31 && dt->month >= 1 && dt->month <= 12 &&
           dt->year >= 2000 && dt->year <= 2099;
}

#if RTC_SET_TIME_FROM_BUILD
/* Construit une date a partir de __DATE__ ("Oct 19 2026") et __TIME__ ("10:44:00") */
static void datetime_from_build(RTC_DateTime *dt)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *d = __DATE__;
    const char *t = __TIME__;

    dt->month = 1;
    for (int i = 0; i < 12; i++)
        if (strncmp(&months[i * 3], d, 3) == 0) dt->month = (uint8_t)(i + 1);

    dt->day     = (uint8_t)(((d[4] == ' ') ? 0 : (d[4] - '0')) * 10 + (d[5] - '0'));
    dt->year    = (uint16_t)((d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 + (d[10] - '0'));
    dt->hours   = (uint8_t)((t[0] - '0') * 10 + (t[1] - '0'));
    dt->minutes = (uint8_t)((t[3] - '0') * 10 + (t[4] - '0'));
    dt->seconds = (uint8_t)((t[6] - '0') * 10 + (t[7] - '0'));

    /* Jour de la semaine (algorithme de Sakamoto) : 1 = lundi ... 7 = dimanche */
    static const int k[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = dt->year - (dt->month < 3);
    int w = (y + y / 4 - y / 100 + y / 400 + k[dt->month - 1] + dt->day) % 7; /* 0 = dimanche */
    dt->dayOfWeek = (uint8_t)(w == 0 ? 7 : w);
}
#endif

bool Test_RTC_Run(void)
{
    bool found = i2c_scan();

    if (!GroveRTC_Init() || !found)
    {
        printf("  ECHEC : la RTC ne repond pas a l'adresse 0x68.\r\n");
        printf("  Verifiez : module sur un port I2C, interrupteur du shield sur 5V.\r\n");
        return false;
    }
    printf("  RTC detectee.\r\n");

#if RTC_SET_TIME_FROM_BUILD
    RTC_DateTime set;
    datetime_from_build(&set);
    if (GroveRTC_SetDateTime(&set))
        print_datetime("Heure reglee a :", &set);
    else
        printf("  Attention : ecriture de l'heure echouee\r\n");
#endif

    RTC_DateTime t1, t2;
    if (!GroveRTC_GetDateTime(&t1))
    {
        printf("  ECHEC : lecture I2C de la date impossible\r\n");
        return false;
    }
    print_datetime("Lecture 1 :", &t1);

    HAL_Delay(2100);

    if (!GroveRTC_GetDateTime(&t2))
    {
        printf("  ECHEC : 2e lecture I2C impossible\r\n");
        return false;
    }
    print_datetime("Lecture 2 :", &t2);

    /* L'horloge doit avoir avance d'environ 2 secondes */
    int s1 = t1.hours * 3600 + t1.minutes * 60 + t1.seconds;
    int s2 = t2.hours * 3600 + t2.minutes * 60 + t2.seconds;
    int diff = (s2 - s1 + 86400) % 86400;

    if (diff == 0)
    {
        printf("  ECHEC : l'horloge ne tourne pas (oscillateur arrete / pas de pile).\r\n");
        printf("  -> Mettez RTC_SET_TIME_FROM_BUILD a 1 dans tests_config.h, flashez une fois, puis remettez a 0.\r\n");
        return false;
    }
    if (diff < 1 || diff > 4)
        printf("  Attention : ecart de %d s au lieu de ~2 s\r\n", diff);
    else
        printf("  L'horloge tourne (+%d s). OK\r\n", diff);

    if (!datetime_is_valid(&t2))
        printf("  Attention : date incoherente -> reglez l'heure (RTC_SET_TIME_FROM_BUILD).\r\n");

    return true;
}
