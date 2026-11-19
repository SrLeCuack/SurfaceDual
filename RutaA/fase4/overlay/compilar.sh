#!/bin/bash
# Compila el overlay del framework (RutaA/fase4/overlay/SurfaceDualFramework.apk) con las herramientas del launcher.
# preparar.sh lo copia a /vendor/overlay. Uso (Git Bash): bash RutaA/fase4/overlay/compilar.sh
set -e
cd "$(dirname "$0")"; H=../../../herramientas; BT=$(ls -d $H/sdk/build-tools/*); AJ=$H/sdk/platforms/android-37.0/android.jar
B=build; rm -rf $B; mkdir -p $B
"$BT/aapt2.exe" compile --dir res -o $B/res.zip
"$BT/aapt2.exe" link -o $B/base.apk --manifest AndroidManifest.xml -I "$AJ" --min-sdk-version 34 --target-sdk-version 36 $B/res.zip
"$BT/zipalign.exe" -f -p 4 $B/base.apk $B/alineado.apk
cmd //c "$(cygpath -w $BT/apksigner.bat)" sign --ks "$(cygpath -w $H/debug.keystore)" --ks-pass pass:android \
  --out "$(cygpath -w SurfaceDualFramework.apk)" "$(cygpath -w $B/alineado.apk)"
rm -rf $B; ls -l SurfaceDualFramework.apk
