# Actualizar Surface Dual ya instalado con los programas de una versión nueva (Actualizar.cmd, o Instalar.cmd cuando ve que
# ya está instalado). Cambia los programas de Windows, los scripts y la pantalla de inicio de Android; NO toca los discos de
# Android (apps y datos), las descargas ni el Linux auxiliar. Android no se apaga (al apagar, algunos equipos dan pantalla azul:
# ESTADO.md 163): si estaba encendido queda en pausa. Revisión: Codex y Gemini (F7/plan/rev_107_*.md).
# shortcut: no rearma la imagen de Android; si una versión cambia la imagen (preparar.sh, agente, kernel), hará falta otro paso.
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
& robocopy.exe $Origen $Destino /E /XF *.vhdx *.avhdx *.vhd launcher_instalado.txt /XD descargas wsl .git /R:3 /W:3 /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw (T "No se pudieron copiar todos los programas a $Destino (código $LASTEXITCODE). Cierre Surface Dual del todo (o reinicie Windows) y repita Actualizar.cmd." "Not all programs could be copied to $Destino (code $LASTEXITCODE). Close Surface Dual completely (or restart Windows) and run Actualizar.cmd again.") }
if (-not $SinAndroid) { Start-Process (Join-Path $f4 'host\VisorVM.exe') '--restaurar' -Wait -ErrorAction SilentlyContinue }   # Windows a su frecuencia y giro de siempre

if ($apkCambia -and -not $SinAndroid) {
  Di (T '  3/4 Actualizar la pantalla de inicio de Android (Android se enciende un momento por detrás)...' '  3/4 Updating the Android home screen (Android starts briefly in the background)...')
  $adb = Join-Path $Destino 'descargas\platform-tools\adb.exe'
  if (-not (Test-Path $adb)) { Di (T '  No está adb de la instalación: la pantalla de inicio se queda como estaba.' '  The adb from the installation is missing: the home screen stays as it was.') Yellow }
  else {
    # Como la fase 6 del instalador: adb se juzga por $LASTEXITCODE (PowerShell 5.1 con 'Stop' tomaba sus avisos como errores)
    # y puertos libres por si Hyper-V reservó 5037/5556.
    $ErrorActionPreference = 'Continue'; $ok = $false; $puente = $null
    function Libre([int]$p) { try { $l = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $p); $l.Start(); $l.Stop(); $true } catch { $false } }
    $env:ANDROID_ADB_SERVER_PORT = "$(5037, 15037, 25037, 35037 | Where-Object { Libre $_ } | Select-Object -First 1)"
    $pp = 5556, 15556, 25556, 35556 | Where-Object { Libre $_ } | Select-Object -First 1; $d = "127.0.0.1:$pp"
    try {
      if (-not $pp -or -not $env:ANDROID_ADB_SERVER_PORT) { throw 'puertos' }
      if ((Get-VM -Name $VM).State -eq 'Paused') { Resume-VM -Name $VM -ErrorAction Stop } elseif ((Get-VM -Name $VM).State -ne 'Running') { Start-VM -Name $VM -ErrorAction Stop }
      $puente = Start-Process (Join-Path $f4 'host\HvPuente.exe') -ArgumentList $hecho.VmId, $pp, 5555 -WindowStyle Hidden -PassThru -ErrorAction Stop
      $listo = $false
      for ($i = 0; $i -lt 60 -and -not $listo; $i++) { & $adb connect $d *> $null; $listo = "$(& $adb -s $d shell getprop sys.boot_completed 2>$null)".Trim() -eq '1'; if (-not $listo) { Start-Sleep 5 } }
      if ($listo) { & $adb -s $d install -r (Join-Path $Destino $apk) *> $null; $ok = -not $LASTEXITCODE }
    } catch { } finally {
      & $adb kill-server *> $null; if ($puente) { Stop-Process -Id $puente.Id -Force -ErrorAction SilentlyContinue }
      if ((Get-VM -Name $VM).State -eq 'Running') { Suspend-VM -Name $VM -ErrorAction SilentlyContinue }   # en pausa hasta que la persona entre
    }
    $ErrorActionPreference = 'Stop'
    if ($ok) { Set-Content $recibo $huella -Encoding ascii }
    else { Di (T '  No se pudo actualizar la pantalla de inicio; lo demás sí. Repita Actualizar.cmd más tarde.' '  The home screen could not be updated; everything else was. Run Actualizar.cmd again later.') Yellow }
  }
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
