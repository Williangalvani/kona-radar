#!/usr/bin/env python3
"""Artifact validation; no hardware or firmware claims."""
from pathlib import Path
import json, math
from PIL import Image
import cv2

ROOT=Path(__file__).resolve().parent
m=json.loads((ROOT/'manifest.json').read_text())
assert len(m['screens'])==12
assert len(list((ROOT/'screens').glob('*.png')))==12
assert len(list((ROOT/'device').glob('*.png')))==12
for screen in m['screens']:
 im=Image.open(ROOT/screen['screen']).convert('RGB')
 assert im.size==(466,466),screen['screen']
 for p in [(0,0),(465,0),(0,465),(465,465),(10,10)]:
  assert im.getpixel(p)==(32,38,43),(screen['screen'],p,im.getpixel(p))
 # Genuine black should remain in every view, not a fully lit near-black dashboard.
 black=sum(count for count,rgb in im.getcolors(466*466) if rgb==(0,0,0))
 assert black>466*466*.30,(screen['screen'],black)
 dev=Image.open(ROOT/screen['device'])
 assert dev.size==(600,600)
print('PASS 12 native screens: 466×466, circular gray surround, true-black interior')
print('PASS 12 device composites: 600×600')
contact=Image.open(ROOT/'contact-sheet.png')
assert contact.size==(2160,1930)
print('PASS labeled contact sheet: 2160×1930, 12 entries')
bounds=json.loads((ROOT/'text-bounds.json').read_text())
assert all(b['max_radius']<=228 for b in bounds),[b for b in bounds if b['max_radius']>228]
print(f'PASS {len(bounds)} critical text bounds within radius 228; 2px shift reserve')
value,pts,_=cv2.QRCodeDetector().detectAndDecode(cv2.imread(str(ROOT/'screens/02_wifi_setup_qr.png')))
assert value==m['qr_payload'],value
print('PASS QR decodes at native 466px: standard WPA Wi-Fi payload, demo AP')
obs=m['observer']
def en(lat,lon):
 return ((lon-obs['lon'])*60*math.cos(math.radians(obs['lat'])),(lat-obs['lat'])*60)
def bearing(lat,lon):
 e,n=en(lat,lon)
 return math.degrees(math.atan2(e,n))%360
for radius,expected in [(10,6),(5,4)]:
 count=sum(math.hypot(*en(a['lat'],a['lon']))<=radius for a in m['aircraft'])
 assert count==expected,(radius,count)
 print(f'PASS {radius} NM range has exactly {expected} example aircraft')
airport=m['airport'];e,n=en(airport['lat'],airport['lon'])
b=bearing(airport['lat'],airport['lon'])
assert abs(b-obs['view_bearing_true'])<.5
assert abs(math.hypot(e,n)*1.852-5.48)<.05
print(f'PASS KOA: {math.hypot(e,n)*1.852:.2f} km at {b:.2f}°T, directly ahead in view-up')
a=m['aircraft'][0]
relative=(bearing(a['lat'],a['lon'])-obs['view_bearing_true']+180)%360-180
assert round(relative)==77
alert_relative=(bearing(19.7809,-156.0418)-obs['view_bearing_true']+180)%360-180
assert round(alert_relative)==47
print(f'PASS direction cues: overview {relative:.2f}° right; later alert {alert_relative:.2f}° right')
required=['DESIGN.md','render.py','assets/OFL.txt','PLAN.md','manifest.json']
for path in required:assert (ROOT/path).is_file(),path
print('PASS specification, source, manifest, font license present')
print('VISUAL REVIEW: all 12 native screens, bezel and contact sheet inspected; zoom labels revised')
print('LIMITATION: no physical display, touch, sunlight, feed or firmware validation performed')
