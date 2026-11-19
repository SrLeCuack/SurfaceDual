package com.surfacedual.launcher;

import android.app.Activity;
import android.appwidget.AppWidgetHost;
import android.appwidget.AppWidgetHostView;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProviderInfo;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONException;

/**
 * Widgets de Android en las páginas del inicio (AppWidgetHost). Preferencia "paginas": JSON [[id, ...], ...]; la página 0 es
 * la de las apps. RELOJ (-1) es nuestro reloj grande, un widget más que se puede quitar y volver a poner.
 */
class Widgets {
    static final int PEDIR_ENLACE = 41, PEDIR_CONFIG = 42, RELOJ = -1;
    static final String YT_MUSIC = "com.google.android.apps.youtube.music";

    final Inicio ini;
    final AppWidgetManager mgr;
    final AppWidgetHost host;

    Widgets(Inicio i) {
        ini = i;
        mgr = AppWidgetManager.getInstance(i);
        // Al abrir o cerrar un panel (otra ventana) el inicio pierde y recupera el foco de ventana, y los botones de los
        // widgets funden su fondo unos 10 cuadros: el inicio se redibujaba a la vez que el panel y el RenderThread cambiaba
        // de superficie en cada cuadro (A/B 17/11, 12 corridas: tirones de paneles 8,3 % -> 4,2 %). Al widget no se le
        // avisa de la pérdida de foco mientras hay un panel abierto (sí de la ganancia: sin cambio de estado no hay fundido).
        host = new AppWidgetHost(i, 1024) {
            @Override protected AppWidgetHostView onCreateView(android.content.Context c, int id, AppWidgetProviderInfo info) {
                return new AppWidgetHostView(c) {
                    @Override public void dispatchWindowFocusChanged(boolean foco) {
                        // Solo se calla la pérdida por un panel nuestro; si el inicio pasa detrás de otra app, el widget se entera.
                        if (foco || FOCO_WIDGETS || Paneles.de(getContext()).abierto == null) super.dispatchWindowFocusChanged(foco);
                    }
                };
            }
        };
    }

    /** setprop debug.sd.launcher.widget_foco 1 (y reiniciar el launcher) = los widgets ven el foco como antes. */
    static final boolean FOCO_WIDGETS = prop("debug.sd.launcher.widget_foco").equals("1");
    static String prop(String nombre) {   // API oculta por reflexión: cualquier fallo cuenta como sin valor
        try { return (String) Class.forName("android.os.SystemProperties").getMethod("get", String.class, String.class).invoke(null, nombre, ""); }
        catch (Throwable e) { return ""; }
    }

    /** Páginas guardadas; la primera vez, el reloj y los widgets que ya hubiera (formato viejo "widgets"). */
    List<List<Integer>> paginas() {
        List<List<Integer>> r = new ArrayList<>();
        String s = ini.prefs.getString("paginas", null);
        if (s != null) try {
            JSONArray a = new JSONArray(s);
            for (int i = 0; i < a.length(); i++) {
                JSONArray p = a.getJSONArray(i);
                List<Integer> l = new ArrayList<>();
                for (int j = 0; j < p.length(); j++) l.add(p.getInt(j));
                r.add(l);
            }
        } catch (JSONException e) { r.clear(); }   // corrupta: se empieza de cero
        if (r.isEmpty()) {
            List<Integer> p0 = new ArrayList<>();
            p0.add(RELOJ);
            for (String x : ini.prefs.getString("widgets", "").split(","))
                try { p0.add(Integer.parseInt(x)); } catch (NumberFormatException e) { }
            r.add(p0);
        }
        return r;
    }

    void guardar(List<List<Integer>> p) {
        JSONArray a = new JSONArray();
        for (List<Integer> l : p) a.put(new JSONArray(l));
        ini.prefs.edit().putString("paginas", a.toString()).remove("widgets").apply();
    }

    List<Integer> ids() {
        List<Integer> l = new ArrayList<>();
        for (List<Integer> p : paginas()) for (int id : p) if (id >= 0) l.add(id);
        return l;
    }

    /** Olvida el id (lo libera en el host y lo saca de su página) sin redibujar. */
    void olvidar(int id) {
        if (id >= 0) host.deleteAppWidgetId(id);
        List<List<Integer>> p = paginas();
        for (List<Integer> l : p) l.remove(Integer.valueOf(id));
        guardar(p);
    }

    int altoDe(int id) {
        if (id == RELOJ) return ini.dp(190);
        AppWidgetProviderInfo info = mgr.getAppWidgetInfo(id);
        return info != null && info.minHeight > 0 ? info.minHeight : ini.dp(110);
    }

    /** Columna con esos widgets; anchoMax en px (el widget se recorta a la columna). */
    LinearLayout vista(List<Integer> ids, int anchoMax, int pagina) {
        LinearLayout col = new LinearLayout(ini);
        col.setOrientation(LinearLayout.VERTICAL);
        float d = ini.getResources().getDisplayMetrics().density;
        for (int id : ids) {
            FrameLayout marco = marcoPulsacionLarga(id, pagina);
            int w, h;
            if (id == RELOJ) {
                marco.addView(ini.reloj());
                w = anchoMax; h = ViewGroup.LayoutParams.WRAP_CONTENT;
            } else {
                // info == null puede ser pasajero (el proveedor se está actualizando): no se borra el id, solo se muestra la tarjeta de error con "Quitar"
                AppWidgetProviderInfo info = mgr.getAppWidgetInfo(id);
                w = Math.max(Math.min(info != null && info.minWidth > 0 ? info.minWidth : ini.dp(250), anchoMax), Math.min(anchoMax, ini.dp(300)));
                h = Math.max(info != null && info.minHeight > 0 ? info.minHeight : ini.dp(110), ini.dp(100));
                try {
                    AppWidgetHostView hv = host.createView(ini, id, info);
                    hv.updateAppWidgetSize(new Bundle(), (int) (w / d), (int) (h / d), (int) (w / d), (int) (h / d));
                    marco.addView(hv, Inicio.lleno());
                } catch (RuntimeException e) {   // proveedor roto o ausente: no tumba el launcher
                    TextView t = new TextView(ini);
                    ini.estilo(t, 14, false, Inicio.TEXTO2);
                    t.setGravity(Gravity.CENTER);
                    t.setText("Widget no disponible (mantén pulsado para quitarlo)");
                    t.setBackground(ini.fondo(Inicio.TARJETA, Inicio.BORDE, 24));
                    marco.addView(t, Inicio.lleno());
                }
            }
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(w, h);
            lp.bottomMargin = ini.dp(16);
            col.addView(marco, lp);
        }
        return col;
    }

    /** Borra los ids sin proveedor; solo se llama cuando un paquete se quitó de verdad (no al actualizarse). */
    void limpiarSinProveedor() {
        for (int id : ids()) if (mgr.getAppWidgetInfo(id) == null) olvidar(id);
    }

    /** Libera en el host los ids que no están guardados (huérfanos de un agregado interrumpido). */
    void reconciliar() {
        List<Integer> mios = ids();
        int pend = ini.prefs.getInt("widget_pendiente", -1);
        for (int id : host.getAppWidgetIds()) if (!mios.contains(id) && id != pend) host.deleteAppWidgetId(id);
    }

    // Los widgets suelen tener hijos pulsables que se comen la pulsación larga: se vigila en dispatchTouchEvent y,
    // al reconocerla, se cancela el toque del hijo para que el widget no reciba además el clic.
    FrameLayout marcoPulsacionLarga(int id, int pagina) {
        return new FrameLayout(ini) {
            float x0, y0;
            boolean largaHecha;
            MotionEvent ultimo;
            final Runnable largo = () -> {
                largaHecha = true;
                MotionEvent c = MotionEvent.obtain(ultimo);
                c.setAction(MotionEvent.ACTION_CANCEL);
                super.dispatchTouchEvent(c);
                c.recycle();
                Inicio.rebotar(this);
                menu(this, id, pagina);
            };
            { setClickable(true); }   // así el marco recibe también CANCEL si el ScrollView empieza a desplazarse
            @Override public boolean dispatchTouchEvent(MotionEvent e) {
                if (largaHecha) {   // se traga el resto del gesto
                    if (e.getActionMasked() == MotionEvent.ACTION_UP || e.getActionMasked() == MotionEvent.ACTION_CANCEL) largaHecha = false;
                    return true;
                }
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        x0 = e.getX(); y0 = e.getY();
                        if (ultimo != null) ultimo.recycle();
                        ultimo = MotionEvent.obtain(e);
                        postDelayed(largo, ViewConfiguration.getLongPressTimeout());
                        break;
                    case MotionEvent.ACTION_MOVE:
                        int t = ViewConfiguration.get(ini).getScaledTouchSlop();
                        if (Math.abs(e.getX() - x0) > t || Math.abs(e.getY() - y0) > t) removeCallbacks(largo);
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                    case MotionEvent.ACTION_POINTER_DOWN:
                        removeCallbacks(largo);
                        break;
                }
                return super.dispatchTouchEvent(e);
            }
            @Override protected void onDetachedFromWindow() { removeCallbacks(largo); super.onDetachedFromWindow(); }
        };
    }

    void menu(View ancla, int id, int pagina) {
        List<List<Integer>> p = paginas();
        PopupMenu m = new PopupMenu(ini, ancla);
        m.getMenu().add(0, 1, 0, "Quitar");
        m.getMenu().add(0, 2, 1, "Subir");
        m.getMenu().add(0, 3, 2, "Bajar");
        if (pagina > 0) m.getMenu().add(0, 4, 3, "Mover a la página anterior");
        m.getMenu().add(0, 5, 4, pagina < p.size() - 1 ? "Mover a la página siguiente" : "Mover a una página nueva");
        m.setOnMenuItemClickListener(it -> {
            if (ini.bloqueado()) return true;
            int k = it.getItemId();
            if (k == 1) { olvidar(id); ini.mostrar(); return true; }
            if (pagina >= p.size()) return true;
            List<Integer> l = p.get(pagina);
            int i = l.indexOf(id);
            if (i < 0) return true;
            if (k == 2 && i > 0) { l.remove(i); l.add(i - 1, id); }
            else if (k == 3 && i < l.size() - 1) { l.remove(i); l.add(i + 1, id); }
            else if (k == 4) { l.remove(i); p.get(pagina - 1).add(id); }
            else if (k == 5) { l.remove(i); if (pagina == p.size() - 1) p.add(new ArrayList<>()); p.get(pagina + 1).add(id); }
            guardar(p);
            ini.mostrar();
            if (k >= 4) ini.carril.ir(ini.carril.base() + pagina + (k == 4 ? -1 : 1), true);
            return true;
        });
        ini.mostrarMenu(m, ancla);
    }

    // ---------- agregar ----------

    List<AppWidgetProviderInfo> de(String pkg) {
        List<AppWidgetProviderInfo> l = new ArrayList<>();
        for (AppWidgetProviderInfo i : mgr.getInstalledProviders())
            if ((i.widgetCategory & AppWidgetProviderInfo.WIDGET_CATEGORY_HOME_SCREEN) != 0 && (pkg == null || pkg.equals(i.provider.getPackageName()))) l.add(i);
        return l;
    }

    /** Primera vez: el widget de YouTube Music debajo del reloj (en silencio: el agente concede "grantbind"; si no, se reintenta). */
    void porOmision() {
        if (ini.prefs.getBoolean("ytm_hecho", false)) return;
        List<AppWidgetProviderInfo> l = de(YT_MUSIC);
        if (l.isEmpty()) return;
        AppWidgetProviderInfo info = l.get(0);
        for (AppWidgetProviderInfo i : l) if (i.minHeight < info.minHeight && i.minWidth >= ini.dp(200)) info = i;   // el más bajo (barra de reproducción)
        int id = host.allocateAppWidgetId();
        boolean ok;
        try { ok = mgr.bindAppWidgetIdIfAllowed(id, info.provider); } catch (RuntimeException e) { ok = false; }
        if (!ok) { host.deleteAppWidgetId(id); return; }
        List<List<Integer>> p = paginas();
        List<Integer> p0 = p.get(0);
        int r = p0.indexOf(RELOJ);
        p0.add(r < 0 ? p0.size() : r + 1, id);
        guardar(p);
        ini.prefs.edit().putBoolean("ytm_hecho", true).apply();
        ini.mostrar();
    }

    void selector(int pagina) { selector(pagina, null); }

    /** Selector como la foto I: buscador arriba, apps con widgets a la izquierda y sus vistas previas a la derecha. */
    void selector(int pagina, String elegida) {
        PackageManager pm = ini.getPackageManager();
        Map<String, List<AppWidgetProviderInfo>> porApp = new LinkedHashMap<>();
        List<AppWidgetProviderInfo> todos = de(null);
        todos.sort((a, b) -> etiquetaApp(pm, a.provider.getPackageName()).compareToIgnoreCase(etiquetaApp(pm, b.provider.getPackageName())));
        porApp.put(ini.getPackageName(), new ArrayList<>());   // el reloj propio va primero
        for (AppWidgetProviderInfo i : todos) porApp.computeIfAbsent(i.provider.getPackageName(), k -> new ArrayList<>()).add(i);

        LinearLayout cont = new LinearLayout(ini);
        cont.setOrientation(LinearLayout.VERTICAL);
        cont.setPadding(0, ini.dp(8), 0, ini.dp(24));
        EditText buscar = new EditText(ini);
        buscar.setHint("Buscar");
        ini.estilo(buscar, 15, false, Color.WHITE);
        buscar.setHintTextColor(Inicio.TEXTO3);
        buscar.setSingleLine(true);
        buscar.setPadding(ini.dp(20), 0, ini.dp(20), 0);
        buscar.setBackground(new Vidrio.Placa(buscar, ini.dp(24), 0xB326262C));
        cont.addView(buscar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ini.dp(48)));
        LinearLayout cuerpo = new LinearLayout(ini);
        LinearLayout.LayoutParams lc = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1);
        lc.topMargin = ini.dp(16);
        cont.addView(cuerpo, lc);
        LinearLayout lista = new LinearLayout(ini);
        lista.setOrientation(LinearLayout.VERTICAL);
        lista.setPadding(ini.dp(8), ini.dp(8), ini.dp(8), ini.dp(8));
        ScrollView svl = new ScrollView(ini);
        svl.setVerticalScrollBarEnabled(false);
        svl.addView(lista);
        svl.setBackground(new Vidrio.Placa(svl, ini.dp(24), 0xA6141418));
        svl.setClipToOutline(true);
        cuerpo.addView(svl, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 4));
        LinearLayout previas = new LinearLayout(ini);
        previas.setOrientation(LinearLayout.VERTICAL);
        previas.setGravity(Gravity.CENTER_HORIZONTAL);
        previas.setPadding(ini.dp(20), ini.dp(24), ini.dp(20), ini.dp(24));
        ScrollView svp = new ScrollView(ini);
        svp.setVerticalScrollBarEnabled(false);
        svp.addView(previas);
        svp.setBackground(new Vidrio.Placa(svp, ini.dp(24), 0x2EFFFFFF));
        svp.setClipToOutline(true);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 5);
        lp.leftMargin = ini.dp(16);
        cuerpo.addView(svp, lp);

        View[] marcada = new View[1];
        Runnable[] llenarLista = new Runnable[1];
        llenarLista[0] = () -> {
            String q = Inicio.normalizar(buscar.getText().toString().trim());
            lista.removeAllViews();
            View primera = null;
            for (Map.Entry<String, List<AppWidgetProviderInfo>> e : porApp.entrySet()) {
                String pkg = e.getKey();
                boolean propio = pkg.equals(ini.getPackageName());
                String nombre = propio ? "Surface Dual" : etiquetaApp(pm, pkg);
                List<AppWidgetProviderInfo> ws = new ArrayList<>();
                for (AppWidgetProviderInfo i : e.getValue())
                    if (q.isEmpty() || Inicio.normalizar(nombre).contains(q) || Inicio.normalizar(i.loadLabel(pm)).contains(q)) ws.add(i);
                if (propio ? !(q.isEmpty() || "reloj surface dual".contains(q)) : ws.isEmpty()) continue;
                LinearLayout f = new LinearLayout(ini);
                f.setGravity(Gravity.CENTER_VERTICAL);
                f.setPadding(ini.dp(12), 0, ini.dp(16), 0);
                f.setMinimumHeight(ini.dp(60));
                ImageView ic = new ImageView(ini);
                Inicio.App a = ini.buscar(pkg);
                ic.setImageDrawable(propio ? ini.ico("ic_apps") : a != null ? a.icono : iconoApp(pm, pkg));
                f.addView(ic, new LinearLayout.LayoutParams(ini.dp(40), ini.dp(40)));
                TextView n = new TextView(ini);
                ini.estilo(n, 16, true, Color.WHITE);
                n.setText(nombre);
                n.setSingleLine(true);
                n.setPadding(ini.dp(16), 0, ini.dp(8), 0);
                f.addView(n, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
                TextView cuenta = new TextView(ini);
                ini.estilo(cuenta, 13, false, Inicio.TEXTO3);
                cuenta.setText(String.valueOf(propio ? 1 : ws.size()));
                f.addView(cuenta);
                f.setOnClickListener(v -> {
                    if (marcada[0] != null) marcada[0].setBackground(null);
                    v.setBackground(ini.fondo(0x1FFFFFFF, 0, 16));
                    marcada[0] = v;
                    llenarPrevias(previas, propio ? null : ws, pagina);
                });
                lista.addView(f);
                if (primera == null || pkg.equals(elegida)) primera = f;
            }
            if (primera != null) primera.performClick();
        };
        buscar.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) { }
            @Override public void onTextChanged(CharSequence s, int a, int b, int c) { }
            @Override public void afterTextChanged(Editable s) { llenarLista[0].run(); }
        });
        llenarLista[0].run();
        ini.abrirHoja("Widgets", cont, false);
    }

    /** Vistas previas (imagen del proveedor, o su icono) decodificadas fuera del hilo de la interfaz. null = el reloj propio. */
    void llenarPrevias(LinearLayout previas, List<AppWidgetProviderInfo> ws, int pagina) {
        previas.removeAllViews();
        if (ws == null) {
            View r = ini.reloj();
            r.setScaleX(.6f); r.setScaleY(.6f);
            previas.addView(previa(r, "Reloj y fecha", v -> agregarPropio(pagina)));
            return;
        }
        int dpi = ini.getResources().getDisplayMetrics().densityDpi;
        PackageManager pm = ini.getPackageManager();
        for (AppWidgetProviderInfo info : ws) {
            ImageView img = new ImageView(ini);
            img.setScaleType(ImageView.ScaleType.FIT_CENTER);
            img.setAdjustViewBounds(true);
            img.setMaxHeight(ini.dp(220));
            img.setMinimumHeight(ini.dp(96));
            int cw = Math.max(1, (info.minWidth + ini.dp(73) - 1) / ini.dp(73)), ch = Math.max(1, (info.minHeight + ini.dp(118) - 1) / ini.dp(118));
            previas.addView(previa(img, info.loadLabel(pm) + "  ·  " + cw + " x " + ch, v -> { ini.cerrarHoja(); enlazar(info, pagina); }));
            Vidrio.hilo.execute(() -> {
                Drawable d = null;
                try { d = info.loadPreviewImage(ini, dpi); } catch (RuntimeException ignored) { }
                if (d == null) try { d = info.loadIcon(ini, dpi); } catch (RuntimeException ignored) { }
                Drawable fin = d;
                if (fin != null) Vidrio.principal.post(() -> img.setImageDrawable(fin));
            });
        }
    }

    View previa(View contenido, CharSequence texto, View.OnClickListener al) {
        LinearLayout c = new LinearLayout(ini);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setGravity(Gravity.CENTER_HORIZONTAL);
        c.setPadding(ini.dp(12), ini.dp(12), ini.dp(12), ini.dp(12));
        c.setBackground(ini.seleccionable());
        c.addView(contenido, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        TextView t = new TextView(ini);
        ini.estilo(t, 13, true, Inicio.TEXTO2);
        t.setText(texto);
        t.setGravity(Gravity.CENTER);
        t.setPadding(0, ini.dp(10), 0, 0);
        c.addView(t);
        c.setOnClickListener(al);
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        l.bottomMargin = ini.dp(12);
        c.setLayoutParams(l);
        return c;
    }

    void agregarPropio(int pagina) {
        ini.cerrarHoja();
        List<List<Integer>> p = paginas();
        for (List<Integer> l : p) l.remove(Integer.valueOf(RELOJ));   // un solo reloj propio
        p.get(Math.max(0, Math.min(pagina, p.size() - 1))).add(0, RELOJ);
        guardar(p);
        ini.mostrar();
    }

    static String etiquetaApp(PackageManager pm, String pkg) {
        try { return pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString(); }
        catch (PackageManager.NameNotFoundException e) { return pkg; }
    }

    static Drawable iconoApp(PackageManager pm, String pkg) {
        try { return pm.getApplicationIcon(pkg); } catch (PackageManager.NameNotFoundException e) { return pm.getDefaultActivityIcon(); }
    }

    void enlazar(AppWidgetProviderInfo info, int pagina) {
        descartarPendiente();   // un agregado anterior sin terminar
        int id = host.allocateAppWidgetId();
        // en preferencias: el launcher puede recrearse mientras el sistema pide permiso
        ini.prefs.edit().putInt("widget_pendiente", id).putInt("widget_pagina", pagina).apply();
        if (mgr.bindAppWidgetIdIfAllowed(id, info.provider)) { configurar(id); return; }
        try {
            ini.startActivityForResult(new Intent(AppWidgetManager.ACTION_APPWIDGET_BIND)
                    .putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, id)
                    .putExtra(AppWidgetManager.EXTRA_APPWIDGET_PROVIDER, info.provider), PEDIR_ENLACE);
        } catch (ActivityNotFoundException | SecurityException e) {
            descartarPendiente();
            Toast.makeText(ini, "El sistema no permite agregar este widget", Toast.LENGTH_SHORT).show();
        }
    }

    void configurar(int id) {
        AppWidgetProviderInfo info = mgr.getAppWidgetInfo(id);
        if (info != null && info.configure != null && (info.widgetFeatures & AppWidgetProviderInfo.WIDGET_FEATURE_CONFIGURATION_OPTIONAL) == 0) {
            try { host.startAppWidgetConfigureActivityForResult(ini, id, 0, PEDIR_CONFIG, null); return; }
            catch (ActivityNotFoundException | SecurityException e) { descartarPendiente(); return; }
        }
        agregar(id);
    }

    void agregar(int id) {
        int pagina = ini.prefs.getInt("widget_pagina", 0);
        ini.prefs.edit().remove("widget_pendiente").apply();
        List<List<Integer>> p = paginas();
        p.get(Math.max(0, Math.min(pagina, p.size() - 1))).add(id);
        guardar(p);
        ini.mostrar();
    }

    void descartarPendiente() {
        int id = ini.prefs.getInt("widget_pendiente", -1);
        ini.prefs.edit().remove("widget_pendiente").apply();
        if (id >= 0) host.deleteAppWidgetId(id);
    }

    /** Devuelve true si el resultado era de este módulo. */
    boolean resultado(int pedido, int res) {
        if (pedido != PEDIR_ENLACE && pedido != PEDIR_CONFIG) return false;
        int id = ini.prefs.getInt("widget_pendiente", -1);
        if (id < 0) return true;
        if (res != Activity.RESULT_OK) descartarPendiente();
        else if (pedido == PEDIR_ENLACE) configurar(id);
        else agregar(id);
        return true;
    }
}
