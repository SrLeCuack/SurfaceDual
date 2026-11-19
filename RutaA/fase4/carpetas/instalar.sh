#!/bin/bash
# Carpetas de Windows en Android (sdcarpetas.c + host/HvCarpetas.cs): compila sdcarpetas (NDK, como sdaudio) y lo instala en el vendor $1 con su .rc.
# Lo llama preparar.sh; la política SELinux está en ../sepolicy/carpetas.cil.
set -e
F=$(cd "$(dirname "$0")" && pwd); S=$(cd "$F/../../.." && pwd); V=$1
NDKT=$S/herramientas/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64
clang --target=aarch64-linux-android35 --sysroot="$NDKT/sysroot" -resource-dir="$NDKT/lib/clang/18" -mno-outline-atomics -std=c11 \
  -O2 -Wall -Wextra -fPIE -pie -fuse-ld=lld -rtlib=compiler-rt "$F/sdcarpetas.c" -o "$V/bin/sdcarpetas"
setfattr -n security.selinux -v u:object_r:sdcarpetas_exec:s0 "$V/bin/sdcarpetas"
# Arranca con /data/media/0 ya descifrado (FBE); HvCarpetas.exe se conecta cuando quiera (sdcarpetas solo escucha).
cat > "$V/etc/init/sdcarpetas.rc" <<'RC'
service sdcarpetas /vendor/bin/sdcarpetas
    class main
    user root
    group root media_rw
    disabled
    seclabel u:r:sdcarpetas:s0
    stdio_to_kmsg

on property:sys.user.0.ce_available=true
    start sdcarpetas
RC
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 "$V/etc/init/sdcarpetas.rc"
grep -q sdcarpetas "$V/etc/selinux/vendor_file_contexts" ||
  echo '/(vendor|system/vendor)/bin/sdcarpetas   u:object_r:sdcarpetas_exec:s0' >> "$V/etc/selinux/vendor_file_contexts"
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 "$V/etc/selinux/vendor_file_contexts"
