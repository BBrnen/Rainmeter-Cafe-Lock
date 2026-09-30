@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -STA -File "%~dp0Update-ShelfSuite.ps1"
set "F1_RESULT=%ERRORLEVEL%"
if not "%F1_RESULT%"=="0" (
  echo The update did not complete. Read the error above or the updater dialog.
  echo If Windows policy blocks scripts, ask your PC administrator about that policy.
  echo This launcher does not change execution policy or request elevation.
  pause
)
exit /b %F1_RESULT%
