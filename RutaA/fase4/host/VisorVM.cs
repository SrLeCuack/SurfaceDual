// Visor WinForms para Android en Hyper-V por hv_sock (AF_HYPERV, vsock 5001).
// Protocolo: RutaA/fase4/pantalla_protocolo.md.
// Uso: VisorVM.exe [VmId]
using System;
using System.Diagnostics;
using System.Drawing;
using System.Management;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

class HvEndPoint : EndPoint
{
    readonly Guid vm, servicio;
    public HvEndPoint(Guid v, Guid s) { vm = v; servicio = s; }
    public override AddressFamily AddressFamily => (AddressFamily)34; // AF_HYPERV
    public override SocketAddress Serialize()
    {
        var a = new SocketAddress((AddressFamily)34, 36);
        byte[] v = vm.ToByteArray(), s = servicio.ToByteArray();
        for (int i = 0; i < 16; i++) { a[4 + i] = v[i]; a[20 + i] = s[i]; }
        return a;
    }
    public override EndPoint Create(SocketAddress a) => this;
}

class VisorVM : Form
{
    readonly Guid vmId;
    D3D pantalla;
    int ancho, alto;
    volatile bool terminar;
    bool primerCuadro;
    int volcados;
    // Medición con Windows bloqueado (DWM frena Present): SURFACEDUAL_SIN_PRESENTAR=1 mide transporte y Android sin pantalla.
    static readonly bool sinPresentar = Environment.GetEnvironmentVariable("SURFACEDUAL_SIN_PRESENTAR") == "1";
    // Pruebas sin quitarle la pantalla al usuario: SURFACEDUAL_FONDO=1 crea el Visor sin ventana visible; recibe la imagen y
    // la guarda en su textura (captura con el evento Local\SurfaceDualVisorVM.Captura) y manda los toques de adb igual.
    static readonly bool enFondo = Environment.GetEnvironmentVariable("SURFACEDUAL_FONDO") == "1";
    // Diagnóstico 0.a (SURFACEDUAL_DIAG_CUADROS=1): una línea por cuadro recibido en %TEMP%\visorvm_cuadros.log.
    static readonly bool diagCuadros = Environment.GetEnvironmentVariable("SURFACEDUAL_DIAG_CUADROS") == "1";
    long dSuma, dN, dNeg, dArea, aSuma, aN, aNeg;   // a*: franja de abajo (40 %, donde sale el teclado)
    Socket sockActual;

    public VisorVM(Guid vm)
    {
        vmId = vm;
        Text = "VisorVM";
        FormBorderStyle = FormBorderStyle.None;
        WindowState = FormWindowState.Maximized;
        BackColor = Color.Black;
        SetStyle(ControlStyles.Opaque, true);
        KeyPreview = true;
        // Arranque en frío (~36 s medidos hasta el primer cuadro): pantalla de encendido con logo, barra y frases (pedido del
        // usuario 16/11). Se quita con el primer cuadro.
        encendiendo = new Arranque { Dock = DockStyle.Fill };
        Controls.Add(encendiendo);
        // Por hora real (si la Surface duerme, un temporizador no avanza): se comprueba cada minuto mientras está suspendido.
        autoApagado.Interval = 60000;
        autoApagado.Tick += (s, e) =>
        {
            if (MinutosAutoApagado() <= 0) { autoApagado.Stop(); return; }   // desactivado en el menú mientras estaba suspendido
            if (DateTime.UtcNow >= vencimiento) { autoApagado.Stop(); Apagar(); }
        };
        // Windows bloqueado o en reposo con Android delante: pausar la VM (con GPU-P en marcha el SoC no llega al reposo
        // profundo: Gemini 16/11) y reanudarla al volver, sin cambiar de sistema; cuenta como suspendido para el apagado de 1 h.
        // Bloquear la tableta con Android delante (E17, 17/11): Android duerme y enseña su pantalla de bloqueo con la música, que
        // sigue sonando; la VM no se pausa salvo que el agente diga que no suena nada ("reposo"). Al desbloquear vuelve Android
        // (despierta en su pantalla de bloqueo). Solo "Cambiar a Windows" o el teclado salen de Android.
        ControlesCompartidos.Reposo = () => { if (bloqueado && visible.WaitOne(0)) EnPausa(); };
        Microsoft.Win32.SystemEvents.SessionSwitch += (s, e) =>
        {
            // Desbloqueo con Android ya oculto (teclado puesto en la pantalla de bloqueo): sin esto "bloqueado" quedaba en true y,
            // con Android otra vez delante, tras un reposo la VM no se reanudaba y un "reposo" del agente la pausaba.
            if (e.Reason == Microsoft.Win32.SessionSwitchReason.SessionUnlock && bloqueado && !visible.WaitOne(0)) { bloqueado = false; ControlesCompartidos.Enviar("bloqueo", "0"); return; }
            if (!visible.WaitOne(0)) return;
            if (e.Reason == Microsoft.Win32.SessionSwitchReason.SessionLock) { bloqueado = true; Tecla(142, true); Tecla(142, false); ControlesCompartidos.Enviar("bloqueo", "1"); }   // KEY_SLEEP
            else if (e.Reason == Microsoft.Win32.SessionSwitchReason.SessionUnlock) { bloqueado = false; DePausa(); ControlesCompartidos.Enviar("bloqueo", "0"); Despertar(); }
        };
        Microsoft.Win32.SystemEvents.PowerModeChanged += (s, e) =>
        {
            if (!visible.WaitOne(0)) return;
            if (e.Mode == Microsoft.Win32.PowerModes.Suspend) EnPausa();
            else if (e.Mode == Microsoft.Win32.PowerModes.Resume && !bloqueado) DePausa();   // sin pantalla de bloqueo: reanudar ya
        };
    }
    volatile bool bloqueado;
    void EnPausa() { estadosVm.Add(9); IniciarAutoApagado(); }
    void DePausa() { autoApagado.Stop(); estadosVm.Add(2); }
    // Suspendido: tras AutoApagarMin (menú de la Bandeja; por omisión 60, 0 = nunca) se apaga solo y libera la RAM.
    void IniciarAutoApagado()
    {
        if (InvokeRequired) { BeginInvoke((Action)IniciarAutoApagado); return; }   // los avisos de sistema llegan en otro hilo
        if (MinutosAutoApagado() > 0) { vencimiento = DateTime.UtcNow.AddMinutes(MinutosAutoApagado()); autoApagado.Start(); }
    }
    // Windows no deja que un proceso en segundo plano se ponga delante (Activate solo hace parpadear la barra de tareas;
    // medido 16/11: el Visor quedaba detrás). Una pulsación sintética de Alt cuenta como entrada propia y lo permite.
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    void ForzarFrente()
    {
        if (GetForegroundWindow() == Handle) return;
        keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero);   // Alt abajo/arriba
        SetForegroundWindow(Handle); Activate();
    }
    static int MinutosAutoApagado() { return (Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "AutoApagarMin", 60) as int?) ?? 60; }
    Arranque encendiendo;

    // ---- Encendido y apagado (pedido del usuario 16/11): la Bandeja sabe si Android está delante por este evento; "Apagar"
    // (botón del launcher o tras 1 h suspendido) apaga Android de forma ordenada y libera sus 12 GB (apagar.ps1).
    readonly EventWaitHandle visible = new EventWaitHandle(false, EventResetMode.ManualReset, @"Local\SurfaceDualVisorVM.Visible");
    readonly System.Windows.Forms.Timer autoApagado = new System.Windows.Forms.Timer();
    DateTime vencimiento;
    protected override void OnShown(EventArgs e) { visible.Set(); ThreadPool.QueueUserWorkItem(_ => Silencioso()); base.OnShown(e); }
    void Apagar()
    {
        visible.Reset(); Hide(); Enlace.Mostrado(false);
        // apagar.ps1 cierra este proceso y el audio, reanuda si está en pausa y pide el apagado ordenado (Stop-VM -Force).
        Process.Start(new ProcessStartInfo("powershell", "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" +
            System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "..", "apagar.ps1") + "\"") { UseShellExecute = false, CreateNoWindow = true });
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        // Esc va a Android ("atrás"); el visor se cierra con Alt+F4 (WndProc).
        base.OnKeyDown(e);
    }

    // ---- Entrada: toques (WM_POINTER) y ratón -> Android por vsock 5002 (entrada.cpp del composer) ----
    // Mensaje de 24 bytes: magia "SDTQ", tipo 0 abajo/1 mover/2 arriba, dedo, x, y (píxeles de Android), presión.
    Socket entrada;
    readonly System.Collections.Generic.Dictionary<int, int> dedos = new System.Collections.Generic.Dictionary<int, int>();   // puntero -> ranura
    readonly byte[] toque = new byte[24];

    void EnviarToque(uint tipo, int dedo, Point c, int tecla = -1, uint presion = 0)
    {
        Tocado();
        var d = Destino();
        if (tecla < 0 && (d.Width <= 0 || d.Height <= 0 || ancho <= 0)) return;
        int x = tecla >= 0 ? tecla : Math.Max(0, Math.Min(ancho - 1, (int)((c.X - d.X) * (long)ancho / d.Width)));
        int y = tecla >= 0 ? 0 : Math.Max(0, Math.Min(alto - 1, (int)((c.Y - d.Y) * (long)alto / d.Height)));
        BitConverter.GetBytes(0x51544453u).CopyTo(toque, 0); BitConverter.GetBytes(tipo).CopyTo(toque, 4);
        BitConverter.GetBytes(dedo).CopyTo(toque, 8); BitConverter.GetBytes(x).CopyTo(toque, 12);
        BitConverter.GetBytes(y).CopyTo(toque, 16); BitConverter.GetBytes(presion).CopyTo(toque, 20);
        if (entrada == null)
        {
            // pulsar y soltar llegan juntos; con la cola llena se guardan solo las sueltas (nunca una tecla pegada)
            if (tecla >= 0 && (teclasPendientes.Count < 32 || presion == 0)) teclasPendientes.Add((byte[])toque.Clone());
            ConectarEntrada(); return;
        }
        try { entrada.Send(toque); }
        catch (Exception) { try { entrada?.Close(); } catch { } entrada = null; }   // conexión caída: el siguiente toque reconecta
    }

    // Conectar en otro hilo: con Android arrancando, Connect espera ~30 s y en el hilo de la ventana la congelaba (pantalla
    // de carga negra, medido 16/11). Los toques de mientras se pierden; las teclas (p. ej. KEY_WAKEUP) se mandan al conectar.
    // Solo el hilo de la ventana toca entrada y teclasPendientes.
    int conectando;
    readonly System.Collections.Generic.List<byte[]> teclasPendientes = new System.Collections.Generic.List<byte[]>();
    void ConectarEntrada()
    {
        if (Interlocked.Exchange(ref conectando, 1) == 1) return;
        new Thread(() =>
        {
            try
            {
                var s = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1);   // sin NoDelay: AF_HYPERV lo rechaza
                s.Connect(new HvEndPoint(vmId, Guid.Parse("0000138a-facb-11e6-bd58-64006a7986d3")));   // vsock 5002
                BeginInvoke((Action)(() =>
                {
                    entrada = s;
                    try { foreach (var t in teclasPendientes) s.Send(t); } catch { }
                    teclasPendientes.Clear();
                    conectando = 0;   // aquí y no antes: un toque entre medias abriría otra conexión (Gemini 16/11)
                }));
            }
            catch (Exception) { conectando = 0; }   // la VM aún no escucha: el siguiente toque reintenta
        }) { IsBackground = true }.Start();
    }

    // Tecla de Windows (scancode del conjunto 1) -> código de Linux. Las básicas coinciden; las extendidas (E0) no.
    static int Linux(int sc, bool ext)
    {
        if (!ext) return sc;
        switch (sc)
        {
            case 0x1C: return 96; case 0x1D: return 97; case 0x35: return 98; case 0x38: return 100;   // KPENTER, RCTRL, KPSLASH, RALT
            case 0x47: return 102; case 0x48: return 103; case 0x49: return 104; case 0x4B: return 105; // HOME, UP, PGUP, LEFT
            case 0x4D: return 106; case 0x4F: return 107; case 0x50: return 108; case 0x51: return 109; // RIGHT, END, DOWN, PGDN
            case 0x52: return 110; case 0x53: return 111; case 0x5B: return 125; case 0x5C: return 126; // INSERT, DELETE, META
            case 0x5D: return 127; default: return 0;                                                  // COMPOSE (menú)
        }
    }

    // ---- Pluma (Surface Pen): presión, inclinación, borrador y botón (tipo 4 del protocolo de entrada) ----
    [DllImport("user32.dll")] static extern bool GetPointerType(uint id, out uint tipo);
    [StructLayout(LayoutKind.Sequential)] struct PointerInfo
    {
        public uint tipo, id, frameId, flags;
        public IntPtr dispositivo, destino;
        public Point pixel, himetric, pixelRaw, himetricRaw;
        public uint tiempo, historial;
        public int datos;
        public uint teclas;
        public ulong rendimiento;
        public uint cambioBoton;
    }
    [StructLayout(LayoutKind.Sequential)] struct PointerPenInfo
    {
        public PointerInfo puntero;
        public uint flags, mascara, presion, rotacion;
        public int tiltX, tiltY;
    }
    [DllImport("user32.dll")] static extern bool GetPointerPenInfo(uint id, out PointerPenInfo info);
    [DllImport("user32.dll")] static extern bool GetPointerPenInfoHistory(uint id, ref uint cantidad, [Out] PointerPenInfo[] info);

    int plumaId = -1;   // puntero de la pluma en rango (para WM_POINTERCAPTURECHANGED)
    void Pluma(int mensaje, int id)
    {
        PointerPenInfo p;
        if (mensaje == 0x024A || !GetPointerPenInfo((uint)id, out p)) { EnviarToque(4u, 0, Point.Empty, -1, 0); plumaId = -1; return; }   // fuera de rango
        plumaId = id;
        // Windows junta varias muestras de la pluma por mensaje (la Surface Pen informa más rápido que la pantalla): se mandan
        // todas, de la más vieja a la más nueva, para que el trazo no salga a tramos.
        if (mensaje == 0x0245 && p.puntero.historial > 1)
        {
            uint n = p.puntero.historial;
            var h = new PointerPenInfo[n];
            if (GetPointerPenInfoHistory((uint)id, ref n, h)) { for (int i = (int)n - 1; i >= 0; i--) MuestraPluma(h[i]); return; }
        }
        MuestraPluma(p);
    }
    void MuestraPluma(PointerPenInfo p)
    {
        // Mismos bits que el Visor de QEMU: en rango (INRANGE 2), contacto (INCONTACT 4), borrador (invertida/goma) y botón.
        int estado = (p.puntero.flags & 2) != 0 ? 1 : 0;
        if (estado != 0)
        {
            if ((p.puntero.flags & 4) != 0) estado |= 2;
            if ((p.flags & 6) != 0) estado |= 4;
            if ((p.flags & 1) != 0) estado |= 8;
        }
        uint presion = (p.mascara & 1) != 0 ? Math.Min(1024u, p.presion) : ((estado & 2) != 0 ? 1024u : 0u);
        int tx = (p.mascara & 4) != 0 ? Math.Max(-90, Math.Min(90, p.tiltX)) : 0, ty = (p.mascara & 8) != 0 ? Math.Max(-90, Math.Min(90, p.tiltY)) : 0;
        EnviarToque(4u, estado, PointToClient(p.puntero.pixel), -1, presion | (uint)(tx + 90) << 16 | (uint)(ty + 90) << 24);
    }

    protected override void WndProc(ref Message m)
    {
        const int WM_POINTERUPDATE = 0x0245, WM_POINTERDOWN = 0x0246, WM_POINTERUP = 0x0247;
        if (m.Msg == 0x0100 || m.Msg == 0x0101 || m.Msg == 0x0104 || m.Msg == 0x0105)   // WM_(SYS)KEYDOWN/UP -> Android
        {
            long lp = (long)m.LParam; bool abajo = m.Msg == 0x0100 || m.Msg == 0x0104;
            int k = Linux((int)((lp >> 16) & 0xFF), (lp & (1L << 24)) != 0);
            bool alt4 = abajo && m.Msg == 0x0104 && (int)(long)m.WParam == (int)Keys.F4;
            if (alt4) { Cambiar(false); return; }   // Alt+F4 vuelve a Windows (el visor queda residente); Esc es "atrás" en Android
            Tecla(k, abajo);   // solo sin gancho (si SetWindowsHookEx falló): con él, las teclas no llegan aquí
            m.Result = IntPtr.Zero; return;
        }
        if (m.Msg == 0x0246) punteros.Add((int)((long)m.WParam & 0xFFFF));   // dedos y pluma puestos (E25: no cambiar Hz con uno puesto)
        if (m.Msg == 0x0247 || m.Msg == 0x024A || m.Msg == 0x024C) punteros.Remove((int)((long)m.WParam & 0xFFFF));   // arriba, fuera de rango (pluma), trazo perdido
        if (punteros.Count == 0 && hzPendiente) { hzPendiente = false; BeginInvoke((Action)(() => Pantalla120(enFrente))); }   // al soltar el último: la frecuencia pendiente
        if (m.Msg == 0x024C)   // WM_POINTERCAPTURECHANGED: el trazo se perdió, levantar ese dedo o la pluma
        {
            int id = (int)((long)m.WParam & 0xFFFF), r;
            if (id == dedoEsquina) dedoEsquina = -1;
            if (id == dedoIsla) dedoIsla = -1;
            if (id == dedoBorde) dedoBorde = -1;
            if (id == dedoTiron && cortina != null) Cortina(muestraDy, true);   // la cortina a medias se abre o se cierra
            inicioDedo.Remove(id); ultimoDedo.Remove(id);
            if (dedos.TryGetValue(id, out r)) { EnviarToque(2u, r, Point.Empty); dedos.Remove(id); }
            else if (id == plumaId) { EnviarToque(4u, 0, Point.Empty, -1, 0); plumaId = -1; }   // solo la pluma: no cortar su trazo por otro puntero
        }
        uint tipoPuntero;
        if ((m.Msg >= WM_POINTERUPDATE && m.Msg <= WM_POINTERUP || m.Msg == 0x024A) &&
            GetPointerType((uint)((long)m.WParam & 0xFFFF), out tipoPuntero) && tipoPuntero == 3)   // PT_PEN
        {
            Pluma(m.Msg, (int)((long)m.WParam & 0xFFFF)); m.Result = IntPtr.Zero; return;
        }
        if (m.Msg >= WM_POINTERUPDATE && m.Msg <= WM_POINTERUP)
        {
            int id = (int)((long)m.WParam & 0xFFFF);
            long lp = (long)m.LParam;
            var p = PointToClient(new Point((short)(lp & 0xFFFF), (short)((lp >> 16) & 0xFFFF)));
            Tocado();   // también los gestos del visor (tirón de la cortina, bordes): 120 Hz mientras el dedo se mueve
            if (Gesto(m.Msg, id, p)) { m.Result = IntPtr.Zero; return; }
            if (m.Msg == WM_POINTERDOWN && !dedos.ContainsKey(id))
            {
                int ranura = 0;
                while (ranura < 10 && dedos.ContainsValue(ranura)) ranura++;
                if (ranura < 10) dedos[id] = ranura;
            }
            int r;
            if (dedos.TryGetValue(id, out r))
            {
                EnviarToque(m.Msg == WM_POINTERDOWN ? 0u : m.Msg == WM_POINTERUPDATE ? 1u : 2u, r, p);
                if (m.Msg == WM_POINTERUP) dedos.Remove(id);
            }
            m.Result = IntPtr.Zero;   // atendido: Windows no lo convierte en ratón
            return;
        }
        base.WndProc(ref m);
    }

    // ---- 120 Hz solo mientras Android está delante: Windows queda en el modo que eligió el usuario (p. ej. 60 Hz) ----
    // CDS_FULLSCREEN es temporal: Windows vuelve al modo del registro al quitarlo o si el proceso muere.
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] struct ModoPantalla
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string nombre;
        public short version, versionDriver, tam, extra; public int campos;
        public int x, y, orientacion, salidaFija; public short color, duplex, resY, tt, intercalar;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string formulario;
        public short ppp; public int bits, ancho, alto, banderas, hz;
        public int icm, intento, medio, trama, r1, r2, panAncho, panAlto;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool EnumDisplaySettings(string d, int i, ref ModoPantalla m);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int ChangeDisplaySettingsEx(string d, ref ModoPantalla m, IntPtr h, int f, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int ChangeDisplaySettingsEx(string d, IntPtr m, IntPtr h, int f, IntPtr p);
    // Frecuencia adaptativa (ESTADO 135, F7/plan/youtube_notas.md): el panel sigue al contenido de Android, como ProMotion o la
    // frecuencia dinámica de Windows. 120 Hz al tocar o con más de 60 cuadros/s; 60 Hz con vídeo de 30/60; 48 Hz con vídeo de 24
    // o pantalla quieta. Medido 18/11 en este OLED: 120 -> 60 Hz ahorra ~0,4-1,1 W y 120 -> 48 Hz ~0,7-1,0 W.
    // SURFACEDUAL_HZ_ADAPTATIVA=0 lo apaga (siempre 120 Hz, como antes).
    // shortcut: el vídeo de 25/50 fps va a 48/60 Hz (el panel no tiene 50 Hz), pasar a 100 Hz si alguien nota el tirón.
    static readonly bool hzAdaptativa = Environment.GetEnvironmentVariable("SURFACEDUAL_HZ_ADAPTATIVA") != "0";
    internal static int HzPara(double fps, bool tocando) { return tocando || fps > 63 ? 120 : fps > 26 ? 60 : 48; }
    int hzPuesta, hzUsuario, hzContenido = 120, hzCandidata, ticsCandidata, cuadrosHz, ultimaEntradaMs;
    bool modoTemporal, virtualPuesta, hzPendiente;
    readonly System.Collections.Generic.HashSet<int> punteros = new System.Collections.Generic.HashSet<int>();
    void Tocado()   // pedir 120 Hz en cuanto hay entrada (con un dedo puesto, Pantalla120 lo deja para cuando se suelte: E25)
    {
        ultimaEntradaMs = Environment.TickCount;
        if (hzAdaptativa && hzContenido != 120 && enFrente && IsHandleCreated) { hzContenido = 120; BeginInvoke((Action)(() => Pantalla120(enFrente))); }
    }
    // E25 (F7/plan/hz_e25_notas.md): ChangeDisplaySettingsEx cambia el MODO del panel (la señal física): el visor sale negro un
    // instante y Windows corta el toque en curso. Desde Windows 11 la ruta admite "frecuencia virtual": la señal física sigue
    // igual (120 Hz) y solo cambia el ritmo de DWM (DISPLAYCONFIG_PATH_TARGET_INFO.refreshRate con QDC/SDC_VIRTUAL_REFRESH_RATE_AWARE,
    // learn.microsoft.com .../ns-wingdi-displayconfig_path_info). Medido aquí: ~28 ms, sin negro, EnumDisplaySettings da la nueva.
    [StructLayout(LayoutKind.Sequential)] struct DcRuta
    {
        public uint oLuidBajo; public int oLuidAlto; public uint oId, oModo, oEstado;
        public uint tLuidBajo; public int tLuidAlto; public uint tId, tModo, tTecnologia, tRotacion, tEscala, hzN, hzD, tLineas; public int tDisponible; public uint tEstado;
        public uint banderas;
    }
    [StructLayout(LayoutKind.Sequential)] struct DcModo { public uint tipo, id, luidBajo; public int luidAlto; public ulong a, b, c, d, e, f; }   // 64 bytes; no se toca
    [DllImport("user32.dll")] static extern int GetDisplayConfigBufferSizes(uint f, out uint np, out uint nm);
    [DllImport("user32.dll")] static extern int QueryDisplayConfig(uint f, ref uint np, [Out] DcRuta[] p, ref uint nm, [Out] DcModo[] m, IntPtr t);
    [DllImport("user32.dll")] static extern int SetDisplayConfig(uint np, DcRuta[] p, uint nm, DcModo[] m, uint f);
    static readonly string marcaVirtual = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_hz_virtual.txt");
    bool Marcar() { try { System.IO.File.WriteAllText(marcaVirtual, hzUsuario.ToString()); } catch { } return true; }
    static int HzVirtual(int hz)   // 0 = hecho; frecuencia virtual del panel interno (con la frecuencia física del modo como tope)
    {
        const uint QDC = 2 | 0x10 | 0x40, SDC = 0x20 | 0x80 | 0x8000 | 0x20000;   // ACTIVAS|VIRTUAL_MODE|VIRTUAL_REFRESH_RATE; USE_SUPPLIED|APPLY|...
        uint np, nm; int r = GetDisplayConfigBufferSizes(QDC, out np, out nm); if (r != 0) return r;
        var p = new DcRuta[np]; var m = new DcModo[nm];
        r = QueryDisplayConfig(QDC, ref np, p, ref nm, m, IntPtr.Zero); if (r != 0) return r;
        // panel integrado (INTERNAL, LVDS, DisplayPort o UDI embebidos) que admite modo virtual
        int k = Array.FindIndex(p, x => (x.tTecnologia == 0x80000000u || x.tTecnologia == 6 || x.tTecnologia == 11 || x.tTecnologia == 13) && (x.banderas & 8) != 0);
        if (k < 0) return -1;
        p[k].hzN = (uint)hz; p[k].hzD = 1;
        return SetDisplayConfig(np, p, nm, m, SDC);
    }
    readonly Stopwatch cronoHz = Stopwatch.StartNew();
    System.Windows.Forms.Timer relojHz;
    void AdaptarHz(object o, EventArgs e)   // cada 500 ms con Android delante
    {
        double s = cronoHz.Elapsed.TotalSeconds; cronoHz.Restart();
        int n = Interlocked.Exchange(ref cuadrosHz, 0);
        if (!enFrente || ControlesCompartidos.Lectura || s <= 0) return;
        int hz = HzPara(n / s, dedos.Count > 0 || unchecked((uint)(Environment.TickCount - ultimaEntradaMs)) < 2000);
        // Subir, al momento; bajar, solo tras 3 s seguidos pidiendo lo mismo (un vídeo que carga o una pausa corta no mueven el panel).
        if (hz != hzCandidata) { hzCandidata = hz; ticsCandidata = 0; }
        if (hz > hzContenido || hz < hzContenido && ++ticsCandidata >= 6) { hzContenido = hz; Pantalla120(true); }
    }
    void Pantalla120(bool si)
    {
        if (HzPara(60, false) != 60 || HzPara(24, false) != 48 || HzPara(0, false) != 48 || HzPara(90, false) != 120 || HzPara(0, true) != 120)
            throw new InvalidOperationException("HzPara");   // autoprueba
        if (hzAdaptativa)
        {
            if (relojHz == null) { relojHz = new System.Windows.Forms.Timer { Interval = 500 }; relojHz.Tick += AdaptarHz; }
            if (!si || !relojHz.Enabled) { hzContenido = 120; hzCandidata = ticsCandidata = 0; cuadrosHz = 0; cronoHz.Restart(); }   // al volver a Android se empieza en 120 Hz y de cero
            relojHz.Enabled = si;
        }
        // Modo lectura de Android (ESTADO 116 y 125): 48 Hz fijos. Medido 18/11 en este panel OLED con página quieta:
        // 120 -> 60 Hz ahorra ~0,4 W y 120 -> 48 Hz ~0,7-1,0 W (30 Hz no ahorra más).
        int hz = !si ? 0 : ControlesCompartidos.Lectura ? 48 : hzAdaptativa ? hzContenido : 120;
        if (hz == hzPuesta) return;
        // E25 medido: cualquier cambio de frecuencia (también la virtual, sin negro) corta el toque en curso (InjectTouchInput 87).
        // Con un dedo puesto se aplica al soltar. 10 s sin mensajes de puntero = un "arriba" perdido: no bloquear la frecuencia.
        if (si && punteros.Count > 0 && unchecked((uint)(Environment.TickCount - ultimaEntradaMs)) < 10000) { hzPendiente = true; return; }
        punteros.Clear(); hzPendiente = false;
        var reloj = Stopwatch.StartNew();
        int r = 0, rv = 0;
        var m = new ModoPantalla(); m.tam = (short)Marshal.SizeOf(m);
        if (hz != 0 && !EnumDisplaySettings(null, -1, ref m)) return;
        if (hz != 0 && !modoTemporal && !virtualPuesta) hzUsuario = m.hz;   // con lo nuestro puesto, el "actual" ya no es el del usuario
        if (hz == 0 || hz == hzUsuario || hz == 120 && hzUsuario > 120)
        {   // la frecuencia del usuario (su elección manda): quitar nuestro cambio, si lo hay; si falla, la próxima llamada lo reintenta
            if (virtualPuesta) { r = HzVirtual(hzUsuario); if (r != 0) return; virtualPuesta = false; try { System.IO.File.Delete(marcaVirtual); } catch { } }
            if (modoTemporal) { r = ChangeDisplaySettingsEx(null, IntPtr.Zero, IntPtr.Zero, 0, IntPtr.Zero); if (r != 0) return; modoTemporal = false; }
            m.hz = hzUsuario;
        }
        // Sin cambio de modo (E25). No muere con el proceso como CDS_FULLSCREEN: la marca (escrita ANTES) deja al próximo visor devolver la del usuario.
        // shortcut: si el usuario cambia la frecuencia en Configuración con Android delante, al salir vuelve la de antes; releer en WM_DISPLAYCHANGE si pasa.
        else if (!modoTemporal && (virtualPuesta || Marcar()) && (rv = HzVirtual(hz)) == 0) { virtualPuesta = true; m.hz = hz; }
        else
        {   // sin frecuencia virtual (pantalla externa, Windows viejo): el cambio temporal de siempre, que al salir de Android, o si el visor muere, vuelve solo
            m.hz = hz; m.campos = 0x400000;   // DM_DISPLAYFREQUENCY
            r = ChangeDisplaySettingsEx(null, ref m, IntPtr.Zero, 4 /* CDS_FULLSCREEN */, IntPtr.Zero);
            if (r != 0 && hz == 48) { m.hz = 60; r = ChangeDisplaySettingsEx(null, ref m, IntPtr.Zero, 4, IntPtr.Zero); }   // panel sin 48 Hz: 60
            if (r == 0) modoTemporal = true; else m.hz = modoTemporal ? hzPuesta : hzUsuario;   // no cambió: sigue la de antes
        }
        hzPuesta = hz;   // también si el panel no la aceptó: no se reintenta cada 500 ms
        // Por debajo de 120 Hz (adaptativa) Present va con intervalo 0 (los cuadros de más se descartan, sin romper la imagen:
        // no se usa ALLOW_TEARING): con intervalo 1 Android nunca pasaría de 60 cuadros/s recibidos y no se vería que una
        // animación pide volver a 120 Hz (comprobado con F7/pruebas/youtube_hz_prueba.ps1). En lectura (48 Hz fijos), como antes.
        if (pantalla != null) pantalla.sincronia = hz != 0 && m.hz < 120 && hzAdaptativa && !ControlesCompartidos.Lectura ? 0u : 1u;
        if (!hzAdaptativa || hz == 0 || ControlesCompartidos.Lectura || r != 0 || rv != 0 || reloj.ElapsedMilliseconds > 100)   // los cambios adaptativos normales no llenan el registro
        try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_tiempos.log"),
            string.Format("{0:o} pantalla120={1} hz={4} r={2} ms={3:F0} virtual={5} rv={6}\r\n", DateTime.Now, si, r, reloj.Elapsed.TotalMilliseconds, hz, virtualPuesta, rv)); } catch { }
    }
    // KEY_WAKEUP (143): Android pudo dormirse (doble toque en el launcher = dormir) y sin esto se quedaba negro; despierto,
    // Android la descarta (no llega a las apps).
    void Despertar() { Tecla(143, true); Tecla(143, false); }
    protected override void OnActivated(EventArgs e) { enFrente = true; if (ControlesCompartidos.Lectura) ThreadPool.QueueUserWorkItem(_ => Silencioso()); Pantalla120(true); Gancho(true); GestosWindows(false); Despertar(); Invalidate(); base.OnActivated(e); }

    // Gestos táctiles de 3 y 4 dedos: con Android delante son de Android (Recientes, etc.), no de Windows (cambiar de app,
    // vista de tareas), y no sacan de Android (pedido del usuario 16/11). No hay opción por ventana (Microsoft: solo el
    // interruptor global de Ajustes > Táctil). shortcut: SPI 0x2030/0x2031 sin documentar (lo usa
    // github.com/thisisthepy/desktop-subsystem-for-android PR #475; aplica en vivo); si Windows lo cambia, leer lo delata.
    // El valor original se guarda en HKCU para restaurarlo aunque el visor se cierre de golpe.
    [DllImport("user32.dll")] static extern bool SystemParametersInfoW(uint a, uint u, ref uint p, uint f);
    [DllImport("user32.dll")] static extern bool SystemParametersInfoW(uint a, uint u, IntPtr p, uint f);
    const string ClaveSD = @"HKEY_CURRENT_USER\Software\SurfaceDual";
    static uint LeerGestos() { uint v = 1; SystemParametersInfoW(0x2030, 0, ref v, 0); return v; }
    static void PonerGestos(uint v)
    {
        // Solo SPIF_SENDCHANGE (2), sin guardarlo en el perfil: si el visor muere, al reiniciar Windows vuelve a tener sus gestos.
        if (!SystemParametersInfoW(0x2031, 0, (IntPtr)v, 2) || (LeerGestos() != 0) != (v != 0)) SystemParametersInfoW(0x2031, 0, ref v, 2);
    }
    // Lo que el visor cambia en Windows con Android delante y sobrevive a su muerte (Stop-Process -Force, excepción): frecuencia
    // virtual (marca en %TEMP%), gestos 3/4, bloqueo de giro y umbral de ahorro (HKCU). Sin marca ni valores guardados, no hace nada.
    static void RestaurarWindows()
    {
        try { int h; if (!int.TryParse(System.IO.File.ReadAllText(marcaVirtual), out h) || HzVirtual(h) == 0) System.IO.File.Delete(marcaVirtual); } catch { }   // si falla, la marca queda para reintentar
        GestosWindows(true);
        try { GiroCompartido.Soltar(); ControlesCompartidos.RestaurarAhorro(); } catch (Exception) { }
    }
    static void GestosWindows(bool windows)
    {
        try
        {
            object guardado = Microsoft.Win32.Registry.GetValue(ClaveSD, "Gestos3y4Original", null);
            if (!windows)
            {
                if (guardado == null) Microsoft.Win32.Registry.SetValue(ClaveSD, "Gestos3y4Original", (int)LeerGestos());
                PonerGestos(0);
            }
            else if (guardado != null)
            {
                PonerGestos((uint)(int)guardado);
                using (var k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(@"Software\SurfaceDual", true)) k?.DeleteValue("Gestos3y4Original", false);
            }
        }
        catch (Exception) { }
    }

    // ---- Teclado entero para Android mientras está delante (WH_KEYBOARD_LL, como el Visor de QEMU): la tecla Windows
    // (= cajón de apps), Alt+Tab, Alt+F4, etc. van a Android. Quedan para Windows: Win+L (bloquear) y Ctrl+Alt+Supr.
    delegate IntPtr GanchoTeclado(int codigo, IntPtr mensaje, IntPtr datos);
    [DllImport("user32.dll", SetLastError = true)] static extern IntPtr SetWindowsHookEx(int tipo, GanchoTeclado cb, IntPtr modulo, uint hilo);
    [DllImport("user32.dll")] static extern bool UnhookWindowsHookEx(IntPtr g);
    [DllImport("user32.dll")] static extern IntPtr CallNextHookEx(IntPtr g, int c, IntPtr m, IntPtr d);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern short GetAsyncKeyState(int t);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint c, uint t);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern IntPtr GetModuleHandle(string n);
    [StructLayout(LayoutKind.Sequential)] struct TeclaLL { public uint vk, scan, flags, tiempo; public UIntPtr extra; }
    IntPtr gancho;
    bool winAbajo, winSolo;
    GanchoTeclado cbGancho;   // campo: que el GC no recoja el delegado que llama Windows
    void Gancho(bool poner)
    {
        if (poner && gancho == IntPtr.Zero) { cbGancho = Teclado; gancho = SetWindowsHookEx(13, cbGancho, GetModuleHandle(null), 0); }
        else if (!poner && gancho != IntPtr.Zero) { UnhookWindowsHookEx(gancho); gancho = IntPtr.Zero; }
    }
    IntPtr Teclado(int codigo, IntPtr mensaje, IntPtr datos)
    {
        int msg = mensaje.ToInt32();
        if (codigo < 0 || GetForegroundWindow() != Handle || (msg != 0x100 && msg != 0x101 && msg != 0x104 && msg != 0x105))
            return CallNextHookEx(gancho, codigo, mensaje, datos);
        var t = (TeclaLL)Marshal.PtrToStructure(datos, typeof(TeclaLL));
        bool win = ((GetAsyncKeyState(0x5B) | GetAsyncKeyState(0x5C)) & 0x8000) != 0, abajo = msg == 0x100 || msg == 0x104;
        if ((t.vk == 0x4C && win) || (t.vk == 0x2E && (GetAsyncKeyState(0x11) & GetAsyncKeyState(0x12) & 0x8000) != 0))
            return CallNextHookEx(gancho, codigo, mensaje, datos);
        // Sin atajos para cambiar de sistema (pedido del usuario 16/11): solo el botón "Cambiar a Windows" o poner el teclado.
        // Tecla Windows sola = cajón de apps de Android (como Inicio en Windows); con otra tecla, META para Android.
        if (t.vk == 0x5B || t.vk == 0x5C)
        {
            if (abajo) { if (!winAbajo) winAbajo = winSolo = true; }
            else { winAbajo = false; if (winSolo) ControlesCompartidos.Enviar("panel", "cajon"); else Tecla(125, false); winSolo = false; }
            return (IntPtr)1;
        }
        if (winAbajo && winSolo && abajo) { winSolo = false; Tecla(125, true); }
        // El teclado táctil de Windows y SendInput por VK llegan con scan 0.
        // Botones físicos de volumen de la Surface y teclas multimedia: a Android por su tecla virtual (antes se perdían:
        // su scan extendido no estaba en Linux()). Android mueve su volumen y el agente lo lleva a Windows.
        int media = t.vk == 0xAF ? 115 : t.vk == 0xAE ? 114 : t.vk == 0xAD ? 113 : t.vk == 0xB0 ? 163 : t.vk == 0xB1 ? 165 : t.vk == 0xB2 ? 166 : t.vk == 0xB3 ? 164 : 0;
        if (media != 0) { Tecla(media, abajo); return (IntPtr)1; }
        Tecla(Linux((int)(t.scan != 0 ? t.scan : MapVirtualKey(t.vk, 0)), (t.flags & 1) != 0), abajo);
        return (IntPtr)1;
    }
    // Una tecla a Android; las repeticiones de Windows se ignoran (el teclado uinput tiene EV_REP: Android repite solo).
    void Tecla(int k, bool abajo)
    {
        if (k <= 0 || (abajo ? !teclasAbajo.Add(k) : !teclasAbajo.Remove(k))) return;
        EnviarToque(3u, k, Point.Empty, abajo ? 1 : 0);
    }

    // Con Android delante mandan sus gestos (atrás desde el borde, inicio, recientes), no los de Windows; y sin el
    // "mantener presionado = clic derecho", flicks ni círculo de toque, que retrasan el toque largo. Solo en esta ventana.
    [ComImport, Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IPropertyStore
    {
        [PreserveSig] int GetCount(out uint n);
        [PreserveSig] int GetAt(uint i, out ClavePropiedad k);
        [PreserveSig] int GetValue(ref ClavePropiedad k, IntPtr valor);
        [PreserveSig] int SetValue(ref ClavePropiedad k, IntPtr valor);
        [PreserveSig] int Commit();
    }
    [StructLayout(LayoutKind.Sequential)] struct ClavePropiedad { public Guid fmtid; public uint pid; }
    [DllImport("shell32.dll")] static extern int SHGetPropertyStoreForWindow(IntPtr h, ref Guid iid, out IPropertyStore ps);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool SetPropW(IntPtr h, string nombre, IntPtr valor);
    void ApagarGestosWindows()
    {
        SetPropW(Handle, "MicrosoftTabletPenServiceProperty", (IntPtr)(0x1 | 0x8 | 0x10 | 0x10000));
        var iid = new Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99");
        IPropertyStore ps;
        if (SHGetPropertyStoreForWindow(Handle, ref iid, out ps) != 0) return;
        var k = new ClavePropiedad { fmtid = new Guid("32CE38B2-2C9A-41B1-9BC5-B3784394AA44"), pid = 2 };   // PKEY_EdgeGesture_DisableTouchWhenFullscreen
        IntPtr v = Marshal.AllocCoTaskMem(24);
        try
        {
            for (int i = 0; i < 24; i += 4) Marshal.WriteInt32(v, i, 0);
            Marshal.WriteInt16(v, 0, 11);    // VT_BOOL
            Marshal.WriteInt16(v, 8, -1);    // VARIANT_TRUE
            ps.SetValue(ref k, v);
            ps.Commit();
        }
        finally { Marshal.FreeCoTaskMem(v); Marshal.ReleaseComObject(ps); }
    }

    // Teclas pulsadas aquí: si el foco se va a otra ventana, su WM_KEYUP no llega y Android las dejaría pegadas.
    readonly System.Collections.Generic.HashSet<int> teclasAbajo = new System.Collections.Generic.HashSet<int>();
    protected override void OnDeactivate(EventArgs e)
    {
        foreach (int k in teclasAbajo) EnviarToque(3u, k, Point.Empty, 0);
        teclasAbajo.Clear();
        winAbajo = winSolo = false;
        Gancho(false);
        GestosWindows(true);
        Pantalla120(false);
        enFrente = false; if (ControlesCompartidos.Lectura) ThreadPool.QueueUserWorkItem(_ => Silencioso());
        base.OnDeactivate(e);
    }

    // ---- Gestos de las tabletas Samsung (One UI), como el Visor de QEMU: desde una esquina superior en diagonal = ventana
    // flotante; dos dedos desde el borde inferior hacia arriba = pantalla dividida (el agente los ejecuta). El resto, a Android.
    int dedoEsquina = -1; Point inicioEsquina; bool esquinaIzq;
    // Isla dinámica del launcher: está bajo la barra de estado de Android (no se puede tocar ahí), así que el visor convierte
    // los toques que empiezan en su rectángulo ("isla x0,y0,x1,y1", del agente) en gestos y se los manda (17/11).
    int dedoIsla = -1, tIsla; Point inicioIsla; bool islaMantenida;
    // Atrás desde el borde izquierdo o derecho: sin barra de tareas de Quickstep (el inicio es nuestro launcher) SystemUI no
    // engancha su EdgeBackGestureHandler (medido 16/11: mIsAttached=false), así que lo detecta el visor y manda KEY_BACK.
    int dedoBorde = -1; Point inicioBorde; bool bordeIzq;
    // Arriba y abajo (pedido 16/11, fotos de la Galaxy Tab): los paneles y la multitarea los dibuja nuestro launcher.
    int dedoTiron = -1, movTiron, tInicioTiron; Point inicioTiron, ultimoTiron; bool tironArriba, lineaAvisada;
    // Cortina: "tiron <noti|qs> <px bajados> [<px/s al soltar>]" al launcher (Paneles.tiron); sin la velocidad = sigue el dedo.
    string cortina; int muestraDy, velCortina; long tMuestra; bool cortinaEnviada;
    static long Ms() { return Stopwatch.GetTimestamp() * 1000 / Stopwatch.Frequency; }
    void Cortina(int y, bool suelta)
    {
        var al = escribirLauncher; bool ok = al != null;
        try { if (ok) al("tiron " + cortina + " " + y + (suelta ? " " + velCortina : "")); }
        catch (Exception e)   // el launcher cierra la cortina a medias al perder el visor
        {
            ok = false; ReconectarLauncher();
            try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_tiempos.log"),
                string.Format("{0:o} cortina_envio_fallo {1}\r\n", DateTime.Now, e.GetType().Name)); } catch { }
        }
        // Falló la primera línea (nada abierto): el panel entero por el agente, como antes. A medias, no: alternar() lo cerraría.
        if (!ok && !cortinaEnviada && !suelta) ControlesCompartidos.Enviar("panel", cortina);
        cortinaEnviada = ok;
        if (suelta || !ok) { cortina = null; cortinaEnviada = false; dedoTiron = -1; }
    }
    void SoltarLinea() { if (lineaAvisada) { lineaAvisada = false; ControlesCompartidos.Enviar("linea", "suelta"); } }
    System.Windows.Forms.Timer temporizadorTironCampo;
    System.Windows.Forms.Timer temporizadorTiron
    {
        get
        {
            if (temporizadorTironCampo != null) return temporizadorTironCampo;
            temporizadorTironCampo = new System.Windows.Forms.Timer { Interval = 20 };
            temporizadorTironCampo.Tick += (s, e) =>
            {
                // subió más de 150 px y lleva 180 ms quieto: recientes (sus movimientos y el soltar ya no hacen nada)
                if (dedoTiron == -1 || tironArriba) { temporizadorTironCampo.Stop(); SoltarLinea(); return; }
                bool quieto = Math.Abs(ultimoTiron.X - inicioTiron.X) <= 24 && Math.Abs(ultimoTiron.Y - inicioTiron.Y) <= 24;
                if (!quieto) SoltarLinea();   // se movió: ya no es mantener la línea
                if (inicioTiron.Y - ultimoTiron.Y > 150 && Environment.TickCount - movTiron > 180)
                { dedoTiron = -1; temporizadorTironCampo.Stop(); ControlesCompartidos.Enviar("panel", "recientes"); }
                // Mantener pulsada la línea de abajo: Circle to Search (como en Pixel/Galaxy; punto 18, 17/11). A los 120 ms quieto
                // la línea empieza a crecer (respuesta inmediata, E13) y a los 350 ms se abre; un temblor de hasta 24 px no lo cancela.
                else if (quieto && Environment.TickCount - tInicioTiron > 350)
                { dedoTiron = -1; temporizadorTironCampo.Stop(); lineaAvisada = false; ControlesCompartidos.Enviar("panel", "circulo"); }
                else if (quieto && !lineaAvisada && Environment.TickCount - tInicioTiron > 120) { lineaAvisada = true; ControlesCompartidos.Enviar("linea", "mantener"); }
            };
            return temporizadorTironCampo;
        }
    }
    // No era el gesto: el toque se le entrega a Android desde donde empezó.
    void Entregar(int id, Point inicio, Point p, bool soltar)
    {
        int r = RanuraLibre();
        if (r >= 10) return;
        var dF = Destino();
        EnviarToque(0u, r, Fisico(inicio, dF)); EnviarToque(1u, r, Fisico(p, dF));
        if (soltar) EnviarToque(2u, r, Fisico(p, dF)); else { dedos[id] = r; inicioDedo[id] = inicio; ultimoDedo[id] = p; }
    }
    readonly System.Collections.Generic.Dictionary<int, Point> inicioDedo = new System.Collections.Generic.Dictionary<int, Point>(),
                                                            ultimoDedo = new System.Collections.Generic.Dictionary<int, Point>();
    int RanuraLibre() { int r = 0; while (r < 10 && dedos.ContainsValue(r)) r++; return r; }
    // Android girado (E4/E14, 17/11): las zonas de gestos (arriba, abajo, bordes, asa, isla) van en coordenadas de Android, es
    // decir, "arriba" es el borde que el usuario ve arriba. Logico gira el punto dentro del destino; Fisico lo deshace. La pantalla
    // de Windows queda horizontal con Android delante (GiroCompartido), así que el destino siempre es el panel físico.
    static Rectangle DestinoLogico(Rectangle d) { return ControlesCompartidos.Giro % 2 == 1 ? new Rectangle(d.X, d.Y, d.Height, d.Width) : d; }
    static Point Logico(Point p, Rectangle d)
    {
        int x = p.X - d.X, y = p.Y - d.Y;
        switch (ControlesCompartidos.Giro)
        {
            case 1: return new Point(d.X + y, d.Y + d.Width - x);
            case 2: return new Point(d.X + d.Width - x, d.Y + d.Height - y);
            case 3: return new Point(d.X + d.Height - y, d.Y + x);
            default: return p;
        }
    }
    static Point Fisico(Point l, Rectangle d)
    {
        int x = l.X - d.X, y = l.Y - d.Y;
        switch (ControlesCompartidos.Giro)
        {
            case 1: return new Point(d.X + d.Width - y, d.Y + x);
            case 2: return new Point(d.X + d.Width - x, d.Y + d.Height - y);
            case 3: return new Point(d.X + y, d.Y + d.Height - x);
            default: return l;
        }
    }
    static bool Autoprueba()   // ida y vuelta de los 4 giros (assert-based self-check)
    {
        var d = new Rectangle(10, 20, 300, 200); int g0 = ControlesCompartidos.Giro; bool ok = true;
        for (int g = 0; g < 4; g++) { ControlesCompartidos.Giro = g; var q = new Point(57, 133); ok &= Fisico(Logico(q, d), d) == q; }
        ControlesCompartidos.Giro = 1; ok &= Logico(new Point(10 + 300, 20), d) == new Point(10, 20);   // borde derecho físico = arriba
        ControlesCompartidos.Giro = g0; return ok;
    }
    bool Gesto(int msg, int id, Point p)   // true: el mensaje es del gesto y no va a Android
    {
        const int DOWN = 0x0246, UP = 0x0247;
        var dF = Destino();
        var d = DestinoLogico(dF);
        p = Logico(p, dF);
        int anchoL = ControlesCompartidos.Giro % 2 == 1 ? alto : ancho, altoL = ControlesCompartidos.Giro % 2 == 1 ? ancho : alto;   // píxeles de Android girado
        if (msg == DOWN)
        {
            int rx = p.X - d.X, ry = p.Y - d.Y;
            // Otro dedo mientras se sube desde abajo: es un gesto de varios dedos (dividir, recientes): el primero pasa a normal.
            if (dedoTiron != -1 && !tironArriba) { int t = dedoTiron; dedoTiron = -1; Entregar(t, inicioTiron, ultimoTiron, false); }
            var isla = ControlesCompartidos.Isla;
            if (isla != null && dedos.Count == 0 && dedoIsla == -1 && ancho > 0 && alto > 0)
            {
                long ax = (long)rx * anchoL / Math.Max(1, d.Width), ay = (long)ry * altoL / Math.Max(1, d.Height);
                if (ax >= isla[0] && ax <= isla[2] && ay >= isla[1] && ay <= isla[3])
                { dedoIsla = id; inicioIsla = p; tIsla = Environment.TickCount; islaMantenida = false; return true; }
            }
            bool izq = rx >= 0 && rx <= 24 && ry >= 0 && ry <= 24, der = rx >= d.Width - 24 && rx <= d.Width && ry >= 0 && ry <= 24;
            if (dedos.Count == 0 && dedoEsquina == -1 && (izq || der)) { dedoEsquina = id; inicioEsquina = p; esquinaIzq = izq; return true; }
            bool arriba = ry >= 0 && ry <= 24, abajo = ry >= d.Height - 48 && ry <= d.Height;   // abajo, 48 px: la línea es más grande (E13)
            if (dedos.Count == 0 && dedoTiron == -1 && (arriba || abajo))
            {
                dedoTiron = id; inicioTiron = ultimoTiron = p; tironArriba = arriba; movTiron = tInicioTiron = Environment.TickCount; lineaAvisada = false;
                if (!arriba) temporizadorTiron.Start();
                return true;
            }
            bool bIzq = rx >= 0 && rx <= 40, bDer = rx >= d.Width - 40 && rx <= d.Width;
            // Sobre el asa del Panel Edge el toque es del launcher (arrastrarla o abrir el panel), no "Atrás".
            var asa = ControlesCompartidos.Asa;
            if (asa != null && alto > 0)
            {
                int ya = (int)((long)ry * altoL / Math.Max(1, d.Height));
                if (ya >= asa[1] && ya <= asa[2] && (asa[0] == 1 ? bDer : bIzq)) bIzq = bDer = false;
            }
            if (dedos.Count == 0 && dedoBorde == -1 && (bIzq || bDer)) { dedoBorde = id; inicioBorde = p; bordeIzq = bIzq; return true; }
            inicioDedo[id] = p; ultimoDedo[id] = p;
            return false;
        }
        if (id == dedoIsla)
        {
            int dx = p.X - inicioIsla.X, dy = p.Y - inicioIsla.Y;
            bool quieto = Math.Abs(dx) < 24 && Math.Abs(dy) < 24;
            if (msg != UP)
            {
                // mantener (450 ms quieto) despliega ya, con el dedo puesto, como en el iPhone
                if (quieto && !islaMantenida && Environment.TickCount - tIsla >= 450) { islaMantenida = true; ControlesCompartidos.Enviar("isla", "mantener"); }
                return true;
            }
            dedoIsla = -1;
            if (islaMantenida) return true;
            string g = Math.Abs(dx) > 40 && Math.Abs(dx) > Math.Abs(dy) ? (dx < 0 ? "izq" : "der")
                     : Math.Abs(dy) > 40 ? (dy < 0 ? "arriba" : "abajo")
                     : Environment.TickCount - tIsla >= 450 ? "mantener"
                     : "toque," + (long)(inicioIsla.X - d.X) * anchoL / Math.Max(1, d.Width);
            ControlesCompartidos.Enviar("isla", g);
            return true;
        }
        if (id == dedoEsquina)
        {
            int dx = p.X - inicioEsquina.X, dy = p.Y - inicioEsquina.Y, hx = esquinaIzq ? dx : -dx;   // hx: hacia el centro
            bool diagonal = hx > dy / 2 && dy > hx / 2;
            if (msg != UP && hx > 40 && dy > 40 && diagonal && dx * dx + dy * dy > 14400) { ControlesCompartidos.Enviar("flotar", "1"); dedoEsquina = -1; return true; }
            if (msg == UP || (dx * dx + dy * dy > 1600 && !diagonal)) { dedoEsquina = -1; Entregar(id, inicioEsquina, p, msg == UP); }
            return true;
        }
        if (id == dedoTiron)
        {
            int dx = p.X - inicioTiron.X, dy = p.Y - inicioTiron.Y;
            if (tironArriba)   // bajar desde arriba: izquierda = notificaciones, derecha = ajustes rápidos (como una Galaxy Tab)
            {
                // Con el canal directo al launcher (4445) el panel baja siguiendo el dedo, como la cortina de Android (18/11:
                // antes era una orden a los 80 px y el panel aparecía de golpe). Sin canal, la orden de siempre.
                int y = (int)((long)dy * altoL / Math.Max(1, d.Height));   // píxeles de Android
                long t = Ms();
                if (cortina == null && escribirLauncher != null && msg != UP && dy > 16 && Math.Abs(dx) < dy)
                { cortina = inicioTiron.X - d.X < d.Width / 2 ? "noti" : "qs"; muestraDy = y; tMuestra = t; velCortina = 0; cortinaEnviada = false; }
                if (cortina != null)
                {
                    // px/s en ventanas de 30 ms; al soltar, con lo que haya desde la última muestra (8 ms como poco)
                    if (t - tMuestra >= (msg == UP ? 8 : 30)) { velCortina = (int)((y - muestraDy) * 1000 / (t - tMuestra)); muestraDy = y; tMuestra = t; }
                    Cortina(y, msg == UP);
                    return true;
                }
                if (msg != UP && dy > 80 && Math.Abs(dx) < dy)
                {
                    // Sin canal al launcher (reiniciado o rearmado, 18/11): panel entero ahora y reconexión ya, no a los 5-60 s,
                    // para que el siguiente tirón vuelva a ser cortina. Queda anotado (si vuelve a pasar, se ve cuándo).
                    dedoTiron = -1; ControlesCompartidos.Enviar("panel", inicioTiron.X - d.X < d.Width / 2 ? "noti" : "qs");
                    ReconectarLauncher();
                    try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_tiempos.log"),
                        string.Format("{0:o} cortina_sin_canal giro={1}\r\n", DateTime.Now, ControlesCompartidos.Giro)); } catch { }
                    return true;
                }
                if (msg == UP || Math.Abs(dx) > 60) { dedoTiron = -1; Entregar(id, inicioTiron, p, msg == UP); }
                return true;
            }
            if (Math.Abs(p.X - ultimoTiron.X) + Math.Abs(p.Y - ultimoTiron.Y) > 12) { ultimoTiron = p; movTiron = Environment.TickCount; }
            if (msg == UP)   // subir y soltar = inicio; subir y mantener lo resuelve temporizadorTiron (recientes)
            {
                dedoTiron = -1; temporizadorTiron.Stop(); SoltarLinea();
                if (-dy > 120) { Tecla(172, true); Tecla(172, false); }   // KEY_HOMEPAGE -> HOME (Generic.kl)
                else Entregar(id, inicioTiron, p, true);
            }
            return true;
        }
        if (id == dedoBorde)
        {
            int dy = Math.Abs(p.Y - inicioBorde.Y), hx = bordeIzq ? p.X - inicioBorde.X : inicioBorde.X - p.X;   // hx: hacia el centro
            if (msg == UP && hx > 120 && dy < hx) { dedoBorde = -1; Tecla(158, true); Tecla(158, false); return true; }   // KEY_BACK
            if (msg == UP || (dy > 60 && dy > hx)) { dedoBorde = -1; Entregar(id, inicioBorde, p, msg == UP); }
            return true;
        }
        if (msg == UP) { inicioDedo.Remove(id); ultimoDedo.Remove(id); return false; }
        if (!dedos.ContainsKey(id)) return false;
        ultimoDedo[id] = p;
        if (dedos.Count < 2) return false;
        if (dedos.Count >= 3)   // tres dedos hacia arriba = recientes de Android (pedido 16/11: ya no son de Windows)
        {
            int arriba3 = 0;
            foreach (var k in dedos.Keys)
            {
                Point i3, u3;
                if (inicioDedo.TryGetValue(k, out i3) && ultimoDedo.TryGetValue(k, out u3) && i3.Y - u3.Y > 120) arriba3++;
            }
            if (arriba3 >= 3)
            {
                ControlesCompartidos.Enviar("panel", "recientes");
                foreach (var kv in dedos) EnviarToque(2u, kv.Value, Fisico(ultimoDedo.ContainsKey(kv.Key) ? ultimoDedo[kv.Key] : p, dF));
                dedos.Clear(); inicioDedo.Clear(); ultimoDedo.Clear();
                return true;
            }
        }
        int subidos = 0;
        foreach (var k in dedos.Keys)
        {
            Point ini, ult;
            if (inicioDedo.TryGetValue(k, out ini) && ultimoDedo.TryGetValue(k, out ult) && d.Y + d.Height - ini.Y <= 40 && ini.Y - ult.Y > 150) subidos++;
        }
        if (subidos < 2) return false;
        ControlesCompartidos.Enviar("split", "1");
        foreach (var kv in dedos) EnviarToque(2u, kv.Value, Fisico(ultimoDedo.ContainsKey(kv.Key) ? ultimoDedo[kv.Key] : p, dF));
        dedos.Clear(); inicioDedo.Clear(); ultimoDedo.Clear();   // sus MOVE/UP restantes ya no tocan Android
        return true;
    }

    // Ratón (sin pantalla táctil): botón izquierdo = un dedo en la ranura 9.
    protected override void OnMouseDown(MouseEventArgs e) { if (e.Button == MouseButtons.Left) EnviarToque(0, 9, e.Location); base.OnMouseDown(e); }
    protected override void OnMouseMove(MouseEventArgs e) { if (e.Button == MouseButtons.Left) EnviarToque(1, 9, e.Location); base.OnMouseMove(e); }
    protected override void OnMouseUp(MouseEventArgs e) { if (e.Button == MouseButtons.Left) EnviarToque(2, 9, e.Location); base.OnMouseUp(e); }

    // Al volver a Android (tres dedos, Alt+Tab, Mostrar) o tras pasar a 120 Hz, Windows pide repintar y Android solo manda
    // cuadros cuando algo cambia: sin esto la ventana quedaba negra. Si DXGI falla en pleno cambio de modo, el próximo cuadro repinta.
    protected override void OnPaint(PaintEventArgs e)
    {
        if (pantalla != null && primerCuadro && !enFondo && !sinPresentar) try { pantalla.Presentar(ClientSize, Destino()); } catch { }
    }
    protected override void OnPaintBackground(PaintEventArgs e) { }

    Rectangle Destino()
    {
        if (ancho <= 0 || alto <= 0) return ClientRectangle;
        float k = Math.Min((float)ClientSize.Width / ancho, (float)ClientSize.Height / alto);
        int dw = (int)(ancho * k), dh = (int)(alto * k);
        return new Rectangle((ClientSize.Width - dw) / 2, (ClientSize.Height - dh) / 2, dw, dh);
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        var paso = Stopwatch.StartNew(); var pasos = new System.Text.StringBuilder();
        try { pantalla = new D3D(Handle); }
        catch (Exception ex) { MessageBox.Show(ex.Message, "D3D11"); BeginInvoke((Action)Close); return; }
        pasos.AppendFormat(" d3d={0}", paso.ElapsedMilliseconds); paso.Restart();
        ApagarGestosWindows();
        pasos.AppendFormat(" gestos={0}", paso.ElapsedMilliseconds); paso.Restart();
        ControlesCompartidos.DelLauncher = OrdenLauncher;
        // Modo lectura: con Android delante, la pantalla a su tasa (60/120) y el EcoQoS ampliado; al ocultar ya vuelve todo solo.
        ControlesCompartidos.AlLectura = () => BeginInvoke((Action)(() => {
            Pantalla120(false); if (enFrente && visible.WaitOne(0)) Pantalla120(true);
            ThreadPool.QueueUserWorkItem(_ => Silencioso());
        }));
        MusicaCompartida.Iniciar(Handle);
        if (!Autoprueba()) Trace.WriteLine("giro: la autoprueba de Logico/Fisico falló");
        Enlace.Iniciar(IpVm);   // agente de Android (volumen, brillo, radios, idioma, ...): F3/agente --escuchar 4449
        pasos.AppendFormat(" enlace={0}", paso.ElapsedMilliseconds); paso.Restart();
        Enlace.Mostrado(true);
        pasos.AppendFormat(" mostrado={0}", paso.ElapsedMilliseconds);
        try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_tiempos.log"),
            string.Format("{0:o} inicio_ms{1}\r\n", DateTime.Now, pasos)); } catch { }
        new Thread(HiloRed) { IsBackground = true }.Start();
        Avisos.Iniciar(this, () => visible.WaitOne(0), e => estadosVm.Add(e), () => pedirMostrar.Set(), ReconectarLauncher);
        new Thread(HiloLauncher) { IsBackground = true }.Start();
        new Thread(HiloResidente) { IsBackground = true }.Start();
        new Thread(HiloVm) { IsBackground = true }.Start();
        // Diagnóstico sin quitarle la pantalla al usuario: Local\SurfaceDualVisorVM.Captura -> %TEMP%\visorvm_captura.png.
        new Thread(() =>
        {
            var e = new EventWaitHandle(false, EventResetMode.AutoReset, @"Local\SurfaceDualVisorVM.Captura");
            while (!terminar && e.WaitOne())
                try { Invoke((Action)(() => pantalla?.Guardar(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_captura.png")))); }
                catch (InvalidOperationException) { return; }
                catch (Exception) { }
        }) { IsBackground = true }.Start();
    }

    // ---- Residente: volver a Windows oculta la ventana y pausa la VM (WMI, ~50–100 ms medido) sin matar el proceso;
    // volver a Android la muestra (con el último cuadro) y la reanuda. Antes: pausar.ps1/iniciar.ps1, 1,6–3 s medidos.
    // La Bandeja pide el cambio con estos eventos; si no existen (Visor cerrado) usa los scripts en frío.
    readonly EventWaitHandle pedirMostrar = new EventWaitHandle(false, EventResetMode.AutoReset, @"Local\SurfaceDualVisorVM.Mostrar");
    readonly EventWaitHandle pedirOcultar = new EventWaitHandle(false, EventResetMode.AutoReset, @"Local\SurfaceDualVisorVM.Ocultar");
    readonly System.Collections.Concurrent.BlockingCollection<ushort> estadosVm = new System.Collections.Concurrent.BlockingCollection<ushort>();
    void HiloResidente()
    {
        var h = new WaitHandle[] { pedirMostrar, pedirOcultar };
        while (!terminar)
        {
            int i = WaitHandle.WaitAny(h);
            try { BeginInvoke((Action)(() => Cambiar(i == 0))); } catch (InvalidOperationException) { return; }   // ventana cerrada
        }
    }
    // "Modo Android silencioso" (punto 9, plan de Grok F7/plan/silencioso_a.md): con Android delante, los procesos de fondo
    // prescindibles de Windows (del usuario: los de SYSTEM no se dejan) pasan a EcoQoS; al volver, Windows decide otra vez.
    // Nunca la VM, el visor, DWM, el audio ni la red.
    static readonly string[] prescindibles = { "OneDrive", "Widgets", "WidgetService", "PhoneExperienceHost", "CrossDeviceService",
        "SearchHost", "StartMenuExperienceHost", "msedgewebview2", "Teams", "ms-teams", "Spotify" };
    // Modo lectura: además estos (F7/plan/windows_apagable.md, "estricto"); fuera de él, Windows decide (0/0, lo de siempre).
    static readonly string[] estrictos = { "OneDrive.Sync.Service", "WWAHost", "ClickToDo", "AppActions", "CrossDeviceResume", "msedge", "brave" };
    [StructLayout(LayoutKind.Sequential)] struct Estrangular { public uint version, control, estado; }
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetProcessInformation(IntPtr h, int clase, ref Estrangular info, int tam);
    volatile bool enFrente;   // el visor tiene el foco (con Alt+Tab a otra app de Windows, los "estrictos" vuelven a la normalidad)
    // Aplica el estado vigente (no el de quien la pidió): las llamadas en cola pueden llegar desordenadas y la última manda igual.
    void Silencioso()
    {
        lock (estrictos) {
        bool si = visible.WaitOne(0) && !terminar, lectura = si && enFrente && ControlesCompartidos.Lectura;
        var e = new Estrangular { version = 1, control = si ? 1u : 0u, estado = si ? 1u : 0u };   // 0/0: Windows decide (por omisión)
        var x = new Estrangular { version = 1, control = lectura ? 1u : 0u, estado = lectura ? 1u : 0u };
        foreach (string n in estrictos)
            foreach (var p in Process.GetProcessesByName(n))
                using (p) try { SetProcessInformation(p.Handle, 4 /* ProcessPowerThrottling */, ref x, Marshal.SizeOf(x)); } catch (Exception) { }
        foreach (string n in prescindibles)
            foreach (var p in Process.GetProcessesByName(n))
                using (p) try { SetProcessInformation(p.Handle, 4 /* ProcessPowerThrottling */, ref e, Marshal.SizeOf(e)); } catch (Exception) { }
        }
    }

    void Cambiar(bool mostrar)
    {
        var reloj = Stopwatch.StartNew();
        if (mostrar)
        {
            autoApagado.Stop();
            Show(); WindowState = FormWindowState.Maximized; ForzarFrente();
            visible.Set();
            estadosVm.Add(2);   // reanudar después de mostrar: la ventana aparece ya con el último cuadro
            Enlace.Mostrado(true);   // Android vuelve a recibir el estado de Windows (volumen, brillo, ...)
            Avisos.Mostrado(); ReconectarLauncher();   // fin de las ventanas de avisos; la conexión con el launcher murió en la pausa
            ThreadPool.QueueUserWorkItem(_ => Silencioso());   // fuera del hilo de la ventana: el cambio sigue en ~12 ms
        }
        else
        {
            visible.Reset();
            IniciarAutoApagado();   // tras 1 h suspendido (menú de la Bandeja) se apaga solo
            Hide();             // Windows al frente al instante; OnDeactivate suelta teclas y vuelve al modo de pantalla del usuario
            Enlace.Mostrado(false);   // con la VM en pausa no se le encolan avisos
            ThreadPool.QueueUserWorkItem(_ => Silencioso());
            estadosVm.Add(9);   // pausa: 0 % de CPU
            Avisos.Ocultado();  // con "Avisos en Windows" encendido: ventanas cada 15 min (Avisos.cs); apagado, nada
        }
        try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_tiempos.log"),
            string.Format("{0:o} cambio mostrar={1} ventana_ms={2:F0}\r\n", DateTime.Now, mostrar, reloj.Elapsed.TotalMilliseconds)); } catch { }
    }
    // Pausa = cero (B4): quitado con la VM en pausa; los reintentos de aquí, de Enlace y de los ayudantes (VmActiva en
    // HvPuente.cs, mismo nombre) esperan a que se reanude en vez de despertar cada 1–5 s.
    internal static readonly EventWaitHandle vmActiva = new EventWaitHandle(true, EventResetMode.ManualReset, @"Local\SurfaceDualVmActiva");
    internal static void Reintentar(int ms) { if (vmActiva.WaitOne(0)) Thread.Sleep(ms); else vmActiva.WaitOne(60000); }

    // R1: al reanudar, cada ayudante vivo y con el VmId de esta VM; si no, se lanza (iniciar.ps1 hace lo mismo en frío).
    static readonly string[] ayudantes = { "HvAudio.exe", "HvCamara.exe", "HvSensores.exe", "HvCarpetas.exe", "HvMdns.exe" };
    void Ayudantes()
    {
        string id = vmId.ToString(), dir = AppDomain.CurrentDomain.BaseDirectory;
        var vivos = new System.Collections.Generic.HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using (var s = new ManagementObjectSearcher("SELECT ProcessId, Name, CommandLine FROM Win32_Process WHERE Name LIKE 'Hv%.exe'"))
            foreach (ManagementObject p in s.Get())
            {
                string n = (string)p["Name"], c = (string)p["CommandLine"];
                if (Array.IndexOf(ayudantes, n) < 0) continue;
                // HvMdns no lleva VmId; sin línea de órdenes legible, se da por bueno (no matar a ciegas).
                if (n == "HvMdns.exe" || c == null || c.IndexOf(id, StringComparison.OrdinalIgnoreCase) >= 0) vivos.Add(n);
                else try { using (var v = Process.GetProcessById((int)(uint)p["ProcessId"])) v.Kill(); } catch (Exception) { }   // de otra VM
            }
        foreach (string n in ayudantes)
            if (!vivos.Contains(n))
            {
                Trace.WriteLine("ayudante relanzado: " + n);
                Process.Start(new ProcessStartInfo(System.IO.Path.Combine(dir, n), n == "HvMdns.exe" ? "" : id)
                    { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = dir }).Dispose();
            }
    }

    // Pausar (9) y reanudar (2) en orden de llegada, fuera del hilo de la interfaz (Msvm_ComputerSystem.RequestStateChange).
    void HiloVm()
    {
        ManagementObject vm = null;
        foreach (ushort pedido in estadosVm.GetConsumingEnumerable())
            try
            {
                ushort estado = pedido, mas;
                while (estadosVm.TryTake(out mas)) estado = mas;   // cambios rápidos: solo cuenta el último
                if (vm == null)
                    using (var s = new ManagementObjectSearcher(@"root\virtualization\v2",
                        "SELECT * FROM Msvm_ComputerSystem WHERE Name='" + vmId.ToString().ToUpperInvariant() + "'"))
                        foreach (ManagementObject o in s.Get()) vm = o;
                if (vm == null) continue;
                if (estado == 9) { vmActiva.Reset(); MusicaCompartida.Quitar(); }   // antes de pausar: nadie reintenta contra la VM congelada; sus botones no responderían
                var p = vm.GetMethodParameters("RequestStateChange");
                p["RequestedState"] = estado;
                vm.InvokeMethod("RequestStateChange", p, null);   // 4096 = trabajo asíncrono iniciado; 32775 = ya en ese estado
                // Esperar a que termine (EnabledState 2 = corriendo, 9 = en pausa; ~50–100 ms medido): una orden contraria
                // lanzada a mitad de una pausa la rechaza Hyper-V y la VM quedaría congelada con la ventana a la vista.
                for (int i = 0; i < 100; i++)
                {
                    vm.Get();
                    if (Convert.ToUInt16(vm["EnabledState"]) == estado) break;
                    Thread.Sleep(20);
                }
                // Tras desbloquear Windows o salir del reposo no llega OnActivated ni hay cuadro nuevo: quedaba negro (0.c, 16/11).
                // Oculto (ventana de avisos): sin la tecla de despertar, la pantalla de Android sigue como estaba.
                if (estado == 2)
                {
                    BeginInvoke((Action)(() => { if (visible.WaitOne(0)) { Despertar(); Invalidate(); } }));
                    vmActiva.Set();   // reintentos al momento
                    MusicaCompartida.Pedir();   // si la conexión con el launcher sobrevivió a la pausa
                    try { Ayudantes(); } catch (Exception e) { Trace.WriteLine("ayudantes: " + e.Message); }
                }
            }
            catch { vm = null; vmActiva.Set(); }   // VM rearmada (otro objeto): se busca de nuevo en la siguiente orden; sin saber su estado, reintentos normales
    }

    // "Cambiar a Windows" del launcher: Android no llega al host (firewall), así que el Visor se conecta al launcher
    // (AWindows.escuchar, puerto 4445) y espera la línea "windows"; entonces vuelve a Windows (Cambiar).
    void HiloLauncher()
    {
        while (!terminar)
        {
            try
            {
                string ip = IpVm();
                if (ip != null)
                    using (var c = new TcpClient())
                    {
                        // 3 s de tope: contra la VM en pausa el intento se quedaba ~21 s y la ventana de avisos se iba esperando.
                        if (!c.ConnectAsync(ip, 4445).Wait(3000)) throw new TimeoutException();
                        launcher = c;
                        // Keepalive de 2 s + 1 s: tras rearmar o reiniciar la VM el Android muerto no manda RST y ReadLine
                        // esperaba para siempre (el launcher nuevo no tenía visor: "No se pudo avisar a Windows", 17/11).
                        var ka = new byte[12];
                        BitConverter.GetBytes(1).CopyTo(ka, 0); BitConverter.GetBytes(2000).CopyTo(ka, 4); BitConverter.GetBytes(1000).CopyTo(ka, 8);
                        c.Client.IOControl(System.Net.Sockets.IOControlCode.KeepAliveValues, ka, null);
                        var s = c.GetStream();
                        Action<string> escribir = t => { byte[] b = System.Text.Encoding.ASCII.GetBytes(t + "\n"); lock (s) s.Write(b, 0, b.Length); };
                        Avisos.Conectado(escribir);
                        // La cortina escribe una línea por movimiento desde el hilo de la ventana: sin Nagle y sin quedarse esperando.
                        c.NoDelay = true; c.SendTimeout = 100; escribirLauncher = escribir;
                        MusicaCompartida.Escribir = escribir; MusicaCompartida.Pedir();   // la música de Android, a los controles de Windows
                        // Líneas con tope de 96 KB antes de interpretarlas (vienen de Android; la carátula de "media" ocupa hasta
                        // ~64 KB): la que se pasa se descarta entera.
                        var l = new System.Text.StringBuilder(); var buf = new byte[4096]; bool larga = false; int n;
                        while ((n = s.Read(buf, 0, buf.Length)) > 0)
                            for (int i = 0; i < n; i++)
                                if (buf[i] == '\n') { if (!larga) OrdenLauncher(l.ToString()); l.Clear(); larga = false; }
                                else if (l.Length < 96 * 1024) l.Append((char)buf[i]);
                                else larga = true;
                    }
            }
            catch { }
            escribirLauncher = null;
            MusicaCompartida.Escribir = null; MusicaCompartida.Quitar();   // sin launcher no hay quien obedezca los botones
            launcher = null;
            // Launcher aún sin arrancar o reiniciado: reintento a los 5 s. Con Android oculto (en pausa) no hay a quién
            // llamar: 60 s, o antes si ReconectarLauncher avisa (mostrar Android o ventana de avisos).
            // En una ventana de avisos, cada 1 s: la VM recién reanudada tarda en contestar y la ventana dura ~25 s.
            // VM en pausa: ni WMI ni conexión cada 60 s (creaban hilos del pool que despertaban ~10 veces/s); al reanudar, ya.
            if (!vmActiva.WaitOne(0)) WaitHandle.WaitAny(new WaitHandle[] { reconectarLauncher, vmActiva });
            else reconectarLauncher.WaitOne(visible.WaitOne(0) ? 5000 : Avisos.enVentana ? 1000 : 60000);
        }
    }

    volatile TcpClient launcher;
    volatile Action<string> escribirLauncher;   // línea al launcher por 4445 (null sin conexión)
    readonly AutoResetEvent reconectarLauncher = new AutoResetEvent(false);
    void ReconectarLauncher() { var c = launcher; if (c != null) try { c.Close(); } catch (Exception) { } reconectarLauncher.Set(); }

    // "windows" (Suspender) u "apagar" del launcher: por 4445 o, si esa conexión está caída, por el agente (Enlace "volver").
    void OrdenLauncher(string l)
    {
        if (l == "windows") { pedirMostrar.Reset(); pedirOcultar.Set(); }   // la última orden gana
        else if (l == "apagar") BeginInvoke((Action)Apagar);   // botón Apagar del launcher
        else if (l.StartsWith("media ")) MusicaCompartida.Linea(l);   // música de Android (Enlace.cs)
        else Avisos.Linea(l);   // "avisos on|off", "alarma <ms>", "aviso ..." (Avisos.cs)
    }

    // IP de la VM: su MAC (WMI de Hyper-V) en la tabla ARP, donde el Default Switch la deja como entrada permanente.
    [DllImport("iphlpapi.dll")] static extern int GetIpNetTable(IntPtr tabla, ref int tam, bool ordenar);
    string IpVm()
    {
        string mac = null;
        using (var s = new ManagementObjectSearcher(@"root\virtualization\v2", "SELECT InstanceID, Address FROM Msvm_SyntheticEthernetPortSettingData"))
            foreach (ManagementObject o in s.Get())
                if (((string)o["InstanceID"]).StartsWith("Microsoft:" + vmId, StringComparison.OrdinalIgnoreCase)) mac = (string)o["Address"];
        if (string.IsNullOrEmpty(mac)) return null;
        int n = 0; GetIpNetTable(IntPtr.Zero, ref n, false);
        IntPtr t = Marshal.AllocHGlobal(n);
        try
        {
            if (GetIpNetTable(t, ref n, false) != 0) return null;
            for (int i = 0, filas = Marshal.ReadInt32(t); i < filas; i++)
            {
                IntPtr f = t + 4 + i * 24;   // MIB_IPNETROW: índice, largo MAC, MAC[8], IPv4, tipo
                if (Marshal.ReadInt32(f, 4) != 6 || Marshal.ReadInt32(f, 20) == 2) continue;   // 2 = MIB_IPNET_TYPE_INVALID
                var b = new byte[6]; Marshal.Copy(f + 8, b, 0, 6);
                if (string.Equals(BitConverter.ToString(b).Replace("-", ""), mac, StringComparison.OrdinalIgnoreCase))
                    return new IPAddress((uint)Marshal.ReadInt32(f, 16)).ToString();
            }
        }
        finally { Marshal.FreeHGlobal(t); }
        return null;
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        terminar = true;
        visible.Reset();
        GestosWindows(true);
        Silencioso();   // terminar y visible ya dicen "fuera": todo vuelve a lo de Windows
        Pantalla120(false);
        try { sockActual?.Close(); } catch { }
        try { entrada?.Close(); } catch { } entrada = null;
        base.OnFormClosed(e);
        if (pantalla != null) { pantalla.Dispose(); pantalla = null; }
    }

    static bool LeerExacto(Socket s, byte[] b, int off, int tam)
    {
        int total = 0;
        while (total < tam)
        {
            int n = s.Receive(b, off + total, tam - total, SocketFlags.None);
            if (n <= 0) return false;
            total += n;
        }
        return true;
    }

    void AsegurarTamano(int nw, int nh)
    {
        if (nw == ancho && nh == alto) return;
        pantalla.Tamano(nw, nh);
        ancho = nw; alto = nh;
    }
    // Capa de vídeo DEVICE (F7/plan/video_capa_diseno.md): el Visor pinta el vídeo de Android debajo de la interfaz.
    // APAGADA por omisión (A/B 18/11, F7/plan/video_capa_medidas.txt: no bajó los núcleos de la VM: SurfaceFlinger sigue
    // recomponiendo el client target en cada cuadro). SURFACEDUAL_CAPA_VIDEO=1 la enciende; en Android, debug.sd.hwc.capa_video 0/1.
    static readonly bool capaVideo = Environment.GetEnvironmentVariable("SURFACEDUAL_CAPA_VIDEO") == "1";
    void HiloRed()
    {
        var servicio = Guid.Parse("00001389-facb-11e6-bd58-64006a7986d3"); // vsock 5001 en plantilla Linux
        var ep = new HvEndPoint(vmId, servicio);
        byte[] cabecera = new byte[40];
        byte[] rectCab = new byte[16];
        byte[] listo = new byte[8];
        listo[0] = 0x53; listo[1] = 0x44; listo[2] = 0x4F; listo[3] = 0x4B; // "SDOK" ("SDO4": el Visor pinta la capa de vídeo)
        byte[] registro = new byte[52], bufVideo = new byte[0];
        var rectangulos = new System.Collections.Generic.List<Rectangle>();
        var bloques = new System.Collections.Generic.List<byte[]>();

        while (!terminar)
        {
            Socket s = null;
            try
            {
                s = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1); // HV_PROTOCOL_RAW
                s.Connect(ep);
                sockActual = s;

                int cuadros = 0;
                long bytes = 0;
                var crono = Stopwatch.StartNew();
                double fps = 0, mbps = 0, msCopia = 0;

                while (!terminar)
                {
                    if (!LeerExacto(s, cabecera, 0, 40)) break;
                    if (BitConverter.ToUInt32(cabecera, 0) != 0x46504453) break; // "SDPF"

                    uint id = BitConverter.ToUInt32(cabecera, 8);
                    int nw = (int)BitConverter.ToUInt32(cabecera, 12);
                    int nh = (int)BitConverter.ToUInt32(cabecera, 16);
                    int nRect = (int)BitConverter.ToUInt32(cabecera, 24);

                    uint version = BitConverter.ToUInt32(cabecera, 4);
                    int escala = version == 1 ? 1 : (int)BitConverter.ToUInt32(cabecera, 28);
                    bool v4 = version == 4;   // capa de vídeo DEVICE: registro de 52 bytes; nRect puede ser 0
                    bool bc1 = (version == 3 || v4) && BitConverter.ToUInt32(cabecera, 20) == 71;
                    if ((!bc1 && (version != 1 && version != 2 && !v4 || BitConverter.ToUInt32(cabecera, 20) != 1)) ||
                        (bc1 || v4 ? escala != 1 : escala != 1 && escala != 2) ||
                        nw <= 0 || nh <= 0 || nw > 16384 || nh > 16384 || (long)nw * nh * 4 > int.MaxValue || nRect < (v4 ? 0 : 1))
                        throw new InvalidOperationException("Cabecera de pantalla inválida.");
                    rectangulos.Clear();

                    double msActualizacion = 0;
                    long bytesCuadro = 40;
                    bool hayVideo = false, videoNuevo = false; Rectangle videoDestino = Rectangle.Empty;
                    float[] videoRecorte = new float[4]; int videoAncho = 0, videoAlto = 0;
                    if (v4)
                    {   // {hayVideo, nuevo, formato, ancho, alto, destino x,y,w,h, recorte l,t,r,b (float)}; composer.cpp RegistroVideo
                        if (!LeerExacto(s, registro, 0, 52)) goto finConexion;
                        bytesCuadro += 52;
                        hayVideo = BitConverter.ToUInt32(registro, 0) != 0; videoNuevo = BitConverter.ToUInt32(registro, 4) != 0;
                        videoAncho = BitConverter.ToInt32(registro, 12); videoAlto = BitConverter.ToInt32(registro, 16);
                        videoDestino = new Rectangle(BitConverter.ToInt32(registro, 20), BitConverter.ToInt32(registro, 24),
                            BitConverter.ToInt32(registro, 28), BitConverter.ToInt32(registro, 32));
                        for (int k = 0; k < 4; k++) videoRecorte[k] = BitConverter.ToSingle(registro, 36 + 4 * k);
                        if ((hayVideo || videoNuevo) && (BitConverter.ToUInt32(registro, 8) != 71 || videoAncho <= 0 || videoAlto <= 0 ||
                            videoAncho > 16384 || videoAlto > 16384 || (videoAncho & 3) != 0 || (videoAlto & 3) != 0 ||
                            videoDestino.X < 0 || videoDestino.Y < 0 || videoDestino.Width <= 0 || videoDestino.Height <= 0 ||
                            (long)videoDestino.X + videoDestino.Width > nw || (long)videoDestino.Y + videoDestino.Height > nh ||
                            !(videoRecorte[0] >= 0 && videoRecorte[1] >= 0 && videoRecorte[2] > videoRecorte[0] && videoRecorte[3] > videoRecorte[1] &&
                              videoRecorte[2] <= videoAncho && videoRecorte[3] <= videoAlto)))
                            throw new InvalidOperationException("Registro de vídeo inválido.");
                    }

                    for (int i = 0; i < nRect; i++)
                    {
                        if (!LeerExacto(s, rectCab, 0, 16)) goto finConexion;
                        bytesCuadro += 16;
                        int rx = BitConverter.ToInt32(rectCab, 0);
                        int ry = BitConverter.ToInt32(rectCab, 4);
                        int rw = BitConverter.ToInt32(rectCab, 8);
                        int rh = BitConverter.ToInt32(rectCab, 12);

                        if (rx < 0 || ry < 0 || rw <= 0 || rh <= 0 || (long)rx + rw > nw || (long)ry + rh > nh)
                            throw new InvalidOperationException("Rectángulo fuera de pantalla.");
                        if (bc1 && ((rx & 3) != 0 || (ry & 3) != 0 || ((rw & 3) != 0 && rx + rw != nw) || ((rh & 3) != 0 && ry + rh != nh)))
                            throw new InvalidOperationException("Rectángulo BC1 no alineado a bloques.");
                        int tam = bc1 ? D3D.BytesBC1(rw, rh) : checked(((rw + escala - 1) / escala) * ((rh + escala - 1) / escala) * 4);
                        if (i == bloques.Count) bloques.Add(new byte[tam]);
                        else if (tam > bloques[i].Length) bloques[i] = new byte[tam];
                        byte[] pixelBuf = bloques[i];
                        if (!LeerExacto(s, pixelBuf, 0, tam)) goto finConexion;
                        bytesCuadro += tam;
                        // Diagnóstico (SURFACEDUAL_VOLCAR_BC1=1): guarda los 4 primeros rectángulos BC1 para decodificarlos aparte.
                        if (bc1 && volcados < 4 && Environment.GetEnvironmentVariable("SURFACEDUAL_VOLCAR_BC1") == "1")
                            try
                            {
                                using (var f = System.IO.File.Create(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "bc1_volcado_" + volcados++ + ".bin")))
                                {
                                    foreach (int v in new[] { rx, ry, rw, rh }) f.Write(BitConverter.GetBytes(v), 0, 4);
                                    f.Write(pixelBuf, 0, tam);
                                }
                            }
                            catch { }

                        rectangulos.Add(new Rectangle(rx, ry, rw, rh));
                        if (diagCuadros)   // brillo aproximado: BC1 = color0 de cada bloque (RGB565); RGBA = 1 de cada 16 píxeles
                            for (int k = 0; k + 3 < tam; k += bc1 ? 8 : 64)
                            {
                                int v = bc1 ? ((pixelBuf[k + 1] >> 3) * 8 + ((pixelBuf[k + 1] & 7) << 3 | pixelBuf[k] >> 5) * 4 + (pixelBuf[k] & 31) * 8) / 3
                                            : (pixelBuf[k] + pixelBuf[k + 1] + pixelBuf[k + 2]) / 3;
                                dSuma += v; dN++; if (v < 10) dNeg++;
                                int yy = bc1 ? ry + (k / 8) / ((rw + 3) / 4) * 4 : ry + (k / 4) / rw;
                                if (yy >= nh * 6 / 10) { aSuma += v; aN++; if (v < 10) aNeg++; }
                            }
                        dArea += (long)rw * rh;
                    }
                    if (videoNuevo)
                    {
                        int tamVideo = D3D.BytesBC1(videoAncho, videoAlto);
                        if (bufVideo.Length < tamVideo) bufVideo = new byte[tamVideo];
                        if (!LeerExacto(s, bufVideo, 0, tamVideo)) goto finConexion;
                        bytesCuadro += tamVideo;
                    }
                    if (diagCuadros)
                    {
                        try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_cuadros.log"),
                            string.Format("{0:HH:mm:ss.fff} id={1} rects={2} area={3}% brillo={4} negro={5}% {6} r={7},{8},{9}x{10} abajo={11}/{12}%\r\n", DateTime.Now, id, nRect,
                                dArea * 100 / ((long)nw * nh), dN > 0 ? dSuma / dN : -1, dN > 0 ? dNeg * 100 / dN : -1, bc1 ? "bc1" : "rgba",
                                rectangulos[0].X, rectangulos[0].Y, rectangulos[0].Width, rectangulos[0].Height,
                                aN > 0 ? aSuma / aN : -1, aN > 0 ? aNeg * 100 / aN : -1)); } catch { }
                        dSuma = dN = dNeg = dArea = aSuma = aN = aNeg = 0;
                    }
                    bool presentado = false;
                    Invoke((Action)(() =>
                    {
                        if (terminar) return;
                        var reloj = Stopwatch.StartNew();
                        if (sinPresentar) presentado = true;   // medición: recibir y confirmar sin dibujar
                        else
                        {
                            AsegurarTamano(nw, nh);
                            for (int i = 0; i < rectangulos.Count; i++) {
                                Rectangle r = rectangulos[i];
                                pantalla.Subir(r.X, r.Y, r.Width, r.Height, bloques[i], escala, bc1);
                            }
                            pantalla.Video(hayVideo, videoDestino, videoRecorte, videoAncho, videoAlto, videoNuevo ? bufVideo : null);
                            presentado = enFondo || pantalla.Presentar(ClientSize, Destino());   // en fondo: recibe y guarda, sin ventana
                        }
                        msActualizacion += reloj.Elapsed.TotalMilliseconds;
                        if (presentado) {
                            if (!primerCuadro)   // tiempo de cambio Windows -> Android (lo lee medir.ps1)
                            {
                                primerCuadro = true;
                                if (encendiendo != null) { encendiendo.Guardar(); Controls.Remove(encendiendo); encendiendo.Dispose(); encendiendo = null; }
                                try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_tiempos.log"),
                                    string.Format("{0:o} primer_cuadro_ms={1:F0}\r\n", DateTime.Now, (DateTime.Now - Process.GetCurrentProcess().StartTime).TotalMilliseconds)); } catch { }
                            }
                            msCopia = msActualizacion;
                            cuadros++;
                            Interlocked.Increment(ref cuadrosHz);
                            bytes += bytesCuadro;

                            if (crono.ElapsedMilliseconds >= 1000)
                            {
                                fps = cuadros * 1000.0 / crono.ElapsedMilliseconds;
                                mbps = (bytes / 1048576.0) / (crono.ElapsedMilliseconds / 1000.0);
                                cuadros = 0;
                                bytes = 0;
                                crono.Restart();
                                Text = string.Format("VisorVM - {0:F1} fps | {1:F1} MB/s | subida+present: {2:F2} ms ({3}x{4})",
                                    fps, mbps, msCopia, ancho, alto);
                            }

                        }
                    }));
                    if (!presentado) break; // Sin SDOK: al reconectar se recibe de nuevo la pantalla completa.
                    if (terminar) break;
                    listo[3] = (byte)((cabecera[32] & 1) != 0 && capaVideo ? 0x34 : 0x4B);   // el composer sabe v4: pedirlo
                    listo[4] = (byte)id; listo[5] = (byte)(id >> 8);
                    listo[6] = (byte)(id >> 16); listo[7] = (byte)(id >> 24);
                    s.Send(listo, 8, SocketFlags.None);
                }
            finConexion:;
            }
            catch (System.Reflection.TargetInvocationException ex)
            {
                if (!terminar && !IsDisposed)
                    Invoke((Action)(() =>
                    {
                        MessageBox.Show((ex.InnerException ?? ex).Message, "VisorVM - D3D11");
                        Close();
                    }));
            }
            catch { }
            finally
            {
                if (s != null) { try { s.Close(); } catch { } if (sockActual == s) sockActual = null; }
            }
            if (!terminar) Reintentar(1000);
        }
    }

    static Guid BuscarVm(string[] args)
    {
        if (args.Length > 0 && Guid.TryParse(args[0], out Guid id)) return id;
        try
        {
            using (var s = new ManagementObjectSearcher(@"root\virtualization\v2", "SELECT Name FROM Msvm_ComputerSystem WHERE ElementName='SurfaceDualAndroid'"))
            {
                foreach (ManagementObject o in s.Get())
                    if (Guid.TryParse((string)o["Name"], out id)) return id;
            }
        }
        catch { }
        try
        {
            var p = Process.Start(new ProcessStartInfo("powershell", "-NoProfile -Command \"(Get-VM -Name SurfaceDualAndroid).Id.ToString()\"")
            {
                RedirectStandardOutput = true, UseShellExecute = false, CreateNoWindow = true
            });
            string outStr = p.StandardOutput.ReadToEnd().Trim();
            p.WaitForExit();
            if (Guid.TryParse(outStr, out id)) return id;
        }
        catch { }
        throw new InvalidOperationException("No se encontró la VM 'SurfaceDualAndroid'. Pasa el VmId como argumento: VisorVM.exe <VmId>");
    }

    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();

    [STAThread]
    static int Main(string[] args)
    {
        if (args.Length == 1 && args[0] == "--autoprueba-avisos") return Avisos.Autoprueba();
        if (args.Length == 1 && args[0] == "--autoprueba-musica") return MusicaCompartida.Autoprueba();
        if (args.Length == 1 && args[0] == "--autoprueba-bc1")
        {
            if (D3D.BytesBC1(4, 4) != 8 || D3D.BytesBC1(5, 7) != 32 || D3D.BytesBC1(2880, 1920) != 2764800)
                throw new InvalidOperationException("Empaquetado BC1 incorrecto.");
            return 0;
        }
        // Una sola instancia: dos visores pelearían por la VM, el vsock y DirectX (revisión Gemini/DeepSeek 16/11).
        bool unico; var mutex = new Mutex(true, @"Local\SurfaceDualVisorVM.Unico", out unico);
        if (!unico) return 0;

        SetProcessDPIAware();
        RestaurarWindows();   // si el visor anterior se cerró de golpe con Android delante: gestos, giro, frecuencia y ahorro
        if (args.Length == 1 && args[0] == "--restaurar") return 0;   // solo eso (Bandeja al iniciar sesión; scripts tras matar el visor)
        AppDomain.CurrentDomain.UnhandledException += (s, e) => RestaurarWindows();   // excepción en un hilo: Windows no queda a 48 Hz ni sin giro
        Application.EnableVisualStyles();
        Guid vmId;
        try { vmId = BuscarVm(args); }
        catch (Exception ex) { MessageBox.Show(ex.Message, "VisorVM", MessageBoxButtons.OK, MessageBoxIcon.Error); return 1; }
        if (enFondo) { var f = new VisorVM(vmId); IntPtr h = f.Handle; Application.Run(); GC.KeepAlive(f); GC.KeepAlive(mutex); return 0; }   // diagnóstico: sin ventana
        Application.Run(new VisorVM(vmId));
        GC.KeepAlive(mutex);   // el mutex vive mientras el visor
        return 0;
    }
}

// Pantalla de encendido en frío: logo propio (robot dentro de una ventana), barra de progreso al estilo de Windows 11 y
// frases que van cambiando. La barra avanza según lo que tardó el último arranque (HKCU ArranqueMs; 36 s medidos) y se
// queda en 95 % hasta el primer cuadro real. Solo vive durante el arranque.
sealed class Arranque : Control
{
    static readonly string[] frases = { "Despertando al robot…", "Preparando la casa…", "Peleando por la RAM…",
        "Calentando la tarjeta gráfica…", "Acomodando tus apps…", "Poniendo la mesa…", "Casi listo…" };
    readonly Stopwatch reloj = Stopwatch.StartNew();
    readonly System.Windows.Forms.Timer animacion = new System.Windows.Forms.Timer { Interval = 33 };
    readonly int esperadoMs = (Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "ArranqueMs", 36000) as int?) ?? 36000;
    static readonly Color acento = Color.FromArgb(0x4C, 0xC2, 0xFF), verde = Color.FromArgb(0x3D, 0xDC, 0x84);

    public Arranque()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint, true);
        BackColor = Color.Black;
        animacion.Tick += (s, e) => Invalidate();
        animacion.Start();
    }

    public void Guardar()   // primer cuadro: recordar cuánto tardó para calibrar la barra la próxima vez
    {
        animacion.Stop();
        try { Microsoft.Win32.Registry.SetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "ArranqueMs", (int)Math.Min(120000, reloj.ElapsedMilliseconds)); } catch { }
    }

    protected override void Dispose(bool d) { if (d) animacion.Dispose(); base.Dispose(d); }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
        g.Clear(Color.Black);
        float u = Math.Min(Width, Height) / 100f;   // unidad: 1 % del lado corto
        float cx = Width / 2f, cy = Height * 0.42f;

        // Logo: ventana redondeada (blanca) con barra de título y, dentro, la cabeza de un robot verde con antenas.
        var ventana = new RectangleF(cx - 11 * u, cy - 9 * u, 22 * u, 18 * u);
        using (var marco = Redondeado(ventana, 2.2f * u))
        using (var lapiz = new Pen(Color.FromArgb(230, 255, 255, 255), 0.7f * u))
        using (var fondo = new SolidBrush(Color.FromArgb(28, 28, 28)))
        { g.FillPath(fondo, marco); g.DrawPath(lapiz, marco); g.DrawLine(lapiz, ventana.Left, ventana.Top + 3.6f * u, ventana.Right, ventana.Top + 3.6f * u); }
        using (var punto = new SolidBrush(Color.FromArgb(200, 255, 255, 255)))
            for (int i = 0; i < 3; i++) g.FillEllipse(punto, ventana.Right - (2.2f + 1.6f * i) * u - 0.5f * u, ventana.Top + 1.3f * u, u, u);
        using (var robot = new SolidBrush(verde))
        using (var antena = new Pen(verde, 0.6f * u) { StartCap = System.Drawing.Drawing2D.LineCap.Round, EndCap = System.Drawing.Drawing2D.LineCap.Round })
        {
            var cabeza = new RectangleF(cx - 6 * u, cy - 1.2f * u, 12 * u, 12 * u);
            g.FillPie(robot, cabeza.X, cabeza.Y, cabeza.Width, cabeza.Height, 180, 180);   // media luna: la cabeza
            g.DrawLine(antena, cx - 3.2f * u, cy - 0.5f * u, cx - 4.4f * u, cy - 2.6f * u);
            g.DrawLine(antena, cx + 3.2f * u, cy - 0.5f * u, cx + 4.4f * u, cy - 2.6f * u);
            using (var ojo = new SolidBrush(Color.FromArgb(28, 28, 28)))
            { g.FillEllipse(ojo, cx - 3.2f * u, cy + 1.6f * u, 1.3f * u, 1.3f * u); g.FillEllipse(ojo, cx + 1.9f * u, cy + 1.6f * u, 1.3f * u, 1.3f * u); }
        }

        // Barra de progreso de Windows 11: fina, redondeada, color de acento.
        float avance = Math.Min(0.95f, (float)reloj.ElapsedMilliseconds / Math.Max(5000, esperadoMs));
        var pista = new RectangleF(cx - 16 * u, cy + 15 * u, 32 * u, 0.6f * u);
        using (var p = Redondeado(pista, pista.Height / 2)) using (var b = new SolidBrush(Color.FromArgb(60, 60, 60))) g.FillPath(b, p);
        if (avance > 0.01f)
            using (var p = Redondeado(new RectangleF(pista.X, pista.Y, pista.Width * avance, pista.Height), pista.Height / 2))
            using (var b = new SolidBrush(acento)) g.FillPath(b, p);

        using (var titulo = new Font("Segoe UI Variable Display", 2.6f * u, FontStyle.Bold, GraphicsUnit.Pixel))
        using (var texto = new Font("Segoe UI Variable Text", 1.8f * u, GraphicsUnit.Pixel))
        using (var nota = new Font("Segoe UI Variable Text", 1.3f * u, GraphicsUnit.Pixel))
        using (var blanco = new SolidBrush(Color.White)) using (var gris = new SolidBrush(Color.FromArgb(170, 170, 170)))
        using (var centro = new StringFormat { Alignment = StringAlignment.Center })
        {
            g.DrawString("Surface Dual", titulo, blanco, cx, cy + 10.5f * u, centro);
            g.DrawString(frases[(int)(reloj.ElapsedMilliseconds / 2500) % frases.Length], texto, gris, cx, cy + 17.5f * u, centro);
            g.DrawString("El primer encendido tarda unos 30 segundos; después, cambiar entre Windows y Android es instantáneo.",
                nota, gris, cx, Height - 6 * u, centro);
        }
    }

    static System.Drawing.Drawing2D.GraphicsPath Redondeado(RectangleF r, float radio)
    {
        var p = new System.Drawing.Drawing2D.GraphicsPath(); float d = Math.Min(radio * 2, Math.Min(r.Width, r.Height));
        if (d <= 0) { p.AddRectangle(r); return p; }
        p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90); p.CloseFigure();
        return p;
    }
}

// Interop mínimo: índices de las tablas COM de d3d11.h y dxgi.h (Windows SDK).
sealed class D3D : IDisposable
{
    IntPtr dispositivo, contexto, cadena, destino, textura, vista, vs, ps, sampler;
    IntPtr destinoPantalla, auxiliar, vistaAuxiliar, parametros, psAuxiliar;
    IntPtr comprimida, vistaComprimida, psBC1;
    // Capa de vídeo (v4): textura BC1 del búfer de vídeo, su recorte y dónde va en la pantalla de Android.
    IntPtr video, vistaVideo, psVideo, psUI, mezcla;
    int anchoVideo, altoVideo; bool hayVideo; Rectangle videoDestino; float[] videoRecorte = new float[4];
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int CrearMezcla(IntPtr self, [In] int[] desc, out IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void FijarMezcla(IntPtr self, IntPtr estado, IntPtr factor, uint mascara);
    internal static int BytesBC1(int ancho, int alto) { return checked(((ancho + 3) / 4) * ((alto + 3) / 4) * 8); }
    int anchoAuxiliar, altoAuxiliar;
    int anchoVentana, altoVentana;
    [StructLayout(LayoutKind.Sequential)] struct Modo { public uint ancho, alto, numerador, denominador, formato, scanline, escalado; }
    [StructLayout(LayoutKind.Sequential)] struct CadenaDesc
    {
        public Modo modo;
        public uint muestras, calidad, uso, buffers;
        public IntPtr ventana;
        public int enVentana;
        public uint efecto, flags;
    }
    [StructLayout(LayoutKind.Sequential)] struct TexturaDesc
    {
        public uint ancho, alto, niveles, array, formato, muestras, calidad, uso, bind, cpu, misc;
    }
    [StructLayout(LayoutKind.Sequential)] struct SamplerDesc
    {
        public uint filtro, u, v, w;
        public float sesgo;
        public uint anisotropia, comparacion;
        public float r, g, b, a, minLod, maxLod;
    }
    [StructLayout(LayoutKind.Sequential)] struct BufferDesc { public uint bytes, uso, bind, cpu, misc, paso; }
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int CrearBuffer(IntPtr self, ref BufferDesc desc, IntPtr datos, out IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void ActualizarBuffer(IntPtr self, IntPtr recurso, uint sub, IntPtr caja, [In] float[] datos, uint paso, uint profundidad);
    [StructLayout(LayoutKind.Sequential)] struct Caja { public uint izquierda, arriba, frente, derecha, abajo, fondo; }
    [StructLayout(LayoutKind.Sequential)] struct Viewport { public float x, y, ancho, alto, min, max; }
    [DllImport("d3d11.dll", CallingConvention = CallingConvention.StdCall)]
    static extern int D3D11CreateDeviceAndSwapChain(IntPtr adaptador, uint driver, IntPtr software, uint flags,
        [In] uint[] niveles, uint cantidad, uint sdk, ref CadenaDesc desc, out IntPtr cadena,
        out IntPtr dispositivo, out uint nivel, out IntPtr contexto);
    [DllImport("d3dcompiler_47.dll", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
    static extern int D3DCompile([In] byte[] fuente, UIntPtr longitud, string nombre, IntPtr defines, IntPtr include,
        string entrada, string perfil, uint flags, uint flags2, out IntPtr codigo, out IntPtr errores);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int CrearTextura(IntPtr self, ref TexturaDesc desc, IntPtr datos, out IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int CrearVista(IntPtr self, IntPtr recurso, IntPtr desc, out IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int CrearShader(IntPtr self, IntPtr codigo, UIntPtr longitud, IntPtr clases, out IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int CrearSampler(IntPtr self, ref SamplerDesc desc, out IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int ObtenerBuffer(IntPtr self, uint indice, ref Guid iid, out IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Redimensionar(IntPtr self, uint cantidad, uint ancho, uint alto, uint formato, uint flags);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Present(IntPtr self, uint sync, uint flags);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Objetos(IntPtr self, uint inicio, uint cantidad, ref IntPtr objeto);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Shader(IntPtr self, IntPtr objeto, IntPtr clases, uint cantidad);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Topologia(IntPtr self, uint tipo);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void RenderTargets(IntPtr self, uint cantidad, ref IntPtr objeto, IntPtr profundidad);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Viewports(IntPtr self, uint cantidad, ref Viewport viewport);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Limpiar(IntPtr self, IntPtr objeto, [In] float[] color);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Dibujar(IntPtr self, uint vertices, uint inicio);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Actualizar(IntPtr self, IntPtr recurso, uint sub, ref Caja caja, IntPtr datos, uint paso, uint profundidad);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate IntPtr BlobPtr(IntPtr self);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate UIntPtr BlobTam(IntPtr self);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void SinArgumentos(IntPtr self);

    static T Metodo<T>(IntPtr objeto, int indice) where T : class
    {
        return (T)(object)Marshal.GetDelegateForFunctionPointer(
            Marshal.ReadIntPtr(Marshal.ReadIntPtr(objeto), indice * IntPtr.Size), typeof(T));
    }
    static void Comprobar(int hr) { Marshal.ThrowExceptionForHR(hr); }
    static void Soltar(ref IntPtr p) { if (p != IntPtr.Zero) { Marshal.Release(p); p = IntPtr.Zero; } }

    public D3D(IntPtr ventana)
    {
        // Comprobación pequeña de los tamaños ABI que afectan al interop ARM64.
        if (IntPtr.Size != 8 || Marshal.SizeOf(typeof(CadenaDesc)) != 72 ||
            Marshal.SizeOf(typeof(TexturaDesc)) != 44 || Marshal.SizeOf(typeof(SamplerDesc)) != 52)
            throw new PlatformNotSupportedException("El visor requiere el ABI ARM64 de 64 bits.");
        try
        {
            var desc = new CadenaDesc { modo = new Modo { formato = 28 }, muestras = 1,
                uso = 0x20, buffers = 2, ventana = ventana, enVentana = 1, efecto = 4 };
            uint nivel;
            Comprobar(D3D11CreateDeviceAndSwapChain(IntPtr.Zero, 1, IntPtr.Zero, 0,
                new uint[] { 0xb000 }, 1, 7, ref desc, out cadena, out dispositivo, out nivel, out contexto));
            const string fuente = @"
struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };
V VS(uint id : SV_VertexID) {
    V o; o.uv = float2((id << 1) & 2, id & 2);
    o.p = float4(o.uv * float2(2,-2) + float2(-1,1), 0, 1); return o;
}
Texture2D imagen : register(t0); SamplerState filtro : register(s0);
float4 PS(V i) : SV_Target { return float4(imagen.Sample(filtro, i.uv).rgb, 1); }";
            vs = Compilar(fuente, "VS", "vs_4_0", 12);
            ps = Compilar(fuente, "PS", "ps_4_0", 15);
            psAuxiliar = Compilar(fuente + @"
cbuffer Region : register(b0) { float2 origen; float2 limite; };
float4 PSAux(V i) : SV_Target {
    uint w,h; imagen.GetDimensions(w,h);
    float2 uv = clamp((i.p.xy-origen)*0.5, float2(0.5,0.5), limite-0.5)/float2(w,h);
    return float4(imagen.Sample(filtro, uv).rgb, 1);
}", "PSAux", "ps_4_0", 15);
            psBC1 = Compilar(fuente + @"
float4 PSBC1(V i) : SV_Target {
    uint w,h; imagen.GetDimensions(w,h);
    return imagen.Sample(filtro, i.p.xy / float2(w,h));   // con alfa: el hueco del vídeo es BC1 transparente
}", "PSBC1", "ps_4_0", 15);
            psVideo = Compilar(fuente + @"
cbuffer Region : register(b0) { float2 origen; float2 limite; };
float4 PSVideo(V i) : SV_Target { return float4(imagen.Sample(filtro, origen + i.uv * limite).rgb, 1); }", "PSVideo", "ps_4_0", 15);
            psUI = Compilar(fuente + @"
float4 PSUI(V i) : SV_Target { return imagen.Sample(filtro, i.uv); }", "PSUI", "ps_4_0", 15);
            // Interfaz encima del vídeo con alfa premultiplicado (SurfaceFlinger compone premultiplicado): ONE, INV_SRC_ALPHA.
            var md = new int[66]; md[2] = 1; md[3] = 2; md[4] = 6; md[5] = 1; md[6] = 2; md[7] = 6; md[8] = 1; md[9] = 0xF;
            Comprobar(Metodo<CrearMezcla>(dispositivo, 20)(dispositivo, md, out mezcla));
            var bd = new BufferDesc { bytes = 16, bind = 4 };
            Comprobar(Metodo<CrearBuffer>(dispositivo, 3)(dispositivo, ref bd, IntPtr.Zero, out parametros));
            var sd = new SamplerDesc { filtro = 0x15, u = 3, v = 3, w = 3, comparacion = 1, maxLod = float.MaxValue };
            Comprobar(Metodo<CrearSampler>(dispositivo, 23)(dispositivo, ref sd, out sampler));
        }
        catch { Dispose(); throw; }
    }

    IntPtr Compilar(string fuente, string entrada, string perfil, int indice)
    {
        IntPtr codigo = IntPtr.Zero, errores = IntPtr.Zero, shader;
        try
        {
            byte[] bytes = System.Text.Encoding.ASCII.GetBytes(fuente);
            int hr = D3DCompile(bytes, (UIntPtr)(uint)bytes.Length, "VisorVM", IntPtr.Zero, IntPtr.Zero,
                entrada, perfil, 1 << 15, 0, out codigo, out errores);
            if (hr < 0) throw new InvalidOperationException(errores == IntPtr.Zero ? "D3DCompile falló." :
                Marshal.PtrToStringAnsi(Metodo<BlobPtr>(errores, 3)(errores)));
            Comprobar(Metodo<CrearShader>(dispositivo, indice)(dispositivo,
                Metodo<BlobPtr>(codigo, 3)(codigo), Metodo<BlobTam>(codigo, 4)(codigo), IntPtr.Zero, out shader));
            return shader;
        }
        finally { Soltar(ref errores); Soltar(ref codigo); }
    }

    int anchoPantalla, altoPantalla;
    public void Tamano(int ancho, int alto)
    {
        anchoPantalla = ancho; altoPantalla = alto;
        IntPtr nula = IntPtr.Zero;
        Metodo<Objetos>(contexto, 8)(contexto, 0, 1, ref nula);
        Metodo<RenderTargets>(contexto, 33)(contexto, 0, ref nula, IntPtr.Zero);
        Soltar(ref vistaAuxiliar); Soltar(ref auxiliar);
        Soltar(ref vistaComprimida); Soltar(ref comprimida);
        Soltar(ref destinoPantalla); Soltar(ref vista); Soltar(ref textura);
        var desc = new TexturaDesc { ancho = (uint)ancho, alto = (uint)alto,
            niveles = 1, array = 1, formato = 28, muestras = 1, bind = 8 | 0x20 }; // DEFAULT, RGBA, SRV y render target
        Comprobar(Metodo<CrearTextura>(dispositivo, 5)(dispositivo, ref desc, IntPtr.Zero, out textura));
        Comprobar(Metodo<CrearVista>(dispositivo, 7)(dispositivo, textura, IntPtr.Zero, out vista));
        Comprobar(Metodo<CrearVista>(dispositivo, 9)(dispositivo, textura, IntPtr.Zero, out destinoPantalla));
        anchoAuxiliar = (ancho + 1) / 2; altoAuxiliar = (alto + 1) / 2;
        desc.ancho = (uint)anchoAuxiliar; desc.alto = (uint)altoAuxiliar; desc.bind = 8;
        Comprobar(Metodo<CrearTextura>(dispositivo, 5)(dispositivo, ref desc, IntPtr.Zero, out auxiliar));
        Comprobar(Metodo<CrearVista>(dispositivo, 7)(dispositivo, auxiliar, IntPtr.Zero, out vistaAuxiliar));
        desc.ancho = (uint)((ancho + 3) & ~3); desc.alto = (uint)((alto + 3) & ~3); desc.formato = 71;
        Comprobar(Metodo<CrearTextura>(dispositivo, 5)(dispositivo, ref desc, IntPtr.Zero, out comprimida));
        Comprobar(Metodo<CrearVista>(dispositivo, 7)(dispositivo, comprimida, IntPtr.Zero, out vistaComprimida));
    }

    public unsafe void Subir(int x, int y, int ancho, int alto, byte[] red, int escala, bool bc1 = false)
    {
        IntPtr nula = IntPtr.Zero;
        Metodo<Objetos>(contexto, 8)(contexto, 0, 1, ref nula);
        Metodo<RenderTargets>(contexto, 33)(contexto, 0, ref nula, IntPtr.Zero);
        if (bc1)
        {
            var bloques = new Caja { izquierda = (uint)x, arriba = (uint)y,
                derecha = (uint)(x + ((ancho + 3) & ~3)), abajo = (uint)(y + ((alto + 3) & ~3)), fondo = 1 };
            fixed (byte* p = red)
                Metodo<Actualizar>(contexto, 48)(contexto, comprimida, 0, ref bloques, (IntPtr)p, (uint)((ancho + 3) / 4) * 8, 0);
            DibujarTextura(ref destinoPantalla, ref vistaComprimida, new Rectangle(x, y, ancho, alto), false, true);
            Metodo<Objetos>(contexto, 8)(contexto, 0, 1, ref nula);
            Metodo<RenderTargets>(contexto, 33)(contexto, 0, ref nula, IntPtr.Zero);
            return;
        }
        int pw = (ancho + escala - 1) / escala, ph = (alto + escala - 1) / escala;
        var caja = new Caja { izquierda = (uint)(escala == 1 ? x : 0), arriba = (uint)(escala == 1 ? y : 0),
            derecha = (uint)(escala == 1 ? x + ancho : pw), abajo = (uint)(escala == 1 ? y + alto : ph), fondo = 1 };
        // El búfer recibido permanece fijado hasta que UpdateSubresource consume los datos.
        fixed (byte* p = red)
            Metodo<Actualizar>(contexto, 48)(contexto, escala == 1 ? textura : auxiliar, 0, ref caja, (IntPtr)p, (uint)pw * 4, 0);
        if (escala == 2)
        {
            // El viewport coloca el bloque ampliado en su rectángulo real.
            Metodo<ActualizarBuffer>(contexto, 48)(contexto, parametros, 0, IntPtr.Zero,
                new float[] { x, y, pw, ph }, 0, 0);
            Metodo<Objetos>(contexto, 16)(contexto, 0, 1, ref parametros);
            DibujarTextura(ref destinoPantalla, ref vistaAuxiliar, new Rectangle(x, y, ancho, alto), true);
            Metodo<Objetos>(contexto, 8)(contexto, 0, 1, ref nula);
            Metodo<RenderTargets>(contexto, 33)(contexto, 0, ref nula, IntPtr.Zero);
        }
    }

    // Capa de vídeo: hay = false la quita; bc1 != null trae un cuadro nuevo (ancho x alto, múltiplos de 4).
    public unsafe void Video(bool hay, Rectangle destino, float[] recorte, int ancho, int alto, byte[] bc1)
    {
        if (bc1 != null)
        {
            if (video == IntPtr.Zero || ancho != anchoVideo || alto != altoVideo)
            {
                Soltar(ref vistaVideo); Soltar(ref video);
                var desc = new TexturaDesc { ancho = (uint)ancho, alto = (uint)alto, niveles = 1, array = 1, formato = 71, muestras = 1, bind = 8 };
                Comprobar(Metodo<CrearTextura>(dispositivo, 5)(dispositivo, ref desc, IntPtr.Zero, out video));
                Comprobar(Metodo<CrearVista>(dispositivo, 7)(dispositivo, video, IntPtr.Zero, out vistaVideo));
                anchoVideo = ancho; altoVideo = alto;
            }
            var caja = new Caja { derecha = (uint)ancho, abajo = (uint)alto, fondo = 1 };
            fixed (byte* p = bc1)
                Metodo<Actualizar>(contexto, 48)(contexto, video, 0, ref caja, (IntPtr)p, (uint)(ancho / 4) * 8, 0);
        }
        hayVideo = hay && video != IntPtr.Zero;
        videoDestino = destino;
        if (hayVideo) { videoRecorte[0] = recorte[0] / anchoVideo; videoRecorte[1] = recorte[1] / altoVideo;
            videoRecorte[2] = (recorte[2] - recorte[0]) / anchoVideo; videoRecorte[3] = (recorte[3] - recorte[1]) / altoVideo; }
    }

    // La pantalla de Android en rect (coordenadas de destinoActual): sin vídeo, la textura opaca como siempre; con vídeo,
    // el vídeo en su sitio y encima la interfaz mezclada con su alfa (el hueco que deja SurfaceFlinger es transparente).
    void Componer(ref IntPtr destinoActual, Rectangle rect)
    {
        if (!hayVideo) { DibujarTextura(ref destinoActual, ref vista, rect); return; }
        float ex = (float)rect.Width / anchoPantalla, ey = (float)rect.Height / altoPantalla;
        var r = Rectangle.FromLTRB(rect.X + (int)Math.Round(videoDestino.Left * ex), rect.Y + (int)Math.Round(videoDestino.Top * ey),
            rect.X + (int)Math.Round(videoDestino.Right * ex), rect.Y + (int)Math.Round(videoDestino.Bottom * ey));
        Metodo<ActualizarBuffer>(contexto, 48)(contexto, parametros, 0, IntPtr.Zero, videoRecorte, 0, 0);
        Metodo<Objetos>(contexto, 16)(contexto, 0, 1, ref parametros);
        DibujarTextura(ref destinoActual, ref vistaVideo, r, pixel: psVideo);
        Metodo<FijarMezcla>(contexto, 35)(contexto, mezcla, IntPtr.Zero, 0xFFFFFFFF);
        DibujarTextura(ref destinoActual, ref vista, rect, pixel: psUI);
        Metodo<FijarMezcla>(contexto, 35)(contexto, IntPtr.Zero, IntPtr.Zero, 0xFFFFFFFF);
    }

    void DibujarTextura(ref IntPtr destinoActual, ref IntPtr origen, Rectangle rect, bool reducida = false, bool bc1 = false, IntPtr pixel = default(IntPtr))
    {
        Metodo<RenderTargets>(contexto, 33)(contexto, 1, ref destinoActual, IntPtr.Zero);
        var vp = new Viewport { x = rect.X, y = rect.Y, ancho = rect.Width, alto = rect.Height, max = 1 };
        Metodo<Viewports>(contexto, 44)(contexto, 1, ref vp);
        Metodo<Topologia>(contexto, 24)(contexto, 4);
        Metodo<Shader>(contexto, 11)(contexto, vs, IntPtr.Zero, 0);
        Metodo<Shader>(contexto, 9)(contexto, pixel != IntPtr.Zero ? pixel : bc1 ? psBC1 : reducida ? psAuxiliar : ps, IntPtr.Zero, 0);
        Metodo<Objetos>(contexto, 8)(contexto, 0, 1, ref origen);
        Metodo<Objetos>(contexto, 10)(contexto, 0, 1, ref sampler);
        Metodo<Dibujar>(contexto, 13)(contexto, 3, 0);
    }

    // Diagnóstico: guarda la imagen de Android que tiene el Visor (aunque la ventana no esté delante) como PNG a 1/4.
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void CopiarRecurso(IntPtr self, IntPtr destino, IntPtr origen);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Mapear(IntPtr self, IntPtr recurso, uint sub, int tipo, uint flags, out MapeoD3D m);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate void Desmapear(IntPtr self, IntPtr recurso, uint sub);
    [StructLayout(LayoutKind.Sequential)] struct MapeoD3D { public IntPtr datos; public uint pasoFila, pasoProfundidad; }
    public unsafe void Guardar(string ruta)
    {
        if (textura == IntPtr.Zero) return;
        var desc = new TexturaDesc { ancho = (uint)anchoPantalla, alto = (uint)altoPantalla, niveles = 1, array = 1, formato = 28, muestras = 1,
                                     uso = 3, cpu = 0x20000 };   // STAGING, lectura de CPU
        IntPtr copia, compuesta = IntPtr.Zero, rtv = IntPtr.Zero;
        Comprobar(Metodo<CrearTextura>(dispositivo, 5)(dispositivo, ref desc, IntPtr.Zero, out copia));
        try
        {
            if (hayVideo)   // la captura incluye el vídeo: se compone como en pantalla, a tamaño real
            {
                var cd = new TexturaDesc { ancho = (uint)anchoPantalla, alto = (uint)altoPantalla, niveles = 1, array = 1, formato = 28, muestras = 1, bind = 8 | 0x20 };
                Comprobar(Metodo<CrearTextura>(dispositivo, 5)(dispositivo, ref cd, IntPtr.Zero, out compuesta));
                Comprobar(Metodo<CrearVista>(dispositivo, 9)(dispositivo, compuesta, IntPtr.Zero, out rtv));
                Metodo<Limpiar>(contexto, 50)(contexto, rtv, new float[] { 0, 0, 0, 1 });
                Componer(ref rtv, new Rectangle(0, 0, anchoPantalla, altoPantalla));
                IntPtr nula = IntPtr.Zero;
                Metodo<RenderTargets>(contexto, 33)(contexto, 0, ref nula, IntPtr.Zero);
            }
            Metodo<CopiarRecurso>(contexto, 47)(contexto, copia, compuesta != IntPtr.Zero ? compuesta : textura);
            MapeoD3D m;
            Comprobar(Metodo<Mapear>(contexto, 14)(contexto, copia, 0, 1, 0, out m));
            try
            {
                int w = anchoPantalla / 4, h = altoPantalla / 4;
                using (var bmp = new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppRgb))
                {
                    var bd = bmp.LockBits(new Rectangle(0, 0, w, h), System.Drawing.Imaging.ImageLockMode.WriteOnly, bmp.PixelFormat);
                    for (int y = 0; y < h; y++)
                    {
                        byte* o = (byte*)m.datos + (long)(y * 4) * m.pasoFila; byte* d = (byte*)bd.Scan0 + (long)y * bd.Stride;
                        for (int x = 0; x < w; x++) { byte* p = o + x * 16; d[x * 4] = p[2]; d[x * 4 + 1] = p[1]; d[x * 4 + 2] = p[0]; d[x * 4 + 3] = 255; }
                    }
                    bmp.UnlockBits(bd);
                    bmp.Save(ruta, System.Drawing.Imaging.ImageFormat.Png);
                }
            }
            finally { Metodo<Desmapear>(contexto, 15)(contexto, copia, 0); }
        }
        finally { Soltar(ref copia); Soltar(ref rtv); Soltar(ref compuesta); }
    }

    public volatile uint sincronia = 1;   // 0 = Present sin esperar al panel (frecuencia adaptativa por debajo de 120 Hz)
    public bool Presentar(Size tam, Rectangle rect)
    {
        if (tam.Width <= 0 || tam.Height <= 0) return false;
        if (destino == IntPtr.Zero || anchoVentana != tam.Width || altoVentana != tam.Height)
        {
            IntPtr nula = IntPtr.Zero;
            Metodo<RenderTargets>(contexto, 33)(contexto, 0, ref nula, IntPtr.Zero);
            Soltar(ref destino);
            Comprobar(Metodo<Redimensionar>(cadena, 13)(cadena, 2, (uint)tam.Width, (uint)tam.Height, 28, 0));
            IntPtr buffer = IntPtr.Zero;
            try
            {
                var iid = new Guid("6f15aaf2-d208-4e89-9ab4-489535d34f9c");
                Comprobar(Metodo<ObtenerBuffer>(cadena, 9)(cadena, 0, ref iid, out buffer));
                Comprobar(Metodo<CrearVista>(dispositivo, 9)(dispositivo, buffer, IntPtr.Zero, out destino));
            }
            finally { Soltar(ref buffer); }
            anchoVentana = tam.Width; altoVentana = tam.Height;
        }
        // Flip descarta el contenido: redibujamos todo y enlazamos el destino tras cada Present.
        Metodo<RenderTargets>(contexto, 33)(contexto, 1, ref destino, IntPtr.Zero);
        Metodo<Limpiar>(contexto, 50)(contexto, destino, new float[] { 0, 0, 0, 1 });
        Componer(ref destino, rect);
        int hr = Metodo<Present>(cadena, 8)(cadena, sincronia, 0);
        Comprobar(hr);
        return hr == 0;
    }

    public void Dispose()
    {
        if (contexto != IntPtr.Zero) Metodo<SinArgumentos>(contexto, 110)(contexto);
        Soltar(ref vistaAuxiliar); Soltar(ref auxiliar); Soltar(ref destinoPantalla);
        Soltar(ref destino); Soltar(ref vista); Soltar(ref textura); Soltar(ref sampler);
        Soltar(ref parametros); Soltar(ref psAuxiliar);
        Soltar(ref vistaComprimida); Soltar(ref comprimida); Soltar(ref psBC1);
        Soltar(ref vistaVideo); Soltar(ref video); Soltar(ref psVideo); Soltar(ref psUI); Soltar(ref mezcla);
        Soltar(ref ps); Soltar(ref vs); Soltar(ref cadena); Soltar(ref contexto); Soltar(ref dispositivo);
    }
}
