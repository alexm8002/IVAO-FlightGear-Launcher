# Phase 51 — Trafic IVAO vers objets AI FlightGear

Le bridge reprend le protocole et la création AI du test autonome réussi : TCP Telnet port 5503, commandes terminees par CRLF, `nasal` + `##EOF##`, `fgcommand('add-aiobject', ...)` avec `type:'aircraft'`, fichier `Aircraft/c172/Models/c172.ac`.

À partir de positions FSD, il crée des objets AI et tente de retrouver chaque objet à travers le noeud `/ai/models/aircraft` et son callsign `IVAO-...`, pour modifier ses propriétés de position. Les compteurs sont enregistrés sous `/sim/ivao-bridge/`.

Le modèle C172 est **temporaire**, celui dont la création est déjà validée sur votre installation ; un mapping des types ICAO suivra après validation du bridge. Le visuel 3D du C172 (assemblage géométrique) reste celui constaté avec le fichier `.ac`.

Lancement FlightGear (en complément des autres arguments existants) :

```
--telnet=socket,in,10,127.0.0.1,5503,tcp
--allow-nasal-from-sockets
--prop:/sim/ai/enabled=true
```

La commande bridge demeure :

```
./build_macos.sh '/Volumes/Sans titre/X-Plane 12/Resources/plugins/ivao_pilot/mac_x64/ivao_pilot.xpl'
./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 --fg-ai --fg-ai-port 5503 2>&1 | tee phase51.log
```

**Sécurité** : n'activez l'exécution Nasal depuis socket qu'avec un service Telnet accessible sur 127.0.0.1.

Validation locale : compilation Clang stricte et test unitaire FSD. Le fonctionnement 3D macOS reste à vérifier sur le simulateur cible.
