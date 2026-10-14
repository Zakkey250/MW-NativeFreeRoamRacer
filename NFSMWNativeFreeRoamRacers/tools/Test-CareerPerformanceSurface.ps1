$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-EncounterBattleSurface.ps1')
$source=Get-Content (Join-Path $projectRoot 'src\VehiclePerformance.inl') -Raw
$matches=[regex]::Matches($source,'\{(0x[0-9A-Fa-f]+),\{([^}]+)\}\}')
if($matches.Count -ne 7){throw 'Unexpected performance guard count'}
foreach($match in $matches) {
    $va=[Convert]::ToUInt32($match.Groups[1].Value.Substring(2),16)
    $expected=($match.Groups[2].Value.Split(',') | ForEach-Object { '{0:X2}' -f [int]$_ }) -join ' '
    if((ReadTargetBytes $va 8).Replace('-',' ') -ne $expected){throw "Performance guard mismatch $va"}
    'PASS performance read/apply native guard {0:X8}' -f $va
}
foreach($guard in @(
    @(0x007A4D68,'50 4D 7A 00 58 4D 7A 00 50 4D 7A 00 58 4D 7A 00 58 4D 7A 00 50 4D 7A 00 50 4D 7A 00'),
    @(0x00576A00,'D0 69 57 00 D6 69 57 00 DC 69 57 00 E2 69 57 00 E8 69 57 00 EE 69 57 00 F4 69 57 00'),
    @(0x00689947,'8D 4C 24 14 51 8B 4C 24 14 E8 BB 58 EE FF'),
    @(0x0056F214,'81 C1 18 01 00 00 51 50 E8 DF 05 11 00'),
    @(0x0067F861,'8B 0C B7 51 8D 54 24 10 56 52 E8 60 92 FF FF')
)) {
    if((ReadTargetBytes $guard[0] ($guard[1].Split(' ').Count)).Replace('-',' ') -ne $guard[1]){throw 'Performance package mapping/application mismatch'}
    'PASS native package tier mapping / private record application {0:X8}' -f $guard[0]
}
if($source -match 'SetMaximum|NosAdded|WritePrivateProfile|MH_CreateHook'){throw 'Performance must not force unlocks/NOS, hook or edit INIs'}
if(!$source.Contains('(2,type,shop,0,false)')){throw 'Explicit career regular shop filter missing'}
'PASS career-only locks, no backroom/Junkman/global tuning/NOS bypass, post-construction seven-level verification'
