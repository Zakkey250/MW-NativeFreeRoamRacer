"""Original vector-like two-racer glyph; deterministic single-image MW TPK.
No input artwork/game textures. DXT3 constant RGB + explicit alpha, one mip.
TPK layout checked against the installed MW version-5 UI texture packs.
"""
from pathlib import Path
import struct, hashlib, json
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
NAME='NFR_ENCOUNTER_READY'
PACK='NFR_PROMPT'
def bh(s):
    h=0xffffffff
    for c in s.encode('ascii'): h=(h*33+c)&0xffffffff
    return h
def chunk(i,p): return struct.pack('<II',i,len(p))+p
def u32(b,p,v): struct.pack_into('<I',b,p,v)
def text(b,p,s): b[p:p+len(s)]=s.encode('ascii')

def build():
    scale=4
    mask=Image.new('L',(128*scale,128*scale))
    d=ImageDraw.Draw(mask)
    def poly(points,fill=255):d.polygon([(x*scale,y*scale) for x,y in points],fill=fill)
    def car(x,y):
        # Top-down racer silhouette with clear windshield/rear glass cutouts.
        poly([(x+5,y),(x+25,y),(x+29,y+9),(x+29,y+58),(x+25,y+65),(x+5,y+65),(x+1,y+58),(x+1,y+9)])
        poly([(x+6,y+14),(x+24,y+14),(x+22,y+25),(x+8,y+25)],0)
        poly([(x+8,y+46),(x+22,y+46),(x+24,y+53),(x+6,y+53)],0)
        for yy in (10,45):
            poly([(x-2,y+yy),(x+1,y+yy),(x+1,y+yy+12),(x-2,y+yy+12)])
            poly([(x+29,y+yy),(x+32,y+yy),(x+32,y+yy+12),(x+29,y+yy+12)])
    car(28,42);car(71,20)
    # Two forward arrows separate the icon from a parked-car symbol.
    for x,y in ((36,23),(79,1)):
        poly([(x,y+8),(x+7,y),(x+14,y+8),(x+10,y+8),(x+10,y+14),(x+4,y+14),(x+4,y+8)])
    mask=mask.resize((128,128),Image.Resampling.LANCZOS)
    # Native event icons are warm gold. Uniform RGB makes DXT3 deterministic.
    rgb=(239,193,103); color=((rgb[0]>>3)<<11)|((rgb[1]>>2)<<5)|(rgb[2]>>3)
    pixels=bytearray()
    decoded=Image.new('RGBA',(128,128))
    for by in range(0,128,4):
        for bx in range(0,128,4):
            a=0
            for y in range(4):
                for x in range(4):
                    value=(mask.getpixel((bx+x,by+y))+8)//17
                    a|=value<<(4*(y*4+x))
                    decoded.putpixel((bx+x,by+y),((color>>11)*255//31,((color>>5)&63)*255//63,(color&31)*255//31,value*17))
            pixels+=struct.pack('<QHHI',a,color,color,0)
    info=bytearray(124);u32(info,0,5);text(info,4,PACK);text(info,32,'NativeFreeRoamRacers\\Prompt.tpk');u32(info,96,bh(PACK))
    header=bytearray(124);text(header,12,NAME);u32(header,36,bh(NAME));u32(header,40,0x001a93cf)
    u32(header,56,len(pixels));u32(header,64,128*128);struct.pack_into('<HH',header,68,128,128)
    header[72:76]=bytes((7,7,0x24,0));header[78]=1;header[81]=5;header[85]=2;header[86]=1;header[89]=1
    struct.pack_into('<HH',header,100,256,256)
    comp=bytearray(32);struct.pack_into('<III',comp,8,1,5,6);comp[20:24]=b'DXT3'
    part1=chunk(0x33310001,info)+chunk(0x33310002,struct.pack('<II',bh(NAME),0))+chunk(0x33310004,header)+chunk(0x33310005,comp)
    body=chunk(0,bytes(48))+chunk(0xb3310000,part1)
    pad=(- (8+len(body)))%128
    if pad:body+=chunk(0,bytes(pad-8))
    datainfo=bytearray(24);u32(datainfo,8,1);u32(datainfo,12,bh(PACK))
    data=chunk(0x33320001,datainfo)+chunk(0,bytes(80))+chunk(0x33320002,b'\x11'*120+pixels)
    pack=chunk(0xb3300000,body+chunk(0xb3320000,data))
    out=ROOT/'artifacts'/'Release';out.mkdir(parents=True,exist_ok=True)
    target=out/'NFSMWNativeFreeRoamRacers.tpk';target.write_bytes(pack)
    assets=ROOT/'assets';assets.mkdir(exist_ok=True);decoded.save(assets/'encounter-ready.png')
    # Decode is exactly the DXT3 image, not a higher-quality unrelated mockup.
    preview=Image.new('RGBA',(256,256),(24,27,32,255));preview.alpha_composite(decoded.resize((192,192)),(32,32));preview.save(assets/'encounter-ready-preview.png')
    assert len(pixels)==16384 and struct.unpack_from('<I',pack,4)[0]+8==len(pack)
    assert pack.count(NAME.encode())==1
    print(json.dumps({'textureName':NAME,'textureHash':f'{bh(NAME):08X}','bytes':len(pack),'sha256':hashlib.sha256(pack).hexdigest().upper(),'textures':1,'size':[128,128],'mips':1},indent=2))
if __name__=='__main__':build()
