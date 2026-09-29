@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0CafeShelfDropProbe.ps1" -DisposableVM
echo.
pause
