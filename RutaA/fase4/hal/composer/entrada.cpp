// Entrada táctil del Visor (Windows) -> Android en la VM de Hyper-V: escucha vsock 5002 y crea una pantalla táctil
// multitoque por uinput (protocolo B de Linux). Vive en el proceso del composer (ya tiene vsock en la política propia).
// Mensaje del host (little-endian, 24 bytes): { uint32 magia "SDTQ"; uint32 tipo 0=abajo 1=mover 2=arriba;
//   uint32 dedo (0..9); int32 x, y (píxeles de la pantalla de Android); uint32 presion (0..1023, 0 = sin dato) }.
// Tipo 3 = tecla: dedo = código de tecla de Linux (KEY_*), x = 1 pulsada / 0 soltada (teclado uinput aparte).
// Tipo 4 = pluma: dedo = estado (1 en rango, 2 contacto, 4 borrador, 8 botón lateral), x, y; presion = presión (bits
//   0-15, 0..1024) | (inclinación X + 90) << 16 | (inclinación Y + 90) << 24.
#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/system_properties.h>
#include <thread>
#include <unistd.h>

#define ELOG(...) __android_log_print(ANDROID_LOG_ERROR, "hwc.hyperv.entrada", __VA_ARGS__)

#include <atomic>
extern std::atomic<bool> pantallaEncendida;   // composer.cpp (setPowerMode)

namespace {

struct Toque { uint32_t magia, tipo, dedo; int32_t x, y; uint32_t presion; };
static_assert(sizeof(Toque) == 24, "protocolo de entrada");
constexpr uint32_t MAGIA = 0x51544453;   // "SDTQ"
constexpr int DEDOS = 10;

bool emitir(int fd, int tipo, int codigo, int valor) {
    input_event e = {}; e.type = tipo; e.code = codigo; e.value = valor;
    return write(fd, &e, sizeof e) == sizeof e;
}

int crearTactil(int ancho, int alto) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { ELOG("uinput: %s", strerror(errno)); return -1; }
    ioctl(fd, UI_SET_EVBIT, EV_KEY); ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_EVBIT, EV_ABS); ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);   // pantalla táctil, no touchpad
    uinput_abs_setup a = {};
    auto eje = [&](int codigo, int min, int max) { a.code = codigo; a.absinfo = {}; a.absinfo.minimum = min; a.absinfo.maximum = max;
                                                   ioctl(fd, UI_SET_ABSBIT, codigo); ioctl(fd, UI_ABS_SETUP, &a); };
    eje(ABS_MT_SLOT, 0, DEDOS - 1); eje(ABS_MT_TRACKING_ID, 0, 65535);
    eje(ABS_MT_POSITION_X, 0, ancho - 1); eje(ABS_MT_POSITION_Y, 0, alto - 1); eje(ABS_MT_PRESSURE, 0, 1023);
    uinput_setup s = {}; s.id.bustype = BUS_VIRTUAL; s.id.vendor = 0x5344; s.id.product = 0x0001;
    snprintf(s.name, sizeof s.name, "SurfaceDual Touch");
    if (ioctl(fd, UI_DEV_SETUP, &s) || ioctl(fd, UI_DEV_CREATE)) { ELOG("UI_DEV_CREATE: %s", strerror(errno)); close(fd); return -1; }
    return fd;
}

int crearTeclado() {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { ELOG("uinput teclado: %s", strerror(errno)); return -1; }
    ioctl(fd, UI_SET_EVBIT, EV_KEY); ioctl(fd, UI_SET_EVBIT, EV_REP);
    for (int k = KEY_ESC; k <= KEY_MICMUTE; k++) ioctl(fd, UI_SET_KEYBIT, k);
    uinput_setup s = {}; s.id.bustype = BUS_VIRTUAL; s.id.vendor = 0x5344; s.id.product = 0x0002;
    snprintf(s.name, sizeof s.name, "SurfaceDual Keyboard");
    if (ioctl(fd, UI_DEV_SETUP, &s) || ioctl(fd, UI_DEV_CREATE)) { ELOG("teclado UI_DEV_CREATE: %s", strerror(errno)); close(fd); return -1; }
    return fd;
}

int crearPluma(int ancho, int alto) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { ELOG("uinput pluma: %s", strerror(errno)); return -1; }
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for (int k : {BTN_TOOL_PEN, BTN_TOOL_RUBBER, BTN_TOUCH, BTN_STYLUS}) ioctl(fd, UI_SET_KEYBIT, k);
    ioctl(fd, UI_SET_EVBIT, EV_ABS); ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
    uinput_abs_setup a = {};
    auto eje = [&](int codigo, int min, int max) { a.code = codigo; a.absinfo = {}; a.absinfo.minimum = min; a.absinfo.maximum = max;
                                                   ioctl(fd, UI_SET_ABSBIT, codigo); ioctl(fd, UI_ABS_SETUP, &a); };
    eje(ABS_X, 0, ancho - 1); eje(ABS_Y, 0, alto - 1); eje(ABS_PRESSURE, 0, 1024); eje(ABS_TILT_X, -90, 90); eje(ABS_TILT_Y, -90, 90);
    uinput_setup s = {}; s.id.bustype = BUS_VIRTUAL; s.id.vendor = 0x5344; s.id.product = 0x0003;
    snprintf(s.name, sizeof s.name, "SurfaceDual Pen");
    if (ioctl(fd, UI_DEV_SETUP, &s) || ioctl(fd, UI_DEV_CREATE)) { ELOG("pluma UI_DEV_CREATE: %s", strerror(errno)); close(fd); return -1; }
    return fd;
}

void servir() {
    char prop[PROP_VALUE_MAX] = {}; int ancho = 2880, alto = 1920, hz;
    __system_property_get("vendor.hyperv.pantalla", prop);
    sscanf(prop, "%dx%d@%d", &ancho, &alto, &hz);
    int tactil = crearTactil(ancho, alto), teclado = crearTeclado(), pluma = crearPluma(ancho, alto);
    int s = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_vm dir = {}; dir.svm_family = AF_VSOCK; dir.svm_cid = VMADDR_CID_ANY; dir.svm_port = 5002;
    if (tactil < 0 || s < 0 || bind(s, (sockaddr *)&dir, sizeof dir) || listen(s, 1)) { ELOG("vsock 5002: %s", strerror(errno)); return; }
    int idSiguiente = 1, pistas[DEDOS] = {};   // tracking id por dedo (0 = levantado)
    bool teclas[KEY_MICMUTE + 1] = {}, plumaEnRango = false;   // para soltarlas si se cae la conexión
    uint32_t tragados = 0;   // dedos del toque que despertó la pantalla (no se entregan)
    for (;;) {
        int c = accept4(s, nullptr, nullptr, SOCK_CLOEXEC);
        if (c < 0) { if (errno == EINTR) continue; ELOG("accept: %s", strerror(errno)); return; }
        Toque t;
        for (;;) {
            size_t leidos = 0;
            while (leidos < sizeof t) { ssize_t n = read(c, (char *)&t + leidos, sizeof t - leidos); if (n <= 0) break; leidos += n; }
            if (leidos < sizeof t || t.magia != MAGIA) break;
            if (t.tipo == 3) {   // tecla
                if (teclado >= 0 && t.dedo >= KEY_ESC && t.dedo <= KEY_MICMUTE) {
                    emitir(teclado, EV_KEY, (int)t.dedo, t.x ? 1 : 0); emitir(teclado, EV_SYN, SYN_REPORT, 0);
                    teclas[t.dedo] = t.x != 0;
                }
                continue;
            }
            if (t.tipo == 4) {   // pluma
                if (pluma >= 0) {
                    bool rango = t.dedo & 1, contacto = t.dedo & 2, borrador = t.dedo & 4;
                    emitir(pluma, EV_KEY, BTN_TOOL_PEN, rango && !borrador); emitir(pluma, EV_KEY, BTN_TOOL_RUBBER, rango && borrador);
                    emitir(pluma, EV_KEY, BTN_TOUCH, contacto); emitir(pluma, EV_KEY, BTN_STYLUS, (t.dedo & 8) != 0);
                    if (rango) {
                        emitir(pluma, EV_ABS, ABS_X, t.x); emitir(pluma, EV_ABS, ABS_Y, t.y);
                        emitir(pluma, EV_ABS, ABS_PRESSURE, contacto ? (int)(t.presion & 0xFFFF) : 0);
                        emitir(pluma, EV_ABS, ABS_TILT_X, (int)((t.presion >> 16) & 0xFF) - 90);
                        emitir(pluma, EV_ABS, ABS_TILT_Y, (int)(t.presion >> 24) - 90);
                    }
                    emitir(pluma, EV_SYN, SYN_REPORT, 0);
                    plumaEnRango = rango;
                }
                continue;
            }
            if (t.dedo >= DEDOS) break;
            // Pantalla apagada (dormir del launcher o tiempo de espera): el toque solo la enciende, como en un móvil; ese
            // gesto entero (y los dedos que se sumen) no llega a Android, para no pulsar a ciegas lo que haya debajo.
            bool alguno0 = false; for (int p : pistas) alguno0 |= p != 0;
            if (t.tipo == 0 && (tragados || (!alguno0 && !pantallaEncendida))) {
                if (!tragados && teclado >= 0) { emitir(teclado, EV_KEY, KEY_WAKEUP, 1); emitir(teclado, EV_KEY, KEY_WAKEUP, 0); emitir(teclado, EV_SYN, SYN_REPORT, 0); }
                tragados |= 1u << t.dedo; continue;
            }
            if (tragados & (1u << t.dedo)) { if (t.tipo == 2) tragados &= ~(1u << t.dedo); continue; }
            emitir(tactil, EV_ABS, ABS_MT_SLOT, (int)t.dedo);
            if (t.tipo == 2) {
                emitir(tactil, EV_ABS, ABS_MT_TRACKING_ID, -1); pistas[t.dedo] = 0;
            } else {
                if (!pistas[t.dedo]) {
                    pistas[t.dedo] = idSiguiente; idSiguiente = idSiguiente % 65535 + 1;
                    emitir(tactil, EV_ABS, ABS_MT_TRACKING_ID, pistas[t.dedo]);
                    // El kernel descarta un valor igual al anterior de la ranura: un toque nuevo en el mismo sitio saldría
                    // sin posición. Un valor distinto antes del real (mismo SYN) obliga a emitirla.
                    emitir(tactil, EV_ABS, ABS_MT_POSITION_X, t.x ? t.x - 1 : 1); emitir(tactil, EV_ABS, ABS_MT_POSITION_Y, t.y ? t.y - 1 : 1);
                }
                emitir(tactil, EV_ABS, ABS_MT_POSITION_X, t.x); emitir(tactil, EV_ABS, ABS_MT_POSITION_Y, t.y);
                emitir(tactil, EV_ABS, ABS_MT_PRESSURE, t.presion ? (int)t.presion : 512);
            }
            bool alguno = false; for (int p : pistas) alguno |= p != 0;
            emitir(tactil, EV_KEY, BTN_TOUCH, alguno); emitir(tactil, EV_SYN, SYN_REPORT, 0);
        }
        // Conexión caída: levantar dedos, teclas y pluma para que Android no quede con nada pegado (p. ej. Alt tras Alt+F4).
        for (int d = 0; d < DEDOS; d++) if (pistas[d]) { emitir(tactil, EV_ABS, ABS_MT_SLOT, d); emitir(tactil, EV_ABS, ABS_MT_TRACKING_ID, -1); pistas[d] = 0; }
        emitir(tactil, EV_KEY, BTN_TOUCH, 0); emitir(tactil, EV_SYN, SYN_REPORT, 0); tragados = 0;
        bool soltada = false;
        for (int k = KEY_ESC; k <= KEY_MICMUTE; k++) if (teclas[k]) { emitir(teclado, EV_KEY, k, 0); teclas[k] = false; soltada = true; }
        if (soltada) emitir(teclado, EV_SYN, SYN_REPORT, 0);
        if (plumaEnRango) {
            for (int k : {BTN_TOUCH, BTN_STYLUS, BTN_TOOL_PEN, BTN_TOOL_RUBBER}) emitir(pluma, EV_KEY, k, 0);
            emitir(pluma, EV_SYN, SYN_REPORT, 0); plumaEnRango = false;
        }
        close(c);
    }
}

}  // namespace

void iniciarEntrada() { std::thread(servir).detach(); }
