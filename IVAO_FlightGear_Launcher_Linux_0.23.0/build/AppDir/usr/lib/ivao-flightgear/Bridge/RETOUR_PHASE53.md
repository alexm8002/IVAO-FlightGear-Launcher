# Retour stable a la phase 53

Cette archive est une copie des sources de la phase 53 (sans moteur de lissage Nasal des phases 54/55).
Objectif: retrouver creation, affichage et mise a jour des appareils AI.

Important: redemarrer completement FlightGear pour supprimer les anciens objets AI et les timers Nasal des versions 54/55.

Compilation:
./build_macos.sh "/Volumes/Sans titre/X-Plane 12/Resources/plugins/ivao_pilot/mac_x64/ivao_pilot.xpl"

Lancement:
./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 --fg-ai --fg-ai-port 5503 2>&1 | tee phase53_restore.log

La phase 53 ne produit pas de lissage de mouvement et peut rester saccadee.
