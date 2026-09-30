; Shared, deterministic prerequisite policy. No registry writes or execution here.
; Stack inputs: present, silent, explicitly requested. Output: 0 skip, 1 install, 2 ask.
Function RuntimeDecision
 Pop $2
 Pop $1
 Pop $0
 Push 0
FunctionEnd

; Stack inputs: installer exit code, present after install. Output: 1 success, 0 failure.
Function RuntimeInstallResult
 Pop $1
 Pop $0
 Push 0
FunctionEnd
