param([string]$BuildDirectory = "$PSScriptRoot/../x64-Release")
$ErrorActionPreference = 'Stop'
$build = (Resolve-Path $BuildDirectory).Path
$root = Join-Path $env:RUNNER_TEMP ('CafeLock-' + [guid]::NewGuid())
$skins = Join-Path $root 'Skins'
$fixture = Join-Path $skins 'LockTest'
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$ini = Join-Path $root 'Rainmeter.ini'
@"
[Rainmeter]
SkinPath=$skins\
DisableVersionCheck=1
DisableAutoUpdate=1
DisableDragging=0
TrayIcon=1
Logging=1
[LockTest]
Active=1
WindowX=100
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
LeftMouseUpAction=[!CommandMeasure Script "Tab()"]["#CURRENTPATH#Launch.cmd"]
MouseOverAction=[!CommandMeasure Script "Mark('hover')"]
MouseLeaveAction=[!CommandMeasure Script "Mark('leave')"]
'@ | Set-Content (Join-Path $fixture 'Test.ini')
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
"@echo off`r`necho ok>`"$fixture\launched.txt`"`r`n" | Set-Content (Join-Path $fixture 'Launch.cmd')
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class LockNative {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] struct CopyData { public IntPtr Id; public int Size; public IntPtr Data; }
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w, out Rect r);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
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
  $tray = [LockNative]::FindWindow('RainmeterTrayClass', $null)
  $control = [LockNative]::FindWindow('DummyRainWClass', 'Rainmeter control window')
  Wait-For { Test-Path "$fixture/updates.txt" } 'Lua timer updates'
  $before = Position $window
  foreach ($keys in @(@(), @(0x11), @(0x10), @(0x12), @(0x11,0x12), @(0x11,0x10,0x12))) {
    try {
      foreach ($key in $keys) { [LockNative]::keybd_event($key,0,0,[UIntPtr]::Zero) }
      $hit = [LockNative]::Send($window,0x84,0,(110 -bor (110 -shl 16)))
      if ($hit.ToInt64() -ne 1) { throw 'Locked skin exposed a drag caption' }
      [void][LockNative]::Send($window,0x112,0xF012,0) # direct SC_MOVE bypass
      [void][LockNative]::Send($window,0x201,1,(10 -bor (10 -shl 16)))
      [void][LockNative]::Send($window,0x202,0,(10 -bor (10 -shl 16)))
      [void][LockNative]::Send($window,0x100,0x27,0) # selected/group keyboard move
      [void][LockNative]::Send($window,0x7b,0,-1) # keyboard context menu
      if ((Position $window) -ne $before) { throw 'Skin moved with modifier keys' }
    } finally { foreach ($key in $keys) { [LockNative]::keybd_event($key,0,2,[UIntPtr]::Zero) } }
  }
  Wait-For { Test-Path "$fixture/down.txt" } 'left-down action'
  Wait-For { Test-Path "$fixture/tab.txt" } 'Lua tab/meter actions'
  Wait-For { Test-Path "$fixture/launched.txt" } 'ordinary application launch'
  [void][LockNative]::SetCursorPos(110,110)
  [void][LockNative]::Send($window,0x200,0,(10 -bor (10 -shl 16)))
  Wait-For { Test-Path "$fixture/hover.txt" } 'hover action'
  [void][LockNative]::SetCursorPos(600,600)
  [void][LockNative]::Send($window,0x2A3,0,0)
  Wait-For { Test-Path "$fixture/leave.txt" } 'mouse-leave action'
  $commands = @('!Manage','!EditSkin LockTest Test.ini','!DeactivateConfig LockTest','!DeactivateConfigGroup LockGroup',
    '!ToggleConfig LockTest Test.ini','!ActivateConfig LockTest Test.ini','!LoadLayout Missing',
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
  foreach ($mouse in @(0x202,0x203,0x204,0x205)) { [void][LockNative]::Send($tray,1125,0,$mouse) }
  [LockNative]::Bang($window, '!CommandMeasure Script "Mark(''fence'')"')
  Wait-For { Test-Path "$fixture/fence.txt" } 'command processing after blocked management'
  Start-Sleep -Milliseconds 800
  if (-not [LockNative]::IsWindow($window) -or $process.HasExited) { throw 'Management unloaded the skin or quit' }
  if ((Position $window) -ne $before) { throw 'Management command moved skin' }
  if ([LockNative]::FindWindow('#32768',$null) -ne [IntPtr]::Zero) { throw 'Context menu opened' }
  if ([LockNative]::FindWindow('#32770','Manage Rainmeter') -ne [IntPtr]::Zero) { throw 'Manage dialog opened' }
  if ((Get-Content $ini -Raw) -match 'CafeLock=0') { throw 'Configuration write was accepted' }
  Write-Output 'PASS: startup; modifier drag/keyboard/menu guards; management via skin/main IPC and tray; clicks; Lua tabs/updates; hover; app launch.'
} finally {
  if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
