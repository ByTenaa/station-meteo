#include "test_gps.h"
#include "grove_gps_air530z.h"
#include "tests_config.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef GPS_UART_HANDLE; /* defini dans grove_gps_air530z.c */

#define GPS_CAPTURE_LINES 6
#define GPS_LINE_LEN      96

/* Verifie la somme de controle NMEA : $....*HH */
static bool nmea_checksum_ok(const char *line)
{
    if (line[0] != '$') return false;

    const char *star = strchr(line, '*');
    if (star == NULL || strlen(star) < 3) return false;

    uint8_t sum = 0;
    for (const char *p = line + 1; p < star; p++)
        sum ^= (uint8_t)*p;

    unsigned expected = 0;
    if (sscanf(star + 1, "%2x", &expected) != 1) return false;
    return sum == (uint8_t)expected;
}

/* Affiche un double sans printf flottant (non supporte par newlib-nano par defaut) */
static void print_coord(const char *label, double v)
{
    long micro = (long)(v * 1000000.0);
    const char *sign = (micro < 0) ? "-" : "";
    if (micro < 0) micro = -micro;
    printf("  %s %s%ld.%06ld\r\n", label, sign, micro / 1000000L, micro % 1000000L);
}

bool Test_GPS_Run(void)
{
    static char lines[GPS_CAPTURE_LINES][GPS_LINE_LEN];

    printf("  Init GPS (UART4, 9600 bauds, port A0)\r\n");
    GroveGPS_Init();
    __HAL_UART_CLEAR_OREFLAG(&GPS_UART_HANDLE);

    /*
     * 1) Capture de quelques trames brutes.
     * On stocke d'abord puis on affiche ensuite : afficher pendant la reception
     * ferait perdre des caracteres (UART en polling, sans buffer).
     */
    int n = 0;
    uint32_t start = HAL_GetTick();
    while (n < GPS_CAPTURE_LINES && (HAL_GetTick() - start) < 3000)
    {
        if (GroveGPS_ReadLine(lines[n], GPS_LINE_LEN, 1500) && lines[n][0] == '$')
            n++;
    }

    if (n == 0)
    {
        printf("  ECHEC : aucune trame NMEA recue en 3 s.\r\n");
        printf("  Verifiez : GPS sur le port A0, alimentation, sens TX/RX (swap UART4).\r\n");
        return false;
    }

    int valid = 0;
    printf("  %d trames recues :\r\n", n);
    for (int i = 0; i < n; i++)
    {
        bool ok = nmea_checksum_ok(lines[i]);
        if (ok) valid++;
        printf("    %s %s\r\n", ok ? "[ok]" : "[??]", lines[i]);
    }
    if (valid == 0)
    {
        printf("  ECHEC : trames illisibles (checksum faux) -> mauvais debit ou parasites.\r\n");
        return false;
    }

    /* 2) Lecture d'une trame GGA (position) */
    GPS_Data gps;
    memset(&gps, 0, sizeof(gps));
    __HAL_UART_CLEAR_OREFLAG(&GPS_UART_HANDLE);

    printf("  Attente d'une trame GGA (%u s max)...\r\n", GPS_TEST_TIMEOUT_MS / 1000);
    if (!GroveGPS_ReadGGA(&gps, GPS_TEST_TIMEOUT_MS))
    {
        printf("  Pas de trame GGA recue, mais le module communique. OK\r\n");
        return true;
    }

    printf("  GGA : %s\r\n", gps.raw_sentence);
    printf("  Qualite du fix : %u, satellites : %u\r\n", gps.fix_quality, gps.satellites);

    if (gps.valid)
    {
        print_coord("Latitude  :", gps.latitude);
        print_coord("Longitude :", gps.longitude);
    }
    else
    {
        printf("  Pas encore de position (normal en interieur : placez l'antenne\r\n");
        printf("  pres d'une fenetre, le premier fix peut prendre plusieurs minutes).\r\n");
    }

    /* Le module communique correctement : test reussi, meme sans fix */
    return true;
}