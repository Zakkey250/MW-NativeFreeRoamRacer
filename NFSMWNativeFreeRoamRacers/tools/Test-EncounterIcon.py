"""Read-only independent chunk walk and DXT3 decode of the release icon."""
import struct
from pathlib import Path
from PIL import Image
p=Path(__file__).resolve().parents[1]
b=(p/'artifacts/Release/NFSMWNativeFreeRoamRacers.tpk').read_bytes()
chunks={}
def walk(start,end):
    while start<end:
        tag,n=struct.unpack_from('<II',b,start);stop=start+8+n
        assert stop<=end
        if tag in (0xb3300000,0xb3310000,0xb3320000):walk(start+8,stop)
        elif tag: assert tag not in chunks;chunks[tag]=b[start+8:stop]
        start=stop
    assert start==end
walk(0,len(b))
assert set(chunks)=={0x33310001,0x33310002,0x33310004,0x33310005,0x33320001,0x33320002}
h=chunks[0x33310004];assert len(h)==124
assert h[12:36].split(b'\0')[0]==b'NFR_ENCOUNTER_READY'
assert struct.unpack_from('<I',h,36)[0]==struct.unpack_from('<I',chunks[0x33310002])[0]==0xa808654b
assert struct.unpack_from('<HH',h,68)==(128,128) and h[72:75]==bytes((7,7,0x24)) and h[78]==1
assert chunks[0x33310005][20:24]==b'DXT3'
payload=chunks[0x33320002][120:];assert len(payload)==16384==struct.unpack_from('<I',h,56)[0]
out=Image.new('RGBA',(128,128))
for i in range(1024):
    alpha,c0,c1,index=struct.unpack_from('<QHHI',payload,i*16)
    assert index==0 and c0==c1
    rgb=((c0>>11)*255//31,((c0>>5)&63)*255//63,(c0&31)*255//31)
    for j in range(16):out.putpixel(((i%32)*4+j%4,(i//32)*4+j//4),rgb+(((alpha>>(j*4))&15)*17,))
assert out.tobytes()==Image.open(p/'assets/encounter-ready.png').convert('RGBA').tobytes()
assert out.getbbox() and out.getextrema()[3]==(0,255)
print('PASS TPK chunk boundaries, one image, keys, dimensions, format, payload size, 1024 DXT3 blocks and exact decoded preview')
