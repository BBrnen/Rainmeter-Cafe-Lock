param([Parameter(Mandatory=$true)][string]$CacheDirectory)
$ErrorActionPreference='Stop'
$script=Join-Path $PSScriptRoot '../Build/CafeDependencies/AcquireRuntime.ps1'
$failures=0
function Check([string]$Name,[bool]$Passed) {
 if (!$Passed) { $script:failures++ }
 Write-Host "$(if($Passed){'PASS'}else{'FAIL'}) $Name"
}
$verified=& $script -Offline -CacheDirectory $CacheDirectory
Check 'real offline installer verified by digest and Microsoft signature' ($verified.Verified -eq $true -and
 $verified.Sha256 -eq '771042DB15CB5C463BAC51A8408E70183D7130E8AC946709384C2223DA582C1B' -and
 $verified.SignerThumbprint -eq '4028CAD637509D4744B17EC5B42AED8D7A31E6AF' -and $verified.InstallerFileVersion -eq '1.3.271.7')
$fixture=Join-Path $CacheDirectory ('reject-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$rejected=$false
try { & $script -Offline -CacheDirectory $fixture | Out-Null } catch { $rejected=$true }
Check 'missing offline cache rejected without downloading' $rejected
[IO.File]::WriteAllText((Join-Path $fixture 'MicrosoftEdgeWebView2RuntimeInstallerX64.exe'),'not a signed installer')
$rejected=$false
try { & $script -Offline -CacheDirectory $fixture | Out-Null } catch { $rejected=$true }
Check 'corrupt cached installer rejected without execution' $rejected
if($failures){throw "$failures runtime acquisition checks failed"}
