#!/bin/bash
# HAL de sensores de SurfaceDual (sensores.cpp): AIDL android.hardware.sensors v3 con los sensores de Windows (HvSensores.exe,
# vsock 5009). Se compila con el NDK como los demás HAL (hal/compilar.sh) y sustituye al multihal del emulador.
#   wsl -u root -e bash .../hal/sensores/compilar.sh [dir del vendor]   (con dir: lo instala; lo llama preparar.sh)
set -e
C=$(cd "$(dirname "$0")" && pwd); H=$(dirname "$C"); S=$(cd "$H/../../.." && pwd); V=$1
NDKT=$S/herramientas/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64; SYS=$NDKT/sysroot; RD=$NDKT/lib/clang/18
O=/opt/fase4/hal/sensores; mkdir -p $O/obj
G=/opt/fase4/hal/libaidl_graficos.a; [ -f $G ] || { echo "falta $G: correr antes hal/compilar.sh"; exit 1; }   # common (NativeHandle)
CXX="clang++ --target=aarch64-linux-android35 --sysroot=$SYS -resource-dir=$RD -mno-outline-atomics -std=c++20 -O2 -fPIC
     -D__ANDROID_VENDOR__ -DBINDER_STABILITY_SUPPORT -I$C/gen/include -I$H/gen/include
     -I/opt/fase4/binder_ndk/include_cpp -I/opt/fase4/binder_ndk/include_ndk -I/opt/fase4/binder_ndk/include_platform"
for c in $(find $C/gen/src -name '*.cpp'); do
  o=$O/obj/$(echo ${c#$C/gen/src/} | tr / _).o
  [ $o -nt $c ] || $CXX -c $c -o $o &
  while [ $(jobs -r | wc -l) -ge $(nproc) ]; do sleep 0.2; done
done; wait
rm -f $O/libaidl_sensores.a; ar rcs $O/libaidl_sensores.a $O/obj/*.o
# Símbolos de libbinder_ndk más nuevos que el NDK r27c: se resuelven al cargar (como el resto de HAL).
$CXX -fPIE -pie $C/sensores.cpp $O/libaidl_sensores.a $G -fuse-ld=lld -stdlib=libc++ -static-libstdc++ -rtlib=compiler-rt \
  --unwindlib=libunwind -lbinder_ndk -llog -Wl,--unresolved-symbols=ignore-in-object-files -Wl,--gc-sections -s \
  -o $O/android.hardware.sensors-service.multihal
ls -la $O/android.hardware.sensors-service.multihal
[ -n "$V" ] || exit 0

# Instalación (preparar.sh). Mismo nombre que el multihal del emulador: conserva su etiqueta (hal_sensors_default_exec).
ETQ() { setfattr -n security.selinux -v u:object_r:$1:s0 "${@:2}"; }
rm -f $V/lib64/hw/android.hardware.sensors@2.1-impl.ranchu.so $V/etc/sensors/hals.conf
install -m 0755 $O/android.hardware.sensors-service.multihal $V/bin/hw/android.hardware.sensors-service.multihal
ETQ hal_sensors_default_exec $V/bin/hw/android.hardware.sensors-service.multihal
cat > $V/etc/init/android.hardware.sensors-service-multihal.rc <<'RC'
service vendor.sensors-hal-surfacedual /vendor/bin/hw/android.hardware.sensors-service.multihal
    class hal
    user system
    group system
    task_profiles ServiceCapacityLow
RC
cat > $V/etc/vintf/manifest/android.hardware.sensors-multihal.xml <<'XML'
<manifest version="9.0" type="device">
    <hal format="aidl">
        <name>android.hardware.sensors</name>
        <version>3</version>
        <fqname>ISensors/default</fqname>
    </hal>
</manifest>
XML
# Solo lo que de verdad hay en la Surface Pro 11 (medido con Windows.Devices.Sensors 18/11): sin barómetro, proximidad,
# humedad, temperatura ni bisagra.
cat > $V/etc/permissions/surfacedual_sensores.xml <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<permissions>
    <feature name="android.hardware.sensor.accelerometer" />
    <feature name="android.hardware.sensor.gyroscope" />
    <feature name="android.hardware.sensor.compass" />
    <feature name="android.hardware.sensor.light" />
</permissions>
XML
ETQ vendor_configs_file $V/etc/init/android.hardware.sensors-service-multihal.rc \
    $V/etc/vintf/manifest/android.hardware.sensors-multihal.xml $V/etc/permissions/surfacedual_sensores.xml
