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
$shortcutPath = Join-Path $root 'Original Café shortcut.lnk'
$workingDirectory = Join-Path $root 'Shortcut working directory'
New-Item -ItemType Directory -Path $workingDirectory | Out-Null
$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($shortcutPath)
$link.TargetPath = Join-Path $root 'AppOne\Probe.exe'
$link.Arguments = '"argument with spaces" /fixture'
$link.WorkingDirectory = $workingDirectory
$link.IconLocation = (Join-Path $env:WINDIR 'System32/shell32.dll') + ',3'
$link.Save()
[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link)
[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
$shortcutHash = (Get-FileHash -LiteralPath $shortcutPath -Algorithm SHA256).Hash
# These are new user configuration files in the disposable profile, not edits
# to the upstream skin, engine, themes, icons, or configurator.
# The pinned ASCII ShelfEngine runs through Rainmeter's legacy-codepage Lua
# bridge. Encode non-ASCII literal bytes explicitly; writing UTF-8 verbatim
# changes the path on that bridge. Storage must perform this same round-trip
# check and reject characters the active bridge cannot represent.
Add-Type @'
using System.Runtime.InteropServices;
public static class ShelfFixtureEncoding {
 [DllImport("kernel32.dll")] public static extern uint GetACP();
}
'@
[Text.Encoding]::RegisterProvider([Text.CodePagesEncodingProvider]::Instance)
$legacyEncoding = [Text.Encoding]::GetEncoding([int][ShelfFixtureEncoding]::GetACP(), [Text.EncoderFallback]::ExceptionFallback, [Text.DecoderFallback]::ExceptionFallback)
$pathBytes = $legacyEncoding.GetBytes($shortcutPath.Replace('\','/'))
$appOne = -join ($pathBytes | ForEach-Object { if ($_ -ge 128) { '\' + $_.ToString('D3') } else { [char]$_ } })
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
@"
ShelfConfig = {
  defaultIcon = 'file.png',
  tabs = {
    { name = 'ONLINE', items = {} },
    { name = 'OFFLINE', items = {} },
    { name = 'INTERNET', items = {} }
  }
}
"@ | Set-Content "$skins/Shelf Suite/Shelf2/config.lua"
@"
ShelfConfig = {
  defaultIcon = 'file.png',
  tabs = {
    { name = 'SCHOOL/WORK', items = {} },
    { name = 'SCHOOL/WORK 2', items = {} },
    { name = 'SCHOOL/WORK 3', items = {} },
    { name = 'SCHOOL/WORK 4', items = {} },
    { name = 'SCHOOL/WORK 5', items = {} }
  }
}
"@ | Set-Content "$skins/Shelf Suite/Shelf3/config.lua"
& "$PSScriptRoot/../CafeShelfNameProbe.exe" "$skins/Shelf Suite/Shelf1/config.lua"
if($LASTEXITCODE -ne 0){throw 'Native name encoding/rejection probe failed'}
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
function Read-ShelfLayout($target) {
  $snapshot = Join-Path $root (([guid]::NewGuid()).ToString() + '.layout')
  $luaPath = $snapshot.Replace('\','/')
  $lua = "local f=assert(io.open('$luaPath','w')); local function v(n) local m=SKIN:GetMeter(n); return m and (tostring(m:GetX())..','..tostring(m:GetW())) or '' end; f:write(tostring(SKIN:GetW())..'|'..v('MeterSettingsGear')..'|'..v('MeterTab1Bg')..'|'..v('MeterTab1Text')..'|'..v('MeterTab2Bg')..'|'..v('MeterTab2Text')..'|'..v('MeterTab3Bg')..'|'..v('MeterTab3Text')..'|'..v('MeterTab4Bg')..'|'..v('MeterTab5Bg')); f:close()"
  [LockNative]::Bang($target,"!CommandMeasure MeasureEngine `"$lua`"")
  Wait-For { (Test-Path $snapshot) -and (Get-Item $snapshot).Length -gt 0 } 'ShelfSuite layout snapshot'
  $parts = (Get-Content $snapshot -Raw).Trim().Split('|')
  function Pair($value) { $pair=$value.Split(','); return [pscustomobject]@{ X=[int][double]$pair[0]; W=[int][double]$pair[1] } }
  return [pscustomobject]@{
    Width=[int][double]$parts[0]; Gear=(Pair $parts[1]); Tab1=(Pair $parts[2]); Text1=(Pair $parts[3]);
    Tab2=(Pair $parts[4]); Text2=(Pair $parts[5]); Tab3=(Pair $parts[6]); Text3=(Pair $parts[7]);
    Tab4=(Pair $parts[8]); Tab5=(Pair $parts[9])
  }
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
  if ([LockNative]::FindWindow('RainmeterCafeShelfEditor',$null) -ne [IntPtr]::Zero) { throw 'Locked gear opened the hosted editor' }
}
$process = $null
try {
  # Compare the meter's actual shell dispatch to opening the same shortcut.
  Start-Process -FilePath $shortcutPath
  Assert-StandardLaunch 'AppOne'
  $shortcutBaseline = Get-Content -LiteralPath "$root/AppOne/launch-details.txt" -Raw
  if (-not $shortcutBaseline.Contains("cwd=$workingDirectory") -or -not $shortcutBaseline.Contains('"argument with spaces" /fixture')) {
    throw 'Windows shortcut baseline did not preserve fixture working directory and arguments'
  }
  Remove-Item -LiteralPath "$root/AppOne/launched.txt"
  Remove-Item -LiteralPath "$root/AppOne/launch-details.txt"
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
  $shortWindow = [LockNative]::FindWindow('RainmeterMeterWindow',"$skins\Shelf Suite\Shelf2\Shelf.ini")
  $longWindow = [LockNative]::FindWindow('RainmeterMeterWindow',"$skins\Shelf Suite\Shelf3\Shelf.ini")
  $tray = [LockNative]::FindWindow('RainmeterTrayClass',$null)
  $control = [LockNative]::FindWindow('DummyRainWClass','Rainmeter control window')
  Wait-For { (Read-ShelfState) -eq "1|65|O'Brien <Cafe>" } 'initial literal name through real stock meters'
  Write-Output 'PASS: native-saved quotes/HTML-like literal name survives loaded ShelfSuite/Rainmeter layers.'
  $shortLayout = Read-ShelfLayout $shortWindow
  Write-Output "ShelfSuite short layout: width=$($shortLayout.Width), gear=$($shortLayout.Gear.X), tab1=$($shortLayout.Tab1.X),$($shortLayout.Tab1.W), tab2=$($shortLayout.Tab2.X),$($shortLayout.Tab2.W)"
  if ($shortLayout.Width -ne 480 -or $shortLayout.Tab1.W -ne 85 -or $shortLayout.Tab2.X -lt ($shortLayout.Tab1.X + $shortLayout.Tab1.W + 10) -or $shortLayout.Gear.X -ne ($shortLayout.Width - 35)) {
    throw 'Short ShelfSuite tabs did not retain the 85 px minimum, spacing, 480 px shelf, and aligned gear'
  }
  $longLayout = Read-ShelfLayout $longWindow
  Write-Output "ShelfSuite long layout: width=$($longLayout.Width), gear=$($longLayout.Gear.X), tab1=$($longLayout.Tab1.X),$($longLayout.Tab1.W), text1=$($longLayout.Text1.W), tab2=$($longLayout.Tab2.X),$($longLayout.Tab2.W), tab4=$($longLayout.Tab4.X),$($longLayout.Tab4.W), tab5=$($longLayout.Tab5.X),$($longLayout.Tab5.W)"
  if ($longLayout.Tab1.W -lt ($longLayout.Text1.W + 24) -or $longLayout.Tab2.X -lt ($longLayout.Tab1.X + $longLayout.Tab1.W + 10)) {
    throw 'Long ShelfSuite tab did not fit SCHOOL/WORK with 12 px side padding and spacing'
  }
  if ($longLayout.Width -le 480 -or $longLayout.Tab5.X -lt ($longLayout.Tab4.X + $longLayout.Tab4.W + 10) -or $longLayout.Gear.X -ne ($longLayout.Width - 35)) {
    throw 'Five long ShelfSuite tabs did not expand the shelf and keep the gear aligned'
  }
  Write-Output 'PASS: ShelfSuite tab layout measures text, preserves the short 480 px layout, prevents overlap, and expands only when five long tabs require it.'
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
  if ((Get-Content -LiteralPath "$root/AppOne/launch-details.txt" -Raw) -ne $shortcutBaseline) {
    throw 'ShelfSuite shortcut launch differs from opening the same shortcut in Windows'
  }
  if ((Get-FileHash -LiteralPath $shortcutPath -Algorithm SHA256).Hash -ne $shortcutHash) {
    throw 'Shortcut launch changed original shortcut bytes'
  }
  Write-Output 'PASS: real locked ShelfSuite meter preserves original shortcut arguments, working directory and standard-user token.'
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
  [void](Read-ShelfState)
  if (Test-Path "$HtmlProbeDirectory/launched.txt") { throw 'Maintenance gear escaped to an external HTML handler instead of the guarded editor' }
  Wait-For {
    [LockNative]::FindWindow('RainmeterCafeShelfEditor',$null) -ne [IntPtr]::Zero -or
    [LockNative]::FindWindow('#32770','Rainmeter Cafe Lock - ShelfSuite') -ne [IntPtr]::Zero
  } 'native editor or actionable runtime failure'
  $runtimeError = [LockNative]::FindWindow('#32770','Rainmeter Cafe Lock - ShelfSuite')
  if ($runtimeError -ne [IntPtr]::Zero) {
    [void][LockNative]::Send($runtimeError,0x10,0,0)
    Write-Output 'NOTE: native runtime error shown; this runner has not verified the hosted browser UI.'
  }
  # Ordinary HTML must still use its Windows association through Rainmeter.
  [LockNative]::Bang($control,('["' + $root + '\control.html"]'))
  Assert-StandardLaunch 'HtmlHandler'
  Remove-Item -LiteralPath "$HtmlProbeDirectory/launched.txt"
  [LockNative]::Bang($control,'!Move 160 160 "Shelf Suite\Shelf1"')
  Wait-For { (Position $window) -eq '160,160' } 'ShelfSuite maintenance movement'
  [void][LockNative]::Send($tray,0x111,4091,0)
  Wait-For { [LockNative]::FindWindow('RainmeterCafeShelfEditor',$null) -eq [IntPtr]::Zero } 'Lock Now closes hosted editor'
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
  Write-Output "PASS: ShelfSuite gear uses native host after password unlock; ordinary HTML still launches non-elevated; Lock Now closes the host and blocks the gear; all upstream files unchanged. Revision: $revision"
} catch {
  $lastState = Get-ChildItem $root -Filter '*.state' -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($lastState) { Write-Output "Last ShelfSuite state: $(Get-Content $lastState.FullName -Raw)" }
  Get-ChildItem $root -Filter '*.event' -File | ForEach-Object { Write-Output "$($_.Name): $(Get-Content $_.FullName -Raw)" }
  Get-ChildItem $root -Filter '*.log' -File | ForEach-Object { Get-Content $_.FullName -Tail 20 }
  throw
} finally {
  if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
