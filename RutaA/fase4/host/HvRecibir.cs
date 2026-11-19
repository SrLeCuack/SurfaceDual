// Receptor de la medición de hv_sock en Windows (AF_HYPERV): se conecta a la VM y lee todo lo que manda vsock_envio.
//   HvRecibir.exe <VmId> [puerto=5000]
using System;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;

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

static class HvRecibir
{
    static int Main(string[] args)
    {
        var vm = Guid.Parse(args[0]);
        int puerto = args.Length > 1 ? int.Parse(args[1]) : 5000;
        var servicio = Guid.Parse(puerto.ToString("x8") + "-facb-11e6-bd58-64006a7986d3");   // plantilla vsock de Linux
        using (var s = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1))   // HV_PROTOCOL_RAW
        {
            try { s.Connect(new HvEndPoint(vm, servicio)); }
            catch (SocketException e) { Console.WriteLine("conectar: " + e.SocketErrorCode + " (" + e.NativeErrorCode + ")"); return 1; }
            var buf = new byte[1 << 20]; long total = 0;
            var p = Process.GetCurrentProcess(); var cpu0 = p.TotalProcessorTime; var reloj = Stopwatch.StartNew();
            int n; while ((n = s.Receive(buf)) > 0) total += n;
            reloj.Stop(); p.Refresh();
            double seg = reloj.Elapsed.TotalSeconds, cpu = (p.TotalProcessorTime - cpu0).TotalSeconds;
            Console.WriteLine("recibidos {0} MB en {1:F3} s = {2:F0} MB/s; CPU del receptor {3:F2} s ({4:F0} % de un núcleo)",
                total >> 20, seg, total / 1048576.0 / seg, cpu, cpu / seg * 100);
        }
        return 0;
    }
}
