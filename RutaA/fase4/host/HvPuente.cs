// Puente TCP local -> hv_sock de la VM (AF_HYPERV): "adb connect 127.0.0.1:5556" llega al adbd de Android (vsock 5555).
//   HvPuente.exe <VmId> [puertoLocal=5556] [puertoVsock=5555]
// Solo escucha en 127.0.0.1. Un hilo por sentido y conexión: adb abre pocas.
using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;

class HvEndPoint : EndPoint
{
    readonly Guid vm, servicio;
    public HvEndPoint(Guid v, Guid s) { vm = v; servicio = s; }
    public override AddressFamily AddressFamily => (AddressFamily)34;   // AF_HYPERV
    public override SocketAddress Serialize()
    {
        var a = new SocketAddress((AddressFamily)34, 36);   // SOCKADDR_HV: familia(2) reservado(2) VmId(16) ServiceId(16)
        byte[] v = vm.ToByteArray(), s = servicio.ToByteArray();
        for (int i = 0; i < 16; i++) { a[4 + i] = v[i]; a[20 + i] = s[i]; }
        return a;
    }
    public override EndPoint Create(SocketAddress a) => this;
}

// Pausa = cero (B4): el Visor quita este aviso al pausar la VM y lo pone al reanudarla (VisorVM.Reintentar, mismo nombre).
// Sin Visor el aviso nace puesto: reintentos de siempre.
static class VmActiva
{
    static readonly EventWaitHandle aviso = new EventWaitHandle(true, EventResetMode.ManualReset, @"Local\SurfaceDualVmActiva");
    // Antes de reintentar: ms con la VM corriendo; en pausa, nada hasta que se reanude (60 s de tope por si el Visor murió).
    public static void Reintentar(int ms) { if (aviso.WaitOne(0)) Thread.Sleep(ms); else aviso.WaitOne(60000); }
}

static class HvPuente
{
    static void Copiar(Socket de, Socket a)
    {
        var b = new byte[1 << 16];
        try { int n; while ((n = de.Receive(b)) > 0) a.Send(b, n, SocketFlags.None); } catch (SocketException) { } catch (ObjectDisposedException) { }
        try { a.Shutdown(SocketShutdown.Send); } catch { }
    }

    static int Main(string[] args)
    {
        var vm = Guid.Parse(args[0]);
        int local = args.Length > 1 ? int.Parse(args[1]) : 5556, vsock = args.Length > 2 ? int.Parse(args[2]) : 5555;
        var servicio = Guid.Parse(vsock.ToString("x8") + "-facb-11e6-bd58-64006a7986d3");   // plantilla vsock de Linux
        var l = new TcpListener(IPAddress.Loopback, local);
        l.Start();
        Console.WriteLine("puente 127.0.0.1:{0} -> {1} vsock {2}", local, vm, vsock);
        while (true)
        {
            var c = l.AcceptSocket();
            var h = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1);   // HV_PROTOCOL_RAW
            try { h.Connect(new HvEndPoint(vm, servicio)); }
            catch (SocketException e) { Console.WriteLine("hv_sock: " + e.SocketErrorCode); c.Close(); h.Close(); continue; }
            new Thread(() => { Copiar(c, h); }) { IsBackground = true }.Start();
            new Thread(() => { Copiar(h, c); c.Close(); h.Close(); }) { IsBackground = true }.Start();
        }
    }
}
