@echo off
rem Surface Dual: doble clic para desinstalar y dejar el equipo como estaba.
cd /d "%SystemRoot%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0desinstalar.ps1" %*
pause
