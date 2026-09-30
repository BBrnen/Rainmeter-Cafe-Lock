; Shared, deterministic prerequisite policy. No registry writes or execution here.
; Stack inputs: present, silent, explicitly requested. Output: 0 skip, 1 install, 2 ask.
Function RuntimeDecision
 Pop $2
 Pop $1
 Pop $0
 ${If} $0 == 1
  Push 0
 ${ElseIf} $2 == 1
  Push 1
 ${ElseIf} $1 == 1
  Push 0
 ${Else}
  Push 2
 ${EndIf}
FunctionEnd

; Stack inputs: installer exit code, present after install. Output: 1 success, 0 failure.
Function RuntimeInstallResult
 Pop $1
 Pop $0
 ${If} $1 == 1
 ${AndIf} $0 == 0
  Push 1
 ${ElseIf} $1 == 1
 ${AndIf} $0 == 3010
  Push 1
 ${Else}
  Push 0
 ${EndIf}
FunctionEnd
