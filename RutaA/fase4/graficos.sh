#!/bin/bash
# Fase 4: gráficos nativos para el AOSP de la VM de Hyper-V. Mesa d3d12 como EGL de Android, D3D12/DXCore de Microsoft
# y el UMD de Qualcomm, adaptados a Bionic (fase 1). Van en su propia partición erofs "sdgraficos", montada en
# /vendor/lib64/wsl (el vendor de super tiene 68 MB fijos), con enlaces desde /vendor/lib64 y /vendor/lib64/egl.
# Uso (WSL root, lo llama preparar.sh):  graficos.sh <dir del vendor> <salida.erofs>
set -e
V=$1; SAL=$2; F=$(cd "$(dirname "$0")" && pwd); RA=$(dirname "$F"); R=/tmp/rutaa; M=/opt/mesa/build
G=/opt/fase4/graficos; rm -rf $G; mkdir -p $G
bash "$RA/fase1/correr.sh" >/dev/null   # libs glibc adaptadas a Bionic y la capa libglibcshim (con las sd_*)

# Autocomprobación de la redirección de rutas del shim (misma función que compila correr.sh).
{ echo '#include <stdio.h>'; echo '#include <stdlib.h>'; echo '#include <string.h>'; echo '#include <limits.h>'; echo '#include <assert.h>'
  sed -n '/^static const char \*ruta_wsl/,/^}/p' "$RA/fase1/glibcshim.c"
  cat <<'C'
int main(void) { char b[PATH_MAX];
  assert(!strcmp(ruta_wsl("/usr/lib/wsl/drivers/qcdx.inf_x/libqc.so", b), "/vendor/lib64/wsl/libqc.so"));
  assert(!strcmp(ruta_wsl("/usr/lib/wsl/drivers/qcdx.inf_x", b), "/vendor/lib64/wsl"));
  assert(!strcmp(ruta_wsl("/usr/lib/wsl/lib/libd3d12.so", b), "/vendor/lib64/wsl/libd3d12.so"));
  assert(!strcmp(ruta_wsl("/usr/lib/wsl", b), "/vendor/lib64/wsl"));
  assert(!strcmp(ruta_wsl("/dev/dxg", b), "/dev/dxg")); return 0; }
C
} > /tmp/prueba_ruta.c
clang -o /tmp/prueba_ruta /tmp/prueba_ruta.c && /tmp/prueba_ruta

UMD=$(dirname "$(find /usr/lib/wsl/drivers -name libqcdx12arm64wslum.so | head -1)")
for n in libglibcshim.so libdxcore.so libd3d12.so libd3d12core.so libstdc++.so.6 libgcc_s.so.1; do cp $R/system/lib64/$n $G/; done
for n in libqcdx12arm64wslum.so qcgpuarm64compilercore.so; do cp "$R$UMD/$n" $G/; done
basename "$UMD" > $G/version_umd.txt   # si Windows actualiza el driver de la GPU, hay que regenerar (UMD y KMD deben casar)
# Las libs de Microsoft/Qualcomm piden rutas /usr/lib/wsl/... (las da el host): se desvían a las sd_* del shim.
# sigaction: struct de glibc distinto del de Bionic (lo traduce el shim; lo usa el compilador de shaders).
printf '%s\n' "dlopen sd_dlopen" "fopen sd_fopen" "open sd_open" "access sd_access" "stat sd_stat" "lstat sd_lstat" \
  "opendir sd_opendir" "readlink sd_readlink" "realpath sd_realpath" "sigaction sd_sigaction" > /tmp/sd_simbolos.txt
for n in libdxcore.so libd3d12.so libd3d12core.so libqcdx12arm64wslum.so qcgpuarm64compilercore.so; do
  patchelf --rename-dynamic-symbols /tmp/sd_simbolos.txt $G/$n
done
cp $M/src/egl/libEGL.so $G/libEGL_mesa.so; cp $M/src/mapi/es2api/libGLESv2.so $G/libGLESv2_mesa.so
cp $M/src/gallium/targets/dri/libgallium_dri.so $G/
# libd3d12/libd3d12core traen RUNPATH "$$$$ORIGIN:$ORIGIN" (el enlazador de Bionic avisa en cada carga); sobran.
for n in libEGL_mesa.so libGLESv2_mesa.so libgallium_dri.so libd3d12.so libd3d12core.so; do patchelf --remove-rpath $G/$n; done
patchelf --set-soname libEGL_mesa.so $G/libEGL_mesa.so; patchelf --set-soname libGLESv2_mesa.so $G/libGLESv2_mesa.so
for n in $(ls $G); do setfattr -n security.selinux -v u:object_r:same_process_hal_file:s0 $G/$n; done
setfattr -n security.selinux -v u:object_r:same_process_hal_file:s0 $G
# Gralloc propio sobre D3D12 (hal/gralloc): reemplaza al allocator y al mapper "ranchu" (memoria de QEMU).
bash "$F/hal/compilar.sh" >/dev/null
AL=android.hardware.graphics.allocator-service.ranchu; HWC=android.hardware.graphics.composer3-service.ranchu
cp /opt/fase4/hal/$AL /opt/fase4/hal/$HWC /opt/fase4/hal/mapper.ranchu.so $G/
setfattr -n security.selinux -v u:object_r:same_process_hal_file:s0 $G/mapper.ranchu.so
setfattr -n security.selinux -v u:object_r:hal_graphics_allocator_default_exec:s0 $G/$AL
# Composer propio (hal/composer): pantalla real y envío al Visor por vsock (RutaA/fase4/pantalla_protocolo.md).
setfattr -n security.selinux -v u:object_r:hal_graphics_composer_default_exec:s0 $G/$HWC
cp /opt/fase4/hal/sdaudio $G/
setfattr -n security.selinux -v u:object_r:hal_graphics_composer_default_exec:s0 $G/sdaudio
# Vídeo por la GPU de la VM (Codec2 c2.surfacedual.*): driver VA de Mesa d3d12, libva y el servicio, que sustituye al
# de goldfish (mismo .rc, misma instancia "default" del manifiesto; dominio mediacodec).
bash "$F/video/compilar_android.sh" >/dev/null; bash "$F/hal/codec2/compilar.sh" >/dev/null 2>&1
C2=android.hardware.media.c2-service.surfacedual
cp /opt/fase4/video/d3d12_drv_video.so /opt/fase4/video/inst/lib/libva.so /opt/fase4/video/inst/lib/libva-drm.so $G/
for n in d3d12_drv_video.so libva.so libva-drm.so; do setfattr -n security.selinux -v u:object_r:same_process_hal_file:s0 $G/$n; done
mkfs.erofs -zlz4hc,12 -b4096 -T1230768000 --mount-point=/vendor/lib64/wsl $SAL $G >/dev/null
ls -la $SAL

# En el vendor: punto de montaje, enlaces donde Android busca (sphal: /vendor/lib64 y /vendor/lib64/egl) y etiquetas.
mkdir -p $V/lib64/wsl; setfattr -n security.selinux -v u:object_r:same_process_hal_file:s0 $V/lib64/wsl
for n in libglibcshim.so libdxcore.so libd3d12.so libd3d12core.so libstdc++.so.6 libgcc_s.so.1 libgallium_dri.so libva.so libva-drm.so; do
  ln -sfn wsl/$n $V/lib64/$n; setfattr -h -n security.selinux -v u:object_r:same_process_hal_file:s0 $V/lib64/$n
done
ln -sfn ../../lib64/wsl/$HWC $V/bin/hw/$HWC; setfattr -h -n security.selinux -v u:object_r:hal_graphics_composer_default_exec:s0 $V/bin/hw/$HWC
ln -sfn ../../lib64/wsl/$AL $V/bin/hw/$AL; setfattr -h -n security.selinux -v u:object_r:hal_graphics_allocator_default_exec:s0 $V/bin/hw/$AL
C2G=android.hardware.media.c2-service-goldfish   # sin hwcodec de QEMU no ofrecía nada; su ruta la usan el .rc y SELinux
# En el vendor, no enlazado a wsl/: el enlazador elige la sección de ld.config por la ruta real (en wsl/ no ve libcodec2_vndk).
cp /opt/fase4/hal/codec2/$C2 $V/bin/hw/$C2G; setfattr -n security.selinux -v u:object_r:mediacodec_exec:s0 $V/bin/hw/$C2G
cp "$F/hal/codec2/seccomp.policy" $V/etc/seccomp_policy/surfacedual-c2.policy   # lo carga main() (SetUpMinijail)
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/seccomp_policy/surfacedual-c2.policy
# Lista de códecs: fuera las entradas de goldfish (sin hwcodec de QEMU no existen) y media_codecs_google_video.xml (que solo
# traía goldfish) pasa a ser la nuestra, con límites y rendimiento medidos. Rango 0x100 del almacén < 0x200 de c2.android.*.
perl -0pi -e 's{\s*<MediaCodec name="c2\.goldfish\.[^"]*".*?</MediaCodec>}{}gs' $V/etc/media_codecs.xml
! grep -q goldfish $V/etc/media_codecs.xml
cp "$F/hal/codec2/media_codecs_surfacedual.xml" $V/etc/media_codecs_google_video.xml
for x in media_codecs.xml media_codecs_google_video.xml; do setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/$x; done
ln -sfn ../wsl/mapper.ranchu.so $V/lib64/hw/mapper.ranchu.so; setfattr -h -n security.selinux -v u:object_r:same_process_hal_file:s0 $V/lib64/hw/mapper.ranchu.so
for n in libEGL_mesa.so libGLESv2_mesa.so; do
  ln -sfn ../wsl/$n $V/lib64/egl/$n; setfattr -h -n security.selinux -v u:object_r:same_process_hal_file:s0 $V/lib64/egl/$n
done
# El .rc del emulador fija ro.hardware.egl=emulation (libEGL de gfxstream); aquí, Mesa.
sed -i 's|setprop ro.hardware.egl \${ro.boot.hardwareegl:-emulation}|setprop ro.hardware.egl mesa|' $V/etc/init/hw/init.ranchu.rc
grep -q 'setprop ro.hardware.egl mesa' $V/etc/init/hw/init.ranchu.rc
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/init/hw/init.ranchu.rc
# Audio comparte el dominio del composer para reutilizar su acceso a vsock.
cat > $V/etc/init/vendor.sdaudio.rc <<'RC'
service vendor.sdaudio /vendor/lib64/wsl/sdaudio
    class hal
    user system
    group audio
RC
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/init/vendor.sdaudio.rc
# Densidad que SurfaceFlinger exige (pantalla 2880x1920 a 267 dpi).
printf '\n# SurfaceDual Hyper-V\nro.sf.lcd_density=320\nro.surface_flinger.max_frame_buffer_acquired_buffers=3\n' >> $V/build.prop
# /dev/dxg nace 0600 root: como cualquier GPU de Android (kgsl, mali), lo abren todas las apps; SELinux limita por gpu_device.
printf '\n# GPU de Hyper-V (dxgkrnl) y fences de D3D12 como sync_file (RutaA/fase4/sdsync_contrato.md)\n/dev/dxg                  0666   root       root\n/dev/sdsync               0666   root       root\n' >> $V/etc/ueventd.rc
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/ueventd.rc
cat >> $V/etc/selinux/vendor_file_contexts <<'FC'
/(vendor|system/vendor)/lib(64)?/wsl(/.*)?   u:object_r:same_process_hal_file:s0
/(vendor|system/vendor)/lib64/wsl/android\.hardware\.graphics\.allocator-service\.ranchu   u:object_r:hal_graphics_allocator_default_exec:s0
/(vendor|system/vendor)/lib64/wsl/android\.hardware\.graphics\.composer3-service\.ranchu   u:object_r:hal_graphics_composer_default_exec:s0
/(vendor|system/vendor)/lib64/wsl/sdaudio   u:object_r:hal_graphics_composer_default_exec:s0
/dev/dxg                     u:object_r:gpu_device:s0
/dev/sdsync                  u:object_r:gpu_device:s0
FC
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/selinux/vendor_file_contexts
