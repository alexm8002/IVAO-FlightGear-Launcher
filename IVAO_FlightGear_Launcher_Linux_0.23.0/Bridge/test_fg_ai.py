#!/usr/bin/env python3
"""Test autonome FlightGear: cree un avion AI via Telnet/Nasal TCP, pas UDP."""
import argparse
import socket
import time

parser = argparse.ArgumentParser(description='Test de creation AI dans FlightGear')
parser.add_argument('--host', default='127.0.0.1')
parser.add_argument('--port', type=int, default=5503)
parser.add_argument('--model', default='Aircraft/c172/Models/c172.ac', help='Chemin modele FlightGear')
args = parser.parse_args()


def receive(sock, duration=0.5):
    end = time.monotonic() + duration
    chunks = []
    while time.monotonic() < end:
        sock.settimeout(min(.2, max(.02, end - time.monotonic())))
        try:
            data = sock.recv(8192)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            pass
    result = b''.join(chunks).decode('utf-8', 'replace')
    if result.strip():
        print(result.strip())
    return result


try:
    sock = socket.create_connection((args.host, args.port), timeout=3)
except OSError as exc:
    raise SystemExit(f'Connexion TCP {args.host}:{args.port} impossible : {exc}')

with sock:
    print(f'Connecte a {args.host}:{args.port} (TCP Telnet FlightGear).')
    receive(sock, .4)
    sock.sendall(b'get /position/latitude-deg\r\n')
    answer = receive(sock, .8)
    if not answer.strip():
        raise SystemExit('Aucune reponse au GET. Verifier --telnet=socket,in,10,127.0.0.1,5503,tcp (et non UDP).')

    model = args.model.replace('\\', '\\\\').replace('"', '\\"')
    nasal = f'''var p = geo.aircraft_position();
var h = getprop("/orientation/heading-deg");
var a = getprop("/position/altitude-ft") + 300;
var ok = fgcommand("add-aiobject", props.Node.new({{
  "type": "aircraft",
  "model": "{model}",
  "latitude": p.lat(),
  "longitude": p.lon(),
  "altitude": a,
  "heading": h,
  "callsign": "TESTAI01"
}}));
setprop("/sim/test-ai-create/ok", ok);
setprop("/sim/test-ai-create/lat", p.lat());
setprop("/sim/test-ai-create/lon", p.lon());
'''
    print('Creation de TESTAI01 a la verticale du joueur, 300 ft au-dessus...')
    sock.sendall(('nasal\r\n' + nasal + '\r\n##EOF##\r\n').encode())
    receive(sock, 1.5)
    for prop in ('/sim/test-ai-create/ok', '/sim/test-ai-create/lat', '/sim/test-ai-create/lon', '/ai/models/count'):
        sock.sendall((f'get {prop}\r\n').encode())
        print(f'[{prop}]')
        receive(sock, .65)
    sock.sendall(b'quit\r\n')
print('Si ok=true mais pas d avion visible, verifier le chemin du modele et /ai/models dans le navigateur de proprietes.')
