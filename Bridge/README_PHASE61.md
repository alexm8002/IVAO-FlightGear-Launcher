# Phase 61 - Modele FlightGear AI XML

Base : phase 60 Fix 3 validee (connexion/deconnexion IVAO, PBH, Telnet preserve).

Changement unique dans fg_traffic.c :
- ancien : Aircraft/c172/Models/c172.ac
- nouveau : Aircraft/A380/388-AirFrance.xml

Le chemin est relatif au repertoire AI/ de fgdata, ce qui correspond a :
~/Library/Application Support/FlightGear/fgdata_2024_1/AI/Aircraft/A380/388-AirFrance.xml

Verifier la presence du fichier XML et de ses ressources .ac sur le Mac FlightGear.
FlightGear peut ne charger le modele qu'a la creation de l'objet ; redemarrer FlightGear
pour eviter que d'anciens objets IVAO aient encore le chemin de l'ancien modele.

Aucune modification des temps Telnet, de la logique Nasal, des paquets FSD,
de la frequence d'actualisation ou du nettoyage de session.
