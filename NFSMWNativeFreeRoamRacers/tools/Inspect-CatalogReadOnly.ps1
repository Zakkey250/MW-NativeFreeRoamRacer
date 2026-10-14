param([Parameter(Mandatory=$true)][int]$GamePid)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CatalogReadOnly {
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr p,IntPtr a,byte[] b,int n,out IntPtr read);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr p);
 public static byte[] Read(IntPtr p,long a,int n){byte[] b=new byte[n];IntPtr r;if(!ReadProcessMemory(p,new IntPtr(a),b,n,out r)||r.ToInt64()!=n)throw new Exception("Read failed "+a.ToString("X"));return b;}
}
'@
$process=Get-Process -Id $GamePid
$expected=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted\speed.exe')
if($process.Path -ne $expected -or (Get-FileHash $expected).Hash -ne 'B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06'){throw 'Unsupported process'}
$handle=[CatalogReadOnly]::OpenProcess(0x410,$false,$GamePid)
if($handle -eq [IntPtr]::Zero){throw 'Read-only open failed'}
try {
 function U32([long]$a){[BitConverter]::ToUInt32([CatalogReadOnly]::Read($handle,$a,4),0)}
 function Node([long]$map,[uint32]$key) {
  if(!$key){return 0}
  $ptr=U32 $map;$size=U32 ($map+4);if(!$ptr -or $size -gt 32768){throw 'Invalid hash map'}
  $data=[CatalogReadOnly]::Read($handle,$ptr,($size*12))
  for($i=0;$i -lt $size;$i++) {if([BitConverter]::ToUInt32($data,$i*12) -eq $key -and [BitConverter]::ToUInt32($data,$i*12+4) -ne ($ptr+$i*12)) {return ($ptr+$i*12)}}
  return 0
 }
 $db=U32 0x90DCBC;$dbPrivate=U32 ($db+4)
 $classes=@{};foreach($key in [uint32]0x4A97EC8F,[uint32]::Parse('85885722','HexNumber')) {
  $node=Node ($dbPrivate+8) $key;$cls=U32 ($node+4);$classes[$key]=$cls
  'CLASS {0:X8} ptr={1:X8} private={2:X8}' -f $key,$cls,(U32 ($cls+8))
 }
 $pv=$classes[[uint32]0x4A97EC8F];$fe=$classes[[uint32]::Parse('85885722','HexNumber')]
 $pvMap=(U32 ($pv+8))+0x1C;$feMap=(U32 ($fe+8))+0x1C
 foreach($map in $pvMap,$feMap) {
  'MAP addr={0:X8} capacity={1} count={2}' -f $map,(U32 ($map+4)),(U32 ($map+8))
  $ptr=U32 $map;$size=U32 ($map+4);$shown=0
  if($size -gt 32768){throw 'Invalid map capacity'}
  for($i=0;$i -lt $size -and $shown -lt 8;$i++) {
   $key=U32 ($ptr+$i*12);$value=U32 ($ptr+$i*12+4)
   if($key -and $value -and $value -ne ($ptr+$i*12)) {' SAMPLE key={0:X8} value={1:X8}' -f $key,$value;$shown++}
  }
 }
 foreach($hex in 'D5C414E5','52505E1A','377F6C3A') {
  $key=[uint32]::Parse($hex,'HexNumber');$node=Node $pvMap $key;if(!$node){continue};$row=U32 ($node+4)
  'PVEHICLE key={0:X8} row={1:X8} layout={2:X8}' -f $key,$row,(U32 ($row+0x18))
  $fieldNode=Node $row ([uint32]::Parse('85885722','HexNumber'))
  if(!$fieldNode){'frontend not in direct collection map';continue}
  $nodeBytes=[CatalogReadOnly]::Read($handle,$fieldNode,12)
  $value=U32 ($fieldNode+4);$flags=$nodeBytes[11]
  'FIELD node={0:X8} value={1:X8} flags={2:X2}' -f $fieldNode,$value,$flags
  if($flags -band 0x10){$value=(U32 ($row+0x18))+$value}
  $ref=[CatalogReadOnly]::Read($handle,$value,12)
  $a=[BitConverter]::ToUInt32($ref,0);$b=[BitConverter]::ToUInt32($ref,4);$c=[BitConverter]::ToUInt32($ref,8)
  'REF raw={0:X8}/{1:X8}/{2:X8}' -f $a,$b,$c
  foreach($candidate in $b,$c) {
   $found=Node $feMap $candidate
   if($found) {
    $front=U32 ($found+4);$layout=U32 ($front+0x18)
    'FRONTEND matchedKey={0:X8} row={1:X8} actualKey={2:X8} layout={3:X8}' -f $candidate,$front,(U32 ($front+0x20)),$layout
    if($layout){'FRONTEND cost={0} customize={1}' -f (U32 ($layout+0x4C)),([CatalogReadOnly]::Read($handle,($layout+0x58),1)[0])}
   }
  }
 }
} finally {[void][CatalogReadOnly]::CloseHandle($handle)}
