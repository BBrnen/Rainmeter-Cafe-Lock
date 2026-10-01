param([Parameter(Mandatory=$true)][string]$UpstreamDirectory)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/..").Path
$bundle=Join-Path $repo 'ThirdParty/ShelfSuite/F1Compatibility'
function Need([bool]$condition,[string]$message){if(-not $condition){throw $message}}
Need ((git -C $UpstreamDirectory rev-parse HEAD).Trim() -eq 'd4f186ba0b5c262c7559b80841132f5fd3884f3c') 'Pinned ShelfSuite v2.1 source required.'
foreach($path in @('manifest.json','payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc','README.md','LICENSE-ShelfSuite.txt')){
 Need (Test-Path -LiteralPath (Join-Path $bundle $path)) "Missing native F1 bundle file: $path"
}
$manifest=Get-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Raw | ConvertFrom-Json
Need ($manifest.UpstreamRevision -eq 'd4f186ba0b5c262c7559b80841132f5fd3884f3c') 'Unexpected upstream provenance.'
Need ($manifest.F1Revision -eq 'c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9') 'Unexpected F1 provenance.'
Need ($manifest.PatchSha256 -eq '6c4913af57f1b8542d287c72b08e3ee840275cae51b7f7a96df40707891956e9') 'Unexpected F1 patch digest.'
Need (@($manifest.Recognition).Count -eq 72) 'Recognition catalog must contain exactly 72 approved variants.'
$files=@($manifest.Files.Path)
$actualFiles=($files | Sort-Object) -join '|'
$expectedFiles=('payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc' | Sort-Object) -join '|'
Need ($actualFiles -eq $expectedFiles) 'Bundle may contain only the two approved payload files.'
$all=Get-ChildItem -LiteralPath $bundle -Recurse -File | ForEach-Object {[IO.Path]::GetRelativePath($bundle,$_.FullName).Replace('\','/')}
Need (-not ($all | Where-Object {$_ -match '(?i)(^|/)config\.lua$|(^|/)icons/|(^|/)themes/'})) 'Native F1 bundle contains forbidden user data.'
Write-Output 'PASS: native F1 payload provenance and allowlist.'
