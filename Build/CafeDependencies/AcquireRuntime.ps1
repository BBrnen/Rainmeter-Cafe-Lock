param([switch]$Offline,[string]$CacheDirectory=(Join-Path $PSScriptRoot '../../work-package/dependencies/runtime'))
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$pin=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'runtime.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $CacheDirectory | Out-Null
$CacheDirectory=(Resolve-Path -LiteralPath $CacheDirectory).Path
$file=Join-Path $CacheDirectory $pin.fileName
function Verify-Runtime([string]$Path) {
 $digest=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
 if($digest -ine $pin.sha256){throw 'Runtime installer checksum mismatch; refusing to package or execute it.'}
 $signature=Get-AuthenticodeSignature -LiteralPath $Path
 if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ine $pin.signerThumbprint -or
  $signature.SignerCertificate.Subject -notmatch '^CN=Microsoft Corporation,') { throw 'Runtime Microsoft signature verification failed.' }
 $version=(Get-Item -LiteralPath $Path).VersionInfo.FileVersion
 if($version -ne $pin.installerFileVersion){throw 'Runtime installer version mismatch.'}
 [pscustomobject]@{ Verified=$true; Path=$file; RuntimeVersion=$pin.runtimeVersion; InstallerFileVersion=$version;
  Sha256=$digest; Signer=$signature.SignerCertificate.Subject; SignerThumbprint=$signature.SignerCertificate.Thumbprint; Source=$pin.url }
}
if(!(Test-Path -LiteralPath $file -PathType Leaf)) {
 if($Offline){throw 'The verified offline Runtime installer is missing.'}
 $temporary=$file+'.'+[guid]::NewGuid().ToString('N')+'.partial'
 try {
  Invoke-WebRequest -Uri $pin.url -OutFile $temporary
  Verify-Runtime $temporary | Out-Null
  Move-Item -LiteralPath $temporary -Destination $file
 } finally { if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary} }
}
$verified=Verify-Runtime $file
# This script only verifies bytes. Execution belongs to the optional setup choice.
$verified | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $CacheDirectory 'acquisition.json') -Encoding utf8NoBOM
$verified
