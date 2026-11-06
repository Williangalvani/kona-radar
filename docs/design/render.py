#!/usr/bin/env python3
"""Kona Radar / balcony instrument concept. Render at 3x; no live network data."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from math import sin, cos, radians, atan2, degrees, hypot
import json
import qrcode

ROOT=Path(__file__).resolve().parent
S=3
W=H=466
P={
 'black':'#000000','outside':'#20262B','white':'#F2F4EB',
 'muted':'#97A7A9','quiet':'#64797C','line':'#23383C',
 'panel':'#0C171B','panel_line':'#294148','cyan':'#80E1E8',
 'cyan_dim':'#28535B','amber':'#FFC271','green':'#B8E3AD',
 'red':'#FF896D','coast':'#294045','land':'#000000',
 'night_text':'#A66C45','night_accent':'#D1945D',
}
F={
 'regular':'PlexCondensed-Regular.ttf', 'medium':'PlexCondensed-Medium.ttf',
 'bold':'PlexCondensed-SemiBold.ttf','mono':'PlexMono-Medium.ttf',
 'body':'PlexSans-Medium.ttf'
}
CACHE={}
MANIFEST=[]
BOUNDS=[]
# Placeholder observer (not a real balcony): about 3 NM east of KOA, facing it.
# The firmware gets the real one from the setup page or include/config.local.h.
OBS=(19.7470,-155.9940)
KOA=(19.738765,-156.045631)
R17=(19.75385525,-156.04428864)
R35=(19.72367472,-156.04697358)
VIEW=260
AIRCRAFT=[
 {'id':'HAL208','operator':'Hawaiian','type':'B712','lat':19.840,'lon':-156.036,'alt':1900,'gs':132,'track':185,'vr':-600,'squawk':'3142','state':'approach'},
 {'id':'SWA250','operator':'Southwest','type':'B38M','lat':19.786,'lon':-156.043,'alt':2300,'gs':155,'track':5,'vr':1400,'squawk':'4271','state':'departing'},
 {'id':'ASA875','operator':'Alaska','type':'B738','lat':19.6748,'lon':-156.1098,'alt':4400,'gs':210,'track':34,'vr':-700,'squawk':'2143','state':'traffic'},
 {'id':'HAL123','operator':'Hawaiian','type':'B712','lat':19.6714,'lon':-156.0302,'alt':3600,'gs':178,'track':344,'vr':-500,'squawk':'3254','state':'traffic'},
 {'id':'MHO507','operator':'Mokulele','type':'C208','lat':19.7886,'lon':-155.9698,'alt':2800,'gs':110,'track':189,'vr':0,'squawk':'1200','state':'traffic'},
 {'id':'N123BH','operator':'Tour helicopter','type':'EC30','lat':19.7151,'lon':-156.0191,'alt':900,'gs':85,'track':319,'vr':0,'squawk':'1200','state':'helicopter'},
]
QR_PAYLOAD='WIFI:T:WPA;S:Kona-Radar-7A2C;P:look-up-7412;;'

def c(v): return P.get(v,v)
def font(size,weight='regular'):
 key=(size,weight)
 if key not in CACHE: CACHE[key]=ImageFont.truetype(str(ROOT/'assets'/F[weight]),round(size*S))
 return CACHE[key]
def pt(p):return tuple(round(v*S) for v in p)
def box(p):return tuple(round(v*S) for v in p)

class Canvas:
 def __init__(self,name):
  self.name=name;self.im=Image.new('RGB',(W*S,H*S),c('black'));self.d=ImageDraw.Draw(self.im)
 def text(self,x,y,s,size=18,color='white',weight='regular',align='center',tracking=0,check=True):
  f=font(size,weight)
  widths=[self.d.textlength(ch,font=f)/S for ch in s]
  width=(sum(widths)+tracking*(len(s)-1)) if tracking else self.d.textlength(s,font=f)/S
  if align=='center': x-=width/2
  elif align=='right': x-=width
  if tracking:
   xx=x
   for ch,ww in zip(s,widths):self.d.text(pt((xx,y)),ch,font=f,fill=c(color),anchor='lt');xx+=ww+tracking
  else:self.d.text(pt((x,y)),s,font=f,fill=c(color),anchor='lt')
  bb=self.d.textbbox(pt((x,y)),s,font=f,anchor='lt')
  # Text bbox is used to validate circular safe area, excluding intentionally clipped map labels.
  if check:
   corners=[(x,y),(x+width,y),(x,y+(bb[3]-bb[1])/S),(x+width,y+(bb[3]-bb[1])/S)]
   far=max(hypot(a-233,b-233) for a,b in corners)
   BOUNDS.append({'screen':self.name,'text':s,'max_radius':round(far,2),'bounds':[round(x,1),y,round(width,1),round((bb[3]-bb[1])/S,1)]})
  return width
 def line(self,points,color='line',width=1):
  self.d.line([pt(p) for p in points],fill=c(color),width=max(1,round(width*S)),joint='curve')
 def ellipse(self,b,fill=None,outline=None,width=1):
  self.d.ellipse(box(b),fill=c(fill) if fill else None,outline=c(outline) if outline else None,width=max(1,round(width*S)))
 def rect(self,b,fill=None,outline=None,width=1,r=0):
  if r:self.d.rounded_rectangle(box(b),round(r*S),fill=c(fill) if fill else None,outline=c(outline) if outline else None,width=max(1,round(width*S)))
  else:self.d.rectangle(box(b),fill=c(fill) if fill else None,outline=c(outline) if outline else None,width=max(1,round(width*S)))
 def poly(self,p,fill,outline=None,width=1):
  self.d.polygon([pt(a) for a in p],fill=c(fill))
  if outline:self.line(p+[p[0]],outline,width)
 def arc(self,center,r,start,end,color='cyan',width=2):
  self.d.arc(box((center[0]-r,center[1]-r,center[0]+r,center[1]+r)),start,end,fill=c(color),width=round(width*S))
 def dashed(self,p1,p2,color='line',width=1,dash=5,gap=5):
  dx,dy=p2[0]-p1[0],p2[1]-p1[1];length=hypot(dx,dy)
  if length==0:return
  start=0
  while start<length:
   end=min(length,start+dash)
   self.line([(p1[0]+dx*start/length,p1[1]+dy*start/length),(p1[0]+dx*end/length,p1[1]+dy*end/length)],color,width)
   start+=dash+gap
 def tag(self,x,y,text,color='cyan',size=12):
  w=self.d.textlength(text,font=font(size,'mono'))/S+20
  self.rect((x-w/2,y,x+w/2,y+24),'#100B07' if color=='night_text' else 'panel','#472D1A' if color=='night_text' else 'panel_line',r=6)
  self.text(x,y+6,text,size,color,'mono')
 def button(self,y,text,color='cyan',width=228):
  self.rect((233-width/2,y,233+width/2,y+48),'panel',color,r=12)
  self.text(233,y+14,text,18,color,'bold',tracking=.4)
 def finish(self):
  mask=Image.new('L',(W*S,H*S),0);md=ImageDraw.Draw(mask)
  md.ellipse((S,S,(W-1)*S,(H-1)*S),fill=255)
  outside=Image.new('RGB',self.im.size,c('outside'));outside.paste(self.im,(0,0),mask)
  self.im=outside
  self.d=ImageDraw.Draw(self.im)
  self.d.ellipse(box((1,1,465,465)),outline='#304044',width=S)
  return self.im.resize((466,466),Image.Resampling.LANCZOS)

def arrow(canvas,x,y,angle,color='cyan',size=18,width=2):
 a=radians(angle)
 def xy(px,py):return (x+px*cos(a)-py*sin(a),y+px*sin(a)+py*cos(a))
 canvas.line([xy(0,size*.65),xy(0,-size*.65)],color,width)
 canvas.line([xy(-size*.4,-size*.18),xy(0,-size*.65),xy(size*.4,-size*.18)],color,width)

def plane(canvas,x,y,heading,color='cyan',size=18,selected=False):
 a=radians(heading-VIEW)
 # A solid, deliberately simple aircraft silhouette, suitable as a small LVGL vector asset.
 points=[(0,-.68),(.12,-.38),(.12,-.10),(.64,.16),(.64,.30),(.12,.14),(.10,.47),(.27,.62),(.27,.73),(0,.62),(-.27,.73),(-.27,.62),(-.10,.47),(-.12,.14),(-.64,.30),(-.64,.16),(-.12,-.10),(-.12,-.38)]
 q=[(x+(xx*cos(a)-yy*sin(a))*size,y+(xx*sin(a)+yy*cos(a))*size) for xx,yy in points]
 if selected:
  canvas.ellipse((x-19,y-19,x+19,y+19),None,color,1)
  canvas.ellipse((x-23,y-23,x+23,y+23),None,'cyan_dim',.6)
 canvas.poly(q,color)
 # Tail history, not a fake rotating radar sweep.
 dx,dy=sin(a),-cos(a)
 for i in [1,2,3]:
  xx=x-dx*(15+i*7);yy=y-dy*(15+i*7)
  canvas.ellipse((xx-1,yy-1,xx+1,yy+1),'cyan_dim' if color!='night_accent' else '#4E2D1E')

def helicopter(canvas,x,y,color='cyan'):
 canvas.ellipse((x-5,y-7,x+5,y+6),None,color,1.5)
 canvas.line([(x-10,y-2),(x+10,y-2)],color,1.5)
 canvas.line([(x,y+6),(x,y+14),(x+4,y+14)],color,1.5)

def wifi(canvas,x,y,color='cyan',size=42):
 for r in [size,size*.67,size*.34]:canvas.arc((x,y),r,220,320,color,2.4)
 canvas.ellipse((x-3,y-3,x+3,y+3),color)

def checkmark(canvas,x,y,color='green',size=30):canvas.line([(x-size*.6,y),(x-size*.18,y+size*.42),(x+size*.68,y-size*.52)],color,4)

def footer(canvas,text='EXAMPLE DATA',color='quiet'):
 canvas.text(233,429,text,11,color,'mono',tracking=.6)

def top(canvas,title,subtitle=None):
 canvas.line([(207,27),(259,27)],'cyan',2)
 canvas.text(233,48,title,16,'muted','bold',tracking=1.8)
 if subtitle:canvas.text(233,78,subtitle,28,'white','bold')

def local(lat,lon):
 return ((lon-OBS[1])*60*cos(radians(OBS[0])),(lat-OBS[0])*60)
def projection(lat,lon,rng=10):
 e,n=local(lat,lon);a=radians(VIEW);scale=166/rng
 return 233+(e*cos(a)-n*sin(a))*scale,252+(-e*sin(a)-n*cos(a))*scale

def polar(lat,lon):
 e,n=local(lat,lon)
 return hypot(e,n),degrees(atan2(e,n))%360
def rel(bearing):
 return (bearing-VIEW+180)%360-180
def side(r):
 return f'RIGHT {round(abs(r))}°' if r>=0 else f'LEFT {round(abs(r))}°'
def compass(b):
 return ['N','NE','E','SE','S','SW','W','NW'][round(b/45)%8]
# HAL208 later on final for 17 (alert example): 1.6 NM out, 650 ft, 120 kt.
ALERT_POS=(19.7809,-156.0418)
HAL208_REL=rel(polar(19.840,-156.036)[1])

def observer(canvas,x,y,night=False,label=True):
 col='night_accent' if night else 'white'
 canvas.poly([(x,y-13),(x-16,y-36),(x+16,y-36)],'#1D1A12' if night else '#102A30')
 canvas.ellipse((x-8,y-8,x+8,y+8),'black',col,1.4)
 canvas.ellipse((x-3,y-3,x+3,y+3),col)
 if label:canvas.text(x,y+15,'YOU',11,col,'mono')

def map_base(canvas,rng=10,night=False,labels=True):
 # View-up observer-centered range geometry, north rotated to the right of screen.
 line='#291C14' if night else 'line';coast='#342419' if night else 'coast'
 # Schematic west coast, transformed from geographic sample points; not navigation cartography.
 coast_geo=[(19.92,-155.91),(19.87,-155.96),(19.82,-156.04),(19.77,-156.065),(19.73,-156.067),(19.68,-156.045),(19.64,-156.005),(19.59,-155.97)]
 coast_pts=[projection(lat,lon,rng) for lat,lon in coast_geo]
 # Land and ocean stay truly black: coastline supplies context without a static lit land mass.
 canvas.poly(coast_pts+[(0,466),(466,466)],'black')
 canvas.line(coast_pts,coast,1.2)
 for r in [166/3,166*2/3,166]:canvas.ellipse((233-r,252-r,233+r,252+r),None,line,1)
 canvas.dashed((233,87),(233,329),line,.7,3,8)
 canvas.dashed((68,252),(398,252),line,.7,3,8)
 # Perimeter ticks: no continuous sweep, no false radar sensor implication.
 for bearing in range(0,360,15):
  a=radians(bearing-VIEW);r1=161 if bearing%45 else 156
  canvas.line([(233+sin(a)*r1,252-cos(a)*r1),(233+sin(a)*166,252-cos(a)*166)],line,1)
 # True-north label rotates with the map while all text remains upright.
 na=radians(-VIEW)
 canvas.text(233+sin(na)*182,252-cos(na)*182-7,'N',13,'night_text' if night else 'muted','mono')
 canvas.poly([(233,90),(229,98),(237,98)],'night_text' if night else 'cyan')
 # Draw the actual 17/35 true alignment from the FAA threshold coordinates.
 p17=projection(*R17,rng);p35=projection(*R35,rng)
 canvas.line([p35,p17],'night_text' if night else 'white',5 if rng==5 else 4)
 canvas.dashed(p35,p17,'black',1,2,3)
 ax,ay=projection(*KOA,rng)
 if labels:
  canvas.text(ax,ay-27,'KOA',14,'night_text' if night else 'white','bold')
  canvas.text(p17[0]+14,p17[1]-5,'17',10,'night_text' if night else 'muted','mono',check=False)
  canvas.text(p35[0]-14,p35[1]-2,'35',10,'night_text' if night else 'muted','mono',check=False)
 observer(canvas,233,252,night)
 return p17,p35

def radar_header(canvas,rng=10,count=6,night=False):
 col='night_text' if night else 'white'
 canvas.text(233,41,'KOA',26,col,'bold',tracking=2)
 canvas.text(233,73,f'VIEW {VIEW}°T · {rng:02d} NM',13,'night_text' if night else 'cyan','mono')
 canvas.tag(92,115,f'{count} ACFT','night_text' if night else 'muted',11)

def radar_screen(name='06_radar_view_aligned',night=False,rng=10):
 cv=Canvas(name)
 p17,p35=map_base(cv,rng,night)
 # Main ring layout prioritizes readable selected traffic over labeling every dot.
 positions={
  'HAL208':(358,184),'SWA250':(276,186),'ASA875':(165,108),
  'HAL123':(118,233),'MHO507':(316,305),'N123BH':(165,264),
 }
 positions_zoom={
  'SWA250':(326,146),'HAL123':(100,184),'MHO507':(361,300),'N123BH':(145,232)
 }
 count=0
 selected='SWA250' if rng==5 else 'HAL208'
 for ac in AIRCRAFT:
  x,y=projection(ac['lat'],ac['lon'],rng)
  e,n=local(ac['lat'],ac['lon'])
  if hypot(e,n)>rng:continue
  count+=1
  col='night_accent' if night else ('amber' if ac['id']==selected else ('green' if ac['state']=='departing' else 'cyan'))
  if ac['id']=='HAL208':cv.dashed((x-20,y+2),p17,'#684E29' if not night else '#493020',1,4,4)
  if ac['state']=='helicopter':helicopter(cv,x,y,col)
  else:plane(cv,x,y,ac['track'],col,16,ac['id']==selected)
  lx,ly=(positions_zoom if rng==5 else positions)[ac['id']]
  cv.text(lx,ly,ac['id'],12,col,'mono')
  if ac['id']=='SWA250' and rng==10:arrow(cv,lx+31,ly+6,0,col,7,1)
 # Top and bottom opaque clear zones; no map ring or trail behind text.
 cv.rect((100,34,366,91),'black',r=8)
 radar_header(cv,rng,count,night)
 if rng==5:
  cv.text(374,176,'+2',13,'amber','mono')
  cv.text(374,194,'OUTSIDE',9,'muted','mono')
 # Compact bottom information dock; its upper edge doubles as a visual horizon.
 x1,y1,x2,y2=91,337,375,409
 cv.rect((x1,y1,x2,y2),'#100B07' if night else 'panel','#472D1A' if night else 'panel_line',r=15)
 if rng==5:
  cv.text(109,349,'DEPARTING',11,'green','mono',align='left',tracking=.6)
  cv.text(109,370,'SWA250',26,'white','bold',align='left')
  cv.text(354,350,'↑ 2,300 FT',16,'green','medium',align='right')
  cv.text(354,378,'155 KT',16,'muted','mono',align='right')
 else:
  cv.text(109,349,'APPROACHING KOA',11,'night_accent' if night else 'amber','mono',align='left',tracking=.3)
  cv.text(109,370,'~3 MIN',29,'night_accent' if night else 'white','bold',align='left')
  cv.text(354,350,'HAL208',16,'night_text' if night else 'muted','medium',align='right')
  cv.text(354,377,side(HAL208_REL),19,'night_accent' if night else 'amber','bold',align='right')
 footer(cv,'NIGHT · EXAMPLE DATA' if night else 'EXAMPLE DATA','night_text' if night else 'quiet')
 return cv


def splash():
 cv=Canvas('01_splash')
 # Quiet instrument ident, with runway and incoming aircraft as the product mark.
 cv.ellipse((164,82,302,220),None,'line',1)
 cv.arc((233,151),69,218,286,'cyan',2)
 for a in [-60,-30,0,30,60]:
  ar=radians(a)
  cv.line([(233+sin(ar)*61,151-cos(ar)*61),(233+sin(ar)*67,151-cos(ar)*67)],'cyan_dim',1)
 cv.line([(206,166),(260,158)],'white',5)
 cv.dashed((208,166),(258,158),'black',1,4,3)
 plane(cv,248,129,185,'amber',25)
 cv.text(233,252,'KONA',49,'white','bold',tracking=5)
 cv.text(233,308,'R A D A R',19,'cyan','medium')
 cv.text(233,357,'A window into the sky.',17,'muted')
 cv.rect((196,399,270,402),'line',r=1)
 cv.rect((196,399,229,402),'cyan',r=1)
 cv.text(233,421,'STARTING',10,'quiet','mono',tracking=1.4)
 return cv

def setup():
 cv=Canvas('02_wifi_setup_qr')
 top(cv,'FIRST FLIGHT')
 cv.text(233,79,'Set up with your phone',27,'white','bold')
 cv.text(233,113,'1  Scan to join setup Wi-Fi',17,'muted','medium')
 qr=qrcode.QRCode(version=None,error_correction=qrcode.constants.ERROR_CORRECT_M,box_size=1,border=4)
 qr.add_data(QR_PAYLOAD);qr.make(fit=True)
 mat=qr.get_matrix();modules=len(mat)
 # Integer 4px modules, unaltered quiet zone, no styling or logo across the code.
 size=modules*4
 image=Image.new('RGB',(size,size),'white');d=ImageDraw.Draw(image)
 for yy,row in enumerate(mat):
  for xx,on in enumerate(row):
   if on:d.rectangle((xx*4,yy*4,xx*4+3,yy*4+3),fill='black')
 x=(466-size)//2;y=145
 cv.im.paste(image.resize((size*S,size*S),Image.Resampling.NEAREST),pt((x,y)))
 cv.text(233,y+size+16,'2  Follow the sign-in page',17,'white','medium')
 cv.text(233,371,'Kona-Radar-7A2C',16,'cyan','mono')
 cv.text(233,395,'PASS  look-up-7412',13,'muted','mono')
 cv.text(233,420,'No page? 192.168.4.1',12,'quiet','mono')
 # No claim that BLE is required; QR joins the AP and the OS opens the portal.
 return cv

def connecting():
 cv=Canvas('03_wifi_connecting');top(cv,'WI-FI SETUP')
 cv.text(233,83,'Joining Wi-Fi',32,'white','bold')
 cv.text(233,127,'Balcony Wi-Fi',18,'muted','medium')
 cv.ellipse((166,173,300,307),None,'line',2)
 cv.arc((233,240),67,238,334,'cyan',3)
 wifi(cv,233,254,'cyan',35)
 cv.text(233,330,'Checking the connection…',19,'white','medium')
 cv.line([(151,380),(315,380)],'line',2)
 for x,fill in [(151,'green'),(233,'cyan'),(315,'line')]:cv.ellipse((x-4,376,x+4,384),fill)
 cv.text(151,396,'PHONE',10,'green','mono')
 cv.text(233,396,'WI-FI',10,'cyan','mono')
 cv.text(315,396,'ONLINE',10,'quiet','mono')
 footer(cv,'SETUP DEMO')
 return cv

def connected():
 cv=Canvas('04_wifi_connected');top(cv,'WI-FI SETUP')
 cv.text(233,83,'You’re connected',32,'white','bold')
 cv.ellipse((173,164,293,284),'#0B1610','#294635',1)
 checkmark(cv,233,225,'green',35)
 cv.text(233,309,'Balcony Wi-Fi',21,'green','medium')
 cv.text(233,340,'Your sky is ready.',18,'muted')
 cv.button(372,'OPEN RADAR','green',214)
 footer(cv,'SETUP DEMO')
 return cv

def failed():
 cv=Canvas('05_wifi_failed');top(cv,'WI-FI SETUP')
 cv.text(233,83,'Couldn’t connect',32,'white','bold')
 cv.ellipse((181,149,285,253),'#1B0D08','#593024',1)
 cv.line([(233,177),(233,211)],'red',4)
 cv.ellipse((230,223,236,229),'red')
 cv.text(233,278,'Check the Wi-Fi name',19,'muted')
 cv.text(233,304,'or password on your phone.',19,'muted')
 cv.button(345,'TRY AGAIN','red',218)
 cv.text(233,409,'Hold 5s to reset Wi-Fi',14,'muted','medium')
 footer(cv,'SETUP DEMO')
 return cv

def detail():
 cv=Canvas('08_aircraft_detail')
 top(cv,'AIRCRAFT')
 cv.line([(113,70),(104,79),(113,88)],'muted',2)
 cv.text(233,83,'HAL208',46,'white','bold',tracking=.5)
 cv.text(233,139,'Hawaiian  ·  Boeing 717',18,'muted','medium')
 cv.tag(233,171,'LIKELY ARRIVAL · RWY 17','amber',11)
 cv.line([(89,221),(377,221)],'line',1)
 # Two intentionally generous columns, the important glance values are not tiny telemetry.
 for x,label,value,unit in [(150,'ALTITUDE','1,900','FT'),(318,'GROUND SPEED','132','KT')]:
  cv.text(x,237,label,11,'muted','mono',tracking=.6)
  cv.text(x,261,value,36,'white','bold')
  cv.text(x,303,unit,12,'quiet','mono')
 cv.line([(233,239),(233,316)],'line',1)
 cv.text(150,335,'↓ 600 FT/MIN',16,'muted','medium')
 cv.text(318,335,'185° TRUE',16,'muted','medium')
 cv.rect((100,371,366,414),'panel','panel_line',r=11)
 cv.text(233,383,side(HAL208_REL)+'  ·  ~3 MIN',21,'amber','bold')
 footer(cv,'EXAMPLE · SQUAWK 3142')
 return cv

def alert():
 cv=Canvas('09_look_up_alert')
 _,brg=polar(*ALERT_POS);r=rel(brg)
 cv.arc((233,233),219,270+r-18,270+r+18,'amber',4)
 cv.text(233,51,'LIKELY LANDING · RWY 17',12,'amber','mono')
 cv.text(233,93,'LOOK RIGHT' if r>=0 else 'LOOK LEFT',45,'white','bold')
 cv.text(233,148,f'{round(abs(r))}° from your balcony view',18,'muted','medium')
 # Arrow is always relative to balcony heading, not an unlabeled compass north.
 cx,cy=233,248
 cv.ellipse((cx-60,cy-60,cx+60,cy+60),None,'line',1)
 cv.arc((cx,cy),60,270+min(0,r),270+max(0,r),'amber',2)
 cv.line([(cx,cy-69),(cx,cy-60)],'quiet',1)
 arrow(cv,cx+4,cy-4,r,'amber',65,5)
 cv.ellipse((cx-4,cy-4,cx+4,cy+4),'white')
 cv.text(233,325,'~1 MIN',43,'amber','bold')
 cv.text(233,378,f'HAL208  ·  {compass(brg)} {round(brg)}°T',20,'white','medium')
 cv.text(233,409,'650 FT  ·  120 KT',14,'muted','mono')
 footer(cv,'EXAMPLE · ESTIMATE ONLY')
 return cv

def idle():
 cv=Canvas('11_idle_dim')
 cv.text(233,126,'KOA',20,'#354044','bold',tracking=2)
 cv.text(233,184,'06',66,'#465354','bold')
 cv.text(233,266,'aircraft nearby',18,'#3C484A','medium')
 cv.ellipse((160,324,165,329),'#634D32')
 cv.text(246,319,'Next arrival ~3 min',15,'#5E5140','medium')
 cv.line([(207,371),(259,371)],'#202D30',1)
 cv.text(233,389,'Tap to wake',15,'#3C484A','medium')
 footer(cv,'EXAMPLE · DIM IDLE','#2C3638')
 return cv

def orientation():
 cv=Canvas('12_balcony_alignment')
 top(cv,'BALCONY ALIGNMENT')
 cv.text(233,85,'Face the airport',29,'white','bold')
 cv.text(233,127,f'{VIEW}°',53,'cyan','bold')
 cv.text(233,188,'TRUE BEARING · VIEW UP',12,'muted','mono')
 # A tiny orientation diagram, not a claim to know the user's current device heading.
 cv.poly([(233,279),(198,232),(268,232)],'#102228')
 cv.dashed((233,277),(233,227),'cyan_dim',1,4,4)
 ra=radians(5-VIEW);hx,hy=sin(ra)*24.3,-cos(ra)*24.3
 cv.line([(233-hx,224.5-hy),(233+hx,224.5+hy)],'white',4)
 cv.text(233,210,'KOA',11,'muted','bold')
 observer(cv,233,283,False,False)
 nm,_=polar(*KOA)
 cv.text(233,305,f'{nm:.1f} NM / {nm*1.852:.1f} KM AHEAD',12,'muted','mono')
 cv.text(233,337,f'{OBS[0]:.4f}, {OBS[1]:.4f}  (EXAMPLE)',13,'quiet','mono')
 cv.rect((110,365,356,410),'panel','panel_line',r=12)
 cv.text(160,381,'NORTH UP',14,'muted','bold')
 cv.rect((233,370,351,405),'cyan_dim',r=8)
 cv.text(292,381,'VIEW UP',14,'cyan','bold')
 footer(cv,'EDIT ON PHONE · CONFIGURABLE')
 return cv

def device(im):
 # Simple round graphite enclosure; nothing obscures the real 466px circular screen.
 size=600;sc=2
 bg=Image.new('RGB',(size*sc,size*sc),'#12181C')
 d=ImageDraw.Draw(bg)
 # Low-key radial background and broad surface shadow.
 shadow=Image.new('RGBA',bg.size,(0,0,0,0));sd=ImageDraw.Draw(shadow)
 sd.ellipse((37*sc,49*sc,563*sc,579*sc),fill=(0,0,0,150));shadow=shadow.filter(ImageFilter.GaussianBlur(13*sc))
 bg=Image.alpha_composite(bg.convert('RGBA'),shadow).convert('RGB');d=ImageDraw.Draw(bg)
 for r in range(257,232,-1):
  t=(257-r)/25
  value=int(34+13*sin(t*3.14))
  if r>=255:value=65-(257-r)*6
  d.ellipse(((300-r)*sc,(300-r)*sc,(300+r)*sc,(300+r)*sc),fill=(value,value+5,value+7))
 d.ellipse((53*sc,53*sc,547*sc,547*sc),outline='#172024',width=2*sc)
 d.arc((44*sc,44*sc,556*sc,556*sc),208,286,fill='#697476',width=sc)
 d.ellipse((64*sc,64*sc,536*sc,536*sc),fill='#000000',outline='#090C0F',width=3*sc)
 screen=im.resize((466*sc,466*sc),Image.Resampling.LANCZOS)
 mask=Image.new('L',(466*sc,466*sc),0);md=ImageDraw.Draw(mask);md.ellipse((sc,sc,465*sc,465*sc),fill=255)
 bg.paste(screen,(67*sc,67*sc),mask)
 return bg.resize((600,600),Image.Resampling.LANCZOS)

SCREENS=[
 ('01_splash','Power on','Runway-inspired ident',splash),
 ('02_wifi_setup_qr','01 / Phone setup','Scan, join, configure',setup),
 ('03_wifi_connecting','02 / Connecting','One clear progress state',connecting),
 ('04_wifi_connected','03 / Connected','Ready for the sky',connected),
 ('05_wifi_failed','04 / Connection failed','Retry + 5-second reset',failed),
 ('06_radar_view_aligned','View-aligned radar','10 NM · six example aircraft',lambda:radar_screen()),
 ('07_radar_zoom_5nm','Closer to the runway','5 NM · departure selected',lambda:radar_screen('07_radar_zoom_5nm',rng=5)),
 ('08_aircraft_detail','Aircraft detail','Tap a target to inspect',detail),
 ('09_look_up_alert','Look up','Direction first, data second',alert),
 ('10_night_mode','Night mode','Warm, low-luminance palette',lambda:radar_screen('10_night_mode',night=True)),
 ('11_idle_dim','Quiet idle','Dim + shift, tap to wake',idle),
 ('12_balcony_alignment','Your balcony view','Configurable observer + bearing',orientation),
]

def contact_sheet(images):
 # 4 x 3, enough native resolution to inspect all screens by zooming.
 cw,ch=540,570
 sheet=Image.new('RGB',(2160,1930),'#11181C');d=ImageDraw.Draw(sheet)
 def text(x,y,s,size=18,color='#F2F4EB',weight='regular'):
  f=ImageFont.truetype(str(ROOT/'assets'/F[weight]),size)
  d.text((x,y),s,font=f,fill=color,anchor='lt')
 text(56,43,'KONA RADAR',43,weight='bold')
 text(56,99,'THE BALCONY INSTRUMENT',15,'#80E1E8','mono')
 text(1172,49,'466 × 466 AMOLED  /  UI CONCEPT 01',17,'#97A7A9','mono')
 text(1172,83,f'Observer-centered · view up {VIEW}° true · Kona, Hawaiʻi',20,'#F2F4EB')
 d.line((56,142,2104,142),fill='#2A3A40',width=1)
 for i,(key,title,sub,fn) in enumerate(SCREENS):
  col,row=i%4,i//4;x=col*cw;y=166+row*ch
  dev=images[key][1].resize((490,490),Image.Resampling.LANCZOS)
  sheet.paste(dev,(x+25,y))
  text(x+42,y+491,f'{i+1:02d}',14,'#80E1E8','mono')
  text(x+78,y+488,title,24,'#F2F4EB','bold')
  text(x+78,y+521,sub,17,'#97A7A9')
 text(56,1892,'ALL TRAFFIC IS SIMULATED · QR IS A DEMO SETUP NETWORK · NOT FOR NAVIGATION',12,'#71878B','mono')
 text(1780,1892,'DESIGNED FOR LOOKING UP',12,'#80E1E8','mono')
 return sheet

def main():
 images={}
 for name,title,sub,fn in SCREENS:
  cv=fn();im=cv.finish();dev=device(im)
  im.save(ROOT/'screens'/f'{name}.png');dev.save(ROOT/'device'/f'{name}_device.png')
  images[name]=(im,dev)
  MANIFEST.append({'id':name,'title':title,'screen':f'screens/{name}.png','device':f'device/{name}_device.png','screen_size':[466,466],'device_size':[600,600]})
 sheet=contact_sheet(images);sheet.save(ROOT/'contact-sheet.png')
 (ROOT/'manifest.json').write_text(json.dumps({'screens':MANIFEST,'observer':{'lat':OBS[0],'lon':OBS[1],'view_bearing_true':VIEW},'airport':{'lat':KOA[0],'lon':KOA[1]},'aircraft':AIRCRAFT,'qr_payload':QR_PAYLOAD},indent=2))
 (ROOT/'text-bounds.json').write_text(json.dumps(BOUNDS,indent=2))
 issues=[x for x in BOUNDS if x['max_radius']>228]
 print(f'Rendered {len(images)} screens, {len(images)} device renders, contact sheet {sheet.size}.')
 print('Critical text outside 228px safe circle:',len(issues))
 for issue in issues:print(issue)

if __name__=='__main__':main()
