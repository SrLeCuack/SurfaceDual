# Modo Android en equipos de 16 GB (pantallas azules de dxgmms2 con la memoria casi llena, ESTADO.md 175): al entrar en Android
# se CONGELAN los programas de Windows que tienen ventana (todos sus procesos: navegador, Claude, Office...) y DESPUÉS se vacía su
# memoria (orden del usuario: congelados ya no la vuelven a pedir). Al volver a Windows se descongelan tal como estaban.
# No se tocan: Windows (escritorio, entrada, pantalla, procesos compartidos), Surface Dual, la consola, el Administrador de tareas
# ni quien llamó a este script (la Bandeja, o la sesión que lo esté probando). Revisión Codex/Gemini (F7/plan/rev_1016_*.md):
# una sola ejecución a la vez (mutex), cada proceso se anota ANTES de seguir con el siguiente (si se corta, nada queda sin
# anotar) y con su hora de inicio (no descongelar otro programa que reutilice el número).
#   congelar.ps1 -Accion Congelar | Descongelar
param([ValidateSet('Congelar', 'Descongelar')][string]$Accion = 'Congelar')
$lista = Join-Path $env:TEMP 'surfacedual_congelados.txt'
if (-not ('SD.Hielo' -as [type])) {
  Add-Type -Namespace SD -Name Hielo -MemberDefinition @'
[DllImport("ntdll.dll")] public static extern int NtSuspendProcess(IntPtr h);
[DllImport("ntdll.dll")] public static extern int NtResumeProcess(IntPtr h);
[DllImport("psapi.dll")] public static extern bool EmptyWorkingSet(IntPtr h);
'@
}
$mutex = New-Object Threading.Mutex($false, 'Local\SurfaceDualCongelar')
$tengo = try { $mutex.WaitOne(20000) } catch [Threading.AbandonedMutexException] { $true }
if (-not $tengo) { return }   # otra ejecución no termina: no tocar nada (Codex)
try {
  # Descongelar siempre primero: nunca dos veces congelado, y si una vuelta a Windows se perdió, aquí se arregla.
  $quedan = foreach ($l in Get-Content $lista -ErrorAction SilentlyContinue) {
    $id, $inicio = $l -split ','
    $p = Get-Process -Id $id -ErrorAction SilentlyContinue
    if ($p -and "$($p.StartTime.Ticks)" -eq $inicio) { try { if ([SD.Hielo]::NtResumeProcess($p.Handle) -ne 0) { $l } } catch { $l } }   # el que falle, para reintentar
  }
  if ($quedan) { $quedan | Set-Content $lista -Encoding ascii } else { Remove-Item $lista -ErrorAction SilentlyContinue }
  if ($Accion -eq 'Descongelar') { return }

  $ses = (Get-Process -Id $PID).SessionId
  $padres = @{}; foreach ($w in Get-CimInstance Win32_Process -Property ProcessId, ParentProcessId) { $padres[[int]$w.ProcessId] = [int]$w.ParentProcessId }   # una sola consulta (era 1,9 s)
  $ancestros = @(); $x = $PID
  for ($i = 0; $i -lt 32 -and $x; $i++) { $ancestros += $x; $x = $padres[$x] }
  $no = 'VisorVM', 'Bandeja', 'explorer', 'dwm', 'csrss', 'winlogon', 'sihost', 'ctfmon', 'TextInputHost', 'ShellExperienceHost',
    'StartMenuExperienceHost', 'SearchHost', 'LockApp', 'ApplicationFrameHost', 'ShellHost', 'SecurityHealthSystray', 'Taskmgr',
    'conhost', 'OpenConsole', 'WindowsTerminal', 'powershell', 'pwsh', 'cmd', 'msedgewebview2',
    'svchost', 'dllhost', 'rundll32', 'RuntimeBroker', 'backgroundTaskHost', 'taskhostw', 'audiodg', 'fontdrvhost'   # compartidos (Gemini)
  $enSesion = Get-Process | Where-Object { $_.SessionId -eq $ses -and $_.Id -notin $ancestros }
  $nombres = $enSesion | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero -and $_.ProcessName -notin $no -and $_.ProcessName -notlike 'Hv*' } |
    Select-Object -ExpandProperty ProcessName -Unique
  $n = 0
  foreach ($p in $enSesion | Where-Object { $_.ProcessName -in $nombres }) {
    try {
      Add-Content $lista "$($p.Id),$($p.StartTime.Ticks)" -Encoding ascii   # anotado ANTES de congelar: si se corta, nada queda sin anotar (Codex)
      if ([SD.Hielo]::NtSuspendProcess($p.Handle) -eq 0) { $n++ }   # descongelar uno que no llegó a congelarse no le hace nada
    } catch { }   # sin permiso (programa como administrador): se deja
  }
  # Después de congelar: vaciar la memoria de los programas de la sesión (menos lo que dibuja la pantalla y Surface Dual).
  foreach ($p in $enSesion | Where-Object { $_.ProcessName -notin 'VisorVM', 'dwm', 'csrss', 'winlogon' -and $_.ProcessName -notlike 'Hv*' }) {
    try { [void][SD.Hielo]::EmptyWorkingSet($p.Handle) } catch { }
  }
  "congelados: $n procesos de $(@($nombres).Count) programas ($($nombres -join ', '))"
} finally { if ($tengo) { $mutex.ReleaseMutex() } }
