# Surface Dual, instalador: piezas compartidas por instalar.ps1, desinstalar.ps1 y pruebas.ps1 (F7/plan/instalador_diseno.md).
# Windows PowerShell 5.1. Leer-Equipo solo lee; Evaluar-Equipo es pura (se prueba con equipos falsos en pruebas.ps1).
$VM = 'SurfaceDualAndroid'; $DISTRO = 'SurfaceDualArmado'; $REGLA = 'SurfaceDual-HvMdns'
$DATOS = Join-Path $env:ProgramData 'SurfaceDual'   # estado del instalador (previo.json, hecho.json)
$QCOM = 'HKLM:\SOFTWARE\QCOM\Drivers\DxKMD'; $QVALOR = 'DisableParavirtOtherVirtualContext'
$SID_HV = 'S-1-5-32-578'   # "Administradores de Hyper-V": por SID, el nombre cambia con el idioma
$CARACT = 'Microsoft-Hyper-V-All', 'Microsoft-Hyper-V', 'HypervisorPlatform', 'VirtualMachinePlatform', 'Microsoft-Windows-Subsystem-Linux'
$MIN_RAM_GB = 15.5   # 16 GB nominales: la VM pide 8 GB fijos (crear_vm.ps1) y Windows necesita el resto
$MIN_DISCO_GB = 64   # suposición: descargas ~3 GB + imagen ~8 GB + discos de Android recién armados ~25 GB + margen
function Salir([int]$c) { if (-not $Simular -and (Es-Admin)) { [void](Read-Host "Pulse Intro para cerrar esta ventana") }; exit $c }   # la ventana elevada se cerraría sin dejar leer
$Simular = $false; $Cambios = New-Object System.Collections.ArrayList

function Di([string]$t, [string]$color = 'Gray') { Write-Host $t -ForegroundColor $color }
function Titulo([string]$t) { Di ''; Di "== $t ==" Cyan }
function Pasos([string]$titulo, [string[]]$lineas) {   # lo que exige a una persona: lista numerada, un paso por línea
  Di ''; Di $titulo Yellow; $i = 0; foreach ($l in $lineas) { $i++; Di "  $i. $l" Yellow }
}
# Todo cambio en el equipo pasa por aquí: en simulación solo se anuncia. $Cambios lo comprueba pruebas.ps1.
function Hacer([string]$que, [scriptblock]$orden) {
  if ($Simular) { Di "  [simulación] $que" DarkGray; [void]$Cambios.Add($que); return }
  Di "  $que..."; & $orden
}
function Preguntar([string]$pregunta, [string]$palabra) {   # confirmación escrita; en simulación se da por aceptada
  if ($Simular) { Di "  [simulación] se preguntaría: $pregunta (hay que escribir $palabra)" DarkGray; return $true }
  (Read-Host "$pregunta Escriba $palabra y pulse Intro para aceptar") -ceq $palabra
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
  if (-not $e.Arm64) { Nota 'Procesador' BLOQUEA "Este equipo no es ARM64 ($($e.Cpu)). Surface Dual solo funciona en equipos con Snapdragon X." }
  elseif ($e.Cpu -notmatch 'Snapdragon|Qualcomm') { Nota 'Procesador' BLOQUEA "ARM64 pero no Qualcomm ($($e.Cpu)): la gráfica de Android necesita la GPU Adreno." }
  else { Nota 'Procesador' OK $e.Cpu }
  if ($e.Build -lt 26100) { Nota 'Windows' BLOQUEA "Hace falta Windows 11 24H2 (compilación 26100) o posterior; este tiene la $($e.Build). Actualice Windows y vuelva a probar." }
  else { Nota 'Windows' OK "Windows 11, compilación $($e.Build), edición $($e.Edicion)" }
  if ($e.RamGB -lt $MIN_RAM_GB) { Nota 'Memoria' BLOQUEA "Hacen falta 16 GB de memoria; este equipo tiene $($e.RamGB) GB (Android usa 8 GB fijos)." }
  else { Nota 'Memoria' OK "$($e.RamGB) GB" }
  if (-not (Ruta-Valida $e.Destino)) { Nota 'Carpeta' BLOQUEA "La carpeta de instalación '$($e.Destino)' no vale: sin espacios, acentos ni eñes (por ejemplo C:\SurfaceDual)." }
  elseif ($null -eq $e.LibreGB) { Nota 'Disco' BLOQUEA "No existe la unidad de '$($e.Destino)'." }
  elseif ($e.LibreGB -lt $MIN_DISCO_GB) { Nota 'Disco' BLOQUEA "Hacen falta $MIN_DISCO_GB GB libres; hay $($e.LibreGB) GB. Libere espacio y vuelva a probar." }
  else { Nota 'Disco' OK "$($e.LibreGB) GB libres" }
  if ($e.HyperV) { Nota 'Hyper-V' OK 'activado' }
  elseif (-not $casa) { Nota 'Hyper-V' ARREGLA 'No está activado; el instalador lo activa (característica oficial de esta edición). Pedirá reiniciar.' }
  elseif ($e.PaquetesHV -gt 0) { Nota 'Hyper-V' ARREGLA 'Windows Home: Microsoft no ofrece Hyper-V en Home. El instalador puede activarlo con los paquetes que Windows ya trae (método NO oficial, experimental); pedirá su permiso y reiniciar.' }
  else { Nota 'Hyper-V' BLOQUEA 'Windows Home sin los paquetes de Hyper-V: aquí no se puede activar. Hace falta Windows 11 Pro.' }
  if ($e.HyperV -and -not $e.Hipervisor) { Nota 'Virtualización' AVISO 'Hyper-V está instalado pero el hipervisor no está en marcha: reinicie; si sigue igual, active la virtualización en el firmware (UEFI).' }
  if ($e.EnGrupo) { Nota 'Permiso de Hyper-V' OK 'su usuario ya puede manejar máquinas virtuales' }
  else { Nota 'Permiso de Hyper-V' ARREGLA 'El instalador añade su usuario al grupo de administradores de Hyper-V (hay que cerrar sesión o reiniciar).' }
  if ($e.Wsl) { Nota 'WSL' OK 'instalado (se usa solo para armar la imagen de Android)' }
  else { Nota 'WSL' ARREGLA 'Falta el Subsistema de Windows para Linux; el instalador lo instala (oficial de Microsoft). Pedirá reiniciar.' }
  # GPU-P: "no se pudo saber" no es "no compatible" (antes de activar Hyper-V no hay forma de preguntarlo).
  if ($null -eq $e.Gpu) { Nota 'Gráfica para Android' AVISO 'Se comprobará cuando Hyper-V esté activado y su usuario tenga permiso.' }
  elseif (-not ($e.Gpu -match 'VEN_QCOM')) { Nota 'Gráfica para Android' BLOQUEA 'Hyper-V no ofrece la GPU Adreno para compartirla (GPU-P). Actualice el controlador gráfico de Qualcomm con Windows Update y vuelva a probar.' }
  else { Nota 'Gráfica para Android' OK "GPU Adreno compartible; controlador $($e.DriverQcom)" }
  if ($e.ClaveQcom -eq 0) { Nota 'Vídeo por GPU' OK 'ajuste del controlador ya puesto' }
  else { Nota 'Vídeo por GPU' ARREGLA 'El instalador pone el ajuste del controlador Qualcomm que permite el vídeo por GPU en Android (pide reiniciar).' }
  if ($e.Switch -eq $false) { Nota 'Red' BLOQUEA 'Falta la red "Default Switch" de Hyper-V. Reinicie; si sigue faltando, repare Hyper-V.' }
  if ($e.VmExiste -and -not $e.NuestraVm) { Nota 'Instalación previa' BLOQUEA "Ya existe una máquina virtual '$VM' que este instalador no creó. No se toca: desinstálela antes o use otro equipo." }
  if (-not $e.ConCorriente) { Nota 'Batería' AVISO 'Conecte el cargador: la instalación tarda cerca de una hora.' }
  $r
}
function Mostrar-Evaluacion($r) {
  $c = @{ OK = 'Green'; AVISO = 'Yellow'; ARREGLA = 'Yellow'; BLOQUEA = 'Red' }
  $t = @{ OK = '[ bien ]'; AVISO = '[ aviso ]'; ARREGLA = '[ se arregla ]'; BLOQUEA = '[ IMPIDE ]' }
  foreach ($x in $r) { Di ("{0,-14} {1,-22} {2}" -f $t[$x.Estado], $x.Nombre, $x.Mensaje) $c[$x.Estado] }
}
