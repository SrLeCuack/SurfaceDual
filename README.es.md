# Surface Dual

[English](README.md) | **Español**

**Quita el teclado y tu Surface se convierte en una tableta Android. Pon el teclado y vuelves a Windows.**

Surface Dual ejecuta Android 17 con Google Play en una máquina virtual de Hyper-V en equipos Windows 11 con Snapdragon X.
Android usa de verdad la gráfica y el chip de vídeo del equipo, y en pausa sus procesadores quedan parados (0 % de CPU, medido).

> **Proyecto experimental.** Solo se ha probado en una Surface Pro 11 (Snapdragon X Elite), y el instalador todavía no se ha
> probado de principio a fin en otro equipo (sí sus autopruebas). Úsalo bajo tu propio riesgo. El instalador crea antes un punto
> de restauración de Windows e incluye un desinstalador que deshace lo que hizo.

## Qué hace

- **Teclado fuera = Android, teclado puesto = Windows.** Al volver a Windows, Android queda en pausa.
  Medido: reanudar Android 0,1 s, ponerlo en pausa 4,9 s, arranque en frío 33-36 s.
- **Gráficos por GPU** (la Adreno del Snapdragon, compartida con Windows) y **vídeo por hardware** dentro de Android.
- **Google Play**, con cuenta de Google y apps normales.
- **Modo lectura** para manga, manhwa y libros: un botón en los ajustes rápidos que deja la pantalla a 48 Hz con la página quieta,
  quita las animaciones y pone a dormir lo que no hace falta. Nunca usa escala de grises.
- **Sonido y micrófono, cámaras, giro de pantalla y sensores, carpetas de Windows dentro de Android y Chromecast.**
- **Interfaz de tableta:** tarjetas flotantes para avisos y ajustes rápidos, panel lateral de apps, ventanas flotantes, pantalla
  dividida y Circle to Search.
- **Con el teclado puesto, gestos del trackpad:** tres dedos hacia abajo muestran el escritorio de Windows y lo que suena en Android
  sigue sonando; tres dedos a los lados cambian entre Android y Windows.
- **Arranque vigilado:** si Android se cuelga al encender, se reinicia solo.
- **Versión de producción:** Android no depurable; solo tu propio PC puede conectarse a Android, con su clave.

## Batería (medido en una Surface Pro 11, todo el equipo)

| Tarea (brillo 30 %) | Android (Surface Dual) | Windows |
|---|---|---|
| Leer manga, una página cada 10 s (sin cargador, 10 min) | **2,37 W** (Mihon con Modo lectura) | 3,83 W (Edge) |
| Ver YouTube a pantalla completa (con cargador) | 11,28 W | 11,31 W (Edge) |

En esta prueba de lectura, Android gastó **un 38 % menos** que Edge en Windows. YouTube gasta lo mismo en los dos.

## Requisitos

- Equipo con **Snapdragon X** (gráfica Adreno) y **Windows 11**. Solo se ha probado en una Surface Pro 11.
- **16 GB de RAM** (Android usa 8 GB mientras está encendido) y **64 GB libres** en disco.
- Una cuenta de **administrador** de Windows e Internet: se descargan unos 2,2 GB de Android, más Ubuntu y herramientas.
- Hyper-V. En Windows 11 **Home** el instalador lo activa de forma **experimental**, y solo si escribes `SI` para confirmarlo.

## Instalación

1. Descarga el `.zip` de la última versión en [Releases](../../releases) y descomprímelo.
2. Doble clic en `instalador\Instalar.cmd`. Primero comprueba tu equipo y pide permiso antes de cambiar nada.
3. Si todo sale "bien", escribe `SI` para instalar y acepta el aviso de Windows (UAC).
4. Espera. El paso más largo es armar Android: cerca de una hora (estimado; depende de tu Internet). Al final, reinicia Windows una vez.
5. Quita el teclado (o abre "Android (Surface Dual)" en el menú Inicio), abre Play Store e inicia sesión con tu cuenta de Google.

El instalador está en español y se instala en `C:\SurfaceDual`. Si se corta, vuelve a abrir `Instalar.cmd` y sigue
donde se quedó.

**Desinstalar:** doble clic en `instalador\Desinstalar.cmd`. Deshace lo que hizo el instalador, con el estado de Windows que anotó antes de instalar.

## Límites conocidos

- En Ajustes > Acerca de, el modelo aparece como `sdk_gphone64_arm64`: es la imagen oficial del emulador de Google.
- YouTube llega como máximo a 1080p60. Netflix no funciona, porque el equipo no está certificado por Google.
- Las alarmas de Android no suenan mientras estás en Windows, porque Android está en pausa.
- Con los gestos del trackpad, Android no se pone en pausa hasta que vuelves a él.

## Cómo funciona (resumen)

Android corre dentro de una máquina virtual de Windows (Hyper-V), aparte de tus archivos. Por dentro: Hyper-V ejecuta la imagen oficial de Android de Google con un kernel de Linux propio (basado en el
de WSL de Microsoft). La gráfica se comparte con GPU-P y Mesa (D3D12). El Visor de Windows muestra la pantalla y envía el
toque, el lápiz y el teclado; varios ayudantes conectan el sonido, la cámara, los sensores y las carpetas por vsock.
La imagen de Android se descarga de Google en tu equipo, con tu aceptación: este repositorio no contiene nada de Google.

## Apoyar el proyecto

Si te resulta útil, puedes apoyarlo con el botón **Sponsor** de este repositorio. ¡Gracias!

## Licencia y avisos

El código propio de Surface Dual tiene licencia [MIT](LICENSE). Los componentes de terceros conservan su licencia; ver
[instalador/LICENCIAS.md](instalador/LICENCIAS.md) (el kernel de Linux es GPL-2.0 y su fuente está en `RutaA/fase4/kernel_*`).

Proyecto independiente, sin relación con Microsoft, Google ni Qualcomm. Surface y Windows son marcas de Microsoft; Android
y Google Play, de Google; Qualcomm, Snapdragon y Adreno, de Qualcomm Technologies, Inc.

La imagen de Android con Google Play la licencia Google con el Acuerdo de licencia del SDK de Android
(https://developer.android.com/studio/terms), que tú aceptas al instalar; cumplirlo es tu responsabilidad.
