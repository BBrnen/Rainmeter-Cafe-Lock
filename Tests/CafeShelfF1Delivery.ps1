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
if ($Suite -in @('Preflight','All')) {
    Import-Module "$PackageDirectory/Updater.psm1" -Force
    Assert-True ($null -ne (Get-Command Get-F1Preflight -ErrorAction SilentlyContinue)) 'read-only preflight must exist'
    $pkg=Read-F1Package $PackageDirectory
    function Skin { return New-SkinFixture (Join-Path $scratch ([Guid]::NewGuid().ToString('N'))) $upstream }
    Test-Case 'StockThreeShelvesRecognized' {
        $root=Skin; $p=Get-F1Preflight $root $pkg
        Assert-True ($p.Changes.Count -eq 5) 'stock installation should need exactly five changes'
        foreach ($c in @($p.Changes | Where-Object { $_.RelativePath -like 'Shelf*' })) {
            $before=$script:Utf8.GetString($c.OriginalBytes)
            $nl="`n"; if ($before.IndexOf("`r`n") -ge 0) { $nl="`r`n" }
            $after=$before.Replace("AccurateText=1$nl", "AccurateText=1${nl}DynamicWindowSize=1$nl")
            Assert-True ((Hash-Bytes $script:Utf8.GetBytes($after)) -eq $c.OutputHash) 'INI changed beyond approved insertion'
        }
    }
    Test-Case 'GeneratedShelfRecognized' {
        $root=Skin
        $h=$script:Utf8.GetString([IO.File]::ReadAllBytes("$repo/Library/CafeShelf/ShelfTemplate.h"))
        $template=[regex]::Match($h,'(?s)R"CAFE_TEMPLATE\((.*?)\)CAFE_TEMPLATE";').Groups[1].Value
        Write-Fixture "$root/Shelf4/Shelf.ini" ($script:Utf8.GetBytes($template))
        Assert-True ((Get-F1Preflight $root $pkg).Changes.Count -eq 5) 'already-dynamic generated shelf should be recognized untouched'
        Write-Fixture "$root/Shelf4/Shelf.ini" ($script:Utf8.GetBytes($template.Replace("DynamicWindowSize=1`r`n",'').Replace("DynamicWindowSize=1`n",'')))
        Assert-True ((Get-F1Preflight $root $pkg).Changes.Count -eq 6) 'older generated shelf needs dynamic entry'
    }
    Test-Case 'AllFourThemesPreserved' {
        foreach ($theme in @('DeepOcean','Forest','Terracotta','Obsidian')) {
            foreach ($nl in @("`n","`r`n")) {
                $root=Skin
                $text=$script:Utf8.GetString([IO.File]::ReadAllBytes("$root/Shelf1/Shelf.ini")).Replace("`r`n","`n")
                $text=[regex]::Replace($text,'(?m)^@IncludeTheme=.*$',"@IncludeTheme=#@#Themes\$theme.inc").Replace("`n",$nl)
                Write-Fixture "$root/Shelf1/Shelf.ini" ($script:Utf8.GetBytes($text))
                $c=@((Get-F1Preflight $root $pkg).Changes | Where-Object { $_.RelativePath -eq 'Shelf1/Shelf.ini' })[0]
                Assert-True ($script:Utf8.GetString($c.OutputBytes).IndexOf("@IncludeTheme=#@#Themes\$theme.inc") -ge 0) 'theme not preserved'
            }
        }
    }
    Test-Case 'KnownMixedInstallRecognized' {
        $root=Skin; $p=Get-F1Preflight $root $pkg
        Write-Fixture (Join-Path $root $p.Changes[0].RelativePath) $p.Changes[0].OutputBytes
        Assert-True ((Get-F1Preflight $root $pkg).Changes.Count -eq 4) 'known partial update should complete'
    }
    Test-Case 'AlreadyF1HasZeroChanges' {
        $root=Skin; foreach ($c in (Get-F1Preflight $root $pkg).Changes) { Write-Fixture (Join-Path $root $c.RelativePath) $c.OutputBytes }
        Assert-True ((Get-F1Preflight $root $pkg).Changes.Count -eq 0) 'already F1 must be no-op'
    }
    Test-Case 'OnlyDeclaredTargetsOpened' {
        $root=Skin
        $hold=[IO.File]::Open("$root/Shelf1/config.lua",[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        $module=Get-Module Updater
        & $module { $script:Accesses=New-Object Collections.Generic.List[string]; $script:RealRead=(Get-Command Read-F1Target).ScriptBlock; function script:Read-F1Target($Root,$RelativePath) { $script:Accesses.Add($RelativePath); & $script:RealRead $Root $RelativePath } }
        try {
            Get-F1Preflight $root $pkg | Out-Null
            $reads=@(& $module { $script:Accesses.ToArray() })
            Assert-True (@(Compare-Object $reads @('@Resources/ShelfEngine.lua','@Resources/Variables.inc','Shelf1/Shelf.ini','Shelf2/Shelf.ini','Shelf3/Shelf.ini')).Count -eq 0) 'updater read unrelated target'
        } finally { $hold.Dispose(); Import-Module "$PackageDirectory/Updater.psm1" -Force }
    }
    foreach ($bad in @('UnknownShelf','CustomizedMetadata','DuplicateSection','DuplicateSetting','UnknownTheme','MissingIni','InvalidUTF8','UTF8BOM','UTF16','MixedNewline','CustomizedShared','AmbiguousShelf','OverflowShelf','NoShelves','WrongRootName')) {
        Test-Case $bad {
            $root=Skin; $file="$root/Shelf1/Shelf.ini"; $text=$script:Utf8.GetString([IO.File]::ReadAllBytes($file))
            switch ($bad) {
                UnknownShelf { Write-Fixture "$root/Shelf4/Shelf.ini" $script:Utf8.GetBytes('[Rainmeter]') }
                CustomizedMetadata { Write-Fixture $file $script:Utf8.GetBytes($text.Replace('Author=Martin Santos','Author=Someone')) }
                DuplicateSection { Write-Fixture $file $script:Utf8.GetBytes($text+"`r`n[Rainmeter]`r`n") }
                DuplicateSetting { Write-Fixture $file $script:Utf8.GetBytes($text.Replace('AccurateText=1',"AccurateText=1`r`nAccurateText=1")) }
                UnknownTheme { Write-Fixture $file $script:Utf8.GetBytes($text.Replace('@IncludeTheme=#@#Themes\DeepOcean.inc','@IncludeTheme=#@#Themes\Custom.inc')) }
                MissingIni { [IO.File]::Delete($file) }
                InvalidUTF8 { Write-Fixture $file ([byte[]]@(255,254,253)) }
                UTF8BOM { Write-Fixture $file ([byte[]](@(239,187,191)+[IO.File]::ReadAllBytes($file))) }
                UTF16 { Write-Fixture $file ([Text.Encoding]::Unicode.GetBytes($text)) }
                MixedNewline { Write-Fixture $file $script:Utf8.GetBytes($text.Replace('[Rainmeter]'+"`r`n",'[Rainmeter]'+"`n")) }
                CustomizedShared { Write-Fixture "$root/@Resources/Variables.inc" $script:Utf8.GetBytes('custom') }
                AmbiguousShelf { Write-Fixture "$root/Shelf01/Shelf.ini" ([IO.File]::ReadAllBytes($file)) }
                OverflowShelf { Write-Fixture "$root/Shelf99999999999999999999/Shelf.ini" ([IO.File]::ReadAllBytes($file)) }
                NoShelves { foreach ($n in 1..3) { [IO.Directory]::Move("$root/Shelf$n","$root/Other$n") } }
                WrongRootName { [IO.Directory]::Move($root,($root+' wrong')); $root += ' wrong' }
            }
            $before=@(Get-ChildItem (Split-Path $root -Parent) -Recurse -File | ForEach-Object { $_.FullName+'|'+(Hash-Bytes ([IO.File]::ReadAllBytes($_.FullName))) })
            Assert-Refused { Get-F1Preflight $root (Read-F1Package $PackageDirectory) } $bad
            $after=@(Get-ChildItem (Split-Path $root -Parent) -Recurse -File | ForEach-Object { $_.FullName+'|'+(Hash-Bytes ([IO.File]::ReadAllBytes($_.FullName))) })
            Assert-True (@(Compare-Object $before $after).Count -eq 0) 'refusal made persistent writes'
        }
    }
    Test-Case 'BusyIniRefused' {
        $root=Skin; $hold=[IO.File]::Open("$root/Shelf2/Shelf.ini",'Open','ReadWrite','None')
        try { Assert-Refused { Get-F1Preflight $root (Read-F1Package $PackageDirectory) } 'busy INI' } finally { $hold.Dispose() }
    }
    Test-Case 'UnreadableIniRefused' {
        $root=Skin; $file="$root/Shelf2/Shelf.ini"; $acl=Get-Acl -LiteralPath $file
        $deny=New-Object Security.AccessControl.FileSystemAccessRule([Security.Principal.WindowsIdentity]::GetCurrent().User,'Read','Deny')
        $limited=Get-Acl -LiteralPath $file; $limited.AddAccessRule($deny); Set-Acl -LiteralPath $file -AclObject $limited
        try { Assert-Refused { Get-F1Preflight $root (Read-F1Package $PackageDirectory) } 'unreadable INI' } finally { Set-Acl -LiteralPath $file -AclObject $acl }
    }
    Test-Case 'RedirectedShelfRefused' {
        $root=Skin; [IO.Directory]::Move("$root/Shelf2","$root/Other2")
        New-Item -ItemType Junction -Path "$root/Shelf2" -Target "$root/Other2" | Out-Null
        Assert-Refused { Get-F1Preflight $root (Read-F1Package $PackageDirectory) } 'redirected shelf'
    }
    Test-Case 'SpacesAndNonEnglishPath' { $root=New-SkinFixture "$scratch/space café" $upstream; Assert-True ((Get-F1Preflight $root (Read-F1Package $PackageDirectory)).Changes.Count -eq 5) 'Unicode path failed' }
    Test-Case 'PackageInsideSkinRefused' { $root=Skin; Copy-Item $PackageDirectory "$root/package" -Recurse; Assert-Refused { Get-F1Preflight $root (Read-F1Package "$root/package") } 'package inside skin' }
    Test-Case 'HardLinkRefused' {
        $root=Skin; $path="$root/Shelf1/Shelf.ini"
        New-Item -ItemType HardLink -Path "$root/Shelf1/linked.ini" -Target $path | Out-Null
        Assert-Refused { Get-F1Preflight $root (Read-F1Package $PackageDirectory) } 'linked file'
    }
    Test-Case 'RedirectedAncestorRefused' {
        $root=Skin; $parent=Split-Path $root -Parent
        New-Item -ItemType Junction -Path "$scratch/junction" -Target $parent | Out-Null
        Assert-Refused { Get-F1Preflight "$scratch/junction/Shelf Suite" (Read-F1Package $PackageDirectory) } 'redirected ancestor'
    }
    Test-Case 'NetworkRootRefused' { Assert-Refused { Get-F1Preflight '\\localhost\not-a-share\Shelf Suite' (Read-F1Package $PackageDirectory) } 'network path' }
}
Write-Output "PASS: $script:Passed F1 delivery checks ($Suite); disposable fixtures: $scratch"
