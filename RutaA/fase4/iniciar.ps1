# Android en Hyper-V (Ruta A): arranca o reanuda la VM y abre el Visor (pantalla + toque + teclado) y el audio.
# Volver a Windows: pausar.ps1 (la VM queda en pausa: 0 % de CPU y reanuda al instante).
# Requisitos: VM armada (wsl -u root -e bash RutaA/fase4/preparar.sh) y el usuario en "Administradores de Hyper-V".
$ErrorActionPreference = 'Stop'
$n = 'SurfaceDualAndroid'; $h = Join-Path $PSScriptRoot 'host'
$vm = Get-VM -Name $n -ErrorAction SilentlyContinue
if (-not $vm) { & (Join-Path $PSScriptRoot 'crear_vm.ps1') -Segundos 2 | Out-Null; $vm = Get-VM -Name $n }
switch ($vm.State) {
  'Paused' { Resume-VM -Name $n }
  'Running' { }
  default {
    # Tras un rearme (preparar.sh cambia android.vhdx por uno nuevo) el disco pierde el permiso de la VM y Start-VM da "Acceso
    # denegado" (19/11). Se da el mismo que pone Hyper-V; repetirlo no cambia nada. Y los registros de %TEMP% no crecen sin tope.
    Get-VMHardDiskDrive -VMName $n | ForEach-Object { icacls $_.Path /grant "NT VIRTUAL MACHINE\$($vm.Id.Guid):(R,W)" | Out-Null }
    Get-Item "$env:TEMP\visorvm_*.log", "$env:TEMP\surfacedual_*.log" -ErrorAction SilentlyContinue | Where-Object Length -gt 1MB | Remove-Item -ErrorAction SilentlyContinue
    # 8 GB (17/11: Android usa 2,9 GB en reposo y 3,8 GB con 8 apps + vídeo; a 8 GB, 0 muertes de lmkd; libera 4 GB a
    # Windows). Sin 8 GB libres Hyper-V no arranca: mejor con 6 que no abrir (la próxima vez vuelve a probar 8).
    foreach ($gb in 8, 6) {
      Set-VM -Name $n -MemoryStartupBytes ($gb * 1GB)
      try { Start-VM -Name $n; break } catch { if ($gb -eq 6) { throw } }
    }
    # Si este arranque se cuelga (1 de 7 el 19/11), a los 60 s lo corta y arranca otra vez; aparte, para no retener a la Bandeja.
    Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$(Join-Path $PSScriptRoot 'vigilar_arranque.ps1')`""
  }
}
$id = (Get-VM -Name $n).Id.Guid
# VM corriendo: los ayudantes que esperaban la reanudación (VmActiva, HvPuente.cs) reintentan ya.
$a = $null; if ([Threading.EventWaitHandle]::TryOpenExisting('Local\SurfaceDualVmActiva', [ref]$a)) { [void]$a.Set() }
# Un Visor de otra VM (rearme = VM nueva con otro Id) se queda en "arrancando" para siempre: fuera, y se abre uno nuevo.
Get-CimInstance Win32_Process -Filter "Name='VisorVM.exe'" | Where-Object { $_.CommandLine -notmatch $id } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
$residente = [bool](Get-Process VisorVM -ErrorAction SilentlyContinue)
if (-not $residente) { Start-Process (Join-Path $h 'VisorVM.exe') $id }
# Ayudantes: uno de cada y con el VmId de esta VM; uno de otra VM, fuera (el Visor repite esto cada vez que reanuda la VM;
# va antes de avisarle para que no lancen los dos el mismo). HvAudio: sonido y micrófono. HvCamara: cámaras, abiertas solo
# mientras una app de Android las usa. HvSensores: acelerómetro, giroscopio, brújula, luz, encendidos solo mientras una app
# los usa. HvCarpetas: carpetas de Windows (9P, vsock 5008); pausar.ps1 no lo cierra (los archivos abiertos en Android siguen
# valiendo). HvMdns (sin VmId): Chromecast, el Default Switch (NAT) no deja pasar el mDNS; reenvía solo _googlecast._tcp.
# Puertos vsock (Android escucha): 5000 tubería QEMU (gfxstream), 5001 pantalla, 5002 entrada, 5003/5004 audio, 5006 vídeo (prueba), 5007 cámara, 5008 carpetas 9P, 5009 sensores, 5555 adb; TCP del agente 4445/4446/4449/4451. Uno nuevo: buscar antes con grep.
$vivos = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'Hv%.exe'")
foreach ($a in 'HvAudio', 'HvCamara', 'HvSensores', 'HvCarpetas', 'HvMdns') {
  $bien = $false
  foreach ($p in $vivos | Where-Object Name -eq "$a.exe") {
    if ($a -eq 'HvMdns' -or $null -eq $p.CommandLine -or $p.CommandLine -match $id) { $bien = $true }
    else { Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue }
  }
  $arg = @{}; if ($a -ne 'HvMdns') { $arg.ArgumentList = $id }
  if (-not $bien) { Start-Process (Join-Path $h "$a.exe") @arg -WindowStyle Hidden }
}
if ($residente) { $e = $null; $o = $null; if ([Threading.EventWaitHandle]::TryOpenExisting('Local\SurfaceDualVisorVM.Ocultar', [ref]$o)) { [void]$o.Reset() }; if ([Threading.EventWaitHandle]::TryOpenExisting('Local\SurfaceDualVisorVM.Mostrar', [ref]$e)) { [void]$e.Set() } }   # residente oculto
