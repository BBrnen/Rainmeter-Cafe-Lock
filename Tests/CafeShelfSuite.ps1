param(
  [Parameter(Mandatory=$true)][string]$ShelfSuiteDirectory,
  [Parameter(Mandatory=$true)][string]$HtmlProbeDirectory,
  [string]$BuildDirectory = "$PSScriptRoot/../x64-Release"
)
$ErrorActionPreference = 'Stop'
# Association changes and UI automation belong only on the disposable CI runner.
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Run this test only on a disposable GitHub Actions Windows runner' }
$build = (Resolve-Path $BuildDirectory).Path
$vendor = (Resolve-Path $ShelfSuiteDirectory).Path
# actions/checkout checks the pinned commit out detached. Read its identity
# without launching a console child from the restricted-token UI test process.
$revision = (Get-Content "$vendor/.git/HEAD" -Raw).Trim()
if ($revision -ne 'd4f186ba0b5c262c7559b80841132f5fd3884f3c') { throw 'Unexpected ShelfSuite revision' }
$root = Join-Path $env:RUNNER_TEMP ('CafeShelfSuite-' + [guid]::NewGuid())
$skins = Join-Path $root 'Skins'
New-Item -ItemType Directory $skins | Out-Null
Copy-Item -LiteralPath "$vendor/Shelf Suite" -Destination $skins -Recurse
$originals = @(Get-ChildItem "$skins/Shelf Suite" -Recurse -File | Get-FileHash -Algorithm SHA256)
foreach ($name in @('AppOne','AppTwo')) {
  New-Item -ItemType Directory "$root/$name" | Out-Null
  Copy-Item "$PSScriptRoot/../CafeLaunchProbe.exe" "$root/$name/Probe.exe"
}
# These are new user configuration files in the disposable profile, not edits
# to the upstream skin, engine, themes, icons, or configurator.
$appOne = "$root/AppOne/Probe.exe".Replace('\','/')
$appTwo = "$root/AppTwo/Probe.exe".Replace('\','/')
@"
ShelfConfig = {
  defaultIcon = 'file.png',
  tabs = {
    { name = 'First', items = {{ label = 'First app', action = '$appOne', icon = 'file.png' }} },
    { name = 'Second', items = {{ label = 'Second app', action = '$appTwo', icon = 'file.png' }} }
  }
}
"@ | Set-Content "$skins/Shelf Suite/Shelf1/config.lua"
$ini = Join-Path $root 'Rainmeter.ini'
@"
[Rainmeter]
SkinPath=$skins\
DisableVersionCheck=1
DisableAutoUpdate=1
TrayIcon=1
Logging=1
[Shelf Suite\Shelf1]
Active=1
WindowX=100
WindowY=100
Draggable=1
AlwaysOnTop=2
[Shelf Suite\Shelf2]
Active=1
WindowX=650
WindowY=100
[Shelf Suite\Shelf3]
Active=1
WindowX=100
WindowY=350
"@ | Set-Content $ini
. "$PSScriptRoot/CafeTestUi.ps1"
function Click-Shelf([int]$x,[int]$y) {
  $r = [LockNative+Rect]::new()
  [void][LockNative]::GetWindowRect($window,[ref]$r)
  [void][LockNative]::SetCursorPos($r.Left+$x,$r.Top+$y)
  [void][LockNative]::Send($window,0x200,0,($x -bor ($y -shl 16)))
  [void][LockNative]::Send($window,0x201,1,($x -bor ($y -shl 16)))
  [void][LockNative]::Send($window,0x202,0,($x -bor ($y -shl 16)))
}
function Read-ShelfState {
  $snapshot = Join-Path $root (([guid]::NewGuid()).ToString() + '.state')
  $luaPath = $snapshot.Replace('\','/')
  # Introspection through the existing Lua API; the upstream engine is untouched.
  $lua = "local f=assert(io.open('$luaPath','w')); f:write(tostring(ActiveTab)..'|'..tostring(SKIN:GetMeter('MeterIcon1'):GetY())..'|'..SKIN:GetMeter('MeterIcon1Text'):GetOption('Text')); f:close()"
  [LockNative]::Bang($window,"!CommandMeasure MeasureEngine `"$lua`"")
  Wait-For { (Test-Path $snapshot) -and (Get-Item $snapshot).Length -gt 0 } 'stock Lua engine snapshot'
  return (Get-Content $snapshot -Raw)
}
function Assert-StandardLaunch($folder) {
  $probeFolder = if ($folder -eq 'HtmlHandler') { $HtmlProbeDirectory } else { "$root/$folder" }
  Wait-For { Test-Path "$probeFolder/launched.txt" } "$folder launch"
  if ((Get-Content "$probeFolder/launched.txt" -Raw) -ne 'standard') { throw "$folder was elevated" }
}
function Assert-GearBlocked {
  $marker = "$HtmlProbeDirectory/launched.txt"
  if (Test-Path $marker) { Remove-Item -LiteralPath $marker }
  Click-Shelf 452 20
  [void](Read-ShelfState) # Fence the skin's command queue after the click.
  Start-Sleep -Milliseconds 800
  if (Test-Path $marker) { throw 'Locked ShelfSuite gear launched the configurator' }
  if ([LockNative]::FindWindow('#32770','Manage Rainmeter') -ne [IntPtr]::Zero) { throw 'Gear bypassed maintenance authorization' }
}
$process = $null
try {
  '<html>Association control</html>' | Set-Content "$root/control.html"
  Start-Process "$root/control.html"
  Assert-StandardLaunch 'HtmlHandler'
  Remove-Item -LiteralPath "$HtmlProbeDirectory/launched.txt"
  Write-Output 'PASS: standard-user control HTML dispatch reached the recorder.'

  $process = Start-Process "$build/Rainmeter.exe" -ArgumentList "`"$ini`"" -PassThru
  foreach ($shelf in 1..3) {
    $title = "$skins\Shelf Suite\Shelf$shelf\Shelf.ini"
    Wait-For { [LockNative]::FindWindow('RainmeterMeterWindow',$title) -ne [IntPtr]::Zero } "stock Shelf$shelf startup"
  }
  $window = [LockNative]::FindWindow('RainmeterMeterWindow',"$skins\Shelf Suite\Shelf1\Shelf.ini")
  $tray = [LockNative]::FindWindow('RainmeterTrayClass',$null)
  $control = [LockNative]::FindWindow('DummyRainWClass','Rainmeter control window')
  Wait-For { (Read-ShelfState) -eq '1|65|First app' } 'initial stock meters'
  $before = Position $window
  # Preserve each stock action, then observe its result in the same UI callback.
  # On a hosted desktop WM_MOUSELEAVE can arrive before an external snapshot.
  # No production action or upstream file is replaced by this instrumentation.
  $luaRoot = $root.Replace('\','/')
  $observe = @"
function CafeObserve(name)
  local f=assert(io.open('$luaRoot/'..name..'.event','w'));
  f:write(tostring(SKIN:GetMeter('MeterIcon1'):GetY())); f:close();
end
for _, pair in ipairs({{'MouseOverAction','hover'},{'MouseLeaveAction','leave'}}) do
  local original=SKIN:GetMeter('MeterIcon1'):GetOption(pair[1]);
  local observer='[!CommandMeasure MeasureEngine '..string.char(34)..'CafeObserve('..string.char(39)..pair[2]..string.char(39)..')'..string.char(34)..']';
  SKIN:Bang('!SetOption','MeterIcon1',pair[1],original..observer);
end
SKIN:Bang('!UpdateMeter','MeterIcon1');
"@
  [LockNative]::Bang($window,"!CommandMeasure MeasureEngine `"$($observe.Replace("`n",' ').Replace("`r",' '))`"")
  [void](Read-ShelfState)
  [void][LockNative]::SetCursorPos(155,185)
  [void][LockNative]::Send($window,0x200,0,(55 -bor (85 -shl 16)))
  Wait-For { Test-Path "$root/hover.event" } 'stock hover callback'
  if ((Get-Content "$root/hover.event" -Raw) -ne '62') { throw 'Stock hover action did not raise the icon' }
  Click-Shelf 55 85
  Assert-StandardLaunch 'AppOne'
  [void][LockNative]::SetCursorPos(900,700)
  [void][LockNative]::Send($window,0x2A3,0,0)
  Wait-For { Test-Path "$root/leave.event" } 'stock mouse-leave callback'
  if ((Get-Content "$root/leave.event" -Raw) -ne '65') { throw 'Stock leave action did not restore the icon' }
  Click-Shelf 145 25
  Wait-For { (Read-ShelfState) -eq '2|65|Second app' } 'stock tab switch and meter content'
  Click-Shelf 55 85
  Assert-StandardLaunch 'AppTwo'
  [void][LockNative]::Send($window,0x100,0x27,0)
  [void][LockNative]::Send($window,0x112,0xF012,0)
  [void][LockNative]::Send($window,0x7b,0,-1)
  if ((Position $window) -ne $before) { throw 'Locked ShelfSuite moved' }
  if ([LockNative]::FindWindow('#32768',$null) -ne [IntPtr]::Zero) { throw 'Locked ShelfSuite context menu opened' }
  Assert-GearBlocked
  Write-Output 'PASS: unchanged ShelfSuite shelves load; locked launcher icons, tabs, Lua/meter updates and hover work; app launches stay standard-user; movement/context menu/gear blocked.'

  [void][LockNative]::Send($tray,0x111,4090,0)
  $dialog = Password-Dialog 'Create Cafe Lock Password'
  $password = 'ShelfSuite disposable test password!'
  Submit-Password $dialog $password $password
  Wait-For { -not [LockNative]::IsWindow($dialog) } 'ShelfSuite maintenance password creation'
  Click-Shelf 452 20
  Assert-StandardLaunch 'HtmlHandler'
  [LockNative]::Bang($control,'!Move 160 160 "Shelf Suite\Shelf1"')
  Wait-For { (Position $window) -eq '160,160' } 'ShelfSuite maintenance movement'
  [void][LockNative]::Send($tray,0x111,4091,0)
  Assert-GearBlocked
  [LockNative]::Bang($control,'!Move 260 260 "Shelf Suite\Shelf1"')
  [void](Read-ShelfState)
  if ((Position $window) -ne '160,160') { throw 'Relocked ShelfSuite moved' }
  foreach ($original in $originals) {
    if ((Get-FileHash -LiteralPath $original.Path -Algorithm SHA256).Hash -ne $original.Hash) { throw "Upstream ShelfSuite file changed: $($original.Path)" }
  }
  foreach ($file in Get-ChildItem $root -Recurse -File | Where-Object { $_.Extension -in '.ini','.log' }) {
    if ((Get-Content -LiteralPath $file.FullName -Raw).Contains($password)) { throw 'Plaintext password in configuration/logs' }
  }
  Write-Output "PASS: ShelfSuite configurator dispatch allowed after password unlock, remains non-elevated, blocked again by Lock Now; all upstream files unchanged. Revision: $revision"
} catch {
  $lastState = Get-ChildItem $root -Filter '*.state' -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($lastState) { Write-Output "Last ShelfSuite state: $(Get-Content $lastState.FullName -Raw)" }
  Get-ChildItem $root -Filter '*.event' -File | ForEach-Object { Write-Output "$($_.Name): $(Get-Content $_.FullName -Raw)" }
  Get-ChildItem $root -Filter '*.log' -File | ForEach-Object { Get-Content $_.FullName -Tail 20 }
  throw
} finally {
  if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
