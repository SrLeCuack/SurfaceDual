/*
 * Copyright (C) 2023 The Android Open Source Project
 * Copyright (C) 2026 SurfaceDual: canal por vsock con HvCamara.exe en lugar de la tubería de QEMU.
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

#define FAILURE_DEBUG_PREFIX "canal"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/vm_sockets.h>

#include <algorithm>
#include <mutex>
#include <thread>

#include <log/log.h>

#include "debug.h"
#include "qemu_channel.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {
namespace hw {
namespace {
constexpr unsigned kPuerto = 5007;
constexpr size_t kMaxLista = 4096;
constexpr size_t kMaxError = 256;

std::mutex gMutex;   // una consulta a la vez: hay una sola conexión
int gFd = -1;

bool leerTodo(const int fd, void* p, size_t n) {
    uint8_t* b = static_cast<uint8_t*>(p);
    while (n) {
        const ssize_t r = recv(fd, b, n, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        b += r; n -= r;
    }
    return true;
}

bool escribirTodo(const int fd, const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    while (n) {
        const ssize_t r = send(fd, b, n, MSG_NOSIGNAL);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        b += r; n -= r;
    }
    return true;
}

void cerrarLocked() {
    if (gFd >= 0) { close(gFd); gFd = -1; }
}
}  // namespace

bool canalAnalizarCabecera(const char cab[8], const size_t max, size_t* largo) {
    size_t n = 0;
    for (int i = 0; i < 8; ++i) {
        const char c = cab[i];
        const int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 :
                      (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (d < 0) return false;
        n = n * 16 + d;
    }
    if (n > max) return false;
    *largo = n;
    return true;
}

void canalIniciar(std::function<void(const std::string&)> alListar) {
    std::thread([alListar = std::move(alListar)] {
        int s;
        for (;;) {   // un fallo al arrancar (vsock aún sin cargar) no deja al HAL sin canal para siempre
            s = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
            sockaddr_vm a = {};
            a.svm_family = AF_VSOCK;
            a.svm_port = kPuerto;
            a.svm_cid = VMADDR_CID_ANY;
            if (s >= 0 && !bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) && !listen(s, 1)) break;
            ALOGE("canal: vsock %u: %s", kPuerto, strerror(errno));
            if (s >= 0) close(s);
            sleep(5);
        }
        for (;;) {
            sockaddr_vm p = {};
            socklen_t pl = sizeof(p);
            const int c = accept4(s, reinterpret_cast<sockaddr*>(&p), &pl, SOCK_CLOEXEC);
            if (c < 0) {
                if (errno != EINTR) { ALOGE("canal: accept: %s", strerror(errno)); sleep(1); }
                continue;
            }
            // Frontera de confianza: solo Windows (CID 2), esperas acotadas y lista corta de texto imprimible.
            // Si el host se cuelga, la cámara falla a los 3 s en vez de dejar colgado a cameraserver.
            const timeval t = {3, 0};
            setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
            setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));
            char cab[8];
            size_t n = 0;
            std::string lista;
            bool bien = p.svm_cid == VMADDR_CID_HOST && leerTodo(c, cab, 8) &&
                        canalAnalizarCabecera(cab, kMaxLista, &n);
            if (bien) {
                lista.resize(n);
                bien = leerTodo(c, lista.data(), n) &&
                       std::all_of(lista.begin(), lista.end(), [](char x) { return x == '\n' || (x >= ' ' && x <= '~'); });
            }
            if (!bien) {
                ALOGE("canal: conexión rechazada (cid=%u)", p.svm_cid);
                close(c);
                continue;
            }
            ALOGI("canal: HvCamara conectado");
            {
                std::lock_guard<std::mutex> g(gMutex);
                cerrarLocked();
                gFd = c;
            }
            alListar(lista);
        }
    }).detach();
}

bool canalConsulta(const char* const consulta, std::vector<uint8_t>* const datos, const size_t maxDatos,
                   std::string* const motivo) {
    std::lock_guard<std::mutex> g(gMutex);
    if (motivo) motivo->clear();
    if (gFd < 0) return FAILURE_V(false, "'%s': HvCamara no está conectado", consulta);

    char cab[8];
    char tipo[3] = {};
    size_t n = 0;
    if (!escribirTodo(gFd, consulta, strlen(consulta) + 1) || !leerTodo(gFd, cab, 8) ||
        !canalAnalizarCabecera(cab, std::max(maxDatos, kMaxError) + 3, &n) || n < 2 ||
        !leerTodo(gFd, tipo, std::min<size_t>(n, 3))) {
        cerrarLocked();
        return FAILURE_V(false, "'%s': conexión perdida", consulta);
    }
    const bool ok = !memcmp(tipo, "ok", 2);
    if ((!ok && memcmp(tipo, "ko", 2)) || (n >= 3 && tipo[2] != ':' && tipo[2] != '\0') || (ok && n > 3 && n - 3 > maxDatos)) {
        cerrarLocked();
        return FAILURE_V(false, "'%s': respuesta inválida", consulta);
    }
    const size_t resto = n > 3 ? n - 3 : 0;
    std::vector<uint8_t> error;
    std::vector<uint8_t>* const destino = (ok && datos) ? datos : &error;
    destino->resize(resto);
    if (!leerTodo(gFd, destino->data(), resto)) {
        cerrarLocked();
        return FAILURE_V(false, "'%s': conexión perdida", consulta);
    }
    if (!ok) {
        if (motivo) motivo->assign(error.begin(), error.end());
        return FAILURE_V(false, "'%s': %.*s", consulta, int(std::min(resto, kMaxError)),
                         reinterpret_cast<const char*>(error.data()));
    }
    return true;
}

}  // namespace hw
}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
