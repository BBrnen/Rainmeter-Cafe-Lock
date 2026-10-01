Unicode true
!include "LogicLib.nsh"
Name "Cafe Runtime policy tests"
OutFile "${TEST_OUTPUT}"
RequestExecutionLevel user
SilentInstall silent
!include "..\Build\CafeInstaller\RuntimePolicy.nsh"
Var Failures
!macro Decision present perUser silent requested expected
 Push ${present}
 Push ${perUser}
 Push ${silent}
 Push ${requested}
 Call RuntimeDecision
 Pop $4
 ${If} $4 != ${expected}
  IntOp $Failures $Failures + 1
 ${EndIf}
!macroend
!macro Result code present expected
 Push ${code}
 Push ${present}
 Call RuntimeInstallResult
 Pop $3
 ${If} $3 != ${expected}
  IntOp $Failures $Failures + 1
 ${EndIf}
!macroend
Section
 StrCpy $Failures 0
 !insertmacro Decision 1 0 0 0 0
 !insertmacro Decision 1 1 1 1 0
 !insertmacro Decision 0 0 1 0 0
 !insertmacro Decision 0 0 1 1 1
 !insertmacro Decision 0 0 0 0 2
 !insertmacro Decision 0 0 0 1 1
 !insertmacro Decision 0 1 0 0 2
 !insertmacro Decision 0 1 1 1 1
 !insertmacro Result 0 1 1
 !insertmacro Result 3010 1 1
 !insertmacro Result 0 0 0
 !insertmacro Result 1603 1 0
 SetErrorLevel $Failures
SectionEnd
