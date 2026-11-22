#!/bin/bash
# Fase 1 de la ruta A: ¿puede un proceso Bionic usar D3D12 (y el driver de Qualcomm) por /dev/dxg?
# Se corre en WSL como root:  wsl -u root -e bash /mnt/c/SurfaceDual/RutaA/fase1/correr.sh
set -e
F1=$(cd "$(dirname "$0")" && pwd); RA=$(dirname "$F1")
SYS=$RA/../herramientas/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/sysroot
# UMD: el del controlador que Windows usa AHORA. Tras actualizarlo quedan dos en el DriverStore y "find | head -1" daba el
# viejo (22/11: 31.0.137 y 31.0.178). Se le pregunta a Windows (interop); si no responde, el más nuevo. graficos.sh usa este.
R=/tmp/rutaa
A=$(/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -Command '(Get-CimInstance Win32_VideoController | Where-Object Name -match Adreno).InstalledDisplayDrivers' 2>/dev/null | grep -o 'qcdx[^\]*' | head -1)
if [ -n "$A" ] && [ -f "/usr/lib/wsl/drivers/$A/libqcdx12arm64wslum.so" ]; then UMD=/usr/lib/wsl/drivers/$A
else UMD=$(dirname "$(ls -td /usr/lib/wsl/drivers/*/libqcdx12arm64wslum.so | head -1)"); fi   # shortcut: tras revertir el controlador sin interop elegiría el nuevo; con interop (lo normal) no pasa
[ -f "$UMD/libqcdx12arm64wslum.so" ] || { echo "ERROR: no se encontró el controlador de la GPU en /usr/lib/wsl/drivers"; exit 1; }
command -v patchelf >/dev/null || DEBIAN_FRONTEND=noninteractive apt-get install -y -qq patchelf >/dev/null
CC="clang --target=aarch64-linux-android35 --sysroot=$SYS -fuse-ld=lld -O2 -mno-outline-atomics -nodefaultlibs -lc -ldl -lm"

umount -l $R/dev $R/proc $R/umd_original 2>/dev/null || true; rm -rf $R; mkdir -p $R/system/bin $R/system/lib64 $R/dev $R/proc $R/tmp "$R$UMD" $R/umd_original
echo "$UMD" > $R/umd_ruta.txt
$CC -shared -fPIC -fvisibility=hidden -Wl,-soname,libglibcshim.so "$F1/glibcshim.c" -o $R/system/lib64/libglibcshim.so
$CC -fPIE -pie "$F1/poc.c" -o $R/system/bin/poc
cp "$RA"/bionic/linker64 $R/system/bin/; cp "$RA"/bionic/*.so $R/system/lib64/

# glibc -> Bionic: NEEDED a la capa y a las libm/libdl de Bionic; sin versiones de símbolo (Bionic no tiene GLIBC_2.x).
adaptar() {   # $1 = .so glibc ya copiada
  patchelf --replace-needed libc.so.6 libglibcshim.so --replace-needed libpthread.so.0 libglibcshim.so \
           --replace-needed libdl.so.2 libdl.so --replace-needed libm.so.6 libm.so "$1"
  # ld.so: renombrar (no quitar); su entrada de verneed debe seguir en DT_NEEDED para el enlazador de Bionic.
  patchelf --replace-needed ld-linux-aarch64.so.1 libdl.so "$1" 2>/dev/null || true
  # Todas (también las definidas): Bionic no resuelve las autorreferencias con versión GLIBCXX/GLIBC.
  nm -D "$1" | awk '/@/{print $NF}' | sed 's/@.*//' | sort -u > /tmp/versiones.txt
  python3 "$F1/unicos_a_globales.py" "$1"
  xargs -a /tmp/versiones.txt -n 400 sh -c 'a=""; for s; do a="$a --clear-symbol-version $s"; done; patchelf $a "$0"' "$1"
}
for f in /usr/lib/wsl/lib/libdxcore.so /usr/lib/wsl/lib/libd3d12.so /usr/lib/wsl/lib/libd3d12core.so \
         "$(readlink -f /usr/lib/aarch64-linux-gnu/libstdc++.so.6)" /usr/lib/aarch64-linux-gnu/libgcc_s.so.1; do
  n=$(basename "$f"); case $n in libstdc++.so.6.*) n=libstdc++.so.6;; esac
  cp "$f" $R/system/lib64/$n; adaptar $R/system/lib64/$n
done
# Driver de Qualcomm: se carga por ruta absoluta del almacén de drivers; misma ruta dentro del chroot.
mount --bind "$UMD" $R/umd_original
for f in "$UMD"/*; do ln -sf /umd_original/$(basename "$f") "$R$UMD/"; done
for n in libqcdx12arm64wslum.so qcgpuarm64compilercore.so; do rm -f "$R$UMD/$n"; cp "$UMD/$n" "$R$UMD/$n"; adaptar "$R$UMD/$n"; done
mkdir -p $R/usr/lib/wsl/lib; ln -sf /system/lib64/libdxcore.so /system/lib64/libd3d12.so /system/lib64/libd3d12core.so $R/usr/lib/wsl/lib/
mount --bind /dev $R/dev; mount -t proc proc $R/proc
echo "== ejecutando poc (Bionic)"
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin LD_LIBRARY_PATH=/system/lib64 /usr/sbin/chroot $R /system/bin/poc 2>&1 || echo "poc salió con $?" ; true
