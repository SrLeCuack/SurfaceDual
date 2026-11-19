"""Crea initrd_whpx = initrd_whpx_base + bloque bootconfig (port de WhpxEmulatorBridge.java:3666-4030).
Uso: python bootconfig.py <avd> <sysDir> <ancho> <alto> <densidad> [hz]
"""
import os, re, struct, sys

avd, sysdir, w, h, dpi = sys.argv[1:6]
hz = sys.argv[6] if len(sys.argv) > 6 else '60'

vb = {'size': '6656', 'hash_alg': 'sha256',
      'digest': '7d083757a5e20cd0c568182e509f6d02da844505ce73ef80b2f1ada3f02ae18c'}  # valores por defecto del original
try:
    for kv in re.findall(r'param:\s*"androidboot\.vbmeta\.(size|hash_alg|digest)=([^"]*)"',
                         open(os.path.join(sysdir, 'VerifiedBootParams.textproto')).read()):
        vb[kv[0]] = kv[1]
except FileNotFoundError:
    pass
pub = open(os.path.expanduser('~/.android/adbkey.pub')).read().strip()

p = {
    'boot_devices': '3f000000.pcie', 'dalvik.vm.heapsize': '576m',
    'debug.hwui.renderer': 'skiagl', 'debug.renderengine.backend': 'skiaglthreaded',
    'hardware': 'ranchu', 'hardware.egl': 'emulation', 'hardware.gralloc': 'ranchu',
    'hardware.gltransport': 'pipe', 'hardware.vulkan': '0', 'zygote.disable_gl_preload': '1',
    'radio.noril': 'yes', 'vendor.radio.noril': '1', 'opengles.version': '196608',  # sin logcat *:V: el registro detallado despierta a logd sin parar
    'lcd_density': dpi, 'whpx.gfx_port': '5000', 'qemu': '1', 'qemu.adb.pubkey': pub,
    'qemu.avd_name': 'surfacedual', 'qemu.camera_hq_edge_processing': '0', 'qemu.camera_protocol_ver': '1',
    'qemu.cpuvulkan.version': '0', 'qemu.gltransport.drawFlushInterval': '800', 'qemu.gltransport.name': 'pipe',
    # 0: sin servidor de medios en el host (con 2 los decodificadores goldfish abortan en el primer cuadro)
    'qemu.hwcodec.avcdec': '0', 'qemu.hwcodec.hevcdec': '0', 'qemu.hwcodec.vpxdec': '0',
    'qemu.skin': f'{w}x{h}',  # sin screen_off_timeout infinito: que Android apague su pantalla (batería)
    'qemu.virtiowifi': '0', 'qemu.vsync': hz, 'qemu.allowsuspend': '1',  # Android suspende como un teléfono con la pantalla apagada (sin el wake lock del emulador)
    'selinux': 'permissive',  # sin androidboot.console: el servicio de consola serie de Android cuesta CPU y avisa "Serial console enabled"
    'verifiedbootstate': 'orange', 'flash.locked': '0', 'service.adb.tcp.port': '5555',
    'serialno': 'EMULATOR37X2X13X0', 'vbmeta.digest': vb['digest'], 'vbmeta.hash_alg': vb['hash_alg'],
    'vbmeta.size': vb['size'], 'veritymode': 'disabled',
}
params = ''.join(f'androidboot.{k}="{v}"\n' for k, v in p.items()).encode()
params += b'\0' * (-len(params) % 4)
base = open(os.path.join(avd, 'initrd_whpx_base'), 'rb').read()
base += b'\0' * (-len(base) % 4)
trailer = params + struct.pack('<II', len(params), sum(params)) + b'#BOOTCONFIG\n'
open(os.path.join(avd, 'initrd_whpx'), 'wb').write(base + trailer)
print('initrd_whpx', len(base) + len(trailer), 'bytes')
