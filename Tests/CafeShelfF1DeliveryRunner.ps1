param([string]$ZipPath,[string]$UpstreamDirectory)
$ErrorActionPreference='Stop'
function Test-F1DeliveryArchive([string]$Path) {
    $zip=[IO.Path]::GetFullPath($Path)
    $sum=[IO.File]::ReadAllText($zip+'.sha256').Trim()
    if ($sum -notmatch '^([0-9a-f]{64})  ([^\\/]+)$' -or $Matches[2] -cne [IO.Path]::GetFileName($zip)) { throw 'Archive checksum record is invalid.' }
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $hash=([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($zip)))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
    if ($hash -cne $Matches[1]) { throw 'Archive checksum mismatch.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $want=@('manifest.json','LICENSE-ShelfSuite.txt','payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc','Updater.psm1','Update-ShelfSuite.ps1','Update-ShelfSuite.cmd','README.md')
        $names=@($archive.Entries | ForEach-Object {$_.FullName.Replace('\','/')})
        if ($names.Count -ne $want.Count -or @(Compare-Object $names $want -CaseSensitive).Count) { throw 'Unexpected or missing ZIP content; extraction refused.' }
        return $names
    } finally { $archive.Dispose() }
}
function Invoke-F1DeliveryRunner([string]$ArchivePath,[string]$SourceDirectory) {
    if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) { throw 'Disposable GitHub Windows runner only.' }
    Test-F1DeliveryArchive $ArchivePath | Out-Null
    $extract=Join-Path $env:RUNNER_TEMP ('CafeF1Extract-'+[Guid]::NewGuid().ToString('N'))
    if (Test-Path -LiteralPath $extract) { throw 'Extraction staging must be fresh.' }
    [IO.Directory]::CreateDirectory($extract) | Out-Null
    [IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($ArchivePath),$extract)
    $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    & icacls $extract /grant "*${sid}:(OI)(CI)F" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not permit fixture access.' }
    & icacls $extract /setintegritylevel '(OI)(CI)M' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not set fixture integrity.' }
    $power=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    & "$PSScriptRoot/../RunAsStandard.exe" $power -NoProfile -File "$PSScriptRoot/CafeShelfF1Delivery.ps1" -Suite All -PackageDirectory $extract -UpstreamDirectory ([IO.Path]::GetFullPath($SourceDirectory)) -StandardUser
    if ($LASTEXITCODE -ne 0) { throw "Extracted-ZIP standard-user tests failed: $LASTEXITCODE" }
    Write-Output 'PASS extracted ZIP: complete package tests ran under the restricted token.'
}
if ($MyInvocation.InvocationName -ne '.') { Invoke-F1DeliveryRunner $ZipPath $UpstreamDirectory }
