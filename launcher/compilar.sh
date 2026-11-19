#!/bin/bash
# Compila el launcher a launcher/build/SurfaceDualLauncher.apk con las herramientas de herramientas/
# (JDK 21 ARM64, android.jar 37, build-tools 37). Sin Gradle: javac -> d8 -> aapt2 -> zipalign -> apksigner.
# "bash compilar.sh feed SurfaceDualFeed" compila el puente de Discover (launcher/feed) a build_feed/.
set -e
APP=${1:-app}; SALIDA=${2:-SurfaceDualLauncher}
cd "$(dirname "$0")"
H=../herramientas; JDK=$(ls -d $H/jdk-21*); BT=$(ls -d $H/sdk/build-tools/*); AJ=$H/sdk/platforms/android-37.0/android.jar
B=build${1:+_$1}; rm -rf $B; mkdir -p $B/clases $B/dex
export JAVA_HOME=$(cygpath -w "$JDK"); export PATH="$JDK/bin:$PATH"

javac -encoding UTF-8 --release 17 -Xlint:-options -cp "$AJ" -d $B/clases $(find $APP/src -name '*.java')
cmd //c "$(cygpath -w $BT/d8.bat)" --release --min-api 34 --lib "$(cygpath -w $AJ)" --output "$(cygpath -w $B/dex)" $(find $B/clases -name '*.class' -exec cygpath -w {} \;)
"$BT/aapt2.exe" link -o $B/base.apk --manifest $APP/AndroidManifest.xml -I "$AJ" --min-sdk-version 34 --target-sdk-version 36
(cd $B/dex && jar -uf ../base.apk classes.dex)   # jar del JDK añade el dex al zip
[ ! -d $APP/assets ] || (cd $APP && jar -uf ../$B/base.apk assets)   # assets con jar: aapt2 -A guarda las rutas con "\" en Windows y el sistema no las encuentra
"$BT/zipalign.exe" -f -p 4 $B/base.apk $B/alineado.apk
[ -f $H/debug.keystore ] || keytool -genkeypair -keystore $H/debug.keystore -storepass android -keypass android \
  -alias surfacedual -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Surface Dual" >/dev/null 2>&1
cmd //c "$(cygpath -w $BT/apksigner.bat)" sign --ks "$(cygpath -w $H/debug.keystore)" --ks-pass pass:android \
  --out "$(cygpath -w $B/$SALIDA.apk)" "$(cygpath -w $B/alineado.apk)"
ls -l $B/$SALIDA.apk
