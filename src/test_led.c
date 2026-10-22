#include "test_led.h"
#include "grove_rgb.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>

typedef struct
{
    const char *name;
    uint8_t r, g, b;
} LedColor;

/* Couleurs utilisees par la station (cf. README) + couleurs primaires */
static const LedColor colors[] = {
    {"rouge",  255,   0,   0},
    {"vert",     0, 255,   0},
    {"bleu",     0,   0, 255},
    {"jaune",  255, 255,   0},
    {"orange", 255,  80,   0},
    {"blanc",  255, 255, 255},
};

bool Test_LED_Run(void)
{
    printf("  Init LED RGB (CLK=PB10/D6, DATA=PA8/D7)\r\n");
    GroveRGB_Init();

    for (unsigned i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
    {
        printf("  -> %s\r\n", colors[i].name);
        GroveRGB_SetColor(colors[i].r, colors[i].g, colors[i].b);
        HAL_Delay(1000);
    }

    GroveRGB_Off();
    printf("  -> eteinte\r\n");

    /* Pas de retour possible du P9813 : la verification est visuelle. */
    printf("  Verifiez que la LED a bien affiche les 6 couleurs dans l'ordre.\r\n");
    printf("  Rien ne s'allume ? Cable sur D6, interrupteur du shield sur 5V.\r\n");
    return true;
}