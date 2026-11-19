package com.surfacedual.launcher;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.ActivityOptions;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.PlaybackState;
import android.os.SystemClock;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.view.WindowManager;
import android.view.animation.Interpolator;
import android.widget.Chronometer;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Isla dinámica (como la Dynamic Island de Apple o la Magic Capsule de Honor): píldora negra arriba en el centro que
 * cambia de forma con un muelle. Tres formas: COMPACTA (icono a la izquierda e información a la derecha), MÍNIMA (dos
 * actividades: la principal compacta y la segunda como un círculo aparte a la derecha) y DESPLEGADA (tarjeta con controles).
 * Gestos (Apple HIG "Live Activities": tocar abre la app, mantener despliega; aquí el toque también despliega porque el
 * usuario quiere los controles, y la app se abre tocando la carátula o el título de la tarjeta): tocar o mantener =
 * desplegar; tocar el círculo = abrir su app; de lado = cambiar de actividad (con una sola: minimizar/restaurar); hacia
 * arriba o tocar fuera = recoger. Medidas ×1,5 las del iPhone (tableta a ~45 cm): compacta 36 dp, tarjeta 40 % del lado largo.
 * Fuentes (las da IslaOyente): música/vídeo, llamadas, navegación, temporizadores y cronómetros, alarmas, grabación de
 * pantalla, descargas con progreso, y avisos breves (cargador, silencio, volumen) que duran 2 s.
 * Fluidez: la ventana solo cambia de tamaño al desplegar y al recoger; el cambio de forma se dibuja (sin medir ni
 * colocar vistas en cada cuadro). Batería: sin animaciones continuas salvo las ondas de la música (12 cuadros/s).
 */
class Isla {
    static final int ACENTO = 0xFF60CDFF, TEXTO2 = 0xBFFFFFFF, VERDE = 0xFF34C759, ROJO = 0xFFFF453A;
    // Prioridad: llamada > navegación > tiempo/alarma > grabación > multimedia > progreso; los avisos breves, encima de todo.
    static final int P_PROGRESO = 1, P_MEDIA = 2, P_GRABAR = 3, P_TIEMPO = 4, P_NAV = 5, P_LLAMADA = 6, P_BREVE = 9;

    /** Una actividad en curso. */
    static class Act {
        String clave; int prio; boolean destacada;   // destacada: "Live Update" (aviso promovido de Android 16+)
        Drawable icono; Bitmap imagen; CharSequence titulo, texto; int color = ACENTO;
        long base; boolean crono, cuentaAtras;   // cronómetro: base en tiempo de reloj (Notification.when)
        int progreso = -1;                       // 0..100, -1 sin barra
        boolean ondas;                           // música sonando
        PendingIntent abrir; Notification.Action[] acciones; MediaController mc;
    }

    final Context ctx;
    final WindowManager wm;
    final WindowManager.LayoutParams params;
    final FrameLayout raiz;
    final Capsula capsula;
    final FrameLayout compacta;
    final ImageView icCompacto;
    final TextView txCompacto;
    final Chronometer crCompacto;
    final Ondas ondas;
    final Anillo anillo;
    final LinearLayout expandida, filaBotones;
    final ImageView icGrande;
    final TextView tituloGrande, textoGrande;
    final Chronometer crGrande;
    final Barra barra;
    final LinearLayout filaTiempo;
    final TextView tPasado, tFalta;
    final Paneles.Deslizador volumen;
    final AudioManager am;
    int volPuesto = -1;
    final Circulo mini;
    final Typeface normal, semi;
    LinearLayout derecha;
    final int altoC, diamMini, hueco, lado;
    int anchoE, altoE;   // la tarjeta: se miden al desplegar (giro de pantalla y contenido)
    int anchoC;   // a la medida de su contenido (sin hueco negro en medio: la Surface no tiene cámara ahí)
    boolean minimizada;   // de lado con una sola actividad: solo el icono (como esconderla hacia la cámara en el iPhone)
    ValueAnimator anchura;

    final Map<String, Act> acts = new LinkedHashMap<>();
    Act breve;                 // aviso de 2 s
    String preferida;          // la principal que eligió el usuario deslizando
    Act principal, segunda;
    boolean puesta, desplegada, barraOculta, bloqueada;   // barraOculta: vídeo a pantalla completa (lo dice el agente)
    int manijaX = -1;          // borde izquierdo del asa de ventanas de Android en la app de delante; -1 = no hay
    ValueAnimator corrida;
    String publicada = "?", enviando;   // lo que el visor sabe de la isla y lo que va de camino al agente
    ValueAnimator morfo;

    Isla(Context c) {
        ctx = c;
        wm = c.getSystemService(WindowManager.class);
        // Compacta dentro de la franja de la barra de estado (como en el iPhone): nunca tapa el contenido de las apps.
        int idBarra = c.getResources().getIdentifier("status_bar_height", "dimen", "android");
        int alturaBarra = idBarra > 0 ? c.getResources().getDimensionPixelSize(idBarra) : dp(24);
        // ×1,5 la barra (36 dp en la Surface): la de Apple es 36,67 pt en un teléfono a ~30 cm; la tableta se mira a ~45 cm.
        altoC = Math.round(Math.max(alturaBarra, dp(24)) * 1.5f); lado = altoC - dp(8);
        am = c.getSystemService(AudioManager.class);
        anchoC = dp(160); anchoE = dp(576); altoE = dp(240); diamMini = altoC; hueco = dp(8);
        Typeface fn, fs;
        try {
            fn = Typeface.createFromAsset(c.getAssets(), "fonts/Selawik-Regular.ttf");
            fs = Typeface.createFromAsset(c.getAssets(), "fonts/Selawik-Semibold.ttf");
        } catch (RuntimeException e) { fn = Typeface.DEFAULT; fs = Typeface.DEFAULT_BOLD; }
        normal = fn; semi = fs;

        // En la franja de la barra de estado, bajo sus iconos (el centro está vacío). Ahí los toques se los queda la barra
        // (SystemUI): el visor toma los que empiezan en su rectángulo y nos los manda por el agente (gesto()). Sin
        // FLAG_NOT_TOUCHABLE: con él Android baja la ventana al 80 % de opacidad (medido: alpha=0.8 en dumpsys).
        params = new WindowManager.LayoutParams(anchoC, altoC, WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                        | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        params.setFitInsetsTypes(0);   // sin esto Android la baja por debajo de la barra de estado
        params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        params.setTitle("SurfaceDual isla");

        raiz = new FrameLayout(c) {   // toques de la desplegada (los de la compacta llegan por gesto())
            float x0, y0;
            @Override public boolean dispatchTouchEvent(MotionEvent e) {
                int ac = e.getActionMasked();
                if (ac == MotionEvent.ACTION_OUTSIDE) { recoger(); return true; }
                if (ac == MotionEvent.ACTION_DOWN) { x0 = e.getRawX(); y0 = e.getRawY(); }
                float dx = e.getRawX() - x0, dy = e.getRawY() - y0;
                if (!desplegada) {   // la compacta sin el visor (no la toma): un toque es un toque; recogiéndose, nada
                    if (ac == MotionEvent.ACTION_UP && !animando() && Math.abs(dx) < dp(12) && Math.abs(dy) < dp(12)) gesto("toque," + (int) e.getRawX());
                    return true;
                }
                if (ac == MotionEvent.ACTION_UP && !barra.arrastrando && dy < -dp(40) && -dy > 2 * Math.abs(dx)) {   // hacia arriba: recoger
                    e.setAction(MotionEvent.ACTION_CANCEL);
                    super.dispatchTouchEvent(e);
                    recoger();
                    return true;
                }
                return super.dispatchTouchEvent(e) || true;   // controles de la tarjeta
            }
        };
        // A la superpuesta no le llegan los insets de la app de delante (medido: con vídeo a pantalla completa seguía "visible"):
        // la barra y el asa de Android los mira el agente en SurfaceFlinger (barra(), manija()); aquí el estado al arrancar.
        Inicio.peticionAgente(c, "ventanas", r -> {
            String[] v = r.trim().split(" ");
            if (v.length == 2) { barra(v[0].equals("oculta")); manija(v[1]); }
        }, null);
        // El rectángulo de toques sigue a la ventana (ancho, círculo, giro): se mira en cada cuadro y solo se envía si cambia.
        raiz.getViewTreeObserver().addOnPreDrawListener(() -> { publicar(); return true; });

        capsula = new Capsula(c);
        raiz.addView(capsula, new FrameLayout.LayoutParams(anchoE, altoE, Gravity.TOP | Gravity.CENTER_HORIZONTAL));

        // --- compacta: icono a la izquierda, a la derecha texto/cronómetro/ondas/anillo ---
        compacta = new FrameLayout(c);
        icCompacto = new ImageView(c);
        icCompacto.setScaleType(ImageView.ScaleType.CENTER_CROP);
        redondear(icCompacto, dp(8));
        compacta.addView(icCompacto, new FrameLayout.LayoutParams(lado, lado, Gravity.START | Gravity.CENTER_VERTICAL) {{ leftMargin = dp(4); }});
        LinearLayout derecha = new LinearLayout(c);
        derecha.setGravity(Gravity.CENTER_VERTICAL | Gravity.END);
        txCompacto = texto(15, true, Color.WHITE);
        crCompacto = new Chronometer(c);
        estilo(crCompacto, 15, true, Color.WHITE);
        ondas = new Ondas(c);
        anillo = new Anillo(c);
        derecha.addView(txCompacto, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        derecha.addView(crCompacto, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        derecha.addView(ondas, new LinearLayout.LayoutParams(dp(24), dp(20)) {{ leftMargin = dp(8); }});
        derecha.addView(anillo, new LinearLayout.LayoutParams(dp(26), dp(26)) {{ leftMargin = dp(8); }});
        this.derecha = derecha;
        compacta.addView(derecha, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT, Gravity.END) {{ rightMargin = dp(14); }});
        capsula.addView(compacta, new FrameLayout.LayoutParams(anchoC, altoC, Gravity.TOP | Gravity.CENTER_HORIZONTAL));

        // --- desplegada (como la de Apple): carátula y textos (tocarlos abre la app); avance arrastrable con tiempos;
        // botones; volumen (Apple no lo trae; lo pidió el usuario). Arriba deja libre la franja de la barra de estado,
        // donde los toques son de SystemUI.
        expandida = new LinearLayout(c);
        expandida.setOrientation(LinearLayout.VERTICAL);
        expandida.setPadding(dp(24), dp(22), dp(24), dp(16));
        LinearLayout arriba = new LinearLayout(c);
        arriba.setGravity(Gravity.CENTER_VERTICAL);
        arriba.setOnClickListener(v -> { abrir(principal); recoger(); });
        icGrande = new ImageView(c);
        icGrande.setScaleType(ImageView.ScaleType.CENTER_CROP);
        redondear(icGrande, dp(14));
        arriba.addView(icGrande, new LinearLayout.LayoutParams(dp(64), dp(64)));
        LinearLayout textos = new LinearLayout(c);
        textos.setOrientation(LinearLayout.VERTICAL);
        tituloGrande = texto(20, true, Color.WHITE);
        textoGrande = texto(16, false, TEXTO2);
        crGrande = new Chronometer(c);
        estilo(crGrande, 34, true, Color.WHITE);
        textos.addView(tituloGrande);
        textos.addView(textoGrande);
        textos.addView(crGrande);
        arriba.addView(textos, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1) {{ leftMargin = dp(16); }});
        expandida.addView(arriba);
        filaTiempo = new LinearLayout(c);
        filaTiempo.setGravity(Gravity.CENTER_VERTICAL);
        tPasado = texto(13, true, TEXTO2);
        tFalta = texto(13, true, TEXTO2);
        barra = new Barra(c);
        filaTiempo.addView(tPasado);
        filaTiempo.addView(barra, new LinearLayout.LayoutParams(0, dp(32), 1) {{ leftMargin = dp(12); rightMargin = dp(12); }});
        filaTiempo.addView(tFalta);
        expandida.addView(filaTiempo, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT) {{ topMargin = dp(12); }});
        filaBotones = new LinearLayout(c);
        filaBotones.setGravity(Gravity.CENTER);
        expandida.addView(filaBotones, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(80)) {{ topMargin = dp(4); }});
        volumen = new Paneles.Deslizador(c, Paneles.de(c).icono("ic_volume", Color.WHITE), 0f, v -> {
            int i = Math.round(v * Math.max(1, am.getStreamMaxVolume(AudioManager.STREAM_MUSIC)));
            if (i != volPuesto) { volPuesto = i; try { am.setStreamVolume(AudioManager.STREAM_MUSIC, i, 0); } catch (SecurityException ignored) { } }   // "no molestar" puede negarlo
        });
        expandida.addView(volumen, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(44)) {{ topMargin = dp(4); }});
        expandida.setAlpha(0f);
        expandida.setVisibility(View.INVISIBLE);
        capsula.addView(expandida, new FrameLayout.LayoutParams(anchoE, altoE, Gravity.TOP | Gravity.CENTER_HORIZONTAL));

        // --- mínima: la segunda actividad, círculo aparte a la derecha ---
        mini = new Circulo(c);
        raiz.addView(mini, new FrameLayout.LayoutParams(diamMini, diamMini, Gravity.TOP | Gravity.END));
        mini.setVisibility(View.GONE);
        publicar();   // "off": el visor pudo quedarse con el rectángulo de antes de reiniciarse el launcher
    }

    // ---------- entradas (IslaOyente) ----------

    void poner(Act a) { acts.put(a.clave, a); refrescar(); }
    void quitar(String clave) { if (acts.remove(clave) != null) refrescar(); }
    /** Las de avisos se sustituyen en bloque (las de música no se tocan). */
    void ponerAvisos(List<Act> l) {
        acts.values().removeIf(a -> a.mc == null && !a.clave.equals("privacidad"));   // la de micrófono/cámara la da el agente
        for (Act a : l) acts.put(a.clave, a);
        refrescar();
    }
    void avisoBreve(Act a) {
        if (desplegada) { leerVolumen(); return; }   // con la tarjeta abierta no se tapa: su deslizador ya lo enseña
        a.prio = P_BREVE;
        breve = a;
        raiz.removeCallbacks(finBreve);
        raiz.postDelayed(finBreve, 2000);
        refrescar();
    }
    final Runnable finBreve = () -> { breve = null; refrescar(); };

    /** Elige la principal y la segunda y lo pinta. */
    void refrescar() {
        List<Act> orden = new ArrayList<>(acts.values());
        orden.sort((x, y) -> y.prio != x.prio ? y.prio - x.prio : Boolean.compare(y.destacada, x.destacada));
        Act p = breve != null ? breve : null;
        if (p == null && preferida != null) for (Act a : orden) if (a.clave.equals(preferida)) p = a;
        if (p == null && !orden.isEmpty()) p = orden.get(0);
        Act s = null;
        for (Act a : orden) if (a != p) { s = a; break; }
        Act antes = principal;
        principal = p; segunda = s;
        if (p == null) { quitarVentana(); return; }
        boolean otra = antes == null || !antes.clave.equals(p.clave);   // la música llega como Act nueva en cada cambio: misma clave, sin fundido
        if (otra) minimizada = false;
        pintarCompacta(p, antes != null && otra);
        pintarMini(s);
        if (desplegada) { pintarDesplegada(p); if (!animando() && medirTarjeta()) { params.height = altoE; actualizarVentana(); } }   // el contenido cambió de alto
        ponerVentana();
        aplicarVisibilidad();
    }

    /** Toque en la compacta que tomó el visor: "toque,x" (x en píxeles de Android), "mantener", "abajo", "izq", "der", "arriba". */
    void gesto(String g) {
        if (principal == null || desplegada || raiz.getVisibility() != View.VISIBLE) return;
        if (g.matches("toque,[0-9]{1,5}")) {
            int[] p = new int[2];
            mini.getLocationOnScreen(p);
            boolean enMini = segunda != null && mini.getVisibility() == View.VISIBLE && Integer.parseInt(g.substring(6)) > p[0] - hueco / 2;
            if (enMini) abrir(segunda);   // el círculo, como la mínima de Apple: abre su app
            else desplegar();             // Apple abre la app; aquí enseña los controles (la app, tocando la carátula)
        } else if (g.equals("mantener") || g.equals("abajo")) desplegar();
        else if (g.equals("izq") || g.equals("der")) {   // de lado: otra actividad; con una sola, minimizar o restaurar
            if (segunda != null) cambiar();
            else { minimizada = !minimizada; pintarCompacta(principal, false); }
        }
    }

    /** Dice al visor (por el agente) dónde tomar los toques de la isla, u "off". Una petición a la vez: llegan en orden.
     *  shortcut: si el agente se reinicia olvida el rectángulo hasta el próximo cambio; si molesta, reenviar al reconectar. */
    void publicar() {
        String v = "off";
        if (puesta && !desplegada && raiz.getHeight() <= altoC && raiz.getVisibility() == View.VISIBLE && raiz.getWidth() > 0) {   // recogiéndose: aún no
            int[] p = new int[2];
            raiz.getLocationOnScreen(p);
            v = p[0] + " " + p[1] + " " + (p[0] + raiz.getWidth()) + " " + (p[1] + raiz.getHeight());
        }
        if (enviando != null || v.equals(publicada)) return;
        String e = enviando = v;
        Inicio.peticionAgente(ctx, "isla " + e, r -> { publicada = e; enviando = null; publicar(); },
                () -> raiz.postDelayed(() -> { enviando = null; publicar(); }, 5000));   // sin agente: reintento lento
    }

    void cambiar() {   // deslizar de lado: la segunda pasa a principal
        if (segunda == null) return;
        preferida = segunda.clave;
        refrescar();
    }

    // ---------- pintar ----------

    void pintarCompacta(Act a, boolean fundir) {
        if (desplegada) compacta.setAlpha(0f);   // desplegada: la compacta no se ve encima de la tarjeta
        else if (fundir) { compacta.setAlpha(0f); compacta.animate().alpha(1f).setDuration(180); }
        if (a.imagen != null) icCompacto.setImageBitmap(a.imagen); else icCompacto.setImageDrawable(a.icono);
        boolean conCrono = a.crono;
        crCompacto.setVisibility(conCrono ? View.VISIBLE : View.GONE);
        if (conCrono) cronometro(crCompacto, a); else crCompacto.stop();
        txCompacto.setVisibility(!conCrono && a.texto != null && a.progreso < 0 ? View.VISIBLE : View.GONE);
        txCompacto.setText(a.texto);
        txCompacto.setTextColor(a.prio == P_BREVE || a.prio == P_LLAMADA ? a.color : Color.WHITE);
        ondas.setVisibility(a.ondas ? View.VISIBLE : View.GONE);
        ondas.color = a.color;
        ondas.activa(a.ondas && puesta && !minimizada && !desplegada);   // tapadas por la tarjeta: sin redibujar 12 veces/s
        derecha.setVisibility(minimizada ? View.INVISIBLE : View.VISIBLE);
        anillo.setVisibility(a.progreso >= 0 ? View.VISIBLE : View.GONE);
        anillo.poner(a.progreso, a.color);
        ajustarAncho(a);
    }

    /** Ancho de la compacta a la medida: icono + lo de la derecha con márgenes normales (se mide al cambiar de actividad). */
    void ajustarAncho(Act a) {
        CharSequence real = crCompacto.getText();
        if (a.crono) crCompacto.setText("00:00");   // los segundos no la ensanchan cada tic
        derecha.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED), View.MeasureSpec.makeMeasureSpec(altoC, View.MeasureSpec.EXACTLY));
        if (a.crono) crCompacto.setText(real);
        int nuevo = minimizada ? altoC : Math.max(dp(150), Math.min(dp(420), dp(4) + lado + dp(14) + derecha.getMeasuredWidth() + dp(14)));
        if (nuevo == anchoC) return;
        int viejo = anchoC;
        anchoC = nuevo;
        ViewGroup.LayoutParams lc = compacta.getLayoutParams();
        lc.width = nuevo;
        compacta.setLayoutParams(lc);
        // La píldora se estira o encoge con el muelle (solo se dibuja); la ventana, a la mayor mientras dura.
        if (anchura != null) anchura.cancel();
        capsula.anchoDibujo = viejo;
        anchura = ValueAnimator.ofFloat(viejo, nuevo);
        anchura.setDuration(420);
        anchura.setInterpolator(MUELLE);
        anchura.addUpdateListener(v -> { capsula.anchoDibujo = (float) v.getAnimatedValue(); capsula.forma(capsula.f); });
        anchura.addListener(new AnimatorListenerAdapter() {
            @Override public void onAnimationEnd(Animator x) { capsula.anchoDibujo = -1; capsula.forma(capsula.f); pintarMini(segunda); }
        });
        if (!desplegada && !animando()) { params.width = Math.max(viejo, nuevo) + (segunda != null ? hueco + diamMini : 0); params.x = xVentana(); actualizarVentana(); }
        anchura.start();
    }

    void pintarMini(Act s) {
        boolean ver = s != null && !desplegada;
        mini.setVisibility(ver ? View.VISIBLE : View.GONE);
        if (s != null) mini.poner(s.imagen != null ? s.imagen : null, s.icono, s.color);
        int ancho = desplegada ? anchoE : anchoC + (ver ? hueco + diamMini : 0);
        // la ventana queda justo sobre la píldora y el círculo: corrida a la derecha la mitad de lo que añade el círculo
        params.x = xVentana();
        capsula.setTranslationX(desplegada || !ver ? 0 : -(hueco + diamMini) / 2f);
        if (params.width != ancho && !animando() && (anchura == null || !anchura.isRunning())) { params.width = ancho; actualizarVentana(); }
    }

    void pintarDesplegada(Act a) {
        if (a.imagen != null) icGrande.setImageBitmap(a.imagen); else icGrande.setImageDrawable(a.icono);
        tituloGrande.setText(a.titulo != null ? a.titulo : a.texto);
        textoGrande.setText(a.titulo != null ? a.texto : null);
        textoGrande.setVisibility(a.titulo != null && a.texto != null && !a.crono ? View.VISIBLE : View.GONE);
        crGrande.setVisibility(a.crono ? View.VISIBLE : View.GONE);
        if (a.crono) cronometro(crGrande, a); else crGrande.stop();
        boolean media = a.mc != null;
        PlaybackState ps = media ? a.mc.getPlaybackState() : null;
        boolean suena = ps != null && ps.getState() == PlaybackState.STATE_PLAYING;
        barra.alSoltar = null;
        tPasado.setVisibility(media ? View.VISIBLE : View.GONE);
        tFalta.setVisibility(media ? View.VISIBLE : View.GONE);
        if (media) {
            barra.alSoltar = f -> { long d = duracion(a); if (d > 0) a.mc.getTransportControls().seekTo((long) (f * d)); };
            barra.poner(0, a.color);
            avance(a);
            raiz.removeCallbacks(tic);
            if (suena) raiz.postDelayed(tic, 1000);   // 1 Hz solo con la tarjeta abierta y sonando
        } else barra.poner(a.progreso / 100f, a.color);
        filaTiempo.setVisibility(media || a.progreso >= 0 ? View.VISIBLE : View.GONE);
        volumen.setVisibility(media ? View.VISIBLE : View.GONE);
        volumen.lleno.setColor(a.color); volumen.centro.setColor(a.color);
        if (media) leerVolumen();
        filaBotones.removeAllViews();
        if (media) {
            boolean[] sonando = {suena};
            filaBotones.addView(boton("ic_media_previous", 72, v -> a.mc.getTransportControls().skipToPrevious()));
            ImageView[] play = new ImageView[1];
            play[0] = boton(suena ? "ic_media_pause" : "ic_media_play", 80, v -> {
                if (sonando[0]) a.mc.getTransportControls().pause(); else a.mc.getTransportControls().play();
                sonando[0] = !sonando[0];   // al instante; el estado real llega después por la sesión y la repinta
                play[0].setImageDrawable(Paneles.de(ctx).icono(sonando[0] ? "ic_media_pause" : "ic_media_play", Color.WHITE));
            });
            filaBotones.addView(play[0]);
            filaBotones.addView(boton("ic_media_next", 72, v -> a.mc.getTransportControls().skipToNext()));
        } else if (a.acciones != null) {
            for (int i = 0; i < Math.min(3, a.acciones.length); i++) {   // temporizador: pausar/restablecer; llamada: colgar; descarga: cancelar
                Notification.Action ac = a.acciones[i];
                TextView b = texto(17, true, i == a.acciones.length - 1 && a.prio == P_LLAMADA ? ROJO : Color.WHITE);
                b.setText(ac.title);
                b.setGravity(Gravity.CENTER);
                b.setBackground(Paneles.de(ctx).fondo(i == a.acciones.length - 1 && a.prio == P_LLAMADA ? 0x40FF453A : 0x26FFFFFF, 0, 26));
                b.setOnClickListener(v -> enviar(ac.actionIntent));
                filaBotones.addView(b, new LinearLayout.LayoutParams(0, dp(52), 1) {{ leftMargin = dp(6); rightMargin = dp(6); }});
            }
        }
        filaBotones.setVisibility(filaBotones.getChildCount() > 0 ? View.VISIBLE : View.GONE);
    }

    static long duracion(Act a) { MediaMetadata md = a.mc.getMetadata(); return md == null ? 0 : md.getLong(MediaMetadata.METADATA_KEY_DURATION); }

    /** Barra y tiempos de la canción (lo pasado y lo que falta, como Apple). */
    void avance(Act a) {
        PlaybackState ps = a.mc.getPlaybackState();
        long dur = duracion(a), pos = ps == null ? 0 : ps.getPosition();
        if (ps != null && ps.getState() == PlaybackState.STATE_PLAYING && ps.getLastPositionUpdateTime() > 0) pos += (long) ((SystemClock.elapsedRealtime() - ps.getLastPositionUpdateTime()) * ps.getPlaybackSpeed());
        pos = Math.max(0, dur > 0 ? Math.min(pos, dur) : pos);
        if (!barra.arrastrando) barra.poner(dur > 0 ? pos / (float) dur : 0, barra.frente.getColor());
        tPasado.setText(formatearTiempo(pos));
        tFalta.setText(dur > 0 ? "-" + formatearTiempo(dur - pos) : "");
    }

    final Runnable tic = new Runnable() {
        @Override public void run() {
            Act a = principal;
            if (!desplegada || a == null || a.mc == null) return;
            avance(a);
            PlaybackState ps = a.mc.getPlaybackState();
            if (ps != null && ps.getState() == PlaybackState.STATE_PLAYING) raiz.postDelayed(this, 1000);
        }
    };

    void leerVolumen() {
        volPuesto = am.getStreamVolume(AudioManager.STREAM_MUSIC);
        volumen.poner(volPuesto / (float) Math.max(1, am.getStreamMaxVolume(AudioManager.STREAM_MUSIC)));
    }

    void cronometro(Chronometer c, Act a) {
        c.setBase(SystemClock.elapsedRealtime() + (a.base - System.currentTimeMillis()));
        c.setCountDown(a.cuentaAtras);
        if (puesta) c.start(); else c.stop();   // 1 Hz solo mientras se ve
    }

    ImageView boton(String icono, int ladoDp, View.OnClickListener al) {
        ImageView b = Paneles.de(ctx).botonIcono(icono, ladoDp, al);   // el de los paneles (con onda al tocar)
        LinearLayout.LayoutParams l = (LinearLayout.LayoutParams) b.getLayoutParams();
        l.leftMargin = l.rightMargin = dp(20);
        return b;
    }

    // ---------- desplegar / recoger (muelle, sin layout por cuadro) ----------

    boolean animando() { return morfo != null && morfo.isRunning(); }

    void desplegar() {
        if (principal == null || desplegada) return;
        desplegada = true;
        pintarDesplegada(principal);
        mini.setVisibility(View.GONE);
        if (corrida != null) corrida.cancel();   // la corrida junto al asa movería la tarjeta cada cuadro
        ondas.activa(false);
        medirTarjeta();
        params.width = anchoE; params.height = altoE; params.x = 0;
        capsula.setTranslationX(0);
        actualizarVentana();   // un solo cambio de tamaño al empezar
        publicar();
        expandida.setVisibility(View.VISIBLE);
        animar(1f, null);
    }

    /** Tarjeta: 40 % del lado largo (576 dp en la Surface; la de Apple, 371 pt ×1,5 = 556), igual en horizontal y en
     *  vertical (cabe de sobra); alto a la medida del contenido. true si cambió (solo entonces hay una pasada de layout). */
    boolean medirTarjeta() {
        android.util.DisplayMetrics m = ctx.getResources().getDisplayMetrics();
        int ancho = Math.min(Math.round(0.4f * Math.max(m.widthPixels, m.heightPixels)), m.widthPixels - dp(32));
        expandida.measure(View.MeasureSpec.makeMeasureSpec(ancho, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        int alto = Math.max(altoC * 3, expandida.getMeasuredHeight());
        if (ancho == anchoE && alto == altoE && capsula.getLayoutParams().height == alto) return false;
        anchoE = ancho; altoE = alto;
        for (View v : new View[] { capsula, expandida }) { ViewGroup.LayoutParams l = v.getLayoutParams(); l.width = anchoE; l.height = altoE; v.setLayoutParams(l); }
        return true;
    }

    void recoger() {
        if (!desplegada) return;
        desplegada = false;
        raiz.removeCallbacks(tic);
        animar(0f, () -> {
            expandida.setVisibility(View.INVISIBLE);
            crGrande.stop();
            if (principal != null) ondas.activa(principal.ondas && puesta && !minimizada);
            params.height = altoC;
            pintarMini(segunda);   // ancho y posición de la compacta (y el círculo si hay dos)
            params.width = anchoC + (segunda != null ? hueco + diamMini : 0);
            params.x = xVentana();
            actualizarVentana();   // y otro al terminar
        });
    }

    void animar(float hasta, Runnable fin) {
        if (morfo != null) morfo.cancel();
        morfo = ValueAnimator.ofFloat(capsula.f, hasta);
        morfo.setDuration(hasta > capsula.f ? 520 : 380);
        morfo.setInterpolator(MUELLE);
        morfo.addUpdateListener(v -> {
            float f = (float) v.getAnimatedValue();
            capsula.forma(f);
            compacta.setAlpha(Math.max(0f, 1f - f * 2.5f));
            float e = Math.max(0f, Math.min(1f, (f - 0.35f) / 0.65f));
            expandida.setAlpha(e);
            expandida.setScaleX(0.92f + 0.08f * e); expandida.setScaleY(0.92f + 0.08f * e);
        });
        morfo.addListener(new AnimatorListenerAdapter() {
            boolean cancelada;
            @Override public void onAnimationCancel(Animator a) { cancelada = true; }
            @Override public void onAnimationEnd(Animator a) { if (!cancelada) { capsula.forma(hasta); if (fin != null) fin.run(); } }
        });
        morfo.start();
    }

    /** Muelle amortiguado (ζ=0,7: se pasa ~5 % y se asienta), como la isla de Apple [suposición: Apple no publica sus valores]. */
    static final Interpolator MUELLE = t -> {
        if (t >= 1f) return 1f;
        double w = 13, z = 0.7, wd = w * Math.sqrt(1 - z * z);
        return (float) (1 - Math.exp(-z * w * t) * (Math.cos(wd * t) + z * w / wd * Math.sin(wd * t)));
    };
    static { assert MUELLE.getInterpolation(0f) == 0f && MUELLE.getInterpolation(1f) == 1f && MUELLE.getInterpolation(0.5f) > 0.9f; }

    // ---------- ventana ----------

    void ponerVentana() {
        if (puesta || !Settings.canDrawOverlays(ctx)) return;
        try { wm.addView(raiz, params); puesta = true; } catch (RuntimeException ignored) { }
        if (principal != null) pintarCompacta(principal, false);   // arranca cronómetros y ondas ya visibles
    }

    void quitarVentana() {
        if (!puesta) return;
        if (morfo != null) morfo.cancel();
        desplegada = false;
        capsula.forma(0f);
        compacta.setAlpha(1f);
        expandida.setVisibility(View.INVISIBLE);
        crCompacto.stop(); crGrande.stop(); ondas.activa(false); raiz.removeCallbacks(tic);
        params.width = anchoC; params.height = altoC; params.x = 0;
        try { wm.removeViewImmediate(raiz); } catch (IllegalArgumentException ignored) { }
        puesta = false;
        publicar();
    }

    void actualizarVentana() { if (puesta) try { wm.updateViewLayout(raiz, params); } catch (IllegalArgumentException ignored) { } }

    /** Escondida con la pantalla apagada o bloqueada, a pantalla completa (barra de estado oculta) y con un panel abierto. */
    void aplicarVisibilidad() {
        boolean ocultar = bloqueada || barraOculta && !desplegada || Paneles.de(ctx).abierto != null;
        raiz.setVisibility(ocultar ? View.INVISIBLE : View.VISIBLE);
        publicar();
    }

    void barra(boolean oculta) { if (oculta != barraOculta) { barraOculta = oculta; aplicarVisibilidad(); } }

    /** Asa de ventanas de Android ("x0,x1" o "no"): la compacta se corre a su izquierda, del mismo tamaño, como la cápsula
     *  de Honor junto a la cámara; sin asa (inicio) vuelve al centro. El rectángulo de toques la sigue (publicar). */
    void manija(String m) {
        int x = m.matches("[0-9]{1,5},[0-9]{1,5}") ? Integer.parseInt(m.substring(0, m.indexOf(','))) : -1;
        if (x == manijaX) return;
        manijaX = x;
        if (corrida != null) corrida.cancel();
        if (!puesta || desplegada) { params.x = xVentana(); actualizarVentana(); return; }
        corrida = ValueAnimator.ofInt(params.x, xVentana());
        corrida.setDuration(380);
        corrida.setInterpolator(MUELLE);
        corrida.addUpdateListener(v -> { params.x = (int) v.getAnimatedValue(); actualizarVentana(); });
        corrida.start();
    }

    /** Dónde va la ventana (desde el centro): desplegada, en el centro; con asa, justo a su izquierda; si no, centrada la píldora. */
    int xVentana() {
        if (desplegada) return 0;
        if (manijaX >= 0) return manijaX - hueco - params.width / 2 - ctx.getResources().getDisplayMetrics().widthPixels / 2;
        return mini.getVisibility() == View.VISIBLE ? (hueco + diamMini) / 2 : 0;
    }

    void destruir() { acts.clear(); breve = null; quitarVentana(); }

    // ---------- abrir ----------

    void abrir(Act a) {
        if (a == null) return;
        if (a.abrir != null) { enviar(a.abrir); return; }
        if (a.mc != null) {
            if (a.mc.getSessionActivity() != null) { enviar(a.mc.getSessionActivity()); return; }
            Intent i = ctx.getPackageManager().getLaunchIntentForPackage(a.mc.getPackageName());
            if (i != null) ctx.startActivity(i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        }
    }

    void enviar(PendingIntent pi) {
        if (pi == null) return;
        try {   // Android 14+: quien envía debe permitir abrir la actividad desde segundo plano
            pi.send(ctx, 0, null, null, null, null, ActivityOptions.makeBasic()
                    .setPendingIntentBackgroundActivityStartMode(ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED).toBundle());
        } catch (PendingIntent.CanceledException ignored) { }
    }

    // ---------- piezas ----------

    TextView texto(float sp, boolean seminegrita, int color) {
        TextView t = new TextView(ctx);
        estilo(t, sp, seminegrita, color);
        t.setSingleLine(true);
        t.setEllipsize(TextUtils.TruncateAt.END);
        return t;
    }

    void estilo(TextView t, float sp, boolean seminegrita, int color) {
        t.setTypeface(seminegrita ? semi : normal);
        t.setTextSize(sp);
        t.setTextColor(color);
        t.setFontFeatureSettings("tnum");   // cifras del mismo ancho: el cronómetro no baila
    }

    int dp(float v) { return (int) (v * ctx.getResources().getDisplayMetrics().density + 0.5f); }

    static void redondear(View v, float radio) {
        v.setOutlineProvider(new ViewOutlineProvider() {
            @Override public void getOutline(View view, Outline outline) { outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), radio); }
        });
        v.setClipToOutline(true);
    }

    static String formatearTiempo(long ms) {
        long s = ms / 1000;
        return String.format(Locale.getDefault(), "%d:%02d", s / 60, s % 60);
    }

    /** Color medio de la carátula (para las ondas y la barra), aclarado si es muy oscuro. */
    static int colorMedio(Bitmap b) {
        if (b == null) return ACENTO;
        Bitmap p = Bitmap.createScaledBitmap(b.getConfig() == Bitmap.Config.HARDWARE ? b.copy(Bitmap.Config.ARGB_8888, false) : b, 1, 1, true);
        int c = p.getPixel(0, 0);
        float[] hsv = new float[3];
        Color.colorToHSV(c, hsv);
        hsv[1] = Math.max(hsv[1], 0.45f); hsv[2] = Math.max(hsv[2], 0.85f);
        return Color.HSVToColor(hsv);
    }

    /** La píldora: un rectángulo redondeado negro que pasa de compacta (f=0) a desplegada (f=1); recorta su contenido. */
    class Capsula extends FrameLayout {
        float f;
        final RectF r = new RectF();
        final Paint negro = new Paint(Paint.ANTI_ALIAS_FLAG);
        Capsula(Context c) {
            super(c);
            setWillNotDraw(false);
            negro.setColor(Color.BLACK);
            setOutlineProvider(new ViewOutlineProvider() {
                @Override public void getOutline(View v, Outline o) { calcular(); o.setRoundRect(Math.round(r.left), Math.round(r.top), Math.round(r.right), Math.round(r.bottom), radio()); }
            });
            setClipToOutline(true);
        }
        float anchoDibujo = -1;   // ancho de la compacta mientras se estira (−1: el de ahora)
        float radio() { return altoC / 2f + (dp(40) - altoC / 2f) * Math.max(0f, Math.min(1f, f)); }
        void calcular() {
            float c0 = anchoDibujo > 0 ? anchoDibujo : anchoC;
            float g = Math.min(1f, f);   // el rebote del muelle por encima de 1 lo recortaría la ventana: se queda en la tarjeta
            float w = c0 + (anchoE - c0) * g, h = altoC + (altoE - altoC) * g;
            float cx = getWidth() / 2f;
            r.set(cx - w / 2f, 0, cx + w / 2f, Math.max(altoC * .8f, h));
        }
        void forma(float nueva) { f = nueva; invalidateOutline(); invalidate(); }   // sin medir ni colocar
        @Override protected void onDraw(Canvas c) { calcular(); float ra = radio(); c.drawRoundRect(r, ra, ra, negro); }
    }

    /** Círculo de la segunda actividad (forma mínima). */
    static class Circulo extends View {
        final Paint negro = new Paint(Paint.ANTI_ALIAS_FLAG), anillo = new Paint(Paint.ANTI_ALIAS_FLAG);
        Drawable icono; Bitmap imagen;
        final android.graphics.Path recorte = new android.graphics.Path();
        Circulo(Context c) {
            super(c);
            negro.setColor(Color.BLACK);
            anillo.setStyle(Paint.Style.STROKE);
            anillo.setStrokeWidth(2 * c.getResources().getDisplayMetrics().density);
        }
        void poner(Bitmap b, Drawable d, int color) { imagen = b; icono = d; anillo.setColor(color); invalidate(); }
        @Override protected void onDraw(Canvas c) {
            float r = getWidth() / 2f, m = r * 0.42f;
            c.drawCircle(r, r, r, negro);
            c.drawCircle(r, r, r - anillo.getStrokeWidth(), anillo);
            int l = Math.round(r - m), t = l, d = Math.round(2 * m);
            if (imagen != null) {
                recorte.reset(); recorte.addCircle(r, r, m, android.graphics.Path.Direction.CW);
                c.save(); c.clipPath(recorte);
                c.drawBitmap(imagen, null, new android.graphics.Rect(l, t, l + d, t + d), null);
                c.restore();
            } else if (icono != null) { icono.setBounds(l, t, l + d, t + d); icono.draw(c); }
        }
    }

    /** Ondas de la música, con el color de la carátula; se mueven a 12 cuadros/s solo mientras suena y se ve. */
    static class Ondas extends View {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        int color = ACENTO; boolean viva; long t0;
        final Runnable paso = new Runnable() { @Override public void run() { invalidate(); if (viva) postDelayed(this, 83); } };
        Ondas(Context c) { super(c); p.setStrokeCap(Paint.Cap.ROUND); }
        void activa(boolean si) { if (si == viva) return; viva = si; removeCallbacks(paso); if (si) post(paso); else invalidate(); }
        @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); removeCallbacks(paso); }
        @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); if (viva) post(paso); }
        @Override protected void onDraw(Canvas c) {
            float paso = getWidth() / 4f, h = getHeight(), my = h / 2f, t = SystemClock.uptimeMillis() / 1000f;
            p.setColor(color);
            p.setStrokeWidth(paso * 0.55f);
            for (int i = 0; i < 4; i++) {
                float a = viva ? 0.35f + 0.65f * (0.5f + 0.5f * (float) Math.sin(t * (7 + i * 2.3) + i * 1.7)) : 0.45f;
                float x = paso * (i + 0.5f), y = (my - paso * 0.3f) * a;
                c.drawLine(x, my - y, x, my + y, p);
            }
        }
    }

    /** Anillo de progreso (descargas, instalaciones). */
    static class Anillo extends View {
        final Paint fondo = new Paint(Paint.ANTI_ALIAS_FLAG), frente = new Paint(Paint.ANTI_ALIAS_FLAG);
        int pr;
        final RectF r = new RectF();
        Anillo(Context c) {
            super(c);
            float d = c.getResources().getDisplayMetrics().density;
            for (Paint p : new Paint[] { fondo, frente }) { p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(2.5f * d); p.setStrokeCap(Paint.Cap.ROUND); }
            fondo.setColor(0x33FFFFFF);
        }
        void poner(int p, int color) { pr = p; frente.setColor(color); invalidate(); }
        @Override protected void onDraw(Canvas c) {
            float s = fondo.getStrokeWidth();
            r.set(s, s, getWidth() - s, getHeight() - s);
            c.drawOval(r, fondo);
            c.drawArc(r, -90, 360f * Math.max(0, pr) / 100f, false, frente);
        }
    }

    /** Barra de avance de la tarjeta: fina; con alSoltar (música) se arrastra, engorda mientras tanto y salta al soltar. */
    static class Barra extends View {
        interface Soltar { void en(float f); }
        final Paint fondo = new Paint(Paint.ANTI_ALIAS_FLAG), frente = new Paint(Paint.ANTI_ALIAS_FLAG);
        float pr; boolean arrastrando; Soltar alSoltar;
        Barra(Context c) { super(c); fondo.setColor(0x33FFFFFF); }
        void poner(float p, int color) { pr = Math.max(0f, Math.min(1f, p)); frente.setColor(color); invalidate(); }
        @Override protected void onDraw(Canvas c) {
            float w = getWidth(), my = getHeight() / 2f, g = (arrastrando ? 5 : 3) * getResources().getDisplayMetrics().density;
            c.drawRoundRect(0, my - g, w, my + g, g, g, fondo);
            if (pr > 0) c.drawRoundRect(0, my - g, Math.max(2 * g, w * pr), my + g, g, g, frente);
        }
        @Override public boolean onTouchEvent(MotionEvent e) {
            if (alSoltar == null) return false;
            int ac = e.getActionMasked();
            if (ac == MotionEvent.ACTION_DOWN) { getParent().requestDisallowInterceptTouchEvent(true); arrastrando = true; }
            if (ac == MotionEvent.ACTION_CANCEL) arrastrando = false;
            else pr = Math.max(0f, Math.min(1f, e.getX() / Math.max(1, getWidth())));
            if (ac == MotionEvent.ACTION_UP && arrastrando) { arrastrando = false; alSoltar.en(pr); }
            invalidate();
            return true;
        }
    }
}
