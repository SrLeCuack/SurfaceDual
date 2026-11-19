@echo off
rem Surface Dual: doble clic para comprobar el equipo e instalar. No cambia nada sin preguntar.
cd /d "%SystemRoot%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalar.ps1" %*
pause
