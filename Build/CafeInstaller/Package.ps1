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
# Read-only native compatibility data; never package an updater script or user skin.
$f1Source = Join-Path $repo 'ThirdParty/ShelfSuite/F1Compatibility'
$f1Destination = Join-Path $payload 'Compatibility/ShelfSuiteF1'
foreach ($relative in @('manifest.json','README.md','LICENSE-ShelfSuite.txt','payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc')) {
 $target = Join-Path $f1Destination $relative
 New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
 Copy-Item -LiteralPath (Join-Path $f1Source $relative) -Destination $target
 if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath (Join-Path $f1Source $relative)).Hash) { throw "F1 payload staging failed: $relative" }
}
# Verify the exact offline prerequisite; acquisition never executes it.
$runtime = & "$repo/Build/CafeDependencies/AcquireRuntime.ps1"
& "$repo/Tests/CafeRuntimeAcquisition.ps1" -CacheDirectory (Split-Path $runtime.Path -Parent)
$notices = Join-Path $payload 'Notices'
New-Item -ItemType Directory $notices | Out-Null
$sdkPin = (Get-Content "$repo/Build/CafeDependencies/dependencies.json" -Raw | ConvertFrom-Json).webView2
$sdk = Join-Path $repo "work-package/dependencies/$($sdkPin.package).$($sdkPin.version)"
Copy-Item "$sdk/LICENSE.txt" "$notices/WebView2-LICENSE.txt"
Copy-Item "$sdk/NOTICE.txt" "$notices/WebView2-NOTICE.txt"
Copy-Item "$repo/Library/CafeShelf/UI/LICENSE-ShelfSuite.txt" "$notices/ShelfSuite-LICENSE.txt"
Copy-Item "$repo/Docs/CafeLock-Usability-Verification.md" "$payload/README-ShelfSuite-Editor.md"
# No builder-local cache path appears in distributed metadata.
$runtime | Select-Object -Property * -ExcludeProperty Path | ConvertTo-Json |
 Set-Content "$notices/WebView2-runtime-manifest.json" -Encoding utf8NoBOM
Copy-Item "$notices/WebView2-runtime-manifest.json" "$out/WebView2-runtime-manifest.json"
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
 & $MakeNsis /WX "/DPAYLOAD=$payload" "/DOUTFILE=$installer" "/DDELETE_MANIFEST=$manifest" "/DRUNTIME_INSTALLER=$($runtime.Path)" Installer.nsi
 if ($LASTEXITCODE -ne 0) { throw 'NSIS compilation failed' }
} finally { Pop-Location }
# Source accompanies every build for reproducibility and GPL redistribution.
git -C $repo archive --format=zip "--output=$out/Rainmeter-Cafe-Lock-source.zip" HEAD
if ($LASTEXITCODE -ne 0) { throw 'Source archive failed' }
Copy-Item "$repo/Docs/CafeLock-Deployment.md" "$out/README.md"
Get-ChildItem $out -File | Where-Object Name -ne 'SHA256SUMS.txt' | Get-FileHash -Algorithm SHA256 | ForEach-Object {
 "$($_.Hash)  $([IO.Path]::GetFileName($_.Path))"
} | Set-Content "$out/SHA256SUMS.txt"
Get-Content -LiteralPath "$out/SHA256SUMS.txt"
