# Phase 59 — corrections ciblées depuis la Phase 57

- Référence : phase 57, pas phase 58.
- Décodage `#DPCALLSIGN:VID` tel qu’observé sur IVAO, et tolérance `#DP:CALLSIGN:VID`. La suppression intervient au prochain cycle FlightGear.
- Extrapolation **modeste** latitude/longitude côté hôte à partir du heading et de la GS du dernier `@N`. Au maximum 4 secondes, seulement si GS>3 kt et PBH valide. Pas de timer Nasal ni de modification de la logique add/remove AI.
- La fréquence de sortie reste celle de la phase 57 (~1 Hz), non 20 Hz. Ceci n’est pas encore un lissage complet ; l’objectif est d’abord de tester un mouvement intermédiaire sans casser la base.
- Pas de données `^`/`#SL`/`#ST` (absentes de la capture IVAO).
- Attention : l’approximation heading ≈ route sol n'est pas exacte en présence de vent de travers. Recalage sur chaque position réelle.

Redémarrer FlightGear complètement, puis compiler et lancer selon les mêmes commandes que la phase 57.
