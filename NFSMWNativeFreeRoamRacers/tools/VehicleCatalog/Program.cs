using VaultLib.Core;
using VaultLib.Core.DB;
using VaultLib.Core.Pack;
using VaultLib.Core.Hashing;
using VaultLib.Core.Data;
using VaultLib.Core.Types;
using System.Text.RegularExpressions;
using System.Security.Cryptography;

if (args.Length < 2 || (args[1] == "--export" && args.Length != 3)) {
    Console.Error.WriteLine("Usage: VehicleCatalog <installed game root> --export <header path> | <class> [row]");
    Environment.ExitCode = 2;
    return;
}

new VaultLib.Support.MostWanted.ModuleDef().Load();
HashManager.LoadDictionary("tools/OGVI-1.4/release_windows/plugins/Attribulator.Plugins.SpeedProfiles/Resources/hashes.txt");
var db = new Database(new DatabaseOptions("MOST_WANTED", DatabaseType.X86Database));
foreach (var file in new[] { "attributes.bin", "fe_attrib.bin" }) {
    using var reader = new BinaryReader(File.OpenRead(Path.Combine(args[0], "GLOBAL", file)));
    new StandardVaultPack().Load(reader, db, new PackLoadingOptions());
}
db.CompleteLoad();
object? Data(VltCollection? row, string field) {
    for (; row != null; row = row.Parent) {
        if (row.GetData().TryGetValue(field, out var value)) return value;
    }
    return null;
}
string Value(object? data, string property = "Value") => data?.GetType().GetProperty(property)?.GetValue(data)?.ToString() ?? "";
if (args[1] == "--export") {
    var rows = db.RowManager.EnumerateFlattenedCollections().ToList();
    var candidates = new List<(string Model, uint Key, int Cost, bool Customizable, string Row)>();
    foreach (var row in rows.Where(r => r.Class.Name == "pvehicle" && r.Parent?.Name == "racers")) {
        string model = Value(Data(row, "MODEL")).ToUpperInvariant();
        string frontend = Value(Data(row, "frontend"), "CollectionKey");
        var fe = rows.FirstOrDefault(r => r.Class.Name == "frontend" && r.Name == frontend);
        if (!Regex.IsMatch(model, "^[A-Z0-9_]{1,31}$") || fe == null ||
            !int.TryParse(Value(Data(fe, "Cost")), out int cost) || cost < 0 ||
            !File.Exists(Path.Combine(args[0], "CARS", model, "GEOMETRY.BIN")) ||
            !File.Exists(Path.Combine(args[0], "CARS", model, "TEXTURES.BIN"))) continue;
        uint key = row.Name.StartsWith("0x") ? Convert.ToUInt32(row.Name[2..], 16) : VLT32Hasher.Hash(row.Name);
        candidates.Add((model, key, cost, Value(Data(fe, "IsCustomizable")) == "True", row.Name));
    }
    var unique = candidates.GroupBy(c => c.Model).Select(g => g.OrderBy(c => c.Row == c.Model.ToLowerInvariant() ? 0 : 1).ThenBy(c => c.Key).First()).OrderBy(c => c.Model).ToList();
    if (unique.Count < 8) throw new Exception("Too few validated installed racer records");
    var lines = new List<string> { "// Generated from installed VLT. Run tools/VehicleCatalog to refresh. Do not hand edit.", "#pragma once", "struct InstalledVehicle { const char* model; unsigned int key; int cost; bool customizable = true; };", "constexpr InstalledVehicle kInstalledVehicles[] = {" };
    foreach (var car in unique) lines.Add($"    {{\"{car.Model}\", 0x{car.Key:X8}u, {car.Cost}, {car.Customizable.ToString().ToLowerInvariant()}}}, // {car.Row}");
    lines.Add("};");
    lines.Add("struct CatalogAsset { const wchar_t* path; const char* sha256; };");
    lines.Add("constexpr CatalogAsset kCatalogAssets[] = {");
    foreach (string file in new[] { "GLOBAL/attributes.bin", "GLOBAL/fe_attrib.bin", "GLOBAL/GlobalB.lzc" }) {
        using var stream = File.OpenRead(Path.Combine(args[0], file));
        lines.Add($"    {{L\"{file}\", \"{Convert.ToHexString(SHA256.HashData(stream))}\"}},");
    }
    lines.Add("};");
    File.WriteAllLines(args[2], lines);
    foreach (var c in unique) Console.WriteLine($"{c.Model}|{c.Key:X8}|{c.Cost}|{c.Row}");
    Console.WriteLine($"EXPORTED {unique.Count} models; runtime also checks CarTypeInfo usage, model and native stock performance.");
    return;
}
foreach (var row in db.RowManager.EnumerateFlattenedCollections().Where(r => r.Class.Name == args[1] && (args.Length < 3 || r.Name == args[2]))) {
    Console.WriteLine($"ROW {row.Name} parent={row.Parent?.Name}");
    foreach (var pair in row.GetData()) {
        var val = pair.Value;
        var props = val.GetType().GetProperties().Where(p => p.GetIndexParameters().Length == 0).Select(p => $"{p.Name}={p.GetValue(val)}");
        Console.WriteLine($"  {pair.Key} {val.GetType().Name} [{string.Join(',', props)}]");
    }
}
