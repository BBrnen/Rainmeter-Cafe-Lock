param([string]$MakeNsis="${env:ProgramFiles(x86)}/NSIS/makensis.exe")
$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/..").Path
$directory=Join-Path $repo 'work-package/runtime-policy'
New-Item -ItemType Directory -Force $directory | Out-Null
$test=Join-Path $directory 'CafeRuntimePolicy.exe'
Push-Location $PSScriptRoot
try {
 & $MakeNsis /WX "/DTEST_OUTPUT=$test" CafeRuntimePolicy.nsi
 if($LASTEXITCODE -ne 0){throw 'Runtime policy test compilation failed'}
} finally {Pop-Location}
$process=Start-Process -FilePath $test -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0){throw "$($process.ExitCode) of 12 Runtime decision/result checks failed"}
Write-Host 'PASS 12 Runtime decision/result checks (machine/per-user presence, silent opt-in, prompt, success, failure). No Runtime installer executed.'
