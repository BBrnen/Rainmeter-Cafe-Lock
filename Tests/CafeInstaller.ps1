param([switch]$Standard)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Installer integration test runs only on a disposable GitHub runner' }
$repo = (Resolve-Path "$PSScriptRoot/..").Path
$installed = Join-Path $env:ProgramW6432 'Rainmeter Cafe Lock'
$profile = Join-Path $env:APPDATA 'Rainmeter Cafe Lock'
$setup = Join-Path $repo 'dist/Rainmeter-Cafe-Lock-4.5.26.3894-x64-Setup.exe'
function Run-Setup {
 $p = Start-Process $setup -ArgumentList '/S' -WindowStyle Hidden -Wait -PassThru
 if ($p.ExitCode -ne 0) { throw "Setup failed: $($p.ExitCode)" }
}
if ($Standard) {
 $probe = Join-Path $installed 'customer-write-probe.txt'
 $denied = $false
 try { [IO.File]::WriteAllText($probe,'must not succeed') } catch [UnauthorizedAccessException] { $denied = $true }
 if (-not $denied) { throw 'Standard user could write the program directory' }
 $denied = $false
 try { $f = [IO.File]::Open("$installed/Rainmeter.dll",[IO.FileMode]::Open,[IO.FileAccess]::Write); $f.Dispose() } catch [UnauthorizedAccessException] { $denied = $true }
 if (-not $denied) { throw 'Standard user could overwrite Rainmeter.dll' }
 # Test normal Start-menu-style launch without an explicit INI override.
 $p = Start-Process "$installed/Rainmeter.exe" -WindowStyle Hidden -PassThru
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
  Write-Output 'PASS: protected program files; installed standard-user default-profile launch.'
 } finally { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
 & "$env:ProgramFiles/PowerShell/7/pwsh.exe" -NoProfile -File "$repo/Tests/CafeLockSmoke.ps1" -BuildDirectory $installed -StandardUser -Maintenance
 if ($LASTEXITCODE -ne 0) { throw 'Installed password/runtime regression failed' }
 exit 0
}
if (Test-Path $installed) { throw 'Runner already has Cafe Lock installed; refusing to alter it' }
if (Test-Path $profile) { throw 'Runner already has a Cafe Lock profile' }
Run-Setup
foreach ($path in @('Rainmeter.exe','Rainmeter.dll','SkinInstaller.exe','RestartRainmeter.exe','Uninstall.exe','LICENSE','Defaults/Skins/illustro','Languages/1033.dll')) {
 if (-not (Test-Path "$installed/$path")) { throw "Missing installed file: $path" }
}
if (Test-Path "$installed/RainmeterCafeMaintenance.exe") { throw 'Obsolete UAC helper packaged' }
$key = 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock'
if (-not (Test-Path $key)) { throw 'Uninstall registration missing' }
$link = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'Rainmeter Cafe Lock/Rainmeter Cafe Lock.lnk'
if (-not (Test-Path $link)) { throw 'Start menu shortcut missing' }
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
if ((Get-FileHash "$profile/installer-preservation-test.txt").Hash -ne $before.Hash) { throw 'Upgrade modified profile data' }
# Unknown installation files are preserved by the generated uninstall manifest.
'preserve unknown file' | Set-Content "$installed/unknown-test.txt"
$uninstall = Start-Process "$installed/Uninstall.exe" -ArgumentList '/S',"_?=$installed" -WindowStyle Hidden -Wait -PassThru
if ($uninstall.ExitCode -ne 0) { throw 'Uninstall failed' }
if (Test-Path "$installed/Rainmeter.exe") { throw 'Uninstall left the application binary' }
if (Test-Path $key) { throw 'Uninstall left registration' }
if (Test-Path $link) { throw 'Uninstall left shortcut' }
if (-not (Test-Path "$installed/unknown-test.txt")) { throw 'Uninstall removed an unknown file' }
if ((Get-FileHash "$profile/installer-preservation-test.txt").Hash -ne $before.Hash) { throw 'Uninstall removed profile data' }
Write-Output 'PASS: install; standard-user runtime/password regressions; running update refusal; upgrade; safe uninstall; profile preservation.'
