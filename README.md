# Worldwide Weather Watcher — Livrable 1 : Analyse du système

## Sommaire

1. [Introduction](#1-introduction)
2. [Diagramme de composants](#2-diagramme-de-composants)
3. [Diagramme de cas d'utilisation](#3-diagramme-de-cas-dutilisation)
4. [Diagramme d'activités](#4-diagramme-dactivités)
5. [Diagrammes de séquence](#5-diagrammes-de-séquence)
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
<img width="1150" height="567" alt="image" src="https://github.com/user-attachments/assets/4f4401b7-9774-4bab-9226-1cfc41662c10" />



### 2.1 Objectif

Le diagramme de composants décrit l'**architecture matérielle** de la station : quels éléments la composent et par quel **bus ou interface** ils communiquent avec la carte. Il répond à la question « de quoi est faite la station et comment les éléments sont-ils reliés ? ».

### 2.2 Composant central

La **carte STM32L476RG** (microcontrôleur Cortex M4) est le cœur du système. Tous les autres composants sont reliés à elle : c'est elle qui lit les capteurs, traite les données, pilote la LED, gère les boutons et écrit sur la carte SD. Aucun composant périphérique ne communique directement avec un autre.

### 2.3 Composants périphériques et interfaces

Chaque liaison est annotée avec le type d'interface utilisé.

| Composant | Rôle | Interface |
|---|---|---|
| **Capteur de pression atmosphérique** | Mesure la pression de l'air | **I2C** |
| **Hygromètre** | Mesure le taux d'humidité | **I2C** |
| **Capteurs de température** | Mesurent la température de l'air | **I2C** |
| **Capteur de luminosité** | Mesure l'intensité lumineuse | **ADC** (conversion analogique-numérique) |
| **GPS** | Fournit la position géographique | **USART** |
| **Horloge RTC** | Fournit la date et l'heure (horodatage des mesures) | **I2C** |
| **Lecteur de carte SD** | Permet l'écriture des mesures sur la carte SD | **SPI** |
| **Carte SD** | Support de stockage des données (reliée au lecteur par un lien en pointillés car **amovible**) | — |
| **Boutons poussoirs** | Permettent à l'utilisateur de changer de mode | **GPIO + EXTI** |
| **LED** | Indique l'état/le mode courant du système | **GPIO** |
| **Module tiers** | Emplacement prévu pour les modules ajoutés ultérieurement | Selon le module |

### 2.4 Lecture du schéma

- Chaque lien aboutit à une **interface** (demi-cercle en notation UML) de la carte STM32.
- Plusieurs composants partagent le même type de bus (**I2C** pour la pression, l'hygrométrie, la température et la RTC). Ils se distinguent par leur adresse sur le bus.
- Les **capteurs** et les **boutons** sont des composants d'**entrée** : ils fournissent des mesures ou des commandes.
- La **LED** est un composant de **sortie** : elle informe l'utilisateur.
- Les interfaces **EXTI** (interruptions externes) permettent à la carte de réagir immédiatement à un événement, comme l'appui sur un bouton, sans interroger en permanence l'état des broches.
- La **carte SD** est amovible : l'équipage la retire pour récupérer les données sur ordinateur.

### 2.5 Évolutivité

Le **module tiers** représente les modules prévus par le cahier des charges (température de l'eau, force du courant marin, force du vent, taux de particules fines). Leur interface n'est pas figée sur le schéma, car elle dépend du module : analogique, I2C ou 2-wire. L'architecture centralisée autour de la STM32 permet de les ajouter sans remettre en cause l'existant.

---

## 3. Diagramme de cas d'utilisation

<img width="810" height="667" alt="image" src="https://github.com/user-attachments/assets/55b1464a-93dd-4b1a-9725-86581ff1da21" />


### 3.1 Objectif

Le diagramme de cas d'utilisation décrit **ce que le système permet de faire** du point de vue de ses utilisateurs et des acteurs externes, sans entrer dans le détail technique. Le grand rectangle **« STATION »** représente la frontière du système.

### 3.2 Les acteurs

- **Membre d'équipage** : personne qui utilise la station au quotidien à bord du navire.
- **Technicien** : acteur plus spécialisé. La flèche allant du technicien vers le membre d'équipage indique une **généralisation** : un technicien *est* un membre d'équipage et hérite donc de tous ses cas d'utilisation, auxquels s'ajoutent la maintenance et la configuration.
- **Horloge RTC** : acteur secondaire (à droite), un composant externe que la station sollicite pour obtenir l'heure.

### 3.3 Les cas d'utilisation

| Cas d'utilisation | Acteur | Description |
|---|---|---|
| **Démarrer en mode standard** | Membre d'équipage | Démarrage de la station dans son mode de fonctionnement par défaut |
| **Revenir en mode standard** | Membre d'équipage | Retour au mode standard depuis un autre mode |
| **Basculer en mode Économique** | Membre d'équipage | Accès en appuyant **5 secondes sur le bouton vert** |
| **Basculer en mode Maintenance** | Technicien | Accès en appuyant **5 secondes sur le bouton rouge** |
| **Basculer en mode Configuration** | Technicien | Retour automatique en mode standard au bout de **30 minutes d'inactivité** |

### 3.4 Les relations entre cas d'utilisation

- **« Changer de mode »** : les quatre cas *Revenir en mode standard*, *Basculer en mode Économique*, *Basculer en mode Maintenance* et *Basculer en mode Configuration* sont regroupés sous ce cas général. Ce sont les variantes d'une même action : changer le mode de fonctionnement de la station.
- **`<<include>>` — Horodater des mesures** : *Démarrer en mode standard* **inclut obligatoirement** l'horodatage des mesures. Chaque mesure est systématiquement datée.
- **`<<use>>` — Horloge RTC** : pour horodater, la station s'appuie sur l'acteur **Horloge RTC**, qui fournit la date et l'heure.
- **`<<extend>>` — Être alerté en cas d'erreurs** : ce comportement est **optionnel** et ne se déclenche que si une erreur survient pendant le fonctionnement standard (la LED signale alors le problème).

### 3.5 Points clés

- Le système distingue **deux niveaux de droits** : l'équipage (démarrage, retour au standard, mode économique) et le technicien (maintenance et configuration).
- Le **mode standard** est le mode de référence, auquel on revient depuis les autres modes.
- La gestion des erreurs est une **extension** du mode standard : elle n'intervient qu'en cas de problème.

---

## 4. Diagramme d'activités
<img width="1280" height="732" alt="image" src="https://github.com/user-attachments/assets/1d2ab831-2b23-4269-b3d8-2fe3f6e2b6ab" />
<img width="1335" height="640" alt="image" src="https://github.com/user-attachments/assets/60c90cda-b25e-45bd-8d88-91f0e39db46d" />



### 4.1 Objectif

Le diagramme d'activités décrit le **comportement dynamique** de la station : l'enchaînement des actions, les décisions et les boucles, depuis l'allumage jusqu'à la gestion des modes et des erreurs. Les conditions des décisions sont notées entre crochets (`[oui]`, `[Erreur de lecture]`, `[Carte SD pleine]`…).

### 4.2 Démarrage

1. Le **nœud initial** mène à l'action **« Allumer le système »**.
2. Une décision teste **`[Bouton rouge pressé]`** :
   - **`[oui]`** : la station démarre en **mode configuration** ;
   - **`[non]`** : la station démarre en **mode standard**.

### 4.3 Mode configuration (encadré jaune)

1. **Ouvrir l'interface série** et **allumer la LED jaune**.
2. **Réception des commandes de l'utilisateur** : cette action produit l'objet **« Paramètres »**, c'est-à-dire les données de configuration saisies.
3. Une décision teste **`[Inactivité 30 min]`** :
   - **`[non]`** : retour à la réception de commandes (la station attend de nouveaux paramètres) ;
   - **`[oui]`** : sortie du mode configuration vers l'initialisation du mode standard.

### 4.4 Mode standard (encadré vert)

1. **Initialiser** les capteurs, l'horloge RTC et la carte SD.
2. **Lire les capteurs**, puis une décision teste **`[Lecture réussie]`** :
   - **`[Erreur de lecture]`** : **allumer la LED d'erreur capteur**, puis retour à l'initialisation ;
   - sinon, la lecture est valide et le flux continue.
3. **Horodater les données** : la date et l'heure fournies par la RTC sont associées aux mesures.
4. Les mesures horodatées forment l'objet **« Ligne de mesures »**.
5. **Enregistrer sur la carte SD**.
6. Une décision **« Vérification SD »** teste l'état de la carte :
   - **`[Carte SD pleine]`** : **allumer la LED d'erreur**, puis retour à l'initialisation ;
   - **`[espace disponible]`** : le flux continue.
7. Une décision teste **« Délai de 10 min écoulé ? »**, qui correspond à la période d'acquisition du mode standard :
   - **`[oui]`** : un nouveau cycle recommence depuis « Allumer le système », ce qui referme la **boucle principale** ;
   - **`[non]`** : la station surveille les boutons pour un éventuel changement de mode (voir 4.5).

### 4.5 Modes économique et maintenance

<!-- VÉRIFIER : cette sous-section doit correspondre à la partie basse de ton diagramme d'activités -->

Depuis le mode standard, un **appui de 5 secondes** sur un bouton déclenche un changement de mode :

- **Bouton vert** → **mode économique** (LED bleue) : réduction de la fréquence d'acquisition et GPS actif **une mesure sur deux** ;
- **Bouton rouge** → **mode maintenance** (LED orange) : les données des capteurs sont redirigées vers l'interface série et l'écriture sur la carte SD est désactivée.

Un nouvel appui de 5 secondes permet de quitter le mode courant.

### 4.6 Données manipulées

| Objet | Où apparaît-il ? | Contenu |
|---|---|---|
| **Paramètres** | Mode configuration | Valeurs de configuration saisies via l'interface série |
| **Ligne de mesures** | Mode standard | Mesures des capteurs et horodatage fournis par la RTC |

### 4.7 Points clés

- Le système fonctionne en **boucle continue** tant qu'il est sous tension : le diagramme n'a **pas de nœud final**.
- Les **erreurs** sont traitées explicitement : erreur de lecture d'un capteur et carte SD pleine. Chacune allume une LED d'erreur, puis le système se réinitialise.
- Le **mode configuration** est atteint dès le démarrage (bouton rouge pressé) et se termine automatiquement après 30 minutes d'inactivité.

---

## 5. Diagrammes de séquence

Les échanges sont répartis en deux diagrammes : l'**affichage des données** (utilisation de la station) et la **récupération des données** (lecture de la carte SD).

### 5.1 Les participants (lignes de vie)

Chaque ligne de vie est notée `nom : Type`.

| Ligne de vie | Rôle |
|---|---|
| **pc : Ordinateur** | Poste externe utilisé pour lire les données de la carte SD |
| **technicien : Utilisateur** | Personne qui pilote la station |
| **ihm : BoutonsEtLEDs** | Interface homme-machine : boutons (entrées) et LED (sorties) |
| **station : CarteArduino** | Logique de contrôle de la station (carte de pilotage) |
| **capteurs : Capteurs** | Ensemble des capteurs et du GPS |
| **sd : CarteSD** | Carte SD qui stocke les mesures |

### 5.2 Diagramme de séquence de l'affichage des données
<img width="1467" height="913" alt="image" src="https://github.com/user-attachments/assets/cec8db31-d47d-4ce4-9e26-1ab683738d1e" />
<img width="1467" height="357" alt="image" src="https://github.com/user-attachments/assets/4d6cb4e8-652f-48d8-8eab-a557dc8cb142" />



Le temps s'écoule de haut en bas. Les flèches pleines sont des **messages envoyés** et les flèches en pointillés des **retours**.

**a) Démarrage en mode standard (messages 1 à 4)**

1. Le technicien **allume** la station via l'IHM.
2. L'IHM déclenche l'**activation du mode standard** (avec bouton) sur la station.
3. La station **active les capteurs**.
4. Les données sont **stockées sur la carte SD**.

**b) Mode maintenance (messages 5 à 9)**

5. Le technicien demande le **passage en mode maintenance**.
6. La station **active le mode maintenance**.
7. Elle **demande les données** aux capteurs.
8. Les capteurs **renvoient les données** (retour en pointillés).
9. Les données sont **affichées à l'écran** via l'IHM (retour en pointillés).

**c) Mode économique (messages 10 à 13)**

10. Le technicien demande le **passage en mode économie**.
11. La station **active le mode économie**.
12. Elle ordonne la **réduction de la fréquence d'acquisition** et l'**activation du GPS une fois sur deux**.
13. Le **stockage sur la carte SD** est confirmé à la station (retour en pointillés).

**d) Mode configuration (messages 14 à 19)**

14. Le technicien demande le **passage en mode config**.
15. La station **active le mode config**.
16. L'**interface série est ouverte** (retour en pointillés vers l'IHM).
17. Le technicien **saisit les données de configuration**.
18. L'IHM **transmet** ces données à la station.
19. La station **applique les données de configuration** aux capteurs.

### 5.3 Diagramme de séquence de la récupération des données


20. Le technicien **récupère la carte SD** de la station (retour en pointillés).
21. Il **lit les données** sur l'**ordinateur**.
22. Il **remet la carte SD** dans la station.
23. Le technicien demande le **passage en mode standard** (ou celui-ci a lieu automatiquement après **30 minutes d'inactivité**).
24. La station **active le mode standard** et reprend l'acquisition normale.

### 5.4 Points clés

- Chaque changement de mode suit le même schéma : **technicien → IHM → station**, puis la station agit sur les **capteurs** et la **carte SD**.
- La **carte SD** est le moyen d'échange entre la station et l'extérieur : le technicien la retire, lit son contenu sur l'ordinateur, puis la remet en place.
- Le mode économique réduit l'acquisition et active le GPS **une mesure sur deux**, ce qui est cohérent avec le diagramme d'activités.

---

## 6. Conclusion

Ces quatre diagrammes se complètent :

| Diagramme | Point de vue | Question à laquelle il répond |
|---|---|---|
| **Composants** | Architecture matérielle | De quoi est composée la station et par quelles interfaces ? |
| **Cas d'utilisation** | Fonctionnel | Que permet-elle de faire, et à qui ? |
| **Activités** | Comportement | Comment le système fonctionne-t-il pas à pas, erreurs comprises ? |
| **Séquence** | Interactions dans le temps | Qui échange quoi, et dans quel ordre ? |

### Récapitulatif des modes de fonctionnement

| Mode | Accès | LED | Comportement |
|---|---|---|---|
| **Standard** | Démarrage par défaut | — | Acquisition toutes les 10 min, enregistrement sur la carte SD |
| **Configuration** | Bouton rouge pressé au démarrage | Jaune | Interface série ouverte, retour au standard après 30 min d'inactivité |
| **Économique** | Appui de 5 s sur le bouton vert | Bleue | Acquisition réduite, GPS une mesure sur deux |
| **Maintenance** | Appui de 5 s sur le bouton rouge | Orange | Données envoyées sur l'interface série, écriture SD désactivée |

Ces diagrammes constituent la base de la phase de développement : les composants définissent le câblage et les drivers à utiliser, les cas d'utilisation fixent les fonctionnalités attendues, et les diagrammes d'activités et de séquence guident l'écriture de la machine à états du système.

Projet réalisé par AZOUZ Abderrazek, L'HOMME Louis et LO Andrea.
