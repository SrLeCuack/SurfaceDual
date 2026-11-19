#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sound/asound.h>
#include <time.h>
#include <unistd.h>

#define FRAMES 480
#define LOG(...) __android_log_print(ANDROID_LOG_ERROR, "sdaudio", __VA_ARGS__)
_Static_assert(sizeof(uint32_t) * 4 == 16, "cabecera SDAU");

static void exact(struct snd_pcm_hw_params *p, int key, unsigned value)
{
    struct snd_interval *i = &p->intervals[key - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    i->min = i->max = value;
    i->integer = 1;
}

// Abre un PCM de aloop con formato fijo. Captura: arranca ya; reproducción: arranca sola con `start` cuadros.
static int open_pcm(const char *path, int flags, unsigned rate, unsigned channels, unsigned frames, unsigned start)
{
    int fd = open(path, flags | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;
    struct snd_pcm_info info = {0};
    if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info) < 0) goto fail;
    // Solo la subcorriente cero está emparejada con la del HAL (aloop une subcorrientes del mismo número).
    if (info.subdevice != 0) { errno = EBUSY; goto fail; }
    struct snd_pcm_hw_params p = {0};
    for (unsigned n = 0; n < sizeof(p.masks) / sizeof(p.masks[0]); n++)
        for (unsigned b = 0; b < sizeof(p.masks[n].bits) / sizeof(p.masks[n].bits[0]); b++)
            p.masks[n].bits[b] = ~0U;
    for (unsigned n = 0; n < sizeof(p.intervals) / sizeof(p.intervals[0]); n++)
        p.intervals[n].max = ~0U;
    int keys[] = {SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_HW_PARAM_SUBFORMAT};
    unsigned values[] = {SNDRV_PCM_ACCESS_RW_INTERLEAVED, SNDRV_PCM_FORMAT_S16_LE, SNDRV_PCM_SUBFORMAT_STD};
    for (unsigned n = 0; n < 3; n++) {
        struct snd_mask *m = &p.masks[keys[n] - SNDRV_PCM_HW_PARAM_FIRST_MASK];
        memset(m, 0, sizeof(*m));
        m->bits[values[n] / 32] = 1U << (values[n] % 32);
    }
    exact(&p, SNDRV_PCM_HW_PARAM_CHANNELS, channels);
    exact(&p, SNDRV_PCM_HW_PARAM_RATE, rate);
    exact(&p, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, frames);
    exact(&p, SNDRV_PCM_HW_PARAM_PERIODS, 4);
    exact(&p, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, frames * 4);
    p.rmask = ~0U;
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &p) < 0) goto fail;
    struct snd_pcm_sw_params sw = {0};
    sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sw.period_step = 1;
    sw.avail_min = frames;
    sw.start_threshold = start;
    sw.stop_threshold = frames * 4;
    sw.boundary = frames * 4;
    while (sw.boundary <= (unsigned long)(LONG_MAX / 2 - frames * 4)) sw.boundary *= 2;
    if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw) < 0 || ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0 ||
        ((flags & O_ACCMODE) == O_RDONLY && ioctl(fd, SNDRV_PCM_IOCTL_START) < 0)) goto fail;
    return fd;
fail:;
    int saved = errno;
    close(fd);
    errno = saved;
    return -1;
}

static int capture(unsigned frames) { return open_pcm("/dev/snd/pcmC0D1c", O_RDONLY, 48000, 2, frames, 1); }

static int send_all(int fd, const void *data, size_t size)
{
    const char *p = data;
    while (size) {
        ssize_t n = send(fd, p, size, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n; size -= (size_t)n;
    }
    return 0;
}

// Micrófono (vsock 5004): Android manda {SDMI, tasa, canales, 16} al empezar a grabar y cada 100 ms mientras graba
// (si la VM se pausa dejan de llegar y Windows suelta el micro); tasa 0 = dejó de grabar. Windows manda PCM S16 con
// ese formato, que entra por pcmC0D1p y el HAL lo lee en pcmC0D0c (pareja de aloop).
#define MIC_MAGIC 0x494d4453

// Formato con el que el HAL tiene configurada la captura, del texto de /proc ("closed" si no graba).
static int parse_hw(const char *text, unsigned *rate, unsigned *channels)
{
    const char *f = strstr(text, "\nformat: "), *c = strstr(text, "\nchannels: "), *r = strstr(text, "\nrate: ");
    if (!f || !c || !r || strncmp(f + 9, "S16_LE\n", 7)) return 0;
    *channels = (unsigned)strtoul(c + 11, NULL, 10);
    *rate = (unsigned)strtoul(r + 7, NULL, 10);
    return *rate >= 8000 && *rate <= 48000 && *channels >= 1 && *channels <= 2;
}

static int configured(const char *path, unsigned *rate, unsigned *channels)
{
    char text[512];
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    ssize_t n = read(fd, text, sizeof(text) - 1);
    close(fd);
    if (n <= 0) return 0;
    text[n] = 0;
    return parse_hw(text, rate, channels);
}

static long long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

// Lee los avisos de inotify del nodo del HAL. Al abrir, el HAL fija el formato unos ms después: mirar /proc
// hasta 50 veces (cada 20 ms); al cerrar, una vez.
static int notified(int watch, int pending)
{
    _Alignas(struct inotify_event) char events[sizeof(struct inotify_event) * 16];
    ssize_t n = read(watch, events, sizeof(events));
    for (char *p = events; n > 0 && p < events + n; p += sizeof(struct inotify_event) + ((struct inotify_event *)p)->len)
        pending = (((struct inotify_event *)p)->mask & IN_OPEN) ? 50 : pending ? pending : 1;
    return pending;
}

// Altavoz (vsock 5003): cabecera SDAU y PCM 48000/2/S16 solo mientras el HAL tiene abierta su salida (pcmC0D0p);
// sin sonido no se lee ni se envía nada y Windows pausa su dispositivo. AudioFlinger cierra la salida a los 3 s de
// silencio (medido: ~TinyalsaSink), pero una vez tras un vídeo el HAL la dejó abierta escribiendo ceros: tras 2 s de
// ceros no se envía nada y se lee en bloques de 100 ms (10 despertares/s en vez de 100) hasta la primera muestra con
// sonido, que se manda con hasta 100 ms de retraso (Windows lo recorta).
static void stream(int host, int watch, int listener)
{
    const uint32_t header[] = {0x55414453, 48000, 2, 16};
    struct timeval timeout = {.tv_sec = 1};
    if (setsockopt(host, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG("SO_SNDTIMEO: %s", strerror(errno)); return;
    }
    if (send_all(host, header, sizeof(header))) return;
    int pcm = -1, pending = 1, silent = 0, hal = 0;
    unsigned frames = FRAMES;
    static int16_t samples[FRAMES * 10 * 2];
    for (;;) {
        if (pending || (hal && pcm < 0)) {   // si la captura falló, se reintenta cada 1 s mientras el HAL siga abierto
            unsigned r, c;
            hal = configured("/proc/asound/card0/pcm0p/sub0/hw_params", &r, &c);
            pending = hal ? 0 : pending - 1;
            if (hal && pcm < 0 && (pcm = capture(frames)) < 0) LOG("captura 48000/2/S16_LE (HAL %u/%u): %s", r, c, strerror(errno));
            if (!hal && pcm >= 0) { close(pcm); pcm = -1; frames = FRAMES; silent = 0; }
        }
        struct pollfd fds[] = {{host, POLLIN, 0}, {watch, POLLIN, 0}, {pcm, POLLIN, 0}, {listener, POLLIN, 0}};
        int ready = poll(fds, 4, pending ? 20 : hal && pcm < 0 ? 1000 : -1);
        if (ready < 0) { if (errno == EINTR) continue; break; }
        if (fds[0].revents || fds[3].revents) break;   // Windows cerró, o llama de nuevo (ver serve)
        if (fds[1].revents) pending = notified(watch, pending);
        if (pcm < 0 || !fds[2].revents) continue;
        if (!(fds[2].revents & (POLLIN | POLLERR))) break;
        struct snd_xferi x = {.buf = samples, .frames = frames};
        int rc = ioctl(pcm, SNDRV_PCM_IOCTL_READI_FRAMES, &x);
        int error = rc < 0 ? errno : x.result < 0 ? (int)-x.result : 0;
        if (error == EPIPE) {
            if (ioctl(pcm, SNDRV_PCM_IOCTL_PREPARE) == 0 && ioctl(pcm, SNDRV_PCM_IOCTL_START) == 0) continue;
        } else if (error == EAGAIN || error == EINTR) continue;
        else if (!error && x.result > 0 && x.result <= (long)frames) {
            int zero = 1;
            for (long i = 0; zero && i < x.result * 2; i++) zero = !samples[i];
            silent = zero ? (silent > 200 ? silent : silent + 1) : 0;
            if (silent <= 200 && send_all(host, samples, (size_t)x.result * 4)) break;
            // Cambio de ritmo: a bloques de 100 ms tras 2 s de ceros; de vuelta a 10 ms con el primer sonido.
            if ((silent > 200) != (frames != FRAMES)) {
                close(pcm);
                frames = silent > 200 ? FRAMES * 10 : FRAMES;
                if ((pcm = capture(frames)) < 0) LOG("captura (%u cuadros): %s", frames, strerror(errno));
            }
            continue;
        }
        LOG("captura: %s; reintento en 1 s", strerror(error ? error : EIO));
        close(pcm); pcm = -1;
    }
    if (pcm >= 0) close(pcm);
}

static void microphone(int host, int watch, int listener)
{
    struct timeval timeout = {.tv_sec = 1};
    if (setsockopt(host, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) return;
    static char data[48000 / 100 * 4 * 8];   // 80 ms del formato mayor
    size_t have = 0;
    unsigned rate = 0, channels = 0;
    int pcm = -1, pending = 1, changed = 0, failed = 0;
    long long alive = 0;
    for (;;) {
        if (pending || failed) {   // la captura se abrió o cerró (o falló abrir pcmC0D1p: reintento cada 1 s)
            unsigned r = 0, c = 0;
            if (!configured("/proc/asound/card0/pcm0c/sub0/hw_params", &r, &c)) r = c = 0;
            pending = r ? 0 : pending - 1;
            failed = 0;
            if (r != rate || c != channels) {
                if (pcm >= 0) { close(pcm); pcm = -1; }
                // Periodo de 10 ms y 4 periodos; arranca con 20 ms en cola para aguantar el vaivén de Windows.
                if (r && (pcm = open_pcm("/dev/snd/pcmC0D1p", O_WRONLY, r, c, r / 100, r / 50)) < 0) {
                    LOG("micro %u/%u: %s", r, c, strerror(errno)); r = c = 0; failed = 1;
                }
                changed = r != rate || c != channels;
                rate = r; channels = c; have = 0;
            }
        }
        long long t = now_ms();
        if (changed || (rate && t - alive >= 100)) {
            const uint32_t header[] = {MIC_MAGIC, rate, channels, 16};
            if (send_all(host, header, sizeof(header))) break;
            alive = t; changed = 0;
        }
        struct pollfd fds[] = {{host, POLLIN, 0}, {watch, POLLIN, 0}, {listener, POLLIN, 0}};
        int ready = poll(fds, 3, pending ? 20 : failed ? 1000 : rate ? 100 : -1);
        if (ready < 0) { if (errno == EINTR) continue; break; }
        if (fds[2].revents) break;   // Windows llama de nuevo (ver serve)
        if (fds[1].revents) pending = notified(watch, pending);
        if (!fds[0].revents) continue;
        ssize_t n = recv(host, data + have, sizeof(data) - have, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        have += (size_t)n;
        size_t frame = channels * 2, frames = frame ? have / frame : 0;
        if (pcm >= 0 && frames) {
            struct snd_xferi x = {.buf = data, .frames = frames};
            // Hueco de Windows (underrun): rearmar y seguir. Búfer lleno (EAGAIN o parcial): se descarta lo que sobra.
            if (ioctl(pcm, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x) < 0 && errno == EPIPE &&
                ioctl(pcm, SNDRV_PCM_IOCTL_PREPARE) == 0) ioctl(pcm, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x);
        }
        size_t used = frame ? frames * frame : have;
        memmove(data, data + used, have - used);
        have -= used;
    }
    if (pcm >= 0) close(pcm);
}

static int listen_vsock(unsigned port)
{
    int listener = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_vm address = {.svm_family = AF_VSOCK, .svm_port = port, .svm_cid = VMADDR_CID_ANY};
    if (listener < 0 || bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(listener, 1) < 0) {
        LOG("vsock %u: %s", port, strerror(errno));
        if (listener >= 0) close(listener);
        return -1;
    }
    return listener;
}

// Atiende al host de un puerto; inotify en el nodo del HAL para saber sin sondeo cuándo abre o cierra.
// La llamada nueva manda: si Windows cerró con la VM en pausa el aviso de cierre puede no llegar nunca, y con la conexión
// vieja colgada nadie atendía la nueva (sin sonido hasta reiniciar Android). El manejador vigila `listener` y sale.
struct canal { unsigned port; const char *node; void (*handler)(int host, int watch, int listener); };

static void *serve(void *arg)
{
    const struct canal *c = arg;
    int watch = inotify_init1(IN_CLOEXEC), listener = listen_vsock(c->port);
    if (watch < 0 || listener < 0 || inotify_add_watch(watch, c->node, IN_OPEN | IN_CLOSE) < 0) {
        LOG("canal %u (%s): %s", c->port, c->node, strerror(errno));
        if (watch >= 0) close(watch);
        if (listener >= 0) close(listener);
        return NULL;
    }
    for (;;) {
        int host = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
        if (host < 0) { if (errno == EINTR) continue; LOG("accept: %s", strerror(errno)); sleep(1); continue; }
        c->handler(host, watch, listener);
        close(host);
    }
}

int main(void)
{
    static const struct canal altavoz = {5003, "/dev/snd/pcmC0D0p", stream}, micro = {5004, "/dev/snd/pcmC0D0c", microphone};
    pthread_t thread;
    if ((errno = pthread_create(&thread, NULL, serve, (void *)&micro))) LOG("hilo del micro: %s", strerror(errno));
    serve((void *)&altavoz);
    return 1;
}
