# IVAO FlightGear Launcher Linux v0.1 (prototype)

Version Linux du lanceur macOS v0.7 : quatre chemins, sauvegarde des paramètres, FlightGear GUI puis bridge permanent et Altitude.

## Prérequis

- Linux x86_64, Python 3, tkinter (`python3-tk` sur Debian/Ubuntu), GCC, `file`, `ldd`, `libpcap`.
- FlightGear installé, chemin vers l'exécutable `fgfs` et son dossier FGRoot.
- Application **Altitude pour Linux** installée (binaire ELF/AppImage).
- **Plugin X-Plane IVAO Linux `lin_x64/*.xpl`**, pas le plugin macOS `mac_x64/*.xpl`.
- Capture FSD : `libpcap` et droits suffisants pour capturer le trafic sur l'interface choisie (sur Linux, les permissions de capture demandent une configuration spécifique). Aucun lancement root du GUI n'est recommandé.
- Les bibliothèques nécessaires au plugin IVAO doivent être présentes : `ldd Bridge/ivao_pilot_test.xpl`.

## Démarrage

```bash
sudo apt install python3-tk gcc libc6-dev file libpcap0.8
cd IVAO_FlightGear_Linux_v01
./install.sh
python3 launcher.py
```

Sélectionner les chemins, puis Démarrer. L'application configure `fg-altitude29.xml` dans FGRoot, compile le bridge C et le shim, lance le GUI FlightGear avec paramètres locaux, attend que Fly! ouvre Telnet 5503, démarre le bridge `--run` sans timeout puis ouvre Altitude.

Données utilisateur : `~/.config/ivao-flightgear-launcher/settings.json` ; logs : `~/.local/state/ivao-flightgear-launcher/logs/`.

## Limites / sécurité

**Le portage Linux ne peut pas être considéré fonctionnel avec IVAO avant test avec le véritable plugin Linux.** Les dépendances dynamiques et les symboles XPLM exigés par le binaire peuvent différer de la version macOS. Le système de chargement Linux ELF dépend du `DT_NEEDED` du plugin ; les alias XPLM fournis ne garantissent pas que le noyau puisse trouver toutes les autres bibliothèques. `build_linux.sh` ne modifie pas le binaire fourni.

Le plugin officiel n'est pas redistribué. Pour une distribution publique, autorisation IVAO et audit de sécurité nécessaires.

Le protocole `--allow-nasal-from-sockets` donne accès à Nasal : ne pas exposer les sockets Telnet sur le réseau, rester sur 127.0.0.1. Le binaire `fgfs --launcher` doit accepter les paramètres de configuration selon sa version. Ne pas démarrer deux exemplaires simultanés du bridge.

Pour éviter le root : sur Linux, capabilités de capture et permissions réseau devront être mises en place par paquet d'installation plutôt que d'exécuter le lanceur en superutilisateur.
