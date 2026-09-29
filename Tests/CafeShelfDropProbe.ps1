param([switch]$DisposableVM)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$onCi = $env:GITHUB_ACTIONS -eq 'true'
if (!$onCi -and !$DisposableVM) { throw 'Use this only on a disposable CI desktop or explicitly selected spare PC/VM.' }
if (!$onCi) {
    Write-Host 'This test moves the mouse in its own test window. Use a spare PC/VM.'
    Write-Host 'It creates temporary fixtures only; it does not change Rainmeter or ShelfSuite.'
    Write-Host 'Press any key to start, then leave the mouse alone until the test ends.'
    [void][Console]::ReadKey($true)
}
$repo = Split-Path $PSScriptRoot -Parent
$temporaryRoot = if ($onCi) { $env:RUNNER_TEMP } else { $env:TEMP }
$directory = Join-Path $temporaryRoot ("CafeShelfDrop-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
Write-Host "Diagnostic files: $directory"
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
    if ($onCi) {
        & "$repo/Build/CafeDependencies/Restore.ps1"
        $sdk = Join-Path $repo 'work-package/dependencies/Microsoft.Web.WebView2.1.0.4258.31'
        $compilerArgs = @(
            '/nologo', '/EHsc', '/W4', '/WX', '/DNOMINMAX', '/utf-8', '/MT',
            "/I$sdk/build/native/include", "$repo/Tests/CafeShelfDropProbe.cpp",
            '/Fe:CafeShelfDropProbe.exe', '/link', "$sdk/build/native/x64/WebView2LoaderStatic.lib",
            'advapi32.lib', 'user32.lib', 'ole32.lib', 'oleaut32.lib', 'shell32.lib', 'shlwapi.lib', 'uuid.lib', 'version.lib'
        )
        & cl.exe @compilerArgs
        if ($LASTEXITCODE -ne 0) { throw 'Drop probe compilation failed' }
        & cl.exe /nologo /EHsc /W4 /WX /DNOMINMAX "$repo/Tests/RunAsStandard.cpp" /Fe:CafeShelfStandard.exe /link Advapi32.lib Shlwapi.lib
        if ($LASTEXITCODE -ne 0) { throw 'Standard-user harness compilation failed' }

        # Publish a reviewable test kit even if this runner lacks an interactive desktop.
        $kit = Join-Path $repo 'work-package/CafeShelfDropKit'
        New-Item -ItemType Directory -Path $kit -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $directory 'CafeShelfDropProbe.exe') -Destination $kit
        foreach ($name in @('CafeShelfDropProbe.cpp', 'CafeShelfDropProbe.ps1', 'RunDropProbe.cmd', 'CafeShelfDropProbe-README.md')) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $kit
        }
        Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination (Join-Path $kit 'Rainmeter-LICENSE')
        Copy-Item -LiteralPath (Join-Path $repo 'Build/CafeDependencies/dependencies.json') -Destination $kit
        Get-ChildItem -LiteralPath $sdk -Recurse -File |
            Where-Object { $_.Name -match 'license|notice' } |
            ForEach-Object {
                $relative = $_.FullName.Substring($sdk.Length).TrimStart('\', '/')
                $destination = Join-Path (Join-Path $kit 'SDK-notices') $relative
                New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
                Copy-Item -LiteralPath $_.FullName -Destination $destination
            }
        [System.IO.File]::WriteAllText((Join-Path $kit 'BUILD.md'), "Test-only diagnostic built from $env:GITHUB_SHA. Not a Rainmeter installer.")
        $binaryHash = (Get-FileHash -LiteralPath (Join-Path $kit 'CafeShelfDropProbe.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
        [System.IO.File]::WriteAllText((Join-Path $kit 'SHA256SUMS'), "$binaryHash  CafeShelfDropProbe.exe")
        $parseErrors = $null
        $tokens = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile((Join-Path $kit 'CafeShelfDropProbe.ps1'), [ref]$tokens, [ref]$parseErrors)
        if ($parseErrors.Count -ne 0) { throw 'Packaged diagnostic script does not parse.' }
    } else {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'CafeShelfDropProbe.exe') -Destination $directory
    }

    $outLog = Join-Path $directory 'probe.stdout.log'
    $errLog = Join-Path $directory 'probe.stderr.log'
    $probeArguments = @($link, $exe, $folder, $document, (Join-Path $directory 'BrowserProfile'))
    if ($onCi) {
        $program = Join-Path $directory 'CafeShelfStandard.exe'
        $probeArguments = @((Join-Path $directory 'CafeShelfDropProbe.exe')) + $probeArguments
    } else {
        $program = Join-Path $directory 'CafeShelfDropProbe.exe'
        $probeArguments += '--disposable-vm'
    }
    $quotedArguments = $probeArguments | ForEach-Object { '"' + $_ + '"' }
    $probe = Start-Process -FilePath $program -ArgumentList $quotedArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
    try {
        if (!$probe.WaitForExit(120000)) {
            & taskkill.exe /PID $probe.Id /T /F | Out-Null
            throw 'Real drop probe exceeded 120 seconds; this desktop has not proved drag/drop.'
        }
        $probe.Refresh()
        if ($probe.ExitCode -ne 0) { throw "Real Windows drop probe failed ($($probe.ExitCode)); keep the diagnostic logs." }
        Write-Host 'PASS: all four real Windows drops preserved their original paths.'
    } finally {
        if (Test-Path -LiteralPath $outLog) { Get-Content -LiteralPath $outLog }
        if (Test-Path -LiteralPath $errLog) { Get-Content -LiteralPath $errLog }
        if ($onCi) {
            Copy-Item -LiteralPath $outLog -Destination $kit -ErrorAction SilentlyContinue
            Copy-Item -LiteralPath $errLog -Destination $kit -ErrorAction SilentlyContinue
        }
        $probe.Dispose()
    }
} finally {
    Pop-Location
}
