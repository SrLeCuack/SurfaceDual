# Surface Dual, instalador: piezas compartidas por instalar.ps1, desinstalar.ps1 y pruebas.ps1 (F7/plan/instalador_diseno.md).
# Windows PowerShell 5.1. Leer-Equipo solo lee; Evaluar-Equipo es pura (se prueba con equipos falsos en pruebas.ps1).
$VM = 'SurfaceDualAndroid'; $DISTRO = 'SurfaceDualArmado'; $REGLA = 'SurfaceDual-HvMdns'
$DATOS = Join-Path $env:ProgramData 'SurfaceDual'   # estado del instalador (previo.json, hecho.json)
$QCOM = 'HKLM:\SOFTWARE\QCOM\Drivers\DxKMD'; $QVALOR = 'DisableParavirtOtherVirtualContext'
$SID_HV = 'S-1-5-32-578'   # "Administradores de Hyper-V": por SID, el nombre cambia con el idioma
$CARACT = 'Microsoft-Hyper-V-All', 'Microsoft-Hyper-V', 'HypervisorPlatform', 'VirtualMachinePlatform', 'Microsoft-Windows-Subsystem-Linux'
$MIN_RAM_GB = 15.5   # 16 GB nominales: la VM pide 8 GB fijos (crear_vm.ps1) y Windows necesita el resto
$MIN_DISCO_GB = 64   # suposición: descargas ~3 GB + imagen ~8 GB + discos de Android recién armados ~25 GB + margen
function Salir([int]$c) { if (-not $Simular -and (Es-Admin)) { [void](Read-Host (T "Pulse Intro para cerrar esta ventana" "Press Enter to close this window")) }; exit $c }   # la ventana elevada se cerraría sin dejar leer
$Simular = $false; $Cambios = New-Object System.Collections.ArrayList

# Idioma (es|en): se elige al instalar y se guarda en HKCU\Software\SurfaceDual "Idioma" (lo leen la Bandeja y el Visor, y de ahí
# pasa a Android). T lleva los dos textos juntos, donde se usan.
$CLAVE = 'HKCU:\Software\SurfaceDual'
function T([string]$es, [string]$en) { if ($Idioma -eq 'en') { $en } else { $es } }
function Idioma-Windows { if ((Get-UICulture).TwoLetterISOLanguageName -eq 'es') { 'es' } else { 'en' } }
function Idioma-Guardado { $v = (Get-ItemProperty $CLAVE -Name Idioma -ErrorAction SilentlyContinue).Idioma; if ($v -cin 'es', 'en') { $v } }   # -cin: Bandeja y Visor solo entienden minúsculas
# El pedido con -Idioma manda; si no, el ya guardado (reanudar tras reiniciar no vuelve a preguntar); si no, se pregunta (Intro = el de Windows).
function Elegir-Idioma([string]$pedido, [bool]$preguntar) {
  $w = Idioma-Windows; $g = Idioma-Guardado
  if ($pedido) { return $pedido.ToLower() }; if ($g) { return $g }; if (-not $preguntar) { return $w }
  $r = Read-Host "Idioma / Language:  1 = Español   2 = English   (Intro/Enter = $(if ($w -eq 'es') { 'Español' } else { 'English' }))"
  if ($r.Trim() -eq '1') { 'es' } elseif ($r.Trim() -eq '2') { 'en' } else { $w }
}
# Etiqueta que se le pone a Android (igual que Enlace.cs): la de Windows si es el mismo idioma; si no, idioma elegido + región de Windows.
function Etiqueta-Android([string]$idioma, [Globalization.CultureInfo]$ui = (Get-UICulture), [string]$region = [Globalization.RegionInfo]::CurrentRegion.TwoLetterISORegionName) {
  if ($ui.TwoLetterISOLanguageName -eq $idioma) { $ui.Name } else { "$idioma-$region" }
}

# Paquetes de Hyper-V de Windows Home (nombre~clave~arq~idioma~versión.mum): Windows guarda varias versiones de cada uno y las
# viejas pueden haberse limpiado, y añadirlas da 1168 "no encontrado" (20/11, usuario de Reddit). Por paquete e idioma, sus
# versiones de la más nueva a la más vieja; primero los neutros (sin idioma), que los de idioma dependen de ellos.
function Paquetes-HV([string[]]$nombres) {   # lista de grupos (cada uno, sus versiones); con , para que uno solo no se desenvuelva (Gemini)
  $r = New-Object System.Collections.ArrayList
  $nombres | Group-Object { $p = ($_ -replace '\.mum$') -split '~'; "$($p[0])~$($p[2])~$($p[3])" } | Sort-Object { ($_.Name -split '~')[2] -ne '' }, Name |
    ForEach-Object { [void]$r.Add(@($_.Group | Sort-Object { [version]((($_ -replace '\.mum$') -split '~')[4]) } -Descending)) }
  , $r
}
# Tras armar, el Linux auxiliar retiene varios GB unos segundos aunque se cierre y Hyper-V no puede encender Android
# (0x800705AA, 22/11 en un equipo de 16 GB). Se cierra el nuestro y, si no queda otro Linux abierto, se apaga WSL entero.
function Soltar-Wsl {
  & wsl.exe --terminate $DISTRO 2>$null | Out-Null
  $vivas = "$(& wsl.exe --list --running --quiet 2>$null)" -replace "[`0\s]", ''
  if (-not $LASTEXITCODE -and -not $vivas) { & wsl.exe --shutdown 2>$null | Out-Null }   # si la consulta falla, no se apaga (Codex)
}
# Encender Android reintentando 2 min (la memoria se libera sola en segundos); si no, un error que explica qué cerrar.
function Encender-Android {
  for ($k = 0; $k -lt 12 -and (Get-VM -Name $VM).State -ne 'Running'; $k++) {
    try { Start-VM -Name $VM -ErrorAction Stop } catch { $e1 = $_; if ("$e1" -notmatch '0x800705AA|RAM|memor') { throw }; Start-Sleep 10 }   # solo la memoria se espera
  }
  if ((Get-VM -Name $VM).State -ne 'Running') {
    if ("$e1" -match '0x800705AA|RAM|memor') { throw (T "Windows no tiene $([math]::Round((Get-VM -Name $VM).MemoryStartup / 1GB)) GB libres para Android. Cierre programas que usen mucha memoria y otras máquinas virtuales (por ejemplo Docker, Windows Sandbox o el modo Cowork de Claude) y repita. Detalle: $e1" "Windows does not have $([math]::Round((Get-VM -Name $VM).MemoryStartup / 1GB)) GB free for Android. Close programs that use a lot of memory and other virtual machines (for example Docker, Windows Sandbox or Claude's Cowork mode) and try again. Details: $e1") }
    throw $e1
  }
}
function Di([string]$t, [string]$color = 'Gray') { Write-Host $t -ForegroundColor $color }
function Titulo([string]$t) { Di ''; Di "== $t ==" Cyan }
function Pasos([string]$titulo, [string[]]$lineas) {   # lo que exige a una persona: lista numerada, un paso por línea
  Di ''; Di $titulo Yellow; $i = 0; foreach ($l in $lineas) { $i++; Di "  $i. $l" Yellow }
}
# Todo cambio en el equipo pasa por aquí: en simulación solo se anuncia. $Cambios lo comprueba pruebas.ps1.
function Hacer([string]$que, [scriptblock]$orden) {
  if ($Simular) { Di (T "  [simulación] $que" "  [simulation] $que") DarkGray; [void]$Cambios.Add($que); return }
  Di "  $que..."; & $orden
}
function Preguntar([string]$pregunta, [string]$palabra) {   # confirmación escrita; en simulación se da por aceptada
  if ($Simular) { Di (T "  [simulación] se preguntaría: $pregunta (hay que escribir $palabra)" "  [simulation] would ask: $pregunta (you must type $palabra)") DarkGray; return $true }
  (Read-Host (T "$pregunta Escriba $palabra y pulse Intro para aceptar" "$pregunta Type $palabra and press Enter to accept")) -ceq $palabra
}
function Es-Admin { ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) }
function Ruta-Valida([string]$r) { $r -match '^[A-Za-z]:(\\[A-Za-z0-9_.-]+)+$' }   # preparar.sh usa la ruta sin comillas: ni espacios ni acentos
function Ruta-Wsl([string]$r) { '/mnt/' + $r.Substring(0, 1).ToLower() + ($r.Substring(2) -replace '\\', '/') }
function Leer-Json([string]$f) { if (Test-Path $f) { Get-Content $f -Raw -Encoding UTF8 | ConvertFrom-Json } }
function Guardar-Json($o, [string]$f) { $o | ConvertTo-Json -Depth 5 | Set-Content $f -Encoding UTF8 }
function Distros-Wsl {   # nombres de las distros; sin WSL, lista vacía
  if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) { return @() }
  $env:WSL_UTF8 = '1'; @(& wsl.exe -l -q 2>$null | ForEach-Object { "$_".Trim() } | Where-Object { $_ })
}

# Solo lectura. Devuelve los datos del equipo que Evaluar-Equipo juzga. $null = no se pudo saber (todavía).
function Leer-Equipo([string]$Destino) {
  $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
  $so = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
  $cs = Get-CimInstance Win32_ComputerSystem
  $unidad = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$($Destino.Substring(0, 2))'"
  $hv = [bool](Get-Service vmms -ErrorAction SilentlyContinue) -and [bool](Get-Command Get-VM -ErrorAction SilentlyContinue)
  $gpu = $null; $switch = $null; $hayVm = $null
  if ($hv) {   # sin el grupo de Hyper-V (o sin admin) estas órdenes fallan: queda "no se pudo saber"
    try { $gpu = @(Get-VMHostPartitionableGpu -ErrorAction Stop | ForEach-Object Name) } catch { }
    try { $switch = [bool](Get-VMSwitch -ErrorAction Stop | Where-Object Name -eq "Default Switch") } catch { }
    try { $hayVm = [bool](Get-VM -ErrorAction Stop | Where-Object Name -eq $VM) } catch { }
  }
  $bat = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue | Select-Object -First 1
  $wslOk = $false
  if (Get-Command wsl.exe -ErrorAction SilentlyContinue) { & wsl.exe --status *> $null; $wslOk = $LASTEXITCODE -eq 0 }
  @{
    Arm64       = $cpu.Architecture -eq 12
    Cpu         = "$($cpu.Name)".Trim()
    Build       = [int]$so.CurrentBuild
    Edicion     = "$($so.EditionID)"
    RamGB       = [math]::Round($cs.TotalPhysicalMemory / 1GB, 1)
    LibreGB     = if ($unidad) { [math]::Round($unidad.FreeSpace / 1GB, 1) } else { $null }
    Destino     = $Destino
    Hipervisor  = [bool]$cs.HypervisorPresent
    HyperV      = $hv
    PaquetesHV  = @(Get-ChildItem "$env:SystemRoot\servicing\Packages\Microsoft-Hyper-V*.mum" -ErrorAction SilentlyContinue).Count
    EnGrupo     = [bool]((& "$env:SystemRoot\System32\whoami.exe" /groups) -match $SID_HV)
    Wsl         = $wslOk
    Gpu         = $gpu
    DriverQcom  = "$((Get-CimInstance Win32_VideoController | Where-Object PNPDeviceID -match 'QCOM' | Select-Object -First 1).DriverVersion)"
    ClaveQcom   = (Get-ItemProperty $QCOM -Name $QVALOR -ErrorAction SilentlyContinue).$QVALOR
    Switch      = $switch
    VmExiste    = $hayVm
    NuestraVm   = [bool](Leer-Json (Join-Path $DATOS 'hecho.json')).VmCreada
    ConCorriente = if ($bat) { $bat.BatteryStatus -ne 1 } else { $true }   # 1 = descargándose
  }
}

# Pura. Estado: OK, AVISO (se puede seguir), ARREGLA (lo hace el instalador), BLOQUEA (no se puede instalar aquí).
function Evaluar-Equipo($e) {
  $r = New-Object System.Collections.ArrayList
  function Nota($n, $s, $m) { [void]$r.Add([pscustomobject]@{ Nombre = $n; Estado = $s; Mensaje = $m }) }
  $casa = $e.Edicion -match '^Core'
  if (-not $e.Arm64) { Nota 'Procesador' BLOQUEA (T "Este equipo no es ARM64 ($($e.Cpu)). Surface Dual solo funciona en equipos con Snapdragon X." "This PC is not ARM64 ($($e.Cpu)). Surface Dual only works on PCs with Snapdragon X.") }
  elseif ($e.Cpu -notmatch 'Snapdragon|Qualcomm') { Nota 'Procesador' BLOQUEA (T "ARM64 pero no Qualcomm ($($e.Cpu)): la gráfica de Android necesita la GPU Adreno." "ARM64 but not Qualcomm ($($e.Cpu)): Android graphics need the Adreno GPU.") }
  else { Nota 'Procesador' OK $e.Cpu }
  if ($e.Build -lt 26100) { Nota 'Windows' BLOQUEA (T "Hace falta Windows 11 24H2 (compilación 26100) o posterior; este tiene la $($e.Build). Actualice Windows y vuelva a probar." "Windows 11 24H2 (build 26100) or later is required; this PC has build $($e.Build). Update Windows and try again.") }
  else { Nota 'Windows' OK (T "Windows 11, compilación $($e.Build), edición $($e.Edicion)" "Windows 11, build $($e.Build), edition $($e.Edicion)") }
  if ($e.RamGB -lt $MIN_RAM_GB) { Nota 'Memoria' BLOQUEA (T "Hacen falta 16 GB de memoria; este equipo tiene $($e.RamGB) GB (Android usa 8 GB fijos)." "16 GB of memory is required; this PC has $($e.RamGB) GB (Android uses a fixed 8 GB).") }
  else { Nota 'Memoria' OK "$($e.RamGB) GB" }
  if (-not (Ruta-Valida $e.Destino)) { Nota 'Carpeta' BLOQUEA (T "La carpeta de instalación '$($e.Destino)' no vale: sin espacios, acentos ni eñes (por ejemplo C:\SurfaceDual)." "The install folder '$($e.Destino)' is not valid: no spaces or accented letters (for example C:\SurfaceDual).") }
  elseif ($null -eq $e.LibreGB) { Nota 'Disco' BLOQUEA (T "No existe la unidad de '$($e.Destino)'." "The drive of '$($e.Destino)' does not exist.") }
  elseif ($e.LibreGB -lt $MIN_DISCO_GB) { Nota 'Disco' BLOQUEA (T "Hacen falta $MIN_DISCO_GB GB libres; hay $($e.LibreGB) GB. Libere espacio y vuelva a probar." "$MIN_DISCO_GB GB of free space is required; there are $($e.LibreGB) GB. Free up space and try again.") }
  else { Nota 'Disco' OK (T "$($e.LibreGB) GB libres" "$($e.LibreGB) GB free") }
  if ($e.HyperV) { Nota 'Hyper-V' OK (T 'activado' 'enabled') }
  elseif (-not $casa) { Nota 'Hyper-V' ARREGLA (T 'No está activado; el instalador lo activa (característica oficial de esta edición). Pedirá reiniciar.' 'Not enabled; the installer enables it (an official feature of this edition). A restart will be needed.') }
  elseif ($e.PaquetesHV -gt 0) { Nota 'Hyper-V' ARREGLA (T 'Windows Home: Microsoft no ofrece Hyper-V en Home. El instalador puede activarlo con los paquetes que Windows ya trae (método NO oficial, experimental); pedirá su permiso y reiniciar.' 'Windows Home: Microsoft does not offer Hyper-V on Home. The installer can enable it with the packages Windows already includes (NOT an official method, experimental); it will ask for your permission and a restart.') }
  else { Nota 'Hyper-V' BLOQUEA (T 'Windows Home sin los paquetes de Hyper-V: aquí no se puede activar. Hace falta Windows 11 Pro.' 'Windows Home without the Hyper-V packages: it cannot be enabled here. Windows 11 Pro is required.') }
  if ($e.HyperV -and -not $e.Hipervisor) { Nota 'Virtualización' AVISO (T 'Hyper-V está instalado pero el hipervisor no está en marcha: reinicie; si sigue igual, active la virtualización en el firmware (UEFI).' 'Hyper-V is installed but the hypervisor is not running: restart; if nothing changes, turn on virtualization in the firmware (UEFI).') }
  if ($e.EnGrupo) { Nota 'Permiso de Hyper-V' OK (T 'su usuario ya puede manejar máquinas virtuales' 'your account can already manage virtual machines') }
  else { Nota 'Permiso de Hyper-V' ARREGLA (T 'El instalador añade su usuario al grupo de administradores de Hyper-V (hay que cerrar sesión o reiniciar).' 'The installer adds your account to the Hyper-V Administrators group (you will need to sign out or restart).') }
  if ($e.Wsl) { Nota 'WSL' OK (T 'instalado (se usa solo para armar la imagen de Android)' 'installed (only used to build the Android image)') }
  else { Nota 'WSL' ARREGLA (T 'Falta el Subsistema de Windows para Linux; el instalador lo instala (oficial de Microsoft). Pedirá reiniciar.' 'Windows Subsystem for Linux is missing; the installer installs it (official, from Microsoft). A restart will be needed.') }
  # GPU-P: "no se pudo saber" no es "no compatible" (antes de activar Hyper-V no hay forma de preguntarlo).
  if ($null -eq $e.Gpu) { Nota 'Gráfica para Android' AVISO (T 'Se comprobará cuando Hyper-V esté activado y su usuario tenga permiso.' 'It will be checked once Hyper-V is enabled and your account has permission.') }
  elseif (-not ($e.Gpu -match 'VEN_QCOM')) { Nota 'Gráfica para Android' BLOQUEA (T 'Hyper-V no ofrece la GPU Adreno para compartirla (GPU-P). Actualice el controlador gráfico de Qualcomm con Windows Update y vuelva a probar.' 'Hyper-V does not offer the Adreno GPU for sharing (GPU-P). Update the Qualcomm graphics driver with Windows Update and try again.') }
  else { Nota 'Gráfica para Android' OK (T "GPU Adreno compartible; controlador $($e.DriverQcom)" "Adreno GPU can be shared; driver $($e.DriverQcom)") }
  if ($e.ClaveQcom -eq 0) { Nota 'Vídeo por GPU' OK (T 'ajuste del controlador ya puesto' 'driver setting already in place') }
  else { Nota 'Vídeo por GPU' ARREGLA (T 'El instalador pone el ajuste del controlador Qualcomm que permite el vídeo por GPU en Android (pide reiniciar).' 'The installer applies the Qualcomm driver setting that allows GPU video in Android (needs a restart).') }
  if ($e.Switch -eq $false) { Nota 'Red' BLOQUEA (T 'Falta la red "Default Switch" de Hyper-V. Reinicie; si sigue faltando, repare Hyper-V.' 'The Hyper-V "Default Switch" network is missing. Restart; if it is still missing, repair Hyper-V.') }
  if ($e.VmExiste -and -not $e.NuestraVm) { Nota 'Instalación previa' BLOQUEA (T "Ya existe una máquina virtual '$VM' que este instalador no creó. No se toca: desinstálela antes o use otro equipo." "A virtual machine named '$VM' already exists and this installer did not create it. It is left alone: remove it first or use another PC.") }
  if (-not $e.ConCorriente) { Nota 'Batería' AVISO (T 'Conecte el cargador: la instalación tarda cerca de una hora.' 'Plug in the charger: the installation takes about an hour.') }
  $r
}
function Mostrar-Evaluacion($r) {
  $c = @{ OK = 'Green'; AVISO = 'Yellow'; ARREGLA = 'Yellow'; BLOQUEA = 'Red' }
  $t = @{ OK = (T '[ bien ]' '[ ok ]'); AVISO = (T '[ aviso ]' '[ notice ]'); ARREGLA = (T '[ se arregla ]' '[ will be fixed ]'); BLOQUEA = (T '[ IMPIDE ]' '[ BLOCKS ]') }
  # Nombre es también la clave que usan instalar.ps1 y pruebas.ps1: se queda en español y aquí se enseña traducido.
  $n = @{ 'Procesador' = 'Processor'; 'Memoria' = 'Memory'; 'Carpeta' = 'Folder'; 'Disco' = 'Disk'; 'Virtualización' = 'Virtualization'; 'Permiso de Hyper-V' = 'Hyper-V permission'
    'Gráfica para Android' = 'Graphics for Android'; 'Vídeo por GPU' = 'GPU video'; 'Red' = 'Network'; 'Instalación previa' = 'Previous install'; 'Batería' = 'Battery' }
  foreach ($x in $r) { Di ("{0,-18} {1,-22} {2}" -f $t[$x.Estado], $(if ($Idioma -eq 'en' -and $n[$x.Nombre]) { $n[$x.Nombre] } else { $x.Nombre }), $x.Mensaje) $c[$x.Estado] }
}
