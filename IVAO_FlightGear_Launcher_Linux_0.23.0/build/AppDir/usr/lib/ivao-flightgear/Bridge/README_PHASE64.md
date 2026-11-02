# Phase 64 - Reconnexion bridge sans redemarrer FlightGear

Le journal phase63.log montre 21 erreurs de suppression au premier lot : les vieux modeles IVAO vivent encore dans FlightGear.

La phase 64 ajoute une passe explicite de nettoyage au demarrage. La routine de suppression utilise le champ id s il est disponible, sinon l index du noeud aircraft (a verifier avec FlightGear 2024.1.7). `last-orphans` compte les objets dont la suppression reste en echec. Les objets sans callsign `IVAO-` ne sont jamais vises.

Aucune modification du parsing FSD, de la correspondance modele, des delais Telnet, ou du code Nasal de creation.

Limite: cette variante doit etre testee avec FG 2024.1.7; le fallback getIndex pour `remove-aiobject` n'a pas ete teste en simulateur. Si `last-orphans` reste positif, fournir le log et ne pas compter sur la suppression.
