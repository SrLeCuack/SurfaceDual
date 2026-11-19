// Servicio android.hardware.graphics.allocator.IAllocator/default (v3) de SurfaceDual sobre D3D12 (ver comun.h).
// Reemplaza a android.hardware.graphics.allocator-service.ranchu (que exige la memoria "address space" de QEMU).
#include "comun.h"
#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <aidl/android/hardware/graphics/allocator/BnAllocator.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <errno.h>
#include <fcntl.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

using namespace aidl::android::hardware::graphics::allocator;
using aidl::android::hardware::common::NativeHandle;
using ndk::ScopedAStatus;

namespace {

ScopedAStatus error(AllocationError e) { return ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(e)); }

// Memoria de CPU compartible: dma-buf del montón "system". Un memfd de este HAL no lo pueden recibir las apps (SELinux:
// memfd_file de hal_graphics_allocator solo para servicios; medido 16/11, la BufferQueue de Fotos es de la app).
int memoriaCpu(const char *nombre, size_t tam) {
    static std::atomic<int> heap{-1};
    if (heap < 0) {   // se reintenta: un fallo al arrancar no debe dejar a las apps sin búferes de CPU
        int fd = open("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC), nada = -1;
        if (fd >= 0 && !heap.compare_exchange_strong(nada, fd)) close(fd);
    }
    dma_heap_allocation_data a = {tam, 0, O_RDWR | O_CLOEXEC, 0};
    if (heap >= 0 && ioctl(heap, DMA_HEAP_IOCTL_ALLOC, &a) == 0) return (int)a.fd;
    LOG_E("dma_heap: %s; memfd (solo servicios)", strerror(errno));
    int fd = (int)syscall(__NR_memfd_create, nombre, MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd >= 0 && ftruncate(fd, (off_t)tam) != 0) { close(fd); fd = -1; }
    return fd;
}

struct Plan { const sdg::Formato *f; sdg::Tipo tipo; int stride; uint64_t tam; };

// Decide cómo se reserva; false = no soportado (el llamador decide BAD_DESCRIPTOR o UNSUPPORTED).
bool planear(const BufferDescriptorInfo &d, Plan *p) {
    uint64_t uso = static_cast<uint64_t>(d.usage);
    int fmt = sdg::resolverFormato(static_cast<int>(d.format), uso);
    p->f = sdg::formato(fmt);
    if (static_cast<uint64_t>(d.usage) & (1ull << 14)) return false;   // PROTECTED: no hay memoria protegida
    if (d.reservedSize > 0) return false;   // shortcut: sin región reservada (no hay memoria compartible); añadir si un códec la pide
    if (!p->f || sdg::gpuNoPuede(fmt, uso) || d.width <= 0 || d.height <= 0 || d.layerCount != 1 || d.width > 16384 || d.height > 16384) return false;
    if (fmt == 0x21) {   // BLOB: ancho = bytes
        if (d.height != 1) return false;
        *p = {p->f, sdg::tipoPara(fmt, uso), d.width, (uint64_t)d.width};
        return true;
    }
    p->stride = sdg::alinear(d.width, 16);
    p->tam = (uint64_t)p->stride * d.height * p->f->bpp;
    p->tipo = sdg::tipoPara(fmt, uso);
    if (sdg::Yuv y; sdg::yuv(fmt, p->stride, d.height, &y)) p->tam = y.tam;   // tam = el memfd YUV
    return true;
}

// Textura D3D12 compartida (fd de dxg).
int crearTextura(const BufferDescriptorInfo &d, Plan *p) {
    ID3D12Device *dev = sdg::dispositivo();
    if (!dev) return -1;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = (UINT64)d.width; rd.Height = (UINT)d.height;
    rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.Format = p->f->dxgi; rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (p->tipo == sdg::T_YUV_GPU) rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;   // la escribe yuv.hlsl (mapper)
    // Montón DEFAULT: el CUSTOM visible por CPU con D3D12_HEAP_FLAG_SHARED lo rechaza el driver (E_INVALIDARG, medido);
    // el lock de CPU lo resuelve el mapper copiando por GPU.
    ID3D12Resource *r = nullptr;
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0};
    HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_SHARED, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&r));
    p->tipo = sdg::T_D3D12_GPU;
    if (FAILED(hr)) { LOG_E("CreateCommittedResource %dx%d fmt %d: 0x%08x", d.width, d.height, (int)p->f->dxgi, (unsigned)hr); return -1; }
    HANDLE h = nullptr;
    hr = dev->CreateSharedHandle(r, nullptr, GENERIC_ALL, nullptr, &h);
    r->Release();   // el fd de dxg mantiene vivo el recurso
    if (FAILED(hr)) { LOG_E("CreateSharedHandle: 0x%08x", (unsigned)hr); return -1; }
    D3D12_RESOURCE_ALLOCATION_INFO ai = dev->GetResourceAllocationInfo(0, 1, &rd);
    p->tam = ai.SizeInBytes;
    return (int)(intptr_t)h;
}

// Reserva anticipada de búferes grandes: una textura de pantalla (2880x1920, 22 MB) tarda 10–20 ms en D3D12 y Circle to
// Search pide 6 (1 captura y, 180 ms después, 5 seguidas; medido 17/11). Tras dar una >= 8 MB, un hilo crea otras iguales
// de una en una (en paralelo frenaban la primera: 13 -> 23 ms, medido) y las guarda 2 s. Cuántas: las que pidió la ráfaga
// anterior de esa clave (2 la primera vez, 5 como mucho; se recuerda por clave). Es memoria recién creada (D3D12 y el montón dma-buf la entregan
// a ceros), nunca un búfer que un cliente usó: no puede llevar contenido de otro proceso. Solo T_MEMFD y T_D3D12_GPU.
constexpr uint64_t RESERVA_MIN = 8u << 20;
constexpr int RESERVA_MAX = 5;
struct Reserva {
    BufferDescriptorInfo d; Plan p{}; bool valida = false; uint64_t clave = 0;   // clave: sube al cambiar de tamaño/formato
    std::vector<int> fds; int objetivo = 2, rafaga = 0, rafagaAnterior = 0; bool hilo = false;
    std::chrono::steady_clock::time_point ultima;
    uint64_t id = 0;
    std::unordered_map<uint64_t, int> aprendidas;   // ráfaga por clave: la pantalla de inicio pide otras entre dos capturas
};
std::mutex mxReserva;
std::condition_variable cvReserva;
Reserva reserva;

int crearFd(const BufferDescriptorInfo &d, Plan *p) { return p->tipo == sdg::T_MEMFD ? memoriaCpu("gralloc", p->tam) : crearTextura(d, p); }
uint64_t idClave(const BufferDescriptorInfo &d, const Plan &p) {
    return p.tipo == sdg::T_MEMFD ? (p.tam << 2 | 1) : ((uint64_t)d.width << 40 | (uint64_t)d.height << 16 | (uint64_t)p.f->dxgi << 2 | 2);
}
bool mismaClave(const BufferDescriptorInfo &d, const Plan &p) {
    const Reserva &r = reserva;
    return r.valida && r.p.tipo == p.tipo && (p.tipo == sdg::T_MEMFD ? r.p.tam == p.tam : r.d.width == d.width && r.d.height == d.height &&
           r.p.f->dxgi == p.f->dxgi);
}
// Crea de una en una hasta el objetivo; 2 s sin peticiones, las libera y recuerda el tamaño de la ráfaga.
void hiloReserva() {
    std::unique_lock<std::mutex> l(mxReserva);
    for (;;) {
        if ((int)reserva.fds.size() < reserva.objetivo) {
            BufferDescriptorInfo d = reserva.d; Plan q = reserva.p; uint64_t clave = reserva.clave;
            l.unlock();
            int fd = crearFd(d, &q);
            l.lock();
            if (fd < 0) reserva.objetivo = (int)reserva.fds.size();   // sin memoria: no insistir
            else if (reserva.clave == clave) { reserva.fds.push_back(fd); reserva.p.tam = q.tam; }
            else close(fd);
            cvReserva.notify_all();
            continue;
        }
        auto fin = reserva.ultima + std::chrono::seconds(2);
        if (std::chrono::steady_clock::now() >= fin) {
            for (int x : reserva.fds) close(x);
            reserva.fds.clear();
            reserva.rafagaAnterior = reserva.aprendidas[reserva.id] = reserva.rafaga; reserva.rafaga = 0; reserva.hilo = false;
            return;
        }
        cvReserva.wait_until(l, fin);
    }
}
// Da una reserva si casa (esperando hasta 30 ms a la que se está creando) y pide reponer.
int tomarReserva(const BufferDescriptorInfo &d, Plan *p) {
    if ((p->tipo != sdg::T_MEMFD && p->tipo != sdg::T_D3D12_GPU) || p->tam < RESERVA_MIN) return -1;
    std::unique_lock<std::mutex> l(mxReserva);
    if (!mismaClave(d, *p)) {
        for (int x : reserva.fds) close(x);
        reserva.fds.clear();
        if (reserva.valida && reserva.rafaga) reserva.aprendidas[reserva.id] = reserva.rafaga;
        if (reserva.aprendidas.size() > 16) reserva.aprendidas.clear();
        reserva.d = d; reserva.p = *p; reserva.valida = true; reserva.clave++; reserva.id = idClave(d, *p);
        reserva.rafaga = 0;
        auto it = reserva.aprendidas.find(reserva.id);
        reserva.rafagaAnterior = it == reserva.aprendidas.end() ? 0 : it->second;
    } else if (reserva.fds.empty() && reserva.hilo && (int)reserva.fds.size() < reserva.objetivo) {
        cvReserva.wait_for(l, std::chrono::milliseconds(30), [] { return !reserva.fds.empty() || !reserva.hilo; });
    }
    int fd = -1;
    if (!reserva.fds.empty()) { fd = reserva.fds.back(); reserva.fds.pop_back(); p->tam = reserva.p.tam; }
    reserva.rafaga++;
    reserva.ultima = std::chrono::steady_clock::now();
    reserva.objetivo = std::min(RESERVA_MAX, std::max({2, reserva.rafagaAnterior - 1, reserva.rafaga}));
    cvReserva.notify_all();
    return fd;
}
// Tras dar el búfer (no antes: dos creaciones a la vez se frenan) arranca el hilo que repone.
void reponer() {
    std::lock_guard<std::mutex> l(mxReserva);
    if (reserva.valida && !reserva.hilo) { reserva.hilo = true; std::thread(hiloReserva).detach(); }
}

class Allocator : public BnAllocator {
    std::atomic<uint64_t> siguienteId{1};

    ScopedAStatus uno(const BufferDescriptorInfo &d, Plan p, NativeHandle *out) {
        int fd, fdYuv = -1;
        auto t0 = std::chrono::steady_clock::now();
        if ((fd = tomarReserva(d, &p)) >= 0) {}
        else if (p.tipo == sdg::T_MEMFD) fd = memoriaCpu("gralloc", p.tam);
        else if (p.tipo != sdg::T_YUV_GPU) fd = crearTextura(d, &p);
        else if (Plan t = p; (fd = crearTextura(d, &t)) >= 0 && (fdYuv = memoriaCpu("gralloc-yuv", p.tam)) < 0) { close(fd); fd = -1; }
        if (fd < 0) return error(AllocationError::NO_RESOURCES);
        if ((p.tipo == sdg::T_MEMFD || p.tipo == sdg::T_D3D12_GPU) && p.tam >= RESERVA_MIN) reponer();
        if (auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); ms > 5 || p.tam >= RESERVA_MIN)
            LOG_I("reserva%s: %dx%d fmt %d tipo %d %llu B en %.1f ms", ms > 5 ? " lenta" : "", d.width, d.height, (int)d.format, p.tipo,
                  (unsigned long long)p.tam, ms);
        uint64_t uso = static_cast<uint64_t>(d.usage), id = siguienteId++ | ((uint64_t)getpid() << 40);
        out->fds.emplace_back(fd);
        if (fdYuv >= 0) out->fds.emplace_back(fdYuv);   // el último fd es siempre el que mapea la CPU
        out->ints = {sdg::MAGIA, d.width, d.height, static_cast<int>(d.format), d.layerCount, (int)(uint32_t)uso,
                     (int)(uso >> 32), p.stride, (int)(uint32_t)p.tam, (int)(p.tam >> 32), (int)(uint32_t)id,
                     (int)(id >> 32), p.tipo};
        out->ints.resize(sdg::N_INTS, 0);
        memcpy(out->ints.data() + sdg::I_NOMBRE, d.name.data(), std::min<size_t>(31, d.name.size()));   // 32 bytes, termina en 0
        return ScopedAStatus::ok();
    }

  public:
    ScopedAStatus allocate(const std::vector<uint8_t> &, int32_t, AllocationResult *) override {
        return error(AllocationError::UNSUPPORTED);   // solo mapper v5 (allocate2)
    }
    ScopedAStatus allocate2(const BufferDescriptorInfo &d, int32_t n, AllocationResult *r) override {
        Plan p;
        if (n <= 0 || n > 64 || !planear(d, &p)) return error(AllocationError::UNSUPPORTED);   // 64 = máx. de una BufferQueue
        r->stride = p.stride; r->buffers.clear();
        for (int i = 0; i < n; i++) {
            NativeHandle h;
            if (auto s = uno(d, p, &h); !s.isOk()) return s;
            r->buffers.push_back(std::move(h));
        }
        return ScopedAStatus::ok();
    }
    ScopedAStatus isSupported(const BufferDescriptorInfo &d, bool *r) override { Plan p; *r = planear(d, &p); return ScopedAStatus::ok(); }
    ScopedAStatus getIMapperLibrarySuffix(std::string *r) override { *r = "ranchu"; return ScopedAStatus::ok(); }
    ScopedAStatus isMultiViewSupported(const std::vector<BufferDescriptorInfo> &, int32_t, bool *r) override { *r = false; return ScopedAStatus::ok(); }
    ScopedAStatus allocateMultiView(const std::vector<BufferDescriptorInfo> &, int32_t, AllocationResult *) override {
        return error(AllocationError::UNSUPPORTED);
    }
};

}  // namespace

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(4);
    auto a = ndk::SharedRefBase::make<Allocator>();
    const std::string nombre = std::string(Allocator::descriptor) + "/default";
    if (AServiceManager_addService(a->asBinder().get(), nombre.c_str()) != STATUS_OK) { LOG_E("no se registró %s", nombre.c_str()); return 1; }
    LOG_I("%s listo", nombre.c_str());
    ABinderProcess_joinThreadPool();
    return 1;
}
