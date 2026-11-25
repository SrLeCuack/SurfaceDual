# Volver a Windows desde Android en Hyper-V: cierra el Visor y el audio y pausa la VM (0 % de CPU; iniciar.ps1 la reanuda).
# Un fallo no corta el resto: queda en %TEMP%\surfacedual_pausar.log y el script sale con 1.
$n = 'SurfaceDualAndroid'; $f = @()
Get-Process VisorVM, HvAudio, HvCamara, HvSensores, HvMdns -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue -ErrorVariable +f
# Visor muerto de golpe con Android delante: Windows quedaba a 48 Hz, sin giro y con el umbral de ahorro al 100 %.
Get-Process VisorVM -ErrorAction SilentlyContinue | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
try { & (Join-Path $PSScriptRoot 'congelar.ps1') -Accion Descongelar } catch { }   # Visor ya cerrado: los programas de Windows, tal como estaban
Start-Process (Join-Path $PSScriptRoot 'host\VisorVM.exe') '--restaurar' -Wait -ErrorAction SilentlyContinue -ErrorVariable +f
# HvCarpetas sigue vivo: que no reintente contra la VM en pausa (VmActiva, HvPuente.cs).
$a = $null; if ([Threading.EventWaitHandle]::TryOpenExisting('Local\SurfaceDualVmActiva', [ref]$a)) { [void]$a.Reset() }
if ((Get-VM -Name $n -ErrorAction SilentlyContinue -ErrorVariable +f).State -eq 'Running') { Suspend-VM -Name $n -ErrorAction SilentlyContinue -ErrorVariable +f }
if ($f) { $f | ForEach-Object { "{0:s} pausar.ps1: {1}" -f (Get-Date), $_ } | Add-Content (Join-Path $env:TEMP 'surfacedual_pausar.log'); exit 1 }
