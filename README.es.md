# Surface Dual

[English](README.md) | **Español**

**Quita el teclado y tu Surface se convierte en una tableta Android. Pon el teclado y vuelves a Windows.**

Surface Dual ejecuta Android 17 con Google Play en una máquina virtual de Hyper-V en equipos Windows 11 con chip Snapdragon X (solo ARM64).
Android usa de verdad la gráfica y el chip de vídeo del equipo, y en pausa sus procesadores quedan parados (0 % de CPU, medido).
Es una alternativa al Subsistema de Windows para Android (WSA), que Microsoft retiró, para equipos con Snapdragon X (Windows en ARM).

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
- **Gesto táctil con el teclado quitado:** mantén tres dedos quietos en la pantalla unos 0,7 s para cambiar entre Windows y
  Android, en los dos sentidos (idea y primera versión de BeShO-ShO). `HKCU\Software\SurfaceDual`: `GestoDedos` (3, o 0 para
  apagarlo) y `GestoMs` (700).
- **Arranque vigilado:** si Android se cuelga al encender, se reinicia solo.
- **Versión de producción:** Android no depurable; solo tu propio PC puede conectarse a Android, con su clave.

## Batería (medido en una Surface Pro 11, todo el equipo)

| Tarea (brillo 30 %) | Android (Surface Dual) | Windows |
|---|---|---|
| Leer manga, una página cada 10 s (sin cargador, 10 min) | **2,37 W** (Mihon con Modo lectura) | 3,83 W (Edge) |
| Ver YouTube a pantalla completa (con cargador) | 11,28 W | 11,31 W (Edge) |

En esta prueba de lectura, Android gastó **un 38 % menos** que Edge en Windows. YouTube gasta lo mismo en los dos.

## Requisitos

- Equipo **ARM64 con chip Qualcomm Snapdragon X** (gráfica Adreno) y **Windows 11**. Solo se ha probado en una Surface Pro 11.
  **No funciona en PCs con Intel o AMD (x86/x64):** el instalador lo comprueba primero y se detiene sin cambiar nada.
- **16 GB de RAM** (Android usa 4 GB mientras está encendido con 16 GB, 8 GB con 32 GB; `MemoriaVmGB` en `HKCU\Software\SurfaceDual` lo fija a mano) y **64 GB libres** en disco.
  **Con 16 GB la memoria va justa.** Algunos probadores con 16 GB tuvieron pantallas azules (error 0x139 en `dxgmms2.sys`, el
  administrador de memoria gráfica de Windows) usando Android con la memoria casi llena. Por eso, en equipos de menos de 24 GB,
  Surface Dual **congela tus programas de Windows y libera su memoria mientras Android está en pantalla** (navegadores, Claude,
  Codex, Office, Teams, Discord, Spotify…, aunque estén en la bandeja) y los descongela tal como estaban al volver a Windows. Si
  hay otras máquinas virtuales encendidas (el Cowork de Claude, WSL, Docker), te pide cerrarlas, porque no se pueden pausar sin
  riesgo. Desde la 1.0.18 Android además usa **4 GB en vez de 6** en estos equipos: un probador con una Surface de 16 GB
  (BeShO-ShO) vio que con 6 GB a Windows le quedaban unos 60 MB libres, y con 4 GB, dos días sin problemas. El precio: Android
  recarga algunas apps al volver a ellas. Si prefieres 6 GB, activa **Más memoria a Android** en el panel rápido de la bandeja
  (solo aparece en estos equipos) y cierra del todo tus programas de Windows mientras usas Android; se aplica la próxima vez
  que Android arranque. Para la experiencia más estable, cierra los programas pesados y otras máquinas virtuales antes de abrir
  Android. Con 32 GB nada de esto hace falta.
- Una cuenta de **administrador** de Windows e Internet: se descargan unos 2,2 GB de Android, más Ubuntu y herramientas.
- Hyper-V. En Windows 11 **Home** el instalador lo activa de forma **experimental**, y solo si escribes `SI` para confirmarlo.

## Instalación

1. Descarga el `.zip` de la última versión en [Releases](../../releases) y descomprímelo.
2. Doble clic en `instalador\Instalar.cmd` y elige el idioma: `1` = Español (`2` = inglés). Primero comprueba tu equipo
   y pide permiso antes de cambiar nada.
3. Si todo sale "bien", escribe `SI` para instalar y acepta el aviso de Windows (UAC).
4. Espera. El paso más largo es armar Android: cerca de una hora (estimado; depende de tu Internet). Al final, reinicia Windows una vez.
5. Quita el teclado (o abre "Android (Surface Dual)" en el menú Inicio), abre Play Store e inicia sesión con tu cuenta de Google.

**¿Ya lo tienes instalado? Actualizar es igual de fácil:** descarga la versión nueva, descomprímela y haz doble clic en
`instalador\Instalar.cmd` (o `instalador\Actualizar.cmd`). Detecta que Surface Dual ya está instalado y solo cambia los
programas y la pantalla de inicio de Android. Tus apps y datos de Android se quedan igual. Si la versión nueva cambia algo dentro de Android, lo apaga
una vez y lo rearma (5-10 minutos), conservando también tus apps y datos.

El instalador, Android y los programas de Surface Dual (icono de la bandeja, visor, pantalla de inicio, ajustes rápidos)
usan el idioma que elijas. Se instala en `C:\SurfaceDual`. Si se corta, vuelve a abrir `Instalar.cmd` y sigue
donde se quedó.

**Desinstalar:** doble clic en `instalador\Desinstalar.cmd`. Deshace lo que hizo el instalador, con el estado de Windows que anotó antes de instalar.

## Si algo falla

Doble clic en `instalador\Diagnostico.cmd`. Solo lee (no cambia nada) y crea un `.zip` en el Escritorio con lo necesario
para encontrar el problema: versión de Windows, RAM, controlador de la gráfica, registros de pantallas azules y los
registros de Surface Dual. Quita tu nombre de usuario, el del equipo, el número de serie, los nombres de redes Wi-Fi,
correos e IP. Si hubo una pantalla azul, pregunta antes de incluir el archivo del error; ese mándalo en privado. Abre un
Issue en GitHub (o manda un mensaje directo) con el `.zip`.

## Límites conocidos

- En Ajustes > Acerca de, el modelo aparece como `sdk_gphone64_arm64`: es la imagen oficial del emulador de Google.
- YouTube llega como máximo a 1080p60. Netflix no funciona ([por qué](#por-qué-no-funciona-netflix)).
- Las alarmas de Android no suenan mientras estás en Windows, porque Android está en pausa.
- Con los gestos del trackpad, Android no se pone en pausa hasta que vuelves a él.
- El Bluetooth está apagado dentro de Android (Windows se queda con la radio; el sonido de Android ya sale por Windows). Por
  eso tres pantallas de Ajustes se cierran solas: Dispositivos conectados, Sonido y vibración, y Ayuda y comentarios
  (lo reportó BeShO-ShO). Todas las demás pantallas de Ajustes funcionan.
- **Google Meet: úsalo en Chrome dentro de Android**, donde el sonido y el micrófono funcionan. En la app de Meet las
  llamadas conectan pero sin sonido: la app no arranca su audio en este equipo. En la 1.0.18 se añadió el auricular que
  pedía, pero eso solo no basta (lo reportó BeShO-ShO).

### Por qué no funciona Netflix

Netflix se abre y se cierra sola a los pocos segundos. Lo investigamos: al arrancar, la app de Netflix crea una clave de
seguridad y comprueba quién la certifica. En un teléfono o tableta certificados esa clave sale de un chip de seguridad
aprobado por Google; en Surface Dual la crea un programa dentro de la máquina virtual, así que Netflix decide en el propio
equipo cerrarse. La comprobación de Google (Play Integrity) tampoco pasa, ni en su nivel básico: desde Android 13 ese nivel
también exige claves certificadas por Google.

Las únicas formas de saltarlo serían falsificar la identidad de otro equipo o usar claves filtradas. Este proyecto no lo
hace ni lo hará. **Usa Netflix en Windows** (pon el teclado), donde funciona normal. Otras apps que exigen un equipo
certificado por Google (algunas de bancos, por ejemplo) pueden no funcionar tampoco.

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
