#!/usr/bin/env python3
"""Linux FlightGear / IVAO launcher (prototype). Local app settings, no plugin redistribution."""
import json
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

HERE = Path(__file__).resolve().parent
BASE = Path(os.getenv('XDG_CONFIG_HOME', Path.home()/'.config'))/'ivao-flightgear-launcher'
LOGS = Path(os.getenv('XDG_STATE_HOME', Path.home()/'.local/state'))/'ivao-flightgear-launcher'/'logs'
SETTINGS = BASE/'settings.json'
BRIDGE = HERE/'Bridge'

class UI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('IVAO FlightGear Launcher — Linux v0.1')
        self.geometry('830x445')
        self.vars = {}
        self.bridge = None
        self.fg = None
        self.alt = None
        self.busy = False
        self.cancel = threading.Event()
        self.logs = []
        defaults = {'plugin':'','fgroot':'','altitude':'','flightgear':'/usr/bin/fgfs','interface':'enp0s3','diagnostics':False}
        try: defaults.update(json.loads(SETTINGS.read_text()))
        except (OSError, ValueError): pass
        title=ttk.Label(self,text='IVAO FlightGear Launcher',font=('Sans',17,'bold'));title.pack(pady=(14,8))
        frame=ttk.Frame(self,padding=12); frame.pack(fill='x')
        for key, caption in [('plugin','Plugin X-Plane Linux (.xpl ELF 64-bit)'),('fgroot','FlightGear FGRoot'),('altitude','Altitude Linux (AppImage ou executable)'),('flightgear','FlightGear (fgfs executable)')]:
            line=ttk.Frame(frame);line.pack(fill='x',pady=4)
            ttk.Label(line,text=caption,width=39).pack(side='left')
            v=tk.StringVar(value=defaults[key]);self.vars[key]=v
            ttk.Entry(line,textvariable=v).pack(side='left',fill='x',expand=True,padx=6)
            ttk.Button(line,text='Parcourir',command=lambda k=key:self.browse(k)).pack(side='left')
        row=ttk.Frame(frame);row.pack(fill='x',pady=8)
        ttk.Label(row,text='Interface réseau',width=39).pack(side='left')
        self.vars['interface']=tk.StringVar(value=defaults['interface'])
        ttk.Entry(row,textvariable=self.vars['interface'],width=20).pack(side='left',padx=6)
        self.vars['diagnostics']=tk.BooleanVar(value=defaults['diagnostics'])
        ttk.Checkbutton(row,text='Diagnostics',variable=self.vars['diagnostics']).pack(side='left',padx=12)
        actions=ttk.Frame(frame);actions.pack(fill='x',pady=13)
        self.startbtn=ttk.Button(actions,text='Démarrer FlightGear + IVAO',command=self.start);self.startbtn.pack(side='left')
        ttk.Button(actions,text='Arrêter le bridge',command=self.stop).pack(side='left',padx=8)
        ttk.Button(actions,text='Ouvrir les logs',command=lambda:self.open_logs()).pack(side='left')
        self.status=tk.StringVar(value='Prêt. Choisir le plugin Linux (lin_x64), pas mac_x64.')
        ttk.Label(self,textvariable=self.status,wraplength=780).pack(fill='x',padx=18,pady=12)
        self.protocol('WM_DELETE_WINDOW',self.close)

    def browse(self,key):
        if key=='fgroot': path=filedialog.askdirectory(title='Choisir FGRoot')
        else: path=filedialog.askopenfilename(title='Sélectionner '+key, filetypes=[('Tous fichiers','*')])
        if path:self.vars[key].set(path)

    def status_text(self,value):self.after(0,lambda:self.status.set(value))
    def values(self):return {k:v.get() for k,v in self.vars.items()}
    def save(self,values):
        BASE.mkdir(parents=True,exist_ok=True)
        SETTINGS.write_text(json.dumps(values,ensure_ascii=False,indent=2))

    def validate(self,values):
        plugin=Path(values['plugin']);fgroot=Path(values['fgroot']); fg=Path(values['flightgear']);alt=Path(values['altitude'])
        if not plugin.is_file():raise ValueError('Sélectionne le plugin Linux lin_x64/*.xpl.')
        marker=subprocess.run(['file','-b',str(plugin)],capture_output=True,text=True,check=True).stdout
        if 'ELF 64-bit' not in marker:raise ValueError('Ce plugin n’est pas un binaire Linux ELF 64-bit : sélectionne lin_x64.')
        if not (fgroot/'Protocol').is_dir() or not (fgroot/'AI').is_dir():raise ValueError('FGRoot doit contenir Protocol et AI.')
        if not fg.is_file() or not os.access(fg,os.X_OK):raise ValueError('FlightGear doit être un exécutable fgfs valide.')
        if not alt.is_file() or not os.access(alt,os.X_OK):raise ValueError('Altitude doit être un exécutable Linux ou AppImage exécutable.')
        if not values['interface'] or not all(c.isalnum() or c in '-_.' for c in values['interface']):raise ValueError('Nom de l’interface réseau invalide.')

    def start(self):
        if self.busy:return
        values=self.values()
        try:self.validate(values);self.save(values)
        except Exception as e:messagebox.showerror('Configuration invalide',str(e));return
        self.busy=True;self.cancel.clear();self.startbtn.config(state='disabled')
        threading.Thread(target=self.worker,args=(values,),daemon=True).start()

    def worker(self,v):
        try:
            LOGS.mkdir(parents=True,exist_ok=True)
            self.log('Démarrage demandé')
            target=Path(v['fgroot'])/'Protocol/fg-altitude29.xml'
            source=BRIDGE/'fg-altitude29.xml'
            if target.exists():
                if target.read_bytes()!=source.read_bytes():raise RuntimeError('Protocole FGRoot existant différent : '+str(target))
            else:shutil.copy2(source,target)
            self.status_text('Compilation du bridge Linux…')
            self.log('Compilation du bridge Linux')
            logfile=open(LOGS/'build.log','ab')
            result=subprocess.run(['bash',str(BRIDGE/'build_linux.sh'),v['plugin']],cwd=BRIDGE,stdout=logfile,stderr=subprocess.STDOUT)
            logfile.close()
            if result.returncode:raise RuntimeError('Compilation échouée. Voir build.log (code %d).'%result.returncode)
            self.status_text('Ouverture du launcher graphique FlightGear…')
            fgargs=[v['flightgear'],'--launcher','--fg-root='+v['fgroot'],'--disable-ai-traffic','--allow-nasal-from-sockets',
                    '--telnet=socket,in,10,127.0.0.1,5502,tcp','--telnet=socket,in,10,127.0.0.1,5503,tcp',
                    '--generic=socket,out,20,127.0.0.1,5501,udp,fg-altitude29']
            self.fg=subprocess.Popen(fgargs,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
            self.log('FlightGear launcher démarré')
            self.status_text('Clique sur Fly! dans FlightGear. En attente du port 5503…')
            while not self.cancel.is_set():
                try:
                    with socket.create_connection(('127.0.0.1',5503),timeout=0.5):break
                except OSError:time.sleep(0.5)
            if self.cancel.is_set():return
            self.status_text('Lancement du bridge permanent…')
            environ=os.environ.copy()
            environ['LD_LIBRARY_PATH']=str(BRIDGE)+(':'+environ['LD_LIBRARY_PATH'] if environ.get('LD_LIBRARY_PATH') else '')
            bridgeargs=[str(BRIDGE/'altitude_host'),str(BRIDGE/'ivao_pilot_test.xpl'),'--run','--traffic-interface',v['interface'],'--fg-ai','--fg-ai-port','5503']
            if v['diagnostics']:bridgeargs+=['--log-file',str(LOGS/'bridge.log')]
            self.bridge=subprocess.Popen(bridgeargs,cwd=BRIDGE,env=environ,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
            time.sleep(1)
            if self.bridge.poll() is not None:raise RuntimeError('Bridge arrêté (code %d). Active Diagnostics; vérifier les dépendances ELF via ldd.'%self.bridge.returncode)
            self.status_text('Démarrage d’IVAO Altitude…')
            self.alt=subprocess.Popen([v['altitude']],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
            self.log('Altitude démarrée')
            self.status_text('FlightGear, bridge et Altitude démarrés. Bridge permanent.')
        except Exception as e:
            self.log('ERREUR: '+str(e));self.status_text('ERREUR : '+str(e))
            self.kill_bridge()
        finally:
            self.busy=False;self.after(0,lambda:self.startbtn.config(state='normal'))

    def log(self,s):
        LOGS.mkdir(parents=True,exist_ok=True)
        with (LOGS/'launcher.log').open('a',encoding='utf-8') as f:f.write(time.strftime('%F %T ')+s+'\n')
    def kill_bridge(self):
        if self.bridge and self.bridge.poll() is None:
            self.bridge.terminate()
            try:self.bridge.wait(timeout=5)
            except subprocess.TimeoutExpired:self.bridge.kill()
        self.bridge=None
    def stop(self):
        self.cancel.set();self.kill_bridge();self.status_text('Bridge arrêté. FlightGear et Altitude ne sont pas forcés à quitter.')
    def open_logs(self):
        LOGS.mkdir(parents=True,exist_ok=True)
        try:subprocess.Popen(['xdg-open',str(LOGS)])
        except OSError:messagebox.showinfo('Logs',str(LOGS))
    def close(self):self.stop();self.destroy()

if __name__=='__main__':
    UI().mainloop()
