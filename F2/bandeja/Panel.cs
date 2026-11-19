// PanelRapido rápido de Surface Dual: se abre al tocar el ícono de la bandeja, con el aspecto de Configuración rápida de
// Windows 11 (Win+A). Windows no admite botones de terceros dentro de esa Configuración rápida (verificado 18/11), así
// que es un panel propio: acrílico de DWM, esquinas redondeadas, tema y color de acento del sistema. Se crea al abrirlo
// y se destruye al cerrarlo: oculto no existe (sin temporizadores ni consultas).
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Management;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("Surface Dual")]   // nombre que muestra Windows en "Otros iconos de la bandeja del sistema"

static partial class Bandeja
{
    static void Olvidar()
    {
        foreach (var c in new[] { "AlQuitarTeclado", "AlPonerTeclado" })
            Microsoft.Win32.Registry.SetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", c, "");
    }
    static bool AutoApagar
    {
        get { return ((Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "AutoApagarMin", 60) as int?) ?? 60) > 0; }
        set { Microsoft.Win32.Registry.SetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "AutoApagarMin", value ? 60 : 0); }
    }

    // Brillo por WMI (root\wmi, WmiMonitorBrightness / WmiSetBrightness, como lo hace Windows). Cada llamada tarda
    // decenas de ms: un solo hilo aplica siempre el último valor pedido y descarta los intermedios al arrastrar.
    static int LeerBrillo()
    {
        try
        {
            using (var s = new ManagementObjectSearcher(@"root\wmi", "SELECT CurrentBrightness FROM WmiMonitorBrightness WHERE Active=TRUE"))
                foreach (ManagementObject o in s.Get()) return Convert.ToInt32(o["CurrentBrightness"]);
        }
        catch { }
        return -1;   // sin pantalla con brillo regulable (p. ej. monitor externo): el panel no muestra el control
    }
    static readonly object cerrojoBrillo = new object();
    static int brilloPedido = -1; static bool fijandoBrillo;
    static void PedirBrillo(int v)
    {
        lock (cerrojoBrillo) { brilloPedido = v; if (fijandoBrillo) return; fijandoBrillo = true; }
        ThreadPool.QueueUserWorkItem(_ =>
        {
            for (int hecho = -1; ; )
            {
                int o;
                lock (cerrojoBrillo) { o = brilloPedido; if (o == hecho) { fijandoBrillo = false; return; } }
                try
                {
                    using (var s = new ManagementObjectSearcher(@"root\wmi", "SELECT * FROM WmiMonitorBrightnessMethods WHERE Active=TRUE"))
                        foreach (ManagementObject m in s.Get()) m.InvokeMethod("WmiSetBrightness", new object[] { 1u, (byte)o });
                }
                catch (Exception ex) { System.Diagnostics.Trace.WriteLine("brillo: " + ex.Message); }
                hecho = o;
            }
        });
    }

    sealed class PanelRapido : Form
    {
        static PanelRapido abierto;
        static int cerradoEn = Environment.TickCount - 10000;

        // Tocar el ícono abre o cierra. Con el panel abierto, tocar el ícono primero le quita el foco (se cierra) y luego
        // llega el clic: si acaba de cerrarse por eso, no se vuelve a abrir.
        public static void Alternar(string tema = null, bool prueba = false)
        {
            if (abierto != null) { abierto.Close(); return; }
            if (!prueba && Environment.TickCount - cerradoEn < 400) return;
            abierto = new PanelRapido(tema, prueba);
            abierto.Show();
            abierto.Activate(); SetForegroundWindow(abierto.Handle);
        }

        [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr h, int a, ref int v, int t);
        [DllImport("dwmapi.dll")] static extern int DwmExtendFrameIntoClientArea(IntPtr h, ref Margenes m);
        [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
        [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr hdc);
        [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr hdc);
        [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr hdc, IntPtr o);
        [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr o);
        [DllImport("gdi32.dll")] static extern bool BitBlt(IntPtr d, int x, int y, int w, int h, IntPtr s, int sx, int sy, int rop);
        struct Margenes { public int l, r, t, b; }

        readonly bool claro, vidrio, prueba;
        readonly float e;
        internal readonly Color tinta, tenue, tarjeta, tarjetaEncima, borde, acento, sobreAcento, pie, pista, pulgar;
        readonly Color solido, velo;
        internal readonly Font fTitulo, fTexto, fPequena, fIcono, fFlecha;
        readonly int yPie;
        readonly Principal principal;
        readonly Boton apagar;
        readonly Deslizador brillo;
        readonly Etiqueta memoria;
        readonly System.Windows.Forms.Timer animacion = new System.Windows.Forms.Timer { Interval = 10 };
        internal int px(float v) { return (int)Math.Round(v * e); }

        PanelRapido(string tema, bool prueba)
        {
            this.prueba = prueba;
            // Configuración rápida sigue el modo de Windows (barra de tareas), no el de las aplicaciones.
            claro = tema != null ? tema == "claro" : ((Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize",
                                                                                         "SystemUsesLightTheme", 1) as int?) ?? 1) != 0;
            bool transparencia = ((Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize",
                                                                     "EnableTransparency", 1) as int?) ?? 1) != 0;
            using (var g = CreateGraphics()) e = g.DpiX / 96f;
            // Colores de WinUI 3 (Fluent): texto, relleno de controles y trazos sobre acrílico; acento como los botones de Windows.
            tinta = claro ? Color.FromArgb(0xE4, 0, 0, 0) : Color.White;
            tenue = claro ? Color.FromArgb(0x9E, 0, 0, 0) : Color.FromArgb(0xC8, 255, 255, 255);
            tarjeta = claro ? Color.FromArgb(0xB3, 255, 255, 255) : Color.FromArgb(0x0F, 255, 255, 255);
            tarjetaEncima = claro ? Color.FromArgb(0x80, 0xF9, 0xF9, 0xF9) : Color.FromArgb(0x15, 255, 255, 255);
            borde = claro ? Color.FromArgb(0x0F, 0, 0, 0) : Color.FromArgb(0x12, 255, 255, 255);
            pie = claro ? Color.FromArgb(0x08, 0, 0, 0) : Color.FromArgb(0x30, 0, 0, 0);
            pista = claro ? Color.FromArgb(0x72, 0, 0, 0) : Color.FromArgb(0x8B, 255, 255, 255);
            pulgar = claro ? Color.White : Color.FromArgb(0x45, 0x45, 0x45);
            solido = claro ? Color.FromArgb(0xEE, 0xEE, 0xEE) : Color.FromArgb(0x2C, 0x2C, 0x2C);
            velo = Color.FromArgb(claro ? 0x99 : 0x50, solido);   // tinte sobre el acrílico: más opaco, como el de Configuración rápida
            acento = Acento(claro);
            sobreAcento = claro ? Color.White : Color.Black;
            fTitulo = new Font("Segoe UI Variable Text Semibold", 15f * e, GraphicsUnit.Pixel);
            fTexto = new Font("Segoe UI Variable Text", 13f * e, GraphicsUnit.Pixel);
            fPequena = new Font("Segoe UI Variable Small", 12f * e, GraphicsUnit.Pixel);
            fIcono = new Font("Segoe Fluent Icons", 16f * e, GraphicsUnit.Pixel);
            fFlecha = new Font("Segoe Fluent Icons", 12f * e, GraphicsUnit.Pixel);

            Text = "Surface Dual"; AccessibleName = "Surface Dual: panel rápido";
            FormBorderStyle = FormBorderStyle.None; StartPosition = FormStartPosition.Manual; ShowInTaskbar = false; TopMost = true; KeyPreview = true;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.Opaque, true);

            // Medidas de Configuración rápida: 360 de ancho, márgenes de 20, controles táctiles de 44 o más.
            int ancho = px(360), m = px(20), interior = ancho - 2 * m, y = m;
            principal = new Principal(this) { Bounds = new Rectangle(m, y, interior, px(72)) };
            y += px(72) + px(16);
            int anchoFicha = (interior - 2 * px(12)) / 3, altoFicha = px(48 + 6 + 34);
            var fichas = new Pieza[]
            {
                new Ficha(this, "Preguntar al quitar teclado", "\uE765", "Preguntar al quitar o poner el teclado si se cambia de sistema",
                          () => Automatico, v => Automatico = v),
                new Ficha(this, "Apagar tras 1 h en pausa", "\uE823", "Apagar Android tras 1 hora en pausa para liberar su memoria",
                          () => AutoApagar, v => AutoApagar = v),
                new Ficha(this, "Olvidar elecciones", "\uE72C", "Volver a preguntar aunque se marcara Recordar mi elección", null, null),
            };
            for (int i = 0; i < fichas.Length; i++) fichas[i].Bounds = new Rectangle(m + i * (anchoFicha + px(12)), y, anchoFicha, altoFicha);
            fichas[2].Enabled = Recordado("AlQuitarTeclado") != "" || Recordado("AlPonerTeclado") != "";
            ((Ficha)fichas[2]).Accion = () => { Olvidar(); fichas[2].Enabled = false; };
            y += altoFicha + px(8);
            brillo = new Deslizador(this) { Bounds = new Rectangle(m - px(4), y, interior + px(8), px(44)), Visible = false };
            y += px(44) + px(12);
            yPie = y;
            memoria = new Etiqueta(this) { Bounds = new Rectangle(m, y, interior - px(150), px(60)) };
            apagar = new Boton(this, "Apagar Android", "\uE7E8") { Bounds = new Rectangle(ancho - m - px(150) + px(8), y + px(8), px(150), px(44)), Enabled = false };
            apagar.Accion = () => { Apagar(); principal.Subtitulo = "Apagando…"; apagar.Enabled = false; };
            Controls.Add(principal); Controls.AddRange(fichas); Controls.Add(brillo); Controls.Add(memoria); Controls.Add(apagar);

            // Encima de la barra de tareas, a la derecha y a 12 del borde, en la pantalla donde está el puntero (como Win+A).
            var zona = Screen.FromPoint(Cursor.Position);
            var t = zona.WorkingArea;
            Size = new Size(ancho, y + px(60));
            bool barraArriba = t.Top > zona.Bounds.Top;
            destino = new Point(t.Right - ancho - px(12), barraArriba ? t.Top + px(12) : t.Bottom - Height - px(12));
            Location = new Point(destino.X, destino.Y + (barraArriba ? -1 : 1) * px(40));

            // Fondo acrílico solo si Windows lo admite y la transparencia está activada; si no, color liso del tema.
            int v1 = 2; DwmSetWindowAttribute(Handle, 33, ref v1, 4);                       // DWMWA_WINDOW_CORNER_PREFERENCE = redondeado
            v1 = claro ? 0 : 1; DwmSetWindowAttribute(Handle, 20, ref v1, 4);               // DWMWA_USE_IMMERSIVE_DARK_MODE
            v1 = 3;                                                                         // DWMSBT_TRANSIENTWINDOW = acrílico (Windows 11 22621+)
            vidrio = transparencia && DwmSetWindowAttribute(Handle, 38, ref v1, 4) == 0;
            if (vidrio) { var mg = new Margenes { l = -1, r = -1, t = -1, b = -1 }; vidrio = DwmExtendFrameIntoClientArea(Handle, ref mg) == 0; }
            BackColor = vidrio ? Color.Black : solido;

            // Entrada como la de Windows: sube 40 px y se frena en ~200 ms. El temporizador solo vive durante la animación.
            int inicio = 0, y0 = Location.Y;
            animacion.Tick += (s, a) =>
            {
                if (inicio == 0) inicio = Environment.TickCount;
                double k = Math.Min(1, (Environment.TickCount - inicio) / 200.0), suave = 1 - Math.Pow(1 - k, 3);
                Location = new Point(destino.X, (int)Math.Round(y0 + (destino.Y - y0) * suave));
                if (k >= 1) animacion.Stop();
            };

            // Estado de Android y brillo en segundo plano (WMI tarda): el panel sale al instante y se completa al llegar.
            ThreadPool.QueueUserWorkItem(_ =>
            {
                var est = Estado(); int b = LeerBrillo();
                try { BeginInvoke((Action)(() => Mostrar(est, b))); } catch (Exception) { }   // ya cerrado (en un hilo del sistema, una excepción tumbaría la Bandeja)
            });
        }
        readonly Point destino;

        static Color Acento(bool claro)
        {
            try
            {   // UISettings.GetColorValue: el acento del usuario; los botones de Windows 11 usan Dark1 (claro) y Light2 (oscuro).
                var c = new Windows.UI.ViewManagement.UISettings().GetColorValue(claro ? Windows.UI.ViewManagement.UIColorType.AccentDark1
                                                                                       : Windows.UI.ViewManagement.UIColorType.AccentLight2);
                return Color.FromArgb(255, c.R, c.G, c.B);
            }
            catch { return claro ? Color.FromArgb(0x00, 0x5F, 0xB8) : Color.FromArgb(0x4C, 0xC2, 0xFF); }
        }

        void Mostrar(EstadoAndroid est, int b)
        {
            if (IsDisposed) return;   // se cerró con la consulta en camino (revisión Gemini/Nemotron 18/11)
            principal.Subtitulo = est == EstadoAndroid.Activo ? "Encendido · al instante"
                                : est == EstadoAndroid.Suspendido ? "Listo · en pausa"
                                : est == EstadoAndroid.Encendiendo ? "Encendiendo…"
                                : "Apagado · tarda unos 35 s";
            memoria.Text = est == EstadoAndroid.Apagado ? "Android no usa memoria" : "Android usa 12 GB";
            apagar.Enabled = est != EstadoAndroid.Apagado;
            if (b >= 0) { brillo.Nivel = b; brillo.Visible = true; }
            memoria.Invalidate();
        }

        protected override CreateParams CreateParams { get { var p = base.CreateParams; p.ExStyle |= 0x80; return p; } }   // WS_EX_TOOLWINDOW: fuera de Alt+Tab
        protected override void OnShown(EventArgs a) { base.OnShown(a); principal.Focus(); animacion.Start(); }
        protected override void OnDeactivate(EventArgs a) { base.OnDeactivate(a); if (!prueba) Close(); }
        protected override void OnKeyDown(KeyEventArgs a) { if (a.KeyCode == Keys.Escape) Close(); else base.OnKeyDown(a); }
        protected override void OnFormClosed(FormClosedEventArgs a)
        {
            animacion.Dispose();
            foreach (var f in new[] { fTitulo, fTexto, fPequena, fIcono, fFlecha }) f.Dispose();
            if (abierto == this) abierto = null;
            cerradoEn = Environment.TickCount;
            base.OnFormClosed(a);
            if (prueba) Application.Exit();
        }
        public void IrAAndroid() { Entrar(); Close(); }   // primero Entrar: aún tenemos el permiso de poner el Visor delante

        // Dibujo con canal alfa: cada pieza se pinta en un mapa de bits ARGB (fondo transparente = se ve el acrílico) y
        // se copia entero a la ventana. El texto de GDI (TextRenderer) no lleva alfa y sobre el acrílico saldría hueco.
        protected override void OnPaintBackground(PaintEventArgs a) { }
        protected override void OnPaint(PaintEventArgs a) { Pintar(a.Graphics, this, null); }
        internal void Pintar(Graphics destino, Control c, Action<Graphics> dibujar)
        {
            int w = c.ClientSize.Width, h = c.ClientSize.Height;
            if (w <= 0 || h <= 0) return;
            using (var b = new Bitmap(w, h, PixelFormat.Format32bppPArgb))
            {
                using (var g = Graphics.FromImage(b))
                {
                    g.Clear(vidrio ? velo : solido);
                    if (c != this) g.TranslateTransform(-c.Left, -c.Top);
                    using (var r = new SolidBrush(pie)) g.FillRectangle(r, 0, yPie, ClientSize.Width, ClientSize.Height - yPie);
                    using (var l = new Pen(borde)) g.DrawLine(l, 0, yPie, ClientSize.Width, yPie);
                    g.ResetTransform();
                    g.SmoothingMode = SmoothingMode.AntiAlias; g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
                    if (dibujar != null) dibujar(g);
                }
                IntPtr hdc = destino.GetHdc(), mem = CreateCompatibleDC(hdc), hb = b.GetHbitmap(Color.FromArgb(0)), antes = SelectObject(mem, hb);
                BitBlt(hdc, 0, 0, w, h, mem, 0, 0, 0x00CC0020);   // SRCCOPY: copia también el alfa
                SelectObject(mem, antes); DeleteObject(hb); DeleteDC(mem); destino.ReleaseHdc(hdc);
            }
        }

        internal static GraphicsPath Redondo(RectangleF r, float radio)
        {
            var p = new GraphicsPath(); float d = radio * 2;
            p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90); p.CloseFigure();
            return p;
        }
        internal static void Centrado(Graphics g, string s, Font f, Color c, RectangleF r, StringAlignment h = StringAlignment.Center)
        {
            using (var b = new SolidBrush(c))
            using (var sf = new StringFormat { Alignment = h, LineAlignment = StringAlignment.Center, Trimming = StringTrimming.EllipsisWord })
                g.DrawString(s, f, b, r, sf);
        }
    }

    // Pieza del panel: control propio con foco, teclado (Intro/Espacio), nombre para Narrador y anillo de foco de Windows 11.
    abstract class Pieza : Control
    {
        protected readonly PanelRapido p;
        protected bool encima, pulsado;
        public Action Accion;
        protected Pieza(PanelRapido p, string nombre, AccessibleRole rol)
        {
            this.p = p; Text = AccessibleName = nombre; AccessibleRole = rol; TabStop = true;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.Opaque | ControlStyles.Selectable, true);
        }
        protected abstract void Dibujar(Graphics g);
        public virtual bool? Marcada { get { return null; } }
        public virtual string Valor { get { return null; } }
        public virtual void Pulsar() { if (Accion != null) Accion(); }
        protected override void OnPaintBackground(PaintEventArgs a) { }
        protected override void OnPaint(PaintEventArgs a)
        {
            p.Pintar(a.Graphics, this, g =>
            {
                Dibujar(g);
                if (Focused && ShowFocusCues)   // anillo de foco solo al usar el teclado, como Windows
                    using (var r = PanelRapido.Redondo(new RectangleF(1, 1, Width - 3, Height - 3), p.px(6)))
                    using (var l = new Pen(p.tinta, p.px(2))) g.DrawPath(l, r);
            });
        }
        protected Color Relleno(Color c) { return !Enabled ? c : pulsado ? Color.FromArgb(c.A * 8 / 10, c) : encima ? Color.FromArgb(c.A * 9 / 10, c) : c; }
        protected override void OnMouseEnter(EventArgs a) { encima = true; Invalidate(); base.OnMouseEnter(a); }
        protected override void OnMouseLeave(EventArgs a) { encima = pulsado = false; Invalidate(); base.OnMouseLeave(a); }
        protected override void OnMouseDown(MouseEventArgs a) { if (a.Button == MouseButtons.Left) { pulsado = true; Focus(); Invalidate(); } base.OnMouseDown(a); }
        protected override void OnMouseUp(MouseEventArgs a)
        {
            bool era = pulsado; pulsado = false; Invalidate(); base.OnMouseUp(a);
            if (era && ClientRectangle.Contains(a.Location)) Pulsar();
        }
        protected override void OnKeyDown(KeyEventArgs a)
        {
            if (a.KeyCode == Keys.Enter || a.KeyCode == Keys.Space) { Pulsar(); a.Handled = true; }
            base.OnKeyDown(a);
        }
        protected override void OnGotFocus(EventArgs a) { Invalidate(); base.OnGotFocus(a); }
        protected override void OnLostFocus(EventArgs a) { Invalidate(); base.OnLostFocus(a); }
        protected override void OnEnabledChanged(EventArgs a) { Invalidate(); base.OnEnabledChanged(a); }
        protected override AccessibleObject CreateAccessibilityInstance() { return new Accesible(this); }
        sealed class Accesible : ControlAccessibleObject
        {
            readonly Pieza c;
            public Accesible(Pieza c) : base(c) { this.c = c; }
            public override AccessibleStates State
            {
                get { var s = base.State; if (c.Marcada == true) s |= AccessibleStates.Checked; return s; }
            }
            public override string Value { get { return c.Valor ?? base.Value; } }
            public override string DefaultAction { get { return c.Marcada == null ? "Pulsar" : "Alternar"; } }
            public override void DoDefaultAction() { if (c.Enabled) c.Pulsar(); }
        }
    }

    // Botón principal "Ir a Android": ancho, color de acento, insignia con el robot de Android y estado debajo.
    sealed class Principal : Pieza
    {
        string subtitulo = "";
        public Principal(PanelRapido p) : base(p, "Ir a Android", AccessibleRole.PushButton) { Accion = p.IrAAndroid; }
        public string Subtitulo { set { subtitulo = value; AccessibleDescription = value; Invalidate(); } }
        protected override void Dibujar(Graphics g)
        {
            using (var r = PanelRapido.Redondo(new RectangleF(0, 0, Width - 1, Height - 1), p.px(8)))
            using (var b = new SolidBrush(Relleno(p.acento))) g.FillPath(b, r);
            float s = p.px(44), x = p.px(14), y = (Height - s) / 2;
            using (var b = new SolidBrush(Color.FromArgb(0x3D, 0xDC, 0x84))) g.FillEllipse(b, x, y, s, s);   // verde Android
            Robot(g, x + s * 0.2f, y + s * 0.22f, s * 0.6f);
            float tx = x + s + p.px(14), ancho = Width - tx - p.px(40);
            PanelRapido.Centrado(g, "Ir a Android", p.fTitulo, p.sobreAcento, new RectangleF(tx, Height / 2f - p.px(22), ancho, p.px(24)), StringAlignment.Near);
            PanelRapido.Centrado(g, subtitulo, p.fPequena, Color.FromArgb(0xD0, p.sobreAcento), new RectangleF(tx, Height / 2f + p.px(1), ancho, p.px(20)), StringAlignment.Near);
            PanelRapido.Centrado(g, "\uE76C", new Font(p.fIcono.FontFamily, p.fIcono.Size * 0.75f, GraphicsUnit.Pixel), p.sobreAcento,
                           new RectangleF(Width - p.px(40), 0, p.px(28), Height));   // flecha ›
        }
        // Cabeza del robot de Android (azul marino de la marca sobre el verde), dibujada sin archivos.
        static void Robot(Graphics g, float x, float y, float s)
        {
            var marino = Color.FromArgb(0x07, 0x30, 0x42);
            using (var b = new SolidBrush(marino)) g.FillPie(b, x, y + s * 0.3f, s, s * 0.9f, 180, 180);
            using (var l = new Pen(marino, s * 0.07f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
            {
                g.DrawLine(l, x + s * 0.27f, y + s * 0.42f, x + s * 0.14f, y + s * 0.2f);
                g.DrawLine(l, x + s * 0.73f, y + s * 0.42f, x + s * 0.86f, y + s * 0.2f);
            }
            using (var b = new SolidBrush(Color.FromArgb(0x3D, 0xDC, 0x84)))
            {
                float o = s * 0.11f;
                g.FillEllipse(b, x + s * 0.3f - o / 2, y + s * 0.56f - o / 2, o, o);
                g.FillEllipse(b, x + s * 0.7f - o / 2, y + s * 0.56f - o / 2, o, o);
            }
        }
    }

    // Ficha de Configuración rápida: botón redondeado con ícono (relleno de acento si está activa) y nombre debajo.
    sealed class Ficha : Pieza
    {
        readonly string icono; readonly Func<bool> leer; readonly Action<bool> escribir;
        public Ficha(PanelRapido p, string nombre, string icono, string ayuda, Func<bool> leer, Action<bool> escribir)
            : base(p, nombre, leer != null ? AccessibleRole.CheckButton : AccessibleRole.PushButton)
        { this.icono = icono; this.leer = leer; this.escribir = escribir; AccessibleDescription = ayuda; }
        public override bool? Marcada { get { return leer == null ? (bool?)null : leer(); } }
        public override void Pulsar()
        {
            if (escribir != null) { escribir(!leer()); Invalidate(); AccessibilityNotifyClients(AccessibleEvents.StateChange, -1); }
            else base.Pulsar();
        }
        protected override void Dibujar(Graphics g)
        {
            bool activa = Marcada == true;
            var caja = new RectangleF(0.5f, 0.5f, Width - 1, p.px(48));
            using (var r = PanelRapido.Redondo(caja, p.px(6)))
            {
                using (var b = new SolidBrush(Relleno(activa ? p.acento : encima && Enabled ? p.tarjetaEncima : p.tarjeta))) g.FillPath(b, r);
                if (!activa) using (var l = new Pen(p.borde)) g.DrawPath(l, r);
            }
            Color tinta = activa ? p.sobreAcento : Enabled ? p.tinta : Color.FromArgb(0x5C, p.tinta);
            PanelRapido.Centrado(g, icono, p.fIcono, tinta, caja);
            PanelRapido.Centrado(g, Text, p.fPequena, Enabled ? p.tinta : Color.FromArgb(0x5C, p.tinta), new RectangleF(0, p.px(52), Width, Height - p.px(52)));
        }
    }

    // Botón del pie (ícono + texto, sin relleno hasta pasar por encima).
    sealed class Boton : Pieza
    {
        readonly string icono;
        public Boton(PanelRapido p, string nombre, string icono) : base(p, nombre, AccessibleRole.PushButton) { this.icono = icono; }
        protected override void Dibujar(Graphics g)
        {
            if (encima && Enabled)
                using (var r = PanelRapido.Redondo(new RectangleF(0.5f, 0.5f, Width - 1, Height - 1), p.px(6)))
                using (var b = new SolidBrush(Relleno(p.tarjetaEncima))) g.FillPath(b, r);
            Color c = Enabled ? p.tinta : Color.FromArgb(0x5C, p.tinta);
            PanelRapido.Centrado(g, icono, p.fIcono, c, new RectangleF(p.px(8), 0, p.px(28), Height));
            PanelRapido.Centrado(g, Text, p.fTexto, c, new RectangleF(p.px(40), 0, Width - p.px(44), Height), StringAlignment.Near);
        }
    }

    sealed class Etiqueta : Pieza
    {
        public Etiqueta(PanelRapido p) : base(p, "", AccessibleRole.StaticText) { TabStop = false; SetStyle(ControlStyles.Selectable, false); }
        protected override void Dibujar(Graphics g) { PanelRapido.Centrado(g, Text, p.fPequena, p.tenue, new RectangleF(0, 0, Width, Height), StringAlignment.Near); }
        protected override void OnTextChanged(EventArgs a) { AccessibleName = Text; base.OnTextChanged(a); }
    }

    // Deslizador de brillo como el de Windows: sol, pista con la parte llena de acento y pulgar redondo.
    sealed class Deslizador : Pieza
    {
        int valor;
        public Deslizador(PanelRapido p) : base(p, "Brillo", AccessibleRole.Slider) { }
        public int Nivel { get { return valor; } set { valor = Math.Max(0, Math.Min(100, value)); Invalidate(); } }
        public override string Valor { get { return valor + " %"; } }
        float X0 { get { return p.px(48); } }
        float X1 { get { return Width - p.px(18); } }
        void Cambiar(int v)
        {
            v = Math.Max(0, Math.Min(100, v));
            if (v == valor) return;
            Nivel = v; PedirBrillo(v);
            AccessibilityNotifyClients(AccessibleEvents.ValueChange, -1);
        }
        void DesdeRaton(int x) { Cambiar((int)Math.Round((x - X0) * 100 / (X1 - X0))); }
        protected override void OnMouseDown(MouseEventArgs a) { base.OnMouseDown(a); if (a.Button == MouseButtons.Left) DesdeRaton(a.X); }
        protected override void OnMouseMove(MouseEventArgs a) { base.OnMouseMove(a); if (pulsado) DesdeRaton(a.X); }
        public override void Pulsar() { }
        protected override bool IsInputKey(Keys k) { return k == Keys.Left || k == Keys.Right || k == Keys.Up || k == Keys.Down || base.IsInputKey(k); }
        protected override void OnKeyDown(KeyEventArgs a)
        {
            int d = a.KeyCode == Keys.Right || a.KeyCode == Keys.Up ? 1 : a.KeyCode == Keys.Left || a.KeyCode == Keys.Down ? -1
                  : a.KeyCode == Keys.PageUp ? 10 : a.KeyCode == Keys.PageDown ? -10 : a.KeyCode == Keys.Home ? -100 : a.KeyCode == Keys.End ? 100 : 0;
            if (d != 0) { Cambiar(valor + d); a.Handled = true; }
            base.OnKeyDown(a);
        }
        protected override void Dibujar(Graphics g)
        {
            float cy = Height / 2f, x = X0 + (X1 - X0) * valor / 100f, alto = p.px(4);
            PanelRapido.Centrado(g, "\uE706", p.fIcono, p.tinta, new RectangleF(p.px(4), 0, p.px(36), Height));   // sol
            using (var b = new SolidBrush(p.pista)) using (var r = PanelRapido.Redondo(new RectangleF(X0, cy - alto / 2, X1 - X0, alto), alto / 2)) g.FillPath(b, r);
            using (var b = new SolidBrush(p.acento)) using (var r = PanelRapido.Redondo(new RectangleF(X0, cy - alto / 2, Math.Max(alto, x - X0), alto), alto / 2)) g.FillPath(b, r);
            float ext = p.px(10), inte = pulsado ? p.px(5) : encima ? p.px(7) : p.px(6);
            using (var b = new SolidBrush(p.pulgar)) g.FillEllipse(b, x - ext, cy - ext, 2 * ext, 2 * ext);
            using (var l = new Pen(p.borde)) g.DrawEllipse(l, x - ext, cy - ext, 2 * ext, 2 * ext);
            using (var b = new SolidBrush(p.acento)) g.FillEllipse(b, x - inte, cy - inte, 2 * inte, 2 * inte);
        }
    }
}
