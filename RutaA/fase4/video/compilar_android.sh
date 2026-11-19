#!/bin/bash
# Pila de vídeo por GPU DENTRO de Android (VM, GPU-P con VIDEO_DECODE desbloqueado): libdrm + libva (estáticas),
# FFmpeg (estático, solo decodificadores con VA-API) y el driver VA de Mesa d3d12 (d3d12_drv_video.so).
# Fuente aparte de /opt/mesa (gráficos): copia del árbol con sus parches + uno propio (pantalla VA sin DRM).
#   wsl -u root -e bash /mnt/c/SurfaceDual/RutaA/fase4/video/compilar_android.sh
set -e
H=$(cd "$(dirname "$0")" && pwd); S=$(cd "$H/../../.." && pwd)
NDKT=$S/herramientas/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64; SYS=$NDKT/sysroot; RD=$NDKT/lib/clang/18
O=/opt/fase4/video; I=$O/inst; mkdir -p $O/src $I
export PATH=/root/venv/bin:$PATH PKG_CONFIG_LIBDIR=$I/lib/pkgconfig PKG_CONFIG_PATH=
CC="clang --target=aarch64-linux-android35 --sysroot=$SYS -resource-dir=$RD -mno-outline-atomics"
cat > $O/cruz.txt <<CRUZ
[binaries]
c = [$(for w in $CC; do printf "'%s', " $w; done)]
cpp = [$(for w in ${CC/clang/clang++} -stdlib=libc++; do printf "'%s', " $w; done)]
ar = 'llvm-ar'
strip = 'llvm-strip'
c_ld = 'lld'
cpp_ld = 'lld'
pkg-config = 'pkg-config'
[built-in options]
c_args = ['-include', 'sys/sysmacros.h']
cpp_args = ['-include', 'sys/sysmacros.h']
c_link_args = ['-rtlib=compiler-rt', '--unwindlib=libunwind']
cpp_link_args = ['-rtlib=compiler-rt', '--unwindlib=libunwind', '-static-libstdc++']
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8'
endian = 'little'
CRUZ

# libdrm (solo el núcleo: libva-drm lo enlaza; no hay DRM en la VM) y libva, estáticas.
if [ ! -f $I/lib/libdrm.a ]; then
  rm -rf $O/b-drm; meson setup $O/b-drm /opt/mesa/src/subprojects/libdrm-2.4.123 --cross-file $O/cruz.txt --prefix=$I --libdir=lib \
    -Ddefault_library=static -Dbuildtype=release -Dintel=disabled -Dradeon=disabled -Damdgpu=disabled -Dnouveau=disabled \
    -Dvmwgfx=disabled -Domap=disabled -Dexynos=disabled -Dfreedreno=disabled -Dtegra=disabled -Dvc4=disabled -Detnaviv=disabled \
    -Dcairo-tests=disabled -Dman-pages=disabled -Dvalgrind=disabled -Dtests=false -Dinstall-test-programs=false >/dev/null
  ninja -C $O/b-drm install >/dev/null
fi
if [ ! -f $I/lib/libva.a ]; then
  [ -d $O/src/libva ] || { mkdir -p $O/src/libva; curl -fsSL https://github.com/intel/libva/archive/refs/tags/2.22.0.tar.gz | tar xz --strip-components=1 -C $O/src/libva; }
  rm -rf $O/b-va; meson setup $O/b-va $O/src/libva --cross-file $O/cruz.txt --prefix=$I --libdir=lib -Ddefault_library=static \
    -Dbuildtype=release -Ddisable_drm=false -Dwith_x11=no -Dwith_glx=no -Dwith_wayland=no -Dwith_win32=no -Denable_docs=false \
    -Ddriverdir=/vendor/lib64/wsl/dri >/dev/null
  ninja -C $O/b-va install >/dev/null
fi

# FFmpeg: solo lo que usa el códec (decodificadores + VA-API) y la prueba (mov/matroska + file).
if [ ! -f $I/lib/libavcodec.a ]; then
  [ -d $O/src/ffmpeg ] || { mkdir -p $O/src/ffmpeg; curl -fsSL https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz | tar xJ --strip-components=1 -C $O/src/ffmpeg; }
  rm -rf $O/b-ff; mkdir -p $O/b-ff; cd $O/b-ff
  $O/src/ffmpeg/configure --prefix=$I --target-os=android --arch=aarch64 --enable-cross-compile --cc="$CC" --cxx="${CC/clang/clang++}" \
    --ld="$CC -fuse-ld=lld -rtlib=compiler-rt --unwindlib=none" --ar=llvm-ar --nm=llvm-nm --ranlib=llvm-ranlib --strip=llvm-strip \
    --pkg-config=pkg-config --enable-static --disable-shared --enable-pic --disable-programs --disable-doc --disable-debug \
    --disable-everything --disable-autodetect --enable-vaapi --enable-libdrm \
    --enable-decoder=h264,hevc,vp9,av1 --enable-hwaccel=h264_vaapi,hevc_vaapi,vp9_vaapi,av1_vaapi \
    --enable-parser=h264,hevc,vp9,av1 --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,vp9_superframe_split \
    --enable-demuxer=mov,matroska --enable-protocol=file --disable-avdevice --disable-avfilter --disable-swresample --disable-swscale \
    --extra-cflags="-O2" >$O/ff-configure.log 2>&1 || { tail -20 $O/ff-configure.log; exit 1; }
  make -j$(nproc) >/dev/null 2>$O/ff-make.log || { tail -30 $O/ff-make.log; exit 1; }
  make install >/dev/null
fi

# Mesa VA (d3d12) sin EGL/DRI: así meson construye el driver VA suelto (con DRI va dentro de libgallium_dri).
# Árbol de /opt/mesa con sus parches + pantalla sin DRM (null winsys -> GALLIUM_DRIVER=d3d12).
M=$O/mesa
if [ ! -f $M/src/meson.build ]; then
  mkdir -p $M; cp -a /opt/mesa/src $M/src
fi
cd $M/src
grep -q "SurfaceDual: sin DRM" src/gallium/auxiliary/vl/vl_winsys_drm.c || perl -0pi -e \
  's/(   if \(pipe_loader_drm_probe_fd\(&vscreen->dev, fd, false\)\)\n      vscreen->pscreen = pipe_loader_create_screen\(vscreen->dev, false\);\n)/$1   if (!vscreen->pscreen && !vscreen->dev && pipe_loader_sw_probe_null(&vscreen->dev))   \/\/ SurfaceDual: sin DRM (d3d12 por \/dev\/dxg)\n      vscreen->pscreen = pipe_loader_create_screen(vscreen->dev, false);\n/' \
  src/gallium/auxiliary/vl/vl_winsys_drm.c
grep -q "SurfaceDual: sin DRM" src/gallium/auxiliary/vl/vl_winsys_drm.c
grep -q "pipe_loader_sw" src/gallium/auxiliary/vl/vl_winsys_drm.c
grep -q '#include "pipe-loader/pipe_loader.h"' src/gallium/auxiliary/vl/vl_winsys_drm.c
# El cargador sw de Android no trae el winsys nulo (solo DRI/KMS); para VA sin pantalla basta con él.
grep -q "SurfaceDual: winsys nulo" src/gallium/auxiliary/pipe-loader/pipe_loader_sw.c || perl -0pi -e \
  's/#if !DETECT_OS_ANDROID\n(      \{\n         \.name = "null",)/#if 1   \/\/ SurfaceDual: winsys nulo también en Android (VA)\n$1/g' \
  src/gallium/auxiliary/pipe-loader/pipe_loader_sw.c
grep -q "SurfaceDual: winsys nulo" src/gallium/auxiliary/pipe-loader/pipe_loader_sw.c
if [ ! -f $M/build/build.ninja ]; then
  meson setup $M/build --cross-file $O/cruz.txt --force-fallback-for=libdrm,directx-headers -Dbuildtype=release -Dplatforms=android \
    -Dandroid-stub=true -Dplatform-sdk-version=35 -Dgallium-drivers=d3d12 -Dvulkan-drivers= -Degl=disabled -Dgles2=disabled -Dopengl=false \
    -Dgles1=disabled -Dglx=disabled -Dgbm=disabled -Dllvm=disabled -Dshared-llvm=disabled -Dzstd=disabled -Dexpat=disabled \
    -Dvalgrind=disabled -Dlibunwind=disabled -Dandroid-libbacktrace=disabled -Dallow-fallback-for=libdrm -Dlibdrm:amdgpu=disabled \
    -Dlibdrm:radeon=disabled -Dlibdrm:nouveau=disabled -Dlibdrm:intel=disabled -Dlibdrm:freedreno=disabled -Dlibdrm:vc4=disabled \
    -Dlibdrm:etnaviv=disabled -Dlibdrm:exynos=disabled -Dlibdrm:omap=disabled -Dlibdrm:tegra=disabled -Dlibdrm:vmwgfx=disabled \
    -Dlibdrm:tests=false -Dgallium-va=enabled -Dgallium-d3d12-video=enabled -Dvideo-codecs=h264dec,h265dec,vp9dec,av1dec \
    >$O/mesa-setup.log 2>&1 || { tail -30 $O/mesa-setup.log; exit 1; }
fi
ninja -C $M/build src/gallium/targets/va/libgallium_drv_video.so 2>&1 | tail -5
cp $M/build/src/gallium/targets/va/libgallium_drv_video.so $O/d3d12_drv_video.so
ls -la $O/d3d12_drv_video.so $I/lib/*.a

# Prueba vprueba (va a /data/local/tmp junto a d3d12_drv_video.so y libva.so; ver su cabecera).
$CC -O2 -fPIE -pie -I$I/include "$H/vprueba.c" -L$I/lib -lavformat -lavcodec -lavutil -lva -lva-drm -ldrm -lz -lm -ldl -llog \
   -fuse-ld=lld -rtlib=compiler-rt --unwindlib=none -o $O/vprueba
ls -la $O/vprueba
