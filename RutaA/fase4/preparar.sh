#!/bin/bash
# Fase 4, puerta 2: nuestro AOSP 17 arranca en la VM de Hyper-V hasta montar system/vendor desde super (first stage).
# Disco 0: GRUB + kernel Android + ramdisk (del emulador, con fstab propio y bootconfig para Hyper-V). Disco 1: system.img.
#   wsl -u root -e bash /mnt/c/SurfaceDual/RutaA/fase4/preparar.sh
set -e
F=$(cd "$(dirname "$0")" && pwd); S=$(cd "$F/../.." && pwd); W=/opt/fase4; K=/opt/kernel/src
IMG=$S/F2/imagenes/android-37/arm64-v8a
# Arranque verificado propio (AVB, 18/11): vbmeta firmada con NUESTRA clave RSA-4096 (no la de Google) y dm-verity obligatorio
# en system, vendor, product, system_ext y sdgraficos; estado honesto "yellow". VERIFICADO=0 lo quita (vuelve a "orange").
# Clave privada fuera del repo (copia de seguridad: guardar ese archivo en un sitio seguro; sin ella hay que firmar con otra).
VERIFICADO=${VERIFICADO:-0}; AVB="python3 $F/herramientas/avbtool.py"; LLAVE=${AVB_LLAVE:-/mnt/c/Users/usuario/.surfacedual/avb_key.pem}
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq lz4 cpio dosfstools gdisk mtools qemu-utils grub-efi-arm64-bin attr erofs-utils >/dev/null
make -s -C $K/tools/bootconfig >/dev/null 2>&1 || true

# Ramdisk del emulador (base sin bootconfig) + fstab de primera etapa (el emulador lo pasa por device tree; Hyper-V es ACPI).
rm -rf $W/rd; mkdir -p $W/rd; cd $W/rd
lz4 -dc $S/F2/build/avd/initrd_whpx_base 2>/dev/null | cpio -id 2>/dev/null || true
# fstab completo para Hyper-V (sufijo propio "hyperv": el /vendor/etc/fstab.ranchu apunta a discos virtio del emulador).
# userdata con FBE v2 pero sin keydirectory/checkpoint/quota: exigen dm-default-key/dm-bow, que el kernel WSL no trae.
cat > fstab.hyperv <<'FSTAB'
system      /system      erofs ro  wait,logical,first_stage_mount
vendor      /vendor      erofs ro  wait,logical,first_stage_mount
product     /product     erofs ro  wait,logical,first_stage_mount
system_ext  /system_ext  erofs ro  wait,logical,first_stage_mount
/dev/block/by-name/sdgraficos  /vendor/lib64/wsl  erofs  ro  wait,first_stage_mount
/dev/block/by-name/metadata  /metadata  ext4  noatime,nosuid,nodev  wait,formattable,first_stage_mount
/dev/block/by-name/userdata  /data      ext4  noatime,nosuid,nodev  wait,check,formattable,latemount,fileencryption=aes-256-xts:aes-256-cts:v2,fsverity
/dev/block/zram0 none swap defaults zramsize=50%
FSTAB
if [ "$VERIFICADO" = 1 ]; then sed -i -E '/^(system|vendor|product|system_ext) |sdgraficos/s/first_stage_mount/first_stage_mount,avb/' fstab.hyperv; fi
# Imagen de producción: el ramdisk del emulador fuerza ro.debuggable=1 y ro.adb.secure=0 (force_debuggable, adb_debug.prop y
# setprop en init.ranchu.rc). shortcut: solo con PRODUCCION=1: probado 17/11, adbd queda "unauthorized" (no acepta la clave del
# PC de /adb_keys en una imagen no depurable); antes de usarlo hay que autorizar la clave en /data/misc/adb/adb_keys.
if [ "${PRODUCCION:-0}" = 1 ]; then
  rm -f force_debuggable adb_debug.prop
  sed -i '/setprop ro.debuggable 1/d; /setprop ro.force.debuggable 1/d; /setprop ro.adb.secure 0/d' init.ranchu.rc
  grep -cE 'debuggable|adb.secure' init.ranchu.rc | grep -qx 0
fi
find . | cpio -o -H newc 2>/dev/null | lz4 -l -9 > $W/ramdisk.lz4

# Vendor para Hyper-V: el de QEMU (/root/vsd/src, ya sin UWB ni radio: F4/plan_vendor_sd.md) monta /data en segunda etapa con
# "mount_all /vendor/etc/fstab.ranchu" (espera /dev/block/vdc). Se le pone nuestro fstab, conservando etiquetas SELinux.
V=$W/vendor_hv; rm -rf $V; cp -a /root/vsd/src $V
cp fstab.hyperv $V/etc/fstab.hyperv; setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/fstab.hyperv
sed -i 's|/vendor/etc/fstab.ranchu|/vendor/etc/fstab.hyperv|' $V/etc/init/hw/init.ranchu.rc
# Hyper-V no es QEMU: fuera ro.kernel.qemu=1 (en el dispositivo solo lo leen apps; el sistema usa ro.boot.qemu). cat conserva la etiqueta.
grep -vx 'ro.kernel.qemu=1' $V/build.prop > $W/bp.tmp; cat $W/bp.tmp > $V/build.prop
# Nuestro composer da fences de present válidas. Con vsync_reactor_ignore_present_fences=true (el de goldfish) SurfaceFlinger no
# apaga nunca el vsync del composer: 120 avisos/s en reposo, ~3,5 % de un núcleo entre composer y SF (medido 18/11).
grep -vx 'debug.sf.vsync_reactor_ignore_present_fences=true' $V/build.prop > $W/bp.tmp; cat $W/bp.tmp > $V/build.prop
# Shell en la consola serie (servicio "console" del init.rc de system; en imágenes user solo arranca con ro.debuggable=1).
# En producción no: es una shell para cualquier programa de Windows por la tubería COM (consola.ps1) y Android avisa "afecta
# el rendimiento" (visto 19/11).
[ "${PRODUCCION:-0}" = 1 ] || printf '\non boot\n    start console\n' >> $V/etc/init/hw/init.ranchu.rc
# En Hyper-V la consola es el PL011 (ttyAMA0); sin etiqueta queda como "device" y SELinux niega abrirla.
echo '/dev/ttyAMA0                 u:object_r:console_device:s0' >> $V/etc/selinux/vendor_file_contexts
# Discos de datos con su etiqueta (20/11): sin ella fsck (e2fsck) no podía abrir userdata ("Permission denied") y Android no se
# reparaba solo; tras muchos apagados sucios /data no montó ("Structure needs cleaning") y Android no arrancaba.
printf '/dev/block/by-name/userdata   u:object_r:userdata_block_device:s0
/dev/block/by-name/metadata   u:object_r:metadata_block_device:s0
' >> $V/etc/selinux/vendor_file_contexts
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/selinux/vendor_file_contexts
# Mesa consulta vendor.mesa.* en cada proceso con GL; sin contexto propio (vendor_default_prop) las apps no pueden leerlas y
# libc escribía "Access denied finding property" (~20 líneas/s con apps abiertas, medido 17/11). Tipo y lectura: surfacedual.cil.
printf '\nvendor.mesa.                 u:object_r:vendor_mesa_prop:s0\n' >> $V/etc/selinux/vendor_property_contexts
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/selinux/vendor_property_contexts
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/init/hw/init.ranchu.rc   # sed -i la borra
# HAL de sensores del emulador: exige la tubería de QEMU, cae en bucle y system_server espera al HAL declarado hasta
# que el Watchdog lo mata. Sin declarar, Android arranca sin sensores (los de la Surface irán por un HAL propio).
rm -f $V/etc/init/android.hardware.sensors-service-multihal.rc $V/etc/vintf/manifest/android.hardware.sensors-multihal.xml
# Sin funciones de Bluetooth: si no, la pila de BT de Android (sin HAL) cae en bucle y Rescue Party acaba restaurando
# de fábrica (medido: se perdieron apps instaladas). El BT real irá por el host.
rm -f $V/etc/permissions/android.hardware.bluetooth.xml $V/etc/permissions/android.hardware.bluetooth_le.xml
# handheld_core_hardware.xml también la declaraba: con bluetooth_on=1 com.google.android.bluetooth abortaba ~7 veces en
# cada arranque (hci_ != nullptr, sin HAL; medido 17/11). La radio BT es de Windows; el audio de Android ya sale por él.
sed -i '/"android.hardware.bluetooth/d' $V/etc/permissions/handheld_core_hardware.xml; grep -c '"android.hardware.bluetooth' $V/etc/permissions/handheld_core_hardware.xml | grep -qx 0
# Sensores: también los declaraba (acelerómetro, giroscopio, barómetro, humedad...) sin HAL que los dé ("No Sensors on the
# device"): Android no debe anunciar hardware que no tiene. Volver a declararlos cuando exista el HAL de sensores de la Surface.
sed -i '/<feature name="android.hardware.sensor\./d' $V/etc/permissions/handheld_core_hardware.xml; grep -c '<feature name="android.hardware.sensor\.' $V/etc/permissions/handheld_core_hardware.xml | grep -qx 0
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/permissions/handheld_core_hardware.xml
# Igual con Bluetooth y huella del emulador (abortan en bucle sin su hardware virtual); el BT real irá por el host.
rm -f $V/etc/init/bluetooth-service-default.rc $V/etc/vintf/manifest/bluetooth-service-default.xml \
      $V/etc/init/android.hardware.biometrics.fingerprint-service.ranchu.rc $V/etc/vintf/manifest/android.hardware.biometrics.fingerprint-service.ranchu.xml
# Solo del emulador: la cámara "ranchu" (tubería de QEMU; queda la emulada de google y cameraserver para la cámara futura)
# y bt_vhci_forwarder (puente virtio del HAL de BT, ya quitado). Procesos residentes que no hacen nada en Hyper-V.
rm -f $V/etc/init/android.hardware.camera.provider.ranchu.rc $V/etc/vintf/manifest/android.hardware.camera.provider.ranchu.xml
sed -i '/^service bt_vhci_forwarder /,/^$/d' $V/etc/init/hw/init.ranchu.rc; grep -c bt_vhci_forwarder $V/etc/init/hw/init.ranchu.rc | grep -qx 0
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/init/hw/init.ranchu.rc
grep -c fstab.hyperv $V/etc/init/hw/init.ranchu.rc
# El composer propio también crea la pantalla táctil por /dev/uinput (uhid:uhid 0660): grupo uhid.
sed -i 's/^    group graphics drmrpc$/    group graphics drmrpc uhid/' $V/etc/init/hwc3.rc; grep -q "drmrpc uhid" $V/etc/init/hwc3.rc
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/init/hwc3.rc
# Red: el adaptador sintético de Hyper-V (hv_netvsc) sale como eth0; Android solo lo gestiona (DHCP, validación) si el
# dispositivo declara Ethernet. La VM se conecta al "Default Switch" (NAT de Windows) en crear_vm.ps1.
printf '<?xml version="1.0" encoding="utf-8"?>\n<permissions>\n    <feature name="android.hardware.ethernet" />\n</permissions>\n' > $V/etc/permissions/android.hardware.ethernet.xml
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/permissions/android.hardware.ethernet.xml
# Circle to Search: Quickstep (ContextualSearchInvoker) solo invoca si el dispositivo declara esta función (la de los Pixel).
printf '<?xml version="1.0" encoding="utf-8"?>\n<permissions>\n    <feature name="com.google.android.feature.CONTEXTUAL_SEARCH" />\n</permissions>\n' > $V/etc/permissions/surfacedual_circulo.xml
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/permissions/surfacedual_circulo.xml
# No anunciar lo que no respaldamos (18/11): KeyMint es por software (no hay almacén de claves por hardware) y no hay
# arranque verificado (veritymode disabled, estado orange). Declarar de menos es honesto. Revertir: borrar estas 3 líneas.
rm -f $V/etc/permissions/android.hardware.hardware_keystore_V4.xml
printf '<?xml version="1.0" encoding="utf-8"?>
<permissions>
    <unavailable-feature name="android.hardware.hardware_keystore" />
    <unavailable-feature name="android.software.verified_boot" />
</permissions>
' > $V/etc/permissions/surfacedual_honesto.xml
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/permissions/surfacedual_honesto.xml
# Lápiz (dispositivo "SurfaceDual_Pen" del visor): pantalla táctil interna que gira con Android. Antes lo escribía el agente
# en /data/system/devices/idc al arrancar (root escribiendo datos del sistema); ahora va en la imagen, como los de QEMU.
printf 'device.internal = 1\ntouch.deviceType = touchScreen\n# El visor entrega coordenadas del panel físico (Windows no gira con Android delante): Android las gira con su\n# pantalla, igual que el táctil ("rot").\ntouch.orientationAware = 1\n' > $V/usr/idc/SurfaceDual_Pen.idc
setfattr -n security.selinux -v u:object_r:vendor_idc_file:s0 $V/usr/idc/SurfaceDual_Pen.idc
# El overlay de conectividad del emulador declara "eth0;11,12,14;;" (sin NOT_RESTRICTED=13): red restringida que nunca
# es la predeterminada. Sin él, Android usa sus valores por omisión (eth\d sin restricciones).
rm -f $V/overlay/goldfish_overlay_connectivity_google.apk
# Reglas SELinux propias (RutaA/fase4/sepolicy/*.cil): al quitar la precompilada, init compila la política con ellas.
cat "$F"/sepolicy/*.cil >> $V/etc/selinux/vendor_sepolicy.cil
rm -f $V/etc/selinux/precompiled_sepolicy $V/etc/selinux/precompiled_sepolicy.*.sha256
# Video por software: el mapper sube cada cuadro YUV a su textura con D3D12 dentro de mediaswcodec, que corre con seccomp.
# Lo que le falta a su lista lo mide hal/gralloc/prueba_yuv seccomp (16/11: fcntl); mediaswcodec añade este archivo.
printf 'fcntl: 1\n' > $V/etc/seccomp_policy/mediaswcodec.policy
setfattr -n security.selinux -v u:object_r:vendor_configs_file:s0 $V/etc/seccomp_policy/mediaswcodec.policy
bash "$F/graficos.sh" $V $W/graficos.erofs   # Mesa d3d12 + D3D12 + UMD de Qualcomm (partición sdgraficos)
bash "$F/hal/camara/compilar.sh" $V >/dev/null   # cámaras de Windows (HvCamara.exe, vsock 5007) en lugar de las emuladas
bash "$F/hal/sensores/compilar.sh" $V >/dev/null   # sensores reales de la Surface (HvSensores.exe, vsock 5009)
bash "$F/carpetas/instalar.sh" $V   # carpetas de Windows en /data/media/0 por 9P (HvCarpetas.exe, vsock 5008)
mkfs.erofs -zlz4hc,12 -b4096 -C16384 -T1230768000 -Ufa639d88-7274-5c53-a140-a3d0858c0e11 --mount-point=/vendor $W/vendor_hv.erofs $V >/dev/null
VO=734003200; VMAX=68796416; [ $(stat -c %s $W/vendor_hv.erofs) -le $VMAX ] || { echo "vendor_hv no cabe"; exit 1; }   # offset y hueco de vendor en super
# Árbol de hash de dm-verity al final de la partición (siempre: sin "avb" en el fstab nadie lo lee).
$AVB add_hashtree_footer --image $W/vendor_hv.erofs --partition_name vendor --partition_size $VMAX --hash_algorithm sha256 --do_not_generate_fec

# bootconfig: el del emulador (F2/bootconfig.py) cambiando boot_devices (PCIe de QEMU) por el PARTUUID de super.
python3 - "$S/F2/bootconfig.py" > $W/bootconfig.txt <<'PY'
import os, re, subprocess, sys
src = open(sys.argv[1], encoding='utf-8').read()
pares = dict(re.findall(r"'([a-z0-9_.]+)':\s*'([^']*)'", src))
pares.pop('boot_devices', None)
pares.pop('selinux', None)   # imagen user: init lo ignora y SELinux queda en enforcing (medido 17/11); no aparentar otra cosa
pares.update({'boot_part_uuid': '58f96366-3c87-4bc2-add4-e423a556e2fd', 'fstab_suffix': 'hyperv', 'hardware': 'ranchu',
              'verifiedbootstate': 'orange', 'console': 'ttyAMA0'})
# Número de serie: el REAL de la Surface donde se instala (Win32_BIOS, usuario 18/11), leído en cada armado; nunca va al repo
# (solo /opt/fase4). Sin serie válido (BIOS vacía, "To be filled", ceros) queda el ya guardado o uno propio SD+12 hex.
sn = '/opt/fase4/serialno'
try: real = subprocess.run(['powershell.exe', '-NoProfile', '-Command', '(Get-CimInstance Win32_BIOS).SerialNumber'], capture_output=True, text=True, timeout=60).stdout.strip()
except Exception: real = ''
VALIDO = r'[0-9A-Za-z]{6,20}'   # el de la Surface Pro 11: 14 alfanuméricos; solo letras y cifras (va entre comillas al bootconfig y a adb)
def valido(v): return bool(re.fullmatch(VALIDO, v)) and len(set(v)) > 1   # descarta "To be filled...", vacío y "000000"
guardado = open(sn).read().strip() if os.path.exists(sn) else ''
serie = real if valido(real) else guardado if valido(guardado) else 'SD' + os.urandom(6).hex().upper()
if serie != guardado:   # solo root lo lee (0600); temporal + replace: un corte no deja el archivo vacío
    fd = os.open(sn + '.tmp', os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600); os.write(fd, serie.encode()); os.fsync(fd); os.close(fd); os.replace(sn + '.tmp', sn)
os.chmod(sn, 0o600); pares['serialno'] = serie
pares['qemu.adb.pubkey'] = open(os.path.expanduser('~/.android/adbkey.pub')).read().strip()   # en bootconfig.py es variable, no literal
for k, v in sorted(pares.items()):
    if '{' in v or '}' in v: continue   # valores calculados en tiempo de ejecución en bootconfig.py
    print(f'androidboot.{k} = "{v}"')
PY
cp $W/ramdisk.lz4 $W/ramdisk-bc.img; $K/tools/bootconfig/bootconfig -a $W/bootconfig.txt $W/ramdisk-bc.img
wc -l $W/bootconfig.txt

# Disco de arranque: GRUB + kernel + ramdisk con bootconfig.
D=$W/disco; rm -rf $D; mkdir -p $D; dd if=/dev/zero of=$D/arranque.raw bs=1M count=256 status=none
P=$(( $(stat -c %s $D/arranque.raw) / 512 - 34 - 2048 )); sgdisk -n 1:2048:$((2048 + P - 1)) -t 1:ef00 $D/arranque.raw >/dev/null
mkfs.vfat -F 32 -C $D/efi.img $((P / 2)) >/dev/null; export MTOOLS_SKIP_CHECK=1
# loglevel=1: el kernel solo escribe en la consola serie pánicos/alertas. Con loglevel=5 su hilo pr/ttyAMA-1 gastaba 3-9 % de
# un núcleo en reposo (cada carácter del PL011 es una salida al hipervisor; medido 17/11); dmesg/logcat lo siguen teniendo
# todo y la shell de la consola (servicio console) sigue igual. Para depurar el arranque por la serie: loglevel=5 (o 7).
# ptrace_scope=0: nuestro kernel (config de WSL) trae Yama, que el kernel de Android no tiene (stock_android14-6.1.config:
# "CONFIG_SECURITY_YAMA is not set"); con Yama en 1 el proceso hijo de Netflix no podía hacer ptrace a la app (su protección) y la app se mataba.
# hv_utils.timesync_implicit=1 (R2): el reloj de Android se para con la VM en pausa y Hyper-V no manda la orden de sincronizar al
# reanudar (drivers/hv/hv_util.c: "the sync flag may not always be sent on resume"); así la muestra de hora siguiente (cada ~5 s)
# lo pone en hora si va 5 s o más atrasado. Sin código en Windows ni despertares nuevos.
cat > $D/grub.cfg <<'CFG'
serial --unit=0 --speed=115200
terminal_input serial console
terminal_output serial console
set timeout=0
search --no-floppy --file --set=root /Image
linux /Image console=ttyAMA0,115200 rdinit=/init.stock bootconfig loglevel=1 printk.devkmsg=on sysctl.kernel.yama.ptrace_scope=0 hv_utils.timesync_implicit=1
initrd /ramdisk.img
boot
CFG
if [ -n "${LOGLEVEL:-}" ]; then sed -i "s/ loglevel=1 / loglevel=$LOGLEVEL /" $D/grub.cfg; fi   # depurar: LOGLEVEL=7 al armar
grub-mkstandalone -O arm64-efi -o $D/BOOTAA64.EFI --modules="part_gpt fat linux serial terminal echo search search_fs_file" "boot/grub/grub.cfg=$D/grub.cfg"
mmd -i $D/efi.img ::/EFI ::/EFI/BOOT; mcopy -i $D/efi.img $D/BOOTAA64.EFI ::/EFI/BOOT/BOOTAA64.EFI
mcopy -i $D/efi.img "$F/Image-android" ::/Image; mcopy -i $D/efi.img $W/ramdisk-bc.img ::/ramdisk.img
dd if=$D/efi.img of=$D/arranque.raw bs=512 seek=2048 conv=notrunc status=none
[ ! -f "$F/arranque.vhdx" ] || cp -f "$F/arranque.vhdx" "$F/arranque.vhdx.anterior"   # el anterior, hasta que actualizar.ps1 vea arrancar a Android (Gemini)
qemu-img convert -f raw -O vhdx -o subformat=dynamic $D/arranque.raw "$F/arranque.vhdx"
# Disco de Android: vbmeta + super (mismo PARTUUID que en el emulador) + metadata + userdata nuevos (sin cifrar: prueba).
# El crudo se guarda en /opt (ext4 de WSL): copiar super desde /mnt/c tarda ~15 min; después solo se reescriben vendor y datos.
A=$W/android.raw
if [ ! -f $A ]; then
  read S1 E1 <<<"$(sgdisk -i 1 "$IMG/system.img" | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
  read S2 E2 <<<"$(sgdisk -i 2 "$IMG/system.img" | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
  U2=$(sgdisk -i 2 "$IMG/system.img" | awk '/unique GUID/{print $4}')
  N1=$((E1 - S1 + 1)); N2=$((E2 - S2 + 1)); MD=$((16 * 2048)); UD=$((256 * 1024 * 2048)); GD=$((512 * 2048))
  TOT=$((2048 + N1 + N2 + MD + UD + GD + 2048)); truncate -s $((TOT * 512)) $A.tmp
  sgdisk -n 1:2048:+$N1 -c 1:vbmeta -n 2:0:+$N2 -c 2:super -u 2:$U2 -n 3:0:+$MD -c 3:metadata -n 4:0:+$UD -c 4:userdata -n 5:0:+$GD -c 5:sdgraficos $A.tmp >/dev/null
  dd if="$IMG/system.img" of=$A.tmp bs=512 skip=$S1 seek=2048 count=$N1 conv=notrunc,sparse status=none
  S2N=$(sgdisk -i 2 $A.tmp | awk '/First sector/{print $3}'); dd if="$IMG/system.img" of=$A.tmp bs=1M iflag=skip_bytes,count_bytes oflag=seek_bytes skip=$((S2 * 512)) seek=$((S2N * 512)) count=$((N2 * 512)) conv=notrunc,sparse status=none
  [ $S2N -eq $S2 ] || { echo "super cambió de offset"; exit 1; }   # VO cuenta desde el inicio del disco
  mv $A.tmp $A; rm -f $W/datos_ok   # disco nuevo: datos de fábrica
fi
[ "$(dd if=$A bs=1 skip=$((VO + 1024)) count=4 status=none | od -An -tx4 | tr -d ' ')" = e0f5e1e2 ] || { echo "no hay erofs en VO"; exit 1; }
dd if=/dev/zero of=$A bs=4096 seek=$((VO / 4096)) count=$((VMAX / 4096)) conv=notrunc status=none
dd if=$W/vendor_hv.erofs of=$A bs=4096 seek=$((VO / 4096)) conv=notrunc status=none
# system_ext con un .rc del lado del sistema (vendor_init no puede fijar propiedades del sistema): adb por vsock.
# Antes también forzaba sys.use_memfd=true porque el kernel no tenía ashmem; desde que el kernel lleva ashmem
# (RutaA/fase4/kernel_ashmem) ya no hace falta: con memfd sin memfd_class se rompían las colas FMQ del audio.
# Offsets de super: RutaA/fase4/lp_particiones.py. Se genera una vez (caché en $W).
SO=803209216; SMAX=142467072
SEV=44   # subir al cambiar surfacedual.rc, surfacedual_agente.dex o el overlay: obliga a regenerar la caché
[ "$(cat $W/system_ext_hv.version 2>/dev/null)" = $SEV ] || rm -f $W/system_ext_hv.erofs
if [ ! -f $W/system_ext_hv.erofs ]; then
  dd if=$A of=/tmp/se.erofs bs=1M iflag=skip_bytes,count_bytes skip=$SO count=$SMAX status=none
  # loop + cp -a conserva los xattrs (etiquetas SELinux); fsck.erofs 1.7 --extract no.
  rm -rf /tmp/se /tmp/se_mnt; mkdir -p /tmp/se_mnt; mount -o loop,ro /tmp/se.erofs /tmp/se_mnt; cp -a /tmp/se_mnt /tmp/se; umount /tmp/se_mnt
  # Apps solo del emulador: MultiDisplayProvider reintentaba cada 2 s una tubería goldfish (~100 MB), radio y satélite sin
  # módem (F7/plan/sobrantes_propuesta.md). Las de /system y /product siguen, deshabilitadas con pm disable-user (persiste en /data).
  rm -rf /tmp/se/priv-app/MultiDisplayProvider /tmp/se/priv-app/EmulatorRadioConfig /tmp/se/priv-app/CFSatelliteService
  # + adb por vsock (en Hyper-V no hay red de emulador): el host llega con AF_HYPERV (RutaA/fase4/host).
  # Rescue Party desactivado: ante caídas en bucle restaura de fábrica (mejor ver y arreglar la causa).
  # Ajustes en dos paneles (0.f): Settings solo parte la pantalla con persist.settings.large_screen_opt.enabled
  # (ActivityEmbeddingUtils.SHOULD_ENABLE_LARGE_SCREEN_OPTIMIZATION, AOSP main; medido 16/11: isSettingsSplitSupported = false).
  # wake_lock: dormido (doble toque en el launcher) el kernel entraba en suspensión y ya no despertaba con nada (ni toque,
  # ni adb, ni pausa/reanudar: medido 16/11) y el usuario veía negro. La batería la ahorra la pausa de la VM (VisorVM al ocultarse).
  # prime_shader_cache 0: SurfaceFlinger precompilaba sus shaders en el hilo RenderEngine (9,6 s en d3d12, medido con ftrace
  # 16/11); su primer cuadro esperaba detrás y system_server se quedaba ~9,3 s en WaitForDisplay (getBootDisplayModeSupport).
  # adb_keys = la clave del PC (qemu.adb.pubkey del bootconfig), escrita aquí en cada arranque. Antes se borraba y se dejaba que
  # init.adb-setup.ranchu.sh (vendor, servicio asíncrono) la copiara: carrera, a veces la copiaba antes del borrado y PRODUCCION=1
  # quedaba "unauthorized" (19/11, 1 de 2). El script del vendor solo copia si falta el archivo: ya no hace nada. Antes: quedó
  # vacío cuando la clave no llegaba al bootconfig; sin él PRODUCCION=1 da "unauthorized". Aquí (dominio init, antes que el
  # post-fs-data del vendor); desde el .rc del vendor no puede (vendor_init: denegado).
  # Agente con grupo shell: escribe sus temporales en /data/local/tmp (shell:shell 0771) sin CAP_DAC_OVERRIDE (SELinux 17/11).
  printf 'on early-init\n    setprop persist.sys.disable_rescue true\n    setprop service.sf.prime_shader_cache 0\n\non boot\n    write /sys/power/wake_lock surfacedual\n    setprop persist.settings.large_screen_opt.enabled true\n    setprop service.adb.listen_addrs vsock:5555\n    start adbd\n\nservice surfacedual_agente /system/bin/app_process /system_ext/framework Agente --escuchar 4449\n    class main\n    user root\n    group root shell\n    disabled\n    seclabel u:r:surfacedual_agente:s0\n    setenv CLASSPATH /system_ext/framework/surfacedual_agente.dex\n\non property:sys.boot_completed=1\n    start surfacedual_agente\n\non post-fs-data\n    mkdir /data/misc/adb 02750 system shell\n    write /data/misc/adb/adb_keys ${ro.boot.qemu.adb.pubkey}\n    chown system shell /data/misc/adb/adb_keys\n    chmod 0640 /data/misc/adb/adb_keys\n' > /tmp/se/etc/init/surfacedual.rc
  setfattr -n security.selinux -v "$(getfattr --only-values -n security.selinux "$(ls /tmp/se/etc/init/*.rc | head -1)")" /tmp/se/etc/init/surfacedual.rc
  # Overlay propio del framework (RutaA/fase4/overlay: 120 Hz, Circle to Search; compilar antes con overlay/compilar.sh).
  # En system_ext y no en vendor: los RRO de product (el autogenerado del emulador) mandan sobre los de vendor y vaciaban
  # config_defaultContextualSearchPackageName; system_ext es la última partición en el orden de overlays.
  mkdir -p /tmp/se/overlay /tmp/se/framework
  install -m 0644 "$F/overlay/SurfaceDualFramework.apk" /tmp/se/overlay/SurfaceDualFramework.apk
  setfattr -n security.selinux -v u:object_r:system_file:s0 /tmp/se/overlay /tmp/se/overlay/SurfaceDualFramework.apk
  install -m 0644 "$S/F3/agente/agente.dex" /tmp/se/framework/surfacedual_agente.dex
  setfattr -n security.selinux -v "$(getfattr --only-values -n security.selinux "$(ls /tmp/se/etc/init/*.rc | head -1)")" /tmp/se/framework/surfacedual_agente.dex
  mkfs.erofs -zlz4hc,12 -b4096 -C16384 -T1230768000 -Ubb98928d-2e92-54eb-9c0c-62d81631b302 --mount-point=/system_ext $W/system_ext_hv.erofs /tmp/se >/dev/null
  [ $(stat -c %s $W/system_ext_hv.erofs) -le $SMAX ] || { rm $W/system_ext_hv.erofs; echo "system_ext_hv no cabe"; exit 1; }
  echo $SEV > $W/system_ext_hv.version
fi
$AVB add_hashtree_footer --image $W/system_ext_hv.erofs --partition_name system_ext --partition_size $SMAX --hash_algorithm sha256 --do_not_generate_fec
dd if=/dev/zero of=$A bs=4096 seek=$((SO / 4096)) count=$((SMAX / 4096)) conv=notrunc status=none
dd if=$W/system_ext_hv.erofs of=$A bs=4096 seek=$((SO / 4096)) conv=notrunc status=none
# Teléfono y Mensajes fuera de la imagen (E19, pedido del usuario 17/11): sin módem no sirven; antes solo pm disable-user.
# product se rehace una vez (caché por PEV) quitando sus dos carpetas; lo demás queda igual (loop + cp -a conserva etiquetas).
PO=945815552; PMAX=1691279360
PEV=6   # subir al cambiar lo que se quita de product o la identidad
[ "$(cat $W/product_hv.version 2>/dev/null)" = $PEV ] || rm -f $W/product_hv.erofs
if [ ! -f $W/product_hv.erofs ]; then
  dd if=$A of=/tmp/pr.erofs bs=1M iflag=skip_bytes,count_bytes skip=$PO count=$PMAX status=none
  rm -rf /tmp/pr /tmp/pr_mnt; mkdir -p /tmp/pr_mnt; mount -o loop,ro /tmp/pr.erofs /tmp/pr_mnt; cp -a /tmp/pr_mnt /tmp/pr; umount /tmp/pr_mnt; rm -f /tmp/pr.erofs
  rm -rf /tmp/pr/priv-app/GoogleDialer /tmp/pr/priv-app/PrebuiltBugle
  # Identidad: la de la imagen (sdk_gphone64_arm64/emu64a, el emulador de Google que ES este sistema), sin cambiarla.
  # Circle to Search: con ella abre de forma estable; con marca/modelo/huella "SurfaceDual" la app de Google lo cerraba al
  # instante (medido 18/11, F7/plan/cts_notas.md; la comprobación interna de Google no se conoce). El nombre "Surface Dual" se ve por Ajustes (settings global device_name).
  # Solo se cambia characteristics=tablet ("emulator" hacía decir a Ajustes "dispositivo emulado"). Se quita el bloque anterior.
  BP=/tmp/pr/etc/build.prop; sed -n '/^# SurfaceDual identidad/q;p' $BP > /tmp/bp.pr; cat /tmp/bp.pr > $BP
  printf '# SurfaceDual identidad\nro.build.characteristics=tablet\n' >> $BP
  mkfs.erofs -zlz4hc -b4096 -T1230768000 --mount-point=/product $W/product_hv.erofs /tmp/pr >/dev/null; rm -rf /tmp/pr
  [ $(stat -c %s $W/product_hv.erofs) -le $PMAX ] || { rm $W/product_hv.erofs; echo "product_hv no cabe"; exit 1; }
  echo $PEV > $W/product_hv.version
fi
PZ=$(stat -c %s $W/product_hv.erofs)
dd if=$W/product_hv.erofs of=$A bs=4096 seek=$((PO / 4096)) conv=notrunc status=none
dd if=/dev/zero of=$A bs=4096 seek=$(((PO + PZ) / 4096)) count=$(((PMAX - PZ) / 4096)) conv=notrunc status=none
# Datos de Android (metadata + userdata): se conservan entre armados; de fábrica solo con disco nuevo o FABRICA=1.
# encrypt/verity: FBE y fs-verity (sin "encrypt", vold falla con EOPNOTSUPP y reinicia).
# Con android.vhdx en uso nunca de fábrica sin FABRICA=1: un crudo nuevo (o sin datos_ok) se rellena con sus datos (revisión Codex 20/11).
if { [ ! -f $W/datos_ok ] && [ ! -f "$F/android.vhdx" ]; } || [ "${FABRICA:-0}" = 1 ]; then
  L=$(losetup -Pf --show $A); mkfs.ext4 -q -F -L metadata ${L}p3; mkfs.ext4 -q -F -O encrypt,verity,stable_inodes -L userdata ${L}p4; losetup -d $L
  touch $W/datos_ok
elif [ -f "$F/android.vhdx" ]; then
  # Android escribe en android.vhdx, no en el crudo: antes de regenerarlo se copian de vuelta metadata y userdata
  # (si no, cada armado devolvía los datos al estado de fábrica: medido, se perdían las apps instaladas).
  qemu-img convert -f vhdx -O raw "$F/android.vhdx" $W/actual.raw
  for p in 3 4; do
    read P0 P1 <<<"$(sgdisk -i $p $A | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
    read Q0 Q1 <<<"$(sgdisk -i $p $W/actual.raw | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
    # Mismo inicio y la del disco en uso no mayor (si no, la copia mezclaría datos de otra partición): se para sin tocar
    # nada. Puede ser menor si el crudo ya se amplió (userdata 256 GiB) y el armado anterior se cortó antes de convertir.
    [ "$Q0" = "$P0" ] && [ "$Q1" -le "$P1" ] ||
      { echo "ERROR: la partición $p cambió de lugar; datos sin copiar (android.vhdx intacto; FABRICA=1 empieza de cero)"; exit 1; }
    # Vaciar antes la región: con conv=sparse, dd no escribe los bloques a cero y quedarían restos viejos.
    fallocate --punch-hole --keep-size -o $((P0 * 512)) -l $(((P1 - P0 + 1) * 512)) $A
    dd if=$W/actual.raw of=$A bs=1M iflag=skip_bytes,count_bytes oflag=seek_bytes skip=$((P0 * 512)) seek=$((P0 * 512)) \
       count=$(((Q1 - Q0 + 1) * 512)) conv=notrunc,sparse status=none
  done
  rm -f $W/actual.raw
fi
# userdata de 256 GiB (pedido 16/11: películas y manga; antes 32). Se amplía en sitio y conserva los datos: p4 mantiene
# inicio y GUID, sdgraficos (p5) se recrea detrás (su contenido se escribe abajo en cada armado) y resize2fs agranda el
# ext4 fuera de línea (FBE cifra archivos, no bloques: inodos y claves no cambian). Plan revisado con Kimi (F7/plan/almacen_a.md).
UDN=$((256 * 1024 * 2048)); GD=$((512 * 2048))
read U0 U1 <<<"$(sgdisk -i 4 $A | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
if [ $((U1 - U0 + 1)) -lt $UDN ]; then
  G4=$(sgdisk -i 4 $A | awk '/unique GUID/{print $4}'); G5=$(sgdisk -i 5 $A | awk '/unique GUID/{print $4}')
  truncate -s $(((U0 + UDN + GD + 2048 + 34) * 512)) $A
  sgdisk -e -d 5 -d 4 -n 4:$U0:+$UDN -t 4:8300 -c 4:userdata -u 4:$G4 -n 5:0:+$GD -t 5:8300 -c 5:sdgraficos -u 5:$G5 $A >/dev/null
fi
# ext4 al tamaño de la partición (también si un armado anterior se cortó entre el particionado y resize2fs).
L=$(losetup -Pf --show $A)
FS=$(dumpe2fs -h ${L}p4 2>/dev/null | awk -F: '/^Block count/{c=$2} /^Block size/{s=$2} END{print c * s}')
if [ "${FS:-0}" -lt $(( $(blockdev --getsize64 ${L}p4) - 1048576 )) ]; then
  R=0; R2=9; e2fsck -fp ${L}p4 || R=$?   # 1 = errores menores corregidos (normal tras apagar la VM); con set -e hace falta ||
  if [ $R -le 1 ]; then R2=0; resize2fs ${L}p4 >/dev/null 2>&1 || R2=$?; fi
  [ $R -le 1 ] && [ $R2 -eq 0 ] || { losetup -d $L; echo "ERROR: e2fsck=$R resize2fs=$R2 en userdata (android.vhdx sin tocar)"; exit 1; }
fi
# fast_commit (18/11): un fsync que cambia el tamaño del archivo (WAL de SQLite, descargas) escribía 3 bloques en serie (datos +
# diario + commit: 741 us por operación); con el commit rápido, 2. Lo soportan el kernel (5.10+) y el e2fsck de Android (1.47).
# Revertir: tune2fs -O ^fast_commit sobre la partición (con la VM apagada).
if ! dumpe2fs -h ${L}p4 2>/dev/null | grep -q fast_commit; then
  R=0; e2fsck -fp ${L}p4 >/dev/null || R=$?
  [ $R -le 1 ] && tune2fs -O fast_commit ${L}p4 >/dev/null || { losetup -d $L; echo "ERROR: fast_commit (e2fsck=$R); android.vhdx sin tocar"; exit 1; }
fi
losetup -d $L
# sdgraficos (después de ampliar: su posición puede haber cambiado)
read G5 E5 <<<"$(sgdisk -i 5 $A | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
[ $(stat -c %s $W/graficos.erofs) -le $(((E5 - G5 + 1) * 512)) ] || { echo "graficos no cabe"; exit 1; }
$AVB add_hashtree_footer --image $W/graficos.erofs --partition_name sdgraficos --partition_size $(((E5 - G5 + 1) * 512)) --hash_algorithm sha256 --do_not_generate_fec
dd if=$W/graficos.erofs of=$A bs=512 seek=$G5 conv=notrunc status=none
# vbmeta propia: descriptores de system (cadena con su clave del emulador), product y system_dlkm (sin cambios) + los nuevos de
# vendor, system_ext y sdgraficos (los del mismo nombre sustituyen a los viejos). init de primera etapa compara su resumen con el
# del bootconfig antes de montar; sin clave de hardware que lo ancle, el estado honesto es "yellow" (clave del usuario).
read S1 E1 <<<"$(sgdisk -i 1 $A | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
dd if=/dev/zero of=$A bs=512 seek=$S1 count=$((E1 - S1 + 1)) conv=notrunc status=none
if [ "$VERIFICADO" = 1 ]; then
  [ -f "$LLAVE" ] || { mkdir -p "$(dirname "$LLAVE")"; openssl genrsa -out "$LLAVE" 4096 2>/dev/null; }
  mkdir -p $W/avb; read Q0 Q1 <<<"$(sgdisk -i 1 "$IMG/system.img" | awk '/First sector/{f=$3} /Last sector/{l=$3} END{print f, l}')"
  dd if="$IMG/system.img" of=$W/avb/emulador.img bs=512 skip=$Q0 count=$((Q1 - Q0 + 1)) status=none
  # vbmeta encadenada de system (está al final de su partición): calculate_vbmeta_digest la busca como system.img en el mismo directorio.
  read SYO SYT <<<"$(python3 $F/lp_particiones.py $A $(( $(sgdisk -i 2 $A | awk '/First sector/{print $3}') * 512 )) | awk '$1=="system"{print $2, $3}')"
  read VOF VSZ <<<"$(dd if=$A bs=1 skip=$((SYO + SYT - 64)) count=64 status=none | python3 -c 'import sys,struct; d=sys.stdin.buffer.read(); assert d[:4]==b"AVBf"; print(*struct.unpack(">QQ", d[20:36]))')"
  dd if=$A of=$W/avb/system.img bs=1 iflag=skip_bytes,count_bytes skip=$((SYO + VOF)) count=$VSZ status=none
  $AVB make_vbmeta_image --output $W/avb/vbmeta.img --key "$LLAVE" --algorithm SHA256_RSA4096 --include_descriptors_from_image $W/avb/emulador.img \
    --include_descriptors_from_image $W/vendor_hv.erofs --include_descriptors_from_image $W/system_ext_hv.erofs --include_descriptors_from_image $W/graficos.erofs
  [ $(stat -c %s $W/avb/vbmeta.img) -le $(((E1 - S1 + 1) * 512)) ]
  dd if=$W/avb/vbmeta.img of=$A bs=512 seek=$S1 conv=notrunc status=none
  DG=$($AVB calculate_vbmeta_digest --image $W/avb/vbmeta.img --hash_algorithm sha256)
  sed -i '/androidboot.verifiedbootstate\|androidboot.veritymode\|androidboot.vbmeta/d' $W/bootconfig.txt
  printf 'androidboot.%s = "%s"\n' verifiedbootstate yellow veritymode enforcing vbmeta.device_state locked vbmeta.hash_alg sha256 \
    vbmeta.size $(( $(stat -c %s $W/avb/vbmeta.img) + VSZ )) vbmeta.digest $DG >> $W/bootconfig.txt
  cp $W/ramdisk.lz4 $W/ramdisk-bc.img; $K/tools/bootconfig/bootconfig -a $W/bootconfig.txt $W/ramdisk-bc.img
  mcopy -o -i $D/efi.img $W/ramdisk-bc.img ::/ramdisk.img; dd if=$D/efi.img of=$D/arranque.raw bs=512 seek=2048 conv=notrunc status=none
  qemu-img convert -f raw -O vhdx -o subformat=dynamic $D/arranque.raw "$F/arranque.vhdx"
else
  dd if="$IMG/system.img" of=$A bs=512 skip=$(sgdisk -i 1 "$IMG/system.img" | awk '/First sector/{print $3}') seek=$S1 count=$((E1 - S1 + 1)) conv=notrunc status=none
fi
sgdisk -p $A | tail -4
# Convertir aparte y sustituir al final: si falla (p. ej. sin espacio) el android.vhdx en uso queda intacto (revisión Gemini).
rm -f "$F/android.vhdx.tmp"; qemu-img convert -f raw -O vhdx -o subformat=dynamic $A "$F/android.vhdx.tmp"   # en línea propia: set -e corta si falla
[ ! -f "$F/android.vhdx" ] || mv -f "$F/android.vhdx" "$F/android.vhdx.anterior"   # el anterior, hasta ver que Android arranca (revisión Codex)
mv -f "$F/android.vhdx.tmp" "$F/android.vhdx"
ls -la "$F"/*.vhdx
