$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$baseline=Join-Path (Split-Path $project) 'rollback/2026-09-22-pre-alpha86/source'
function Read([string]$p){(Get-Content -LiteralPath $p -Raw).Replace("`r`n","`n")}
$old=Read (Join-Path $baseline 'src/Runtime.cpp');$new=Read (Join-Path $project 'src/Runtime.cpp')
if($old.Replace('alpha.85-performance-reference-guard','alpha.86-lazy-performance-reference') -cne $new){throw 'Unexpected runtime/construction change'}
foreach($folder in @('src','include','config')) {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -Recurse -File) {
        $relative=$file.FullName.Substring($baseline.Length+1)
        if($relative -in 'src\Runtime.cpp','src\PerformanceReferences.inl','include\Version.h'){continue}
        if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $project $relative)).Hash){throw "Unexpected change $relative"}
    }
}
$oldRef=Read (Join-Path $baseline 'src/PerformanceReferences.inl')
$newRef=Read (Join-Path $project 'src/PerformanceReferences.inl')
$replacement=@'
            // RefSpec::mCollectionPtr is a lazy cache, not proof of existence.
            // Native Instance(RefSpec), 00456CCA..00456CFD, resolves a null
            // cache by class/key. Check that target without writing the cache.
            if(!ref.mCollectionKey)return false;
'@
$expected=$oldRef.Replace('            if(!ref.mCollectionKey||!ref.mCollectionPtr)return false;',$replacement.Replace("`r`n","`n"))
$expected=$expected.Replace('if(found!=ref.mCollectionPtr)return false;','if(ref.mCollectionPtr&&found!=ref.mCollectionPtr)return false;')
if($expected -cne $newRef){throw 'Unexpected reference validator change'}
'PASS only null-cache semantics changed; lookup/class/key/stale/table checks, rejection cache, preconstruction gates and all unrelated settings/behavior preserved'
