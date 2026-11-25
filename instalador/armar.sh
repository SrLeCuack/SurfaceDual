#!/bin/bash
# Fase 3 del instalador ("Imagen", F7/plan/instalador_diseno.md): en una distro WSL Ubuntu 24.04 ARM64 recién importada, como
# root, deja todo lo que necesita RutaA/fase4/preparar.sh sin nada del equipo de origen y lo lanza (arranque.vhdx + android.vhdx).
#   SD_LICENCIA_GOOGLE=ACEPTO bash armar.sh [raíz]   raíz = carpeta de instalación (por omisión, la de encima de instalador/)
#   bash armar.sh --simular [raíz]                     solo dice qué pasos haría y cuáles ya están hechos; no cambia nada
#   bash armar.sh --prueba <dir> [raíz]               solo c) y d) hacia <dir> con el zip local y compara con lo de este equipo
# Variables: SD_REHACER=1 rehace android.vhdx de fábrica (borra los datos de Android); SD_ADB_DIR = carpeta .android de Windows.
set -euo pipefail
MODO=real P=
case "${1:-}" in --simular) MODO=simular; shift;; --prueba) MODO=prueba; P=${2:?falta el directorio de prueba}; shift 2;; esac
R=$(cd "${1:-$(dirname "$0")/..}" && pwd); D=$R/instalador
IMGZ=$R/F2/imagenes/arm64-v8a-37.0_r08.zip; IMG=$R/F2/imagenes/android-37/arm64-v8a
IMG_URL=https://dl.google.com/android/repository/sys-img/google_apis_playstore/arm64-v8a-37.0_r08.zip
IMG_TAM=2166618231 IMG_SHA=ac251e27178d78cffb0b7ee577d79643491a89a6
NDK_URL=https://dl.google.com/android/repository/android-ndk-r27c-linux.zip
NDK_TAM=663987688 NDK_SHA=090e8083a715fdb1a3e402d0763c388abb03fb4e
NDK=$R/herramientas/ndk; NDKP=android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64
INITRD=$R/F2/build/avd/initrd_whpx_base; VSD=/root/vsd/src; PUB=/root/.android/adbkey.pub; F4=$R/RutaA/fase4
t() { if [ "${SD_IDIOMA:-es}" = en ]; then echo "$2"; else echo "$1"; fi; }   # idioma elegido en el instalador (SD_IDIOMA=es|en)
[ "$(id -u)" = 0 ] || { t "ERROR: hay que ejecutarlo como root (wsl -u root)" "ERROR: this must run as root (wsl -u root)"; exit 1; }

paso() {   # paso <letra> <qué hace> <condición de "ya hecho"> <función>
  if eval "$3"; then t "[$1] $2: ya estaba hecho" "[$1] $2: already done"; return; fi
  if [ "$MODO" = simular ]; then t "[$1] $2: se haría ahora" "[$1] $2: would be done now"; return; fi
  echo "[$1] $2..."; "$4"; t "[$1] $2: listo" "[$1] $2: done"
}
licencia() { [ "${SD_LICENCIA_GOOGLE:-}" = ACEPTO ] || { t "ERROR: sin aceptar la licencia del SDK de Android (SD_LICENCIA_GOOGLE=ACEPTO) no se descarga nada de Google" "ERROR: nothing is downloaded from Google unless the Android SDK license is accepted (SD_LICENCIA_GOOGLE=ACEPTO)"; exit 1; }; }
verificar() { [ "$(stat -c %s "$1" 2>/dev/null)" = "$2" ] && [ "$(sha1sum < "$1" | cut -d' ' -f1)" = "$3" ]; }
descargar() {   # url destino tamaño sha1: reanuda lo ya bajado (.part) y solo deja el archivo final si casa
  verificar "$2" "$3" "$4" && return; mkdir -p "$(dirname "$2")"
  verificar "$2.part" "$3" "$4" || curl -fL --retry 5 -C - -o "$2.part" "$1"
  verificar "$2.part" "$3" "$4" || { rm -f "$2.part"; t "ERROR: $2 llegó dañado (tamaño o SHA-1); vuelva a ejecutar" "ERROR: $2 arrived damaged (size or SHA-1); run it again"; exit 1; }
  mv "$2.part" "$2"
}
extraer() { python3 -I -c 'import sys, zipfile; z = zipfile.ZipFile(sys.argv[1]); p = tuple(sys.argv[3:])
z.extractall(sys.argv[2], [n for n in z.namelist() if n.startswith(p)] if p else None)' "$@"; }   # zip destino [prefijos...]
lp() {   # offset y tamaño de una partición lógica de super en system.img
  python3 -I "$F4/lp_particiones.py" "$IMG/system.img" $(( $(sgdisk -i 2 "$IMG/system.img" | awk '/First sector/{print $3}') * 512 )) |
    awk -v p="$1" '$1 == p {print $2, $3}'
}
montar() { umount -q "$4" 2>/dev/null || true; mkdir -p "$4"; mount -t erofs -o loop,ro,offset="$2",sizelimit="$3" "$1" "$4"; }

# a) Paquetes: los que piden preparar.sh, graficos.sh, fase1/correr.sh, los HAL, codec2, vídeo (libva y FFmpeg se recompilan
# siempre/aquí), carpetas, lp_particiones.py, bootconfig.py y avbtool.py; más curl/zstd/openssl de este script.
PAQ="lz4 cpio dosfstools gdisk mtools qemu-utils grub-efi-arm64-bin attr erofs-utils e2fsprogs util-linux python3 openssl
     curl ca-certificates xz-utils zstd clang lld llvm binutils libc6-dev patchelf perl make pkg-config meson ninja-build"
apt_hecho() { [ "$(dpkg-query -W -f='${Status}\n' $PAQ 2>/dev/null | grep -c 'ok installed')" = "$(echo $PAQ | wc -w)" ]; }
apt_haz() { apt-get update -qq; DEBIAN_FRONTEND=noninteractive apt-get install -y -qq $PAQ >/dev/null; }

# g) Clave adb de ESTE equipo. preparar.sh mete la pública en el bootconfig; la privada solo la usa adb.exe de Windows, que
# la busca en %USERPROFILE%\.android (si la persona ya usaba adb, se reutiliza la suya).
clave_publica() {   # formato de adb (RSAPublicKey de Android): 64 palabras, n0inv = -1/n mod 2^32, n y R^2 mod n en little endian, e
  [[ $(openssl rsa -in "$1" -noout -text 2>/dev/null) == *"publicExponent: 65537 "* ]] || { t "ERROR: $1 no es una clave RSA de adb" "ERROR: $1 is not an adb RSA key"; exit 1; }
  openssl rsa -in "$1" -noout -modulus | python3 -I -c 'import base64, struct, sys
n = int(sys.stdin.read().split("=")[1], 16); assert n.bit_length() == 2048, "adb exige RSA-2048"
r = struct.pack("<II", 64, -pow(n, -1, 1 << 32) % (1 << 32)) + n.to_bytes(256, "little") + pow(2, 4096, n).to_bytes(256, "little") + struct.pack("<I", 65537)
assert len(r) == 524 and struct.unpack_from("<I", r, 4)[0] * n % (1 << 32) == (1 << 32) - 1
print(base64.b64encode(r).decode() + " " + sys.argv[1])' "$2"
}
adb_haz() {
  local WA; WA=${SD_ADB_DIR:-$(wslpath -u "$(cd /mnt/c && cmd.exe /d /c 'echo %USERPROFILE%' | tr -d '\r')")/.android}
  [ -d "$(dirname "$WA")" ] || { t "ERROR: no encuentro la carpeta de usuario de Windows ($WA)" "ERROR: the Windows user folder was not found ($WA)"; exit 1; }
  mkdir -p "$WA" /root/.android
  # Solo la clave que crea Surface Dual lleva "surfacedual@armado" (el desinstalador borra esa y ninguna otra; Codex).
  local tag=surfacedual@armado; [ -f "$WA/adbkey" ] && tag=surfacedual@suya || openssl genrsa -out "$WA/adbkey" 2048 2>/dev/null
  [ -s "$WA/adbkey.pub" ] || clave_publica "$WA/adbkey" $tag > "$WA/adbkey.pub"
  install -m 0644 "$WA/adbkey.pub" $PUB
}

# b) Imagen de Google (con Play): la licencia la acepta la persona en el instalador de Windows.
imagen_haz() { licencia; descargar $IMG_URL "$IMGZ" $IMG_TAM $IMG_SHA; extraer "$IMGZ" "$(dirname "$IMG")"; echo $IMG_SHA > "$IMG/.sd_extraido"; }

# c) Ramdisk base sin bootconfig (instalador/initrd_base.py), con la clave adb de este equipo.
initrd_haz() {
  mkdir -p "$(dirname "$INITRD")"
  lz4 -dc "$IMG/ramdisk.img" | python3 -I "$D/initrd_base.py" $PUB | lz4 -l -9 -q > "$INITRD.tmp"; mv "$INITRD.tmp" "$INITRD"
}

# d) Vendor del emulador sin UWB y con la radio deshabilitada (receta de F4/rev_vendor_sd_codex.md), con sus etiquetas SELinux.
vendor_haz() {
  local V M; V=$(lp vendor); M=$(dirname "$VSD")/mnt
  [ "$V" = "734003200 68796416" ] || { t "ERROR: vendor en '$V' de super; preparar.sh espera 734003200 68796416" "ERROR: vendor at '$V' in super; preparar.sh expects 734003200 68796416"; exit 1; }
  montar "$IMG/system.img" $V "$M"; rm -rf "$VSD.tmp"; cp -a --preserve=all "$M" "$VSD.tmp"; umount "$M"
  rm "$VSD.tmp/etc/permissions/android.hardware.uwb.xml"
  sed -i '1a\    disabled' "$VSD.tmp/etc/init/android.hardware.radio.ranchu.rc"
  setfattr -h -n security.selinux -v u:object_r:vendor_configs_file:s0 "$VSD.tmp/etc/init/android.hardware.radio.ranchu.rc"   # sed -i la borra
  # Tablet solo Wi-Fi: sin funciones de telefonía (ESTADO.md, "Telefonía quitada del vendor").
  local H=$VSD.tmp/etc/permissions/handheld_core_hardware.xml E; E=$(getfattr --absolute-names --only-values -n security.selinux "$H")
  sed -i '/<feature name="android.hardware.telephony/d' "$H"; setfattr -n security.selinux -v "$E" "$H"
  mv "$VSD.tmp" "$VSD"
}

# e) Bionic de la propia imagen (fase1/correr.sh): linker64 y libc/libdl/libm/libdl_android del APEX com.android.runtime
# (ext4 dentro del .apex, que es un zip), ld-android y libz de /system/lib64. Mismos SHA-1 que RutaA/bionic (medido 18/11).
BIONIC="linker64 libc.so libdl.so libm.so libdl_android.so ld-android.so libz.so"
bionic_hecho() { local f; for f in $BIONIC; do [ -s "$R/RutaA/bionic/$f" ] || return 1; done; }
bionic_haz() {
  local T=/tmp/sd_bionic; montar "$IMG/system.img" $(lp system) $T/sys
  extraer $T/sys/system/apex/com.android.runtime.apex $T apex_payload.img
  mkdir -p $T/rt "$R/RutaA/bionic"; mount -o loop,ro $T/apex_payload.img $T/rt
  cp $T/rt/bin/linker64 $T/rt/lib64/bionic/{libc,libdl,libm,libdl_android}.so $T/sys/system/lib64/{ld-android,libz}.so "$R/RutaA/bionic/"
  umount $T/rt $T/sys; rm -rf $T
}

# f) NDK r27c (solo el sysroot y las bibliotecas de clang: los scripts compilan con el clang de Ubuntu) y siembra de
# precompilados propios (instalador/paquete.txt, sección [wsl]).
ndk_haz() {
  licencia; descargar $NDK_URL "$NDK/android-ndk-r27c-linux.zip" $NDK_TAM $NDK_SHA
  extraer "$NDK/android-ndk-r27c-linux.zip" "$NDK" $NDKP/sysroot/ $NDKP/lib/clang/
  touch "$NDK/android-ndk-r27c/.sd_extraido"; rm "$NDK/android-ndk-r27c-linux.zip"
}
VA=/opt/fase4/video/mesa/build
siembra_haz() {
  [ -f "$R/precompilados.tar.zst" ] || { t "ERROR: falta $R/precompilados.tar.zst (lo crea empaquetar.ps1)" "ERROR: $R/precompilados.tar.zst is missing (empaquetar.ps1 creates it)"; exit 1; }
  zstd -dcq "$R/precompilados.tar.zst" | tar -x -C /
  # video/compilar_android.sh llama siempre a ninja en el árbol de Mesa VA; el real lleva rutas de la máquina de origen y
  # aquí recompilaría contra un NDK que no existe. Con el driver ya sembrado como salida "phony", ninja no tiene trabajo.
  # shortcut: no recompila el driver VA; para hacerlo en destino, borrar $VA y sembrar /opt/fase4/video/mesa/src completo.
  [ -f $VA/build.ninja ] || printf 'build src/gallium/targets/va/libgallium_drv_video.so: phony\n' > $VA/build.ninja
  touch /opt/.sd_sembrado
}

# h) Armado. No pisa un android.vhdx existente (son los datos de Android) salvo SD_REHACER=1.
armado_hecho() { [ -f "$F4/arranque.vhdx" ] && [ -f "$F4/android.vhdx" ] && [ "${SD_REHACER:-0}" != 1 ]; }
armado_haz() {
  [ ! -f "$F4/android.vhdx" ] || [ "${SD_REHACER:-0}" = 1 ] ||
    { t "ERROR: ya hay un android.vhdx (datos de Android); no se toca. SD_REHACER=1 lo rehace de fábrica" "ERROR: an android.vhdx (Android data) already exists; it is left alone. SD_REHACER=1 rebuilds it from factory"; exit 1; }
  # Producción (19/11): no depurable, adb solo con la clave de este equipo (la de g), sin consola serie. Instalar el launcher,
  # ponerlo de inicio y añadir el botón por adb (instalar.ps1, fase 5) funciona igual como usuario shell: probado 19/11.
  FABRICA=1 VERIFICADO=0 PRODUCCION=1 bash "$F4/preparar.sh"
  [ -f "$F4/arranque.vhdx" ] && [ -f "$F4/android.vhdx" ] || { t "ERROR: preparar.sh terminó sin arranque.vhdx y android.vhdx" "ERROR: preparar.sh finished without arranque.vhdx and android.vhdx"; exit 1; }
}

if [ "$MODO" = prueba ]; then   # en seco: zip local, sin red, todo dentro de $P; compara con lo que ya hay en este equipo
  mkdir -p "$P"; P=$(cd "$P" && pwd); IMG=$P/imagen/arm64-v8a; INITRD=$P/initrd_whpx_base; VSD=$P/vsd/src
  verificar "$IMGZ" $IMG_TAM $IMG_SHA && echo "zip: tamaño y SHA-1 correctos" || { echo "zip: NO casa"; exit 1; }
  extraer "$IMGZ" "$P/imagen" arm64-v8a/ramdisk.img arm64-v8a/system.img
  rm -rf "$INITRD" "$VSD"; paso c "initrd_whpx_base" false initrd_haz; paso d "$(t "vendor en $VSD" "vendor in $VSD")" false vendor_haz
  rm -rf "$P/rd_nuevo" "$P/rd_actual"; mkdir -p "$P/rd_nuevo" "$P/rd_actual"
  cmp -s "$INITRD" "$R/F2/build/avd/initrd_whpx_base" && echo "initrd: bytes idénticos" || echo "initrd: bytes distintos (cmp)"
  (cd "$P/rd_nuevo" && lz4 -dc "$INITRD" | cpio -id --quiet); (cd "$P/rd_actual" && { lz4 -dc "$R/F2/build/avd/initrd_whpx_base" 2>/dev/null | cpio -id --quiet; } || true)
  echo "--- lo que extrae preparar.sh (actual frente a nuevo):"; diff -r --no-dereference "$P/rd_actual" "$P/rd_nuevo" || true
  ficha() { (cd "$1" && find . -printf '%p %y %m %U %G %l\n' | sort); }
  diff <(ficha "$P/rd_actual") <(ficha "$P/rd_nuevo") || true
  echo "--- vendor (/root/vsd/src frente a nuevo):"; diff -r --no-dereference /root/vsd/src "$VSD" && echo "contenido idéntico"
  xattrs() { (cd "$1" && find . -print0 | sort -z | xargs -0 getfattr -h -d -m - --absolute-names 2>/dev/null); }
  diff <(ficha /root/vsd/src) <(ficha "$VSD") && diff <(xattrs /root/vsd/src) <(xattrs "$VSD") &&
    echo "modos, dueños, enlaces y xattrs ($(xattrs "$VSD" | grep -c '^security.selinux=') etiquetas SELinux): idénticos"
  exit 0
fi

t "Surface Dual: armado en $R (modo $MODO)" "Surface Dual: building in $R (mode $MODO)"
paso a "$(t "paquetes de Ubuntu" "Ubuntu packages")" apt_hecho apt_haz
paso g "$(t "clave adb de este equipo" "adb key of this PC")" "[ -s $PUB ]" adb_haz
paso b "$(t "imagen de Google en $IMG" "Google image in $IMG")" "[ -f '$IMG/.sd_extraido' ]" imagen_haz
paso c "initrd_whpx_base" "[ -f '$INITRD' ]" initrd_haz
paso d "$(t "vendor en $VSD" "vendor in $VSD")" "[ -d $VSD/etc ]" vendor_haz
paso e "$(t "Bionic de la imagen en RutaA/bionic" "Bionic from the image in RutaA/bionic")" bionic_hecho bionic_haz
paso f "NDK r27c (sysroot)" "[ -f '$NDK/android-ndk-r27c/.sd_extraido' ]" ndk_haz
paso f "$(t "precompilados propios en /opt" "our prebuilt files in /opt")" "[ -f /opt/.sd_sembrado ]" siembra_haz
paso h "$(t "arranque.vhdx y android.vhdx (preparar.sh)" "arranque.vhdx and android.vhdx (preparar.sh)")" armado_hecho armado_haz
t "Surface Dual: armado terminado" "Surface Dual: build finished"
