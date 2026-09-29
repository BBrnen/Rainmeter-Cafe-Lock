# Shared native UI helpers for the disposable Windows CI tests.
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class LockNative {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] struct CopyData { public IntPtr Id; public int Size; public IntPtr Data; }
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w, out Rect r);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetDlgItemText(IntPtr w,int id,string text);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetDlgItemText(IntPtr w,int id,System.Text.StringBuilder text,int size);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
 public delegate bool EnumChildProc(IntPtr w,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr w,EnumChildProc proc,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr w,System.Text.StringBuilder text,int size);
 public static IntPtr Child(IntPtr parent,string text) {
   IntPtr found=IntPtr.Zero;
   EnumChildWindows(parent,(w,p)=>{var b=new System.Text.StringBuilder(256);GetWindowText(w,b,256);if(b.ToString()==text)found=w;return true;},IntPtr.Zero);
   return found;
 }
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,uint msg,IntPtr wp,IntPtr lp);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
 [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int key);
 [StructLayout(LayoutKind.Sequential)] struct IconId { public uint Size; public IntPtr Window; public uint Id; public Guid Guid; }
 [DllImport("shell32.dll")] static extern int Shell_NotifyIconGetRect(ref IconId id, out Rect rect);
 public static bool HasTrayIcon(IntPtr window) {
   var id = new IconId {Size=(uint)Marshal.SizeOf(typeof(IconId)),Window=window,Id=100};
   Rect rect; return Shell_NotifyIconGetRect(ref id,out rect)==0;
 }
 [DllImport("user32.dll", SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr w,uint msg,IntPtr wp,IntPtr lp,uint flags,uint ms,out IntPtr result);
 public static IntPtr Send(IntPtr w,uint msg,long wp,long lp) {
   IntPtr result; if(SendMessageTimeout(w,msg,new IntPtr(wp),new IntPtr(lp),2,20000,out result)==IntPtr.Zero)
     throw new Exception("Window message failed or timed out: " + msg);
   return result;
 }
 public static void Bang(IntPtr w,string text) {
   IntPtr data=Marshal.StringToHGlobalUni(text), ptr=IntPtr.Zero;
   try {
     CopyData cds=new CopyData {Id=new IntPtr(1),Size=(text.Length+1)*2,Data=data};
     ptr=Marshal.AllocHGlobal(Marshal.SizeOf(cds)); Marshal.StructureToPtr(cds,ptr,false);
     Send(w,0x4a,0,ptr.ToInt64());
   } finally { if(ptr!=IntPtr.Zero) Marshal.FreeHGlobal(ptr); Marshal.FreeHGlobal(data); }
 }
}
'@
function Wait-For($predicate, $description) {
  $deadline = [DateTime]::UtcNow.AddSeconds(15)
  while (-not (& $predicate)) {
    if ([DateTime]::UtcNow -gt $deadline) { throw "Timed out: $description" }
    Start-Sleep -Milliseconds 100
  }
}
function Position($window) {
  $r = [LockNative+Rect]::new()
  if (-not [LockNative]::GetWindowRect($window, [ref]$r)) { throw 'Missing skin window' }
  return "$($r.Left),$($r.Top)"
}
function Password-Dialog($title) {
  Wait-For { [LockNative]::FindWindow('#32770',$title) -ne [IntPtr]::Zero } $title
  return [LockNative]::FindWindow('#32770',$title)
}
function Type-PasswordField($dialog, $id, [string]$value) {
  $edit = [LockNative]::GetDlgItem($dialog,$id)
  if ($edit -eq [IntPtr]::Zero) { if ($value.Length) { throw 'Missing password input' }; return }
  [void][LockNative]::SetDlgItemText($dialog,$id,'')
  foreach ($character in $value.ToCharArray()) {
    [void][LockNative]::PostMessage($edit,0x102,[IntPtr]([int]$character),[IntPtr]::Zero)
  }
}
function Submit-Password($dialog, [string]$password, [string]$confirm = '', [string]$current = '') {
  if (-not $password.Length) { throw 'Test fixture supplied an empty password' }
  Type-PasswordField $dialog 100 $password
  Type-PasswordField $dialog 101 $confirm
  Type-PasswordField $dialog 102 $current
  [void][LockNative]::SetDlgItemText($dialog,203,'')
  # Characters and the click share Rainmeter's UI queue, like ordinary typing.
  [void][LockNative]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)
  Wait-For {
    if (-not [LockNative]::IsWindow($dialog)) { return $true }
    $statusText = [Text.StringBuilder]::new(256)
    [void][LockNative]::GetDlgItemText($dialog,203,$statusText,256)
    return $statusText.Length -gt 0
  } 'password submission completed'

}
function Assert-PasswordError($dialog, $expected = '') {
  $text = [Text.StringBuilder]::new(256)
  [void][LockNative]::GetDlgItemText($dialog,203,$text,256)
  if (-not [LockNative]::IsWindow($dialog) -or $text.Length -eq 0) { throw 'Expected password error with dialog still open' }
  if ($expected -and $text.ToString() -ne $expected) { throw "Unexpected validation message: $text" }
}
function Cancel-Password($title) {
  $dialog = Password-Dialog $title
  [void][LockNative]::Send($dialog,0x111,2,0)
}
