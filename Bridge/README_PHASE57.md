# Phase 57 — PBH exact sur base Phase 53

Décode l’entier uint32 immédiatement après GS dans les trames FSD `@` : pitch bits 31-22, bank bits 21-12, heading bits 11-2 ; conversion degrés /1023. Pitch et bank normalisés sur [-180,180].

Le flux de position, la création et la suppression des objets AI de la phase 53 restent identiques. Seules les propriétés `orientation/true-heading-deg`, `orientation/pitch-deg`, `orientation/roll-deg` sont mises à jour, et l’argument `heading` à la création. Pas de timer, aucun lissage.

Attention : document de référence VATSIM; compatibilité du champ PBH dans les trames IVAO à vérifier dans FlightGear.
