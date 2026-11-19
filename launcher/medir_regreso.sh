#!/bin/bash
# Mide el costo de volver a la pantalla de inicio: N ciclos "abrir Ajustes -> HOME".
# Uso: bash launcher/medir_regreso.sh [apk] [N]. Imprime CPU del launcher (invitado, ticks de 1/100 s) y de QEMU+renderer (host, s).
export MSYS_NO_PATHCONV=1
cd "$(dirname "$0")/.."
APK=${1:-launcher/build/SurfaceDualLauncher.apk}; N=${2:-20}
A="F2/build/platform-tools/adb.exe -s 127.0.0.1:5555"
$A install -r "$APK" >/dev/null && $A shell cmd package set-home-activity com.surfacedual.launcher/.Inicio >/dev/null
$A shell am force-stop com.surfacedual.launcher; $A shell input keyevent HOME; sleep 5
ticks() { $A shell "cat /proc/\$(pidof com.surfacedual.launcher)/stat" | awk '{print $14+$15}'; }
host() { powershell -NoProfile -Command "[math]::Round((Get-Process qemu-system-aarch64,GfxstreamPipeServer | Measure-Object CPU -Sum).Sum,2)"; }
t0=$(ticks); h0=$(host)
for i in $(seq $N); do
  $A shell am start -a android.settings.SETTINGS >/dev/null; sleep 2
  $A shell input keyevent HOME; sleep 2
done
t1=$(ticks); h1=$(host)
echo "$(basename "$APK"): launcher $((t1-t0)) ticks en $N regresos | host $(awk "BEGIN{print $h1-$h0}") s"
