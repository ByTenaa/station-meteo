#include "test_button.h"
#include "grove_button.h"
#include "board_pins.h"
#include "tests_config.h"
#include <stdio.h>

/*
 * On lit directement le niveau de la broche plutot que GroveButtonX_IsPressed() :
 * le test fonctionne ainsi quelle que soit la logique du bouton (actif haut ou bas)
 * et indique laquelle est la bonne.
 */
static GPIO_PinState read_stable(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(port, pin);
    HAL_Delay(20); /* anti-rebond */
    while (HAL_GPIO_ReadPin(port, pin) != s)
    {
        s = HAL_GPIO_ReadPin(port, pin);
        HAL_Delay(20);
    }
    return s;
}

static bool test_one_button(const char *name, GPIO_TypeDef *port, uint16_t pin,
                            bool (*driver_is_pressed)(void))
{
    GPIO_PinState idle = read_stable(port, pin);
    printf("  %s : niveau au repos = %s\r\n", name, idle == GPIO_PIN_SET ? "HAUT" : "BAS");
    printf("  Appuyez sur %s (%u s max)...\r\n", name, BUTTON_TEST_TIMEOUT_MS / 1000);

    uint32_t start = HAL_GetTick();
    while (read_stable(port, pin) == idle)
    {
        if (HAL_GetTick() - start > BUTTON_TEST_TIMEOUT_MS)
        {
            printf("  ECHEC : aucun appui detecte sur %s.\r\n", name);
            return false;
        }
    }

    GPIO_PinState pressed = (idle == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    bool drv = driver_is_pressed();
    printf("  Appui detecte ! niveau appuye = %s\r\n", pressed == GPIO_PIN_SET ? "HAUT" : "BAS");
    printf("  %sIsPressed() du driver renvoie %s pendant l'appui%s\r\n",
           name[7] == '1' ? "GroveButton1_" : "GroveButton2_",
           drv ? "true" : "false",
           drv ? " (OK)" : " -> logique INVERSEE dans grove_button.c !");

    printf("  Relachez %s...\r\n", name);
    start = HAL_GetTick();
    while (read_stable(port, pin) != idle)
    {
        if (HAL_GetTick() - start > BUTTON_TEST_TIMEOUT_MS)
        {
            printf("  ECHEC : %s reste appuye.\r\n", name);
            return false;
        }
    }
    printf("  %s relache. OK\r\n", name);
    return true;
}

bool Test_Button_Run(void)
{
    printf("  Init boutons (B1=PA10/D2, B2=PB3/D3)\r\n");
    GroveButton_Init();

    bool ok1 = test_one_button("bouton 1", BUTTON1_PORT, BUTTON1_PIN, GroveButton1_IsPressed);
    bool ok2 = test_one_button("bouton 2", BUTTON2_PORT, BUTTON2_PIN, GroveButton2_IsPressed);

    if (!ok1 || !ok2)
        printf("  Verifiez que le double bouton est sur le port D2 du shield.\r\n");

    return ok1 && ok2;
}