package com.surfacedual.launcher;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Orden del agente (gestos que detecta el visor): --es cual qs|noti|recientes|cajon. La misma orden otra vez cierra.
 * Pruebas desde adb (sin pellizco real): --es cual editar|ajustes|fondo|widgets abre eso en el inicio.
 * Modo lectura desde adb: --es lectura on|off.
 * Prueba desde adb: --es tareas "39:com.android.settings,34:com.android.vending" sustituye la lista del agente.
 */
public class OrdenPanel extends BroadcastReceiver {
    @Override public void onReceive(Context c, Intent i) {
        String cual = i.getStringExtra("cual"), tareas = i.getStringExtra("tareas"), isla = i.getStringExtra("isla");
        // Del agente: "mic|cam <paquete> 1|0" (micrófono/cámara en uso) y "volwin" (el volumen lo cambió Windows: sin aviso).
        if (isla != null && IslaOyente.activo != null) IslaOyente.activo.delAgente(isla);
        String linea = i.getStringExtra("linea");   // mantener la línea de abajo (visor): crece, se suelta o se abre Circle to Search
        if (linea != null && linea.matches("mantener|suelta|disparo")) Borde.de(c).lineaGesto(linea);
        if (tareas != null && tareas.matches("([0-9]+:[A-Za-z0-9_.]+(,[0-9]+:[A-Za-z0-9_.]+)*)?")) Paneles.ultimasTareas = tareas;
        String lectura = i.getStringExtra("lectura");   // modo lectura por adb (medición): --es lectura on|off, el mismo camino que el botón
        if (lectura != null && lectura.matches("on|off")) Inicio.peticionAgente(c, "lectura " + lectura, null, null);
        if (cual != null && cual.matches("qs|noti|recientes|cajon")) Paneles.de(c).alternar(cual);
        else if (cual != null && cual.matches("editar|ajustes|fondo|widgets"))
            { Inicio.panelPedido = cual; c.startActivity(new Intent(c, Inicio.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
    }
}
