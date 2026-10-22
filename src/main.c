/*
 * Station meteo - 4 modes de fonctionnement pilotes par les boutons
 *
 *   STANDARD      : demarrage normal, mesure + ecriture SD             LED verte
 *   CONFIGURATION : demarrage avec le bouton ROUGE appuye, pas de mesure LED jaune
 *                   (retour en standard apres 30 min, ou ROUGE 5 s)
 *   ECONOMIQUE    : VERT 5 s depuis standard, mesures 2x moins souvent  LED bleue
 *                   (ROUGE 5 s -> retour standard)
 *   MAINTENANCE   : ROUGE 5 s depuis standard, pas d'ecriture SD,       LED orange
 *                   carte SD retirable (ROUGE 5 s -> retour standard)
 */
#include <stdio.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"
#include "setup.h"
#include "board_pins.h"
#include "grove_rgb.h"
#include "acquisition.h"
#include "acquisition_config.h"

/* ---------- Reglages ---------- */

/* Quel bouton est le ROUGE ? 1 = bouton 1 (D2/PA10), 2 = bouton 2 (D3/PB3) */
#define BOUTON_ROUGE          1

#define APPUI_LONG_MS         5000UL
#define CONFIG_TIMEOUT_MS     (30UL * 60UL * 1000UL)

/* ---------- Modes ---------- */

typedef enum { MODE_STANDARD, MODE_CONFIG, MODE_ECO, MODE_MAINTENANCE } Mode;
static const char *NOM_MODE[] = {"STANDARD", "CONFIGURATION", "ECONOMIQUE", "MAINTENANCE"};

static Mode mode = MODE_STANDARD;
static uint32_t debut_mode = 0;
static uint32_t prochaine_mesure = 0;

/* ---------- Boutons : appui long de 5 s ---------- */

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t debut_appui;
    bool appuye;
    bool deja_declenche;
} Bouton;

#if BOUTON_ROUGE == 1
static Bouton rouge = {BUTTON1_PORT, BUTTON1_PIN, 0, false, false};
static Bouton vert  = {BUTTON2_PORT, BUTTON2_PIN, 0, false, false};
#else
static Bouton rouge = {BUTTON2_PORT, BUTTON2_PIN, 0, false, false};
static Bouton vert  = {BUTTON1_PORT, BUTTON1_PIN, 0, false, false};
#endif

static bool bouton_lu(const Bouton *b)
{
    /* appui = broche reliee a la masse (pull-up) */
    return HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_RESET;
}

static void boutons_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;

    g.Pin = rouge.pin;
    HAL_GPIO_Init(rouge.port, &g);
    g.Pin = vert.pin;
    HAL_GPIO_Init(vert.port, &g);
    HAL_Delay(10);

    /* un bouton deja enfonce au demarrage ne compte pas comme appui long */
    rouge.appuye = rouge.deja_declenche = bouton_lu(&rouge);
    vert.appuye  = vert.deja_declenche  = bouton_lu(&vert);
}

/* Renvoie true une seule fois quand le bouton est maintenu APPUI_LONG_MS */
static bool appui_long(Bouton *b)
{
    bool lu = bouton_lu(b);

    if (lu && !b->appuye)                 /* debut d'appui */
    {
        b->debut_appui = HAL_GetTick();
        b->deja_declenche = false;
    }
    b->appuye = lu;

    if (lu && !b->deja_declenche && (HAL_GetTick() - b->debut_appui) >= APPUI_LONG_MS)
    {
        b->deja_declenche = true;
        return true;
    }
    return false;
}

/* ---------- LED ---------- */

static void led_mode(void)
{
    switch (mode)
    {
        case MODE_STANDARD:    GroveRGB_SetColor(0, 255, 0);   break; /* vert   */
        case MODE_CONFIG:      GroveRGB_SetColor(255, 200, 0); break; /* jaune  */
        case MODE_ECO:         GroveRGB_SetColor(0, 0, 255);   break; /* bleu   */
        case MODE_MAINTENANCE: GroveRGB_SetColor(255, 60, 0);  break; /* orange */
    }
}

/* ---------- Changement de mode ---------- */

static uint32_t intervalle_ms(void)
{
    return (mode == MODE_ECO) ? ACQ_PERIOD_MS * 2UL : ACQ_PERIOD_MS;
}

static void changer_mode(Mode nouveau)
{
    mode = nouveau;
    debut_mode = HAL_GetTick();
    prochaine_mesure = HAL_GetTick();     /* premiere mesure tout de suite */
    led_mode();

    printf("\r\n>>> Mode %s\r\n", NOM_MODE[mode]);
    switch (mode)
    {
        case MODE_CONFIG:
            printf("Acquisition desactivee. Retour en standard apres 30 min (ou ROUGE 5 s).\r\n");
            break;
        case MODE_ECO:
            printf("Une mesure toutes les %lu s.\r\n", (unsigned long)(intervalle_ms() / 1000UL));
            break;
        case MODE_MAINTENANCE:
            printf("Ecriture SD arretee : la carte peut etre retiree.\r\n");
            break;
        default:
            printf("Une mesure toutes les %lu s -> %s\r\n",
                   (unsigned long)(intervalle_ms() / 1000UL), ACQ_CSV_FILENAME);
            break;
    }
}

static void gerer_boutons(void)
{
    bool r = appui_long(&rouge);
    bool v = appui_long(&vert);

    switch (mode)
    {
        case MODE_STANDARD:
            if (r)      changer_mode(MODE_MAINTENANCE);
            else if (v) changer_mode(MODE_ECO);
            break;
        case MODE_ECO:
        case MODE_MAINTENANCE:
        case MODE_CONFIG:
            if (r)      changer_mode(MODE_STANDARD);
            break;
    }
}

/* ---------- Programme principal ---------- */

int main(void)
{
    Global_Init();
    setvbuf(stdout, NULL, _IONBF, 0);
    HAL_Delay(200);

    GroveRGB_Init();
    boutons_init();

    printf("\r\n=== STATION METEO ===\r\n");

    bool demarrage_config = bouton_lu(&rouge);   /* ROUGE appuye a l'allumage ? */

    Acquisition_Init();
    changer_mode(demarrage_config ? MODE_CONFIG : MODE_STANDARD);

    while (1)
    {
        gerer_boutons();

        switch (mode)
        {
            case MODE_STANDARD:
            case MODE_ECO:
                if ((int32_t)(HAL_GetTick() - prochaine_mesure) >= 0)
                {
                    prochaine_mesure += intervalle_ms();
                    if (!Acquisition_Run())
                    {
                        GroveRGB_SetColor(255, 0, 0);   /* rouge : erreur SD */
                        HAL_Delay(500);
                    }
                    led_mode();
                }
                break;

            case MODE_MAINTENANCE:
                /* pas d'ecriture sur la carte SD : elle peut etre retiree */
                break;

            case MODE_CONFIG:
                if (HAL_GetTick() - debut_mode >= CONFIG_TIMEOUT_MS)
                    changer_mode(MODE_STANDARD);
                break;
        }

        HAL_Delay(10);
    }
}