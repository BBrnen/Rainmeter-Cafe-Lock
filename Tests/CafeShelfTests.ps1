param(
    [ValidateSet('Protocol', 'HostPolicy', 'Controller', 'Selection', 'Host', 'Browser', 'All')]
    [string]$Suite = 'Protocol'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path $repo 'work-package/CafeShelfTests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    $tests = if ($Suite -eq 'All') { @('Protocol', 'HostPolicy', 'Controller', 'Selection', 'Host', 'Browser') } else { @($Suite) }
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
        }[$test]
        $compilerArgs = @(
            '/nologo', '/EHsc', '/W4', '/WX', '/DNOMINMAX', '/D_HAS_EXCEPTIONS=0', '/GR-', '/GL', '/utf-8',
            "$repo/Tests/$source",
            "$repo/Library/CafeShelf/Session.cpp",
            "$repo/Library/CafeShelf/Protocol.cpp",
            "$repo/Library/CafeShelf/HostPolicy.cpp",
            "$repo/Library/CafeShelf/Controller.cpp",
            "/Fe:CafeShelf$test.exe"
        )
        $arguments = @()
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
            }
            $compilerArgs += @(
                '/MT', "/I$sdk/build/native/include",
                "$repo/Library/CafeShelf/Host.cpp", "$repo/Library/CafeShelf/Selection.cpp",
                '/link', "$sdk/build/native/x64/WebView2LoaderStatic.lib",
                'advapi32.lib', 'user32.lib', 'ole32.lib', 'oleaut32.lib', 'shell32.lib', 'shlwapi.lib', 'uuid.lib', 'version.lib'
            )
        }
        & cl.exe @compilerArgs
        if ($LASTEXITCODE -ne 0) { throw "CafeShelf $test compilation failed" }
        & "./CafeShelf$test.exe" @arguments
        if ($LASTEXITCODE -ne 0) { $failed += $test }
    }
    if ($failed.Count -gt 0) { throw "CafeShelf assertions failed: $($failed -join ', ')" }
} finally {
    Pop-Location
}
