# Phase 60 fix2 - correction du generateur XPLM

Le script build_macos.sh regenere xplm_shim.c via make_shim.py.
Dans fix1, la fonction shim_ivao_online etait seulement presente dans
le fichier genere, puis perdue lors de chaque compilation. Le host
affichait `no online status API` et desactivait le trafic 3D.

Fix2 ajoute l export dans make_shim.py, puis regenere xplm_shim.c.
Aucun changement du protocole FSD, de PBH ou des objets AI.

Tester la presence du symbole sur Mac apres le build :

    nm -gU libXPLMShim.dylib | grep shim_ivao_online

Puis verifier dans les logs :

    [FG TRAFFIC] IVAO session online; traffic enabled

Et batch=... aircraft=... apres la connexion.
