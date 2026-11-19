"""Cambia en .dynsym el binding STB_GNU_UNIQUE (10, exclusivo de glibc) por STB_GLOBAL (1).
El enlazador de Bionic no ve los símbolos 'u' (miembros estáticos de plantillas en libstdc++ y en el driver de
Qualcomm) y falla con 'cannot locate symbol'. Uso: python3 unicos_a_globales.py <archivo.so> ...
"""
import struct, sys

for ruta in sys.argv[1:]:
    with open(ruta, 'r+b') as f:
        d = bytearray(f.read())
        assert d[:4] == b'\x7fELF' and d[4] == 2 and d[5] == 1, 'solo ELF64 little endian'
        shoff, = struct.unpack_from('<Q', d, 0x28)
        shentsize, shnum = struct.unpack_from('<HH', d, 0x3A)
        cambios = 0
        for i in range(shnum):
            tipo, = struct.unpack_from('<I', d, shoff + i * shentsize + 4)
            if tipo != 11:   # SHT_DYNSYM
                continue
            off, tam = struct.unpack_from('<QQ', d, shoff + i * shentsize + 0x18)
            entsize, = struct.unpack_from('<Q', d, shoff + i * shentsize + 0x38)
            for s in range(off, off + tam, entsize):
                info = d[s + 4]
                if info >> 4 == 10:
                    d[s + 4] = (1 << 4) | (info & 0xF)
                    cambios += 1
        f.seek(0); f.write(d)
        print(f'{ruta}: {cambios} símbolos únicos -> globales')
