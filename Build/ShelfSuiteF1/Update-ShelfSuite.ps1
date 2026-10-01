Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$script:F1UiPackageDirectory=$PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'Updater.psm1') -Force -Global
Add-Type -AssemblyName System.Windows.Forms
function Show-F1Message([string]$Text,[bool]$ErrorMessage=$false) {
    $icon=[Windows.Forms.MessageBoxIcon]::Information
    if ($ErrorMessage) { $icon=[Windows.Forms.MessageBoxIcon]::Error }
    [Windows.Forms.MessageBox]::Show($Text,'ShelfSuite F1 Compatibility',[Windows.Forms.MessageBoxButtons]::OK,$icon) | Out-Null
}
function Select-F1Folder {
    $picker=New-Object Windows.Forms.FolderBrowserDialog
    $picker.Description='Choose your existing Shelf Suite directory. No second skin will be installed.'
    $picker.ShowNewFolderButton=$false
    try { if ($picker.ShowDialog() -eq [Windows.Forms.DialogResult]::OK) { return $picker.SelectedPath }; return $null } finally { $picker.Dispose() }
}
function Confirm-F1Update([string]$Text) {
    return [Windows.Forms.MessageBox]::Show($Text,'Confirm ShelfSuite F1 update',[Windows.Forms.MessageBoxButtons]::OKCancel,[Windows.Forms.MessageBoxIcon]::Question) -eq [Windows.Forms.DialogResult]::OK
}
function Assert-F1UiReady {
    $principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run Update-ShelfSuite normally, not as administrator.' }
    & (Get-Module Updater) { Assert-F1RainmeterClosed }
}
function Invoke-F1GuidedUpdate {
    try {
        Assert-F1UiReady
        $package=Read-F1Package $script:F1UiPackageDirectory
        if (-not $package.Complete) { throw 'This package is incomplete. Extract the verified compatibility ZIP.' }
        Show-F1Message "This updates F1 tabs in your EXISTING Shelf Suite only. Close Rainmeter normally first.`r`n`r`nMake a separate personal backup yourself if you want one. This updater backs up only files it will change and never reads or copies launcher configuration."
        $root=Select-F1Folder
        if (-not $root) { return 0 }
        $plan=Get-F1Preflight $root $package
        if ($plan.Changes.Count -eq 0) { Show-F1Message 'This Shelf Suite already has the approved F1 compatibility files. No action needed; no backup created.'; return 0 }
        $list=(@($plan.Changes | ForEach-Object {$_.RelativePath}) -join "`r`n")
        if (-not (Confirm-F1Update ("Existing installation:`r`n"+$plan.Root+"`r`n`r`nOnly these files will change:`r`n"+$list+"`r`n`r`nA verified timestamped backup will be created beside Shelf Suite before any replacement. Select OK to update or Cancel to change nothing."))) { return 0 }
        $result=Invoke-F1Update $plan
        if ($result.Status -eq 'Updated') { Show-F1Message ("Update verified. Keep your backup:`r`n"+$result.BackupDirectory+"`r`n`r`nStart Rainmeter yourself and perform the README's manual checks."); return 0 }
        if ($result.Status -eq 'NoAction') { Show-F1Message 'No action needed. No backup created.'; return 0 }
        $text=($result.Errors -join "`r`n")
        if ($result.BackupDirectory) {
            $text += "`r`n`r`nKeep this backup:`r`n"+$result.BackupDirectory
            if ([IO.File]::Exists((Join-Path $result.BackupDirectory 'RESTORE.txt'))) { $text += "`r`nRead RESTORE.txt for manual recovery." }
            else { $text += "`r`nBackup preparation is incomplete; installed files were not replaced. Do not restore from this incomplete backup. Resolve the reported failure before retrying." }
        }
        if ($result.Status -eq 'FailedRecovered') { $text += "`r`nNo updater changes remain in installed files; inspect any outside edits before retrying." }
        else { $text += "`r`nManual inspection/restore required for:`r`n"+($result.ManualRecoveryPaths -join "`r`n") }
        Show-F1Message $text $true
        return 1
    } catch { Show-F1Message $_.Exception.Message $true; return 1 }
}
if ($MyInvocation.InvocationName -ne '.') { exit (Invoke-F1GuidedUpdate) }
