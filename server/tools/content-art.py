# Draws server/content-defaults/logo.png and background.jpg (original art).
#   python3 tools/content-art.py   (needs Pillow; run from server/)
import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'content-defaults'))
import math, random
from PIL import Image, ImageDraw, ImageFilter, ImageFont
GOLD=(212,175,55); GOLD2=(255,214,102); DARK=(20,20,24)
S=4
# ---- logo 512 ----
W=512*S
im=Image.new('RGBA',(W,W),(0,0,0,0)); d=ImageDraw.Draw(im)
c=W//2
d.ellipse([8*S,8*S,W-8*S,W-8*S],fill=GOLD)
d.ellipse([22*S,22*S,W-22*S,W-22*S],fill=DARK)
d.ellipse([34*S,34*S,W-34*S,W-34*S],outline=(212,175,55,120),width=2*S)
# crown
cy=180*S; base_y=280*S; left=146*S; right=W-146*S
pts=[(left,base_y),(left-6*S,cy-10*S),(left+60*S,cy+50*S),(c,cy-60*S),(right-60*S,cy+50*S),(right+6*S,cy-10*S),(right,base_y)]
d.polygon(pts,fill=GOLD2)
for (x,y) in [(left-6*S,cy-10*S),(c,cy-60*S),(right+6*S,cy-10*S)]:
    r=16*S; d.ellipse([x-r,y-r,x+r,y+r],fill=GOLD2)
d.rectangle([left,base_y+14*S,right,base_y+44*S],fill=GOLD2)
r=13*S; d.ellipse([c-r,230*S-r,c+r,230*S+r],fill=DARK)
f=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf',40*S)
t="KINGMAKERS"; tw=d.textlength(t,font=f)
d.text((c-tw/2,350*S),t,font=f,fill=GOLD2)
im=im.resize((512,512),Image.LANCZOS); im.save('logo.png',optimize=True)
# ---- background 1920x1080 ----
BW,BH=1920,1080
bg=Image.new('RGB',(BW,BH))
px=bg.load()
for y in range(BH):
    t=y/BH
    # night sky to warm horizon glow
    top=(10,10,16); hor=(70,52,22)
    k=t**2.2
    col=tuple(int(top[i]+(hor[i]-top[i])*k) for i in range(3))
    for x in range(BW): px[x,y]=col
d=ImageDraw.Draw(bg)
random.seed(7)
for _ in range(160):
    x=random.randrange(BW); y=random.randrange(int(BH*0.5)); b=random.randrange(90,200)
    d.point((x,y),fill=(b,b,b))
# glow sun
glow=Image.new('L',(BW,BH),0); g=ImageDraw.Draw(glow)
g.ellipse([BW//2-520,BH-520,BW//2+520,BH+520],fill=150)
glow=glow.filter(ImageFilter.GaussianBlur(160))
bg=Image.composite(Image.new('RGB',(BW,BH),(150,110,40)),bg,glow)
d=ImageDraw.Draw(bg)
# skyline: two layers
def skyline(seed,base,minh,maxh,col,winp):
    random.seed(seed); x=-20
    while x<BW:
        w=random.randrange(50,140); h=random.randrange(minh,maxh)
        d.rectangle([x,base-h,x+w,BH],fill=col)
        if random.random()<0.25: d.rectangle([x+w//2-3,base-h-40,x+w//2+3,base-h],fill=col)
        for wy in range(base-h+12,BH-10,22):
            for wx in range(x+8,x+w-10,18):
                if random.random()<winp: d.rectangle([wx,wy,wx+6,wy+9],fill=(230,190,90))
        x+=w+random.randrange(0,12)
skyline(3,BH-120,180,420,(28,24,20),0.0)
skyline(11,BH,160,520,(12,12,14),0.12)
# vignette
v=Image.new('L',(BW,BH),0); vd=ImageDraw.Draw(v)
vd.ellipse([-300,-250,BW+300,BH+250],fill=255)
v=v.filter(ImageFilter.GaussianBlur(200))
bg=Image.composite(bg,Image.new('RGB',(BW,BH),(0,0,0)),v)
bg.save('background.jpg',quality=86,optimize=True,progressive=True)
