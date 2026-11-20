# Instalador de Surface Dual (F7/plan/instalador_diseno.md). Lo lanza Instalar.cmd.
#   sin opciones  -> solo comprueba el equipo (no cambia nada) y ofrece instalar
#   -Simular      -> recorre todas las fases diciendo lo que haría, sin cambiar nada
#   -Instalar     -> instala (pide permiso de administrador). Se puede repetir: sigue donde se quedó.
#   -Idioma es|en -> idioma del instalador, de los programas y de Android; sin él se pregunta al empezar (por omisión, el de Windows)
# Nunca reinicia, suspende ni apaga la pantalla por su cuenta, y no emite sonidos.
param([switch]$Instalar, [switch]$Simular, [string]$Destino = 'C:\SurfaceDual', [string]$Usuario, [string]$EquipoFalso, [ValidateSet('', 'es', 'en')][string]$Idioma)
$ErrorActionPreference = 'Stop'; $sim = $Simular.IsPresent
. (Join-Path $PSScriptRoot 'comun.ps1'); $Simular = $sim
$Idioma = Elegir-Idioma $Idioma (-not $Simular)   # lo primero: todo lo demás ya sale en ese idioma
trap { Di "ERROR: $_" Red; Di (T 'No se siguió. Lo ya hecho queda anotado; puede repetir o desinstalar.' 'Stopped. What was already done is recorded; you can run it again or uninstall.') Red; Salir 1 }
Set-Location $env:SystemRoot   # no retener la carpeta de instalación
$Origen = Split-Path $PSScriptRoot
$yo = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$UBUNTU = 'https://cloud-images.ubuntu.com/wsl/releases/24.04/current'; $ROOTFS = 'ubuntu-noble-wsl-arm64-wsl.rootfs.tar.gz'
$ADB_ZIP = 'https://dl.google.com/android/repository/platform-tools-latest-windows.zip'
$LICENCIA = 'https://developer.android.com/studio/terms'

Titulo (T 'Surface Dual: comprobación del equipo' 'Surface Dual: checking this PC')
# -EquipoFalso <json>: solo para pruebas.ps1 (recorrer la simulación como si fuera otro equipo); nunca con -Instalar.
if ($EquipoFalso -and -not $Simular) { throw (T '-EquipoFalso solo vale con -Simular' '-EquipoFalso only works with -Simular') }
$e = if ($EquipoFalso) { $h = @{}; (Get-Content $EquipoFalso -Raw | ConvertFrom-Json).PSObject.Properties | ForEach-Object { $h[$_.Name] = $_.Value }; $h } else { Leer-Equipo $Destino }
$ev = Evaluar-Equipo $e; Mostrar-Evaluacion $ev
if ($ev | Where-Object Estado -eq BLOQUEA) { Di ''; Di (T 'No se puede instalar en este equipo por lo marcado como IMPIDE. No se ha cambiado nada.' 'It cannot be installed on this PC because of the items marked BLOCKS. Nothing has been changed.') Red; Salir 2 }
if (-not $Instalar -and -not $Simular) {
  Di ''; Di (T 'El equipo sirve. No se ha cambiado nada.' 'This PC is suitable. Nothing has been changed.') Green
  if (-not (Preguntar (T '¿Instalar Surface Dual ahora?' 'Install Surface Dual now?') (T 'SI' 'YES'))) { Salir 0 }
  $Instalar = $true
}
if ($Instalar -and -not $Simular) {
  if (Test-Path (Join-Path $Origen 'instalador\empaquetar.ps1')) { Di (T 'Esta carpeta es el repositorio de desarrollo, no un paquete: cree el paquete con empaquetar.ps1 e instale desde él.' 'This folder is the development repository, not a package: create the package with empaquetar.ps1 and install from it.') Red; Salir 2 }
  if (-not (Es-Admin)) {   # relanzar con UAC, diciendo quién es la persona (para detectar que el UAC lo aceptó otra cuenta)
    Pasos (T 'Windows va a pedir permiso de administrador:' 'Windows is going to ask for administrator permission:') @((T 'Aparecerá una ventana "¿Quieres permitir que esta aplicación haga cambios en el dispositivo?".' 'A window will appear: "Do you want to allow this app to make changes to your device?".'), (T 'Pulse "Sí". Se abrirá otra ventana azul que continúa la instalación.' 'Select "Yes". Another blue window will open and continue the installation.'))
    try { Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Instalar -Destino `"$Destino`" -Usuario $yo -Idioma $Idioma" }
    catch { Di (T 'No se dio el permiso de administrador: sin él no se puede instalar. No se ha cambiado nada.' 'Administrator permission was not given: without it, it cannot be installed. Nothing has been changed.') Red; exit 2 }
    exit 0
  }
  # shortcut: la cuenta de la persona debe ser administradora (accesos, WSL y grupo son por usuario); con cuenta estándar + otra de admin, hacer la instalación por fases separadas.
  if ($Usuario -and $Usuario -ne $yo) { Di (T 'El permiso de administrador lo dio OTRA cuenta. Inicie sesión con una cuenta que sea administradora y repita. No se ha cambiado nada.' 'Administrator permission was given by ANOTHER account. Sign in with an account that is an administrator and run it again. Nothing has been changed.') Red; Salir 2 }
}
$fPrevio = Join-Path $DATOS 'previo.json'; $fHecho = Join-Path $DATOS 'hecho.json'
$hecho = Leer-Json $fHecho; if (-not $hecho) { $hecho = [pscustomobject]@{ Destino = $Destino; Usuario = $yo } }
# Reanudación: mandan la carpeta y la persona de la primera vez (mezclar rutas o cuentas dejaría cosas sin deshacer).
if ($hecho.Destino -ne $Destino) { Di (T "  Se continúa la instalación ya empezada en $($hecho.Destino)." "  Continuing the installation already started in $($hecho.Destino).") Yellow; $Destino = $hecho.Destino }
if ($Instalar -and -not $Simular -and $hecho.Usuario -ne $yo) { Di (T 'Esta instalación la empezó otra cuenta de Windows. Continúela con aquella cuenta o desinstale primero.' 'This installation was started by another Windows account. Continue it with that account or uninstall first.') Red; Salir 2 }
function Marcar($k, $v) { $hecho | Add-Member -NotePropertyName $k -NotePropertyValue $v -Force; if (-not $Simular) { Guardar-Json $hecho $fHecho } }
function Dism([string]$argumentos) {   # 3010 = bien, pide reiniciar
  $p = Start-Process dism.exe -ArgumentList "/Online /NoRestart /English $argumentos" -Wait -PassThru -WindowStyle Hidden
  if ($p.ExitCode -notin 0, 3010) { throw (T "DISM $argumentos falló con el código $($p.ExitCode). Detalle: C:\Windows\Logs\DISM\dism.log" "DISM $argumentos failed with code $($p.ExitCode). Details: C:\Windows\Logs\DISM\dism.log") }
}
function Bajar([string]$url, [string]$a) {   # curl.exe viene con Windows; -C - reanuda una descarga cortada
  & curl.exe -fL --retry 5 -C - -o $a $url; if ($LASTEXITCODE) { throw (T "No se pudo descargar $url (¿hay Internet?). Vuelva a ejecutar Instalar: sigue donde se quedó." "Could not download $url (is there an Internet connection?). Run Instalar again: it continues where it stopped.") }
}
$reiniciar = $false

Titulo (T 'Fase 1 de 6: proteger el equipo (punto de restauración y estado previo)' 'Phase 1 of 6: protect this PC (restore point and previous state)')
Hacer (T "crear la carpeta de estado $DATOS y anotar cómo está el equipo ANTES de tocar nada" "create the state folder $DATOS and record how this PC is BEFORE changing anything") {
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
        RegistroUsuario = Test-Path 'HKCU:\Software\SurfaceDual'; Idioma = Idioma-Guardado; Distros = @(Distros-Wsl); Wsl = $e.Wsl
        Restauracion = [bool](Get-CimInstance -Namespace root\default -ClassName SystemRestoreConfig -ErrorAction SilentlyContinue | Where-Object RPSessionInterval -eq 1)
      }) $fPrevio
  }
}
if (-not $hecho.PuntoRestauracion) {
  Hacer (T 'crear un punto de restauración de Windows ("Surface Dual: antes de instalar")' 'create a Windows restore point ("Surface Dual: before installing")') {
    $antes = @(Get-ComputerRestorePoint -ErrorAction SilentlyContinue).Count
    if (-not (Leer-Json $fPrevio).Restauracion) { Enable-ComputerRestore -Drive "$env:SystemDrive\"; Marcar RestauracionActivada $true }
    Checkpoint-Computer -Description (T 'Surface Dual: antes de instalar' 'Surface Dual: before installing') -RestorePointType MODIFY_SETTINGS -WarningAction SilentlyContinue
    $p = @(Get-ComputerRestorePoint -ErrorAction SilentlyContinue)
    # Windows no crea más de un punto cada 24 h: uno de hoy también vale; sin ninguno, no se sigue.
    $ultimo = if ($p) { [Management.ManagementDateTimeConverter]::ToDateTime($p[-1].CreationTime) }
    if ($p.Count -le $antes -and (-not $ultimo -or $ultimo -lt (Get-Date).AddHours(-24))) { throw (T 'Windows no creó el punto de restauración. Active "Protección del sistema" en la unidad C: y repita.' 'Windows did not create the restore point. Turn on "System Protection" for drive C: and run it again.') }
    Marcar PuntoRestauracion "$ultimo"; Di (T "  Punto de restauración disponible: $ultimo" "  Restore point available: $ultimo") Green
  }
}
Hacer (T "guardar el idioma elegido ($Idioma) para los programas de Surface Dual y para Android" "save the chosen language ($Idioma) for the Surface Dual programs and for Android") {
  if (-not (Test-Path $CLAVE)) { New-Item $CLAVE | Out-Null }; Set-ItemProperty $CLAVE -Name Idioma -Value $Idioma; Marcar Idioma $Idioma
}

Titulo (T 'Fase 2 de 6: preparar Windows (Hyper-V, WSL, permiso y ajuste de la gráfica)' 'Phase 2 of 6: prepare Windows (Hyper-V, WSL, permission and graphics setting)')
if (-not $e.HyperV) {
  $casa = $e.Edicion -match '^Core'; $ok = $true
  if ($casa) {
    Di (T '  Su Windows es Home. Microsoft solo ofrece Hyper-V en Pro, Enterprise y Education.' '  Your Windows is Home. Microsoft only offers Hyper-V on Pro, Enterprise and Education.') Yellow
    Di (T '  Se puede activar con los paquetes que Windows Home ya trae dentro (no se descarga nada), pero es un uso NO' '  It can be enabled with the packages Windows Home already includes (nothing is downloaded), but this use is NOT') Yellow
    Di (T '  oficial y experimental: una actualización de Windows podría desactivarlo. Se puede deshacer con el desinstalador.' '  official and is experimental: a Windows update could turn it off. The uninstaller can undo it.') Yellow
    $ok = Preguntar (T '¿Activar Hyper-V en este Windows Home?' 'Enable Hyper-V on this Windows Home?') (T 'SI' 'YES')
  }
  if (-not $ok) { Di (T 'Sin Hyper-V no se puede instalar. No se ha cambiado nada más.' 'It cannot be installed without Hyper-V. Nothing else has been changed.') Red; Salir 2 }
  if ($casa) { Hacer (T "añadir los $($e.PaquetesHV) paquetes de Hyper-V que ya están en Windows (DISM /Add-Package)" "add the $($e.PaquetesHV) Hyper-V packages already present in Windows (DISM /Add-Package)") {
      foreach ($m in Get-ChildItem "$env:SystemRoot\servicing\Packages\Microsoft-Hyper-V*.mum") { Dism "/Add-Package:`"$($m.FullName)`"" }; Marcar PaquetesHome $true } }
  Hacer (T 'activar la característica Hyper-V (DISM /Enable-Feature Microsoft-Hyper-V /All)' 'enable the Hyper-V feature (DISM /Enable-Feature Microsoft-Hyper-V /All)') { Dism '/Enable-Feature /FeatureName:Microsoft-Hyper-V /All'; Marcar HyperVActivado $true }
  $reiniciar = $true
}
if (-not $e.Wsl) { Hacer (T 'instalar el Subsistema de Windows para Linux (wsl --install --no-distribution)' 'install Windows Subsystem for Linux (wsl --install --no-distribution)') {
    & wsl.exe --install --no-distribution; if ($LASTEXITCODE) { throw (T 'No se pudo instalar WSL. Compruebe Internet y repita.' 'WSL could not be installed. Check the Internet connection and run it again.') }; Marcar WslInstalado $true }; $reiniciar = $true }
if ($e.ClaveQcom -ne 0) { Hacer (T "poner $QVALOR=0 en HKLM\SOFTWARE\QCOM\Drivers\DxKMD (vídeo por GPU en Android)" "set $QVALOR=0 in HKLM\SOFTWARE\QCOM\Drivers\DxKMD (GPU video in Android)") {
    if (-not (Test-Path $QCOM)) { New-Item $QCOM -Force | Out-Null }   # sin -Force si ya existe: New-Item -Force vaciaría la clave
    New-ItemProperty $QCOM -Name $QVALOR -PropertyType DWord -Value 0 -Force | Out-Null; Marcar QcomPuesta $true }; $reiniciar = $true }
if (-not $reiniciar -and -not $e.EnGrupo -and -not $hecho.GrupoPuesto) {   # el grupo no existe hasta reiniciar con Hyper-V activado
  Hacer (T 'añadir su usuario al grupo de administradores de Hyper-V' 'add your account to the Hyper-V Administrators group') {
    if (-not (Get-LocalGroupMember -SID $SID_HV | Where-Object { $_.SID.Value -eq $yo })) { Add-LocalGroupMember -SID $SID_HV -Member $yo; Marcar GrupoAñadido $true }
    Marcar GrupoPuesto $true }
}
if ($reiniciar) {
  Pasos (T 'Ahora hace falta REINICIAR Windows (el instalador no lo hace solo):' 'Windows now needs a RESTART (the installer does not do it for you):') @(
    (T 'Guarde y cierre lo que tenga abierto.' 'Save and close anything you have open.'),
    (T 'Abra el menú Inicio, pulse el botón de encendido y elija "Reiniciar".' 'Open the Start menu, select the power button and choose "Restart".'),
    (T 'Cuando Windows vuelva, haga doble clic otra vez en "Instalar.cmd". Verá que continúa en la fase 2 o 3.' 'When Windows is back, double-click "Instalar.cmd" again. You will see it continue at phase 2 or 3.'))
  if (-not $Simular) { Salir 0 }
}

Titulo (T 'Fase 3 de 6: obtener y armar Android (lo más largo: cerca de una hora, con Internet)' 'Phase 3 of 6: get and build Android (the longest part: about an hour, with Internet)')
if (-not $Simular -and -not (@(Get-VMHostPartitionableGpu | ForEach-Object Name) -match 'VEN_QCOM')) {
  Di (T 'Hyper-V no ofrece la GPU Adreno para compartirla (GPU-P). Actualice el controlador gráfico con Windows Update, reinicie y repita.' 'Hyper-V does not offer the Adreno GPU for sharing (GPU-P). Update the graphics driver with Windows Update, restart and run it again.') Red; Salir 2 }
if ($Origen -ne $Destino -and -not $hecho.Copiado) { Hacer (T "copiar el paquete a $Destino" "copy the package to $Destino") {
    # Carpeta exclusiva: el desinstalador la borra entera, así que no puede tener nada de antes.
    if ((Test-Path $Destino) -and (Get-ChildItem $Destino -Force | Select-Object -First 1)) { throw (T "La carpeta $Destino ya existe y no está vacía. Vacíela o elija otra con: Instalar.cmd -Destino C:\OtraCarpeta" "The folder $Destino already exists and is not empty. Empty it or choose another one with: Instalar.cmd -Destino C:\OtherFolder") }
    & robocopy.exe $Origen $Destino /E /NFL /NDL /NJH /NJS /NP | Out-Null; if ($LASTEXITCODE -ge 8) { throw (T "No se pudo copiar a $Destino" "Could not copy to $Destino") }; Marcar Copiado $true } }
$f4 = Join-Path $Destino 'RutaA\fase4'
if (-not (Test-Path (Join-Path $f4 'android.vhdx')) -or $Simular) {
  Di (T "  Android con Google Play se descarga de Google (unos 2,2 GB). Sus condiciones: $LICENCIA" "  Android with Google Play is downloaded from Google (about 2.2 GB). Its terms: $LICENCIA")
  if (-not (Preguntar (T '¿Acepta las condiciones de licencia del SDK de Android de Google?' 'Do you accept the license terms of the Google Android SDK?') (T 'ACEPTO' 'ACCEPT'))) { Di (T 'Sin aceptar no se puede descargar Android. Puede repetir Instalar.cmd cuando quiera.' 'Android cannot be downloaded without accepting. You can run Instalar.cmd again whenever you want.') Red; Salir 2 }
  if ((Distros-Wsl) -contains $DISTRO -and -not $hecho.Distro -and -not $Simular) { throw (T "Ya existe un Linux llamado '$DISTRO' que este instalador no creó. No se toca: quítelo (wsl --unregister $DISTRO) o cámbiele el nombre." "A Linux named '$DISTRO' already exists and this installer did not create it. It is left alone: remove it (wsl --unregister $DISTRO) or rename it.") }
  if ((Distros-Wsl) -notcontains $DISTRO) {
    $bajadas = Join-Path $Destino 'descargas'; $tar = Join-Path $bajadas $ROOTFS
    Hacer (T "descargar Ubuntu 24.04 ARM64 oficial ($UBUNTU) y comprobar su SHA-256" "download the official Ubuntu 24.04 ARM64 ($UBUNTU) and check its SHA-256") {
      New-Item $bajadas -ItemType Directory -Force | Out-Null; Bajar "$UBUNTU/$ROOTFS" $tar; Bajar "$UBUNTU/SHA256SUMS" "$tar.sumas"
      $suma = (Get-FileHash $tar -Algorithm SHA256).Hash
      if (-not (Select-String -Path "$tar.sumas" -Pattern "^$suma\s+\*?$([regex]::Escape($ROOTFS))" -Quiet)) { Remove-Item $tar; throw (T 'La descarga de Ubuntu llegó dañada. Repita Instalar.cmd.' 'The Ubuntu download arrived damaged. Run Instalar.cmd again.') }
    }
    Hacer (T "crear el Linux auxiliar '$DISTRO' (aparte de cualquier Linux suyo; se quita al desinstalar)" "create the helper Linux '$DISTRO' (separate from any Linux of yours; removed when you uninstall)") {
      & wsl.exe --import $DISTRO (Join-Path $Destino 'wsl') $tar --version 2; if ($LASTEXITCODE) { throw (T 'No se pudo crear el Linux auxiliar (wsl --import).' 'The helper Linux could not be created (wsl --import).') }; Marcar Distro $true }
  }
  Hacer (T 'armar Android dentro del Linux auxiliar (instalador/armar.sh: descarga la imagen, la adapta y toma el número de serie de ESTE equipo)' 'build Android inside the helper Linux (instalador/armar.sh: downloads the image, adapts it and takes the serial number of THIS PC)') {
    $r = Ruta-Wsl $Destino
    & wsl.exe -d $DISTRO -u root -e env SD_LICENCIA_GOOGLE=ACEPTO SD_IDIOMA=$Idioma bash "$r/instalador/armar.sh" $r
    if ($LASTEXITCODE) { throw (T 'El armado de Android falló (el motivo está en las líneas de arriba). Repita Instalar.cmd: sigue donde se quedó.' 'Building Android failed (the reason is in the lines above). Run Instalar.cmd again: it continues where it stopped.') }
    & wsl.exe --terminate $DISTRO | Out-Null
  }
}

Titulo (T 'Fase 4 de 6: crear la máquina virtual de Android' 'Phase 4 of 6: create the Android virtual machine')
if (-not $hecho.VmId) {
  Hacer (T "crear la máquina virtual '$VM' (crear_vm.ps1: 8 GB, GPU compartida)" "create the virtual machine '$VM' (crear_vm.ps1: 8 GB, shared GPU)") {
    # crear_vm.ps1 borra cualquier VM con ese nombre: se vuelve a mirar justo antes.
    if (Get-VM | Where-Object { $_.Name -eq $VM -and $_.Id.Guid -ne $hecho.VmCreada }) { throw (T "Ya existe una máquina virtual '$VM' que este instalador no creó. No se toca." "A virtual machine named '$VM' already exists and this installer did not create it. It is left alone.") }
    # Si crear_vm.ps1 falla a medias, la VM ya existe: se anota igual para poder repetir o desinstalar.
    try { & (Join-Path $f4 'crear_vm.ps1') -Segundos 2 | Out-Null } finally { $n = Get-VM | Where-Object Name -eq $VM; if ($n) { Marcar VmCreada $n.Id.Guid } }
    Marcar VmId $hecho.VmCreada
  }
}

Titulo (T 'Fase 5 de 6: programas de Windows y accesos' 'Phase 5 of 6: Windows programs and shortcuts')
Hacer (T 'crear los accesos "Surface Dual" (arranca con Windows) y "Android (Surface Dual)" (menú Inicio)' 'create the shortcuts "Surface Dual" (starts with Windows) and "Android (Surface Dual)" (Start menu)') {
  $b = Join-Path $Destino 'F2\build\emulator\Bandeja.exe'; $w = New-Object -ComObject WScript.Shell
  $previo = Leer-Json $fPrevio   # lo que ya existía antes de instalar no se pisa (el desinstalador no sabría devolverlo)
  if (-not $previo.AccesoInicio) { $a = $w.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Startup')) 'Surface Dual.lnk')); $a.TargetPath = $b; $a.Save() }
  if (-not $previo.AccesoMenu) { $a = $w.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Programs')) 'Android (Surface Dual).lnk')); $a.TargetPath = $b; $a.Arguments = '/entrar'; $a.Save() }
  Marcar Accesos $true
}
Hacer (T 'permitir en el cortafuegos que Android encuentre televisores para enviar contenido (solo HvMdns.exe, UDP 5353, red local)' 'allow Android through the firewall to find TVs to cast to (only HvMdns.exe, UDP 5353, local network)') {
  if ((Leer-Json $fPrevio).Firewall) { Di (T '  Ya había una regla con ese nombre de antes: se deja como estaba.' '  A rule with that name already existed: it is left as it was.') Yellow; return }
  Remove-NetFirewallRule -Name $REGLA -ErrorAction SilentlyContinue
  New-NetFirewallRule -Name $REGLA -DisplayName 'SurfaceDual HvMdns (Cast en Android)' -Direction Inbound -Action Allow -Protocol UDP -LocalPort 5353 `
    -Program (Join-Path $f4 'host\HvMdns.exe') -RemoteAddress LocalSubnet -Profile Any | Out-Null
  Marcar Firewall $true
}

Titulo (T 'Fase 6 de 6: primer arranque de Android (pantalla de inicio de Surface Dual)' 'Phase 6 of 6: first start of Android (Surface Dual home screen)')
if (-not $hecho.Launcher) {
  try {
    Hacer (T 'descargar adb de Google, esperar a que Android arranque e instalar la pantalla de inicio y el botón "Cambiar a Windows"' 'download adb from Google, wait for Android to start and install the home screen and the "Switch to Windows" button') {
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
        if (-not $listo) { throw (T 'Android no terminó de arrancar en 5 minutos.' 'Android did not finish starting in 5 minutes.') }
        & $adb -s $d install -r (Join-Path $Destino 'launcher\build\SurfaceDualLauncher.apk'); if ($LASTEXITCODE) { throw (T 'No se pudo instalar la pantalla de inicio.' 'The home screen could not be installed.') }
        & $adb -s $d shell cmd package set-home-activity com.surfacedual.launcher/.Inicio; if ($LASTEXITCODE) { throw (T 'No se pudo poner la pantalla de inicio.' 'The home screen could not be set.') }
        & $adb -s $d shell cmd statusbar add-tile com.surfacedual.launcher/.AWindows; if ($LASTEXITCODE) { throw (T 'No se pudo añadir el botón Cambiar a Windows.' 'The Switch to Windows button could not be added.') }
        # Android en el idioma elegido ya desde este reinicio (si fallara, el Visor se lo pone igual al entrar: Enlace.cs)
        & $adb -s $d shell cmd locale set-device-locale (Etiqueta-Android $Idioma) *> $null
        & $adb -s $d reboot   # el agente de la imagen concede los permisos del launcher al arrancar con él ya instalado
        Marcar Launcher $true
      } finally { & $adb kill-server *> $null; Stop-Process -Id $puente.Id -Force -ErrorAction SilentlyContinue }
    }
  } catch { Di (T "  No se completó el primer arranque: $_" "  The first start was not completed: $_") Yellow; Di (T '  Lo demás está instalado. Repita Instalar.cmd para reintentar solo este paso.' '  Everything else is installed. Run Instalar.cmd again to retry only this step.') Yellow }
}

Titulo (T 'Instalación terminada' 'Installation finished')
if ($Simular) { Di (T "Simulación: no se ha cambiado nada. Órdenes que se habrían ejecutado: $($Cambios.Count)." "Simulation: nothing has been changed. Commands that would have run: $($Cambios.Count).") Green; Salir 0 }
if (-not $hecho.Launcher) { Di (T 'Falta el primer arranque de Android (fase 6). Repita Instalar.cmd para completarlo.' 'The first start of Android (phase 6) is still missing. Run Instalar.cmd again to complete it.') Yellow; Salir 3 }
Pasos (T 'Lo que queda lo hace usted:' 'What is left is up to you:') @(
  (T 'Reinicie Windows una vez (menú Inicio, botón de encendido, "Reiniciar"): así su usuario estrena el permiso de Hyper-V.' 'Restart Windows once (Start menu, power button, "Restart"): that is when your account gets the Hyper-V permission.'),
  (T 'Al volver verá el icono de Surface Dual junto al reloj. Para entrar en Android: quite el teclado, o abra "Android (Surface Dual)" en el menú Inicio.' 'When it is back you will see the Surface Dual icon next to the clock. To enter Android: detach the keyboard, or open "Android (Surface Dual)" in the Start menu.'),
  (T 'Dentro de Android, abra "Play Store" e inicie sesión con su cuenta de Google.' 'Inside Android, open "Play Store" and sign in with your Google account.'),
  (T 'Para quitarlo todo y dejar el equipo como estaba: doble clic en "Desinstalar.cmd".' 'To remove everything and leave this PC as it was: double-click "Desinstalar.cmd".'))
Salir 0
