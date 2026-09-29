# Acquisition evidence for the approved SDK pin. Does not execute package contents.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Run this acquisition probe on a disposable CI runner.' }
$version = '1.0.4258.31'
$url = "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$version/microsoft.web.webview2.$version.nupkg"
$directory = Join-Path $env:RUNNER_TEMP ("CafeShelfSdk-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$package = Join-Path $directory 'webview2.nupkg'
Invoke-WebRequest -Uri $url -OutFile $package
& nuget.exe verify $package -All -Verbosity detailed -NonInteractive
if ($LASTEXITCODE -ne 0) { throw 'SDK package signature validation failed.' }
$hash = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output "CAFE_SDK_VERSION=$version"
Write-Output "CAFE_SDK_SHA256=$hash"
$expanded = Join-Path $directory 'expanded'
[System.IO.Compression.ZipFile]::ExtractToDirectory($package, $expanded)
$header = Join-Path $expanded 'build/native/include/WebView2.h'
if (!(Test-Path -LiteralPath $header -PathType Leaf)) { throw 'SDK header missing' }
foreach ($architecture in @('x64', 'x86')) {
    $loader = Join-Path $expanded "build/native/$architecture/WebView2LoaderStatic.lib"
    if (!(Test-Path -LiteralPath $loader -PathType Leaf)) { throw "Missing $architecture static loader" }
}
Write-Output 'CAFE_SDK_LAYOUT=verified'
