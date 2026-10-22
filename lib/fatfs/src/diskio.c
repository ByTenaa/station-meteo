/*
 * Liaison entre FatFs et la carte SD (driver sd_logger, SPI1).
 * Un seul volume : le lecteur 0 = la carte SD.
 */
#include "ff.h"
#include "diskio.h"
#include "sd_logger.h"

static DSTATUS g_status = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return g_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;

    if (SDLogger_Init())
    {
        SDLogger_SetFastClock();
        g_status = 0;
    }
    else
    {
        g_status = STA_NOINIT;
    }
    return g_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (g_status & STA_NOINIT) return RES_NOTRDY;

    for (UINT i = 0; i < count; i++)
    {
        if (!SDLogger_ReadBlock((uint32_t)sector + i, buff + (i * 512U)))
            return RES_ERROR;
    }
    return RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (g_status & STA_NOINIT) return RES_NOTRDY;

    for (UINT i = 0; i < count; i++)
    {
        if (!SDLogger_WriteBlock((uint32_t)sector + i, buff + (i * 512U)))
            return RES_ERROR;
    }
    return RES_OK;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;
    if (g_status & STA_NOINIT) return RES_NOTRDY;

    switch (cmd)
    {
        case CTRL_SYNC:
            return SDLogger_WaitReady() ? RES_OK : RES_ERROR;
        case GET_SECTOR_SIZE:
            *(WORD *)buff = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

/* Appele par l'application quand la carte ne repond plus (retiree, erreur) */
void disk_force_reinit(void)
{
    g_status = STA_NOINIT;
}
