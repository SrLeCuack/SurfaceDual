#!/bin/bash
# Fase 4: kernel para arrancar nuestro AOSP 17 en la VM de Hyper-V. Mismo árbol que la fase 2 (linux-msft-wsl-6.18), pero:
# sin initramfs incrustado (GRUB pasa el ramdisk de Android) y con lo que Android necesita compilado dentro.
# El .config se rehace desde cero cada vez (base: el kernel de WSL que corre aquí), para que sea reproducible.
#   wsl -u root -e bash /mnt/c/SurfaceDual/RutaA/fase4/kernel_android.sh
set -e -o pipefail
F=$(cd "$(dirname "$0")" && pwd); K=/opt/kernel/src
cd $K
# Capacidad SELinux memfd_class de ACK android17-6.18.
P="$F/kernel_memfd/memfd_class.patch"
if patch -p1 -N --dry-run < "$P"; then
  patch -p1 -N < "$P"
elif patch -p1 -R --dry-run < "$P"; then
  echo 'memfd_class ya aplicado'
else
  echo 'ERROR: memfd_class.patch no coincide con el árbol' >&2
  exit 1
fi
# Driver local: reemplazo de fuentes y altas únicas en Kconfig/Makefile.
cp "$F/kernel_sdsync/sdsync.c" drivers/misc/sdsync.c
cp "$F/kernel_sdsync/sdsync.h" drivers/misc/sdsync.h
if ! grep -q '^config SDSYNC$' drivers/misc/Kconfig; then
  cat >> drivers/misc/Kconfig <<'EOF'

config SDSYNC
 bool "Surface Dual eventfd to sync_file"
 depends on SYNC_FILE
EOF
fi
grep -qF 'obj-$(CONFIG_SDSYNC) += sdsync.o' drivers/misc/Makefile || \
  echo 'obj-$(CONFIG_SDSYNC) += sdsync.o' >> drivers/misc/Makefile
mkdir -p drivers/misc/uapi
cp "$F/kernel_ashmem/ashmem.c" drivers/misc/ashmem.c
cp "$F/kernel_ashmem/ashmem.h" drivers/misc/ashmem.h
cp "$F/kernel_ashmem/uapi/ashmem.h" drivers/misc/uapi/ashmem.h
if ! grep -q '^config ASHMEM$' drivers/misc/Kconfig; then
  cat >> drivers/misc/Kconfig <<'EOF'

config ASHMEM
 bool "Enable the Anonymous Shared Memory Subsystem"
 depends on SHMEM
EOF
fi
grep -qF 'obj-$(CONFIG_ASHMEM) += ashmem.o' drivers/misc/Makefile || \
  echo 'obj-$(CONFIG_ASHMEM) += ashmem.o' >> drivers/misc/Makefile
# Sondeo breve y adaptativo antes de WFI (cpuidle sdpoll + gobernador haltpoll): Hyper-V tarda ~80 us en despertar una vCPU
# dormida; 4 KiB O_DSYNC 135 -> ~60 us (nativo 53; medido 18/11). En reposo la ventana cae a 0. Apagar: haltpoll.guest_halt_poll_ns=0.
bash "$F/kernel_sdpoll/aplicar.sh" $K
zcat /proc/config.gz > .config
# Fase 2 (binder, consola PL011) + fase 4 (EROFS, F2FS, ZRAM, cifrado de archivos, fs-verity, bootconfig).
./scripts/config --enable ANDROID_BINDER_IPC --enable ANDROID_BINDERFS --set-str ANDROID_BINDER_DEVICES "binder,hwbinder,vndbinder" \
  --enable MEMFD_CREATE --enable PSI --enable UHID --enable INPUT_UINPUT --enable FUSE_FS \
  --enable SECURITY_SELINUX --enable DM_VERITY --enable BLK_DEV_DM --enable VIRTIO_INPUT --enable HID_MULTITOUCH \
  --enable SERIAL_AMBA_PL011_CONSOLE --set-str LOCALVERSION "-surfacedual" --set-str INITRAMFS_SOURCE "" --set-str CMDLINE "" \
  --enable EROFS_FS --enable EROFS_FS_ZIP --enable EROFS_FS_ZIP_LZMA --enable F2FS_FS --enable ZRAM \
  --enable FS_ENCRYPTION --enable FS_ENCRYPTION_INLINE_CRYPT --enable FS_VERITY --enable BLK_INLINE_ENCRYPTION \
  --enable BLK_INLINE_ENCRYPTION_FALLBACK --enable DM_VERITY_FEC --enable BOOT_CONFIG
# Requisitos de Android (kernel/configs main c/android-6.12/android-base.config): netfilter (raw, mangle, xt_*), etc.
# Solo las opciones activadas: las "is not set" son de cumplimiento (VTS) y alguna rompería WSL/Hyper-V.
grep -E '^CONFIG_[A-Z0-9_]+=' "$F/kernel_cfg/android-base.config" >> .config
./scripts/config --enable DMA_SHARED_BUFFER --enable SYNC_FILE --enable EVENTFD --enable SDSYNC
./scripts/config --enable SHMEM --enable ASHMEM
./scripts/config --enable SDPOLL_CPUIDLE
# Sin Yama, como el kernel de Android (stock_android14-6.1.config): con Yama, Netflix no podía hacer ptrace a su propio proceso
# (protección anti-depuración). Mientras no se recompile, preparar.sh lo apaga con sysctl.kernel.yama.ptrace_scope=0.
./scripts/config --disable SECURITY_YAMA
# Audio: Hyper-V no da tarjeta de sonido a Linux; una de loopback recibe lo que reproduce el HAL de audio y un daemon lo
# manda a Windows por vsock (RutaA/fase4/audio).
./scripts/config --enable SND_ALOOP --enable SND_PCM --enable SND_TIMER
make olddefconfig >/dev/null
# En Android no se cargan módulos: lo de red/netfilter (y lo que pide la base) que quedó "=m" pasa a integrado.
# Se repite porque al pasar un padre a "=y" aparecen hijos nuevos.
for i in 1 2 3 4; do
  sed -i -E 's/^(CONFIG_(NETFILTER|NF_|NFT_|IP_NF|IP6_NF|IP_SET|NET_SCH|NET_CLS|NET_ACT|NET_EM|L2TP|PPP|TUN|VETH|IFB|XFRM|INET|IPV6|BRIDGE|VLAN_8021Q|NET_IPGRE|NET_IPIP|NET_UDP_TUNNEL|NET_FOU|DUMMY|BPF|CRYPTO|HID)[A-Z0-9_]*)=m$/\1=y/' .config
  grep -E '^CONFIG_[A-Z0-9_]+=y' "$F/kernel_cfg/android-base.config" | sed 's/=y$//' | while read -r k; do sed -i "s/^$k=m\$/$k=y/" .config; done
  grep -E '^CONFIG_[A-Z0-9_]+=y' "$F/kernel_cfg/android-base.config" >> .config
  make olddefconfig >/dev/null
done
grep -qx 'CONFIG_SDSYNC=y' .config || { echo 'FALTA CONFIG_SDSYNC=y'; exit 1; }
grep -qx 'CONFIG_ASHMEM=y' .config || { echo 'FALTA CONFIG_ASHMEM=y'; exit 1; }
grep -qx 'CONFIG_SDPOLL_CPUIDLE=y' .config || { echo 'FALTA CONFIG_SDPOLL_CPUIDLE=y'; exit 1; }
grep -c $'\x01' .config && { echo ".config dañado"; exit 1; } || true
for c in EROFS_FS F2FS_FS ZRAM FS_ENCRYPTION FS_VERITY BOOT_CONFIG ANDROID_BINDERFS DXGKRNL IP_NF_RAW IP6_NF_RAW HYPERV_VSOCKETS; do
  grep -E "^CONFIG_$c=" .config || echo "FALTA $c"
done | tr '\n' ' '; echo
# Firma neutra en /proc/version: sin esto lleva "root@<nombre de este equipo>" y el escáner del instalador rechaza el paquete.
export KBUILD_BUILD_USER=surfacedual KBUILD_BUILD_HOST=build KBUILD_BUILD_TIMESTAMP='Thu Nov 19 00:00:00 UTC 2026'
make -j$(nproc) Image 2>&1 | tail -1
grep -aqF "(surfacedual@build)" arch/arm64/boot/Image || { echo "Image sin la firma neutra" >&2; exit 1; }
cp arch/arm64/boot/Image "$F/Image-android"
ls -la "$F/Image-android"
