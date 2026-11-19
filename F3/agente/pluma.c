// Sin libc: el LLVM ARM64 instalado basta; solo se usa la ABI estable de Linux uinput.
typedef unsigned short u16;
typedef short i16;
typedef unsigned int u32;
typedef long i64;
void *memcpy(void *destino, const void *origen, unsigned long n) {
    char *d = destino; const char *s = origen;
    for (unsigned long i = 0; i < n; ++i) d[i] = s[i];
    return destino;
}
struct evento { i64 sec, usec; u16 tipo, codigo; int valor; };
struct dispositivo { char nombre[80]; u16 bus, vendedor, producto, version; u32 ff; int max[64], min[64], fuzz[64], flat[64]; };
struct muestra { u16 x, y, presion; i16 tx, ty; u16 flags, ancho, alto; };
_Static_assert(sizeof(struct evento) == 24 && sizeof(struct dispositivo) == 1116 && sizeof(struct muestra) == 16, "ABI Linux ARM64");
static long sistema(long n, long a, long b, long c, long d) {
    register long x8 __asm__("x8") = n, x0 __asm__("x0") = a, x1 __asm__("x1") = b, x2 __asm__("x2") = c, x3 __asm__("x3") = d;
    __asm__ volatile("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory", "cc");
    return x0;
}
static int escribir(int fd, const void *p, long n) {
    const char *b = p;
    while (n) { long r = sistema(64, fd, (long)b, n, 0); if (r == -4) continue; if (r <= 0) return -1; b += r; n -= r; }
    return 0;
}
static int liberar(int fd) {
    const struct evento e[] = {{0, 0, 1, 330, 0}, {0, 0, 1, 320, 0}, {0, 0, 1, 321, 0},
        {0, 0, 1, 331, 0}, {0, 0, 3, 24, 0}, {0, 0, 0, 0, 0}};
    return escribir(fd, e, sizeof(e));
}
static int eventos(const struct muestra *m, struct evento e[10]) {
    int n = 0;
    e[n++] = (struct evento){0, 0, 1, 330, !!(m->flags & 2)};
    e[n++] = (struct evento){0, 0, 1, 320, !!(m->flags & 1) && !(m->flags & 4)};
    e[n++] = (struct evento){0, 0, 1, 321, !!(m->flags & 1) && !!(m->flags & 4)};
    e[n++] = (struct evento){0, 0, 1, 331, !!(m->flags & 8)};
    // Fuera de proximidad no hay posición válida: uinput conserva la anterior.
    if (m->flags & 1) {
        e[n++] = (struct evento){0, 0, 3, 0, m->x};
        e[n++] = (struct evento){0, 0, 3, 1, m->y};
    }
    e[n++] = (struct evento){0, 0, 3, 24, m->flags & 2 ? m->presion : 0};
    e[n++] = (struct evento){0, 0, 3, 26, m->tx};
    e[n++] = (struct evento){0, 0, 3, 27, m->ty};
    e[n++] = (struct evento){0, 0, 0, 0, 0};
    return n;
}
static int control(int fd, long orden, long valor) { return sistema(29, fd, orden, valor, 0) < 0 ? -1 : 0; }
static int crear(int ancho, int alto) {
    int fd = sistema(56, -100, (long)"/dev/uinput", 1 | 02000000, 0);
    if (fd < 0) return -1;
    struct dispositivo d = { .nombre = "SurfaceDual Pen", .bus = 6, .vendedor = 0x045e, .producto = 1, .version = 1 };
    int r = control(fd, 0x40045564, 1) | control(fd, 0x40045564, 3) | control(fd, 0x4004556e, 1);
    const int botones[] = {330, 320, 321, 331}, ejes[] = {0, 1, 24, 26, 27};
    for (int i = 0; i < 4; ++i) r |= control(fd, 0x40045565, botones[i]);
    for (int i = 0; i < 5; ++i) r |= control(fd, 0x40045567, ejes[i]);
    d.max[0] = ancho - 1; d.max[1] = alto - 1; d.max[24] = 1024;
    d.min[26] = d.min[27] = -90; d.max[26] = d.max[27] = 90;
    if (r || escribir(fd, &d, sizeof(d)) || control(fd, 0x5501, 0)) { sistema(57, fd, 0, 0, 0); return -1; }
    return fd;
}
static int ejecutar(void) {
    int fd = -1, ancho = 0, alto = 0, rc = 0;
    for (;;) {
        struct muestra m; char *p = (char *)&m; long falta = sizeof(m);
        while (falta) { long n = sistema(63, 0, (long)p, falta, 0); if (n == -4) continue; if (n <= 0) { rc = n < 0 || falta != sizeof(m); goto fin; } p += n; falta -= n; }
        if (m.ancho < 2 || m.alto < 2 || m.x >= m.ancho || m.y >= m.alto || m.presion > 1024 || m.tx < -90 || m.tx > 90 || m.ty < -90 || m.ty > 90 || (m.flags & ~15) || (!(m.flags & 1) && (m.flags & 14))) { rc = 1; break; }
        if (m.ancho != ancho || m.alto != alto) {
            if (fd >= 0) { liberar(fd); control(fd, 0x5502, 0); sistema(57, fd, 0, 0, 0); }
            fd = crear(m.ancho, m.alto); if (fd < 0) { rc = 1; break; } ancho = m.ancho; alto = m.alto;
        }
        struct evento e[10];
        int n = eventos(&m, e);
        if (escribir(fd, e, n * sizeof(e[0]))) { rc = 1; break; }
    }
fin:
    if (fd >= 0) { liberar(fd); control(fd, 0x5502, 0); sistema(57, fd, 0, 0, 0); }
    if (rc) { const char error[] = "SurfaceDual Pen: protocolo/uinput no disponible\n"; escribir(2, error, sizeof(error) - 1); }
    return rc;
}
void _start(void) { sistema(93, ejecutar(), 0, 0, 0); __builtin_unreachable(); }
