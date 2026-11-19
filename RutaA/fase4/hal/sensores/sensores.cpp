// HAL de sensores de SurfaceDual (AIDL android.hardware.sensors v3, ISensors/default): acelerómetro, giroscopio,
// magnetómetro y luz reales de la Surface. Los lee HvSensores.exe (Windows) y los manda por vsock 5009 SOLO mientras
// alguna app de Android tiene el sensor activo: sin oyentes, Windows apaga el sensor y aquí todo duerme en recv/accept.
// Android saca de estos (SensorFusion de sensorservice) gravedad, vector de rotación, orientación y aceleración lineal.
//
// Canal (el HAL escucha y Windows se conecta, como cámara y audio: así Windows no registra un servicio de Hyper-V, que
// pide administrador):
//   Windows -> Android al conectar: 8 dígitos hex con el largo + texto, una línea por sensor presente ("acel\n"...).
//   Android -> Windows: "<clave> <periodo en µs>\n" (0 = apagar) al activar, desactivar o cambiar la frecuencia.
//   Windows -> Android: registros de 24 bytes {u32 handle, f32 x, y, z, i64 edad en µs} ya en unidades y ejes de Android.
//
// Sin libfmq (C++ de la plataforma, no casa con la libc++ del NDK): la cola de eventos FMQ se escribe a mano con el mismo
// formato que libfmq (MessageQueueBase.h: punteros de lectura/escritura en bytes, anillo y palabra de EventFlag).
#include <aidl/android/hardware/sensors/BnSensors.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/log.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, "sensores.surfacedual", __VA_ARGS__)
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, "sensores.surfacedual", __VA_ARGS__)

namespace {
using namespace aidl::android::hardware::sensors;
using aidl::android::hardware::common::fmq::MQDescriptor;
using aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ndk::ScopedAStatus;
using Payload = Event::EventPayload;

constexpr unsigned kPuerto = 5009;
constexpr int kCampos = 24;   // bytes por registro de Windows

struct Def {
    int32_t handle;
    const char* clave;   // nombre en el canal
    SensorType tipo;
    const char* nombre;
    float rango, resolucion, potenciaMa;
    int32_t minUs, maxUs, flags;
};
// Rangos de los sensores de una tableta (±8 g, ±2000 °/s); frecuencias según MinimumReportInterval de Windows (acelerómetro
// y giroscopio 2 ms, magnetómetro 10 ms, luz 250 ms; medido 18/11) con tope de 200 Hz: Android no pide más a una tableta.
const Def kSensores[] = {
    {1, "acel", SensorType::ACCELEROMETER, "Acelerometro (Surface)", 78.4532f, 0.0024f, 0.15f, 5000, 1000000,
     SensorInfo::SENSOR_FLAG_BITS_CONTINUOUS_MODE},
    {2, "giro", SensorType::GYROSCOPE, "Giroscopio (Surface)", 34.9066f, 0.0011f, 0.5f, 5000, 1000000,
     SensorInfo::SENSOR_FLAG_BITS_CONTINUOUS_MODE},
    {3, "magnet", SensorType::MAGNETIC_FIELD, "Magnetometro (Surface)", 4900.0f, 0.15f, 0.1f, 20000, 1000000,
     SensorInfo::SENSOR_FLAG_BITS_CONTINUOUS_MODE},
    {4, "luz", SensorType::LIGHT, "Sensor de luz (Surface)", 100000.0f, 1.0f, 0.1f, 250000, 0,
     SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE},
    // Giro de pantalla ya decidido por Windows (SimpleOrientationSensor, solo avisa al cambiar). Con este sensor,
    // WindowOrientationListener usa OrientationSensorJudge en vez de leer el acelerómetro a 15 Hz todo el rato (medido
    // 18/11: 0,42 % de un núcleo en Windows solo para el giro automático). Sin wake-up: no hace falta la cola de wakelocks.
    {5, "orient", SensorType::DEVICE_ORIENTATION, "Orientacion (Windows)", 3.0f, 1.0f, 0.01f, 0, 0,
     SensorInfo::SENSOR_FLAG_BITS_ON_CHANGE_MODE},
};
constexpr int kN = sizeof(kSensores) / sizeof(kSensores[0]);
const Def* def(int32_t h) { return h >= 1 && h <= kN ? &kSensores[h - 1] : nullptr; }

int64_t ahoraNs() {   // elapsedRealtimeNano: el reloj de los eventos de sensores
    timespec t;
    clock_gettime(CLOCK_BOOTTIME, &t);
    return int64_t(t.tv_sec) * 1000000000 + t.tv_nsec;
}

// Cola FMQ sincronizada de eventos (lado escritor). Frontera de confianza: el descriptor llega de system_server; se
// comprueba su forma antes de mapear.
class Cola {
public:
    bool mapear(const MQDescriptor<Event, SynchronizedReadWrite>& d) {
        desmapear();
        if (d.quantum != int32_t(sizeof(Event)) || d.grantors.size() < 4 || d.handle.fds.empty()) return false;
        void* p[4];
        for (int i = 0; i < 4; ++i) {
            const auto& g = d.grantors[i];
            if (g.fdIndex < 0 || g.fdIndex >= int(d.handle.fds.size()) || g.offset < 0 || g.extent <= 0 ||
                g.extent > (64 << 20) || (g.offset & 7)) { desmapear(); return false; }
            const long pag = sysconf(_SC_PAGESIZE);
            const off_t base = off_t(g.offset / pag * pag);
            const size_t largo = size_t(g.offset - base + g.extent);
            void* m = mmap(nullptr, largo, PROT_READ | PROT_WRITE, MAP_SHARED, d.handle.fds[g.fdIndex].get(), base);
            if (m == MAP_FAILED) { desmapear(); return false; }
            mapas_.push_back({m, largo});
            p[i] = static_cast<uint8_t*>(m) + (g.offset - base);
        }
        tam_ = size_t(d.grantors[2].extent);
        if (tam_ % sizeof(Event) || d.grantors[0].extent < 8 || d.grantors[1].extent < 8 || d.grantors[3].extent < 4) {
            desmapear();
            return false;
        }
        lee_ = static_cast<std::atomic<uint64_t>*>(p[0]);
        escribe_ = static_cast<std::atomic<uint64_t>*>(p[1]);
        anillo_ = static_cast<uint8_t*>(p[2]);
        bandera_ = static_cast<std::atomic<uint32_t>*>(p[3]);
        return true;
    }
    // Sin sitio se tira (system_server va atrasado): nunca se bloquea el hilo del canal.
    bool escribir(const Event* ev, size_t n) {
        if (!anillo_) return false;
        const size_t bytes = n * sizeof(Event);
        const uint64_t w = escribe_->load(std::memory_order_relaxed), r = lee_->load(std::memory_order_acquire);
        if (w - r > tam_ || tam_ - (w - r) < bytes || w % sizeof(Event)) return false;
        const size_t o = size_t(w % tam_), primero = std::min(bytes, tam_ - o);
        memcpy(anillo_ + o, ev, primero);
        memcpy(anillo_, reinterpret_cast<const uint8_t*>(ev) + primero, bytes - primero);
        escribe_->store(w + bytes, std::memory_order_release);
        const uint32_t bit = ISensors::EVENT_QUEUE_FLAG_BITS_READ_AND_PROCESS;
        if (!(bandera_->fetch_or(bit) & bit))   // como EventFlag::wake
            syscall(__NR_futex, bandera_, FUTEX_WAKE_BITSET, INT32_MAX, nullptr, nullptr, bit);
        return true;
    }
    ~Cola() { desmapear(); }
private:
    void desmapear() {
        for (auto& m : mapas_) munmap(m.first, m.second);
        mapas_.clear();
        anillo_ = nullptr;
    }
    std::vector<std::pair<void*, size_t>> mapas_;
    std::atomic<uint64_t>* lee_ = nullptr;
    std::atomic<uint64_t>* escribe_ = nullptr;
    std::atomic<uint32_t>* bandera_ = nullptr;
    uint8_t* anillo_ = nullptr;
    size_t tam_ = 0;
};

class Sensores : public BnSensors {
public:
    // Presentes según la lista de Windows (fija desde el primer getSensorsList; sensorservice no la vuelve a pedir).
    std::atomic<uint32_t> presentes{0};

    ScopedAStatus getSensorsList(std::vector<SensorInfo>* out) override {
        out->clear();
        for (const auto& s : kSensores) {
            if (!(presentes & (1u << s.handle))) continue;
            SensorInfo i;
            i.sensorHandle = s.handle;
            i.name = s.nombre;
            i.vendor = "Microsoft (Windows)";
            i.version = 1;
            i.type = s.tipo;
            i.maxRange = s.rango;
            i.resolution = s.resolucion;
            i.power = s.potenciaMa;
            i.minDelayUs = s.minUs;
            i.maxDelayUs = s.maxUs;
            i.flags = s.flags;
            out->push_back(std::move(i));
        }
        return ScopedAStatus::ok();
    }
    ScopedAStatus setOperationMode(ISensors::OperationMode m) override {
        return m == ISensors::OperationMode::NORMAL ? ScopedAStatus::ok()
                                                     : ScopedAStatus::fromServiceSpecificError(ERROR_BAD_VALUE);
    }
    ScopedAStatus activate(int32_t h, bool on) override {
        if (!def(h) || !(presentes & (1u << h))) return ScopedAStatus::fromServiceSpecificError(ERROR_BAD_VALUE);
        std::lock_guard<std::mutex> g(mx_);
        activos_[h - 1] = on;
        pedirLocked(h);
        return ScopedAStatus::ok();
    }
    ScopedAStatus batch(int32_t h, int64_t periodoNs, int64_t /*latencia: sin FIFO, se entrega al momento*/) override {
        const Def* s = def(h);
        if (!s || !(presentes & (1u << h)) || periodoNs < 0) return ScopedAStatus::fromServiceSpecificError(ERROR_BAD_VALUE);
        std::lock_guard<std::mutex> g(mx_);
        int64_t us = periodoNs / 1000;
        if (us < s->minUs) us = s->minUs;
        if (s->maxUs && us > s->maxUs) us = s->maxUs;
        periodoUs_[h - 1] = int32_t(us);
        if (activos_[h - 1]) pedirLocked(h);
        return ScopedAStatus::ok();
    }
    ScopedAStatus flush(int32_t h) override {
        if (!def(h) || !(presentes & (1u << h))) return ScopedAStatus::fromServiceSpecificError(ERROR_BAD_VALUE);
        Event e;
        e.sensorHandle = h;
        e.sensorType = SensorType::META_DATA;
        Payload::MetaData m;
        m.what = Payload::MetaData::MetaDataEventType::META_DATA_FLUSH_COMPLETE;
        e.payload.set<Payload::Tag::meta>(m);
        std::lock_guard<std::mutex> g(colaMx_);
        cola_.escribir(&e, 1);
        return ScopedAStatus::ok();
    }
    ScopedAStatus initialize(const MQDescriptor<Event, SynchronizedReadWrite>& eventos,
                             const MQDescriptor<int32_t, SynchronizedReadWrite>& /*wakeLock: sin sensores wake-up*/,
                             const std::shared_ptr<ISensorsCallback>& /*sin sensores dinámicos*/) override {
        {
            std::lock_guard<std::mutex> g(mx_);   // system_server reinició: empieza sin nada activo
            for (int i = 0; i < kN; ++i) { activos_[i] = false; pedirLocked(i + 1); }
        }
        std::lock_guard<std::mutex> g(colaMx_);
        if (!cola_.mapear(eventos)) {
            LOG_E("cola de eventos inválida (quantum %d, %zu grantors)", eventos.quantum, eventos.grantors.size());
            return ScopedAStatus::fromServiceSpecificError(ERROR_BAD_VALUE);
        }
        return ScopedAStatus::ok();
    }
    ScopedAStatus injectSensorData(const Event&) override { return noHay(); }
    ScopedAStatus registerDirectChannel(const ISensors::SharedMemInfo&, int32_t*) override { return noHay(); }
    ScopedAStatus unregisterDirectChannel(int32_t) override { return noHay(); }
    ScopedAStatus configDirectReport(int32_t, int32_t, ISensors::RateLevel, int32_t*) override { return noHay(); }

    // Hilo del canal: conexión nueva de Windows -> volver a pedirle lo activo.
    void conectado(int fd) {
        std::lock_guard<std::mutex> g(mx_);
        fd_ = fd;
        for (int h = 1; h <= kN; ++h) if (activos_[h - 1]) pedirLocked(h);
    }
    void desconectado() { std::lock_guard<std::mutex> g(mx_); fd_ = -1; }

    // Registro de Windows -> evento de Android (frontera de confianza: handle presente y valores finitos).
    bool publicar(const uint8_t* r) {
        int32_t h;
        float v[3];
        int64_t edadUs;
        memcpy(&h, r, 4);
        memcpy(v, r + 4, 12);
        memcpy(&edadUs, r + 16, 8);
        const Def* s = def(h);
        if (!s || !(presentes & (1u << h)) || edadUs < 0 || edadUs > 10000000 ||
            !std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2])) return false;
        Event e;
        e.timestamp = ahoraNs() - edadUs * 1000;
        e.sensorHandle = h;
        e.sensorType = s->tipo;
        if (s->tipo == SensorType::DEVICE_ORIENTATION && !(v[0] == 0 || v[0] == 1 || v[0] == 2 || v[0] == 3)) return false;
        if (s->tipo == SensorType::LIGHT || s->tipo == SensorType::DEVICE_ORIENTATION) {
            e.payload.set<Payload::Tag::scalar>(v[0]);
        } else {
            Payload::Vec3 x;
            x.x = v[0]; x.y = v[1]; x.z = v[2];
            x.status = SensorStatus::ACCURACY_HIGH;
            e.payload.set<Payload::Tag::vec3>(x);
        }
        std::lock_guard<std::mutex> g(colaMx_);
        cola_.escribir(&e, 1);
        return true;
    }

private:
    static ScopedAStatus noHay() { return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION); }
    void pedirLocked(int32_t h) {
        if (fd_ < 0) return;
        char l[48];
        const int n = snprintf(l, sizeof(l), "%s %d\n", kSensores[h - 1].clave,
                               activos_[h - 1] ? std::max<int32_t>(periodoUs_[h - 1], 1) : 0);
        // Sin bloquear: con Windows atascado no se retiene mx_ (y con él a sensorservice); la orden es corta.
        if (send(fd_, l, n, MSG_NOSIGNAL | MSG_DONTWAIT) != n) LOG_E("no se pudo pedir %s a Windows", kSensores[h - 1].clave);
    }
    std::mutex mx_;
    bool activos_[kN] = {};
    int32_t periodoUs_[kN] = {66667, 66667, 66667, 250000, 200000};   // SENSOR_DELAY_NORMAL hasta que llegue batch()
    int fd_ = -1;
    std::mutex colaMx_;
    Cola cola_;
};

bool leerTodo(int fd, void* p, size_t n) {
    auto* b = static_cast<uint8_t*>(p);
    while (n) {
        const ssize_t r = recv(fd, b, n, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        b += r; n -= size_t(r);
    }
    return true;
}

// Lista de Windows: líneas con la clave de cada sensor presente. Devuelve la máscara (bit = handle).
uint32_t analizarLista(const std::string& t) {
    uint32_t m = 0;
    size_t i = 0;
    while (i < t.size()) {
        size_t f = t.find('\n', i);
        if (f == std::string::npos) f = t.size();
        const std::string c = t.substr(i, f - i);
        for (const auto& s : kSensores) if (c == s.clave) m |= 1u << s.handle;
        i = f + 1;
    }
    return m;
}

void canal(std::shared_ptr<Sensores> hal, std::function<void()> listo) {
    int s;
    for (;;) {   // vsock puede no estar listo en el primer instante del arranque
        s = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
        sockaddr_vm a = {};
        a.svm_family = AF_VSOCK;
        a.svm_port = kPuerto;
        a.svm_cid = VMADDR_CID_ANY;
        if (s >= 0 && !bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) && !listen(s, 1)) break;
        LOG_E("vsock %u: %s", kPuerto, strerror(errno));
        if (s >= 0) close(s);
        sleep(5);
    }
    for (;;) {
        sockaddr_vm p = {};
        socklen_t pl = sizeof(p);
        const int c = accept4(s, reinterpret_cast<sockaddr*>(&p), &pl, SOCK_CLOEXEC);
        if (c < 0) { if (errno != EINTR) sleep(1); continue; }
        // Frontera de confianza: solo Windows (CID 2) y lista corta de texto.
        char cab[9] = {};
        unsigned n = 0;
        std::string lista;
        const timeval t = {3, 0};
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
        setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));
        bool bien = p.svm_cid == VMADDR_CID_HOST && leerTodo(c, cab, 8) && sscanf(cab, "%8x", &n) == 1 && n <= 1024;
        if (bien) { lista.resize(n); bien = leerTodo(c, lista.data(), n); }
        if (!bien) { close(c); continue; }
        if (!hal->presentes) hal->presentes = analizarLista(lista);
        LOG_I("HvSensores conectado (sensores 0x%x)", unsigned(hal->presentes));
        listo();
        const timeval sin = {0, 0};   // los datos llegan solo con sensores activos: esperar sin límite
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &sin, sizeof(sin));
        hal->conectado(c);
        uint8_t r[kCampos];
        while (leerTodo(c, r, sizeof(r)))
            if (!hal->publicar(r)) LOG_E("registro inválido de Windows (descartado)");   // tamaño fijo: se sigue en sincronía
        hal->desconectado();
        close(c);
    }
}

// Autocomprobación ("--prueba", en la VM): la cola escrita a mano se lee igual que la leería libfmq.
int prueba() {
    int fallos = 0;
    auto mal = [&fallos](bool c, const char* q) { if (!c) { printf("FALLO: %s\n", q); ++fallos; } };
    const size_t cap = 3 * sizeof(Event);
    const int fd = memfd_create("prueba", MFD_CLOEXEC);
    mal(fd >= 0 && ftruncate(fd, 4096) == 0, "memfd");
    MQDescriptor<Event, SynchronizedReadWrite> d;
    d.quantum = sizeof(Event);
    d.grantors = {{0, 0, 8}, {0, 8, 8}, {0, 64, int64_t(cap)}, {0, 16, 4}};
    d.handle.fds.emplace_back(dup(fd));
    Cola q;
    mal(q.mapear(d), "mapear");
    auto* m = static_cast<uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    Event e;
    e.sensorHandle = 1;
    e.payload.set<Payload::Tag::scalar>(7.0f);
    for (int i = 0; i < 3; ++i) mal(q.escribir(&e, 1), "escribir");
    mal(!q.escribir(&e, 1), "llena rechaza");
    uint64_t w, r = 2 * sizeof(Event);
    memcpy(&w, m + 8, 8);
    mal(w == cap && *reinterpret_cast<uint32_t*>(m + 16) == 1u, "puntero y bandera");
    memcpy(m, &r, 8);   // el lector consumió 2: el 4.º y 5.º dan la vuelta al anillo
    mal(q.escribir(&e, 1) && q.escribir(&e, 1) && !q.escribir(&e, 1), "vuelta");
    Event leido;
    memcpy(&leido, m + 64 + sizeof(Event), sizeof(Event));   // índice 1 = el 5.º escrito
    mal(leido.sensorHandle == 1 && leido.payload.get<Payload::Tag::scalar>() == 7.0f, "contenido");
    d.quantum = 8;
    mal(!q.mapear(d), "quantum falso rechazado");
    mal(analizarLista("acel\ngiro\nluz\nxx\norient\n") == 0x36, "lista");
    printf("%s (%d fallos), sizeof(Event)=%zu\n", fallos ? "MAL" : "OK", fallos, sizeof(Event));
    return fallos != 0;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "--prueba")) return prueba();
    ABinderProcess_setThreadPoolMaxThreadCount(2);
    ABinderProcess_startThreadPool();
    auto hal = ndk::SharedRefBase::make<Sensores>();
    std::mutex m;
    std::condition_variable cv;
    bool listo = false;
    std::thread(canal, hal, [&] { std::lock_guard<std::mutex> g(m); listo = true; cv.notify_all(); }).detach();
    {
        // HvSensores se conecta en cuanto la VM escucha; si no llega en 3 s se anuncian los cuatro de la Surface
        // (sensorservice pide la lista una sola vez y espera al HAL declarado: no se puede bloquear el arranque).
        std::unique_lock<std::mutex> l(m);
        if (!cv.wait_for(l, std::chrono::seconds(3), [&] { return listo; })) {
            // shortcut: lista fija si Windows no contestó a tiempo (vale para la Surface Pro 11, que tiene los cuatro); pasar a sensores dinámicos si se usa en otro equipo con sensores distintos.
            uint32_t todos = 0;
            for (const auto& s : kSensores) todos |= 1u << s.handle;
            uint32_t cero = 0;
            hal->presentes.compare_exchange_strong(cero, todos);
        }
    }
    const std::string inst = std::string(Sensores::descriptor) + "/default";
    if (AServiceManager_addService(hal->asBinder().get(), inst.c_str()) != STATUS_OK) {
        LOG_E("no se pudo registrar %s", inst.c_str());
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 1;
}
