// mapper.ranchu.so de SurfaceDual: IMapper stable-C v5 (AIMapper_loadIMapper) para los búferes de allocator.cpp.
// Se carga en cada proceso que usa gráficos (SurfaceFlinger, apps): no toca D3D12 salvo en lock() de búferes D3D12.
#include "comun.h"
#include "yuv_dxil.h"
#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <errno.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <sys/resource.h>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <poll.h>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

using namespace android::hardware::graphics::mapper;
using aidl::android::hardware::graphics::common::ExtendableType;

namespace {

struct Registro {
    sdg::Meta meta = {};           // lo que cambia set*Metadata (por proceso)
    void *cpu = nullptr;           // memfd mapeado
    ID3D12Resource *res = nullptr; // textura abierta en este proceso (solo para lock)
    std::vector<uint8_t> copia;    // vista lineal de la textura mientras está bloqueada
    uint64_t usoLock = 0;
    bool sucio = false;            // escrito por CPU y aún sin subir a la textura
    int locks = 0;                 // locks de lectura simultáneos permitidos: la copia vive hasta el último unlock
};
std::mutex mx;
std::unordered_map<const native_handle_t *, Registro> registros;

Registro *buscar(buffer_handle_t b) {
    auto it = registros.find(b);
    return it == registros.end() ? nullptr : &it->second;
}
int fdRec(buffer_handle_t b) { return b->data[sdg::F_RECURSO]; }
int fdCpu(buffer_handle_t b) { return b->data[b->numFds - 1]; }   // memfd (T_MEMFD) o memfd YUV (T_YUV_GPU)
int tipo(buffer_handle_t b) { return sdg::entero(b, sdg::I_TIPO); }
uint64_t tam(buffer_handle_t b) { return sdg::largo(b, sdg::I_TAM_LO); }
int formatoReal(buffer_handle_t b) { return sdg::resolverFormato(sdg::entero(b, sdg::I_FORMATO), sdg::largo(b, sdg::I_USO_LO)); }
int bytesFila(buffer_handle_t b) {
    auto *f = sdg::formato(formatoReal(b));
    return sdg::entero(b, sdg::I_STRIDE) * (f ? f->bpp : 1);
}

// El contrato de lock exige esperar la fence de adquisición: sin límite de tiempo, reintentando si llega una señal.
bool esperar(int fence) {
    if (fence < 0) return true;
    pollfd p = {fence, POLLIN, 0};
    int r;
    do r = poll(&p, 1, -1); while (r < 0 && errno == EINTR);
    close(fence);
    return r == 1 && !(p.revents & (POLLERR | POLLNVAL));
}

// Lock de CPU de una textura D3D12 (montón DEFAULT; el CUSTOM compartido lo rechaza el driver): copia por GPU a un búfer
// READBACK al leer y desde uno UPLOAD al escribir, con una cola propia del proceso (perezosa). Cola COMPUTE: copia y
// también lanza yuv.hlsl.
struct Gpu { ID3D12CommandQueue *q = nullptr; ID3D12CommandAllocator *ca = nullptr; ID3D12GraphicsCommandList *cl = nullptr;
             ID3D12Fence *f = nullptr; UINT64 v = 0; };
Gpu *gpu() {
    static Gpu *g = [] () -> Gpu * {
        ID3D12Device *d = sdg::dispositivo();
        auto *x = new Gpu;
        D3D12_COMMAND_QUEUE_DESC qd = {}; qd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        if (!d || FAILED(d->CreateCommandQueue(&qd, IID_PPV_ARGS(&x->q))) ||
            FAILED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&x->ca))) ||
            FAILED(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, x->ca, nullptr, IID_PPV_ARGS(&x->cl))) ||
            FAILED(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&x->f)))) { LOG_E("sin cola de GPU"); return nullptr; }
        x->cl->Close();
        return x;
    }();
    return g;
}
ID3D12Resource *bufer(D3D12_HEAP_TYPE t, UINT64 n) {
    D3D12_HEAP_PROPERTIES hp = {t};
    D3D12_RESOURCE_DESC rd = {}; rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = n; rd.Height = 1; rd.DepthOrArraySize = 1;
    rd.MipLevels = 1; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *r = nullptr;
    sdg::dispositivo()->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        t == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&r));
    return r;
}
// UPLOAD reutilizado (las subidas son síncronas y van bajo mx): crearlo en cada cuadro de video cuesta. Con referencia.
// La 1.ª escritura de cada página nueva cuesta ~21 µs (fallo de página del host: 22 MB = ~100 ms, medido por el subagente
// de rendimiento). Al importar un búfer que la CPU escribe, un hilo de baja prioridad lo deja creado y tocado (calentar).
// Uno por proceso, del mayor búfer escribible (tope 40 MB al calentar); 10 s sin subidas se libera (vigía), usado o no.
// shortcut: no escucha la presión de memoria (onTrimMemory es de Java); lo acota el vigía a los periodos con subidas.
std::mutex mxU;
std::condition_variable cvU;
ID3D12Resource *u = nullptr;
UINT64 tamU = 0, tamCalentando = 0;
bool vigia = false;
std::chrono::steady_clock::time_point usoU;
void vigilar() {   // bajo mxU, con u puesto
    usoU = std::chrono::steady_clock::now();
    if (vigia) return;
    vigia = true;
    std::thread([] {
        std::unique_lock<std::mutex> l(mxU);
        for (;;) {
            auto fin = usoU + std::chrono::seconds(10);
            if (std::chrono::steady_clock::now() >= fin) break;
            cvU.wait_until(l, fin);
        }
        if (u && !tamCalentando) { u->Release(); u = nullptr; tamU = 0; }
        vigia = false;
    }).detach();
}
ID3D12Resource *subida(UINT64 n) {
    std::unique_lock<std::mutex> l(mxU);
    cvU.wait(l, [n] { return tamU >= n || tamCalentando < n; });   // si ya se calienta uno que basta, esperarlo
    if (tamU < n) {
        l.unlock();
        ID3D12Resource *r = bufer(D3D12_HEAP_TYPE_UPLOAD, n);
        l.lock();
        if (r && tamU < n) { if (u) u->Release(); u = r; tamU = n; } else if (r) r->Release();
    }
    if (u) { u->AddRef(); vigilar(); }
    return u;
}
void calentarSubida(UINT64 n) {
    static const bool sf = !strcmp(getprogname(), "surfaceflinger");   // importa los búferes de todos, no los escribe
    n = std::min<UINT64>(n, 40u << 20);
    {
        std::lock_guard<std::mutex> l(mxU);
        if (sf || tamU >= n || tamCalentando) return;   // uno a la vez: tamaños crecientes no acumulan hilos ni búferes
        tamCalentando = n;
    }
    std::thread([n] {
        setpriority(PRIO_PROCESS, 0, 10);   // Linux: 0 = este hilo
        ID3D12Resource *r = sdg::dispositivo() && gpu() ? bufer(D3D12_HEAP_TYPE_UPLOAD, n) : nullptr;   // gpu(): cola y lista también
        volatile uint8_t *p = nullptr;
        if (r && SUCCEEDED(r->Map(0, nullptr, (void **)&p))) { for (UINT64 i = 0; i < n; i += 4096) p[i] = 0; r->Unmap(0, nullptr); }
        {
            std::lock_guard<std::mutex> l(mxU);
            if (r && tamU < n) { if (u) u->Release(); u = r; tamU = n; r = nullptr; vigilar(); }
            if (tamCalentando == n) tamCalentando = 0;
        }
        if (r) r->Release();
        cvU.notify_all();
    }).detach();
}
// Grabar en serie con Mesa del mismo proceso (mismo dispositivo D3D12: el UMD de Qualcomm no aguanta dos hilos en su
// librería de blit; RutaA/fase1b/parches/d3d12_umd_serie.patch, interruptor debug.mesa.sd.umd.serie). Sin Mesa cargado
// (códecs) no hace nada. Al encontrarla se queda abierta (no se descarga) y los punteros se guardan: sin dlopen por copia.
struct EnSerieUmd {
    using Fn = void (*)();
    Fn soltar = nullptr;
    EnSerieUmd() {
        static std::atomic<Fn> tomarG{nullptr}, soltarG{nullptr};
        if (!soltarG) {
            void *m = dlopen("libgallium_dri.so", RTLD_NOW | RTLD_NOLOAD);   // referencia que nunca se suelta
            Fn t = m ? (Fn)dlsym(m, "d3d12_sd_umd_lock") : nullptr, u = m ? (Fn)dlsym(m, "d3d12_sd_umd_unlock") : nullptr;
            if (t && u) { tomarG = t; soltarG = u; } else if (m) dlclose(m);
        }
        if ((soltar = soltarG.load())) tomarG.load()();
    }
    ~EnSerieUmd() { if (soltar) soltar(); }
};
// Cierra la lista, la ejecuta y espera a la GPU.
bool ejecutar(Gpu *g) {
    if (FAILED(g->cl->Close())) return false;
    ID3D12CommandList *l[] = {g->cl};
    g->q->ExecuteCommandLists(1, l);
    return SUCCEEDED(g->q->Signal(g->f, ++g->v)) && SUCCEEDED(g->f->SetEventOnCompletion(g->v, nullptr)) &&   // nullptr: espera
           g->f->GetCompletedValue() != UINT64_MAX;
}
bool abrirTextura(buffer_handle_t b, Registro *r) {
    if (r->res) return true;
    ID3D12Device *d = sdg::dispositivo();
    return d && gpu() && SUCCEEDED(d->OpenSharedHandle((HANDLE)(intptr_t)fdRec(b), IID_PPV_ARGS(&r->res)));
}
UINT alto(buffer_handle_t b) { return (UINT)sdg::entero(b, sdg::I_ALTO); }
// haciaTextura=false: textura -> copia; true: copia -> textura.
bool copiarGpu(buffer_handle_t b, Registro *r, bool haciaTextura) {
    Gpu *g = gpu(); ID3D12Device *d = sdg::dispositivo();
    D3D12_RESOURCE_DESC desc = r->res->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT filas; UINT64 bytesUtiles, total;
    d->GetCopyableFootprints(&desc, 0, 1, 0, &fp, &filas, &bytesUtiles, &total);
    filas = std::min<UINT>(filas, alto(b));   // copia mide bytesFila * alto del handle (lo trae la app): no pasar de ahí
    ID3D12Resource *in = haciaTextura ? subida(total) : bufer(D3D12_HEAP_TYPE_READBACK, total);
    if (!in) return false;
    uint8_t *p = nullptr; size_t pitch = (size_t)bytesFila(b), n = std::min<size_t>(pitch, (size_t)bytesUtiles);
    if (haciaTextura) {
        if (FAILED(in->Map(0, nullptr, (void **)&p))) { in->Release(); return false; }
        for (UINT y = 0; y < filas; y++) memcpy(p + fp.Offset + (size_t)y * fp.Footprint.RowPitch, r->copia.data() + y * pitch, n);
        in->Unmap(0, nullptr);
    }
    D3D12_TEXTURE_COPY_LOCATION tex = {r->res, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX}, lin = {in, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
    tex.SubresourceIndex = 0; lin.PlacedFootprint = fp;
    D3D12_RESOURCE_BARRIER br = {}; br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = r->res;
    br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    br.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    br.Transition.StateAfter = haciaTextura ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COPY_SOURCE;
    {
        EnSerieUmd serie;
        g->ca->Reset(); g->cl->Reset(g->ca, nullptr);
        g->cl->ResourceBarrier(1, &br);
        if (haciaTextura) g->cl->CopyTextureRegion(&tex, 0, 0, 0, &lin, nullptr); else g->cl->CopyTextureRegion(&lin, 0, 0, 0, &tex, nullptr);
        std::swap(br.Transition.StateBefore, br.Transition.StateAfter);   // vuelve a COMMON para el resto de procesos
        g->cl->ResourceBarrier(1, &br);
    }
    bool ok = ejecutar(g);
    if (ok && !haciaTextura) {
        ok = SUCCEEDED(in->Map(0, nullptr, (void **)&p));
        if (ok) {
            for (UINT y = 0; y < filas; y++) memcpy(r->copia.data() + y * pitch, p + fp.Offset + (size_t)y * fp.Footprint.RowPitch, n);
            D3D12_RANGE nada = {0, 0}; in->Unmap(0, &nada);
        }
    }
    in->Release();
    return ok;
}
// YUV -> textura RGBA en la GPU (yuv.hlsl): la CPU solo copia el memfd YUV (1,5 B/píxel) al UPLOAD que lee el shader.
// Matriz: BT.709 en HD, BT.601 en SD (rango limitado). shortcut: el dataspace lo fija el cliente en otro proceso (los
// metadatos aquí son por proceso) y no se ve; si llega video BT.2020 o de rango completo, pasarlo por el handle.
bool subirYuv(buffer_handle_t b, Registro *r) {
    Gpu *g = gpu(); ID3D12Device *d = sdg::dispositivo();
    static ID3D12RootSignature *raiz = nullptr;
    static ID3D12PipelineState *pso = nullptr;
    static ID3D12DescriptorHeap *uav = nullptr;
    if (!pso) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {}; pd.CS = {yuv_dxil, sizeof yuv_dxil};
        D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
        if (!raiz && FAILED(d->CreateRootSignature(0, yuv_root_signature, sizeof yuv_root_signature, IID_PPV_ARGS(&raiz)))) return false;
        if (!uav && FAILED(d->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&uav)))) return false;
        pd.pRootSignature = raiz;
        EnSerieUmd serie;
        if (FAILED(d->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso)))) return false;
    }
    sdg::Yuv l;
    int w = sdg::entero(b, sdg::I_ANCHO), h = (int)alto(b);
    if (!sdg::yuv(formatoReal(b), sdg::entero(b, sdg::I_STRIDE), h, &l) || !r->cpu || tam(b) < l.tam) return false;
    ID3D12Resource *in = subida(l.tam);
    uint8_t *p = nullptr;
    if (!in) return false;
    if (FAILED(in->Map(0, nullptr, (void **)&p))) { in->Release(); return false; }
    memcpy(p, r->cpu, l.tam);
    in->Unmap(0, nullptr);
    D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {}; ud.Format = DXGI_FORMAT_R8G8B8A8_UNORM; ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    d->CreateUnorderedAccessView(r->res, nullptr, &ud, uav->GetCPUDescriptorHandleForHeapStart());
    static const float k709[] = {1.793f, 0.213f, 0.533f, 2.112f}, k601[] = {1.596f, 0.391f, 0.813f, 2.018f};
    uint32_t c[12] = {(uint32_t)w, (uint32_t)h, (uint32_t)l.y, (uint32_t)l.cb, (uint32_t)l.cr, (uint32_t)l.sy, (uint32_t)l.sc, (uint32_t)l.paso};
    memcpy(c + 8, h >= 720 ? k709 : k601, sizeof k709);
    D3D12_RESOURCE_BARRIER br = {}; br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = r->res;
    br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    br.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; br.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    {
        EnSerieUmd serie;
        g->ca->Reset(); g->cl->Reset(g->ca, pso);
        g->cl->ResourceBarrier(1, &br);
        g->cl->SetDescriptorHeaps(1, &uav);
        g->cl->SetComputeRootSignature(raiz);
        g->cl->SetComputeRootShaderResourceView(0, in->GetGPUVirtualAddress());
        g->cl->SetComputeRootDescriptorTable(1, uav->GetGPUDescriptorHandleForHeapStart());
        g->cl->SetComputeRoot32BitConstants(2, 12, c, 0);
        g->cl->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
        std::swap(br.Transition.StateBefore, br.Transition.StateAfter);   // vuelve a COMMON para el resto de procesos
        g->cl->ResourceBarrier(1, &br);
    }
    bool ok = ejecutar(g);
    in->Release();
    return ok;
}
bool leer(buffer_handle_t b, Registro *r) { bool ok = copiarGpu(b, r, false); if (!ok) LOG_E("lock: lectura por GPU falló"); return ok; }
bool escribir(buffer_handle_t b, Registro *r) {
    bool ok = tipo(b) == sdg::T_YUV_GPU ? subirYuv(b, r) : copiarGpu(b, r, true);
    if (!ok) LOG_E("unlock: escritura por GPU falló");
    return ok;
}

// La textura del fd debe ser la que dice el handle: una app puede mandar el fd D3D12 de otro búfer suyo con otros ints;
// Mesa lo importaría mal y SurfaceFlinger abortaría (reinicio de Android; medido con PruebaImplDef). Solo en SurfaceFlinger:
// en cada app costaría un dispositivo D3D12 más y el daño de un búfer falso se queda en esa app.
// shortcut: otros procesos que componen búferes ajenos (SystemUI) no lo comprueban; ampliarlo si alguno se cae así.
bool texturaCasa(const native_handle_t *h) {
    static const bool sf = !strcmp(getprogname(), "surfaceflinger");
    int t = sdg::entero(h, sdg::I_TIPO);
    if (!sf || (t != sdg::T_D3D12_GPU && t != sdg::T_YUV_GPU)) return true;
    ID3D12Device *d = sdg::dispositivo();
    ID3D12Resource *r = nullptr;
    if (!d || FAILED(d->OpenSharedHandle((HANDLE)(intptr_t)h->data[sdg::F_RECURSO], IID_PPV_ARGS(&r)))) return false;
    D3D12_RESOURCE_DESC x = r->GetDesc();
    r->Release();
    const sdg::Formato *f = sdg::formato(sdg::resolverFormato(sdg::entero(h, sdg::I_FORMATO), sdg::largo(h, sdg::I_USO_LO)));
    return f && x.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && x.Format == f->dxgi && x.DepthOrArraySize == 1 &&
           x.MipLevels == 1 && x.Width == (UINT64)sdg::entero(h, sdg::I_ANCHO) && x.Height == (UINT)sdg::entero(h, sdg::I_ALTO);
}

AIMapper_Error importBuffer(const native_handle_t *h, buffer_handle_t *out) {
    if (!sdg::valido(h) || !texturaCasa(h)) return AIMAPPER_ERROR_BAD_BUFFER;
    if (sdg::Yuv l; tipo(h) == sdg::T_YUV_GPU && sdg::yuv(formatoReal(h), sdg::entero(h, sdg::I_STRIDE), (int)alto(h), &l))
        calentarSubida(l.tam);
    else if (tipo(h) == sdg::T_D3D12_GPU && (sdg::largo(h, sdg::I_USO_LO) & sdg::USO_CPU_ESCRIBIR))
        calentarSubida((UINT64)sdg::alinear(bytesFila(h), 256) * alto(h));   // = huella de GetCopyableFootprints
    native_handle_t *c = native_handle_clone(h);
    if (!c) return AIMAPPER_ERROR_NO_RESOURCES;
    std::lock_guard<std::mutex> l(mx);
    registros[c];
    *out = c;
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error freeBuffer(buffer_handle_t b) {
    std::lock_guard<std::mutex> l(mx);
    Registro *r = buscar(b);
    if (!r) return AIMAPPER_ERROR_BAD_BUFFER;
    if (r->cpu) munmap(r->cpu, tam(b));
    if (r->res) r->res->Release();
    registros.erase(b);
    native_handle_close(b); native_handle_delete(const_cast<native_handle_t *>(b));
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error getTransportSize(buffer_handle_t b, uint32_t *nfds, uint32_t *nints) {
    if (!sdg::valido(b)) return AIMAPPER_ERROR_BAD_BUFFER;
    *nfds = b->numFds; *nints = sdg::N_INTS;
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error lock(buffer_handle_t b, uint64_t uso, ARect reg, int fence, void **out) {
    if (!esperar(fence)) return AIMAPPER_ERROR_NO_RESOURCES;
    std::lock_guard<std::mutex> l(mx);
    Registro *r = buscar(b);
    if (!r) return AIMAPPER_ERROR_BAD_BUFFER;
    if (!(uso & (sdg::USO_CPU_LEER | sdg::USO_CPU_ESCRIBIR))) return AIMAPPER_ERROR_BAD_VALUE;
    if (reg.left < 0 || reg.top < 0 || reg.left > reg.right || reg.top > reg.bottom ||
        reg.right > sdg::entero(b, sdg::I_ANCHO) || reg.bottom > (int)alto(b)) return AIMAPPER_ERROR_BAD_VALUE;
    if (tipo(b) == sdg::T_MEMFD || tipo(b) == sdg::T_YUV_GPU) {
        if (!r->cpu) {
            // El tamaño del handle no basta (lo trae la app): un fd más corto daría SIGBUS al tocar el final.
            struct stat st;
            if (fstat(fdCpu(b), &st) != 0 || (uint64_t)st.st_size < tam(b)) return AIMAPPER_ERROR_BAD_BUFFER;
            void *p = mmap(nullptr, tam(b), PROT_READ | PROT_WRITE, MAP_SHARED, fdCpu(b), 0);
            if (p == MAP_FAILED) return AIMAPPER_ERROR_NO_RESOURCES;
            r->cpu = p;
        }
        // YUV: el memfd es el original; al escribir, el unlock lo convierte a la textura (hay que abrirla aquí).
        if (tipo(b) == sdg::T_YUV_GPU && (uso & sdg::USO_CPU_ESCRIBIR) && !abrirTextura(b, r)) return AIMAPPER_ERROR_NO_RESOURCES;
        *out = r->cpu;
    } else {   // textura D3D12 (T_D3D12_GPU o T_D3D12_CPU)
        if (!abrirTextura(b, r)) return AIMAPPER_ERROR_NO_RESOURCES;
        if (r->locks == 0) {
            r->copia.resize((size_t)bytesFila(b) * alto(b));
            // También al escribir: quien bloquea una región conserva el resto. Si falla, una copia en ceros lo borraría al subir.
            // Solo escritura de todo el búfer: no hay nada que conservar y la lectura por GPU (READBACK nuevo, ~20 ms a 22 MB
            // por sus páginas nuevas) sobra; el contenido no escrito queda indefinido, como permite CPU_WRITE sin CPU_READ.
            bool todo = reg.left == 0 && reg.top == 0 && reg.right == sdg::entero(b, sdg::I_ANCHO) && reg.bottom == (int)alto(b);
            if (((uso & sdg::USO_CPU_LEER) || !todo) && !leer(b, r)) { r->copia.clear(); r->copia.shrink_to_fit(); return AIMAPPER_ERROR_NO_RESOURCES; }
        }
        *out = r->copia.data();
    }
    r->usoLock |= uso; r->locks++;
    if (uso & sdg::USO_CPU_ESCRIBIR) r->sucio = true;
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error unlock(buffer_handle_t b, int *fenceSalida) {
    std::lock_guard<std::mutex> l(mx);
    Registro *r = buscar(b);
    if (!r) return AIMAPPER_ERROR_BAD_BUFFER;
    if (r->locks == 0) return AIMAPPER_ERROR_BAD_BUFFER;   // unlock sin lock
    bool ok = true;
    if (--r->locks == 0) {
        if (tipo(b) != sdg::T_MEMFD && r->res && r->sucio) ok = escribir(b, r);
        r->sucio = false;
        if (tipo(b) != sdg::T_MEMFD) { r->copia.clear(); r->copia.shrink_to_fit(); }   // YUV: no hay copia
        r->usoLock = 0;
    }
    *fenceSalida = -1;
    return ok ? AIMAPPER_ERROR_NONE : AIMAPPER_ERROR_BAD_BUFFER;   // el contrato de unlock solo admite NONE o BAD_BUFFER
}

AIMapper_Error flushLockedBuffer(buffer_handle_t b) {
    std::lock_guard<std::mutex> l(mx);
    Registro *r = buscar(b);
    if (!r || r->locks == 0) return AIMAPPER_ERROR_BAD_BUFFER;   // el contrato exige un búfer bloqueado
    // shortcut: lo escrito entre este flush y el unlock no se sube (no se detecta sin comparar); si un productor lo hace,
    // volver a subir siempre en unlock.
    if (tipo(b) != sdg::T_MEMFD && r->res && r->sucio) {
        if (!escribir(b, r)) return AIMAPPER_ERROR_BAD_BUFFER;   // contrato: NONE o BAD_BUFFER
        r->sucio = false;
    }
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error rereadLockedBuffer(buffer_handle_t b) {
    std::lock_guard<std::mutex> l(mx);
    Registro *r = buscar(b);
    if (!r || r->locks == 0) return AIMAPPER_ERROR_BAD_BUFFER;
    if (tipo(b) != sdg::T_MEMFD && r->res && !r->copia.empty() && !leer(b, r)) return AIMAPPER_ERROR_NO_RESOURCES;
    return AIMAPPER_ERROR_NONE;
}

// ---- Metadatos estándar (codificación gralloc4 de IMapperMetadataTypes.h) ----
using SMT = aidl::android::hardware::graphics::common::StandardMetadataType;
template <SMT T> using SM = StandardMetadata<T>;

ExtendableType ext(const char *nombre, int64_t v) { return ExtendableType{nombre, v}; }

std::vector<aidl::android::hardware::graphics::common::PlaneLayout> planos(buffer_handle_t b) {
    using namespace aidl::android::hardware::graphics::common;
    const sdg::Formato *f = sdg::formato(formatoReal(b));
    PlaneLayout p;
    int bits = f ? f->bpp * 8 : 8;
    auto comp = [&](int64_t t, int64_t off, int64_t tam) {
        p.components.push_back({ext("android.hardware.graphics.common.PlaneLayoutComponentType", t), off, tam});
    };
    constexpr int64_t R = 1 << 10, G = 1 << 11, B = 1 << 12, A = 1 << 30, Y = 1 << 0, CB = 1 << 1, CR = 1 << 2;
    if (sdg::Yuv l; sdg::yuv(formatoReal(b), sdg::entero(b, sdg::I_STRIDE), (int)alto(b), &l)) {   // Y + croma (1 o 2 planos)
        int w = sdg::entero(b, sdg::I_ANCHO), h = (int)alto(b);
        auto plano = [&](int64_t off, int stride, int paso, int ancho, int altoP, int sub) {
            PlaneLayout q; q.offsetInBytes = off; q.sampleIncrementInBits = paso * 8; q.strideInBytes = stride;
            q.widthInSamples = ancho; q.heightInSamples = altoP; q.totalSizeInBytes = (int64_t)stride * altoP;
            q.horizontalSubsampling = sub; q.verticalSubsampling = sub;
            return q;
        };
        std::vector<PlaneLayout> v = {plano(l.y, l.sy, 1, w, h, 1)};
        v[0].components.push_back({ext("android.hardware.graphics.common.PlaneLayoutComponentType", Y), 0, 8});
        auto croma = [&](PlaneLayout &q, int64_t t, int64_t bits) {
            q.components.push_back({ext("android.hardware.graphics.common.PlaneLayoutComponentType", t), bits, 8});
        };
        if (l.paso == 2) {   // NV12/NV21: un plano intercalado que empieza en el primer byte de croma
            int64_t ini = std::min(l.cb, l.cr);
            v.push_back(plano(ini, l.sc, 2, (w + 1) / 2, (h + 1) / 2, 2));
            croma(v[1], CB, (l.cb - ini) * 8); croma(v[1], CR, (l.cr - ini) * 8);
        } else {   // YV12 / I420: planos de Cb y Cr (cada uno en su desplazamiento)
            v.push_back(plano(l.cb, l.sc, 1, (w + 1) / 2, (h + 1) / 2, 2)); croma(v[1], CB, 0);
            v.push_back(plano(l.cr, l.sc, 1, (w + 1) / 2, (h + 1) / 2, 2)); croma(v[2], CR, 0);
        }
        return v;
    }
    if (f && f->android == 4) { comp(B, 0, 5); comp(G, 5, 6); comp(R, 11, 5); }   // RGB_565
    else if (f && f->android == 0x2b) { comp(R, 0, 10); comp(G, 10, 10); comp(B, 20, 10); comp(A, 30, 2); }
    else if (f && f->android != 0x21) {
        int c = (int)strlen(f->componentes), t = bits / c;
        for (int i = 0; i < c; i++) {
            char x = f->componentes[i];
            if (x != 'X') comp(x == 'R' ? R : x == 'G' ? G : x == 'B' ? B : A, i * t, t);
        }
    }
    p.offsetInBytes = 0; p.sampleIncrementInBits = bits; p.strideInBytes = bytesFila(b);
    p.widthInSamples = sdg::entero(b, sdg::I_ANCHO); p.heightInSamples = alto(b);
    p.totalSizeInBytes = (int64_t)bytesFila(b) * alto(b); p.horizontalSubsampling = 1; p.verticalSubsampling = 1;
    return {p};
}

int32_t getStandardMetadata(buffer_handle_t b, int64_t tipoM, void *dst, size_t n) {
    using namespace aidl::android::hardware::graphics::common;
    std::lock_guard<std::mutex> l(mx);
    Registro *r = buscar(b);
    if (!r) return -AIMAPPER_ERROR_BAD_BUFFER;
    const sdg::Meta &m = r->meta;
    const sdg::Formato *f = sdg::formato(formatoReal(b));
    uint64_t uso = sdg::largo(b, sdg::I_USO_LO);
    switch (static_cast<SMT>(tipoM)) {
    case SMT::BUFFER_ID: return SM<SMT::BUFFER_ID>::value::encode(sdg::largo(b, sdg::I_ID_LO), dst, n);
    case SMT::NAME: return SM<SMT::NAME>::value::encode(std::string((const char *)&b->data[b->numFds + sdg::I_NOMBRE], strnlen((const char *)&b->data[b->numFds + sdg::I_NOMBRE], 31)), dst, n);
    case SMT::WIDTH: return SM<SMT::WIDTH>::value::encode((uint64_t)sdg::entero(b, sdg::I_ANCHO), dst, n);
    case SMT::HEIGHT: return SM<SMT::HEIGHT>::value::encode((uint64_t)alto(b), dst, n);
    case SMT::LAYER_COUNT: return SM<SMT::LAYER_COUNT>::value::encode((uint64_t)sdg::entero(b, sdg::I_CAPAS), dst, n);
    case SMT::PIXEL_FORMAT_REQUESTED:
        return SM<SMT::PIXEL_FORMAT_REQUESTED>::value::encode(static_cast<PixelFormat>(sdg::entero(b, sdg::I_FORMATO)), dst, n);
    case SMT::PIXEL_FORMAT_FOURCC: return SM<SMT::PIXEL_FORMAT_FOURCC>::value::encode(f ? f->fourcc : 0u, dst, n);
    case SMT::PIXEL_FORMAT_MODIFIER: return SM<SMT::PIXEL_FORMAT_MODIFIER>::value::encode(0ull, dst, n);   // lineal (vista de lock)
    case SMT::USAGE: return SM<SMT::USAGE>::value::encode(static_cast<BufferUsage>(uso), dst, n);
    case SMT::ALLOCATION_SIZE: return SM<SMT::ALLOCATION_SIZE>::value::encode(tam(b), dst, n);
    case SMT::PROTECTED_CONTENT: return SM<SMT::PROTECTED_CONTENT>::value::encode((uint64_t)((uso >> 14) & 1), dst, n);
    case SMT::COMPRESSION:
        return SM<SMT::COMPRESSION>::value::encode(ext("android.hardware.graphics.common.Compression", 0), dst, n);
    case SMT::INTERLACED:
        return SM<SMT::INTERLACED>::value::encode(ext("android.hardware.graphics.common.Interlaced", 0), dst, n);
    case SMT::CHROMA_SITING:
        return SM<SMT::CHROMA_SITING>::value::encode(ext("android.hardware.graphics.common.ChromaSiting", 0), dst, n);
    case SMT::PLANE_LAYOUTS: return SM<SMT::PLANE_LAYOUTS>::value::encode(planos(b), dst, n);
    case SMT::CROP: {
        Rect c = m.hayCrop ? Rect{m.crop[0], m.crop[1], m.crop[2], m.crop[3]} : Rect{0, 0, sdg::entero(b, sdg::I_ANCHO), (int)alto(b)};
        return SM<SMT::CROP>::value::encode(std::vector<Rect>{c}, dst, n);
    }
    case SMT::DATASPACE: return SM<SMT::DATASPACE>::value::encode(static_cast<Dataspace>(m.dataspace), dst, n);
    case SMT::BLEND_MODE: return SM<SMT::BLEND_MODE>::value::encode(static_cast<BlendMode>(m.blend), dst, n);
    case SMT::SMPTE2086: {
        std::optional<Smpte2086> v;
        if (m.haySmpte) v = Smpte2086{{m.smpte[0], m.smpte[1]}, {m.smpte[2], m.smpte[3]}, {m.smpte[4], m.smpte[5]},
                                      {m.smpte[6], m.smpte[7]}, m.smpte[8], m.smpte[9]};
        return SM<SMT::SMPTE2086>::value::encode(v, dst, n);
    }
    case SMT::CTA861_3: {
        std::optional<Cta861_3> v;
        if (m.hayCta) v = Cta861_3{m.cta[0], m.cta[1]};
        return SM<SMT::CTA861_3>::value::encode(v, dst, n);
    }
    case SMT::STRIDE: return SM<SMT::STRIDE>::value::encode((uint32_t)sdg::entero(b, sdg::I_STRIDE), dst, n);
    default: return -AIMAPPER_ERROR_UNSUPPORTED;
    }
}

AIMapper_Error setStandardMetadata(buffer_handle_t b, int64_t tipoM, const void *v, size_t n) {
    std::lock_guard<std::mutex> l(mx);
    Registro *r = buscar(b);
    if (!r) return AIMAPPER_ERROR_BAD_BUFFER;
    sdg::Meta &m = r->meta;
    switch (static_cast<SMT>(tipoM)) {
    case SMT::DATASPACE:
        if (auto x = SM<SMT::DATASPACE>::value::decode(v, n)) { m.dataspace = static_cast<int32_t>(*x); return AIMAPPER_ERROR_NONE; }
        return AIMAPPER_ERROR_BAD_VALUE;
    case SMT::BLEND_MODE:
        if (auto x = SM<SMT::BLEND_MODE>::value::decode(v, n)) { m.blend = static_cast<int32_t>(*x); return AIMAPPER_ERROR_NONE; }
        return AIMAPPER_ERROR_BAD_VALUE;
    case SMT::CROP:
        if (auto x = SM<SMT::CROP>::value::decode(v, n); x && x->size() == 1) {
            auto &c = (*x)[0]; m.crop[0] = c.left; m.crop[1] = c.top; m.crop[2] = c.right; m.crop[3] = c.bottom; m.hayCrop = 1;
            return AIMAPPER_ERROR_NONE;
        }
        return AIMAPPER_ERROR_BAD_VALUE;
    case SMT::SMPTE2086:
        if (auto x = SM<SMT::SMPTE2086>::value::decode(v, n)) {
            m.haySmpte = x->has_value();
            if (m.haySmpte) {
                auto &s = **x; float t[10] = {s.primaryRed.x, s.primaryRed.y, s.primaryGreen.x, s.primaryGreen.y, s.primaryBlue.x,
                                              s.primaryBlue.y, s.whitePoint.x, s.whitePoint.y, s.maxLuminance, s.minLuminance};
                memcpy(m.smpte, t, sizeof t);
            }
            return AIMAPPER_ERROR_NONE;
        }
        return AIMAPPER_ERROR_BAD_VALUE;
    case SMT::CTA861_3:
        if (auto x = SM<SMT::CTA861_3>::value::decode(v, n)) {
            m.hayCta = x->has_value();
            if (m.hayCta) { m.cta[0] = (*x)->maxContentLightLevel; m.cta[1] = (*x)->maxFrameAverageLightLevel; }
            return AIMAPPER_ERROR_NONE;
        }
        return AIMAPPER_ERROR_BAD_VALUE;
    default: return AIMAPPER_ERROR_UNSUPPORTED;
    }
}

int32_t getMetadata(buffer_handle_t b, AIMapper_MetadataType t, void *dst, size_t n) {
    if (!strcmp(t.name, "android.hardware.graphics.common.StandardMetadataType")) return getStandardMetadata(b, t.value, dst, n);
    return -AIMAPPER_ERROR_UNSUPPORTED;
}
AIMapper_Error setMetadata(buffer_handle_t b, AIMapper_MetadataType t, const void *v, size_t n) {
    if (!strcmp(t.name, "android.hardware.graphics.common.StandardMetadataType")) return setStandardMetadata(b, t.value, v, n);
    return AIMAPPER_ERROR_UNSUPPORTED;
}

AIMapper_Error listSupportedMetadataTypes(const AIMapper_MetadataTypeDescription **lista, size_t *n) {
    static const char *E = "android.hardware.graphics.common.StandardMetadataType";
    static const AIMapper_MetadataTypeDescription d[] = {
        {{E, (int64_t)SMT::BUFFER_ID}, nullptr, true, false}, {{E, (int64_t)SMT::NAME}, nullptr, true, false},
        {{E, (int64_t)SMT::WIDTH}, nullptr, true, false}, {{E, (int64_t)SMT::HEIGHT}, nullptr, true, false},
        {{E, (int64_t)SMT::LAYER_COUNT}, nullptr, true, false}, {{E, (int64_t)SMT::PIXEL_FORMAT_REQUESTED}, nullptr, true, false},
        {{E, (int64_t)SMT::PIXEL_FORMAT_FOURCC}, nullptr, true, false}, {{E, (int64_t)SMT::PIXEL_FORMAT_MODIFIER}, nullptr, true, false},
        {{E, (int64_t)SMT::USAGE}, nullptr, true, false}, {{E, (int64_t)SMT::ALLOCATION_SIZE}, nullptr, true, false},
        {{E, (int64_t)SMT::PROTECTED_CONTENT}, nullptr, true, false}, {{E, (int64_t)SMT::COMPRESSION}, nullptr, true, false},
        {{E, (int64_t)SMT::INTERLACED}, nullptr, true, false}, {{E, (int64_t)SMT::CHROMA_SITING}, nullptr, true, false},
        {{E, (int64_t)SMT::PLANE_LAYOUTS}, nullptr, true, false}, {{E, (int64_t)SMT::CROP}, nullptr, true, true},
        {{E, (int64_t)SMT::DATASPACE}, nullptr, true, true}, {{E, (int64_t)SMT::BLEND_MODE}, nullptr, true, true},
        {{E, (int64_t)SMT::SMPTE2086}, nullptr, true, true}, {{E, (int64_t)SMT::CTA861_3}, nullptr, true, true},
        {{E, (int64_t)SMT::STRIDE}, nullptr, true, false},
    };
    *lista = d; *n = sizeof d / sizeof d[0];
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error dumpBuffer(buffer_handle_t b, AIMapper_DumpBufferCallback cb, void *ctx) {
    const AIMapper_MetadataTypeDescription *d; size_t n;
    listSupportedMetadataTypes(&d, &n);
    std::vector<uint8_t> v(4096);
    for (size_t i = 0; i < n; i++) {
        int32_t t = getStandardMetadata(b, d[i].metadataType.value, v.data(), v.size());
        if (t > 0 && (size_t)t <= v.size()) cb(ctx, d[i].metadataType, v.data(), (size_t)t);
    }
    return AIMAPPER_ERROR_NONE;
}
AIMapper_Error dumpAllBuffers(AIMapper_BeginDumpBufferCallback inicio, AIMapper_DumpBufferCallback cb, void *ctx) {
    std::vector<buffer_handle_t> todos;
    { std::lock_guard<std::mutex> l(mx); for (auto &x : registros) todos.push_back(x.first); }
    for (auto b : todos) { inicio(ctx); dumpBuffer(b, cb, ctx); }
    return AIMAPPER_ERROR_NONE;
}

AIMapper_Error getReservedRegion(buffer_handle_t b, void **out, uint64_t *n) {
    if (!sdg::valido(b)) return AIMAPPER_ERROR_BAD_BUFFER;
    *out = nullptr; *n = 0;   // el allocator no reserva región (ver planear)
    return AIMAPPER_ERROR_NONE;
}

}  // namespace

extern "C" __attribute__((visibility("default"))) AIMapper_Error AIMapper_loadIMapper(AIMapper **out) {
    static AIMapper m = [] {
        AIMapper x = {};
        x.version = AIMAPPER_VERSION_5;
        x.v5 = {importBuffer, freeBuffer, getTransportSize, lock, unlock, flushLockedBuffer, rereadLockedBuffer,
                getMetadata, getStandardMetadata, setMetadata, setStandardMetadata, listSupportedMetadataTypes,
                dumpBuffer, dumpAllBuffers, getReservedRegion};
        return x;
    }();
    *out = &m;
    return AIMAPPER_ERROR_NONE;
}
