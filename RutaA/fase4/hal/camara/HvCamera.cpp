/*
 * Copyright (C) 2025 The Android Open Source Project
 * Copyright (C) 2026 SurfaceDual: cámaras de Windows (HvCamara.exe por vsock) en lugar de las de QEMU (GasQemuCamera).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define FAILURE_DEBUG_PREFIX "HvCamera"

#include <inttypes.h>
#include <string.h>

#include <chrono>

#include <libyuv/convert.h>
#include <libyuv/scale.h>
#include <ui/GraphicBufferMapper.h>

#include "HvCamera.h"
#include "debug.h"
#include "qemu_channel.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {
namespace hw {
namespace {
constexpr char kClass[] = "HvCamera";
}  // namespace

bool nv12AI420(const uint8_t* const src, const int W, const int H, const android_ycbcr& d,
               const int w, const int h, std::vector<uint8_t>* const tmp) {
    if (W <= 0 || H <= 0 || w <= 0 || h <= 0 || ((W | H | w | h) & 1) || d.chroma_step != 1) {
        return FAILURE(false);
    }
    // Recorte centrado con el aspecto del destino: con vista previa 16:9 y foto 4:3 a la vez, la imagen no se deforma.
    int cw = W, ch = H;
    if (int64_t(W) * h > int64_t(H) * w) {
        cw = int(int64_t(H) * w / h) & ~1;
    } else {
        ch = int(int64_t(W) * h / w) & ~1;
    }
    if (cw <= 0 || ch <= 0) {
        return FAILURE(false);
    }
    const int cx = ((W - cw) / 2) & ~1, cy = ((H - ch) / 2) & ~1;
    const uint8_t* const sy = src + size_t(cy) * W + cx;
    const uint8_t* const suv = src + size_t(W) * H + size_t(cy / 2) * W + cx;
    uint8_t* const y = static_cast<uint8_t*>(d.y);
    uint8_t* const u = static_cast<uint8_t*>(d.cb);
    uint8_t* const v = static_cast<uint8_t*>(d.cr);
    const int ys = int(d.ystride), cs = int(d.cstride);
    if (cw == w && ch == h) {
        return libyuv::NV12ToI420(sy, W, suv, W, y, ys, u, cs, v, cs, w, h) == 0;
    }
    // Escalar en NV12 al tamaño final y separar U/V ya pequeño: medio paso menos sobre el cuadro grande que
    // NV12->I420 + I420Scale (por cuadro de vista previa a 30/s).
    tmp->resize(size_t(w) * h * 3 / 2);
    uint8_t* const ty = tmp->data();
    uint8_t* const tuv = ty + size_t(w) * h;
    return libyuv::NV12Scale(sy, W, suv, W, cw, ch, ty, w, tuv, w, w, h, libyuv::kFilterBilinear) == 0 &&
           libyuv::NV12ToI420(ty, w, tuv, w, y, ys, u, cs, v, cs, w, h) == 0;
}

HvCamera::HvCamera(const Parameters& params) : BaseQemuCamera(params) {}

HvCamera::~HvCamera() {
    close();
}

bool HvCamera::configure(const CameraMetadata& sessionParams,
                         const size_t nStreams,
                         const Stream* streams,
                         const HalStream* halStreams) {
    Rect<uint16_t> mayor = {0, 0};
    mStreams.resize(nStreams);
    for (size_t i = 0; i < nStreams; ++i, ++streams, ++halStreams) {
        LOG_ALWAYS_FATAL_IF(streams->id != halStreams->id);
        StreamInfo& si = mStreams[i];
        si.id = streams->id;
        si.size.width = streams->width;
        si.size.height = streams->height;
        si.blobBufferSize = streams->bufferSize;
        si.format = halStreams->overrideFormat;
        if (si.size.area() > mayor.area()) {
            mayor = si.size;
        }
    }
    applyMetadata(sessionParams);

    // Windows entrega un solo tamaño, el mayor de los flujos (la foto sale a resolución completa); los demás se
    // recortan/escalan aquí. Medido 17/11: cambiar de tamaño con la captura en marcha deja ~0,6 s de cuadros negros
    // mientras Windows reajusta la exposición (fotos oscuras), escalar en Windows gasta ~25 % de un núcleo y mandar
    // 1440p en vez de 1080p solo sube la VM de 64 a 67 % (el grueso es mostrar 30 cuadros/s). Reconfigurar al mismo
    // tamaño no reabre la cámara de Windows.
    if (mIniciada && mayor == mCaptura) {
        return true;
    }
    char q[128];
    snprintf(q, sizeof(q), "start name=%s dim=%ux%u", mParams.name.c_str(), mayor.width, mayor.height);
    mIniciada = canalConsulta(q);
    mCaptura = mIniciada ? mayor : Rect<uint16_t>{0, 0};
    return mIniciada;
}

void HvCamera::close() {
    if (mIniciada) {
        // Suelta la cámara de Windows (se apaga su luz) en cuanto Android la cierra.
        char q[96];
        snprintf(q, sizeof(q), "stop name=%s", mParams.name.c_str());
        canalConsulta(q);
        mIniciada = false;
    }
    mStreams.clear();
    mStats = {};
    mCuadro = {};
    mTmp = {};
}

std::tuple<int64_t, int64_t, CameraMetadata,
           std::vector<StreamBuffer>, std::vector<DelayedStreamBuffer>>
HvCamera::processCaptureRequest(CameraMetadata metadataUpdate,
                                Span<CachedStreamBuffer*> csbs) {
    CameraMetadata resultMetadata = metadataUpdate.metadata.empty() ?
        updateCaptureResultMetadata() :
        applyMetadata(std::move(metadataUpdate));

    // Un cuadro de Windows por petición para todos sus búferes: vista previa, análisis y foto ven la misma imagen.
    // Llega con una cabecera {ancho, alto, edad en µs desde la captura en Windows} y luego NV12 compacto.
    const size_t tam = kCabecera + mCaptura.area() * 3 / 2;
    const auto t0 = std::chrono::steady_clock::now();
    char q[128];
    snprintf(q, sizeof(q), "frame name=%s", mParams.name.c_str());
    std::string motivo;
    bool hayCuadro = mIniciada && canalConsulta(q, &mCuadro, tam, &motivo) && mCuadro.size() == tam;
    if (mIniciada && motivo == "no abierta") {
        // HvCamara se reinició (conexión nueva, sin cámara abierta): volver a pedirla una vez. No ante otros fallos: con
        // "ocupada" (la tiene otra cámara de Android) reabrir le robaría la cámara a la otra sesión.
        char s[128];
        snprintf(s, sizeof(s), "start name=%s dim=%ux%u", mParams.name.c_str(), mCaptura.width, mCaptura.height);
        hayCuadro = canalConsulta(s) && canalConsulta(q, &mCuadro, tam) && mCuadro.size() == tam;
    }
    uint32_t w = 0, h = 0;
    uint64_t edad = 0;
    if (hayCuadro) {
        memcpy(&w, &mCuadro[0], 4);
        memcpy(&h, &mCuadro[4], 4);
        memcpy(&edad, &mCuadro[8], 8);
        hayCuadro = w == mCaptura.width && h == mCaptura.height;   // frontera de confianza: lo que dice el host
    }
    const auto t1 = std::chrono::steady_clock::now();

    const size_t csbsSize = csbs.size();
    std::vector<StreamBuffer> outputBuffers;
    std::vector<DelayedStreamBuffer> delayedOutputBuffers;
    outputBuffers.reserve(csbsSize);

    for (size_t i = 0; i < csbsSize; ++i) {
        CachedStreamBuffer* csb = csbs[i];
        LOG_ALWAYS_FATAL_IF(!csb);  // otherwise mNumBuffersInFlight will be hard

        const StreamInfo* si = csb->getStreamInfo<StreamInfo>();
        if (!si) {
            const int32_t id = csb->getStreamId();
            const auto sii = std::find_if(mStreams.begin(), mStreams.end(),
                                          [id](const StreamInfo& si){ return id == si.id; });
            if (sii == mStreams.end()) {
                ALOGE("%s:%s:%d could not find stream=%d in the cache",
                      kClass, __func__, __LINE__, csb->getStreamId());
            } else {
                si = &*sii;
                csb->setStreamInfo(si);
            }
        }

        if (!si) {
            outputBuffers.push_back(csb->finish(false));
        } else if (si->format == PixelFormat::BLOB) {
            delayedOutputBuffers.push_back(captureFrameJpeg(*si, csb, hayCuadro));
        } else {
            outputBuffers.push_back(csb->finish(hayCuadro && captureFrameYUV(*si, csb)));
        }
    }

    if (hayCuadro) {
        // Diagnóstico cada 5 s mientras hay vista previa: tiempos por cuadro (latencia ~ edad + red + proceso).
        const auto t2 = std::chrono::steady_clock::now();
        using us = std::chrono::microseconds;
        mStats.n++;
        mStats.edad += edad;
        mStats.red += std::chrono::duration_cast<us>(t1 - t0).count();
        mStats.proceso += std::chrono::duration_cast<us>(t2 - t1).count();
        mStats.bufs += csbsSize;
        if (t2 - mStats.desde > std::chrono::seconds(5)) {
            const double s = std::chrono::duration<double>(t2 - mStats.desde).count();
            ALOGI("%s: %.1f cuadros/s %ux%u, %.1f búferes/petición; ms por cuadro: edad en Windows %.1f, espera+envío %.1f, "
                  "conversión+subida %.1f", mParams.name.c_str(), mStats.n / s, w, h, double(mStats.bufs) / mStats.n,
                  mStats.edad / 1000.0 / mStats.n, mStats.red / 1000.0 / mStats.n, mStats.proceso / 1000.0 / mStats.n);
            mStats = {};
            mStats.desde = t2;
        }
    }

    // Sin cuadro ni tras reabrir: Windows no tiene la cámara (HvCamara cerrado, permiso denegado u otra app la usa).
    // ERROR_DEVICE hace que la app cierre la cámara en vez de reintentar 30 veces por segundo.
    return make_tuple((hayCuadro ? mFrameDurationNs : FAILURE(-1)),
                      mSensorExposureDurationNs,
                      std::move(resultMetadata), std::move(outputBuffers),
                      std::move(delayedOutputBuffers));
}

bool HvCamera::captureFrameYUV(const StreamInfo& si, CachedStreamBuffer* csb) {
    if (!csb->waitAcquireFence(mFrameDurationNs / 2000000)) {
        return FAILURE(false);
    }
    android_ycbcr ycbcr;
    if (GraphicBufferMapper::get().lockYCbCr(
            csb->getBuffer(), static_cast<uint32_t>(BufferUsage::CPU_WRITE_OFTEN),
            {si.size.width, si.size.height}, &ycbcr) != NO_ERROR) {
        return FAILURE(false);
    }
    const bool ok = nv12AI420(mCuadro.data() + kCabecera, mCaptura.width, mCaptura.height, ycbcr,
                              si.size.width, si.size.height, &mTmp);
    // unlock sube el YUV a la textura de la GPU (vista previa); si falla, el búfer va como error.
    return (GraphicBufferMapper::get().unlock(csb->getBuffer()) == NO_ERROR) && ok;
}

DelayedStreamBuffer HvCamera::captureFrameJpeg(const StreamInfo& si, CachedStreamBuffer* csb,
                                               bool ok) {
    // I420 al tamaño de la foto ahora (mCuadro se reutiliza en la próxima petición); se comprime en el hilo de diferidos.
    const int w = si.size.width, h = si.size.height;
    auto image = std::make_shared<std::vector<uint8_t>>();
    android_ycbcr ycbcr = {};
    if (ok) {
        image->resize(size_t(w) * h + 2 * size_t(w / 2) * (h / 2));
        uint8_t* const y = image->data();
        ycbcr = android_ycbcr{y, y + size_t(w) * h, y + size_t(w) * h + size_t(w / 2) * (h / 2),
                              size_t(w), size_t(w / 2), 1, {}};
        ok = nv12AI420(mCuadro.data() + kCabecera, mCaptura.width, mCaptura.height, ycbcr, w, h, &mTmp);
    }

    return [csb, image, ycbcr, imageSize = si.size, metadata = mCaptureResultMetadata,
            jpegBufferSize = si.blobBufferSize, frameDurationNs = mFrameDurationNs,
            ok](const bool process) -> StreamBuffer {
        if (ok && process && csb->waitAcquireFence(frameDurationNs / 1000000)) {
            return csb->finish(compressJpeg(imageSize, ycbcr, metadata, csb->getBuffer(),
                                            jpegBufferSize));
        }
        return csb->finish(false);
    };
}

}  // namespace hw
}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
