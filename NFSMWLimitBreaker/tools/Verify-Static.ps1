param(
    [Parameter(Mandatory=$true)][string]$GameExe
)
$ErrorActionPreference='Stop'
$expectedSize=6029312
$hashes=@(
    '80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C',
    'B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06'
)
$file=Get-Item -LiteralPath $GameExe
$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $GameExe).Hash
if($file.Length -ne $expectedSize -or $hash -notin $hashes){
    throw "Unsupported executable: size=$($file.Length) sha256=$hash"
}
$bytes=[IO.File]::ReadAllBytes($file.FullName)
$sites=@(
    @{Name='audio-mixer-pool-mask'; Offset=0xCC3A9; Expected=[byte[]](0x25,0x00,0x30,0x56,0x00,0x05,0x00,0x40,0x00)},
    @{Name='traffic-capacity-hook-prologue'; Offset=0x1EBD0; Expected=[byte[]](0x53,0x55,0x8B,0xD9,0x8B,0x83,0x94,0,0,0)},
    @{Name='vehicle-demand-compare'; Offset=0x287817; Expected=[byte[]](0x83,0xF8,0x14)},
    @{Name='vehicle-demand-excess'; Offset=0x28781C; Expected=[byte[]](0x8D,0x50,0xEC)},
    @{Name='victim-vector-reserve'; Offset=0x287854; Expected=[byte[]](0x6A,0x14)},
    @{Name='victim-vector-vtable-operand'; Offset=0x287850; Expected=[byte[]](0x34,0xA2,0x8A,0x00)},
    @{Name='ai-vehicle-manager-vector-vtable-operand'; Offset=0x3CA61; Expected=[byte[]](0xD8,0x18,0x89,0x00)},
    @{Name='traffic-capacity-compare'; Offset=0x1EC13; Expected=[byte[]](0x83,0xF8,0x14)},
    @{Name='traffic-capacity-scale'; Offset=0x26330; Expected=[byte[]](0x83,0xC2,0x14)}
)
foreach($site in $sites){
    for($i=0;$i -lt $site.Expected.Count;$i++){
        if($bytes[$site.Offset+$i] -ne $site.Expected[$i]){
            throw ('Patch signature mismatch: {0} file+0x{1:X}' -f $site.Name,($site.Offset+$i))
        }
    }
    Write-Output ('PASS {0} file+0x{1:X}' -f $site.Name,$site.Offset)
}
Write-Output "PASS executable size=$($file.Length) sha256=$hash"
