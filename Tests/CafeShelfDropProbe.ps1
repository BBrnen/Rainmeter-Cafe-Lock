param([switch]$DisposableVM, [switch]$ProcessRegression, [string]$ProbeExe)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$onCi = $env:GITHUB_ACTIONS -eq 'true'
if (!$onCi -and !$DisposableVM) { throw 'Use this only on a disposable CI desktop or explicitly selected spare PC/VM.' }

function Start-ProbeProcess {
    param([string]$Program, [string[]]$Arguments, [string]$OutLog, [string]$ErrLog)
    $child = Start-Process -FilePath $Program -ArgumentList $Arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $OutLog -RedirectStandardError $ErrLog
    # Windows PowerShell 5.1 loses redirected-process exit codes without this
    # retained handle. Keep it before waiting (PowerShell issue 5421).
    $null = $child.Handle
    return $child
}
if ($ProcessRegression) {
    if (!$onCi) { throw 'Process regression is CI-only.' }
    foreach ($expected in @(0, 7)) {
        $stem = Join-Path $env:RUNNER_TEMP ("CafeShelfExit-" + [guid]::NewGuid().ToString('N'))
        $child = Start-ProbeProcess -Program $ProbeExe -Arguments @("--exit-check-$expected") -OutLog "$stem.out" -ErrLog "$stem.err"
        try {
            if (!$child.WaitForExit(10000)) {
                & taskkill.exe /PID $child.Id /T /F | Out-Null
                throw 'Process regression timed out'
            }
            $child.Refresh()
            $observed = $child.ExitCode
            if ($null -eq $observed -or $observed -ne $expected) {
                throw "FAIL Windows PowerShell exit code: expected $expected, observed [$observed]"
            }
            Write-Host "PASS Windows PowerShell preserves exit code $expected"
        } finally { $child.Dispose() }
    }
    exit 0
}

if (!$onCi) {
    Write-Host 'MANUAL drop test: use a spare PC/VM. This version does not move the mouse.'
    Write-Host 'It creates temporary fixtures only; it does not change Rainmeter or ShelfSuite.'
    Write-Host 'File Explorer will open with four prepared test items. Drag them into the test window as prompted.'
    Write-Host 'Press any key to start. You have up to ten minutes to finish the four drops.'
    [void][Console]::ReadKey($true)
}
$repo = Split-Path $PSScriptRoot -Parent
$temporaryRoot = if ($onCi) { $env:RUNNER_TEMP } else { $env:TEMP }
$directory = Join-Path $temporaryRoot ("CafeShelfDrop-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
Write-Host "Diagnostic files: $directory"
$fixturesDirectory = Join-Path $directory 'Drop these four items'
New-Item -ItemType Directory -Path $fixturesDirectory | Out-Null
$folder = Join-Path $fixturesDirectory 'Folder with spaces'
New-Item -ItemType Directory -Path $folder | Out-Null
$exe = Join-Path $fixturesDirectory 'Application.exe'
Copy-Item -LiteralPath (Join-Path $env:WINDIR 'System32/notepad.exe') -Destination $exe
$document = Join-Path $fixturesDirectory 'Document.txt'
[System.IO.File]::WriteAllText($document, 'Harmless drop fixture; never executed.')
$link = Join-Path $fixturesDirectory 'Shortcut.lnk'
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

        & .\CafeShelfDropProbe.exe --window-regression
        if ($LASTEXITCODE -ne 0) { throw 'Hidden-startup window regression failed' }

        # The downloadable kit runs under built-in Windows PowerShell 5.1.
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -ProcessRegression -ProbeExe (Join-Path $directory 'CafeShelfDropProbe.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Windows PowerShell process regression failed' }

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
        $probeArguments += '--manual-disposable-vm'
        Invoke-Item -LiteralPath $fixturesDirectory
    }
    $quotedArguments = $probeArguments | ForEach-Object { '"' + $_ + '"' }
    $probe = Start-ProbeProcess -Program $program -Arguments $quotedArguments -OutLog $outLog -ErrLog $errLog
    try {
        $waitMs = if ($onCi) { 120000 } else { 660000 }
        if (!$probe.WaitForExit($waitMs)) {
            & taskkill.exe /PID $probe.Id /T /F | Out-Null
            throw 'The drop test exceeded its time limit; no completed drag/drop result was obtained.'
        }
        $probe.Refresh()
        if ($null -eq $probe.ExitCode) { throw 'Diagnostic exit code unavailable; do not treat this run as a pass.' }
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
