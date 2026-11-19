// Prueba de la entrada táctil (entrada.cpp del composer): manda un deslizamiento de un dedo por vsock 5002.
//   HvToque.exe <VmId> x1 y1 x2 y2 [pasos=20] [ms_por_paso=8]   (coordenadas de la pantalla de Android)
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
        var a = new SocketAddress((AddressFamily)34, 36);
        byte[] v = vm.ToByteArray(), s = servicio.ToByteArray();
        for (int i = 0; i < 16; i++) { a[4 + i] = v[i]; a[20 + i] = s[i]; }
        return a;
    }
    public override EndPoint Create(SocketAddress a) => this;
}

static class HvToque
{
    static void Toque(Socket s, uint tipo, int x, int y)
    {
        var b = new byte[24];
        BitConverter.GetBytes(0x51544453u).CopyTo(b, 0); BitConverter.GetBytes(tipo).CopyTo(b, 4);
        BitConverter.GetBytes(0u).CopyTo(b, 8); BitConverter.GetBytes(x).CopyTo(b, 12); BitConverter.GetBytes(y).CopyTo(b, 16);
        s.Send(b);
    }

    static int Main(string[] a)
    {
        var vm = Guid.Parse(a[0]);
        if (a[1] == "tecla")   // HvToque.exe <VmId> tecla <código Linux>...: pulsa y suelta cada tecla
            using (var t = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1))
            {
                t.Connect(new HvEndPoint(vm, Guid.Parse("0000138a-facb-11e6-bd58-64006a7986d3")));
                for (int i = 2; i < a.Length; i++)
                    foreach (int v in new[] { 1, 0 })
                    {
                        var b = new byte[24];
                        BitConverter.GetBytes(0x51544453u).CopyTo(b, 0); BitConverter.GetBytes(3u).CopyTo(b, 4);
                        BitConverter.GetBytes(int.Parse(a[i])).CopyTo(b, 8); BitConverter.GetBytes(v).CopyTo(b, 12);
                        t.Send(b); Thread.Sleep(30);
                    }
                Thread.Sleep(100);
                return 0;
            }
        if (a[1] == "pluma")   // HvToque.exe <VmId> pluma x1 y1 x2 y2: trazo con presión media (acercar, tocar, mover, levantar)
            using (var t = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1))
            {
                t.Connect(new HvEndPoint(vm, Guid.Parse("0000138a-facb-11e6-bd58-64006a7986d3")));
                int px1 = int.Parse(a[2]), py1 = int.Parse(a[3]), px2 = int.Parse(a[4]), py2 = int.Parse(a[5]);
                Action<int, int, int, uint> p = (estado, x, y, presion) =>
                {
                    var b = new byte[24];
                    BitConverter.GetBytes(0x51544453u).CopyTo(b, 0); BitConverter.GetBytes(4u).CopyTo(b, 4);
                    BitConverter.GetBytes(estado).CopyTo(b, 8); BitConverter.GetBytes(x).CopyTo(b, 12); BitConverter.GetBytes(y).CopyTo(b, 16);
                    BitConverter.GetBytes(presion | 90u << 16 | 90u << 24).CopyTo(b, 20);
                    t.Send(b); Thread.Sleep(10);
                };
                p(1, px1, py1, 0);
                for (int i = 0; i <= 20; i++) p(3, px1 + (px2 - px1) * i / 20, py1 + (py2 - py1) * i / 20, 512);
                p(1, px2, py2, 0); p(0, px2, py2, 0);
                Thread.Sleep(100);
                return 0;
            }
        int x1 = int.Parse(a[1]), y1 = int.Parse(a[2]), x2 = int.Parse(a[3]), y2 = int.Parse(a[4]);
        int pasos = a.Length > 5 ? int.Parse(a[5]) : 20, ms = a.Length > 6 ? int.Parse(a[6]) : 8;
        using (var s = new Socket((AddressFamily)34, SocketType.Stream, (ProtocolType)1))
        {
            s.Connect(new HvEndPoint(vm, Guid.Parse("0000138a-facb-11e6-bd58-64006a7986d3")));   // vsock 5002
            Toque(s, 0, x1, y1);
            for (int i = 1; i <= pasos; i++) { Thread.Sleep(ms); Toque(s, 1, x1 + (x2 - x1) * i / pasos, y1 + (y2 - y1) * i / pasos); }
            Toque(s, 2, x2, y2);
            Thread.Sleep(100);
        }
        return 0;
    }
}
