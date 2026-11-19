# Instalador de Surface Dual (F7/plan/instalador_diseno.md). Lo lanza Instalar.cmd.
#   sin opciones  -> solo comprueba el equipo (no cambia nada) y ofrece instalar
#   -Simular      -> recorre todas las fases diciendo lo que haría, sin cambiar nada
#   -Instalar     -> instala (pide permiso de administrador). Se puede repetir: sigue donde se quedó.
# Nunca reinicia, suspende ni apaga la pantalla por su cuenta, y no emite sonidos.
param([switch]$Instalar, [switch]$Simular, [string]$Destino = 'C:\SurfaceDual', [string]$Usuario, [string]$EquipoFalso)
$ErrorActionPreference = 'Stop'; $sim = $Simular.IsPresent
. (Join-Path $PSScriptRoot 'comun.ps1'); $Simular = $sim
trap { Di "ERROR: $_" Red; Di 'No se siguió. Lo ya hecho queda anotado; puede repetir o desinstalar.' Red; Salir 1 }
Set-Location $env:SystemRoot   # no retener la carpeta de instalación
$Origen = Split-Path $PSScriptRoot
$yo = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$UBUNTU = 'https://cloud-images.ubuntu.com/wsl/releases/24.04/current'; $ROOTFS = 'ubuntu-noble-wsl-arm64-wsl.rootfs.tar.gz'
$ADB_ZIP = 'https://dl.google.com/android/repository/platform-tools-latest-windows.zip'
$LICENCIA = 'https://developer.android.com/studio/terms'

Titulo 'Surface Dual: comprobación del equipo'
# -EquipoFalso <json>: solo para pruebas.ps1 (recorrer la simulación como si fuera otro equipo); nunca con -Instalar.
if ($EquipoFalso -and -not $Simular) { throw '-EquipoFalso solo vale con -Simular' }
$e = if ($EquipoFalso) { $h = @{}; (Get-Content $EquipoFalso -Raw | ConvertFrom-Json).PSObject.Properties | ForEach-Object { $h[$_.Name] = $_.Value }; $h } else { Leer-Equipo $Destino }
$ev = Evaluar-Equipo $e; Mostrar-Evaluacion $ev
if ($ev | Where-Object Estado -eq BLOQUEA) { Di ''; Di 'No se puede instalar en este equipo por lo marcado como IMPIDE. No se ha cambiado nada.' Red; Salir 2 }
if (-not $Instalar -and -not $Simular) {
  Di ''; Di 'El equipo sirve. No se ha cambiado nada.' Green
  if (-not (Preguntar '¿Instalar Surface Dual ahora?' 'SI')) { Salir 0 }
  $Instalar = $true
}
if ($Instalar -and -not $Simular) {
  if (Test-Path (Join-Path $Origen 'instalador\empaquetar.ps1')) { Di 'Esta carpeta es el repositorio de desarrollo, no un paquete: cree el paquete con empaquetar.ps1 e instale desde él.' Red; Salir 2 }
  if (-not (Es-Admin)) {   # relanzar con UAC, diciendo quién es la persona (para detectar que el UAC lo aceptó otra cuenta)
    Pasos 'Windows va a pedir permiso de administrador:' @('Aparecerá una ventana "¿Quieres permitir que esta aplicación haga cambios en el dispositivo?".', 'Pulse "Sí". Se abrirá otra ventana azul que continúa la instalación.')
    try { Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Instalar -Destino `"$Destino`" -Usuario $yo" }
    catch { Di 'No se dio el permiso de administrador: sin él no se puede instalar. No se ha cambiado nada.' Red; exit 2 }
    exit 0
  }
  # shortcut: la cuenta de la persona debe ser administradora (accesos, WSL y grupo son por usuario); con cuenta estándar + otra de admin, hacer la instalación por fases separadas.
  if ($Usuario -and $Usuario -ne $yo) { Di 'El permiso de administrador lo dio OTRA cuenta. Inicie sesión con una cuenta que sea administradora y repita. No se ha cambiado nada.' Red; Salir 2 }
}
$fPrevio = Join-Path $DATOS 'previo.json'; $fHecho = Join-Path $DATOS 'hecho.json'
$hecho = Leer-Json $fHecho; if (-not $hecho) { $hecho = [pscustomobject]@{ Destino = $Destino; Usuario = $yo } }
# Reanudación: mandan la carpeta y la persona de la primera vez (mezclar rutas o cuentas dejaría cosas sin deshacer).
if ($hecho.Destino -ne $Destino) { Di "  Se continúa la instalación ya empezada en $($hecho.Destino)." Yellow; $Destino = $hecho.Destino }
if ($Instalar -and -not $Simular -and $hecho.Usuario -ne $yo) { Di 'Esta instalación la empezó otra cuenta de Windows. Continúela con aquella cuenta o desinstale primero.' Red; Salir 2 }
function Marcar($k, $v) { $hecho | Add-Member -NotePropertyName $k -NotePropertyValue $v -Force; if (-not $Simular) { Guardar-Json $hecho $fHecho } }
function Dism([string]$argumentos) {   # 3010 = bien, pide reiniciar
  $p = Start-Process dism.exe -ArgumentList "/Online /NoRestart /English $argumentos" -Wait -PassThru -WindowStyle Hidden
  if ($p.ExitCode -notin 0, 3010) { throw "DISM $argumentos falló con el código $($p.ExitCode). Detalle: C:\Windows\Logs\DISM\dism.log" }
}
function Bajar([string]$url, [string]$a) {   # curl.exe viene con Windows; -C - reanuda una descarga cortada
  & curl.exe -fL --retry 5 -C - -o $a $url; if ($LASTEXITCODE) { throw "No se pudo descargar $url (¿hay Internet?). Vuelva a ejecutar Instalar: sigue donde se quedó." }
}
$reiniciar = $false

Titulo 'Fase 1 de 6: proteger el equipo (punto de restauración y estado previo)'
Hacer "crear la carpeta de estado $DATOS y anotar cómo está el equipo ANTES de tocar nada" {
  New-Item $DATOS -ItemType Directory -Force | Out-Null
  if (-not (Test-Path $fPrevio)) {   # solo la primera vez: es la referencia del desinstalador
    $car = @{}; foreach ($c in $CARACT) { $car[$c] = "$((Get-WindowsOptionalFeature -Online -FeatureName $c -ErrorAction SilentlyContinue).State)" }
    $inicio = [Environment]::GetFolderPath('Startup'); $menu = [Environment]::GetFolderPath('Programs')
    Guardar-Json ([pscustomobject]@{
        Fecha = (Get-Date).ToString('s'); Caracteristicas = $car
        EnGrupo = [bool](Get-LocalGroupMember -SID $SID_HV -ErrorAction SilentlyContinue | Where-Object { $_.SID.Value -eq $yo })
        QcomExistia = Test-Path 'HKLM:\SOFTWARE\QCOM'; QcomValor = $e.ClaveQcom
        Firewall = [bool](Get-NetFirewallRule -Name $REGLA -ErrorAction SilentlyContinue)
        AccesoInicio = Test-Path (Join-Path $inicio 'Surface Dual.lnk'); AccesoMenu = Test-Path (Join-Path $menu 'Android (Surface Dual).lnk')
        RegistroUsuario = Test-Path 'HKCU:\Software\SurfaceDual'; Distros = @(Distros-Wsl); Wsl = $e.Wsl
        Restauracion = [bool](Get-CimInstance -Namespace root\default -ClassName SystemRestoreConfig -ErrorAction SilentlyContinue | Where-Object RPSessionInterval -eq 1)
      }) $fPrevio
  }
}
if (-not $hecho.PuntoRestauracion) {
  Hacer 'crear un punto de restauración de Windows ("Surface Dual: antes de instalar")' {
    $antes = @(Get-ComputerRestorePoint -ErrorAction SilentlyContinue).Count
    if (-not (Leer-Json $fPrevio).Restauracion) { Enable-ComputerRestore -Drive "$env:SystemDrive\"; Marcar RestauracionActivada $true }
    Checkpoint-Computer -Description 'Surface Dual: antes de instalar' -RestorePointType MODIFY_SETTINGS -WarningAction SilentlyContinue
    $p = @(Get-ComputerRestorePoint -ErrorAction SilentlyContinue)
    # Windows no crea más de un punto cada 24 h: uno de hoy también vale; sin ninguno, no se sigue.
    $ultimo = if ($p) { [Management.ManagementDateTimeConverter]::ToDateTime($p[-1].CreationTime) }
    if ($p.Count -le $antes -and (-not $ultimo -or $ultimo -lt (Get-Date).AddHours(-24))) { throw 'Windows no creó el punto de restauración. Active "Protección del sistema" en la unidad C: y repita.' }
    Marcar PuntoRestauracion "$ultimo"; Di "  Punto de restauración disponible: $ultimo" Green
  }
}

Titulo 'Fase 2 de 6: preparar Windows (Hyper-V, WSL, permiso y ajuste de la gráfica)'
if (-not $e.HyperV) {
  $casa = $e.Edicion -match '^Core'; $ok = $true
  if ($casa) {
    Di '  Su Windows es Home. Microsoft solo ofrece Hyper-V en Pro, Enterprise y Education.' Yellow
    Di '  Se puede activar con los paquetes que Windows Home ya trae dentro (no se descarga nada), pero es un uso NO' Yellow
    Di '  oficial y experimental: una actualización de Windows podría desactivarlo. Se puede deshacer con el desinstalador.' Yellow
    $ok = Preguntar '¿Activar Hyper-V en este Windows Home?' 'SI'
  }
  if (-not $ok) { Di 'Sin Hyper-V no se puede instalar. No se ha cambiado nada más.' Red; Salir 2 }
  if ($casa) { Hacer "añadir los $($e.PaquetesHV) paquetes de Hyper-V que ya están en Windows (DISM /Add-Package)" {
      foreach ($m in Get-ChildItem "$env:SystemRoot\servicing\Packages\Microsoft-Hyper-V*.mum") { Dism "/Add-Package:`"$($m.FullName)`"" }; Marcar PaquetesHome $true } }
  Hacer 'activar la característica Hyper-V (DISM /Enable-Feature Microsoft-Hyper-V /All)' { Dism '/Enable-Feature /FeatureName:Microsoft-Hyper-V /All'; Marcar HyperVActivado $true }
  $reiniciar = $true
}
if (-not $e.Wsl) { Hacer 'instalar el Subsistema de Windows para Linux (wsl --install --no-distribution)' {
    & wsl.exe --install --no-distribution; if ($LASTEXITCODE) { throw 'No se pudo instalar WSL. Compruebe Internet y repita.' }; Marcar WslInstalado $true }; $reiniciar = $true }
if ($e.ClaveQcom -ne 0) { Hacer "poner $QVALOR=0 en HKLM\SOFTWARE\QCOM\Drivers\DxKMD (vídeo por GPU en Android)" {
    if (-not (Test-Path $QCOM)) { New-Item $QCOM -Force | Out-Null }   # sin -Force si ya existe: New-Item -Force vaciaría la clave
    New-ItemProperty $QCOM -Name $QVALOR -PropertyType DWord -Value 0 -Force | Out-Null; Marcar QcomPuesta $true }; $reiniciar = $true }
if (-not $reiniciar -and -not $e.EnGrupo -and -not $hecho.GrupoPuesto) {   # el grupo no existe hasta reiniciar con Hyper-V activado
  Hacer 'añadir su usuario al grupo de administradores de Hyper-V' {
    if (-not (Get-LocalGroupMember -SID $SID_HV | Where-Object { $_.SID.Value -eq $yo })) { Add-LocalGroupMember -SID $SID_HV -Member $yo; Marcar GrupoAñadido $true }
    Marcar GrupoPuesto $true }
}
if ($reiniciar) {
  Pasos 'Ahora hace falta REINICIAR Windows (el instalador no lo hace solo):' @(
    'Guarde y cierre lo que tenga abierto.',
    'Abra el menú Inicio, pulse el botón de encendido y elija "Reiniciar".',
    'Cuando Windows vuelva, haga doble clic otra vez en "Instalar.cmd". Verá que continúa en la fase 2 o 3.')
  if (-not $Simular) { Salir 0 }
}

Titulo 'Fase 3 de 6: obtener y armar Android (lo más largo: cerca de una hora, con Internet)'
if (-not $Simular -and -not (@(Get-VMHostPartitionableGpu | ForEach-Object Name) -match 'VEN_QCOM')) {
  Di 'Hyper-V no ofrece la GPU Adreno para compartirla (GPU-P). Actualice el controlador gráfico con Windows Update, reinicie y repita.' Red; Salir 2 }
if ($Origen -ne $Destino -and -not $hecho.Copiado) { Hacer "copiar el paquete a $Destino" {
    # Carpeta exclusiva: el desinstalador la borra entera, así que no puede tener nada de antes.
    if ((Test-Path $Destino) -and (Get-ChildItem $Destino -Force | Select-Object -First 1)) { throw "La carpeta $Destino ya existe y no está vacía. Vacíela o elija otra con: Instalar.cmd -Destino C:\OtraCarpeta" }
    & robocopy.exe $Origen $Destino /E /NFL /NDL /NJH /NJS /NP | Out-Null; if ($LASTEXITCODE -ge 8) { throw "No se pudo copiar a $Destino" }; Marcar Copiado $true } }
$f4 = Join-Path $Destino 'RutaA\fase4'
if (-not (Test-Path (Join-Path $f4 'android.vhdx')) -or $Simular) {
  Di "  Android con Google Play se descarga de Google (unos 2,2 GB). Sus condiciones: $LICENCIA"
  if (-not (Preguntar '¿Acepta las condiciones de licencia del SDK de Android de Google?' 'ACEPTO')) { Di 'Sin aceptar no se puede descargar Android. Puede repetir Instalar.cmd cuando quiera.' Red; Salir 2 }
  if ((Distros-Wsl) -contains $DISTRO -and -not $hecho.Distro -and -not $Simular) { throw "Ya existe un Linux llamado '$DISTRO' que este instalador no creó. No se toca: quítelo (wsl --unregister $DISTRO) o cámbiele el nombre." }
  if ((Distros-Wsl) -notcontains $DISTRO) {
    $bajadas = Join-Path $Destino 'descargas'; $tar = Join-Path $bajadas $ROOTFS
    Hacer "descargar Ubuntu 24.04 ARM64 oficial ($UBUNTU) y comprobar su SHA-256" {
      New-Item $bajadas -ItemType Directory -Force | Out-Null; Bajar "$UBUNTU/$ROOTFS" $tar; Bajar "$UBUNTU/SHA256SUMS" "$tar.sumas"
      $suma = (Get-FileHash $tar -Algorithm SHA256).Hash
      if (-not (Select-String -Path "$tar.sumas" -Pattern "^$suma\s+\*?$([regex]::Escape($ROOTFS))" -Quiet)) { Remove-Item $tar; throw 'La descarga de Ubuntu llegó dañada. Repita Instalar.cmd.' }
    }
    Hacer "crear el Linux auxiliar '$DISTRO' (aparte de cualquier Linux suyo; se quita al desinstalar)" {
      & wsl.exe --import $DISTRO (Join-Path $Destino 'wsl') $tar --version 2; if ($LASTEXITCODE) { throw 'No se pudo crear el Linux auxiliar (wsl --import).' }; Marcar Distro $true }
  }
  Hacer 'armar Android dentro del Linux auxiliar (instalador/armar.sh: descarga la imagen, la adapta y toma el número de serie de ESTE equipo)' {
    $r = Ruta-Wsl $Destino
    & wsl.exe -d $DISTRO -u root -e env SD_LICENCIA_GOOGLE=ACEPTO bash "$r/instalador/armar.sh" $r
    if ($LASTEXITCODE) { throw 'El armado de Android falló (el motivo está en las líneas de arriba). Repita Instalar.cmd: sigue donde se quedó.' }
    & wsl.exe --terminate $DISTRO | Out-Null
  }
}

Titulo 'Fase 4 de 6: crear la máquina virtual de Android'
if (-not $hecho.VmId) {
  Hacer "crear la máquina virtual '$VM' (crear_vm.ps1: 8 GB, GPU compartida)" {
    # crear_vm.ps1 borra cualquier VM con ese nombre: se vuelve a mirar justo antes.
    if (Get-VM | Where-Object { $_.Name -eq $VM -and $_.Id.Guid -ne $hecho.VmCreada }) { throw "Ya existe una máquina virtual '$VM' que este instalador no creó. No se toca." }
    # Si crear_vm.ps1 falla a medias, la VM ya existe: se anota igual para poder repetir o desinstalar.
    try { & (Join-Path $f4 'crear_vm.ps1') -Segundos 2 | Out-Null } finally { $n = Get-VM | Where-Object Name -eq $VM; if ($n) { Marcar VmCreada $n.Id.Guid } }
    Marcar VmId $hecho.VmCreada
  }
}

Titulo 'Fase 5 de 6: programas de Windows y accesos'
Hacer 'crear los accesos "Surface Dual" (arranca con Windows) y "Android (Surface Dual)" (menú Inicio)' {
  $b = Join-Path $Destino 'F2\build\emulator\Bandeja.exe'; $w = New-Object -ComObject WScript.Shell
  $previo = Leer-Json $fPrevio   # lo que ya existía antes de instalar no se pisa (el desinstalador no sabría devolverlo)
  if (-not $previo.AccesoInicio) { $a = $w.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Startup')) 'Surface Dual.lnk')); $a.TargetPath = $b; $a.Save() }
  if (-not $previo.AccesoMenu) { $a = $w.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Programs')) 'Android (Surface Dual).lnk')); $a.TargetPath = $b; $a.Arguments = '/entrar'; $a.Save() }
  Marcar Accesos $true
}
Hacer 'permitir en el cortafuegos que Android encuentre televisores para enviar contenido (solo HvMdns.exe, UDP 5353, red local)' {
  if ((Leer-Json $fPrevio).Firewall) { Di '  Ya había una regla con ese nombre de antes: se deja como estaba.' Yellow; return }
  Remove-NetFirewallRule -Name $REGLA -ErrorAction SilentlyContinue
  New-NetFirewallRule -Name $REGLA -DisplayName 'SurfaceDual HvMdns (Cast en Android)' -Direction Inbound -Action Allow -Protocol UDP -LocalPort 5353 `
    -Program (Join-Path $f4 'host\HvMdns.exe') -RemoteAddress LocalSubnet -Profile Any | Out-Null
  Marcar Firewall $true
}

Titulo 'Fase 6 de 6: primer arranque de Android (pantalla de inicio de Surface Dual)'
if (-not $hecho.Launcher) {
  try {
    Hacer 'descargar adb de Google, esperar a que Android arranque e instalar la pantalla de inicio y el botón "Cambiar a Windows"' {
      $pt = Join-Path $Destino 'descargas'; $adb = Join-Path $pt 'platform-tools\adb.exe'; $d = '127.0.0.1:5556'
      # shortcut: adb.exe de Google es x86 (emulado) y "latest" no tiene hash fijo (HTTPS de dl.google.com); solo se usa aquí, una vez. Sobra cuando el launcher vaya en la imagen.
      if (-not (Test-Path $adb)) { New-Item $pt -ItemType Directory -Force | Out-Null; Bajar $ADB_ZIP "$pt\adb.zip"; Expand-Archive "$pt\adb.zip" $pt -Force }
      if ((Get-VM -Name $VM).State -ne 'Running') { Start-VM -Name $VM }
      $puente = Start-Process (Join-Path $f4 'host\HvPuente.exe') -ArgumentList $hecho.VmId, 5556, 5555 -WindowStyle Hidden -PassThru
      try {
        $listo = $false
        for ($i = 0; $i -lt 60 -and -not $listo; $i++) {   # hasta 5 min
          & $adb connect $d *> $null; $listo = "$(& $adb -s $d shell getprop sys.boot_completed 2>$null)".Trim() -eq '1'
          if (-not $listo) { Start-Sleep 5 }
        }
        if (-not $listo) { throw 'Android no terminó de arrancar en 5 minutos.' }
        & $adb -s $d install -r (Join-Path $Destino 'launcher\build\SurfaceDualLauncher.apk'); if ($LASTEXITCODE) { throw 'No se pudo instalar la pantalla de inicio.' }
        & $adb -s $d shell cmd package set-home-activity com.surfacedual.launcher/.Inicio; if ($LASTEXITCODE) { throw 'No se pudo poner la pantalla de inicio.' }
        & $adb -s $d shell cmd statusbar add-tile com.surfacedual.launcher/.AWindows; if ($LASTEXITCODE) { throw 'No se pudo añadir el botón Cambiar a Windows.' }
        & $adb -s $d reboot   # el agente de la imagen concede los permisos del launcher al arrancar con él ya instalado
        Marcar Launcher $true
      } finally { & $adb kill-server *> $null; Stop-Process -Id $puente.Id -Force -ErrorAction SilentlyContinue }
    }
  } catch { Di "  No se completó el primer arranque: $_" Yellow; Di '  Lo demás está instalado. Repita Instalar.cmd para reintentar solo este paso.' Yellow }
}

Titulo 'Instalación terminada'
if ($Simular) { Di "Simulación: no se ha cambiado nada. Órdenes que se habrían ejecutado: $($Cambios.Count)." Green; Salir 0 }
if (-not $hecho.Launcher) { Di 'Falta el primer arranque de Android (fase 6). Repita Instalar.cmd para completarlo.' Yellow; Salir 3 }
Pasos 'Lo que queda lo hace usted:' @(
  'Reinicie Windows una vez (menú Inicio, botón de encendido, "Reiniciar"): así su usuario estrena el permiso de Hyper-V.',
  'Al volver verá el icono de Surface Dual junto al reloj. Para entrar en Android: quite el teclado, o abra "Android (Surface Dual)" en el menú Inicio.',
  'Dentro de Android, abra "Play Store" e inicie sesión con su cuenta de Google.',
  'Para quitarlo todo y dejar el equipo como estaba: doble clic en "Desinstalar.cmd".')
Salir 0
