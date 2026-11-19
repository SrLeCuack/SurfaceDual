// GraphicBufferMapper (inc/ui/GraphicBufferMapper.h) sobre AIMapper v5 de mapper.ranchu.so (hal/gralloc/mapper.cpp).
#include <dlfcn.h>
#include <log/log.h>
#include <ui/GraphicBufferMapper.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include "../gralloc/comun.h"

namespace android {
namespace {
AIMapper* mapper() {
    static AIMapper* m = [] {
        AIMapper* r = nullptr;
        void* so = dlopen("/vendor/lib64/hw/mapper.ranchu.so", RTLD_NOW | RTLD_LOCAL);
        auto cargar = so ? (AIMapper_Error (*)(AIMapper**))dlsym(so, "AIMapper_loadIMapper") : nullptr;
        if (!cargar || cargar(&r) != AIMAPPER_ERROR_NONE || !r) ALOGE("sin mapper.ranchu.so: %s", dlerror());
        return r;
    }();
    return m;
}
status_t error(AIMapper_Error e) { return e == AIMAPPER_ERROR_NONE ? NO_ERROR : UNKNOWN_ERROR; }
}  // namespace

GraphicBufferMapper& GraphicBufferMapper::get() { static GraphicBufferMapper g; return g; }

status_t GraphicBufferMapper::importBufferNoValidate(const native_handle_t* h, const native_handle_t** out) {
    AIMapper* m = mapper();
    return m ? error(m->v5.importBuffer(h, out)) : NO_INIT;
}

status_t GraphicBufferMapper::freeBuffer(const native_handle_t* h) {
    AIMapper* m = mapper();
    return m ? error(m->v5.freeBuffer(h)) : NO_INIT;
}

status_t GraphicBufferMapper::lock(const native_handle_t* h, uint32_t usage, const Rect& r, void** out) {
    AIMapper* m = mapper();
    return m ? error(m->v5.lock(h, usage, ARect{r.left, r.top, r.right, r.bottom}, -1, out)) : NO_INIT;
}

status_t GraphicBufferMapper::lockYCbCr(const native_handle_t* h, uint32_t usage, const Rect& r, android_ycbcr* out) {
    // El handle ya pasó por importBuffer (valido() de comun.h), así que sus ints son los de nuestro allocator.
    sdg::Yuv l;
    const int f = sdg::resolverFormato(sdg::entero(h, sdg::I_FORMATO), sdg::largo(h, sdg::I_USO_LO));
    if (!sdg::yuv(f, sdg::entero(h, sdg::I_STRIDE), sdg::entero(h, sdg::I_ALTO), &l)) return UNKNOWN_ERROR;
    void* base = nullptr;
    const status_t s = lock(h, usage, r, &base);
    if (s != NO_ERROR) return s;
    uint8_t* p = static_cast<uint8_t*>(base);
    *out = android_ycbcr{p + l.y, p + l.cb, p + l.cr, (size_t)l.sy, (size_t)l.sc, (size_t)l.paso, {}};
    return NO_ERROR;
}

status_t GraphicBufferMapper::unlock(const native_handle_t* h) {
    AIMapper* m = mapper();
    if (!m) return NO_INIT;
    int fence = -1;
    const status_t s = error(m->v5.unlock(h, &fence));
    if (fence >= 0) close(fence);   // nuestro mapper siempre devuelve -1 (unlock síncrono)
    return s;
}

}  // namespace android
