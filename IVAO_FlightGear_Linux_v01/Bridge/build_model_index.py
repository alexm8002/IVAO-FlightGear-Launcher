#!/usr/bin/env python3
"""Build FlightGear AI model inventory from live FG_ROOT or 'ls -R' inventory.
TSV columns compatible with fg_traffic.c: ICAO4, airline ICAO3 or ---, Aircraft/...xml.
"""
import argparse, os, re, sys
from pathlib import Path

# Map realistic ICAO prefixes to the AI model XML prefixes in fgdata 2024.1.
# Entries with no reasonable direct match are not indexed rather than misidentified.
FAMILY={'220':'BCS2','339':'A339','717':'B712','733':'B733','73C':'B733','73F':'B733',
 '73P':'B734','735':'B735','736':'B736','738':'B738','73K':'B738',
 '739':'B739','73J':'B739','744':'B744','74F':'B744','74N':'B744','74Y':'B744',
 '752':'B752','753':'B753','763':'B763','767':'B763','76W':'B763','76Y':'B763',
 '77F':'B77F','77L':'B77L','77W':'B77W','788':'B788','789':'B789',
 '388':'A388','332':'A332','333':'A333','342':'A342','343':'A343','345':'A345','346':'A346',
 '359':'A359','35K':'A35K','359':'A359', 'E70':'E170','E90':'E190','E95':'E195',
 'CR2':'CRJ2','CR7':'CRJ7','CR9':'CRJ9', 'DH3':'DH8C','DH4':'DH8D',
 'SF3':'SF34','JS41':'JS41','A300':'A300','AB6':'A306','ABY':'A306','A33X':'A332'}
AIRLINES={
'AFR':'AirFrance','EZY':'EasyJet','EJU':'EasyJet','RYR':'Ryanair','DLH':'Lufthansa','KLM':'KLM',
'SWR':'Swiss','BAW':'BritishAirways','AAL':'American','AAN':'American','TVF':'Transavia',
'VLG':'Vueling','IBE':'Iberia','IBS':'IberiaExpress','HOP':'HOP','WZZ':'WizzAir','WMT':'WizzAir',
'UAE':'Emirates','QTR':'Qatar','FDX':'FedEx','UPS':'UPS','THY':'Turkish','EIN':'AerLingus',
'EWG':'Eurowings','EWG':'Eurowings','TAP':'TAP','TUI':'Tuifly','TRA':'Transavia',
'AEA':'AirEuropa','VOE':'Volotea','EZS':'EasyJet','SAS':'SAS','DAL':'Delta','CFG':'Condor',
'CFG':'Condor','SXS':'SunExpress','SWA':'SouthWestAirlines','UAL':'UnitedAirlines',
'ACA':'AirCanada','ASA':'AlaskaAirlines','JBU':'JetBlue','NAX':'Norwegian',
'IBE':'Iberia','RAM':'RoyalAirMaroc','THT':'TAP','AZA':'Alitalia',
'FIN':'Finnair','LOT':'LOT','AEE':'Aegean','SIA':'SingaporeAirlines',
'QFA':'Qantas','ETD':'Etihad','ETH':'Ethiopian','ANA':'ANA','JAL':'JAL','KAL':'Korean',
'CCA':'AirChina','CES':'ChinaEastern','CSN':'ChinaSouthern','TAR':'Tarom','BEL':'Belavia',
'RJA':'RoyalJordanian','SVA':'Saudia','MSR':'EgyptAir','ICE':'Icelandair','AIC':'AirIndia',
'VIR':'VirginAtlantic','VJS':'VistaJet','PGT':'Pegasus','NJE':'NetJets',
'GLO':'GOL','JAF':'JetAirways','AFL':'Aeroflot','AUI':'UkraineIntl',
'QTR':'QatarAirways','EVA':'EvaAir','EZY':'EasyJet','EJU':'EasyJet',
}
# Only aircraft descriptor XMLs, not submodel, lighting, main or scenario XMLs.
SKIP_TERMS={'main','fallback','set','navlights','lights','effects','model','performancedb', 'catalog', 'defaultmodel'}

def normalize(s):return re.sub('[^a-z0-9]','',s.casefold())
def aircraft_type(folder, stem):
    token=stem.split('-')[0].split('_')[0].upper()
    if re.fullmatch(r'A\d{3}', token):return token
    if re.fullmatch(r'B\d{3}', token):return token
    if re.fullmatch(r'[A-Z0-9]{4}', token) and token not in {'MAIN'}:
        return token
    if folder=='757' and stem.startswith('757-'):
        if stem.startswith('757-200-'): return 'B752'
        if stem.startswith('757-300-'): return 'B753'
    if token in FAMILY:return FAMILY[token]
    # 3-digit codes often omit A/B; family resolves based on folder.
    if re.fullmatch(r'\d{3}', token):
        if folder.startswith('A'):return 'A'+token
        if folder[0].isdigit():return 'B'+token
    # Some XMLs are bare non-ICAO filenames, don't guess.
    return None

def is_model_xml(rel):
    if rel.suffix.lower()!='.xml' or len(rel.parts)!=2:return False
    stem=rel.stem
    if re.search(r'(?i)(?:^|[-_])(main|fallback|set|lights|navlights|effect)(?:$|[-_])',stem):return False
    if normalize(stem) in SKIP_TERMS:return False
    return True

def airline_for(rel):
    stem=rel.stem
    # Trim model code including the exceptional 757-200 and 757-300.
    tail=re.sub(r'^[A-Za-z0-9]+(?:-\d{3})?[-_]', '', stem)
    if tail.lower() in {'default','generic','main'}:return '---'
    n=normalize(tail)
    for icao,label in AIRLINES.items():
        if n==normalize(label):return icao
    aliases={'swissair':'SWR','swiss':'SWR','britishairways':'BAW','americanairlines':'AAL',
     'american':'AAL','unitedairlines':'UAL','united':'UAL','qatarairways':'QTR',
     'qatar':'QTR','turkishairlines':'THY','turkish':'THY','delta':'DAL',
     'easyjet':'EZY','wizz':'WZZ','wizzair':'WZZ','volotea':'VOE',
     'ryanair':'RYR','airfrance':'AFR','aircanada':'ACA','airchinа':'CCA',
     'airchina':'CCA','klm':'KLM','lufthansa':'DLH','hopp':'HOP',
     'airnewzealand':'ANZ','jet2':'EXS','iberiaexpress':'IBS','iberia':'IBE',
     'transavia':'TVF','southwestairlines':'SWA','sunexpress':'SXS',
     'royalairmaroc':'RAM','aerlingus':'EIN','finnair':'FIN','airbaltic':'BTI',
     'tap':'TAP','vueling':'VLG','norwegian':'NAX','emirates':'UAE',
     'fedex':'FDX','ups':'UPS','sas':'SAS','scandinavianairlines':'SAS',
     'etihadairways':'ETD','etihad':'ETD','airasia':'AXM','airasiax':'XAX',
     'airalgerie':'DAH','aircorsica':'CCM','aircaraibes':'FWI','volotea':'VOE',
     'britishairways':'BAW','horizon':'QXE','hop':'HOP'}
    return aliases.get(n,'---')

def rows_from_paths(paths):
    results=[]
    for rel in paths:
        if not is_model_xml(rel):continue
        typ=aircraft_type(rel.parts[0],rel.stem)
        if not typ:continue
        results.append((typ,airline_for(rel),'Aircraft/'+rel.as_posix()))
    return sorted(set(results),key=lambda r:(r[0],r[1]=='---',r[1],r[2]))

def paths_from_listing(source):
    folder=None;paths=[]
    for line in source.read_text(errors='replace').splitlines():
        m=re.fullmatch(r'\./([^/]+):',line.strip())
        if m:
            folder=m.group(1)
            continue
        if line.strip().startswith('./') and line.strip().endswith(':'):
            folder=None;continue
        if not folder:continue
        for file in re.split(r'\s{2,}|\t+',line.strip()):
            file=file.strip()
            if file.lower().endswith('.xml') and '/' not in file:
                paths.append(Path(folder)/file)
    return paths

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--inventory',type=Path,help='Saved ls -R output instead of live directory')
    ap.add_argument('--fg-root',type=Path,help='FGRoot choisi dans le lanceur (requis sans --inventory)')
    ap.add_argument('--output',default='fg_models.tsv');args=ap.parse_args()
    root=args.fg_root
    if not args.inventory and root is None:
        ap.error('--fg-root est obligatoire sans --inventory')
    base=root/'AI/Aircraft' if root is not None else None
    if args.inventory:
        if not args.inventory.is_file():sys.exit('Inventory file does not exist')
        paths=paths_from_listing(args.inventory)
    elif base is not None and base.is_dir():
        paths=(p.relative_to(base) for p in base.rglob('*.xml'))
    else:
        sys.exit(f'ERROR: no AI/Aircraft at {base}; provide --inventory')
    rows=rows_from_paths(paths)
    if not rows:sys.exit('No model XML found')
    Path(args.output).write_text(''.join('\t'.join(row)+'\n' for row in rows))
    print(f'[FG MODEL] indexed {len(rows)} AI variants ({len(set(r[0] for r in rows))} types) -> {args.output}')
if __name__=='__main__':main()
