using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Management;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Windows.Devices.Radios;

public static class Enlace
{
    public static readonly object Cerrojo = new object();
    static bool activo;
    public static bool Activo
    {
        get { lock (Cerrojo) return activo; }
        set { lock (Cerrojo) activo = value; }
    }
    static BateriaCompartida bateria;
    static PortapapelesCompartido portapapeles;

    // En el hilo de la ventana (el portapapeles exige STA). El espejo del nombre del Wi-Fi no aplica (Ethernet en la VM).
    public static void Iniciar(Func<string> ip)
    {
        ControlesCompartidos.RestaurarAhorro();   // si el Visor murió con el umbral de ahorro cambiado
        Application.ApplicationExit += (s, e) => ControlesCompartidos.RestaurarAhorro();
        portapapeles = new PortapapelesCompartido();
        GiroCompartido.Iniciar();
        ControlesCompartidos.Iniciar(ip);
        bateria = new BateriaCompartida();
    }

    public static void Mostrado(bool si)
    {
        lock (Cerrojo) activo = si;
        GiroCompartido.Mostrado(si);
        ControlesCompartidos.Ubicacion(null);   // la ubicación de Windows solo corre con Android delante
        if (si)
        {
            ControlesCompartidos.Republicar();
            ControlesCompartidos.Enviar("despierta", "1");   // vuelve de la pausa: el agente corta las conexiones de push muertas
        }
        else ControlesCompartidos.RestaurarAhorro();   // el ahorro que pidió Android no sigue en modo Windows
    }
}

// Portapapeles compartido (punto 7), solo texto, por el canal del agente: "clip <base64 UTF-8>" (tope Tope). Sin sondeo:
// WM_CLIPBOARDUPDATE. Sin ecos: lo que llega de Android queda como último "clip" y ControlesCompartidos no lo reenvía.
class PortapapelesCompartido : NativeWindow
{
    [DllImport("user32.dll")] static extern bool AddClipboardFormatListener(IntPtr h);
    public const int Tope = 256 * 1024;   // en base64 (~192 KB de texto): más grande no se comparte
    static PortapapelesCompartido yo;
    readonly SynchronizationContext ui = SynchronizationContext.Current;

    public PortapapelesCompartido()
    {
        CreateHandle(new CreateParams());
        AddClipboardFormatListener(Handle);
        yo = this;
    }

    protected override void WndProc(ref Message m)
    {
        base.WndProc(ref m);
        if (m.Msg == 0x031D) Leer();
    }

    void Leer()
    {
        if (!Enlace.Activo) return;   // con Windows delante no se lee nada: al mostrar Android se publica el actual
        string t;
        try { t = Clipboard.ContainsText() ? Clipboard.GetText() : null; }
        catch (ExternalException) { return; }
        if (string.IsNullOrEmpty(t)) return;   // imágenes o vacío: Android conserva el suyo
        string b = Convert.ToBase64String(Encoding.UTF8.GetBytes(t));
        if (b.Length <= Tope) ControlesCompartidos.Enviar("clip", b); else Trace.WriteLine("clip: demasiado grande");
    }

    public static void Publicar() { var y = yo; if (y != null) y.ui.Post(x => y.Leer(), null); }

    public static void Poner(string t)
    {
        var y = yo;
        if (y != null) y.ui.Post(x => { try { if (t.Length > 0) Clipboard.SetText(t); else Clipboard.Clear(); } catch (Exception e) { Trace.WriteLine("clip: " + e.Message); } }, null);
    }
}

// Giro (punto 12): la orientación física que da Windows gira Android ("rot 0|90|180|270", grados en sentido antihorario,
// igual que Surface.ROTATION_* de Android). Con Android delante Windows no gira su pantalla: el panel queda apaisado
// (2880x1920) y Android gira su contenido. Sin sondeo: aviso del sensor. Respeta el bloqueo de giro de Windows.
static class GiroCompartido
{
    [DllImport("user32.dll")] static extern bool SetDisplayAutoRotationPreferences(int o);
    [DllImport("user32.dll")] static extern bool GetAutoRotationState(out int estado);
    // E22 (18/11): la preferencia solo rige con el Visor en primer plano (y Microsoft no la garantiza): si otra ventana tiene
    // el foco, Windows gira su escritorio y se ve antes que Android. Con Android a la vista se echa el "Bloqueo de giro" de
    // Windows y se quita al salir. shortcut: SetAutoRotation sin documentar (user32 ordinal 2507, el del interruptor de
    // Ajustes rápidos); si una versión de Windows lo quita, falla y queda solo la preferencia.
    [DllImport("user32.dll", EntryPoint = "#2507")] static extern bool SetAutoRotation(bool activar);
    const string Clave = @"HKEY_CURRENT_USER\Software\SurfaceDual";
    static bool bloqueoUsuario;   // el bloqueo que ya tenía el usuario: Android tampoco gira (como antes)
    static Windows.Devices.Sensors.SimpleOrientationSensor sensor;

    public static void Iniciar()
    {
        Soltar();   // si el Visor murió con el bloqueo echado
        Application.ApplicationExit += (s, e) => Soltar();
        try { sensor = Windows.Devices.Sensors.SimpleOrientationSensor.GetDefault(); }
        catch (Exception e) { Trace.WriteLine("giro: " + e.Message); }
    }

    static bool Nuestro() { try { return (Microsoft.Win32.Registry.GetValue(Clave, "GiroBloqueado", 0) as int?) == 1; } catch (Exception) { return false; } }
    static bool Bloqueado() { int e; return GetAutoRotationState(out e) && (e & 1 /* AR_DISABLED */) != 0; }
    internal static void Soltar()
    {
        if (!Nuestro()) return;
        try { if (SetAutoRotation(true)) Microsoft.Win32.Registry.SetValue(Clave, "GiroBloqueado", 0); }   // si falla, se reintenta al salir o al arrancar
        catch (Exception e) { Trace.WriteLine("giro: " + e.Message); }
    }

    // En el hilo de la ventana. El aviso del sensor solo con Android delante (con Windows delante nadie lo necesita).
    public static void Mostrado(bool si)
    {
        SetDisplayAutoRotationPreferences(si ? 1 /* LANDSCAPE */ : 0 /* NONE */);
        if (si) Bloquear(); else Soltar();
        if (sensor == null) return;
        sensor.OrientationChanged -= Cambio;   // nunca dos veces
        if (si) sensor.OrientationChanged += Cambio;
    }

    static void Bloquear()
    {
        if (Nuestro()) return;   // ya echado (Mostrado se repite): bloqueoUsuario sigue siendo el de antes
        try
        {
            bloqueoUsuario = Bloqueado();
            // Solo con Windows ya apaisado: el bloqueo congela la orientación actual (en vertical, Android saldría en una
            // franja). shortcut: monitor principal = panel de la Surface; con pantalla externa como principal, revisar.
            if (bloqueoUsuario || SystemInformation.PrimaryMonitorSize.Width <= SystemInformation.PrimaryMonitorSize.Height) return;
            Microsoft.Win32.Registry.SetValue(Clave, "GiroBloqueado", 1);   // antes de bloquear: si el proceso muere, el próximo arranque lo suelta
            if (!SetAutoRotation(false)) Microsoft.Win32.Registry.SetValue(Clave, "GiroBloqueado", 0);   // falló: queda solo la preferencia
        }
        catch (Exception e) { Trace.WriteLine("giro: " + e.Message); }
    }

    static void Cambio(Windows.Devices.Sensors.SimpleOrientationSensor s, Windows.Devices.Sensors.SimpleOrientationSensorOrientationChangedEventArgs e) { Enviar(e.Orientation); }

    public static void Publicar() { var s = sensor; if (s != null) Enviar(s.GetCurrentOrientation()); }

    static void Enviar(Windows.Devices.Sensors.SimpleOrientation o)
    {
        if ((int)o > 3 || bloqueoUsuario) return;   // boca arriba/abajo: el último. El bloqueo propio (Mostrado) no cuenta
        ControlesCompartidos.Enviar("rot", ((int)o * 90).ToString());
    }
}

// Batería de Windows en la de Android ("bat <porcentaje>,<red on|off>,<estado>" por el canal del agente). Sin sondeo:
// WM_POWERBROADCAST con el aviso de porcentaje (GUID_BATTERY_PERCENTAGE_REMAINING).
class BateriaCompartida : NativeWindow
{
    [DllImport("user32.dll")] static extern IntPtr RegisterPowerSettingNotification(IntPtr h, ref Guid g, int flags);
    static Guid porcentaje = new Guid("A7AD8041-B45A-4CAE-87A3-EECBB468A9E1");

    public BateriaCompartida()
    {
        CreateHandle(new CreateParams());
        RegisterPowerSettingNotification(Handle, ref porcentaje, 0);
    }

    protected override void WndProc(ref Message m)
    {
        base.WndProc(ref m);
        if (m.Msg == 0x0218) ControlesCompartidos.Enviar("bat", Estado());   // solo encola; sin cambios no se manda (sin ecos)
    }

    public static string Estado()
    {
        var p = SystemInformation.PowerStatus;
        int pct = p.BatteryLifePercent > 1 ? 100 : (int)Math.Round(p.BatteryLifePercent * 100);
        bool red = p.PowerLineStatus == PowerLineStatus.Online;
        string e = (p.BatteryChargeStatus & BatteryChargeStatus.Charging) != 0 ? "charging"
                 : red ? (pct >= 100 ? "full" : "not-charging") : "discharging";
        return pct + "," + (red ? "on" : "off") + "," + e;
    }
}

static class ControlesCompartidos
{
    public static bool simularWifi;
    static TcpClient cliente;
    static readonly Dictionary<string, string> ultimo = new Dictionary<string, string>();
    static readonly System.Collections.Specialized.OrderedDictionary pendientes = new System.Collections.Specialized.OrderedDictionary();
    static readonly AutoResetEvent hayPendientes = new AutoResetEvent(false);
    static readonly object cerrojo = new object();
    static Guid propio = Guid.NewGuid();
    static int briLocal = Environment.TickCount - 100000;
    static IMMDeviceEnumerator enumerador;
    static IAudioEndpointVolume vol;
    static readonly AvisoVolumen avisoVol = new AvisoVolumen();
    static readonly AvisoDispositivo avisoDisp = new AvisoDispositivo();
    static ManagementEventWatcher brillo;
    static IReadOnlyList<Radio> radios = new Radio[0];
    static System.Threading.Tasks.Task radioPendiente;
    static readonly object radioCerrojo = new object();

    public static void Iniciar(Func<string> obtenerIp)
    {
        new Thread(() =>
        {
            try { enumerador = (IMMDeviceEnumerator)new MMDeviceEnumerator(); enumerador.RegisterEndpointNotificationCallback(avisoDisp); Altavoz(); }
            catch (Exception e) { Trace.WriteLine("volumen: " + e.Message); }
            try
            {
                brillo = new ManagementEventWatcher(@"root\wmi", "SELECT * FROM WmiMonitorBrightnessEvent");
                brillo.EventArrived += (s, e) => { if (Environment.TickCount - briLocal > 1000) Enviar("bri", Convert.ToString(e.NewEvent["Brightness"])); };
                brillo.Start();
            }
            catch (Exception e) { Trace.WriteLine("brillo: " + e.Message); }
            try
            {
                if (Radio.RequestAccessAsync().AsTask().Result == RadioAccessStatus.Allowed) radios = Radio.GetRadiosAsync().AsTask().Result;
                foreach (var r in radios) r.StateChanged += (x, o) => EnviarRadio(x);
            }
            catch (Exception e) { Trace.WriteLine("radios: " + e.Message); }
            new Thread(Escritor) { IsBackground = true }.Start();
            // Ahorro de energía de Windows -> Android (pedido del usuario 16/11: compartido). Sin sondeo: aviso de WinRT.
            try { Windows.System.Power.PowerManager.EnergySaverStatusChanged += (s, e) => EnviarAhorro(); }
            catch (Exception e) { Trace.WriteLine("ahorro: " + e.Message); }
            Microsoft.Win32.SystemEvents.TimeChanged += (s, e) => EnviarRegion();
            Microsoft.Win32.SystemEvents.UserPreferenceChanged += (s, e) => { if (e.Category == Microsoft.Win32.UserPreferenceCategory.Locale) EnviarRegion(); };

            while (true)
            {
                try
                {
                    string ip = obtenerIp != null ? obtenerIp() : null;
                    if (!string.IsNullOrEmpty(ip))
                    {
                        var c = new TcpClient();
                        c.Connect(ip, 4449);
                        c.SendTimeout = 500;
                        lock (cerrojo) { if (cliente != null) cliente.Close(); cliente = c; ultimo.Clear(); pendientes.Clear(); }
                        Republicar();
                        Leer(c);
                        if (Lectura) { Lectura = false; var a = AlLectura; if (a != null) a(); }   // sin sesión, el modo lectura no sigue en Windows
                        Ubicacion(false);   // sin sesión no hay quien pida la ubicación: el agente lo repite al reconectar
                        RestaurarAhorro();
                    }
                }
                catch (Exception e) { Trace.WriteLine("enlace: " + e.Message); }
                VisorVM.Reintentar(5000);   // con la VM en pausa, ni WMI ni conexión hasta reanudar
            }
        }) { IsBackground = true }.Start();
    }

    static void Leer(TcpClient c)
    {
        try
        {
            var s = new BufferedStream(c.GetStream());   // líneas "clip" largas: sin una llamada al sistema por byte
            var l = new StringBuilder();
            for (int b; (b = s.ReadByte()) >= 0; )
                if (b == '\n') { Aplicar(l.ToString()); l.Clear(); }
                else if (l.Length >= 64 && (l.Length >= PortapapelesCompartido.Tope + 5 || l.Length == 64 && l.ToString(0, 5) != "clip ")) break;   // solo "clip" pasa de 64
                else l.Append((char)b);
        }
        catch (Exception) { }
        lock (cerrojo) if (cliente == c) cliente = null;
        c.Close();
    }

    internal static volatile int[] Asa;   // { lado (1 = derecha), y0, y1 } en píxeles de Android; null = sin asa
    internal static Action<string> DelLauncher;
    internal static volatile bool Lectura;   // modo lectura de Android ("lectura on|off" del agente): el visor baja a 60 Hz y amplía EcoQoS
    internal static Action AlLectura;
    internal static volatile int Giro;     // rotación de Android (0-3, del agente): el visor gira con ella sus zonas de gestos
    internal static Action Reposo;         // Windows bloqueado y Android sin sonar: el visor pausa la VM   // el visor: "windows" / "apagar" que el launcher manda por el agente
    internal static volatile int[] Isla;  // { x0, y0, x1, y1 } de la isla compacta en píxeles de Android; null = no se ve
    static bool Aplicar(string l)
    {
        Trace.WriteLine("<- " + Resumen(l));
        var p = l.Split(' ');
        int n;
        if (p.Length != 2) return false;
        // Asa del Panel Edge del launcher: "asa d,1200,1500" (lado d/i, franja vertical en píxeles de Android) u "asa off".
        // El visor no convierte en "Atrás" los toques que empiezan en ella (punto 19, 17/11).
        if (p[0] == "giro" && p[1].Length == 1 && p[1][0] >= '0' && p[1][0] <= '3') { Giro = p[1][0] - '0'; return true; }
        if (p[0] == "reposo" && p[1] == "1") { var r = Reposo; if (r != null) r(); return true; }
        if (p[0] == "volver" && (p[1] == "windows" || p[1] == "apagar"))
        {
            var d = DelLauncher;
            if (d != null) d(p[1]);
            return true;
        }
        if (p[0] == "asa")
        {
            var a = p[1].Split(',');
            int y0, y1;
            if (p[1] == "off") Asa = null;
            else if (a.Length == 3 && (a[0] == "d" || a[0] == "i") && int.TryParse(a[1], out y0) && int.TryParse(a[2], out y1) && y0 >= 0 && y1 > y0 && y1 - y0 <= 4000)
                Asa = new int[] { a[0] == "d" ? 1 : 0, y0, y1 };
            return true;
        }
        // Isla dinámica del launcher (va bajo la barra de estado de Android, que no la deja tocar): "isla x0,y0,x1,y1" en
        // píxeles de Android u "isla off". Los toques que empiezan dentro los convierte el visor en gestos de la isla.
        if (p[0] == "isla")
        {
            var a = p[1].Split(',');
            int x0, y0, x1, y1;
            if (p[1] == "off") Isla = null;
            else if (a.Length == 4 && int.TryParse(a[0], out x0) && int.TryParse(a[1], out y0) && int.TryParse(a[2], out x1) && int.TryParse(a[3], out y1)
                     && x0 >= 0 && y0 >= 0 && x1 > x0 && y1 > y0 && x1 - x0 <= 2000 && y1 - y0 <= 400)
                Isla = new int[] { x0, y0, x1, y1 };
            return true;
        }
        bool numero = int.TryParse(p[1], out n) && n >= 0 && n <= 100 && p[1] == n.ToString();
        bool onOff = p[1] == "on" || p[1] == "off";
        if (!(p[0] == "vol" || p[0] == "bri" ? numero
            : p[0] == "wifi" || p[0] == "bt" || p[0] == "ahorro" || p[0] == "gps" || p[0] == "lectura" ? onOff
            : p[0] == "clip" && p[1].Length > 0 && p[1].Length <= PortapapelesCompartido.Tope && Enlace.Activo)) return false;   // clip: solo con Android delante
        lock (cerrojo) { ultimo[p[0]] = p[1]; pendientes.Remove(p[0]); }
        bool radioAplicado = false;
        try
        {
            if (p[0] == "clip") PortapapelesCompartido.Poner(Encoding.UTF8.GetString(Convert.FromBase64String(p[1])));
            else if (p[0] == "gps") Ubicacion(p[1] == "on");
            else if (p[0] == "ahorro") Ahorro(p[1] == "on");
            else if (p[0] == "lectura") { Lectura = p[1] == "on"; var a = AlLectura; if (a != null) a(); }
            else if (p[0] == "vol")
            {
                if (vol != null) vol.SetMasterVolumeLevelScalar(n / 100f, ref propio);
                if (vol != null && n > 0) vol.SetMute(false, ref propio);
            }
            else if (p[0] == "bri")
            {
                briLocal = Environment.TickCount;
                using (var b = new ManagementObjectSearcher(@"root\wmi", "SELECT * FROM WmiMonitorBrightnessMethods"))
                    foreach (ManagementObject o in b.Get()) o.InvokeMethod("WmiSetBrightness", new object[] { 0u, (byte)n });
            }
            else if (p[0] == "wifi" || p[0] == "bt")
            {
                bool encender = p[1] == "on";
                lock (radioCerrojo) foreach (var r in radios)
                {
                    if (r.Kind != (p[0] == "wifi" ? RadioKind.WiFi : RadioKind.Bluetooth) || (r.State == RadioState.On) == encender) continue;
                    if (simularWifi && r.Kind == RadioKind.WiFi) { Trace.WriteLine("simulado: " + r.Kind + " " + p[1]); continue; }
                    if (radioPendiente != null && !radioPendiente.Wait(4000)) { Trace.WriteLine("radio ocupado"); continue; }
                    var t = r.SetStateAsync(encender ? RadioState.On : RadioState.Off).AsTask();
                    radioPendiente = t;
                    radioAplicado = t.Wait(4000) && t.Result == RadioAccessStatus.Allowed;
                    if (!radioAplicado) Trace.WriteLine("radio " + r.Kind + ": " + (t.IsCompleted ? t.Result.ToString() : "sin respuesta en 4 s"));
                }
            }
        }
        catch (Exception e) { Trace.WriteLine("aplicar: " + e.Message); }
        if (p[0] == "vol") EnviarVolActual();
        else if (p[0] == "bri") EnviarBrillo();
        else if ((p[0] == "wifi" || p[0] == "bt") && !radioAplicado) foreach (var r in radios) EnviarRadio(r);
        return radioAplicado;
    }

    // Ubicación (punto 12): Windows -> "loc lat,lon,precisión" solo mientras una app de Android pide el GPS ("gps on" del
    // agente) y Android está delante; si no, el Geolocator se suelta (deja de gastar). pedido null: solo recalcular.
    static Windows.Devices.Geolocation.Geolocator geo;
    static bool gpsPedido;
    static readonly object gpsCerrojo = new object();
    internal static void Ubicacion(bool? pedido)
    {
        lock (gpsCerrojo)
        {
            if (pedido.HasValue) gpsPedido = pedido.Value;
            bool querer = gpsPedido && Enlace.Activo;
            if (querer == (geo != null)) return;
            try
            {
                if (querer)
                {
                    geo = new Windows.Devices.Geolocation.Geolocator { ReportInterval = 30000, MovementThreshold = 10 };
                    geo.PositionChanged += Posicion;
                }
                else { geo.PositionChanged -= Posicion; geo = null; }
            }
            catch (Exception e) { Trace.WriteLine("ubicación: " + e.Message); geo = null; }
        }
    }

    static void Posicion(Windows.Devices.Geolocation.Geolocator g, Windows.Devices.Geolocation.PositionChangedEventArgs e)
    {
        var c = e.Position.Coordinate; var p = c.Point.Position;
        Enviar("loc", string.Format(System.Globalization.CultureInfo.InvariantCulture, "{0:F6},{1:F6},{2:F0}", p.Latitude, p.Longitude, Math.Min(c.Accuracy, 999999)));
    }

    // Ahorro de batería de Android -> ahorro de energía de Windows (punto 8). Sin administrador ni API oculta: el umbral
    // "activar al X %" del plan activo pasa a 100 % y al quitarlo vuelve al del usuario (guardado en el registro, por si
    // el Visor muere). La WNF_PO_ENERGY_SAVER_OVERRIDE se descartó: sin documentar ni fuente del identificador.
    // shortcut: con cargador o al 100 % Windows no lo enciende (medido 17/11 en CA: Disabled -> Off); no hay API documentada para forzarlo.
    [DllImport("powrprof.dll")] static extern uint PowerGetActiveScheme(IntPtr raiz, out IntPtr plan);
    [DllImport("powrprof.dll")] static extern uint PowerReadDCValueIndex(IntPtr raiz, ref Guid plan, ref Guid grupo, ref Guid ajuste, out uint valor);
    [DllImport("powrprof.dll")] static extern uint PowerWriteDCValueIndex(IntPtr raiz, ref Guid plan, ref Guid grupo, ref Guid ajuste, uint valor);
    [DllImport("powrprof.dll")] static extern uint PowerSetActiveScheme(IntPtr raiz, ref Guid plan);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr p);
    static Guid subAhorro = new Guid("de830923-a562-41af-a086-e3a2c6bad2da"), umbral = new Guid("e69653ca-cf7f-4f05-aa73-cb833fa90ad4");
    static readonly object ahorroCerrojo = new object();
    const string Clave = @"Software\SurfaceDual";

    static bool PlanActivo(out Guid plan)
    {
        IntPtr pp; plan = Guid.Empty;
        if (PowerGetActiveScheme(IntPtr.Zero, out pp) != 0) return false;
        plan = (Guid)Marshal.PtrToStructure(pp, typeof(Guid)); LocalFree(pp);
        return true;
    }

    static void Ahorro(bool on)
    {
        if (!on) { RestaurarAhorro(); return; }
        lock (ahorroCerrojo)
        {
            Guid plan; uint v;
            if (!PlanActivo(out plan)) return;
            using (var k = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(Clave))
                if (k.GetValue("UmbralAhorroPlan") as string != plan.ToString())
                {
                    RestaurarAhorro();   // otro plan activo desde entonces: el viejo vuelve a su valor primero
                    if (PowerReadDCValueIndex(IntPtr.Zero, ref plan, ref subAhorro, ref umbral, out v) != 0) return;   // sin el original no se toca
                    k.SetValue("UmbralAhorroOriginal", (int)v); k.SetValue("UmbralAhorroPlan", plan.ToString());
                }
            if (PowerWriteDCValueIndex(IntPtr.Zero, ref plan, ref subAhorro, ref umbral, 100) == 0) PowerSetActiveScheme(IntPtr.Zero, ref plan);
        }
    }

    // Plan y valor guardados en HKCU: al ocultar Android, al perder la sesión, al cerrar el Visor y al arrancar (si murió).
    internal static void RestaurarAhorro()
    {
        lock (ahorroCerrojo)
            using (var k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(Clave, true))
            {
                var s = k == null ? null : k.GetValue("UmbralAhorroPlan") as string;
                var v = k == null ? null : k.GetValue("UmbralAhorroOriginal") as int?;
                Guid plan, activo;
                if (s == null || v == null || !Guid.TryParse(s, out plan)) return;
                if (PowerWriteDCValueIndex(IntPtr.Zero, ref plan, ref subAhorro, ref umbral, (uint)v.Value) != 0) return;   // queda guardado: se reintenta
                k.DeleteValue("UmbralAhorroPlan", false); k.DeleteValue("UmbralAhorroOriginal", false);
                if (PlanActivo(out activo) && activo == plan) PowerSetActiveScheme(IntPtr.Zero, ref plan);
            }
    }

    static string Resumen(string l)   // en el registro, ni el portapapeles ni la ubicación: solo clave y tamaño
    {
        return l.StartsWith("clip ") || l.StartsWith("loc ") ? l.Substring(0, l.IndexOf(' ')) + " (" + l.Length + " c)" : l;
    }

    public static string Bluetooth(string orden)
    {
        if ((orden == "bt on" || orden == "bt off") && Aplicar(orden)) return orden.Substring(3);
        foreach (var r in radios) if (r.Kind == RadioKind.Bluetooth) return r.State == RadioState.On ? "on" : "off";
        return "no";
    }

    public static void Republicar()
    {
        EnviarVolActual(); EnviarBrillo();
        foreach (var r in radios) EnviarRadio(r);
        EnviarRegion(); EnviarAhorro(); Enviar("bat", BateriaCompartida.Estado());
        GiroCompartido.Publicar(); PortapapelesCompartido.Publicar();   // el portapapeles llega después de "listo" (hilo de la ventana)
        Enviar("listo", "1");
        hayPendientes.Set();
    }

    internal static void Enviar(string clave, string valor)
    {
        lock (cerrojo)
        {
            string u;
            if (cliente == null) return;
            if (clave != "flotar" && clave != "split" && clave != "panel" && clave != "isla" && clave != "despierta" && ultimo.TryGetValue(clave, out u) && u == valor) { pendientes.Remove(clave); return; }
            pendientes[clave] = valor;
        }
        hayPendientes.Set();
    }

    static void Escritor()
    {
        while (hayPendientes.WaitOne())
            while (true)
            {
                string linea = null; TcpClient c;
                lock (cerrojo)
                {
                    if (!Enlace.Activo || pendientes.Count == 0 || cliente == null) break;
                    foreach (System.Collections.DictionaryEntry e in pendientes) { linea = e.Key + " " + e.Value; break; }
                    pendientes.RemoveAt(0);
                    c = cliente;
                    int espacio = linea.IndexOf(' ');
                    if (linea.Substring(0, espacio) != "flotar" && linea.Substring(0, espacio) != "split")
                        ultimo[linea.Substring(0, espacio)] = linea.Substring(espacio + 1);
                }
                try
                {
                    var b = Encoding.ASCII.GetBytes(linea + "\n"); c.GetStream().Write(b, 0, b.Length);
                    Trace.WriteLine("-> " + Resumen(linea));
                }
                catch (Exception)
                {
                    lock (cerrojo) if (cliente == c) cliente = null;
                    c.Close();
                }
            }
    }

    static void EnviarVol(float f, bool mudo) { Enviar("vol", (mudo ? 0 : (int)Math.Round(f * 100)).ToString()); }

    static void EnviarVolActual()
    {
        try { float f; bool m; vol.GetMasterVolumeLevelScalar(out f); vol.GetMute(out m); EnviarVol(f, m); } catch (Exception) { }
    }

    static void EnviarBrillo()
    {
        try { using (var b = new ManagementObjectSearcher(@"root\wmi", "SELECT CurrentBrightness FROM WmiMonitorBrightness")) foreach (var o in b.Get()) Enviar("bri", Convert.ToString(o["CurrentBrightness"])); }
        catch (Exception) { }
    }

    [DllImport("kernel32.dll")] static extern ushort GetUserDefaultUILanguage();
    [DllImport("icu.dll", CharSet = CharSet.Unicode)] static extern int ucal_getTimeZoneIDForWindowsID(string winid, int largo, string region, StringBuilder id, int capacidad, ref int error);
    static void EnviarRegion()
    {
        try { Enviar("idioma", new System.Globalization.CultureInfo(GetUserDefaultUILanguage()).Name); } catch (Exception) { }
        try
        {
            TimeZoneInfo.ClearCachedData();
            string w = TimeZoneInfo.Local.Id;
            var id = new StringBuilder(64); int error = 0;
            int n = ucal_getTimeZoneIDForWindowsID(w, w.Length, null, id, id.Capacity, ref error);
            if (error <= 0 && n > 0) Enviar("zona", id.ToString(0, n));
        }
        catch (Exception) { }
    }

    static void EnviarAhorro()
    {
        try { Enviar("ahorro", Windows.System.Power.PowerManager.EnergySaverStatus == Windows.System.Power.EnergySaverStatus.On ? "on" : "off"); }
        catch (Exception) { }
    }

    static void EnviarRadio(Radio r)
    {
        if (r.Kind == RadioKind.WiFi || r.Kind == RadioKind.Bluetooth)
            Enviar(r.Kind == RadioKind.WiFi ? "wifi" : "bt", r.State == RadioState.On ? "on" : "off");
    }

    static void Altavoz()
    {
        lock (avisoVol)
        {
            IMMDevice d; object o;
            enumerador.GetDefaultAudioEndpoint(0, 0, out d);
            var iid = typeof(IAudioEndpointVolume).GUID;
            d.Activate(ref iid, 23, IntPtr.Zero, out o);
            var nuevo = (IAudioEndpointVolume)o;
            nuevo.RegisterControlChangeNotify(avisoVol);
            if (vol != null) try { vol.UnregisterControlChangeNotify(avisoVol); } catch (Exception) { }
            vol = nuevo;
            EnviarVolActual();
        }
    }

    class AvisoVolumen : IAudioEndpointVolumeCallback
    {
        public int OnNotify(IntPtr d)
        {
            if ((Guid)Marshal.PtrToStructure(d, typeof(Guid)) != propio)
                EnviarVol(BitConverter.ToSingle(BitConverter.GetBytes(Marshal.ReadInt32(d, 20)), 0), Marshal.ReadInt32(d, 16) != 0);
            return 0;
        }
    }

    class AvisoDispositivo : IMMNotificationClient
    {
        public int OnDeviceStateChanged(string id, int estado) { return 0; }
        public int OnDeviceAdded(string id) { return 0; }
        public int OnDeviceRemoved(string id) { return 0; }
        public int OnDefaultDeviceChanged(int flujo, int rol, string id)
        {
            if (flujo == 0 && rol == 0) ThreadPool.QueueUserWorkItem(x => { try { Altavoz(); } catch (Exception) { } });
            return 0;
        }
        public int OnPropertyValueChanged(string id, ClavePropiedad k) { return 0; }
    }

    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator { }

    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDeviceEnumerator
    {
        void EnumAudioEndpoints();
        void GetDefaultAudioEndpoint(int flujo, int rol, out IMMDevice d);
        void GetDevice();
        void RegisterEndpointNotificationCallback(IMMNotificationClient c);
    }

    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDevice { void Activate(ref Guid iid, int ctx, IntPtr p, [MarshalAs(UnmanagedType.IUnknown)] out object o); }

    [ComImport, Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioEndpointVolume
    {
        void RegisterControlChangeNotify(IAudioEndpointVolumeCallback c);
        void UnregisterControlChangeNotify(IAudioEndpointVolumeCallback c);
        void GetChannelCount();
        void SetMasterVolumeLevel();
        void SetMasterVolumeLevelScalar(float v, ref Guid contexto);
        void GetMasterVolumeLevel();
        void GetMasterVolumeLevelScalar(out float v);
        void SetChannelVolumeLevel();
        void SetChannelVolumeLevelScalar();
        void GetChannelVolumeLevel();
        void GetChannelVolumeLevelScalar();
        void SetMute([MarshalAs(UnmanagedType.Bool)] bool m, ref Guid contexto);
        void GetMute([MarshalAs(UnmanagedType.Bool)] out bool m);
    }

    [ComImport, Guid("657804FA-D6AD-4496-8A60-352752AF4F89"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioEndpointVolumeCallback { [PreserveSig] int OnNotify(IntPtr datos); }

    [ComImport, Guid("7991EEC9-7E89-4D85-8390-6C703CEC60C0"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMNotificationClient
    {
        [PreserveSig] int OnDeviceStateChanged([MarshalAs(UnmanagedType.LPWStr)] string id, int estado);
        [PreserveSig] int OnDeviceAdded([MarshalAs(UnmanagedType.LPWStr)] string id);
        [PreserveSig] int OnDeviceRemoved([MarshalAs(UnmanagedType.LPWStr)] string id);
        [PreserveSig] int OnDefaultDeviceChanged(int flujo, int rol, [MarshalAs(UnmanagedType.LPWStr)] string id);
        [PreserveSig] int OnPropertyValueChanged([MarshalAs(UnmanagedType.LPWStr)] string id, ClavePropiedad k);
    }

    public struct ClavePropiedad { public Guid fmtid; public int pid; }
}

// Música de Android en los controles multimedia de Windows (panel de volumen, pantalla de bloqueo, teclas multimedia y
// auriculares): SystemMediaTransportControls de la ventana del visor (interop Win32, como Chrome). Del launcher por 4445
// (IslaOyente.mediaAWindows), solo al cambiar: "media <play|pause|off> <pos ms> <dur ms> <título> <artista> [<carátula JPEG>]"
// (base64 UTF-8 o "-"; sin el último campo, la carátula sigue). Los botones vuelven: "media play|pause|next|prev|seek <ms>";
// "media ?" pide todo (al conectar y al reanudar la VM). Solo con Android delante (también con Windows bloqueado); se quita
// al pausar la VM o perder la conexión. Con el visor delante el gancho de teclado ya manda las teclas multimedia a Android
// y se las come: nunca llegan por los dos caminos.
static class MusicaCompartida
{
    [ComImport, Guid("ddb0472d-c911-4a1f-86d9-dc3d71a95f5a"), InterfaceType(ComInterfaceType.InterfaceIsIInspectable)]
    interface ISystemMediaTransportControlsInterop { [return: MarshalAs(UnmanagedType.IInspectable)] object GetForWindow(IntPtr ventana, [In] ref Guid iid); }
    internal static volatile Action<string> Escribir;   // al launcher; null = sin conexión
    static SynchronizationContext ui; static IntPtr ventana;
    static Windows.Media.SystemMediaTransportControls smtc;   // solo en el hilo de la ventana
    static bool completo;   // false tras quitar (carátula borrada): la próxima línea sin carátula la pide otra vez

    public static void Iniciar(IntPtr h) { ui = SynchronizationContext.Current; ventana = h; }   // en el hilo de la ventana
    public static void Pedir() { Mandar("?"); }
    static void Mandar(string orden) { var w = Escribir; if (w != null) try { w("media " + orden); } catch (Exception) { } }

    internal sealed class Pista { public string Estado, Titulo, Artista; public long Pos, Dur; public bool ConCaratula; public byte[] Jpg; }

    /// <summary>Valida y descifra una línea "media": null si no cumple el formato (frontera de confianza: viene de Android).</summary>
    internal static Pista Leer(string l)
    {
        var p = l.Split(' '); var r = new Pista();
        if (p.Length < 6 || p.Length > 7 || p[0] != "media" || (p[1] != "play" && p[1] != "pause" && p[1] != "off")
            || !long.TryParse(p[2], out r.Pos) || !long.TryParse(p[3], out r.Dur) || r.Pos < 0 || r.Dur < 0 || r.Dur > 864000000000) return null;
        r.Estado = p[1]; r.ConCaratula = p.Length == 7;
        try
        {
            r.Titulo = Texto(p[4]); r.Artista = Texto(p[5]);
            if (r.ConCaratula && p[6] != "-") r.Jpg = Convert.FromBase64String(p[6]);
        }
        catch (FormatException) { return null; }
        if (r.Jpg != null && (r.Jpg.Length < 3 || r.Jpg[0] != 0xFF || r.Jpg[1] != 0xD8 || r.Jpg[2] != 0xFF)) r.Jpg = null;   // solo JPEG
        return r;
    }
    static string Texto(string b)
    {
        if (b == "-") return "";
        string t = System.Text.RegularExpressions.Regex.Replace(Encoding.UTF8.GetString(Convert.FromBase64String(b)), @"\p{C}+", " ").Trim();
        return t.Length > 200 ? t.Substring(0, 200) : t;   // Android manda 120 como mucho
    }

    internal static int Autoprueba()
    {
        var a = Leer("media play 1000 200000 SG9sYQ== - /9j/"); var b = Leer("media pause 5 0 - QQo=");
        bool ok = a != null && a.Estado == "play" && a.Pos == 1000 && a.Dur == 200000 && a.Titulo == "Hola" && a.Artista == "" && a.ConCaratula && a.Jpg != null
            && b != null && !b.ConCaratula && b.Artista == "A" && Leer("media off 0 0 - - -").Jpg == null
            && Leer("media play 1 2 - - iVBO").Jpg == null   // PNG: no
            && Leer("media stop 0 0 - -") == null && Leer("media play -1 0 - -") == null && Leer("media play 0 0 ### -") == null
            && Leer("media play 0 0 -") == null && Leer("media play 0 0 - - - x") == null && Leer("aviso 0 0 - -") == null;
        return ok ? 0 : 1;
    }

    internal static void Linea(string l)   // hilo del launcher (línea ya acotada)
    {
        var p = Leer(l); var u = ui;
        if (p != null && u != null) u.Post(x => Poner(p), null);
    }

    internal static void Quitar() { var u = ui; if (u != null) u.Post(x => Limpiar(), null); }

    static void Poner(Pista p)
    {
        if (p.Estado == "off" || !Enlace.Activo) { Limpiar(); return; }
        try
        {
            if (smtc == null)
            {
                var iid = new Guid("99FA3FF4-1742-42A6-902E-087D41F965EC");   // ISystemMediaTransportControls
                var f = (ISystemMediaTransportControlsInterop)System.Runtime.InteropServices.WindowsRuntime.WindowsRuntimeMarshal.GetActivationFactory(typeof(Windows.Media.SystemMediaTransportControls));
                smtc = (Windows.Media.SystemMediaTransportControls)f.GetForWindow(ventana, ref iid);
                smtc.IsPlayEnabled = smtc.IsPauseEnabled = smtc.IsNextEnabled = smtc.IsPreviousEnabled = true;
                smtc.ButtonPressed += (s, e) =>
                {
                    var t = e.Button;
                    if (t == Windows.Media.SystemMediaTransportControlsButton.Play) Mandar("play");
                    else if (t == Windows.Media.SystemMediaTransportControlsButton.Pause) Mandar("pause");
                    else if (t == Windows.Media.SystemMediaTransportControlsButton.Next) Mandar("next");
                    else if (t == Windows.Media.SystemMediaTransportControlsButton.Previous) Mandar("prev");
                };
                smtc.PlaybackPositionChangeRequested += (s, e) => Mandar("seek " + (long)Math.Max(0, e.RequestedPlaybackPosition.TotalMilliseconds));
            }
            if (!p.ConCaratula && !completo) Pedir();
            completo |= p.ConCaratula;
            smtc.IsEnabled = true;
            smtc.PlaybackStatus = p.Estado == "play" ? Windows.Media.MediaPlaybackStatus.Playing : Windows.Media.MediaPlaybackStatus.Paused;
            var d = smtc.DisplayUpdater;
            if (d.Type != Windows.Media.MediaPlaybackType.Music) d.Type = Windows.Media.MediaPlaybackType.Music;
            d.MusicProperties.Title = p.Titulo; d.MusicProperties.Artist = p.Artista;
            if (p.ConCaratula && p.Jpg == null) d.Thumbnail = null;
            else if (p.ConCaratula)
            {
                // Un flujo de WinRT de verdad: con MemoryStream.AsRandomAccessStream() Windows dejaba la carátula vacía (medido).
                var ras = new Windows.Storage.Streams.InMemoryRandomAccessStream();
                ras.WriteAsync(System.Runtime.InteropServices.WindowsRuntime.WindowsRuntimeBufferExtensions.AsBuffer(p.Jpg)).AsTask().Wait();
                ras.Seek(0);
                d.Thumbnail = Windows.Storage.Streams.RandomAccessStreamReference.CreateFromStream(ras);
            }
            d.Update();
            var tl = new Windows.Media.SystemMediaTransportControlsTimelineProperties();
            tl.EndTime = tl.MaxSeekTime = TimeSpan.FromMilliseconds(p.Dur);
            tl.Position = TimeSpan.FromMilliseconds(Math.Min(p.Pos, p.Dur));
            smtc.UpdateTimelineProperties(tl);
        }
        catch (Exception e) { Trace.WriteLine("música: " + e.Message); }
    }

    static void Limpiar()   // hilo de la ventana
    {
        completo = false;
        if (smtc == null) return;
        try { smtc.PlaybackStatus = Windows.Media.MediaPlaybackStatus.Closed; smtc.DisplayUpdater.ClearAll(); smtc.DisplayUpdater.Update(); smtc.IsEnabled = false; }
        catch (Exception e) { Trace.WriteLine("música: " + e.Message); }
    }
}

// Redes Wi-Fi de Windows para el launcher (mantener pulsado Wi-Fi en los ajustes rápidos; pedido 19/11). Android sale por
// Windows, así que elegir red es cambiar la de Windows. Órdenes por 4445: "wifi lista", "wifi buscar" (lista + nueva búsqueda),
// "wifi conectar <ssid hex>" (solo redes a la vista con perfil guardado en Windows) y "wifi panel" (el panel de redes de
// Windows: ahí se escribe la contraseña de una red nueva; nunca pasa por aquí). Respuesta: "wifi inicio", "wifi <ssid hex> <señal 0-100>
// <c|g|n><s|a>" por red (conectada/guardada/nueva, segura/abierta) y "wifi fin"; "wifi error" si no pudo pedir la conexión. API nativa de WLAN (wlanapi.dll):
// learn.microsoft.com/windows/win32/api/wlanapi (WLAN_AVAILABLE_NETWORK = 628 bytes; desplazamientos de wlanapi.h).
static class WifiWindows
{
    [DllImport("wlanapi.dll")] static extern int WlanOpenHandle(uint version, IntPtr r, out uint negociada, out IntPtr h);
    [DllImport("wlanapi.dll")] static extern int WlanCloseHandle(IntPtr h, IntPtr r);
    [DllImport("wlanapi.dll")] static extern int WlanEnumInterfaces(IntPtr h, IntPtr r, out IntPtr lista);
    [DllImport("wlanapi.dll")] static extern int WlanGetAvailableNetworkList(IntPtr h, ref Guid interfaz, uint banderas, IntPtr r, out IntPtr lista);
    [DllImport("wlanapi.dll")] static extern int WlanScan(IntPtr h, ref Guid interfaz, IntPtr ssid, IntPtr ie, IntPtr r);
    [DllImport("wlanapi.dll")] static extern int WlanConnect(IntPtr h, ref Guid interfaz, ref Conexion c, IntPtr r);
    [DllImport("wlanapi.dll")] static extern void WlanFreeMemory(IntPtr p);
    [StructLayout(LayoutKind.Sequential)] struct Conexion { public int modo; [MarshalAs(UnmanagedType.LPWStr)] public string perfil; public IntPtr ssid, bssids; public int tipoBss, banderas; }
    class Red { public string hex, perfil; public int calidad, tipoBss; public bool conectada, segura; }

    static string Hex(byte[] b) { var s = new StringBuilder(); foreach (byte x in b) s.Append(x.ToString("x2")); return s.ToString(); }

    // Redes a la vista del primer adaptador Wi-Fi, una por SSID (Windows repite la que tiene perfil): la conectada, las guardadas y el resto por señal.
    static List<Red> Redes(IntPtr h, out Guid interfaz)
    {
        var redes = new Dictionary<string, Red>(); interfaz = Guid.Empty;
        IntPtr li, lr;
        if (WlanEnumInterfaces(h, IntPtr.Zero, out li) != 0) return new List<Red>();
        try
        {
            if (Marshal.ReadInt32(li) < 1) return new List<Red>();
            interfaz = (Guid)Marshal.PtrToStructure(li + 8, typeof(Guid));   // WLAN_INTERFACE_INFO_LIST: 2 DWORD y la primera interfaz
        }
        finally { WlanFreeMemory(li); }
        if (WlanGetAvailableNetworkList(h, ref interfaz, 0, IntPtr.Zero, out lr) != 0) return new List<Red>();
        try
        {
            int n = Marshal.ReadInt32(lr);
            for (int i = 0; i < n && i < 256; i++)
            {
                IntPtr e = lr + 8 + i * 628;
                int largo = Marshal.ReadInt32(e, 512), banderas = Marshal.ReadInt32(e, 620);   // DOT11_SSID.uSSIDLength, dwFlags (1 conectada, 2 con perfil)
                if (largo < 1 || largo > 32) continue;   // red oculta
                var ssid = new byte[largo]; Marshal.Copy(e + 516, ssid, 0, largo);
                var r = new Red { hex = Hex(ssid), perfil = (banderas & 2) != 0 ? Marshal.PtrToStringUni(e) : null, calidad = Math.Max(0, Math.Min(100, Marshal.ReadInt32(e, 604))), tipoBss = Marshal.ReadInt32(e, 548),
                                  conectada = (banderas & 1) != 0, segura = Marshal.ReadInt32(e, 608) != 0 };
                Red v;
                if (!redes.TryGetValue(r.hex, out v) || r.conectada && !v.conectada || r.perfil != null && v.perfil == null) redes[r.hex] = r;
            }
        }
        finally { WlanFreeMemory(lr); }
        var l = new List<Red>(redes.Values);
        l.Sort((a, b) => a.conectada != b.conectada ? (a.conectada ? -1 : 1) : (a.perfil != null) != (b.perfil != null) ? (a.perfil != null ? -1 : 1) : b.calidad - a.calidad);
        return l;
    }

    internal static void Linea(string l, Action<string> escribir)
    {
        if (l == "wifi panel") { try { Process.Start(new ProcessStartInfo("ms-availablenetworks:") { UseShellExecute = true }); } catch (Exception e) { Trace.WriteLine("wifi panel: " + e.Message); } return; }
        bool conectar = l.StartsWith("wifi conectar ");
        if (l != "wifi lista" && l != "wifi buscar" && !conectar) return;
        uint v; IntPtr h = IntPtr.Zero;
        try
        {
            if (!conectar && escribir != null) escribir("wifi inicio");   // lista nueva: el launcher tira una a medias
            Guid g = Guid.Empty; var redes = WlanOpenHandle(2, IntPtr.Zero, out v, out h) == 0 ? Redes(h, out g) : new List<Red>();   // sin WLAN: lista vacía
            if (conectar)
            {
                // Viene de Android: solo una red que Windows ve ahora y de la que ya guarda perfil (él pone la clave, no nosotros).
                var r = redes.Find(x => x.hex == l.Substring(14) && x.perfil != null);
                var c = new Conexion { modo = 0 /* wlan_connection_mode_profile */, perfil = r == null ? null : r.perfil, tipoBss = r == null ? 0 : r.tipoBss /* el del perfil: con otro, WlanConnect puede fallar */ };
                int k = r == null ? -1 : WlanConnect(h, ref g, ref c, IntPtr.Zero);
                if (k != 0 && escribir != null) escribir("wifi error");   // el launcher lo dice
                return;
            }
            if (escribir != null)
            {
                for (int i = 0; i < redes.Count && i < 24; i++)
                    escribir("wifi " + redes[i].hex + " " + redes[i].calidad + " " + (redes[i].conectada ? "c" : redes[i].perfil != null ? "g" : "n") + (redes[i].segura ? "s" : "a"));
                escribir("wifi fin");
            }
            if (l == "wifi buscar" && g != Guid.Empty) WlanScan(h, ref g, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        }
        catch (Exception e) { Trace.WriteLine("wifi: " + e.Message); }   // launcher desconectado a media lista
        finally { if (h != IntPtr.Zero) WlanCloseHandle(h, IntPtr.Zero); }
    }
}
