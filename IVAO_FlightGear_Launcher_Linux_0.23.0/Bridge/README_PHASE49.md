# Phase 49 — Correctif du canal Telnet FlightGear

Les journaux Phase 48 montrent toujours `response_bytes=0`, alors que le décodeur IVAO reçoit les avions. Ce build refuse maintenant d’envoyer du Nasal tant que le port 5502 ne répond pas réellement à une requête FlightGear `get /sim/time/elapsed-sec`. Les échecs sont espacés de 10 secondes. La réponse à la commande Nasal est ensuite lue et rapportée.

Lancer comme en phase 48. Si `TELNET_PROBE_FAILED`, vérifier que le port 5502 est bien le serveur **Telnet FlightGear**, pas un socket de commandes unidirectionnel. Le support `--allow-nasal-from-sockets` doit être activé et le port lié à localhost.

L’injection de modèles dans le simulateur reste conditionnelle à la validation du canal Telnet et de la commande Nasal ; aucune création 3D n’est affirmée sans réponse de FlightGear.
