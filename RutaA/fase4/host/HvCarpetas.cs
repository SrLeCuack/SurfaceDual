// Carpetas de Windows dentro de Android: servidor 9P2000.L por hv_sock (vsock 5008; lado Android: RutaA/fase4/carpetas).
// Android (sdcarpetas) monta el 9p del kernel con trans=fd y enlaza cada carpeta en /data/media/0 (Movies, Pictures,
// Download, Documents, DCIM): MediaStore, galerías, VLC y Files las ven como propias y no se duplica nada en disco.
// El invitado es menos fiable que el host: solo estas carpetas; sin "..", enlaces/uniones, flujos ":" ni nombres reservados;
// tamaños acotados; el proyecto SurfaceDual (ejecutables del host) queda fuera; lo que escribe Android lleva la marca de
// Internet (Zone.Identifier) como cualquier descarga; los archivos de OneDrive que están solo en la nube no se abren
// (bajarían GB al indexar la galería: "Mantener siempre en este dispositivo" los hace visibles).
//   HvCarpetas.exe <VmId>     (iniciar.ps1)
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

static class HvCarpetas
{
    const int Puerto = 5008, MsizeMax = 1 << 20, FidsMax = 1 << 18, Trabajadores = 16;
    const uint GidMediaRw = 1023;
    // errno de Linux
    const int EPERM = 1, ENOENT = 2, EIO = 5, EBADF = 9, EACCES = 13, EBUSY = 16, EEXIST = 17, EXDEV = 18, ENOTDIR = 20,
              EISDIR = 21, EINVAL = 22, EMFILE = 24, ENOSPC = 28, ENAMETOOLONG = 36, ENOTEMPTY = 39, ENODATA = 61,
              EPROTO = 71, EOPNOTSUPP = 95;

    class Fid { public string Ruta; public FileStream F; public bool Dir, Xattr, Marcado; public List<string> Lista; }
    class Error9 : Exception { public readonly int N; public Error9(int n) { N = n; } }

    // Carpeta de Android -> carpeta conocida de Windows (la real, también si está en OneDrive).
    static readonly string[,] Mapa = {
        { "Movies", "18989B1D-99B5-455B-841C-AB7C74E4DDFC" }, { "Pictures", "33E28130-4E1E-4676-835A-98395C3BC3BB" },
        { "Download", "374DE290-123F-4565-9164-39C4925E467B" }, { "Documents", "FDD39AD0-238F-46AF-ADB4-6C85480369C7" },
        { "DCIM", "AB5FB87B-7CE2-4F83-915D-550846C9F2B5" } };   // DCIM = Álbum de cámara
    static readonly Dictionary<string, string> Recursos = new Dictionary<string, string>();
    static readonly List<string> Fuera = new List<string>();   // rutas que no se ven nunca (otros recursos anidados, el proyecto)

    // ---------- Win32 ----------
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 4)]   // WIN32_FIND_DATAW (FILETIME alinea a 4)
    struct Datos
    {
        public FileAttributes Atrib; public long Creado, Acceso, Escrito; public uint TamAlto, TamBajo, Etiqueta, R1;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string Nombre;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)] public string Corto;
    }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr FindFirstFileExW(string p, int nivel, out Datos d, int busqueda, IntPtr f, int opc);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern bool FindNextFileW(IntPtr h, out Datos d);
    [DllImport("kernel32.dll")] static extern bool FindClose(IntPtr h);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool MoveFileExW(string a, string b, int f);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool GetDiskFreeSpaceExW(string p, out ulong libre, out ulong total, out ulong totalLibre);
    [DllImport("shell32.dll")] static extern int SHGetKnownFolderPath(ref Guid id, uint flags, IntPtr token, out IntPtr ruta);

    // Nombre y atributos de una ruta sin abrirla (no despierta archivos de la nube). null si no existe.
    static Datos? Info(string ruta)
    {
        Datos d; var h = FindFirstFileExW(ruta, 1, out d, 0, IntPtr.Zero, 0);
        if (h == new IntPtr(-1)) return null;
        FindClose(h);
        // FindFirstFile también acepta el nombre corto 8.3 ("SURFAC~1"): solo vale el nombre real (si no, se saltaría Fuera).
        return string.Equals(d.Nombre, Path.GetFileName(ruta), StringComparison.OrdinalIgnoreCase) ? d : (Datos?)null;
    }
    // Enlaces simbólicos y uniones (etiqueta "name surrogate"): apuntarían fuera de la carpeta. Los de OneDrive no lo son.
    static bool Enlace(Datos d) => (d.Atrib & FileAttributes.ReparsePoint) != 0 && (d.Etiqueta & 0x20000000) != 0;
    static bool Oculto(Datos d) => (d.Atrib & (FileAttributes.Hidden | FileAttributes.System)) == (FileAttributes.Hidden | FileAttributes.System);
    // Archivo solo en la nube (OneDrive "a petición"): no se muestra; abrirlo (o que la galería lo indexe) lo descargaría
    // entero (150 GB aquí). "Mantener siempre en este dispositivo" en Windows lo hace aparecer en Android.
    static bool EnNube(Datos d) => (d.Atrib & FileAttributes.Directory) == 0 && (((int)d.Atrib & 0x00400000) != 0 || (d.Atrib & FileAttributes.Offline) != 0);
    static bool Excluida(string ruta) => Fuera.Any(f => string.Equals(ruta, f, StringComparison.OrdinalIgnoreCase));

    static bool NombreValido(string n)
    {
        if (n.Length == 0 || n.Length > 255 || n == "." || n == ".." || n.EndsWith(".") || n.EndsWith(" ")) return false;
        foreach (var c in n) if (c < 32 || "<>:\"/\\|?*".IndexOf(c) >= 0) return false;
        var b = n.Split('.')[0].TrimEnd(' ').ToUpperInvariant();
        if (b == "CON" || b == "PRN" || b == "AUX" || b == "NUL" || b == "CONIN$" || b == "CONOUT$") return false;
        return !(b.Length == 4 && (b.StartsWith("COM") || b.StartsWith("LPT")) && "0123456789¹²³".IndexOf(b[3]) >= 0);
    }

    // Hijo de una carpeta compartida, validado. dir == null: raíz virtual (solo se llega a los recursos).
    static string Hijo(string dir, string nombre)
    {
        if (dir == null) { string r; if (Recursos.TryGetValue(nombre, out r)) return r; throw new Error9(ENOENT); }
        if (!NombreValido(nombre)) throw new Error9(EINVAL);
        var p = dir + "\\" + nombre;   // shortcut: rutas de más de 260 caracteres dan ENAMETOOLONG; pasar a "\\?\" si se necesitan
        if (Excluida(p)) throw new Error9(EACCES);
        return p;
    }

    static int Errno(Exception e)
    {
        if (e is Error9) return ((Error9)e).N;
        if (e is FileNotFoundException || e is DirectoryNotFoundException) return ENOENT;
        if (e is UnauthorizedAccessException) return EACCES;
        if (e is PathTooLongException) return ENAMETOOLONG;
        if (e is ArgumentException || e is NotSupportedException) return EINVAL;
        switch (Marshal.GetHRForException(e) & 0xFFFF)
        {
            case 2: case 3: return ENOENT;
            case 5: return EACCES;
            case 32: case 33: return EBUSY;
            case 17: return EXDEV;
            case 80: case 183: return EEXIST;
            case 112: return ENOSPC;
            case 123: return EINVAL;
            case 145: return ENOTEMPTY;
            case 206: return ENAMETOOLONG;
        }
        return EIO;
    }

    // ---------- Mensajes ----------
    class Lector
    {
        readonly byte[] b; int p; readonly int fin;
        public Lector(byte[] buf, int ini, int n) { b = buf; p = ini; fin = n; }
        void Hay(int n) { if (n < 0 || p + n > fin) throw new Error9(EPROTO); }
        public byte U8() { Hay(1); return b[p++]; }
        public ushort U16() { Hay(2); var v = BitConverter.ToUInt16(b, p); p += 2; return v; }
        public uint U32() { Hay(4); var v = BitConverter.ToUInt32(b, p); p += 4; return v; }
        public ulong U64() { Hay(8); var v = BitConverter.ToUInt64(b, p); p += 8; return v; }
        public string Txt() { int n = U16(); Hay(n); var s = Encoding.UTF8.GetString(b, p, n); p += n; return s; }
        public int Pos => p;
        public void Saltar(int n) { Hay(n); p += n; }
    }

    class Escritor
    {
        public byte[] B = new byte[256]; public int N = 7;
        void Sitio(int n) { if (N + n > B.Length) Array.Resize(ref B, Math.Max(B.Length * 2, N + n)); }
        public Escritor U8(int v) { Sitio(1); B[N++] = (byte)v; return this; }
        public Escritor U16(int v) { Sitio(2); B[N++] = (byte)v; B[N++] = (byte)(v >> 8); return this; }
        public Escritor U32(uint v) { Sitio(4); for (int i = 0; i < 4; i++) B[N++] = (byte)(v >> (8 * i)); return this; }
        public Escritor U64(ulong v) { Sitio(8); for (int i = 0; i < 8; i++) B[N++] = (byte)(v >> (8 * i)); return this; }
        public Escritor Txt(string s) { var t = Encoding.UTF8.GetBytes(s); U16(t.Length); Sitio(t.Length); Buffer.BlockCopy(t, 0, B, N, t.Length); N += t.Length; return this; }
        public Escritor Qid(bool dir, ulong ruta) => U8(dir ? 0x80 : 0).U32(0).U64(ruta);
        public void Reservar(int n) { Sitio(n); }
    }

    // Identidad estable del archivo para el kernel (qid.path): ruta sin distinguir mayúsculas, como NTFS.
    static ulong Id(string ruta)
    {
        if (ruta == null) return 1;
        ulong h = 14695981039346656037;
        foreach (var c in ruta.ToUpperInvariant()) { h ^= c; h *= 1099511628211; }
        return h | 2;
    }

    static ulong Unix(long ft, out ulong ns) { long t = ft - 116444736000000000; if (t < 0) t = 0; ns = (ulong)(t % 10000000) * 100; return (ulong)(t / 10000000); }

    // ---------- Sesión 9P ----------
    class Sesion
    {
        readonly Socket s; readonly ConcurrentDictionary<uint, Fid> fids = new ConcurrentDictionary<uint, Fid>();
        readonly ConcurrentDictionary<ushort, ManualResetEventSlim> enCurso = new ConcurrentDictionary<ushort, ManualResetEventSlim>();
        readonly SemaphoreSlim cupo = new SemaphoreSlim(Trabajadores);
        readonly object envio = new object();
        int msize = 8192; uint uid = GidMediaRw;
        public Sesion(Socket so) { s = so; }

        Fid F(uint n) { Fid f; if (!fids.TryGetValue(n, out f)) throw new Error9(EBADF); return f; }
        void Nuevo(uint n, Fid f)
        {
            if (fids.Count >= FidsMax) throw new Error9(EMFILE);
            if (!fids.TryAdd(n, f)) throw new Error9(EBADF);   // el fid ya existe: error del cliente
        }

        void Enviar(Escritor w, byte tipo, ushort tag)
        {
            BitConverter.GetBytes(w.N).CopyTo(w.B, 0); w.B[4] = tipo; w.B[5] = (byte)tag; w.B[6] = (byte)(tag >> 8);
            lock (envio) s.Send(w.B, 0, w.N, SocketFlags.None);
        }

        static void LeerTodo(Socket s, byte[] b, int n)
        {
            for (int p = 0; p < n;) { int r = s.Receive(b, p, n - p, SocketFlags.None); if (r <= 0) throw new EndOfStreamException(); p += r; }
        }

        public void Servir()
        {
            var cab = new byte[7];
            try
            {
                for (;;)
                {
                    LeerTodo(s, cab, 4);
                    int tam = BitConverter.ToInt32(cab, 0);
                    if (tam < 7 || tam > msize) throw new InvalidDataException("mensaje 9P de " + tam + " bytes");
                    var m = new byte[tam];
                    LeerTodo(s, m, tam - 4);   // m[0]=tipo, m[1..2]=tag, m[3..]=cuerpo
                    byte tipo = m[0]; ushort tag = (ushort)(m[1] | m[2] << 8);
                    if (tipo == 100) { Atender(tipo, tag, m); continue; }   // Tversion: en orden, nada en curso
                    cupo.Wait();   // contrapresión: como mucho 16 peticiones (y sus búferes) a la vez
                    var hecho = new ManualResetEventSlim();
                    enCurso[tag] = hecho;
                    ThreadPool.QueueUserWorkItem(_ =>
                    {
                        try { Atender(tipo, tag, m); }
                        finally { ManualResetEventSlim h; enCurso.TryRemove(tag, out h); hecho.Set(); cupo.Release(); }
                    });
                }
            }
            catch (Exception e) when (e is SocketException || e is EndOfStreamException || e is ObjectDisposedException) { }
            finally
            {
                foreach (var f in fids.Values) lock (f) f.F?.Dispose();
                fids.Clear();
            }
        }

        void Atender(byte tipo, ushort tag, byte[] m)
        {
            var w = new Escritor();
            try
            {
                var r = new Lector(m, 3, m.Length);
                Responder(tipo, tag, r, w, m);
                Enviar(w, (byte)(tipo + 1), tag);
            }
            catch (Exception e) when (!(e is SocketException || e is ObjectDisposedException))
            {
                var er = new Escritor().U32((uint)Errno(e));
                if (Depurar) Console.Error.WriteLine("T{0} -> errno {1}: {2}", tipo, Errno(e), e.Message);
                try { Enviar(er, 7, tag); } catch (SocketException) { } catch (ObjectDisposedException) { }
            }
        }

        void Getattr(Escritor w, string ruta)
        {
            Datos d;
            if (ruta == null) d = new Datos { Atrib = FileAttributes.Directory, Escrito = Arranque, Acceso = Arranque, Creado = Arranque };
            else { var i = Info(ruta); if (i == null) throw new Error9(ENOENT); d = i.Value; }
            bool dir = (d.Atrib & FileAttributes.Directory) != 0;
            ulong tam = dir ? 0 : ((ulong)d.TamAlto << 32 | d.TamBajo);
            uint modo = dir ? 0x4000u | 0x5F8 /*02770*/ : 0x8000u | ((d.Atrib & FileAttributes.ReadOnly) != 0 ? 0x120u /*0440*/ : 0x1B0u /*0660*/);
            w.U64(0x7FF).Qid(dir, Id(ruta)).U32(modo).U32(uid).U32(GidMediaRw).U64(dir ? 2u : 1u).U64(0).U64(tam).U64(65536).U64((tam + 511) / 512);
            ulong ns, s1 = Unix(d.Acceso, out ns); w.U64(s1).U64(ns);
            s1 = Unix(d.Escrito, out ns); w.U64(s1).U64(ns).U64(s1).U64(ns);   // ctime = mtime (Windows no tiene ctime)
            s1 = Unix(d.Creado, out ns); w.U64(s1).U64(ns).U64(0).U64(0);
        }

        // Lo que escribe Android lleva la marca de Internet (SmartScreen, Vista protegida), como una descarga.
        static void Marcar(Fid f)
        {
            if (f.Marcado) return;
            f.Marcado = true;
            try { using (var h = new FileStream(new Microsoft.Win32.SafeHandles.SafeFileHandle(CrearFlujo(f.Ruta + ":Zone.Identifier"), true), FileAccess.Write)) { var t = Encoding.ASCII.GetBytes("[ZoneTransfer]\r\nZoneId=3\r\n"); h.Write(t, 0, t.Length); } }
            catch (Exception) { }   // volúmenes sin flujos (FAT): sin marca
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr CreateFileW(string n, uint acc, uint comp, IntPtr seg, uint disp, uint flags, IntPtr plantilla);
        static IntPtr CrearFlujo(string n) { var h = CreateFileW(n, 0x40000000, 0, IntPtr.Zero, 2, 0, IntPtr.Zero); if (h == new IntPtr(-1)) throw new IOException(); return h; }

        void Responder(byte tipo, ushort tag, Lector r, Escritor w, byte[] m)
        {
            switch (tipo)
            {
                case 100: // Tversion
                {
                    int pedido = (int)Math.Min(r.U32(), MsizeMax); var v = r.Txt();
                    foreach (var f in fids.Values) lock (f) f.F?.Dispose();
                    fids.Clear();
                    if (pedido < 4096) throw new Error9(EINVAL);
                    msize = pedido;
                    w.U32((uint)msize).Txt(v == "9P2000.L" ? v : "unknown");
                    return;
                }
                case 104: // Tattach fid afid uname aname n_uname
                {
                    uint fid = r.U32(); r.U32(); r.Txt(); var aname = r.Txt();
                    uint u; if (uint.TryParse(aname, out u) && u >= 1000) uid = u;   // dueño que espera MediaProvider (lo manda sdcarpetas)
                    Nuevo(fid, new Fid { Dir = true });
                    w.Qid(true, Id(null));
                    return;
                }
                case 110: // Twalk fid newfid nwname wname*
                {
                    var f = F(r.U32()); uint nuevo = r.U32(); int n = r.U16();
                    if (n > 16) throw new Error9(EINVAL);
                    string ruta = f.Ruta; bool dir = f.Dir; var qids = new List<KeyValuePair<bool, string>>();
                    for (int i = 0; i < n; i++)
                    {
                        var nombre = r.Txt();
                        try
                        {
                            if (!dir) throw new Error9(ENOTDIR);
                            var p = Hijo(ruta, nombre); var d = Info(p);
                            if (d == null || Enlace(d.Value) || Oculto(d.Value) || EnNube(d.Value)) throw new Error9(ENOENT);
                            ruta = p; dir = (d.Value.Atrib & FileAttributes.Directory) != 0;
                            qids.Add(new KeyValuePair<bool, string>(dir, ruta));
                        }
                        catch (Error9) when (i > 0) { break; }   // camino parcial: el cliente lo interpreta como ENOENT
                    }
                    if (qids.Count == n) Clonar(f, nuevo, new Fid { Ruta = ruta, Dir = dir });
                    w.U16(qids.Count); foreach (var q in qids) w.Qid(q.Key, Id(q.Value));
                    return;
                }
                case 24: // Tgetattr fid mask
                { var f = F(r.U32()); Getattr(w, f.Ruta); return; }
                case 26: // Tsetattr fid valid mode uid gid size atime mtime
                {
                    var f = F(r.U32()); uint v = r.U32(); r.U32(); r.U32(); r.U32(); ulong tam = r.U64();
                    ulong aS = r.U64(), aN = r.U64(), mS = r.U64(), mN = r.U64();
                    if (f.Ruta == null) throw new Error9(EPERM);
                    if ((v & 0x8) != 0)   // tamaño
                    {
                        if (f.Dir) throw new Error9(EISDIR);
                        if (tam > long.MaxValue) throw new Error9(EINVAL);
                        lock (f)
                            if (f.F != null && f.F.CanWrite) { f.F.SetLength((long)tam); Marcar(f); }
                            else using (var fs = new FileStream(f.Ruta, FileMode.Open, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete)) { fs.SetLength((long)tam); Marcar(f); }
                    }
                    if ((v & 0x20) != 0)   // mtime (MTIME_SET 0x100: el valor dado; si no, ahora)
                    {
                        var t = (v & 0x100) != 0 ? new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc).AddTicks((long)Math.Min(mS, 253402300799UL - 62135596800UL) * 10000000 + (long)(mN % 1000000000) / 100) : DateTime.UtcNow;
                        if (f.Dir) Directory.SetLastWriteTimeUtc(f.Ruta, t); else File.SetLastWriteTimeUtc(f.Ruta, t);
                    }
                    return;   // modo, dueño y grupo: Windows no los tiene; se aceptan sin efecto (MediaProvider los fija)
                }
                case 12: // Tlopen fid flags
                {
                    var f = F(r.U32()); uint fl = r.U32();
                    if (f.Dir || f.Xattr) { w.Qid(f.Dir, Id(f.Ruta)).U32(0); return; }
                    var d = Info(f.Ruta); if (d == null) throw new Error9(ENOENT);
                    if (EnNube(d.Value)) throw new Error9(EACCES);
                    var acc = (fl & 3) == 0 ? FileAccess.Read : (fl & 3) == 1 ? FileAccess.Write : FileAccess.ReadWrite;
                    var modo = (fl & 0x200) != 0 && acc != FileAccess.Read ? FileMode.Truncate : FileMode.Open;
                    lock (f) { f.F?.Dispose(); f.F = new FileStream(f.Ruta, modo, acc, FileShare.ReadWrite | FileShare.Delete, 1); if (modo == FileMode.Truncate) Marcar(f); }
                    w.Qid(false, Id(f.Ruta)).U32(0);
                    return;
                }
                case 14: // Tlcreate fid name flags mode gid
                {
                    var f = F(r.U32()); var nombre = r.Txt(); uint fl = r.U32();
                    if (!f.Dir || f.Ruta == null) throw new Error9(f.Ruta == null ? EACCES : ENOTDIR);
                    var p = Hijo(f.Ruta, nombre);
                    var modo = (fl & 0x80) != 0 ? FileMode.CreateNew : (fl & 0x200) != 0 ? FileMode.Create : FileMode.OpenOrCreate;
                    Propio(p);   // antes de tocar el disco: el aviso de Windows llega enseguida y no debe volver a Android
                    lock (f)
                    {
                        f.F = new FileStream(p, modo, FileAccess.ReadWrite, FileShare.ReadWrite | FileShare.Delete, 1);
                        f.Ruta = p; f.Dir = false; f.Lista = null; Marcar(f);
                    }
                    w.Qid(false, Id(p)).U32(0);
                    return;
                }
                case 116: // Tread fid offset count
                {
                    var f = F(r.U32()); ulong off = r.U64(); int n = (int)Math.Min(r.U32(), (uint)(msize - 11));
                    if (f.Xattr) { w.U32(0); return; }
                    if (f.F == null) throw new Error9(EBADF);
                    if (off > long.MaxValue) throw new Error9(EINVAL);
                    int pos = w.N; w.U32(0); w.Reservar(n); int leidos;
                    lock (f) { if (f.F == null) throw new Error9(EBADF); f.F.Position = (long)off; leidos = 0; int k; while (leidos < n && (k = f.F.Read(w.B, w.N + leidos, n - leidos)) > 0) leidos += k; }
                    BitConverter.GetBytes(leidos).CopyTo(w.B, pos); w.N += leidos;
                    return;
                }
                case 118: // Twrite fid offset count data
                {
                    var f = F(r.U32()); ulong off = r.U64(); int n = (int)r.U32(); int ini = r.Pos; r.Saltar(n);
                    if (f.F == null || !f.F.CanWrite) throw new Error9(EBADF);
                    if (off > long.MaxValue) throw new Error9(EINVAL);
                    Propio(f.Ruta);
                    lock (f) { if (f.F == null) throw new Error9(EBADF); Marcar(f); f.F.Position = (long)off; f.F.Write(m, ini, n); }
                    w.U32((uint)n);
                    return;
                }
                case 120: // Tclunk
                { Fid f; if (!fids.TryRemove(r.U32(), out f)) throw new Error9(EBADF); lock (f) { f.F?.Dispose(); f.F = null; } return; }
                case 40: // Treaddir fid offset count
                {
                    var f = F(r.U32()); ulong off = r.U64(); int n = (int)Math.Min(r.U32(), (uint)(msize - 11));
                    if (!f.Dir) throw new Error9(ENOTDIR);
                    List<string> l;
                    lock (f) { if (off == 0 || f.Lista == null) f.Lista = Listar(f.Ruta); l = f.Lista; }
                    int pos = w.N; w.U32(0); int ini = w.N;
                    for (ulong i = off; i < (ulong)l.Count; i++)
                    {
                        var nombre = l[(int)i]; var p = f.Ruta == null ? Recursos[nombre] : f.Ruta + "\\" + nombre;
                        bool esDir = f.Ruta == null || nombre.EndsWith("\\");
                        if (esDir && f.Ruta != null) { nombre = nombre.TrimEnd('\\'); p = p.TrimEnd('\\'); }
                        int tam = 13 + 8 + 1 + 2 + Encoding.UTF8.GetByteCount(nombre);
                        if (w.N - ini + tam > n) break;
                        w.Qid(esDir, Id(p)).U64(i + 1).U8(esDir ? 4 : 8).Txt(nombre);
                    }
                    BitConverter.GetBytes(w.N - ini).CopyTo(w.B, pos);
                    return;
                }
                case 72: // Tmkdir dfid name mode gid
                {
                    var f = F(r.U32()); var nombre = r.Txt();
                    if (f.Ruta == null) throw new Error9(EACCES);
                    var p = Hijo(f.Ruta, nombre);
                    if (Info(p) != null) throw new Error9(EEXIST);
                    Propio(p); Directory.CreateDirectory(p);
                    w.Qid(true, Id(p));
                    return;
                }
                case 76: // Tunlinkat dfid name flags
                {
                    var f = F(r.U32()); var nombre = r.Txt(); uint fl = r.U32();
                    if (f.Ruta == null) throw new Error9(EACCES);
                    var p = Hijo(f.Ruta, nombre); var d = Info(p);
                    if (d == null || Enlace(d.Value) || Oculto(d.Value)) throw new Error9(ENOENT);
                    bool dir = (d.Value.Atrib & FileAttributes.Directory) != 0;
                    Propio(p);
                    if ((fl & 0x200) != 0) { if (!dir) throw new Error9(ENOTDIR); Directory.Delete(p, false); }
                    else { if (dir) throw new Error9(EISDIR); File.Delete(p); }
                    return;
                }
                case 74: // Trenameat olddirfid oldname newdirfid newname
                {
                    var a = F(r.U32()); var na = r.Txt(); var b = F(r.U32()); var nb = r.Txt();
                    if (a.Ruta == null || b.Ruta == null) throw new Error9(EACCES);
                    string pa = Hijo(a.Ruta, na), pb = Hijo(b.Ruta, nb); var d = Info(pa);
                    if (d == null || Enlace(d.Value) || Oculto(d.Value)) throw new Error9(ENOENT);
                    if (string.Equals(pa, pb, StringComparison.Ordinal)) return;
                    bool dir = (d.Value.Atrib & FileAttributes.Directory) != 0;
                    if (dir && pb.StartsWith(pa + "\\", StringComparison.OrdinalIgnoreCase)) throw new Error9(EINVAL);
                    // Archivos: reemplaza como rename(2) y copia entre discos. Carpetas entre discos: EXDEV (el cliente copia).
                    Propio(pa); Propio(pb);
                    if (!MoveFileExW(pa, pb, dir ? 0 : 3)) throw new Error9(Errno(new IOException("", Marshal.GetHRForLastWin32Error())));
                    foreach (var x in fids.Values)   // los fids abiertos siguen al archivo (como QEMU v9fs_fix_fid_paths)
                        lock (x)
                            if (x.Ruta != null && (string.Equals(x.Ruta, pa, StringComparison.OrdinalIgnoreCase) || x.Ruta.StartsWith(pa + "\\", StringComparison.OrdinalIgnoreCase)))
                                x.Ruta = pb + x.Ruta.Substring(pa.Length);
                    return;
                }
                case 8: // Tstatfs fid
                {
                    var f = F(r.U32()); ulong libre, total, tl;
                    if (!GetDiskFreeSpaceExW(f.Ruta ?? Recursos.Values.First(), out libre, out total, out tl)) throw new Error9(EIO);
                    w.U32(0x01021997).U32(4096).U64(total / 4096).U64(tl / 4096).U64(libre / 4096).U64(1 << 24).U64(1 << 23).U64(0).U32(255);
                    return;
                }
                case 50: // Tfsync fid datasync
                { var f = F(r.U32()); lock (f) f.F?.Flush(true); return; }
                case 52: // Tlock: bloqueos POSIX solo locales (Android no comparte archivos con Windows a la vez)
                { F(r.U32()); w.U8(0); return; }
                case 54: // Tgetlock fid type start length proc_id client_id
                {
                    F(r.U32()); r.U8(); ulong ini = r.U64(), lon = r.U64(); uint pid = r.U32(); var cli = r.Txt();
                    w.U8(2).U64(ini).U64(lon).U32(pid).Txt(cli);   // F_UNLCK
                    return;
                }
                case 30: // Txattrwalk fid newfid name: sin atributos extendidos (la etiqueta SELinux va en el montaje)
                {
                    var f = F(r.U32()); uint nuevo = r.U32(); var nombre = r.Txt();
                    if (nombre.Length != 0) throw new Error9(ENODATA);
                    Nuevo(nuevo, new Fid { Ruta = f.Ruta, Xattr = true });
                    w.U64(0);
                    return;
                }
                case 108: // Tflush oldtag: se espera a que la petición termine (su respuesta va antes que Rflush)
                {
                    ManualResetEventSlim h; ushort viejo = r.U16();
                    if (viejo != tag && enCurso.TryGetValue(viejo, out h)) h.Wait();
                    return;
                }
                case 32: case 16: case 18: case 70: case 22: case 102: case 122:
                    // xattrcreate, symlink, mknod, link, readlink, auth, remove: no se permiten
                    throw new Error9(tipo == 32 || tipo == 102 ? EOPNOTSUPP : EPERM);
                default:
                    throw new Error9(EOPNOTSUPP);
            }
        }

        // Twalk: clonado/creación del nuevo fid (nwname 0 = mismo archivo).
        void Clonar(Fid viejo, uint nuevo, Fid nf)
        {
            Fid actual;
            if (fids.TryGetValue(nuevo, out actual) && ReferenceEquals(actual, viejo)) { lock (viejo) { viejo.Ruta = nf.Ruta; viejo.Dir = nf.Dir; viejo.Lista = null; } return; }
            Nuevo(nuevo, nf);
        }
    }

    static long Arranque = DateTime.UtcNow.ToFileTimeUtc();
    static readonly bool Depurar = Environment.GetEnvironmentVariable("SURFACEDUAL_DEPURAR") == "1";   // errores 9P a stderr

    // Contenido de una carpeta: los directorios llevan "\" al final. Sin ocultos de sistema, enlaces ni rutas excluidas.
    static List<string> Listar(string dir)
    {
        if (dir == null) return Recursos.Keys.ToList();
        var l = new List<string>(); Datos d;
        var h = FindFirstFileExW(dir + "\\*", 1, out d, 0, IntPtr.Zero, 2 /*LARGE_FETCH*/);
        if (h == new IntPtr(-1)) throw new Error9(Marshal.GetLastWin32Error() == 2 ? ENOENT : EACCES);
        try
        {
            do
            {
                if (d.Nombre == "." || d.Nombre == ".." || Enlace(d) || Oculto(d) || EnNube(d) || Excluida(dir + "\\" + d.Nombre)) continue;
                l.Add((d.Atrib & FileAttributes.Directory) != 0 ? d.Nombre + "\\" : d.Nombre);
            } while (FindNextFileW(h, out d));
        }
        finally { FindClose(h); }
        return l;
    }

    // ---------- Avisos de cambios hechos en Windows (para que MediaStore los indexe) ----------
    static readonly ConcurrentDictionary<string, DateTime> propios = new ConcurrentDictionary<string, DateTime>(StringComparer.OrdinalIgnoreCase);
    static readonly HashSet<string> pendientes = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    static Timer espera; static volatile Socket avisos;
    static void Propio(string p) { var d = Path.GetDirectoryName(p); if (d != null) propios[d] = DateTime.UtcNow; }

    static void Vigilar()
    {
        espera = new Timer(_ => Avisar());
        foreach (var kv in Recursos)
        {
            var nombre = kv.Key; var raiz = kv.Value;
            var v = new FileSystemWatcher(raiz) { IncludeSubdirectories = true, InternalBufferSize = 65536,
                NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite | NotifyFilters.Size | NotifyFilters.Attributes };   // Attributes: OneDrive deja un archivo en local
            FileSystemEventHandler h = (o, e) => Cambio(nombre, raiz, e.FullPath);
            v.Created += h; v.Changed += h; v.Deleted += h;
            v.Renamed += (o, e) => { Cambio(nombre, raiz, e.OldFullPath); Cambio(nombre, raiz, e.FullPath); };
            v.Error += (o, e) => { lock (pendientes) pendientes.Add(nombre); espera.Change(2000, Timeout.Infinite); };   // desbordado: todo el recurso
            v.EnableRaisingEvents = true;
        }
    }

    static void Cambio(string nombre, string raiz, string ruta)
    {
        var dir = Path.GetDirectoryName(ruta);
        if (dir == null || dir.Length < raiz.Length) return;
        DateTime t; if (propios.TryGetValue(dir, out t) && DateTime.UtcNow - t < TimeSpan.FromSeconds(5)) return;   // lo hizo Android
        foreach (var f in Fuera) if (ruta.StartsWith(f + "\\", StringComparison.OrdinalIgnoreCase) || string.Equals(ruta, f, StringComparison.OrdinalIgnoreCase)) return;
        lock (pendientes) pendientes.Add(nombre + dir.Substring(raiz.Length).Replace('\\', '/'));
        espera.Change(2000, Timeout.Infinite);   // 2 s sin cambios: un solo aviso por tanda
    }

    static void Avisar()
    {
        string[] l; lock (pendientes) { l = pendientes.ToArray(); pendientes.Clear(); }
        foreach (var k in propios.Keys.ToArray()) { DateTime t; if (propios.TryGetValue(k, out t) && DateTime.UtcNow - t > TimeSpan.FromSeconds(10)) propios.TryRemove(k, out t); }
        // Solo las carpetas más altas (escanear una incluye sus subcarpetas); demasiadas: el recurso entero.
        var sel = l.Where(a => !l.Any(b => b != a && a.StartsWith(b + "/", StringComparison.OrdinalIgnoreCase))).ToList();
        if (sel.Count > 32) sel = sel.Select(a => a.Split('/')[0]).Distinct().ToList();
        var s = avisos; if (s == null || sel.Count == 0) return;
        try { var b = Encoding.UTF8.GetBytes(string.Join("\n", sel) + "\n"); lock (s) s.Send(b); } catch (Exception) { }
    }

    // ---------- Arranque ----------
    static void Configurar()
    {
        for (int i = 0; i < Mapa.GetLength(0); i++)
        {
            var g = new Guid(Mapa[i, 1]); IntPtr p;
            if (SHGetKnownFolderPath(ref g, 0x8000 /*KF_FLAG_CREATE*/, IntPtr.Zero, out p) != 0) continue;
            var ruta = Marshal.PtrToStringUni(p).TrimEnd('\\'); Marshal.FreeCoTaskMem(p);
            if (Directory.Exists(ruta) && !Recursos.Values.Contains(ruta, StringComparer.OrdinalIgnoreCase)) Recursos[Mapa[i, 0]] = ruta;
        }
        string img;   // Álbum de cámara sin carpeta conocida registrada (medido 17/11: 0x80070002): su sitio por omisión
        if (!Recursos.ContainsKey("DCIM") && Recursos.TryGetValue("Pictures", out img) && Directory.Exists(img + "\\Camera Roll")) Recursos["DCIM"] = img + "\\Camera Roll";
        // Un recurso dentro de otro (Álbum de cámara dentro de Imágenes) solo se ve una vez: si no, la galería lo duplica.
        foreach (var a in Recursos.Values) foreach (var b in Recursos.Values) if (a.StartsWith(b + "\\", StringComparison.OrdinalIgnoreCase)) Fuera.Add(a);
        // El proyecto (VisorVM.exe, scripts, discos de la VM) vive en Descargas: Android no debe poder tocarlo.
        var exe = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
        Fuera.Add(Path.GetFullPath(Path.Combine(exe, @"..\..\..")).TrimEnd('\\'));
        Fuera.Add(exe);
    }

    static void Autocomprobar()
    {
        Func<string, bool> v = NombreValido;
        if (!v("foto 1.jpg") || !v("Ñandú ✓.mkv") || v("..") || v(".") || v("a/b") || v("a\\b") || v("x:y") || v("CON") || v("com1.txt") ||
            v("nul") || v("fin.") || v("fin ") || v("") || v(new string('a', 256)) || !v("console.log") || !v("COM10"))
            throw new Exception("Autocomprobación de nombres fallida");
        Recursos["Movies"] = @"C:\x\Videos"; Fuera.Add(@"C:\x\Videos\SurfaceDual");
        try { Hijo(@"C:\x\Videos", "SurfaceDual"); throw new Exception("Autocomprobación: ruta excluida accesible"); } catch (Error9 e) when (e.N == EACCES) { }
        try { Hijo(null, "Windows"); throw new Exception("Autocomprobación: recurso inexistente"); } catch (Error9 e) when (e.N == ENOENT) { }
        if (Hijo(null, "Movies") != @"C:\x\Videos" || Hijo(@"C:\x\Videos", "a.mkv") != @"C:\x\Videos\a.mkv") throw new Exception("Autocomprobación de rutas fallida");
        if (Id(@"C:\A\b") != Id(@"c:\a\B") || Id(@"C:\a") == Id(@"C:\b")) throw new Exception("Autocomprobación de qid fallida");
        Recursos.Clear(); Fuera.Clear();
    }

    static int Main(string[] args)
    {
        Autocomprobar();
        Guid vm; int tcp = 0;
        // --tcp N: prueba contra WSL (su localhost reenviado) en lugar de hv_sock; mismo protocolo.
        if (args.Length == 2 && args[0] == "--tcp" && int.TryParse(args[1], out tcp)) vm = Guid.Empty;
        else if (args.Length != 1 || !Guid.TryParse(args[0], out vm)) { Console.Error.WriteLine("Uso: HvCarpetas.exe <VmId>"); return 1; }
        Configurar();
        foreach (var kv in Recursos) Console.WriteLine("{0} -> {1}", kv.Key, kv.Value);
        foreach (var f in Fuera) Console.WriteLine("fuera: " + f);
        Vigilar();
        var servicio = new Guid(Puerto.ToString("x8") + "-facb-11e6-bd58-64006a7986d3");
        // Dos conexiones a sdcarpetas: "P" lleva el 9P (la monta el kernel), "N" los avisos de cambios. Mientras están
        // abiertas no hay nada que hacer (0 % de CPU, también con la VM en pausa); sin Android, un intento cada 2 s.
        for (;;)
        {
            try
            {
                using (Socket n = Conectar(vm, servicio, 'N', tcp), p = Conectar(vm, servicio, 'P', tcp))   // P la última: su montaje espera al servidor
                {
                    Console.WriteLine("Android conectado");
                    avisos = n;
                    var t = DateTime.UtcNow;
                    new Sesion(p).Servir();
                    avisos = null;
                    // Android cerró enseguida (no pudo montar): no insistir cada 2 s.
                    if (DateTime.UtcNow - t < TimeSpan.FromSeconds(5)) Thread.Sleep(60000);
                }
                Console.WriteLine("Android desconectado");
            }
            // La VM ya no existe (rearmada con otro VmId o apagada): se sale; iniciar.ps1 abre otro con el VmId nuevo.
            catch (SocketException e) when (e.SocketErrorCode == SocketError.AddressNotAvailable && tcp == 0) { Console.WriteLine("VM no disponible"); return 0; }
            // Android sin escuchar todavía: el connect espera 30 s sin gastar CPU y se reintenta.
            catch (SocketException e) when (e.SocketErrorCode == SocketError.ConnectionRefused || e.SocketErrorCode == SocketError.TimedOut ||
                                              e.SocketErrorCode == SocketError.ConnectionReset || e.SocketErrorCode == SocketError.HostUnreachable) { }
            catch (Exception e) { Console.Error.WriteLine("Carpetas: " + e.GetBaseException().Message); }
            avisos = null;
            VmActiva.Reintentar(2000);   // con la VM en pausa, ningún intento hasta reanudar
        }
    }

    static Socket Conectar(Guid vm, Guid servicio, char tipo, int tcp)
    {
        var s = tcp != 0 ? new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp) : new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1);
        try
        {
            if (tcp != 0) { s.NoDelay = true; s.Connect(System.Net.IPAddress.Loopback, tcp); } else s.Connect(new HvEndPoint(vm, servicio));
            s.Send(new[] { (byte)tipo }); return s;
        }
        catch { s.Dispose(); throw; }
    }
}
