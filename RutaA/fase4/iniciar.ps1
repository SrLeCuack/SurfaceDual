# Android en Hyper-V (Ruta A): arranca o reanuda la VM y abre el Visor (pantalla + toque + teclado) y el audio.
# Volver a Windows: pausar.ps1 (la VM queda en pausa: 0 % de CPU y reanuda al instante).
# Requisitos: VM armada (wsl -u root -e bash RutaA/fase4/preparar.sh) y el usuario en "Administradores de Hyper-V".
$ErrorActionPreference = 'Stop'
$n = 'SurfaceDualAndroid'; $h = Join-Path $PSScriptRoot 'host'
# El actualizador está rearmando Android (cambia sus discos): no se enciende. Un cerrojo de más de 1 h es de un corte: no vale.
$c = Get-Item (Join-Path $PSScriptRoot 'actualizando') -ErrorAction SilentlyContinue
if ($c -and $c.LastWriteTime -gt (Get-Date).AddHours(-1)) {
  $en = (Get-ItemProperty 'HKCU:\Software\SurfaceDual' -ErrorAction SilentlyContinue).Idioma -eq 'en'
  [void](New-Object -ComObject WScript.Shell).Popup($(if ($en) { 'Surface Dual is updating Android. Wait until the update window says it is done.' } else { 'Surface Dual está actualizando Android. Espere a que la ventana de actualización diga que terminó.' }), 10, 'Surface Dual', 64)
  Get-Process VisorVM -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Process (Join-Path $h 'VisorVM.exe') '--restaurar' -Wait; exit 1
}
# Menos de 24 GB: antes de encender Android se congelan los programas de Windows y se vacía su memoria (pantallas azules de
# dxgmms2 con la RAM y la memoria de vídeo llenas, ESTADO.md 175). Al volver a Windows: pausar.ps1 o el Visor los descongelan.
if ((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB -lt 24) { try { & (Join-Path $PSScriptRoot 'congelar.ps1') -Accion Congelar | Out-Null } catch { } }
trap { try { & (Join-Path $PSScriptRoot 'congelar.ps1') -Accion Descongelar } catch { }; break }   # Android no arrancó: Windows, tal como estaba
$vm = Get-VM -Name $n -ErrorAction SilentlyContinue
if (-not $vm) { & (Join-Path $PSScriptRoot 'crear_vm.ps1') -Segundos 2 | Out-Null; $vm = Get-VM -Name $n }
switch ($vm.State) {
  'Paused' { Resume-VM -Name $n }
  'Running' { if ($vm.Uptime.TotalSeconds -lt 60) { $frio = (Get-Date).AddSeconds(-$vm.Uptime.TotalSeconds - 1) } }   # otra llamada mientras arranca: espera igual
  default {
    # Tras un rearme (preparar.sh cambia android.vhdx por uno nuevo) el disco pierde el permiso de la VM y Start-VM da "Acceso
    # denegado" (19/11). Se da el mismo que pone Hyper-V; repetirlo no cambia nada. Y los registros de %TEMP% no crecen sin tope.
    Get-VMHardDiskDrive -VMName $n | ForEach-Object { icacls $_.Path /grant "NT VIRTUAL MACHINE\$($vm.Id.Guid):(R,W)" | Out-Null }
    Get-Item "$env:TEMP\visorvm_*.log", "$env:TEMP\surfacedual_*.log" -ErrorAction SilentlyContinue | Where-Object Length -gt 1MB | Remove-Item -ErrorAction SilentlyContinue
    # Memoria según la del equipo (20/11). Android usa 2,9 GB en reposo y 3,8 GB con 8 apps + vídeo (17/11). Con 32 GB, 8 (o 6
    # si no caben). Con 16 GB, 6: con 8, a Windows le quedaban ~8 GB y con el navegador usaba el disco como memoria (congelamientos
    # de un probador). Sin memoria libre Hyper-V no arranca (OutOfMemory, medido con 4 GB libres): se avisa, se "duerme" la memoria
    # de los programas de Windows que no se usan (pasa a disco; siguen abiertos) y se reintenta; si tampoco, aviso claro.
    $en = (Get-ItemProperty 'HKCU:\Software\SurfaceDual' -ErrorAction SilentlyContinue).Idioma -eq 'en'
    function Aviso([string]$es, [string]$ing, [int]$seg) { [void](New-Object -ComObject WScript.Shell).Popup($(if ($en) { $ing } else { $es }), $seg, 'Surface Dual', 64) }
    $tam = if ((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB -ge 24) { 8, 6 } else { , 6 }
    function Falta { try { (Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory).AvailableMBytes / 1024 -lt $tam[-1] + 0.5 } catch { $true } }
    $frio = Get-Date; $arrancada = $false; $fallo = $null; $memoria = $false
    foreach ($intento in 1, 2) {
      foreach ($gb in $tam) {
        Set-VM -Name $n -MemoryStartupBytes ($gb * 1GB)
        try { Start-VM -Name $n; $arrancada = $true; break } catch { $fallo = $_ }
      }
      $memoria = -not $arrancada -and (Falta)   # otro fallo (no de memoria): ni liberar ni culpar a la memoria (revisión Codex)
      if (-not $memoria -or $intento -eq 2) { break }
      Aviso "Android necesita $($tam[-1]) GB de memoria libre. Liberando memoria de Windows: sus programas siguen abiertos, pero pueden tardar un momento al volver a usarlos." "Android needs $($tam[-1]) GB of free memory. Freeing Windows memory: your programs stay open, but they may take a moment when you use them again." 5
      if (-not ('SD.Mem' -as [type])) { Add-Type -Namespace SD -Name Mem -MemberDefinition '[DllImport("psapi.dll")] public static extern bool EmptyWorkingSet(IntPtr h);' }
      $ses = (Get-Process -Id $PID).SessionId
      foreach ($p in Get-Process | Where-Object { $_.SessionId -eq $ses -and $_.Id -ne $PID -and $_.ProcessName -notin 'VisorVM', 'dwm', 'csrss', 'winlogon' }) { try { [void][SD.Mem]::EmptyWorkingSet($p.Handle) } catch { } }
      for ($i = 0; $i -lt 30 -and (Falta); $i++) { Start-Sleep 1 }
    }
    if (-not $arrancada) {
      Get-Process VisorVM -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Process (Join-Path $h 'VisorVM.exe') '--restaurar' -Wait
      if ($memoria) { Aviso "No hay memoria suficiente para Android ($($tam[-1]) GB). Cierre algunos programas (por ejemplo, pestañas del navegador) u otras máquinas virtuales (Docker, Windows Sandbox, el modo Cowork de Claude) y vuelva a intentarlo." "There is not enough memory for Android ($($tam[-1]) GB). Close some programs (for example, browser tabs) or other virtual machines (Docker, Windows Sandbox, Claude's Cowork mode) and try again." 0 }
      throw $fallo
    }
    # Si este arranque se cuelga en GRUB (ver vigilar_arranque.ps1), lo corta y arranca otra vez; aparte, para no retener a la Bandeja.
    Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$(Join-Path $PSScriptRoot 'vigilar_arranque.ps1')`"", '-Desde', $frio.ToString('o')
  }
}
$id = (Get-VM -Name $n).Id.Guid
# VM corriendo: los ayudantes que esperaban la reanudación (VmActiva, HvPuente.cs) reintentan ya.
$a = $null; if ([Threading.EventWaitHandle]::TryOpenExisting('Local\SurfaceDualVmActiva', [ref]$a)) { [void]$a.Set() }
# Un Visor de otra VM (rearme = VM nueva con otro Id) se queda en "arrancando" para siempre: fuera, y se abre uno nuevo.
# Solo el que lleva OTRO VmId: el que abre la Bandeja va sin argumentos (busca la VM él solo) y matarlo dejaba a Android
# abriéndose y cerrándose (19/11: el nuevo chocaba con el mutex del que aún moría).
Get-CimInstance Win32_Process -Filter "Name='VisorVM.exe'" | Where-Object { $_.CommandLine -match '[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}' -and $_.CommandLine -notmatch $id } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force; Wait-Process -Id $_.ProcessId -Timeout 5 -ErrorAction SilentlyContinue }   # que suelte el mutex antes de abrir otro (Codex)
$residente = [bool](Get-Process VisorVM -ErrorAction SilentlyContinue)
if (-not $residente) { Start-Process (Join-Path $h 'VisorVM.exe') $id }
# Ayudantes: uno de cada y con el VmId de esta VM; uno de otra VM, fuera (el Visor repite esto cada vez que reanuda la VM;
# va antes de avisarle para que no lancen los dos el mismo). HvAudio: sonido y micrófono. HvCamara: cámaras, abiertas solo
# mientras una app de Android las usa. HvSensores: acelerómetro, giroscopio, brújula, luz, encendidos solo mientras una app
# los usa. HvCarpetas: carpetas de Windows (9P, vsock 5008); pausar.ps1 no lo cierra (los archivos abiertos en Android siguen
# valiendo). HvMdns (sin VmId): Chromecast, el Default Switch (NAT) no deja pasar el mDNS; reenvía solo _googlecast._tcp.
# Puertos vsock (Android escucha): 5000 tubería QEMU (gfxstream), 5001 pantalla, 5002 entrada, 5003/5004 audio, 5006 vídeo (prueba), 5007 cámara, 5008 carpetas 9P, 5009 sensores, 5555 adb; TCP del agente 4445/4446/4449/4451. Uno nuevo: buscar antes con grep.
# Arranque en frío: los ayudantes, después de que GRUB entregue el control al kernel (evento 18601 de Hyper-V, 0-1 s).
# Lanzados mientras el firmware lee el kernel, 1 de cada 10-15 arranques se colgaba ahí; con ellos ya abiertos o sin ellos,
# 1 de 400 (medido 19/11, ESTADO.md 149). El Visor no espera: abierto durante GRUB, 0 de 30. Si se cuelga, el vigilante corta
# y rearranca (hasta 3 veces, ~47 s medidos) y aquí se sigue esperando el 18601 del rearranque, 60 s como mucho. Sin 18601 al
# final: si el registro se lee es que no arrancó y no se abren; si leerlo da error, se abren como siempre.
if ($frio) {
  $ev = @{ LogName = 'Microsoft-Windows-Hyper-V-Worker-Admin'; StartTime = $frio }; $tope = (Get-Date).AddSeconds(60); $so = $false
  while (-not $so -and (Get-Date) -lt $tope) {
    $so = [bool](Get-WinEvent -FilterHashtable ($ev + @{ Id = 18601 }) -ErrorAction SilentlyContinue | Where-Object { $_.Message -match $id })
    if (-not $so) { Start-Sleep -Milliseconds 300 }
  }
  if (-not $so) {
    if ((Get-VM -Name $n).State -ne 'Running') { return }
    try { [void](Get-WinEvent -FilterHashtable ($ev + @{ Id = 18500 }) -ErrorAction Stop); return }   # se lee: no arrancó
    catch { if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { return } }   # se lee y está vacío: tampoco
  }
}
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
