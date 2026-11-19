package com.surfacedual.launcher;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.provider.Settings;
import android.view.DragEvent;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Panel Edge (Samsung One UI, fotos V, W, X, Y): asa casi transparente en el borde derecho, encima de todo. Mantener y arrastrar
 * la mueve arriba/abajo (se recuerda); deslizarla hacia dentro (o tocarla) abre una columna de vidrio con las apps elegidas,
 * el cajón y el lápiz del editor. El visor de Windows toma como "Atrás" los arrastres que empiezan junto al borde: la franja del
 * asa se le publica por el agente ("asa d y0 y1") para que la deje pasar.
 */
class Borde {
    static final String DEFECTO = "app.mihon,com.microsoft.emmx,com.google.android.youtube,com.google.android.apps.bard,com.anthropic.claude,com.google.android.apps.youtube.music";
    static Borde yo;

    final Context ctx;
    final WindowManager wm;
    final SharedPreferences prefs;
    final Paneles p;   // piezas de estilo compartidas (iconos, textos, vidrio)
    View asa, panel, editor, linea;
    WindowManager.LayoutParams lpAsa;

    static Borde de(Context c) {
        if (yo == null) yo = new Borde(c.getApplicationContext());
        return yo;
    }

    Borde(Context c) {
        p = Paneles.de(c);
        ctx = p.ctx;
        wm = ctx.getSystemService(WindowManager.class);
        prefs = c.getSharedPreferences("inicio", Context.MODE_PRIVATE);
        // Al girar (E14, 17/11): el asa vuelve a caber en la nueva altura y el visor recibe su franja (en vertical no se abría).
        ctx.registerComponentCallbacks(new android.content.ComponentCallbacks() {
            int orientacion = ctx.getResources().getConfiguration().orientation;
            @Override public void onConfigurationChanged(android.content.res.Configuration c) {
                if (c.orientation == orientacion) return;
                orientacion = c.orientation;
                cerrarPanel();
                if (asa != null && lpAsa != null) {
                    lpAsa.y = Math.max(p.barraEstado(), Math.min(prefs.getInt("asa_y", p.pantalla()[1] / 4), p.pantalla()[1] - lpAsa.height));
                    try { wm.updateViewLayout(asa, lpAsa); } catch (IllegalArgumentException ignored) { }
                }
                publicar();
            }
            @Override public void onLowMemory() { }
        });
    }

    boolean activo() { return prefs.getBoolean("edge", true); }

    /** Pone o quita el asa según la opción "Paneles Edge" y avisa al visor de su franja. */
    void actualizar() {
        if (linea == null && Settings.canDrawOverlays(ctx)) ponerLinea();
        if (activo() && asa == null && Settings.canDrawOverlays(ctx)) ponerAsa();
        else if (!activo() && asa != null) {
            cerrarPanel();
            try { wm.removeViewImmediate(asa); } catch (IllegalArgumentException ignored) { }
            if (panelHecho != null) try { wm.removeViewImmediate(panelHecho); } catch (IllegalArgumentException ignored) { }
            panelHecho = null; panel = null;
            asa = null;
        }
        publicar();
    }

    void publicar() {
        String orden = asa == null || lpAsa == null ? "asa n 0 0" : "asa d " + lpAsa.y + " " + (lpAsa.y + lpAsa.height);
        Inicio.peticionAgente(ctx, orden, null, null);
    }

    void ponerAsa() {
        int alto = p.dp(140);
        lpAsa = new WindowManager.LayoutParams(p.dp(22), alto, WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN, PixelFormat.TRANSLUCENT);
        lpAsa.gravity = Gravity.TOP | Gravity.END;
        lpAsa.y = Math.max(p.barraEstado(), Math.min(prefs.getInt("asa_y", p.pantalla()[1] / 4), p.pantalla()[1] - alto));
        lpAsa.setTitle("SurfaceDual asa");
        lpAsa.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        lpAsa.setFitInsetsTypes(0);
        View v = new View(ctx) {
            final android.graphics.Paint pin = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);
            float x0, y0, yVentana; boolean moviendo, hecho;
            final Runnable largo = () -> { moviendo = true; animate().scaleX(1.6f).setDuration(120); };
            // Sutil pero visible sobre cualquier fondo (pedido 17/11: casi no se veía): blanco al 60 % con halo oscuro, como One UI.
            { pin.setColor(0x99FFFFFF); pin.setShadowLayer(p.dp(2), 0, 0, 0x66000000); setPivotX(p.dp(22)); }
            @Override protected void onDraw(android.graphics.Canvas c) {   // raya fina y redonda, casi transparente
                float w = p.dp(5), x = getWidth() - w - p.dp(4);
                c.drawRoundRect(x, p.dp(10), x + w, getHeight() - p.dp(10), w / 2, w / 2, pin);
            }
            @Override public boolean onTouchEvent(MotionEvent e) {
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        x0 = e.getRawX(); y0 = e.getRawY(); yVentana = lpAsa.y; moviendo = false; hecho = false;
                        postDelayed(largo, ViewConfiguration.getLongPressTimeout());
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        float dx = e.getRawX() - x0, dy = e.getRawY() - y0;
                        if (moviendo) {
                            lpAsa.y = Math.round(Math.max(p.barraEstado(), Math.min(yVentana + dy, p.pantalla()[1] - lpAsa.height)));
                            try { wm.updateViewLayout(this, lpAsa); } catch (IllegalArgumentException ignored) { }
                        } else if (!hecho && dx < -p.dp(16) && -dx > Math.abs(dy)) { removeCallbacks(largo); hecho = true; abrirPanel(); }
                        else if (Math.abs(dy) > p.dp(12) && !moviendo) removeCallbacks(largo);
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        removeCallbacks(largo);
                        if (moviendo) {
                            moviendo = false;
                            animate().scaleX(1f).setDuration(160);
                            prefs.edit().putInt("asa_y", lpAsa.y).apply();
                            publicar();
                        } else if (!hecho && e.getActionMasked() == MotionEvent.ACTION_UP
                                && Math.abs(e.getRawX() - x0) < p.dp(12) && Math.abs(e.getRawY() - y0) < p.dp(12)) abrirPanel();   // tocarla también abre
                        return true;
                }
                return super.onTouchEvent(e);
            }
        };
        v.setContentDescription("Panel Edge");
        try { wm.addView(v, lpAsa); asa = v; } catch (RuntimeException e) { asa = null; }
    }

    /**
     * Línea de navegación abajo, siempre visible (pedido 18, fotos T y U). Solo se dibuja: el gesto (subir = inicio, subir y
     * mantener = Recientes, mantenerla quieta = Circle to Search) lo detecta el visor en los 48 px de abajo. Tamaño de tableta
     * Pixel (E13, 17/11: la de 136 dp era muy chica) y, al mantenerla, crece y se ilumina antes de abrir Circle to Search.
     * shortcut: también se ve sobre vídeo a pantalla completa; ocultarla con las barras del sistema si molesta.
     */
    android.animation.ValueAnimator lineaAnim;
    float lineaCrece;   // 0 normal, 1 mantenida, más de 1 al abrirse Circle to Search

    void lineaGesto(String g) {
        View v = linea;
        if (v == null) return;
        if (lineaAnim != null) lineaAnim.cancel();
        lineaAnim = g.equals("disparo") ? android.animation.ValueAnimator.ofFloat(lineaCrece, 1.35f, 0f)
                : android.animation.ValueAnimator.ofFloat(lineaCrece, g.equals("mantener") ? 1f : 0f);
        lineaAnim.setDuration(g.equals("mantener") ? 230 : g.equals("disparo") ? 360 : 150);   // mantener: lo que falta hasta los 350 ms
        lineaAnim.setInterpolator(Inicio.DESACELERA);
        lineaAnim.addUpdateListener(a -> { lineaCrece = (float) a.getAnimatedValue(); v.invalidate(); });
        lineaAnim.start();
    }

    void ponerLinea() {
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(p.dp(300), p.dp(20), WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.TRANSLUCENT);
        lp.gravity = Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
        lp.alpha = .8f;   // ventana que no se toca: con más de 0,8 Android bloquea los toques que la atraviesan hacia las apps
        lp.setTitle("SurfaceDual linea");
        lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        lp.setFitInsetsTypes(0);
        View v = new View(ctx) {
            final android.graphics.Paint claro = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG),
                    borde = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);
            { borde.setColor(0x33000000); }   // blanca con un filo oscuro: se ve sobre fondos claros y oscuros
            @Override protected void onDraw(android.graphics.Canvas c) {
                float k = lineaCrece, w = p.dp(200) * (1 + .25f * k), h = p.dp(6) * (1 + .4f * Math.min(k, 1f)), r = h / 2;
                float x = (getWidth() - w) / 2, y = getHeight() - p.dp(6) - h;
                claro.setColor(Math.round(0xD9 + (0xFF - 0xD9) * Math.min(k, 1f)) << 24 | 0xFFFFFF);
                c.drawRoundRect(x - 1, y - 1, x + w + 1, y + h + 1, r + 1, r + 1, borde);
                c.drawRoundRect(x, y, x + w, y + h, r, r, claro);
            }
        };
        try { wm.addView(v, lp); linea = v; } catch (RuntimeException e) { linea = null; }
    }

    List<String> elegidas() {
        String s = prefs.getString("edge_apps", null);
        List<String> l = new ArrayList<>(Arrays.asList((s != null ? s : DEFECTO).split(",")));
        l.removeIf(pkg -> pkg.isEmpty() || etiqueta(pkg) == null);   // las no instaladas no se muestran
        return l;
    }

    void guardar(List<String> l) { prefs.edit().putString("edge_apps", String.join(",", l)).apply(); }

    String etiqueta(String pkg) {
        Inicio.App a = Inicio.instancia == null ? null : Inicio.instancia.buscar(pkg);
        if (a != null) return a.nombre;
        PackageManager pm = ctx.getPackageManager();
        try { return pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString(); }
        catch (PackageManager.NameNotFoundException e) { return null; }
    }

    // ---------- panel (foto W) ----------

    // La ventana del panel se hace una vez y queda puesta e invisible; solo se rehace si cambian las apps elegidas o la
    // pantalla (17/11: crearla en cada apertura daba tirones).
    FrameLayout panelHecho;
    String panelClave;

    void abrirPanel() {
        if (panel != null) return;
        Paneles.de(ctx).cerrar();
        int[] pant = p.pantalla();
        String clave = String.join(",", elegidas()) + "|" + pant[0] + "x" + pant[1] + "|" + ctx.getResources().getConfiguration().fontScale;
        if (panelHecho == null || !clave.equals(panelClave)) {
            if (panelHecho != null) try { wm.removeViewImmediate(panelHecho); } catch (IllegalArgumentException ignored) { }
            panelHecho = hacerPanel();
            panelClave = clave;
            if (panelHecho == null) return;
        }
        FrameLayout raiz = panelHecho;
        View col = raiz.getChildAt(0);
        FrameLayout.LayoutParams l = (FrameLayout.LayoutParams) col.getLayoutParams();
        int centro = lpAsa == null ? pant[1] / 2 : lpAsa.y + lpAsa.height / 2, alto = col.getMeasuredHeight() > 0 ? col.getMeasuredHeight() : col.getHeight();
        int arriba = Math.max(p.barraEstado() + p.dp(8), Math.min(centro - alto / 2, pant[1] - alto - p.dp(16)));
        if (l.topMargin != arriba) { l.topMargin = arriba; col.setLayoutParams(l); }   // junto al asa (se puede haber movido)
        panel = raiz;
        raiz.setVisibility(View.VISIBLE);
        raiz.requestFocus();
        col.setTranslationX(p.dp(120));
        col.setAlpha(0f);
        col.animate().translationX(0).alpha(1f).setDuration(200).setInterpolator(Inicio.DESACELERA).withLayer();
        if (asa != null) asa.animate().alpha(0f).setDuration(120);
    }

    FrameLayout hacerPanel() {
        FrameLayout raiz = new FrameLayout(ctx) {
            @Override public boolean dispatchKeyEvent(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.KEYCODE_BACK || e.getKeyCode() == KeyEvent.KEYCODE_ESCAPE) { if (e.getAction() == KeyEvent.ACTION_UP) cerrarPanel(); return true; }
                return super.dispatchKeyEvent(e);
            }
            @Override public boolean onTouchEvent(MotionEvent e) { if (e.getActionMasked() == MotionEvent.ACTION_UP) cerrarPanel(); return true; }
        };
        raiz.setFocusableInTouchMode(true);
        // Del alto de sus apps (pedido 19/11: con 4 apps, de 4, sin hueco abajo) hasta el 84 % de la pantalla; caben tantas
        // filas como permita ese tope y solo después se pasa a 2 o más columnas.
        List<String> apps = elegidas();
        int[] pant = p.pantalla();
        int tope = Math.round(pant[1] * .84f), resto = p.dp(14 + 52 + 8), ancho = p.dp(ANCHO_CELDA);
        List<LinearLayout> celdas = new ArrayList<>();
        int altoCelda = 1;
        for (String pkg : apps) {
            LinearLayout b = celda(pkg, false);
            b.setOnClickListener(v -> { cerrarPanel(); abrirApp(pkg); });
            b.measure(View.MeasureSpec.makeMeasureSpec(ancho, View.MeasureSpec.EXACTLY), View.MeasureSpec.UNSPECIFIED);
            altoCelda = Math.max(altoCelda, b.getMeasuredHeight());
            celdas.add(b);
        }
        int cols = columnas(apps.size(), (tope - resto) / altoCelda);
        // Por filas (8 apps con 7 de alto = 4 + 4, equilibrado). shortcut: sin tope de columnas; con más de ~80 apps se saldría por la izquierda.
        android.widget.GridLayout rejilla = new android.widget.GridLayout(ctx);
        rejilla.setColumnCount(cols);
        for (LinearLayout b : celdas) rejilla.addView(b, new ViewGroup.LayoutParams(ancho, ViewGroup.LayoutParams.WRAP_CONTENT));
        rejilla.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);   // su alto real (nombres de una o dos líneas)
        int alto = Math.min(tope, resto + rejilla.getMeasuredHeight());
        LinearLayout col = new LinearLayout(ctx);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setGravity(Gravity.CENTER_HORIZONTAL);
        col.setPadding(p.dp(8), p.dp(14), p.dp(8), p.dp(8));
        col.setBackground(new Vidrio.Placa(col, p.dp(32), 0xB31C1C21));
        col.setElevation(p.dp(16));
        col.setClickable(true);
        col.addView(rejilla, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, 0, 1));
        LinearLayout fila = new LinearLayout(ctx);
        fila.setGravity(Gravity.CENTER);
        fila.addView(p.botonIcono("ic_apps", 52, v -> { cerrarPanel(); Paneles.de(ctx).alternar("cajon"); }));
        fila.addView(p.botonIcono("ic_mode_edit", 52, v -> { cerrarPanel(); abrirEditor(); }));
        col.addView(fila);
        int anchoCol = cols * ancho + p.dp(16);
        FrameLayout.LayoutParams l = new FrameLayout.LayoutParams(anchoCol, alto, Gravity.TOP | Gravity.END);
        l.rightMargin = p.dp(14);
        col.measure(View.MeasureSpec.makeMeasureSpec(anchoCol, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(alto, View.MeasureSpec.EXACTLY));
        raiz.addView(col, l);
        raiz.setVisibility(View.INVISIBLE);
        WindowManager.LayoutParams lp = pantallaCompleta("SurfaceDual edge");
        try { wm.addView(raiz, lp); } catch (RuntimeException e) { return null; }
        return raiz;
    }

    static final int ANCHO_CELDA = 108;   // dp por columna del panel y de la columna del editor

    /** Columnas para n apps cuando caben "filas" por columna: una sola columna hasta llenar el alto, luego 2, 3... */
    static int columnas(int n, int filas) { filas = Math.max(1, filas); return Math.max(1, (n + filas - 1) / filas); }

    void cerrarPanel() {
        View v = panel;
        if (v == null) return;
        panel = null;
        if (asa != null) asa.animate().alpha(1f).setDuration(160);
        View col = ((ViewGroup) v).getChildAt(0);
        col.animate().translationX(p.dp(120)).alpha(0f).setDuration(140).setInterpolator(Inicio.ACELERA).withLayer()
                .withEndAction(() -> { if (panel != v) v.setVisibility(View.INVISIBLE); });   // queda puesta para la próxima vez
    }

    WindowManager.LayoutParams pantallaCompleta(String titulo) {
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY, WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN, PixelFormat.TRANSLUCENT);
        lp.setFitInsetsTypes(0);
        lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        lp.setTitle(titulo);
        return lp;
    }

    LinearLayout celda(String pkg, boolean quitar) {
        LinearLayout b = new LinearLayout(ctx);
        b.setOrientation(LinearLayout.VERTICAL);
        b.setGravity(Gravity.CENTER_HORIZONTAL);
        b.setPadding(0, p.dp(4), 0, p.dp(6));
        FrameLayout caja = new FrameLayout(ctx);
        ImageView ic = new ImageView(ctx);
        ic.setImageDrawable(p.iconoPaquete(pkg));
        caja.addView(ic, new FrameLayout.LayoutParams(p.dp(64), p.dp(64), Gravity.CENTER));   // = Inicio.iconoUniforme: nítido
        if (quitar) {
            TextView menos = new TextView(ctx);
            menos.setText("−");
            menos.setGravity(Gravity.CENTER);
            menos.setTextColor(Color.WHITE);
            menos.setTextSize(14);
            GradientDrawable g = new GradientDrawable();
            g.setShape(GradientDrawable.OVAL);
            g.setColor(0xFFE5484D);
            menos.setBackground(g);
            menos.setOnClickListener(v -> { List<String> l = elegidas(); l.remove(pkg); guardar(l); rehacerEditor(); });
            caja.addView(menos, new FrameLayout.LayoutParams(p.dp(22), p.dp(22), Gravity.TOP | Gravity.START));
        }
        b.addView(caja, new LinearLayout.LayoutParams(p.dp(72), p.dp(70)));
        TextView t = p.texto(etiqueta(pkg), 13, false, Color.WHITE);
        t.setGravity(Gravity.CENTER);
        t.setPadding(p.dp(2), p.dp(2), p.dp(2), 0);
        b.addView(t, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        b.setBackground(new android.graphics.drawable.RippleDrawable(android.content.res.ColorStateList.valueOf(0x26FFFFFF), null, p.fondo(Color.WHITE, 0, 18)));
        return b;
    }

    void abrirApp(String pkg) {
        Inicio.App a = Inicio.instancia == null ? null : Inicio.instancia.buscar(pkg);
        if (a != null) { Inicio.instancia.abrir(a); return; }
        Intent i = ctx.getPackageManager().getLaunchIntentForPackage(pkg);
        if (i != null) ctx.startActivity(i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
    }

    // ---------- editor (foto X): todas las apps a la izquierda, la columna elegida a la derecha ----------

    void abrirEditor() {
        if (editor != null) return;
        FrameLayout raiz = new FrameLayout(ctx) {
            @Override public boolean dispatchKeyEvent(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.KEYCODE_BACK || e.getKeyCode() == KeyEvent.KEYCODE_ESCAPE) { if (e.getAction() == KeyEvent.ACTION_UP) cerrarEditor(); return true; }
                return super.dispatchKeyEvent(e);
            }
        };
        raiz.setFocusableInTouchMode(true);
        raiz.addView(new Vidrio.Fondo(ctx, 0xE6000000), Inicio.lleno());
        llenarEditor(raiz);
        try { wm.addView(raiz, pantallaCompleta("SurfaceDual edge editor")); editor = raiz; } catch (RuntimeException e) { return; }
        raiz.requestFocus();
        raiz.setAlpha(0f);
        raiz.animate().alpha(1f).setDuration(167).setInterpolator(Inicio.DESACELERA).withLayer();
    }

    void rehacerEditor() {
        if (editor == null) return;
        FrameLayout r = (FrameLayout) editor;
        while (r.getChildCount() > 1) r.removeViewAt(1);
        llenarEditor(r);
    }

    void llenarEditor(FrameLayout raiz) {
        List<String> elegidas = elegidas();
        LinearLayout todo = new LinearLayout(ctx);
        todo.setPadding(p.dp(32), p.barraEstado() + p.dp(16), p.dp(24), p.dp(24));
        LinearLayout izq = new LinearLayout(ctx);
        izq.setOrientation(LinearLayout.VERTICAL);
        LinearLayout cab = p.fila();
        LinearLayout titulos = new LinearLayout(ctx);
        titulos.setOrientation(LinearLayout.VERTICAL);
        titulos.addView(p.texto("Aplic.", 24, true, Inicio.ACENTO));
        titulos.addView(p.texto(elegidas.size() + " añadida(s) · toca o arrastra para añadir", 13, false, Inicio.TEXTO2));
        cab.addView(titulos, p.peso());
        TextView listo = p.pastilla("Listo");
        listo.setOnClickListener(v -> cerrarEditor());
        cab.addView(listo);
        izq.addView(cab);
        // Todas las apps del cajón (las ya añadidas, atenuadas)
        PackageManager pm = ctx.getPackageManager();
        List<String> todas = new ArrayList<>();
        if (Inicio.instancia != null) for (Inicio.App a : Inicio.instancia.apps) { if (!todas.contains(a.pkg)) todas.add(a.pkg); }
        else for (ResolveInfo r : pm.queryIntentActivities(new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER), 0))
            if (!todas.contains(r.activityInfo.packageName) && !r.activityInfo.packageName.equals(ctx.getPackageName())) todas.add(r.activityInfo.packageName);
        GridView g = new GridView(ctx);
        g.setNumColumns(p.pantalla()[0] > p.pantalla()[1] ? 8 : 5);
        g.setVerticalSpacing(p.dp(8));
        g.setSelector(android.R.color.transparent);
        g.setVerticalScrollBarEnabled(false);
        g.setAdapter(new android.widget.BaseAdapter() {
            @Override public int getCount() { return todas.size(); }
            @Override public Object getItem(int i) { return todas.get(i); }
            @Override public long getItemId(int i) { return i; }
            @Override public View getView(int i, View r, ViewGroup padre) {
                String pkg = todas.get(i);
                LinearLayout c = celda(pkg, false);
                c.setAlpha(elegidas.contains(pkg) ? .3f : 1f);
                c.setOnClickListener(v -> { List<String> l = elegidas(); if (!l.contains(pkg)) { l.add(pkg); guardar(l); rehacerEditor(); } });
                c.setOnLongClickListener(v -> { Inicio.rebotar(v); v.startDragAndDrop(ClipData.newPlainText("app", pkg), new View.DragShadowBuilder(v), pkg, 0); return true; });
                return c;
            }
        });
        LinearLayout.LayoutParams lg = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1);
        lg.topMargin = p.dp(20);
        izq.addView(g, lg);
        todo.addView(izq, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1));

        // Columna elegida: "−" para quitar; arrastrar dentro reordena (se suelta a la altura deseada).
        LinearLayout col = new LinearLayout(ctx);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setGravity(Gravity.CENTER_HORIZONTAL);
        col.setPadding(p.dp(8), p.dp(14), p.dp(8), p.dp(14));
        for (String pkg : elegidas) {
            LinearLayout c = celda(pkg, true);
            c.setOnLongClickListener(v -> { Inicio.rebotar(v); v.startDragAndDrop(ClipData.newPlainText("app", pkg), new View.DragShadowBuilder(v), pkg, 0); return true; });
            col.addView(c);
        }
        View hueco = new View(ctx);   // "hueco para añadir", como la foto
        hueco.setBackground(p.fondo(0, 0x66FFFFFF, 18));
        LinearLayout.LayoutParams lh = new LinearLayout.LayoutParams(p.dp(64), p.dp(64));
        lh.topMargin = p.dp(8);
        col.addView(hueco, lh);
        ScrollView sv = new ScrollView(ctx);
        sv.setVerticalScrollBarEnabled(false);
        sv.addView(col);
        sv.setBackground(new Vidrio.Placa(sv, p.dp(28), 0xCC1C1C21));
        sv.setClipToOutline(true);
        sv.setOnDragListener((v, e) -> {
            if (!(e.getLocalState() instanceof String)) return false;
            if (e.getAction() == DragEvent.ACTION_DROP) {
                String pkg = (String) e.getLocalState();
                float y = e.getY() + sv.getScrollY();
                int pos = 0;
                for (int i = 0; i < col.getChildCount() - 1; i++) if (y > col.getChildAt(i).getTop() + col.getChildAt(i).getHeight() / 2f) pos = i + 1;
                List<String> l = elegidas();
                int antes = l.indexOf(pkg);
                if (antes >= 0) { l.remove(antes); if (antes < pos) pos--; }
                l.add(Math.min(pos, l.size()), pkg);
                guardar(l);
                sv.post(this::rehacerEditor);
            }
            return true;
        });
        // Soltar una de la columna sobre la lista de todas = quitarla.
        g.setOnDragListener((v, e) -> {
            if (!(e.getLocalState() instanceof String)) return false;
            if (e.getAction() == DragEvent.ACTION_DROP) {
                List<String> l = elegidas();
                if (l.remove((String) e.getLocalState())) { guardar(l); g.post(this::rehacerEditor); }
            }
            return true;
        });
        LinearLayout.LayoutParams lc = new LinearLayout.LayoutParams(p.dp(ANCHO_CELDA + 16), ViewGroup.LayoutParams.WRAP_CONTENT);
        lc.leftMargin = p.dp(28);
        lc.gravity = Gravity.CENTER_VERTICAL;
        todo.addView(sv, lc);
        raiz.addView(todo, Inicio.lleno());
    }

    void cerrarEditor() {
        View v = editor;
        if (v == null) return;
        editor = null;
        v.animate().alpha(0f).setDuration(140).setInterpolator(Inicio.ACELERA).withLayer()
                .withEndAction(() -> { try { wm.removeViewImmediate(v); } catch (IllegalArgumentException ignored) { } });
    }

    static { assert DEFECTO.split(",").length == 6 && columnas(6, 7) == 1 && columnas(7, 7) == 1 && columnas(8, 7) == 2 && columnas(3, 0) == 3 && columnas(0, 7) == 1; }
}
