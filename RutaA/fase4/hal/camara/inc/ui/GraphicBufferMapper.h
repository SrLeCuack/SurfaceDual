#pragma once
// GraphicBufferMapper de libui reducido a lo que usa el HAL de cámara, sobre el mapper estable en C (AIMapper v5) de
// nuestro gralloc (mapper.ranchu.so). libui es C++ de la plataforma y no casa con la libc++ del NDK (ver unique_fd.h).
#include <cutils/native_handle.h>
#include <stdint.h>
#include <system/graphics.h>

namespace android {

using status_t = int32_t;
enum : status_t { NO_ERROR = 0, NO_INIT = -19, UNKNOWN_ERROR = -2147483647 - 1 };

struct Rect {
    int32_t left, top, right, bottom;
    Rect(int32_t w, int32_t h) : left(0), top(0), right(w), bottom(h) {}
};

class GraphicBufferMapper {
public:
    static GraphicBufferMapper& get();
    status_t importBufferNoValidate(const native_handle_t* h, const native_handle_t** out);
    status_t freeBuffer(const native_handle_t* h);
    status_t lock(const native_handle_t* h, uint32_t usage, const Rect& r, void** out);
    // Solo búferes YUV 4:2:0 de nuestro gralloc (I420 o NV21): la disposición de planos sale de su handle (comun.h).
    status_t lockYCbCr(const native_handle_t* h, uint32_t usage, const Rect& r, android_ycbcr* out);
    status_t unlock(const native_handle_t* h);
};

}  // namespace android
