"""Read-only native ABI and source integration checks; not gameplay acceptance."""
import hashlib
import json
from pathlib import Path
import pefile

project = Path(__file__).resolve().parents[1]
workspace = project.parent
target = json.loads((workspace / 'integration/NFSMW-ExecutableTarget.json').read_text(encoding='utf-8-sig'))
exe = Path(((__import__('os').environ.get('NFS_GAME_ROOT') or (r'E:\EA GAMES' if __import__('os').environ.get('COMPUTERNAME', '').upper() == 'ZAKKEY' else r'\\ZAKKEY\EA GAMES')) + '\\' + 'Need for Speed Most Wanted Main\\speed.exe'))
data = exe.read_bytes()
assert len(data) == target['size'] and hashlib.sha256(data).hexdigest().upper() == target['sha256']
pe = pefile.PE(data=data)
guards = {
    0x507460: '53 8B 5C 24 08 81 3B 10 40 13 80',
    0x507534: 'C7 40 04 00 00 00 00',
    0x507558: '66 C7 46 7C FF FF',
    0x506D00: '51 56 8B 74 24 0C 56 E8 84 FF FF FF',
    0x507510: '8B 74 24 18 85 F6',  # second cdecl argument is owner pack
    0x507661: 'E8 FA FD FF FF 83 C4 08',  # container unload forwards both args
    0x5076F2: 'E8 69 FD FF FF 83 C4 08',  # streamed solid unload same ABI
}
for address, value in guards.items():
    expected = bytes.fromhex(value)
    assert pe.get_data(address - pe.OPTIONAL_HEADER.ImageBase, len(expected)) == expected, hex(address)
    print(f'PASS native solid lifecycle ABI {address:08X}')
runtime = (project / 'src/Runtime.cpp').read_text(encoding='utf-8-sig')
guard = (project / 'src/SolidLifetime.inl').read_text(encoding='utf-8-sig')
assert 'ValidateSolidLifetimeSurface()' in runtime
assert runtime.index('Address(kSolidUnload)),&SolidUnloadHook') < runtime.index('MH_EnableHook(MH_ALL_HOOKS)')
assert guard.count('g_originalSolidUnload(chunk,owner);') == 1
assert 'if(entry.pointer!=solid)continue;' in guard and 'if(pack!=owner)' in guard
assert '0xFFFF' in guard and 'patch.pack+0x7C' in guard
assert 'CIVICFN2' not in guard and '3AFD3772' not in guard
baseline = workspace / 'rollback/2026-09-22-pre-alpha87/source/src'
for name in ('VehiclePerformance.inl', 'SpawnEligibility.inl'):
    assert (project / 'src' / name).read_text(encoding='utf-8-sig').splitlines() == (baseline / name).read_text(encoding='utf-8-sig').splitlines()
    print(f'PASS quarantine removed, alpha86 admission restored: {name}')
print('PASS exact-identity unlink, required hook integration, no vehicle/LB gate')
