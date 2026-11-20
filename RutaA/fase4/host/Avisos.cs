// Avisos de Android en Windows (F7/plan/avisos_diseno.md). Interruptor "Avisos en Windows" de los ajustes rápidos de Android,
// apagado por omisión: apagado, aquí no corre nada (ni temporizador) y la VM queda en pausa total, como siempre.
// Encendido, con Android oculto: cada 15 min (o a la hora de la próxima alarma de Reloj) el visor reanuda la VM sin mostrarla,
// el agente corta la conexión de push muerta ("despierta"), el launcher manda lo nuevo por 4445 y la VM vuelve a la pausa.
// Líneas del launcher: "avisos on|off", "alarma <ms UTC|0>", "aviso <id hex> <app> <título> <texto>" (base64 UTF-8 o "-").
// Al launcher: "pendientes <ms UTC desde que se ocultó>" y "abrir <id>".
// Se enseñan en una ventanita propia y no como toast: en esta Surface las notificaciones de Windows están desactivadas
// (ToastEnabled=0) y ese ajuste es del usuario. Se respeta "no molestar"/pantalla completa (SHQueryUserNotificationState).
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

static class Avisos
{
    const string Clave = @"HKEY_CURRENT_USER\Software\SurfaceDual";
    static readonly DateTime Epoca = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);
    static volatile bool activos = ((Microsoft.Win32.Registry.GetValue(Clave, "AvisosAndroid", 0) as int?) ?? 0) == 1;   // guardado: el visor puede reiniciarse con Android en pausa
    static long alarmaMs;                       // próxima alarma de Reloj (ms UTC; 0 = ninguna)
    internal static volatile bool enVentana; static volatile bool bloqueada;  // bloqueada: sesión de Windows bloqueada (nadie mira: sin ventanas)
    static DateTime oculto, ultima, fin, tope;  // solo en el hilo de la interfaz
    static readonly HashSet<string> mostrados = new HashSet<string>();
    static readonly System.Windows.Forms.Timer reloj = new System.Windows.Forms.Timer();
    static Form visor; static Func<bool> visible; static Action<ushort> estadoVm; static Action mostrarAndroid, reconectar;
    static Globo globo;

    /// <summary>Cuándo toca la siguiente ventana: 15 min después de la última o, si cae antes, a la hora de la alarma.</summary>
    internal static DateTime Proxima(DateTime ultima, long alarmaMs)
    {
        DateTime v = ultima.AddMinutes(15), a = Epoca.AddMilliseconds(Math.Max(0, Math.Min(alarmaMs, 4102444800000)));
        return a > ultima && a < v ? a : v;
    }

    /// <summary>Valida y descifra una línea "aviso": null si no cumple el formato (frontera de confianza: viene de Android).</summary>
    internal static string[] Leer(string l)
    {
        string[] p = l.Split(' ');
        if (p.Length != 5 || p[0] != "aviso" || !Regex.IsMatch(p[1], "^[0-9a-f]{1,16}$")) return null;
        var r = new string[4]; r[0] = p[1];
        for (int i = 2; i < 5; i++)
            try { r[i - 1] = p[i] == "-" ? "" : Regex.Replace(Encoding.UTF8.GetString(Convert.FromBase64String(p[i])), @"\p{C}+", " ").Trim(); }
            catch (FormatException) { return null; }
        return r;
    }

    internal static int Autoprueba()
    {
        var t = new DateTime(2026, 11, 17, 10, 0, 0, DateTimeKind.Utc);
        long ms = (long)(t - Epoca).TotalMilliseconds;
        bool ok = Proxima(t, 0) == t.AddMinutes(15)                       // sin alarma
            && Proxima(t, ms + 5 * 60000) == t.AddMinutes(5)              // alarma antes de la ventana
            && Proxima(t, ms + 20 * 60000) == t.AddMinutes(15)            // alarma después: la ventana de siempre
            && Proxima(t, ms - 60000) == t.AddMinutes(15)                 // alarma pasada
            && Proxima(t, long.MaxValue) == t.AddMinutes(15)              // valor absurdo: sin desbordar
            && Leer("aviso 1f QQ== - SG9sYQ==") != null && Leer("aviso 1f QQ== - SG9sYQ==")[3] == "Hola" && Leer("aviso 1f QQ== - SG9sYQ==")[2] == ""
            && Leer("aviso XYZ QQ== - -") == null && Leer("aviso 1f QQ== - ###") == null && Leer("aviso 1f QQ==") == null
            && Leer("aviso 1f " + Convert.ToBase64String(new byte[] { 65, 10, 7, 66 }) + " - -")[1] == "A B";   // sin caracteres de control
        return ok ? 0 : 1;
    }

    // En el hilo de la ventana. estadoVm: pausar (9) / reanudar (2); reconectar: rehacer ya la conexión con el launcher.
    internal static void Iniciar(Form f, Func<bool> esVisible, Action<ushort> estado, Action mostrar, Action reconectarLauncher)
    {
        visor = f; visible = esVisible; estadoVm = estado; mostrarAndroid = mostrar; reconectar = reconectarLauncher;
        reloj.Tick += (s, e) => Tic();
        Microsoft.Win32.SystemEvents.SessionSwitch += (s, e) =>
        {
            if (e.Reason == Microsoft.Win32.SessionSwitchReason.SessionLock) { bloqueada = true; Cortar(); }
            else if (e.Reason == Microsoft.Win32.SessionSwitchReason.SessionUnlock) bloqueada = false;
        };
        Microsoft.Win32.SystemEvents.PowerModeChanged += (s, e) => { if (e.Mode == Microsoft.Win32.PowerModes.Suspend) Cortar(); };
    }

    // Windows se bloquea o duerme a mitad de una ventana: la VM vuelve a la pausa ya (no se queda corriendo).
    static void Cortar() { try { visor.BeginInvoke((Action)(() => { if (enVentana) { fin = DateTime.UtcNow; Tic(); } })); } catch (Exception) { } }   // ventana cerrada

    internal static void Ocultado()
    {
        if (!activos) return;   // apagado: nada que hacer, pausa total
        oculto = ultima = DateTime.UtcNow; enVentana = false; mostrados.Clear();
        Programar();
    }

    internal static void Mostrado()
    {
        reloj.Stop(); enVentana = false;
        if (globo != null) globo.Hide();
    }

    // Por hora real (como el apagado automático): si la Surface duerme, el temporizador no avanza; por eso 60 s como mucho.
    static void Programar()
    {
        double falta = ((enVentana ? fin : Proxima(ultima, Interlocked.Read(ref alarmaMs))) - DateTime.UtcNow).TotalMilliseconds;
        reloj.Interval = (int)Math.Max(1000, Math.Min(60000, falta));
        reloj.Start();
    }

    static void Tic()
    {
        reloj.Stop();
        if (visible()) { enVentana = false; return; }   // Android delante: Cambiar ya reanudó; aquí no se pausa nada
        if (!activos) { if (enVentana) estadoVm(9); enVentana = false; return; }   // apagado a mitad de ventana: de vuelta a la pausa
        DateTime ahora = DateTime.UtcNow;
        if (enVentana)
        {
            if (ahora >= fin) { enVentana = false; ultima = ahora; estadoVm(9); Anotar("ventana fin"); }
        }
        else if (ahora >= Proxima(ultima, Interlocked.Read(ref alarmaMs)))
        {
            if (bloqueada) ultima = ahora;   // se salta; al desbloquear, la siguiente
            else
            {
                enVentana = true; fin = ahora.AddSeconds(25); tope = ahora.AddSeconds(75);
                estadoVm(2); Anotar("ventana inicio");
                ControlesCompartidos.Enviar("despierta", "1");   // el agente corta la conexión de push muerta: GMS reconecta al momento
                reconectar();                                     // y el launcher recibe "pendientes" en cuanto conecte
            }
        }
        Programar();
    }

    /// <summary>Recién conectado con el launcher (hilo del launcher): lo que haya que pedirle.</summary>
    internal static void Conectado(Action<string> escribir)
    {
        if (enVentana) { escribir("pendientes " + (long)(oculto - Epoca).TotalMilliseconds); Anotar("launcher conectado"); }
        string a = Interlocked.Exchange(ref abrirPendiente, null);
        if (a != null) escribir("abrir " + a);
    }
    static string abrirPendiente;   // aviso tocado en Windows, a la espera de conexión con el launcher

    /// <summary>Línea del launcher que no es "windows"/"apagar" (hilo del launcher; ya acotada a 4 KB).</summary>
    internal static void Linea(string l)
    {
        if (l == "avisos on" || l == "avisos off")
        {
            activos = l == "avisos on";
            try { Microsoft.Win32.Registry.SetValue(Clave, "AvisosAndroid", activos ? 1 : 0); } catch (Exception) { }
            return;
        }
        long ms;
        if (l.StartsWith("alarma ") && long.TryParse(l.Substring(7), out ms) && ms >= 0) { Interlocked.Exchange(ref alarmaMs, ms); return; }
        string[] a = activos ? Leer(l) : null;
        if (a != null) try { visor.BeginInvoke((Action)(() => Mostrar(a))); } catch (Exception) { }   // ventana cerrada
    }

    [DllImport("shell32.dll")] static extern int SHQueryUserNotificationState(out int estado);
    static void Mostrar(string[] a)
    {
        int q;
        if (visible() || mostrados.Contains(a[0])) return;   // con Android delante ya se ve allí
        if (SHQueryUserNotificationState(out q) != 0 || q != 5) return;   // no molestar, pantalla completa o presentación: se reintenta en la siguiente ventana
        mostrados.Add(a[0]);
        if (enVentana) { DateTime f = DateTime.UtcNow.AddSeconds(8); fin = f > tope ? tope : f > fin ? f : fin; }   // tras una ráfaga suelen venir más
        if (globo == null) globo = new Globo(id =>
        {
            Interlocked.Exchange(ref abrirPendiente, id);
            mostrarAndroid();   // muestra y reanuda
            reconectar();       // la conexión con el launcher puede estar muerta por la pausa: al rehacerla sale "abrir"
        });
        globo.Poner(a);
        Anotar("aviso mostrado " + a[0]);   // sin contenido: solo el id
    }
    // Al registro de tiempos del visor (el mismo de Cambiar): para medir ventanas y latencia.
    static void Anotar(string t)
    {
        try { System.IO.File.AppendAllText(System.IO.Path.Combine(System.IO.Path.GetTempPath(), "visorvm_tiempos.log"), string.Format("{0:o} avisos {1}\r\n", DateTime.Now, t)); } catch (Exception) { }
    }
}

// Ventanita abajo a la derecha, sin robar el foco; se quita sola a los 8 s; tocarla abre el aviso en Android.
sealed class Globo : Form
{
    readonly Label app = new Label(), titulo = new Label(), texto = new Label();
    readonly System.Windows.Forms.Timer quitar = new System.Windows.Forms.Timer { Interval = 8000 };
    string id; int juntos;
    protected override bool ShowWithoutActivation { get { return true; } }
    protected override CreateParams CreateParams { get { var c = base.CreateParams; c.ExStyle |= 0x08000000 | 0x80 | 0x8; return c; } }   // NOACTIVATE, TOOLWINDOW, TOPMOST

    public Globo(Action<string> abrir)
    {
        FormBorderStyle = FormBorderStyle.None; ShowInTaskbar = false; StartPosition = FormStartPosition.Manual;
        BackColor = Color.FromArgb(32, 32, 32); Padding = new Padding(16, 12, 16, 12);
        Font = new Font("Segoe UI", 10f);
        foreach (Label l in new[] { texto, titulo, app })   // Dock.Top: el último añadido queda arriba
        {
            l.Dock = DockStyle.Top; l.AutoSize = false; l.UseMnemonic = false; l.AutoEllipsis = true; l.ForeColor = Color.White;
            l.Click += (s, e) => { Hide(); abrir(id); };
            Controls.Add(l);
        }
        app.ForeColor = Color.FromArgb(170, 170, 170);
        titulo.Font = new Font(Font, FontStyle.Bold);
        Click += (s, e) => { Hide(); abrir(id); };
        quitar.Tick += (s, e) => Hide();
        VisibleChanged += (s, e) => { if (!Visible) { quitar.Stop(); juntos = 0; } };
    }

    public void Poner(string[] a)
    {
        id = a[0]; juntos++;
        app.Text = a[1] + " · Android" + (juntos > 1 ? Idioma.T("  (y " + (juntos - 1) + " más)", "  (and " + (juntos - 1) + " more)") : "");
        titulo.Text = a[2]; texto.Text = a[3];
        int linea = Font.Height + 4;
        app.Height = titulo.Height = linea; texto.Height = linea * 2;
        Rectangle z = Screen.PrimaryScreen.WorkingArea;
        Size = new Size(Math.Min(z.Width - 32, linea * 18), linea * 4 + Padding.Vertical);
        Location = new Point(z.Right - Width - 16, z.Bottom - Height - 16);
        quitar.Stop(); quitar.Start();
        if (!Visible) Show();
    }
}
