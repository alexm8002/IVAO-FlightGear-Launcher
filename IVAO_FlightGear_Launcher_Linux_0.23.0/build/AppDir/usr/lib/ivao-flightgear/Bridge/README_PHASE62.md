# Phase 62 — Compact Nasal + A380 XML

Base phase 60 Fix 3 pour Telnet, session IVAO, PBH et #DP.
Phase 61 avait des `Nasal parse error ... line 162` avec 16 pilotes et `last-created=0`.
Le script Nasal construisait une boucle de creation et recherche complete pour chaque pilote.
Phase 62 encode les positions dans un tableau court, puis execute une seule boucle de reconciliation.
Le modele reste `Aircraft/A380/388-AirFrance.xml` (relatif au dossier AI de fgdata).

Attention : ce correctif supprime la cause probable de saturation du parseur, mais le chargement effectif de ton XML depuis FlightGear doit etre valide sur ton Mac.
Ne change pas les delais Telnet restaurees par la phase 60 Fix 3.

Verifier dans les logs: aucune `Nasal parse error`, `last-created > 0`,
puis les fichiers FlightGear `fgfs.log` si `last-created` augmente sans geometrie visible.
