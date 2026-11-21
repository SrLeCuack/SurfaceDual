# Actualizar Surface Dual ya instalado con los programas de una versión nueva (Actualizar.cmd, o Instalar.cmd cuando ve que
# ya está instalado). Cambia los programas de Windows, los scripts y la pantalla de inicio de Android; NO toca los discos de
# Android (apps y datos), las descargas ni el Linux auxiliar. Android no se apaga (al apagar, algunos equipos dan pantalla azul:
# ESTADO.md 163): si estaba encendido queda en pausa. Revisión: Codex y Gemini (F7/plan/rev_107_*.md).
# Si la versión cambia lo de dentro de Android (instalador\imagen.txt distinto del recibo imagen_instalada.txt), rearma Android
# con preparar.sh en el Linux auxiliar, SIN fábrica (apps y datos se conservan); para eso Android tiene que apagarse una vez.
#   -Estado <carpeta con hecho.json> y -SinAndroid: solo para probarlo (a mano), sin tocar el Android de verdad.
param([string]$Usuario, [ValidateSet('', 'es', 'en')][string]$Idioma, [string]$Estado, [switch]$SinAndroid)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'comun.ps1')
$Idioma = Elegir-Idioma $Idioma $false
trap { Di "ERROR: $_" Red; Di (T 'No se terminó de actualizar. Sus apps y datos de Android no se tocaron; puede repetir Actualizar.cmd.' 'The update did not finish. Your Android apps and data were not touched; you can run Actualizar.cmd again.') Red; Salir 1 }
Set-Location $env:SystemRoot
$Origen = Split-Path $PSScriptRoot
$yo = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$hecho = Leer-Json (Join-Path $(if ($Estado) { $Estado } else { $DATOS }) 'hecho.json')
if (-not $hecho -or -not $hecho.Launcher) { Di (T 'Surface Dual no está instalado en este equipo (o la instalación no terminó). Use Instalar.cmd.' 'Surface Dual is not installed on this PC (or the installation did not finish). Use Instalar.cmd.') Yellow; Salir 2 }
$Destino = $hecho.Destino
if (-not $Estado -and -not (Ruta-Valida $Destino) -or -not (Test-Path (Join-Path $Destino 'RutaA\fase4\host'))) { Di (T "La instalación anotada ($Destino) no se encuentra. Use Instalar.cmd." "The recorded installation ($Destino) was not found. Use Instalar.cmd.") Red; Salir 2 }
if ((Resolve-Path $Origen).Path.TrimEnd('\') -eq (Resolve-Path $Destino).Path.TrimEnd('\')) { Di (T "Esta es la carpeta ya instalada. Descargue la versión nueva, descomprímala y abra su Actualizar.cmd." "This is the installed folder. Download the new version, extract it and open its Actualizar.cmd.") Yellow; Salir 2 }
$vNueva = (Get-Content (Join-Path $Origen 'instalador\version.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
$vVieja = (Get-Content (Join-Path $Destino 'instalador\version.txt') -ErrorAction SilentlyContinue | Select-Object -First 1); if (-not $vVieja) { $vVieja = T 'anterior' 'previous' }
Titulo (T "Actualizar Surface Dual: de la versión $vVieja a la $vNueva" "Update Surface Dual: from version $vVieja to $vNueva")

if (-not $Estado -and -not (Es-Admin)) {   # igual que el instalador: UAC, diciendo quién es la persona
  Pasos (T 'Windows va a pedir permiso de administrador:' 'Windows is going to ask for administrator permission:') @((T 'Aparecerá una ventana "¿Quieres permitir que esta aplicación haga cambios en el dispositivo?".' 'A window "Do you want to allow this app to make changes to your device?" will appear.'), (T 'Pulse "Sí".' 'Click "Yes".'))
  try { Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Usuario $yo -Idioma $Idioma" }
  catch { Di (T 'No se dio el permiso de administrador: no se actualizó nada.' 'Administrator permission was not given: nothing was updated.') Red; exit 2 }
  exit 0
}
if ($Usuario -and $Usuario -ne $yo) { Di (T 'El permiso lo dio OTRA cuenta de Windows. Inicie sesión con la cuenta que instaló Surface Dual y repita.' 'Permission was given by ANOTHER Windows account. Sign in with the account that installed Surface Dual and try again.') Red; Salir 2 }

$f4 = Join-Path $Destino 'RutaA\fase4'; $apk = 'launcher\build\SurfaceDualLauncher.apk'
# La pantalla de inicio se compara con el "recibo" de la última instalación correcta, no con la copia (si adb fallaba, al
# repetir la copia ya era igual y no se volvía a instalar: Codex). Sin recibo (instalaciones viejas): se instala.
$recibo = Join-Path $Destino 'instalador\launcher_instalado.txt'
$huella = (Get-FileHash (Join-Path $Origen $apk) -Algorithm SHA256).Hash
$apkCambia = $huella -ne (Get-Content $recibo -ErrorAction SilentlyContinue | Select-Object -First 1)
$ayudantes = 'Bandeja', 'VisorVM', 'HvAudio', 'HvCamara', 'HvSensores', 'HvCarpetas', 'HvMdns', 'HvPuente'

Di (T '  1/4 Cerrar los programas de Surface Dual (si Android está encendido, queda en pausa con lo que tenía abierto)...' '  1/4 Closing Surface Dual programs (if Android is on, it stays paused with what it had open)...')
if (-not $SinAndroid) {
  Get-Process $ayudantes -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Get-Process $ayudantes -ErrorAction SilentlyContinue | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue   # todos: si no, sus .exe siguen en uso
  if ((Get-VM -Name $VM).State -eq 'Running') { Suspend-VM -Name $VM }   # pausa, nunca apagado
}

Di (T '  2/4 Copiar los programas nuevos (sin tocar los discos de Android ni las descargas)...' '  2/4 Copying the new programs (without touching Android disks or downloads)...')
& robocopy.exe $Origen $Destino /E /XF *.vhdx *.avhdx *.vhd launcher_instalado.txt imagen_instalada.txt /XD descargas wsl .git /R:3 /W:3 /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw (T "No se pudieron copiar todos los programas a $Destino (código $LASTEXITCODE). Cierre Surface Dual del todo (o reinicie Windows) y repita Actualizar.cmd." "Not all programs could be copied to $Destino (code $LASTEXITCODE). Close Surface Dual completely (or restart Windows) and run Actualizar.cmd again.") }
if (-not $SinAndroid) { Start-Process (Join-Path $f4 'host\VisorVM.exe') '--restaurar' -Wait -ErrorAction SilentlyContinue }   # Windows a su frecuencia y giro de siempre

$adb = Join-Path $Destino 'descargas\platform-tools\adb.exe'; $puente = $null; $d = $null
# Encender o reanudar Android, abrir el puente de adb y esperar a que arranque (5 min como mucho). Lo usan el apagado limpio
# y la pantalla de inicio. Como la fase 6 del instalador: adb se juzga por $LASTEXITCODE (PowerShell 5.1 con 'Stop' tomaba
# sus avisos como errores) y puertos libres por si Hyper-V reservó 5037/5556.
function Conectar {
  $ErrorActionPreference = 'Continue'
  function Libre([int]$p) { try { $l = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $p); $l.Start(); $l.Stop(); $true } catch { $false } }
  $env:ANDROID_ADB_SERVER_PORT = "$(5037, 15037, 25037, 35037 | Where-Object { Libre $_ } | Select-Object -First 1)"
  $pp = 5556, 15556, 25556, 35556 | Where-Object { Libre $_ } | Select-Object -First 1; $script:d = "127.0.0.1:$pp"
  if (-not $pp -or -not $env:ANDROID_ADB_SERVER_PORT -or -not (Test-Path $adb)) { return $false }
  if ((Get-VM -Name $VM).State -eq 'Off' -and (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB -lt 24) { Set-VM -Name $VM -MemoryStartupBytes 6GB }   # como iniciar.ps1
  # Recién rearmado, el Linux auxiliar puede retener memoria unos segundos aunque se cierre (medido 20/11: 'no se pueden asignar
  # 8192 MB' en un equipo de 32 GB): se reintenta 2 min.
  if ((Get-VM -Name $VM).State -eq 'Paused') { Resume-VM -Name $VM }
  for ($k = 0; $k -lt 12 -and (Get-VM -Name $VM).State -ne 'Running'; $k++) { try { Start-VM -Name $VM -ErrorAction Stop } catch { Start-Sleep 10 } }
  if ((Get-VM -Name $VM).State -ne 'Running') { return $false }
  $script:puente = Start-Process (Join-Path $f4 'host\HvPuente.exe') -ArgumentList $hecho.VmId, $pp, 5555 -WindowStyle Hidden -PassThru
  for ($i = 0; $i -lt 60; $i++) { & $adb connect $script:d *> $null; if ("$(& $adb -s $script:d shell getprop sys.boot_completed 2>$null)".Trim() -eq '1') { return $true }; Start-Sleep 5 }
  $false
}
function Soltar { if (Test-Path $adb) { & $adb kill-server *> $null }; if ($script:puente) { Stop-Process -Id $script:puente.Id -Force -ErrorAction SilentlyContinue; $script:puente = $null } }

$imgNueva = Get-Content (Join-Path $Origen 'instalador\imagen.txt') -ErrorAction SilentlyContinue | Select-Object -First 1
$reciboImg = Join-Path $Destino 'instalador\imagen_instalada.txt'; $rearmado = $false
if ($imgNueva -and $imgNueva -ne (Get-Content $reciboImg -ErrorAction SilentlyContinue | Select-Object -First 1) -and -not $SinAndroid) {
  Di (T '  Esta versión cambia cosas dentro de Android: se rearma Android (unos 5-10 minutos). Sus apps y datos se conservan.' '  This version changes things inside Android: Android is rebuilt (about 5-10 minutes). Your apps and data are kept.') Cyan
  # Mientras se rearma, iniciar.ps1 no enciende Android (menú Inicio o teclado): cambiaría sus discos a medias (revisión Codex).
  $cerrojo = Join-Path $f4 'actualizando'; New-Item $cerrojo -ItemType File -Force | Out-Null
  try {
    if ((Get-VM -Name $VM).State -ne 'Off') {
      # Apagado limpio: la imagen vieja no conoce el "apagar ya" del agente y apagar.ps1 caería en el apagado de Hyper-V (pantallas
      # azules en un probador). "svc power shutdown" por adb es el apagado de Android y viene en toda imagen: medido 7 s, 18508 (20/11).
      try { if (Conectar) { & $adb -s $d shell svc power shutdown *> $null; for ($i = 0; $i -lt 60 -and (Get-VM -Name $VM).State -ne 'Off'; $i++) { Start-Sleep 1 } } } finally { Soltar }
      if ((Get-VM -Name $VM).State -ne 'Off') { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $f4 'apagar.ps1') }
    }
    if ((Get-VM -Name $VM).State -ne 'Off') { throw (T 'Android no se apagó: reinicie Windows y repita Actualizar.cmd.' 'Android did not shut down: restart Windows and run Actualizar.cmd again.') }
    # Un disco que Hyper-V no cerró limpio (Windows o la VM caídos) guarda un registro de cambios pendiente y qemu-img se niega a
    # leerlo (probador, 1.0.9: "contains a log that needs to be replayed"). Montarlo y desmontarlo en Windows aplica ese
    # registro con el mismo controlador que usa Hyper-V (en uno limpio solo toca metadatos del contenedor). Sin letra de unidad.
    # Si no se desmonta, se para aquí: WSL no podría leerlo (revisión Codex y Gemini).
    foreach ($r in (Get-VMHardDiskDrive -VMName $VM).Path) {
      if ((Get-VHD -Path $r).Attached) { throw (T "El disco $r está montado en Windows: reinicie Windows y repita Actualizar.cmd." "The disk $r is mounted in Windows: restart Windows and run Actualizar.cmd again.") }
      Mount-VHD -Path $r -NoDriveLetter
      for ($i = 0; $i -lt 5 -and (Get-VHD -Path $r).Attached; $i++) { Dismount-VHD -Path $r -ErrorAction SilentlyContinue; if ((Get-VHD -Path $r).Attached) { Start-Sleep 2 } }
      if ((Get-VHD -Path $r).Attached) { throw (T "No se pudo desmontar ${r}: reinicie Windows y repita Actualizar.cmd. Sus datos no se tocaron." "Could not unmount ${r}: restart Windows and run Actualizar.cmd again. Your data was not touched.") }
    }
    # preparar.sh deja los discos anteriores como *.vhdx.anterior: se borran cuando Android arranca (paso 3/4).
    & wsl.exe -d $DISTRO -u root -e env PRODUCCION=1 VERIFICADO=0 bash "$(Ruta-Wsl $Destino)/RutaA/fase4/preparar.sh" | Out-Null
    $fallo = $LASTEXITCODE; & wsl.exe --terminate $DISTRO | Out-Null   # que suelte su memoria antes de encender Android (como el instalador)
    if ($fallo) { throw (T "No se pudo rearmar Android (código $fallo). Sus datos no se tocaron: repita Actualizar.cmd." "Android could not be rebuilt (code $fallo). Your data was not touched: run Actualizar.cmd again.") }
    foreach ($r in (Get-VMHardDiskDrive -VMName $VM).Path) {   # disco nuevo: sin esto Start-VM da "Acceso denegado" (como iniciar.ps1)
      icacls $r /grant "NT VIRTUAL MACHINE\$((Get-VM -Name $VM).Id.Guid):(R,W)" | Out-Null
      if ($LASTEXITCODE) { throw (T "No se pudo dar permiso a Android sobre $r. Repita Actualizar.cmd." "Could not give Android permission on $r. Run Actualizar.cmd again.") }
    }
  } finally { Remove-Item $cerrojo -ErrorAction SilentlyContinue }
  Set-Content $reciboImg $imgNueva -Encoding ascii; $apkCambia = $true; $rearmado = $true   # tras rearmar, se comprueba que arranca
}

# También si quedó una copia *.anterior sin comprobar (una vez anterior no se pudo encender): se comprueba y se borra (Codex).
if (($apkCambia -or (Get-ChildItem $f4 -Filter '*.vhdx.anterior' -ErrorAction SilentlyContinue)) -and -not $SinAndroid) {
  Di (T '  3/4 Actualizar la pantalla de inicio de Android (Android se enciende un momento por detrás)...' '  3/4 Updating the Android home screen (Android starts briefly in the background)...')
  $ok = $false; $listo = $false
  try { $listo = Conectar; if ($listo) { & $adb -s $d install -r (Join-Path $Destino $apk) *> $null; $ok = -not $LASTEXITCODE } }
  catch { } finally {
    Soltar
    if ((Get-VM -Name $VM).State -eq 'Running') { Suspend-VM -Name $VM -ErrorAction SilentlyContinue }   # en pausa hasta que la persona entre
  }
  if ($ok) { Set-Content $recibo $huella -Encoding ascii }
  else { Di (T '  No se pudo actualizar la pantalla de inicio; lo demás sí. Repita Actualizar.cmd más tarde.' '  The home screen could not be updated; everything else was. Run Actualizar.cmd again later.') Yellow }
  $viejos = Get-ChildItem $f4 -Filter '*.vhdx.anterior' -ErrorAction SilentlyContinue
  if ($listo) { $viejos | Remove-Item -Force -ErrorAction SilentlyContinue }   # Android arrancó: fuera los anteriores (también si quedaron de una vez anterior)
  elseif ($viejos) { Di (T "  Android se rearmó, pero no se pudo comprobar que arranca. Ábralo como siempre: si arranca bien, repita Actualizar.cmd para terminar (pone la pantalla de inicio y borra la copia anterior). Si no arranca, ejecute instalador\Diagnostico.cmd y escríbanos: su disco anterior quedó guardado en $f4 (*.vhdx.anterior)." "  Android was rebuilt, but it could not be checked that it starts. Open it as usual: if it starts fine, run Actualizar.cmd again to finish (it installs the home screen and deletes the previous copy). If it does not start, run instalador\Diagnostico.cmd and contact us: your previous disk was kept in $f4 (*.vhdx.anterior).") Yellow }
} else { Di (T '  3/4 La pantalla de inicio de Android ya está al día.' '  3/4 The Android home screen is already up to date.') }

Di (T '  4/4 Volver a abrir el icono de Surface Dual...' '  4/4 Reopening the Surface Dual icon...')
# Con explorer.exe se abre como la persona, sin administrador (si no, luego no podría cerrar sus propios ayudantes).
if (-not $SinAndroid) { Start-Process explorer.exe "`"$(Join-Path $Destino 'F2\build\emulator\Bandeja.exe')`"" }

$estado = if ($SinAndroid) { 'Off' } else { "$((Get-VM -Name $VM -ErrorAction SilentlyContinue).State)" }
Titulo (T 'Actualizado' 'Updated')
Pasos (T 'Listo:' 'Done:') @(
  (T "Surface Dual está en la versión $vNueva. Sus apps y datos de Android siguen igual." "Surface Dual is now on version $vNueva. Your Android apps and data are unchanged."),
  $(if ($estado -eq 'Paused') { T 'Android quedó en pausa: para entrar, quite el teclado o abra "Android (Surface Dual)" en el menú Inicio.' 'Android is paused: to open it, detach the keyboard or open "Android (Surface Dual)" from the Start menu.' }
    else { T 'Para entrar a Android, quite el teclado o abra "Android (Surface Dual)" en el menú Inicio.' 'To open Android, detach the keyboard or open "Android (Surface Dual)" from the Start menu.' }))
Salir 0
