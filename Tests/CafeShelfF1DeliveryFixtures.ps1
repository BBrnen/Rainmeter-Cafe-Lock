$script:Utf8 = New-Object Text.UTF8Encoding($false, $true)
function Hash-Bytes([byte[]]$Bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant() } finally { $sha.Dispose() }
}
function Write-Fixture([string]$Path, [byte[]]$Bytes) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    [IO.File]::WriteAllBytes($Path, $Bytes)
}
function New-SkinFixture([string]$Parent, [string]$UpstreamDirectory) {
    $root = Join-Path $Parent 'Shelf Suite'
    foreach ($rel in @('@Resources/ShelfEngine.lua', '@Resources/Variables.inc', 'Shelf1/Shelf.ini', 'Shelf2/Shelf.ini', 'Shelf3/Shelf.ini')) {
        Write-Fixture (Join-Path $root $rel) ([IO.File]::ReadAllBytes((Join-Path $UpstreamDirectory "Shelf Suite/$rel")))
    }
    # Harness-owned sentinels only. The updater must not even open them.
    foreach ($rel in @('Shelf1/config.lua', 'Rainmeter.ini', 'CafeLock.ini', '@Resources/Icons/owner.png', '@Resources/Themes/owner.inc')) {
        Write-Fixture (Join-Path $root $rel) ($script:Utf8.GetBytes('PRIVATE-SENTINEL-never-read-by-updater'))
    }
    return $root
}
function Assert-True([bool]$Condition, [string]$Message) { if (-not $Condition) { throw "ASSERT: $Message" } }
function Assert-Refused([scriptblock]$Action, [string]$Name) {
    $refused = $false
    try { & $Action | Out-Null } catch { $refused = $true }
    Assert-True $refused "$Name must refuse"
}
function Test-Case([string]$Name, [scriptblock]$Action) {
    & $Action
    $script:Passed++
    Write-Output "PASS $Name"
}
if (-not ('CafeF1TestToken' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CafeF1TestToken {
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetShortPathName(string path,System.Text.StringBuilder buffer,uint size);
    public static string ShortPath(string path) {
        var buffer=new System.Text.StringBuilder(32768);
        uint size=GetShortPathName(path,buffer,(uint)buffer.Capacity);
        if(size==0 || size>=buffer.Capacity) throw new System.ComponentModel.Win32Exception();
        return buffer.ToString();
    }
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetKernelObjectSecurity(IntPtr handle,uint info,byte[] buffer,uint length,out uint needed);
    public static string ProcessAcl() {
        uint needed; GetKernelObjectSecurity(GetCurrentProcess(),4,null,0,out needed);
        byte[] bytes=new byte[needed];
        if(!GetKernelObjectSecurity(GetCurrentProcess(),4,bytes,needed,out needed)) return "query error="+Marshal.GetLastWin32Error();
        return new System.Security.AccessControl.RawSecurityDescriptor(bytes,0).GetSddlForm(System.Security.AccessControl.AccessControlSections.Access);
    }
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int type,IntPtr buffer,int length,out int needed);
    [DllImport("advapi32.dll")] static extern IntPtr GetSidSubAuthorityCount(IntPtr sid);
    [DllImport("advapi32.dll")] static extern IntPtr GetSidSubAuthority(IntPtr sid,uint index);
    public static int Integrity() {
        IntPtr token; if(!OpenProcessToken(GetCurrentProcess(),8,out token)) throw new Exception("Token query failed");
        IntPtr buffer=Marshal.AllocHGlobal(1024);
        try {
            int needed; if(!GetTokenInformation(token,25,buffer,1024,out needed)) throw new Exception("Integrity query failed");
            IntPtr sid=Marshal.ReadIntPtr(buffer); uint count=Marshal.ReadByte(GetSidSubAuthorityCount(sid));
            return Marshal.ReadInt32(GetSidSubAuthority(sid,count-1));
        } finally { Marshal.FreeHGlobal(buffer); CloseHandle(token); }
    }
}
'@
}
