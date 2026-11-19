// Uso: HvMdns.exe (oculto, lo arranca iniciar.ps1 junto a HvAudio). Deja ver los Chromecast/Google Home de la Wi-Fi desde Android.
// Solo Google Cast: las consultas mDNS de la VM (Default Switch) salen a la Wi-Fi como "legacy unicast" (puerto efímero, así la
// respuesta entra sin regla de firewall en la Wi-Fi) y las respuestas vuelven en multicast al Default Switch. Sin sondeos: solo eventos.
// Compilar: el csc de compilar.sh con -target:winexe -main:HvMdns -out:HvMdns.exe HvMdns.cs (solo mscorlib, System, System.Core).
using System;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using System.Threading;

static class HvMdns
{
    static readonly IPAddress Grupo = IPAddress.Parse("224.0.0.251");
    static readonly IPEndPoint Destino = new IPEndPoint(Grupo, 5353);
    static readonly byte[] Cast = Encoding.ASCII.GetBytes("\x0b_googlecast\x04_tcp");
    const int Maximo = 8;  // consultas reenviadas por segundo: tope contra bucles o tormentas

    static bool EsCast(byte[] b, int n)
    {
        for (int i = 12; i + Cast.Length <= n; i++)
        {
            int j = 0;
            while (j < Cast.Length && char.ToLowerInvariant((char)b[i + j]) == char.ToLowerInvariant((char)Cast[j])) j++;
            if (j == Cast.Length) return true;
        }
        return false;
    }
    static bool EsRespuesta(byte[] b, int n) { return n > 12 && (b[2] & 0x80) != 0; }

    static IPAddress Ip(Func<NetworkInterface, bool> cual)
    {
        return NetworkInterface.GetAllNetworkInterfaces()
            .Where(i => i.OperationalStatus == OperationalStatus.Up && cual(i))
            .SelectMany(i => i.GetIPProperties().UnicastAddresses).Select(a => a.Address)
            .FirstOrDefault(a => a.AddressFamily == AddressFamily.InterNetwork);
    }
    // La red de casa: la interfaz con puerta de enlace IPv4 que no es virtual de Hyper-V (Wi-Fi o Ethernet real).
    static IPAddress Casa()
    {
        return Ip(i => !i.Name.StartsWith("vEthernet") && i.GetIPProperties().GatewayAddresses
            .Any(g => g.Address.AddressFamily == AddressFamily.InterNetwork && !g.Address.Equals(IPAddress.Any)));
    }

    static Socket Udp(IPEndPoint donde)
    {
        var s = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
        s.IOControl(-1744830452, new byte[4], null);  // SIO_UDP_CONNRESET off: un ICMP "inalcanzable" no rompe ReceiveFrom
        s.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);  // 5353 lo comparte el mDNS de Windows
        s.Bind(donde);
        s.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastTimeToLive, 255);
        s.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastLoopback, false);
        return s;
    }

    static void SelfCheck()
    {
        var q = new byte[] { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }.Concat(Encoding.ASCII.GetBytes("\x0b_GoogleCast\x04_tcp\x05local\0\0\x0c\0\x01")).ToArray();
        var otro = new byte[] { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }.Concat(Encoding.ASCII.GetBytes("\x08_airplay\x04_tcp\x05local\0\0\x0c\0\x01")).ToArray();
        if (!EsCast(q, q.Length) || EsCast(otro, otro.Length) || EsRespuesta(q, q.Length)) throw new Exception("Autocomprobación HvMdns fallida");
        q[2] = 0x84;
        if (!EsRespuesta(q, q.Length)) throw new Exception("Autocomprobación HvMdns fallida");
    }

    static int Main()
    {
        SelfCheck();
        var ipVm = Ip(i => i.Name == "vEthernet (Default Switch)");
        if (ipVm == null) { Console.Error.WriteLine("HvMdns: no hay vEthernet (Default Switch)"); return 1; }
        var vm = Udp(new IPEndPoint(ipVm, 5353));
        vm.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.AddMembership, new MulticastOption(Grupo, ipVm));
        vm.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastInterface, ipVm.GetAddressBytes());
        var lan = Udp(new IPEndPoint(IPAddress.Any, 0));
        var casa = Casa();  // se recalcula solo cuando Windows avisa de un cambio de red (Wi-Fi nueva, apagada...)
        NetworkChange.NetworkAddressChanged += (o, e) => casa = Casa();

        // Respuestas de la Wi-Fi (unicast a nuestro puerto efímero) -> multicast al Default Switch.
        new Thread(() =>
        {
            var b = new byte[9000]; EndPoint de = new IPEndPoint(IPAddress.Any, 0);
            for (;;)
                try
                {
                    int n = lan.ReceiveFrom(b, ref de);
                    if (((IPEndPoint)de).Port != 5353 || !EsRespuesta(b, n) || !EsCast(b, n)) continue;
                    b[0] = b[1] = 0;  // id 0, como una respuesta multicast normal
                    vm.SendTo(b, n, SocketFlags.None, Destino);
                }
                catch (SocketException e) { Console.Error.WriteLine("HvMdns lan: " + e.Message); }
        }) { IsBackground = true }.Start();

        // Consultas de Google Cast de la VM -> Wi-Fi, con tope por segundo. Lo que manda este mismo equipo se ignora (sin bucles).
        var buf = new byte[9000]; EndPoint origen = new IPEndPoint(IPAddress.Any, 0);
        long segundo = 0; int cuenta = 0;
        for (;;)
            try
            {
                int n = vm.ReceiveFrom(buf, ref origen);
                if (((IPEndPoint)origen).Address.Equals(ipVm) || EsRespuesta(buf, n) || !EsCast(buf, n)) continue;
                long ahora = Environment.TickCount / 1000;
                if (ahora != segundo) { segundo = ahora; cuenta = 0; }
                if (++cuenta > Maximo) continue;
                var ip = casa;
                if (ip == null) continue;
                lan.SetSocketOption(SocketOptionLevel.IP, SocketOptionName.MulticastInterface, ip.GetAddressBytes());
                lan.SendTo(buf, n, SocketFlags.None, Destino);
            }
            catch (SocketException e) { Console.Error.WriteLine("HvMdns vm: " + e.Message); }
    }
}
