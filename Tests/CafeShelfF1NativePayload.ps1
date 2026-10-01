param([Parameter(Mandatory=$true)][string]$UpstreamDirectory)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/..").Path
$bundle=Join-Path $repo 'ThirdParty/ShelfSuite/F1Compatibility'
function Need([bool]$condition,[string]$message){if(-not $condition){throw $message}}
function PinnedSourceAndPatchOnly {
 Need ((git -C $UpstreamDirectory rev-parse HEAD).Trim() -eq 'd4f186ba0b5c262c7559b80841132f5fd3884f3c') 'Pinned ShelfSuite v2.1 source required.'
 Need (-not (git -C $UpstreamDirectory status --porcelain)) 'Pinned ShelfSuite source must be clean.'
 Need ((Get-FileHash -LiteralPath (Join-Path $repo 'ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch') -Algorithm SHA256).Hash.ToLowerInvariant() -eq '6c4913af57f1b8542d287c72b08e3ee840275cae51b7f7a96df40707891956e9') 'F1 patch bytes changed unexpectedly.'
}
function NormalizedHash([string]$Path) {
 $bytes=[IO.File]::ReadAllBytes($Path)
 $text=[Text.Encoding]::UTF8.GetString($bytes).Replace("`r`n", "`n")
 return (Get-FileHash -InputStream ([IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes($text))) -Algorithm SHA256).Hash
}
function PayloadMatchesPatchedSource {
 $temporary=Join-Path ([IO.Path]::GetTempPath()) ('CafeShelfF1Payload-' + [guid]::NewGuid().ToString('N'))
 try {
  git clone --shared --quiet $UpstreamDirectory $temporary
  if ($LASTEXITCODE -ne 0) { throw 'Could not create disposable pinned ShelfSuite checkout.' }
  git -C $temporary apply --whitespace=nowarn --unidiff-zero (Join-Path $repo 'ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch')
  if ($LASTEXITCODE -ne 0) { throw 'The approved F1 patch did not apply to the pinned ShelfSuite source.' }
  foreach($relative in @('@Resources/ShelfEngine.lua','@Resources/Variables.inc')) {
   $expected=Join-Path $temporary ('Shelf Suite/' + $relative)
   $actual=Join-Path $bundle ('payload/' + $relative)
   Need ((NormalizedHash $actual) -eq (NormalizedHash $expected)) "Payload does not match patched ShelfSuite source after newline normalization: $relative"
  }
 } finally { if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Recurse -Force} }
}
function RecognitionCatalogCoversOnlyApprovedVariants {
 param($Manifest)
 Need (@($Manifest.Recognition).Count -eq 72) 'Recognition catalog must contain exactly 72 approved variants.'
 $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($entry in @($Manifest.Recognition)) {
  $kind=[string]$entry.Kind
  Need ($kind -in @('Shared','Ini')) 'Recognition catalog contains an unknown entry kind.'
  foreach($hash in @([string]$entry.InputHash,[string]$entry.OutputHash)) { Need ($hash -match '^[0-9a-f]{64}$') 'Recognition catalog contains an invalid hash.' }
  if($kind -eq 'Shared') {
   Need ([string]$entry.Path -in @('@Resources/ShelfEngine.lua','@Resources/Variables.inc')) 'Recognition catalog contains an unapproved shared path.'
   Need ([string]$entry.Payload -eq ('payload/' + [string]$entry.Path)) 'Recognition catalog contains an unexpected payload reference.'
  } else {
   Need ([string]$entry.Family -in @('Stock1','Stock2','Stock3','Generated')) 'Recognition catalog contains an unapproved shelf family.'
   Need ([string]$entry.Theme -in @('DeepOcean','Forest','Terracotta','Obsidian')) 'Recognition catalog contains an unapproved theme.'
  }
  Need ($seen.Add((ConvertTo-Json $entry -Compress))) 'Recognition catalog contains a duplicate entry.'
 }
}
function NoFullSkinOrUserDataBundled {
 param([string[]]$All)
 Need (-not ($All | Where-Object {$_ -match '(?i)(^|/)config\.lua$|(^|/)icons/|(^|/)themes/'})) 'Native F1 bundle contains forbidden user data.'
}
function ManifestRejectsTraversalOrDuplicates {
 param([string[]]$Files)
 Need (($Files | Where-Object {$_ -match '(^|/)\.\.?(/|$)|:'}).Count -eq 0) 'Manifest contains a traversal or drive-qualified file path.'
 Need ((@($Files | Select-Object -Unique).Count -eq @($Files).Count)) 'Manifest lists a duplicate payload file.'
}
PinnedSourceAndPatchOnly
foreach($path in @('manifest.json','payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc','README.md','LICENSE-ShelfSuite.txt')){
 Need (Test-Path -LiteralPath (Join-Path $bundle $path)) "Missing native F1 bundle file: $path"
}
$manifest=Get-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Raw | ConvertFrom-Json
Need ($manifest.UpstreamRevision -eq 'd4f186ba0b5c262c7559b80841132f5fd3884f3c') 'Unexpected upstream provenance.'
Need ($manifest.F1Revision -eq 'c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9') 'Unexpected F1 provenance.'
Need ($manifest.PatchSha256 -eq '6c4913af57f1b8542d287c72b08e3ee840275cae51b7f7a96df40707891956e9') 'Unexpected F1 patch digest.'
$files=@($manifest.Files.Path)
$actualFiles=($files | Sort-Object) -join '|'
$expectedFiles=('payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc' | Sort-Object) -join '|'
Need ($actualFiles -eq $expectedFiles) 'Bundle may contain only the two approved payload files.'
$all=Get-ChildItem -LiteralPath $bundle -Recurse -File | ForEach-Object {[IO.Path]::GetRelativePath($bundle,$_.FullName).Replace('\','/')}
RecognitionCatalogCoversOnlyApprovedVariants $manifest
ManifestRejectsTraversalOrDuplicates $files
NoFullSkinOrUserDataBundled $all
PayloadMatchesPatchedSource
Write-Output 'PASS: native F1 payload provenance and allowlist.'
