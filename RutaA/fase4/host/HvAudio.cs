// Uso: HvAudio.exe <VmId>. Compilar con HvPuente.cs y -main:HvAudio -target:exe. Altavoz (vsock 5003) y micro (5004).
using System;
using System.IO;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Threading;

static class HvAudio
{
    const int Bytes = 48000 * 2 * 2 / 100, Count = 4;   // bloques de 10 ms; Count: búferes del micro (waveIn)
    [StructLayout(LayoutKind.Sequential, Pack = 2)]
    struct Format
    {
        public ushort Tag, Channels;
        public uint Rate, BytesPerSecond;
        public ushort BlockAlign, Bits, Extra;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct Header
    {
        public IntPtr Data;
        public uint Length, Recorded;
        public UIntPtr User;
        public uint Flags, Loops;
        public IntPtr Next;
        public UIntPtr Reserved;
    }
    [DllImport("winmm.dll")] static extern uint waveInOpen(out IntPtr device, uint id, ref Format format, IntPtr callback, IntPtr instance, uint flags);
    [DllImport("winmm.dll")] static extern uint waveInPrepareHeader(IntPtr device, IntPtr header, uint size);
    [DllImport("winmm.dll")] static extern uint waveInAddBuffer(IntPtr device, IntPtr header, uint size);
    [DllImport("winmm.dll")] static extern uint waveInStart(IntPtr device);
    [DllImport("winmm.dll")] static extern uint waveInReset(IntPtr device);
    [DllImport("winmm.dll")] static extern uint waveInUnprepareHeader(IntPtr device, IntPtr header, uint size);
    [DllImport("winmm.dll")] static extern uint waveInClose(IntPtr device);

    static void Check(uint result)
    {
        if (result != 0) throw new IOException("winmm: " + result);
    }
    static void Read(Socket socket, byte[] bytes)
    {
        int offset = 0;
        while (offset < bytes.Length)
        {
            int n = socket.Receive(bytes, offset, bytes.Length - offset, SocketFlags.None);
            if (n == 0) throw new EndOfStreamException("Audio desconectado");
            offset += n;
        }
    }
    static void Validate(byte[] header)
    {
        if (header.Length != 16 || BitConverter.ToUInt32(header, 0) != 0x55414453 ||
            BitConverter.ToUInt32(header, 4) != 48000 || BitConverter.ToUInt32(header, 8) != 2 ||
            BitConverter.ToUInt32(header, 12) != 16) throw new InvalidDataException("Cabecera SDAU incompatible");
    }
    static int pico, bloques, huecos, descartados, espera, limpios;   // diagnóstico por segundo
    static int meta = 2 * Bytes;   // colchón buscado en el socket (20-80 ms): sube 10 ms con cada hueco y baja tras 60 s limpios
    static double paso = 1;        // cuadros de Android gastados por cuadro que suena (1 = sin retocar)

    static int M(byte[] b, int o) { return (short)(b[o] | (b[o + 1] << 8)); }
    // Mayor nivel de n bytes S16. Sin Math.Abs(short): con una muestra a fondo de escala (-32768) lanza OverflowException y
    // tumbaba el proceso entero (visor de sucesos 17/11: 05:50, 13:56, 19:08 y 19:27): Android mudo hasta reiniciarlo.
    static int Pico(byte[] b, int n, int pico)
    {
        for (int k = 0; k + 1 < n; k += 2) { int v = M(b, k); if (v < 0) v = -v; if (v > pico) pico = v; }
        return pico;
    }
    // Interpolación cúbica (Catmull-Rom) entre b y c; con t = 0 devuelve b exacto.
    static short Cubica(int a, int b, int c, int d, double t)
    {
        double v = b + 0.5 * t * (c - a + t * (2.0 * a - 5.0 * b + 4.0 * c - d + t * (3.0 * (b - c) + d - a)));
        return (short)(v > 32767 ? 32767 : v < -32768 ? -32768 : Math.Floor(v + 0.5));
    }
    // Cuadros que Remuestrear lee del fifo (el cuadro 0 es historia).
    static int Necesarios(double paso, double fase) { return (int)(fase + (Bytes / 4 - 1) * paso) + 4; }
    // Saca un bloque (480 cuadros S16 estéreo) del fifo avanzando `paso` cuadros de entrada por cuadro de salida desde la
    // posición fraccionaria `fase` (entre los cuadros 1 y 2). Devuelve los cuadros gastados. Con paso 1 y fase 0 copia tal cual.
    static int Remuestrear(byte[] fifo, byte[] salida, double paso, ref double fase)
    {
        if (paso == 1 && fase == 0) { Buffer.BlockCopy(fifo, 4, salida, 0, Bytes); return Bytes / 4; }
        for (int j = 0; j < Bytes / 4; j++)
        {
            double x = fase + j * paso;
            int k = (int)x;
            for (int c = 0; c < 4; c += 2)
            {
                int o = k * 4 + c;
                short v = Cubica(M(fifo, o), M(fifo, o + 4), M(fifo, o + 8), M(fifo, o + 12), x - k);
                salida[j * 4 + c] = (byte)v; salida[j * 4 + c + 1] = (byte)(v >> 8);
            }
        }
        double fin = fase + Bytes / 4 * paso;
        int gastados = (int)(fin + 1e-9);
        fase = fin - gastados < 1e-9 ? 0 : fin - gastados;
        return gastados;
    }
    // Colchón (bytes de sobra en el socket en el peor momento del último medio segundo) -> paso: proporcional a lo que sobra
    // o falta (0,02 % por ms, hasta un 1 %: no se oye), en vez de tirar o repetir. Medido 17/11 con WASAPI: en marcha el
    // paso se queda en 1,0000 (copia exacta); tras un silencio el altavoz de Windows tarda ~100 ms en despertar y ese
    // retraso se recupera así en unos 9 s.
    static double Paso(int colchon, int meta)
    {
        double ms = (colchon - meta) / 192.0;
        return Math.Abs(ms) <= 2 ? 1 : 1 + Math.Max(-0.01, Math.Min(0.01, ms * 0.0002));
    }

    // WASAPI compartido, por eventos. Antes waveOut con 4 búferes de 10 ms: su capa interna se quedaba sin datos sin avisar
    // (medido 17/11 con la VM mandando limpio al 100,0 % del tiempo real: Windows gastaba ~1,2 % menos, 38 cortes por minuto
    // en lo que sonaba y 760 ms tirados por minuto) y tardaba 60-200 ms en arrancar tras cada silencio.
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator { }
    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDeviceEnumerator
    {
        int EnumAudioEndpoints();
        [PreserveSig] int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice device);
    }
    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDevice
    {
        [PreserveSig] int Activate(ref Guid iid, int context, IntPtr parameters, [MarshalAs(UnmanagedType.IUnknown)] out object o);
        int OpenPropertyStore();
        [PreserveSig] int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
    }
    [ComImport, Guid("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioClient
    {
        [PreserveSig] int Initialize(int mode, uint flags, long duration, long period, ref Format format, IntPtr session);
        [PreserveSig] int GetBufferSize(out uint frames);
        int GetStreamLatency();
        [PreserveSig] int GetCurrentPadding(out uint frames);
        int IsFormatSupported();
        int GetMixFormat();
        int GetDevicePeriod();
        [PreserveSig] int Start();
        [PreserveSig] int Stop();
        int Reset();
        [PreserveSig] int SetEventHandle(IntPtr handle);
        [PreserveSig] int GetService(ref Guid iid, [MarshalAs(UnmanagedType.IUnknown)] out object o);
    }
    [ComImport, Guid("F294ACFC-3146-4483-A7BF-ADDCA7C260E2"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioRenderClient
    {
        [PreserveSig] int GetBuffer(uint frames, out IntPtr data);
        [PreserveSig] int ReleaseBuffer(uint frames, uint flags);
    }
    [DllImport("avrt.dll", CharSet = CharSet.Unicode)] static extern IntPtr AvSetMmThreadCharacteristics(string task, ref uint index);
    [DllImport("avrt.dll")] static extern bool AvRevertMmThreadCharacteristics(IntPtr handle);
    static void Hr(int hr) { if (hr < 0) throw new IOException("WASAPI: 0x" + hr.ToString("x8")); }
    // El altavoz predeterminado de Windows (el mismo que mueve el volumen del visor) y su identificador.
    static IMMDevice Altavoz(IMMDeviceEnumerator todos, out string id)
    {
        IMMDevice d;
        Hr(todos.GetDefaultAudioEndpoint(0, 0, out d));
        Hr(d.GetId(out id));
        return d;
    }

    const int Cola = 3 * Bytes / 4;   // cuadros que se mantienen en la cola de Windows: 30 ms (3 periodos del motor de audio)
    // Android solo manda sonido mientras reproduce. Sin datos 500 ms, el flujo de WASAPI se para (el motor de audio de
    // Windows descansa y este hilo espera sin despertares); no se cierra para no perder el principio de cada sonido.
    // Sin cortes: 30 ms en la cola de Windows y un colchón (`meta`) en el socket. La VM manda a ráfagas y su reloj no es el
    // de Windows: lo que sobra o falta se corrige remuestreando un pelo (Paso); con sonido en marcha nunca se tira nada,
    // salvo más de 400 ms de sobra (Windows atascado). Al arrancar tras un silencio sí se tira lo viejo (no se oye).
    // Cualquier fallo (altavoz retirado o cambiado, motor atascado 1 s) lanza y Serve reconecta y reabre todo.
    static void Play(Socket socket)
    {
        var wire = new byte[16];
        Read(socket, wire);
        Validate(wire);
        var format = new Format { Tag = 1, Channels = 2, Rate = 48000, BytesPerSecond = 192000, BlockAlign = 4, Bits = 16 };
        var todos = (IMMDeviceEnumerator)new MMDeviceEnumerator();
        string id, ahora;
        object o;
        IMMDevice altavoz = Altavoz(todos, out id);
        Guid iid = typeof(IAudioClient).GUID;
        Hr(altavoz.Activate(ref iid, 23 /* CLSCTX_ALL */, IntPtr.Zero, out o));
        var client = (IAudioClient)o;
        IAudioRenderClient render = null;
        uint mm = 0;
        IntPtr prioridad = AvSetMmThreadCharacteristics("Pro Audio", ref mm);   // que otro proceso no retrase este hilo
        var evento = new AutoResetEvent(false);
        try
        {
            {
                // Compartido + eventos; Windows convierte 48000/2/S16 al formato del altavoz (AUTOCONVERTPCM | SRC_DEFAULT_QUALITY).
                Hr(client.Initialize(0, 0x80000000 | 0x08000000 | 0x00040000, 600000 /* 60 ms */, 0, ref format, IntPtr.Zero));
                uint capacidad;
                Hr(client.GetBufferSize(out capacidad));
                if (capacidad < Cola + Bytes / 4) throw new IOException("WASAPI: búfer de " + capacidad + " cuadros");
                Hr(client.SetEventHandle(evento.SafeWaitHandle.DangerousGetHandle()));
                iid = typeof(IAudioRenderClient).GUID;
                Hr(client.GetService(ref iid, out o));
                render = (IAudioRenderClient)o;
                var bytes = new byte[Bytes];
                var old = new byte[16 * Bytes];
                var fifo = new byte[2 * Bytes];   // lo justo para un bloque; el colchón vive en el socket
                int have = 4, least = int.MaxValue, n = 0;   // un cuadro de silencio como historia
                double fase = 0;
                bool parado = true, sonando = false;
                for (;;)
                {
                    uint cola;
                    if (parado)
                    {
                        if (sonando) { Hr(client.Stop()); sonando = false; }
                        socket.Poll(-1, SelectMode.SelectRead);
                        // El usuario cambió de altavoz (auriculares, Bluetooth) durante el silencio: reabrir en el nuevo.
                        Marshal.ReleaseComObject(Altavoz(todos, out ahora));
                        if (ahora != id) throw new IOException("cambió el altavoz de Windows");
                        // Arranque: juntar colchón (como mucho 60 ms de espera) y tirar lo viejo que sobre.
                        for (int t0 = Environment.TickCount; socket.Available > 0 && socket.Available < meta + Cola * 4 && Environment.TickCount - t0 < 60; ) Thread.Sleep(5);
                        Tirar(socket, old, socket.Available - meta - Cola * 4 - Bytes);
                        // Del sonido anterior solo se guarda el trozo de cuadro a medias (alineación); la historia, a silencio.
                        Buffer.BlockCopy(fifo, have - have % 4, fifo, 4, have % 4);
                        Array.Clear(fifo, 0, 4);
                        have = 4 + have % 4; fase = 0;
                        parado = false; paso = 1; least = int.MaxValue; n = 0;
                    }
                    Hr(client.GetCurrentPadding(out cola));
                    if (cola + Bytes / 4 > Cola)
                    {
                        // Cola llena: arrancar si hace falta y esperar a que Windows gaste un periodo (10 ms).
                        if (!sonando) { Hr(client.Start()); sonando = true; }
                        if (!evento.WaitOne(1000)) throw new IOException("WASAPI atascado");
                        continue;
                    }
                    // Volver a la copia exacta: con paso 1 y fase a medias, este bloque la lleva justo a cero.
                    double p = paso != 1 || fase == 0 ? paso : fase >= 0.5 ? 1 + (1 - fase) / (Bytes / 4) : 1 - fase / (Bytes / 4);
                    int need = Necesarios(p, fase) * 4, colchon = socket.Available + have - need;
                    if (colchon < least) least = colchon;
                    if (colchon > 40 * Bytes) { Tirar(socket, old, colchon - meta); least = int.MaxValue; }
                    else if (++n % 50 == 0) { paso = Paso(least, meta); espera = least / 192; least = int.MaxValue; }
                    bool falto = colchon < 0;
                    while (have < need && !parado)
                    {
                        if (!socket.Poll(500000, SelectMode.SelectRead)) { parado = true; break; }
                        int got = socket.Receive(fifo, have, need - have, SocketFlags.None);
                        if (got <= 0) throw new EndOfStreamException("Audio desconectado");
                        have += got;
                    }
                    if (parado)
                    {
                        // Fin del sonido. Si era tan corto que ni llenó la cola, se le deja sonar antes de parar.
                        if (!sonando) { Hr(client.Start()); sonando = true; Thread.Sleep(Cola / 48 + 20); }
                        continue;
                    }
                    if (falto && sonando)
                    {
                        // ¿Se vació la cola de Windows mientras se esperaba a Android? Entonces se oyó un hueco: más
                        // colchón desde ahora, y se rehace antes de seguir (como mucho 100 ms) para no tartamudear.
                        Hr(client.GetCurrentPadding(out cola));
                        if (cola == 0)
                        {
                            huecos++; limpios = 0;
                            meta = Math.Min(meta + Bytes, 8 * Bytes);
                            for (int t0 = Environment.TickCount; socket.Available < meta && Environment.TickCount - t0 < 100; ) Thread.Sleep(5);
                        }
                    }
                    else if (++limpios >= 6000) { limpios = 0; meta = Math.Max(meta - Bytes, 2 * Bytes); }
                    int used = Remuestrear(fifo, bytes, p, ref fase) * 4;
                    Buffer.BlockCopy(fifo, used, fifo, 0, have - used);
                    have -= used;
                    // Diagnóstico: pico de nivel por segundo (100 bloques de 10 ms) para comprobar que llega sonido.
                    pico = Pico(bytes, Bytes, pico);
                    if (++bloques % 100 == 0)
                    {
                        Console.WriteLine("nivel pico: " + pico + ", huecos: " + huecos + ", descartado: " + descartados / 192 + " ms, en espera: " + espera + " ms, paso: " + paso.ToString("F4") + ", meta: " + meta / 192 + " ms");
                        pico = huecos = descartados = 0;
                        // Cambio de altavoz con el sonido en marcha: se mira una vez por segundo.
                        Marshal.ReleaseComObject(Altavoz(todos, out ahora));
                        if (ahora != id) throw new IOException("cambió el altavoz de Windows");
                    }
                    IntPtr destino;
                    Hr(render.GetBuffer(Bytes / 4, out destino));
                    Marshal.Copy(bytes, 0, destino, Bytes);
                    Hr(render.ReleaseBuffer(Bytes / 4, 0));
                }
            }
        }
        finally
        {
            client.Stop();
            if (render != null) Marshal.ReleaseComObject(render);
            Marshal.ReleaseComObject(client);   // suelta el flujo ya, sin esperar al recolector
            Marshal.ReleaseComObject(altavoz);
            Marshal.ReleaseComObject(todos);
            evento.Dispose();   // después de soltar el flujo que lo usa
            if (prioridad != IntPtr.Zero) AvRevertMmThreadCharacteristics(prioridad);
        }
    }
    // Tira del socket `extra` bytes viejos (en cuadros enteros).
    static void Tirar(Socket socket, byte[] old, int extra)
    {
        extra &= ~3;
        for (int got; extra > 0; extra -= got, descartados += got)
            if ((got = socket.Receive(old, 0, Math.Min(extra, old.Length), SocketFlags.None)) <= 0)
                throw new EndOfStreamException("Audio desconectado");
    }
    // Micrófono (vsock 5004): Android manda {SDMI, tasa, canales, 16} al empezar a grabar y cada 100 ms mientras graba;
    // tasa y canales 0 = dejó de grabar. Windows contesta con PCM S16 de ese formato.
    static void MicHeader(byte[] header, out uint rate, out uint channels)
    {
        rate = BitConverter.ToUInt32(header, 4);
        channels = BitConverter.ToUInt32(header, 8);
        if (BitConverter.ToUInt32(header, 0) != 0x494d4453 || BitConverter.ToUInt32(header, 12) != 16 ||
            !(rate == 0 && channels == 0 || rate >= 8000 && rate <= 48000 && channels >= 1 && channels <= 2))
            throw new InvalidDataException("Cabecera SDMI incompatible");
    }
    static void Microphone(Socket socket)
    {
        // VM pausada a mitad de un envío: cortar la conexión (y soltar el micro) en vez de quedarse bloqueado.
        socket.SendTimeout = 500;
        var header = new byte[16];
        for (bool pending = false; ; )
        {
            if (!pending) Read(socket, header);
            uint rate, channels;
            MicHeader(header, out rate, out channels);
            pending = rate != 0 && Record(socket, rate, channels, header);
        }
    }
    // Abre el micro de Windows mientras Android graba y sigue vivo. Devuelve true si dejó en header otro formato.
    static bool Record(Socket socket, uint rate, uint channels, byte[] header)
    {
        int bytes = (int)(rate / 100 * channels * 2);   // búferes de 10 ms
        var format = new Format { Tag = 1, Channels = (ushort)channels, Rate = rate, BytesPerSecond = rate * channels * 2, BlockAlign = (ushort)(channels * 2), Bits = 16 };
        var headers = new IntPtr[Count];
        var data = new IntPtr[Count];
        var prepared = new bool[Count];
        uint size = (uint)Marshal.SizeOf(typeof(Header));
        var next = new byte[16];
        var buffer = new byte[bytes];
        using (var completed = new AutoResetEvent(false))
        {
            IntPtr device;
            // WAVE_MAPPER sobre WASAPI compartido: Windows convierte del formato del micro al que pide Android.
            Check(waveInOpen(out device, UInt32.MaxValue, ref format, completed.SafeWaitHandle.DangerousGetHandle(), IntPtr.Zero, 0x50000)); // CALLBACK_EVENT
            Console.WriteLine("micro abierto: " + rate + " Hz, " + channels + " canales");
            try
            {
                for (int i = 0; i < Count; i++)
                {
                    data[i] = Marshal.AllocHGlobal(bytes);
                    headers[i] = Marshal.AllocHGlobal((int)size);
                    Marshal.StructureToPtr(new Header { Data = data[i], Length = (uint)bytes }, headers[i], false);
                    Check(waveInPrepareHeader(device, headers[i], size));
                    prepared[i] = true;
                    Check(waveInAddBuffer(device, headers[i], size));
                }
                Check(waveInStart(device));
                int alive = Environment.TickCount, peak = 0, blocks = 0;
                for (int i = 0; ; )
                {
                    completed.WaitOne(100);
                    for (;;)
                    {
                        var h = (Header)Marshal.PtrToStructure(headers[i], typeof(Header));
                        if ((h.Flags & 1) == 0) break;   // WHDR_DONE: los búferes vuelven en orden
                        int n = (int)h.Recorded;
                        Marshal.Copy(data[i], buffer, 0, n);
                        peak = Pico(buffer, n, peak);
                        if (++blocks % 100 == 0) { Console.WriteLine("micro pico: " + peak); peak = 0; }
                        socket.Send(buffer, 0, n, SocketFlags.None);
                        Check(waveInAddBuffer(device, headers[i], size));
                        i = (i + 1) % Count;
                    }
                    while (socket.Available >= 16)
                    {
                        Read(socket, next);
                        uint r, c;
                        MicHeader(next, out r, out c);
                        if (r != rate || c != channels) { Array.Copy(next, header, 16); return r != 0; }
                        alive = Environment.TickCount;
                    }
                    // Sin señal de vida: Android ya no está delante (VM pausada) o la conexión está medio muerta: soltar el
                    // micro (finally) y reconectar; al volver, Android anuncia otra vez que graba.
                    if (Environment.TickCount - alive > 300) throw new IOException("micro: Android no responde");
                }
            }
            finally
            {
                uint reset = waveInReset(device);
                for (int i = 0; i < Count; i++)
                {
                    if (prepared[i] && (reset != 0 || waveInUnprepareHeader(device, headers[i], size) != 0)) continue;
                    if (headers[i] != IntPtr.Zero) Marshal.FreeHGlobal(headers[i]);
                    if (data[i] != IntPtr.Zero) Marshal.FreeHGlobal(data[i]);
                }
                uint close = waveInClose(device);
                // Si no cierra (micro retirado), no se libera su memoria y se sigue: matar el proceso dejaba mudo el altavoz.
                Console.WriteLine(reset != 0 || close != 0 ? "micro sin cerrar: " + reset + "/" + close : "micro cerrado");
            }
        }
    }
    static void SelfCheck()
    {
        var good = new byte[16];
        uint[] values = { 0x55414453, 48000, 2, 16 };
        for (int i = 0; i < 4; i++) Array.Copy(BitConverter.GetBytes(values[i]), 0, good, i * 4, 4);
        Validate(good);
        for (int i = 0; i < good.Length; i++)
        {
            good[i] ^= 1;
            bool rejected = false;
            try { Validate(good); } catch (InvalidDataException) { rejected = true; }
            if (!rejected) throw new Exception("Autocomprobación SDAU fallida");
            good[i] ^= 1;
        }
        uint rate, channels;
        uint[][] mic = { new uint[] { 0x494d4453, 16000, 1, 16 }, new uint[] { 0x494d4453, 0, 0, 16 },
                         new uint[] { 0x494d4453, 7999, 1, 16 }, new uint[] { 0x494d4453, 48000, 3, 16 }, new uint[] { 0x55414453, 48000, 2, 16 } };
        for (int m = 0; m < mic.Length; m++)
        {
            for (int i = 0; i < 4; i++) Array.Copy(BitConverter.GetBytes(mic[m][i]), 0, good, i * 4, 4);
            bool rejected = false;
            try { MicHeader(good, out rate, out channels); } catch (InvalidDataException) { rejected = true; }
            if (rejected != (m >= 2)) throw new Exception("Autocomprobación SDMI fallida");
        }
        if (Bytes != 1920 || Marshal.SizeOf(typeof(Header)) != 48 || Marshal.SizeOf(typeof(Format)) != 18)
            throw new Exception("Disposición ARM64 incompatible");
        // Remuestreo: una rampa (cuadro k = 8k en los dos canales) debe salir exacta con paso 1 y lineal con cualquier otro.
        var rampa = new byte[2 * Bytes]; var fuera = new byte[Bytes];
        for (int k = 0; k < rampa.Length / 4; k++) for (int c = 0; c < 4; c += 2) { rampa[k * 4 + c] = (byte)(k * 8); rampa[k * 4 + c + 1] = (byte)(k * 8 >> 8); }
        double fase = 0;
        if (Remuestrear(rampa, fuera, 1, ref fase) != 480 || fase != 0 || M(fuera, 0) != 8 || M(fuera, Bytes - 2) != 480 * 8)
            throw new Exception("Autocomprobación de copia fallida");
        int gastados = Remuestrear(rampa, fuera, 1.004, ref fase);   // 480 * 1,004 = 481,92
        if (gastados != 481 || Math.Abs(fase - 0.92) > 1e-6 || Necesarios(1.01, 0.999) > rampa.Length / 4 || Math.Abs(M(fuera, 400) - (1 + 100 * 1.004) * 8) > 1)
            throw new Exception("Autocomprobación de remuestreo fallida");
        double vuelta = 1 + (1 - fase) / 480;   // el bloque que devuelve la fase a cero
        if (Remuestrear(rampa, fuera, vuelta, ref fase) != 481 || fase != 0) throw new Exception("Autocomprobación de fase fallida");
        if (Pico(new byte[] { 0, 0x80, 5, 0 }, 4, 0) != 32768) throw new Exception("Autocomprobación de pico fallida");
        if (Paso(2 * Bytes, 2 * Bytes) != 1 || Paso(6 * Bytes, 2 * Bytes) <= 1 || Paso(0, 4 * Bytes) >= 1 || Paso(100 * Bytes, 0) != 1.01)
            throw new Exception("Autocomprobación de paso fallida");
    }
    static int Main(string[] args)
    {
        Guid vm;
        if (args.Length != 1 || !Guid.TryParse(args[0], out vm))
        {
            Console.Error.WriteLine("Uso: HvAudio.exe <VmId>");
            return 1;
        }
        SelfCheck();
        new Thread(() => Serve(vm, 5004, Microphone)) { IsBackground = true }.Start();
        Serve(vm, 5003, Play);
        return 0;
    }
    static void Serve(Guid vm, uint port, Action<Socket> body)
    {
        var service = new Guid(port.ToString("x8") + "-facb-11e6-bd58-64006a7986d3");
        for (;;)
        {
            try
            {
                using (var socket = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1))
                {
                    socket.Connect(new HvEndPoint(vm, service));
                    body(socket);
                }
            }
            catch (SocketException e) { Console.Error.WriteLine("Audio " + port + ": " + e.Message); }
            catch (IOException e) { Console.Error.WriteLine("Audio " + port + ": " + e.Message); }
            catch (InvalidDataException e) { Console.Error.WriteLine("Audio " + port + ": " + e.Message); }
            catch (Exception e) { Console.Error.WriteLine("Audio " + port + ": " + e); }   // nada tumba el audio: se reintenta
            VmActiva.Reintentar(1000);
        }
    }
}
