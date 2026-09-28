param([string]$BuildDirectory = "$PSScriptRoot/../x64-Release", [switch]$Maintenance, [switch]$StandardUser, [switch]$ExpectElevationUnavailable)
$ErrorActionPreference = 'Stop'
$build = (Resolve-Path $BuildDirectory).Path
$root = Join-Path $env:RUNNER_TEMP ('CafeLock-' + [guid]::NewGuid())
$skins = Join-Path $root 'Skins'
$fixture = Join-Path $skins 'LockTest'
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
New-Item -ItemType Directory -Path "$root/Layouts/Replacement" -Force | Out-Null
"[Rainmeter]`nSkinPath=$skins\`n" | Set-Content "$root/Layouts/Replacement/Rainmeter.ini"
$ini = Join-Path $root 'Rainmeter.ini'
@"
[Rainmeter]
SkinPath=$skins\
DisableVersionCheck=1
DisableAutoUpdate=1
DisableDragging=0
TrayIcon=1
Logging=1
ConfigEditor=$root\Editor.exe
[LockTest]
Active=1
WindowX=100
WindowY=100
Draggable=1
[LockPeer]
Active=1
WindowX=400
WindowY=100
Draggable=1
"@ | Set-Content $ini
@'
[Rainmeter]
Update=100
AccurateText=1
BackgroundMode=2
SolidColor=30,30,30,255
Group=LockGroup
DragGroup=LockGroup
[Script]
Measure=Script
ScriptFile=#CURRENTPATH#Test.lua
[Button]
Meter=String
Text=Lock test button
W=240
H=80
SolidColor=70,70,70,255
LeftMouseDownAction=[!CommandMeasure Script "Mark('down')"]
LeftMouseUpAction=[!CommandMeasure Script "Tab()"]["#CURRENTPATH#Launch.exe"]
MouseOverAction=[!CommandMeasure Script "Mark('hover')"]
MouseLeaveAction=[!CommandMeasure Script "Mark('leave')"]
'@ | Set-Content (Join-Path $fixture 'Test.ini')
"[Rainmeter]`nUpdate=1000`n[Label]`nMeter=String`nText=Alternate variant" | Set-Content "$fixture/ZAlternate.ini"
@'
function Mark(name)
  local f = assert(io.open(SKIN:GetVariable('CURRENTPATH') .. name .. '.txt', 'w'))
  f:write('ok')
  f:close()
end
function Initialize() count = 0 end
function Update()
  count = count + 1
  if count == 3 then Mark('updates') end
  return count
end
function Tab()
  SKIN:Bang('!SetVariable', 'ActiveTab', '2')
  SKIN:Bang('!SetOption', 'Button', 'Text', 'Tab two')
  SKIN:Bang('!UpdateMeter', 'Button')
  SKIN:Bang('!Redraw')
  if SKIN:GetVariable('ActiveTab') == '2' then Mark('tab') end
end
'@ | Set-Content (Join-Path $fixture 'Test.lua')
Copy-Item "$PSScriptRoot/../CafeLaunchProbe.exe" "$fixture/Launch.exe"
Copy-Item "$PSScriptRoot/../CafeLaunchProbe.exe" "$root/Editor.exe"
Copy-Item -LiteralPath $fixture -Destination "$skins/LockPeer" -Recurse
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class LockNative {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] struct CopyData { public IntPtr Id; public int Size; public IntPtr Data; }
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w, out Rect r);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,uint msg,IntPtr wp,IntPtr lp);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
 [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int key);
 [StructLayout(LayoutKind.Sequential)] struct IconId { public uint Size; public IntPtr Window; public uint Id; public Guid Guid; }
 [DllImport("shell32.dll")] static extern int Shell_NotifyIconGetRect(ref IconId id, out Rect rect);
 public static bool HasTrayIcon(IntPtr window) {
   var id = new IconId {Size=(uint)Marshal.SizeOf(typeof(IconId)),Window=window,Id=100};
   Rect rect; return Shell_NotifyIconGetRect(ref id,out rect)==0;
 }
 [DllImport("user32.dll", SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr w,uint msg,IntPtr wp,IntPtr lp,uint flags,uint ms,out IntPtr result);
 public static IntPtr Send(IntPtr w,uint msg,long wp,long lp) {
   IntPtr result; if(SendMessageTimeout(w,msg,new IntPtr(wp),new IntPtr(lp),2,2000,out result)==IntPtr.Zero)
     throw new Exception("Window message failed or timed out: " + msg);
   return result;
 }
 public static void Bang(IntPtr w,string text) {
   IntPtr data=Marshal.StringToHGlobalUni(text), ptr=IntPtr.Zero;
   try {
     CopyData cds=new CopyData {Id=new IntPtr(1),Size=(text.Length+1)*2,Data=data};
     ptr=Marshal.AllocHGlobal(Marshal.SizeOf(cds)); Marshal.StructureToPtr(cds,ptr,false);
     Send(w,0x4a,0,ptr.ToInt64());
   } finally { if(ptr!=IntPtr.Zero) Marshal.FreeHGlobal(ptr); Marshal.FreeHGlobal(data); }
 }
}
'@
function Wait-For($predicate, $description) {
  $deadline = [DateTime]::UtcNow.AddSeconds(15)
  while (-not (& $predicate)) {
    if ([DateTime]::UtcNow -gt $deadline) { throw "Timed out: $description" }
    Start-Sleep -Milliseconds 100
  }
}
function Position($window) {
  $r = [LockNative+Rect]::new()
  if (-not [LockNative]::GetWindowRect($window, [ref]$r)) { throw 'Missing skin window' }
  return "$($r.Left),$($r.Top)"
}
$process = $null
try {
  $process = Start-Process (Join-Path $build 'Rainmeter.exe') -ArgumentList "`"$ini`"" -PassThru
  $title = Join-Path $fixture 'Test.ini'
  Wait-For { [LockNative]::FindWindow('RainmeterMeterWindow', $title) -ne [IntPtr]::Zero } 'skin startup'
  $window = [LockNative]::FindWindow('RainmeterMeterWindow', $title)
  $peerTitle = Join-Path "$skins/LockPeer" 'Test.ini'
  Wait-For { [LockNative]::FindWindow('RainmeterMeterWindow',$peerTitle) -ne [IntPtr]::Zero } 'peer skin startup'
  $peer = [LockNative]::FindWindow('RainmeterMeterWindow',$peerTitle)
  $peerBefore = Position $peer
  $tray = [LockNative]::FindWindow('RainmeterTrayClass', $null)
  $control = [LockNative]::FindWindow('DummyRainWClass', 'Rainmeter control window')
  Wait-For { Test-Path "$fixture/updates.txt" } 'Lua timer updates'
  $before = Position $window
  # A filtered process cannot reliably inject keys into the runner's elevated
  # foreground console. Run those input checks in the ordinary runner pass;
  # the standard-user pass independently exercises the real authorization/app tokens.
  $combinations = @(@(), @(0x11), @(0x10), @(0x12), @(0x11,0x12), @(0x11,0x10,0x12))
  if ($StandardUser) { $combinations = ,@() }
  foreach ($keys in $combinations) {
    try {
      foreach ($key in $keys) { [LockNative]::keybd_event($key,0,0,[UIntPtr]::Zero) }
      foreach ($key in $keys) { Wait-For { [LockNative]::GetAsyncKeyState($key) -lt 0 } 'modifier injection' }
      $hit = [LockNative]::Send($window,0x84,0,(110 -bor (110 -shl 16)))
      if ($hit.ToInt64() -ne 1) { throw 'Locked skin exposed a drag caption' }
      [void][LockNative]::Send($window,0x112,0xF012,0) # direct SC_MOVE bypass
      [void][LockNative]::Send($window,0x201,1,(10 -bor (10 -shl 16)))
      [void][LockNative]::Send($window,0x202,0,(10 -bor (10 -shl 16)))
      [void][LockNative]::Send($window,0x100,0x27,0) # selected/group keyboard move
      [void][LockNative]::Send($window,0x7b,0,-1) # keyboard context menu
      if ((Position $window) -ne $before) { throw 'Skin moved with modifier keys' }
      if ((Position $peer) -ne $peerBefore) { throw 'Group peer moved with modifier keys' }
    } finally { foreach ($key in $keys) { [LockNative]::keybd_event($key,0,2,[UIntPtr]::Zero) } }
  }
  Wait-For { Test-Path "$fixture/down.txt" } 'left-down action'
  Wait-For { Test-Path "$fixture/tab.txt" } 'Lua tab/meter actions'
  Wait-For { Test-Path "$fixture/launched.txt" } 'ordinary application launch'
  if ($StandardUser -and (Get-Content "$fixture/launched.txt" -Raw) -ne 'standard') { throw 'Skin-launched application was elevated' }
  [void][LockNative]::SetCursorPos(110,110)
  [void][LockNative]::Send($window,0x200,0,(10 -bor (10 -shl 16)))
  Wait-For { Test-Path "$fixture/hover.txt" } 'hover action'
  [void][LockNative]::SetCursorPos(600,600)
  [void][LockNative]::Send($window,0x2A3,0,0)
  Wait-For { Test-Path "$fixture/leave.txt" } 'mouse-leave action'
  $commands = @('!Manage','!EditSkin LockTest Test.ini','!DeactivateConfig LockTest','!DeactivateConfigGroup LockGroup',
    '!ToggleConfig LockTest Test.ini','!ActivateConfig LockTest ZAlternate.ini','!LoadLayout Replacement',
    '!Move 400 400 LockTest','!SetWindowPosition 400 400 LockTest','!Draggable 1 LockTest',
    '!DraggableGroup 1 LockGroup','!SkinMenu LockTest','!SkinCustomMenu LockTest','!TrayMenu','!Quit',
    '!RainmeterManage','!Execute [!Manage][!DeactivateConfig LockTest]',
    "!WriteKeyValue Rainmeter CafeLock 0 `"$ini`"")
  foreach ($command in $commands) {
    [LockNative]::Bang($window, $command)
    [LockNative]::Bang($control, $command)
  }
  foreach ($id in @(4002,4003,4004,4007,4030,4053)) {
    [void][LockNative]::Send($window,0x111,$id,0)
    [void][LockNative]::Send($tray,0x111,$id,0)
  }
  foreach ($mouse in @(0x202,0x203,0x204)) { [void][LockNative]::Send($tray,1125,0,$mouse) }
  foreach ($handle in @($window,$tray,$control)) { [void][LockNative]::Send($handle,0x10,0,0) }
  [LockNative]::Bang($window, '!CommandMeasure Script "Mark(''fence'')"')
  Wait-For { Test-Path "$fixture/fence.txt" } 'command processing after blocked management'
  Start-Sleep -Milliseconds 800
  if (-not [LockNative]::IsWindow($window) -or $process.HasExited) { throw 'Management unloaded the skin or quit' }
  if (-not [LockNative]::IsWindow($peer)) { throw 'Management unloaded the group peer' }
  # Shell_NotifyIcon IPC to the hosted runner's elevated Explorer is blocked by UIPI
  # for the filtered token. The desktop pass checks the actual tray icon.
  if (-not $StandardUser -and -not [LockNative]::HasTrayIcon($tray)) { throw 'Restricted maintenance tray icon is missing' }
  if ((Position $window) -ne $before) { throw 'Management command moved skin' }
  if ([LockNative]::FindWindow('#32768',$null) -ne [IntPtr]::Zero) { throw 'Context menu opened' }
  if ([LockNative]::FindWindow('#32770','Manage Rainmeter') -ne [IntPtr]::Zero) { throw 'Manage dialog opened' }
  if (Test-Path "$root/editor-opened.txt") { throw 'Edit launched the configured editor' }
  if ((Get-Content $ini -Raw) -match 'CafeLock=0') { throw 'Configuration write was accepted' }
  Write-Output "PASS: locked command/action regression; Lua tabs/updates; hover; app launch. Modifier injection included: $(-not $StandardUser)"
  # No window-message payload can substitute for a live authenticated helper.
  1..5 | ForEach-Object { [void][LockNative]::Send($control,0x113,4092,1) }
  [LockNative]::Bang($control,'!Move 150 150 LockTest')
  Start-Sleep -Milliseconds 300
  if ((Position $window) -ne $before) { throw 'Fake authorization notification unlocked Rainmeter' }
  if ($ExpectElevationUnavailable) {
    [void][LockNative]::PostMessage($tray,0x111,[IntPtr]4090,[IntPtr]::Zero)
    Wait-For {
      (Get-Content "$root/Rainmeter.log" -Raw) -match 'Helper exited before authorization \(2\)'
    } 'non-elevated helper refuses authorization on the restricted hosted runner'
    [LockNative]::Bang($control,'!Move 450 450 LockTest')
    Start-Sleep -Milliseconds 300
    if ((Position $window) -ne $before) { throw 'Non-elevated helper unlocked Rainmeter' }
    Write-Output 'PASS: standard-user Rainmeter and launcher; unavailable elevation fails closed. Real credential-based UAC remains a manual test.'
  }
  if ($Maintenance) {
    # Hosted runner is already elevated: this covers the real helper handshake,
    # not the human interaction with a UAC secure-desktop credential dialog.
    [void][LockNative]::PostMessage($tray,0x111,[IntPtr]4090,[IntPtr]::Zero)
    Wait-For {
      [LockNative]::Bang($control,'!Move 150 150 LockTest')
      (Position $window) -eq '150,150'
    } 'elevated helper authorizes maintenance and movement resumes'
    [void][LockNative]::PostMessage($window,0x7b,[IntPtr]::Zero,[IntPtr](-1))
    Wait-For { [LockNative]::FindWindow('#32768',$null) -ne [IntPtr]::Zero } 'maintenance context menu'
    [void][LockNative]::Send($window,0x1f,0,0)
    Wait-For { [LockNative]::FindWindow('#32768',$null) -eq [IntPtr]::Zero } 'context menu dismissal'
    [LockNative]::Bang($control,'!Manage')
    Wait-For { [LockNative]::FindWindow('#32770','Manage Rainmeter') -ne [IntPtr]::Zero } 'maintenance Manage dialog'
    [LockNative]::Bang($control,'!EditSkin LockTest Test.ini')
    Wait-For { Test-Path "$root/editor-opened.txt" } 'maintenance editor'
    if ($StandardUser -and (Get-Content "$root/editor-opened.txt" -Raw) -ne 'standard') { throw 'Maintenance elevated the editor' }
    [LockNative]::Bang($control,"!WriteKeyValue Rainmeter MaintenanceWrite yes `"$ini`"")
    Wait-For { (Get-Content $ini -Raw) -match 'MaintenanceWrite=yes' } 'maintenance configuration write'
    if (-not $StandardUser) {
    [LockNative]::keybd_event(0x11,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    try {
      $hit = [LockNative]::Send($window,0x84,0,(160 -bor (160 -shl 16)))
      if ($hit.ToInt64() -ne 2) { throw 'Maintenance Ctrl-drag was not restored' }
    } finally { [LockNative]::keybd_event(0x11,0,2,[UIntPtr]::Zero) }
    }
    [void][LockNative]::Send($tray,0x111,4091,0)
    Wait-For { [LockNative]::FindWindow('#32770','Manage Rainmeter') -eq [IntPtr]::Zero } 'Lock Now closes Manage'
    [LockNative]::Bang($control,'!Move 350 350 LockTest')
    [LockNative]::Bang($control,"!WriteKeyValue Rainmeter AfterLock forbidden `"$ini`"")
    Start-Sleep -Milliseconds 300
    if ((Position $window) -ne '150,150') { throw 'Lock Now failed to block movement' }
    if ((Get-Content $ini -Raw) -match 'AfterLock=forbidden') { throw 'Lock Now failed to block writes' }
    # Repeated approval, unload/activate, then restart from maintenance.
    [void][LockNative]::PostMessage($tray,0x111,[IntPtr]4090,[IntPtr]::Zero)
    Wait-For {
      [LockNative]::Bang($control,'!Move 160 160 LockTest')
      (Position $window) -eq '160,160'
    } 'second maintenance authorization'
    $restoredHit = [LockNative]::Send($window,0x84,0,(170 -bor (170 -shl 16)))
    if ($restoredHit.ToInt64() -ne 2) { throw 'Relock overwrote the saved draggable preference' }
    [LockNative]::Bang($control,'!DeactivateConfig LockPeer')
    Wait-For { -not [LockNative]::IsWindow($peer) } 'maintenance unload'
    [LockNative]::Bang($control,'!ActivateConfig LockPeer Test.ini')
    Wait-For { [LockNative]::FindWindow('RainmeterMeterWindow',"$skins\LockPeer\Test.ini") -ne [IntPtr]::Zero } 'maintenance activate'
    [LockNative]::Bang($control,'!Quit')
    Wait-For { $process.HasExited } 'maintenance normal exit'
    $process = Start-Process (Join-Path $build 'Rainmeter.exe') -ArgumentList "`"$ini`"" -PassThru
    Wait-For { [LockNative]::FindWindow('RainmeterMeterWindow',"$fixture\Test.ini") -ne [IntPtr]::Zero } 'restarted skin'
    $window = [LockNative]::FindWindow('RainmeterMeterWindow',"$fixture\Test.ini")
    $control = [LockNative]::FindWindow('DummyRainWClass','Rainmeter control window')
    $tray = [LockNative]::FindWindow('RainmeterTrayClass',$null)
    $beforeRestart = Position $window
    [LockNative]::Bang($control,'!Move 450 450 LockTest')
    Start-Sleep -Milliseconds 300
    if ((Position $window) -ne $beforeRestart) { throw 'Restart retained maintenance authorization' }
    Write-Output "PASS: real helper unlock; Manage/Edit/write; Lock Now; repeat authorization; Unload/Activate; process restart locked. Standard-user pass: $StandardUser"
  }
  # Exercise the documented Windows protocol without signing out/rebooting the runner.
  foreach ($reason in @(0,2147483648,1073741824)) {
    foreach ($handle in @($window,$tray,$control)) {
      if ([LockNative]::Send($handle,0x11,0,$reason).ToInt64() -ne 1) { throw 'Window vetoed Windows end-session query' }
      [void][LockNative]::Send($handle,0x16,0,$reason) # cancelled shutdown must keep running
    }
  }
  if ($process.HasExited) { throw 'Cancelled Windows shutdown closed Rainmeter' }
  [void][LockNative]::Send($control,0x16,1,2147483648)
  Wait-For { $process.HasExited } 'Windows confirmed sign-out closes locked Rainmeter'
  Write-Output 'PASS: shutdown/restart/logoff queries accepted; cancelled shutdown preserved session; confirmed logoff exits while locked.'

} catch {
  Get-ChildItem $root -Filter '*.log' -File -ErrorAction SilentlyContinue | ForEach-Object { Get-Content $_.FullName -Tail 50 }
  throw
} finally {
  if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
