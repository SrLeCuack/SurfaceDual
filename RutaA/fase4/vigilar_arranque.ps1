# Arranque colgado de la VM (~1 de cada 8 arranques en frío; ESTADO.md 149). Evidencia del 19/11 (F7/plan/arranque_colgado/):
# se cuelga DENTRO de GRUB, en la orden `linux` (lectura del kernel desde el disco con el firmware de Hyper-V), antes de entregar
# el control al kernel: la vCPU 0 queda al ~98 %, las otras a 0, y Hyper-V nunca anota el evento 18601 ("arrancó correctamente
# un sistema operativo" = ExitBootServices), que en un arranque sano sale en 0-1 s. Android aún no ha tocado su disco: cortar
# ahí no pierde nada. iniciar.ps1 lanza esto aparte en cada arranque en frío: a los 6 s, si no hay 18601 y la vCPU 0 gira sola,
# guarda la pantalla de la VM (PNG), corta y arranca otra vez; hasta 3 intentos. Queda en %TEMP%\surfacedual_arranque.log.
# Los ayudantes los abre el iniciar.ps1 que lanzó esto, que sigue esperando el 18601 (también el del rearranque).
# -Probar: no espera ni toca la VM; dice qué decidiría con las muestras de vCPU dadas (autocomprobación).
# -Desde: cuándo pidió iniciar.ps1 el arranque (un arranque anterior a eso no es el vigilado).
param([double[]]$Probar, [datetime]$Desde = (Get-Date).AddSeconds(-30))
function Colgada([double[]]$v) { $v.Count -gt 1 -and $v[0] -gt 90 -and -not ($v | Select-Object -Skip 1 | Where-Object { $_ -ge 0.05 }) }
if ($Probar) { Colgada $Probar; return }
$n = 'SurfaceDualAndroid'; $log = Join-Path $env:TEMP 'surfacedual_arranque.log'
function Anotar([string]$t) { "{0:s} {1}" -f (Get-Date), $t | Add-Content $log }
# Lo que muestra la pantalla de la VM (ahí escribe GRUB sus errores): miniatura RGB565 de Hyper-V a PNG. Si falla, se sigue.
function Pantalla([string]$png) {
  try {
    Add-Type -AssemblyName System.Drawing; $ns = 'root\virtualization\v2'; $w = 1024; $h = 768
    $vs = Get-CimInstance -Namespace $ns -ClassName Msvm_ComputerSystem -Filter "ElementName='$n'" | Get-CimAssociatedInstance -ResultClassName Msvm_VirtualSystemSettingData | Where-Object VirtualSystemType -eq 'Microsoft:Hyper-V:System:Realized' | Select-Object -First 1
    $r = Get-CimInstance -Namespace $ns -ClassName Msvm_VirtualSystemManagementService | Invoke-CimMethod -MethodName GetVirtualSystemThumbnailImage -Arguments @{ TargetSystem = $vs; WidthPixels = [uint16]$w; HeightPixels = [uint16]$h }
    if ($r.ReturnValue -ne 0 -or -not $r.ImageData) { return $false }
    $b = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format16bppRgb565)
    $d = $b.LockBits((New-Object System.Drawing.Rectangle(0, 0, $w, $h)), 'WriteOnly', $b.PixelFormat)
    [Runtime.InteropServices.Marshal]::Copy([byte[]]$r.ImageData, 0, $d.Scan0, [Math]::Min($r.ImageData.Length, $d.Stride * $h)); $b.UnlockBits($d)
    $b.Save($png, [System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose(); $true
  } catch { $false }
}
# Eventos de Hyper-V de ESTE arranque ($desde = cuándo arrancó): 18500 "se inició" y 18601 "arrancó un sistema operativo".
# $true/$false si el registro se pudo leer; $null si no (con $null nunca se corta: "no lo sé" no es "no ha arrancado").
function Evento([int]$id, [guid]$vm, [datetime]$desde) {
  try { [bool](Get-WinEvent -FilterHashtable @{ LogName = 'Microsoft-Windows-Hyper-V-Worker-Admin'; Id = $id; StartTime = $desde } -ErrorAction Stop | Where-Object { $_.Message -match $vm }) }
  catch { if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { $false } else { $null } }
}
# El arranque vigilado es el que hay al lanzar esto (y después, el que rearranca este mismo script): uno más viejo o uno
# que encendió otro no se toca.
function Inicio { $vm = Get-VM -Name $n -ErrorAction SilentlyContinue; if ($vm -and $vm.State -eq 'Running') { (Get-Date).AddSeconds(-$vm.Uptime.TotalSeconds) } }
function Mismo([datetime]$a) { $b = Inicio; $b -and [math]::Abs(($b - $a).TotalSeconds) -le 2 }
$inicio = Inicio; if (-not $inicio -or $inicio -lt $Desde.AddSeconds(-2)) { return }
for ($intento = 1; $intento -le 3; $intento++) {
  Start-Sleep 6
  if (-not (Mismo $inicio)) { break }   # apagada, en pausa o vuelta a encender por otro: nada
  $vm = Get-VM -Name $n; $desde = $inicio.AddSeconds(-1)
  # Sin 18601 puede ser solo que el registro no se deja leer: tiene que verse el 18500 de este mismo arranque (el registro
  # se lee) y, además, la firma de CPU en dos muestras válidas seguidas con todas las vCPU (una VM sana a estas alturas
  # tiene todos los núcleos trabajando).
  if ((Evento 18601 $vm.Id $desde) -ne $false -or -not (Evento 18500 $vm.Id $desde)) { break }
  $v = $null; $firmas = 0
  foreach ($m in @(Get-Counter '\Hyper-V Hypervisor Virtual Processor(surfacedualandroid:*)\% Guest Run Time' -SampleInterval 2 -MaxSamples 2 -ErrorAction SilentlyContinue)) {
    $v = @($m.CounterSamples | Sort-Object InstanceName | ForEach-Object CookedValue)
    if ($v.Count -eq $vm.ProcessorCount -and (Colgada $v)) { $firmas++ }
  }
  if ($firmas -ne 2) { break }
  $png = Join-Path $env:TEMP ("surfacedual_arranque_{0:yyyyMMdd_HHmmss}.png" -f (Get-Date)); $hayPng = Pantalla $png
  # Justo antes de cortar, otra vez: sigue siendo el mismo arranque (no la apagaron y encendieron mientras se medía) y sigue
  # sin 18601 (leído de verdad). Con 18601 el kernel ya manda y Android puede estar escribiendo: no se corta (revisión Codex
  # y Gemini). Queda la ventana de milisegundos entre esta consulta y el corte, a los ~11 s de un paso que sano tarda 1 s.
  if (-not (Mismo $inicio) -or (Evento 18601 $vm.Id $desde) -ne $false) { break }
  $a = (Get-Date) - $inicio; Stop-VM -Name $n -TurnOff -Force
  $que = "arranque colgado en GRUB (sin evento 18601 a los {0:0} s; vCPU % = {1}); pantalla de la VM: {2}" -f $a.TotalSeconds, (($v | ForEach-Object { [int]$_ }) -join ' '), $(if ($hayPng) { $png } else { 'no se pudo guardar' })
  if ($intento -eq 3) {   # tres seguidos: apagada y con aviso, mejor que un logotipo eterno
    Anotar "$que; tercer cuelgue seguido: la VM queda apagada"
    [void](New-Object -ComObject WScript.Shell).Popup("Android no pudo arrancar (3 intentos). Vuelve a abrirlo.`nRegistro: $log", 3600, 'Surface Dual', 0); break   # 1 h y se cierra sola
  }
  Anotar "$que; corte y nuevo arranque (intento $($intento + 1) de 3)"
  # Solo se adopta el rearranque propio: si Start-VM falla (p. ej. otro la encendió en ese segundo), se deja.
  Start-Sleep 1; try { Start-VM -Name $n -ErrorAction Stop } catch { Anotar "no volvió a arrancar: $($_.Exception.Message)"; break }
  $inicio = Inicio; if (-not $inicio) { break }
}
