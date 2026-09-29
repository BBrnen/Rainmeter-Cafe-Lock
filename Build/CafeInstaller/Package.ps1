param([string]$MakeNsis = "${env:ProgramFiles(x86)}/NSIS/makensis.exe")
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$payload = Join-Path $repo 'work-package/payload'
$out = Join-Path $repo 'dist'
if (Test-Path $payload) { throw 'Package staging must be fresh' }
New-Item -ItemType Directory -Force $payload,$out | Out-Null
foreach ($name in @('Rainmeter.exe','Rainmeter.dll','RestartRainmeter.exe','SkinInstaller.exe')) {
 Copy-Item "$repo/x64-Release/$name" $payload
}
New-Item -ItemType Directory "$payload/Plugins","$payload/Languages","$payload/Defaults" | Out-Null
Get-ChildItem "$repo/x64-Release/Plugins/*.dll" | Where-Object Name -NotLike '*Example*' | Copy-Item -Destination "$payload/Plugins"
Copy-Item "$repo/x32-Release/Languages/*.dll" "$payload/Languages"
Copy-Item "$repo/Build/Skins","$repo/Build/Layouts" "$payload/Defaults" -Recurse
Copy-Item "$repo/Application/Rainmeter.exe.config","$repo/LICENSE" $payload
Copy-Item "$repo/Docs/CafeLock-Deployment.md" "$payload/README-Cafe-Lock.md"
# Exact list: uninstall never recursively deletes unknown files or user profiles.
$manifest = Join-Path $repo 'work-package/DeleteFiles.nsh'
$lines = @(Get-ChildItem $payload -Recurse -File | ForEach-Object {
 'Delete "$INSTDIR\' + [IO.Path]::GetRelativePath($payload,$_.FullName) + '"'
})
$lines += @(Get-ChildItem $payload -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
 'RMDir "$INSTDIR\' + [IO.Path]::GetRelativePath($payload,$_.FullName) + '"'
})
$lines | Set-Content $manifest
$installer = Join-Path $out 'Rainmeter-Cafe-Lock-4.5.26.3894-x64-Setup.exe'
Push-Location $PSScriptRoot
try {
 & $MakeNsis /WX "/DPAYLOAD=$payload" "/DOUTFILE=$installer" "/DDELETE_MANIFEST=$manifest" Installer.nsi
 if ($LASTEXITCODE -ne 0) { throw 'NSIS compilation failed' }
} finally { Pop-Location }
# Source accompanies every build for reproducibility and GPL redistribution.
git -C $repo archive --format=zip "--output=$out/Rainmeter-Cafe-Lock-source.zip" HEAD
if ($LASTEXITCODE -ne 0) { throw 'Source archive failed' }
Copy-Item "$repo/Docs/CafeLock-Deployment.md" "$out/README.md"
Get-ChildItem $out -File | Get-FileHash -Algorithm SHA256 | ForEach-Object {
 "$($_.Hash)  $([IO.Path]::GetFileName($_.Path))"
} | Set-Content "$out/SHA256SUMS.txt"
