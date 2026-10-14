"""Read-only whole-native-section equivalence; not loaded-MOD/gameplay acceptance."""
from pathlib import Path
import hashlib
import json
import pefile

workspace = Path(__file__).resolve().parents[2]
paths = [
    Path(((__import__('os').environ.get('NFS_GAME_ROOT') or (r'E:\EA GAMES' if __import__('os').environ.get('COMPUTERNAME', '').upper() == 'ZAKKEY' else r'\\ZAKKEY\EA GAMES')) + '\\' + 'Need for Speed Most Wanted Main\\speed.exe')),
    Path(((__import__('os').environ.get('NFS_GAME_ROOT') or (r'E:\EA GAMES' if __import__('os').environ.get('COMPUTERNAME', '').upper() == 'ZAKKEY' else r'\\ZAKKEY\EA GAMES')) + '\\' + 'Need For Speed Most Wanted REDUX 3.04 - Main\\speed.exe')),
]
manifests = ['NFSMW-ExecutableTarget.json', 'NFRR-Redux-ExecutableTarget.json']
pes = []
for path, name in zip(paths, manifests):
    manifest = json.loads((workspace / 'integration' / name).read_text(encoding='utf-8-sig'))
    data = path.read_bytes()
    assert len(data) == manifest['size']
    assert hashlib.sha256(data).hexdigest().upper() == manifest['sha256']
    pe = pefile.PE(data=data)
    assert pe.FILE_HEADER.Machine == 0x14c
    assert pe.FILE_HEADER.Characteristics & 0x20
    assert pe.OPTIONAL_HEADER.ImageBase == 0x400000
    assert pe.OPTIONAL_HEADER.AddressOfEntryPoint == 0x3c4040
    pes.append(pe)
    print('PASS exact PE identity', manifest['id'], manifest['sha256'])
for name in (b'.text', b'.rdata', b'.data'):
    sections = [next(s for s in pe.sections if s.Name.rstrip(b'\0') == name) for pe in pes]
    a, b = sections
    assert (a.VirtualAddress, a.Misc_VirtualSize, a.SizeOfRawData, a.Characteristics) == (
        b.VirtualAddress, b.Misc_VirtualSize, b.SizeOfRawData, b.Characteristics)
    assert a.get_data() == b.get_data()
    print('PASS entire native section', name.decode(), hex(a.VirtualAddress),
          a.SizeOfRawData, hashlib.sha256(a.get_data()).hexdigest())
print('PASS unchanged native address layout; runtime hook/owner checks still required')
