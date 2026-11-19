// PoC fase 1: un proceso Bionic (Android) crea un dispositivo D3D12 en la Adreno por /dev/dxg.
// En WSL no hay DXGI: el adaptador se obtiene con DXCore (como hace Mesa d3d12) y se pasa a D3D12CreateDevice.
// Puerta: S_OK y el dispositivo reporta su nodo. Imprime cada paso.
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>

typedef struct { uint32_t a; uint16_t b, c; uint8_t d[8]; } GUID;
static const GUID IID_ID3D12Device = {0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};
static const GUID IID_IDXCoreAdapterFactory = {0x78ee5945, 0xc36e, 0x4b13, {0xa6, 0x69, 0x00, 0x5d, 0xd1, 0x1c, 0x0f, 0x06}};
static const GUID IID_IDXCoreAdapterList = {0x526c7776, 0x40e9, 0x459b, {0xb7, 0x11, 0xf3, 0x2a, 0xd7, 0x6d, 0xfc, 0x28}};
static const GUID IID_IDXCoreAdapter = {0xf0db4c7f, 0xfe5a, 0x42a2, {0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e}};
static const GUID ATRIBUTO_D3D12_GRAFICOS = {0x0c9ece4d, 0x2f6e, 0x4f01, {0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1}};

typedef int32_t (*CrearFabrica)(const GUID *iid, void **fabrica);
typedef int32_t (*CrearLista)(void *self, uint32_t n, const GUID *atributos, const GUID *iid, void **lista);
typedef uint32_t (*Contar)(void *self);
typedef int32_t (*Obtener)(void *self, uint32_t i, const GUID *iid, void **adaptador);
typedef int32_t (*CrearDispositivo)(void *adaptador, int nivel, const GUID *iid, void **dispositivo);
typedef uint32_t (*ObtenerNodos)(void *self);
#define METODO(obj, i) ((*(void ***) (obj))[i])

int main(void) {
    void *dx = dlopen("libdxcore.so", RTLD_NOW), *d3d = dlopen("libd3d12.so", RTLD_NOW);
    if (!dx || !d3d) { printf("dlopen: %s\n", dlerror()); return 1; }
    void *fabrica = 0, *lista = 0, *adaptador = 0, *dispositivo = 0;
    int32_t hr = ((CrearFabrica) dlsym(dx, "DXCoreCreateAdapterFactory"))(&IID_IDXCoreAdapterFactory, &fabrica);
    printf("DXCoreCreateAdapterFactory hr=0x%08x\n", (unsigned) hr);
    if (hr) return 2;
    hr = ((CrearLista) METODO(fabrica, 3))(fabrica, 1, &ATRIBUTO_D3D12_GRAFICOS, &IID_IDXCoreAdapterList, &lista);
    uint32_t n = hr ? 0 : ((Contar) METODO(lista, 4))(lista);
    printf("CreateAdapterList hr=0x%08x adaptadores=%u\n", (unsigned) hr, n);
    if (!n) return 3;
    hr = ((Obtener) METODO(lista, 3))(lista, 0, &IID_IDXCoreAdapter, &adaptador);
    printf("GetAdapter(0) hr=0x%08x\n", (unsigned) hr);
    if (hr) return 4;
    hr = ((CrearDispositivo) dlsym(d3d, "D3D12CreateDevice"))(adaptador, 0xb000 /* nivel 11_0 */, &IID_ID3D12Device, &dispositivo);
    printf("D3D12CreateDevice hr=0x%08x dispositivo=%p\n", (unsigned) hr, dispositivo);
    if (hr || !dispositivo) return 5;
    // ID3D12Device: IUnknown (3) + ID3D12Object (4) -> GetNodeCount es la entrada 7.
    printf("GetNodeCount=%u\nPUERTA_FASE1=OK\n", ((ObtenerNodos) METODO(dispositivo, 7))(dispositivo));
    return 0;
}
