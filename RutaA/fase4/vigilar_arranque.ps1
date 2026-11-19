# Arranque colgado de la VM (visto 19/11: 1 de 7 arranques en frío; el 16/11 ya se anotó "Start-VM solo a veces no arranca"):
# la vCPU 0 se queda al ~98 % y las otras a 0, sin tocar el disco, y el usuario ve el logotipo para siempre. Causa sin
# hallar. iniciar.ps1 lanza esto aparte en cada arranque en frío: a los 60 s mira y, si es eso, corta y arranca otra vez (una
# sola vez; el visor y casi todos los ayudantes siguen vivos y se enganchan solos). Queda en %TEMP%\surfacedual_arranque.log.
# -Probar: no espera ni toca la VM; dice qué decidiría con las muestras dadas (autocomprobación).
param([double[]]$Probar)
function Colgada([double[]]$v) { $v.Count -gt 1 -and $v[0] -gt 90 -and -not ($v | Select-Object -Skip 1 | Where-Object { $_ -ge 0.05 }) }
if ($Probar) { Colgada $Probar; return }
$n = 'SurfaceDualAndroid'; Start-Sleep 60
# Solo el arranque que se vigilaba: en marcha y con 50-150 s (apagada, en pausa o vuelta a encender: nada).
function Vigilada { $vm = Get-VM -Name $n -ErrorAction SilentlyContinue; if ($vm -and $vm.State -eq 'Running' -and $vm.Uptime.TotalSeconds -ge 50 -and $vm.Uptime.TotalSeconds -le 150) { $vm.ProcessorCount } else { 0 } }
$cpus = Vigilada; if (-not $cpus) { return }
# Tres muestras seguidas (15 s), todas las vCPU presentes y todas con la firma: una sola no corta una VM sana (revisión Codex).
$v = $null
foreach ($m in @(Get-Counter '\Hyper-V Hypervisor Virtual Processor(surfacedualandroid:*)\% Guest Run Time' -SampleInterval 5 -MaxSamples 3 -ErrorAction SilentlyContinue)) {
  $v = @($m.CounterSamples | Sort-Object InstanceName | ForEach-Object CookedValue)
  if ($v.Count -ne $cpus -or -not (Colgada $v)) { return }
}
if (-not $v -or -not (Vigilada)) { return }   # sin contadores, o la pausaron/apagaron mientras se medía
"{0:s} arranque colgado (vCPU % = {1}): corte y nuevo arranque" -f (Get-Date), (($v | ForEach-Object { [int]$_ }) -join ' ') | Add-Content (Join-Path $env:TEMP 'surfacedual_arranque.log')
# HvCarpetas se cerraba o no según el momento (VM no disponible) y nadie lo reabría (19/11): se cierra aquí siempre y, solo si
# la VM quedó en marcha, iniciar.ps1 (rama Running: no arranca nada ni lanza otro vigilante) abre los ayudantes que falten.
Stop-VM -Name $n -TurnOff -Force; Get-Process HvCarpetas -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1; Start-VM -Name $n -ErrorAction SilentlyContinue
if ((Get-VM -Name $n).State -eq 'Running') { & (Join-Path $PSScriptRoot 'iniciar.ps1') }
else { "{0:s} no volvió a arrancar: {1}" -f (Get-Date), (Get-VM -Name $n).State | Add-Content (Join-Path $env:TEMP 'surfacedual_arranque.log') }
