; Rainmeter Cafe Lock, GPL v2 or later. Dedicated x64 package; upstream installer unchanged.
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!ifndef PAYLOAD
 !error "PAYLOAD and OUTFILE must be supplied by Package.ps1"
!endif
Name "Rainmeter Cafe Lock"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\Rainmeter Cafe Lock"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
AllowSkipFiles off
VIProductVersion "4.5.26.3894"
VIAddVersionKey "ProductName" "Rainmeter Cafe Lock"
VIAddVersionKey "FileDescription" "Rainmeter Cafe Lock x64 Setup"
VIAddVersionKey "FileVersion" "4.5.26.3894"
VIAddVersionKey "LegalCopyright" "Rainmeter contributors; GPL v2 or later"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TEXT "Rainmeter Cafe Lock is installed. Open it from the Start menu in the cafe Windows account, then use Unlock / Enter Maintenance Mode to create the password before customer use. It will start locked when any Windows user signs in. Setup does not launch Rainmeter as administrator."
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
 ${IfNot} ${RunningX64}
  MessageBox MB_ICONSTOP "Rainmeter Cafe Lock requires 64-bit Windows." /SD IDOK
  SetErrorLevel 1633
  Quit
 ${EndIf}
 SetRegView 64
 SetShellVarContext all
FunctionEnd

!macro CheckNotRunning
 IfFileExists "$INSTDIR\Rainmeter.dll" 0 +7
 System::Call 'kernel32::CreateFileW(w "$INSTDIR\Rainmeter.dll", i 0x40000000, i 0, p 0, i 3, i 0, p 0) p .r0'
 StrCmp $0 -1 0 +4
 MessageBox MB_ICONSTOP "Close Rainmeter Cafe Lock from Maintenance Mode before updating or uninstalling." /SD IDOK
 SetErrorLevel 1618
 Quit
 System::Call 'kernel32::CloseHandle(p r0)'
!macroend

Section "Install"
 ; Fixed protected location, including silent installs. No writable portable target.
 StrCpy $INSTDIR "$PROGRAMFILES64\Rainmeter Cafe Lock"
 !insertmacro CheckNotRunning
 SetOutPath "$INSTDIR"
 File /r "${PAYLOAD}\*.*"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 ; Language-independent SIDs. No permissions on user settings/skins are changed.
 nsExec::ExecToStack '"$SYSDIR\icacls.exe" "$INSTDIR" /inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F" "*S-1-5-32-545:(OI)(CI)RX"'
 Pop $0
 Pop $1
 StrCmp $0 0 +4
 MessageBox MB_ICONSTOP "Could not secure the installation directory. Setup failed." /SD IDOK
 SetErrorLevel 5
 Quit
 CreateDirectory "$SMPROGRAMS\Rainmeter Cafe Lock"
 CreateShortCut "$SMPROGRAMS\Rainmeter Cafe Lock\Rainmeter Cafe Lock.lnk" "$INSTDIR\Rainmeter.exe"
 ; Shared Startup runs with each signed-in user's token, never as a service/admin task.
 CreateShortCut "$SMSTARTUP\Rainmeter Cafe Lock.lnk" "$INSTDIR\Rainmeter.exe"
 WriteRegStr HKLM "Software\Rainmeter Cafe Lock" "InstallLocation" "$INSTDIR"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock" "DisplayName" "Rainmeter Cafe Lock (x64)"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock" "DisplayVersion" "4.5.26.3894"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock" "Publisher" "Rainmeter Cafe Lock project"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock" "DisplayIcon" "$INSTDIR\Rainmeter.exe,0"
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock" "NoModify" 1
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock" "NoRepair" 1
SectionEnd

Function un.onInit
 SetRegView 64
 SetShellVarContext all
 ; Uninstall only our fixed application directory.
 StrCpy $INSTDIR "$PROGRAMFILES64\Rainmeter Cafe Lock"
 !insertmacro CheckNotRunning
FunctionEnd
Section "Uninstall"
 ; Generated list removes only files shipped by this package; preserve user data.
 !include "${DELETE_MANIFEST}"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 Delete "$SMPROGRAMS\Rainmeter Cafe Lock\Rainmeter Cafe Lock.lnk"
 Delete "$SMSTARTUP\Rainmeter Cafe Lock.lnk"
 RMDir "$SMPROGRAMS\Rainmeter Cafe Lock"
 DeleteRegKey HKLM "Software\Rainmeter Cafe Lock"
 DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Rainmeter Cafe Lock"
SectionEnd
