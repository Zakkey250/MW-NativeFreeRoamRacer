"""Read-only target checks without stale historical source-freeze gates."""
import hashlib
import pathlib
import re
import sys
import pefile

exe = pathlib.Path(sys.argv[1])
assert hashlib.sha256(exe.read_bytes()).hexdigest().upper() in {
    "80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C",
    "B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06",
}
pe = pefile.PE(str(exe))
image = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
root = pathlib.Path(__file__).resolve().parents[1]
source = (root / "src/VehiclePerformance.inl").read_text(encoding="utf-8-sig")
guards = re.findall(r"\{(0x[0-9A-Fa-f]+),\{([^}]+)\}\}", source)
assert len(guards) == 13
checks = [(int(va, 16), bytes(int(x.strip(), 0) for x in values.split(","))) for va, values in guards]
checks += [
    (0x7A4D68, bytes.fromhex("50 4D 7A 00 58 4D 7A 00 50 4D 7A 00 58 4D 7A 00 58 4D 7A 00 50 4D 7A 00 50 4D 7A 00")),
    (0x576A00, bytes.fromhex("D0 69 57 00 D6 69 57 00 DC 69 57 00 E2 69 57 00 E8 69 57 00 EE 69 57 00 F4 69 57 00")),
    (0x689947, bytes.fromhex("8D 4C 24 14 51 8B 4C 24 14 E8 BB 58 EE FF")),
    (0x56F214, bytes.fromhex("81 C1 18 01 00 00 51 50 E8 DF 05 11 00")),
    (0x67F861, bytes.fromhex("8B 0C B7 51 8D 54 24 10 56 52 E8 60 92 FF FF")),
    (0x672E30, bytes.fromhex("56 53 E8 F9 FE FF FF 83 C4 08")),
]
for va, expected in checks:
    assert image[va-base:va-base+len(expected)] == expected, hex(va)
    print(f"PASS performance ABI / mapping surface {va:08X}")
assert "(2,type,shop,0,false)" in source
runtime = (root / "src/Runtime.cpp").read_text(encoding="utf-8-sig")
assert "g_performanceRetry={};" in runtime.split("void ClearWorld(")[1].split("\n}")[0]
assert "VerifySpawnPerformance(created, customization, vehicleKey)" in runtime
assert "alpha.86-lazy-performance-reference" in runtime
retry = (root / "src/PerformanceRetry.inl").read_text(encoding="utf-8-sig")
assert not any(word in retry for word in ["JESKO", "LimitBreaker", "WritePrivateProfile", "MH_CreateHook"])
print("PASS common nonpersistent cooldown, world reset, unchanged career gate, no model/provider special case")

references = (root / "src/PerformanceReferences.inl").read_text(encoding="utf-8-sig")
keys = re.findall(r"0x[0-9A-F]+", references.split("kPerformanceClassKeys{")[1].split("};")[0])
assert len(keys) == 7
for t, key in enumerate(keys):
    offset = 0x8F87A0 + t*28 + 8 - base
    assert int.from_bytes(image[offset:offset+4], "little") == int(key, 16)
print("PASS all seven native field/class keys match runtime upgrade table")
for va, data in [
    (0x678898, "83 F8 02"),
    (0x6788AD, "83 F8 03 75 07 8B F7 BF 02 00 00 00"),
]:
    expected=bytes.fromhex(data)
    assert image[va-base:va-base+len(expected)] == expected, hex(va)
assert "ValidateNativePerformanceReferences(key,&instance,*plan)" in source
assert "CIVICFN2" not in references and "WritePrivateProfile" not in references
assert "cachedUntilCatalogChange=1" in references
print("PASS native endpoint mapping, preconstruction validation, no model-name blacklist or INI write")

# Native RefSpec cache is optional: null triggers class/key lookup before use.
for va, data in [(0x456CCA, "8B 47 08 85 C0 75 2C"), (0x456CEB, "53 8B C8 E8 6D EC FF FF")]:
    expected = bytes.fromhex(data)
    assert image[va-base:va-base+len(expected)] == expected, hex(va)
assert "if(!ref.mCollectionKey)return false;" in references
assert "if(ref.mCollectionPtr&&found!=ref.mCollectionPtr)return false;" in references
assert "!ref.mCollectionKey||!ref.mCollectionPtr" not in references
print("PASS native lazy resolution semantics; missing target still resolved and checked without cache mutation")
