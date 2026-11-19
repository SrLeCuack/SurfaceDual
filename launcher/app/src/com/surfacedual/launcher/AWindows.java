package com.surfacedual.launcher;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.service.quicksettings.TileService;
import android.widget.Toast;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicBoolean;

/** Botón "Cambiar a Windows" de la configuración rápida, y el aviso al visor de Windows. */
public class AWindows extends TileService {
    // El visor escucha en 127.0.0.1:4445 del host; slirp lo expone al invitado como 10.0.2.2:4445.
    static final String HOST = "10.0.3.2";   // red del sistema (eth1): funciona aunque el Wi-Fi de Android esté apagado
    static final int PUERTO = 4445;
    static final AtomicBoolean enCurso = new AtomicBoolean(), escuchando = new AtomicBoolean();
    static volatile Socket visorVM;   // Hyper-V: conexión abierta por el VisorVM de Windows (null en QEMU)

    /** Hyper-V: no hay 10.0.3.2 y el firewall de Windows no deja entrar, así que el VisorVM se conecta aquí y espera. */
    static void escuchar(Context c) {
        if (!escuchando.compareAndSet(false, true)) return;
        Context app = c.getApplicationContext();
        android.net.ConnectivityManager cm = app.getSystemService(android.net.ConnectivityManager.class);
        new Thread(() -> {
            try (ServerSocket ss = new ServerSocket(PUERTO)) {
                for (;;) {
                    Socket s;
                    try { s = ss.accept(); }
                    catch (java.io.IOException e) { if (ss.isClosed()) throw e; continue; }   // fallo de una conexión: seguir escuchando
                    // Solo Windows (la puerta de enlace de la VM, como el agente en 4449): otra app no puede quedarse el canal
                    // y dejar sin efecto Suspender/Apagar (revisión Gemini 17/11).
                    if (!deWindows(cm, s.getInetAddress())) { try { s.close(); } catch (Exception e) { } continue; }
                    Socket viejo = visorVM;
                    visorVM = s;
                    if (viejo != null) try { viejo.close(); } catch (Exception e) { }
                    // Leer hasta el final dice cuándo se fue el visor (cerrado o reiniciado); si no, la primera orden se perdía
                    // escrita en un socket muerto. Sus líneas ("pendientes <ms>", "abrir <id>") son de Avisos en Windows.
                    new Thread(() -> {
                        try {
                            IslaOyente.alConectar(app);
                            java.io.InputStream in = s.getInputStream();
                            StringBuilder l = new StringBuilder();
                            for (int b; (b = in.read()) >= 0; ) {
                                if (b != '\n') { if (l.length() < 96) l.append((char) b); continue; }   // tope: lo que sobra se corta y no casa ("wifi" con SSID de 32 bytes en hex: 78)
                                if (l.indexOf("tiron ") == 0) Paneles.delVisor(app, l.toString());
                                else if (l.indexOf("wifi ") == 0) Paneles.wifiDelVisor(app, l.toString());
                                else IslaOyente.delVisor(l.toString());
                                l.setLength(0);
                            }
                        } catch (Exception e) { }
                        if (visorVM == s) visorVM = null;
                    }).start();
                }
            } catch (Exception e) { escuchando.set(false); }
        }).start();
    }

    /** Una línea ASCII al visor por 4445 (un escritor a la vez). false = sin visor. Nunca en el hilo principal. */
    static boolean alVisor(String linea) {
        Socket v = visorVM;
        if (v == null) return false;
        synchronized (AWindows.class) {
            try {
                OutputStream o = v.getOutputStream();
                o.write((linea + "\n").getBytes("US-ASCII"));
                o.flush();
                return true;
            } catch (Exception e) { if (visorVM == v) visorVM = null; return false; }   // visor cerrado (si no se reconectó entretanto)
        }
    }

    static boolean deWindows(android.net.ConnectivityManager cm, java.net.InetAddress a) {
        android.net.LinkProperties lp = cm == null ? null : cm.getLinkProperties(cm.getActiveNetwork());
        if (lp == null || a == null) return false;
        String ip = a.getHostAddress().replaceFirst("^::ffff:", "");
        for (android.net.RouteInfo r : lp.getRoutes())
            if (r.isDefaultRoute() && r.getGateway() != null && ip.equals(r.getGateway().getHostAddress())) return true;
        return false;
    }

    /** Pregunta cómo salir (pedido del usuario 16/11): Suspender = volver al instante después (Android sigue ocupando su RAM);
     *  Apagar = libera la RAM, la próxima vez enciende en ~20 s. */
    static android.app.AlertDialog dialogo(Context c) {
        return new android.app.AlertDialog.Builder(c).setTitle("Volver a Windows")
            .setMessage("Suspender: Android queda en pausa y vuelve al instante (si pasa 1 hora sin volver, se apaga solo).\nApagar: libera la memoria; encender de nuevo tarda unos 35 segundos.")
            .setPositiveButton("Suspender", (d, w) -> volver(c, false))
            .setNegativeButton("Apagar", (d, w) -> volver(c, true))
            .setNeutralButton("Cancelar", null).create();
    }

    static void volver(Context c) { volver(c, false); }

    /** Pide al visor volver a Windows (suspender) o apagar Android. Hilo propio: nunca bloquea la interfaz; una petición a la vez. */
    static void volver(Context c, boolean apagar) {
        if (!enCurso.compareAndSet(false, true)) return;
        Context app = c.getApplicationContext();
        String orden = apagar ? "apagar" : "windows";
        new Thread(() -> {
            if (alVisor(orden)) { enCurso.set(false); return; }
            // Sin conexión del visor (p. ej. recién reiniciada la VM): por el agente, que siempre está conectado con Windows
            // (4449). Si tampoco, QEMU (10.0.3.2) y, por último, el aviso.
            Inicio.peticionAgente(app, "volver " + orden, r -> enCurso.set(false), () -> new Thread(() -> directo(app, orden)).start());
        }).start();
    }

    static void directo(Context app, String orden) {
        try (Socket s = new Socket()) {
            s.connect(new InetSocketAddress(HOST, PUERTO), 1000);
            OutputStream o = s.getOutputStream();
            o.write((orden + "\n").getBytes("US-ASCII"));   // también aquí "Apagar" apaga
            o.flush();
        } catch (Exception e) {
            new Handler(Looper.getMainLooper()).post(() ->
                    Toast.makeText(app, "No se pudo avisar a Windows", Toast.LENGTH_SHORT).show());
        } finally {
            enCurso.set(false);
        }
    }

    @Override public void onClick() { showDialog(dialogo(this)); }
}
