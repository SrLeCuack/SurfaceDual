// Uso: HvSensores.exe <VmId>   (HvSensores.exe --listar: solo enseña los sensores). Compilar: ver compilar.sh (como HvCamara).
// Sensores reales de la Surface para Android (HAL RutaA/fase4/hal/sensores) por vsock 5009. Android escucha y este programa
// se conecta y manda la lista; Android pide "<clave> <periodo µs>" (0 = apagar) y solo entonces se enciende el sensor de
// Windows: sin apps de Android que los usen no hay ningún sensor encendido ni despertares. Con la VM en pausa el envío
// agota su espera, se corta la conexión y se apagan; al volver, el HAL pide otra vez lo activo.
// Datos: registros de 24 bytes {int handle, float x, y, z, long edad µs} ya en unidades y ejes de Android. Los ejes de
// Windows y de Android son los mismos (x derecha, y arriba, z hacia el usuario, sobre la orientación natural: apaisada en
// la Surface y en Android); cambian las unidades y el signo del acelerómetro (Windows da la gravedad en g hacia abajo,
// Android la fuerza de reacción en m/s²: tumbada boca arriba, Windows z = -1 g y Android z = +9,81).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using Windows.Devices.Sensors;

static class HvSensores
{
    const float G = 9.80665f, Rad = (float)(Math.PI / 180);
    static Accelerometer acel = Accelerometer.GetDefault();
    static Gyrometer giro = Gyrometer.GetDefault();
    static Magnetometer magnet = Magnetometer.GetDefault();
    static LightSensor luz = LightSensor.GetDefault();
    static SimpleOrientationSensor orient = SimpleOrientationSensor.GetDefault();
    static Socket android;
    static readonly object envio = new object();
    static readonly byte[] registro = new byte[24];
    static int tirados;

    static string Lista()
    {
        var sb = new StringBuilder();
        if (acel != null) sb.Append("acel\n");
        if (giro != null) sb.Append("giro\n");
        if (magnet != null) sb.Append("magnet\n");
        if (luz != null) sb.Append("luz\n");
        if (orient != null) sb.Append("orient\n");
        return sb.ToString();
    }

    // Edad de la lectura (reloj QPC, el mismo de Stopwatch) para que Android ponga la hora real de la medida.
    static long Edad(TimeSpan? pc)
    {
        if (!pc.HasValue) return 0;
        long ahora = (long)(Stopwatch.GetTimestamp() * (10000000.0 / Stopwatch.Frequency));
        return Math.Max(0, Math.Min(10000000, (ahora - pc.Value.Ticks) / 10));
    }

    // Con la VM en pausa el socket se llena: se tira la lectura en vez de bloquear el hilo de Windows.
    static unsafe void Enviar(int handle, float x, float y, float z, long edad)
    {
        lock (envio)
        {
            var s = android;
            if (s == null) return;
            fixed (byte* p = registro)   // sin reservar memoria por lectura (hasta cientos por segundo)
            {
                *(int*)p = handle; *(float*)(p + 4) = x; *(float*)(p + 8) = y; *(float*)(p + 12) = z; *(long*)(p + 16) = edad;
            }
            try { s.Send(registro); }
            catch (SocketException e) when (e.SocketErrorCode == SocketError.TimedOut)
            {
                // VM en pausa (socket lleno). Un envío cortado a medias desalinearía los registros: se cierra la conexión,
                // Servir apaga los sensores de Windows y, al reanudarse la VM, el HAL vuelve a pedir lo que tenga activo.
                tirados++;
                s.Close();
            }
            catch (SocketException) { } catch (ObjectDisposedException) { }
        }
    }

    static void Acel(Accelerometer s, AccelerometerReadingChangedEventArgs e)
    {
        var r = e.Reading;
        Enviar(1, (float)-r.AccelerationX * G, (float)-r.AccelerationY * G, (float)-r.AccelerationZ * G, Edad(r.PerformanceCount));
    }
    static void Giro(Gyrometer s, GyrometerReadingChangedEventArgs e)
    {
        var r = e.Reading;
        Enviar(2, (float)r.AngularVelocityX * Rad, (float)r.AngularVelocityY * Rad, (float)r.AngularVelocityZ * Rad, Edad(r.PerformanceCount));
    }
    static void Magnet(Magnetometer s, MagnetometerReadingChangedEventArgs e)
    {
        var r = e.Reading;
        Enviar(3, r.MagneticFieldX, r.MagneticFieldY, r.MagneticFieldZ, Edad(r.PerformanceCount));
    }
    static void Luz(LightSensor s, LightSensorReadingChangedEventArgs e)
    {
        var r = e.Reading;
        Enviar(4, r.IlluminanceInLux, 0, 0, Edad(r.PerformanceCount));
    }

    // Giro decidido por Windows: 0..3 = Surface.ROTATION_* de Android (antihorario). Boca arriba/abajo no cambia el giro.
    static void Orient(SimpleOrientationSensor s, SimpleOrientationSensorOrientationChangedEventArgs e) { EnviarOrient(e.Orientation); }
    static void EnviarOrient(SimpleOrientation o) { if ((int)o <= 3) Enviar(5, (int)o, 0, 0, 0); }

    // Pantalla de Windows apagada (B3): ningún sensor de Windows encendido aunque Android lo pida; al encenderse vuelve lo pedido.
    static readonly Dictionary<string, int> pedidos = new Dictionary<string, int>();
    static bool pantalla = true;
    static void Pedir(string clave, int us) { lock (pedidos) { pedidos[clave] = us; Aplicar(clave, pantalla ? us : 0); } }
    static void Pantalla(bool si)
    {
        lock (pedidos)
        {
            if (si == pantalla) return;
            pantalla = si;
            Console.WriteLine("pantalla " + (si ? "encendida" : "apagada"));
            foreach (var kv in pedidos) Aplicar(kv.Key, si ? kv.Value : 0);
        }
    }
    delegate uint Aviso(IntPtr contexto, uint tipo, IntPtr ajuste);
    [StructLayout(LayoutKind.Sequential)] struct Suscripcion { public Aviso Llamada; public IntPtr Contexto; }
    [DllImport("powrprof.dll")] static extern uint PowerSettingRegisterNotification(ref Guid ajuste, uint flags, ref Suscripcion destino, out IntPtr registro);
    static Suscripcion suscripcion;   // vive todo el proceso: Windows llama a su delegado
    static void VigilarPantalla()
    {
        var g = new Guid("6FE69556-704A-47A0-8F24-C28D936FDA47");   // GUID_CONSOLE_DISPLAY_STATE: 0 apagada, 1 encendida, 2 atenuada
        // PBT_POWERSETTINGCHANGE; POWERBROADCAST_SETTING = {GUID, DWORD tamaño, datos}: el estado va en el byte 20.
        suscripcion.Llamada = (c, tipo, p) => { if (tipo == 0x8013) Pantalla(Marshal.ReadInt32(p, 20) != 0); return 0; };
        IntPtr r;
        uint e = PowerSettingRegisterNotification(ref g, 2 /* DEVICE_NOTIFY_CALLBACK */, ref suscripcion, out r);
        if (e != 0) Console.Error.WriteLine("Sensores: sin aviso de pantalla (" + e + "): siguen como los pida Android");
    }

    // Enciende (periodo > 0) o apaga un sensor de Windows. Quitar el último manejador apaga el sensor de Windows.
    static void Aplicar(string clave, int us)
    {
        uint ms = (uint)Math.Max(1, us / 1000);
        Console.WriteLine(clave + (us > 0 ? " cada " + ms + " ms" : " apagado"));
        switch (clave)
        {
            case "acel":
                if (acel == null) return;
                acel.ReadingChanged -= Acel;
                if (us > 0) { acel.ReportInterval = Math.Max(acel.MinimumReportInterval, ms); acel.ReadingChanged += Acel; } else acel.ReportInterval = 0;
                break;
            case "giro":
                if (giro == null) return;
                giro.ReadingChanged -= Giro;
                if (us > 0) { giro.ReportInterval = Math.Max(giro.MinimumReportInterval, ms); giro.ReadingChanged += Giro; } else giro.ReportInterval = 0;
                break;
            case "magnet":
                if (magnet == null) return;
                magnet.ReadingChanged -= Magnet;
                if (us > 0) { magnet.ReportInterval = Math.Max(magnet.MinimumReportInterval, ms); magnet.ReadingChanged += Magnet; } else magnet.ReportInterval = 0;
                break;
            case "luz":
                if (luz == null) return;
                luz.ReadingChanged -= Luz;
                if (us > 0)
                {
                    luz.ReportInterval = Math.Max(luz.MinimumReportInterval, ms);
                    luz.ReadingChanged += Luz;
                    var r = luz.GetCurrentReading();   // sensor "al cambiar": Android necesita el valor actual al activarlo
                    if (r != null) Enviar(4, r.IlluminanceInLux, 0, 0, 0);
                }
                else luz.ReportInterval = 0;
                break;
            case "orient":
                if (orient == null) return;
                orient.OrientationChanged -= Orient;
                if (us > 0) { orient.OrientationChanged += Orient; EnviarOrient(orient.GetCurrentOrientation()); }   // valor actual al activarlo
                break;
        }
    }

    static void ApagarTodo() { lock (pedidos) { pedidos.Clear(); foreach (var c in new[] { "acel", "giro", "magnet", "luz", "orient" }) Aplicar(c, 0); } }

    // Orden: "<clave> <periodo µs>\n", como mucho 63 bytes (frontera de confianza: viene de la VM).
    static bool Orden(string l, out string clave, out int us)
    {
        clave = null; us = 0;
        var p = l.Split(' ');
        if (p.Length != 2 || !new[] { "acel", "giro", "magnet", "luz", "orient" }.Contains(p[0]) || !int.TryParse(p[1], out us) ||
            us < 0 || us > 10000000) return false;
        clave = p[0];
        return true;
    }

    static void Servir(Socket s)
    {
        s.SendTimeout = 3000;
        var lista = Encoding.ASCII.GetBytes(Lista());
        s.Send(Encoding.ASCII.GetBytes(lista.Length.ToString("x8")));
        s.Send(lista);
        s.SendTimeout = 100;   // con la VM en pausa el registro no cabe: como mucho 0,1 s retenido el hilo del sensor
        lock (envio) android = s;
        try
        {
            var b = new byte[64];
            for (int n = 0; ; )
            {
                // Espera sin despertares hasta la próxima orden (o el corte de la conexión).
                if (s.Receive(b, n, 1, SocketFlags.None) != 1) throw new EndOfStreamException("Android desconectado");
                if (b[n] != '\n') { if (++n == b.Length) throw new InvalidDataException("orden demasiado larga"); continue; }
                string clave; int us;
                if (!Orden(Encoding.ASCII.GetString(b, 0, n), out clave, out us)) throw new InvalidDataException("orden inválida");
                Pedir(clave, us);
                n = 0;
            }
        }
        finally
        {
            lock (envio) android = null;
            ApagarTodo();
            if (tirados > 0) { Console.WriteLine("lecturas tiradas (VM en pausa): " + tirados); tirados = 0; }
        }
    }

    static void SelfCheck()
    {
        string c; int us;
        if (!Orden("acel 66667", out c, out us) || c != "acel" || us != 66667 || Orden("acel -1", out c, out us) ||
            Orden("baro 1000", out c, out us) || Orden("acel 1 2", out c, out us) || Orden("luz", out c, out us))
            throw new Exception("Autocomprobación de órdenes fallida");
    }

    static int Main(string[] args)
    {
        SelfCheck();
        Console.Write(Lista());
        if (args.Length == 1 && args[0] == "--listar") return 0;
        Guid vm;
        if (args.Length != 1 || !Guid.TryParse(args[0], out vm)) { Console.Error.WriteLine("Uso: HvSensores.exe <VmId>"); return 1; }
        var servicio = new Guid(5009.ToString("x8") + "-facb-11e6-bd58-64006a7986d3");
        VigilarPantalla();
        for (int espera = 1000; ; espera = Math.Min(espera * 2, 5000))
        {
            try
            {
                using (var socket = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1))
                {
                    socket.Connect(new HvEndPoint(vm, servicio));
                    Console.WriteLine("Android conectado");
                    espera = 500;
                    Servir(socket);
                }
            }
            catch (SocketException e) when (e.SocketErrorCode == SocketError.ConnectionRefused || e.SocketErrorCode == SocketError.TimedOut) { }
            catch (Exception e) { Console.Error.WriteLine("Sensores: " + e.GetBaseException().Message); }
            // Mientras Android arranca o se reinicia el HAL: reintento de 1 a 5 s; con la VM en pausa, ninguno hasta reanudar.
            VmActiva.Reintentar(espera);
        }
    }
}
