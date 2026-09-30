; Rainmeter Cafe Lock, GPL v2 or later. Dedicated x64 package; upstream installer unchanged.
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "RuntimePolicy.nsh"
Var RuntimePresent
Var RuntimeUserPresent
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

Function DetectRuntime
 ; Microsoft documents HKLM's 32-bit view and HKCU for Evergreen detection.
 StrCpy $RuntimePresent 0
 StrCpy $RuntimeUserPresent 0
 SetRegView 32
 ReadRegStr $0 HKLM "Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" "pv"
 ${If} $0 != ""
 ${AndIf} $0 != "0.0.0.0"
  StrCpy $RuntimePresent 1
 ${Else}
  ReadRegStr $0 HKCU "Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" "pv"
  ${If} $0 != ""
  ${AndIf} $0 != "0.0.0.0"
   StrCpy $RuntimeUserPresent 1
  ${EndIf}
 ${EndIf}
 SetRegView 64
FunctionEnd

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

Section "Optional WebView2 prerequisite"
 Call DetectRuntime
 StrCpy $3 0
 IfSilent 0 +2
 StrCpy $3 1
 ${GetParameters} $4
 ${GetOptions} $4 "/INSTALLWEBVIEW2=" $5
 StrCpy $4 0
 ${If} $5 == "1"
  StrCpy $4 1
 ${EndIf}
 Push $RuntimePresent
 Push $RuntimeUserPresent
 Push $3
 Push $4
 Call RuntimeDecision
 Pop $3
 ${If} $3 == 0
  Goto runtime_done
 ${ElseIf} $3 == 2
  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "The ShelfSuite editor needs Microsoft WebView2 Runtime. Install the bundled Microsoft prerequisite now? It works offline. Choosing No leaves normal Rainmeter launchers usable; the editor will explain the missing prerequisite." /SD IDNO IDYES runtime_install
  Goto runtime_done
 ${EndIf}
 runtime_install:
 InitPluginsDir
 SetOutPath "$PLUGINSDIR"
 ; Package.ps1 verifies digest/version/Authenticode before these bytes are embedded.
 File /oname=MicrosoftEdgeWebView2RuntimeInstallerX64.exe "${RUNTIME_INSTALLER}"
 ClearErrors
 ExecWait '"$PLUGINSDIR\MicrosoftEdgeWebView2RuntimeInstallerX64.exe" /silent /install' $6
 ${If} ${Errors}
  StrCpy $6 1603
 ${EndIf}
 Call DetectRuntime
 Push $6
 Push $RuntimePresent
 Call RuntimeInstallResult
 Pop $3
 Delete "$PLUGINSDIR\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
 ${If} $3 != 1
  DetailPrint "WebView2 setup did not complete (exit $6). Rainmeter is installed; editor prerequisite still needs attention."
  MessageBox MB_ICONEXCLAMATION "Rainmeter Cafe Lock is installed, but WebView2 setup failed (exit $6). Existing launchers remain usable. Install or repair Microsoft WebView2 Runtime before using the ShelfSuite editor." /SD IDOK
  SetErrorLevel 1603
  Quit
 ${EndIf}
 runtime_done:
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
