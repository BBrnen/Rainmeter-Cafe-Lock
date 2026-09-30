$script:Utf8 = New-Object Text.UTF8Encoding($false, $true)
function Hash-Bytes([byte[]]$Bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant() } finally { $sha.Dispose() }
}
function Write-Fixture([string]$Path, [byte[]]$Bytes) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    [IO.File]::WriteAllBytes($Path, $Bytes)
}
function New-SkinFixture([string]$Parent, [string]$UpstreamDirectory) {
    $root = Join-Path $Parent 'Shelf Suite'
    foreach ($rel in @('@Resources/ShelfEngine.lua', '@Resources/Variables.inc', 'Shelf1/Shelf.ini', 'Shelf2/Shelf.ini', 'Shelf3/Shelf.ini')) {
        Write-Fixture (Join-Path $root $rel) ([IO.File]::ReadAllBytes((Join-Path $UpstreamDirectory "Shelf Suite/$rel")))
    }
    # Harness-owned sentinels only. The updater must not even open them.
    foreach ($rel in @('Shelf1/config.lua', 'Rainmeter.ini', 'CafeLock.ini', '@Resources/Icons/owner.png', '@Resources/Themes/owner.inc')) {
        Write-Fixture (Join-Path $root $rel) ($script:Utf8.GetBytes('PRIVATE-SENTINEL-never-read-by-updater'))
    }
    return $root
}
function Assert-True([bool]$Condition, [string]$Message) { if (-not $Condition) { throw "ASSERT: $Message" } }
function Assert-Refused([scriptblock]$Action, [string]$Name) {
    $refused = $false
    try { & $Action | Out-Null } catch { $refused = $true }
    Assert-True $refused "$Name must refuse"
}
function Test-Case([string]$Name, [scriptblock]$Action) {
    & $Action
    $script:Passed++
    Write-Output "PASS $Name"
}
