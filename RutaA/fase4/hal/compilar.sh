#!/bin/bash
# Compila los HAL de gráficos de Hyper-V (WSL root, clang ARM64 nativo + sysroot del NDK r27c, como fase1b).
#   wsl -u root -e bash /mnt/c/SurfaceDual/RutaA/fase4/hal/compilar.sh
set -e
H=$(cd "$(dirname "$0")" && pwd); S=$(cd "$H/../../.." && pwd)
NDKT=$S/herramientas/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64; SYS=$NDKT/sysroot; RD=$NDKT/lib/clang/18
O=/opt/fase4/hal; mkdir -p $O/obj; [ "$(cat $O/opciones 2>/dev/null)" = v2 ] || { rm -f $O/obj/*.o; echo v2 > $O/opciones; }   # cambiar v2 al cambiar CXX
# Cabeceras de libbinder_ndk de Android 17 (frameworks/native android17-release): las del NDK r27c son viejas para estos stubs.
[ -d /opt/fase4/binder_ndk/include_ndk ] || { B=https://android.googlesource.com/platform/frameworks/native/+archive/refs/heads/android17-release/libs/binder/ndk
  for p in include_ndk include_cpp include_platform; do mkdir -p /opt/fase4/binder_ndk/$p; curl -fsSL "$B/$p.tar.gz" | tar xz -C /opt/fase4/binder_ndk/$p; done; }
CXX="clang++ --target=aarch64-linux-android35 --sysroot=$SYS -resource-dir=$RD -mno-outline-atomics -std=c++20 -O2 -fPIC
     -D__ANDROID_VENDOR__ -DBINDER_STABILITY_SUPPORT -I/opt/fase4/binder_ndk/include_cpp -I/opt/fase4/binder_ndk/include_ndk -I/opt/fase4/binder_ndk/include_platform -I$H/gen/include -fuse-ld=lld -stdlib=libc++ -static-libstdc++ -rtlib=compiler-rt --unwindlib=libunwind"
# Stubs AIDL (una vez; se recompilan si cambia el .cpp).
for c in $(find $H/gen/src -name '*.cpp'); do
  o=$O/obj/$(echo ${c#$H/gen/src/} | tr / _).o
  [ $o -nt $c ] || $CXX -c $c -o $o &
  while [ $(jobs -r | wc -l) -ge $(nproc) ]; do sleep 0.2; done
done; wait
ar rcs $O/libaidl_graficos.a $O/obj/*.o
ls -la $O/libaidl_graficos.a

# Gralloc (allocator + mapper) sobre D3D12. libcutils (native_handle_*) del stub de Mesa solo para enlazar; en el
# dispositivo resuelven las libs reales. Símbolos de libbinder_ndk más nuevos que el NDK r27c: se resuelven al cargar.
DX=$(find /opt/mesa/src/subprojects -path '*/include/directx/d3d12.h' -print -quit); DX=${DX%/include/directx/d3d12.h}
GI="-I$DX/include -I$DX/include/wsl/stubs -I/opt/mesa/src/include/android_stub -I$H/inc/mapper -I$H/inc/implutils"
GL="-L/opt/mesa/build/src/android_stub -lcutils -llog -Wl,--unresolved-symbols=ignore-in-object-files"
$CXX $GI -c $DX/src/dxguids.cpp -o $O/dxguids.o
$CXX $GI -fPIE -pie $H/gralloc/allocator.cpp $O/dxguids.o $O/libaidl_graficos.a -lbinder_ndk $GL -ldl \
     -o $O/android.hardware.graphics.allocator-service.ranchu
$CXX $GI -shared -fvisibility=hidden $H/gralloc/mapper.cpp $O/dxguids.o $O/libaidl_graficos.a -lbinder_ndk $GL -ldl \
     -Wl,-soname,mapper.ranchu.so -o $O/mapper.ranchu.so
# Autocomprobación del YUV del mapper (se corre a mano en la VM; ver su cabecera). No va a la imagen.
$CXX $GI -fPIE -pie $H/gralloc/prueba_yuv.cpp $O/dxguids.o $O/libaidl_graficos.a -lbinder_ndk $GL -ldl -o $O/prueba_yuv
$CXX $GI -I$H/inc/shim -I$H/inc/c3 -fPIE -pie $H/composer/composer.cpp $H/composer/entrada.cpp $O/dxguids.o $O/libaidl_graficos.a -lbinder_ndk $GL -ldl \
     -o $O/android.hardware.graphics.composer3-service.ranchu
ls -la $O/android.hardware.graphics.allocator-service.ranchu $O/mapper.ranchu.so $O/android.hardware.graphics.composer3-service.ranchu

# Audio Android: C y ALSA del sysroot, sin tinyalsa ni bibliotecas C++.
clang --target=aarch64-linux-android35 --sysroot="$SYS" -resource-dir="$RD" -mno-outline-atomics -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -fPIE -pie -fuse-ld=lld -rtlib=compiler-rt \
  "$H/../audio/sdaudio.c" -llog -o "$O/sdaudio"
