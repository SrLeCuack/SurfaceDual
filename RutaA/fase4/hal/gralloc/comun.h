// Gralloc de SurfaceDual para la VM de Hyper-V (Ruta A, fase 3 paso 3): búferes = recursos D3D12 compartidos (fd de dxg),
// que Mesa d3d12 importa con OpenSharedHandle (d3d12_resource.cpp, WINSYS_HANDLE_TYPE_FD) sin copias. Los búferes sin uso
// de GPU (BLOB, solo CPU) y el YUV de los códecs son dma-buf del montón system (T_MEMFD: el nombre es histórico). Lo comparten allocator.cpp (servicio IAllocator) y mapper.cpp (mapper.ranchu.so).
#pragma once
#include <wsl/winadapter.h>
#include <directx/d3d12.h>
#include <directx/dxcore.h>
#include <dxguids/dxguids.h>
#undef interface   // winadapter.h la define como "struct" y rompe las cabeceras de libbinder_ndk
#include <android/log.h>
#include <cutils/native_handle.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, "gralloc.hyperv", __VA_ARGS__)
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, "gralloc.hyperv", __VA_ARGS__)

namespace sdg {

// native_handle: fds = {recurso (D3D12 o dma-buf)} o, en T_YUV_GPU, {textura RGBA, dma-buf YUV}; ints = I_* (el nombre va en
// I_NOMBRE, 32 bytes). Sin página de metadatos compartida: un memfd de un HAL de vendor no lo pueden abrir SurfaceFlinger
// ni las apps (SELinux: hal_graphics_allocator_default_tmpfs). Dataspace/HDR viajan en el estado de la capa, no en el búfer.
enum { F_RECURSO, N_FDS };
enum { I_MAGIA, I_ANCHO, I_ALTO, I_FORMATO, I_CAPAS, I_USO_LO, I_USO_HI, I_STRIDE, I_TAM_LO, I_TAM_HI,
       I_ID_LO, I_ID_HI, I_TIPO, I_NOMBRE, N_INTS = I_NOMBRE + 8 };
constexpr int MAGIA = 0x53444856;   // "SDHV"
// D3D12_CPU: montón CUSTOM (lock por Read/WriteToSubresource). YUV_GPU: el códec escribe YUV en el dma-buf y el unlock lo
// convierte en la GPU (yuv.hlsl) a la textura RGBA, que es lo que Mesa importa (parche en RutaA/fase1b/compilar_mesa.sh).
enum Tipo { T_D3D12_CPU = 0, T_D3D12_GPU = 1, T_MEMFD = 2, T_YUV_GPU = 3 };

// Metadatos que se pueden cambiar con set*Metadata. shortcut: por proceso (no se propagan entre procesos ni importaciones,
// como pide IMapper); SurfaceFlinger toma dataspace/HDR del estado de la capa. Compartir con un servicio si algo lo exige.
struct Meta {
    uint32_t magia;
    int32_t dataspace, blend;
    int32_t hayCrop, crop[4];
    int32_t haySmpte; float smpte[10];
    int32_t hayCta; float cta[2];
};

inline int entero(const native_handle_t *h, int i) { return h->data[h->numFds + i]; }
inline uint64_t largo(const native_handle_t *h, int lo) {
    return (uint32_t)entero(h, lo) | ((uint64_t)(uint32_t)entero(h, lo + 1) << 32);
}

// PixelFormat de Android -> DXGI, bytes por píxel y fourcc DRM (para Mesa y PIXEL_FORMAT_FOURCC).
struct Formato { int android; DXGI_FORMAT dxgi; int bpp; uint32_t fourcc; const char *componentes; };
constexpr uint32_t fourcc(char a, char b, char c, char d) { return a | (b << 8) | (c << 16) | ((uint32_t)d << 24); }
inline const Formato *formato(int f) {
    static const Formato t[] = {
        {1, DXGI_FORMAT_R8G8B8A8_UNORM, 4, fourcc('A', 'B', '2', '4'), "RGBA"},     // RGBA_8888
        {2, DXGI_FORMAT_R8G8B8A8_UNORM, 4, fourcc('X', 'B', '2', '4'), "RGBX"},     // RGBX_8888
        {4, DXGI_FORMAT_B5G6R5_UNORM, 2, fourcc('R', 'G', '1', '6'), "BGR"},        // RGB_565 (bits altos = R)
        {5, DXGI_FORMAT_B8G8R8A8_UNORM, 4, fourcc('A', 'R', '2', '4'), "BGRA"},     // BGRA_8888
        {0x16, DXGI_FORMAT_R16G16B16A16_FLOAT, 8, fourcc('A', 'B', '4', 'H'), "RGBA"},   // RGBA_FP16
        {0x2b, DXGI_FORMAT_R10G10B10A2_UNORM, 4, fourcc('A', 'B', '3', '0'), "RGBA"},    // RGBA_1010102
        {0x38, DXGI_FORMAT_R8_UNORM, 1, fourcc('R', '8', ' ', ' '), "R"},               // R_8
        {0x21, DXGI_FORMAT_UNKNOWN, 1, 0, ""},                                          // BLOB: memfd
        // YUV 4:2:0 de 8 bits para los códecs por software (bpp = el plano Y). dxgi = la textura RGBA de T_YUV_GPU.
        {0x32315659, DXGI_FORMAT_R8G8B8A8_UNORM, 1, fourcc('Y', 'V', '1', '2'), ""},     // YV12
        {0x23, DXGI_FORMAT_R8G8B8A8_UNORM, 1, fourcc('Y', 'U', '1', '2'), ""},           // YCBCR_420_888 (flexible): I420
        {0x11, DXGI_FORMAT_R8G8B8A8_UNORM, 1, fourcc('N', 'V', '2', '1'), ""},           // YCRCB_420_SP (NV21)
    };
    for (auto &x : t) if (x.android == f) return &x;
    return nullptr;
}
// IMPLEMENTATION_DEFINED (34) se resuelve por el uso, como minigbm: decodificador o cámara -> YCbCr_420_888 (I420: la CPU
// lo escribe con lockYCbCr y, con uso de GPU, Mesa importa la textura RGBA de T_YUV_GPU); lo demás -> RGBA_8888. El handle
// guarda el 34 pedido (PIXEL_FORMAT_REQUESTED debe casar en validateBufferSize) y el uso, así que todos resuelven igual.
constexpr uint64_t USO_YUV = (1ull << 22) | (1ull << 17) | (1ull << 18);   // VIDEO_DECODER, CAMERA_OUTPUT, CAMERA_INPUT
constexpr int resolverFormato(int f, uint64_t uso) { return f != 0x22 ? f : (uso & USO_YUV) ? 0x23 : 1; }
static_assert(resolverFormato(0x22, (1ull << 22) | (1 << 8)) == 0x23 && resolverFormato(0x22, (1 << 8) | (1 << 11)) == 1 &&
              resolverFormato(0x22, 1ull << 17) == 0x23 && resolverFormato(5, 1ull << 22) == 5);

constexpr uint64_t USO_GPU = (1 << 8) | (1 << 9) | (1 << 11) | (1 << 12) | (1 << 24);   // TEXTURE, RENDER_TARGET, COMPOSER_*, DATA_BUFFER
constexpr uint64_t USO_CPU_LEER = 0xf, USO_CPU_ESCRIBIR = 0xf0;
// Única regla de "la GPU no puede con este formato": R_8 no está en get_fourcc_from_hal_format de Mesa, así que su EGLImage
// falla y SurfaceFlinger aborta al componerlo. Con uso de GPU se rechaza al reservar y al importar (error recuperable).
// Si Mesa gana un formato o comun.h añade uno, el barrido PruebaImplDef.java (probar_impldef.sh) lo comprueba.
constexpr bool gpuNoPuede(int fResuelto, uint64_t uso) { return fResuelto == 0x38 && (uso & USO_GPU); }
// Tipo que reserva allocator.cpp para un formato ya resuelto y un uso (valido() exige el mismo): sin uso de GPU o BLOB,
// memoria de CPU; YUV con GPU, T_YUV_GPU (textura RGBA + YUV); el resto, textura D3D12.
constexpr Tipo tipoPara(int f, uint64_t uso) {
    return f == 0x21 || !(uso & USO_GPU) ? T_MEMFD : (f == 0x32315659 || f == 0x23 || f == 0x11) ? T_YUV_GPU : T_D3D12_GPU;
}
static_assert(tipoPara(1, 1 << 8) == T_D3D12_GPU && tipoPara(0x23, 1 << 8) == T_YUV_GPU && tipoPara(0x23, 0x33) == T_MEMFD &&
              tipoPara(0x21, 1 << 8) == T_MEMFD);
inline int alinear(int x, int a) { return (x + a - 1) / a * a; }

// Planos YUV (bytes desde el inicio del memfd). YV12 según su definición en graphics.h: stride de Y alineado a 16,
// el de croma ALIGN(stride/2, 16), Cr antes que Cb. YCbCr_420_888 = I420 (igual con Cb antes): los códecs c2.android lo
// piden (raw.pixel-format 35) y con NV12 intercalaban por CPU (medido 16/11: mediaswcodec 65 % -> 118 %). NV21: intercalado.
struct Yuv { int64_t y, cb, cr; int sy, sc, paso; uint64_t tam; };
inline bool yuv(int f, int stride, int alto, Yuv *o) {
    if (stride <= 0 || stride > 16384 || stride % 16 || alto <= 0 || alto > 16384) return false;   // acota la aritmética
    int64_t ty = (int64_t)stride * alto, h2 = (alto + 1) / 2;
    if (f == 0x32315659 || f == 0x23) {
        int sc = alinear(stride / 2, 16); int64_t a = ty, b = ty + sc * h2;   // a = primer plano de croma
        *o = f == 0x23 ? Yuv{0, a, b, stride, sc, 1, (uint64_t)(b + sc * h2)} : Yuv{0, b, a, stride, sc, 1, (uint64_t)(b + sc * h2)};
        return true;
    }
    if (f == 0x11) { *o = {0, ty + 1, ty, stride, stride, 2, (uint64_t)(ty + stride * h2)}; return true; }
    return false;
}

// Clase real de un fd del handle (readlink de /proc/self/fd): textura compartida de dxgkrnl o, para la CPU, dma-buf del
// montón o memfd (allocator.cpp, memoriaCpu) con al menos 'tam' bytes. Medido 17/11: "anon_inode:dxgresource", "/dmabuf:".
inline bool claseFd(int fd, char *l, size_t n) {
    char ruta[32];
    snprintf(ruta, sizeof ruta, "/proc/self/fd/%d", fd);
    ssize_t r = readlink(ruta, l, n - 1);
    if (r <= 0) return false;
    l[r] = 0;
    return true;
}
inline bool esFdTextura(int fd) { char l[64]; return claseFd(fd, l, sizeof l) && !strcmp(l, "anon_inode:dxgresource"); }
inline bool esFdCpu(int fd, uint64_t tam) {
    char l[64]; struct stat st;
    return claseFd(fd, l, sizeof l) && (!strncmp(l, "/dmabuf:", 8) || !strncmp(l, "/memfd:", 7)) && fstat(fd, &st) == 0 &&
           (uint64_t)st.st_size >= tam;
}

// Frontera de confianza: los handles llegan de apps (importBuffer). Se comprueba el ABI completo que escribe allocator.cpp:
// tipo y fds, formato conocido, dimensiones acotadas, stride = ALIGN(ancho, 16) (BLOB: = ancho) y, en los búferes que la CPU
// mapea, tamaño >= el calculado (lock además mira el tamaño real del fd).
inline const native_handle_t *valido(const native_handle_t *h) {
    if (!h || h->version != sizeof(native_handle_t) || (h->numFds != 1 && h->numFds != 2) || h->numInts != N_INTS) return nullptr;
    int t = entero(h, I_TIPO), f = resolverFormato(entero(h, I_FORMATO), largo(h, I_USO_LO)), w = entero(h, I_ANCHO),
        al = entero(h, I_ALTO), st = entero(h, I_STRIDE);
    uint64_t tam = largo(h, I_TAM_LO);
    const Formato *fm = formato(f);
    if (entero(h, I_MAGIA) != MAGIA || t < T_D3D12_CPU || t > T_YUV_GPU || (h->numFds == 2) != (t == T_YUV_GPU) || !fm ||
        entero(h, I_CAPAS) != 1 || w <= 0 || al <= 0 || w > 16384 || al > 16384 || gpuNoPuede(f, largo(h, I_USO_LO))) return nullptr;
    for (int i = 0; i < h->numFds; i++) if (h->data[i] < 0) return nullptr;
    uint64_t minimo;
    Yuv l;
    if (f == 0x21) {   // BLOB: ancho = bytes
        if (al != 1 || st != w) return nullptr;
        minimo = (uint64_t)w;
    } else if (st != alinear(w, 16)) {
        return nullptr;
    } else {
        minimo = yuv(f, st, al, &l) ? l.tam : (uint64_t)st * al * fm->bpp;
    }
    // El tipo y la clase real de cada fd deben ser los que daría allocator.cpp: un memfd que dice ser textura (o con uso de GPU)
    // llegaría a Mesa en SurfaceFlinger, fallaría y SurfaceFlinger abortaría (reinicio de Android por una app).
    if (t != tipoPara(f, largo(h, I_USO_LO))) return nullptr;
    if ((t == T_D3D12_GPU || t == T_YUV_GPU) && !esFdTextura(h->data[F_RECURSO])) return nullptr;
    if ((t == T_MEMFD || t == T_YUV_GPU) && (tam < minimo || !esFdCpu(h->data[h->numFds - 1], minimo))) return nullptr;
    return h;   // textura D3D12: su tamaño lo da el recurso
}

// Dispositivo D3D12 del proceso (perezoso): DXCore -> primer adaptador gráfico (como RutaA/fase3/compartir.cpp).
inline ID3D12Device *dispositivo() {
    static ID3D12Device *d = [] () -> ID3D12Device * {
        void *dx = dlopen("libdxcore.so", RTLD_NOW), *d3d = dlopen("libd3d12.so", RTLD_NOW);
        auto fabricaFn = dx ? (HRESULT (*)(REFIID, void **))dlsym(dx, "DXCoreCreateAdapterFactory") : nullptr;
        auto crearFn = d3d ? (PFN_D3D12_CREATE_DEVICE)dlsym(d3d, "D3D12CreateDevice") : nullptr;
        IDXCoreAdapterFactory *f = nullptr; IDXCoreAdapterList *l = nullptr; IDXCoreAdapter *a = nullptr;
        ID3D12Device *r = nullptr;
        if (!fabricaFn || !crearFn || FAILED(fabricaFn(IID_PPV_ARGS(&f)))) { LOG_E("sin libdxcore/libd3d12: %s", dlerror()); return nullptr; }
        if (SUCCEEDED(f->CreateAdapterList(1, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS, IID_PPV_ARGS(&l))) && l->GetAdapterCount() &&
            SUCCEEDED(l->GetAdapter(0, IID_PPV_ARGS(&a)))) crearFn(a, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&r));
        if (a) a->Release();
        if (l) l->Release();
        f->Release();
        if (!r) LOG_E("no se pudo crear el dispositivo D3D12");
        return r;
    }();
    return d;
}

}  // namespace sdg
