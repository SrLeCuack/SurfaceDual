// Medición de hv_sock (Android en la VM -> Windows): escucha en vsock y, al conectar el host, manda MB megabytes.
//   vsock_envio [puerto=5000] [MB=1024]
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int puerto = argc > 1 ? atoi(argv[1]) : 5000, mb = argc > 2 ? atoi(argv[2]) : 1024;
    int s = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }
    struct sockaddr_vm a = {0}; a.svm_family = AF_VSOCK; a.svm_cid = VMADDR_CID_ANY; a.svm_port = puerto;
    if (bind(s, (struct sockaddr *)&a, sizeof a) || listen(s, 1)) { perror("bind/listen"); return 1; }
    printf("escuchando vsock %d\n", puerto); fflush(stdout);
    int c = accept(s, NULL, NULL);
    if (c < 0) { perror("accept"); return 1; }
    static char buf[1 << 20];
    memset(buf, 0x5a, sizeof buf);
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    long long total = 0;
    for (int i = 0; i < mb; i++)
        for (size_t o = 0; o < sizeof buf;) { ssize_t n = send(c, buf + o, sizeof buf - o, 0); if (n <= 0) { perror("send"); return 1; } o += n; total += n; }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double seg = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("enviados %lld MB en %.3f s = %.0f MB/s\n", total >> 20, seg, total / 1048576.0 / seg);
    close(c); close(s);
    return 0;
}
