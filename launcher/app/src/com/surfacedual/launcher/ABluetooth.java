package com.surfacedual.launcher;

import android.os.Handler;
import android.os.Looper;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Botón de Bluetooth de la configuración rápida: muestra y mueve el radio Bluetooth de Windows (el visor, por 4445).
 * La pila de Bluetooth de Android sigue apagada: sin controlador entra en un bucle de caídas.
 * shortcut: el estado se pide al abrir la configuración rápida; un cambio hecho en Windows con el panel abierto se ve
 * al volver a abrirlo. Si hiciera falta en vivo, mantener la conexión abierta mientras escucha.
 */
public class ABluetooth extends TileService {
    static final AtomicBoolean enCurso = new AtomicBoolean();   // una petición a la vez: toques rápidos no invierten el botón
    static volatile boolean toquePendiente;   // un toque durante la consulta inicial se atiende al llegar la respuesta
    @Override public void onStartListening() { pedir("bt"); }

    @Override public void onClick() {
        if (enCurso.get()) { toquePendiente = true; return; }
        Tile t = getQsTile();
        if (t != null && t.getState() != Tile.STATE_UNAVAILABLE) pedir(t.getState() == Tile.STATE_ACTIVE ? "bt off" : "bt on");
    }

    /** Hilo propio (nunca bloquea la interfaz). El visor responde "on", "off" o "no" (sin radio). */
    void pedir(String orden) {
        if (!enCurso.compareAndSet(false, true)) return;
        new Thread(() -> {
            String r = null;
            try (Socket s = new Socket()) {
                s.connect(new InetSocketAddress(AWindows.HOST, AWindows.PUERTO), 1000);
                s.setSoTimeout(5000);   // encender el radio tarda; sin respuesta, el botón queda "no disponible"
                s.getOutputStream().write((orden + "\n").getBytes("US-ASCII"));
                r = new BufferedReader(new InputStreamReader(s.getInputStream(), "US-ASCII")).readLine();
            } catch (Exception e) { }
            String estado = r;
            new Handler(Looper.getMainLooper()).post(() -> {
                enCurso.set(false);   // ya con el botón al día: el siguiente toque parte del estado real
                Tile t = getQsTile();
                if (t == null) { toquePendiente = false; return; }   // el panel ya se cerró
                t.setState("on".equals(estado) ? Tile.STATE_ACTIVE : "off".equals(estado) ? Tile.STATE_INACTIVE : Tile.STATE_UNAVAILABLE);
                t.updateTile();
                if (toquePendiente) { toquePendiente = false; onClick(); }
            });
        }).start();
    }
}
