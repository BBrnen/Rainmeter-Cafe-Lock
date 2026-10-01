param([switch]$Standard)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Installer integration test runs only on a disposable GitHub runner' }
$repo = (Resolve-Path "$PSScriptRoot/..").Path
$installed = Join-Path $env:ProgramW6432 'Rainmeter Cafe Lock'
$profile = Join-Path $env:APPDATA 'Rainmeter Cafe Lock'
$startupLink = Join-Path ([Environment]::GetFolderPath('CommonStartup')) 'Rainmeter Cafe Lock.lnk'
$setup = Join-Path $repo 'dist/Rainmeter-Cafe-Lock-4.5.26.3894-x64-Setup.exe'
function Run-Setup {
 $p = Start-Process $setup -ArgumentList '/S' -WindowStyle Hidden -Wait -PassThru
 if ($p.ExitCode -ne 0) { throw "Setup failed: $($p.ExitCode)" }
}
function Assert-F1Bundle {
 $bundle = Join-Path $installed 'Compatibility/ShelfSuiteF1'
 $expected = @('manifest.json','README.md','LICENSE-ShelfSuite.txt','payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc')
 foreach ($relative in $expected) {
  $target = Join-Path $bundle $relative
  if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "F1BundleInstalledAndProtected: missing $relative" }
  if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath (Join-Path "$repo/ThirdParty/ShelfSuite/F1Compatibility" $relative)).Hash) { throw "F1 bundle bytes differ: $relative" }
 }
 $actual = @(Get-ChildItem -LiteralPath $bundle -Recurse -Force -File)
 if ($actual.Count -ne $expected.Count) { throw 'F1 bundle contains unexpected files' }
 Write-Output 'PASS F1BundleInstalledAndProtected: exact runtime allowlist and source bytes.'
 return $bundle
}
if ($Standard) {
 [void](Assert-F1Bundle)
 $bundle = Join-Path $installed 'Compatibility/ShelfSuiteF1'
 foreach ($target in @(Get-ChildItem -LiteralPath $bundle -Recurse -File)) {
  $denied = $false
  try { $f = [IO.File]::Open($target.FullName,'Open','Write'); $f.Dispose() } catch [UnauthorizedAccessException] { $denied = $true }
  if (-not $denied) { throw "Standard user could alter F1 data: $($target.Name)" }
  $denied = $false
  try { [IO.File]::Delete($target.FullName) } catch [UnauthorizedAccessException] { $denied = $true }
  if (-not $denied) { throw "Standard user could replace F1 data: $($target.Name)" }
 }
 $denied = $false
 try { [IO.File]::WriteAllText((Join-Path $bundle 'unexpected-probe.txt'),'must not succeed') } catch [UnauthorizedAccessException] { $denied = $true }
 if (-not $denied) { throw 'Standard user could add F1 bundle files' }
 Write-Output 'PASS StandardUserCanReadButCannotAlterF1Bundle.'
 $probe = Join-Path $installed 'customer-write-probe.txt'
 $denied = $false
 try { [IO.File]::WriteAllText($probe,'must not succeed') } catch [UnauthorizedAccessException] { $denied = $true }
 if (-not $denied) { throw 'Standard user could write the program directory' }
 $denied = $false
 try { $f = [IO.File]::Open("$installed/Rainmeter.dll",[IO.FileMode]::Open,[IO.FileAccess]::Write); $f.Dispose() } catch [UnauthorizedAccessException] { $denied = $true }
 if (-not $denied) { throw 'Standard user could overwrite Rainmeter.dll' }
 # Verify the installed sign-in entry, then launch that actual shortcut as a standard user.
 $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($startupLink)
 if ($shortcut.TargetPath -ne "$installed\Rainmeter.exe" -or $shortcut.Arguments) { throw 'Unexpected startup shortcut target/arguments' }
 $flags = [BitConverter]::ToUInt32([IO.File]::ReadAllBytes($startupLink),20)
 if (($flags -band 0x2000) -ne 0) { throw 'Startup shortcut requests administrator elevation' }
 $p = Start-Process $startupLink -PassThru
 try {
  $deadline = [DateTime]::UtcNow.AddSeconds(20)
  while (-not (Test-Path "$profile/Rainmeter.ini")) {
   if ($p.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Installed default profile failed to initialize' }
   Start-Sleep -Milliseconds 100
  }
  Start-Sleep -Milliseconds 700
  if ($p.HasExited) { throw 'Installed default runtime exited' }
  'preserve me' | Set-Content "$profile/installer-preservation-test.txt"
  # Check that the installer refuses a running DLL rather than killing the app.
  Write-Output 'PASS: protected program files; actual sign-in shortcut launch as standard user; default-profile startup.'
 } finally { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
 & "$env:ProgramFiles/PowerShell/7/pwsh.exe" -NoProfile -File "$repo/Tests/CafeLockSmoke.ps1" -BuildDirectory $installed -StandardUser -Maintenance
 if ($LASTEXITCODE -ne 0) { throw 'Installed password/runtime regression failed' }
 exit 0
}
if (Test-Path $installed) { throw 'Runner already has Cafe Lock installed; refusing to alter it' }
if (Test-Path $profile) { throw 'Runner already has a Cafe Lock profile' }
# Only CI creates/reads these disposable sentinels. Product installation must
# never inspect user shelves, launcher configuration or compatibility backups.
$fixture = Join-Path $env:RUNNER_TEMP ('CafeInstaller-F1-' + [guid]::NewGuid().ToString('N'))
$sentinels = @('Skins/Shelf Suite/Shelf1/config.lua','Skins/Shelf Suite/@Resources/Icons/personal.png','Skins/Shelf Suite/@Resources/Themes/personal.inc','Skins/Shelf Suite-F1-Backup-fixture/RESTORE.txt','Rainmeter.ini','CafeLock.ini')
foreach ($relative in $sentinels) {
 $target = Join-Path $fixture $relative
 New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
 [IO.File]::WriteAllText($target, ('isolated installer preservation sentinel: ' + $relative))
}
function Fixture-Hashes {
 return (@(Get-ChildItem -LiteralPath $fixture -Recurse -Force -File | Sort-Object FullName | ForEach-Object { [IO.Path]::GetRelativePath($fixture,$_.FullName) + ':' + (Get-FileHash -LiteralPath $_.FullName).Hash }) -join '|')
}
$fixtureBefore = Fixture-Hashes
Run-Setup
[void](Assert-F1Bundle)
if ((Fixture-Hashes) -ne $fixtureBefore) { throw 'InstallerNeverTouchesUserShelfSuite failed' }
Write-Output 'PASS InstallerNeverTouchesUserShelfSuite.'
foreach ($path in @('Rainmeter.exe','Rainmeter.dll','SkinInstaller.exe','RestartRainmeter.exe','Uninstall.exe','LICENSE','Defaults/Skins/illustro','Languages/1033.dll','Notices/WebView2-LICENSE.txt','Notices/WebView2-NOTICE.txt','Notices/ShelfSuite-LICENSE.txt','Notices/WebView2-runtime-manifest.json')) {
 if (-not (Test-Path "$installed/$path")) { throw "Missing installed file: $path" }
}
if (Test-Path "$installed/RainmeterCafeMaintenance.exe") { throw 'Obsolete UAC helper packaged' }
$runtimeManifest = Get-Content "$installed/Notices/WebView2-runtime-manifest.json" -Raw | ConvertFrom-Json
$runtimePin = Get-Content "$repo/Build/CafeDependencies/runtime.json" -Raw | ConvertFrom-Json
if ($runtimeManifest.Sha256 -ne $runtimePin.sha256 -or $runtimeManifest.SignerThumbprint -ne $runtimePin.signerThumbprint -or
 $runtimeManifest.InstallerFileVersion -ne $runtimePin.installerFileVersion -or $runtimeManifest.PSObject.Properties.Name -contains 'Path') {
 throw 'Runtime provenance is missing, incorrect or contains a builder-local path'
}
if (Test-Path "$installed/MicrosoftEdgeWebView2RuntimeInstallerX64.exe") { throw 'Temporary prerequisite installer retained in application directory' }
Write-Output 'PASS: installed SDK/ShelfSuite notices and exact Runtime provenance without builder-local paths.'
$key = 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock'
if (-not (Test-Path $key)) { throw 'Uninstall registration missing' }
$link = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Rainmeter Cafe Lock/Rainmeter Cafe Lock.lnk'
if (-not (Test-Path $link)) { throw 'Start menu shortcut missing' }
if (-not (Test-Path $startupLink)) { throw 'Sign-in startup shortcut missing' }
& "$repo/RunAsStandard.exe" "$env:ProgramFiles/PowerShell/7/pwsh.exe" -NoProfile -File "$PSCommandPath" -Standard
if ($LASTEXITCODE -ne 0) { throw 'Installed standard-user tests failed' }
$before = Get-FileHash "$profile/installer-preservation-test.txt"
# Refuse replacement while an image is running; operate only the test's own process.
$p = Start-Process "$installed/Rainmeter.exe" -WindowStyle Hidden -PassThru
try {
 Start-Sleep -Seconds 2
 $attempt = Start-Process $setup -ArgumentList '/S' -WindowStyle Hidden -Wait -PassThru
 if ($attempt.ExitCode -ne 1618 -or $p.HasExited) { throw 'Running-instance update refusal failed' }
} finally { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
Run-Setup
[void](Assert-F1Bundle)
if ((Fixture-Hashes) -ne $fixtureBefore) { throw 'UpgradePreservesExistingShelfSuiteAndBackup failed' }
Write-Output 'PASS UpgradePreservesExistingShelfSuiteAndBackup.'
if (-not (Test-Path $startupLink)) { throw 'Upgrade lost automatic startup' }
if ((Get-FileHash "$profile/installer-preservation-test.txt").Hash -ne $before.Hash) { throw 'Upgrade modified profile data' }
# Unknown installation files are preserved by the generated uninstall manifest.
'preserve unknown file' | Set-Content "$installed/unknown-test.txt"
$uninstall = Start-Process "$installed/Uninstall.exe" -ArgumentList '/S',"_?=$installed" -WindowStyle Hidden -Wait -PassThru
if ($uninstall.ExitCode -ne 0) { throw 'Uninstall failed' }
if (Test-Path "$installed/Compatibility/ShelfSuiteF1") { throw 'Uninstall left owned F1 bundle data' }
if ((Fixture-Hashes) -ne $fixtureBefore) { throw 'UninstallLeavesUserShelfSuiteAndBackup failed' }
Write-Output 'PASS UninstallLeavesUserShelfSuiteAndBackup.'
if (Test-Path "$installed/Rainmeter.exe") { throw 'Uninstall left the application binary' }
if (Test-Path $key) { throw 'Uninstall left registration' }
if (Test-Path $link) { throw 'Uninstall left shortcut' }
if (Test-Path $startupLink) { throw 'Uninstall left automatic startup' }
if (-not (Test-Path "$installed/unknown-test.txt")) { throw 'Uninstall removed an unknown file' }
if ((Get-FileHash "$profile/installer-preservation-test.txt").Hash -ne $before.Hash) { throw 'Uninstall removed profile data' }
Write-Output 'PASS: install; standard-user runtime/password regressions; running update refusal; upgrade; safe uninstall; profile preservation.'
