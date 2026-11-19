// Surface Dual en Windows: ícono en la bandeja (y acceso directo "Android" en Inicio, anclable a la barra de tareas)
// para entrar al modo Android sin teclado. Tocar el ícono abre el panel rápido (Panel.cs); Win+Alt+A va directo a Android.
// No sondea: solo reacciona a toques, al atajo y a cambios de postura.
//   Bandeja.exe            residente (arranca con Windows)
//   Bandeja.exe /entrar    abre Android (si ya hay una Bandeja, se lo pide a ella)
// Android corre en una VM de Hyper-V (RutaA\fase4): Entrar avisa al VisorVM residente o arranca VM y visor (iniciar.ps1);
// volver a Windows suspende la VM (pausar.ps1). QEMU se quitó el 16/11 (pedido del usuario).
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Windows.Forms;

static partial class Bandeja   // el panel rápido está en Panel.cs
{
    static readonly string F2 = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(Application.ExecutablePath), "..", ".."));
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool SetProcessDPIAware();
    [STAThread]
    static void Main(string[] a)
    {
        SetProcessDPIAware();   // sin esto Windows estira el diálogo y se ve borroso en la pantalla de la Surface (200 %)
        if (a.Length > 0 && a[0] == "/probar-pregunta")   // prueba visual del diálogo (sin tocar nada): se cierra solo a los 15 s
        {
            Pregunta.Mostrar("Teclado quitado", "¿Pasar a Android?\nAndroid está apagado: tarda unos 35 segundos en encender.",
                "Pasar a Android", "Seguir en Windows", (s, r) => Application.Exit());
            Application.Run();
            return;
        }
        if (a.Length > 0 && a[0] == "/probar-panel")   // prueba visual del panel (claro/oscuro): se cierra con Esc o a los 15 s
        {
            PanelRapido.Alternar(a.Length > 1 ? a[1] : null, true);
            var fin = new System.Windows.Forms.Timer { Interval = 15000 };
            fin.Tick += (s, e) => Application.Exit();
            fin.Start();
            Application.Run();
            return;
        }
        bool entrar = a.Length > 0 && a[0] == "/entrar";
        bool primera;
        var unica = new Mutex(true, "SurfaceDualBandeja", out primera);
        var pedido = new EventWaitHandle(false, EventResetMode.AutoReset, "SurfaceDualEntrar");
        if (!primera) { if (entrar) pedido.Set(); return; }   // ya hay una Bandeja: que ella abra Android
        RestaurarGestos(); RestaurarWindows();

        var icono = new NotifyIcon { Icon = Icono(), Text = "Surface Dual", Visible = true };
        var menu = new ContextMenuStrip();
        menu.Items.Add("Abrir Android", null, (s, e) => Entrar());
        menu.Items.Add("Apagar Android", null, (s, e) => Apagar());
        var auto = new ToolStripMenuItem("Preguntar al quitar o poner el teclado") { CheckOnClick = true, Checked = Automatico };
        auto.CheckedChanged += (s, e) => Automatico = auto.Checked;
        menu.Items.Add(auto);
        menu.Items.Add("Volver a preguntar (olvidar lo recordado)", null, (s, e) => Olvidar());
        // Lo lee VisorVM al suspender: tras este tiempo suspendido, Android se apaga solo y libera sus 12 GB (0 = nunca).
        var autoApagar = new ToolStripMenuItem("Apagar Android tras 1 hora suspendido") { CheckOnClick = true, Checked = AutoApagar };
        autoApagar.CheckedChanged += (s, e) => AutoApagar = autoApagar.Checked;
        menu.Items.Add(autoApagar);
        menu.Opening += (s, e) => { auto.Checked = Automatico; autoApagar.Checked = AutoApagar; };   // el panel también los cambia
        // Estado de Android en el texto del icono, consultado al pasar por encima (sin sondeo).
        int ultimaConsulta = Environment.TickCount - 10000;
        icono.MouseMove += (s, e) =>
        {
            if (Environment.TickCount - ultimaConsulta < 2000) return;
            ultimaConsulta = Environment.TickCount;
            var est = Estado();
            icono.Text = est == EstadoAndroid.Apagado ? "Surface Dual: Android apagado (no ocupa RAM)"
                       : est == EstadoAndroid.Suspendido ? "Surface Dual: Android en pausa (ocupa 12 GB)"
                       : est == EstadoAndroid.Encendiendo ? "Surface Dual: Android encendiendo" : "Surface Dual: Android activo";
        };
        menu.Items.Add("Salir", null, (s, e) => { icono.Visible = false; Application.Exit(); });
        icono.ContextMenuStrip = menu;
        icono.MouseClick += (s, e) => { if (e.Button == MouseButtons.Left) PanelRapido.Alternar(); };   // panel rápido (Panel.cs)

        var ui = new Control(); ui.CreateControl();   // para volver al hilo de la interfaz desde la espera del aviso
        new Thread(() => { while (pedido.WaitOne()) ui.BeginInvoke((Action)Entrar); }) { IsBackground = true }.Start();
        var postura = new Postura();   // quitar/poner el teclado (o doblarlo) cambia de sistema
        if (entrar) Entrar();
        Application.Run();
        GC.KeepAlive(unica);
        GC.KeepAlive(postura);   // NativeWindow solo guarda una referencia débil: sin esto el GC la recoge y se pierde la postura
    }

    // Ruta A: Android en una VM propia de Hyper-V (RutaA\fase4\iniciar.ps1 / pausar.ps1).
    static readonly string Fase4 = Path.GetFullPath(Path.Combine(F2, "..", "RutaA", "fase4"));
    // Los scripts de Hyper-V tardan segundos: van a un hilo propio, uno a la vez y en orden de llegada (quitar y poner
    // el teclado rápido no los desordena), sin congelar la bandeja ni el aviso de postura.
    static readonly System.Collections.Concurrent.BlockingCollection<string> cola = CrearCola();
    static System.Collections.Concurrent.BlockingCollection<string> CrearCola()
    {
        var c = new System.Collections.Concurrent.BlockingCollection<string>();
        new Thread(() =>
        {
            foreach (string a in c.GetConsumingEnumerable())
                try
                {
                    Process.Start(new ProcessStartInfo("powershell", "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden " + a)
                        { WindowStyle = ProcessWindowStyle.Hidden, CreateNoWindow = true }).WaitForExit();
                }
                catch { }
        }) { IsBackground = true }.Start();
        return c;
    }
    static void Script(string nombre) { cola.Add("-File \"" + Path.Combine(Fase4, nombre) + "\""); }

    // Hyper-V: con el VisorVM residente vivo, el cambio es un aviso (~0,1 s); si no, el script en frío (arranca VM y visor).
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool AllowSetForegroundWindow(int pid);
    static bool AvisarVisorVM(bool mostrar)
    {
        EventWaitHandle e, contrario;
        if (!EventWaitHandle.TryOpenExisting(mostrar ? @"Local\SurfaceDualVisorVM.Mostrar" : @"Local\SurfaceDualVisorVM.Ocultar", out e)) return false;
        // La última orden gana: si la contraria sigue sin atender, se anula (si no, el Visor podría atenderlas al revés).
        if (EventWaitHandle.TryOpenExisting(mostrar ? @"Local\SurfaceDualVisorVM.Ocultar" : @"Local\SurfaceDualVisorVM.Mostrar", out contrario))
            using (contrario) contrario.Reset();
        if (mostrar) AllowSetForegroundWindow(-1);   // la Bandeja tiene la última entrada (el toque): cede al Visor el ponerse delante
        using (e) e.Set();
        return true;
    }

    // El visor apaga los gestos táctiles de 3/4 dedos de Windows con Android delante y guarda el valor original en HKCU; si
    // murió sin devolverlos, la Bandeja (arranca con la sesión) los devuelve (revisión Gemini/DeepSeek 16/11).
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool SystemParametersInfoW(uint a, uint u, IntPtr p, uint f);
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool SystemParametersInfoW(uint a, uint u, ref uint p, uint f);
    static uint LeerGestos() { uint v = 1; SystemParametersInfoW(0x2030, 0, ref v, 0); return v; }
    static void RestaurarGestos()
    {
        try
        {
            object v = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "Gestos3y4Original", null);
            if (v == null || Process.GetProcessesByName("VisorVM").Length > 0) return;
            uint u = Convert.ToUInt32(v);
            // Mismo orden que VisorVM: valor directo y, si no lo toma, por puntero (medido 16/11: el directo funciona).
            if (!SystemParametersInfoW(0x2031, 0, (IntPtr)u, 2) || (LeerGestos() != 0) != (u != 0)) SystemParametersInfoW(0x2031, 0, ref u, 2);
            if ((LeerGestos() != 0) != (u != 0)) return;   // no se pudo: se reintenta en el próximo arranque
            using (var k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(@"Software\SurfaceDual", true)) if (k != null) k.DeleteValue("Gestos3y4Original", false);
        }
        catch (Exception ex) { Trace.WriteLine("gestos: " + ex.Message); }
    }
    static void RestaurarWindows()
    {
        // Giro bloqueado, umbral de ahorro o frecuencia virtual que dejó un visor muerto: siguen tras reiniciar y solo los
        // devuelve un visor nuevo, que puede no abrirse en días. Requiere el VisorVM.exe con "--restaurar" (un viejo abriría Android).
        const string k = @"HKEY_CURRENT_USER\Software\SurfaceDual";
        if (Process.GetProcessesByName("VisorVM").Length > 0 || (Microsoft.Win32.Registry.GetValue(k, "GiroBloqueado", 0) as int?) != 1
            && Microsoft.Win32.Registry.GetValue(k, "UmbralAhorroPlan", null) == null && !File.Exists(Path.Combine(Path.GetTempPath(), "visorvm_hz_virtual.txt"))) return;
        try { using (var p = Process.Start(new ProcessStartInfo(Path.Combine(Fase4, "host", "VisorVM.exe"), "--restaurar") { UseShellExecute = false })) p.WaitForExit(5000); }   // se espera: iniciar.ps1 lo tomaría por un visor residente
        catch (Exception ex) { Trace.WriteLine("restaurar: " + ex.Message); }
    }
    static bool VmExiste()
    {
        try
        {
            using (var s = new System.Management.ManagementObjectSearcher(@"root\virtualization\v2",
                "SELECT Name FROM Msvm_ComputerSystem WHERE ElementName='SurfaceDualAndroid'"))
            using (var c = s.Get()) return c.Count > 0;
        }
        catch { return true; }   // WMI falló: mejor intentar abrir el visor (si no hay VM, él lo avisa) que no mostrar nada
    }

    static void Entrar()
    {
        if (AvisarVisorVM(true)) return;
        // En frío: primero el Visor (su pantalla de carga sale al instante, con el permiso de ponerse delante que da el
        // toque del usuario) y después encender la VM; antes el script encendía primero y no se veía nada ~10 s (16/11).
        // Sin VM (primera vez) lo hace todo iniciar.ps1: crea la VM y después abre el visor.
        if (Process.GetProcessesByName("VisorVM").Length == 0 && VmExiste())   // arrancando aún (sin eventos): no abrir otro
        {
            AllowSetForegroundWindow(-1);
            try { Process.Start(Path.Combine(Fase4, "host", "VisorVM.exe")); }
            catch (Exception ex) { Trace.WriteLine("VisorVM: " + ex.Message); }   // iniciar.ps1 lo intenta de nuevo
        }
        Script("iniciar.ps1");
    }

    static void Salir() { if (!AvisarVisorVM(false)) Script("pausar.ps1"); }
    // Preferencia del usuario, en su propio registro (HKCU, sin administrador). Por defecto activado.
    static bool Automatico
    {
        get { return ((Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "CambioAutomatico", 1) as int?) ?? 1) != 0; }   // sin la clave: activado
        set { Microsoft.Win32.Registry.SetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "CambioAutomatico", value ? 1 : 0); }
    }

    // Postura de la Surface: Windows avisa con WM_SETTINGCHANGE "ConvertibleSlateMode" al quitar, poner o doblar el
    // teclado; SM_CONVERTIBLESLATEMODE = 0 es modo tableta. Sin sondeo.
    class Postura : NativeWindow
    {
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
        [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool RegisterHotKey(IntPtr h, int id, uint mod, uint vk);
        public Postura()
        {
            CreateHandle(new CreateParams());
            // Win+Alt+A = Ir a Android (no figura en la lista de atajos de Windows 11, verificado 18/11). MOD_WIN|MOD_ALT|MOD_NOREPEAT.
            if (!RegisterHotKey(Handle, 1, 0x0008 | 0x0001 | 0x4000, 0x41)) Trace.WriteLine("Win+Alt+A ya está ocupado");
        }
        protected override void WndProc(ref Message m)
        {
            base.WndProc(ref m);
            if (m.Msg == 0x0312) { Entrar(); return; }   // WM_HOTKEY
            if (m.Msg != 0x001A || m.LParam == IntPtr.Zero || !Automatico) return;   // WM_SETTINGCHANGE
            if (System.Runtime.InteropServices.Marshal.PtrToStringUni(m.LParam) != "ConvertibleSlateMode") return;
            CambioTeclado(GetSystemMetrics(0x2003) == 0);
        }
    }

    // ---- Android no se enciende ni se cambia de golpe (pedido del usuario 16/11). Al quitar el teclado se pregunta
    // "¿Pasar a Android?"; al ponerlo con Android delante, "¿Volver a Windows?" (volver = suspender). Así la RAM de Android
    // (12 GB fijos) no se ocupa hasta que el usuario lo enciende. Sin respuesta en 15 s gana quedarse donde está.
    enum EstadoAndroid { Apagado, Suspendido, Activo, Encendiendo }
    static EstadoAndroid Estado()
    {
        try
        {
            using (var s = new System.Management.ManagementObjectSearcher(@"root\virtualization\v2",
                "SELECT EnabledState FROM Msvm_ComputerSystem WHERE ElementName='SurfaceDualAndroid'"))
                foreach (System.Management.ManagementObject o in s.Get())
                {
                    ushort e = Convert.ToUInt16(o["EnabledState"]);   // 2 corriendo, 9 en pausa, 3 apagada, 10 encendiendo
                    return e == 2 ? EstadoAndroid.Activo : e == 9 ? EstadoAndroid.Suspendido : e == 10 ? EstadoAndroid.Encendiendo : EstadoAndroid.Apagado;
                }
        }
        catch { }
        return EstadoAndroid.Apagado;
    }
    static bool AndroidDelante()   // el VisorVM residente marca este evento mientras su ventana está a la vista
    {
        EventWaitHandle e;
        if (!EventWaitHandle.TryOpenExisting(@"Local\SurfaceDualVisorVM.Visible", out e)) return false;
        using (e) return e.WaitOne(0);
    }
    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
    static bool Tableta() { return GetSystemMetrics(0x2003) == 0; }   // SM_CONVERTIBLESLATEMODE: 0 = sin teclado
    static string Recordado(string clave) { return Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", clave, "") as string ?? ""; }

    static void CambioTeclado(bool tableta)
    {
        if (tableta)
        {
            if (AndroidDelante()) return;
            bool apagado = Estado() == EstadoAndroid.Apagado;
            Decidir("AlQuitarTeclado", "android", "Teclado quitado",
                apagado ? "¿Pasar a Android?\nAndroid está apagado: tarda unos 35 segundos en encender." : "¿Pasar a Android?",
                "Pasar a Android", "Seguir en Windows", () => { if (Tableta()) Entrar(); });   // el teclado sigue quitado al responder
        }
        else
        {
            if (!AndroidDelante()) return;
            Decidir("AlPonerTeclado", "windows", "Teclado puesto", "¿Volver a Windows?\nAndroid queda suspendido (vuelve al instante).",
                "Volver a Windows", "Seguir en Android", () => { if (!Tableta()) Salir(); });
        }
    }
    // Si el usuario marcó "recordar", se hace sin preguntar; si no, pregunta táctil.
    static void Decidir(string clave, string valorSi, string titulo, string texto, string si, string no, Action hacer)
    {
        string r = Recordado(clave);
        if (r != "") { if (r == valorSi) hacer(); return; }
        Pregunta.Mostrar(titulo, texto, si, no, (aceptar, recordar) =>
        {
            if (recordar) Microsoft.Win32.Registry.SetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", clave, aceptar ? valorSi : "quedarse");
            if (aceptar) hacer();
        });
    }

    // Diálogo táctil: botones grandes, siempre encima y sin quitar el foco (el Visor a pantalla completa sigue activo).
    // Una sola pregunta a la vez: si el teclado se quita y se pone rápido, la nueva sustituye a la vieja (que cuenta como "no").
    class Pregunta : Form
    {
        static Pregunta actual;
        Action<bool, bool> fin;
        bool respondida;
        readonly CheckBox recordar;
        protected override bool ShowWithoutActivation { get { return true; } }
        protected override CreateParams CreateParams { get { var p = base.CreateParams; p.ExStyle |= 0x8; return p; } }   // WS_EX_TOPMOST

        public static void Mostrar(string titulo, string texto, string si, string no, Action<bool, bool> fin)
        {
            if (actual != null) { actual.recordar.Checked = false; actual.Responder(false); }   // sustituida: no cuenta como respuesta ni se recuerda
            actual = new Pregunta(titulo, texto, si, no, fin);
            actual.Show();
        }

        // Aspecto del ContentDialog de Windows 11 (pedido del usuario 16/11: el anterior era grande y borroso): proceso con
        // conciencia de DPI (Main), medidas en píxeles lógicos × escala, tema claro/oscuro del usuario, esquinas redondeadas
        // de DWM y botón principal con el color de acento.
        [System.Runtime.InteropServices.DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr h, int a, ref int v, int t);
        [System.Runtime.InteropServices.DllImport("dwmapi.dll")] static extern int DwmGetColorizationColor(out uint c, out bool opaco);

        Pregunta(string titulo, string texto, string si, string no, Action<bool, bool> fin)
        {
            this.fin = fin;
            float e; using (var g = CreateGraphics()) e = g.DpiX / 96f;   // escala de la pantalla (p. ej. 2 en la Surface)
            Func<float, int> px = v => (int)Math.Round(v * e);
            bool claro = ((Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize",
                                                              "AppsUseLightTheme", 1) as int?) ?? 1) != 0;
            Color fondo = claro ? Color.White : Color.FromArgb(0x2B, 0x2B, 0x2B), pie = claro ? Color.FromArgb(0xF3, 0xF3, 0xF3) : Color.FromArgb(0x20, 0x20, 0x20);
            Color tinta = claro ? Color.FromArgb(0x1A, 0x1A, 0x1A) : Color.White, tenue = claro ? Color.FromArgb(0x5F, 0x5F, 0x5F) : Color.FromArgb(0xC5, 0xC5, 0xC5);
            uint c; bool op; Color acento = DwmGetColorizationColor(out c, out op) == 0 ? Color.FromArgb(255, Color.FromArgb((int)c)) : Color.FromArgb(0x00, 0x5F, 0xB8);

            int ancho = px(480), margen = px(24), altoPie = px(80), altoBoton = px(40);
            string[] lineas = texto.Split('\n');
            var fTitulo = new Font("Segoe UI Variable Display", 20f * e, FontStyle.Bold, GraphicsUnit.Pixel);
            var fTexto = new Font("Segoe UI Variable Text", 14f * e, GraphicsUnit.Pixel);
            var cuerpo = lineas.Length > 1 ? lineas[1] : "";
            int altoCuerpo = cuerpo == "" ? 0 : TextRenderer.MeasureText(cuerpo, fTexto, new Size(ancho - 2 * margen, 0), TextFormatFlags.WordBreak).Height;
            int alto = margen + px(28) + px(8) + altoCuerpo + px(16) + px(24) + margen + altoPie;

            Text = titulo; FormBorderStyle = FormBorderStyle.None; StartPosition = FormStartPosition.Manual; ShowInTaskbar = false;
            var pantalla = Screen.PrimaryScreen.Bounds;
            Bounds = new Rectangle(pantalla.X + (pantalla.Width - ancho) / 2, pantalla.Y + (pantalla.Height - alto) / 2, ancho, alto);
            BackColor = fondo; ForeColor = tinta;
            int y = margen;
            Controls.Add(new Label { Text = lineas[0], Font = fTitulo, ForeColor = tinta, AutoSize = false, Bounds = new Rectangle(margen, y, ancho - 2 * margen, px(28)) });
            y += px(36);
            if (cuerpo != "") { Controls.Add(new Label { Text = cuerpo, Font = fTexto, ForeColor = tenue, Bounds = new Rectangle(margen, y, ancho - 2 * margen, altoCuerpo) }); y += altoCuerpo + px(16); }
            recordar = new CheckBox { Text = "Recordar mi elección", Font = fTexto, ForeColor = tinta, Bounds = new Rectangle(margen, y, ancho - 2 * margen, px(24)) };
            Controls.Add(recordar);
            var zonaPie = new Panel { BackColor = pie, Bounds = new Rectangle(0, alto - altoPie, ancho, altoPie) };
            int anchoBoton = (ancho - 2 * margen - px(8)) / 2, yb = (altoPie - altoBoton) / 2;
            var bSi = new Boton(si, acento, Color.White, px(4), fTexto) { Bounds = new Rectangle(margen, yb, anchoBoton, altoBoton) };
            var bNo = new Boton(no, claro ? Color.FromArgb(0xFB, 0xFB, 0xFB) : Color.FromArgb(0x37, 0x37, 0x37), tinta, px(4), fTexto,
                                claro ? Color.FromArgb(0xE5, 0xE5, 0xE5) : Color.FromArgb(0x45, 0x45, 0x45))
                      { Bounds = new Rectangle(margen + anchoBoton + px(8), yb, anchoBoton, altoBoton) };
            bSi.Click += (s, ev) => Responder(true);
            bNo.Click += (s, ev) => Responder(false);
            zonaPie.Controls.Add(bSi); zonaPie.Controls.Add(bNo); Controls.Add(zonaPie);
            var limite = new System.Windows.Forms.Timer { Interval = 15000 };
            limite.Tick += (s, e) => { limite.Stop(); recordar.Checked = false; Responder(false); };   // sin respuesta: se queda y no se recuerda
            limite.Start();
        }

        protected override void OnHandleCreated(EventArgs ev)
        {
            base.OnHandleCreated(ev);
            int redondo = 2; DwmSetWindowAttribute(Handle, 33, ref redondo, 4);   // DWMWA_WINDOW_CORNER_PREFERENCE = redondeado
        }

        // Botón plano redondeado como los de Windows 11 (los de WinForms se ven de Windows 7).
        sealed class Boton : Control
        {
            readonly Color relleno, borde; readonly int radio; bool encima, pulsado;
            public Boton(string texto, Color relleno, Color tinta, int radio, Font letra, Color? borde = null)
            {
                Text = texto; this.relleno = relleno; ForeColor = tinta; this.radio = radio; Font = letra; this.borde = borde ?? relleno;
                SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint | ControlStyles.SupportsTransparentBackColor, true);
                Cursor = Cursors.Hand;
            }
            protected override void OnMouseEnter(EventArgs e) { encima = true; Invalidate(); base.OnMouseEnter(e); }
            protected override void OnMouseLeave(EventArgs e) { encima = pulsado = false; Invalidate(); base.OnMouseLeave(e); }
            protected override void OnMouseDown(MouseEventArgs e) { pulsado = true; Invalidate(); base.OnMouseDown(e); }
            protected override void OnMouseUp(MouseEventArgs e) { pulsado = false; Invalidate(); base.OnMouseUp(e); }
            protected override void OnPaint(PaintEventArgs e)
            {
                var g = e.Graphics; g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                g.Clear(Parent != null ? Parent.BackColor : BackColor);
                var c = pulsado ? ControlPaint.Dark(relleno, 0.1f) : encima ? ControlPaint.Light(relleno, 0.15f) : relleno;
                var r = new Rectangle(0, 0, Width - 1, Height - 1); int d = radio * 2;
                using (var p = new System.Drawing.Drawing2D.GraphicsPath())
                {
                    p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
                    p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90); p.CloseFigure();
                    using (var b = new SolidBrush(c)) g.FillPath(b, p);
                    using (var l = new Pen(borde)) g.DrawPath(l, p);
                }
                TextRenderer.DrawText(g, Text, Font, ClientRectangle, ForeColor, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            }
        }

        void Responder(bool aceptar)
        {
            if (respondida) return;
            respondida = true;
            if (actual == this) actual = null;
            bool r = recordar.Checked;
            Close();
            fin(aceptar, r);
        }
    }

    static void Apagar() { Script("apagar.ps1"); }   // apagado ordenado; libera los 12 GB
    // Ícono propio dibujado al vuelo (verde Android, "A"), sin archivos extra.
    static Icon Icono()
    {
        var b = new Bitmap(32, 32);
        using (var g = Graphics.FromImage(b))
        {
            g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
            g.FillEllipse(new SolidBrush(Color.FromArgb(0x3D, 0xDC, 0x84)), 1, 1, 30, 30);
            using (var f = new Font("Segoe UI", 15, FontStyle.Bold, GraphicsUnit.Pixel))
                g.DrawString("A", f, Brushes.Black, new RectangleF(0, 0, 32, 32), new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center });
        }
        return Icon.FromHandle(b.GetHicon());
    }
}
