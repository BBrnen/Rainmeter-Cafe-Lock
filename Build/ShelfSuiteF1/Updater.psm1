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
if (-not ('CafeF1FileInfo' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class CafeF1FileInfo {
    [StructLayout(LayoutKind.Sequential)] struct Info {
        public uint Attributes; public System.Runtime.InteropServices.ComTypes.FILETIME Created,Accessed,Written;
        public uint Volume,SizeHigh,SizeLow,Links,IndexHigh,IndexLow;
    }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern SafeFileHandle CreateFile(string path,uint access,uint share,IntPtr security,uint mode,uint flags,IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetFileInformationByHandle(SafeFileHandle file,out Info info);
    public static string Identity(string path) {
        using (var file=CreateFile(path,0,7,IntPtr.Zero,3,0x02200000,IntPtr.Zero)) {
            Info info;
            if(file.IsInvalid || !GetFileInformationByHandle(file,out info)) throw new Win32Exception();
            if((info.Attributes & 0x400)!=0 || info.Links!=1) throw new InvalidOperationException("Linked file refused");
            return info.Volume.ToString("X8")+":"+info.IndexHigh.ToString("X8")+info.IndexLow.ToString("X8");
        }
    }
}
'@
}
function Get-F1Identity([string]$Path) { $full=Assert-F1PlainPath $Path; return [CafeF1FileInfo]::Identity($full) }
function Read-F1Target([string]$Root,[string]$RelativePath) {
    if ($RelativePath -cnotmatch '^(@Resources/(ShelfEngine\.lua|Variables\.inc)|Shelf[1-9][0-9]*/Shelf\.ini)$') { throw 'Undeclared target refused.' }
    try {
        $path=Assert-F1PlainPath (Join-Path $Root $RelativePath)
        $identity=Get-F1Identity $path
        $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        try {
            if ($stream.Length -gt 1048576) { throw 'Unrecognized oversized source.' }
            $memory=New-Object IO.MemoryStream
            try { $stream.CopyTo($memory); $bytes=$memory.ToArray() } finally { $memory.Dispose() }
            if ((Get-F1Identity $path) -cne $identity) { throw 'Changed file.' }
        } finally { $stream.Dispose() }
        return [pscustomobject]@{ Bytes=$bytes; Identity=$identity }
    } catch { throw "$RelativePath is missing, busy, unreadable, linked or changed. No update was applied." }
}
function Get-F1Preflight([string]$Root,$Package) {
    try { $rootPath=Assert-F1PlainPath $Root } catch { throw 'Shelf Suite directory is missing, non-local or redirected.' }
    if (-not [IO.Directory]::Exists($rootPath) -or [IO.Path]::GetFileName($rootPath.TrimEnd('\')) -cne 'Shelf Suite') { throw 'Choose the existing directory named Shelf Suite.' }
    $packageInfo=Read-F1Package $Package.PackageDirectory
    if ($packageInfo.PackageDirectory.Equals($rootPath,[StringComparison]::OrdinalIgnoreCase) -or $packageInfo.PackageDirectory.StartsWith($rootPath+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Extract the compatibility ZIP outside Shelf Suite.' }
    $directories=@{}; $current=$rootPath
    while ($current) { $directories[$current]=Get-F1Identity $current; $current=[IO.Path]::GetDirectoryName($current) }
    $directories[(Join-Path $rootPath '@Resources')]=Get-F1Identity (Join-Path $rootPath '@Resources')
    $targets=New-Object Collections.Generic.List[string]
    $targets.Add('@Resources/ShelfEngine.lua'); $targets.Add('@Resources/Variables.inc')
    $shelves=@([IO.Directory]::EnumerateDirectories($rootPath) | Sort-Object)
    $numbers=@{}
    foreach ($dir in $shelves) {
        $name=[IO.Path]::GetFileName($dir)
        if ($name -notmatch '(?i)^Shelf[0-9]') { continue }
        $number=0
        if ($name -cnotmatch '^Shelf[1-9][0-9]*$' -or -not [int]::TryParse($name.Substring(5),[ref]$number) -or $numbers.ContainsKey($number)) { throw "$name has an ambiguous or unsupported shelf identifier." }
        $numbers[$number]=$true
        $directories[$dir]=Get-F1Identity $dir
        $targets.Add($name+'/Shelf.ini')
    }
    if ($numbers.Count -eq 0) { throw 'No recognized Shelf<number> folders found.' }
    $changes=New-Object Collections.Generic.List[object]
    foreach ($rel in $targets) {
        $source=Read-F1Target $rootPath $rel
        $hash=Get-F1Hash $source.Bytes
        $kind='Ini'; if ($rel.StartsWith('@Resources/',[StringComparison]::Ordinal)) { $kind='Shared' }
        $matches=@($packageInfo.Recognition | Where-Object { $_.Kind -ceq $kind -and $_.InputHash -ceq $hash -and ($kind -ceq 'Ini' -or $_.Path -ceq $rel) })
        if ($matches.Count -ne 1) { throw "$rel does not match a recognized ShelfSuite v2.1/F1 template, theme or encoding. Restore your known original or inspect it manually; it was not changed." }
        $match=$matches[0]
        if ($hash -ceq $match.OutputHash) { continue }
        if ($kind -ceq 'Shared') {
            $bytes=[IO.File]::ReadAllBytes((Join-Path $packageInfo.PackageDirectory $match.Payload))
            if ($match.Newline -ceq 'CRLF') { $bytes=$script:Utf8.GetBytes($script:Utf8.GetString($bytes).Replace("`n","`r`n")) }
        } else {
            $nl="`n"; if ($match.Newline -ceq 'CRLF') { $nl="`r`n" }
            $bytes=$script:Utf8.GetBytes($script:Utf8.GetString($source.Bytes).Replace("AccurateText=1$nl","AccurateText=1${nl}DynamicWindowSize=1$nl"))
        }
        if ((Get-F1Hash $bytes) -cne $match.OutputHash) { throw "$rel failed approved-output verification." }
        $changes.Add([pscustomobject]@{ RelativePath=$rel; OriginalBytes=$source.Bytes; OutputBytes=$bytes; OriginalHash=$hash; OutputHash=$match.OutputHash; Identity=$source.Identity })
    }
    return [pscustomobject]@{ Root=$rootPath; PackageInfo=$packageInfo; Changes=@($changes.ToArray()); CheckedDirectories=$directories }
}
Export-ModuleMember -Function Read-F1Package,Get-F1Preflight
