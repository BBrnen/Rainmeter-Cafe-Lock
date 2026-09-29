param([switch]$Offline)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$dependency = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json).webView2
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$root = Join-Path $repo 'work-package/dependencies'
New-Item -ItemType Directory -Force -Path $root | Out-Null
$package = Join-Path $root "$($dependency.package).$($dependency.version).nupkg"
if (!(Test-Path -LiteralPath $package -PathType Leaf)) {
    if ($Offline) { throw "Verified offline SDK package is missing: $package" }
    $temporary = $package + '.' + [guid]::NewGuid().ToString('N') + '.nupkg'
    try {
        Invoke-WebRequest -Uri $dependency.url -OutFile $temporary
        if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ine $dependency.sha256) {
            throw 'Downloaded SDK checksum does not match the reviewed dependency manifest.'
        }
        & nuget.exe verify $temporary -Signatures -CertificateFingerprint $dependency.authorCertificateSha256 -NonInteractive
        if ($LASTEXITCODE -ne 0) { throw 'Microsoft SDK author signature verification failed.' }
        Move-Item -LiteralPath $temporary -Destination $package
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}
# The pinned bytes were signature-verified in the recorded acquisition run.
# Checking the same digest permits a previously acquired package to work offline.
if ((Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash -ine $dependency.sha256) {
    throw 'Cached SDK checksum mismatch; refusing to use the cache.'
}
$sdk = Join-Path $root "$($dependency.package).$($dependency.version)"
[System.IO.Compression.ZipFile]::ExtractToDirectory($package, $sdk, $true)
foreach ($relative in @('build/native/include/WebView2.h', 'build/native/x64/WebView2LoaderStatic.lib', 'build/native/x86/WebView2LoaderStatic.lib')) {
    if (!(Test-Path -LiteralPath (Join-Path $sdk $relative) -PathType Leaf)) { throw "SDK file missing: $relative" }
}
Write-Output "Verified WebView2 SDK $($dependency.version)"
