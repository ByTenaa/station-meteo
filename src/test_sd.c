#include "test_sd.h"
#include "sd_logger.h"
#include <stdio.h>

static const char *status_to_str(SDLoggerInitStatus s)
{
    switch (s)
    {
        case SDLOGGER_INIT_OK:                 return "OK";
        case SDLOGGER_INIT_ERR_CMD0:           return "pas de reponse a CMD0 (carte absente / cablage SPI)";
        case SDLOGGER_INIT_ERR_CMD8_PATTERN:   return "reponse CMD8 incorrecte";
        case SDLOGGER_INIT_ERR_ACMD41_TIMEOUT: return "timeout ACMD41 (carte ne sort pas de l'etat idle)";
        case SDLOGGER_INIT_ERR_CMD58:          return "erreur CMD58 (lecture OCR)";
        case SDLOGGER_INIT_ERR_CMD1_TIMEOUT:   return "timeout CMD1 (ancienne carte MMC ?)";
        default:                               return "inconnu";
    }
}

static const char *type_to_str(SDLoggerCardType t)
{
    switch (t)
    {
        case SDLOGGER_CARD_SDSC: return "SDSC (<= 2 Go)";
        case SDLOGGER_CARD_SDHC: return "SDHC/SDXC (> 2 Go)";
        default:                 return "inconnu";
    }
}

bool Test_SD_Run(void)
{
    printf("  Init carte SD (SPI1 : SCK=PA5, MISO=PA6, MOSI=PA7, CS=PB5/D4)\r\n");

    bool ok = SDLogger_Init();
    printf("  Statut : %s\r\n", status_to_str(SDLogger_GetLastInitStatus()));

    if (!ok)
    {
        /* Diagnostic : refait CMD0 en bit-bang pour separer probleme SPI / probleme physique */
        printf("  Diagnostic bit-bang :\r\n");
        SDLogger_BitBangCMD0Test();
        printf("  ECHEC : carte bien inseree ? shield bien enfiche ?\r\n");
        return false;
    }

    printf("  Carte detectee sur %s, type %s. OK\r\n",
           SDLogger_GetActiveCsLabel(), type_to_str(SDLogger_GetCardType()));

    /* SDLogger_WriteLine() n'est pas encore implemente (il faut FatFs) */
    printf("  Remarque : l'ecriture de fichiers n'est pas encore disponible (FatFs a ajouter).\r\n");
    return true;
}
