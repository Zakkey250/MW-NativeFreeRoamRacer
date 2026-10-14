"""Exact-binary ABI and exports, read-only; no game launch."""
import sys,struct,hashlib,pefile
from pathlib import Path
root=Path(__file__).resolve().parents[1]; game=Path(sys.argv[1])
data=(game/'scripts/MWArsenal.asi').read_bytes()
assert hashlib.sha256(data).hexdigest().upper() in (
    '133E8F6FC1AF11CA038414AAE1B5057FA11E0C4B928856F3E357146A4FDF9BEF',
    '392DDA59241F0478EC01BEB3FDE53D7F31DFA8AA153A4F7F9BCE9B50A87FFE48')
pe=pefile.PE(data=data); image=pe.get_memory_mapped_image()
for call,destination,tail,ret in [(0xC974,0x8280,0x8822,b'\xc2\x08\x00'),(0x8F84,0x8CA0,0x8F0F,b'\xc2\x10\x00')]:
    assert image[call]==0xE8 and call+5+struct.unpack_from('<i',image,call+1)[0]==destination
    assert image[destination:destination+5]==b'\x55\x8b\xec\x6a\xfe'
    # EMP has multiple return sites; require the native RET 8 in its exact span.
    assert ret in image[destination:(0x8830 if destination==0x8280 else tail+3)]
print('PASS real Arsenal player EMP C979 stdcall RET8; shock 8F89 stdcall RET16')
assert image[0x8EDD:0x8EDF]==b'\xb0\x01' # success only after physical impulse application
assert image[0x8EF2:0x8EF4]==b'\x32\xc0'
print('PASS shock success/failure branch is after native effect, not input activation')
for path,names in [(root/'artifacts/Release/NFSMWNativeFreeRoamRacers.asi',['NFR_PlayerSpikeHitV1']),
                   (root.parent/'MWArmsAssist/bin/MWArmsAssist.asi',['MWAA_EncounterReadyV1','MWAA_EncounterPoliceEmpV1'])]:
    module=pefile.PE(str(path));exports={e.name.decode() for e in module.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    assert all(n in exports for n in names), (path,exports)
    print('PASS explicit v1 exports:',', '.join(names))
