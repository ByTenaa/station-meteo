#ifndef ACQUISITION_CONFIG_H
#define ACQUISITION_CONFIG_H

/* Periode d'acquisition (ms) : 30 s */
#define ACQ_PERIOD_MS          30000UL

/* Nom du fichier CSV sur la carte SD (format 8.3 : 8 lettres max + extension) */
#define ACQ_CSV_FILENAME       "METEO.CSV"

/*
 * Format du CSV.
 * Par defaut : separateur ';' et virgule decimale -> s'ouvre directement
 * dans Excel en francais (double-clic).
 * Pour Python/pandas : pd.read_csv("METEO.CSV", sep=";", decimal=",")
 * Format international : mettre ',' et '.'.
 */
#define ACQ_CSV_SEPARATOR      ';'
#define ACQ_CSV_DECIMAL        ','

/* Temps max d'ecoute du GPS a chaque mesure (ms) */
#define ACQ_GPS_LISTEN_MS      2500

/* Sens RX/TX du GPS : 1 = swap active (PA0 = RX), 0 = desactive (PA1 = RX).
 * Mettre la valeur trouvee par le test GPS. */
#define ACQ_GPS_SWAP_RX_TX     1

/* 1 = remet la RTC a l'heure avec le GPS (heure UTC) des que le GPS a un fix */
#define ACQ_SET_RTC_FROM_GPS   1

#endif
