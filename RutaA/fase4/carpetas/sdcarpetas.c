// sdcarpetas: carpetas de Windows dentro de Android (lado Windows: RutaA/fase4/host/HvCarpetas.cs).
// Escucha en vsock 5008. HvCarpetas.exe (Windows) abre dos conexiones y manda un byte: 'P' = 9P2000.L, que monta el
// kernel (trans=fd) en RAIZ y se enlaza en /data/media/0/<carpeta> (MediaStore, galerías y Files lo ven como propio);
// 'N' = avisos de carpetas cambiadas desde Windows, que se piden escanear a MediaStore.
// Reposo: bloqueado en poll() (0 % de CPU). Si el 9P se corta (ayudante reiniciado), desmonta y espera otra conexión.
//   sdcarpetas [datos=/data/media/0]   (el argumento solo para probar fuera de Android)
#define _GNU_SOURCE   // POLLRDHUP, accept4
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/vm_sockets.h>

#define PUERTO 5008
#define RAIZ "/mnt/vendor/windows"
static const char *const carpetas[] = {"Movies", "Pictures", "Download", "Documents", "DCIM"};
static const char *datos = "/data/media/0";

static void desmontar(void) {
    char d[256];
    for (unsigned i = 0; i < sizeof carpetas / sizeof *carpetas; i++) {
        snprintf(d, sizeof d, "%s/%s", datos, carpetas[i]);
        umount2(d, MNT_DETACH);   // solo quita el enlace: lo que Android guardó antes debajo sigue ahí
    }
    umount2(RAIZ, MNT_DETACH);
}

// Escanear en MediaStore una carpeta ("Movies/Series"), por la shell como "am broadcast" (dominio shell: cmd/binder).
static void escanear(const char *rel) {
    char uri[1400] = "file:///storage/emulated/0/", *o = uri + strlen(uri);
    for (const unsigned char *c = (const unsigned char *)rel; *c && o < uri + sizeof uri - 4; c++)
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || strchr("/-._~", *c)) *o++ = *c;
        else o += sprintf(o, "%%%02X", *c);
    *o = 0;
#ifdef __ANDROID__
    if (fork() == 0) {
        int n = open("/dev/null", O_RDWR);
        dup2(n, 0); dup2(n, 1); dup2(n, 2);
        // cmd pasa su stdio a system_server: que lo abra la shell (sus fd sí los puede usar; los nuestros no, medido 17/11).
        execl("/system/bin/sh", "sh", "-c", "exec cmd activity broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE -d \"$0\" -p \"$1\" </dev/null >/dev/null 2>&1",
              uri, "com.google.android.providers.media.module", (char *)0);
        _exit(127);
    }
#else
    printf("escanear %s\n", uri);
#endif
}

static int montar(int fd) {
    struct stat st;
    char op[512], o[256], d[256], ctx[160] = "";
    snprintf(d, sizeof d, "%s/Download", datos);
    if (stat(d, &st) && stat(datos, &st)) { perror(datos); return -1; }
    unsigned uid = st.st_uid;   // dueño de las carpetas de /data/media/0: MediaProvider (u0_a208)
#ifdef __ANDROID__
    // 9p no guarda etiquetas: superbloque labeledfs (statfs de MediaProvider) y archivos media_rw_data_file con el nivel
    // MLS de MediaProvider (levelFrom=all de su uid); con s0 a secas SELinux le niega crear (medido 17/11).
    unsigned app = uid % 100000 - 10000, u = uid / 100000;
    if (uid % 100000 >= 10000)
        snprintf(ctx, sizeof ctx, ",fscontext=u:object_r:labeledfs:s0,context=\"u:object_r:media_rw_data_file:s0:c%u,c%u,c%u,c%u\"",
                 app & 0xff, 256 + (app >> 8 & 0xff), 512 + (u & 0xff), 768 + (u >> 8 & 0xff));
    else snprintf(ctx, sizeof ctx, ",context=u:object_r:media_rw_data_file:s0");
#endif
    // aname = ese dueño; cache=mmap: lectura anticipada (vídeo) y mmap, pero metadatos siempre al día (lo que cambie
    // Windows se ve al momento).
    snprintf(op, sizeof op, "trans=fd,rfdno=%d,wfdno=%d,version=9p2000.L,msize=1048576,cache=mmap,access=client,aname=%u%s",
             fd, fd, uid, ctx);
    mkdir(RAIZ, 0700);
    if (mount("windows", RAIZ, "9p", MS_NOSUID | MS_NODEV | MS_NOEXEC, op)) { perror("mount 9p"); return -1; }
    for (unsigned i = 0; i < sizeof carpetas / sizeof *carpetas; i++) {
        snprintf(o, sizeof o, RAIZ "/%s", carpetas[i]);
        snprintf(d, sizeof d, "%s/%s", datos, carpetas[i]);
        if (stat(o, &st)) continue;   // Windows no tiene esa carpeta
        if (mount(o, d, NULL, MS_BIND, NULL)) fprintf(stderr, "bind %s: %s\n", d, strerror(errno));
        else escanear(carpetas[i]);
    }
    fprintf(stderr, "sdcarpetas: carpetas de Windows montadas\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1) datos = argv[1];
    signal(SIGCHLD, SIG_IGN);   // los "sh" de escanear() no quedan zombis
    signal(SIGPIPE, SIG_IGN);
    int l = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_vm a = {.svm_family = AF_VSOCK, .svm_port = PUERTO, .svm_cid = VMADDR_CID_ANY};
    if (l < 0 || bind(l, (struct sockaddr *)&a, sizeof a) || listen(l, 4)) { perror("vsock"); return 1; }
    int p9 = -1, av = -1;
    char lin[2048];
    size_t n = 0;
    for (;;) {
        // Del 9P solo interesa el cierre (el kernel lee sus datos): POLLRDHUP sin POLLIN.
        struct pollfd pf[3] = {{l, POLLIN, 0}, {p9, POLLRDHUP, 0}, {av, POLLIN, 0}};
        if (poll(pf, 3, -1) < 0) { if (errno == EINTR) continue; perror("poll"); return 1; }
        if (pf[1].revents) {
            fprintf(stderr, "sdcarpetas: 9P cerrado, desmonto\n");
            desmontar(); close(p9); p9 = -1;
        }
        if (pf[2].revents) {
            ssize_t r = read(av, lin + n, sizeof lin - 1 - n);
            if (r <= 0) { close(av); av = -1; n = 0; }
            else {
                n += r; lin[n] = 0;
                char *ini = lin, *fin;
                while ((fin = strchr(ini, '\n'))) {
                    *fin = 0;
                    // Del host (más fiable), pero se valida igual: relativa, sin "..", a una de las carpetas.
                    if (p9 >= 0 && !strstr(ini, "..") && ini[0] != '/') escanear(ini);
                    ini = fin + 1;
                }
                n = strlen(ini); memmove(lin, ini, n + 1);
                if (n == sizeof lin - 1) n = 0;   // línea absurda: se descarta
            }
        }
        if (pf[0].revents & POLLIN) {
            struct sockaddr_vm p;
            socklen_t pl = sizeof p;
            int c = accept4(l, (struct sockaddr *)&p, &pl, SOCK_CLOEXEC);
            if (c < 0) continue;
            struct timeval t = {5, 0};
            setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof t);
            char tipo = 0;
            if (p.svm_cid != VMADDR_CID_HOST || read(c, &tipo, 1) != 1) { close(c); continue; }   // solo Windows
            if (tipo == 'P') {
                if (p9 >= 0) { desmontar(); close(p9); }
                struct timeval sin = {0, 0};
                setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &sin, sizeof sin);   // el kernel lee sin plazo
                p9 = montar(c) == 0 ? c : (close(c), -1);
            } else if (tipo == 'N') {
                if (av >= 0) close(av);
                av = c; n = 0;
            } else close(c);
        }
    }
}
