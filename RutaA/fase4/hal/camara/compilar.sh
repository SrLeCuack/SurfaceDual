#!/bin/bash
# HAL de cámara de SurfaceDual: el de goldfish (AOSP device/generic/goldfish/hals/camera, Apache 2.0) con las cámaras de
# Windows por vsock 5007 (HvCamara.exe) en lugar de las de QEMU. Se compila con el NDK como los demás HAL (hal/compilar.sh):
# solo APIs en C del dispositivo (libbinder_ndk, liblog, libcutils, libjpeg, libexif, libyuv) y el mapper de nuestro gralloc.
#   wsl -u root -e bash .../hal/camara/compilar.sh [dir del vendor]   (con dir: lo instala; lo llama preparar.sh)
set -e
C=$(cd "$(dirname "$0")" && pwd); H=$(dirname "$C"); S=$(cd "$H/../../.." && pwd); V=$1
NDKT=$S/herramientas/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64; SYS=$NDKT/sysroot; RD=$NDKT/lib/clang/18
O=/opt/fase4/hal/camara; mkdir -p $O/obj; SRC=/opt/fase4/src_cam; R=https://android.googlesource.com
# Cabeceras de AOSP android17-release (una vez): metadatos de cámara (se compilan dentro: C puro), libjpeg-turbo, libexif, libyuv.
if [ ! -f $SRC/listo ]; then
  get() { mkdir -p $SRC/$2; curl -fsSL "$R/$1/+archive/refs/heads/android17-release/$3.tar.gz" | tar xz -C $SRC/$2; }
  get platform/system/media sm/camera camera
  curl -fsSL "$R/platform/system/media/+/refs/heads/android17-release/private/camera/include/camera_metadata_hidden.h?format=TEXT" | base64 -d > $SRC/sm/camera/src/camera_metadata_hidden.h
  get platform/external/libexif libexif libexif
  get platform/external/libyuv libyuv/include include
  mkdir -p $SRC/jpeg; for f in jpeglib.h jmorecfg.h jerror.h jconfig.h; do
    curl -fsSL "$R/platform/external/libjpeg-turbo/+/refs/heads/android17-release/$f?format=TEXT" | base64 -d > $SRC/jpeg/$f; done
  touch $SRC/listo
fi
G=/opt/fase4/hal/libaidl_graficos.a; [ -f $G ] || { echo "falta $G: correr antes hal/compilar.sh"; exit 1; }   # graphics.common y common
DX=$(find /opt/mesa/src/subprojects -path '*/include/directx/d3d12.h' -print -quit); DX=${DX%/include/directx/d3d12.h}
INC="-I$C/inc -I$C/gen/include -I$H/gen/include -I$SRC/sm/camera/include -I$SRC/sm/camera/src -I$SRC/jpeg -I$SRC -I$SRC/libyuv/include
     -I$H/inc/mapper -I$DX/include -I$DX/include/wsl/stubs -I/opt/mesa/src/include/android_stub"
FL="--target=aarch64-linux-android35 --sysroot=$SYS -resource-dir=$RD -mno-outline-atomics -O2 -fPIC -D__ANDROID_VENDOR__
    -DBINDER_STABILITY_SUPPORT -DLOG_TAG=\"camera.provider.surfacedual\" $INC"
CXX="clang++ $FL -std=c++20 -I/opt/fase4/binder_ndk/include_cpp -I/opt/fase4/binder_ndk/include_ndk -I/opt/fase4/binder_ndk/include_platform -Wno-deprecated-declarations"
# Stubs AIDL de cámara (se recompilan si cambia el .cpp generado por generar_aidl.sh).
for c in $(find $C/gen/src -name '*.cpp'); do
  o=$O/obj/$(echo ${c#$C/gen/src/} | tr / _).o
  [ $o -nt $c ] || $CXX -c $c -o $o &
  while [ $(jobs -r | wc -l) -ge $(nproc) ]; do sleep 0.2; done
done; wait
rm -f $O/libaidl_camara.a; ar rcs $O/libaidl_camara.a $O/obj/*.o
clang $FL -ULOG_TAG -std=c11 -c $SRC/sm/camera/src/camera_metadata.c -o $O/camera_metadata.o
# libjpeg/libexif/libyuv: las del vendor de la imagen (solo para enlazar; en el dispositivo son las mismas).
L=/root/vsd/src/lib64
$CXX -fPIE -pie -Wno-unused-const-variable \
  $C/AFStateMachine.cpp $C/BaseQemuCamera.cpp $C/CachedStreamBuffer.cpp $C/CameraDevice.cpp $C/CameraDeviceSession.cpp \
  $C/CameraProvider.cpp $C/HvCamera.cpp $C/HwCamera.cpp $C/StreamBufferCache.cpp $C/exif.cpp $C/jpeg.cpp \
  $C/list_qemu_cameras.cpp $C/main.cpp $C/mapeador.cpp $C/metadata_utils.cpp $C/qemu_channel.cpp $C/utils.cpp $C/yuv.cpp \
  $O/camera_metadata.o $O/libaidl_camara.a $G \
  -fuse-ld=lld -stdlib=libc++ -static-libstdc++ -rtlib=compiler-rt --unwindlib=libunwind \
  -lbinder_ndk -llog -L/opt/mesa/build/src/android_stub -lcutils $L/libjpeg.so $L/libexif.so $L/libyuv.so -ldl \
  -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-in-object-files -Wl,--gc-sections -s -o $O/android.hardware.camera.provider.ranchu
ls -la $O/android.hardware.camera.provider.ranchu
# Autocomprobación (recorte/escalado, cabeceras del canal, lista): adb push y "<binario> --prueba" en la VM.
[ -n "$V" ] || exit 0

# Instalación en el vendor (preparar.sh). Mismo nombre que el binario de goldfish: conserva su etiqueta SELinux
# (hal_camera_default_exec en vendor_file_contexts). Fuera el HAL emulado de Google (cámaras sintéticas) y el de minigbm.
ETQ() { setfattr -n security.selinux -v u:object_r:$1:s0 "${@:2}"; }
rm -f $V/bin/hw/android.hardware.camera.provider.ranchu_minigbm $V/etc/init/android.hardware.camera.provider.ranchu_minigbm.rc \
      $V/bin/hw/android.hardware.camera.provider@2.7-service-google $V/etc/init/android.hardware.camera.provider@2.7-service-google.rc \
      $V/etc/vintf/manifest/android.hardware.camera.provider@2.7-service-google.xml $V/lib64/libgooglecamerahal*.so \
      $V/lib64/libgooglecamerahwl_impl.so $V/lib64/lib_profiler.so $V/etc/config/emu_camera_*.json \
      $V/etc/permissions/android.hardware.camera.*.prebuilt.xml
install -m 0755 $O/android.hardware.camera.provider.ranchu $V/bin/hw/android.hardware.camera.provider.ranchu
ETQ hal_camera_default_exec $V/bin/hw/android.hardware.camera.provider.ranchu
# Solo arranca: se queda dormido en accept/recv (0 % de CPU) hasta que una app abre la cámara.
cat > $V/etc/init/android.hardware.camera.provider.ranchu.rc <<'RC'
service vendor.camera-provider-surfacedual /vendor/bin/hw/android.hardware.camera.provider.ranchu
    class hal
    user system
    group system
    task_profiles CameraServiceCapacity CameraServicePerformance
RC
cat > $V/etc/vintf/manifest/android.hardware.camera.provider.ranchu.xml <<'XML'
<manifest version="9.0" type="device">
    <hal format="aidl">
        <name>android.hardware.camera.provider</name>
        <version>3</version>
        <fqname>ICameraProvider/internal/0</fqname>
    </hal>
</manifest>
XML
# Funciones que de verdad hay: trasera con autoenfoque y frontal (sin flash, RAW, FULL ni simultáneas).
cat > $V/etc/permissions/surfacedual_camara.xml <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<permissions>
    <feature name="android.hardware.camera" />
    <feature name="android.hardware.camera.any" />
    <feature name="android.hardware.camera.autofocus" />
    <feature name="android.hardware.camera.front" />
</permissions>
XML
# El ajuste "landscape to portrait" del emulador (camera.enable_landscape_to_portrait) le da a las apps una orientación de
# sensor girada y pide ROTATE_AND_CROP, que este HAL no hace: fotos y vídeos salían en vertical con la tablet en
# horizontal (medido 17/11). Aquí la pantalla y las cámaras ya son horizontales: orientación real (0).
E=$(getfattr --only-values -n security.selinux $V/build.prop)
sed -i 's/^camera.enable_landscape_to_portrait=true$/camera.enable_landscape_to_portrait=false/' $V/build.prop
grep -qx 'camera.enable_landscape_to_portrait=false' $V/build.prop; setfattr -n security.selinux -v "$E" $V/build.prop
ETQ vendor_configs_file $V/etc/init/android.hardware.camera.provider.ranchu.rc \
    $V/etc/vintf/manifest/android.hardware.camera.provider.ranchu.xml $V/etc/permissions/surfacedual_camara.xml
