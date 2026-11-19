/*
 * Copyright (C) 2023 The Android Open Source Project
 * Copyright (C) 2026 SurfaceDual: cámaras de Windows por vsock (HvCamara.exe), registradas como internal/0.
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

#include <chrono>
#include <condition_variable>
#include <mutex>

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

#include <stdio.h>
#include <string.h>

#include "CameraProvider.h"
#include "HvCamera.h"
#include "list_qemu_cameras.h"
#include "qemu_channel.h"

namespace {
// Autocomprobación ("--prueba", en la VM): recorte/escalado NV12 -> I420, cabeceras del canal y lista de HvCamara.
int prueba() {
    using namespace android::hardware::camera::provider::implementation;
    int fallos = 0;
    auto mal = [&fallos](bool c, const char* q) { if (!c) { printf("FALLO: %s\n", q); ++fallos; } };
    // Cuadro 8x4 NV12: Y = x (columna), U = 100, V = 200.
    std::vector<uint8_t> nv12(8 * 4 * 3 / 2);
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 8; ++x) nv12[y * 8 + x] = uint8_t(x * 30);
    for (int i = 32; i < 48; i += 2) { nv12[i] = 100; nv12[i + 1] = 200; }
    std::vector<uint8_t> o(64), tmp;
    android_ycbcr d = {o.data(), o.data() + 32, o.data() + 48, 8, 4, 1, {}};
    mal(hw::nv12AI420(nv12.data(), 8, 4, d, 8, 4, &tmp) && o[5] == 150 && o[32] == 100 && o[48] == 200, "copia igual");
    // 4:3 desde 16:9 aprox. (8x4 -> 4x4): recorte centrado de las columnas 2..5.
    android_ycbcr c = {o.data(), o.data() + 16, o.data() + 20, 4, 2, 1, {}};
    mal(hw::nv12AI420(nv12.data(), 8, 4, c, 4, 4, &tmp) && o[0] == 60 && o[3] == 150 && o[16] == 100 && o[20] == 200, "recorte");
    mal(!hw::nv12AI420(nv12.data(), 8, 4, c, 3, 4, &tmp), "impar rechazado");
    // Escalado a la mitad (8x4 -> 4x2): el croma se conserva y el brillo sigue creciendo de izquierda a derecha.
    android_ycbcr m = {o.data(), o.data() + 8, o.data() + 10, 4, 2, 1, {}};
    mal(hw::nv12AI420(nv12.data(), 8, 4, m, 4, 2, &tmp) && o[0] < o[3] && o[8] == 100 && o[10] == 200, "escalado");
    size_t n = 0;
    mal(hw::canalAnalizarCabecera("0000002a", 100, &n) && n == 42, "cabecera");
    mal(!hw::canalAnalizarCabecera("0000002g", 100, &n), "cabecera no hex");
    mal(!hw::canalAnalizarCabecera("00000065", 100, &n), "cabecera larga");
    int k = 0;
    mal(hw::listQemuCameras("name=trasera dir=back framedims=640x480,1920x1080 sensor_orientation=0\n"
                            "name=frontal dir=front framedims=640x480 sensor_orientation=0\n",
                            [&k](hw::HwCameraFactory) { ++k; }) && k == 2, "lista");
    // Coste por cuadro de vista previa (para decidir qué tamaño pedir a Windows).
    for (auto [W, H] : {std::pair{2560, 1440}, std::pair{1920, 1080}}) {
        std::vector<uint8_t> src(size_t(W) * H * 3 / 2, 128), dst(1920 * 1080 * 3 / 2);
        android_ycbcr d = {dst.data(), dst.data() + 1920 * 1080, dst.data() + 1920 * 1080 * 5 / 4, 1920, 960, 1, {}};
        auto a = std::chrono::steady_clock::now();
        for (int i = 0; i < 30; ++i) hw::nv12AI420(src.data(), W, H, d, 1920, 1080, &tmp);
        printf("%dx%d -> 1920x1080: %.2f ms por cuadro\n", W, H,
               std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count() / 30);
    }
    printf("%s (%d fallos)\n", fallos ? "MAL" : "OK", fallos);
    return fallos != 0;
}

// Prueba en vivo con HvCamara sin tocar cameraserver ("--vivo <cámara> <WxH> <cuadros>", como shell en la VM):
// latencia por cuadro y ancho de banda del canal.
int vivo(const char* nombre, const char* dim, int cuadros) {
    using namespace android::hardware::camera::provider::implementation;
    std::mutex m;
    std::condition_variable cv;
    bool listo = false;
    hw::canalIniciar([&](const std::string& lista) {
        printf("lista:\n%s", lista.c_str());
        std::lock_guard<std::mutex> g(m);
        listo = true;
        cv.notify_all();
    });
    {
        std::unique_lock<std::mutex> l(m);
        if (!cv.wait_for(l, std::chrono::seconds(30), [&listo] { return listo; })) { printf("HvCamara no se conectó\n"); return 1; }
    }
    int w = 0, h = 0;
    if (sscanf(dim, "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) return 1;
    char q[128];
    auto t0 = std::chrono::steady_clock::now();
    snprintf(q, sizeof(q), "start name=%s dim=%s", nombre, dim);
    if (!hw::canalConsulta(q)) { printf("start falló\n"); return 1; }
    auto t1 = std::chrono::steady_clock::now();
    printf("start: %lld ms\n", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    std::vector<uint8_t> f;
    snprintf(q, sizeof(q), "frame name=%s", nombre);
    long long peor = 0;
    for (int i = 0; i < cuadros; ++i) {
        auto a = std::chrono::steady_clock::now();
        if (!hw::canalConsulta(q, &f, 16 + size_t(w) * h * 3 / 2)) { printf("frame %d falló\n", i); return 1; }
        peor = std::max<long long>(peor, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - a).count());
    }
    auto t2 = std::chrono::steady_clock::now();
    const double s = std::chrono::duration<double>(t2 - t1).count();
    long long suma = 0;
    for (size_t i = 0; i < size_t(w) * h; i += 4099) suma += f[16 + i];
    printf("%d cuadros en %.2f s: %.1f cuadros/s, %.0f MB/s, peor %lld ms, Y medio ~%lld\n", cuadros, s, cuadros / s,
           cuadros * f.size() / s / 1e6, peor / 1000, suma / (long long)(size_t(w) * h / 4099 + 1));
    FILE* o = fopen("/data/local/tmp/cuadro.nv12", "wb");
    if (o) { fwrite(f.data() + 16, 1, f.size() - 16, o); fclose(o); }
    snprintf(q, sizeof(q), "stop name=%s", nombre);
    return hw::canalConsulta(q) ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
    using namespace android::hardware::camera::provider::implementation;
    using hw::HwCameraFactory;
    if (argc > 1 && !strcmp(argv[1], "--prueba")) return prueba();
    if (argc == 5 && !strcmp(argv[1], "--vivo")) return vivo(argv[2], argv[3], atoi(argv[4]));

    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();

    // Ids "0" (la primera de la lista de HvCamara: la trasera) y "1": Camera.open() y las apps antiguas esperan el 0.
    std::shared_ptr<CameraProvider> hal = ndk::SharedRefBase::make<CameraProvider>(0);

    std::mutex m;
    std::condition_variable cv;
    bool listo = false;
    hw::canalIniciar([&m, &cv, &listo, hal](const std::string& lista) {
        std::vector<HwCameraFactory> camaras;
        hw::listQemuCameras(lista, [&camaras](HwCameraFactory c) { camaras.push_back(std::move(c)); });
        hal->setCameras(std::move(camaras));
        std::lock_guard<std::mutex> g(m);
        listo = true;
        cv.notify_all();
    });
    {
        // HvCamara se conecta en cuanto la VM escucha (reintenta cada segundo): esperar un poco para registrarse ya con
        // las cámaras. Si no llega, se registra igual (cameraserver espera a los HAL declarados) y llegan después.
        std::unique_lock<std::mutex> l(m);
        cv.wait_for(l, std::chrono::seconds(3), [&listo] { return listo; });
    }

    const std::string instance = std::string(CameraProvider::descriptor) + "/internal/0";
    if (AServiceManager_addService(hal->asBinder().get(), instance.c_str()) != STATUS_OK) {
        ALOGE("Failed to register '%s'", instance.c_str());
        return 1;
    }
    ABinderProcess_joinThreadPool();
    return 1;
}
