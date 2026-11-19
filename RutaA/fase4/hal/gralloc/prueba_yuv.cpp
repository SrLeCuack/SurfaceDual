// Autocomprobación del YUV del gralloc (T_YUV_GPU) en la VM, con el mapper real: YV12 -> lock/escribir/unlock ->
// textura RGBA leída de vuelta, colores frente a la fórmula y tiempo por cuadro 1080p. Con "seccomp" corre bajo la lista
// de syscalls de mediaswcodec (el proceso del códec) e imprime las que faltan: van a vendor/etc/seccomp_policy.
//   compilar.sh la deja en /opt/fase4/hal/prueba_yuv; adb push a /data/local/tmp y: LD_LIBRARY_PATH=/vendor/lib64 ./prueba_yuv [seccomp]
#include "mapper.cpp"
#include <cmath>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>

extern "C" long trampolin(long nr, long a, long b, long c, long d, long e, long f);
extern "C" char trampolin_fin[];
asm(".text\n.p2align 4\n.global trampolin\ntrampolin:\n mov x8, x0\n mov x0, x1\n mov x1, x2\n mov x2, x3\n mov x3, x4\n"
    " mov x4, x5\n mov x5, x6\n svc #0\n ret\n.global trampolin_fin\ntrampolin_fin:\n");

static bool vistas[512];
// Syscall fuera de la lista: se apunta y se ejecuta igual desde trampolin (el filtro deja pasar esa dirección).
static void sigsys(int, siginfo_t *si, void *ctx) {
    auto &r = ((ucontext_t *)ctx)->uc_mcontext.regs;
    int nr = si->si_syscall;
    if (nr >= 0 && nr < 512 && !vistas[nr]) { vistas[nr] = true; char m[32]; int n = snprintf(m, sizeof m, "FALTA syscall %d\n", nr); write(2, m, n); }
    r[0] = trampolin(nr, r[0], r[1], r[2], r[3], r[4], r[5]);
}
static void seccomp() {
    // mediaswcodec.policy + crash_dump.no_mmap_mprotect_prctl + code_coverage (vacío) del APEX swcodec de Android 17.
    static const int ok[] = {__NR_futex, __NR_ioctl, __NR_close, __NR_dup, __NR_ppoll, __NR_mprotect, __NR_mmap, __NR_getuid,
        __NR_getrlimit, __NR_fstat, __NR_newfstatat, __NR_fstatfs, __NR_memfd_create, __NR_fsetxattr, __NR_ftruncate, __NR_mremap,
        __NR_munmap, __NR_prctl, __NR_writev, __NR_sigaltstack, __NR_clone, __NR_exit, __NR_lseek, __NR_rt_sigprocmask, __NR_openat,
        __NR_write, __NR_nanosleep, __NR_setpriority, __NR_set_tid_address, __NR_getdents64, __NR_readlinkat, __NR_read, __NR_pread64,
        __NR_gettimeofday, __NR_faccessat, __NR_exit_group, __NR_restart_syscall, __NR_rt_sigreturn, __NR_getrandom, __NR_madvise,
        __NR_getpid, __NR_gettid, __NR_sched_yield, __NR_setsockopt, __NR_sendmsg, __NR_clock_gettime, __NR_getppid, __NR_pipe2,
        __NR_recvmsg, __NR_recvfrom, __NR_sysinfo, __NR_uname, __NR_process_vm_readv, __NR_tgkill, __NR_rt_sigaction,
        __NR_rt_tgsigqueueinfo, __NR_geteuid, __NR_getgid, __NR_getegid, __NR_getgroups};
    constexpr int N = sizeof ok / sizeof ok[0];
    uint64_t ini = (uint64_t)trampolin, fin = (uint64_t)trampolin_fin;
    std::vector<sock_filter> f = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, instruction_pointer) + 4),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (uint32_t)(ini >> 32), 0, 3),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, instruction_pointer)),
        BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, (uint32_t)ini, 0, 1),
        BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, (uint32_t)fin, 0, N + 2),   // dentro de trampolin: permitir
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)),
    };
    for (int i = 0; i < N; i++) f.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (uint32_t)ok[i], (uint8_t)(N - i), 0));
    f.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP));
    f.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
    // shortcut: no filtra los argumentos de mmap/mprotect (PROT_WRITE|PROT_EXEC prohibido en mediaswcodec).
    struct sigaction sa = {}; sa.sa_sigaction = sigsys; sa.sa_flags = SA_SIGINFO; sigaction(SIGSYS, &sa, nullptr);
    sock_fprog p = {(unsigned short)f.size(), f.data()};
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &p)) { perror("seccomp"); exit(1); }
}

static double ms() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static native_handle_t *handle(int fd, int fd2, int w, int h, int fmt, int stride, uint64_t tam, int tipo, uint64_t uso) {
    native_handle_t *n = native_handle_create(fd2 >= 0 ? 2 : 1, sdg::N_INTS);
    n->data[0] = fd; if (fd2 >= 0) n->data[1] = fd2;
    int v[] = {sdg::MAGIA, w, h, fmt, 1, (int)(uint32_t)uso, (int)(uso >> 32), stride, (int)(uint32_t)tam, (int)(tam >> 32), 7, 0, tipo};
    memcpy(&n->data[n->numFds], v, sizeof v);
    return n;
}

// Un formato y tamaño: escribe mitad roja (Cr alto) / mitad azul (Cb alto), sube, lee la textura y compara con la fórmula
// (BT.709 si alto >= 720, BT.601 si no). Detecta Cb/Cr cambiados, planos mal puestos y matriz equivocada.
static int probar(ID3D12Device *d, int FMT, int W, int H, int VUELTAS) {
    const int S = sdg::alinear(W, 16);
    D3D12_RESOURCE_DESC rd = {}; rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = W; rd.Height = H; rd.DepthOrArraySize = 1;
    rd.MipLevels = 1; rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;   // como allocator.cpp
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
    ID3D12Resource *t = nullptr; HANDLE sh = nullptr;
    if (FAILED(d->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_SHARED, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&t))) ||
        FAILED(d->CreateSharedHandle(t, nullptr, GENERIC_ALL, nullptr, &sh))) { printf("FALLO: textura\n"); return 1; }
    t->Release();
    sdg::Yuv l;
    if (!sdg::yuv(FMT, S, H, &l)) { printf("FALLO: yuv()\n"); return 1; }
    int mfd = (int)syscall(__NR_memfd_create, "yuv", 0); ftruncate(mfd, (off_t)l.tam);
    uint64_t uso = 0x33 | (1 << 8);   // CPU lectura/escritura + GPU_TEXTURE
    buffer_handle_t yb, rb;
    if (importBuffer(handle((int)(intptr_t)sh, mfd, W, H, FMT, S, l.tam, sdg::T_YUV_GPU, uso), &yb) ||
        importBuffer(handle(dup((int)(intptr_t)sh), -1, W, H, 1, S, 0, sdg::T_D3D12_GPU, uso), &rb)) { printf("FALLO: import\n"); return 1; }
    auto pl = planos(yb);   // lo que lee lockYCbCr de libui
    const int64_t cb1 = FMT == 0x23 ? (int64_t)S * H : (int64_t)S * H + (int64_t)sdg::alinear(S / 2, 16) * ((H + 1) / 2);   // I420 / YV12
    bool okPl = l.paso == 1 ? pl.size() == 3 && pl[1].components[0].type.value == 2 && pl[2].components[0].type.value == 4 &&
                              pl[1].offsetInBytes == cb1 && pl[1].strideInBytes == sdg::alinear(S / 2, 16)
                            : pl.size() == 2 && pl[1].sampleIncrementInBits == 16 && pl[1].offsetInBytes == (int64_t)S * H &&
                              pl[1].components[0].type.value == 2 && pl[1].components[0].offsetInBits == (FMT == 0x11 ? 8 : 0);   // CB
    const int Y[2] = {63, 32}, U[2] = {102, 240}, V[2] = {240, 118};
    double t0 = 0, peor = 0;
    for (int i = 0; i <= VUELTAS; i++) {
        void *p = nullptr;
        if (lock(yb, 0x30, {0, 0, W, H}, -1, &p)) { printf("FALLO: lock\n"); return 1; }
        auto *m = (uint8_t *)p;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) m[l.y + (size_t)y * l.sy + x] = Y[x >= W / 2];
        for (int y = 0; y < (H + 1) / 2; y++) for (int x = 0; x < (W + 1) / 2; x++) {
            m[l.cb + (size_t)y * l.sc + (size_t)x * l.paso] = U[2 * x >= W / 2]; m[l.cr + (size_t)y * l.sc + (size_t)x * l.paso] = V[2 * x >= W / 2];
        }
        int fence; double a = ms();
        if (unlock(yb, &fence)) { printf("FALLO: unlock\n"); return 1; }
        double dt = ms() - a; if (i) { t0 += dt; peor = std::max(peor, dt); }   // la primera crea la cola y el UPLOAD
    }
    void *q = nullptr;
    if (lock(rb, 0x3, {0, 0, W, H}, -1, &q)) { printf("FALLO: lectura\n"); return 1; }
    int fallos = !okPl;
    for (int lado = 0; lado < 2; lado++) {
        double c = 1.164 * (Y[lado] - 16), u = U[lado] - 128, v = V[lado] - 128;
        const double *k = H >= 720 ? (const double[]){1.793, 0.213, 0.533, 2.112} : (const double[]){1.596, 0.391, 0.813, 2.018};
        double ref[3] = {c + k[0] * v, c - k[1] * u - k[2] * v, c + k[3] * u};
        auto *px = (uint8_t *)q + (size_t)(H / 2) * S * 4 + (size_t)(lado ? W * 3 / 4 : W / 4) * 4;
        for (int k = 0; k < 3; k++) if (std::abs(px[k] - std::clamp(ref[k], 0.0, 255.0)) > 2) fallos++;
        if (px[3] != 255) fallos++;
        if (lado == 0) printf("fmt 0x%x %dx%d: planos %s, RGBA %d %d %d (ref %.0f %.0f %.0f)", FMT, W, H, okPl ? "ok" : "MAL",
                              px[0], px[1], px[2], std::clamp(ref[0], 0.0, 255.0), std::clamp(ref[1], 0.0, 255.0), std::clamp(ref[2], 0.0, 255.0));
    }
    int fence; unlock(rb, &fence);
    printf(", unlock media %.2f ms peor %.2f ms: %s\n", VUELTAS ? t0 / VUELTAS : 0, peor, fallos ? "MAL" : "ok");
    freeBuffer(yb); freeBuffer(rb);
    return fallos;
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "seccomp")) seccomp();
    ID3D12Device *d = sdg::dispositivo();
    if (!d) { printf("FALLO: sin D3D12\n"); return 1; }
    int fallos = probar(d, 0x32315659, 1920, 1080, 30) + probar(d, 0x23, 1280, 720, 5) + probar(d, 0x23, 1918, 1082, 3) +
                 probar(d, 0x11, 641, 361, 3) +
                 probar(d, 0x32315659, 637, 479, 3);
    // Handles forjados (frontera de confianza): import o lock deben rechazarlos.
    const int W = 640, H = 360, S = 640; sdg::Yuv l; sdg::yuv(0x32315659, S, H, &l);
    int fd = (int)syscall(__NR_memfd_create, "corto", 0); ftruncate(fd, 4096);   // el fd es más corto que lo que dice el handle
    buffer_handle_t b;
    int malos = !importBuffer(handle(dup(fd), dup(fd), W, H, 0x32315659, S, l.tam - 1, sdg::T_YUV_GPU, 0x33), &b) +   // tam corto
                !importBuffer(handle(dup(fd), dup(fd), W, H, 0x32315659, S + 16, l.tam * 2, sdg::T_YUV_GPU, 0x33), &b) +   // stride
                !importBuffer(handle(dup(fd), dup(fd), W, H, 1, S, l.tam, sdg::T_YUV_GPU, 0x33), &b) +   // RGBA como YUV
                !importBuffer(handle(dup(fd), -1, W, 0, 0x32315659, S, l.tam, sdg::T_MEMFD, 0x33), &b);   // alto 0
    void *p = nullptr;
    if (importBuffer(handle(dup(fd), -1, W, H, 0x32315659, S, l.tam, sdg::T_MEMFD, 0x33), &b) || !lock(b, 0x3, {0, 0, W, H}, -1, &p)) malos++;
    printf("handles forjados: %s\n", malos ? "ACEPTADOS (MAL)" : "rechazados");
    printf("%s\n", fallos || malos ? "PRUEBA FALLIDA" : "PRUEBA OK");
    return fallos || malos;
}
