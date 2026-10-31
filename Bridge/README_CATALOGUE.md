# Catalogue FlightGear AI issu de l'inventaire fourni

Ce ZIP reprend la phase 63 et inclut `fg_models.tsv` reconstruit depuis le `ls -R` fourni.

- TSV : 3 colonnes sans en-tete `ICAO4<TAB>COMPAGNIE3<TAB>Aircraft/chemin.xml`.
- XML techniques (main, model, lights, effects, fichiers en sous-repertoire) exclus.
- Types 3 caracteres et variantes normalises (738 => B738, 388 => A388, 77W => B77W, 757-200 => B752).
- Priorite runtime : correspondance exacte type et compagnie, sinon livree Default, sinon autre livree du meme type, sinon A388 AFR.
- Codes IVAO EJU/EZS => easyJet EZY ; WMT => WZZ.
- `build_macos.sh` regenere le fichier TSV depuis les XML reels de ton FG_ROOT, ce qui permet de suivre les ajouts locaux. Pour utiliser exclusivement le TSV joint, executer seulement la compilation et conserver le TSV joint apres la compilation.

ATTENTION : ce fichier repose sur la liste textuelle, pas sur l'inspection du contenu des XML ou les chemins du FG_ROOT de l'utilisateur. Le rendu doit etre teste sur FlightGear.
