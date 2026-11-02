#!/usr/bin/env python3
"""Linux launcher for v50 bridge. Requires a native Linux IVAO .xpl plugin."""
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

ROOT=Path(__file__).resolve().parents[1]
BRIDGE=ROOT/'Bridge'
STATE=Path.home()/'.config'/'ivao-flightgear-launcher'
LOGS=Path.home()/'.local'/'state'/'ivao-flightgear-launcher'
CONFIG=STATE/'settings.json'
FIELDS=[('Plugin IVAO Linux (.xpl)', 'plugin'), ('FG_ROOT (dossier data)', 'fgroot'), ('Executable FlightGear (fgfs)', 'fgfs'), ('Executable Altitude (optionnel)', 'altitude'), ('Interface reseau', 'interface')]
DEFAULTS={'plugin':'','fgroot':'/usr/share/games/flightgear','fgfs':shutil.which('fgfs') or '/usr/bin/fgfs','altitude':'','interface':'eth0'}

class Launcher(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('IVAO FlightGear Launcher Linux - 0.23.0')
        self.geometry('850x440')
        self.vars={}
        self.procs=[]
        self.abort=threading.Event()
        STATE.mkdir(parents=True,exist_ok=True)
        LOGS.mkdir(parents=True,exist_ok=True)
        settings=DEFAULTS.copy()
        try: settings.update(json.loads(CONFIG.read_text()))
        except (OSError,ValueError): pass
        frame=ttk.Frame(self,padding=14);frame.pack(fill='both',expand=True)
        ttk.Label(frame,text='IVAO / FlightGear - Linux (based on validated macOS v50)',font=('',15,'bold')).grid(row=0,column=0,columnspan=3,pady=(0,12),sticky='w')
        for row,(title,key) in enumerate(FIELDS,1):
            ttk.Label(frame,text=title).grid(row=row,column=0,sticky='w',pady=6)
            v=tk.StringVar(value=settings.get(key,''));self.vars[key]=v
            ttk.Entry(frame,textvariable=v,width=65).grid(row=row,column=1,sticky='ew',padx=10)
            if key!='interface': ttk.Button(frame,text='Parcourir...',command=lambda k=key:self.browse(k)).grid(row=row,column=2)
        self.status=tk.StringVar(value='Pret. Selectionner le plugin Linux IVAO natif.')
        ttk.Label(frame,textvariable=self.status,wraplength=770).grid(row=6,column=0,columnspan=3,sticky='w',pady=15)
        action=ttk.Frame(frame);action.grid(row=7,column=0,columnspan=3,sticky='w')
        self.start_btn=ttk.Button(action,text='Demarrer FlightGear + IVAO',command=self.start);self.start_btn.pack(side='left',padx=(0,8))
        ttk.Button(action,text='Arreter bridge',command=self.stop).pack(side='left',padx=(0,8))
        ttk.Button(action,text='Dossier journaux',command=lambda:subprocess.Popen(['xdg-open',str(LOGS)])).pack(side='left')
        ttk.Label(frame,text='Le plugin doit etre un ELF Linux 64 bits. Les .xpl macOS sont incompatibles.\nLa capture libpcap necessite des permissions reseau adaptees.',wraplength=780).grid(row=8,column=0,columnspan=3,sticky='w',pady=12)
        frame.columnconfigure(1,weight=1)
        self.protocol('WM_DELETE_WINDOW',self.close)
    def browse(self,k):
        if k=='fgroot': v=filedialog.askdirectory()
        else: v=filedialog.askopenfilename()
        if v:self.vars[k].set(v)
    def set_status(self,s):self.after(0,lambda:self.status.set(s))
    def save(self):CONFIG.write_text(json.dumps({k:v.get().strip() for k,v in self.vars.items()},indent=2))
    def start(self):
        if self.procs and any(p.poll() is None for p in self.procs):messagebox.showinfo('IVAO','Deja en cours');return
        self.save();self.abort.clear()
        self.start_btn.config(state='disabled')
        threading.Thread(target=self.worker,daemon=True).start()
    def worker(self):
        try:
            data={k:v.get().strip() for k,v in self.vars.items()}
            plugin=Path(data['plugin']).expanduser().resolve()
            fgroot=Path(data['fgroot']).expanduser().resolve()
            fgfs=Path(data['fgfs']).expanduser()
            if not plugin.is_file() or plugin.suffix!='.xpl':raise RuntimeError('Plugin Linux .xpl manquant')
            with plugin.open('rb') as f: magic=f.read(5)
            if not magic.startswith(b'\x7fELF'):raise RuntimeError('Le plugin selectionne n est pas un binaire ELF Linux')
            if magic[4]!=2:raise RuntimeError('Le plugin doit etre un ELF 64 bits')
            if not fgroot.joinpath('AI').is_dir() or not fgroot.joinpath('Protocol').is_dir():raise RuntimeError('FG_ROOT invalide: AI/ et Protocol/ manquants')
            if not fgfs.is_file() or not os.access(fgfs,os.X_OK):raise RuntimeError('Executable fgfs introuvable')
            if not BRIDGE.joinpath('altitude_host').is_file():raise RuntimeError('Lancer ./build_linux.sh au prealable')
            # Do not overwrite an existing user-modified protocol.
            proto=fgroot/'Protocol'/'fg-altitude29.xml'
            if not proto.exists():
                try: shutil.copy2(BRIDGE/'fg-altitude29.xml',proto)
                except PermissionError as exc:raise RuntimeError('Copier Bridge/fg-altitude29.xml vers FG_ROOT/Protocol avec les droits administrateur') from exc
            self.set_status('Demarrage FlightGear...')
            env=os.environ.copy()
            flags=[str(fgfs),'--launcher',f'--fg-root={fgroot}','--disable-ai-traffic','--allow-nasal-from-sockets',
                '--telnet=socket,in,10,127.0.0.1,5502,tcp','--telnet=socket,in,10,127.0.0.1,5503,tcp',
                '--generic=socket,out,20,127.0.0.1,5501,udp,fg-altitude29']
            with (LOGS/'flightgear.log').open('a') as log:
                fg=subprocess.Popen(flags,stdout=log,stderr=subprocess.STDOUT,env=env)
            self.procs.append(fg)
            self.set_status('Dans le launcher FlightGear, cliquer Fly! (port 5503 attendu)')
            for _ in range(1200):
                if self.abort.is_set():return
                if fg.poll() is not None:raise RuntimeError('FlightGear est termine (voir flightgear.log)')
                try:
                    with socket.create_connection(('127.0.0.1',5503),timeout=0.3):break
                except OSError:time.sleep(.5)
            else:raise RuntimeError('Port 5503 non disponible')
            # Linux .xpl remains untouched, and the shim is exposed with Linux SONAME aliases.
            # Loading from the bridge working directory preserves its diagnostic CSL asset paths.
            env['LD_LIBRARY_PATH']=str(BRIDGE)+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
            cmd=[str(BRIDGE/'altitude_host'),str(plugin),'--run','--traffic-interface',data['interface'],'--fg-ai','--xpmp-ai','--fg-ai-port','5503','--log-file',str(LOGS/'bridge.log')]
            self.set_status('Demarrage bridge IVAO...')
            bridge=subprocess.Popen(cmd,cwd=BRIDGE,env=env)
            self.procs.append(bridge)
            altitude=data['altitude']
            if altitude:
                if not Path(altitude).is_file():raise RuntimeError('Executable Altitude introuvable')
                self.procs.append(subprocess.Popen([altitude],env=env))
            time.sleep(1)
            if bridge.poll() is not None:raise RuntimeError('Bridge arrete: examiner bridge.log (dependances plugin, libpcap)')
            self.set_status('Bridge en cours. Logs: '+str(LOGS))
        except Exception as e:
            self.set_status('Erreur: '+str(e))
            self.after(0,lambda e=e:messagebox.showerror('IVAO FlightGear',str(e)))
        finally:self.after(0,lambda:self.start_btn.config(state='normal'))
    def stop(self):
        self.abort.set()
        # Stop bridge, not FlightGear or Altitude that users may keep running.
        for p in self.procs[1:2]:
            if p.poll() is None:p.terminate()
        self.set_status('Bridge arrete; FlightGear reste ouvert.')
    def close(self):
        self.save();self.stop();self.destroy()

if __name__=='__main__':Launcher().mainloop()
