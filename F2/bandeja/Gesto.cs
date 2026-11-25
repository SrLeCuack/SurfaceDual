// Gesto de 3 dedos quietos (~0,7 s) para cambiar de sistema (issue #7 de BeShO-ShO, 25/11): desde Windows entra a Android; con
// Android delante vuelve a Windows. Lee la pantalla táctil por Raw Input con RIDEV_INPUTSINK: solo escucha (también con el Visor
// delante), no bloquea ni cambia ningún toque. Moverse (más del 3 % de la pantalla), poner otro dedo o levantar antes lo cancela;
// tras dispararse, hay que levantar todos los dedos. Los gestos de 3 dedos de Windows son deslizar y el de Android en el Visor
// también, así que mantener quieto no choca con ninguno. HKCU\Software\SurfaceDual: GestoDedos (3; 0 = apagado), GestoMs (700).
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

static partial class Bandeja
{
    class TresDedos : NativeWindow
    {
        [StructLayout(LayoutKind.Sequential)] struct Dispositivo { public ushort Pagina, Uso; public uint Flags; public IntPtr Ventana; }
        [DllImport("user32.dll")] static extern bool RegisterRawInputDevices(Dispositivo[] d, uint n, uint tam);
        [DllImport("user32.dll")] static extern uint GetRawInputData(IntPtr h, uint orden, byte[] datos, ref uint tam, uint tamCab);
        [DllImport("user32.dll")] static extern uint GetRawInputDeviceInfo(IntPtr dev, uint orden, IntPtr datos, ref uint tam);
        [DllImport("hid.dll")] static extern int HidP_GetCaps(IntPtr pp, byte[] caps);
        [DllImport("hid.dll")] static extern int HidP_GetUsageValue(int tipo, ushort pagina, ushort enlace, ushort uso, out uint valor, IntPtr pp, byte[] informe, uint largo);
        [DllImport("hid.dll")] static extern int HidP_GetUsages(int tipo, ushort pagina, ushort enlace, ushort[] usos, ref uint n, IntPtr pp, byte[] informe, uint largo);
        [DllImport("hid.dll")] static extern int HidP_GetSpecificValueCaps(int tipo, ushort pagina, ushort enlace, ushort uso, byte[] caps, ref ushort n, IntPtr pp);
        const int Bien = 0x00110000;   // HIDP_STATUS_SUCCESS

        class Pantalla { public IntPtr Pp; public int Enlaces; public uint MaxX = 1, MaxY = 1; }
        class Dedo { public uint X, Y, X0, Y0; public bool Abajo; public int Visto; }
        readonly Dictionary<IntPtr, Pantalla> pantallas = new Dictionary<IntPtr, Pantalla>();
        readonly Dictionary<uint, Dedo> dedos = new Dictionary<uint, Dedo>();
        readonly int cuantos, ms;
        int desde;   // TickCount del inicio de la espera; 0 = sin espera
        bool hecho, anulado;   // disparado o cancelado: hasta levantar todos los dedos
        int quedan = int.MaxValue;   // contactos que faltan por leer en este cuadro (sin Contact Count: todos)

        public TresDedos()
        {
            cuantos = (Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "GestoDedos", 3) as int?) ?? 3;
            ms = (Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\SurfaceDual", "GestoMs", 700) as int?) ?? 700;
            if (cuantos <= 0) return;
            CreateHandle(new CreateParams());
            // Digitalizador (0x0D) / pantalla táctil (0x04), RIDEV_INPUTSINK (0x100): llega aunque la Bandeja no esté delante.
            var d = new[] { new Dispositivo { Pagina = 0x0D, Uso = 0x04, Flags = 0x100, Ventana = Handle } };
            if (!RegisterRawInputDevices(d, 1, (uint)Marshal.SizeOf(typeof(Dispositivo)))) System.Diagnostics.Trace.WriteLine("gesto: sin pantalla táctil");
        }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == 0x00FF) try { Leer(m.LParam); } catch (Exception e) { System.Diagnostics.Trace.WriteLine("gesto: " + e.Message); }   // WM_INPUT
            base.WndProc(ref m);
        }

        Pantalla Datos(IntPtr dev)
        {
            Pantalla p;
            if (pantallas.TryGetValue(dev, out p)) return p;
            uint tam = 0;
            if (GetRawInputDeviceInfo(dev, 0x20000005, IntPtr.Zero, ref tam) != 0 || tam == 0) return null;   // RIDI_PREPARSEDDATA
            p = new Pantalla { Pp = Marshal.AllocHGlobal((int)tam) };   // se queda mientras viva la Bandeja (una por pantalla)
            if (GetRawInputDeviceInfo(dev, 0x20000005, p.Pp, ref tam) == uint.MaxValue) { Marshal.FreeHGlobal(p.Pp); return null; }   // desconectada (Gemini)
            var caps = new byte[64]; HidP_GetCaps(p.Pp, caps);
            p.Enlaces = BitConverter.ToUInt16(caps, 44);   // HIDP_CAPS.NumberLinkCollectionNodes
            var v = new byte[72 * 16]; ushort n = 16;   // HIDP_VALUE_CAPS: LogicalMax en el desplazamiento 44
            if (HidP_GetSpecificValueCaps(0, 0x01, 0, 0x30, v, ref n, p.Pp) == Bien && n > 0) p.MaxX = (uint)Math.Max(1, BitConverter.ToInt32(v, 44));   // LONG
            n = 16; if (HidP_GetSpecificValueCaps(0, 0x01, 0, 0x31, v, ref n, p.Pp) == Bien && n > 0) p.MaxY = (uint)Math.Max(1, BitConverter.ToInt32(v, 44));
            pantallas[dev] = p;
            return p;
        }

        void Leer(IntPtr h)
        {
            int cab = IntPtr.Size == 8 ? 24 : 16;   // RAWINPUTHEADER
            uint tam = 0; GetRawInputData(h, 0x10000003, null, ref tam, (uint)cab);   // RID_INPUT
            var b = new byte[tam];
            if (GetRawInputData(h, 0x10000003, b, ref tam, (uint)cab) != tam || BitConverter.ToUInt32(b, 0) != 2) return;   // RIM_TYPEHID
            var p = Datos(IntPtr.Size == 8 ? (IntPtr)BitConverter.ToInt64(b, 8) : (IntPtr)BitConverter.ToInt32(b, 8));
            if (p == null) return;
            uint largo = BitConverter.ToUInt32(b, cab), veces = BitConverter.ToUInt32(b, cab + 4);
            int ahora = Environment.TickCount;
            var usos = new ushort[32];
            for (uint r = 0; r < veces; r++)
            {
                var inf = new byte[largo]; Array.Copy(b, cab + 8 + r * largo, inf, 0, largo);
                // Contact Count: en modo híbrido solo el primer informe del cuadro lo trae (los demás, 0) y las ranuras sobrantes
                // van a cero; sin él, una ranura vacía con Id 0 "levantaría" al dedo 0 (revisión Codex; Microsoft, packet reporting modes).
                uint cc; if (HidP_GetUsageValue(0, 0x0D, 0, 0x54, out cc, p.Pp, inf, largo) == Bien && cc > 0) quedan = (int)cc;
                // Cada dedo va en su colección lógica (1..n); la 0 busca en toda la superior y repetiría el primero (Codex, Gemini).
                for (ushort l = (ushort)(p.Enlaces > 1 ? 1 : 0); l < Math.Max(1, p.Enlaces) && quedan > 0; l++)
                {
                    uint id, x, y;
                    if (HidP_GetUsageValue(0, 0x0D, l, 0x51, out id, p.Pp, inf, largo) != Bien) continue;   // Contact Identifier
                    if (HidP_GetUsageValue(0, 0x01, l, 0x30, out x, p.Pp, inf, largo) != Bien || HidP_GetUsageValue(0, 0x01, l, 0x31, out y, p.Pp, inf, largo) != Bien) continue;
                    quedan--;
                    uint n = (uint)usos.Length; bool abajo = false;
                    if (HidP_GetUsages(0, 0x0D, l, usos, ref n, p.Pp, inf, largo) == Bien) for (int i = 0; i < n; i++) if (usos[i] == 0x42) abajo = true;   // Tip Switch
                    Dedo d;
                    if (!dedos.TryGetValue(id, out d)) dedos[id] = d = new Dedo();
                    if (abajo && !d.Abajo) { d.X0 = x; d.Y0 = y; }
                    d.X = x; d.Y = y; d.Abajo = abajo; d.Visto = ahora;
                    if (abajo && desde != 0 && (Math.Abs((long)x - d.X0) * 100 > p.MaxX * 3 || Math.Abs((long)y - d.Y0) * 100 > p.MaxY * 3)) anulado = true;   // se movió
                }
            }
            int puestos = 0;
            foreach (var d in dedos.Values) if (d.Abajo && ahora - d.Visto < 300) puestos++;   // uno sin noticias 0,3 s: se dio por levantado
            if (puestos == 0) { desde = 0; hecho = anulado = false; dedos.Clear(); return; }
            if (puestos != cuantos) { if (desde != 0) anulado = true; return; }   // otro dedo, o se levantó uno: cancela
            if (hecho || anulado) return;
            if (desde == 0) { desde = ahora; foreach (var d in dedos.Values) { d.X0 = d.X; d.Y0 = d.Y; } return; }
            if (ahora - desde < ms) return;
            hecho = true;
            EventWaitHandle v; bool androidDelante = EventWaitHandle.TryOpenExisting(@"Local\SurfaceDualVisorVM.Visible", out v) && v.WaitOne(0);
            if (v != null) v.Dispose();
            if (androidDelante) Salir(); else Entrar();
        }
    }
}
