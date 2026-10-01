param(
    [Parameter(Mandatory=$true)][string]$UpstreamDirectory,
    [string]$BuildDirectory = "$PSScriptRoot/../x64-Release"
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Use a disposable Windows runner for F1 native UI tests.' }
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$upstream = (Resolve-Path -LiteralPath $UpstreamDirectory).Path
$bundle = Join-Path $build 'Compatibility/ShelfSuiteF1'
$root = Join-Path $env:RUNNER_TEMP ('CafeF1Ui-' + [guid]::NewGuid().ToString('N'))
$skins = Join-Path $root 'Skins'
New-Item -ItemType Directory -Path $skins | Out-Null
Copy-Item -LiteralPath (Join-Path $upstream 'Shelf Suite') -Destination $skins -Recurse
$shelf = Join-Path $skins 'Shelf Suite'
# Fixture setup only: extra shelves reuse verified contents, never their number.
foreach ($number in @(4,27)) {
    Copy-Item -LiteralPath (Join-Path $shelf 'Shelf2') -Destination (Join-Path $shelf "Shelf$number") -Recurse
}
$targets = @('@Resources/ShelfEngine.lua','@Resources/Variables.inc') + @(1,2,3,4,27 | ForEach-Object { "Shelf$_/Shelf.ini" })
function Target-Hashes { return @($targets | ForEach-Object { (Get-FileHash -LiteralPath (Join-Path $shelf $_)).Hash }) }
function Backups { return @(Get-ChildItem -LiteralPath $skins -Directory -Filter 'Shelf Suite-F1-Backup-*') }
$before = Target-Hashes
# Unrelated data is held unreadable throughout the native operation. Test code
# does not inspect these files while the operation runs.
$config = [IO.File]::Open((Join-Path $shelf 'Shelf27/config.lua'), 'Open', 'Read', 'None')
$ini = Join-Path $root 'Rainmeter.ini'
@"
[Rainmeter]
SkinPath=$skins\
DisableVersionCheck=1
DisableAutoUpdate=1
TrayIcon=1
[Shelf Suite\Shelf1]
Active=1
WindowX=100
WindowY=100
[Shelf Suite\Shelf2]
Active=1
WindowX=100
WindowY=350
"@ | Set-Content -LiteralPath $ini
. "$PSScriptRoot/CafeTestUi.ps1"
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class F1UiNative {
 [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr window);
}
'@
function Click($window) {
    if ($window -eq [IntPtr]::Zero) { throw 'Missing F1 UI control.' }
    [void][LockNative]::PostMessage($window,0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
}
function Result-Dialog([string]$status) {
    $dialog = Password-Dialog ("ShelfSuite F1 - " + $status)
    [void][LockNative]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)
    Wait-For { -not [LockNative]::IsWindow($dialog) } 'F1 result dismissed'
}
function Open-Preview {
    [LockNative]::Bang($control,'!Manage Settings')
    $manage = Password-Dialog 'Manage Rainmeter'
    Wait-For { [LockNative]::Child($manage,'Apply ShelfSuite F1 compatibility') -ne [IntPtr]::Zero } 'F1 Settings button'
    $button = [LockNative]::Child($manage,'Apply ShelfSuite F1 compatibility')
    $script:settings = [F1UiNative]::GetParent($button)
    Click $button
}
$process = $null
try {
    $process = Start-Process -FilePath (Join-Path $build 'Rainmeter.exe') -ArgumentList ('"' + $ini + '"') -PassThru -WindowStyle Hidden
    Wait-For { [LockNative]::FindWindow('RainmeterTrayClass',$null) -ne [IntPtr]::Zero } 'F1 runtime startup'
    $tray = [LockNative]::FindWindow('RainmeterTrayClass',$null)
    $control = [LockNative]::FindWindow('DummyRainWClass','Rainmeter control window')
    Wait-For { [LockNative]::FindWindow('RainmeterMeterWindow',"$shelf\Shelf1\Shelf.ini") -ne [IntPtr]::Zero } 'loaded stock Shelf1'
    $window = [LockNative]::FindWindow('RainmeterMeterWindow',"$shelf\Shelf1\Shelf.ini")
    $position = Position $window
    [void][LockNative]::Send($tray,0x111,118,0)
    [LockNative]::Bang($control,'!Manage Settings')
    $dialog = Password-Dialog 'Create Cafe Lock Password'
    if ([LockNative]::FindWindow('#32770','Manage Rainmeter') -ne [IntPtr]::Zero) { throw 'Locked Mode exposed F1 Settings.' }
    if ((Target-Hashes) -join '|' -ne $before -join '|' -or (Backups).Count) { throw 'Locked F1 invocation changed files.' }
    $password = 'F1 isolated UI test password!'
    Submit-Password $dialog $password $password
    Open-Preview
    $preview = Password-Dialog 'ShelfSuite F1 compatibility'
    Click ([LockNative]::Child($preview,'Cancel'))
    Wait-For { -not [LockNative]::IsWindow($preview) } 'F1 preview cancelled'
    if ((Target-Hashes) -join '|' -ne $before -join '|' -or (Backups).Count) { throw 'Cancel changed F1 files.' }
    Open-Preview
    $preview = Password-Dialog 'ShelfSuite F1 compatibility'
    Click ([LockNative]::Child($preview,'Lock Now'))
    Wait-For { -not [LockNative]::IsWindow($preview) } 'Lock Now cancels preview'
    [void][LockNative]::PostMessage($settings,0x111,[IntPtr]118,[IntPtr]::Zero)
    if ((Target-Hashes) -join '|' -ne $before -join '|' -or (Backups).Count) { throw 'Revoked preview changed files.' }
    [void][LockNative]::Send($tray,0x111,4090,0)
    Submit-Password (Password-Dialog 'Enter Cafe Lock Password') $password
    # An ordinary reader which denies delete/write must refuse without a backup.
    $busy = [IO.File]::Open((Join-Path $shelf '@Resources/ShelfEngine.lua'),'Open','Read','Read')
    try {
        Open-Preview
        $preview = Password-Dialog 'ShelfSuite F1 compatibility'
        Click ([LockNative]::Child($preview,'Apply'))
        Result-Dialog 'Refused'
    } finally { $busy.Dispose() }
    if ((Target-Hashes) -join '|' -ne $before -join '|' -or (Backups).Count) { throw 'Busy target produced writes.' }
    Open-Preview
    $preview = Password-Dialog 'ShelfSuite F1 compatibility'
    Click ([LockNative]::Child($preview,'Apply'))
    Result-Dialog 'Updated'
    if ((Backups).Count -ne 1) { throw 'Native update did not create one verified backup.' }
    if (-not [LockNative]::IsWindow($window) -or (Position $window) -ne $position) { throw 'Native F1 update changed the loaded skin or its position.' }
    $after = Target-Hashes
    Open-Preview
    Result-Dialog 'Already compatible'
    if ((Target-Hashes) -join '|' -ne $after -join '|' -or (Backups).Count -ne 1) { throw 'Already compatible rewrote files or created a backup.' }
    # Only an approved fixture INI is customized for the refusal assertion.
    [IO.File]::AppendAllText((Join-Path $shelf 'Shelf27/Shelf.ini'), "`r`n; unrecognized fixture edit`r`n")
    Open-Preview
    Result-Dialog 'Refused'
    if ((Backups).Count -ne 1) { throw 'Customized additional shelf created a backup.' }
    Write-Output 'PASS F1 native standard-user live UI: locked/forged denial, preview/cancel, Lock Now revocation, busy refusal, additional shelves, update without automatic reload, already-compatible no-op, customized refusal.'
} finally {
    $config.Dispose()
    if ($process -and -not $process.HasExited) {
        [void][LockNative]::Send([LockNative]::FindWindow('RainmeterTrayClass',$null),0x11,0,0)
        [void][LockNative]::Send([LockNative]::FindWindow('RainmeterTrayClass',$null),0x16,1,0)
        if (-not $process.WaitForExit(10000)) { Stop-Process -Id $process.Id -Force }
    }
}
