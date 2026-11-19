#!/usr/bin/env python3
# Lista las particiones lógicas de super (metadatos LP de Android): nombre, offset en el disco y tamaño en bytes.
#   lp_particiones.py <imagen> <offset de super en bytes>
import struct, sys
f = open(sys.argv[1], 'rb'); base = int(sys.argv[2])
f.seek(base + 4096); g = f.read(52)
magia, tam_g = struct.unpack_from('<II', g); assert magia == 0x616c4467, 'sin geometría LP'
f.seek(base + 4096 + 2 * 4096); h = f.read(256)
assert struct.unpack_from('<I', h)[0] == 0x414C5030, 'sin cabecera LP'
tam_h = struct.unpack_from('<I', h, 8)[0]
p_off, p_n, p_tam, e_off, e_n, e_tam = struct.unpack_from('<6I', h, 80)
f.seek(base + 4096 + 2 * 4096 + tam_h); t = f.read(4 * 4096 * 16)
ext = [struct.unpack_from('<QIQI', t, e_off + i * e_tam) for i in range(e_n)]
for i in range(p_n):
    nombre, attr, prim, n, grupo = struct.unpack_from('<36s4I', t, p_off + i * p_tam)
    for sectores, tipo, dato, fuente in ext[prim:prim + n]:
        print(nombre.rstrip(b'\0').decode(), base + dato * 512, sectores * 512)
