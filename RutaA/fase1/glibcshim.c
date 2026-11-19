// Capa mínima glibc -> Bionic para cargar en un proceso Android las .so de Windows/WSL (libd3d12, libdxcore,
// driver de Qualcomm) y el libstdc++/libgcc_s de GNU que piden. Solo los 50 símbolos que Bionic no exporta
// (medidos con nm contra el libc.so de nuestro Android: F7/ruta_a_plan.md, fase 1).
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <syslog.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>
#include <langinfo.h>

#define EXPORT __attribute__((visibility("default")))

EXPORT int *__errno_location(void) { return __errno(); }
EXPORT void __assert_fail(const char *a, const char *f, unsigned l, const char *fn) {
    fprintf(stderr, "assert: %s (%s:%u %s)\n", a, f, l, fn ? fn : ""); abort();
}
EXPORT int bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
EXPORT char **__environ;
EXPORT char __libc_single_threaded = 0;   // conservador: siempre multihilo
EXPORT unsigned long __getauxval(unsigned long t) { return getauxval(t); }
EXPORT char *secure_getenv(const char *n) { return getenv(n); }

// Comprobaciones _FORTIFY de glibc: sin la comprobación, la función normal.
EXPORT int __vfprintf_chk(FILE *f, int fl, const char *fmt, va_list ap) { return vfprintf(f, fmt, ap); }
EXPORT int __fprintf_chk(FILE *f, int fl, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfprintf(f, fmt, ap); va_end(ap); return r; }
EXPORT int __vswprintf_chk(wchar_t *s, size_t n, int fl, size_t sl, const wchar_t *fmt, va_list ap) { return vswprintf(s, n, fmt, ap); }
EXPORT size_t __mbsnrtowcs_chk(wchar_t *d, const char **s, size_t nms, size_t len, mbstate_t *ps, size_t dl) { return mbsnrtowcs(d, s, nms, len, ps); }
EXPORT size_t __mbsrtowcs_chk(wchar_t *d, const char **s, size_t len, mbstate_t *ps, size_t dl) { return mbsrtowcs(d, s, len, ps); }
EXPORT wchar_t *__wmemcpy_chk(wchar_t *d, const wchar_t *s, size_t n, size_t dl) { return wmemcpy(d, s, n); }
EXPORT wchar_t *__wmemset_chk(wchar_t *d, wchar_t c, size_t n, size_t dl) { return wmemset(d, c, n); }
EXPORT void __syslog_chk(int p, int fl, const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsyslog(p, fmt, ap); va_end(ap); }

// glibc 2.38 redirige scanf/strtol a sus versiones C23; para estos usos son equivalentes.
EXPORT int __isoc23_sscanf(const char *s, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsscanf(s, fmt, ap); va_end(ap); return r; }
EXPORT int __isoc23_scanf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vscanf(fmt, ap); va_end(ap); return r; }
EXPORT long __isoc23_strtol(const char *s, char **e, int b) { return strtol(s, e, b); }
EXPORT long long __isoc23_strtoll(const char *s, char **e, int b) { return strtoll(s, e, b); }
EXPORT unsigned long __isoc23_strtoul(const char *s, char **e, int b) { return strtoul(s, e, b); }
EXPORT unsigned long long __isoc23_strtoull(const char *s, char **e, int b) { return strtoull(s, e, b); }

// Locale. El libstdc++ de GNU lee la estructura interna de glibc (__ctype_b/tolower/toupper), y Bionic lee
// mb_cur_max en el primer campo de la suya: el locale_t que damos tiene la forma de glibc con __locales[0] = mb_cur_max.
static const unsigned short *p_b; static const int *p_lower, *p_upper;
struct locale_glibc { void *locales[13]; const unsigned short *b; const int *lower, *upper; const char *nombres[13]; };
static locale_t crear(size_t mb) {
    struct locale_glibc *g = calloc(1, sizeof *g);
    if (!g) return 0;
    g->locales[0] = (void *) mb; g->b = p_b; g->lower = p_lower; g->upper = p_upper;
    for (int i = 0; i < 13; i++) g->nombres[i] = "C";
    return (locale_t) g;
}
EXPORT locale_t __newlocale(int m, const char *n, locale_t base) {
    if (m & (1 << 6)) m = LC_ALL_MASK;   // glibc acepta 1 << LC_ALL como "todas"; Bionic no
    locale_t prueba = newlocale(m, n, 0);   // Bionic valida el nombre
    if (!prueba) return 0;
    size_t mb = *(size_t *) prueba;
    freelocale(prueba);
    if (base && base != LC_GLOBAL_LOCALE) free(base);   // glibc puede consumir la base
    return crear(mb);
}
EXPORT locale_t __duplocale(locale_t l) {
    if (l == LC_GLOBAL_LOCALE) return crear(MB_CUR_MAX);
    struct locale_glibc *g = malloc(sizeof *g);
    if (g) memcpy(g, l, sizeof *g);
    return (locale_t) g;
}
EXPORT void __freelocale(locale_t l) { if (l && l != LC_GLOBAL_LOCALE) free(l); }
EXPORT locale_t __uselocale(locale_t l) { return uselocale(l); }   // Bionic solo lee mb_cur_max (primer campo)
// Los nl_item de glibc son (categoría << 16 | índice); Bionic usa otra numeración. Solo lo que pide libstdc++ en "C".
EXPORT char *__nl_langinfo_l(nl_item i, locale_t l) {
    switch (i) {
        case 0x10000: return ".";   // RADIXCHAR / DECIMAL_POINT
        case 0x10001: return "";    // THOUSEP
        case 0x10002: return "";    // GROUPING
        case 14: return MB_CUR_MAX > 1 ? "UTF-8" : "ANSI_X3.4-1968";   // CODESET
        default: return "";
    }
}
EXPORT int __strcoll_l(const char *a, const char *b, locale_t l) { return strcoll_l(a, b, l); }
EXPORT size_t __strxfrm_l(char *d, const char *s, size_t n, locale_t l) { return strxfrm_l(d, s, n, l); }
EXPORT size_t __strftime_l(char *s, size_t m, const char *f, const struct tm *t, locale_t l) { return strftime_l(s, m, f, t, l); }
EXPORT double __strtod_l(const char *s, char **e, locale_t l) { return strtod_l(s, e, l); }
EXPORT float __strtof_l(const char *s, char **e, locale_t l) { return strtof_l(s, e, l); }
EXPORT int __wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t l) { return wcscoll_l(a, b, l); }
EXPORT size_t __wcsxfrm_l(wchar_t *d, const wchar_t *s, size_t n, locale_t l) { return wcsxfrm_l(d, s, n, l); }
EXPORT size_t __wcsftime_l(wchar_t *s, size_t m, const wchar_t *f, const struct tm *t, locale_t l) { return wcsftime(s, m, f, t); }
EXPORT int __iswctype_l(wint_t c, wctype_t t, locale_t l) { return iswctype_l(c, t, l); }
EXPORT wctype_t __wctype_l(const char *n, locale_t l) { return wctype_l(n, l); }
EXPORT wint_t __towlower_l(wint_t c, locale_t l) { return towlower_l(c, l); }
EXPORT wint_t __towupper_l(wint_t c, locale_t l) { return towupper_l(c, l); }

// gettext sin catálogos: el texto original.
EXPORT char *gettext(const char *m) { return (char *) m; }
EXPORT char *dgettext(const char *d, const char *m) { return (char *) m; }
EXPORT char *bindtextdomain(const char *d, const char *dir) { return (char *) dir; }
EXPORT char *bind_textdomain_codeset(const char *d, const char *c) { return (char *) c; }

// Bionic no tiene cancelación de hilos.
EXPORT int pthread_cancel(unsigned long t) { return ENOSYS; }
EXPORT int pthread_setcancelstate(int s, int *o) { if (o) *o = 0; return 0; }
EXPORT int pthread_setcanceltype(int t, int *o) { if (o) *o = 0; return 0; }

// glibc 2.35+: el desenrollador de libgcc_s prueba primero _dl_find_object; -1 = usar dl_iterate_phdr.
EXPORT int _dl_find_object(void *pc, void *res) { return -1; }

// setjmp vuelve dos veces: no se puede envolver en una función C; salto directo al de Bionic.
// El sigjmp_buf de glibc (312 bytes) es mayor que el de Bionic (256): cabe.
__asm__(".globl __sigsetjmp\n.type __sigsetjmp,%function\n__sigsetjmp: b sigsetjmp\n");

// Tablas ctype de glibc (índices -128..255), con su codificación de bits en little endian.
static unsigned short tabla_b[384];
static int tabla_lower[384], tabla_upper[384];
static void enlazar_tablas(void) { p_b = tabla_b + 128; p_lower = tabla_lower + 128; p_upper = tabla_upper + 128; }

EXPORT const unsigned short **__ctype_b_loc(void) { return &p_b; }
EXPORT const int **__ctype_tolower_loc(void) { return &p_lower; }
EXPORT const int **__ctype_toupper_loc(void) { return &p_upper; }

__attribute__((constructor)) static void iniciar(void) {
    extern char **environ;
    __environ = environ;
    enlazar_tablas();
    for (int i = -128; i < 256; i++) {
        int c = i < 0 ? i + 256 : i;   // como unsigned char
        unsigned short b = 0;
        if (c < 128) {
            if (isupper(c)) b |= 0x100; if (islower(c)) b |= 0x200; if (isalpha(c)) b |= 0x400;
            if (isdigit(c)) b |= 0x800; if (isxdigit(c)) b |= 0x1000; if (isspace(c)) b |= 0x2000;
            if (isprint(c)) b |= 0x4000; if (isgraph(c)) b |= 0x8000; if (isblank(c)) b |= 0x1;
            if (iscntrl(c)) b |= 0x2; if (ispunct(c)) b |= 0x4; if (isalnum(c)) b |= 0x8;
        }
        tabla_b[i + 128] = b;
        tabla_lower[i + 128] = c < 128 ? tolower(c) : i;
        tabla_upper[i + 128] = c < 128 ? toupper(c) : i;
    }
}

// --- Rutas del driver (Ruta A, fase 4) ---------------------------------------------------------------------------
// El host da la ruta del UMD como /usr/lib/wsl/drivers/<inf>/<archivo> (montaje de WSL). En Android la raíz es de solo
// lectura y no hay /usr: el UMD vive en /vendor/lib64/wsl/<archivo>. patchelf --rename-dynamic-symbols hace que las
// libs de Microsoft/Qualcomm llamen a sd_* (fase4/graficos.sh); fuera de esa ruta todo pasa sin cambios.
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *ruta_wsl(const char *p, char *b) {
    static const char pre[] = "/usr/lib/wsl/";
    if (p && !strcmp(p, "/usr/lib/wsl")) { snprintf(b, PATH_MAX, "/vendor/lib64/wsl"); return b; }
    if (!p || strncmp(p, pre, sizeof pre - 1)) return p;
    const char *r = p + sizeof pre - 1;                        // "drivers/<inf>/x", "drivers/<inf>", "lib/x"
    const char *s = strchr(r, '/');                            // tras "drivers" o "lib"
    if (s && !strncmp(r, "drivers/", 8)) s = strchr(s + 1, '/');   // tras "<inf>"
    snprintf(b, PATH_MAX, "/vendor/lib64/wsl%s", s ? s : "");  // sin resto: el propio directorio
    return b;
}
EXPORT void *sd_dlopen(const char *p, int f) { char b[PATH_MAX]; return dlopen(ruta_wsl(p, b), f); }
EXPORT FILE *sd_fopen(const char *p, const char *m) { char b[PATH_MAX]; return fopen(ruta_wsl(p, b), m); }
EXPORT int sd_open(const char *p, int f, ...) {
    va_list a; va_start(a, f); int modo = ((f & O_CREAT) || (f & O_TMPFILE) == O_TMPFILE) ? va_arg(a, int) : 0;   // O_TMPFILE incluye O_DIRECTORY va_end(a);
    char b[PATH_MAX]; return open(ruta_wsl(p, b), f, modo);
}
EXPORT int sd_access(const char *p, int m) { char b[PATH_MAX]; return access(ruta_wsl(p, b), m); }
EXPORT int sd_stat(const char *p, struct stat *s) { char b[PATH_MAX]; return stat(ruta_wsl(p, b), s); }
EXPORT int sd_lstat(const char *p, struct stat *s) { char b[PATH_MAX]; return lstat(ruta_wsl(p, b), s); }
EXPORT DIR *sd_opendir(const char *p) { char b[PATH_MAX]; return opendir(ruta_wsl(p, b)); }
EXPORT ssize_t sd_readlink(const char *p, char *d, size_t n) { char b[PATH_MAX]; return readlink(ruta_wsl(p, b), d, n); }
EXPORT char *sd_realpath(const char *p, char *d) { char b[PATH_MAX]; return realpath(ruta_wsl(p, b), d); }

// struct sigaction de glibc aarch64 {handler, mask de 128 B, flags, restorer} y de Bionic LP64 {flags, handler, mask de
// 8 B, restorer} no casan: sin traducir, el manejador de SIGSEGV del compilador de shaders llegaba a ART como SIG_DFL
// (libsigchain lo registra con su pila en cada programa: miles de líneas por app) y borraba el de debuggerd.
#include <signal.h>
#include <stddef.h>
struct sigaction_glibc { void *h; unsigned long mask[16]; int flags; void *restorer; };
_Static_assert(sizeof(struct sigaction_glibc) == 152 && offsetof(struct sigaction_glibc, flags) == 136, "glibc aarch64");
EXPORT int sd_sigaction(int s, const struct sigaction_glibc *n, struct sigaction_glibc *o) {
    struct sigaction bn = {0}, bo = {0};
    if (n) { bn.sa_handler = (sighandler_t) n->h; bn.sa_flags = n->flags; bn.sa_restorer = n->restorer; memcpy(&bn.sa_mask, n->mask, sizeof bn.sa_mask); }
    int r = sigaction(s, n ? &bn : NULL, o ? &bo : NULL);
    if (!r && o) { memset(o, 0, sizeof *o); o->h = (void *) bo.sa_handler; o->flags = bo.sa_flags; o->restorer = bo.sa_restorer; memcpy(o->mask, &bo.sa_mask, sizeof bo.sa_mask); }
    return r;
}
