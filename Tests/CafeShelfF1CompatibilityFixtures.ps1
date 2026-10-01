param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][string]$UpstreamDirectory,
    [Parameter(Mandatory=$true)][string]$PayloadDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$utf8=[Text.UTF8Encoding]::new($false,$true)
function Write-Bytes([string]$Path,[byte[]]$Bytes) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    [IO.File]::WriteAllBytes($Path,$Bytes)
}
function Text([string]$Path) { return $utf8.GetString([IO.File]::ReadAllBytes($Path)).Replace("`r`n","`n") }
function With-Theme([string]$Value,[string]$Theme,[string]$Newline) {
    $value=[regex]::Replace($Value,'(?m)^@IncludeTheme=#@#Themes\\(DeepOcean|Forest|Terracotta|Obsidian)\.inc$',"@IncludeTheme=#@#Themes\$Theme.inc")
    if($Newline -eq 'CRLF') { $value=$value.Replace("`n","`r`n") }
    return $value
}
function Dynamic([string]$Value) { return $Value.Replace("AccurateText=1`n","AccurateText=1`nDynamicWindowSize=1`n").Replace("AccurateText=1`r`n","AccurateText=1`r`nDynamicWindowSize=1`r`n") }
function Hash([byte[]]$Bytes) { return (Get-FileHash -InputStream ([IO.MemoryStream]::new($Bytes)) -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert-Catalog([string]$TextValue) {
    $hash=Hash $utf8.GetBytes($TextValue)
    if(-not (@($script:manifest.Recognition | Where-Object {$_.InputHash -eq $hash}).Count -eq 1)) { throw "Fixture is not an approved recognition input: $hash" }
}
$script:manifest=Get-Content (Join-Path $PayloadDirectory 'manifest.json') -Raw | ConvertFrom-Json
$root=Join-Path $Directory 'Skins/Shelf Suite'
[IO.Directory]::CreateDirectory((Join-Path $root '@Resources')) | Out-Null
$stock1=Text (Join-Path $UpstreamDirectory 'Shelf Suite/Shelf1/Shelf.ini')
$stock2=Text (Join-Path $UpstreamDirectory 'Shelf Suite/Shelf2/Shelf.ini')
$stock3=Text (Join-Path $UpstreamDirectory 'Shelf Suite/Shelf3/Shelf.ini')
$header=Get-Content (Join-Path $PSScriptRoot '..\Library\CafeShelf\ShelfTemplate.h') -Raw
$generated=[regex]::Match($header,'(?s)R"CAFE_TEMPLATE\((.*?)\)CAFE_TEMPLATE";').Groups[1].Value.Replace("`r`n","`n")
if(-not $generated) { throw 'Could not read the approved generated shelf template.' }
$cases=@{
    'Shelf1'=With-Theme $stock1 'DeepOcean' 'LF'
    'Shelf2'=With-Theme $stock2 'Forest' 'CRLF'
    'Shelf3'=With-Theme $stock3 'Terracotta' 'LF'
    'Shelf4'=With-Theme $generated 'DeepOcean' 'LF'
    'Shelf27'=With-Theme $generated 'Obsidian' 'CRLF'
}
foreach($pair in $cases.GetEnumerator()) { Assert-Catalog $pair.Value; Write-Bytes (Join-Path $root ($pair.Key+'\Shelf.ini')) $utf8.GetBytes($pair.Value) }
Write-Bytes (Join-Path $root '@Resources\ShelfEngine.lua') ([IO.File]::ReadAllBytes((Join-Path $UpstreamDirectory 'Shelf Suite/@Resources/ShelfEngine.lua')))
Write-Bytes (Join-Path $root '@Resources\Variables.inc') ([IO.File]::ReadAllBytes((Join-Path $UpstreamDirectory 'Shelf Suite/@Resources/Variables.inc')))
Write-Bytes (Join-Path $root 'Shelf1\config.lua') $utf8.GetBytes('-- sentinel configuration must never be opened')
Write-Bytes (Join-Path $root '@Resources\Icons\sentinel.png') ([byte[]](1,2,3))
Write-Bytes (Join-Path $root '@Resources\Themes\sentinel.inc') $utf8.GetBytes('[Variables]')
$root
