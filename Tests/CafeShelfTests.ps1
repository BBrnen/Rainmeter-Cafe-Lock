param(
    [ValidateSet('Protocol')]
    [string]$Suite = 'Protocol'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path $repo 'work-package/CafeShelfTests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    $compilerArgs = @(
        '/nologo', '/EHsc', '/W4', '/WX', '/DNOMINMAX', '/utf-8',
        "$repo/Tests/CafeShelfCore.cpp",
        "$repo/Library/CafeShelf/Session.cpp",
        "$repo/Library/CafeShelf/Protocol.cpp",
        '/Fe:CafeShelfCore.exe'
    )
    & cl.exe @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw 'CafeShelfCore compilation failed' }
    & ./CafeShelfCore.exe
    if ($LASTEXITCODE -ne 0) { throw 'CafeShelfCore assertions failed' }
} finally {
    Pop-Location
}
