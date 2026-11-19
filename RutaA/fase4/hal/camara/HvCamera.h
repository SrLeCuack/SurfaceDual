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

#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "BaseQemuCamera.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {
namespace hw {

// Recorte centrado al aspecto del destino + escalado de un cuadro NV12 (W x H, compacto) a I420 (libyuv, NEON).
bool nv12AI420(const uint8_t* nv12, int W, int H, const android_ycbcr& dst, int w, int h,
               std::vector<uint8_t>* tmp);

struct HvCamera : public BaseQemuCamera {
    explicit HvCamera(const Parameters& params);
    ~HvCamera() override;

    bool configure(const CameraMetadata& sessionParams, size_t nStreams,
                   const Stream* streams, const HalStream* halStreams) override;
    void close() override;

    std::tuple<int64_t, int64_t, CameraMetadata, std::vector<StreamBuffer>,
               std::vector<DelayedStreamBuffer>>
        processCaptureRequest(CameraMetadata, Span<CachedStreamBuffer*>) override;

private:
    struct StreamInfo {
        int32_t id;
        uint32_t blobBufferSize;
        PixelFormat format;
        Rect<uint16_t> size;
    };

    bool captureFrameYUV(const StreamInfo& si, CachedStreamBuffer* csb);
    DelayedStreamBuffer captureFrameJpeg(const StreamInfo& si, CachedStreamBuffer* csb, bool ok);

    std::vector<StreamInfo> mStreams;
    Rect<uint16_t> mCaptura = {0, 0};   // tamaño que entrega Windows (el mayor de los flujos)
    std::vector<uint8_t> mCuadro;       // último cuadro NV12 de Windows
    std::vector<uint8_t> mTmp;
    bool mIniciada = false;
    static constexpr size_t kCabecera = 16;
    struct {
        uint64_t n = 0, edad = 0, red = 0, proceso = 0, bufs = 0;
        std::chrono::steady_clock::time_point desde = std::chrono::steady_clock::now();
    } mStats;
};

}  // namespace hw
}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
