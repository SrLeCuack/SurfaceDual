// Prueba en Android (VM): decodifica un archivo con FFmpeg + VA-API (Mesa d3d12, GPU por /dev/dxg) o por software y
// mide fps, CPU del proceso y md5 de los cuadros (plano visible, NV12/YUV420P normalizados a Y + U + V).
//   /data/local/tmp/vprueba <archivo> [hw|sw|gpu] [max_cuadros] [dri=/data/local/tmp]
// Compilar: compilar_android.sh (al final). Comparar md5 hw contra sw: si son iguales, la GPU decodifica bien.
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/md5.h>
#include <libavutil/pixdesc.h>
#include <stdio.h>
#include <sys/resource.h>
#include <time.h>
#include "va_sin_drm.h"

static double ahora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static double cpu(void) { struct rusage u; getrusage(RUSAGE_SELF, &u); return u.ru_utime.tv_sec + u.ru_stime.tv_sec + (u.ru_utime.tv_usec + u.ru_stime.tv_usec) / 1e6; }

static enum AVPixelFormat elegir(AVCodecContext *c, const enum AVPixelFormat *f) {
    for (; *f != AV_PIX_FMT_NONE; f++) if (*f == AV_PIX_FMT_VAAPI) return *f;
    fprintf(stderr, "sin VAAPI para este flujo\n");
    return AV_PIX_FMT_NONE;
}

// md5 del área visible en orden Y, U, V (NV12 se desintercala) para comparar hw y sw.
static void sumar(struct AVMD5 *m, const AVFrame *f) {
    int w = f->width, h = f->height;
    for (int y = 0; y < h; y++) av_md5_update(m, f->data[0] + (size_t)y * f->linesize[0], w);
    uint8_t fila[8192];
    for (int p = 0; p < 2; p++)
        for (int y = 0; y < h / 2; y++) {
            if (f->format == AV_PIX_FMT_NV12) {
                const uint8_t *s = f->data[1] + (size_t)y * f->linesize[1];
                for (int x = 0; x < w / 2; x++) fila[x] = s[2 * x + p];
                av_md5_update(m, fila, w / 2);
            } else av_md5_update(m, f->data[1 + p] + (size_t)y * f->linesize[1 + p], w / 2);
        }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: vprueba <archivo> [hw|sw] [max] [dri]\n"); return 2; }
    int hw = argc < 3 || strcmp(argv[2], "sw"), solo = argc > 2 && !strcmp(argv[2], "gpu"), max = argc > 3 ? atoi(argv[3]) : 1 << 30;
    AVFormatContext *fc = NULL;
    if (avformat_open_input(&fc, argv[1], NULL, NULL) < 0 || avformat_find_stream_info(fc, NULL) < 0) { fprintf(stderr, "no abre\n"); return 1; }
    const AVCodec *cod; int vi = av_find_best_stream(fc, AVMEDIA_TYPE_VIDEO, -1, -1, &cod, 0);
    if (vi < 0) { fprintf(stderr, "sin vídeo\n"); return 1; }
    AVCodecContext *c = avcodec_alloc_context3(cod);
    avcodec_parameters_to_context(c, fc->streams[vi]->codecpar);
    if (hw) {
        double t = ahora();
        VADisplay d = sd_va_abrir(argc > 4 ? argv[4] : "/data/local/tmp");
        if (!d) { fprintf(stderr, "vaInitialize falló\n"); return 1; }
        fprintf(stderr, "VA: %s (%.0f ms)\n", vaQueryVendorString(d), (ahora() - t) * 1000);
        AVBufferRef *dev = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
        ((AVVAAPIDeviceContext *)((AVHWDeviceContext *)dev->data)->hwctx)->display = d;
        if (av_hwdevice_ctx_init(dev) < 0) { fprintf(stderr, "hwdevice\n"); return 1; }
        c->hw_device_ctx = dev; c->get_format = elegir; c->thread_count = 1;
    } else c->thread_count = 0;
    if (avcodec_open2(c, cod, NULL) < 0) { fprintf(stderr, "avcodec_open2\n"); return 1; }
    fprintf(stderr, "%s %dx%d %s\n", cod->name, c->width, c->height, hw ? "GPU (VA-API d3d12)" : "software");

    struct AVMD5 *md5 = av_md5_alloc(); av_md5_init(md5);
    AVPacket *pk = av_packet_alloc(); AVFrame *f = av_frame_alloc(), *s = av_frame_alloc();
    int n = 0, fin = 0; double t0 = ahora(), c0 = cpu();
    while (!fin && n < max) {
        int r = av_read_frame(fc, pk);
        if (r < 0) { avcodec_send_packet(c, NULL); fin = 1; }
        else if (pk->stream_index != vi) { av_packet_unref(pk); continue; }
        else { if (avcodec_send_packet(c, pk) < 0) fprintf(stderr, "send_packet falló\n"); av_packet_unref(pk); }
        while (n < max && avcodec_receive_frame(c, f) == 0) {
            const AVFrame *v = f;
            if (solo) { n++; av_frame_unref(f); continue; }   // "gpu": solo decodificar (sin bajar ni md5)
            if (f->format == AV_PIX_FMT_VAAPI) {   // bajar el cuadro a memoria (lo que haría el códec en modo CPU)
                s->format = AV_PIX_FMT_NV12;
                if (av_hwframe_map(s, f, AV_HWFRAME_MAP_READ) < 0) { fprintf(stderr, "map falló\n"); return 1; }
                v = s;
            }
            sumar(md5, v); n++;
            av_frame_unref(s); av_frame_unref(f);
        }
    }
    double t = ahora() - t0, uc = cpu() - c0;
    uint8_t dig[16]; av_md5_final(md5, dig);
    printf("cuadros=%d %.1f fps CPU=%.0f %% de un núcleo md5=", n, n / t, 100 * uc / t);
    for (int i = 0; i < 16; i++) printf("%02x", dig[i]);
    printf("\n");
    return 0;
}
