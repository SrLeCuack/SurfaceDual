@echo off
rem Surface Dual: actualiza una instalacion existente con esta version (no toca las apps ni los datos de Android).
rem Surface Dual: updates an existing installation to this version (does not touch Android apps or data).
cd /d "%SystemRoot%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0actualizar.ps1" %*
pause
