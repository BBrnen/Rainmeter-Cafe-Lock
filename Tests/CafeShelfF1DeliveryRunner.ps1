param([string]$ZipPath,[string]$UpstreamDirectory)
$ErrorActionPreference='Stop'
function Test-F1DeliveryArchive([string]$Path) {
    $zip=[IO.Path]::GetFullPath($Path)
    $sum=[IO.File]::ReadAllText($zip+'.sha256').Trim()
    if ($sum -notmatch '^([0-9a-f]{64})  ([^\\/]+)$' -or $Matches[2] -cne [IO.Path]::GetFileName($zip)) { throw 'Archive checksum record is invalid.' }
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $hash=([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($zip)))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
    if ($hash -cne $Matches[1]) { throw 'Archive checksum mismatch.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $want=@('manifest.json','LICENSE-ShelfSuite.txt','payload/@Resources/ShelfEngine.lua','payload/@Resources/Variables.inc','Updater.psm1','Update-ShelfSuite.ps1','Update-ShelfSuite.cmd','README.md')
        $names=@($archive.Entries | ForEach-Object {$_.FullName.Replace('\','/')})
        if ($names.Count -ne $want.Count -or @(Compare-Object $names $want -CaseSensitive).Count) { throw 'Unexpected or missing ZIP content; extraction refused.' }
        return $names
    } finally { $archive.Dispose() }
}
function Invoke-F1DeliveryRunner([string]$ArchivePath,[string]$SourceDirectory) {
    if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) { throw 'Disposable GitHub Windows runner only.' }
    Test-F1DeliveryArchive $ArchivePath | Out-Null
    $extract=Join-Path $env:RUNNER_TEMP ('CafeF1Extract-'+[Guid]::NewGuid().ToString('N'))
    if (Test-Path -LiteralPath $extract) { throw 'Extraction staging must be fresh.' }
    [IO.Directory]::CreateDirectory($extract) | Out-Null
    [IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($ArchivePath),$extract)
    $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    & icacls $extract /grant "*${sid}:(OI)(CI)F" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not permit fixture access.' }
    & icacls $extract /setintegritylevel '(OI)(CI)M' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not set fixture integrity.' }
    $power=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    # Git/provenance tests run as build preparation; the owner runtime needs no Git.
    & $power -NoProfile -File "$PSScriptRoot/CafeShelfF1Delivery.ps1" -Suite Package -PackageDirectory $extract -UpstreamDirectory ([IO.Path]::GetFullPath($SourceDirectory))
    if ($LASTEXITCODE -ne 0) { throw 'Package-builder verification failed.' }
    $fixtures=Join-Path $env:RUNNER_TEMP ('CafeF1Prepared-'+[Guid]::NewGuid().ToString('N'))
    & "$PSScriptRoot/../Build/ShelfSuiteF1/Package.ps1" -UpstreamDirectory $SourceDirectory -OutputDirectory $fixtures | Out-Null
    & icacls $fixtures /grant "*${sid}:(OI)(CI)F" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not permit prepared fixture access.' }
    & icacls $fixtures /setintegritylevel '(OI)(CI)M' | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not set prepared fixture integrity.' }
    # The elevated hosted account's default process ACL otherwise gives only
    # Administrators full access. Restricted children need self-access to duplicate
    # their own redirected pipe handles. Adjust this runner token only, then restore.
    $acl=New-F1ChildAclScope
    try {
        & "$PSScriptRoot/../RunAsStandard.exe" $power -NoProfile -File "$PSScriptRoot/CafeShelfF1Delivery.ps1" -Suite All -PackageDirectory $extract -UpstreamDirectory ([IO.Path]::GetFullPath($SourceDirectory)) -FixtureBuildDirectory $fixtures -StandardUser
        if ($LASTEXITCODE -ne 0) { throw "Extracted-ZIP standard-user tests failed: $LASTEXITCODE" }
    } finally { $acl.Dispose() }
    Write-Output 'PASS extracted ZIP: complete package tests ran under the restricted token.'
}
function New-F1ChildAclScope {
    if (-not ('CafeF1ChildAclScope' -as [type])) {
        Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
public sealed class CafeF1ChildAclScope : IDisposable {
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int type,IntPtr buffer,int size,out int needed);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool SetTokenInformation(IntPtr token,int type,IntPtr buffer,int size);
    IntPtr token, original;
    public bool Restored { get; private set; }
    public string Before { get; private set; }
    static IntPtr Read(IntPtr token) {
        int needed; GetTokenInformation(token,6,IntPtr.Zero,0,out needed);
        IntPtr value=Marshal.AllocHGlobal(needed);
        if(!GetTokenInformation(token,6,value,needed,out needed)) { Marshal.FreeHGlobal(value); throw new Win32Exception(); }
        return value;
    }
    static RawAcl Acl(IntPtr value) {
        IntPtr pointer=Marshal.ReadIntPtr(value);
        if(pointer==IntPtr.Zero) throw new InvalidOperationException("Missing default ACL");
        byte[] bytes=new byte[(ushort)Marshal.ReadInt16(pointer,2)]; Marshal.Copy(pointer,bytes,0,bytes.Length);
        return new RawAcl(bytes,0);
    }
    public static string Snapshot() {
        IntPtr handle; if(!OpenProcessToken(GetCurrentProcess(),8,out handle)) throw new Win32Exception();
        IntPtr value=IntPtr.Zero;
        try { value=Read(handle); return new RawSecurityDescriptor(ControlFlags.DiscretionaryAclPresent,null,null,null,Acl(value)).GetSddlForm(AccessControlSections.Access); }
        finally { if(value!=IntPtr.Zero) Marshal.FreeHGlobal(value); CloseHandle(handle); }
    }
    public CafeF1ChildAclScope() {
        if(!OpenProcessToken(GetCurrentProcess(),0x88,out token)) throw new Win32Exception();
        IntPtr bytes=IntPtr.Zero, value=IntPtr.Zero;
        try {
            original=Read(token); RawAcl acl=Acl(original);
            Before=new RawSecurityDescriptor(ControlFlags.DiscretionaryAclPresent,null,null,null,acl).GetSddlForm(AccessControlSections.Access);
            acl.InsertAce(acl.Count,new CommonAce(AceFlags.None,AceQualifier.AccessAllowed,0x10000000,WindowsIdentity.GetCurrent().User,false,null));
            byte[] data=new byte[acl.BinaryLength]; acl.GetBinaryForm(data,0);
            bytes=Marshal.AllocHGlobal(data.Length); Marshal.Copy(data,0,bytes,data.Length);
            value=Marshal.AllocHGlobal(IntPtr.Size); Marshal.WriteIntPtr(value,bytes);
            if(!SetTokenInformation(token,6,value,IntPtr.Size)) throw new Win32Exception();
        } catch { if(original!=IntPtr.Zero) Marshal.FreeHGlobal(original); CloseHandle(token); token=IntPtr.Zero; throw; }
        finally { if(value!=IntPtr.Zero) Marshal.FreeHGlobal(value); if(bytes!=IntPtr.Zero) Marshal.FreeHGlobal(bytes); }
    }
    public void Dispose() {
        if(token==IntPtr.Zero) return;
        try { if(!SetTokenInformation(token,6,original,IntPtr.Size)) throw new Win32Exception(); Restored=true; }
        finally { Marshal.FreeHGlobal(original); CloseHandle(token); token=IntPtr.Zero; }
    }
}
'@
    }
    return New-Object CafeF1ChildAclScope
}
if ($MyInvocation.InvocationName -ne '.') { Invoke-F1DeliveryRunner $ZipPath $UpstreamDirectory }
