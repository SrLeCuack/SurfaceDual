package com.surfacedual.launcher;

import android.app.Notification;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Chronometer;
import android.widget.FrameLayout;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.List;

/**
 * Servicio de escucha de notificaciones: alimenta la isla (música, llamadas, navegación, temporizadores, alarmas,
 * grabación, descargas y avisos breves) y los paneles, sin sondeo. Android lo arranca al conceder 'cmd notification
 * allow_listener' (lo hace el agente).
 */
public class IslaOyente extends NotificationListenerService implements MediaSessionManager.OnActiveSessionsChangedListener {
    MediaSessionManager mediaMgr;
    ComponentName componente;
    MediaController controladorActual;
    Isla isla;
    static IslaOyente activo;   // conectado: los paneles leen sus avisos y su sesión multimedia
    final Handler principal = new Handler(Looper.getMainLooper());
    static final int FLAG_PROMOVIDA = 0x00040000;   // Notification.FLAG_PROMOTED_ONGOING ("Live Updates", Android 16+)

    final MediaController.Callback cb = new MediaController.Callback() {
        @Override public void onPlaybackStateChanged(PlaybackState state) { principal.post(() -> revisarSesiones()); }
        @Override public void onMetadataChanged(MediaMetadata metadata) { principal.post(() -> ponerMedia()); }
        @Override public void onSessionDestroyed() { principal.post(() -> { desvincularControlador(); revisarSesiones(); }); }
    };

    // Avisos breves de 2 s: cargador, modo silencio/vibración y volumen.
    final BroadcastReceiver eventos = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) {
            if (android.app.AlarmManager.ACTION_NEXT_ALARM_CLOCK_CHANGED.equals(i.getAction())) { red.execute(() -> alConectar(c.getApplicationContext())); return; }
            Isla is = isla();
            if (is == null || isInitialStickyBroadcast()) return;   // al registrarse llega el último "pegajoso": no es un cambio
            Isla.Act a = new Isla.Act();
            String ac = i.getAction();
            if (Intent.ACTION_SCREEN_OFF.equals(ac) || Intent.ACTION_USER_PRESENT.equals(ac) || Intent.ACTION_SCREEN_ON.equals(ac)) {
                // escondida con la pantalla apagada o el bloqueo puesto (y el visor deja de tomar sus toques)
                is.bloqueada = Intent.ACTION_SCREEN_OFF.equals(ac) || c.getSystemService(android.app.KeyguardManager.class).isKeyguardLocked();
                is.aplicarVisibilidad();
                return;
            }
            if (Intent.ACTION_POWER_CONNECTED.equals(ac)) {
                BatteryManager bm = c.getSystemService(BatteryManager.class);
                a.clave = "breve"; a.color = Isla.VERDE;
                a.icono = Paneles.de(c).icono("ic_qs_battery_saver", Isla.VERDE);
                a.texto = (bm == null ? "" : bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY) + Idioma.t(" % ", "% ")) + Idioma.t("· Cargando", "· Charging");
            } else if (AudioManager.RINGER_MODE_CHANGED_ACTION.equals(ac)) {
                int m = i.getIntExtra(AudioManager.EXTRA_RINGER_MODE, AudioManager.RINGER_MODE_NORMAL);
                a.clave = "breve"; a.color = m == AudioManager.RINGER_MODE_NORMAL ? Color.WHITE : 0xFFFF9F0A;
                a.icono = Paneles.de(c).icono(m == AudioManager.RINGER_MODE_NORMAL ? "ic_volume" : "ic_audio_vol_mute", a.color);
                a.texto = m == AudioManager.RINGER_MODE_SILENT ? Idioma.t("Silencio", "Silent") : m == AudioManager.RINGER_MODE_VIBRATE ? Idioma.t("Vibración", "Vibrate") : Idioma.t("Sonido", "Sound");
            } else if ("android.media.VOLUME_CHANGED_ACTION".equals(ac)) {
                if (i.getIntExtra("android.media.EXTRA_VOLUME_STREAM_TYPE", -1) != AudioManager.STREAM_MUSIC) return;
                if (android.os.SystemClock.uptimeMillis() - volWindows < 1500) return;   // lo movió Windows (su propio aviso ya se ve)
                AudioManager am = c.getSystemService(AudioManager.class);
                int v = i.getIntExtra("android.media.EXTRA_VOLUME_STREAM_VALUE", 0), max = Math.max(1, am.getStreamMaxVolume(AudioManager.STREAM_MUSIC));
                if (v == i.getIntExtra("android.media.EXTRA_PREV_VOLUME_STREAM_VALUE", v)) return;
                a.clave = "breve"; a.color = Color.WHITE;
                a.icono = Paneles.de(c).icono("ic_volume", Color.WHITE);
                a.progreso = 100 * v / max;
                a.texto = (100 * v / max) + Idioma.t(" %", "%");
            } else return;
            is.avisoBreve(a);
        }
    };

    @Override public void onListenerConnected() {
        super.onListenerConnected();
        principal.post(() -> {
            activo = this;
            if (!ISLA) apagarIsla.run();
            AWindows.escuchar(this);   // el canal con el visor aunque Inicio no haya arrancado (launcher reinstalado o relanzado con la pantalla apagada)
            avisosCambiaron();
            Borde.de(this).actualizar();   // el asa del Panel Edge vive mientras viva este servicio (siempre)
            mediaMgr = (MediaSessionManager) getSystemService(Context.MEDIA_SESSION_SERVICE);
            componente = new ComponentName(this, IslaOyente.class);
            try {
                mediaMgr.addOnActiveSessionsChangedListener(this, componente, principal);
                revisarSesiones();
            } catch (SecurityException ignored) {} // el agente root concede el permiso con allow_listener
            IntentFilter f = new IntentFilter(android.app.AlarmManager.ACTION_NEXT_ALARM_CLOCK_CHANGED);
            if (ISLA) {   // los demás solo los usa la isla: apagada, ni se despierta por ellos
                f.addAction(Intent.ACTION_POWER_CONNECTED);
                f.addAction(AudioManager.RINGER_MODE_CHANGED_ACTION);
                f.addAction("android.media.VOLUME_CHANGED_ACTION");
                f.addAction(Intent.ACTION_SCREEN_OFF); f.addAction(Intent.ACTION_SCREEN_ON); f.addAction(Intent.ACTION_USER_PRESENT);
            }
            registerReceiver(eventos, f, Context.RECEIVER_EXPORTED);   // los manda el sistema
        });
    }

    @Override public void onListenerDisconnected() {
        principal.post(() -> {
            if (activo == this) activo = null;
            principal.removeCallbacks(apagarIsla);
            if (mediaMgr != null) try { mediaMgr.removeOnActiveSessionsChangedListener(this); } catch (Exception ignored) {}
            try { unregisterReceiver(eventos); } catch (IllegalArgumentException ignored) {}
            desvincularControlador();
            if (isla != null) { isla.destruir(); isla = null; }
        });
        super.onListenerDisconnected();
    }

    long volWindows;
    final java.util.Set<String> privadas = new java.util.TreeSet<>();   // "mic paquete", "cam paquete" en uso

    /** Órdenes del agente (OrdenPanel): micrófono/cámara en uso y volumen puesto por Windows. */
    void delAgente(String m) {
        String[] p = m.split(" ");
        if (p[0].equals("volwin")) { volWindows = android.os.SystemClock.uptimeMillis(); return; }
        if (p[0].equals("gesto") && p.length == 2) { if (isla != null) isla.gesto(p[1]); return; }   // toque en la isla, del visor
        if (p[0].equals("barra") && p.length == 2) { if (isla != null) isla.barra(p[1].equals("oculta")); return; }   // pantalla completa
        if (p[0].equals("manija") && p.length == 2) { if (isla != null) isla.manija(p[1]); return; }   // asa de ventanas de Android
        if (p.length != 3 || !p[0].matches("mic|cam") || !p[1].matches("[A-Za-z0-9_.]+")) return;
        if (p[2].equals("1")) privadas.add(p[0] + " " + p[1]); else privadas.remove(p[0] + " " + p[1]);
        Isla is = isla();
        if (is == null) return;
        if (privadas.isEmpty()) { is.quitar("privacidad"); return; }
        boolean cam = false; String pkg = null;
        for (String s : privadas) { if (s.startsWith("cam")) cam = true; pkg = s.substring(4); }
        Isla.Act a = new Isla.Act();
        a.clave = "privacidad"; a.prio = Isla.P_GRABAR; a.color = cam ? Isla.VERDE : 0xFFFF9F0A;   // como iOS: verde cámara, naranja micrófono
        a.icono = Paneles.de(this).iconoPaquete(pkg);
        a.texto = cam ? Idioma.t("Cámara", "Camera") : Idioma.t("Micrófono", "Microphone");
        a.titulo = Paneles.de(this).etiqueta(pkg) + (cam ? Idioma.t(" usa la cámara", " is using the camera") : Idioma.t(" usa el micrófono", " is using the microphone"));
        is.poner(a);
    }

    // Isla APAGADA (decisión del usuario 18/11): la música se controla desde la tarjeta del panel de ajustes rápidos y,
    // en Windows, desde sus controles multimedia. true la vuelve a poner tal cual estaba (F7/plan/isla_notas.md).
    static final boolean ISLA = false;
    // El visor no debe seguir tomando toques en el rectángulo de una isla de antes: "isla off", reintentando cada 5 s sin agente.
    final Runnable apagarIsla = () -> Inicio.peticionAgente(this, "isla off", null, () -> principal.postDelayed(this.apagarIsla, 5000));

    Isla isla() {
        if (isla == null && ISLA && Settings.canDrawOverlays(this)) isla = new Isla(this);
        return isla;
    }

    // Con el panel de notificaciones abierto, la lista se rehace al llegar o irse avisos (una vez por ráfaga de 150 ms);
    // la isla toma de ellos las actividades en curso.
    final Runnable llenar = () -> {
        if (Paneles.yo != null) Paneles.yo.llenarNoti();
        if (Inicio.instancia != null) Inicio.instancia.refrescarPuntos();   // puntos de aviso en los iconos
        actividades();
    };
    void avisosCambiaron() { principal.removeCallbacks(llenar); principal.postDelayed(llenar, 150); }
    @Override public void onNotificationPosted(StatusBarNotification s) { avisosCambiaron(); if (avisosWin(this)) aWindows(s); }

    // ---- Avisos en Windows (F7/plan/avisos_diseno.md; interruptor de los ajustes rápidos, apagado por omisión). Con Android
    // en pausa el visor reanuda la VM cada 15 min (o a la hora de la próxima alarma de Reloj), pide "pendientes <ms>" y
    // enseña lo que llega: "aviso <id> <app> <título> <texto>" (los tres en base64). Tocarlo en Windows: "abrir <id>".
    static final java.util.concurrent.Executor red = java.util.concurrent.Executors.newSingleThreadExecutor();   // el socket, fuera del hilo principal
    static boolean avisosWin(Context c) { return c.getSharedPreferences("avisos", 0).getBoolean("win", false); }
    static void ponerAvisosWin(Context c, boolean on) {
        c.getSharedPreferences("avisos", 0).edit().putBoolean("win", on).apply();
        Context app = c.getApplicationContext();
        red.execute(() -> alConectar(app));
    }

    /** Al conectar el visor, y al cambiar el interruptor o la alarma: lo que Windows necesita para decidir cuándo despertar. */
    static void alConectar(Context c) {
        AWindows.alVisor("avisos " + (avisosWin(c) ? "on" : "off"));
        android.app.AlarmManager.AlarmClockInfo a = c.getSystemService(android.app.AlarmManager.class).getNextAlarmClock();
        AWindows.alVisor("alarma " + (a == null ? 0 : a.getTriggerTime()));
    }

    // Clave (8 hex) + hora (8 hex): un aviso actualizado se enseña otra vez; "abrir" solo mira la clave (sigue valiendo).
    static String idAviso(StatusBarNotification s) { return String.format("%08x%08x", s.getKey().hashCode(), Long.hashCode(s.getPostTime())); }
    static String b64(CharSequence t, int tope) {
        String x = t == null ? "" : t.toString().trim();
        if (x.isEmpty()) return "-";
        return android.util.Base64.encodeToString(x.substring(0, Math.min(tope, x.length())).getBytes(java.nio.charset.StandardCharsets.UTF_8), android.util.Base64.NO_WRAP);
    }

    /** Solo lo que interrumpiría en Android: ni en curso, ni resúmenes, ni silenciosos, ni secretos, ni lo que calla "no molestar". */
    void aWindows(StatusBarNotification s) {
        Notification n = s.getNotification();
        Ranking r = new Ranking();
        boolean suena = Notification.CATEGORY_ALARM.equals(n.category) || Notification.CATEGORY_CALL.equals(n.category);   // en curso, pero avisan
        if (s.getPackageName().equals(getPackageName()) || n.visibility == Notification.VISIBILITY_SECRET
                || !suena && (s.isOngoing() || (n.flags & Notification.FLAG_FOREGROUND_SERVICE) != 0)
                || (n.flags & Notification.FLAG_GROUP_SUMMARY) != 0
                || n.extras.getString(Notification.EXTRA_TEMPLATE, "").contains("Media")) return;
        try {
            if (!getCurrentRanking().getRanking(s.getKey(), r) || r.getImportance() < android.app.NotificationManager.IMPORTANCE_DEFAULT
                    || !r.matchesInterruptionFilter()) return;
        } catch (RuntimeException e) { return; }
        // Privado (como en la pantalla de bloqueo de Android): su versión pública o solo "Nuevo aviso".
        Notification ver = n.visibility == Notification.VISIBILITY_PRIVATE ? n.publicVersion : n;
        CharSequence titulo = ver == null ? null : ver.extras.getCharSequence(Notification.EXTRA_TITLE);
        CharSequence texto = ver == null ? Idioma.t("Nuevo aviso", "New notification") : ver.extras.getCharSequence(Notification.EXTRA_TEXT);
        CharSequence app = s.getPackageName();
        try { app = getPackageManager().getApplicationLabel(getPackageManager().getApplicationInfo(s.getPackageName(), 0)); }
        catch (android.content.pm.PackageManager.NameNotFoundException ignored) { }
        String l = "aviso " + idAviso(s) + " " + b64(app, 40) + " " + b64(titulo, 80) + " " + b64(texto, 200);
        red.execute(() -> AWindows.alVisor(l));
    }

    /** Líneas del visor (AWindows las corta a 64). */
    static void delVisor(String l) {
        IslaOyente o = activo;
        String[] p = l.split(" ");
        if (o != null && p.length >= 2 && p[0].equals("media")) { o.principal.post(() -> o.mediaDeWindows(p)); return; }
        if (o == null || p.length != 2 || !avisosWin(o)) return;
        boolean pend = p[0].equals("pendientes") && p[1].matches("[0-9]{1,15}");
        if (!pend && !(p[0].equals("abrir") && p[1].matches("[0-9a-f]{16}"))) return;
        StatusBarNotification[] todos;
        try { todos = o.getActiveNotifications(); } catch (RuntimeException e) { return; }
        if (todos == null) return;
        for (StatusBarNotification s : todos) {
            if (pend) { if (s.getPostTime() > Long.parseLong(p[1])) o.aWindows(s); continue; }
            if (!idAviso(s).startsWith(p[1].substring(0, 8))) continue;
            Notification n = s.getNotification();
            try {
                if (n.contentIntent != null) n.contentIntent.send(o, 0, null, null, null, null, android.app.ActivityOptions.makeBasic()
                        .setPendingIntentBackgroundActivityStartMode(android.app.ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED).toBundle());
                if ((n.flags & Notification.FLAG_AUTO_CANCEL) != 0) o.cancelNotification(s.getKey());
            } catch (Exception e) { }   // intent cancelado: Android queda delante y el aviso sigue en su panel
            return;
        }
    }
    @Override public void onNotificationRemoved(StatusBarNotification s) { avisosCambiaron(); }

    /** Actividades en curso a partir de los avisos (las de música las da la sesión multimedia). */
    void actividades() {
        Isla is = isla();
        if (is == null) return;
        StatusBarNotification[] avisos;
        try { avisos = getActiveNotifications(); } catch (RuntimeException e) { return; }
        List<Isla.Act> l = new ArrayList<>();
        if (avisos != null) for (StatusBarNotification s : avisos) {
            Isla.Act a = actividad(s);
            if (a != null) l.add(a);
        }
        is.ponerAvisos(l);
    }

    Isla.Act actividad(StatusBarNotification s) {
        Notification n = s.getNotification();
        Bundle x = n.extras;
        String plantilla = x.getString(Notification.EXTRA_TEMPLATE, ""), cat = n.category, pkg = s.getPackageName();
        if (pkg.equals(getPackageName()) || plantilla.endsWith("$MediaStyle") || plantilla.endsWith("$DecoratedMediaCustomViewStyle")) return null;
        boolean curso = s.isOngoing() || (n.flags & Notification.FLAG_FOREGROUND_SERVICE) != 0;
        boolean crono = x.getBoolean(Notification.EXTRA_SHOW_CHRONOMETER);
        int max = x.getInt(Notification.EXTRA_PROGRESS_MAX), pr = x.getInt(Notification.EXTRA_PROGRESS);
        boolean indet = x.getBoolean(Notification.EXTRA_PROGRESS_INDETERMINATE);
        Isla.Act a = new Isla.Act();
        a.clave = s.getKey();
        a.destacada = (n.flags & FLAG_PROMOVIDA) != 0;
        a.titulo = x.getCharSequence(Notification.EXTRA_TITLE);
        a.texto = x.getCharSequence(Notification.EXTRA_TEXT);
        a.abrir = n.contentIntent;
        a.acciones = n.actions;
        // Vista propia sin título ni texto (el temporizador de Reloj): se lee su cronómetro y su primer texto.
        if (a.titulo == null && a.texto == null && n.contentView != null) leerVistaPropia(n, a);
        if (Notification.CATEGORY_CALL.equals(cat) || plantilla.endsWith("$CallStyle")) {
            a.prio = Isla.P_LLAMADA; a.color = Isla.VERDE;
            a.icono = Paneles.de(this).icono("ic_phone", Isla.VERDE);
            if (crono) { a.crono = true; a.base = n.when; }
            else a.texto = a.titulo;
        } else if (Notification.CATEGORY_NAVIGATION.equals(cat)) {
            a.prio = Isla.P_NAV;
            a.icono = icono(n, pkg);
            CharSequence t = a.titulo; a.titulo = a.texto; a.texto = t;   // compacta: la distancia; desplegada: la calle
        } else if (a.crono || (curso && (crono || Notification.CATEGORY_ALARM.equals(cat) || Notification.CATEGORY_STOPWATCH.equals(cat)))) {
            a.prio = Isla.P_TIEMPO; a.color = 0xFFFF9F0A;
            a.icono = icono(n, pkg);
            if (crono) { a.crono = true; a.base = n.when; a.cuentaAtras = x.getBoolean(Notification.EXTRA_CHRONOMETER_COUNT_DOWN); }
            // Temporizador vencido (sonando o perdido): "Terminado" con sus botones (Detener/Restablecer), no una cuenta negativa.
            if (a.crono && a.cuentaAtras && a.base <= System.currentTimeMillis()) { a.crono = false; a.texto = Idioma.t("Terminado", "Time's up"); a.color = Isla.ROJO; }
        } else if (pkg.equals("com.android.systemui") && n.getChannelId() != null && n.getChannelId().contains("screen_record")) {
            a.prio = Isla.P_GRABAR; a.color = Isla.ROJO;
            a.icono = Paneles.de(this).icono("ic_media_route_connected_light_00_mtrl", Isla.ROJO);
            a.texto = "REC";
        } else if (curso && (max > 0 || indet || plantilla.endsWith("$ProgressStyle"))) {
            a.prio = Isla.P_PROGRESO;
            a.icono = icono(n, pkg);
            a.progreso = max > 0 ? Math.max(0, Math.min(100, 100 * pr / max)) : 0;
            if (a.texto == null) a.texto = a.progreso + Idioma.t(" %", "%");
        } else if (n.getChannelId() != null && n.getChannelId().toLowerCase(java.util.Locale.ROOT).contains("timer") && n.actions != null) {
            // Temporizador en pausa (Reloj): sin cronómetro ni "en curso", pero sigue en la isla con Reanudar/Reiniciar, como en el iPhone.
            // shortcut: se reconoce por el canal "timer" (Reloj de Google y DeskClock); otra app de temporizador, añadir su canal.
            a.prio = Isla.P_TIEMPO; a.color = 0xFFFF9F0A;
            a.icono = icono(n, pkg);
            a.texto = Idioma.t("En pausa", "Paused");
        } else if (a.destacada) {   // otra "Live Update" de Android 16+: se muestra tal cual
            a.prio = Isla.P_PROGRESO;
            a.icono = icono(n, pkg);
        } else return null;
        return a;
    }

    void leerVistaPropia(Notification n, Isla.Act a) {
        try {
            View v = n.contentView.apply(this, new FrameLayout(this));
            java.util.ArrayDeque<View> pila = new java.util.ArrayDeque<>();
            pila.push(v);
            while (!pila.isEmpty()) {
                View w = pila.pop();
                if (w instanceof Chronometer && !a.crono) {
                    Chronometer c = (Chronometer) w;
                    a.crono = true;
                    a.cuentaAtras = c.isCountDown();
                    a.base = System.currentTimeMillis() + (c.getBase() - SystemClock.elapsedRealtime());   // a hora de reloj
                } else if (w instanceof TextView && a.titulo == null && ((TextView) w).getText().length() > 0) a.titulo = ((TextView) w).getText();
                if (w instanceof ViewGroup) for (int i = ((ViewGroup) w).getChildCount() - 1; i >= 0; i--) pila.push(((ViewGroup) w).getChildAt(i));
            }
        } catch (RuntimeException ignored) { }   // vista que no se puede inflar aquí: se queda sin cronómetro
    }

    Drawable icono(Notification n, String pkg) {
        try {
            if (n.getLargeIcon() != null) return n.getLargeIcon().loadDrawable(this);
            if (n.getSmallIcon() != null) { Drawable d = n.getSmallIcon().loadDrawable(this); if (d != null) { d.mutate().setTint(Color.WHITE); return d; } }
        } catch (RuntimeException ignored) { }
        return Paneles.de(this).iconoPaquete(pkg);
    }

    // ---------- multimedia ----------

    @Override public void onActiveSessionsChanged(List<MediaController> controllers) {
        principal.post(() -> actualizarSesiones(controllers));
    }

    void revisarSesiones() {
        if (mediaMgr == null || componente == null) return;
        try { actualizarSesiones(mediaMgr.getActiveSessions(componente)); } catch (SecurityException ignored) {}
    }

    void actualizarSesiones(List<MediaController> lista) {
        MediaController mejor = null;
        if (lista != null) {
            for (MediaController mc : lista) {
                PlaybackState ps = mc.getPlaybackState();
                if (ps != null && ps.getState() == PlaybackState.STATE_PLAYING) { mejor = mc; break; }
            }
            if (mejor == null) for (MediaController mc : lista) {
                PlaybackState ps = mc.getPlaybackState();
                if (ps != null && ps.getState() != PlaybackState.STATE_NONE && ps.getState() != PlaybackState.STATE_STOPPED) { mejor = mc; break; }
            }
            // Todas paradas: se sigue escuchando la primera; si no, al volver a sonar (misma sesión) nadie se enteraba (medido 18/11).
            if (mejor == null && !lista.isEmpty()) mejor = lista.get(0);
        }
        if (mejor == null) {
            desvincularControlador();
            mediaAWindows(false);
            if (isla != null) isla.quitar("media");
            return;
        }
        if (controladorActual == null || !controladorActual.getSessionToken().equals(mejor.getSessionToken())) {
            desvincularControlador();
            controladorActual = mejor;
            controladorActual.registerCallback(cb, principal);
        }
        ponerMedia();
    }

    Bitmap caratulaVista; int colorVisto = Isla.ACENTO;

    void ponerMedia() {
        mediaAWindows(false);
        Isla is = isla();
        MediaController mc = controladorActual;
        if (is == null || mc == null) return;
        PlaybackState ps = mc.getPlaybackState();
        if (ps == null || ps.getState() == PlaybackState.STATE_NONE || ps.getState() == PlaybackState.STATE_STOPPED) { is.quitar("media"); return; }
        MediaMetadata md = mc.getMetadata();
        Isla.Act a = new Isla.Act();
        a.clave = "media"; a.prio = Isla.P_MEDIA; a.mc = mc;
        a.ondas = ps.getState() == PlaybackState.STATE_PLAYING;
        Bitmap cov = caratula(md);
        if (md != null) {
            a.titulo = md.getString(MediaMetadata.METADATA_KEY_TITLE);
            a.texto = md.getString(MediaMetadata.METADATA_KEY_ARTIST);
        }
        if (cov != caratulaVista) { caratulaVista = cov; colorVisto = Isla.colorMedio(cov); }   // una vez por carátula
        a.imagen = cov;
        a.color = colorVisto;
        a.icono = Paneles.de(this).iconoPaquete(mc.getPackageName());
        if (a.titulo == null) a.titulo = Idioma.t("Reproduciendo", "Now playing");
        is.poner(a);
    }

    static Bitmap caratula(MediaMetadata md) {
        if (md == null) return null;
        Bitmap cov = md.getBitmap(MediaMetadata.METADATA_KEY_ART);
        if (cov == null) cov = md.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART);
        if (cov == null && md.getDescription() != null) cov = md.getDescription().getIconBitmap();
        return cov;
    }

    // ---- Música de Android en los controles multimedia de Windows (panel de volumen, pantalla de bloqueo, teclas y
    // auriculares; MusicaCompartida en Enlace.cs). Al visor por 4445, solo al cambiar: "media <play|pause|off> <pos ms> <dur ms>
    // <título> <artista> [<carátula JPEG>]" (base64 o "-"; la carátula solo cuando cambia). Del visor: "media play|pause|next|
    // prev|seek <ms>" y "media ?" (mandarlo todo: recién conectado o tras pausar la VM). En el hilo principal.
    String mediaWin = ""; Bitmap caratulaWin; long posWin, horaWin;

    void mediaAWindows(boolean todo) {
        MediaController mc = controladorActual;
        PlaybackState ps = mc == null ? null : mc.getPlaybackState();
        int st = ps == null ? PlaybackState.STATE_NONE : ps.getState();
        String estado = st == PlaybackState.STATE_PAUSED ? "pause"
                : st == PlaybackState.STATE_NONE || st == PlaybackState.STATE_STOPPED || st == PlaybackState.STATE_ERROR ? "off" : "play";
        MediaMetadata md = estado.equals("off") ? null : mc.getMetadata();
        long ahora = SystemClock.elapsedRealtime(), dur = md == null ? 0 : Math.max(0, md.getLong(MediaMetadata.METADATA_KEY_DURATION)), pos = 0;
        if (!estado.equals("off")) {
            pos = ps.getPosition();
            if (estado.equals("play") && ps.getLastPositionUpdateTime() > 0) pos += (long) ((ahora - ps.getLastPositionUpdateTime()) * ps.getPlaybackSpeed());
            pos = Math.max(0, dur > 0 ? Math.min(pos, dur) : pos);
        }
        Bitmap cov = caratula(md);
        String t = md == null ? "-" : b64(md.getString(MediaMetadata.METADATA_KEY_TITLE), 120);
        String a = md == null ? "-" : b64(md.getString(MediaMetadata.METADATA_KEY_ARTIST), 120);
        String clave = estado + " " + dur + " " + t + " " + a;
        // getMetadata() trae otro Bitmap en cada llamada (binder): se comparan los píxeles, no el objeto.
        boolean otraCaratula = cov == null ? caratulaWin != null : caratulaWin == null || !cov.sameAs(caratulaWin);
        long previsto = posWin + (mediaWin.startsWith("play") ? ahora - horaWin : 0);
        if (!todo && !otraCaratula && clave.equals(mediaWin) && Math.abs(pos - previsto) < 2000) return;   // solo el reloj avanzó
        boolean conCaratula = todo || otraCaratula;
        mediaWin = clave; caratulaWin = cov; posWin = pos; horaWin = ahora;
        String l = "media " + estado + " " + pos + " " + clave.substring(estado.length() + 1);
        red.execute(() -> AWindows.alVisor(conCaratula ? l + " " + jpeg(cov) : l));
    }

    /** JPEG de hasta 300 px y 48 KB (64 KB en base64) para Windows; "-" si no hay o no cabe. Fuera del hilo principal. */
    static String jpeg(Bitmap b) {
        if (b == null) return "-";
        try {
            float k = Math.min(1f, 300f / Math.max(b.getWidth(), b.getHeight()));
            Bitmap e = k < 1 ? Bitmap.createScaledBitmap(b, Math.max(1, Math.round(b.getWidth() * k)), Math.max(1, Math.round(b.getHeight() * k)), true) : b;
            java.io.ByteArrayOutputStream o = new java.io.ByteArrayOutputStream();
            try {
                for (int q = 85; q >= 45; q -= 20) {
                    o.reset();
                    if (e.compress(Bitmap.CompressFormat.JPEG, q, o) && o.size() <= 48 * 1024) return android.util.Base64.encodeToString(o.toByteArray(), android.util.Base64.NO_WRAP);
                }
            } finally { if (e != b) e.recycle(); }
        } catch (RuntimeException | OutOfMemoryError ignored) { }   // carátula enorme o rara: sin carátula
        return "-";
    }

    void mediaDeWindows(String[] p) {
        if (p.length == 2 && p[1].equals("?")) { mediaAWindows(true); return; }
        MediaController mc = controladorActual;
        if (mc == null) return;
        MediaController.TransportControls c = mc.getTransportControls();
        if (p.length == 3 && p[1].equals("seek") && p[2].matches("[0-9]{1,12}")) c.seekTo(Long.parseLong(p[2]));
        else if (p.length != 2) return;
        else if (p[1].equals("play")) c.play();
        else if (p[1].equals("pause")) c.pause();
        else if (p[1].equals("next")) c.skipToNext();
        else if (p[1].equals("prev")) c.skipToPrevious();
    }

    void desvincularControlador() {
        if (controladorActual != null) {
            try { controladorActual.unregisterCallback(cb); } catch (Exception ignored) {}
            controladorActual = null;
        }
    }

    @Override public void onDestroy() {
        if (activo == this) activo = null;
        if (mediaMgr != null) try { mediaMgr.removeOnActiveSessionsChangedListener(this); } catch (Exception ignored) {}
        try { unregisterReceiver(eventos); } catch (IllegalArgumentException ignored) {}
        desvincularControlador();
        if (isla != null) { isla.destruir(); isla = null; }
        super.onDestroy();
    }
}
