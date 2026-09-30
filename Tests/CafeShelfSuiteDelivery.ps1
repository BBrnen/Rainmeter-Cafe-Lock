$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path $PSScriptRoot -Parent
$patch = Join-Path $repo 'ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch'
$delivery = Join-Path $repo 'Docs/CafeLock-ShelfSuite-F1-Delivery.md'
$compatibility = Join-Path $repo 'Docs/CafeLock-Compatibility.md'
$deployment = Join-Path $repo 'Docs/CafeLock-Deployment.md'

function Require-Text([string]$text, [string]$needle, [string]$description) {
    if (-not $text.Contains($needle, [StringComparison]::Ordinal)) { throw "Missing $description" }
}

if (-not (Test-Path -LiteralPath $patch)) { throw 'Missing F1 ShelfSuite patch' }
if (-not (Test-Path -LiteralPath $delivery)) { throw 'Missing F1 delivery contract' }
$patchText = Get-Content -LiteralPath $patch -Raw
$deliveryText = Get-Content -LiteralPath $delivery -Raw
$compatibilityText = Get-Content -LiteralPath $compatibility -Raw
$deploymentText = Get-Content -LiteralPath $deployment -Raw

Require-Text $patchText 'd4f186ba0b5c262c7559b80841132f5fd3884f3c' 'exact upstream base declaration'
Require-Text $patchText 'MIT License' 'upstream MIT attribution'
foreach ($path in @(
    'Shelf Suite/@Resources/ShelfEngine.lua',
    'Shelf Suite/@Resources/Variables.inc',
    'Shelf Suite/Shelf1/Shelf.ini',
    'Shelf Suite/Shelf2/Shelf.ini',
    'Shelf Suite/Shelf3/Shelf.ini'
)) { Require-Text $patchText $path "declared patched path $path" }
Require-Text $deliveryText 'ShelfSuite-Cafe-Lock-F1-Compatibility.zip' 'future compatibility artifact name'
Require-Text $deliveryText 'back up' 'owner backup requirement'
Require-Text $deliveryText 'hash' 'upstream hash verification requirement'
Require-Text $deliveryText 'timestamped backup' 'timestamped backup requirement'
Require-Text $deliveryText 'DynamicWindowSize=1' 'recognized INI update rule'
Require-Text $deliveryText 'never read, rewrite, or upload `config.lua`' 'config.lua preservation rule'
Require-Text $deliveryText 'not implemented or shipped by F1' 'non-shipping status'
if ($compatibilityText.Contains('installer already deploys the F1 patch', [StringComparison]::OrdinalIgnoreCase) -or
    $deploymentText.Contains('installer already deploys the F1 patch', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Documentation incorrectly claims the normal installer deploys the F1 patch'
}

Write-Output 'PASS: F1 delivery contract declares provenance, safe future delivery, and non-installer status.'
