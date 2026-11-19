# Surface Dual: qué lleva este paquete y de dónde sale lo demás

Este paquete solo contiene código propio de Surface Dual y componentes libres. No contiene Android, ni Google Play, ni
bibliotecas de Microsoft o Qualcomm: eso lo descarga el instalador de su fuente oficial o lo toma de su propio Windows.

| Componente | Origen | Licencia |
|---|---|---|
| `RutaA/fase4/Image-android` (kernel de Linux) | github.com/microsoft/WSL2-Linux-Kernel, etiqueta `linux-msft-wsl-6.18.58.1`, con la configuración y los cambios de `RutaA/fase4/kernel_*` y `kernel_android.sh` (incluidos) | GPL-2.0. La fuente completa es esa etiqueta pública más los archivos incluidos. |
| Mesa (libEGL, libGLESv2, libgallium_dri, driver VA) | gitlab.freedesktop.org/mesa/mesa 25.2.8 con parches propios | MIT |
| DirectX-Headers | github.com/microsoft/DirectX-Headers | MIT |
| libdrm | gitlab.freedesktop.org/mesa/drm | MIT |
| `bootconfig` | herramienta del kernel de Linux (tools/bootconfig) | GPL-2.0 |
| `avbtool.py` | AOSP | Apache-2.0 |
| HAL de cámara (derivado de goldfish) y stubs AIDL | AOSP | Apache-2.0 |
| `initrd_base.py` | derivado de github.com/DevanshSampat/android-emulator-windows-arm64 | Apache-2.0 |

Se descarga en su equipo, con su aceptación: imagen de sistema de Android con Google Play y NDK (Google, licencia del SDK de
Android: https://developer.android.com/studio/terms), Ubuntu 24.04 (Canonical), FFmpeg 7.1.1 (LGPL, ffmpeg.org) y libva
(MIT), y herramientas de plataforma (adb). Las bibliotecas D3D12 (Microsoft) y el controlador gráfico (Qualcomm) se usan
desde su propia instalación de Windows; no se redistribuyen.
