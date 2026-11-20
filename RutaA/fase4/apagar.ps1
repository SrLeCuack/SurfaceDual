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
# Primero, apagado de verdad pedido a Android (agente, TCP 4449, solo acepta al anfitrión): cierra apps, desmonta /data y se
# apaga solo, como el menú de Android. El del componente de Hyper-V lo apagaba casi en frío: /data sin desmontar en cada apagado
# (dañado tras muchos: Android sin arrancar, 20/11) y pantallas azules de dxgmms2 en un probador. Imágenes viejas: no responde y sigue lo de antes.
# Si el usuario la vuelve a abrir mientras se espera (19/11: "Apagar" y a los 4 s "Ir a Android"), este script no veía el Off
# y cortaba la corriente al Android recién encendido. Reencendida = en marcha desde después de pedir el apagado.
$pedido = Get-Date
function Reencendida { $v = Get-VM -Name $n; $v.State -eq 'Running' -and $v.Uptime -gt [TimeSpan]::Zero -and $v.Uptime -lt ((Get-Date) - $pedido) }
$mac = (Get-VMNetworkAdapter -VMName $n -ErrorAction SilentlyContinue | Select-Object -First 1).MacAddress -replace '(..)(?!$)', '$1-'
$ip = if ($mac) { (Get-NetNeighbor -LinkLayerAddress $mac -AddressFamily IPv4 -ErrorAction SilentlyContinue | Select-Object -First 1).IPAddress }
$enviado = $false
if ($ip -and (Get-VM -Name $n).State -eq 'Running') {
  try { $t = New-Object Net.Sockets.TcpClient; if ($t.ConnectAsync($ip, 4449).Wait(2000)) { $b = [Text.Encoding]::ASCII.GetBytes("apagar ya`n"); $t.GetStream().Write($b, 0, $b.Length); Start-Sleep -Milliseconds 300; $enviado = $true }; $t.Close() } catch { }
}
if ($enviado) {   # sin agente que escuche no se esperan 30 s en balde (revisión Gemini)
  for ($s = 0; $s -lt 30 -and (Get-VM -Name $n).State -ne 'Off' -and -not (Reencendida); $s++) { Start-Sleep 1 }
}
if (Reencendida) { Salir }   # ya es otro arranque del usuario
if ((Get-VM -Name $n).State -eq 'Off') { Get-Process HvCarpetas -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue -ErrorVariable +f; Salir }
$sc = Get-CimInstance -Namespace root\virtualization\v2 -ClassName Msvm_ShutdownComponent -ErrorAction SilentlyContinue | Where-Object { $_.SystemName -eq $vm.Id.Guid.ToUpper() -or $_.SystemName -eq $vm.Id.Guid }
if ($sc) { [void](Invoke-CimMethod -InputObject $sc -MethodName InitiateShutdown -Arguments @{ Force = $true; Reason = 'Surface Dual: apagar Android' } -ErrorAction SilentlyContinue -ErrorVariable +f) }
else { $f += 'sin componente de apagado: corte de corriente' }
for ($s = 0; $s -lt 60 -and (Get-VM -Name $n).State -ne 'Off' -and -not (Reencendida); $s++) { Start-Sleep 1 }
if (Reencendida) { Salir }   # ya es otro arranque del usuario: ni corte ni cerrar sus carpetas
if ((Get-VM -Name $n).State -ne 'Off') { $f += 'no se apagó de forma ordenada: corte de corriente'; Stop-VM -Name $n -TurnOff -Force -ErrorAction SilentlyContinue -ErrorVariable +f }
# Carpetas de Windows: después del apagado, para que Android termine de escribir lo pendiente.
Get-Process HvCarpetas -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue -ErrorVariable +f
Salir
