# Worldwide Weather Watcher — Livrable 1 : Analyse du système

## Sommaire

1. [Introduction](#1-introduction)
2. [Diagramme de composants](#2-diagramme-de-composants)
3. [Diagramme de cas d'utilisation](#3-diagramme-de-cas-dutilisation)
4. [Diagramme d'activités](#4-diagramme-dactivités)
5. [Diagramme de séquence](#5-diagramme-de-séquence)
6. [Conclusion](#6-conclusion)

---

## 1. Introduction

Le projet **Worldwide Weather Watcher (WWW)** consiste à concevoir le prototype d'une **station météo embarquée** destinée à équiper des navires pour le compte de l'AIVM (Agence Internationale pour la Vigilance Météorologique). À long terme, ces navires pourront échanger leurs données afin de prévoir des catastrophes naturelles (cyclones, tempêtes, etc.).

La station :

- mesure plusieurs paramètres grâce à des **capteurs** (pression atmosphérique, température de l'air, hygrométrie, luminosité, position GPS) ;
- exploite ces mesures pour de l'**information instantanée** (affichage) et pour de l'**archivage** sur **carte SD** ;
- est pilotée par un **membre d'équipage** à l'aide de **deux boutons poussoirs** et d'une **LED RGB** indiquant l'état du système ;
- est basée sur un microcontrôleur **Cortex M4** intégré à la carte **STM32 Nucleo L476RG**.

Ce document présente l'analyse du système à travers quatre schémas : composants, cas d'utilisation, activités et séquence.

---

## 2. Diagramme de composants

![Diagramme de composants](docs/images/diagramme_composants.png)

### 2.1 Objectif

Le diagramme de composants décrit l'**architecture matérielle** de la station : quels éléments la composent et comment ils sont reliés entre eux. Il répond à la question « de quoi est faite la station et qui communique avec qui ? ».

### 2.2 Composant central

La **carte STM32L476RG** (microcontrôleur Cortex M4) est le cœur du système. Tous les autres composants sont reliés à elle : c'est elle qui lit les capteurs, traite les données, pilote la LED, gère les boutons et écrit sur la carte SD. Aucun composant périphérique ne communique directement avec un autre.

### 2.3 Composants périphériques

| Composant | Rôle | Type de liaison (d'après le cahier des charges) |
|---|---|---|
| **Capteur de pression atmosphérique** | Mesure la pression de l'air | I2C ou SPI |
| **Hygromètre** | Mesure le taux d'humidité | I2C ou SPI |
| **Thermomètre** | Mesure la température de l'air | I2C ou SPI |
| **Capteur de luminosité** | Mesure l'intensité lumineuse | Analogique |
| **GPS** | Fournit la position géographique | UART |
| **Horloge RTC** | Fournit la date et l'heure du système (horodatage des mesures) | I2C |
| **Lecteur de carte SD** | Permet l'écriture des mesures sur la **carte SD** | SPI |
| **Carte SD** | Support de stockage des données (reliée au lecteur, liaison en pointillés car amovible) | — |
| **Boutons poussoirs** | Permettent à l'utilisateur de changer de mode | Numérique |
| **LED** | Indique l'état/le mode courant du système | 2-wire |

### 2.4 Lecture du schéma

- Chaque lien représente une **interface** (connecteur en demi-cercle en notation UML) entre un composant et la carte STM32.
- Les **capteurs** (en haut et à droite) sont des composants d'**entrée** : ils fournissent des données.
- Les **boutons** sont aussi des entrées : ils fournissent des commandes de l'utilisateur.
- La **LED** est une **sortie** : elle informe l'utilisateur.
- Le **lecteur de carte SD** est une sortie de stockage ; la **carte SD** est reliée au lecteur par un lien en pointillés, car elle est **amovible** (l'équipage la retire pour récupérer les données sur ordinateur).

### 2.5 Évolutivité

Le cahier des charges prévoit l'ajout ultérieur de modules tiers (température de l'eau, force du courant marin, force du vent, taux de particules fines). L'architecture centralisée autour de la STM32 permet de les ajouter sans remettre en cause l'existant.

---

## 3. Diagramme de cas d'utilisation

![Diagramme de cas d'utilisation](docs/images/diagramme_cas_utilisation.png)

### 3.1 Objectif

Le diagramme de cas d'utilisation décrit **ce que le système permet de faire** du point de vue de ses utilisateurs et des systèmes externes, sans entrer dans le détail technique. Le rectangle central représente la frontière du système (la station météo).

### 3.2 Les acteurs

**Acteurs humains (à gauche)**

- **Membre d'équipage** : personne qui utilise la station au quotidien à bord du navire.
- **Technicien** : acteur plus spécialisé. La flèche allant du technicien vers le membre d'équipage indique une **généralisation** : un technicien *est* un membre d'équipage et hérite donc de tous ses droits, auxquels s'ajoutent des fonctions de maintenance et de configuration.

**Acteurs « systèmes » (à droite)**

- **Système d'erreurs** : représente la détection et la gestion des défaillances matérielles.
- **Système carte SD** : représente la gestion du stockage.

### 3.3 Les cas d'utilisation

**Changements de mode (côté utilisateurs)**

| Cas d'utilisation | Acteur | Déclenchement |
|---|---|---|
| **Basculer en mode Économique** | Membre d'équipage | Appui de **5 secondes sur le bouton vert** |
| **Basculer en mode Maintenance** | Technicien | Appui de **5 secondes sur le bouton rouge** |
| **Basculer en mode Configuration** | Technicien | Accès réservé au technicien. Le système **repasse automatiquement en mode standard au bout de 30 minutes d'inactivité** |

Ces trois cas sont reliés par des **flèches en pointillés** au cas **« Démarrer mode standard »** : le mode standard est le mode de référence du système, celui dans lequel on démarre et vers lequel on revient après les autres modes.

**Gestion des erreurs (côté systèmes)**

| Cas d'utilisation | Acteur | Description |
|---|---|---|
| **Gérer erreurs RTC** | Système d'erreurs | Détecte et traite un dysfonctionnement de l'horloge |
| **Gérer erreurs GPS** | Système d'erreurs | Détecte et traite l'absence ou l'invalidité des données GPS |
| **Gérer erreurs capteurs** | Système d'erreurs | Détecte et traite une défaillance des capteurs (incohérence de mesure, absence de réponse…) |
| **Gérer remplissage carte SD pleine** | Système carte SD | Réagit lorsque la carte SD n'a plus d'espace libre |
| **Gérer erreurs d'écriture sur la carte SD** | Système carte SD | Réagit en cas d'échec d'écriture sur la carte |

### 3.4 Points clés

- Le système distingue clairement **deux niveaux de droits** : l'équipage (mode économique) et le technicien (maintenance et configuration).
- Le **mode standard** est le mode par défaut ; les autres modes y sont rattachés.
- Les erreurs sont **gérées de façon autonome** par le système (flèches venant des acteurs systèmes vers les cas d'utilisation), sans action de l'utilisateur.

---

## 4. Diagramme d'activités

![Diagramme d'activités](docs/images/diagramme_activites.png)

### 4.1 Objectif

Le diagramme d'activités décrit le **comportement dynamique** de la station : l'enchaînement des actions, les décisions et les boucles, depuis l'allumage jusqu'à la gestion des modes et des erreurs.

### 4.2 Description du flux

1. **Allumage et initialisation** : le système initialise la carte STM32, les capteurs, l'horloge RTC, le GPS et le lecteur de carte SD.
2. **Démarrage en mode standard** : la LED signale le mode standard, puis le système entre dans sa boucle de fonctionnement.
3. **Boucle d'acquisition** (modes standard et économique) :
   - lecture de l'heure via la RTC ;
   - lecture des capteurs (pression, température, hygrométrie, luminosité) ;
   - lecture de la position GPS ;
   - formatage des données et **écriture sur la carte SD**.
4. **Surveillance des boutons** : à tout moment, un appui long de 5 secondes sur un bouton déclenche un changement de mode :
   - **bouton vert** → mode **économique** (fréquence d'acquisition réduite) ;
   - **bouton rouge** → mode **maintenance** (données affichées en direct, écriture SD interrompue pour pouvoir retirer la carte sans risque) ;
   - mode **configuration** → saisie de paramètres via l'interface série, retour automatique en standard après 30 minutes d'inactivité.
5. **Gestion des erreurs** : en cas d'erreur (RTC, GPS, capteur, SD pleine, erreur d'écriture), le système signale le problème via la LED et applique le comportement prévu (nouvelle tentative, valeur par défaut `NA`, arrêt de l'enregistrement…).

### 4.3 Points clés

- Le système fonctionne en **boucle continue** tant qu'il est sous tension.
- Les **décisions** portent sur l'état des boutons, la validité des données lues et l'état de la carte SD.
- Chaque erreur a une **issue définie**, afin que la station reste fiable sans intervention.

---

## 5. Diagramme de séquence

![Diagramme de séquence](docs/images/diagramme_sequence.png)

### 5.1 Objectif

Le diagramme de séquence montre, dans l'ordre chronologique, les **échanges de messages** entre les acteurs et les composants logiciels/matériels lors des scénarios principaux. Le temps s'écoule de haut en bas.

### 5.2 Les participants (lignes de vie)

| Participant | Rôle |
|---|---|
| **Ordinateur** | Poste externe utilisé pour lire les données récupérées sur la carte SD |
| **Utilisateur** | Membre d'équipage / technicien qui pilote la station |
| **Interface** | Boutons et LED, point d'entrée des commandes de l'utilisateur |
| **Système** | Logique de contrôle (carte STM32) qui gère les modes |
| **Capteurs** | Ensemble des capteurs et du GPS |

Le diagramme est découpé en **deux parties (cadres)**.

### 5.3 Partie 1 — Affichage des données de la station météo (messages 1 à 19)

**a) Démarrage en mode standard (1 à 4)**

1. L'utilisateur **allume** la station via l'interface.
2. L'interface déclenche l'**activation du mode standard** dans le système.
3. Le système **active les capteurs**.
4. Les données mesurées sont **stockées sur la carte SD**.

**b) Mode maintenance (5 à 9)**

5. L'utilisateur demande le **passage en mode maintenance**.
6. Le système **active le mode maintenance**.
7. Il **demande les données** aux capteurs.
8. Les capteurs **envoient les données**.
9. Les données sont **affichées à l'écran** via l'interface.

**c) Mode économique (10 à 13)**

10. L'utilisateur demande le **passage en mode économie**.
11. Le système **active le mode économie**.
12. Il **réduit la fréquence d'acquisition** et **active le GPS** selon la cadence prévue pour ce mode.
13. Les données sont **stockées sur la carte**.

**d) Mode configuration (14 à 19)**

14. L'utilisateur demande le **passage en mode configuration**.
15. Le système **active le mode configuration**.
16. Le système **ouvre l'interface série**.
17. L'utilisateur **saisit les données de configuration**.
18. L'interface **transmet** les données de configuration au système.
19. Le système **applique les données de configuration** aux capteurs.

### 5.4 Partie 2 — Récupération des données de la station météo (messages 20 à 24)

20. L'utilisateur **récupère la carte SD** (retrait du système).
21. Il **lit les données** sur l'**ordinateur**.
22. Il **remet la carte SD** dans la station.
23. L'utilisateur demande le **passage en mode standard** (ou celui-ci a lieu automatiquement après **30 minutes d'inactivité**).
24. Le système **active le mode standard** et reprend l'acquisition normale.

### 5.5 Points clés

- Chaque changement de mode suit le même schéma : **utilisateur → interface → système → capteurs**.
- Les **flèches pleines** sont des messages envoyés ; les **flèches en pointillés** sont des retours/réponses.
- Le scénario de récupération des données montre que la **carte SD est le moyen d'échange** entre la station et l'extérieur.

---

## 6. Conclusion

Ces quatre diagrammes se complètent :

| Diagramme | Point de vue | Question à laquelle il répond |
|---|---|---|
| **Composants** | Architecture matérielle | De quoi est composée la station ? |
| **Cas d'utilisation** | Fonctionnel | Que permet-elle de faire, et à qui ? |
| **Activités** | Comportement | Comment le système fonctionne-t-il pas à pas ? |
| **Séquence** | Interactions dans le temps | Qui échange quoi, et dans quel ordre ? |

Ils constituent la base de la phase de développement : les composants définissent le câblage et les drivers à utiliser, les cas d'utilisation fixent les fonctionnalités attendues, et les diagrammes d'activités et de séquence guident l'écriture du code de la machine à états du système.
