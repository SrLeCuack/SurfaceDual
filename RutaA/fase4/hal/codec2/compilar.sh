#!/bin/bash
# Compila el servicio Codec2 (android.hardware.media.c2-service.surfacedual). Lo llama ../compilar.sh; requiere
# RutaA/fase4/video/compilar_android.sh (FFmpeg, libva y el driver VA de Mesa) y las cabeceras de generar_aidl.sh.
# A diferencia de los otros HAL (NDK + libc++ estático), enlaza clases C++ del vendor (libcodec2_vndk, libcodec2_aidl_V2,
# libstagefright_foundation): se compila con el ABI de libc++ de la plataforma (std::__1, no std::__ndk1) y su libc++.so.
set -e
H=$(cd "$(dirname "$0")" && pwd); S=$(cd "$H/../../../.." && pwd)
NDKT=$S/herramientas/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64; SYS=$NDKT/sysroot; RD=$NDKT/lib/clang/18
O=/opt/fase4/hal/codec2; A=/opt/fase4/aosp; V=/root/vsd/src/lib64; VI=/opt/fase4/video/inst; mkdir -p $O/abi
# Fuentes de AOSP android17-release (solo cabeceras + SimpleC2Component copiado en base/).
B=https://android.googlesource.com/platform
traer() { [ -d $A/$2 ] || { mkdir -p $A/$2; curl -fsSL "$B/$1/+archive/refs/heads/android17-release/$3.tar.gz" | tar xz -C $A/$2; }; }
traer frameworks/av av/codec2 media/codec2
traer frameworks/av av/foundation media/module/foundation
traer system/core core/libutils libutils
traer system/core core/libcutils libcutils
traer system/core core/libsystem libsystem
traer system/libbase libbase include
traer system/logging liblog liblog/include
traer frameworks/native nw libs/nativewindow/include
traer hardware/libhardware hw include_all
# MultiAccessUnitHelper.h (que incluye ComponentStore.h) pide cabeceras de HIDL que no usa: vacías.
mkdir -p $O/shim/hidl $O/shim/hwbinder; : > $O/shim/hidl/Status.h; : > $O/shim/hwbinder/IBinder.h
# ABI de la plataforma: mismo libc++ (v1) con el espacio de nombres std::__1.
sed 's/#define _LIBCPP_ABI_NAMESPACE __ndk1/#define _LIBCPP_ABI_NAMESPACE __1/' $SYS/usr/include/c++/v1/__config_site > $O/abi/__config_site
grep -q "_LIBCPP_ABI_NAMESPACE __1" $O/abi/__config_site
C=$A/av/codec2
INC="-isystem $O/abi -I$H/base -I$H/gen/include -I$H/../gen/include -I/opt/fase4/binder_ndk/include_cpp -I/opt/fase4/binder_ndk/include_ndk
     -I/opt/fase4/binder_ndk/include_platform -I$C/core/include -I$C/vndk/include -I$C/hal/aidl/include -I$C/hal/common/include
     -I$A/av/foundation/include -I$A/core/libutils/include -I$A/core/libcutils/include -I$A/core/libsystem/include
     -I$A/libbase -I$A/liblog -I$A/nw -I$A/hw -I$O/shim -I$VI/include"
CXX="clang++ --target=aarch64-linux-android35 --sysroot=$SYS -resource-dir=$RD -mno-outline-atomics -std=c++20 -O2 -fPIE
     -fno-exceptions -fno-rtti -D__ANDROID_VENDOR__ -DBINDER_STABILITY_SUPPORT -Wno-deprecated-declarations $INC"
for f in base/SimpleC2Component base/SimpleC2Interface; do
  o=$O/$(basename $f).o; [ $o -nt $H/$f.cpp ] && [ $o -nt $0 ] || $CXX -c $H/$f.cpp -o $o
done
# "prueba": busca también d3d12_drv_video.so en /data/local/tmp (servicio lanzado a mano; no va a la imagen).
N=android.hardware.media.c2-service.surfacedual; X=; [ "${1:-}" = prueba ] && { N=$N.prueba; X=-DSD_DRI_PRUEBA; }
$CXX $X -c $H/codec2.cpp -o $O/codec2$X.o
# Libs del vendor (las mismas que usa el servicio goldfish que se sustituye); libbinder_ndk/liblog/libc del NDK. Los
# símbolos de plataforma de libbinder_ndk (AServiceManager_addService...) no están en el stub del NDK: se resuelven al cargar (el
# enlazador de Android falla al arrancar si falta alguno).
L=""; for l in libc++ libavservices_minijail libcodec2_aidl_V2 libcodec2_vndk libcodec2 libstagefright_foundation libutils libcutils libbase \
                android.hardware.media.c2-V2-ndk; do L="$L $V/$l.so"; done
$CXX -pie -nostdlib++ $O/SimpleC2Component.o $O/SimpleC2Interface.o $O/codec2$X.o $L \
  $VI/lib/libavcodec.a $VI/lib/libavutil.a $VI/lib/libva.so $VI/lib/libva-drm.so $VI/lib/libdrm.a -lbinder_ndk -llog -lz -lm -ldl \
  -fuse-ld=lld -rtlib=compiler-rt --unwindlib=none -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-all -o $O/$N
llvm-strip $O/$N
ls -la $O/$N
