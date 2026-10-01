param(
    [ValidateSet('Protocol', 'HostPolicy', 'Controller', 'Selection', 'Host', 'Browser', 'Launcher', 'Icons', 'Config', 'Storage', 'F1Compatibility', 'All')]
    [string]$Suite = 'Protocol',
    [string]$UpstreamDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
if ($UpstreamDirectory) { $UpstreamDirectory = (Resolve-Path -LiteralPath $UpstreamDirectory).Path }
$output = Join-Path $repo 'work-package/CafeShelfTests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    $tests = if ($Suite -eq 'All') { @('Protocol', 'HostPolicy', 'Controller', 'Selection', 'Host', 'Browser', 'Launcher', 'Icons', 'Config', 'Storage') } else { @($Suite) }
    if ($tests -contains 'F1Compatibility') {
        if (-not $UpstreamDirectory) { throw 'F1Compatibility requires the disposable unchanged pinned ShelfSuite checkout.' }
        $upstream = (Resolve-Path $UpstreamDirectory).Path
        $payload = Join-Path $repo 'ThirdParty/ShelfSuite/F1Compatibility'
        $fixtureScript = Join-Path $repo 'Tests/CafeShelfF1CompatibilityFixtures.ps1'
        $compilerArgs = @('/nologo', '/EHsc', '/W4', '/WX', '/DNOMINMAX', '/utf-8',
            "$repo/Tests/CafeShelfF1Compatibility.cpp", "$repo/Library/CafeShelf/F1Compatibility.cpp",
            '/Fe:CafeShelfF1Compatibility.exe', '/link', 'bcrypt.lib')
        & cl.exe @compilerArgs
        if ($LASTEXITCODE -ne 0) { throw 'CafeShelf F1Compatibility compilation failed' }
        function New-F1Fixture([string]$Name) {
            return (& $fixtureScript -Directory (Join-Path $output $Name) -UpstreamDirectory $upstream -PayloadDirectory $payload)
        }
        $root = New-F1Fixture ('F1-' + [guid]::NewGuid().ToString('N'))
        $config = [IO.File]::Open((Join-Path $root 'Shelf1/config.lua'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        $icon = [IO.File]::Open((Join-Path $root '@Resources/Icons/sentinel.png'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        $theme = [IO.File]::Open((Join-Path $root '@Resources/Themes/sentinel.inc'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        try {
            & ./CafeShelfF1Compatibility.exe (Split-Path -Parent $root) $payload preview 5 2
            if ($LASTEXITCODE -ne 0) { throw 'Mixed ShelfN recognition failed' }
        } finally { $theme.Dispose(); $icon.Dispose(); $config.Dispose() }
        function Set-F1Fixture([string]$FixtureRoot) {
            foreach ($relative in @('@Resources/ShelfEngine.lua', '@Resources/Variables.inc')) {
                Copy-Item -LiteralPath (Join-Path $payload ('payload/' + $relative)) -Destination (Join-Path $FixtureRoot $relative) -Force
            }
            foreach ($ini in Get-ChildItem -LiteralPath $FixtureRoot -Directory -Filter 'Shelf*' | ForEach-Object { Join-Path $_.FullName 'Shelf.ini' }) {
                $bytes = [IO.File]::ReadAllBytes($ini)
                $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
                $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
                if (-not $text.Contains("DynamicWindowSize=1$newline")) {
                    $text = $text.Replace("AccurateText=1$newline", "AccurateText=1${newline}DynamicWindowSize=1${newline}")
                }
                [IO.File]::WriteAllBytes($ini, [Text.UTF8Encoding]::new($false).GetBytes($text))
            }
        }
        Set-F1Fixture $root
        & ./CafeShelfF1Compatibility.exe (Split-Path -Parent $root) $payload already 0 7
        if ($LASTEXITCODE -ne 0) { throw 'Manually F1-updated fixture was not a no-op' }
        $partial = New-F1Fixture ('F1-partial-' + [guid]::NewGuid().ToString('N'))
        Set-F1Fixture $partial
        Copy-Item -LiteralPath (Join-Path $upstream 'Shelf Suite/@Resources/Variables.inc') -Destination (Join-Path $partial '@Resources/Variables.inc') -Force
        & ./CafeShelfF1Compatibility.exe (Split-Path -Parent $partial) $payload preview 1 6
        if ($LASTEXITCODE -ne 0) { throw 'Known partial F1 fixture was not recognized' }
        $before = @(Get-ChildItem -LiteralPath $root -Recurse -File | Get-FileHash -Algorithm SHA256 | ForEach-Object { $_.Path + '|' + $_.Hash })
        New-Item -ItemType Directory -Path (Join-Path $root 'Shelf99') | Out-Null
        [IO.File]::WriteAllText((Join-Path $root 'Shelf99/Shelf.ini'), '[Rainmeter]', [Text.UTF8Encoding]::new($false))
        & ./CafeShelfF1Compatibility.exe (Split-Path -Parent $root) $payload refused 0 0
        if ($LASTEXITCODE -ne 0) { throw 'Unexpected additional shelf was accepted' }
        $after = @(Get-ChildItem -LiteralPath $root -Recurse -File | Get-FileHash -Algorithm SHA256 | ForEach-Object { $_.Path + '|' + $_.Hash })
        if (@(Compare-Object $before $after | Where-Object { $_.InputObject -notmatch 'Shelf99\\Shelf.ini\|' }).Count) { throw 'Refusal changed an existing fixture file' }
        $empty = Join-Path $output ('F1-empty-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path (Join-Path $empty 'Skins/Shelf Suite/@Resources') -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $upstream 'Shelf Suite/@Resources/ShelfEngine.lua') -Destination (Join-Path $empty 'Skins/Shelf Suite/@Resources/ShelfEngine.lua')
        Copy-Item -LiteralPath (Join-Path $upstream 'Shelf Suite/@Resources/Variables.inc') -Destination (Join-Path $empty 'Skins/Shelf Suite/@Resources/Variables.inc')
        & ./CafeShelfF1Compatibility.exe (Join-Path $empty 'Skins') $payload refused 0 0
        if ($LASTEXITCODE -ne 0) { throw 'No-shelf fixture was accepted' }
        $allRoot = & $fixtureScript -Directory (Join-Path $output ('F1-all-' + [guid]::NewGuid().ToString('N'))) -UpstreamDirectory $upstream -PayloadDirectory $payload -AllVariants
        & ./CafeShelfF1Compatibility.exe (Split-Path -Parent $allRoot) $payload preview 34 32
        if ($LASTEXITCODE -ne 0) { throw 'Complete approved INI catalog recognition failed' }
        $linkedRoot = New-F1Fixture ('F1-linked-' + [guid]::NewGuid().ToString('N'))
        $linkedParent = Split-Path -Parent (Split-Path -Parent $linkedRoot)
        $junction = Join-Path $output ('F1-junction-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Junction -Path $junction -Target $linkedParent | Out-Null
        & ./CafeShelfF1Compatibility.exe (Join-Path $junction 'Skins') $payload refused 0 0
        if ($LASTEXITCODE -ne 0) { throw 'Redirected ancestor was accepted' }
        New-Item -ItemType HardLink -Path (Join-Path (Split-Path -Parent $linkedRoot) 'linked-engine') -Target (Join-Path $linkedRoot '@Resources/ShelfEngine.lua') | Out-Null
        & ./CafeShelfF1Compatibility.exe (Split-Path -Parent $linkedRoot) $payload refused 0 0
        if ($LASTEXITCODE -ne 0) { throw 'Hard-linked target was accepted' }
        Write-Host 'PASS F1Compatibility: recognized Shelf1, Shelf2, Shelf3, Shelf4, and Shelf27 by complete contents; refused unknown Shelf99 and no-shelf fixtures without reading locked config, icon, or theme sentinels.'
        return
    }
    if ($tests -contains 'Selection' -or $tests -contains 'Host' -or $tests -contains 'Browser') {
        & "$repo/Build/CafeDependencies/Restore.ps1"
        $sdk = Join-Path $repo 'work-package/dependencies/Microsoft.Web.WebView2.1.0.4258.31'
    }
    $failed = @()
    foreach ($test in $tests) {
        $source = @{
            Protocol = 'CafeShelfCore.cpp'
            HostPolicy = 'CafeShelfHostPolicy.cpp'
            Controller = 'CafeShelfController.cpp'
            Selection = 'CafeShelfSelection.cpp'
            Host = 'CafeShelfHostHarness.cpp'
            Browser = 'CafeShelfBrowser.cpp'
            Launcher = 'CafeShelfLauncher.cpp'
            Icons = 'CafeShelfIcons.cpp'
            Config = 'CafeShelfConfig.cpp'
            Storage = 'CafeShelfStorage.cpp'
        }[$test]
        $compilerArgs = @(
            '/nologo', '/EHsc', '/W4', '/WX', '/DNOMINMAX', '/D_HAS_EXCEPTIONS=0', '/DWIN32_LEAN_AND_MEAN', '/DWINVER=0x0601', '/D_WIN32_WINNT=0x0601', '/D_WIN32_IE=0x0601', '/GR-', '/GL', '/utf-8',
            "$repo/Tests/$source",
            "$repo/Library/CafeShelf/Session.cpp",
            "$repo/Library/CafeShelf/Protocol.cpp",
            "$repo/Library/CafeShelf/HostPolicy.cpp",
            "$repo/Library/CafeShelf/Controller.cpp",
            "/Fe:CafeShelf$test.exe"
        )
        $arguments = @()
        $fixtureHashes = @()
        if ($test -eq 'Config') { $compilerArgs += "$repo/Library/CafeShelf/Config.cpp" }
        if ($test -eq 'Storage') {
            $fixtures = Join-Path $output ('Storage-' + [guid]::NewGuid().ToString('N'))
            & "$repo/Tests/CafeShelfStorageFixtures.ps1" -Directory $fixtures
            $arguments = @($fixtures)
            $compilerArgs += @("$repo/Library/CafeShelf/Storage.cpp", "$repo/Library/CafeShelf/Config.cpp", "$repo/Library/CafeShelf/Icons.cpp", '/link', 'advapi32.lib', 'ole32.lib', 'shell32.lib', 'shlwapi.lib', 'windowscodecs.lib', 'gdi32.lib', 'user32.lib', 'uuid.lib', 'bcrypt.lib')
        }
        if ($test -eq 'Launcher' -or $test -eq 'Icons') {
            $fixtures = Join-Path $output ('Launcher-' + [guid]::NewGuid().ToString('N'))
            & "$repo/Tests/CafeShelfFixtures.ps1" -Directory $fixtures
            $fixtureHashes = @(Get-ChildItem -LiteralPath $fixtures -File -Recurse | Get-FileHash -Algorithm SHA256)
            $arguments = @($fixtures)
            if ($test -eq 'Icons') { $compilerArgs += "$repo/Library/CafeShelf/Icons.cpp" }
            $compilerArgs += @("$repo/Library/CafeShelf/Launcher.cpp", '/link', 'version.lib', 'ole32.lib', 'shell32.lib', 'uuid.lib', 'windowscodecs.lib', 'gdi32.lib', 'user32.lib', 'shlwapi.lib')
        }
        if ($test -eq 'Selection') {
            $compilerArgs += @(
                "/I$sdk/build/native/include", "$repo/Library/CafeShelf/Selection.cpp",
                '/link', 'ole32.lib', 'shell32.lib', 'uuid.lib', 'user32.lib'
            )
            $fixtures = Join-Path $output ("Selection-" + [guid]::NewGuid().ToString('N'))
            $folder = Join-Path $fixtures 'Folder with spaces'
            New-Item -ItemType Directory -Path $folder | Out-Null
            $shortcut = Join-Path $fixtures 'Original shortcut.lnk'
            $shell = New-Object -ComObject WScript.Shell
            $link = $shell.CreateShortcut($shortcut)
            $link.TargetPath = Join-Path $env:WINDIR 'System32/notepad.exe'
            $link.WorkingDirectory = $folder
            $link.Save()
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link)
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
            $arguments = @($shortcut, $folder)
        }
        if ($test -eq 'Host' -or $test -eq 'Browser') {
            if ($test -eq 'Browser') {
                if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Real browser integration requires a disposable CI desktop.' }
                & rc.exe /nologo "/I$repo/Library" /foCafeShelfEditor.res "$repo/Library/CafeShelf/EditorResources.rc"
                if ($LASTEXITCODE -ne 0) { throw 'Embedded editor resource compilation failed' }
                $compilerArgs += 'CafeShelfEditor.res'
                $fixtures = Join-Path $output ('Browser-' + [guid]::NewGuid().ToString('N'))
                & "$repo/Tests/CafeShelfStorageFixtures.ps1" -Directory $fixtures
                $arguments = @($fixtures)
            }
            $compilerArgs += @(
                '/MT', "/I$sdk/build/native/include",
                "$repo/Library/CafeShelf/Host.cpp", "$repo/Library/CafeShelf/Selection.cpp",
                "$repo/Library/CafeShelf/Launcher.cpp", "$repo/Library/CafeShelf/Icons.cpp",
                "$repo/Library/CafeShelf/Config.cpp", "$repo/Library/CafeShelf/Storage.cpp",
                '/link', "$sdk/build/native/x64/WebView2LoaderStatic.lib",
                'advapi32.lib', 'user32.lib', 'ole32.lib', 'oleaut32.lib', 'shell32.lib', 'shlwapi.lib', 'uuid.lib', 'version.lib', 'windowscodecs.lib', 'gdi32.lib', 'bcrypt.lib'
            )
        }
        & cl.exe @compilerArgs
        if ($LASTEXITCODE -ne 0) { throw "CafeShelf $test compilation failed" }
        & "./CafeShelf$test.exe" @arguments
        if ($LASTEXITCODE -ne 0) { $failed += $test }
        if ($test -eq 'Launcher' -or $test -eq 'Icons') {
            foreach ($original in $fixtureHashes) {
                if ((Get-FileHash -LiteralPath $original.Path -Algorithm SHA256).Hash -ne $original.Hash) { throw 'Launcher inspection changed a fixture' }
            }
            if (Get-ChildItem -LiteralPath $fixtures -Recurse -Filter 'launcher-was-executed.txt') { throw 'Launcher inspection executed a fixture' }
            if (Test-Path -LiteralPath (Join-Path $output 'launcher-was-executed.txt')) { throw 'Launcher inspection executed a fixture' }
            Write-Host 'PASS inspection did not change fixture bytes or launch fixture applications'
        }
    }
    if ($failed.Count -gt 0) { throw "CafeShelf assertions failed: $($failed -join ', ')" }
} finally {
    Pop-Location
}
