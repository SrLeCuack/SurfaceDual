// Composer de Hyper-V: SurfaceFlinger compone; el Visor conserva las zonas sin daño.
#include "../gralloc/comun.h"
#include "bc1_dxil.h"
#include "diff_dxil.h"
#include "diffcopia_dxil.h"
#include <aidl/android/hardware/graphics/composer3/BnComposer.h>
#include <aidl/android/hardware/graphics/composer3/BnComposerClient.h>
#include <aidl/android/hardware/graphics/composer3/IComposerCallback.h>
#include <android/hardware/graphics/composer3/ComposerServiceWriter.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <sys/system_properties.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <linux/vm_sockets.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

std::atomic<bool> pantallaEncendida{true};   // la lee entrada.cpp: con la pantalla apagada, un toque solo la enciende

using namespace aidl::android::hardware::graphics::composer3;
namespace common = aidl::android::hardware::graphics::common;
using Status = ndk::ScopedAStatus;
static Status ok() { return Status::ok(); }
static Status error(int e = IComposerClient::EX_UNSUPPORTED) { return Status::fromServiceSpecificError(e); }
#define HLOG(...) __android_log_print(ANDROID_LOG_ERROR, "hwc.hyperv", __VA_ARGS__)

struct Rect { int32_t x=0,y=0,w=0,h=0; };
static Rect unir(Rect a, Rect b) {
    if (!a.w || !a.h) return b;
    if (!b.w || !b.h) return a;
    int r=std::max(a.x+a.w,b.x+b.w), d=std::max(a.y+a.h,b.y+b.h);
    a.x=std::min(a.x,b.x); a.y=std::min(a.y,b.y); a.w=r-a.x; a.h=d-a.y; return a;
}
struct Cuadro { uint32_t magia=0x46504453, version=1, id, ancho, alto, formato=1, nRect=1, escala=1, reservado[2]={}; };
struct Listo { uint32_t magia, id; };
// Protocolo v4 (capa de vídeo DEVICE, F7/plan/video_capa_diseno.md): tras la cabecera va este registro; después los
// rectángulos de interfaz (nRect puede ser 0) y, si nuevo, el BC1 del búfer de vídeo entero (ancho x alto).
// El composer marca cabecera.reservado[0]=1 ("sé v4"); el Visor contesta "SDO4" en vez de "SDOK" si quiere v4.
struct RegistroVideo { uint32_t hayVideo=0, nuevo=0, formato=71, ancho=0, alto=0; Rect destino; float recorte[4]={}; };
static_assert(sizeof(Cuadro)==40 && sizeof(Rect)==16 && sizeof(Listo)==8 && sizeof(RegistroVideo)==52);
static_assert(offsetof(Cuadro,reservado)==32 && offsetof(RegistroVideo,destino)==20 && offsetof(RegistroVideo,recorte)==36);   // VisorVM.cs lee estos desplazamientos
constexpr uint32_t SDOK=0x4B4F4453, SDO4=0x344F4453;
constexpr uint64_t USO_SD_VIDEO=1ull<<28, USO_PROTEGIDO=1ull<<14;   // vendor: lo pone c2.surfacedual (codec2.cpp)
static_assert(std::endian::native==std::endian::little);
static_assert(IComposer::version==5 && IComposerClient::version==5);

static bool toca(Rect a, Rect b) {
    return a.w>0 && a.h>0 && b.w>0 && b.h>0 &&
        a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
}
// Más de 1/8 de pantalla va en BC1 (8 veces menos bytes). Medido 17/11: el panel de Ajustes al desplazar (30 %) en RGBA
// tardaba 6 ms en enviarse y no llegaba a 120 Hz. Al pararse se reenvía sin pérdida (afinar).
static int escalaDano(Rect r, int w, int h) {
    return (int64_t)r.w*r.h*8>=(int64_t)w*h ? 2 : 1;
}
static Rect alinearBC1(Rect r, int w, int h) {
    int derecha=std::min(w,(r.x+r.w+3)&~3), abajo=std::min(h,(r.y+r.h+3)&~3);
    r.x&=~3; r.y&=~3; r.w=derecha-r.x; r.h=abajo-r.y; return r;
}
static size_t bytesBC1(Rect r) { return (size_t)((r.w+3)/4)*((r.h+3)/4)*8; }
static void copiarBloque(uint8_t* dst, const uint8_t* origen, size_t pitch, Rect r, int escala) {
    int pw=(r.w+escala-1)/escala, ph=(r.h+escala-1)/escala;
    for (int y=0;y<ph;y++) {
        auto src=origen+(size_t)(r.y+y*escala)*pitch+(size_t)r.x*4;
        auto fila=dst+(size_t)y*pw*4;
        if (escala==1) memcpy(fila,src,(size_t)pw*4);
        else {
            auto entrada=reinterpret_cast<const uint32_t*>(src);
            auto salida=reinterpret_cast<uint32_t*>(fila);
            for (int x=0;x<pw;x++) salida[x]=entrada[x*2];
        }
    }
}
// Caja que devuelve diff.hlsl {minX,minY,maxX,maxY}: sin cambios -> vacía. Si es anómala, cuenta todo r como dañado: la
// copia ya sobrescribió r en imagenGpu y, si no se envía, el diff no lo volvería a ver nunca (basura fija en el Visor).
static Rect cajaDano(const uint32_t k[4], Rect r) {
    if (k[0]==~0u && k[1]==~0u && !k[2] && !k[3]) return {};
    if (k[0]<k[2] && k[1]<k[3] && k[0]>=(uint32_t)r.x && k[1]>=(uint32_t)r.y &&
        k[2]<=(uint32_t)(r.x+r.w) && k[3]<=(uint32_t)(r.y+r.h))
        return {(int32_t)k[0],(int32_t)k[1],(int32_t)(k[2]-k[0]),(int32_t)(k[3]-k[1])};
    return r;
}
static void comprobarBloque() {
    {   const uint32_t nada[4]={~0u,~0u,0,0}, bien[4]={16,0,32,16}, fuera[4]={16,0,4000,16}, rara[4]={5,5,5,9};
        Rect r={0,0,2880,1920}, a=cajaDano(nada,r), b=cajaDano(bien,r), c=cajaDano(fuera,r), e=cajaDano(rara,r);
        assert(!a.w && b.x==16 && b.w==16 && b.h==16 && c.w==2880 && e.h==1920); }
    auto a=alinearBC1({3,5,5,6},2880,1920);
    assert(a.x==0 && a.y==4 && a.w==8 && a.h==8 && bytesBC1(a)==32);
    a=alinearBC1({2879,1919,1,1},2880,1920);
    assert(a.x==2876 && a.y==1916 && a.w==4 && a.h==4 && bytesBC1(a)==8);
    assert(bytesBC1({0,0,2880,1920})==2764800);
    assert(toca({0,0,3,3},{2,2,2,2}) && !toca({0,0,3,3},{3,0,2,2}));
    uint32_t src[20], dst[9]={};
    for (uint32_t i=0;i<20;i++) src[i]=i;
    Rect r={1,1,3,3};
    copiarBloque((uint8_t*)dst,(uint8_t*)src,5*4,r,2);
    assert(dst[0]==6 && dst[1]==8 && dst[2]==16 && dst[3]==18);
    copiarBloque((uint8_t*)dst,(uint8_t*)src,5*4,r,1);
    assert(dst[0]==6 && dst[2]==8 && dst[3]==11 && dst[8]==18);
    uint32_t conPitch[3*64];
    for (uint32_t i=0;i<3*64;i++) conPitch[i]=i;
    copiarBloque((uint8_t*)dst,(uint8_t*)conPitch,256,{0,0,3,3},2);
    assert(dst[0]==0 && dst[1]==2 && dst[2]==128 && dst[3]==130);
    assert(escalaDano({0,0,1,10},10,10)==1 && escalaDano({0,0,2,10},10,10)==2);
}

// Lo que el composer sabe de una capa para decidir si el vídeo va DEVICE (lo pinta el Visor) o CLIENT (SurfaceFlinger).
struct VistaCapa { int64_t id=0; bool device=false, video=false, lista=false; Rect marco; float recorte[4]={}; int transform=0, z=0; float alfa=1; int ancho=0, alto=0; };
// Una sola capa de vídeo, sin girar, opaca, dentro de pantalla, con algo más en CLIENT y nada debajo que la toque (lo de
// debajo iría al client target, que el Visor pinta ENCIMA del vídeo). Si no, 0 = todo CLIENT, como antes.
static int64_t elegirVideo(const std::vector<VistaCapa>& v, int w, int h) {
    const VistaCapa* e=nullptr;
    for (auto& c:v) if (c.video && c.device) { if (e) return 0; e=&c; }
    if (!e || !e->lista || v.size()<2 || e->transform || e->alfa<1.0f || e->ancho<=0 || e->alto<=0 || e->ancho%4 || e->alto%4 ||
        (int64_t)e->ancho*e->alto>(int64_t)w*h) return 0;
    const Rect& m=e->marco; const float* k=e->recorte;
    if (m.w<=0 || m.h<=0 || m.x<0 || m.y<0 || m.x+m.w>w || m.y+m.h>h) return 0;
    if (!(k[0]>=0 && k[1]>=0 && k[2]>k[0] && k[3]>k[1] && k[2]<=e->ancho && k[3]<=e->alto)) return 0;
    for (auto& c:v) if (&c!=e && c.z<=e->z && toca(c.marco,m)) return 0;
    return e->id;
}
static void comprobarVideo() {
    VistaCapa vid; vid.id=7; vid.device=vid.video=vid.lista=true; vid.marco={0,48,2016,1134}; vid.z=11;
    vid.ancho=1920; vid.alto=1080; vid.recorte[2]=1920; vid.recorte[3]=1080;
    VistaCapa app; app.id=8; app.device=true; app.marco={0,0,2880,1920}; app.z=12;
    std::vector<VistaCapa> v={vid,app};
    assert(elegirVideo(v,2880,1920)==7);
    assert(elegirVideo({vid},2880,1920)==0);                       // sin interfaz encima: nada que ganar
    auto u=v; u[1].z=10; assert(elegirVideo(u,2880,1920)==0);       // algo debajo que lo toca
    u=v; u[1].z=10; u[1].marco={0,1500,100,100}; assert(elegirVideo(u,2880,1920)==7);   // debajo pero sin tocarlo
    u=v; u.push_back(vid); u[2].id=9; assert(elegirVideo(u,2880,1920)==0);   // dos vídeos
    u=v; u[0].transform=4; assert(elegirVideo(u,2880,1920)==0);
    u=v; u[0].alfa=0.5f; assert(elegirVideo(u,2880,1920)==0);
    u=v; u[0].device=false; assert(elegirVideo(u,2880,1920)==0);    // SurfaceFlinger la pidió CLIENT
    u=v; u[0].lista=false; assert(elegirVideo(u,2880,1920)==0);
    u=v; u[0].marco.w=3000; assert(elegirVideo(u,2880,1920)==0);
    u=v; u[0].recorte[2]=1921; assert(elegirVideo(u,2880,1920)==0);
    u=v; u[0].alto=1082; assert(elegirVideo(u,2880,1920)==0);       // BC1 exige múltiplos de 4
}

template<class T> struct Com {
    T* p=nullptr;
    ~Com() { if (p) p->Release(); }
    Com()=default; Com(const Com&)=delete; Com& operator=(const Com&)=delete;
    T* operator->() const { return p; }
};
struct Slot { Com<ID3D12Resource> textura; uint64_t id=0, uso=0; int ancho=0, alto=0; };
// Estado por capa que manda SurfaceFlinger; solo los búferes de vídeo (USO_SD_VIDEO) se abren como textura.
struct Capa {
    Composition tipo=Composition::INVALID;   // la que pide SurfaceFlinger (o la aceptada tras validate)
    Rect marco; float recorte[4]={}; int transform=0, z=0; float alfa=1;
    std::map<int32_t,std::unique_ptr<Slot>> slots; int32_t slot=-1;
    ndk::ScopedFileDescriptor acquire;
    Slot* actual() { auto it=slots.find(slot); return it==slots.end() ? nullptr : it->second.get(); }
};

// UAPI de RutaA/fase4/kernel_sdsync/sdsync.h: una sync_file que se señala al escribir en un eventfd.
struct sdsync_desde_eventfd { int32_t eventfd; int32_t fence; };
#define SDSYNC_IOC_DESDE_EVENTFD _IOWR('S', 1, struct sdsync_desde_eventfd)
static void senal(int ev) { uint64_t uno=1; ssize_t n; do n=write(ev,&uno,sizeof uno); while (n<0 && errno==EINTR); close(ev); }

static inline int64_t ahoraNs() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static inline uint32_t deltaUs(int64_t t0, int64_t t1) {
    return t1 > t0 ? (uint32_t)((t1 - t0) / 1000) : 0;
}

struct Estadisticas {
    std::mutex smx;
    uint32_t tAcquire[120]={}, tGpu[120]={}, tMemcpy[120]={};
    uint32_t tEnvio[120]={}, tListo[120]={};
    int nCopia=0, nEnvio=0, nListo=0;

    void registrarCopia(uint32_t acq, uint32_t gpu, uint32_t cpy, const char* origen="target") {
        std::lock_guard l(smx);
        if (nCopia < 120) {
            tAcquire[nCopia] = acq;
            tGpu[nCopia] = gpu;
            tMemcpy[nCopia] = cpy;
            nCopia++;
        }
        if (nCopia >= 120) {
            double medAcq, p95Acq, medGpu, p95Gpu, medCpy, p95Cpy, medEnv, p95Env, medLst, p95Lst;
            calc(tAcquire, nCopia, medAcq, p95Acq);
            calc(tGpu, nCopia, medGpu, p95Gpu);
            calc(tMemcpy, nCopia, medCpy, p95Cpy);
            calc(tEnvio, nEnvio, medEnv, p95Env);
            calc(tListo, nListo, medLst, p95Lst);
            __android_log_print(ANDROID_LOG_INFO, "hwc.hyperv",
                "%s tiempos 120c (ms) [media/p95]: acquire=%.2f/%.2f gpu=%.2f/%.2f memcpy=%.2f/%.2f envio=%.2f/%.2f listo=%.2f/%.2f enviados=%d",
                origen, medAcq, p95Acq, medGpu, p95Gpu, medCpy, p95Cpy, medEnv, p95Env, medLst, p95Lst, nEnvio);   // enviados < 120: el Visor recibió cuadros juntados
            nCopia = nEnvio = nListo = 0;
        }
    }
    void registrarEnvio(uint32_t env) {
        std::lock_guard l(smx);
        if (nEnvio < 120) tEnvio[nEnvio++] = env;
    }
    void registrarListo(uint32_t lst) {
        std::lock_guard l(smx);
        if (nListo < 120) tListo[nListo++] = lst;
    }
private:
    static void calc(uint32_t* datos, int n, double& mediaMs, double& p95Ms) {
        if (n <= 0) { mediaMs = p95Ms = 0.0; return; }
        uint64_t suma = 0;
        for (int i = 0; i < n; i++) suma += datos[i];
        mediaMs = (double)suma / n / 1000.0;
        std::sort(datos, datos + n);
        int idx = (n * 95) / 100;
        if (idx >= n) idx = n - 1;
        p95Ms = (double)datos[idx] / 1000.0;
    }
};

struct Trabajo {
    uint64_t id = 0;
    ID3D12Resource* tex = nullptr;
    ndk::ScopedFileDescriptor acquire;
    Rect dano;
    int evento = -1;   // eventfd de la fence de present (sdsync): se escribe al terminar la copia
    bool sincronico = false;
    // Capa de vídeo DEVICE de este cuadro: info.hayVideo=0 si no hay; video=nullptr si el búfer ya se mandó.
    RegistroVideo info;
    ID3D12Resource* video = nullptr;
    uint64_t idVideo = 0;
    ndk::ScopedFileDescriptor acqVideo;

    Trabajo() = default;
    Trabajo(uint64_t id_, ID3D12Resource* t, ndk::ScopedFileDescriptor acq, Rect d, int ev, bool sinc)
        : id(id_), tex(t), acquire(std::move(acq)), dano(d), evento(ev), sincronico(sinc) {
        if (tex) tex->AddRef();
    }
    ~Trabajo() { if (tex) tex->Release(); if (video) video->Release(); }
    Trabajo(Trabajo&& o) noexcept { *this = std::move(o); }
    Trabajo& operator=(Trabajo&& o) noexcept {
        if (this != &o) {
            if (tex) tex->Release();
            if (video) video->Release();
            id = o.id; tex = o.tex; acquire = std::move(o.acquire);
            dano = o.dano; evento = o.evento; sincronico = o.sincronico;
            info = o.info; video = o.video; idVideo = o.idVideo; acqVideo = std::move(o.acqVideo);
            o.tex = nullptr; o.video = nullptr;
        }
        return *this;
    }
    Trabajo(const Trabajo&) = delete;
    Trabajo& operator=(const Trabajo&) = delete;
};

class Cliente final : public BnComposerClient {
    int w=2880,h=1920,period=8333333;
    std::mutex mx, vmx, imagenMx;
    std::condition_variable vcv;
    std::atomic<bool> parar{false};
    bool vsync=false, encendida=true, presentado=false;
    std::shared_ptr<IComposerCallback> callback;
    std::thread hiloVsync, hiloTrabajador, hiloSocket;
    ndk::ScopedFileDescriptor aviso;
    std::map<int32_t,std::unique_ptr<Slot>> slots;
    int32_t numeroSlots=0, actual=-1;
    ndk::ScopedFileDescriptor acquire;
    Rect danoTarget, pendiente, nitidez;
    int64_t ultimoDano=0;
    uint32_t copiasPendientes=0;
    Com<ID3D12Resource> imagenGpu;
    bool hayImagen=false;
    std::map<int64_t,Capa> capas;
    int64_t siguienteCapa=1;
    // Capa de vídeo DEVICE (bajo mx salvo los atómicos). videoElegida: la que dio validate; 0 = todo CLIENT.
    int64_t videoElegida=0;
    bool matrizColor=false;   // SurfaceFlinger pidió una matriz de color (p. ej. modo lectura o daltonismo)
    std::atomic<bool> visorV4{false}, capaVideoProp{true}, videoRoto{false};
    uint64_t ultimoVideoId=0;   // búfer que ya tiene el Visor (0 = mandar el siguiente aunque no cambie)
    // Tres READBACK de BC1 de vídeo: el trabajador escribe en uno que no esté ni listo ni enviándose.
    Com<ID3D12Resource> rbVideo[3];
    int videoListo=-1, videoEnvio=-1;
    bool videoNuevo=false, videoCambio=false;   // hay BC1 sin mandar; cambió hayVideo/destino sin cuadro de interfaz
    RegistroVideo videoInfo;   // lo último publicado por el trabajador
    Com<ID3D12CommandQueue> cola;
    Com<ID3D12CommandAllocator> asignador;
    Com<ID3D12GraphicsCommandList> lista;
    Com<ID3D12Fence> fence;
    uint64_t valor=0;
    Com<ID3D12Resource> rb;   // READBACK reutilizado (copiar)
    Com<ID3D12RootSignature> raizBC1;
    Com<ID3D12PipelineState> psoBC1;
    Com<ID3D12DescriptorHeap> srvBC1;
    Com<ID3D12Resource> bloquesBC1, rbBC1;
    // Daño real por comparación en la GPU (diff.hlsl): SurfaceFlinger no informa del daño del client target.
    Com<ID3D12RootSignature> raizDiff;
    Com<ID3D12PipelineState> psoDiff;
    Com<ID3D12DescriptorHeap> srvDiff;
    Com<ID3D12Resource> cajaGpu, cajaSubida, cajaLectura;
    bool diffRoto=false;   // el driver no lo admite: se copia sin comparar (todo cuenta como daño, como antes)
    // Una pasada (diffcopia.hlsl): el diff escribe lo distinto en imagenGpu, sin la copia de pantalla entera.
    // setprop debug.sd.hwc.una_pasada 0 vuelve a diff + CopyTextureRegion (A/B en caliente).
    Com<ID3D12RootSignature> raizCopia;
    Com<ID3D12PipelineState> psoCopia;
    bool unaPasada=false;
    std::atomic<bool> unaPasadaProp{true};

    std::mutex colaMx, sincMx;
    std::condition_variable colaCv, sincCv;
    std::queue<Trabajo> colaTrabajos;
    uint64_t siguienteTrabajoId=0, ultimoTrabajoCompletado=0;
    int sdsyncFd=-1;
    int64_t tEnvioFin=0;
    Estadisticas stats;
    Estadisticas statsLectura, statsVideo;

    Rect completa() const { return {0,0,w,h}; }
    void despertar() { uint64_t uno=1; if (aviso.get()>=0) (void)write(aviso.get(),&uno,sizeof uno); }
    // Fence de present: sync_file de sdsync que se señala al escribir en ev. -1 (y ev=-1) si no hay sdsync.
    int nuevaFence(int& ev) {
        ev=-1; if (sdsyncFd<0) return -1;
        ev=eventfd(0,EFD_CLOEXEC);
        sdsync_desde_eventfd s={ev,-1};
        if (ev>=0 && ioctl(sdsyncFd,SDSYNC_IOC_DESDE_EVENTFD,&s)==0 && s.fence>=0) return s.fence;
        static bool avisado=false; if (!avisado) { avisado=true; HLOG("sdsync: %s (present síncrono)",strerror(errno)); }
        if (ev>=0) close(ev); ev=-1; return -1;
    }
    bool gpu() {
        if (cola.p) return asignador.p && lista.p && fence.p;
        auto d=sdg::dispositivo(); if (!d) return false;
        D3D12_COMMAND_QUEUE_DESC q={}; q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        return SUCCEEDED(d->CreateCommandQueue(&q,IID_PPV_ARGS(&cola.p))) &&
            SUCCEEDED(d->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&asignador.p))) &&
            SUCCEEDED(d->CreateCommandList(0,q.Type,asignador.p,nullptr,IID_PPV_ARGS(&lista.p))) &&
            SUCCEEDED(lista->Close()) && SUCCEEDED(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence.p)));
    }
    // Señala y espera a la GPU antes de leer un READBACK o reiniciar el asignador. Con evento nulo SetEventOnCompletion
    // bloquea hasta el valor (documentado); se comprueba igual por si este camino (dxg sin runtime de Windows) no lo cumple.
    bool esperarGpu() {
        if (FAILED(cola->Signal(fence.p,++valor)) || FAILED(fence->SetEventOnCompletion(valor,nullptr))) return false;
        uint64_t v=fence->GetCompletedValue();
        if (v<valor) {
            static bool avisado=false; if (!avisado) { avisado=true; HLOG("SetEventOnCompletion(nullptr) no esperó: se sondea"); }
            for (int i=0; v<valor; i++) {
                if (i==10000) { HLOG("GPU sin terminar en 2 s"); return false; }   // el que llama reenvía todo
                usleep(200); v=fence->GetCompletedValue();
            }
        }
        return v!=UINT64_MAX;   // UINT64_MAX: dispositivo perdido
    }
    static D3D12_RESOURCE_BARRIER transicion(ID3D12Resource* x, D3D12_RESOURCE_STATES antes, D3D12_RESOURCE_STATES despues) {
        D3D12_RESOURCE_BARRIER b={}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition={x,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,antes,despues}; return b;
    }
    bool prepararDiff(ID3D12Device* d, ID3D12Resource* tex) {
        if (diffRoto) return false;
        if (!psoDiff.p) {
            diffRoto=true;   // hasta que todo se cree bien
            auto bufer=[&](D3D12_HEAP_TYPE tipo, D3D12_RESOURCE_STATES estado, Com<ID3D12Resource>& x) {
                D3D12_HEAP_PROPERTIES heap={}; heap.Type=tipo;
                D3D12_RESOURCE_DESC b={}; b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; b.Width=16; b.Height=1;
                b.DepthOrArraySize=1; b.MipLevels=1; b.SampleDesc.Count=1; b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                if (tipo==D3D12_HEAP_TYPE_DEFAULT) b.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                return SUCCEEDED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&b,estado,nullptr,IID_PPV_ARGS(&x.p)));
            };
            D3D12_DESCRIPTOR_HEAP_DESC hd={}; hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            hd.NumDescriptors=2; hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            bool listo=SUCCEEDED(d->CreateRootSignature(0,diff_root_signature,sizeof diff_root_signature,IID_PPV_ARGS(&raizDiff.p)));
            if (listo) {
                D3D12_COMPUTE_PIPELINE_STATE_DESC p={}; p.pRootSignature=raizDiff.p; p.CS={diff_dxil,sizeof diff_dxil};
                listo=SUCCEEDED(d->CreateComputePipelineState(&p,IID_PPV_ARGS(&psoDiff.p)));
            }
            uint32_t* inicial=nullptr;
            listo=listo && SUCCEEDED(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srvDiff.p))) &&
                bufer(D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,cajaSubida) &&
                bufer(D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,cajaLectura) &&
                bufer(D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST,cajaGpu) &&
                SUCCEEDED(cajaSubida->Map(0,nullptr,(void**)&inicial));
            if (!listo) { HLOG("diff no disponible: cada cuadro cuenta como pantalla entera"); return false; }
            inicial[0]=inicial[1]=~0u; inicial[2]=inicial[3]=0;   // queda mapeado (UPLOAD)
            diffRoto=!probarDiff(d,false);
            if (diffRoto) { HLOG("diff: la autoprueba en la GPU falló; cada cuadro cuenta como pantalla entera"); return false; }
            if (cargaTipada(d)) {
                bool listo=SUCCEEDED(d->CreateRootSignature(0,diffcopia_root_signature,sizeof diffcopia_root_signature,IID_PPV_ARGS(&raizCopia.p)));
                if (listo) {
                    D3D12_COMPUTE_PIPELINE_STATE_DESC p={}; p.pRootSignature=raizCopia.p; p.CS={diffcopia_dxil,sizeof diffcopia_dxil};
                    listo=SUCCEEDED(d->CreateComputePipelineState(&p,IID_PPV_ARGS(&psoCopia.p)));
                }
                unaPasada=listo && probarDiff(d,true);
            }
            __android_log_print(ANDROID_LOG_INFO,"hwc.hyperv","diff en una pasada: %s",unaPasada ? "sí" : "no (diff + copia)");
        }
        return true;
    }
    // diffcopia.hlsl lee la imagen del Visor como UAV: carga y escritura tipadas de R8G8B8A8_UNORM (opcionales en D3D12).
    static bool cargaTipada(ID3D12Device* d) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT f={DXGI_FORMAT_R8G8B8A8_UNORM};
        constexpr auto ambas=D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD|D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
        return SUCCEEDED(d->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&f,sizeof f)) && (f.Support2&ambas)==ambas;
    }
    // Compara nuevo con viejo en r (si copiar, copia después r de nuevo a viejo en la misma lista) y da la caja distinta.
    // unaPasadaGpu: diffcopia.hlsl escribe lo distinto en viejo (UAV) en el mismo dispatch; copiar no se usa.
    bool diff(ID3D12Resource* nuevo, ID3D12Resource* viejo, Rect r, bool copiar, Rect& dano, bool unaPasadaGpu=false) {
        auto d=sdg::dispositivo();
        D3D12_SHADER_RESOURCE_VIEW_DESC s={}; s.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D; s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels=1;
        auto h=srvDiff->GetCPUDescriptorHandleForHeapStart();
        d->CreateShaderResourceView(nuevo,&s,h);   // el cuadro anterior ya terminó (espera síncrona): se puede reescribir
        h.ptr+=d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        if (unaPasadaGpu) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC u={}; u.Format=DXGI_FORMAT_R8G8B8A8_UNORM; u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
            d->CreateUnorderedAccessView(viejo,nullptr,&u,h);
            copiar=false;
        } else d->CreateShaderResourceView(viejo,&s,h);
        if (FAILED(asignador->Reset()) || FAILED(lista->Reset(asignador.p,unaPasadaGpu ? psoCopia.p : psoDiff.p))) return false;
        constexpr auto C=D3D12_RESOURCE_STATE_COMMON, L=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            O=D3D12_RESOURCE_STATE_COPY_SOURCE, DST=D3D12_RESOURCE_STATE_COPY_DEST, U=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        auto V=unaPasadaGpu ? U : L;   // estado de viejo durante el dispatch
        lista->CopyBufferRegion(cajaGpu.p,0,cajaSubida.p,0,16);
        D3D12_RESOURCE_BARRIER a[]={transicion(nuevo,C,L),transicion(viejo,C,V),transicion(cajaGpu.p,DST,U)};
        lista->ResourceBarrier(3,a);
        ID3D12DescriptorHeap* heaps[]={srvDiff.p}; lista->SetDescriptorHeaps(1,heaps);
        lista->SetComputeRootSignature(unaPasadaGpu ? raizCopia.p : raizDiff.p);
        lista->SetComputeRootDescriptorTable(0,srvDiff->GetGPUDescriptorHandleForHeapStart());
        lista->SetComputeRootUnorderedAccessView(1,cajaGpu->GetGPUVirtualAddress());
        uint32_t constantes[]={(uint32_t)r.x,(uint32_t)r.y,(uint32_t)r.w,(uint32_t)r.h};
        lista->SetComputeRoot32BitConstants(2,4,constantes,0);
        lista->Dispatch((r.w+15)/16,(r.h+15)/16,1);
        if (unaPasadaGpu) {   // la transición de abajo ya lo cubre; barrera UAV explícita por si un driver no (revisión Nemotron)
            D3D12_RESOURCE_BARRIER u={}; u.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV; u.UAV.pResource=viejo; lista->ResourceBarrier(1,&u);
        }
        D3D12_RESOURCE_BARRIER b[]={transicion(nuevo,L,copiar ? O : C),transicion(viejo,V,copiar ? DST : C),transicion(cajaGpu.p,U,O)};
        lista->ResourceBarrier(3,b);
        if (copiar) {
            D3D12_TEXTURE_COPY_LOCATION src={nuevo,D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX}, dst={viejo,D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
            D3D12_BOX box={(UINT)r.x,(UINT)r.y,0,(UINT)(r.x+r.w),(UINT)(r.y+r.h),1};
            lista->CopyTextureRegion(&dst,r.x,r.y,0,&src,&box);
            D3D12_RESOURCE_BARRIER c[]={transicion(nuevo,O,C),transicion(viejo,DST,C)};
            lista->ResourceBarrier(2,c);
        }
        lista->CopyBufferRegion(cajaLectura.p,0,cajaGpu.p,0,16);
        D3D12_RESOURCE_BARRIER e=transicion(cajaGpu.p,O,DST); lista->ResourceBarrier(1,&e);
        if (FAILED(lista->Close())) return false;
        ID3D12CommandList* comandos[]={lista.p}; cola->ExecuteCommandLists(1,comandos);
        if (!esperarGpu()) return false;
        uint32_t* k=nullptr; D3D12_RANGE rango={0,16}, nada={0,0};
        if (FAILED(cajaLectura->Map(0,&rango,(void**)&k))) return false;
        dano=cajaDano(k,r);
        cajaLectura->Unmap(0,&nada);
        return true;
    }
    // Autoprueba en la GPU real (una vez): un píxel distinto fuera de la rejilla de 16 y en los bordes debe dar su tile.
    // unaPasadaGpu: además, tras escribir lo distinto, un segundo diff debe salir vacío (viejo quedó igual que nuevo).
    bool probarDiff(ID3D12Device* d, bool unaPasadaGpu) {
        constexpr int AN=64, AL=40, PASO=AN*4, TAM=PASO*AL;   // fila de 256 B y 10240 B por imagen: alineados para copiar
        D3D12_HEAP_PROPERTIES heap={}; heap.Type=D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC b={}; b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; b.Width=2*TAM; b.Height=1;
        b.DepthOrArraySize=1; b.MipLevels=1; b.SampleDesc.Count=1; b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_RESOURCE_DESC t={}; t.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; t.Width=AN; t.Height=AL;
        t.DepthOrArraySize=1; t.MipLevels=1; t.Format=DXGI_FORMAT_R8G8B8A8_UNORM; t.SampleDesc.Count=1;
        if (unaPasadaGpu) t.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        Com<ID3D12Resource> subida, viejo, nuevo; uint8_t* m=nullptr;
        if (FAILED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&b,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&subida.p))) ||
            FAILED(subida->Map(0,nullptr,(void**)&m))) return false;
        heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&t,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&viejo.p))) ||
            FAILED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&t,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&nuevo.p)))) return false;
        // x=-2: degradado con los 256 valores en cada canal (el píxel i vale i%256): una pasada debe dejarlos exactos.
        struct Caso { int x,y; Rect esperado; } casos[]={{17,5,{16,0,16,16}},{63,39,{48,32,16,8}},{0,0,{0,0,16,16}},{-1,0,{}},{-2,0,{0,0,AN,AL}}};
        for (auto& c:casos) {
            memset(m,0,2*TAM);
            if (c.x>=0) m[TAM+c.y*PASO+c.x*4+1]=1;   // un solo canal, 1/255: el cambio más pequeño posible
            if (c.x==-2) for (int k=0;k<TAM;k++) m[TAM+k]=(uint8_t)(k/4);
            if (FAILED(asignador->Reset()) || FAILED(lista->Reset(asignador.p,nullptr))) return false;
            for (int i=0;i<2;i++) {
                auto x=i ? nuevo.p : viejo.p;
                D3D12_RESOURCE_BARRIER br=transicion(x,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
                lista->ResourceBarrier(1,&br);
                D3D12_TEXTURE_COPY_LOCATION dst={x,D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX}, src={subida.p,D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
                src.PlacedFootprint.Offset=(UINT64)i*TAM; src.PlacedFootprint.Footprint={t.Format,(UINT)AN,(UINT)AL,1,(UINT)PASO};
                lista->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
                std::swap(br.Transition.StateBefore,br.Transition.StateAfter); lista->ResourceBarrier(1,&br);
            }
            ID3D12CommandList* comandos[]={lista.p};
            if (FAILED(lista->Close())) return false;
            cola->ExecuteCommandLists(1,comandos);
            if (!esperarGpu()) return false;
            Rect dano, otra;
            if (!diff(nuevo.p,viejo.p,{0,0,AN,AL},false,dano,unaPasadaGpu)) return false;
            auto& e=c.esperado;
            if (dano.x!=e.x || dano.y!=e.y || dano.w!=e.w || dano.h!=e.h) {
                HLOG("diff autoprueba (una pasada=%d): píxel (%d,%d) dio %d,%d %dx%d",unaPasadaGpu,c.x,c.y,dano.x,dano.y,dano.w,dano.h); return false;
            }
            if (unaPasadaGpu && (!diff(nuevo.p,viejo.p,{0,0,AN,AL},false,otra,true) || otra.w || otra.h)) {
                HLOG("diff autoprueba: una pasada no dejó igual el píxel (%d,%d)",c.x,c.y); return false;
            }
        }
        __android_log_print(ANDROID_LOG_INFO,"hwc.hyperv","diff: autoprueba en la GPU correcta (una pasada=%d)",unaPasadaGpu);
        return true;
    }
    // Compara el client target con imagenGpu (lo que ya tiene el Visor), copia r y marca como pendiente solo lo distinto.
    bool copiarComparando(ID3D12Resource* tex, Rect r, uint32_t& dGpu) {
        int64_t tg0=ahoraNs();
        Rect dano;
        if (!diff(tex,imagenGpu.p,r,true,dano,unaPasada && unaPasadaProp)) return false;
        dGpu=deltaUs(tg0,ahoraNs());
        std::lock_guard l(mx);
        hayImagen=true;
        pendiente=unir(pendiente,dano);
        if (toca(dano,nitidez)) ultimoDano=ahoraNs();
        return true;
    }
    bool copiar(ID3D12Resource* tex, Rect r, uint32_t& dGpu, uint32_t& dMemcpy, bool leer=false) {
        std::lock_guard gpuLock(imagenMx);
        dGpu = dMemcpy = 0;
        if (!gpu() || !asignador.p || !lista.p || !fence.p) return false;
        if (leer) tex=imagenGpu.p;
        if (!tex) return false;
        auto d=sdg::dispositivo(); auto desc=tex->GetDesc();
        if (desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Width!=(UINT64)w || desc.Height!=(UINT)h ||
            desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM || desc.SampleDesc.Count!=1 || desc.DepthOrArraySize!=1) return false;
        bool nueva=!imagenGpu.p;   // contenido indefinido: la primera copia no se compara
        if (!imagenGpu.p) {
            D3D12_HEAP_PROPERTIES heap={}; heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            auto copia=desc; copia.Flags=cargaTipada(d) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;   // diffcopia.hlsl
            if (FAILED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&copia,
                    D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&imagenGpu.p)))) return false;
        }
        if (!leer && !nueva && prepararDiff(d,tex)) {
            if (copiarComparando(tex,r,dGpu)) return true;
            diffRoto=true; HLOG("diff falló: vuelta a copiar sin comparar");
        }
        D3D12_RESOURCE_DESC bd={}; bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        UINT pitch=(r.w*4+255)&~255;
        bd.Width=(UINT64)pitch*r.h; bd.Height=1; bd.DepthOrArraySize=1; bd.MipLevels=1;
        bd.SampleDesc.Count=1; bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        // Un solo READBACK del tamaño de la pantalla completa, reutilizado: crearlo por cuadro cuesta llamadas a dxgkrnl.
        if (leer && !rb.p) {
            D3D12_RESOURCE_DESC todo=bd; todo.Width=(UINT64)((w*4+255)&~255)*h;
            D3D12_HEAP_PROPERTIES heap={}; heap.Type=D3D12_HEAP_TYPE_READBACK;
            if (FAILED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&todo,D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,IID_PPV_ARGS(&rb.p)))) return false;
        }
        D3D12_TEXTURE_COPY_LOCATION src={tex,D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
        D3D12_TEXTURE_COPY_LOCATION dst={leer ? rb.p : imagenGpu.p,
            leer ? D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT : D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
        if (leer) dst.PlacedFootprint.Footprint={desc.Format,(UINT)r.w,(UINT)r.h,1,pitch};
        D3D12_BOX box={(UINT)r.x,(UINT)r.y,0,(UINT)(r.x+r.w),(UINT)(r.y+r.h),1};
        D3D12_RESOURCE_BARRIER br={}; br.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        br.Transition={tex,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE};
        int64_t tg0 = ahoraNs();
        if (FAILED(asignador->Reset()) || FAILED(lista->Reset(asignador.p,nullptr))) return false;
        lista->ResourceBarrier(1,&br);
        D3D12_RESOURCE_BARRIER destino={}; destino.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        destino.Transition={imagenGpu.p,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST};
        if (!leer) lista->ResourceBarrier(1,&destino);
        lista->CopyTextureRegion(&dst,leer ? 0 : r.x,leer ? 0 : r.y,0,&src,&box);
        if (!leer) {
            std::swap(destino.Transition.StateBefore,destino.Transition.StateAfter);
            lista->ResourceBarrier(1,&destino);
        }
        std::swap(br.Transition.StateBefore,br.Transition.StateAfter); lista->ResourceBarrier(1,&br);
        if (FAILED(lista->Close())) return false;
        ID3D12CommandList* comandos[]={lista.p}; cola->ExecuteCommandLists(1,comandos);
        if (!esperarGpu()) return false;
        int64_t tg1 = ahoraNs();
        dGpu = deltaUs(tg0, tg1);

        if (!leer) {
            std::lock_guard l(mx);
            hayImagen = true;
            pendiente = unir(pendiente, r);
            if (toca(r,nitidez)) ultimoDano = ahoraNs();
        }
        return true;
    }
    // fuente=nullptr: imagenGpu (interfaz) a rbBC1. Con fuente (búfer de la capa de vídeo, ya en COMMON): a destino.
    bool comprimir(Rect r, uint32_t& dGpu, ID3D12Resource* fuente=nullptr, ID3D12Resource* destino=nullptr) {
        std::lock_guard gpuLock(imagenMx);
        if (!gpu() || !(fuente ? fuente : imagenGpu.p)) return false;
        auto d=sdg::dispositivo();
        if (!raizBC1.p && FAILED(d->CreateRootSignature(0,bc1_root_signature,sizeof bc1_root_signature,
                IID_PPV_ARGS(&raizBC1.p)))) return false;
        if (!psoBC1.p) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC p={}; p.pRootSignature=raizBC1.p;
            p.CS={bc1_dxil,sizeof bc1_dxil};
            if (FAILED(d->CreateComputePipelineState(&p,IID_PPV_ARGS(&psoBC1.p)))) return false;
        }
        if (!srvBC1.p) {
            D3D12_DESCRIPTOR_HEAP_DESC hd={}; hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            hd.NumDescriptors=2; hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;   // [0] imagenGpu, [1] vídeo
            if (FAILED(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srvBC1.p)))) return false;
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC s={}; s.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D; s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels=1;
        auto cpu=srvBC1->GetCPUDescriptorHandleForHeapStart(); auto gpuH=srvBC1->GetGPUDescriptorHandleForHeapStart();
        if (fuente) {   // el trabajo anterior ya terminó (espera síncrona bajo imagenMx): se puede reescribir
            auto paso=d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); cpu.ptr+=paso; gpuH.ptr+=paso;
        } else fuente=imagenGpu.p;
        d->CreateShaderResourceView(fuente,&s,cpu);   // imagenGpu solo cambia al crearse: reescribirla cuesta poco
        D3D12_RESOURCE_DESC b={}; b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        b.Width=bytesBC1(completa()); b.Height=1; b.DepthOrArraySize=1; b.MipLevels=1;
        b.SampleDesc.Count=1; b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES heap={};
        if (!bloquesBC1.p) {
            heap.Type=D3D12_HEAP_TYPE_DEFAULT; b.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            if (FAILED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&b,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    nullptr,IID_PPV_ARGS(&bloquesBC1.p)))) return false;
        }
        if (!rbBC1.p) {
            heap.Type=D3D12_HEAP_TYPE_READBACK; b.Flags=D3D12_RESOURCE_FLAG_NONE;
            if (FAILED(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&b,D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,IID_PPV_ARGS(&rbBC1.p)))) return false;
        }
        if (!destino) destino=rbBC1.p;
        int64_t inicio=ahoraNs();
        if (FAILED(asignador->Reset()) || FAILED(lista->Reset(asignador.p,psoBC1.p))) return false;
        D3D12_RESOURCE_BARRIER textura={}; textura.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        textura.Transition={fuente,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
        lista->ResourceBarrier(1,&textura);
        ID3D12DescriptorHeap* heaps[]={srvBC1.p}; lista->SetDescriptorHeaps(1,heaps);
        lista->SetComputeRootSignature(raizBC1.p);
        lista->SetComputeRootDescriptorTable(0,gpuH);
        lista->SetComputeRootUnorderedAccessView(1,bloquesBC1->GetGPUVirtualAddress());
        uint32_t constantes[]={(uint32_t)r.x,(uint32_t)r.y,(uint32_t)r.w,(uint32_t)r.h};
        lista->SetComputeRoot32BitConstants(2,4,constantes,0);
        lista->Dispatch((r.w+31)/32,(r.h+31)/32,1);
        D3D12_RESOURCE_BARRIER salida={}; salida.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        salida.Transition={bloquesBC1.p,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};
        lista->ResourceBarrier(1,&salida);
        lista->CopyBufferRegion(destino,0,bloquesBC1.p,0,bytesBC1(r));
        std::swap(salida.Transition.StateBefore,salida.Transition.StateAfter); lista->ResourceBarrier(1,&salida);
        std::swap(textura.Transition.StateBefore,textura.Transition.StateAfter); lista->ResourceBarrier(1,&textura);
        if (FAILED(lista->Close())) return false;
        ID3D12CommandList* comandos[]={lista.p}; cola->ExecuteCommandLists(1,comandos);
        if (!esperarGpu()) return false;
        dGpu=deltaUs(inicio,ahoraNs()); return true;
    }
    // Comprime a BC1 el búfer de la capa de vídeo en un READBACK libre y lo deja listo para servir(). Si algo falla, la
    // capa vuelve a CLIENT para siempre (videoRoto) y se pide un validate nuevo.
    void publicarVideo(Trabajo& t, int& listo) {
        if (t.acqVideo.get()>=0) {
            pollfd p={t.acqVideo.get(),POLLIN,0}; int n;
            do n=poll(&p,1,3000); while (n<0 && errno==EINTR);
            t.acqVideo=ndk::ScopedFileDescriptor();
            if (n<=0 || !(p.revents&POLLIN)) {   // búfer quizá aún en escritura: no se lee; se reintenta con el siguiente
                HLOG("vídeo: acquire fence sin señalar en 3 s (revents=0x%x)",p.revents);
                std::lock_guard l(mx); ultimoVideoId=0; return;
            }
        }
        int i=0;
        { std::lock_guard l(mx); while (i==videoListo || i==videoEnvio) i++; }
        uint32_t dGpu=0;
        bool ok=false;
        if (!rbVideo[i].p) {
            auto d=sdg::dispositivo();
            D3D12_HEAP_PROPERTIES heap={}; heap.Type=D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC b={}; b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; b.Width=bytesBC1(completa()); b.Height=1;
            b.DepthOrArraySize=1; b.MipLevels=1; b.SampleDesc.Count=1; b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if (d) (void)d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&b,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&rbVideo[i].p));
        }
        if (rbVideo[i].p) ok=comprimir({0,0,(int32_t)t.info.ancho,(int32_t)t.info.alto},dGpu,t.video,rbVideo[i].p);
        if (!ok) {
            if (!videoRoto.exchange(true)) HLOG("capa de vídeo: falló la compresión; vuelve a CLIENT");
            { std::lock_guard l(mx); ultimoVideoId=0; }
            std::shared_ptr<IComposerCallback> cb; { std::lock_guard v(vmx); cb=callback; } if (cb) cb->onRefresh(0);
            return;
        }
        statsVideo.registrarCopia(0,dGpu,0,"video");
        listo=i;
    }
    void trabajar() {
        while (true) {
            Trabajo t;
            {
                std::unique_lock lk(colaMx);
                colaCv.wait(lk, [&] { return parar.load() || !colaTrabajos.empty(); });
                if (parar.load() && colaTrabajos.empty()) break;
                t = std::move(colaTrabajos.front());
                colaTrabajos.pop();
            }

            int64_t t0 = ahoraNs();
            if (t.acquire.get() >= 0) {
                pollfd p = {t.acquire.get(), POLLIN, 0};
                int n;
                do { n = poll(&p, 1, 3000); } while (n < 0 && errno == EINTR);
                if (n <= 0 || !(p.revents & POLLIN)) HLOG("aviso: acquire fence timeout o error (revents=0x%x)", p.revents);
                t.acquire = ndk::ScopedFileDescriptor();
            }
            int64_t t1 = ahoraNs();
            uint32_t dAcq = deltaUs(t0, t1);

            uint32_t dGpu = 0, dMemcpy = 0;
            bool ok = false;
            if (t.tex && t.dano.w && t.dano.h) {
                ok = copiar(t.tex, t.dano, dGpu, dMemcpy);
                // imagenGpu pudo quedar a medias sin daño marcado: reenviarla entera deja al Visor igual que ella.
                if (!ok) { HLOG("falló copia del client target en trabajador"); std::lock_guard l(mx); pendiente=completa(); }
            }

            int listoVideo=-1;
            if (t.video) publicarVideo(t,listoVideo);
            {   // todo el estado del vídeo de este present de una vez (servir() no envía mientras haya trabajos con vídeo)
                std::lock_guard l(mx);
                RegistroVideo n=videoInfo; n.hayVideo=t.info.hayVideo; n.destino=t.info.destino;
                memcpy(n.recorte,t.info.recorte,sizeof n.recorte);
                if (listoVideo>=0) { videoListo=listoVideo; videoNuevo=true; n.ancho=t.info.ancho; n.alto=t.info.alto; }
                if (memcmp(&n,&videoInfo,sizeof n)) { videoInfo=n; videoCambio=true; }
            }
            // Fence de present: se señala al tener el cuadro copiado (el client target ya se puede reutilizar). Medido 17/11:
            // señalarla al confirmar el Visor (unos 12 ms) hacía que SurfaceFlinger frenara: jank de paneles del 7-18 % al 29-39 %.
            if (t.evento>=0) senal(t.evento);
            { std::lock_guard l(mx); --copiasPendientes; }
            despertar();

            stats.registrarCopia(dAcq, dGpu, dMemcpy);

            {   // siempre: la espera diferida de presentar() mira el trabajo anterior, sea cual sea su modo
                std::lock_guard clk(sincMx);
                ultimoTrabajoCompletado = t.id;
            }
            sincCv.notify_all();
        }
    }
    int target(const ClientTarget& t) {
        const auto& b=t.buffer;
        if (b.slot<0 || b.slot>=numeroSlots) return IComposerClient::EX_BAD_PARAMETER;
        if (b.handle) {
            const auto& nh=*b.handle;
            if (nh.fds.size()!=sdg::N_FDS || nh.ints.size()!=sdg::N_INTS || nh.ints[sdg::I_MAGIA]!=sdg::MAGIA ||
                nh.ints[sdg::I_ANCHO]!=w || nh.ints[sdg::I_ALTO]!=h || nh.ints[sdg::I_FORMATO]!=1 ||
                nh.ints[sdg::I_TIPO]==sdg::T_MEMFD || nh.fds[0].get()<0) return IComposerClient::EX_BAD_PARAMETER;
            uint64_t id=(uint32_t)nh.ints[sdg::I_ID_LO] | ((uint64_t)(uint32_t)nh.ints[sdg::I_ID_HI]<<32);
            auto& s=slots[b.slot];
            if (!s || s->id!=id) {
                auto nuevo=std::make_unique<Slot>(); nuevo->id=id;
                auto d=sdg::dispositivo();
                if (!d || FAILED(d->OpenSharedHandle((HANDLE)(intptr_t)nh.fds[0].get(),IID_PPV_ARGS(&nuevo->textura.p))))
                    return IComposerClient::EX_NO_RESOURCES;
                s=std::move(nuevo);
            }
        }
        if (!slots.count(b.slot) || !slots[b.slot]) return IComposerClient::EX_BAD_PARAMETER;
        ndk::ScopedFileDescriptor f(b.fence.get()<0 ? -1 : dup(b.fence.get()));
        if (b.fence.get()>=0 && f.get()<0) return IComposerClient::EX_NO_RESOURCES;
        Rect dano={};
        for (const auto& r:t.damage) {
            if (r.left<0 || r.top<0 || r.right<r.left || r.bottom<r.top || r.right>w || r.bottom>h)
                return IComposerClient::EX_BAD_PARAMETER;
            dano=unir(dano,{r.left,r.top,r.right-r.left,r.bottom-r.top});
        }
        actual=b.slot; acquire=std::move(f); danoTarget=t.damage.empty()?completa():dano; return 0;
    }
    void esperarTrabajo(uint64_t hasta) {   // sin mx tomado: el trabajador lo necesita para terminar
        std::unique_lock clk(sincMx);
        sincCv.wait(clk, [&] { return ultimoTrabajoCompletado >= hasta || parar.load(); });
    }
    int presentar(std::unique_lock<std::mutex>& l, ndk::ScopedFileDescriptor& outFence) {
        // setprop debug.sd.hwc.present_fence (se relee como mucho cada 0,5 s):
        //   sin valor o false = sin fence y present síncrono (A/B 17/11, 24 pasadas: el mejor, jank 7,9 % frente a 9,8 %);
        //   true = fence que se señala al terminar la copia; signaled = present síncrono con fence ya señalada.
        // Espera diferida (por omisión; setprop debug.sd.hwc.espera sincrona = como antes): present(N) vuelve sin esperar su
        // copia; se espera la del cuadro N-1. A/B 17/11 (12 corridas): SurfaceFlinger 7,1 -> 2,2 ms por cuadro, cuadros de
        // SF de 9 ms o más 24 % -> 6 % en paneles y 8,9 % -> 0,3 % al desplazar Ajustes.
        // El client target N sigue intacto hasta entonces: FramebufferSurface (android17-release) solo suelta el búfer
        // anterior en onFrameCommitted, tras el present siguiente ("We only release the frame once an entire new frame
        // has replaced it"). SurfaceFlinger no queda parado ~4-6 ms por cuadro (medido 17/11).
        static int64_t leida=0; static int modo=0; static bool diferida=true;
        if (int64_t t=ahoraNs(); t-leida>500000000) {
            char v[PROP_VALUE_MAX]={}; __system_property_get("debug.sd.hwc.present_fence",v);
            modo=!strcmp(v,"true") || !strcmp(v,"1") ? 1 : !strcmp(v,"signaled") ? 2 : 0; leida=t;
            v[0]=0; __system_property_get("debug.sd.hwc.espera",v);
            if (bool d=strcmp(v,"sincrona")!=0; d!=diferida) { diferida=d; __android_log_print(ANDROID_LOG_INFO,"hwc.hyperv","espera diferida=%d",d); }
            v[0]=0; __system_property_get("debug.sd.hwc.una_pasada",v);
            if (bool u=strcmp(v,"0")!=0; u!=unaPasadaProp) { unaPasadaProp=u; __android_log_print(ANDROID_LOG_INFO,"hwc.hyperv","una pasada=%d",u); }
            v[0]=0; __system_property_get("debug.sd.hwc.capa_video",v);   // 0 = vídeo siempre CLIENT (A/B en caliente)
            if (bool u=strcmp(v,"0")!=0; u!=capaVideoProp) { capaVideoProp=u; __android_log_print(ANDROID_LOG_INFO,"hwc.hyperv","capa de vídeo=%d",u); }
        }
        // Antes de todo, también con daño vacío: al volver de este present SurfaceFlinger suelta el búfer anterior
        // (Output::presentFrameAndReleaseLayers: presentFrame() y después onPresentDisplayCompleted()).
        // Sin condición: en modo síncrono ya está hecho (no cuesta) y así apagar el interruptor en caliente no deja uno suelto.
        { uint64_t hasta = siguienteTrabajoId; l.unlock(); esperarTrabajo(hasta); l.lock(); }
        if (actual<0 || !slots.count(actual) || !slots[actual]) return IComposerClient::EX_BAD_PARAMETER;
        Rect r = presentado ? danoTarget : completa();
        presentado = true;
        danoTarget = {};
        int ev=-1, releaseFd=modo ? nuevaFence(ev) : -1;
        if (modo==2 && ev>=0) { senal(ev); ev=-1; }   // nace señalada; el búfer lo protege la espera síncrona
        // Sin nada que copiar la fence nace señalada: SurfaceFlinger necesita una válida en cada present (ritmo y vsync).
        // Capa de vídeo DEVICE: su estado va en cada cuadro; su búfer solo si el Visor aún no lo tiene. No hace falta fence
        // de liberación: SurfaceFlinger suelta el búfer N-1 tras el present N, que antes espera al trabajo N-1 (arriba).
        RegistroVideo info; ID3D12Resource* vtex=nullptr; uint64_t vid=0; ndk::ScopedFileDescriptor vacq;
        if (auto it=capas.find(videoElegida); it!=capas.end() && it->second.tipo==Composition::DEVICE)
            if (auto* sl=it->second.actual(); sl && sl->textura.p) {
                auto& k=it->second;
                info.hayVideo=1; info.destino=k.marco; memcpy(info.recorte,k.recorte,sizeof info.recorte);
                info.ancho=sl->ancho; info.alto=sl->alto;
                if (sl->id!=ultimoVideoId) { vtex=sl->textura.p; vid=sl->id; vacq=std::move(k.acquire); ultimoVideoId=vid; }
            }
        if ((!r.w || !r.h) && !info.hayVideo && !videoInfo.hayVideo) { if (ev>=0) senal(ev); outFence = ndk::ScopedFileDescriptor(releaseFd); return 0; }
        // ultimoDano lo pone la copia con el daño real: r es casi siempre pantalla entera (SurfaceFlinger no da daño) y
        // con él cualquier animación en otra zona (un vídeo, un spinner) aplazaría para siempre el reenvío sin pérdida.
        ++copiasPendientes;

        uint64_t trabId = ++siguienteTrabajoId;
        bool sinc = (ev < 0);
        {
            std::lock_guard lk(colaMx);
            colaTrabajos.emplace(trabId, slots[actual]->textura.p, std::move(acquire), r, ev, sinc);
            auto& t=colaTrabajos.back(); t.info=info; t.video=vtex; t.idVideo=vid; t.acqVideo=std::move(vacq);
            if (vtex) vtex->AddRef();
            colaCv.notify_one();
        }
        outFence = ndk::ScopedFileDescriptor(releaseFd);
        // Espera síncrona a la copia fuera de mx (modos false y signaled, o sin /dev/sdsync).
        if (sinc && !diferida) { l.unlock(); esperarTrabajo(trabId); l.lock(); }
        return 0;
    }
    void generarVsync() {
        std::unique_lock l(vmx);
        while (!parar) {
            vcv.wait(l,[&]{return parar || (vsync && encendida && callback);}); if (parar) break;
            timespec ahora; clock_gettime(CLOCK_MONOTONIC,&ahora);
            int64_t proximo=(int64_t)ahora.tv_sec*1000000000+ahora.tv_nsec;
            while (!parar && vsync && encendida && callback) {
                proximo+=period; timespec t={proximo/1000000000,proximo%1000000000};
                l.unlock(); int e; do {e=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&t,nullptr);} while(e==EINTR); l.lock();
                if (parar || !vsync || !encendida || !callback) break;
                auto cb=callback; l.unlock(); cb->onVsync(0,proximo,period); l.lock();
                clock_gettime(CLOCK_MONOTONIC,&ahora);
                int64_t n=(int64_t)ahora.tv_sec*1000000000+ahora.tv_nsec;
                if (n>proximo+period) proximo=n;
            }
        }
    }
    static void consumirPartes(std::vector<iovec>& partes, size_t& primera, size_t bytes) {
        while (primera<partes.size() && bytes>=partes[primera].iov_len)
            bytes-=partes[primera++].iov_len;
        if (bytes) {
            partes[primera].iov_base=(uint8_t*)partes[primera].iov_base+bytes;
            partes[primera].iov_len-=bytes;
        }
    }
    static bool enviarPartes(int fd,std::vector<iovec>& partes) {
        size_t primera=0;
        while (primera<partes.size()) {
            msghdr msg={}; msg.msg_iov=partes.data()+primera;
            // Linux admite como maximo 1024 iovec por sendmsg.
            msg.msg_iovlen=std::min<size_t>(1024,partes.size()-primera);
            ssize_t k=sendmsg(fd,&msg,MSG_NOSIGNAL);
            if (k<0 && errno==EINTR) continue;
            if (k<=0) return false;
            consumirPartes(partes,primera,(size_t)k);
        }
        return true;
    }
    void servir() {
        ndk::ScopedFileDescriptor escucha(socket(AF_VSOCK,SOCK_STREAM|SOCK_CLOEXEC,0));
        sockaddr_vm dir={}; dir.svm_family=AF_VSOCK; dir.svm_cid=VMADDR_CID_ANY; dir.svm_port=5001;
        // Si el composer se reinicia, el puerto puede seguir ocupado un momento: reintentar en vez de quedar sin video.
        for (int i=0; escucha.get()<0 || bind(escucha.get(),(sockaddr*)&dir,sizeof dir) || listen(escucha.get(),1); i++) {
            if (i%10==0) HLOG("vsock 5001: %s (reintento)",strerror(errno));
            { std::lock_guard l(vmx); if(parar) return; }
            sleep(1); escucha=ndk::ScopedFileDescriptor(socket(AF_VSOCK,SOCK_STREAM|SOCK_CLOEXEC,0));
        }
        ndk::ScopedFileDescriptor cliente;
        bool ocupado=false; uint32_t id=0; Listo ack={}; size_t recibidos=0;
        for (;;) {
            // Se escucha siempre: si el Visor se cerró con la VM en pausa, el guest no ve el cierre; la conexión nueva
            // sustituye a la vieja. Y un Listo que tarda más de 2 s da la conexión por perdida.
            pollfd fds[]={{aviso.get(),POLLIN,0},{escucha.get(),POLLIN,0},{cliente.get(),POLLIN,0}};
            int timeout=-1;
            { std::lock_guard l(mx);
                if (cliente.get()>=0 && !ocupado && !copiasPendientes && nitidez.w && hayImagen)
                    timeout=(int)std::max<int64_t>(0,(ultimoDano+80000000-ahoraNs()+999999)/1000000);
            }
            if (ocupado) timeout=(int)std::max<int64_t>(0,(tEnvioFin+2000000000-ahoraNs()+999999)/1000000);
            int n; do {n=poll(fds,cliente.get()>=0?3:2,timeout);} while(n<0 && errno==EINTR);
            if (n<0) {HLOG("poll vsock: %s",strerror(errno)); return;}
            if (fds[0].revents&POLLIN) { uint64_t v; (void)read(aviso.get(),&v,sizeof v); }
            { std::lock_guard l(vmx); if(parar) break; }
            if (ocupado && ahoraNs()-tEnvioFin>2000000000) {   // sin Listo: el Visor desapareció
                {std::lock_guard l(mx); pendiente=completa();} cliente=ndk::ScopedFileDescriptor(); ocupado=false; continue;
            }
            if (fds[1].revents&POLLIN) {
                cliente=ndk::ScopedFileDescriptor(accept4(escucha.get(),nullptr,nullptr,SOCK_CLOEXEC));
                if(cliente.get()<0) continue;
                timeval limite={2,0}; setsockopt(cliente.get(),SOL_SOCKET,SO_SNDTIMEO,&limite,sizeof limite);
                ocupado=false; recibidos=0; visorV4=false;   // hasta que el Visor nuevo confirme con "SDO4"
                std::shared_ptr<IComposerCallback> cb;
                { std::lock_guard l(mx); pendiente=completa(); ultimoVideoId=0; videoListo=-1; videoNuevo=false; videoCambio=true; }
                { std::lock_guard l(vmx); cb=callback; } if(cb) cb->onRefresh(0);
            } else if (cliente.get()>=0 && fds[2].revents) {
                ssize_t k=recv(cliente.get(),(uint8_t*)&ack+recibidos,sizeof ack-recibidos,MSG_DONTWAIT);
                if(k>0) {
                    recibidos+=k;
                    if(recibidos==sizeof ack) {
                        if(!ocupado || (ack.magia!=SDOK && ack.magia!=SDO4) || ack.id!=id) k=0;
                        else {
                            // El Visor pide v4 al confirmar ("SDO4"): desde ahí el vídeo puede ir DEVICE (validate lo decide).
                            if (bool v4=ack.magia==SDO4; v4!=visorV4.exchange(v4)) {
                                HLOG("Visor v4=%d", v4);
                                std::shared_ptr<IComposerCallback> cb; { std::lock_guard l(vmx); cb=callback; } if(cb) cb->onRefresh(0);
                            }
                            int64_t tl=ahoraNs();
                            stats.registrarListo(deltaUs(tEnvioFin,tl));
                            ocupado=false; recibidos=0;
                        }
                    }
                }
                if(k==0 || (k<0 && errno!=EAGAIN && errno!=EINTR)) {
                    {std::lock_guard l(mx); pendiente=completa();} cliente=ndk::ScopedFileDescriptor(); continue;
                }
            }
            if(cliente.get()<0 || ocupado) continue;
            Rect r; bool bc1=false; size_t bytes=0;
            bool v4=visorV4; RegistroVideo rv;
            { std::lock_guard l(mx);
                if (!hayImagen) continue;
                // Con vídeo, interfaz y vídeo del mismo present: se espera a que el trabajador termine (lo avisa despertar()).
                if (v4 && copiasPendientes && (videoInfo.hayVideo || videoNuevo)) continue;
                bool soloVideo=v4 && (videoNuevo || videoCambio);   // el vídeo sale aunque la interfaz no cambie
                // Afinar (reenviar sin pérdida lo que fue en BC1) va por franjas de ~3 MB: un toque o cuadro nuevo no espera
                // a 22 MB de pantalla entera (medido: envío p95 50-80 ms y 24 ms de subida en el hilo de la ventana del Visor).
                bool afinar=!pendiente.w && nitidez.w && !copiasPendientes && ahoraNs()-ultimoDano>=80000000;
                r=afinar ? nitidez : pendiente;
                if (afinar) r.h=std::min(r.h,std::max(4,(3<<20)/(r.w*4)));
                if ((!r.w || !r.h) && !soloVideo) continue;
                if (v4) {
                    rv=videoInfo; rv.hayVideo=rv.hayVideo && rv.ancho;   // sin búfer recibido aún: como si no hubiera vídeo
                    rv.nuevo=videoNuevo && videoListo>=0;
                    if (rv.nuevo) { videoEnvio=videoListo; videoListo=-1; }
                    videoNuevo=videoCambio=false;
                }
                if (!r.w || !r.h) r={};
                else {
                // BC1 por omisión (persist.vendor.hyperv.bc1=0 lo apaga). Los cuadros BC1 "viejos" del 16/11 los causaba Mesa al
                // no encolar búferes nuevos (flush de swap saltado en swrast, arreglado en compilar_mesa.sh).
                static const bool bc1Activo = [] { char v[PROP_VALUE_MAX]={}; __system_property_get("persist.vendor.hyperv.bc1",v); return v[0]!='0'; }();
                bc1=bc1Activo && !afinar && !(w%4 || h%4) && escalaDano(r,w,h)==2;
                if (bc1) r=alinearBC1(r,w,h);
                bytes=bc1 ? bytesBC1(r) : (size_t)r.w*r.h*4;
                // La textura propia conserva el contenido tras liberar el client target.
                if (bc1) {
                    ultimoDano=ahoraNs();   // hay movimiento: el afinado espera aunque esta zona no toque la pendiente
                    nitidez=unir(nitidez,r);
                }
                else if (afinar) { nitidez.y+=r.h; nitidez.h-=r.h; if (nitidez.h<=0) nitidez={}; }
                pendiente={};
                }
            }
            // Si algo falla, la conexión se cierra y al volver se manda todo (y el vídeo) de nuevo.
            auto perder=[&]{ {std::lock_guard l(mx); pendiente=completa(); videoEnvio=-1;} cliente=ndk::ScopedFileDescriptor(); };
            uint32_t dGpu=0,dMemcpy=0;
            // Solo este hilo usa rb; el trabajador modifica exclusivamente imagenGpu.
            if (r.w && !(bc1 ? comprimir(r,dGpu) : copiar(nullptr,r,dGpu,dMemcpy,true))) {
                HLOG("fallo readback para envio"); perder(); continue;
            }
            uint8_t* p=nullptr; size_t pitch=(r.w*4+255)&~255;
            auto lectura=bc1 ? rbBC1.p : rb.p;
            D3D12_RANGE rango={0,bc1 ? bytes : pitch*(size_t)r.h};
            if (r.w && FAILED(lectura->Map(0,&rango,(void**)&p))) { perder(); continue; }
            uint8_t* pv=nullptr; ID3D12Resource* lecturaVideo=rv.nuevo ? rbVideo[videoEnvio].p : nullptr;
            size_t bytesVideo=rv.nuevo ? bytesBC1({0,0,(int32_t)rv.ancho,(int32_t)rv.alto}) : 0;
            D3D12_RANGE rangoVideo={0,bytesVideo};
            if (lecturaVideo && FAILED(lecturaVideo->Map(0,&rangoVideo,(void**)&pv))) {
                if (r.w) { D3D12_RANGE nada={0,0}; lectura->Unmap(0,&nada); }
                perder(); continue;
            }
            Cuadro cab; cab.id=++id; cab.ancho=w; cab.alto=h; cab.reservado[0]=1;   // "sé v4"
            cab.escala=1; cab.version=v4 ? 4 : bc1 ? 3 : 1; cab.formato=bc1 ? 71 : 1; cab.nRect=r.w ? 1 : 0;
            std::vector<iovec> partes={{&cab,sizeof cab}};
            if (v4) partes.push_back({&rv,sizeof rv});
            if (r.w) partes.push_back({&r,sizeof r});
            int64_t tc0=ahoraNs();
            if (!r.w) {
            } else if (bc1) {
                partes.push_back({p,bytes});
            } else {
                for (int y=0;y<r.h;y++) partes.push_back({p+(size_t)y*pitch,(size_t)r.w*4});
            }
            if (pv) partes.push_back({pv,bytesVideo});
            int64_t te0=ahoraNs();
            statsLectura.registrarCopia(0,dGpu,deltaUs(tc0,te0),"readback");
            bool okEnvio = enviarPartes(cliente.get(),partes);
            D3D12_RANGE nada={0,0};
            if (r.w) lectura->Unmap(0,&nada);
            if (lecturaVideo) lecturaVideo->Unmap(0,&nada);
            { std::lock_guard l(mx); videoEnvio=-1; }
            int64_t te1=ahoraNs();
            if(!okEnvio) { perder(); continue; }
            stats.registrarEnvio(deltaUs(te0,te1));
            tEnvioFin=te1;
            ocupado=true;
        }
    }
public:
    Cliente() {
        comprobarBloque(); comprobarVideo();
        uint8_t prueba[12]={};
        std::vector<iovec> partes={{prueba,4},{prueba+4,8}};
        size_t primera=0;
        consumirPartes(partes,primera,6);
        assert(primera==1 && partes[1].iov_base==prueba+6 && partes[1].iov_len==6);
        consumirPartes(partes,primera,6); assert(primera==partes.size());
        partes={{prueba,4},{prueba+4,8}}; primera=0;
        consumirPartes(partes,primera,4);
        assert(primera==1 && partes[1].iov_base==prueba+4 && partes[1].iov_len==8);
        char prop[PROP_VALUE_MAX]={}, resto; int ancho,alto,hz;
        __system_property_get("vendor.hyperv.pantalla",prop);
        if (sscanf(prop,"%dx%d@%d%c",&ancho,&alto,&hz,&resto)==3 && ancho>0 && alto>0 && ancho<=16384 && alto<=16384 && hz>=1 && hz<=1000) {
            w=ancho; h=alto; period=1000000000/hz;
        }
        aviso=ndk::ScopedFileDescriptor(eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK));
        if(aviso.get()<0) {HLOG("eventfd: %s",strerror(errno)); return;}
        sdsyncFd = open("/dev/sdsync", O_RDWR | O_CLOEXEC);
        if (sdsyncFd < 0) HLOG("/dev/sdsync no disponible (%s): present síncrono y sin fence", strerror(errno));
        hiloVsync=std::thread(&Cliente::generarVsync,this);
        hiloTrabajador=std::thread(&Cliente::trabajar,this);
        hiloSocket=std::thread(&Cliente::servir,this);
    }
    ~Cliente() override {
        {std::lock_guard l(vmx); parar=true;} vcv.notify_all();
        {std::lock_guard l(colaMx);} colaCv.notify_all(); sincCv.notify_all();
        despertar();
        if(hiloVsync.joinable()) hiloVsync.join();
        if(hiloTrabajador.joinable()) hiloTrabajador.join();
        if(hiloSocket.joinable()) hiloSocket.join();
        if(sdsyncFd>=0) close(sdsyncFd);
    }
    Status registerCallback(const std::shared_ptr<IComposerCallback>& cb) override {
        if(!cb) return error(EX_BAD_PARAMETER);
        {std::lock_guard l(vmx); callback=cb;} vcv.notify_all();
        return cb->onHotplugEvent(0,common::DisplayHotplugEvent::CONNECTED);
    }
    Status setVsyncEnabled(int64_t d,bool enabled) override {
        if(d) return error(EX_BAD_DISPLAY);
        {std::lock_guard l(vmx); vsync=enabled;} vcv.notify_all(); return ok();
    }
    Status setClientTargetSlotCount(int64_t d,int32_t n) override {
        if(d) return error(EX_BAD_DISPLAY); if(n<0 || n>1024) return error(EX_BAD_PARAMETER);
        // Con espera diferida puede quedar un trabajo leyendo un búfer que SurfaceFlinger va a soltar: se termina antes.
        uint64_t hasta; { std::lock_guard l(mx); hasta=siguienteTrabajoId; } esperarTrabajo(hasta);
        std::lock_guard l(mx); slots.clear(); actual=-1; numeroSlots=n; return ok();
    }
    Status createLayer(int64_t d,int32_t count,int64_t* out) override {
        if(d) return error(EX_BAD_DISPLAY); if(count<0) return error(EX_BAD_PARAMETER);
        std::lock_guard l(mx); *out=siguienteCapa++; capas[*out]; return ok();
    }
    Status destroyLayer(int64_t d,int64_t id) override {
        if(d) return error(EX_BAD_DISPLAY); std::lock_guard l(mx);
        return capas.erase(id)?ok():error(EX_BAD_LAYER);
    }
    // Búfer de una capa: solo los de vídeo (USO_SD_VIDEO, nuestro gralloc, RGBA en GPU) se abren; del resto basta el uso.
    void capaBuffer(Capa& k, const Buffer& b) {
        k.slot=b.slot;
        k.acquire=ndk::ScopedFileDescriptor(b.fence.get()>=0 ? dup(b.fence.get()) : -1);
        if (!b.handle) return;
        auto s=std::make_unique<Slot>(); const auto& nh=*b.handle;
        if (nh.fds.size()==sdg::N_FDS && nh.ints.size()==sdg::N_INTS && nh.ints[sdg::I_MAGIA]==sdg::MAGIA && nh.fds[0].get()>=0 &&
            nh.ints[sdg::I_TIPO]==sdg::T_D3D12_GPU && nh.ints[sdg::I_FORMATO]==1) {
            s->uso=(uint32_t)nh.ints[sdg::I_USO_LO] | ((uint64_t)(uint32_t)nh.ints[sdg::I_USO_HI]<<32);
            s->id=(uint32_t)nh.ints[sdg::I_ID_LO] | ((uint64_t)(uint32_t)nh.ints[sdg::I_ID_HI]<<32);
            s->ancho=nh.ints[sdg::I_ANCHO]; s->alto=nh.ints[sdg::I_ALTO];
            if ((s->uso&USO_SD_VIDEO) && !(s->uso&USO_PROTEGIDO)) {
                auto d=sdg::dispositivo();
                if (!d || FAILED(d->OpenSharedHandle((HANDLE)(intptr_t)nh.fds[0].get(),IID_PPV_ARGS(&s->textura.p)))) s->textura.p=nullptr;
            }
        }
        k.slots[b.slot]=std::move(s);
    }
    int64_t elegirCapaVideo() {
        if (!visorV4 || !capaVideoProp || videoRoto || matrizColor) return 0;
        std::vector<VistaCapa> v;
        for (auto& [id,k]:capas) {
            VistaCapa c; c.id=id; c.device=k.tipo==Composition::DEVICE; c.marco=k.marco; memcpy(c.recorte,k.recorte,sizeof c.recorte);
            c.transform=k.transform; c.z=k.z; c.alfa=k.alfa;
            if (auto* sl=k.actual()) { c.video=(sl->uso&USO_SD_VIDEO) && !(sl->uso&USO_PROTEGIDO); c.lista=sl->textura.p; c.ancho=sl->ancho; c.alto=sl->alto; }
            v.push_back(c);
        }
        return elegirVideo(v,w,h);
    }
    Status executeCommands(const std::vector<DisplayCommand>& commands,std::vector<CommandResultPayload>* out) override {
        std::unique_lock l(mx); impl::ComposerServiceWriter writer;
        std::vector<CommandResultPayload> fences;
        for(size_t i=0;i<commands.size();i++) {
            const auto& c=commands[i];
            if(c.display) {writer.setError(i,EX_BAD_DISPLAY); continue;}
            // Matriz de color y brillo se aceptan: todo va en composición CLIENT y SurfaceFlinger aplica la matriz al componer
            // (no anunciamos SKIP_CLIENT_COLOR_TRANSFORM). Un error aquí tumba el validate/present de todo el cuadro y
            // SurfaceFlinger deja negras las capas que creía DEVICE.
            // El Visor no aplica la matriz de color al vídeo: con una que no sea la identidad, el vídeo va CLIENT.
            if(c.colorTransformMatrix) { auto& m=*c.colorTransformMatrix; matrizColor=false;
                for(size_t j=0;j<m.size();j++) if(m[j]!=(j%5==0 ? 1.f : 0.f)) matrizColor=true; }
            if(c.virtualDisplayOutputBuffer || c.pictureProfileId || c.activeConfig) {
                writer.setError(i,EX_UNSUPPORTED); continue;
            }
            bool fallo=false;
            for(const auto& lc:c.layers) {
                auto it=capas.find(lc.layer);
                if(it==capas.end()) {writer.setError(i,EX_BAD_LAYER); fallo=true; continue;}
                auto& k=it->second;
                if(lc.composition) k.tipo=lc.composition->composition;
                if(lc.displayFrame) { auto& f=*lc.displayFrame; k.marco={f.left,f.top,f.right-f.left,f.bottom-f.top}; }
                if(lc.sourceCrop) { auto& f=*lc.sourceCrop; k.recorte[0]=f.left; k.recorte[1]=f.top; k.recorte[2]=f.right; k.recorte[3]=f.bottom; }
                if(lc.transform) k.transform=(int)lc.transform->transform;
                if(lc.z) k.z=lc.z->z;
                if(lc.planeAlpha) k.alfa=lc.planeAlpha->alpha;
                if(lc.bufferSlotsToClear) for(int32_t n:*lc.bufferSlotsToClear) k.slots.erase(n);
                if(lc.buffer) capaBuffer(k,*lc.buffer);
            }
            if(c.clientTarget) {int e=target(*c.clientTarget); if(e) {writer.setError(i,e); fallo=true;}}
            if(fallo) continue;
            if(c.validateDisplay || c.presentOrValidateDisplay) {
                std::vector<int64_t> ids; std::vector<Composition> tipos;
                videoElegida=elegirCapaVideo();
                for(auto& [id,k]:capas) if(auto t=id==videoElegida ? Composition::DEVICE : Composition::CLIENT; k.tipo!=t) {ids.push_back(id); tipos.push_back(t);}
                writer.setChangedCompositionTypes(0,ids,tipos);
                // Que SurfaceFlinger deje alfa 0 en el hueco del vídeo (el Visor mezcla la interfaz encima).
                if(videoElegida) writer.setDisplayRequests(0,0,{videoElegida},{DisplayRequest::LayerRequest::CLEAR_CLIENT_TARGET});
                else writer.setDisplayRequests(0,0,{},{});
                ClientTargetProperty propiedad;
                propiedad.pixelFormat=common::PixelFormat::RGBA_8888; propiedad.dataspace=common::Dataspace::UNKNOWN;
                writer.setClientTargetProperty(0,propiedad,1.f,DimmingStage::NONE);
                if(c.presentOrValidateDisplay) writer.setPresentOrValidateResult(0,PresentOrValidate::Result::Validated);
            }
            if(c.acceptDisplayChanges) for(auto& [id,k]:capas) k.tipo=id==videoElegida ? Composition::DEVICE : Composition::CLIENT;
            if(c.presentDisplay) {
                PresentFence pf; pf.display=0;
                int e=presentar(l,pf.fence);
                if(e) writer.setError(i,e);
                else if(pf.fence.get()>=0) fences.emplace_back(std::move(pf));   // sin fence: no mandar -1 (log por cuadro)
            }
        }
        *out=writer.getPendingCommandResults();
        for(auto& f:fences) out->push_back(std::move(f));
        return ok();
    }
    ::ndk::ScopedAStatus createVirtualDisplay(int32_t in_width, int32_t in_height, ::aidl::android::hardware::graphics::common::PixelFormat in_formatHint, int32_t in_outputBufferSlotCount, ::aidl::android::hardware::graphics::composer3::VirtualDisplay* _aidl_return) override {
        return error();
    }
    ::ndk::ScopedAStatus destroyVirtualDisplay(int64_t in_display) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getActiveConfig(int64_t in_display, int32_t* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = 0; return ok();
    }
    ::ndk::ScopedAStatus getColorModes(int64_t in_display, std::vector<::aidl::android::hardware::graphics::composer3::ColorMode>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = {ColorMode::NATIVE}; return ok();
    }
    ::ndk::ScopedAStatus getDataspaceSaturationMatrix(::aidl::android::hardware::graphics::common::Dataspace in_dataspace, std::vector<float>* _aidl_return) override {
        *_aidl_return = {1,0,0,0,1,0,0,0,1}; return ok();
    }
    ::ndk::ScopedAStatus getDisplayAttribute(int64_t in_display, int32_t in_config, ::aidl::android::hardware::graphics::composer3::DisplayAttribute in_attribute, int32_t* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        if (in_config != 0) return error(IComposerClient::EX_BAD_CONFIG);
        switch (in_attribute) {
            case DisplayAttribute::WIDTH: *_aidl_return = w; break;
            case DisplayAttribute::HEIGHT: *_aidl_return = h; break;
            case DisplayAttribute::VSYNC_PERIOD: *_aidl_return = period; break;
            case DisplayAttribute::DPI_X: case DisplayAttribute::DPI_Y: *_aidl_return = 267000; break;
            case DisplayAttribute::CONFIG_GROUP: *_aidl_return = 0; break;
            default: return error(IComposerClient::EX_BAD_PARAMETER);
        }
        return ok();
    }
    ::ndk::ScopedAStatus getDisplayCapabilities(int64_t in_display, std::vector<::aidl::android::hardware::graphics::composer3::DisplayCapability>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        _aidl_return->clear(); return ok();
    }
    ::ndk::ScopedAStatus getDisplayConfigs(int64_t in_display, std::vector<int32_t>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = {0}; return ok();
    }
    ::ndk::ScopedAStatus getDisplayConnectionType(int64_t in_display, ::aidl::android::hardware::graphics::composer3::DisplayConnectionType* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = DisplayConnectionType::INTERNAL; return ok();
    }
    ::ndk::ScopedAStatus getDisplayIdentificationData(int64_t in_display, ::aidl::android::hardware::graphics::composer3::DisplayIdentification* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getDisplayName(int64_t in_display, std::string* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = "SurfaceDual"; return ok();
    }
    ::ndk::ScopedAStatus getDisplayVsyncPeriod(int64_t in_display, int32_t* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = period; return ok();
    }
    ::ndk::ScopedAStatus getDisplayedContentSample(int64_t in_display, int64_t in_maxFrames, int64_t in_timestamp, ::aidl::android::hardware::graphics::composer3::DisplayContentSample* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getDisplayedContentSamplingAttributes(int64_t in_display, ::aidl::android::hardware::graphics::composer3::DisplayContentSamplingAttributes* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getDisplayPhysicalOrientation(int64_t in_display, ::aidl::android::hardware::graphics::common::Transform* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = ::aidl::android::hardware::graphics::common::Transform::NONE; return ok();
    }
    ::ndk::ScopedAStatus getHdrCapabilities(int64_t in_display, ::aidl::android::hardware::graphics::composer3::HdrCapabilities* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = {}; return ok();
    }
    ::ndk::ScopedAStatus getMaxVirtualDisplayCount(int32_t* _aidl_return) override {
        *_aidl_return = 0; return ok();
    }
    ::ndk::ScopedAStatus getPerFrameMetadataKeys(int64_t in_display, std::vector<::aidl::android::hardware::graphics::composer3::PerFrameMetadataKey>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        _aidl_return->clear(); return ok();
    }
    ::ndk::ScopedAStatus getReadbackBufferAttributes(int64_t in_display, ::aidl::android::hardware::graphics::composer3::ReadbackBufferAttributes* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getReadbackBufferFence(int64_t in_display, ::ndk::ScopedFileDescriptor* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getRenderIntents(int64_t in_display, ::aidl::android::hardware::graphics::composer3::ColorMode in_mode, std::vector<::aidl::android::hardware::graphics::composer3::RenderIntent>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        if (in_mode != ColorMode::NATIVE) return error(); *_aidl_return = {RenderIntent::COLORIMETRIC}; return ok();
    }
    ::ndk::ScopedAStatus getSupportedContentTypes(int64_t in_display, std::vector<::aidl::android::hardware::graphics::composer3::ContentType>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        _aidl_return->clear(); return ok();
    }
    ::ndk::ScopedAStatus getDisplayDecorationSupport(int64_t in_display, std::optional<::aidl::android::hardware::graphics::common::DisplayDecorationSupport>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        _aidl_return->reset(); return ok();
    }
    ::ndk::ScopedAStatus setActiveConfig(int64_t in_display, int32_t in_config) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return in_config == 0 ? ok() : error(IComposerClient::EX_BAD_CONFIG);
    }
    ::ndk::ScopedAStatus setActiveConfigWithConstraints(int64_t in_display, int32_t in_config, const ::aidl::android::hardware::graphics::composer3::VsyncPeriodChangeConstraints& in_vsyncPeriodChangeConstraints, ::aidl::android::hardware::graphics::composer3::VsyncPeriodChangeTimeline* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        if (in_config != 0) return error(IComposerClient::EX_BAD_CONFIG); *_aidl_return = {}; _aidl_return->newVsyncAppliedTimeNanos = in_vsyncPeriodChangeConstraints.desiredTimeNanos; return ok();
    }
    ::ndk::ScopedAStatus setBootDisplayConfig(int64_t in_display, int32_t in_config) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus clearBootDisplayConfig(int64_t in_display) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getPreferredBootDisplayConfig(int64_t in_display, int32_t* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = 0; return ok();
    }
    ::ndk::ScopedAStatus setAutoLowLatencyMode(int64_t in_display, bool in_on) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus setColorMode(int64_t in_display, ::aidl::android::hardware::graphics::composer3::ColorMode in_mode, ::aidl::android::hardware::graphics::composer3::RenderIntent in_intent) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return in_mode == ColorMode::NATIVE && in_intent == RenderIntent::COLORIMETRIC ? ok() : error();
    }
    ::ndk::ScopedAStatus setContentType(int64_t in_display, ::aidl::android::hardware::graphics::composer3::ContentType in_type) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return in_type == ContentType::NONE ? ok() : error();
    }
    ::ndk::ScopedAStatus setDisplayedContentSamplingEnabled(int64_t in_display, bool in_enable, ::aidl::android::hardware::graphics::composer3::FormatColorComponent in_componentMask, int64_t in_maxFrames) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus setPowerMode(int64_t in_display, ::aidl::android::hardware::graphics::composer3::PowerMode in_mode) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        // DOZE sigue mostrando (vsync encendido); DOZE_SUSPEND y OFF lo apagan.
        bool on=in_mode==PowerMode::ON || in_mode==PowerMode::DOZE || in_mode==PowerMode::ON_SUSPEND;
        if (!on) { uint64_t hasta; { std::lock_guard l(mx); hasta=siguienteTrabajoId; } esperarTrabajo(hasta); }   // sin present siguiente que lo espere
        {std::lock_guard l(vmx); encendida=on;} pantallaEncendida=on; vcv.notify_all(); return ok();
    }
    ::ndk::ScopedAStatus setReadbackBuffer(int64_t in_display, const ::aidl::android::hardware::common::NativeHandle& in_buffer, const ::ndk::ScopedFileDescriptor& in_releaseFence) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus setIdleTimerEnabled(int64_t in_display, int32_t in_timeoutMs) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getOverlaySupport(::aidl::android::hardware::graphics::composer3::OverlayProperties* _aidl_return) override {
        *_aidl_return = {}; return ok();
    }
    ::ndk::ScopedAStatus getHdrConversionCapabilities(std::vector<::aidl::android::hardware::graphics::common::HdrConversionCapability>* _aidl_return) override {
        _aidl_return->clear(); return ok();
    }
    ::ndk::ScopedAStatus setHdrConversionStrategy(const ::aidl::android::hardware::graphics::common::HdrConversionStrategy& in_conversionStrategy, ::aidl::android::hardware::graphics::common::Hdr* _aidl_return) override {
        return error();
    }
    ::ndk::ScopedAStatus setRefreshRateChangedCallbackDebugEnabled(int64_t in_display, bool in_enabled) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getDisplayConfigurations(int64_t in_display, int32_t in_maxFrameIntervalNs, std::vector<::aidl::android::hardware::graphics::composer3::DisplayConfiguration>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        DisplayConfiguration c; c.configId = 0; c.width = w; c.height = h; c.vsyncPeriod = period; DisplayConfiguration::Dpi dpi; dpi.x = 267; dpi.y = 267; c.dpi = dpi; *_aidl_return = {c}; return ok();
    }
    ::ndk::ScopedAStatus notifyExpectedPresent(int64_t in_display, const ::aidl::android::hardware::graphics::composer3::ClockMonotonicTimestamp& in_expectedPresentTime, int32_t in_frameIntervalNs) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getMaxLayerPictureProfiles(int64_t in_display, int32_t* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        *_aidl_return = 0; return ok();
    }
    ::ndk::ScopedAStatus startHdcpNegotiation(int64_t in_display, const ::aidl::android::hardware::drm::HdcpLevels& in_levels) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getLuts(int64_t in_display, const std::vector<::aidl::android::hardware::graphics::composer3::Buffer>& in_buffers, std::vector<::aidl::android::hardware::graphics::composer3::Luts>* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }
    ::ndk::ScopedAStatus getDisplayKnownVsyncSample(int64_t in_display, ::aidl::android::hardware::graphics::composer3::VsyncSample* _aidl_return) override {
        if (in_display != 0) return error(IComposerClient::EX_BAD_DISPLAY);
        return error();
    }

};

class Composer final : public BnComposer {
    std::mutex mx;
    std::weak_ptr<IComposerClient> cliente;
public:
    Status createClient(std::shared_ptr<IComposerClient>* out) override {
        std::lock_guard l(mx); if(!cliente.expired()) return error(IComposerClient::EX_NO_RESOURCES);
        *out=ndk::SharedRefBase::make<Cliente>(); cliente=*out; return ok();
    }
    Status getCapabilities(std::vector<Capability>* out) override {out->clear(); return ok();}
};
int main() {
    Rect prueba=unir({0,0,2,2},{1,1,3,3});
    assert(prueba.x==0 && prueba.y==0 && prueba.w==4 && prueba.h==4);
    prueba=unir({}, {5,7,2,3}); assert(prueba.x==5 && prueba.y==7 && prueba.w==2 && prueba.h==3);
    prueba=unir({1,2,3,4},{8,9,1,1}); assert(prueba.x==1 && prueba.y==2 && prueba.w==8 && prueba.h==8);
    ABinderProcess_setThreadPoolMaxThreadCount(4);
    auto servicio=ndk::SharedRefBase::make<Composer>();
    if(AServiceManager_addService(servicio->asBinder().get(),"android.hardware.graphics.composer3.IComposer/default")!=STATUS_OK) {
        HLOG("no se pudo registrar IComposer/default"); return 1;
    }
    __android_log_print(ANDROID_LOG_INFO,"hwc.hyperv","composer3 v%d",IComposer::version);
    void iniciarEntrada(); iniciarEntrada();   // toque del Visor por vsock 5002 (entrada.cpp)
    ABinderProcess_joinThreadPool(); return 0;
}
