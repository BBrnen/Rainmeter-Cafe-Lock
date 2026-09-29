param([Parameter(Mandatory=$true)][string]$ShelfSuiteDirectory)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Disposable GitHub Actions runner only' }
# Configure the existing machine handler before restricting the test token.
# Hosted runners can ignore HKCU association overrides. Do not change UserChoice
# or its protected hash; restore every command value touched in finally.
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ShelfAssociation {
 [DllImport("shell32.dll")] public static extern void SHChangeNotify(uint evt,uint flags,IntPtr item1,IntPtr item2);
 [DllImport("shlwapi.dll",CharSet=CharSet.Unicode)] public static extern int AssocQueryString(uint flags,uint str,string assoc,string extra,System.Text.StringBuilder result,ref uint size);
 public static string Query(uint str) {
   var result=new System.Text.StringBuilder(4096); uint size=4096;
   int hr=AssocQueryString(0,str,".html","open",result,ref size);
   if(hr!=0) throw new Exception("HTML association query failed: " + hr);
   return result.ToString();
 }
}
'@
$progId = [ShelfAssociation]::Query(20)
if ($progId -notmatch '^[a-zA-Z0-9_.\\-]+$') { throw 'Unexpected HTML association identifier' }
$probe = Join-Path $env:RUNNER_TEMP ('CafeHtml-' + [guid]::NewGuid())
New-Item -ItemType Directory $probe | Out-Null
Copy-Item "$PSScriptRoot/../CafeLaunchProbe.exe" "$probe/Probe.exe"
$path = "Software\Classes\$progId\shell\open\command"
$backups = @()
try {
  foreach ($hive in @([Microsoft.Win32.Registry]::LocalMachine,[Microsoft.Win32.Registry]::CurrentUser)) {
    $key = $hive.CreateSubKey($path)
    $values = @{}
    foreach ($name in @('','DelegateExecute')) {
      if ($key.GetValueNames() -contains $name) {
        $values[$name] = @($key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames),$key.GetValueKind($name))
      }
    }
    $backups += @{ Key=$key; Values=$values }
    $key.SetValue('', ('"' + "$probe\Probe.exe" + '" "%1"'))
    $key.SetValue('DelegateExecute','')
  }
  [ShelfAssociation]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
  $resolved = [ShelfAssociation]::Query(1)
  if (-not $resolved.Contains("$probe\Probe.exe")) { throw "Windows did not resolve the test HTML handler: $resolved" }
  Write-Output "PASS: Windows resolves the controlled HTML handler for $progId."
  & "$PSScriptRoot/../RunAsStandard.exe" 'C:\Program Files\PowerShell\7\pwsh.exe' -NoProfile -File "$PSScriptRoot/CafeShelfSuite.ps1" -ShelfSuiteDirectory $ShelfSuiteDirectory -HtmlProbeDirectory $probe
  if ($LASTEXITCODE -ne 0) { throw "ShelfSuite standard-user regression failed: $LASTEXITCODE" }
} finally {
  foreach ($backup in $backups) {
    foreach ($name in @('','DelegateExecute')) {
      if ($backup.Values.ContainsKey($name)) { $backup.Key.SetValue($name,$backup.Values[$name][0],$backup.Values[$name][1]) }
      else { $backup.Key.DeleteValue($name,$false) }
    }
    $backup.Key.Dispose()
  }
  [ShelfAssociation]::SHChangeNotify(0x08000000,0,[IntPtr]::Zero,[IntPtr]::Zero)
}
