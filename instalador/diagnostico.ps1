# Diagnóstico de Surface Dual: junta en un .zip del Escritorio lo que hace falta para investigar un fallo en el equipo de otra
# persona (pantallas azules, congelamientos, arranques). Solo lee. Quita usuario, equipo, número de serie, redes Wi-Fi, correos,
# SID, IP y MAC. El archivo de la pantalla azul (minidump) solo va si la persona lo acepta, y se manda en privado.
#   Diagnostico.cmd [-SinPreguntar]   (-SinPreguntar: no pide admin ni el minidump; lo usa la autoprueba)
# La copia con administrador recibe las rutas y nombres de la persona que lo abrió (si el UAC lo acepta otra cuenta, sus datos
# no se mezclan y lo de esta persona se sigue ocultando). Revisión: Codex y Gemini (F7/plan/rev_diagnostico_*.md).
param([switch]$SinPreguntar, [string]$Usuario = $env:USERNAME, [string]$Perfil = $env:USERPROFILE, [string]$Temporal = $env:TEMP,
  [string]$Escritorio = [Environment]::GetFolderPath('Desktop'), [switch]$Elevado)
. (Join-Path $PSScriptRoot 'comun.ps1')
$Idioma = Elegir-Idioma $null $false
$ErrorActionPreference = 'Continue'   # se junta lo que se pueda: un dato que falte no corta el resto

# Con administrador se leen los minidump de C:\Windows\Minidump; sin él, todo lo demás igual.
if (-not $SinPreguntar -and -not $Elevado -and -not (Es-Admin)) {
  $a = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Elevado -Usuario `"$Usuario`" -Perfil `"$Perfil`" -Temporal `"$Temporal`" -Escritorio `"$Escritorio`""
  try { Start-Process powershell.exe -Verb RunAs -ArgumentList $a -ErrorAction Stop; exit 0 }
  catch { Di (T '  Sin permiso de administrador: se junta todo menos los archivos de pantalla azul.' '  No administrator rights: collecting everything except blue-screen files.') Yellow }
}

$fecha = Get-Date -Format 'yyyyMMdd_HHmmss'
$dir = Join-Path $Temporal "SurfaceDual-diagnostico-$fecha"; New-Item $dir -ItemType Directory -Force | Out-Null
$zip = Join-Path $Escritorio "SurfaceDual-diagnostico-$fecha.zip"

# Se borra de todo texto antes de comprimir (primero lo más largo). Números de serie genéricos ("Default string") no se tocan.
$generico = '^(none|default string|to be filled.*|system serial number|0+|n/?a)$'
$privado = @($Perfil, $env:USERPROFILE, $Usuario, $env:USERNAME, $env:COMPUTERNAME,
  (Get-CimInstance Win32_BIOS -ErrorAction SilentlyContinue).SerialNumber, (Get-CimInstance Win32_ComputerSystemProduct -ErrorAction SilentlyContinue).IdentifyingNumber,
  (Get-CimInstance Win32_ComputerSystemProduct -ErrorAction SilentlyContinue).UUID) +
  @(netsh wlan show profiles 2>$null | Select-String ':\s*(.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }) |
  Where-Object { $_ -and $_.Length -ge 3 -and $_ -notmatch $generico } | Sort-Object Length -Descending -Unique
function Limpio([string]$t) {
  # Solo palabras completas: un usuario "Admin" no debe tapar "Administrador" (20/11, diagnóstico del probador de Reddit)
  foreach ($p in $privado) { $t = $t -replace ('(?<![\w])' + [regex]::Escape($p) + '(?![\w])'), '<privado>' }
  $t = $t -replace '[\w.+-]+@[\w-]+\.[\w.-]+', '<correo>' -replace 'S-1-5-21(-\d+){3,4}', '<SID>'
  $t = $t -replace '\b(?:\d{1,3}\.){3}\d{1,3}\b', '<IP>' -replace '\b(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}\b', '<MAC>'
  $t
}
function Guardar([string]$nombre, $contenido) { Limpio ($contenido | Out-String -Width 400) | Set-Content (Join-Path $dir $nombre) -Encoding utf8 }

Titulo (T 'Surface Dual: diagnóstico (solo lee, no cambia nada)' 'Surface Dual: diagnostics (read-only, changes nothing)')

Di (T '  Equipo y Windows...' '  PC and Windows...')
$os = Get-CimInstance Win32_OperatingSystem; $cs = Get-CimInstance Win32_ComputerSystem; $bios = Get-CimInstance Win32_BIOS -ErrorAction SilentlyContinue
$cv = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -ErrorAction SilentlyContinue
$dg = Get-CimInstance -Namespace root\Microsoft\Windows\DeviceGuard -ClassName Win32_DeviceGuard -ErrorAction SilentlyContinue
$hecho = Get-Content (Join-Path $DATOS 'hecho.json') -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json -ErrorAction SilentlyContinue
Guardar 'equipo.txt' @(
  "Fecha: $(Get-Date -Format s)"
  "Modelo: $($cs.Manufacturer) $($cs.Model); firmware $($bios.SMBIOSBIOSVersion)"
  "Windows: $($os.Caption) $($cv.DisplayVersion) compilación $($os.BuildNumber).$($cv.UBR)"
  "RAM total: $([math]::Round($cs.TotalPhysicalMemory/1GB,1)) GB; libre ahora: $([math]::Round($os.FreePhysicalMemory/1MB,1)) GB"
  "Archivo de paginación: $((Get-CimInstance Win32_PageFileUsage -ErrorAction SilentlyContinue | ForEach-Object { "$($_.AllocatedBaseSize) MB, pico $($_.PeakUsage) MB" }) -join '; ')"
  "Procesador: $((Get-CimInstance Win32_Processor).Name)"
  "Gráfica: $((Get-CimInstance Win32_VideoController | ForEach-Object { "$($_.Name) controlador v$($_.DriverVersion) del $($_.DriverDate)" }) -join '; ')"
  "VBS (seguridad basada en virtualización): estado $($dg.VirtualizationBasedSecurityStatus); servicios en marcha $($dg.SecurityServicesRunning -join ',') (2 = HVCI)"
  "Ajuste de vídeo del instalador (DxKMD $QVALOR): $((Get-ItemProperty $QCOM -ErrorAction SilentlyContinue).$QVALOR)"
  "Surface Dual: idioma $((Get-ItemProperty $CLAVE -ErrorAction SilentlyContinue).Idioma), apagado automático $((Get-ItemProperty $CLAVE -ErrorAction SilentlyContinue).AutoApagarMin) min"
  "Instalador: fases hechas $(if ($hecho) { ($hecho.PSObject.Properties | Where-Object { $_.Value -eq $true } | ForEach-Object Name) -join ', ' } else { '(sin hecho.json)' })"
  "Administrador: $(Es-Admin)"
)

Di (T '  Máquina virtual y Hyper-V...' '  Virtual machine and Hyper-V...')
Guardar 'android_vm.txt' @(
  (Get-VM -Name $VM -ErrorAction SilentlyContinue | Format-List State, Uptime, MemoryStartup, DynamicMemoryEnabled, ProcessorCount, Generation, Version, AutomaticStopAction, AutomaticStartAction | Out-String)
  (Get-VMGpuPartitionAdapter -VMName $VM -ErrorAction SilentlyContinue | Format-List MinPartitionVRAM, MaxPartitionVRAM, OptimalPartitionVRAM, MinPartitionEncode, MaxPartitionEncode, MinPartitionDecode, MaxPartitionDecode, MinPartitionCompute, MaxPartitionCompute | Out-String)
  (Get-VMHostPartitionableGpu -ErrorAction SilentlyContinue | Format-List ValidPartitionCounts, PartitionCount | Out-String)
  (Get-VMHardDiskDrive -VMName $VM -ErrorAction SilentlyContinue | ForEach-Object { "$(Split-Path $_.Path -Leaf) $([math]::Round((Get-Item $_.Path -ErrorAction SilentlyContinue).Length/1GB,1)) GB" })
  'Puertos reservados por Windows (TCP):'; (netsh interface ipv4 show excludedportrange protocol=tcp | Out-String)
  'Procesos de Surface Dual:'; (Get-Process VisorVM, Bandeja, HvAudio, HvCamara, HvSensores, HvCarpetas, HvMdns, HvPuente -ErrorAction SilentlyContinue | Format-Table Name, StartTime, WorkingSet64 -AutoSize | Out-String)
)

Di (T '  Registros de Windows (últimos 7 días)...' '  Windows event logs (last 7 days)...')
$desde = (Get-Date).AddDays(-7)
function Eventos($filtro) {
  $r = Get-WinEvent -FilterHashtable ($filtro + @{ StartTime = $desde }) -ErrorAction SilentlyContinue -ErrorVariable e
  if ($e -and $e[0].FullyQualifiedErrorId -notlike 'NoMatchingEventsFound*') { "(no se pudo leer: $($e[0].Exception.Message))" }
  $r | Sort-Object TimeCreated | ForEach-Object {
    $datos = ([xml]$_.ToXml()).Event.EventData.Data | Where-Object Name | ForEach-Object { "$($_.Name)=$($_.'#text')" }
    "{0:s} [{1}] id={2} {3}: {4} {5}" -f $_.TimeCreated, $_.LevelDisplayName, $_.Id, $_.ProviderName, ($_.Message -replace '\s+', ' '), ($datos -join ' ') }
}
Guardar 'eventos_pantallas_azules.txt' @(
  'Pantallas azules (BugCheck, id 1001; código y sus 4 parámetros en el texto):'; (Eventos @{ LogName = 'System'; ProviderName = 'Microsoft-Windows-WER-SystemErrorReporting' })
  'Apagados inesperados (Kernel-Power 41, con BugcheckCode) y reinicios (6008):'; (Eventos @{ LogName = 'System'; Id = 41, 6008 })
  'Errores de hardware (WHEA):'; (Eventos @{ LogName = 'System'; ProviderName = 'Microsoft-Windows-WHEA-Logger' })
  'Gráfica (Display, reinicios del controlador):'; (Eventos @{ LogName = 'System'; ProviderName = 'Display' })
  'Fallos de controladores en vivo (LiveKernelEvent):'; (Eventos @{ LogName = 'Application'; ProviderName = 'Windows Error Reporting' } | Where-Object { $_ -match 'LiveKernelEvent|BlueScreen' })
)
Guardar 'eventos_hyperv.txt' @(
  (Eventos @{ LogName = 'Microsoft-Windows-Hyper-V-Worker-Admin' })
  (Eventos @{ LogName = 'Microsoft-Windows-Hyper-V-VMMS-Admin'; Level = 1, 2, 3 })
)
Guardar 'eventos_programas.txt' (Eventos @{ LogName = 'Application'; Level = 1, 2 } | Where-Object { $_ -match 'VisorVM|Bandeja|HvAudio|HvCamara|HvSensores|HvCarpetas|HvMdns|HvPuente' })

Di (T '  Registros de Surface Dual...' '  Surface Dual logs...')
foreach ($f in Get-Item "$Temporal\surfacedual_*.log", "$Temporal\visorvm_tiempos.log" -ErrorAction SilentlyContinue) {
  Guardar ("log_" + $f.Name) (Get-Content $f.FullName -Tail 2000 -ErrorAction SilentlyContinue)
}
# Pantalla de la VM cuando se colgó en GRUB (vigilar_arranque.ps1): solo texto de GRUB y el logotipo de Hyper-V, nada de Android.
Copy-Item "$Temporal\surfacedual_arranque_*.png" $dir -ErrorAction SilentlyContinue

Di (T '  Archivos de pantalla azul...' '  Blue-screen files...')
$dumps = @(Get-ChildItem "$env:SystemRoot\Minidump" -Filter *.dmp -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
Guardar 'minidumps_lista.txt' $(if ($dumps) { $dumps | ForEach-Object { "{0} {1:s} {2:N0} KB" -f $_.Name, $_.LastWriteTime, ($_.Length / 1KB) } }
  elseif (Es-Admin) { 'C:\Windows\Minidump vacío' } else { 'Sin permiso de administrador para leer C:\Windows\Minidump' })
$conDump = $false
if ($dumps -and -not $SinPreguntar) {
  Di (T "  Hay $($dumps.Count) archivo(s) de pantalla azul; el más reciente es del $($dumps[0].LastWriteTime)." "  Found $($dumps.Count) blue-screen file(s); the newest is from $($dumps[0].LastWriteTime).") Yellow
  Di (T '  Suele indicar qué controlador falló, pero puede llevar trozos de la memoria de Windows de ese momento.' '  It usually shows which driver failed, but it can contain pieces of Windows memory from that moment.') Yellow
  if ((Read-Host (T '  ¿Incluir el más reciente? (S/N)' '  Include the newest one? (Y/N)')).Trim() -match '^[SsYy]') { Copy-Item $dumps[0].FullName $dir; $conDump = $true }
}

try { Compress-Archive -Path "$dir\*" -DestinationPath $zip -Force -ErrorAction Stop; if (-not (Test-Path $zip)) { throw 'zip' }; Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue }
catch { Di (T "  No se pudo crear el .zip. Los datos quedan en: $dir" "  The .zip could not be created. The data is in: $dir") Red; $zip = $dir }
Titulo (T 'Listo' 'Done')
Pasos (T 'Lo que queda lo hace usted:' 'What is left for you:') @(
  (T "El archivo está en el Escritorio: $(Split-Path $zip -Leaf)" "The file is on your Desktop: $(Split-Path $zip -Leaf)"),
  (T 'Si quiere, ábralo y revise lo que lleva: son textos e imágenes del arranque.' 'If you want, open it and check what is inside: text files and boot screenshots.'),
  $(if ($conDump) { T 'Lleva el archivo de pantalla azul: mándelo en privado (mensaje directo con un enlace de OneDrive o Google Drive), no en un sitio público.' 'It includes the blue-screen file: send it privately (a direct message with a OneDrive or Google Drive link), not on a public site.' }
    else { T 'Súbalo a un Issue de GitHub (SrLeCuack/SurfaceDual) o mande un enlace por mensaje directo.' 'Upload it to a GitHub Issue (SrLeCuack/SurfaceDual) or send a link by direct message.' }))
if (-not $SinPreguntar) { Start-Process explorer.exe "/select,`"$zip`""; if ($Elevado) { [void](Read-Host (T 'Pulse Intro para cerrar esta ventana' 'Press Enter to close this window')) } }
