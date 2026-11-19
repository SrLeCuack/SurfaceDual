// Uso: HvCamara.exe <VmId>   (HvCamara.exe --listar: solo enseña las cámaras). Compilar: ver compilar.sh (como HvAudio).
// Cámaras de la Surface para Android (HAL RutaA/fase4/hal/camara) por vsock 5007. Android escucha y este programa se
// conecta, manda la lista de cámaras y contesta consultas: "start name=N dim=WxH", "frame name=N", "stop name=N".
// Respuesta: 8 dígitos hex con el largo + "ok", "ok:<NV12 W*H*3/2>" o "ko:<motivo>". La cámara de Windows solo está
// abierta mientras Android la usa: "stop", conexión cortada o 1,5 s sin pedir cuadros (VM en pausa) la sueltan (luz apagada).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Graphics.Imaging;
using Windows.Devices.Enumeration;

static class HvCamara
{
    [ComImport, Guid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    unsafe interface IMemoryBufferByteAccess { void GetBuffer(out byte* buffer, out uint capacity); }

    class Camara
    {
        public string Nombre;
        public bool Trasera;
        public MediaFrameSourceGroup Grupo;
        public MediaFrameSourceInfo Fuente;
        public List<int[]> Tamanos = new List<int[]>();
    }

    // shortcut: tope de 2560x1440 (3,7 MP): a 30 cuadros/s ya son 166 MB/s por vsock; 4K serían 373 MB/s. Subirlo si se
    // piden fotos más grandes (y entonces pedir la resolución completa solo en la foto).
    const int MaxPixeles = 2560 * 1440;
    static List<Camara> camaras;
    // Cámara que Android tiene abierta y su captura en Windows (null = suelta).
    static Camara activa;
    static int ancho, alto;
    static MediaCapture captura;
    static MediaFrameReader lector;
    static readonly AutoResetEvent nuevo = new AutoResetEvent(false);

    static List<Camara> Listar()
    {
        var r = new List<Camara>();
        foreach (var g in MediaFrameSourceGroup.FindAllAsync().AsTask().Result)
        {
            // Grupos de una sola cámara a color (sin IR: el "Sensor Group" encendería también la cámara de Windows Hello).
            if (g.SourceInfos.Any(s => s.SourceKind != MediaFrameSourceKind.Color && s.SourceKind != MediaFrameSourceKind.Image)) continue;
            var fuentes = g.SourceInfos.Where(s => s.SourceKind == MediaFrameSourceKind.Color && s.DeviceInformation != null &&
                                                   s.DeviceInformation.EnclosureLocation != null).ToList();
            var f = fuentes.FirstOrDefault(s => s.MediaStreamType == MediaStreamType.VideoPreview) ?? fuentes.FirstOrDefault();
            if (f == null) continue;
            var panel = f.DeviceInformation.EnclosureLocation.Panel;
            if (panel != Panel.Back && panel != Panel.Front) continue;   // shortcut: webcams USB (panel desconocido) no salen
            bool trasera = panel == Panel.Back;
            if (r.Any(c => c.Trasera == trasera)) continue;
            r.Add(new Camara { Nombre = trasera ? "trasera" : "frontal", Trasera = trasera, Grupo = g, Fuente = f });
        }
        // Formatos: hay que abrir la cámara un momento (sin capturar: la luz no se enciende).
        foreach (var c in r)
        {
            using (var mc = new MediaCapture())
            {
                try
                {
                    mc.InitializeAsync(Ajustes(c, MediaCaptureSharingMode.SharedReadOnly)).AsTask().Wait();
                    foreach (var fmt in mc.FrameSources[c.Fuente.Id].SupportedFormats)
                    {
                        int w = (int)fmt.VideoFormat.Width, h = (int)fmt.VideoFormat.Height;
                        if (fmt.Subtype != "NV12" || w * h > MaxPixeles || (w | h) % 2 != 0 || fmt.FrameRate.Denominator == 0 ||
                            fmt.FrameRate.Numerator / fmt.FrameRate.Denominator < 15 || c.Tamanos.Any(t => t[0] == w && t[1] == h)) continue;
                        c.Tamanos.Add(new[] { w, h });
                    }
                }
                catch (Exception e) { Console.Error.WriteLine("cámara " + c.Nombre + ": " + e.GetBaseException().Message); }
            }
        }
        r.RemoveAll(c => c.Tamanos.Count == 0);
        r.Sort((a, b) => b.Trasera.CompareTo(a.Trasera));   // la trasera primero: es la "0" de Android
        return r;
    }

    static MediaCaptureInitializationSettings Ajustes(Camara c, MediaCaptureSharingMode modo)
    {
        return new MediaCaptureInitializationSettings
        {
            SourceGroup = c.Grupo, SharingMode = modo, MemoryPreference = MediaCaptureMemoryPreference.Cpu,
            StreamingCaptureMode = StreamingCaptureMode.Video
        };
    }

    static string Lista()
    {
        var sb = new StringBuilder();
        foreach (var c in camaras)
            sb.Append("name=" + c.Nombre + " dir=" + (c.Trasera ? "back" : "front") + " framedims=" +
                      string.Join(",", c.Tamanos.Select(t => t[0] + "x" + t[1])) + " sensor_orientation=0\n");
        return sb.ToString();
    }

    // Abre la captura de la cámara activa al tamaño pedido. Devuelve null o el motivo del fallo.
    static string Abrir()
    {
        var c = activa;
        try
        {
            captura = new MediaCapture();
            captura.InitializeAsync(Ajustes(c, MediaCaptureSharingMode.ExclusiveControl)).AsTask().Wait();
            var fuente = captura.FrameSources[c.Fuente.Id];
            var fmt = fuente.SupportedFormats.FirstOrDefault(f => f.Subtype == "NV12" && f.VideoFormat.Width == ancho && f.VideoFormat.Height == alto);
            if (fmt == null) { Cerrar(); return "formato"; }
            fuente.SetFormatAsync(fmt).AsTask().Wait();
            lector = captura.CreateFrameReaderAsync(fuente, "NV12").AsTask().Result;
            lector.AcquisitionMode = MediaFrameReaderAcquisitionMode.Realtime;
            lector.FrameArrived += (s, e) => nuevo.Set();
            nuevo.Reset();
            var st = lector.StartAsync().AsTask().Result;
            if (st != MediaFrameReaderStartStatus.Success) { Cerrar(); return st.ToString(); }
            Console.WriteLine("cámara " + c.Nombre + " abierta: " + ancho + "x" + alto);
            return null;
        }
        catch (Exception e)
        {
            Cerrar();
            var b = e.GetBaseException();
            // Privacidad de Windows: "Permitir que las aplicaciones de escritorio accedan a la cámara" desactivado.
            if (b is UnauthorizedAccessException) return "Windows no da permiso de camara a las apps de escritorio";
            return b.Message.Length > 200 ? b.Message.Substring(0, 200) : b.Message;
        }
    }

    static void Cerrar()
    {
        if (lector != null) { try { lector.StopAsync().AsTask().Wait(); } catch { } lector.Dispose(); lector = null; }
        if (captura != null) { captura.Dispose(); captura = null; Console.WriteLine("cámara suelta"); }
    }

    [DllImport("ws2_32.dll", SetLastError = true)] static extern unsafe int send(IntPtr s, byte* buf, int len, int flags);

    // Envía directamente desde la memoria del cuadro de Windows (sin copiarlo a un byte[]: ~3–5 MB por cuadro).
    static unsafe void Enviar(Socket s, byte* p, long n)
    {
        while (n > 0)
        {
            int r = send(s.Handle, p, (int)Math.Min(n, 1 << 20), 0);
            if (r <= 0) throw new SocketException(Marshal.GetLastWin32Error());
            p += r; n -= r;
        }
    }

    // Espera un cuadro posterior al último enviado y lo manda: "ok:" + {ancho, alto, edad en µs desde que Windows lo
    // capturó} + Y y UV compactos (sin relleno de fila). Devuelve false sin enviar nada si no hay cuadro a tiempo.
    static unsafe bool Cuadro(Socket s)
    {
        // Como mucho 0,5 s: el HAL tiene 1 s para vaciar sus búferes al cerrar (flush) y aborta si se pasa.
        for (var fin = DateTime.UtcNow.AddMilliseconds(500); ; )
        {
            int queda = (int)(fin - DateTime.UtcNow).TotalMilliseconds;
            if (queda <= 0 || !nuevo.WaitOne(queda)) return false;
            using (var f = lector.TryAcquireLatestFrame())
            {
                var bmp = f == null || f.VideoMediaFrame == null ? null : f.VideoMediaFrame.SoftwareBitmap;
                if (bmp == null || bmp.BitmapPixelFormat != BitmapPixelFormat.Nv12 || bmp.PixelWidth != ancho || bmp.PixelHeight != alto) continue;
                using (bmp)
                using (var buf = bmp.LockBuffer(BitmapBufferAccessMode.Read))
                using (var r = buf.CreateReference())
                {
                    byte* p; uint cap;
                    ((IMemoryBufferByteAccess)r).GetBuffer(out p, out cap);
                    var y = buf.GetPlaneDescription(0); var uv = buf.GetPlaneDescription(1);
                    if ((long)y.StartIndex + (long)y.Stride * alto > cap || (long)uv.StartIndex + (long)uv.Stride * (alto / 2) > cap ||
                        y.Stride < ancho || uv.Stride < ancho) return false;
                    // SystemRelativeTime usa el reloj QPC, el mismo que Stopwatch.
                    long ahora = (long)(Stopwatch.GetTimestamp() * (10000000.0 / Stopwatch.Frequency));
                    long edad = f.SystemRelativeTime.HasValue ? Math.Max(0, (ahora - f.SystemRelativeTime.Value.Ticks) / 10) : 0;
                    var cab = new byte[3 + 16];
                    cab[0] = (byte)'o'; cab[1] = (byte)'k'; cab[2] = (byte)':';
                    BitConverter.GetBytes(ancho).CopyTo(cab, 3); BitConverter.GetBytes(alto).CopyTo(cab, 7); BitConverter.GetBytes(edad).CopyTo(cab, 11);
                    Enviar(s, Encoding.ASCII.GetBytes((cab.Length + ancho * alto * 3 / 2).ToString("x8")));
                    Enviar(s, cab);
                    if (y.Stride == ancho) Enviar(s, p + y.StartIndex, (long)ancho * alto);
                    else for (int i = 0; i < alto; i++) Enviar(s, p + y.StartIndex + (long)y.Stride * i, ancho);
                    if (uv.Stride == ancho) Enviar(s, p + uv.StartIndex, (long)ancho * alto / 2);
                    else for (int i = 0; i < alto / 2; i++) Enviar(s, p + uv.StartIndex + (long)uv.Stride * i, ancho);
                    return true;
                }
            }
        }
    }

    static void Enviar(Socket s, byte[] b)
    {
        for (int hecho = 0; hecho < b.Length; ) hecho += s.Send(b, hecho, b.Length - hecho, SocketFlags.None);
    }

    static void Responder(Socket s, string texto, byte[] datos = null)
    {
        var t = Encoding.ASCII.GetBytes(texto);
        int n = t.Length + (datos != null ? datos.Length : 0);
        Enviar(s, Encoding.ASCII.GetBytes(n.ToString("x8")));
        Enviar(s, t);
        if (datos != null) Enviar(s, datos);
    }

    // Consulta: texto ASCII terminado en 0, como mucho 255 bytes (frontera de confianza: viene de la VM).
    static string Leer(Socket s)
    {
        var b = new byte[256];
        for (int n = 0; ; n++)
        {
            if (n == b.Length) throw new InvalidDataException("consulta demasiado larga");
            if (s.Receive(b, n, 1, SocketFlags.None) != 1) throw new EndOfStreamException("Android desconectado");
            if (b[n] == 0) return Encoding.ASCII.GetString(b, 0, n);
            if (b[n] < 0x20 || b[n] > 0x7e) throw new InvalidDataException("consulta no imprimible");
        }
    }

    static string Valor(string q, string clave)
    {
        foreach (var p in q.Split(' ')) if (p.StartsWith(clave + "=")) return p.Substring(clave.Length + 1);
        return null;
    }

    static void Atender(Socket s, string q)
    {
        var verbo = q.Split(' ')[0];
        var c = camaras.FirstOrDefault(x => x.Nombre == Valor(q, "name"));
        if (c == null) { Responder(s, "ko:camara desconocida"); return; }
        if (verbo == "start")
        {
            var d = (Valor(q, "dim") ?? "").Split('x');
            int w, h;
            if (d.Length != 2 || !int.TryParse(d[0], out w) || !int.TryParse(d[1], out h) || !c.Tamanos.Any(t => t[0] == w && t[1] == h))
            { Responder(s, "ko:tamano no admitido"); return; }
            Cerrar();
            activa = c; ancho = w; alto = h;
            var error = Abrir();
            if (error != null) activa = null;
            Responder(s, error == null ? "ok" : "ko:" + error);
        }
        else if (verbo == "frame")
        {
            // "no abierta": HvCamara se reinició y el HAL vuelve a pedirla; "ocupada": la tiene la otra cámara de Android.
            if (activa != c) { Responder(s, activa == null ? "ko:no abierta" : "ko:ocupada"); return; }
            string error = lector == null ? Abrir() : null;   // se soltó por inactividad (VM en pausa): reabrir
            if (error == null && !Cuadro(s)) error = "sin cuadros";
            if (error != null) Responder(s, "ko:" + error);
        }
        else if (verbo == "stop")
        {
            if (activa == c) { Cerrar(); activa = null; }
            Responder(s, "ok");
        }
        else Responder(s, "ko:consulta desconocida");
    }

    static void Servir(Socket s)
    {
        s.SendTimeout = 5000;
        s.ReceiveTimeout = 5000;   // una consulta a medias (VM pausada al enviarla) no deja la cámara abierta para siempre
        var lista = Encoding.ASCII.GetBytes(Lista());
        Enviar(s, Encoding.ASCII.GetBytes(lista.Length.ToString("x8")));
        Enviar(s, lista);
        try
        {
            for (;;)
            {
                // Sin cámara abierta se espera sin despertares. Abierta: 1,5 s sin consultas = Android en pausa o colgado;
                // se suelta (luz y batería) y el siguiente "frame" la reabre.
                if (!s.Poll(lector != null ? 1500000 : camaras.Count == 0 ? 30000000 : -1, SelectMode.SelectRead))
                {
                    if (lector != null) { Cerrar(); continue; }
                    // Sin cámaras (p. ej. sin permiso de Windows todavía): mirar cada 30 s; si aparecen, reconectar para
                    // que Android reciba la lista nueva.
                    camaras = Listar();
                    if (camaras.Count > 0) return;
                    continue;
                }
                Atender(s, Leer(s));
            }
        }
        finally { Cerrar(); activa = null; }
    }

    static void SelfCheck()
    {
        if (Valor("start name=trasera dim=1920x1080", "dim") != "1920x1080" || Valor("frame name=frontal", "name") != "frontal" ||
            Valor("frame name=frontal", "dim") != null || Valor("start xname=a", "name") != null)
            throw new Exception("Autocomprobación de consultas fallida");
    }

    static int Main(string[] args)
    {
        SelfCheck();
        camaras = Listar();
        Console.Write(Lista());
        if (args.Length == 1 && args[0] == "--listar") return 0;
        Guid vm;
        if (args.Length != 1 || !Guid.TryParse(args[0], out vm)) { Console.Error.WriteLine("Uso: HvCamara.exe <VmId>"); return 1; }
        var servicio = new Guid(5007.ToString("x8") + "-facb-11e6-bd58-64006a7986d3");
        for (int espera = 1000; ; espera = Math.Min(espera * 2, 5000))
        {
            try
            {
                using (var socket = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1))
                {
                    socket.Connect(new HvEndPoint(vm, servicio));
                    Console.WriteLine("Android conectado");
                    espera = 500;
                    if (camaras.Count == 0) camaras = Listar();   // p. ej. se dio el permiso de cámara después
                    Servir(socket);
                }
            }
            catch (SocketException e) when (e.SocketErrorCode == SocketError.ConnectionRefused || e.SocketErrorCode == SocketError.TimedOut) { }
            catch (Exception e) { Console.Error.WriteLine("Cámara: " + e.GetBaseException().Message); }
            // Mientras Android arranca o se reinicia el HAL: reintento de 1 a 5 s; con la VM en pausa, ninguno hasta reanudar.
            VmActiva.Reintentar(espera);
        }
    }
}
