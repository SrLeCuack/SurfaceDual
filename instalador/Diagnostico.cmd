@echo off
rem Surface Dual: junta en un .zip del Escritorio lo necesario para investigar un fallo (sin datos personales). No cambia nada.
rem Surface Dual: collects what is needed to investigate a problem into a .zip on the Desktop (no personal data). Changes nothing.
cd /d "%SystemRoot%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0diagnostico.ps1" %*
pause
