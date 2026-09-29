$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Real drag/drop probe requires a disposable CI desktop.' }
$repo = Split-Path $PSScriptRoot -Parent
& "$repo/Build/CafeDependencies/Restore.ps1"
$sdk = Join-Path $repo 'work-package/dependencies/Microsoft.Web.WebView2.1.0.4258.31'
$directory = Join-Path $env:RUNNER_TEMP ("CafeShelfDrop-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$folder = Join-Path $directory 'Folder with spaces'
New-Item -ItemType Directory -Path $folder | Out-Null
$exe = Join-Path $directory 'Application.exe'
Copy-Item -LiteralPath (Join-Path $env:WINDIR 'System32/notepad.exe') -Destination $exe
$document = Join-Path $directory 'Document.txt'
[System.IO.File]::WriteAllText($document, 'Harmless drop fixture; never executed.')
$link = Join-Path $directory 'Shortcut.lnk'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($link)
$shortcut.TargetPath = $exe
$shortcut.Arguments = '"argument with spaces"'
$shortcut.WorkingDirectory = $folder
$shortcut.Save()
[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut)
[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
Push-Location $directory
try {
    $compilerArgs = @(
        '/nologo', '/EHsc', '/W4', '/WX', '/DNOMINMAX', '/utf-8', '/MT',
        "/I$sdk/build/native/include",
        "$repo/Tests/CafeShelfDropProbe.cpp",
        '/Fe:CafeShelfDropProbe.exe', '/link',
        "$sdk/build/native/x64/WebView2LoaderStatic.lib",
        'advapi32.lib', 'user32.lib', 'ole32.lib', 'oleaut32.lib', 'shell32.lib', 'shlwapi.lib', 'uuid.lib', 'version.lib'
    )
    & cl.exe @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw 'Drop probe compilation failed' }
    $outLog = Join-Path $directory 'probe.stdout.log'
    $errLog = Join-Path $directory 'probe.stderr.log'
    $arguments = @($link, $exe, $folder, $document, (Join-Path $directory 'BrowserProfile')) |
        ForEach-Object { '"' + $_ + '"' }
    $probe = Start-Process -FilePath (Join-Path $directory 'CafeShelfDropProbe.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
    try {
        if (!$probe.WaitForExit(120000)) {
            Stop-Process -Id $probe.Id -Force
            throw 'Real drop probe exceeded 120 seconds; this desktop has not proved drag/drop.'
        }
        $probe.Refresh()
        if ($probe.ExitCode -ne 0) { throw "Real Windows drop probe failed ($($probe.ExitCode)); do not assume Browse-only delivery is acceptable." }
    } finally {
        if (Test-Path -LiteralPath $outLog) { Get-Content -LiteralPath $outLog }
        if (Test-Path -LiteralPath $errLog) { Get-Content -LiteralPath $errLog }
        $probe.Dispose()
    }
} finally {
    Pop-Location
}
