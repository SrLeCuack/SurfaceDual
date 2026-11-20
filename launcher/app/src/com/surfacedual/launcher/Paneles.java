package com.surfacedual.launcher;

import android.animation.LayoutTransition;
import android.app.ActivityOptions;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.PlaybackState;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.provider.Settings;
import android.service.notification.StatusBarNotification;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Log;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.animation.PathInterpolator;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextClock;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Paneles de tablet con esencia Windows 11 (F7/plan/interfaz_a.md y _b.md, fotos F7/referencia_tab): ajustes rápidos
 * (arriba a la derecha), notificaciones (arriba a la izquierda), multitarea y cajón de apps. Ventanas superpuestas como
 * la Isla: salen encima de cualquier app sin cambiar de actividad. El cajón vive en el inicio (Inicio.abrirCajon). Fluent: acrílico oscuro, radios 16–28 dp, acento
 * #60CDFF, Selawik, entrada 200 ms con curva de desaceleración y salida 140 ms. Sin animaciones continuas.
 */
class Paneles {
    static final String TAG = "paneles";
    static final int ACENTO = Inicio.ACENTO, TEXTO2 = Inicio.TEXTO2, TEXTO3 = 0x8AFFFFFF, SUPERFICIE = 0x12FFFFFF,
            BORDE = 0x1FFFFFFF, APAGADO = 0x1FFFFFFF;
    static Paneles yo;
    static String ultimasTareas = "";                        // "tarea:paquete,..." (la última respuesta del agente)
    static final Map<String, Bitmap> miniaturas = new HashMap<>();
    static final Map<String, Long> fechas = new HashMap<>();   // fecha del archivo de cada miniatura ya decodificada
    static final java.util.concurrent.ExecutorService decodificador = java.util.concurrent.Executors.newSingleThreadExecutor();

    static Paneles de(Context c) {
        if (yo == null) yo = new Paneles(c.getApplicationContext());
        return yo;
    }

    final Context ctx;
    final WindowManager wm;
    final Handler principal = new Handler(Looper.getMainLooper());
    final PathInterpolator entrada = new PathInterpolator(0.1f, 0.9f, 0.2f, 1f), salida = new PathInterpolator(0.7f, 0f, 1f, 0.5f);
    FrameLayout raiz;
    View saliendo;           // la ventana que se está cerrando (se quita ya si se abre otra)
    String abierto;
    View tarjeta;            // lo que se desplaza al salir
    float salidaY;
    LinearLayout listaNoti, rejillaTareas;
    String pintado;          // la lista de tareas que ya está dibujada (no se rehacen las tarjetas si no cambió)

    /**
     * Una ventana por panel, hecha una vez (al arrancar el inicio) y que queda puesta pero invisible: abrir solo refresca
     * los datos, la hace visible y repite las animaciones de entrada. Antes cada apertura creaba la ventana (superficie
     * nueva de 2880x1920) y todas las vistas: "Slow UI thread" y tirones al abrir (medido 17/11).
     */
    static class Hecho {
        FrameLayout raiz; View tarjeta; float salidaY; LinearLayout listaNoti, rejillaTareas;
        final List<Runnable> alAbrir = new ArrayList<>();
        Runnable refrescar;
    }
    final Map<String, Hecho> hechos = new HashMap<>();
    Hecho armando;   // el que se está construyendo: entrar() y velo() le apuntan sus animaciones de entrada
    String clave;    // tamaño de pantalla y dock con que se hicieron (Inicio.mostrar)

    /** Deja hechas las tres ventanas sin mostrarlas (llamado desde el inicio, ya dibujado). */
    void precalentar() {
        for (String c : new String[] { "qs", "noti", "recientes" }) if (Settings.canDrawOverlays(ctx)) hecho(c);
    }

    /** Al girar o cambiar el dock: se rehacen la próxima vez. */
    void olvidar() {
        if (raiz != null) cerrar();
        for (Hecho h : hechos.values()) quitar(h.raiz);
        hechos.clear();
        pintado = null;
    }

    Paneles(Context c) {
        ctx = new ContextThemeWrapper(c, android.R.style.Theme_DeviceDefault);
        wm = ctx.getSystemService(WindowManager.class);
        Inicio.fuentes(c);
        assert paquetes("39:com.a.b,7:c.d").equals(Arrays.asList("com.a.b", "c.d")) && paquetes("").isEmpty() && paquetes(" ").isEmpty();
        assert redDe("wifi 4d6920c3b1 80 gs")[0].equals("Mi ñ") && redDe("wifi 4d6 80 gs") == null && redDe("wifi 4d 80 xs") == null && redDe("wifi  80 gs") == null && redDe("wifi 4d 101 gs") == null;
    }

    // ---------- abrir y cerrar ----------

    // ---------- cortina: bajar desde arriba siguiendo el dedo ----------

    static final Handler HILO = new Handler(Looper.getMainLooper());
    boolean siguiendo;   // la tarjeta va con el dedo (entre el primer "tiron" y el soltar)
    int dyCortina;       // lo último que bajó el dedo

    /** Del visor por 4445: "tiron noti|qs <px bajados> [<px/s al soltar>]". Validado aquí: viene de la red. */
    static void delVisor(Context c, String l) {
        if (!l.matches("tiron (noti|qs) -?[0-9]{1,5}( -?[0-9]{1,6})?")) return;
        String[] p = l.split(" ");
        HILO.post(() -> {
            Paneles yo = de(c); boolean suelta = p.length == 4;
            yo.tiron(p[1], Integer.parseInt(p[2]), suelta, suelta ? Integer.parseInt(p[3]) : 0);
            HILO.removeCallbacks(yo.caducar);
            if (!suelta) HILO.postDelayed(yo.caducar, 1000);   // solo el visor: su soltar puede perderse (el del inicio no)
        });
    }

    // Sin noticias del dedo en 1 s (visor cerrado o reconectado, línea perdida): se suelta donde esté, nunca queda a medias.
    final Runnable caducar = () -> { if (siguiendo && abierto != null) tiron(abierto, dyCortina, true, 0); };

    /**
     * Como la cortina de Android (pedido 18/11): la tarjeta sale de arriba y su borde de abajo va con el dedo (1:1); al
     * soltar termina de abrirse si bajó un tercio o se lanzó hacia abajo, y si no vuelve a subir.
     */
    void tiron(String cual, int dy, boolean suelta, int vel) {
        if (!siguiendo) {
            // ya abierto (bajar otra vez no hace nada; cerrar es tocar fuera) o soltar sin arrastre en curso
            if (cual.equals(abierto) || suelta) return;
            alternar(cual);
            if (!cual.equals(abierto) || tarjeta == null) return;
            siguiendo = true;
            tarjeta.animate().cancel();
            tarjeta.setAlpha(1f); tarjeta.setScaleX(1f); tarjeta.setScaleY(1f);
            // refrescar() pudo cambiar su alto: tras medirla se recoloca aunque el dedo esté quieto
            View tj = tarjeta;
            tj.getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
                @Override public boolean onPreDraw() {   // ya medida: antes del primer cuadro visible
                    tj.getViewTreeObserver().removeOnPreDrawListener(this);
                    if (siguiendo && tj == tarjeta) tiron(cual, dyCortina, false, 0);
                    return true;
                }
            });
        }
        View t = tarjeta;
        if (t == null || !cual.equals(abierto)) { siguiendo = false; return; }
        dyCortina = dy;
        float oculto = -Math.max(t.getBottom(), dp(200));   // fuera de la pantalla por arriba (sin medir aún: 200 dp)
        float y = Math.min(0f, oculto + dy);
        if (!suelta) { t.setTranslationY(y); return; }
        siguiendo = false;
        if (vel > 800 || vel > -300 && y > oculto * 2 / 3) {
            long ms = (long) Math.max(120, Math.min(260, -y * 1000 / Math.max(1200, vel)));
            t.animate().translationY(0f).setDuration(ms).setInterpolator(entrada);
        } else {
            t.animate().cancel(); t.setTranslationY(y);
            salidaY = oculto;   // cerrar() la sube del todo mientras se funde
            cerrar();
        }
    }

    void alternar(String cual) {
        siguiendo = false; HILO.removeCallbacks(caducar);   // la caducidad de un arrastre anterior no toca a este
        long pedido = SystemClock.uptimeMillis();
        if (cual.equals(abierto)) { cerrar(); return; }
        if (cual.equals("cajon")) {
            // El cajón es parte del inicio (así una app se arrastra del cajón al inicio): se trae el inicio con el cajón abierto.
            if (raiz != null) cerrar();
            { Inicio.panelPedido = "cajon"; ctx.startActivity(new Intent(ctx, Inicio.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
            return;
        }
        if (cual.equals("recientes") && Inicio.instancia == null) {
            // shortcut: sin el inicio vivo no hay lista de apps ni dock; se abre el inicio y él pide el panel (Inicio.panelDe).
            // Si se nota, cargar aquí las apps.
            { Inicio.panelPedido = cual; ctx.startActivity(new Intent(ctx, Inicio.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
            return;
        }
        if (Inicio.instancia != null) Inicio.instancia.asentar();   // páginas o Discover a medio deslizar: a su sitio
        if (raiz != null) ocultar(raiz);   // otro panel abierto: el nuevo lo sustituye sin esperar su salida
        if (saliendo != null) ocultar(saliendo);   // reabierto antes de 140 ms
        saliendo = null;
        raiz = null; abierto = null; listaNoti = null; rejillaTareas = null;
        if (!Settings.canDrawOverlays(ctx)) return;
        Hecho h = hecho(cual);
        if (h == null) return;
        abierto = cual; raiz = h.raiz; tarjeta = h.tarjeta; salidaY = h.salidaY; listaNoti = h.listaNoti; rejillaTareas = h.rejillaTareas;
        islaRepasa();
        if (h.refrescar != null) h.refrescar.run();
        for (Runnable a : h.alAbrir) a.run();
        FrameLayout r = raiz;
        r.animate().cancel();
        r.setAlpha(1f);
        mostrar(r, true);
        r.requestFocus();
        r.getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
            @Override public boolean onPreDraw() {
                r.getViewTreeObserver().removeOnPreDrawListener(this);
                Log.i(TAG, cual + " dibujado a los " + (SystemClock.uptimeMillis() - pedido) + " ms de la orden");
                return true;
            }
        });
    }

    /** La ventana del panel: la ya hecha o, la primera vez, una nueva (puesta e invisible). */
    Hecho hecho(String cual) {
        Hecho h = hechos.get(cual);
        if (h != null) return h;
        h = armando = new Hecho();
        FrameLayout r = raiz = new FrameLayout(ctx) {
            @Override public boolean dispatchKeyEvent(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.KEYCODE_BACK || e.getKeyCode() == KeyEvent.KEYCODE_ESCAPE) {
                    if (e.getAction() == KeyEvent.ACTION_UP) cerrar();
                    return true;
                }
                return super.dispatchKeyEvent(e);
            }
            @Override public boolean onTouchEvent(MotionEvent e) {   // toque fuera de las tarjetas (ellas son pulsables)
                if (e.getActionMasked() == MotionEvent.ACTION_UP) cerrar();
                return true;
            }
            // El fundido de salida se aplica a cada hijo: sin capa intermedia de 2880x1920 (la GPU virtual va justa, 17/11).
            @Override public boolean hasOverlappingRendering() { return false; }
        };
        r.setFocusableInTouchMode(true);
        if (cual.equals("qs")) armarQs();
        else if (cual.equals("noti")) armarNoti();
        else armarRecientes();
        // Pantalla completa también para ajustes y avisos: el toque de fuera cierra y no llega a la app de debajo (como la
        // cortina de Android).
        WindowManager.LayoutParams p = new WindowManager.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN | WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
                PixelFormat.TRANSLUCENT);
        p.setFitInsetsTypes(0);   // de borde a borde: la barra de estado queda encima, transparente
        p.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        p.softInputMode = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING;   // el buscador está arriba: el teclado no lo tapa
        p.setTitle("SurfaceDual " + cual);
        h.raiz = r; h.tarjeta = tarjeta; h.salidaY = salidaY; h.listaNoti = listaNoti; h.rejillaTareas = rejillaTareas;
        armando = null; raiz = null; tarjeta = null; listaNoti = null; rejillaTareas = null;
        // Nace oculta (ver mostrar): con su superficie hecha y un primer dibujo, sin verse ni recibir toques.
        if (VIVO) { p.alpha = 0f; p.flags |= WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE; }
        else r.setVisibility(View.INVISIBLE);
        try { wm.addView(r, p); } catch (RuntimeException e) { Log.w(TAG, "sin ventana", e); return null; }
        hechos.put(cual, h);
        return h;
    }

    void cerrar() {
        FrameLayout r = raiz;
        if (r == null) return;
        raiz = null; abierto = null; listaNoti = null; rejillaTareas = null; siguiendo = false; HILO.removeCallbacks(caducar);
        saliendo = r;
        islaRepasa();
        r.animate().cancel();
        r.animate().alpha(0f).setDuration(140).setInterpolator(salida).withEndAction(() -> {
            ocultar(r);
            if (saliendo == r) saliendo = null;
        });
        if (tarjeta != null) tarjeta.animate().translationY(salidaY).scaleX(.97f).scaleY(.97f).setDuration(140).setInterpolator(salida);
        tarjeta = null;
    }

    /** Con un panel abierto la isla se esconde (y el visor deja de tomar sus toques), como bajo la cortina de Android. */
    void islaRepasa() { IslaOyente o = IslaOyente.activo; if (o != null && o.isla != null) o.isla.aplicarVisibilidad(); }

    void ocultar(View r) {
        r.animate().cancel();
        mostrar(r, false);
        r.setAlpha(1f);
    }

    /**
     * Muestra u oculta la ventana de un panel SIN soltar su superficie: oculta = alfa de ventana 0 (SurfaceFlinger no la
     * compone), sin toques ni foco. Con View.INVISIBLE Android destruía la superficie y el primer cuadro de cada apertura
     * reservaba búferes de 22 MB (medido 17/11: el único cuadro tardío de cada animación, 8 de 11 aperturas).
     * setprop debug.sd.launcher.panel_vivo 0 (y reiniciar el launcher) = como antes (A/B).
     */
    void mostrar(View r, boolean si) {
        if (!VIVO) { r.setVisibility(si ? View.VISIBLE : View.INVISIBLE); return; }
        WindowManager.LayoutParams p = (WindowManager.LayoutParams) r.getLayoutParams();
        int f = WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        p.alpha = si ? 1f : 0f;
        p.flags = si ? p.flags & ~f : p.flags | f;
        try { wm.updateViewLayout(r, p); } catch (IllegalArgumentException e) { Log.w(TAG, "ventana ya quitada", e); }
    }
    static final boolean VIVO = !Widgets.prop("debug.sd.launcher.panel_vivo").equals("0");

    void quitar(View r) {
        r.animate().cancel();
        try { wm.removeViewImmediate(r); } catch (IllegalArgumentException ignored) { }
    }

    /** Entrada Fluent: sube (o baja) y aparece con desaceleración, en una capa de GPU mientras dura. */
    void entrar(View v, float desdeY, float escala, long retraso) {
        if (armando != null) armando.alAbrir.add(() -> entrar(v, desdeY, escala, retraso));   // se repite en cada apertura
        v.setAlpha(0f); v.setTranslationY(desdeY); v.setScaleX(escala); v.setScaleY(escala);
        v.animate().alpha(1f).translationY(0f).scaleX(1f).scaleY(1f).setStartDelay(retraso).setDuration(200).setInterpolator(entrada).withLayer();
    }

    /**
     * Velo muy suave detrás de ajustes y avisos: dice "toca fuera para cerrar" sin tapar la app. (Medido 17/11: pasarlo a
     * FLAG_DIM_BEHIND de SurfaceFlinger no mejora los tirones —SF compone la pantalla entera igual—; se queda así.)
     */
    void velo() {
        View v = new View(ctx);
        v.setBackgroundColor(0x33000000);
        raiz.addView(v, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        Runnable fundir = () -> { v.setAlpha(0f); v.animate().alpha(1f).setDuration(167).setInterpolator(entrada); };
        fundir.run();
        if (armando != null) armando.alAbrir.add(fundir);
    }

    /** Tarjeta flotante de acrílico oscuro (arriba a la izquierda o a la derecha). */
    LinearLayout tarjeta(int ancho, int gravedad) {
        LinearLayout t = new LinearLayout(ctx);
        t.setOrientation(LinearLayout.VERTICAL);
        t.setPadding(dp(18), dp(16), dp(18), dp(18));
        t.setBackground(new Vidrio.Placa(t, dp(24), 0xB31C1C21));   // el fondo difuminado detrás (foto N)
        t.setElevation(dp(24));
        t.setClickable(true);   // los toques dentro no llegan a la raíz (que cierra)
        FrameLayout.LayoutParams l = new FrameLayout.LayoutParams(ancho, ViewGroup.LayoutParams.WRAP_CONTENT, gravedad);
        l.topMargin = barraEstado() + dp(8);
        l.leftMargin = l.rightMargin = dp(16);
        raiz.addView(t, l);
        t.setPivotX((gravedad & Gravity.END) == Gravity.END ? ancho : 0);
        t.setPivotY(0);
        entrar(t, -dp(16), .96f, 0);
        tarjeta = t;
        salidaY = -dp(10);
        return t;
    }

    // ---------- ajustes rápidos ----------

    void armarQs() {
        velo();
        LinearLayout t = tarjeta(dp(400), Gravity.TOP | Gravity.END);

        LinearLayout cab = fila();
        TextClock hora = new TextClock(ctx);
        hora.setFormat12Hour("h:mm");
        hora.setFormat24Hour("H:mm");
        estilo(hora, 26, true, Color.WHITE);
        cab.addView(hora);
        TextClock fecha = new TextClock(ctx);
        fecha.setFormat12Hour(Idioma.t("EEE d 'de' MMM", "EEE, MMM d"));
        fecha.setFormat24Hour(Idioma.t("EEE d 'de' MMM", "EEE, MMM d"));
        estilo(fecha, 14, false, TEXTO2);
        fecha.setPadding(dp(10), dp(6), 0, 0);
        cab.addView(fecha, peso());
        // shortcut: "editar" aún no abre nada (orden y elección de botones pendiente); cuando haga falta, una lista arrastrable.
        cab.addView(botonIcono("ic_mode_edit", 40, v -> Toast.makeText(ctx, Idioma.t("Editar los ajustes rápidos: próximamente", "Editing quick settings: coming soon"), Toast.LENGTH_SHORT).show()));
        cab.addView(botonIcono("ic_lock_power_off", 40, v -> {
            cerrar();
            AlertDialog d = AWindows.dialogo(ctx);
            d.getWindow().setType(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY);
            d.show();
        }));
        cab.addView(botonIcono("ic_settings_24dp", 40, v -> abrirAjustes(Settings.ACTION_SETTINGS)));
        t.addView(cab, margen(0, 0, 0, dp(14)));

        WifiManager wifi = ctx.getSystemService(WifiManager.class);
        PowerManager pm = ctx.getSystemService(PowerManager.class);
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        LinearLayout anchos = fila();
        // Wi-Fi: el de Windows (el agente lo sabe); Android sale a Internet por la red de la VM, así que con Internet se ve
        // "Conectado" (17/11: decía "Desactivado" estando en línea, porque Android no tiene Wi-Fi propio).
        android.net.ConnectivityManager cm = ctx.getSystemService(android.net.ConnectivityManager.class);
        Interruptor w = new Interruptor("Wi-Fi", "ic_wifi_signal_4", true, false, (on, mal) -> agente("ajuste wifi " + (on ? "on" : "off"), mal));
        Interruptor bt = new Interruptor("Bluetooth", "ic_qs_bluetooth", true, false, (on, mal) -> agente("ajuste bt " + (on ? "on" : "off"), mal));
        // Mantener pulsado: las redes de Windows (pedido 19/11), que es por donde sale Android. Al abrir, el nombre de la conectada.
        w.vista.setOnLongClickListener(v -> { redesWifi(); return true; });
        botonWifi = w;
        anchos.addView(w.vista, peso());
        anchos.addView(bt.vista, margen(dp(10), 0, 0, 0, 1));
        t.addView(anchos);
        // Avisos de Android en Windows mientras Android está en pausa (apagado = pausa total, como siempre).
        Interruptor avisos = new Interruptor(Idioma.t("Avisos en Windows", "Alerts on Windows"), "ic_notifications_alerted", true, IslaOyente.avisosWin(ctx), (on, mal) -> IslaOyente.ponerAvisosWin(ctx, on));
        // Modo lectura (ESTADO 116): el agente guarda lo que había y lo restaura al quitarlo. Oculto hasta que el agente diga que lo conoce.
        Interruptor lectura = new Interruptor(Idioma.t("Modo lectura", "Reading mode"), "ic_menu_view", true, false, (on, mal) -> agente("lectura " + (on ? "on" : "off"), mal));
        lectura.vista.setVisibility(View.GONE);
        LinearLayout anchos2 = fila();
        anchos2.addView(avisos.vista, peso());
        anchos2.addView(lectura.vista, margen(dp(10), 0, 0, 0, 1));
        t.addView(anchos2, lleno(dp(10)));

        LinearLayout cuadros = fila();
        cuadros.setBackground(fondo(SUPERFICIE, 0, 20));
        cuadros.setPadding(dp(6), dp(12), dp(6), dp(10));
        Interruptor girar = new Interruptor(Idioma.t("Girar", "Auto-rotate"), "ic_qs_auto_rotate", false, false, (on, mal) -> {
            try { Settings.System.putInt(ctx.getContentResolver(), Settings.System.ACCELEROMETER_ROTATION, on ? 1 : 0); }
            catch (RuntimeException e) { mal.run(); }   // sin WRITE_SETTINGS (lo concede el agente)
        });
        Interruptor avion = new Interruptor(Idioma.t("Modo avión", "Airplane mode"), "ic_qs_airplane", false, false, (on, mal) -> agente("ajuste avion " + (on ? "on" : "off"), mal));
        Interruptor ahorro = new Interruptor(Idioma.t("Ahorro", "Battery saver"), "ic_qs_battery_saver", false, false, (on, mal) -> agente("ajuste ahorro " + (on ? "on" : "off"), mal));
        Interruptor dnd = new Interruptor(Idioma.t("No molestar", "Do not disturb"), "ic_qs_dnd", false, false, (on, mal) -> {
            // el oyente de notificaciones puede pedirlo sin el permiso de política de "no molestar"
            if (IslaOyente.activo == null) { mal.run(); return; }
            IslaOyente.activo.requestInterruptionFilter(on ? NotificationManager.INTERRUPTION_FILTER_PRIORITY : NotificationManager.INTERRUPTION_FILTER_ALL);
        });
        cuadros.addView(girar.vista, peso());
        cuadros.addView(avion.vista, peso());
        cuadros.addView(ahorro.vista, peso());
        cuadros.addView(dnd.vista, peso());
        t.addView(cuadros, lleno(dp(10)));
        t.addView(botonWindows(), margen(0, dp(10), 0, 0, 0, ViewGroup.LayoutParams.MATCH_PARENT, dp(60)));

        LinearLayout barras = new LinearLayout(ctx);
        barras.setOrientation(LinearLayout.VERTICAL);
        barras.setBackground(fondo(SUPERFICIE, 0, 20));
        barras.setPadding(dp(14), dp(6), dp(14), dp(6));
        // Brillo y volumen: los de Windows. El agente los oye cambiar en Android y los manda al visor.
        int[] ultimo = {-1, -1};
        Deslizador brillo = new Deslizador(ctx, new Sol(), 0f, v -> {
            int b = Math.round(v * 255);
            if (b == ultimo[0]) return;
            ultimo[0] = b;
            try { Settings.System.putInt(ctx.getContentResolver(), Settings.System.SCREEN_BRIGHTNESS, Math.max(1, b)); } catch (RuntimeException ignored) { }
        });
        barras.addView(brillo, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(48)));
        AudioManager am = ctx.getSystemService(AudioManager.class);
        int max = Math.max(1, am.getStreamMaxVolume(AudioManager.STREAM_MUSIC));
        Runnable[] bocina = {null};
        Deslizador volumen = new Deslizador(ctx, icono("ic_volume", Color.WHITE), 0f, v -> {
            int i = Math.round(v * max);
            if (i == ultimo[1]) return;
            ultimo[1] = i;
            am.setStreamVolume(AudioManager.STREAM_MUSIC, i, 0);
            bocina[0].run();
        });
        // La bocina silencia de verdad (mute de Android): el deslizador se queda donde estaba; moverlo quita el silencio.
        bocina[0] = () -> volumen.icono = icono(am.isStreamMute(AudioManager.STREAM_MUSIC) ? "ic_audio_vol_mute" : "ic_volume", Color.WHITE);
        volumen.alIcono = () -> {
            am.adjustStreamVolume(AudioManager.STREAM_MUSIC, am.isStreamMute(AudioManager.STREAM_MUSIC) ? AudioManager.ADJUST_UNMUTE : AudioManager.ADJUST_MUTE, 0);
            bocina[0].run();
        };
        barras.addView(volumen, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(48)));
        t.addView(barras, lleno(dp(10)));

        // Al abrir (la ventana ya está hecha) solo se leen los estados; la fila de música se rehace solo si cambió la app.
        View[] media = {null};
        MediaController[] mcVisto = {null};
        armando.refrescar = () -> {
            android.net.NetworkCapabilities nc = cm == null ? null : cm.getNetworkCapabilities(cm.getActiveNetwork());
            w.conectado = nc != null && nc.hasCapability(android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED);
            w.poner(w.conectado || (wifi != null && wifi.isWifiEnabled()));
            alVisor("wifi lista", false);
            Inicio.peticionAgente(ctx, "estado", r -> {   // los radios de Windows
                bt.poner(r.contains("bt=on"));
                if (r.contains("wifi=on") || r.contains("wifi=off")) w.poner(r.contains("wifi=on"));
                lectura.vista.setVisibility(r.contains(" lectura=") ? View.VISIBLE : View.GONE);
                lectura.poner(r.contains(" lectura=on"));
            }, null);
            girar.poner(Settings.System.getInt(ctx.getContentResolver(), Settings.System.ACCELEROMETER_ROTATION, 0) == 1);
            avion.poner(Settings.Global.getInt(ctx.getContentResolver(), Settings.Global.AIRPLANE_MODE_ON, 0) == 1);
            ahorro.poner(pm != null && pm.isPowerSaveMode());
            dnd.poner(nm != null && nm.getCurrentInterruptionFilter() > NotificationManager.INTERRUPTION_FILTER_ALL);
            ultimo[0] = ultimo[1] = -1;
            brillo.poner(Settings.System.getInt(ctx.getContentResolver(), Settings.System.SCREEN_BRIGHTNESS, 128) / 255f);
            // En silencio Android informa volumen 0: se deja el deslizador donde estaba.
            if (!am.isStreamMute(AudioManager.STREAM_MUSIC)) volumen.poner(am.getStreamVolume(AudioManager.STREAM_MUSIC) / (float) max);
            bocina[0].run();
            MediaController mc = IslaOyente.activo == null ? null : IslaOyente.activo.controladorActual;
            if (mc != mcVisto[0]) {
                mcVisto[0] = mc;
                if (media[0] != null) t.removeView(media[0]);
                media[0] = mc == null ? null : filaMedia(mc);
                if (media[0] != null) t.addView(media[0], lleno(dp(10)));
            }
        };
    }

    /** "Cambiar a Windows" grande (pedido 7): la misma acción que el botón de antes del inicio (AWindows.dialogo). */
    View botonWindows() {
        LinearLayout b = fila();
        b.setGravity(Gravity.CENTER);
        GradientDrawable g = new GradientDrawable(GradientDrawable.Orientation.TL_BR, new int[]{0xFF7AD7FF, ACENTO, 0xFF3A9BE0});
        g.setCornerRadius(dp(20));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33003049), g, null));
        b.setElevation(dp(4));
        ImageView logo = new ImageView(ctx);
        logo.setImageDrawable(new Logo(0xFF003049));
        b.addView(logo, new LinearLayout.LayoutParams(dp(22), dp(22)));
        TextView t = texto(Idioma.t("Cambiar a Windows", "Switch to Windows"), 16, true, 0xFF003049);
        t.setPadding(dp(12), 0, 0, 0);
        b.addView(t);
        b.setContentDescription(Idioma.t("Cambiar a Windows", "Switch to Windows"));
        b.setOnClickListener(v -> {
            cerrar();
            AlertDialog d = AWindows.dialogo(ctx);
            d.getWindow().setType(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY);
            d.show();
        });
        return b;
    }

    /** Logotipo de Windows: cuatro cuadros. */
    static class Logo extends Drawable {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        Logo(int color) { p.setColor(color); }
        @Override public void draw(Canvas c) {
            android.graphics.Rect r = getBounds();
            float h = r.width() * .06f, m = r.width() / 2f;
            c.drawRect(r.left, r.top, r.left + m - h, r.top + m - h, p);
            c.drawRect(r.left + m + h, r.top, r.right, r.top + m - h, p);
            c.drawRect(r.left, r.top + m + h, r.left + m - h, r.bottom, p);
            c.drawRect(r.left + m + h, r.top + m + h, r.right, r.bottom, p);
        }
        @Override public void setAlpha(int a) { p.setAlpha(a); }
        @Override public void setColorFilter(android.graphics.ColorFilter f) { p.setColorFilter(f); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    interface Cambio { void hacer(boolean on, Runnable deshacer); }

    /** Botón de ajuste: ancho (icono en círculo y estado, como Wi-Fi en One UI) o cuadro con nombre debajo (Windows 11). */
    class Interruptor {
        final LinearLayout vista;
        final GradientDrawable forma;
        final ImageView ic;
        TextView estado;
        boolean on, conectado;   // conectado: con Internet dice "Conectado" en vez de "Activado"
        String red;              // Wi-Fi: el nombre de la red de Windows, si se sabe

        Interruptor(String nombre, String icono, boolean ancho, boolean inicial, Cambio cambio) {
            vista = new LinearLayout(ctx);
            forma = new GradientDrawable();
            ic = new ImageView(ctx);
            ic.setImageDrawable(icono(icono, Color.WHITE));
            ic.setScaleType(ImageView.ScaleType.FIT_CENTER);
            FrameLayout caja = new FrameLayout(ctx);
            caja.setBackground(forma);
            caja.setDuplicateParentStateEnabled(true);   // la onda del toque se ve en el cuadro
            int lado = dp(ancho ? 22 : 24);
            caja.addView(ic, new FrameLayout.LayoutParams(lado, lado, Gravity.CENTER));
            if (ancho) {
                vista.setGravity(Gravity.CENTER_VERTICAL);
                vista.setPadding(dp(10), dp(10), dp(12), dp(10));
                vista.setBackground(presionable(fondo(SUPERFICIE, 0, 20)));
                forma.setShape(GradientDrawable.OVAL);
                vista.addView(caja, new LinearLayout.LayoutParams(dp(42), dp(42)));
                LinearLayout textos = new LinearLayout(ctx);
                textos.setOrientation(LinearLayout.VERTICAL);
                textos.addView(texto(nombre, 14, true, Color.WHITE));
                estado = texto("", 12, false, TEXTO2);
                textos.addView(estado);
                vista.addView(textos, margen(dp(12), 0, 0, 0, 1));
            } else {
                vista.setOrientation(LinearLayout.VERTICAL);
                vista.setGravity(Gravity.CENTER_HORIZONTAL);
                forma.setCornerRadius(dp(16));
                vista.addView(caja, new LinearLayout.LayoutParams(dp(64), dp(52)));
                TextView n = texto(nombre, 12, false, TEXTO2);
                n.setGravity(Gravity.CENTER);
                n.setPadding(dp(2), dp(6), dp(2), 0);
                vista.addView(n);
                caja.setForeground(new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), null, fondo(Color.WHITE, 0, 16)));
            }
            vista.setOnClickListener(v -> {
                boolean ahora = !on;
                poner(ahora);
                cambio.hacer(ahora, () -> poner(!ahora));   // falló: vuelve como estaba
            });
            poner(inicial);
        }

        void poner(boolean v) {
            on = v;
            forma.setColor(on ? ACENTO : APAGADO);
            ic.setColorFilter(on ? 0xFF003049 : Color.WHITE);   // glifo oscuro sobre el acento, como Windows 11
            if (estado != null) estado.setText(on ? (red != null ? red : conectado ? Idioma.t("Conectado", "Connected") : Idioma.t("Activado", "On")) : Idioma.t("Desactivado", "Off"));
        }
    }

    void agente(String orden, Runnable mal) { Inicio.peticionAgente(ctx, orden, null, mal); }

    // ---------- redes Wi-Fi de Windows (mantener pulsado el botón Wi-Fi) ----------
    // Android sale a Internet por Windows (virt_wifi + NAT): elegir red es cambiar la de Windows. El visor da la lista por 4445
    // y conecta las que Windows ya tiene guardadas; una nueva (con contraseña) se elige en el panel de redes de Windows: la
    // contraseña nunca pasa por Android ni por el visor.

    static final List<String[]> redes = new ArrayList<>(), redesNuevas = new ArrayList<>();   // { nombre, ssid hex, señal 0-100, banderas }
    static boolean redesListas;   // Windows ya contestó (lista vacía = no ve redes, no "buscando")
    Interruptor botonWifi;
    AlertDialog dialogoWifi;
    LinearLayout listaWifi;

    /** Del visor por 4445: "wifi <ssid hex> <señal> <c|g|n><s|a>" por red (conectada/guardada/nueva, segura/abierta) entre "wifi inicio" y "wifi fin"; "wifi error" = no conectó. Validado: viene de la red. */
    static void wifiDelVisor(Context c, String l) {
        if (l.equals("wifi error")) { HILO.post(() -> { Toast.makeText(c, Idioma.t("No se pudo conectar", "Couldn't connect"), Toast.LENGTH_SHORT).show(); de(c).pintarWifi(); }); return; }
        boolean fin = l.equals("wifi fin"), inicio = l.equals("wifi inicio");
        String[] red = fin || inicio ? null : redDe(l);
        if (!fin && !inicio && red == null) return;
        HILO.post(() -> {
            if (inicio) { redesNuevas.clear(); return; }   // una lista cortada a medias (visor reconectado) no se mezcla con esta
            if (!fin) { if (redesNuevas.size() < 32) redesNuevas.add(red); return; }
            redes.clear(); redes.addAll(redesNuevas); redesNuevas.clear(); redesListas = true;
            Paneles yo = de(c);
            if (yo.botonWifi != null) {
                yo.botonWifi.red = null;
                for (String[] r : redes) if (r[3].charAt(0) == 'c') yo.botonWifi.red = r[0];
                yo.botonWifi.poner(yo.botonWifi.on);
            }
            yo.pintarWifi();
        });
    }

    /** "wifi <ssid hex> <señal> <banderas>" -> { nombre, hex, señal, banderas }; null si la línea no es válida. */
    static String[] redDe(String l) {
        if (!l.matches("wifi ([0-9a-f]{2}){1,32} (100|[0-9]{1,2}) [cgn][sa]")) return null;
        String[] p = l.split(" ");
        byte[] b = new byte[p[1].length() / 2];
        for (int i = 0; i < b.length; i++) b[i] = (byte) Integer.parseInt(p[1].substring(2 * i, 2 * i + 2), 16);
        return new String[] { new String(b, java.nio.charset.StandardCharsets.UTF_8), p[1], p[2], p[3] };
    }

    /** Una línea al visor, fuera del hilo principal. avisar: si no hay visor, se dice. */
    void alVisor(String l, boolean avisar) {
        new Thread(() -> {
            if (!AWindows.alVisor(l) && avisar) HILO.post(() -> Toast.makeText(ctx, Idioma.t("Windows no responde", "Windows isn't responding"), Toast.LENGTH_SHORT).show());
        }).start();
    }

    void redesWifi() {
        if (dialogoWifi != null) dialogoWifi.dismiss();
        LinearLayout lista = listaWifi = new LinearLayout(ctx);
        lista.setOrientation(LinearLayout.VERTICAL);
        lista.setPadding(dp(12), dp(8), dp(12), 0);
        ScrollView sv = new ScrollView(ctx);
        sv.addView(lista);
        AlertDialog d = dialogoWifi = new AlertDialog.Builder(ctx).setTitle("Wi-Fi").setView(sv)
                .setNeutralButton(Idioma.t("Abrir redes de Windows", "Open in Windows"), (x, w) -> alVisor("wifi panel", true))
                .setPositiveButton(Idioma.t("Cerrar", "Close"), null).create();
        d.getWindow().setType(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY);
        d.setOnDismissListener(x -> { if (dialogoWifi == d) { dialogoWifi = null; listaWifi = null; } });
        d.show();
        pintarWifi();
        alVisor("wifi buscar", true);   // lo que Windows ya sabe, al momento; y busca de nuevo
        for (int ms : new int[] { 3500, 7000 }) HILO.postDelayed(() -> { if (dialogoWifi == d) alVisor("wifi lista", false); }, ms);   // lo encontrado en esa búsqueda (Windows tarda hasta ~4 s)
    }

    void pintarWifi() {
        LinearLayout l = listaWifi;
        if (l == null) return;
        l.removeAllViews();
        if (redes.isEmpty()) { l.addView(vacio(redesListas ? Idioma.t("Windows no ve redes Wi-Fi", "Windows can't find Wi-Fi networks") : Idioma.t("Buscando redes…", "Looking for networks…"))); return; }
        for (String[] r : redes) {
            boolean con = r[3].charAt(0) == 'c', guardada = r[3].charAt(0) != 'n', segura = r[3].charAt(1) == 's';
            LinearLayout f = fila();
            f.setPadding(dp(12), dp(10), dp(12), dp(10));
            f.setBackground(presionable(fondo(0, 0, 16)));
            ImageView ic = new ImageView(ctx);
            ic.setImageDrawable(icono("ic_wifi_signal_" + Math.min(4, Integer.parseInt(r[2]) / 20), con ? ACENTO : Color.WHITE));
            f.addView(ic, new LinearLayout.LayoutParams(dp(26), dp(26)));
            LinearLayout textos = new LinearLayout(ctx);
            textos.setOrientation(LinearLayout.VERTICAL);
            textos.addView(texto(r[0], 15, true, Color.WHITE));
            TextView sub = texto(con ? Idioma.t("Conectada", "Connected") : guardada ? Idioma.t("Guardada: toca para conectar", "Saved: tap to connect") : segura ? Idioma.t("Con contraseña: se elige en Windows", "Secured: choose it in Windows") : Idioma.t("Abierta: se elige en Windows", "Open: choose it in Windows"), 12, false, con ? ACENTO : TEXTO2);
            textos.addView(sub);
            f.addView(textos, margen(dp(14), 0, 0, 0, 1));
            f.setOnClickListener(v -> {
                if (con) return;
                if (!guardada) { alVisor("wifi panel", true); return; }
                sub.setText(Idioma.t("Conectando…", "Connecting…"));
                alVisor("wifi conectar " + r[1], true);
                // Windows tarda unos segundos en cambiar de red; también con la lista ya cerrada (el botón Wi-Fi dice el nombre)
                for (int ms : new int[] { 3000, 8000 }) HILO.postDelayed(() -> alVisor("wifi lista", false), ms);
            });
            l.addView(f);
        }
    }

    View filaMedia(MediaController mc) {
        LinearLayout f = fila();
        f.setBackground(fondo(SUPERFICIE, 0, 20));
        f.setPadding(dp(10), dp(10), dp(8), dp(10));
        MediaMetadata md = mc.getMetadata();
        Bitmap arte = md == null ? null : md.getBitmap(MediaMetadata.METADATA_KEY_ART);
        if (arte == null && md != null) arte = md.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART);
        ImageView caratula = new ImageView(ctx);
        caratula.setScaleType(ImageView.ScaleType.CENTER_CROP);
        if (arte != null) caratula.setImageBitmap(arte);
        else caratula.setImageDrawable(iconoPaquete(mc.getPackageName()));
        Isla.redondear(caratula, dp(12));
        f.addView(caratula, new LinearLayout.LayoutParams(dp(48), dp(48)));
        caratula.setOnClickListener(v -> {
            PendingIntent pi = mc.getSessionActivity();
            cerrar();
            if (pi != null) enviar(pi);
        });
        LinearLayout textos = new LinearLayout(ctx);
        textos.setOrientation(LinearLayout.VERTICAL);
        String tit = md == null ? null : md.getString(MediaMetadata.METADATA_KEY_TITLE);
        String art = md == null ? null : md.getString(MediaMetadata.METADATA_KEY_ARTIST);
        textos.addView(texto(TextUtils.isEmpty(tit) ? etiqueta(mc.getPackageName()) : tit, 14, true, Color.WHITE));
        if (!TextUtils.isEmpty(art)) textos.addView(texto(art, 12, false, TEXTO2));
        f.addView(textos, margen(dp(12), 0, dp(4), 0, 1));
        PlaybackState ps = mc.getPlaybackState();
        boolean[] sonando = {ps != null && ps.getState() == PlaybackState.STATE_PLAYING};
        f.addView(botonIcono("ic_media_previous", 36, v -> mc.getTransportControls().skipToPrevious()));
        ImageView[] play = new ImageView[1];
        play[0] = botonIcono(sonando[0] ? "ic_media_pause" : "ic_media_play", 40, v -> {
            if (sonando[0]) mc.getTransportControls().pause(); else mc.getTransportControls().play();
            sonando[0] = !sonando[0];
            play[0].setImageDrawable(icono(sonando[0] ? "ic_media_pause" : "ic_media_play", Color.WHITE));
        });
        f.addView(play[0]);
        f.addView(botonIcono("ic_media_next", 36, v -> mc.getTransportControls().skipToNext()));
        return f;
    }

    // ---------- notificaciones ----------

    void armarNoti() {
        velo();
        LinearLayout t = tarjeta(dp(380), Gravity.TOP | Gravity.START);
        t.addView(texto(Idioma.t("Notificaciones", "Notifications"), 18, true, Color.WHITE), margen(dp(4), 0, 0, dp(12)));
        listaNoti = new LinearLayout(ctx);
        listaNoti.setOrientation(LinearLayout.VERTICAL);
        listaNoti.setLayoutTransition(new LayoutTransition());   // al quitar uno, los demás suben suaves
        int maxAlto = (int) (pantalla()[1] * .62f);
        ScrollView sv = new ScrollView(ctx) {
            @Override protected void onMeasure(int w, int h) { super.onMeasure(w, View.MeasureSpec.makeMeasureSpec(maxAlto, View.MeasureSpec.AT_MOST)); }
        };
        sv.setVerticalScrollBarEnabled(false);
        sv.setOverScrollMode(View.OVER_SCROLL_NEVER);
        sv.addView(listaNoti);
        t.addView(sv);
        LinearLayout pie = fila();
        TextView ajustes = texto(Idioma.t("Ajustes de notificaciones", "Notification settings"), 14, true, ACENTO);
        ajustes.setPadding(dp(4), dp(10), dp(8), dp(10));
        ajustes.setOnClickListener(v -> abrirAjustes("android.settings.NOTIFICATION_SETTINGS"));
        pie.addView(ajustes, peso());
        TextView borrar = pastilla(Idioma.t("Borrar", "Clear"));
        borrar.setOnClickListener(v -> borrarNoti());
        pie.addView(borrar);
        t.addView(pie, lleno(dp(8)));
        armando.refrescar = this::llenarNoti;
    }

    void llenarNoti() {
        if (listaNoti == null) return;
        LayoutTransition lt = listaNoti.getLayoutTransition();
        listaNoti.setLayoutTransition(null);   // rehacer la lista no se anima entera
        listaNoti.removeAllViews();
        List<StatusBarNotification> l = new ArrayList<>();
        try {
            StatusBarNotification[] activas = IslaOyente.activo == null ? null : IslaOyente.activo.getActiveNotifications();
            if (activas != null) for (StatusBarNotification s : activas) if (mostrable(s)) l.add(s);
        } catch (RuntimeException e) { Log.w(TAG, "avisos", e); }
        l.sort((a, b) -> Long.compare(b.getPostTime(), a.getPostTime()));
        for (StatusBarNotification s : l) listaNoti.addView(filaNoti(s), margen(0, 0, 0, dp(6), 0, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        if (l.isEmpty()) listaNoti.addView(vacio(Idioma.t("No hay notificaciones nuevas", "No new notifications")));
        listaNoti.setLayoutTransition(lt);
    }

    // Los resúmenes de grupo repiten a sus hijos y la multimedia ya está en los ajustes rápidos y en la isla.
    boolean mostrable(StatusBarNotification s) {
        Notification n = s.getNotification();
        return !s.getPackageName().equals(ctx.getPackageName()) && (n.flags & Notification.FLAG_GROUP_SUMMARY) == 0
                && !n.extras.containsKey(Notification.EXTRA_MEDIA_SESSION)
                && !(TextUtils.isEmpty(n.extras.getCharSequence(Notification.EXTRA_TITLE)) && TextUtils.isEmpty(n.extras.getCharSequence(Notification.EXTRA_TEXT)));
    }

    View filaNoti(StatusBarNotification s) {
        Notification n = s.getNotification();
        Deslizable d = new Deslizable(s.isClearable() ? () -> {
            if (IslaOyente.activo != null) IslaOyente.activo.cancelNotification(s.getKey());
        } : null);
        LinearLayout f = new LinearLayout(ctx);
        f.setPadding(dp(12), dp(12), dp(12), dp(12));
        f.setBackground(fondo(SUPERFICIE, 0, 16));
        ImageView ic = new ImageView(ctx);
        ic.setImageDrawable(iconoPaquete(s.getPackageName()));
        f.addView(ic, new LinearLayout.LayoutParams(dp(36), dp(36)));
        LinearLayout textos = new LinearLayout(ctx);
        textos.setOrientation(LinearLayout.VERTICAL);
        long ahora = System.currentTimeMillis();
        CharSequence cuando = ahora - s.getPostTime() < DateUtils.MINUTE_IN_MILLIS ? Idioma.t("Ahora", "Now")
                : DateUtils.getRelativeTimeSpanString(s.getPostTime(), ahora, DateUtils.MINUTE_IN_MILLIS, DateUtils.FORMAT_ABBREV_RELATIVE);
        textos.addView(texto(etiqueta(s.getPackageName()) + " · " + cuando, 12, false, TEXTO3));
        CharSequence tit = n.extras.getCharSequence(Notification.EXTRA_TITLE), cuerpo = n.extras.getCharSequence(Notification.EXTRA_TEXT);
        if (!TextUtils.isEmpty(tit)) textos.addView(texto(tit, 14, true, Color.WHITE));
        if (!TextUtils.isEmpty(cuerpo)) {
            TextView c = texto(cuerpo, 13, false, TEXTO2);
            c.setSingleLine(false);
            c.setMaxLines(2);
            textos.addView(c);
        }
        f.addView(textos, margen(dp(12), 0, 0, 0, 1));
        d.addView(f);
        d.setForeground(new RippleDrawable(ColorStateList.valueOf(0x1FFFFFFF), null, fondo(Color.WHITE, 0, 16)));
        d.setOnClickListener(v -> {
            cerrar();
            if (n.contentIntent != null) enviar(n.contentIntent);
            if ((n.flags & Notification.FLAG_AUTO_CANCEL) != 0 && IslaOyente.activo != null) IslaOyente.activo.cancelNotification(s.getKey());
        });
        return d;
    }

    void borrarNoti() {
        if (IslaOyente.activo == null || listaNoti == null) return;
        for (int i = 0; i < listaNoti.getChildCount(); i++)
            listaNoti.getChildAt(i).animate().translationX(dp(400)).alpha(0f).setStartDelay(i * 25L).setDuration(160).setInterpolator(salida).withLayer();
        principal.postDelayed(() -> { if (IslaOyente.activo != null) IslaOyente.activo.cancelAllNotifications(); },
                160 + 25L * listaNoti.getChildCount());
    }

    /** Fila que se descarta deslizándola de lado (más de un tercio del ancho); sin acción, vuelve a su sitio. */
    class Deslizable extends FrameLayout {
        final Runnable alQuitar;
        final int slop = ViewConfiguration.get(ctx).getScaledTouchSlop();
        float x0, y0;
        boolean arrastrando;

        Deslizable(Runnable alQuitar) { super(ctx); this.alQuitar = alQuitar; }

        // Si un hijo se queda con el toque, el deslizamiento lateral se le quita aquí.
        @Override public boolean onInterceptTouchEvent(MotionEvent e) {
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) { x0 = e.getRawX(); y0 = e.getRawY(); arrastrando = false; }
            else if (e.getActionMasked() == MotionEvent.ACTION_MOVE && empieza(e)) return true;
            return false;
        }

        boolean empieza(MotionEvent e) {
            float dx = e.getRawX() - x0;
            if (arrastrando || alQuitar == null || Math.abs(dx) <= slop || Math.abs(dx) <= Math.abs(e.getRawY() - y0)) return arrastrando;
            arrastrando = true;
            getParent().requestDisallowInterceptTouchEvent(true);
            return true;
        }

        @Override public boolean onTouchEvent(MotionEvent e) {
            float dx = e.getRawX() - x0;
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: x0 = e.getRawX(); y0 = e.getRawY(); arrastrando = false; break;
                case MotionEvent.ACTION_MOVE:
                    if (!arrastrando && empieza(e)) {
                        MotionEvent c = MotionEvent.obtain(e);   // sin pulsación ni clic
                        c.setAction(MotionEvent.ACTION_CANCEL);
                        super.onTouchEvent(c);
                        c.recycle();
                    }
                    if (arrastrando) {
                        setTranslationX(dx);
                        setAlpha(1f - Math.min(1f, Math.abs(dx) / getWidth()));
                        return true;
                    }
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (arrastrando) {
                        arrastrando = false;
                        if (Math.abs(dx) > getWidth() / 3f && e.getActionMasked() == MotionEvent.ACTION_UP) {
                            animate().translationX(Math.signum(dx) * getWidth()).alpha(0f).setDuration(140).setInterpolator(salida).withEndAction(() -> {
                                if (getParent() instanceof ViewGroup) ((ViewGroup) getParent()).removeView(this);
                                alQuitar.run();
                            });
                        } else animate().translationX(0f).alpha(1f).setDuration(167).setInterpolator(entrada);
                        return true;
                    }
                    break;
            }
            return super.onTouchEvent(e);
        }
    }

    // ---------- multitarea ----------

    void armarRecientes() {
        Vidrio.Fondo f = new Vidrio.Fondo(ctx, 0x59000000);
        raiz.addView(f, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        Runnable fundir = () -> { f.setAlpha(0f); f.animate().alpha(1f).setDuration(180).setInterpolator(entrada); };   // sin capa: Fondo ya se funde sin ella
        fundir.run();
        armando.alAbrir.add(fundir);
        LinearLayout col = columna();
        // Como la Galaxy Tab (foto A): dos filas completas y, si hay más, se desliza de lado (antes una 3.ª fila salía cortada).
        rejillaTareas = new LinearLayout(ctx);
        rejillaTareas.setOrientation(LinearLayout.HORIZONTAL);
        rejillaTareas.setGravity(Gravity.CENTER);
        rejillaTareas.setPadding(dp(64), barraEstado(), dp(64), 0);
        rejillaTareas.setOnClickListener(v -> cerrar());   // toque entre tarjetas: volver (el desplazable no deja llegar a la raíz)
        android.widget.HorizontalScrollView sv = new android.widget.HorizontalScrollView(ctx);
        sv.setFillViewport(true);   // pocas tarjetas: centradas
        sv.setHorizontalScrollBarEnabled(false);
        sv.addView(rejillaTareas, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        col.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        TextView todo = pastilla(Idioma.t("Cerrar todo", "Close all"));
        todo.setOnClickListener(v -> cerrarTodas());
        col.addView(todo, margen(0, dp(12), 0, dp(20)));
        View dock = dock();
        col.addView(dock, margen(0, 0, 0, dp(28)));
        tarjeta = sv;
        salidaY = 0f;
        entrar(sv, dp(36), .94f, 0);   // las tarjetas suben y crecen, como Vista de tareas
        entrar(todo, dp(24), 1f, 40);
        entrar(dock, dp(24), 1f, 40);
        armando.refrescar = () -> {
            rejillaTareas.setAlpha(1f); rejillaTareas.setTranslationY(0);   // por si se cerró con "Cerrar todo"
            pintarTareas(ultimasTareas);   // al instante con lo último conocido; la respuesta del agente lo corrige
            Inicio.peticionAgente(ctx, "tareas", r -> {
                String l = r.trim();
                if (!l.equals(ultimasTareas)) { ultimasTareas = l; pintarTareas(l); }
                cargarMiniaturas(l);   // las de antes, ya
                Inicio.peticionAgente(ctx, "minis" + enDisco(l), m -> guardarMiniaturas(m), null);   // y las nuevas al llegar
            }, null);
        };
    }

    static List<String> paquetes(String l) {
        List<String> r = new ArrayList<>();
        for (String p : l.trim().split(",")) {
            int k = p.indexOf(':');
            if (k > 0 && k < p.length() - 1) r.add(p.substring(k + 1));
        }
        return r;
    }

    void pintarTareas(String l) {
        if (rejillaTareas == null || l.equals(pintado)) return;   // misma lista: las tarjetas ya están (sin rehacer vistas)
        pintado = l;
        rejillaTareas.removeAllViews();
        List<String> pkgs = paquetes(l);
        // Tareas de ayudantes sin icono propio (p. ej. la búsqueda de Ajustes, com.google.android.settings.intelligence): fuera.
        if (Inicio.instancia != null) pkgs.removeIf(k -> Inicio.instancia.buscar(k) == null);
        if (pkgs.isEmpty()) { rejillaTareas.addView(vacio(Idioma.t("No hay apps recientes", "No recent apps"))); return; }
        int[] p = pantalla();
        int cols = p[0] > p[1] ? 3 : 2, hueco = dp(28);
        // Ancho: caben "cols" columnas a lo ancho y dos tarjetas (cabecera dp(34) + imagen 3:2) a lo alto sobre "Cerrar todo" y el dock.
        int alto = p[1] - barraEstado() - dp(200);
        int ancho = Math.min(dp(420), Math.min((p[0] - dp(64) * 2 - hueco * (cols - 1)) / cols, ((alto - hueco) / 2 - dp(34)) * 3 / 2));
        LinearLayout col = null;
        for (int i = 0; i < pkgs.size(); i++) {
            if (i % 2 == 0) {   // columnas de dos: la más reciente arriba a la izquierda
                col = new LinearLayout(ctx);
                col.setOrientation(LinearLayout.VERTICAL);
                rejillaTareas.addView(col, margen(i == 0 ? 0 : hueco, 0, 0, 0));
            }
            col.addView(tarjetaTarea(pkgs.get(i), ancho), margen(0, i % 2 == 0 ? 0 : hueco, 0, 0));
        }
    }

    View tarjetaTarea(String pkg, int ancho) {
        Inicio.App a = Inicio.instancia == null ? null : Inicio.instancia.buscar(pkg);
        Drawable ic = a != null ? a.icono : iconoPaquete(pkg);
        LinearLayout c = new LinearLayout(ctx);
        c.setOrientation(LinearLayout.VERTICAL);
        LinearLayout cab = fila();
        ImageView i = new ImageView(ctx);
        i.setImageDrawable(ic);
        cab.addView(i, new LinearLayout.LayoutParams(dp(26), dp(26)));
        cab.addView(texto(a != null ? a.nombre : etiqueta(pkg), 14, true, Color.WHITE), margen(dp(10), 0, dp(8), 0, 1));
        cab.addView(botonIcono("ic_close", 32, v -> quitarTarea(pkg, c)));
        c.addView(cab, margen(dp(4), 0, 0, dp(8)));
        FrameLayout marco = new FrameLayout(ctx);
        marco.setBackgroundColor(colorDe(ic));
        ImageView grande = new ImageView(ctx);
        grande.setImageDrawable(ic);
        marco.addView(grande, new FrameLayout.LayoutParams(dp(80), dp(80), Gravity.CENTER));
        ImageView mini = new ImageView(ctx);
        mini.setScaleType(ImageView.ScaleType.CENTER_CROP);
        mini.setTag("mini:" + pkg);
        Bitmap b = miniaturas.get(pkg);
        if (b != null) mini.setImageBitmap(b);
        marco.addView(mini, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        marco.setForeground(fondo(0, BORDE, 16));
        Isla.redondear(marco, dp(16));
        marco.setElevation(dp(12));
        c.addView(marco, new LinearLayout.LayoutParams(ancho, ancho * 2 / 3));   // 3:2, como la pantalla
        c.setOnClickListener(v -> {
            marco.animate().scaleX(1.04f).scaleY(1.04f).setDuration(140).setInterpolator(entrada);
            if (a != null && Inicio.instancia != null) Inicio.instancia.abrir(a);   // su tarea (agente) o una nueva
            else agente("traer " + pkg, null);
            cerrar();
        });
        // Deslizar la tarjeta hacia arriba la cierra, como en Recientes de Android y One UI (pedido 19/11); corta, vuelve a su sitio.
        c.setOnTouchListener(new View.OnTouchListener() {
            float x0, y0; boolean tira;
            @Override public boolean onTouch(View v, MotionEvent e) {
                int ac = e.getActionMasked();
                float dx = e.getRawX() - x0, dy = e.getRawY() - y0;   // de pantalla: la tarjeta se mueve con el dedo
                if (ac == MotionEvent.ACTION_DOWN) { x0 = e.getRawX(); y0 = e.getRawY(); tira = false; return false; }
                if (ac == MotionEvent.ACTION_MOVE) {
                    if (!tira && dy < -ViewConfiguration.get(ctx).getScaledTouchSlop() && -dy > Math.abs(dx)) {
                        tira = true;
                        v.getParent().requestDisallowInterceptTouchEvent(true);   // el desplazable de lado no se la lleva
                        v.setPressed(false); v.cancelLongPress();                 // ya no es un toque
                    }
                    if (tira) { v.setTranslationY(Math.min(0f, dy)); v.setAlpha(Math.max(.3f, 1f + dy / Math.max(1, v.getHeight()))); }
                    return tira;
                }
                if (!tira || ac != MotionEvent.ACTION_UP && ac != MotionEvent.ACTION_CANCEL) return false;
                tira = false;
                // un tercio de su alto, o un gesto rápido y corto
                if (ac == MotionEvent.ACTION_UP && (dy < -v.getHeight() / 3f || dy < -dp(30) && e.getEventTime() - e.getDownTime() < 250)) quitarTarea(pkg, v);
                else v.animate().translationY(0f).alpha(1f).setDuration(160).setInterpolator(entrada);
                return true;
            }
        });
        return c;
    }

    void quitarTarea(String pkg, View c) {
        agente("cerrar " + pkg, null);
        miniaturas.remove(pkg);
        fechas.remove(pkg);
        c.animate().alpha(0f).translationYBy(-dp(32)).scaleX(.92f).scaleY(.92f).setDuration(160).setInterpolator(salida).withLayer().withEndAction(() -> {
            List<String> resto = new ArrayList<>();
            for (String p : ultimasTareas.split(",")) if (!p.endsWith(":" + pkg) && !p.isEmpty()) resto.add(p);
            ultimasTareas = TextUtils.join(",", resto);
            pintarTareas(ultimasTareas);
        });
    }

    void cerrarTodas() {
        agente("cerrar *", null);
        ultimasTareas = "";
        miniaturas.clear();
        fechas.clear();
        if (rejillaTareas != null) rejillaTareas.animate().alpha(0f).translationY(-dp(40)).setDuration(180).setInterpolator(salida).withLayer();
        principal.postDelayed(this::cerrar, 200);
    }

    /** " paquete,paquete": las miniaturas que ya hay en files/ (el agente no las repite si no cambiaron). */
    String enDisco(String l) {
        StringBuilder r = new StringBuilder();
        for (String pkg : paquetes(l)) if (new File(ctx.getFilesDir(), "mini_" + pkg + ".jpg").exists()) r.append(r.length() == 0 ? " " : ",").append(pkg);
        return r.toString();
    }

    /** Respuesta de "minis": " paquete:jpeg-base64 ..." -> files/mini_<paquete>.jpg (temporal y renombrar), y a la rejilla. */
    void guardarMiniaturas(String m) {
        File dir = ctx.getFilesDir();
        decodificador.execute(() -> {
            for (String t : m.trim().split(" ")) {
                int k = t.indexOf(':');
                if (k <= 0 || !t.substring(0, k).matches("[A-Za-z0-9_.]+")) continue;   // el nombre del archivo sale de aquí
                File f = new File(dir, "mini_" + t.substring(0, k) + ".jpg"), tmp = new File(dir, f.getName() + ".tmp");
                try (java.io.FileOutputStream o = new java.io.FileOutputStream(tmp)) { o.write(android.util.Base64.decode(t.substring(k + 1), android.util.Base64.NO_WRAP)); }
                catch (Exception e) { tmp.delete(); continue; }
                if (!tmp.renameTo(f)) tmp.delete();
            }
            principal.post(() -> cargarMiniaturas(ultimasTareas));
        });
    }

    // Las miniaturas las guarda guardarMiniaturas en files/ (mini_<paquete>.jpg). En la caché solo quedan las apps de la
    // lista y un archivo solo se decodifica si cambió desde la última vez; en un único hilo aparte.
    void cargarMiniaturas(String l) {
        List<String> pkgs = paquetes(l);
        miniaturas.keySet().retainAll(pkgs);
        fechas.keySet().retainAll(pkgs);
        File dir = ctx.getFilesDir();
        decodificador.execute(() -> {
            BitmapFactory.Options o = new BitmapFactory.Options();
            o.inPreferredConfig = Bitmap.Config.RGB_565;   // la mitad de memoria; una foto de pantalla no lo nota
            for (String pkg : pkgs) {
                File f = new File(dir, "mini_" + pkg + ".jpg");
                long fecha = f.lastModified();   // 0: no existe
                Long vista = fechas.get(pkg);
                if (fecha == 0 || (vista != null && vista == fecha)) continue;
                Bitmap b = BitmapFactory.decodeFile(f.getPath(), o);
                if (b == null) continue;
                principal.post(() -> {
                    if (!paquetes(ultimasTareas).contains(pkg)) return;   // la quitaron mientras tanto
                    miniaturas.put(pkg, b);
                    fechas.put(pkg, fecha);
                    View v = rejillaTareas == null ? null : rejillaTareas.findViewWithTag("mini:" + pkg);
                    if (v instanceof ImageView) ((ImageView) v).setImageBitmap(b);
                });
            }
        });
    }

    /** Barra inferior: botón de todas las apps y las apps fijadas del inicio. */
    View dock() {
        LinearLayout d = fila();
        d.setGravity(Gravity.CENTER);
        ImageView todas = botonIcono("ic_apps", 64, v -> alternar("cajon"));
        todas.setBackground(presionable(fondo(0xB31C1C21, BORDE, 32)));
        todas.setPadding(dp(20), dp(20), dp(20), dp(20));
        d.addView(todas, new LinearLayout.LayoutParams(dp(64), dp(64)));
        LinearLayout barra = fila();
        barra.setPadding(dp(10), 0, dp(10), 0);
        barra.setBackground(fondo(0xB31C1C21, BORDE, 32));
        Inicio ini = Inicio.instancia;
        if (ini != null) for (String pkg : ini.dock()) {
            Inicio.App a = ini.buscar(pkg);
            if (a == null) continue;
            ImageView ic = new ImageView(ctx);
            ic.setImageDrawable(a.icono);
            ic.setContentDescription(a.nombre);
            ic.setOnClickListener(v -> { ini.abrir(a); cerrar(); });
            barra.addView(ic, margen(dp(6), 0, dp(6), 0, 0, dp(48), dp(48)));
        }
        if (barra.getChildCount() > 0) d.addView(barra, margen(dp(12), 0, 0, 0, 0, ViewGroup.LayoutParams.WRAP_CONTENT, dp(64)));
        return d;
    }

    // ---------- piezas ----------

    /** Barra de brillo o volumen de Windows 11: icono, pista fina con acento y pulgar redondo. Sin objetos nuevos al dibujar. */
    static class Deslizador extends View {
        interface Cambio { void a(float v); }
        final Paint pista = new Paint(Paint.ANTI_ALIAS_FLAG), lleno = new Paint(Paint.ANTI_ALIAS_FLAG),
                pulgar = new Paint(Paint.ANTI_ALIAS_FLAG), centro = new Paint(Paint.ANTI_ALIAS_FLAG);
        Drawable icono;
        Runnable alIcono;   // toque en el icono (silenciar); sin él, tocar el icono no hace nada
        boolean enIcono;
        final Cambio cambio;
        final float d;
        float valor;

        Deslizador(Context c, Drawable icono, float inicial, Cambio cambio) {
            super(c);
            this.icono = icono;
            this.cambio = cambio;
            d = c.getResources().getDisplayMetrics().density;
            valor = Math.max(0f, Math.min(1f, inicial));
            pista.setColor(0x4DFFFFFF);
            lleno.setColor(ACENTO);
            pulgar.setColor(0xFF45454A);
            centro.setColor(ACENTO);
        }

        void poner(float v) { valor = Math.max(0f, Math.min(1f, v)); invalidate(); }

        float x0() { return 44 * d; }
        float x1() { return getWidth() - 12 * d; }

        @Override protected void onDraw(Canvas c) {
            float my = getHeight() / 2f, a = x0(), b = x1(), x = a + (b - a) * valor, g = 2 * d;
            icono.setBounds(Math.round(4 * d), Math.round(my - 11 * d), Math.round(26 * d), Math.round(my + 11 * d));
            icono.draw(c);
            c.drawRoundRect(a, my - g, b, my + g, g, g, pista);
            c.drawRoundRect(a, my - g, x, my + g, g, g, lleno);
            c.drawCircle(x, my, 10 * d, pulgar);
            c.drawCircle(x, my, 5.5f * d, centro);
        }

        @Override public boolean onTouchEvent(MotionEvent e) {
            int ac = e.getActionMasked();
            if (ac == MotionEvent.ACTION_DOWN) { getParent().requestDisallowInterceptTouchEvent(true); enIcono = e.getX() < 34 * d; }
            // Un toque que empieza en el icono no mueve la barra (antes la bajaba a cero).
            if (enIcono) { if (ac == MotionEvent.ACTION_UP && alIcono != null) { alIcono.run(); invalidate(); } return true; }
            if (ac == MotionEvent.ACTION_DOWN || ac == MotionEvent.ACTION_MOVE || ac == MotionEvent.ACTION_UP) {
                valor = Math.max(0f, Math.min(1f, (e.getX() - x0()) / (x1() - x0())));
                cambio.a(valor);
                invalidate();
            }
            return true;
        }
    }

    /** Sol del brillo (el sistema no trae uno público): círculo y ocho rayos. */
    class Sol extends Drawable {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        Sol() { p.setColor(Color.WHITE); p.setStrokeWidth(dp(1.8f)); p.setStrokeCap(Paint.Cap.ROUND); p.setStyle(Paint.Style.STROKE); }
        @Override public void draw(Canvas c) {
            float cx = getBounds().exactCenterX(), cy = getBounds().exactCenterY(), r = getBounds().width() / 2f;
            c.drawCircle(cx, cy, r * .38f, p);
            for (int i = 0; i < 8; i++) {
                double an = i * Math.PI / 4;
                float co = (float) Math.cos(an), se = (float) Math.sin(an);
                c.drawLine(cx + co * r * .62f, cy + se * r * .62f, cx + co * r * .9f, cy + se * r * .9f, p);
            }
        }
        @Override public void setAlpha(int a) { p.setAlpha(a); }
        @Override public void setColorFilter(android.graphics.ColorFilter f) { p.setColorFilter(f); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    void enviar(PendingIntent pi) {
        try {
            ActivityOptions o = ActivityOptions.makeBasic().setPendingIntentBackgroundActivityStartMode(ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED);
            pi.send(ctx, 0, null, null, null, null, o.toBundle());
        } catch (PendingIntent.CanceledException e) { Log.w(TAG, "aviso vencido", e); }
    }

    void abrirAjustes(String accion) {
        cerrar();
        try { ctx.startActivity(new Intent(accion).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
        catch (ActivityNotFoundException e) { ctx.startActivity(new Intent(Settings.ACTION_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
    }

    LinearLayout columna() {
        LinearLayout col = new LinearLayout(ctx);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setGravity(Gravity.CENTER_HORIZONTAL);
        raiz.addView(col, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        return col;
    }

    LinearLayout fila() {
        LinearLayout f = new LinearLayout(ctx);
        f.setGravity(Gravity.CENTER_VERTICAL);
        return f;
    }

    TextView texto(CharSequence s, float sp, boolean semi, int color) {
        TextView t = new TextView(ctx);
        t.setText(s);
        estilo(t, sp, semi, color);
        t.setSingleLine(true);
        t.setEllipsize(TextUtils.TruncateAt.END);
        return t;
    }

    void estilo(TextView t, float sp, boolean semi, int color) {
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        if ((semi ? Inicio.semi : Inicio.normal) != null) t.setTypeface(semi ? Inicio.semi : Inicio.normal);
    }

    TextView pastilla(String s) {
        TextView t = texto(s, 14, true, Color.WHITE);
        t.setGravity(Gravity.CENTER);
        t.setPadding(dp(22), dp(9), dp(22), dp(9));
        t.setBackground(presionable(fondo(0x1FFFFFFF, BORDE, 20)));
        return t;
    }

    View vacio(String s) {
        TextView t = texto(s, 14, false, TEXTO2);
        t.setGravity(Gravity.CENTER);
        t.setPadding(0, dp(28), 0, dp(28));
        return t;
    }

    ImageView botonIcono(String nombre, int ladoDp, View.OnClickListener al) {
        ImageView b = new ImageView(ctx);
        b.setImageDrawable(icono(nombre, Color.WHITE));
        int m = dp(ladoDp * .26f);
        b.setPadding(m, m, m, m);
        b.setLayoutParams(new LinearLayout.LayoutParams(dp(ladoDp), dp(ladoDp)));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33FFFFFF), null, fondo(Color.WHITE, 0, ladoDp / 2)));
        b.setOnClickListener(al);
        return b;
    }

    Drawable presionable(Drawable fondo) { return new RippleDrawable(ColorStateList.valueOf(0x26FFFFFF), fondo, null); }

    GradientDrawable fondo(int color, int borde, int radioDp) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(color);
        g.setCornerRadius(dp(radioDp));
        if (borde != 0) g.setStroke(dp(1), borde);
        return g;
    }

    /** Iconos del propio Android (framework-res, también los ocultos como ic_qs_airplane), en el color pedido. */
    Drawable icono(String nombre, int color) {
        int id = Resources.getSystem().getIdentifier(nombre, "drawable", "android");
        Drawable d = ctx.getDrawable(id != 0 ? id : android.R.drawable.ic_menu_help).mutate();
        d.setTint(color);
        return d;
    }

    Drawable iconoPaquete(String pkg) {
        Inicio.App a = Inicio.instancia == null ? null : Inicio.instancia.buscar(pkg);
        if (a != null) return a.icono;
        try { return ctx.getPackageManager().getApplicationIcon(pkg); }
        catch (PackageManager.NameNotFoundException e) { return icono("sym_def_app_icon", Color.WHITE); }
    }

    String etiqueta(String pkg) {
        Inicio.App a = Inicio.instancia == null ? null : Inicio.instancia.buscar(pkg);
        if (a != null) return a.nombre;
        try { return ctx.getPackageManager().getApplicationLabel(ctx.getPackageManager().getApplicationInfo(pkg, 0)).toString(); }
        catch (PackageManager.NameNotFoundException e) { return pkg; }
    }

    /** Color medio del icono, oscurecido: fondo de la tarjeta mientras no hay miniatura. */
    static int colorDe(Drawable ic) {
        if (!(ic instanceof BitmapDrawable)) return 0xFF1E1E22;
        int c = Bitmap.createScaledBitmap(((BitmapDrawable) ic).getBitmap(), 1, 1, true).getPixel(0, 0);
        return Color.rgb(Color.red(c) * 28 / 100 + 18, Color.green(c) * 28 / 100 + 18, Color.blue(c) * 28 / 100 + 24);   // oscuro: el icono resalta
    }

    int barraEstado() {
        int id = ctx.getResources().getIdentifier("status_bar_height", "dimen", "android");
        return id > 0 ? ctx.getResources().getDimensionPixelSize(id) : dp(24);
    }

    int[] pantalla() {
        android.graphics.Rect r = wm.getCurrentWindowMetrics().getBounds();
        return new int[]{r.width(), r.height()};
    }

    LinearLayout.LayoutParams lleno(int arriba) { return margen(0, arriba, 0, 0, 0, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT); }
    LinearLayout.LayoutParams peso() { return margen(0, 0, 0, 0, 1); }
    LinearLayout.LayoutParams margen(int i, int a, int d, int b) { return margen(i, a, d, b, 0); }
    LinearLayout.LayoutParams margen(int i, int a, int d, int b, float peso) {
        return margen(i, a, d, b, peso, peso > 0 ? 0 : ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }
    LinearLayout.LayoutParams margen(int i, int a, int d, int b, float peso, int ancho, int alto) {
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ancho, alto, peso);
        l.setMargins(i, a, d, b);
        return l;
    }

    int dp(float v) { return (int) (v * ctx.getResources().getDisplayMetrics().density + 0.5f); }
}
