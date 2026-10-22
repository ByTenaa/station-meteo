#include "test_light.h"
#include "grove_light.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>

extern ADC_HandleTypeDef hadc1; /* defini dans grove_light.c */

#define LIGHT_SAMPLES      10
#define LIGHT_PERIOD_MS    500
#define ADC_MAX            4095U

bool Test_Light_Run(void)
{
    printf("  Init capteur de luminosite (PA4/A2, ADC1 canal 9)\r\n");
    GroveLight_Init();

    /* Calibration de l'ADC (recommandee sur STM32L4 avant la premiere mesure) */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
        printf("  Attention : calibration ADC echouee\r\n");

    printf("  Mesure pendant %u s : passez la main au-dessus du capteur.\r\n",
           (LIGHT_SAMPLES * LIGHT_PERIOD_MS) / 1000);

    uint16_t min = 0xFFFF, max = 0;
    for (int i = 0; i < LIGHT_SAMPLES; i++)
    {
        uint16_t raw = GroveLight_ReadRaw();
        if (raw == 0xFFFF)
        {
            printf("  ECHEC : erreur de conversion ADC\r\n");
            return false;
        }
        if (raw < min) min = raw;
        if (raw > max) max = raw;

        printf("  [%2d] brut = %4u  (%3u %%)\r\n", i + 1, raw, (unsigned)(raw * 100U / ADC_MAX));
        HAL_Delay(LIGHT_PERIOD_MS);
    }

    printf("  min = %u, max = %u, variation = %u\r\n", min, max, max - min);

    if (max <= 5)
    {
        printf("  ECHEC : valeur toujours ~0 -> capteur non branche ou mauvais port (doit etre A2).\r\n");
        return false;
    }
    if (min >= ADC_MAX - 5)
    {
        printf("  ECHEC : valeur bloquee au maximum -> verifier le cablage.\r\n");
        return false;
    }
    if (max - min < 50)
        printf("  Remarque : tres peu de variation. Masquez/eclairez le capteur pour verifier qu'il reagit.\r\n");

    return true;
}
