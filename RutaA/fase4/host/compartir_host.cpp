// Ruta A, paso de presentación: ¿qué da D3DKMTShareObjectWithHost? Crea una textura D3D12 compartida (como el gralloc),
// la abre como recurso KMT y la comparte con el host; imprime el "vail NT handle" y espera para buscarlo en Windows.
//   compartir_host [segundos]
#include <wsl/winadapter.h>
#include <directx/d3d12.h>
#include <directx/dxcore.h>
#include <dxguids/dxguids.h>
#undef interface
#include <misc/d3dkmthk.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#define EXIGE(x) do { if (!(x)) { fprintf(stderr, "fallo: %s\n", #x); return 1; } } while (0)
#define KMT(x) do { int32_t s_ = (x); if (s_ < 0) { fprintf(stderr, "%s -> NTSTATUS 0x%08x\n", #x, (unsigned)s_); return 1; } } while (0)

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    void *dx = dlopen("libdxcore.so", RTLD_NOW), *d3d = dlopen("libd3d12.so", RTLD_NOW);
    EXIGE(dx && d3d);
    auto fabricaFn = (HRESULT (*)(REFIID, void **))dlsym(dx, "DXCoreCreateAdapterFactory");
    auto crearFn = (PFN_D3D12_CREATE_DEVICE)dlsym(d3d, "D3D12CreateDevice");
    auto abrirAdaptador = (int32_t (*)(d3dkmt_openadapterfromluid *))dlsym(dx, "D3DKMTOpenAdapterFromLuid");
    auto crearDispositivo = (int32_t (*)(d3dkmt_createdevice *))dlsym(dx, "D3DKMTCreateDevice");
    auto consultar = (int32_t (*)(d3dkmt_queryresourceinfofromnthandle *))dlsym(dx, "D3DKMTQueryResourceInfoFromNtHandle");
    auto abrirRecurso = (int32_t (*)(d3dkmt_openresourcefromnthandle *))dlsym(dx, "D3DKMTOpenResourceFromNtHandle");
    auto compartirHost = (int32_t (*)(d3dkmt_shareobjectwithhost *))dlsym(dx, "D3DKMTShareObjectWithHost");
    EXIGE(fabricaFn && crearFn && abrirAdaptador && crearDispositivo && consultar && abrirRecurso && compartirHost);

    IDXCoreAdapterFactory *f = nullptr; IDXCoreAdapterList *l = nullptr; IDXCoreAdapter *a = nullptr; ID3D12Device *d = nullptr;
    EXIGE(SUCCEEDED(fabricaFn(IID_PPV_ARGS(&f))));
    EXIGE(SUCCEEDED(f->CreateAdapterList(1, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS, IID_PPV_ARGS(&l))) && l->GetAdapterCount());
    EXIGE(SUCCEEDED(l->GetAdapter(0, IID_PPV_ARGS(&a))) && SUCCEEDED(crearFn(a, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d))));

    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = 1280; rd.Height = 800; rd.DepthOrArraySize = 1;
    rd.MipLevels = 1; rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ID3D12Resource *r = nullptr; HANDLE h = nullptr;
    EXIGE(SUCCEEDED(d->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_SHARED, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&r))));
    EXIGE(SUCCEEDED(d->CreateSharedHandle(r, nullptr, GENERIC_ALL, nullptr, &h)));
    printf("textura compartida fd=%d\n", (int)(intptr_t)h);

    LUID luid = d->GetAdapterLuid();
    d3dkmt_openadapterfromluid ad = {}; ad.adapter_luid.a = luid.LowPart; ad.adapter_luid.b = (uint32_t)luid.HighPart;
    KMT(abrirAdaptador(&ad));
    d3dkmt_createdevice kd = {}; kd.adapter = ad.adapter_handle;
    KMT(crearDispositivo(&kd));

    d3dkmt_queryresourceinfofromnthandle q = {}; q.device = kd.device; q.nt_handle = (uint64_t)(intptr_t)h;
    KMT(consultar(&q));
    printf("asignaciones=%u runtime=%u total=%u recurso=%u\n", q.allocation_count, q.private_runtime_data_size,
           q.total_priv_drv_data_size, q.resource_priv_drv_data_size);
    std::vector<d3dddi_openallocationinfo2> asig(q.allocation_count);
    std::vector<uint8_t> runtime(q.private_runtime_data_size), total(q.total_priv_drv_data_size), recurso(q.resource_priv_drv_data_size);
    d3dkmt_openresourcefromnthandle o = {};
    o.device = kd.device; o.nt_handle = q.nt_handle; o.allocation_count = q.allocation_count;
    o.open_alloc_info = (uint64_t)asig.data();
    o.private_runtime_data_size = (int)runtime.size(); o.private_runtime_data = (uint64_t)runtime.data();
    o.resource_priv_drv_data_size = (uint32_t)recurso.size(); o.resource_priv_drv_data = (uint64_t)recurso.data();
    o.total_priv_drv_data_size = (uint32_t)total.size(); o.total_priv_drv_data = (uint64_t)total.data();
    KMT(abrirRecurso(&o));
    printf("recurso KMT=0x%x\n", o.resource.v);

    d3dkmt_shareobjectwithhost s = {}; s.device_handle = kd.device; s.object_handle = o.resource;
    KMT(compartirHost(&s));
    printf("VAIL_NT_HANDLE=0x%llx pid=%d\n", (unsigned long long)s.object_vail_nt_handle, getpid());
    sleep(argc > 1 ? atoi(argv[1]) : 120);
    return 0;
}
