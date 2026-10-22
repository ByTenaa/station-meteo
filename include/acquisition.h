#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdbool.h>

/**
 * @brief Initialise tous les capteurs et la carte SD.
 *        Affiche sur le port serie les capteurs detectes.
 */
void Acquisition_Init(void);

/**
 * @brief Lit tous les capteurs une fois et ajoute une ligne au fichier CSV.
 * @return true si la ligne a bien ete ecrite sur la carte SD.
 */
bool Acquisition_Run(void);

#endif
