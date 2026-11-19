# Apaga Android de forma ordenada y libera su RAM (8 GB fijos; con GPU-P no hay Save-VM ni memoria dinámica útil: medido).
# Lo usan la Bandeja ("Apagar Android"), el botón Apagar del launcher y el apagado automático tras 1 h suspendido (VisorVM).
# Un fallo no corta el resto: queda en %TEMP%\surfacedual_apagar.log y el script sale con 1.
$n = 'SurfaceDualAndroid'; $f = @()
function Salir { if ($f) { $f | ForEach-Object { "{0:s} apagar.ps1: {1}" -f (Get-Date), $_ } | Add-Content (Join-Path $env:TEMP 'surfacedual_apagar.log'); exit 1 }; exit 0 }
Get-Process VisorVM, HvAudio, HvCamara, HvSensores, HvMdns -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue -ErrorVariable +f
# Visor muerto de golpe con Android delante: Windows quedaba a 48 Hz, sin giro y con el umbral de ahorro al 100 %.
Get-Process VisorVM -ErrorAction SilentlyContinue | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
Start-Process (Join-Path $PSScriptRoot 'host\VisorVM.exe') '--restaurar' -Wait -ErrorAction SilentlyContinue -ErrorVariable +f
$vm = Get-VM -Name $n -ErrorAction SilentlyContinue
if (-not $vm -or $vm.State -eq 'Off') { Get-Process HvCarpetas -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue -ErrorVariable +f; Salir }
# En pausa no atiende el apagado: se reanuda. Stop-VM -Force pide el apagado ordenado a Android (medido 3,5 s, boot reason
# "reboot"); si no queda apagada, se corta la corriente (y queda en el registro).
if ($vm.State -eq 'Paused') { Resume-VM -Name $n -ErrorAction SilentlyContinue -ErrorVariable +f }
# Stop-VM -Force deja en Hyper-V un trabajo "Apagando" que no se cancela y bloquea -TurnOff si Android no termina (visto 18/11:
# >9 min colgado). Se pide el apagado al componente de apagado (no deja trabajo) y se espera como mucho 60 s.
$sc = Get-CimInstance -Namespace root\virtualization\v2 -ClassName Msvm_ShutdownComponent -ErrorAction SilentlyContinue | Where-Object { $_.SystemName -eq $vm.Id.Guid.ToUpper() -or $_.SystemName -eq $vm.Id.Guid }
if ($sc) { [void](Invoke-CimMethod -InputObject $sc -MethodName InitiateShutdown -Arguments @{ Force = $true; Reason = 'Surface Dual: apagar Android' } -ErrorAction SilentlyContinue -ErrorVariable +f) }
else { $f += 'sin componente de apagado: corte de corriente' }
for ($s = 0; $s -lt 60 -and (Get-VM -Name $n).State -ne 'Off'; $s++) { Start-Sleep 1 }
if ((Get-VM -Name $n).State -ne 'Off') { $f += 'no se apagó de forma ordenada: corte de corriente'; Stop-VM -Name $n -TurnOff -Force -ErrorAction SilentlyContinue -ErrorVariable +f }
# Carpetas de Windows: después del apagado, para que Android termine de escribir lo pendiente.
Get-Process HvCarpetas -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue -ErrorVariable +f
Salir
