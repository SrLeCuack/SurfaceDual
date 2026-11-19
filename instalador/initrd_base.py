#!/usr/bin/env python3
# initrd_whpx_base desde el ramdisk.img de Google. Recorte de prepare_ramdisk_base() de
# F2/ext/android-emulator-windows-arm64/image-tools/prepare_whpx_initrd.py (proyecto DevanshSampat/android-emulator-windows-arm64,
# licencia Apache 2.0). Solo el primer segmento: preparar.sh lo abre con "lz4 -dc | cpio -id", que se para en el primer
# TRAILER!!!; los segmentos 2 (módulos virtio y fstab de QEMU) y 3 (gráficos del emulador) nunca llegan a la imagen.
# Diferencia a propósito: /init es un enlace a init.stock en vez del whpx_init del proyecto base (GRUB arranca con
# rdinit=/init.stock, así que no se ejecuta).
#   lz4 -dc ramdisk.img | python3 initrd_base.py adbkey.pub | lz4 -l -9 > initrd_whpx_base
import struct, sys

def entrada(ino, modo, nombre, datos):   # make_cpio_entry del original: newc, uid/gid/mtime 0, nlink 1
    n = nombre.encode() + b'\0'
    r = b'070701' + b''.join(b'%08x' % v for v in (ino, modo, 0, 0, 1, 0, len(datos), 0, 0, 0, 0, len(n), 0)) + n
    r += b'\0' * (-len(r) % 4) + datos
    return r + b'\0' * (-len(r) % 4)

cpio, p, stock = sys.stdin.buffer.read(), 0, []
while True:
    h = cpio[p:p + 110]
    assert h[:6] == b'070701', 'ramdisk.img no es cpio newc'
    modo, tam, ns = int(h[14:22], 16), int(h[54:62], 16), int(h[94:102], 16)
    nombre = cpio[p + 110:p + 109 + ns].decode(); p += (110 + ns + 3) & ~3
    if nombre == 'TRAILER!!!':
        break
    stock.append((nombre, modo, cpio[p:p + tam])); p += (tam + 3) & ~3
assert 'init' in [n for n, _, _ in stock], 'ramdisk.img sin /init'

prop = b'ro.debuggable=1\nro.force.debuggable=1\nro.adb.secure=0\npersist.sys.usb.config=adb\nservice.adb.tcp.port=5555\n'
salir0 = (struct.pack('<16sHHIQQQIHHHHHH', b'\x7fELF\x02\x01\x01' + b'\0' * 9, 2, 0xb7, 1, 0x400078, 64, 0, 0, 64, 56, 1, 64, 0, 0) +
          struct.pack('<IIQQQQQQ', 1, 5, 0, 0x400000, 0x400000, 132, 132, 0x1000) +
          struct.pack('<III', 0x52800ba8, 0xd2800000, 0xd4000001))   # ELF ARM64 de 132 bytes que sale con 0 (whpx_true)
rc = (b'on early-init\n    setprop service.adb.tcp.port 5555\n    setprop ro.debuggable 1\n    setprop ro.force.debuggable 1\n'
      b'    setprop ro.adb.secure 0\n\non post-fs-data\n    setprop service.adb.tcp.port 5555\n    setprop persist.sys.usb.config adb\n'
      b'    start adbd\n\non boot\n    setprop service.adb.tcp.port 5555\n    start adbd\n')
lista = [('adb_keys', 0o100644, open(sys.argv[1], 'rb').read().strip() + b'\n'), ('force_debuggable', 0o100644, b''),
         ('adb_debug.prop', 0o100644, prop), ('whpx_true', 0o100755, salir0), ('init.ranchu.rc', 0o100755, rc)]
hay_prop = False
for nombre, modo, datos in stock:
    if nombre == 'init':
        lista += [('init.stock', modo, datos), ('init', 0o120777, b'init.stock')]
        continue
    if 'build.prop' in nombre:
        hay_prop, datos = True, datos + b'\n' + prop
    lista.append((nombre, modo, datos))
if not hay_prop:
    lista.append(('system/etc/ramdisk/build.prop', 0o100644, prop))
lista.append(('TRAILER!!!', 0, b''))
salida = b''.join(entrada(1000 + i, m, n, d) for i, (n, m, d) in enumerate(lista))
sys.stdout.buffer.write(salida + b'\0' * (-len(salida) % 512))
