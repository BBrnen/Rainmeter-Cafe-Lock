param(
    [ValidateSet('Package', 'Preflight', 'Apply', 'UI', 'All')][string]$Suite = 'All',
    [string]$PackageDirectory,
    [Parameter(Mandatory = $true)][string]$UpstreamDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. "$PSScriptRoot/CafeShelfF1DeliveryFixtures.ps1"
$script:Passed = 0
$repo = Split-Path $PSScriptRoot -Parent
$upstream = [IO.Path]::GetFullPath($UpstreamDirectory)
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('CafeF1Tests-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($scratch) | Out-Null
$builder = Join-Path $repo 'Build/ShelfSuiteF1/Package.ps1'
Assert-True (Test-Path -LiteralPath $builder) 'F1 package builder must exist'
& $builder -UpstreamDirectory $upstream -OutputDirectory (Join-Path $scratch 'build') | Out-Null
$built = Join-Path $scratch 'build/package'
$manifest = Get-Content -LiteralPath "$built/manifest.json" -Raw | ConvertFrom-Json
if (-not $PackageDirectory) { $PackageDirectory = $built }
if ($Suite -in @('Package', 'All')) {
    Test-Case 'ExactPinnedSourceOnly' {
        Assert-True ($manifest.UpstreamRevision -eq 'd4f186ba0b5c262c7559b80841132f5fd3884f3c') 'wrong upstream provenance'
        $bad = Join-Path $scratch 'wrong-revision'
        git clone --quiet --no-hardlinks $upstream $bad
        if ($LASTEXITCODE -ne 0) { throw 'Fixture clone failed' }
        git -C $bad checkout --quiet HEAD~1
        Assert-Refused { & $builder -UpstreamDirectory $bad -OutputDirectory "$scratch/bad-build" } 'wrong revision'
        git -C $bad checkout --quiet d4f186ba0b5c262c7559b80841132f5fd3884f3c
        [IO.File]::AppendAllText("$bad/Shelf Suite/@Resources/Variables.inc",'custom')
        Assert-Refused { & $builder -UpstreamDirectory $bad -OutputDirectory "$scratch/dirty-build" } 'modified upstream'
    }
    Test-Case 'UnchangedF1PatchOnly' {
        Assert-True ($manifest.F1Revision -eq 'c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9') 'wrong F1 provenance'
        Assert-True ($manifest.PatchSha256 -eq (Hash-Bytes ([IO.File]::ReadAllBytes("$repo/ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch")))) 'patch digest mismatch'
        $clone = Join-Path $scratch 'altered-f1'
        git clone --quiet --shared --no-checkout $repo $clone
        git -C $clone checkout --quiet HEAD -- Library/CafeShelf/ShelfTemplate.h ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch
        Write-Fixture "$clone/Build/ShelfSuiteF1/Package.ps1" ([IO.File]::ReadAllBytes($builder))
        [IO.File]::AppendAllText("$clone/ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch",'changed')
        Assert-Refused { & "$clone/Build/ShelfSuiteF1/Package.ps1" -UpstreamDirectory $upstream -OutputDirectory "$scratch/altered-build" } 'altered F1 patch'
    }
    Test-Case 'CatalogMatchesStockAndGeneratedBytes' {
        foreach ($n in 1..3) {
            $bytes = [IO.File]::ReadAllBytes("$upstream/Shelf Suite/Shelf$n/Shelf.ini")
            Assert-True (@($manifest.Recognition | Where-Object { $_.InputHash -eq (Hash-Bytes $bytes) }).Count -ge 1) "Shelf$n missing from recognition"
        }
        Assert-True (@($manifest.Recognition | Where-Object { $_.Kind -eq 'Ini' -and $_.Family -eq 'Generated' }).Count -eq 16) 'generated LF/CRLF/theme/dynamic catalog incomplete'
    }
    Test-Case 'CatalogThemeAndDynamicVariantsOnly' {
        Assert-True (@($manifest.Recognition | Where-Object { $_.Kind -eq 'Ini' }).Count -eq 64) 'unexpected template variants'
        foreach ($entry in @($manifest.Recognition | Where-Object { $_.Kind -eq 'Ini' })) {
            Assert-True ($entry.Theme -in @('DeepOcean','Forest','Terracotta','Obsidian')) 'unknown theme recognized'
            Assert-True ($entry.Encoding -eq 'UTF8-no-BOM') 'unsupported encoding recognized'
        }
    }
    Test-Case 'PayloadAndZipAllowlist' {
        $actual = @(Get-ChildItem -LiteralPath $built -File -Recurse | ForEach-Object { $_.FullName.Substring($built.Length + 1).Replace('\','/') })
        $want = @('manifest.json', 'LICENSE-ShelfSuite.txt', 'payload/@Resources/ShelfEngine.lua', 'payload/@Resources/Variables.inc')
        foreach ($name in @('Updater.psm1','Update-ShelfSuite.ps1','Update-ShelfSuite.cmd','README.md')) {
            if (Test-Path -LiteralPath "$repo/Build/ShelfSuiteF1/$name") { $want += $name }
        }
        Assert-True (@(Compare-Object $actual $want).Count -eq 0) 'unexpected package content'
        foreach ($entry in $manifest.Files) { Assert-True ((Hash-Bytes ([IO.File]::ReadAllBytes("$built/$($entry.Path)"))) -eq $entry.Hash) 'package hash mismatch' }
        foreach ($rel in @('@Resources/ShelfEngine.lua','@Resources/Variables.inc')) {
            $actualText = $script:Utf8.GetString([IO.File]::ReadAllBytes("$scratch/build/source/Shelf Suite/$rel")).Replace("`r`n","`n")
            Assert-True ((Hash-Bytes $script:Utf8.GetBytes($actualText)) -eq (Hash-Bytes ([IO.File]::ReadAllBytes("$built/payload/$rel")))) 'payload differs from applied patch'
        }
    }
    Test-Case 'CorruptPayloadRejected' {
        Assert-True (Test-Path "$PackageDirectory/Updater.psm1") 'package validator must exist'
        Import-Module "$PackageDirectory/Updater.psm1" -Force
        Read-F1Package $built | Out-Null
        $corrupt = Join-Path $scratch 'corrupt-package'
        Copy-Item -LiteralPath $built -Destination $corrupt -Recurse
        [IO.File]::WriteAllText("$corrupt/payload/@Resources/Variables.inc",'corrupt')
        Assert-Refused { Read-F1Package $corrupt } 'corrupt payload'
        [IO.File]::Copy("$built/payload/@Resources/Variables.inc","$corrupt/payload/@Resources/Variables.inc",$true)
        $badManifest = Get-Content "$built/manifest.json" -Raw | ConvertFrom-Json
        $badManifest.Files[0].Path='../outside'
        Write-Fixture "$corrupt/manifest.json" ($script:Utf8.GetBytes(($badManifest | ConvertTo-Json -Depth 8)))
        Assert-Refused { Read-F1Package $corrupt } 'manifest traversal'
        $badManifest = Get-Content "$built/manifest.json" -Raw | ConvertFrom-Json
        $badManifest.Files += $badManifest.Files[0]
        Write-Fixture "$corrupt/manifest.json" ($script:Utf8.GetBytes(($badManifest | ConvertTo-Json -Depth 8)))
        Assert-Refused { Read-F1Package $corrupt } 'duplicate package file'
    }
    Test-Case 'FreshOutputRequired' { Assert-Refused { & $builder -UpstreamDirectory $upstream -OutputDirectory "$scratch/build" } 'existing output' }
}
Write-Output "PASS: $script:Passed F1 delivery checks ($Suite); disposable fixtures: $scratch"
