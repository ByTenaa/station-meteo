#ifndef TESTS_CONFIG_H
#define TESTS_CONFIG_H

/*
 * Configuration des tests materiels.
 * Mettre un test a 0 pour le desactiver (pratique pour tester les capteurs un par un).
 */
#define TEST_LED_ENABLE      1   /* LED RGB chainable (port D6)      */
#define TEST_BUTTON_ENABLE   1   /* Double bouton poussoir (port D2) */
#define TEST_LIGHT_ENABLE    1   /* Capteur de luminosite (port A2)  */
#define TEST_BME680_ENABLE   1
#define TEST_RTC_ENABLE      1   /* Horloge RTC DS1307 (port I2C)    */
#define TEST_GPS_ENABLE      1   /* GPS Air530Z (port A0 / UART4)    */
#define TEST_SD_ENABLE       1   /* Carte SD (shield SPI, CS = D4)   */

/* Pause entre deux tests (ms) */
#define TEST_PAUSE_MS            2000

/* Temps laisse pour appuyer sur chaque bouton (ms) */
#define BUTTON_TEST_TIMEOUT_MS   15000

/* Temps d'ecoute du GPS (ms). Un premier fix en exterieur peut prendre 30 s a plusieurs minutes. */
#define GPS_TEST_TIMEOUT_MS      10000

/*
 * Mettre a 1 UNE FOIS pour regler la RTC sur la date/heure de compilation,
 * flasher, puis remettre a 0 (sinon l'heure est remise a chaque reset).
 */
#define RTC_SET_TIME_FROM_BUILD  0

#endif
