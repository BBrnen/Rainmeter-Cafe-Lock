Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:Utf8 = New-Object Text.UTF8Encoding($false, $true)
function Get-F1Hash([byte[]]$Bytes) {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
}
function Assert-F1PlainPath([string]$Path) {
    $full=[IO.Path]::GetFullPath($Path)
    if ($full -notmatch '^[A-Za-z]:\\' -or (New-Object IO.DriveInfo($full.Substring(0,3))).DriveType -ne 'Fixed') { throw 'Select a local fixed-disk directory.' }
    $item=$full
    while ($item) {
        if ([IO.File]::Exists($item) -or [IO.Directory]::Exists($item)) {
            if (([IO.File]::GetAttributes($item) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Linked or redirected path refused.' }
        } else { throw 'Required path is missing.' }
        $item=[IO.Path]::GetDirectoryName($item)
    }
    return $full
}
function Read-F1Package([string]$PackageDirectory) {
    try {
        $dir=Assert-F1PlainPath $PackageDirectory
        $manifestPath=Assert-F1PlainPath (Join-Path $dir 'manifest.json')
        $m=$script:Utf8.GetString([IO.File]::ReadAllBytes($manifestPath)) | ConvertFrom-Json
        if ($m.SchemaVersion -ne 1 -or $m.UpstreamRevision -ne 'd4f186ba0b5c262c7559b80841132f5fd3884f3c' -or $m.F1Revision -ne 'c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9' -or $m.PatchSha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid provenance.' }
        $allowed=@('LICENSE-ShelfSuite.txt','payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc','Updater.psm1','Update-ShelfSuite.ps1','Update-ShelfSuite.cmd','README.md')
        $seen=@{}
        foreach ($entry in $m.Files) {
            if ($entry.Path -cnotin $allowed -or $seen.ContainsKey($entry.Path) -or $entry.Hash -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid package file list.' }
            $seen[$entry.Path]=$true
            $path=Assert-F1PlainPath (Join-Path $dir $entry.Path)
            if ((Get-F1Hash ([IO.File]::ReadAllBytes($path))) -cne $entry.Hash) { throw 'Package checksum mismatch.' }
        }
        foreach ($required in $allowed[0..3]) { if (-not $seen.ContainsKey($required)) { throw 'Required package file missing.' } }
        if ($m.Complete -and $seen.Count -ne $allowed.Count) { throw 'Incomplete package file list.' }
        if (@($m.Recognition).Count -ne 72) { throw 'Invalid recognition catalog.' }
        foreach ($r in $m.Recognition) {
            if ($r.InputHash -cnotmatch '^[0-9a-f]{64}$' -or $r.OutputHash -cnotmatch '^[0-9a-f]{64}$' -or $r.Newline -cnotin @('LF','CRLF') -or $r.Encoding -cne 'UTF8-no-BOM') { throw 'Invalid recognition entry.' }
            if ($r.Kind -ceq 'Shared') {
                if ($r.Path -cnotin @('@Resources/ShelfEngine.lua','@Resources/Variables.inc') -or $r.Payload -cne ('payload/'+$r.Path)) { throw 'Invalid shared target.' }
                $bytes=[IO.File]::ReadAllBytes((Join-Path $dir $r.Payload))
                if ($r.Newline -ceq 'CRLF') { $bytes=$script:Utf8.GetBytes($script:Utf8.GetString($bytes).Replace("`n","`r`n")) }
                if ((Get-F1Hash $bytes) -cne $r.OutputHash) { throw 'Catalog payload mismatch.' }
            } elseif ($r.Kind -ceq 'Ini') {
                if ($r.Family -cnotin @('Stock1','Stock2','Stock3','Generated') -or $r.Theme -cnotin @('DeepOcean','Forest','Terracotta','Obsidian') -or $r.Dynamic -isnot [bool]) { throw 'Invalid INI variant.' }
            } else { throw 'Invalid target type.' }
        }
        $m | Add-Member -NotePropertyName PackageDirectory -NotePropertyValue $dir
        return $m
    } catch { throw 'Compatibility package is missing, redirected, corrupt or unrecognized. Extract a fresh verified ZIP.' }
}
Export-ModuleMember -Function Read-F1Package
