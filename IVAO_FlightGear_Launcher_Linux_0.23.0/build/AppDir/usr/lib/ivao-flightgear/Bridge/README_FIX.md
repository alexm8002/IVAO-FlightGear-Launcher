# Phase 50 - correctif analyse des arguments

Le parseur accepte maintenant toutes les options dans n'importe quel ordre, y compris :

```sh
./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 --fg-ai --fg-ai-port 5503
```

Relancer `./build_macos.sh /chemin/absolu/ivao_pilot.xpl` pour reconstruire le binaire sur macOS.
