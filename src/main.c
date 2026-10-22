/*
 * Programme de test des capteurs de la station meteo.
 *
 * Lance les tests un par un, affiche le detail sur le moniteur serie
 * (115200 bauds) puis un recapitulatif. Activer/desactiver chaque test
 * dans include/tests_config.h.
 */
#include <stdio.h>
#include "stm32l4xx_hal.h"
#include "setup.h"
#include "tests_config.h"
#include "grove_rgb.h"

#include "test_led.h"
#include "test_button.h"
#include "test_light.h"
#include "test_bme.h"
#include "test_rtc.h"
#include "test_gps.h"
#include "test_sd.h"

typedef struct
{
    const char *name;
    bool (*run)(void);
} TestEntry;

static const TestEntry tests[] = {
#if TEST_LED_ENABLE
    {"LED RGB",           Test_LED_Run},
#endif
#if TEST_BUTTON_ENABLE
    {"Boutons poussoirs", Test_Button_Run},
#endif
#if TEST_LIGHT_ENABLE
    {"Capteur lumiere",   Test_Light_Run},
#endif
#if TEST_BME680_ENABLE
    {"BME680",            Test_BME680_Run},
#endif
#if TEST_RTC_ENABLE
    {"Horloge RTC",       Test_RTC_Run},
#endif
#if TEST_GPS_ENABLE
    {"GPS",               Test_GPS_Run},
#endif
#if TEST_SD_ENABLE
    {"Carte SD",          Test_SD_Run},
#endif
};

#define NB_TESTS (sizeof(tests) / sizeof(tests[0]))

int main(void)
{
    Global_Init();
    setvbuf(stdout, NULL, _IONBF, 0); /* affichage immediat, sans tampon */
    HAL_Delay(500);

    printf("\r\n=========================================\r\n");
    printf("  STATION METEO - TEST DES CAPTEURS\r\n");
    printf("=========================================\r\n");

    bool results[NB_TESTS > 0 ? NB_TESTS : 1];
    unsigned passed = 0;

    for (unsigned i = 0; i < NB_TESTS; i++)
    {
        printf("\r\n----- [%u/%u] %s -----\r\n", i + 1, (unsigned)NB_TESTS, tests[i].name);
        results[i] = tests[i].run();
        printf("=> %s : %s\r\n", tests[i].name, results[i] ? "REUSSI" : "ECHEC");
        if (results[i]) passed++;
        HAL_Delay(TEST_PAUSE_MS);
    }

    printf("\r\n=========== RECAPITULATIF ===========\r\n");
    for (unsigned i = 0; i < NB_TESTS; i++)
        printf("  %-18s %s\r\n", tests[i].name, results[i] ? "OK" : "ECHEC");
    printf("  %u/%u tests reussis\r\n", passed, (unsigned)NB_TESTS);
    printf("=====================================\r\n");

    /* Etat final sur la LED RGB : vert = tout est OK, rouge = au moins un echec */
    GroveRGB_Init();
    if (passed == NB_TESTS)
        GroveRGB_SetColor(0, 255, 0);
    else
        GroveRGB_SetColor(255, 0, 0);

    while (1)
    {
        HAL_Delay(1000);
    }
}
