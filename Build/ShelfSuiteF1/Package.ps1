param([Parameter(Mandatory=$true)][string]$UpstreamDirectory, [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$upstream = [IO.Path]::GetFullPath($UpstreamDirectory)
$output = [IO.Path]::GetFullPath($OutputDirectory)
$pin = 'd4f186ba0b5c262c7559b80841132f5fd3884f3c'
$f1 = 'c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9'
$patchRel = 'ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch'
$utf8 = New-Object Text.UTF8Encoding($false, $true)
function Git-Checked([string]$Directory, [string[]]$Arguments) {
    $start=New-Object Diagnostics.ProcessStartInfo
    $start.FileName=@(Get-Command git.exe -CommandType Application -ErrorAction Stop)[0].Source
    $parts=@('-C',$Directory)+$Arguments
    $start.Arguments=(@($parts | ForEach-Object { '"'+[regex]::Replace([regex]::Replace($_,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"' }) -join ' ')
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $process=New-Object Diagnostics.Process; $process.StartInfo=$start
    try {
        if (-not $process.Start()) { throw 'Could not start native Git.' }
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        $process.WaitForExit(); $text=$stdout.Result; $detail=$stderr.Result
        if ($process.ExitCode -ne 0) { throw ('Source verification/build Git operation failed (exit '+$process.ExitCode+'): '+$detail.Trim()) }
        if (-not $text) { return @() }
        return @($text.TrimEnd("`r","`n") -split '\r?\n')
    } finally { $process.Dispose() }
}
function Digest([byte[]]$Bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
}
function Read-Source([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $text = $utf8.GetString($bytes)
    if ($text.Length -gt 0 -and $text[0] -eq [char]0xfeff) { throw 'Unsupported source BOM.' }
    $lf = $text.Replace("`r`n", "`n")
    if ($lf.IndexOf("`r") -ge 0) { throw 'Unsupported source line endings.' }
    if ($text.IndexOf("`r`n") -ge 0 -and $text.Replace("`r`n", '').IndexOf("`n") -ge 0) { throw 'Mixed source line endings.' }
    return $lf
}
if ((Git-Checked $upstream @('rev-parse','HEAD')) -ne $pin) { throw 'Expected the pinned ShelfSuite v2.1 source revision.' }
if (@(Git-Checked $upstream @('status','--porcelain','--untracked-files=no')).Count) { throw 'Pinned ShelfSuite source is modified.' }
foreach ($rel in @($patchRel, 'Library/CafeShelf/ShelfTemplate.h')) {
    $expected = Git-Checked $repo @('rev-parse', "${f1}:$rel")
    $actual = Git-Checked $repo @('hash-object', "--path=$rel", (Join-Path $repo $rel))
    if ($actual -ne $expected) { throw "Verified F1 source changed: $rel" }
}
if (Test-Path -LiteralPath $output) { throw 'Output directory must not already exist.' }
[IO.Directory]::CreateDirectory($output) | Out-Null
$source = Join-Path $output 'source'
$package = Join-Path $output 'package'
[IO.Directory]::CreateDirectory($source) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $package 'payload/@Resources')) | Out-Null
$paths = @('Shelf Suite/@Resources/ShelfEngine.lua','Shelf Suite/@Resources/Variables.inc','Shelf Suite/Shelf1/Shelf.ini','Shelf Suite/Shelf2/Shelf.ini','Shelf Suite/Shelf3/Shelf.ini')
$archive = Join-Path $output 'source.zip'
Git-Checked $upstream (@('archive','--format=zip',"--output=$archive",$pin,'--') + $paths + @('LICENSE')) | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($archive,$source)
$original = @{}
foreach ($rel in $paths) {
    $original[$rel] = Read-Source (Join-Path $source $rel)
    # git archive may export CRLF according to the builder's checkout policy.
    # Canonicalize only the verified disposable source, not an installed skin.
    [IO.File]::WriteAllBytes((Join-Path $source $rel),$utf8.GetBytes($original[$rel]))
}
Git-Checked $source @('init','--quiet') | Out-Null
Git-Checked $source @('config','core.autocrlf','false') | Out-Null
Git-Checked $source (@('add','--')+$paths) | Out-Null
$temporaryPatch=Join-Path $output 'verified-f1-lf.patch'
# Only this builder's disposable copy uses canonical Git LF transport.
# The committed/working F1 patch is verified above and is never rewritten.
[IO.File]::WriteAllBytes($temporaryPatch,$utf8.GetBytes((Read-Source (Join-Path $repo $patchRel))))
Git-Checked $source @('apply','--unidiff-zero','--check',$temporaryPatch) | Out-Null
Git-Checked $source @('apply','--unidiff-zero',$temporaryPatch) | Out-Null
if (@(Compare-Object @(Git-Checked $source @('diff','--name-only')) $paths).Count) { throw 'Patch touched unexpected paths.' }
$recognition = @()
foreach ($rel in $paths[0..1]) {
    $short = $rel.Substring('Shelf Suite/'.Length)
    $patched = Read-Source (Join-Path $source $rel)
    [IO.File]::WriteAllBytes((Join-Path $package "payload/$short"),$utf8.GetBytes($patched))
    foreach ($newline in @('LF','CRLF')) {
        $outText = $patched
        if ($newline -eq 'CRLF') { $outText = $outText.Replace("`n","`r`n") }
        foreach ($text in @($original[$rel],$patched)) {
            if ($newline -eq 'CRLF') { $text = $text.Replace("`n","`r`n") }
            $recognition += [ordered]@{ Kind='Shared'; Path=$short; InputHash=(Digest $utf8.GetBytes($text)); OutputHash=(Digest $utf8.GetBytes($outText)); Newline=$newline; Encoding='UTF8-no-BOM'; Payload="payload/$short" }
        }
    }
}
$header = Read-Source (Join-Path $repo 'Library/CafeShelf/ShelfTemplate.h')
$match = [regex]::Match($header, '(?s)R"CAFE_TEMPLATE\((.*?)\)CAFE_TEMPLATE";')
if (-not $match.Success) { throw 'Cannot verify trusted generated template.' }
$templates = @{'Stock1'=$original[$paths[2]]; 'Stock2'=$original[$paths[3]]; 'Stock3'=$original[$paths[4]]; 'Generated'=$match.Groups[1].Value}
foreach ($family in @('Stock1','Stock2','Stock3','Generated')) {
    $base = $templates[$family].Replace("DynamicWindowSize=1`n",'')
    if (@([regex]::Matches($base,'(?m)^@IncludeTheme=#@#Themes\\(DeepOcean|Forest|Terracotta|Obsidian)\.inc$')).Count -ne 1) { throw 'Unexpected trusted theme pattern.' }
    foreach ($theme in @('DeepOcean','Forest','Terracotta','Obsidian')) {
        $input = [regex]::Replace($base,'(?m)^@IncludeTheme=#@#Themes\\(DeepOcean|Forest|Terracotta|Obsidian)\.inc$',"@IncludeTheme=#@#Themes\$theme.inc")
        $result = $input.Replace("AccurateText=1`n", "AccurateText=1`nDynamicWindowSize=1`n")
        foreach ($newline in @('LF','CRLF')) {
            $before = $input; $after = $result
            if ($newline -eq 'CRLF') { $before=$before.Replace("`n","`r`n"); $after=$after.Replace("`n","`r`n") }
            foreach ($dynamic in @($false,$true)) {
                $text = $before; if ($dynamic) { $text=$after }
                $recognition += [ordered]@{ Kind='Ini'; Family=$family; Theme=$theme; Dynamic=$dynamic; InputHash=(Digest $utf8.GetBytes($text)); OutputHash=(Digest $utf8.GetBytes($after)); Newline=$newline; Encoding='UTF8-no-BOM' }
            }
        }
    }
}
[IO.File]::Copy((Join-Path $source 'LICENSE'),(Join-Path $package 'LICENSE-ShelfSuite.txt'))
$complete = $true
foreach ($name in @('Updater.psm1','Update-ShelfSuite.ps1','Update-ShelfSuite.cmd','README.md')) {
    $path = Join-Path $PSScriptRoot $name
    if (Test-Path -LiteralPath $path) { [IO.File]::Copy($path,(Join-Path $package $name)) } else { $complete=$false }
}
$files = @()
foreach ($file in Get-ChildItem -LiteralPath $package -File -Recurse) {
    $files += [ordered]@{ Path=$file.FullName.Substring($package.Length+1).Replace('\','/'); Hash=(Digest ([IO.File]::ReadAllBytes($file.FullName))) }
}
$manifest = [ordered]@{ SchemaVersion=1; Complete=$complete; UpstreamRevision=$pin; F1Revision=$f1; PatchSha256=(Digest ([IO.File]::ReadAllBytes((Join-Path $repo $patchRel)))); Files=$files; Recognition=$recognition }
[IO.File]::WriteAllText((Join-Path $package 'manifest.json'),($manifest | ConvertTo-Json -Depth 8),$utf8)
$zipName='INCOMPLETE-fixture.zip'
if ($complete) { $zipName='ShelfSuite-Cafe-Lock-F1-Compatibility.zip' }
$zip = Join-Path $output $zipName
[IO.Compression.ZipFile]::CreateFromDirectory($package,$zip)
[IO.File]::WriteAllText(($zip+'.sha256'),((Digest ([IO.File]::ReadAllBytes($zip)))+'  '+$zipName+"`r`n"),$utf8)
Write-Output $zip
