param(
    [ValidateSet('Package', 'Preflight', 'Apply', 'UI', 'All')][string]$Suite = 'All',
    [string]$PackageDirectory,
    [Parameter(Mandatory = $true)][string]$UpstreamDirectory,
    [switch]$StandardUser
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. "$PSScriptRoot/CafeShelfF1DeliveryFixtures.ps1"
if ($StandardUser) {
    if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Restricted-token verification requires a disposable CI runner.' }
    Assert-True ($PSVersionTable.PSVersion.Major -eq 5 -and $PSVersionTable.PSVersion.Minor -eq 1) 'owner runtime must be Windows PowerShell 5.1'
    $principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    Assert-True (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) 'restricted test token must not be admin'
    Assert-True ([CafeF1TestToken]::Integrity() -eq 8192) 'restricted test token must have medium integrity RID 8192'
    Assert-True ($PackageDirectory -and [IO.Path]::GetFullPath($PackageDirectory).StartsWith([IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) 'standard-user tests must load extracted package inside RUNNER_TEMP'
    Write-Output 'PASS standard-user runtime: Windows PowerShell 5.1, admin=false, integrity RID=8192, extracted package'
}
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
    Test-Case 'CRLFCheckoutPatchBuildsEquivalentPayload' {
        $clone=Join-Path $scratch 'windows-checkout'
        git clone --quiet --shared --no-checkout $repo $clone
        git -C $clone checkout --quiet HEAD -- Library/CafeShelf/ShelfTemplate.h ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch
        Write-Fixture "$clone/Build/ShelfSuiteF1/Package.ps1" ([IO.File]::ReadAllBytes($builder))
        $patch="$clone/ThirdParty/ShelfSuite/patches/0001-cafe-lock-adaptive-tabs.patch"
        $text=$script:Utf8.GetString([IO.File]::ReadAllBytes($patch)).Replace("`r`n","`n").Replace("`n","`r`n")
        [IO.File]::WriteAllBytes($patch,$script:Utf8.GetBytes($text))
        & "$clone/Build/ShelfSuiteF1/Package.ps1" -UpstreamDirectory $upstream -OutputDirectory "$scratch/windows-build" | Out-Null
        foreach ($rel in @('@Resources/ShelfEngine.lua','@Resources/Variables.inc')) {
            Assert-True ((Hash-Bytes ([IO.File]::ReadAllBytes("$scratch/windows-build/package/payload/$rel"))) -eq (Hash-Bytes ([IO.File]::ReadAllBytes("$built/payload/$rel")))) 'Windows patch checkout changes payload'
        }
    }
    Test-Case 'ExtractedZipVerifierRejectsCorruptionAndMissingEntry' {
        Assert-True (Test-Path "$PSScriptRoot/CafeShelfF1DeliveryRunner.ps1") 'extracted ZIP verifier must exist'
        . "$PSScriptRoot/CafeShelfF1DeliveryRunner.ps1"
        $zip=Join-Path "$scratch/build" 'ShelfSuite-Cafe-Lock-F1-Compatibility.zip'
        Assert-True ((Test-F1DeliveryArchive $zip).Count -eq 8) 'complete ZIP file count incorrect'
        $bad=Join-Path $scratch 'bad.zip'; [IO.File]::Copy($zip,$bad); [IO.File]::Copy(($zip+'.sha256'),($bad+'.sha256'))
        [IO.File]::AppendAllText($bad,'corrupt')
        Assert-Refused { Test-F1DeliveryArchive $bad } 'corrupt archive'
        $missing=Join-Path $scratch 'missing.zip'
        $tiny=Join-Path $scratch 'tiny'; [IO.Directory]::CreateDirectory($tiny) | Out-Null
        Write-Fixture "$tiny/manifest.json" $script:Utf8.GetBytes('{}')
        [IO.Compression.ZipFile]::CreateFromDirectory($tiny,$missing)
        Write-Fixture ($missing+'.sha256') $script:Utf8.GetBytes((Hash-Bytes ([IO.File]::ReadAllBytes($missing)))+'  missing.zip')
        Assert-Refused { Test-F1DeliveryArchive $missing } 'missing packaged entry'
    }
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
        $root=Skin; $file="$root/Shelf2/Shelf.ini"; $acl=[IO.File]::GetAccessControl($file)
        $deny=New-Object Security.AccessControl.FileSystemAccessRule([Security.Principal.WindowsIdentity]::GetCurrent().User,'Read','Deny')
        $limited=[IO.File]::GetAccessControl($file); $limited.AddAccessRule($deny); [IO.File]::SetAccessControl($file,$limited)
        try { Assert-Refused { Get-F1Preflight $root (Read-F1Package $PackageDirectory) } 'unreadable INI' } finally { [IO.File]::SetAccessControl($file,$acl) }
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
if ($Suite -in @('Apply','All')) {
    Import-Module "$PackageDirectory/Updater.psm1" -Force
    Assert-True ($null -ne (Get-Command Invoke-F1Update -ErrorAction SilentlyContinue)) 'verified backup/apply must exist'
    function Apply-Skin { return New-SkinFixture (Join-Path $scratch ([Guid]::NewGuid().ToString('N'))) $upstream }
    function Apply-Module {
        Import-Module "$PackageDirectory/Updater.psm1" -Force
        $m=Get-Module Updater
        & $m { function script:Assert-F1RainmeterClosed {} }
        return $m
    }
    Test-Case 'OnlyChangedFilesBackedUp' {
        $root=Apply-Skin; Apply-Module | Out-Null
        $p=Get-F1Preflight $root (Read-F1Package $PackageDirectory); $r=Invoke-F1Update $p
        Assert-True ($r.Status -eq 'Updated' -and $r.ChangedPaths.Count -eq 5) 'update failed'
        $files=@(Get-ChildItem "$($r.BackupDirectory)/original" -Recurse -File)
        Assert-True ($files.Count -eq 5) 'backup copied unrelated files'
        foreach ($c in $p.Changes) {
            Assert-True ((Hash-Bytes ([IO.File]::ReadAllBytes("$($r.BackupDirectory)/original/$($c.RelativePath)"))) -eq $c.OriginalHash) 'backup hash mismatch'
            Assert-True ((Hash-Bytes ([IO.File]::ReadAllBytes("$root/$($c.RelativePath)"))) -eq $c.OutputHash) 'output hash mismatch'
        }
        Assert-True ([IO.File]::ReadAllText("$root/Shelf1/config.lua") -eq 'PRIVATE-SENTINEL-never-read-by-updater') 'harness sentinel changed'
        $record=Get-Content "$($r.BackupDirectory)/recovery.json" -Raw | ConvertFrom-Json
        Assert-True (@($record.Files).Count -eq 5 -and (Get-Content "$($r.BackupDirectory)/RESTORE.txt" -Raw).IndexOf('PRIVATE-SENTINEL') -lt 0) 'recovery record exposed contents'
    }
    Test-Case 'BackupsVerifiedBeforeFirstReplacement' {
        $root=Apply-Skin; $m=Apply-Module; $p=Get-F1Preflight $root (Read-F1Package $PackageDirectory)
        & $m { $script:RealReplace=(Get-Command Replace-F1File).ScriptBlock; function script:Replace-F1File($Source,$Target) {
            $backup=Split-Path (Split-Path (Split-Path $Source -Parent) -Parent) -Parent
            $record=Get-Content "$backup/recovery.json" -Raw | ConvertFrom-Json
            foreach ($f in $record.Files) { if ((Get-F1Hash ([IO.File]::ReadAllBytes("$backup/original/$($f.RelativePath)"))) -cne $f.OriginalHash) { throw 'Backup not verified before replace' } }
            & $script:RealReplace $Source $Target
        } }
        Assert-True ((Invoke-F1Update $p).Status -eq 'Updated') 'all backups were not ready before first write'
    }
    Test-Case 'NoActionCreatesNothing' {
        $root=Apply-Skin; Apply-Module | Out-Null; $p=Get-F1Preflight $root (Read-F1Package $PackageDirectory); Invoke-F1Update $p | Out-Null
        $before=@([IO.Directory]::EnumerateDirectories((Split-Path $root -Parent)))
        $r=Invoke-F1Update (Get-F1Preflight $root (Read-F1Package $PackageDirectory))
        Assert-True ($r.Status -eq 'NoAction' -and -not $r.BackupDirectory) 'no-op made backup'
        Assert-True (@(Compare-Object $before @([IO.Directory]::EnumerateDirectories((Split-Path $root -Parent)))).Count -eq 0) 'no-op writes'
    }
    Test-Case 'ChangedOrRedirectedTargetRefused' {
        $root=Apply-Skin; Apply-Module | Out-Null; $p=Get-F1Preflight $root (Read-F1Package $PackageDirectory)
        [IO.File]::AppendAllText("$root/Shelf2/Shelf.ini",'outside-edit')
        $r=Invoke-F1Update $p
        Assert-True ($r.ChangedPaths.Count -eq 0 -and -not $r.BackupDirectory) 'stale plan wrote files'
    }
    Test-Case 'TimestampCollisionNeverOverwrites' {
        $root=Apply-Skin; $m=Apply-Module; $p=Get-F1Preflight $root (Read-F1Package $PackageDirectory)
        $collision=Join-Path (Split-Path $root -Parent) 'existing-backup'; [IO.Directory]::CreateDirectory($collision) | Out-Null
        & $m { param($fixed) $script:FixedBackup=$fixed; function script:Get-F1BackupName($Root) { return $script:FixedBackup } } $collision
        $r=Invoke-F1Update $p
        Assert-True ($r.ChangedPaths.Count -eq 0) 'existing backup overwritten'
        Assert-True (@([IO.Directory]::EnumerateFileSystemEntries($collision)).Count -eq 0) 'collision directory modified'
    }
    foreach ($failure in @('BackupFailure','CorruptBackupBeforeApply','StagedOutputCorrupt','PartialFailure','OutsideEdit','CorruptBackupOnRecovery','RecoveryFailure','AppliedOutputCorrupt')) {
        Test-Case $failure {
            $root=Apply-Skin; $m=Apply-Module; $p=Get-F1Preflight $root (Read-F1Package $PackageDirectory)
            & $m { param($mode) $script:FaultMode=$mode; $script:ReplaceCount=0; $script:WriteCount=0
                $script:RealWrite=(Get-Command Write-F1New).ScriptBlock
                $script:RealReplace=(Get-Command Replace-F1File).ScriptBlock
                function script:Write-F1New($Path,$Bytes) {
                    $script:WriteCount++
                    if ($script:FaultMode -eq 'BackupFailure' -and $script:WriteCount -eq 2) { throw 'Injected backup failure' }
                    if ($script:FaultMode -eq 'CorruptBackupBeforeApply' -and $Path -like '*/original/*') { $Bytes=[byte[]]@(1,2,3) }
                    if ($script:FaultMode -eq 'StagedOutputCorrupt' -and $Path -like '*/staged/*') { $Bytes=[byte[]]@(1,2,3) }
                    & $script:RealWrite $Path $Bytes
                }
                function script:Replace-F1File($Source,$Target) {
                    $script:ReplaceCount++
                    if ($script:ReplaceCount -eq 2 -and $script:FaultMode -in @('PartialFailure','OutsideEdit','CorruptBackupOnRecovery','RecoveryFailure')) {
                        if ($script:FaultMode -eq 'OutsideEdit') { [IO.File]::AppendAllText($script:FirstTarget,'OUTSIDE-EDIT') }
                        if ($script:FaultMode -eq 'CorruptBackupOnRecovery') { [IO.File]::WriteAllText($script:FirstSource.Replace('/staged/','/original/'),'CORRUPT') }
                        throw 'Injected second replacement failure'
                    }
                    if ($script:ReplaceCount -gt 2 -and $script:FaultMode -eq 'RecoveryFailure') { throw 'Injected restore failure' }
                    & $script:RealReplace $Source $Target
                    if ($script:ReplaceCount -eq 1) { $script:FirstTarget=$Target; $script:FirstSource=$Source }
                    if ($script:FaultMode -eq 'AppliedOutputCorrupt' -and $script:ReplaceCount -eq 1) { [IO.File]::AppendAllText($Target,'OUTSIDE-EDIT') }
                }
            } $failure
            $r=Invoke-F1Update $p
            if ($failure -in @('OutsideEdit','CorruptBackupOnRecovery','RecoveryFailure','AppliedOutputCorrupt')) {
                Assert-True ($r.Status -eq 'ManualRecoveryRequired' -and $r.ManualRecoveryPaths.Count -ge 1) 'unsafe recovery was not reported'
                Assert-True (Test-Path "$($r.BackupDirectory)/recovery.json") 'recovery record missing'
            } else {
                Assert-True ($r.Status -eq 'FailedRecovered') 'failed update did not recover safely'
                foreach ($c in $p.Changes) { Assert-True ((Hash-Bytes ([IO.File]::ReadAllBytes("$root/$($c.RelativePath)"))) -eq $c.OriginalHash) 'failed run altered target' }
            }
            foreach ($rel in @('Shelf1/config.lua','Rainmeter.ini','CafeLock.ini','@Resources/Icons/owner.png','@Resources/Themes/owner.inc')) {
                Assert-True ([IO.File]::ReadAllText("$root/$rel") -eq 'PRIVATE-SENTINEL-never-read-by-updater') 'unrelated fixture modified on failure'
            }
        }
    }
    Import-Module "$PackageDirectory/Updater.psm1" -Force
}
if ($Suite -in @('UI','All')) {
    Assert-True (Test-Path "$PackageDirectory/Update-ShelfSuite.ps1") 'guided updater must exist'
    function Setup-Ui {
        . "$PackageDirectory/Update-ShelfSuite.ps1"
        $script:UiRoot=New-SkinFixture (Join-Path $scratch ([Guid]::NewGuid().ToString('N'))) $upstream
        $script:UiMessages=New-Object Collections.Generic.List[string]; $script:UiConfirm=$false; $script:UiCancelFolder=$false; $script:UiBlocked=$false
        function script:Select-F1Folder { if ($script:UiCancelFolder) { return $null }; return $script:UiRoot }
        function script:Show-F1Message($Text,$ErrorMessage) { $script:UiMessages.Add($Text) }
        function script:Confirm-F1Update($Text) { $script:UiMessages.Add($Text); return $script:UiConfirm }
        function script:Assert-F1UiReady { if ($script:UiBlocked) { throw 'Close Rainmeter normally; process inspection refused.' } }
        $m=Get-Module Updater; & $m { function script:Assert-F1RainmeterClosed {} }
        # Dot-source into script scope, without launching a real dialog on this desktop.
        return ${function:Invoke-F1GuidedUpdate}
    }
    Test-Case 'CancelChangesNothing' {
        $run=Setup-Ui; $before=@([IO.Directory]::EnumerateDirectories((Split-Path $script:UiRoot -Parent)))
        Assert-True ((& $run) -eq 0) 'confirmation cancel is not successful exit'
        Assert-True (@(Compare-Object $before @([IO.Directory]::EnumerateDirectories((Split-Path $script:UiRoot -Parent)))).Count -eq 0) 'cancel created backup'
        $script:UiCancelFolder=$true; Assert-True ((& $run) -eq 0) 'folder cancel failed'
    }
    Test-Case 'RunningRainmeterRefuses' { $run=Setup-Ui; $script:UiBlocked=$true; Assert-True ((& $run) -eq 1) 'running process not refused' }
    Test-Case 'ProcessInspectionFailureRefuses' { $run=Setup-Ui; $script:UiBlocked=$true; Assert-True ((& $run) -eq 1 -and $script:UiMessages.Count -eq 1) 'unavailable process inspection not refused' }
    Test-Case 'ResolvedPathAndChangedListConfirmed' {
        $run=Setup-Ui; $script:UiConfirm=$true
        Assert-True ((& $run) -eq 0) 'guided update failed'
        Assert-True (($script:UiMessages -join "`n").IndexOf($script:UiRoot) -ge 0) 'resolved root not displayed'
        Assert-True (($script:UiMessages -join "`n").IndexOf('Shelf3/Shelf.ini') -ge 0) 'change list missing'
    }
    Test-Case 'NoOpSkipsBackup' {
        $run=Setup-Ui; $script:UiConfirm=$true; & $run | Out-Null
        $before=@([IO.Directory]::EnumerateDirectories((Split-Path $script:UiRoot -Parent))); & $run | Out-Null
        Assert-True (@(Compare-Object $before @([IO.Directory]::EnumerateDirectories((Split-Path $script:UiRoot -Parent)))).Count -eq 0) 'UI no-op created backup'
    }
    Test-Case 'SafeErrorContainsNoSourceContents' {
        $run=Setup-Ui; [IO.File]::AppendAllText("$script:UiRoot/Shelf1/Shelf.ini",'DO-NOT-DISPLAY-CONTENT')
        Assert-True ((& $run) -eq 1) 'custom source not refused'
        Assert-True (($script:UiMessages -join "`n").IndexOf('DO-NOT-DISPLAY-CONTENT') -lt 0 -and ($script:UiMessages -join "`n").IndexOf('Shelf1/Shelf.ini') -ge 0) 'error exposed contents or omitted filename'
    }
    Test-Case 'LauncherUsesBuiltinPowerShellAndDifferentWorkingDirectory' {
        $launch=Join-Path $scratch ('launcher space '+[char]0xe9); [IO.Directory]::CreateDirectory($launch) | Out-Null
        [IO.File]::Copy("$PackageDirectory/Update-ShelfSuite.cmd","$launch/Update-ShelfSuite.cmd")
        $stub='Write-Output ("PS="+$PSVersionTable.PSVersion.ToString()); Write-Output ("SCRIPT="+$PSScriptRoot); exit 0'
        Write-Fixture "$launch/Update-ShelfSuite.ps1" $script:Utf8.GetBytes($stub)
        $start=New-Object Diagnostics.ProcessStartInfo
        $start.FileName=$env:ComSpec; $start.Arguments='/d /c ""'+"$launch\Update-ShelfSuite.cmd"+'""'; $start.WorkingDirectory=$scratch
        $start.UseShellExecute=$false; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true; $start.RedirectStandardInput=$true
        $process=New-Object Diagnostics.Process; $process.StartInfo=$start
        try {
            $process.Start() | Out-Null; $process.StandardInput.WriteLine(' '); $process.StandardInput.Close()
            $output=$process.StandardOutput.ReadToEnd(); $err=$process.StandardError.ReadToEnd(); $process.WaitForExit()
            Assert-True ($process.ExitCode -eq 0 -and $output.IndexOf('PS=5.1') -ge 0 -and $output.IndexOf($launch) -ge 0) "launcher runtime/path failure: $err"
        } finally { $process.Dispose() }
    }
    Test-Case 'PolicyRefusalDoesNotAlterPolicy' {
        $launch=Join-Path $scratch 'policy-refusal'; [IO.Directory]::CreateDirectory($launch) | Out-Null
        [IO.File]::Copy("$PackageDirectory/Update-ShelfSuite.cmd","$launch/Update-ShelfSuite.cmd")
        Write-Fixture "$launch/Update-ShelfSuite.ps1" $script:Utf8.GetBytes('[IO.File]::WriteAllText((Join-Path $PSScriptRoot "unexpected.txt"),"ran"); exit 0')
        $before=[Microsoft.PowerShell.ExecutionPolicy]::Restricted # child process only; persistent policy is untouched
        $start=New-Object Diagnostics.ProcessStartInfo
        $start.FileName=$env:ComSpec; $start.Arguments='/d /c ""'+"$launch\Update-ShelfSuite.cmd"+'""'
        $start.UseShellExecute=$false; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true; $start.RedirectStandardInput=$true
        $start.EnvironmentVariables['PSExecutionPolicyPreference']=$before.ToString()
        $process=New-Object Diagnostics.Process; $process.StartInfo=$start
        try {
            $process.Start() | Out-Null; $process.StandardInput.WriteLine(' '); $process.StandardInput.Close()
            $output=$process.StandardOutput.ReadToEnd(); $err=$process.StandardError.ReadToEnd(); $process.WaitForExit()
            Assert-True ($process.ExitCode -eq 1 -and -not [IO.File]::Exists("$launch/unexpected.txt") -and $output.IndexOf('policy') -ge 0) 'launcher bypassed restricted policy or hid failure'
        } finally { $process.Dispose() }
    }
}
Write-Output "PASS: $script:Passed F1 delivery checks ($Suite); disposable fixtures: $scratch"
