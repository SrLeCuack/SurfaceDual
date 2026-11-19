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

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {
namespace hw {

// Canal con HvCamara.exe (Windows) por vsock 5007. El HAL escucha y el host se conecta, como pantalla y audio: así
// Windows no tiene que registrar un servicio de Hyper-V (pide administrador). Al conectarse, el host manda la lista de
// cámaras (mensaje = 8 dígitos hex con el largo + datos) y alListar la recibe. Después el HAL pregunta (texto ASCIZ) y el
// host contesta con un mensaje "ok", "ok:<datos>" o "ko:<error>" (el formato de qemud camera de goldfish).
void canalIniciar(std::function<void(const std::string&)> alListar);

// true si el host contestó "ok"; los datos van en *datos (como mucho maxDatos bytes). Si contestó "ko", el motivo va en
// *motivo (vacío si no hubo respuesta: canal sin conectar o cortado).
bool canalConsulta(const char* consulta, std::vector<uint8_t>* datos = nullptr, size_t maxDatos = 0,
                   std::string* motivo = nullptr);

// Autocomprobación del análisis de la respuesta (prueba_canal).
bool canalAnalizarCabecera(const char cab[8], size_t maxDatos, size_t* largo);

}  // namespace hw
}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
