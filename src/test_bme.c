/*
 * Test du capteur Grove BME680 (temperature, humidite, pression, gaz).
 *
 * La fonction GroveBME680_ReadEnvironment() de la lib cesi_grove renvoie des
 * valeurs fixes (placeholder). Ce test fait donc la vraie mesure lui-meme :
 *   1. lecture des coefficients de calibration usine,
 *   2. mesure en mode "forced" (une mesure a la demande),
 *   3. application des formules de compensation de la datasheet Bosch.
 */
#include "test_bme.h"
#include "board_pins.h"
#include <stdio.h>
#include <stdint.h>

extern I2C_HandleTypeDef GROVE_I2C_HANDLE; /* defini dans setup.c */

/* Registres BME680 */
#define REG_CHIP_ID      0xD0
#define REG_RESET        0xE0
#define REG_CTRL_HUM     0x72
#define REG_CTRL_MEAS    0x74
#define REG_CONFIG       0x75
#define REG_CTRL_GAS_1   0x71
#define REG_GAS_WAIT_0   0x64
#define REG_RES_HEAT_0   0x5A
#define REG_FIELD0       0x1D
#define REG_COEFF1       0x89 /* 25 octets */
#define REG_COEFF2       0xE1 /* 16 octets */
#define REG_RES_HEAT_VAL 0x00
#define REG_RES_HEAT_RNG 0x02
#define REG_RANGE_SW_ERR 0x04

#define BME680_CHIP_ID   0x61

#define NB_MESURES       3
#define HEATER_TEMP_C    320  /* temperature cible de la plaque chauffante (gaz) */
#define HEATER_TIME_MS   150

static uint16_t bme_addr;

typedef struct
{
    uint16_t t1; int16_t t2; int8_t t3;
    uint16_t p1; int16_t p2; int8_t p3; int16_t p4; int16_t p5;
    int8_t p6; int8_t p7; int16_t p8; int16_t p9; uint8_t p10;
    uint16_t h1; uint16_t h2; int8_t h3; int8_t h4; int8_t h5; uint8_t h6; int8_t h7;
    int8_t gh1; int16_t gh2; int8_t gh3;
    uint8_t res_heat_range; int8_t res_heat_val; int8_t range_sw_err;
} Bme680Calib;

static Bme680Calib cal;

/* ---------------- Acces I2C ---------------- */

static bool rd(uint8_t reg, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&GROVE_I2C_HANDLE, bme_addr, reg, I2C_MEMADD_SIZE_8BIT,
                            data, len, 100) == HAL_OK;
}

static bool wr(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&GROVE_I2C_HANDLE, bme_addr, reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1, 100) == HAL_OK;
}

/* Affiche un float sans printf flottant (non supporte par newlib-nano) */
static void print_fixed(const char *label, float v, int decimals, const char *unit)
{
    long mult = (decimals == 2) ? 100 : (decimals == 1 ? 10 : 1);
    long x = (long)(v * mult + (v >= 0 ? 0.5f : -0.5f));
    const char *sign = (x < 0) ? "-" : "";
    if (x < 0) x = -x;

    if (decimals == 0)
        printf("    %-12s %s%ld %s\r\n", label, sign, x, unit);
    else if (decimals == 1)
        printf("    %-12s %s%ld.%01ld %s\r\n", label, sign, x / mult, x % mult, unit);
    else
        printf("    %-12s %s%ld.%02ld %s\r\n", label, sign, x / mult, x % mult, unit);
}

/* ---------------- Calibration ---------------- */

static bool read_calibration(void)
{
    uint8_t c[41];
    uint8_t v;

    if (!rd(REG_COEFF1, c, 25) || !rd(REG_COEFF2, &c[25], 16))
        return false;

    cal.t1  = (uint16_t)(c[34] << 8 | c[33]);
    cal.t2  = (int16_t)(c[2] << 8 | c[1]);
    cal.t3  = (int8_t)c[3];

    cal.p1  = (uint16_t)(c[6] << 8 | c[5]);
    cal.p2  = (int16_t)(c[8] << 8 | c[7]);
    cal.p3  = (int8_t)c[9];
    cal.p4  = (int16_t)(c[12] << 8 | c[11]);
    cal.p5  = (int16_t)(c[14] << 8 | c[13]);
    cal.p6  = (int8_t)c[16];
    cal.p7  = (int8_t)c[15];
    cal.p8  = (int16_t)(c[20] << 8 | c[19]);
    cal.p9  = (int16_t)(c[22] << 8 | c[21]);
    cal.p10 = c[23];

    cal.h1  = (uint16_t)((c[27] << 4) | (c[26] & 0x0F));
    cal.h2  = (uint16_t)((c[25] << 4) | (c[26] >> 4));
    cal.h3  = (int8_t)c[28];
    cal.h4  = (int8_t)c[29];
    cal.h5  = (int8_t)c[30];
    cal.h6  = c[31];
    cal.h7  = (int8_t)c[32];

    cal.gh1 = (int8_t)c[37];
    cal.gh2 = (int16_t)(c[36] << 8 | c[35]);
    cal.gh3 = (int8_t)c[38];

    if (!rd(REG_RES_HEAT_RNG, &v, 1)) return false;
    cal.res_heat_range = (v & 0x30) >> 4;
    if (!rd(REG_RES_HEAT_VAL, &v, 1)) return false;
    cal.res_heat_val = (int8_t)v;
    if (!rd(REG_RANGE_SW_ERR, &v, 1)) return false;
    cal.range_sw_err = (int8_t)((v & 0xF0) >> 4);

    return true;
}

/* ---------------- Compensation (formules Bosch, version flottante) ---------------- */

static float comp_temperature(uint32_t adc, float *t_fine)
{
    float var1 = (((float)adc / 16384.0f) - ((float)cal.t1 / 1024.0f)) * (float)cal.t2;
    float d    = ((float)adc / 131072.0f) - ((float)cal.t1 / 8192.0f);
    float var2 = d * d * ((float)cal.t3 * 16.0f);
    *t_fine = var1 + var2;
    return *t_fine / 5120.0f;
}

static float comp_pressure(uint32_t adc, float t_fine)
{
    float var1 = (t_fine / 2.0f) - 64000.0f;
    float var2 = var1 * var1 * ((float)cal.p6 / 131072.0f);
    var2 = var2 + (var1 * (float)cal.p5 * 2.0f);
    var2 = (var2 / 4.0f) + ((float)cal.p4 * 65536.0f);
    var1 = ((((float)cal.p3 * var1 * var1) / 16384.0f) + ((float)cal.p2 * var1)) / 524288.0f;
    var1 = (1.0f + (var1 / 32768.0f)) * (float)cal.p1;

    if (var1 == 0.0f) return 0.0f;

    float p = 1048576.0f - (float)adc;
    p = ((p - (var2 / 4096.0f)) * 6250.0f) / var1;
    var1 = ((float)cal.p9 * p * p) / 2147483648.0f;
    var2 = p * ((float)cal.p8 / 32768.0f);
    float p256 = p / 256.0f;
    float var3 = p256 * p256 * p256 * ((float)cal.p10 / 131072.0f);
    p = p + (var1 + var2 + var3 + ((float)cal.p7 * 128.0f)) / 16.0f;
    return p; /* en Pa */
}

static float comp_humidity(uint16_t adc, float temp_c)
{
    float var1 = (float)adc - (((float)cal.h1 * 16.0f) + (((float)cal.h3 / 2.0f) * temp_c));
    float var2 = var1 * (((float)cal.h2 / 262144.0f) *
                 (1.0f + (((float)cal.h4 / 16384.0f) * temp_c) +
                  (((float)cal.h5 / 1048576.0f) * temp_c * temp_c)));
    float var3 = (float)cal.h6 / 16384.0f;
    float var4 = (float)cal.h7 / 2097152.0f;
    float h = var2 + ((var3 + (var4 * temp_c)) * var2 * var2);

    if (h > 100.0f) h = 100.0f;
    if (h < 0.0f)   h = 0.0f;
    return h;
}

static float comp_gas(uint16_t adc, uint8_t range)
{
    static const float k1[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, -0.8f,
                                 0.0f, 0.0f, -0.2f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f};
    static const float k2[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.7f, 0.0f, -0.8f,
                                 -0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float var1 = 1340.0f + (5.0f * (float)cal.range_sw_err);
    float var2 = var1 * (1.0f + k1[range] / 100.0f);
    float var3 = 1.0f + (k2[range] / 100.0f);
    return 1.0f / (var3 * 0.000000125f * (float)(1UL << range) *
                   ((((float)adc - 512.0f) / var2) + 1.0f)); /* en ohms */
}

/* Valeur a ecrire dans res_heat_0 pour chauffer a target_c */
static uint8_t heater_resistance(uint16_t target_c, int amb_c)
{
    float var1 = ((float)cal.gh1 / 16.0f) + 49.0f;
    float var2 = (((float)cal.gh2 / 32768.0f) * 0.0005f) + 0.00235f;
    float var3 = (float)cal.gh3 / 1024.0f;
    float var4 = var1 * (1.0f + (var2 * (float)target_c));
    float var5 = var4 + (var3 * (float)amb_c);
    return (uint8_t)(3.4f * ((var5 * (4.0f / (4.0f + (float)cal.res_heat_range)) *
                     (1.0f / (1.0f + ((float)cal.res_heat_val * 0.002f)))) - 25.0f));
}

/* Duree de chauffe -> code du registre gas_wait_0 */
static uint8_t heater_duration(uint16_t ms)
{
    uint8_t factor = 0;
    if (ms >= 0xFC0) return 0xFF;
    while (ms > 0x3F) { ms /= 4; factor++; }
    return (uint8_t)(ms + factor * 64);
}

/* ---------------- Mesure ---------------- */

typedef struct
{
    float temp_c, hum_pct, press_hpa, gas_ohm;
    bool gas_valid, heat_stable;
} Bme680Mesure;

static bool measure(Bme680Mesure *m, int amb_c)
{
    /* Reglages : humidite x1, temperature x2, pression x16, filtre IIR 3, gaz 320 C / 150 ms */
    if (!wr(REG_CTRL_HUM, 0x01)) return false;
    if (!wr(REG_CONFIG, 0x08)) return false;
    if (!wr(REG_RES_HEAT_0, heater_resistance(HEATER_TEMP_C, amb_c))) return false;
    if (!wr(REG_GAS_WAIT_0, heater_duration(HEATER_TIME_MS))) return false;
    if (!wr(REG_CTRL_GAS_1, 0x10)) return false;          /* run_gas = 1, profil 0 */
    if (!wr(REG_CTRL_MEAS, (2 << 5) | (5 << 2) | 0x01)) return false; /* mode forced */

    /* Attente de la fin de mesure (bit new_data) */
    uint8_t f[15];
    uint32_t t0 = HAL_GetTick();
    do
    {
        HAL_Delay(20);
        if (!rd(REG_FIELD0, f, sizeof(f))) return false;
        if (HAL_GetTick() - t0 > 1000) return false;
    } while ((f[0] & 0x80) == 0);

    uint32_t press_adc = ((uint32_t)f[2] << 12) | ((uint32_t)f[3] << 4) | (f[4] >> 4);
    uint32_t temp_adc  = ((uint32_t)f[5] << 12) | ((uint32_t)f[6] << 4) | (f[7] >> 4);
    uint16_t hum_adc   = (uint16_t)((f[8] << 8) | f[9]);
    uint16_t gas_adc   = (uint16_t)((f[13] << 2) | (f[14] >> 6));
    uint8_t  gas_range = f[14] & 0x0F;

    float t_fine;
    m->temp_c      = comp_temperature(temp_adc, &t_fine);
    m->press_hpa   = comp_pressure(press_adc, t_fine) / 100.0f;
    m->hum_pct     = comp_humidity(hum_adc, m->temp_c);
    m->gas_valid   = (f[14] & 0x20) != 0;
    m->heat_stable = (f[14] & 0x10) != 0;
    m->gas_ohm     = comp_gas(gas_adc, gas_range);
    return true;
}

/* ---------------- Test ---------------- */

bool Test_BME680_Run(void)
{
    uint8_t id = 0;

    /* 1) Detection : adresse 0x76 (defaut Seeed) puis 0x77 */
    bme_addr = 0x76 << 1;
    if (!rd(REG_CHIP_ID, &id, 1) || id != BME680_CHIP_ID)
    {
        bme_addr = 0x77 << 1;
        if (!rd(REG_CHIP_ID, &id, 1) || id != BME680_CHIP_ID)
        {
            printf("  ECHEC : BME680 introuvable en 0x76 et 0x77 (port I2C ? cable ?)\r\n");
            return false;
        }
    }
    printf("  BME680 detecte en 0x%02X (chip ID 0x%02X)\r\n", bme_addr >> 1, id);

    /* 2) Reset logiciel + calibration */
    wr(REG_RESET, 0xB6);
    HAL_Delay(10);

    if (!read_calibration())
    {
        printf("  ECHEC : lecture des coefficients de calibration impossible\r\n");
        return false;
    }

    /* 3) Mesures */
    bool ok = true;
    bool gas_ok = false;
    int amb = 25; /* temperature ambiante estimee pour regler la chauffe */

    for (int i = 0; i < NB_MESURES; i++)
    {
        Bme680Mesure m;
        if (!measure(&m, amb))
        {
            printf("  ECHEC : mesure %d impossible (timeout I2C)\r\n", i + 1);
            return false;
        }
        amb = (int)m.temp_c;

        printf("  Mesure %d :\r\n", i + 1);
        print_fixed("Temperature", m.temp_c, 2, "C");
        print_fixed("Humidite", m.hum_pct, 1, "%");
        print_fixed("Pression", m.press_hpa, 2, "hPa");
        if (m.gas_valid && m.heat_stable)
        {
            print_fixed("Gaz", m.gas_ohm, 0, "ohms");
            gas_ok = true;
        }
        else
        {
            printf("    %-12s pas encore valide (chauffe en cours)\r\n", "Gaz");
        }

        /* Plages de fonctionnement du capteur */
        if (m.temp_c < -40.0f || m.temp_c > 85.0f)       { printf("    -> temperature hors plage !\r\n"); ok = false; }
        if (m.press_hpa < 300.0f || m.press_hpa > 1100.0f) { printf("    -> pression hors plage !\r\n");    ok = false; }
        if (m.hum_pct <= 0.0f || m.hum_pct >= 100.0f)     { printf("    -> humidite suspecte\r\n"); }

        HAL_Delay(1000);
    }

    if (!gas_ok)
        printf("  Remarque : mesure de gaz jamais stable. Elle se stabilise apres quelques minutes de chauffe.\r\n");

    if (!ok)
        printf("  ECHEC : valeurs incoherentes -> verifier le capteur.\r\n");

    return ok;
}