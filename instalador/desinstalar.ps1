# Desinstalador de Surface Dual (F7/plan/instalador_diseno.md, punto 5). Lo lanza Desinstalar.cmd.
# Deshace SOLO lo que hizo instalar.ps1 (hecho.json) y vuelve al estado anotado antes (previo.json), en orden inverso.
#   -Simular -> dice lo que haría, sin cambiar nada.  -CarpetaEstado <carpeta con previo.json y hecho.json> -> solo para pruebas.ps1.
param([switch]$Simular, [string]$CarpetaEstado, [string]$Usuario)
$ErrorActionPreference = 'Stop'; $sim = $Simular.IsPresent
. (Join-Path $PSScriptRoot 'comun.ps1'); $Simular = $sim
trap { Di "ERROR: $_" Red; Di 'No se siguió. Lo ya hecho queda anotado; puede repetir o desinstalar.' Red; Salir 1 }
Set-Location $env:SystemRoot   # no retener la carpeta de instalación
if ($CarpetaEstado) { if (-not $Simular) { throw "-CarpetaEstado solo vale con -Simular" }; $DATOS = $CarpetaEstado }
$previo = Leer-Json (Join-Path $DATOS 'previo.json'); $hecho = Leer-Json (Join-Path $DATOS 'hecho.json')
if (-not $previo -or -not $hecho) { Di "No hay ninguna instalación hecha con este instalador ($DATOS). No se ha cambiado nada." Yellow; Salir 0 }
if (-not $Simular -and -not (Es-Admin)) {
  Pasos 'Windows va a pedir permiso de administrador:' @('Aparecerá una ventana "¿Quieres permitir que esta aplicación haga cambios en el dispositivo?".', 'Pulse "Sí". Se abrirá otra ventana azul que continúa.')
  try { Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Usuario $([Security.Principal.WindowsIdentity]::GetCurrent().User.Value)" }
  catch { Di 'No se dio el permiso de administrador. No se ha cambiado nada.' Red; exit 2 }
  exit 0
}
# Accesos, preferencias, Linux auxiliar y grupo son de la cuenta que instaló: con otra se desharía lo que no es.
$yo = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
if (-not $Simular -and (($Usuario -and $Usuario -ne $yo) -or ($hecho.Usuario -and $hecho.Usuario -ne $yo))) { Di 'Desinstale con la misma cuenta de Windows que instaló (y que sea administradora). No se ha cambiado nada.' Red; Salir 2 }
$Destino = $hecho.Destino; $f4 = Join-Path $Destino 'RutaA\fase4'; $reiniciar = $false
Titulo 'Surface Dual: desinstalar'
if (-not (Preguntar "Se va a quitar Surface Dual de este equipo ($Destino). ¿Seguir?" 'SI')) { Salir 0 }

Hacer 'cerrar los programas de Surface Dual (Bandeja, Visor y ayudantes)' {
  Get-Process Bandeja, VisorVM, HvAudio, HvCamara, HvSensores, HvCarpetas, HvMdns, HvPuente -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "$Destino\*" } | Stop-Process -Force -ErrorAction SilentlyContinue   # solo los de ESTA instalación
}
if ($hecho.VmCreada) { Hacer "apagar y quitar la máquina virtual '$VM'" {
    # Solo la que creó el instalador (mismo Id): otra con el mismo nombre no se toca.
    $mv = Get-VM | Where-Object { $_.Name -eq $VM -and $_.Id.Guid -eq $hecho.VmCreada }
    if ($mv) { Stop-VM -VM $mv -TurnOff -Force -ErrorAction SilentlyContinue; Remove-VM -VM $mv -Force }
  } }
$guardar = -not (Preguntar 'Los datos de Android (apps, fotos, cuentas) están en un disco virtual. ¿BORRARLOS también? (si no escribe la palabra, se conservan)' 'BORRAR')
if ($hecho.Accesos) { Hacer 'quitar los accesos del menú Inicio y del arranque' {
    if (-not $previo.AccesoInicio) { Remove-Item (Join-Path ([Environment]::GetFolderPath('Startup')) 'Surface Dual.lnk') -ErrorAction SilentlyContinue }
    if (-not $previo.AccesoMenu) { Remove-Item (Join-Path ([Environment]::GetFolderPath('Programs')) 'Android (Surface Dual).lnk') -ErrorAction SilentlyContinue }
  } }
if ($hecho.Firewall -and -not $previo.Firewall) { Hacer 'quitar la regla del cortafuegos' { Remove-NetFirewallRule -Name $REGLA -ErrorAction SilentlyContinue } }
if (-not $previo.RegistroUsuario) { Hacer 'devolver los gestos de 3 y 4 dedos de Windows a como estaban y borrar las preferencias de Surface Dual' {
    # La Bandeja guarda aquí el ajuste original mientras Android está delante; si quedó guardado, se repone (mismo SPI que Bandeja.cs).
    $g = (Get-ItemProperty 'HKCU:\Software\SurfaceDual' -ErrorAction SilentlyContinue).Gestos3y4Original
    if ($null -ne $g) {
      $t = Add-Type -PassThru -Name Spi -Namespace SD -MemberDefinition '[DllImport("user32.dll")] public static extern bool SystemParametersInfoW(uint a, uint b, IntPtr c, uint d);'
      [void]$t::SystemParametersInfoW(0x2031, 0, [IntPtr][int]$g, 2)   # shortcut: da por hecho el formato de Bandeja.cs (un entero); si cambia allí, cambiar aquí
    }
    Remove-Item 'HKCU:\Software\SurfaceDual' -Recurse -ErrorAction SilentlyContinue
  } }
Hacer 'borrar la caché de gráficos (%LOCALAPPDATA%\SurfaceDual)' { Remove-Item (Join-Path $env:LOCALAPPDATA 'SurfaceDual') -Recurse -Force -ErrorAction SilentlyContinue }
if ($hecho.Distro -and $previo.Distros -notcontains $DISTRO) { Hacer "quitar el Linux auxiliar '$DISTRO'" { & wsl.exe --unregister $DISTRO | Out-Null } }
if ($hecho.QcomPuesta) { Hacer 'devolver el ajuste del controlador Qualcomm a como estaba' {
    # Solo si sigue con el valor que puso el instalador: un cambio posterior de la persona o del controlador no se pisa.
    if ((Get-ItemProperty $QCOM -Name $QVALOR -ErrorAction SilentlyContinue).$QVALOR -eq 0) {
      if ($null -ne $previo.QcomValor) { Set-ItemProperty $QCOM -Name $QVALOR -Value $previo.QcomValor }
      else {
        Remove-ItemProperty $QCOM -Name $QVALOR
        # Las claves que creó el instalador se quitan solo si siguen vacías (el controlador pudo añadir cosas después).
        if (-not $previo.QcomExistia) { foreach ($k in $QCOM, (Split-Path $QCOM), 'HKLM:\SOFTWARE\QCOM') { $c = Get-Item $k -ErrorAction SilentlyContinue; if ($c -and -not $c.ValueCount -and -not $c.SubKeyCount) { Remove-Item $k } } }
      }
    }
  }; $reiniciar = $true }
if ($hecho.GrupoAñadido -and -not $previo.EnGrupo) { Hacer 'quitar su usuario del grupo de administradores de Hyper-V' {
    Remove-LocalGroupMember -SID $SID_HV -Member ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value) -ErrorAction SilentlyContinue } }
# Hyper-V y WSL: solo si los activó el instalador, y preguntando (otros programas pueden estar usándolos). Por omisión se quedan.
if ($hecho.HyperVActivado -and (Preguntar 'Hyper-V lo activó este instalador. Otros programas (Docker, WSL, Sandbox) pueden usarlo. ¿Desactivarlo? (si no escribe la palabra, se queda activado)' 'DESACTIVAR')) {
  Hacer 'desactivar la característica Hyper-V (los paquetes añadidos en Windows Home quedan en el sistema, inactivos)' {
    Disable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -NoRestart -WarningAction SilentlyContinue | Out-Null }; $reiniciar = $true
}
if ($hecho.RestauracionActivada) { Di '  La "Protección del sistema" de Windows se activó al instalar y se deja activada (es la que guarda el punto de restauración).' }
Hacer "borrar la carpeta $Destino$(if ($guardar) { ' (menos los datos de Android)' })" {
  if ($guardar -and (Test-Path (Join-Path $f4 'android.vhdx'))) {
    $c = "${Destino}_datos_$(Get-Date -Format yyyyMMdd_HHmmss)"; New-Item $c -ItemType Directory | Out-Null
    Move-Item (Join-Path $f4 'android.vhdx'), (Join-Path $f4 'arranque.vhdx') $c
    if (-not (Test-Path (Join-Path $c 'android.vhdx'))) { throw "No se pudieron apartar los datos de Android a $c. No se borra nada." }
    Di "  Los datos de Android quedan en $c (android.vhdx y arranque.vhdx). Bórrelos a mano cuando ya no los quiera." Yellow
  }
  # Solo la carpeta que llenó el instalador (Copiado) y que sigue siendo la nuestra; si se instaló desde la propia carpeta del paquete, se deja.
  if ($hecho.Copiado -and (Ruta-Valida $Destino) -and (Test-Path (Join-Path $Destino 'instalador\instalar.ps1'))) { Remove-Item $Destino -Recurse -Force }
  else { Di "  La carpeta $Destino no la creó el instalador: bórrela a mano si ya no la quiere." Yellow }
  Remove-Item $DATOS -Recurse -Force
}
Titulo 'Desinstalación terminada'
if ($Simular) { Di "Simulación: no se ha cambiado nada. Órdenes que se habrían ejecutado: $($Cambios.Count)." Green; Salir 0 }
if ($reiniciar) { Pasos 'Para terminar, reinicie Windows:' @('Guarde y cierre lo que tenga abierto.', 'Menú Inicio, botón de encendido, "Reiniciar".') }
Di "Si algo no quedó como antes: Panel de control > Recuperación > Abrir Restaurar sistema, y elija el punto ""Surface Dual: antes de instalar"" ($($hecho.PuntoRestauracion))."
Salir 0
