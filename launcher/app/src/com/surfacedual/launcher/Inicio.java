package com.surfacedual.launcher;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.WallpaperManager;
import android.appwidget.AppWidgetProviderInfo;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ShortcutInfo;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.AdaptiveIconDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.provider.Settings;
import android.speech.RecognizerIntent;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Log;
import android.util.TypedValue;
import android.view.DragEvent;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.animation.OvershootInterpolator;
import android.view.animation.PathInterpolator;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.GridView;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.PopupWindow;
import android.widget.ScrollView;
import android.widget.TextClock;
import android.widget.TextView;
import android.widget.Toast;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;
import java.text.Collator;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Pantalla de inicio del modo Android (como el Galaxy Tab del usuario, fotos F7/referencia_tab, con aspecto Windows 11):
 * páginas que se deslizan (y a la izquierda Discover), accesos directos que el usuario elige, cajón de apps al deslizar hacia
 * arriba (en esta misma ventana: así una app se arrastra del cajón al inicio), menús de mantener pulsado con los atajos de
 * cada app, selección múltiple, carpetas y modo edición al pellizcar. Sobre el fondo de pantalla del sistema.
 */
public class Inicio extends Activity {
    static final int ACENTO = 0xFF60CDFF, TARJETA = 0xFF121212, BORDE = 0x1AFFFFFF, TEXTO2 = 0xBFFFFFFF, TEXTO3 = 0x8AFFFFFF;
    static final String DOCK_INICIAL = "app.mihon,com.crunchyroll.crunchyroid,com.google.android.youtube,com.netflix.mediaclient,com.android.vending";
    static final String INICIO_INICIAL = "com.android.settings,com.android.vending,com.android.chrome,com.google.android.apps.photos,com.google.android.documentsui,com.google.android.gm";
    static final int PEDIR_VOZ = 51;
    static final PathInterpolator DESACELERA = new PathInterpolator(0.1f, 0.9f, 0.2f, 1f), ACELERA = new PathInterpolator(0.7f, 0f, 1f, 0.5f);

    static class App { String pkg, nombre; ComponentName comp; Drawable icono; }

    static Typeface normal, semi;   // Selawik (OFL), cargada una sola vez desde assets/fonts
    static Inicio instancia;        // el inicio vivo: Paneles usa su lista de apps, su dock y abrir()

    SharedPreferences prefs;
    App arrastrada;
    View anclaArrastre;
    boolean tocando, arrastreDesdeCajon;
    LinearLayout zonasArrastre;
    Widgets widgets;
    Discover discover;
    Rejilla adaptador, adaptadorCajon;
    FrameLayout raiz, carpetaAbierta, cajon, hoja;
    Edicion edicion;
    Carril carril;
    LinearLayout indicador, barraSel;
    View dockVista;
    String nombreCarpetaAbierta;
    boolean movido, resumida, iniciada, rehacerAlCerrar;   // iniciada: visible (onNewIntent llega con la actividad en pausa)
    float inicioX, inicioY;
    GridView rejilla, rejillaCajon;
    EditText buscarCajon;
    LinearLayout resultadosAjustes;
    PopupWindow menuActual;
    App pendienteArrastre;
    View anclaPendiente;
    float toqueX, toqueY, largoX, largoY;
    float vacioX, vacioY;
    boolean gestoVacio, toqueFondo, libre, paginando;
    float pellizco0;
    VelocityTracker velocidad;
    long ultimoToqueVacio;
    float ultimoVacioX, ultimoVacioY;
    final List<App> apps = new ArrayList<>();
    final List<Object> items = new ArrayList<>();                          // inicio: App o String (carpeta)
    final List<Object> itemsCajon = new ArrayList<>(), vistaCajon = new ArrayList<>();   // cajón: todo; vista = filtrada
    final Map<String, List<App>> visibles = new LinkedHashMap<>();         // carpetas con al menos una app instalada
    final Set<String> seleccion = new LinkedHashSet<>();                    // claves de las apps marcadas
    boolean seleccionando, seleccionEnCajon;
    final Set<String> conAvisos = new HashSet<>();
    final OnBackInvokedCallback atras = this::atras;
    final Runnable repintarVidrio = () -> { if (dockVista != null) dockVista.invalidate(); if (cajon != null) cajon.invalidate(); };
    final BroadcastReceiver cambios = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) {
            // al actualizarse una app (REPLACING) sus widgets no se borran
            if (Intent.ACTION_PACKAGE_REMOVED.equals(i.getAction()) && !i.getBooleanExtra(Intent.EXTRA_REPLACING, false)) widgets.limpiarSinProveedor();
            cargarApps(); mostrar();
        }
    };
    final WallpaperManager.OnColorsChangedListener fondoCambio = (colores, cual) -> {
        if ((cual & WallpaperManager.FLAG_SYSTEM) != 0) Vidrio.preparar(this, true);
    };

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        // Android cambió de idioma con la app viva: paneles, asa e isla guardan textos ya pintados; el sistema relanza el inicio entero
        if (!Idioma.AL_NACER.equals(java.util.Locale.getDefault().getLanguage())) { android.os.Process.killProcess(android.os.Process.myPid()); return; }
        previa = new Configuration(getResources().getConfiguration());
        AWindows.escuchar(this);
        prefs = getSharedPreferences("inicio", MODE_PRIVATE);
        assert gestoNotificaciones(0, 80, 16) && !gestoNotificaciones(80, 80, 16)
                && !gestoNotificaciones(0, -80, 16) && !gestoNotificaciones(0, 8, 16);
        assert !superaSlop(3, 4, 5) && superaSlop(4, 4, 5) && !superaSlop(0, 0, 5);
        assert normalizar("Batería").equals("bateria");
        fuentes(this);
        Vidrio.avisar.add(repintarVidrio);
        Vidrio.preparar(this, true);   // lo guardado al instante; se recalcula en otro hilo por si el fondo cambió sin nosotros
        WallpaperManager.getInstance(this).addOnColorsChangedListener(fondoCambio, Vidrio.principal);
        widgets = new Widgets(this);
        discover = new Discover(this);
        getWindow().setDecorFitsSystemWindows(false);
        getWindow().setStatusBarColor(Color.TRANSPARENT);
        getWindow().setNavigationBarColor(Color.TRANSPARENT);
        IntentFilter f = new IntentFilter();
        f.addAction(Intent.ACTION_PACKAGE_ADDED);
        f.addAction(Intent.ACTION_PACKAGE_REMOVED);
        f.addAction(Intent.ACTION_PACKAGE_CHANGED);
        f.addDataScheme("package");
        registerReceiver(cambios, f, RECEIVER_NOT_EXPORTED);
        getOnBackInvokedDispatcher().registerOnBackInvokedCallback(OnBackInvokedDispatcher.PRIORITY_DEFAULT, atras);   // atrás no sale del inicio
        cargarApps();
        mostrar();
        instancia = this;
        widgets.porOmision();
        Borde.de(this).actualizar();
        panelDe(getIntent());
    }

    static void fuentes(Context c) {
        if (normal != null) return;
        try {
            normal = Typeface.createFromAsset(c.getAssets(), "fonts/Selawik-Regular.ttf");
            semi = Typeface.createFromAsset(c.getAssets(), "fonts/Selawik-Semibold.ttf");
        } catch (RuntimeException e) { normal = semi = null; }   // sin la fuente se usa la del sistema
    }

    // Cajón (ALL_APPS o el que pidió Paneles), modo edición (prueba por adb) o el botón de inicio.
    @Override protected void onNewIntent(Intent i) { super.onNewIntent(i); panelDe(i); }

    /** Panel pedido desde este mismo proceso (OrdenPanel, Paneles). No va como extra del Intent: Inicio está exportada y
     *  cualquier app podría abrir paneles saltándose el permiso DUMP de OrdenPanel (revisión Gemini 17/11). */
    static volatile String panelPedido;

    void panelDe(Intent i) {
        String p = Intent.ACTION_ALL_APPS.equals(i.getAction()) ? "cajon" : panelPedido;
        panelPedido = null;
        if ("cajon".equals(p)) { if (Paneles.yo != null) Paneles.yo.cerrar(); if (cajon == null) abrirCajon(); else if (iniciada) cerrarCajon(); return; }
        if ("editar".equals(p)) { cerrarTodo(); abrirEdicion(); return; }
        if (p != null && p.matches("ajustes|fondo|widgets")) { cerrarTodo(); if (p.equals("ajustes")) Hojas.ajustes(this); else if (p.equals("fondo")) Hojas.fondo(this); else widgets.selector(paginaActual()); return; }
        if (p != null && p.matches("qs|noti|recientes")) { Paneles.de(this).alternar(p); return; }   // otro valor: se ignora
        if (Paneles.yo != null) Paneles.yo.cerrar();   // botón de inicio: fuera paneles, cajón y menús; a la página principal
        if (i.getAction() != null && Intent.ACTION_MAIN.equals(i.getAction()) && i.hasCategory(Intent.CATEGORY_HOME) && iniciada) {
            cerrarTodo();
            if (carril != null) carril.ir(carril.base() + principal(), true);
        }
    }

    @Override protected void onStart() { super.onStart(); iniciada = true; widgets.reconciliar(); widgets.host.startListening(); discover.alIniciar(); }
    @Override protected void onStop() { iniciada = false; widgets.host.stopListening(); discover.alParar(); super.onStop(); }
    @Override protected void onResume() { super.onResume(); resumida = true; discover.estado(3); refrescarRecientesDock(); }
    @Override protected void onPause() { resumida = false; asentar(); discover.estado(1); super.onPause(); }

    /** Un deslizamiento de páginas a medias (pausa, giro, panel abierto) termina en una página, nunca entre dos. */
    void asentar() {
        if (!paginando || carril == null) return;
        paginando = false; tocando = false; gestoVacio = false;
        carril.soltar(0);
    }
    @Override protected void onActivityResult(int pedido, int res, Intent datos) {
        if (pedido == PEDIR_VOZ) {
            List<String> r = datos == null ? null : datos.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS);
            if (res == RESULT_OK && r != null && !r.isEmpty() && buscarCajon != null) buscarCajon.setText(r.get(0));
            return;
        }
        if (pedido == Hojas.PEDIR_IMAGEN) { Hojas.imagenElegida(this, res, datos); return; }
        if (!widgets.resultado(pedido, res)) super.onActivityResult(pedido, res, datos);
    }
    @Override protected void onDestroy() {
        if (instancia == this) instancia = null;
        if (menuActual != null) menuActual.dismiss();
        WallpaperManager.getInstance(this).removeOnColorsChangedListener(fondoCambio);
        Vidrio.avisar.remove(repintarVidrio);
        discover.cerrar();
        unregisterReceiver(cambios);
        super.onDestroy();
    }
    Configuration previa;
    @Override public void onConfigurationChanged(Configuration c) {
        super.onConfigurationChanged(c);
        asentar();
        // Solo el tema (claro/oscuro) u otros que no cambian el tamaño: el inicio no depende de ellos y rehacerlo cerraba la
        // hoja de Ajustes abierta (17/11). Se rehace al girar o cambiar tamaño/densidad.
        int d = previa == null ? -1 : previa.diff(c);
        previa = new Configuration(c);
        if ((d & (android.content.pm.ActivityInfo.CONFIG_ORIENTATION | android.content.pm.ActivityInfo.CONFIG_SCREEN_SIZE
                | android.content.pm.ActivityInfo.CONFIG_SMALLEST_SCREEN_SIZE | android.content.pm.ActivityInfo.CONFIG_DENSITY
                | android.content.pm.ActivityInfo.CONFIG_SCREEN_LAYOUT)) == 0) return;
        if (Vidrio.chico != null && (Vidrio.chico.getWidth() > Vidrio.chico.getHeight()) != (c.orientation == Configuration.ORIENTATION_LANDSCAPE)) Vidrio.preparar(this, true);   // al girar
        mostrar();
    }

    /** Atrás: cierra lo de más arriba (menú, selección, hoja, edición, carpeta, cajón); en el inicio no hace nada. */
    void atras() {
        if (menuActual != null) menuActual.dismiss();
        else if (seleccionando) salirSeleccion();
        else if (hoja != null) cerrarHoja();
        else if (edicion != null) edicion.salir(-1);
        else if (carpetaAbierta != null) cerrarCarpeta();
        else if (cajon != null) cerrarCajon();
        else if (carril != null) carril.ir(carril.base() + principal(), true);
    }

    void cerrarTodo() {
        if (menuActual != null) menuActual.dismiss();
        if (seleccionando) salirSeleccion();
        if (hoja != null) cerrarHoja();
        if (edicion != null) edicion.salir(-1);
        cerrarCarpeta();
        if (cajon != null) cerrarCajon();
    }

    // shortcut: etiquetas e iconos se cargan en el hilo principal; pasarlo a un hilo si con muchas apps el arranque tarda
    void cargarApps() {
        PackageManager pm = getPackageManager();
        List<ResolveInfo> lista = pm.queryIntentActivities(new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER), 0);
        apps.clear();
        for (ResolveInfo r : lista) {
            if (r.activityInfo.packageName.equals(getPackageName())) continue;
            App a = new App();
            a.pkg = r.activityInfo.packageName;
            a.comp = new ComponentName(a.pkg, r.activityInfo.name);
            a.nombre = r.loadLabel(pm).toString();
            a.icono = iconoUniforme(r.loadIcon(pm));
            apps.add(a);
        }
        Collator col = Collator.getInstance(new Locale("es"));
        apps.sort((x, y) -> col.compare(x.nombre, y.nombre));
    }

    App buscar(String pkg) {
        for (App a : apps) if (a.pkg.equals(pkg)) return a;
        return null;
    }

    List<String> dock() { return new ArrayList<>(Arrays.asList(prefs.getString("dock", DOCK_INICIAL).split(","))); }

    /** Lo que está en el inicio (claves de apps y "carpeta:Nombre"). La primera vez: las carpetas que ya había y unas pocas apps. */
    Set<String> enInicio() {
        String s = prefs.getString("en_inicio", null);
        if (s != null) return new LinkedHashSet<>(Arrays.asList(s.split("\n")));
        Set<String> r = new LinkedHashSet<>();
        for (String n : carpetas().keySet()) r.add("carpeta:" + n);
        for (String pkg : INICIO_INICIAL.split(",")) { App a = buscar(pkg); if (a != null) r.add(clave(a)); }
        guardarEnInicio(r);
        return r;
    }
    void guardarEnInicio(Set<String> s) { prefs.edit().putString("en_inicio", String.join("\n", s)).apply(); }
    void ponerEnInicio(String id) { Set<String> s = enInicio(); if (s.add(id)) guardarEnInicio(s); }

    int principal() { return Math.max(0, Math.min(prefs.getInt("principal", 0), widgets.paginas().size() - 1)); }
    int paginaActual() { return carril == null ? 0 : Math.max(0, carril.pagina - carril.base()); }
    boolean bloqueado() {
        if (!prefs.getBoolean("bloqueo", false)) return false;
        Toast.makeText(this, Idioma.t("El diseño del inicio está bloqueado (Ajustes de inicio)", "Home screen layout is locked (Home settings)"), Toast.LENGTH_SHORT).show();
        return true;
    }
    int icono() { return prefs.getInt("icono", 64); }

    // ---------- construcción de la pantalla ----------

    void mostrar() {
        long t0 = SystemClock.uptimeMillis();
        String carpetaAnterior = nombreCarpetaAbierta;
        int paginaAnterior = carril == null ? -1 : paginaActual();
        boolean enDiscoverAntes = enDiscover();   // al girar se queda en Discover (revisión Kimi 17/11)
        boolean cajonAbierto = cajon != null;
        arrastrada = null;
        anclaArrastre = null;
        tocando = false;
        cerrarTodo();
        armarItems();
        boolean vertical = getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        raiz = new FrameLayout(this) {
            @Override public boolean dispatchDragEvent(DragEvent e) {
                if (e.getLocalState() instanceof App) {
                    if (e.getAction() == DragEvent.ACTION_DRAG_STARTED) {
                        inicioX = e.getX(); inicioY = e.getY(); movido = false;
                    } else if (e.getAction() == DragEvent.ACTION_DRAG_LOCATION
                            || e.getAction() == DragEvent.ACTION_DROP) {
                        float dx = e.getX() - inicioX, dy = e.getY() - inicioY;
                        int slop = ViewConfiguration.get(Inicio.this).getScaledTouchSlop();
                        movido |= dx * dx + dy * dy > slop * slop;
                        // Del cajón al inicio: al llevarla casi al borde de arriba, el cajón se aparta y se suelta en el inicio.
                        if (cajon != null && cajon.getVisibility() == View.VISIBLE && e.getY() < dp(72)) dejarVerInicio();
                    }
                }
                return super.dispatchDragEvent(e);
            }
        };
        atenuar();

        carril = new Carril(this);
        carril.base = discover.paginaPropia() ? 1 : 0;
        if (carril.base > 0) carril.addView(discover.pagina(), lleno());
        List<List<Integer>> paginas = widgets.paginas();
        for (int i = 0; i < paginas.size(); i++) carril.addView(i == 0 ? paginaApps(vertical, paginas.get(0)) : paginaWidgets(paginas.get(i), i), lleno());
        raiz.addView(carril, lleno());

        indicador = new LinearLayout(this);
        indicador.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams li = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(16), Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        li.bottomMargin = dp(128);
        raiz.addView(indicador, li);

        FrameLayout.LayoutParams ld = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(80), Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        ld.leftMargin = ld.rightMargin = dp(24);
        ld.bottomMargin = dp(40);
        dockVista = barraDock();
        raiz.addView(dockVista, ld);
        zonasArrastre = new LinearLayout(this);
        // Sobre la columna del reloj, nunca sobre la rejilla: un contenedor encima de los íconos se queda con el
        // soltar (no lo acepta: "Drop result=false") y las carpetas de la primera fila no lo reciben.
        zonasArrastre.setOrientation(LinearLayout.VERTICAL);
        zonasArrastre.setPadding(0, dp(32), 0, 0);
        for (String[] accion : new String[][]{{"Desinstalar", "Uninstall"}, {"Información de la app", "App info"}}) {
            Button zona = boton(Idioma.t(accion[0], accion[1]), Color.WHITE, TARJETA, BORDE);
            recibir(zona, accion[0], false);   // la clave queda en español: recibir() la compara con "Desinstalar"
            LinearLayout.LayoutParams lz = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(64));
            lz.bottomMargin = dp(16);
            zonasArrastre.addView(zona, lz);
        }
        zonasArrastre.setVisibility(View.INVISIBLE);
        FrameLayout.LayoutParams lzs = new FrameLayout.LayoutParams(dp(360), ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.START);
        lzs.leftMargin = dp(32);
        raiz.addView(zonasArrastre, lzs);
        raiz.setOnDragListener((v, e) -> {
            if (!(e.getLocalState() instanceof App)) return false;
            if (e.getAction() == DragEvent.ACTION_DROP) {   // en un hueco del inicio: llega desde el cajón y queda al final
                App a = (App) e.getLocalState();
                if (arrastreDesdeCajon && cajon != null && cajon.getVisibility() != View.VISIBLE && !enInicio().contains(clave(a))) {
                    v.post(() -> { ponerEnInicio(clave(a)); refrescar(); rebotarApp(a); });
                    return true;
                }
                return false;
            }
            if (e.getAction() == DragEvent.ACTION_DRAG_ENDED) {
                zonasArrastre.setVisibility(View.INVISIBLE);
                arrastrada = null;
                anclaArrastre = null;
                tocando = false;
                if (cajon != null && cajon.getVisibility() != View.VISIBLE) quitarCajon();   // se soltó en el inicio
                arrastreDesdeCajon = false;
                if (carpetaAbierta != null) {
                    if (movido || e.getResult()) cerrarCarpeta();
                    else carpetaAbierta.setVisibility(View.VISIBLE);
                }
            }
            return true;
        });
        View.OnLongClickListener agregarWidget = v -> { if (!bloqueado()) widgets.selector(paginaActual()); return true; };   // pulsación larga en zona vacía
        raiz.setOnLongClickListener(agregarWidget);
        setContentView(raiz);
        carril.alCambiar = this::paginaCambio;
        carril.ir(enDiscoverAntes && carril.base() > 0 ? 0
                : paginaAnterior >= 0 ? carril.base() + Math.min(paginaAnterior, paginas.size() - 1) : carril.base() + principal(), false);
        if (cajonAbierto) abrirCajon();
        if (carpetaAnterior != null) abrirCarpeta(carpetaAnterior);
        assert carpetaAnterior == null || !visibles.containsKey(carpetaAnterior)
                || carpetaAnterior.equals(nombreCarpetaAbierta) : "Carpeta perdida al reconstruir";
        Log.i("inicio", "mostrar en " + (SystemClock.uptimeMillis() - t0) + " ms");
        // Paneles (ajustes rápidos, avisos, Recientes) hechos de antemano con el tamaño y el dock de ahora: abrirlos no crea nada.
        // Solo se rehacen si cambió el tamaño o el dock: mostrar() también corre al pausar el inicio (Discover) y cerraba el
        // panel recién abierto (17/11).
        Paneles pn = Paneles.de(this);
        Configuration cf = getResources().getConfiguration();
        String clave = cf.screenWidthDp + "x" + cf.screenHeightDp + "|" + prefs.getString("dock", DOCK_INICIAL) + "|" + prefs.getBoolean("dock_recientes", false);
        if (!clave.equals(pn.clave)) { pn.olvidar(); pn.clave = clave; }
        raiz.postDelayed(pn::precalentar, 600);
    }

    /** Fondo atenuado con el modo oscuro (opción de "Fondo de pantalla y estilo"); el fondo lo pinta el sistema. */
    void atenuar() {
        if (raiz != null) raiz.setBackgroundColor(prefs.getBoolean("atenuar", true) && Vidrio.oscuro(this) ? 0x59000000 : Color.TRANSPARENT);
    }

    static FrameLayout.LayoutParams lleno() { return new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT); }

    void paginaCambio(int i) {
        int n = carril.getChildCount(), b = carril.base();
        indicador.removeAllViews();
        if (n > 1) for (int k = 0; k < n; k++) {
            View p = new View(this);
            boolean disc = k < b, actual = k == i, casa = k - b == principal();
            GradientDrawable g = new GradientDrawable();
            g.setShape(disc ? GradientDrawable.RECTANGLE : GradientDrawable.OVAL);
            g.setCornerRadius(dp(2));
            g.setColor(actual ? Color.WHITE : 0x66FFFFFF);
            p.setBackground(g);
            LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(dp(disc ? 10 : casa ? 8 : 6), dp(disc ? 3 : casa ? 8 : 6));
            l.leftMargin = l.rightMargin = dp(4);
            indicador.addView(p, l);
        }
        if (i < b) discover.visible();
        if (dockVista != null && cajon == null && edicion == null) dockVista.animate().alpha(i < b ? 0f : 1f).setDuration(160);   // Discover a pantalla completa
    }

    View paginaApps(boolean vertical, List<Integer> ids) {
        LinearLayout cuerpo = new LinearLayout(this);
        cuerpo.setOrientation(vertical ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        cuerpo.setPadding(dp(32), dp(48), dp(32), dp(152));
        int ancho = vertical ? getResources().getDisplayMetrics().widthPixels - dp(64) : dp(360);
        LinearLayout col = widgets.vista(ids, ancho, 0);
        ScrollView desplazable = new ScrollView(this);   // con widgets la columna puede ser más alta que la pantalla
        desplazable.setVerticalScrollBarEnabled(false);
        desplazable.addView(col);
        boolean hay = !ids.isEmpty();
        if (hay) cuerpo.addView(desplazable, vertical
                ? new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1)
                : new LinearLayout.LayoutParams(dp(360), ViewGroup.LayoutParams.MATCH_PARENT));

        rejilla = new GridView(this);
        rejilla.setOnTouchListener(this::tocarApp);
        rejilla.setNumColumns(prefs.getInt("columnas", vertical ? 4 : 6));
        rejilla.setVerticalSpacing(dp(12));
        rejilla.setSelector(android.R.color.transparent);
        rejilla.setVerticalScrollBarEnabled(false);
        adaptador = new Rejilla(items, false);
        rejilla.setAdapter(adaptador);
        rejilla.setOnItemClickListener((p, v, i, id) -> pulsar(v, items.get(i), false));
        rejilla.setOnItemLongClickListener((p, v, i, id) -> { pulsarLargo(v, items.get(i), false); return true; });
        recibir(rejilla, AL_FINAL, false);
        LinearLayout.LayoutParams lr = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1);
        if (vertical) lr.topMargin = dp(24);
        else { lr = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1); if (hay) lr.leftMargin = dp(32); }
        cuerpo.addView(rejilla, lr);
        return cuerpo;
    }

    /** Página de solo widgets: columnas tipo mampostería (cada widget va a la columna más corta). */
    View paginaWidgets(List<Integer> ids, int pagina) {
        boolean vertical = getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        int cols = vertical ? 2 : 3, ancho = (getResources().getDisplayMetrics().widthPixels - dp(64) - dp(24) * (cols - 1)) / cols;
        LinearLayout fila = new LinearLayout(this);
        fila.setPadding(dp(32), dp(56), dp(32), dp(152));
        int[] alto = new int[cols];
        List<List<Integer>> reparto = new ArrayList<>();
        for (int c = 0; c < cols; c++) reparto.add(new ArrayList<>());
        for (int id : ids) {
            int c = 0;
            for (int k = 1; k < cols; k++) if (alto[k] < alto[c]) c = k;
            reparto.get(c).add(id);
            alto[c] += widgets.altoDe(id) + dp(16);
        }
        for (int c = 0; c < cols; c++) {
            LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ancho, ViewGroup.LayoutParams.WRAP_CONTENT);
            if (c > 0) l.leftMargin = dp(24);
            fila.addView(widgets.vista(reparto.get(c), ancho, pagina), l);
        }
        ScrollView sv = new ScrollView(this);
        sv.setVerticalScrollBarEnabled(false);
        sv.setFillViewport(true);
        sv.addView(fila);
        if (ids.isEmpty()) {
            FrameLayout f = new FrameLayout(this);
            TextView t = new TextView(this);
            estilo(t, 16, false, TEXTO2);
            t.setGravity(Gravity.CENTER);
            t.setText(Idioma.t("Página vacía\nMantén pulsado o pellizca para añadir widgets", "Empty page\nTouch and hold or pinch to add widgets"));
            f.addView(t, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));
            return f;
        }
        return sv;
    }

    /** El reloj grande de siempre, ahora un widget que se puede quitar (Widgets.RELOJ). */
    View reloj() {
        boolean vertical = getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        TextClock hora = new TextClock(this);
        hora.setFormat24Hour("HH:mm");
        hora.setFormat12Hour("h:mm");
        estilo(hora, vertical ? 128 : 112, true, Color.WHITE);
        hora.setLetterSpacing(-0.02f);
        hora.setShadowLayer(dp(12), 0, dp(2), 0x66000000);   // legible sobre cualquier fondo
        col.addView(hora);
        TextClock fecha = new TextClock(this);
        fecha.setFormat24Hour(Idioma.t("EEEE, d 'de' MMMM", "EEEE, MMMM d"));
        fecha.setFormat12Hour(Idioma.t("EEEE, d 'de' MMMM", "EEEE, MMMM d"));
        estilo(fecha, 22, false, Color.WHITE);
        fecha.setShadowLayer(dp(8), 0, dp(1), 0x66000000);
        col.addView(fecha);
        return col;
    }

    View barraDock() {
        LinearLayout d = new LinearLayout(this);
        d.setGravity(Gravity.CENTER_VERTICAL);
        d.setPadding(dp(20), 0, dp(20), 0);
        List<String> fijas = dock();
        for (String pkg : fijas) {
            App a = buscar(pkg);
            if (a == null) continue;   // app no instalada todavía
            d.addView(iconoDock(a));
        }
        if (prefs.getBoolean("dock_recientes", false)) {   // hasta 3 apps recientes tras una raya, como One UI
            int n = 0;
            for (String pkg : Paneles.paquetes(Paneles.ultimasTareas)) {
                App a = buscar(pkg);
                if (a == null || fijas.contains(pkg)) continue;
                if (n++ == 0) {
                    View raya = new View(this);
                    raya.setBackgroundColor(0x33FFFFFF);
                    LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(dp(1), dp(40));
                    l.leftMargin = l.rightMargin = dp(8);
                    d.addView(raya, l);
                }
                d.addView(iconoDock(a));
                if (n == 3) break;
            }
        }
        HorizontalScrollView h = new HorizontalScrollView(this);   // con muchas apps fijadas, el dock se desplaza en vez de salirse
        recibir(h, null, false);
        h.setHorizontalScrollBarEnabled(false);
        h.setBackground(new Vidrio.Placa(h, dp(24), 0x8C1A1A1E));
        h.setClipToOutline(true);
        h.addView(d, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT));
        return h;
    }

    View iconoDock(App a) {
        ImageView ic = new ImageView(this);
        ic.setImageDrawable(a.icono);
        ic.setContentDescription(a.nombre);
        ic.setOnClickListener(v -> abrir(a));
        ic.setOnTouchListener(this::tocarApp);
        ic.setOnLongClickListener(v -> { ofrecerArrastre(v, a); return true; });
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(dp(56), dp(56));
        l.leftMargin = l.rightMargin = dp(8);
        ic.setLayoutParams(l);
        return ic;
    }

    void refrescarRecientesDock() {
        if (!prefs.getBoolean("dock_recientes", false) || dockVista == null) return;
        peticionAgente(this, "tareas", r -> {
            String l = r.trim();
            if (l.equals(Paneles.ultimasTareas) || raiz == null) return;
            Paneles.ultimasTareas = l;
            ViewGroup.LayoutParams lp = dockVista.getLayoutParams();
            int i = raiz.indexOfChild(dockVista);
            raiz.removeView(dockVista);
            dockVista = barraDock();
            raiz.addView(dockVista, i, lp);
        }, null);
    }

    void refrescarPuntos() {
        conAvisos.clear();
        try {
            if (IslaOyente.activo != null) for (android.service.notification.StatusBarNotification s : IslaOyente.activo.getActiveNotifications())
                if ((s.getNotification().flags & android.app.Notification.FLAG_GROUP_SUMMARY) == 0 && s.isClearable()) conAvisos.add(s.getPackageName());
        } catch (RuntimeException ignored) { }
        if (adaptador != null) adaptador.notifyDataSetChanged();
        if (adaptadorCajon != null) adaptadorCajon.notifyDataSetChanged();
    }

    /** Celda de icono (inicio y cajón): icono con su marca de selección y su punto de aviso, y el nombre debajo. */
    class Rejilla extends BaseAdapter {
        final List<Object> lista;
        final boolean enCajon;
        Rejilla(List<Object> l, boolean enCajon) { lista = l; this.enCajon = enCajon; }
        @Override public int getCount() { return lista.size(); }
        @Override public Object getItem(int i) { return lista.get(i); }
        @Override public long getItemId(int i) { return i; }
        @Override public View getView(int i, View reciclada, ViewGroup padre) {
            LinearLayout c = (LinearLayout) reciclada;
            int t = dp(icono());
            if (c == null) {
                c = new LinearLayout(Inicio.this);
                c.setOrientation(LinearLayout.VERTICAL);
                c.setGravity(Gravity.CENTER_HORIZONTAL);
                c.setPadding(0, dp(8), 0, dp(8));
                FrameLayout caja = new FrameLayout(Inicio.this);
                caja.addView(new ImageView(Inicio.this), new FrameLayout.LayoutParams(t, t, Gravity.CENTER));
                TextView marca = new TextView(Inicio.this);
                marca.setGravity(Gravity.CENTER);
                estilo(marca, 13, true, 0xFF003049);
                caja.addView(marca, new FrameLayout.LayoutParams(dp(22), dp(22), Gravity.TOP | Gravity.START));
                View punto = new View(Inicio.this);
                GradientDrawable pg = new GradientDrawable();
                pg.setShape(GradientDrawable.OVAL);
                pg.setColor(0xFFFF8A65);
                pg.setStroke(dp(2), 0xFF1A1A1E);
                punto.setBackground(pg);
                caja.addView(punto, new FrameLayout.LayoutParams(dp(14), dp(14), Gravity.TOP | Gravity.END));
                c.addView(caja, new LinearLayout.LayoutParams(t + dp(8), t + dp(8)));
                TextView n = new TextView(Inicio.this);
                estilo(n, 14, false, Color.WHITE);
                n.setShadowLayer(dp(4), 0, dp(1), 0x99000000);
                n.setGravity(Gravity.CENTER);
                n.setMaxLines(1);
                n.setEllipsize(TextUtils.TruncateAt.END);
                n.setPadding(dp(4), dp(6), dp(4), 0);
                c.addView(n);
            }
            Object o = lista.get(i);
            recibir(c, o, enCajon);
            boolean app = o instanceof App;
            FrameLayout caja = (FrameLayout) c.getChildAt(0);
            ((ImageView) caja.getChildAt(0)).setImageDrawable(app ? ((App) o).icono : iconoCarpeta((String) o));
            TextView marca = (TextView) caja.getChildAt(1);
            boolean sel = app && seleccionando && seleccionEnCajon == enCajon;
            marca.setVisibility(sel ? View.VISIBLE : View.GONE);
            if (sel) {
                boolean si = seleccion.contains(clave((App) o));
                marca.setText(si ? "✓" : "");
                GradientDrawable g = new GradientDrawable();
                g.setShape(GradientDrawable.OVAL);
                g.setColor(si ? ACENTO : 0x66000000);
                g.setStroke(dp(2), si ? ACENTO : Color.WHITE);
                marca.setBackground(g);
            }
            boolean punto = prefs.getBoolean("puntos", true) && (app ? conAvisos.contains(((App) o).pkg) : carpetaConAvisos((String) o));
            caja.getChildAt(2).setVisibility(punto ? View.VISIBLE : View.GONE);
            TextView n = (TextView) c.getChildAt(1);
            n.setText(app ? ((App) o).nombre : (String) o);
            n.setVisibility(enCajon || prefs.getBoolean("etiquetas", true) ? View.VISIBLE : View.GONE);
            c.setTag(app ? clave((App) o) : "carpeta:" + o);
            return c;
        }
    }

    boolean carpetaConAvisos(String n) {
        List<App> l = visibles.get(n);
        if (l != null) for (App a : l) if (conAvisos.contains(a.pkg)) return true;
        return false;
    }

    // ---------- acciones ----------

    // Si la app ya tiene tarea, el agente la trae al frente (una tarjeta por app, sin trampolines); si no, se abre normal.
    void abrir(App a) { peticionAgente(this, "traer " + a.pkg, null, () -> abrir(a, 0)); }

    void abrir(App a, int extra) {
        try {
            startActivity(new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER).setComponent(a.comp)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED | extra));
        } catch (ActivityNotFoundException | SecurityException e) {   // desinstalada o deshabilitada desde la última carga
            Toast.makeText(this, Idioma.t("No se pudo abrir ", "Couldn't open ") + a.nombre, Toast.LENGTH_SHORT).show();
            cargarApps();
            mostrar();
        }
    }

    /**
     * Pantalla dividida y demás órdenes que una app no puede: las hace el agente de sistema (root, F3/agente) por el socket
     * local "surfacedual.agente". Solo se cree a quien lo atiende con uid 0 (el kernel lo dice: SO_PEERCRED); otra app pudo
     * quedarse el nombre. Responde "ok[ datos]".
     */
    static final android.os.Handler principal = new android.os.Handler(android.os.Looper.getMainLooper());

    static void peticionAgente(Context ctx, String cmd, java.util.function.Consumer<String> exito, Runnable fallo) {
        new Thread(() -> {
            try (android.net.LocalSocket s = new android.net.LocalSocket()) {
                s.connect(new android.net.LocalSocketAddress("surfacedual.agente"));
                if (s.getPeerCredentials().getUid() != 0) throw new SecurityException("surfacedual.agente no es el agente");
                s.getOutputStream().write((cmd + "\n").getBytes("US-ASCII"));
                s.setSoTimeout(20000);   // "minis" tarda hasta 4 s
                String r = new java.io.BufferedReader(new java.io.InputStreamReader(s.getInputStream(), "US-ASCII")).readLine();
                if (r == null || !r.startsWith("ok") || (r.length() > 2 && r.charAt(2) != ' ')) throw new java.io.IOException("sin ok: " + cmd);
                String datos = r.substring(2);   // "" o " datos" (p. ej. la lista de "tareas")
                if (exito != null) principal.post(() -> exito.accept(datos));   // las respuestas, siempre en el hilo de la interfaz
            } catch (Exception e) {
                if (fallo != null) principal.post(fallo);
            }
        }).start();
    }

    void dividir(App a) { peticionAgente(this, "dividir " + a.pkg + "/" + a.comp.getClassName(), null, () -> abrir(a)); }

    void dormir() { peticionAgente(this, "dormir", null, null); }

    // ---------- mantener pulsado, arrastrar ----------

    void ofrecerArrastre(View v, App a) {
        rebotar(v);
        menuApp(v, a, dentro(v, cajon));
        if (tocando && !prefs.getBoolean("bloqueo", false)) {
            pendienteArrastre = a;
            anclaPendiente = v;
            largoX = toqueX; largoY = toqueY;
        }
    }

    static boolean dentro(View v, View padre) {
        if (padre == null) return false;
        for (ViewParent p = v.getParent(); p != null; p = p.getParent()) if (p == padre) return true;
        return false;
    }

    /** El icono "salta" para que se vea cuál se eligió (pedido 16). */
    static void rebotar(View v) {
        v.animate().cancel();
        v.animate().scaleX(1.14f).scaleY(1.14f).setDuration(110).setInterpolator(DESACELERA)
                .withEndAction(() -> v.animate().scaleX(1f).scaleY(1f).setDuration(260).setInterpolator(new OvershootInterpolator(3f)));
    }

    void rebotarApp(App a) {
        if (rejilla == null) return;
        rejilla.post(() -> { View c = rejilla.findViewWithTag(clave(a)); if (c != null) rebotar(c); });
    }

    void prepararArrastre(View v, App a) {
        if (!tocando) { menuApp(v, a, dentro(v, cajon)); return; }   // accesibilidad o teclado
        View origen = getWindow().getDecorView();
        View icono = v instanceof ViewGroup ? ((ViewGroup) v).getChildAt(0) : v;
        arrastrada = a;
        anclaArrastre = v;
        arrastreDesdeCajon = dentro(v, cajon);
        if (!arrastreDesdeCajon) zonasArrastre.setVisibility(View.VISIBLE);
        // Con flags=0, los destinos y localState pertenecen a la ventana de origen.
        if (!origen.startDragAndDrop(null, new View.DragShadowBuilder(icono), a, 0)) {
            zonasArrastre.setVisibility(View.INVISIBLE);
            arrastrada = null;
            anclaArrastre = null;
            tocando = false;
            arrastreDesdeCajon = false;
            menuApp(v, a, dentro(v, cajon));
        } else v.animate().alpha(.35f).setDuration(120).withEndAction(() -> v.postDelayed(() -> v.setAlpha(1f), 400));
        tocando = false;
    }

    boolean tocarApp(View v, MotionEvent e) {
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: tocando = true; arrastrada = null; anclaArrastre = null; break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: tocando = false; break;
        }
        return false;
    }

    boolean desinstalable(App a) {
        try {
            int flags = getPackageManager().getApplicationInfo(a.pkg, 0).flags;
            return (flags & ApplicationInfo.FLAG_SYSTEM) == 0
                    || (flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0;
        } catch (PackageManager.NameNotFoundException e) { return false; }
    }

    /** Desinstalar sin salir del inicio (E10, 17/11: la pantalla del sistema iba torpe): confirmación propia, el icono se encoge
     *  y el agente quita la app; el inicio se rehace al llegar PACKAGE_REMOVED. Si el agente no puede (app del sistema
     *  actualizada, sin agente), la pantalla del sistema como antes. */
    void desinstalar(List<App> apps) {
        List<App> l = new ArrayList<>();
        for (App a : apps) if (desinstalable(a)) l.add(a);
        if (l.isEmpty()) return;
        new AlertDialog.Builder(this)
                .setTitle(l.size() == 1 ? Idioma.t("¿Desinstalar " + l.get(0).nombre + "?", "Uninstall " + l.get(0).nombre + "?") : Idioma.t("¿Desinstalar " + l.size() + " apps?", "Uninstall " + l.size() + " apps?"))
                .setMessage(l.size() == 1 ? Idioma.t("Se quitarán la app y sus datos.", "The app and its data will be removed.") : Idioma.t("Se quitarán las apps y sus datos.", "The apps and their data will be removed."))
                .setNegativeButton(Idioma.t("Cancelar", "Cancel"), null)
                .setPositiveButton(Idioma.t("Desinstalar", "Uninstall"), (dlg, w) -> {
                    for (App a : l) {
                        View v = getWindow().getDecorView().findViewWithTag(clave(a));
                        if (v != null) v.animate().scaleX(.3f).scaleY(.3f).alpha(0f).setDuration(200).setInterpolator(ACELERA).withLayer();
                        peticionAgente(this, "desinstalar " + a.pkg, null, () -> {
                            if (v != null) v.animate().scaleX(1f).scaleY(1f).alpha(1f).setDuration(160);
                            accionApp(a, Intent.ACTION_DELETE);
                        });
                    }
                }).show();
    }

    void accionApp(App a, String accion) {
        try {
            startActivity(new Intent(accion, Uri.fromParts("package", a.pkg, null)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } catch (ActivityNotFoundException | SecurityException e) {
            Toast.makeText(this, Idioma.t("No se pudo abrir la acción para ", "Couldn't open this action for ") + a.nombre, Toast.LENGTH_SHORT).show();
        }
    }

    // null es el dock; las cadenas son carpetas o las dos acciones superiores. enCajon: la celda es del cajón.
    /** Destino "hueco libre de la rejilla": la app pasa al final (17/11: soltarla en un hueco no hacía nada). */
    static final Object AL_FINAL = new Object();

    void recibir(View vista, Object destino, boolean enCajon) {
        vista.setOnDragListener((v, e) -> {
            if (!(e.getLocalState() instanceof App)) return false;
            App a = (App) e.getLocalState();
            if (v instanceof Button && "Desinstalar".equals(destino) && !desinstalable(a)) return false;   // botón, no carpeta homónima
            if (destino instanceof App && clave(a).equals(clave((App) destino))) return false;
            if (e.getAction() == DragEvent.ACTION_DROP) {
                // Como One UI: al centro de un ícono, carpeta; en su borde izquierdo o derecho, desplazarlo.
                float x = e.getX(), w = Math.max(1, v.getWidth());
                boolean borde = x < w * 0.3f || x > w * 0.7f, despues = x > w / 2;
                v.post(() -> {
                    String carpetaA = carpetaDe(a), carpetaD = destino instanceof App ? carpetaDe((App) destino) : null;
                    if (destino == AL_FINAL) mover(a, null, true, enCajon);
                    else if (destino instanceof App && carpetaA != null && carpetaA.equals(carpetaD) && carpetaAbierta != null) {
                        reordenarCarpeta(carpetaA, a, (App) destino, despues);
                    } else if (borde && !(v instanceof Button) && (destino instanceof String || (destino instanceof App && carpetaD == null))) {
                        mover(a, destino, despues, enCajon);
                    } else if (destino == null) {
                        List<String> d = dock();
                        if (!d.contains(a.pkg)) {
                            d.add(a.pkg);
                            prefs.edit().putString("dock", String.join(",", d)).apply();
                            mostrar();
                        }
                    } else if (v instanceof Button) {
                        if ("Desinstalar".equals(destino)) desinstalar(Arrays.asList(a));
                        else accionApp(a, Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                    } else if (destino instanceof String) {
                        meter((String) destino, a);
                    } else {
                        crearCarpeta(Arrays.asList((App) destino, a), (App) destino, enCajon);
                    }
                    rebotarApp(a);
                });
            }
            return true;
        });
    }

    /** Carpeta nueva con esas apps, en el lugar de "lugar"; en el inicio queda también en el inicio. */
    void crearCarpeta(List<App> dentroDe, App lugar, boolean enCajon) {
        String nombre = carpetaDe(lugar);
        if (nombre == null) {
            nombre = Idioma.t("Nueva carpeta", "New folder");
            Map<String, List<String>> cs = carpetas();
            for (int i = 2; cs.containsKey(nombre); i++) nombre = Idioma.t("Nueva carpeta ", "New folder ") + i;
            // La carpeta nueva ocupa el lugar de la app sobre la que se soltó (no salta al final).
            String pref = enCajon ? "orden_cajon" : "orden";
            List<String> ids = new ArrayList<>();
            for (Object o : enCajon ? itemsCajon : items) ids.add(id(o));
            int i = ids.indexOf(clave(lugar));
            if (i >= 0) { ids.add(i, "carpeta:" + nombre); prefs.edit().putString(pref, String.join("\n", ids)).apply(); }
            if (!enCajon) ponerEnInicio("carpeta:" + nombre);
        }
        meter(nombre, dentroDe.toArray(new App[0]));
    }

    // ---------- menú de mantener pulsado (fotos O, P y S) ----------

    void menuApp(View ancla, App a, boolean enCajon) {
        LinearLayout t = new LinearLayout(this);
        t.setOrientation(LinearLayout.VERTICAL);
        t.setPadding(dp(8), dp(12), dp(8), dp(6));
        t.setMinimumWidth(dp(300));
        PopupWindow[] v = new PopupWindow[1];
        Runnable cerrar = () -> { if (v[0] != null) v[0].dismiss(); };

        FrameLayout cab = new FrameLayout(this);
        TextView nom = new TextView(this);
        estilo(nom, 16, true, Color.WHITE);
        nom.setText(a.nombre);
        nom.setGravity(Gravity.CENTER);
        nom.setSingleLine(true);
        nom.setEllipsize(TextUtils.TruncateAt.END);
        nom.setPadding(dp(44), 0, dp(44), 0);
        cab.addView(nom, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(36), Gravity.CENTER));
        ImageView info = iconoBoton("ic_info_outline_24", 36, x -> { cerrar.run(); accionApp(a, Settings.ACTION_APPLICATION_DETAILS_SETTINGS); });
        info.setContentDescription(Idioma.t("Información de la app", "App info"));
        cab.addView(info, new FrameLayout.LayoutParams(dp(36), dp(36), Gravity.END | Gravity.CENTER_VERTICAL));
        t.addView(cab);

        // Atajos de la app (LauncherApps: el launcher por defecto puede leerlos y abrirlos).
        LauncherApps la = getSystemService(LauncherApps.class);
        List<ShortcutInfo> atajos = new ArrayList<>();
        try {
            if (la.hasShortcutHostPermission()) {
                LauncherApps.ShortcutQuery q = new LauncherApps.ShortcutQuery().setPackage(a.pkg).setActivity(a.comp)
                        .setQueryFlags(LauncherApps.ShortcutQuery.FLAG_MATCH_DYNAMIC | LauncherApps.ShortcutQuery.FLAG_MATCH_MANIFEST);
                List<ShortcutInfo> l = la.getShortcuts(q, Process.myUserHandle());
                if (l != null) atajos.addAll(l);
                atajos.sort(Comparator.comparingInt((ShortcutInfo s) -> s.isDynamic() ? 1 : 0).thenComparingInt(ShortcutInfo::getRank));
            }
        } catch (RuntimeException e) { Log.w("inicio", "atajos de " + a.pkg, e); }
        int dpi = getResources().getDisplayMetrics().densityDpi;
        for (int i = 0; i < Math.min(5, atajos.size()); i++) {
            ShortcutInfo s = atajos.get(i);
            Drawable ic = null;
            try { ic = la.getShortcutIconDrawable(s, dpi); } catch (RuntimeException ignored) { }
            CharSequence txt = s.getShortLabel() != null ? s.getShortLabel() : s.getLongLabel();
            t.addView(filaMenu(ic != null ? ic : a.icono, txt, true, x -> {
                cerrar.run();
                try { la.startShortcut(s, null, null); }
                catch (RuntimeException e) { Toast.makeText(this, Idioma.t("No se pudo abrir el atajo", "Couldn't open the shortcut"), Toast.LENGTH_SHORT).show(); }
            }));
        }
        String carpeta = carpetaDe(a);
        if (!enCajon && carpeta != null) t.addView(filaMenu(ico("ic_menu_revert"), Idioma.t("Sacar de la carpeta", "Remove from folder"), false, x -> { cerrar.run(); sacar(a); }));
        t.addView(filaMenu(ico("ic_more_items"), Idioma.t("Abrir en pantalla dividida", "Open in split screen"), false, x -> { cerrar.run(); dividir(a); }));
        boolean enDock = dock().contains(a.pkg);
        t.addView(filaMenu(ico(enDock ? "ic_menu_close_clear_cancel" : "ic_menu_add"), enDock ? Idioma.t("Quitar del dock", "Remove from dock") : Idioma.t("Fijar en el dock", "Pin to dock"), false, x -> {
            cerrar.run();
            List<String> d = dock();
            if (enDock) d.remove(a.pkg); else d.add(a.pkg);
            prefs.edit().putString("dock", String.join(",", d)).apply();
            mostrar();
        }));

        View raya = new View(this);
        raya.setBackgroundColor(0x1FFFFFFF);
        LinearLayout.LayoutParams lr = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1));
        lr.topMargin = lr.bottomMargin = dp(6);
        t.addView(raya, lr);
        LinearLayout pie = new LinearLayout(this);
        pie.addView(botonMenu("ic_check_circle_24px", Idioma.t("Selec.", "Select"), x -> { cerrar.run(); entrarSeleccion(a, enCajon); }), pesoUno());
        if (enCajon) pie.addView(botonMenu("ic_menu_add", Idioma.t("Añadir a Inicio", "Add to Home"), x -> {
            cerrar.run();
            if (bloqueado()) return;
            ponerEnInicio(clave(a));
            refrescar();
            Toast.makeText(this, Idioma.t(a.nombre + " está en el inicio", a.nombre + " is on the Home screen"), Toast.LENGTH_SHORT).show();
        }), pesoUno());
        else pie.addView(botonMenu("ic_delete", Idioma.t("Eliminar", "Remove"), x -> { cerrar.run(); if (!bloqueado()) quitarDelInicio(Arrays.asList(a)); }), pesoUno());
        if (desinstalable(a)) pie.addView(botonMenu("ic_minus", Idioma.t("Desinst.", "Uninstall"), x -> { cerrar.run(); desinstalar(Arrays.asList(a)); }), pesoUno());
        if (!widgets.de(a.pkg).isEmpty()) pie.addView(botonMenu("ic_widgets", "Widgets", x -> { cerrar.run(); widgets.selector(paginaActual(), a.pkg); }), pesoUno());
        t.addView(pie);
        v[0] = mostrarVentana(t, ancla);
    }

    View filaMenu(Drawable ic, CharSequence texto, boolean grande, View.OnClickListener al) {
        LinearLayout f = new LinearLayout(this);
        f.setGravity(Gravity.CENTER_VERTICAL);
        f.setPadding(dp(12), 0, dp(12), 0);
        f.setMinimumHeight(dp(grande ? 48 : 44));
        f.setBackground(seleccionable());
        ImageView i = new ImageView(this);
        i.setImageDrawable(ic);
        f.addView(i, new LinearLayout.LayoutParams(dp(grande ? 28 : 22), dp(grande ? 28 : 22)));
        TextView t = new TextView(this);
        estilo(t, 15, false, Color.WHITE);
        t.setText(texto);
        t.setSingleLine(true);
        t.setEllipsize(TextUtils.TruncateAt.END);
        t.setPadding(dp(14), 0, 0, 0);
        f.addView(t, pesoUno());
        f.setOnClickListener(al);
        return f;
    }

    View botonMenu(String icono, String texto, View.OnClickListener al) {
        LinearLayout b = new LinearLayout(this);
        b.setOrientation(LinearLayout.VERTICAL);
        b.setGravity(Gravity.CENTER);
        b.setPadding(dp(4), dp(8), dp(4), dp(8));
        b.setBackground(seleccionable());
        ImageView i = new ImageView(this);
        i.setImageDrawable(ico(icono));
        b.addView(i, new LinearLayout.LayoutParams(dp(20), dp(20)));
        TextView t = new TextView(this);
        estilo(t, 11, false, TEXTO2);
        t.setText(texto);
        t.setGravity(Gravity.CENTER);
        t.setSingleLine(true);
        t.setPadding(0, dp(4), 0, 0);
        b.addView(t);
        b.setOnClickListener(al);
        return b;
    }

    /** Icono del sistema en blanco (los de framework-res; si falta, uno genérico). */
    Drawable ico(String nombre) { return Paneles.de(this).icono(nombre, Color.WHITE); }

    ImageView iconoBoton(String icono, int ladoDp, View.OnClickListener al) {
        ImageView b = new ImageView(this);
        b.setImageDrawable(ico(icono));
        int m = dp(ladoDp * .24f);
        b.setPadding(m, m, m, m);
        b.setBackground(seleccionable());
        b.setOnClickListener(al);
        return b;
    }

    Drawable seleccionable() {
        return new android.graphics.drawable.RippleDrawable(android.content.res.ColorStateList.valueOf(0x26FFFFFF), null, fondo(Color.WHITE, 0, 14));
    }

    LinearLayout.LayoutParams pesoUno() { return new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1); }

    void quitarDelInicio(List<App> l) {
        Set<String> s = enInicio();
        Map<String, List<String>> cs = carpetas();
        boolean tocoCarpetas = false;
        for (App a : l) {
            s.remove(clave(a));
            String c = carpetaDe(a);
            if (c != null && s.contains("carpeta:" + c)) {   // dentro de una carpeta del inicio: sale de la carpeta
                List<String> lc = cs.get(c);
                if (lc != null && (lc.remove(clave(a)) | lc.remove(a.pkg)) && lc.isEmpty()) { cs.remove(c); s.remove("carpeta:" + c); }
                tocoCarpetas = true;
            }
        }
        guardarEnInicio(s);
        if (tocoCarpetas) guardarCarpetas(cs);
        refrescar();
    }

    // ---------- selección múltiple (foto Q) ----------

    void entrarSeleccion(App a, boolean enCajon) {
        seleccionando = true;
        seleccionEnCajon = enCajon;
        seleccion.clear();
        seleccion.add(clave(a));
        barraSel = new LinearLayout(this);
        barraSel.setGravity(Gravity.CENTER_VERTICAL);
        barraSel.setPadding(dp(8), 0, dp(8), 0);
        barraSel.setBackground(new Vidrio.Placa(barraSel, dp(26), 0xB31E1E24));
        barraSel.setElevation(dp(12));
        barraSel.addView(pildora("ic_minus", Idioma.t("Desinstalar", "Uninstall"), x -> {
            desinstalar(new ArrayList<>(marcadas()));
            salirSeleccion();
        }));
        if (enCajon) barraSel.addView(pildora("ic_menu_add", Idioma.t("Añadir a Inicio", "Add to Home"), x -> {
            if (!bloqueado()) { Set<String> s = enInicio(); for (App m : marcadas()) s.add(clave(m)); guardarEnInicio(s); }
            salirSeleccion();
            refrescar();
        }));
        else barraSel.addView(pildora("ic_delete", Idioma.t("Eliminar", "Remove"), x -> { List<App> m = marcadas(); salirSeleccion(); if (!bloqueado()) quitarDelInicio(m); }));
        barraSel.addView(pildora("ic_folder_24dp", Idioma.t("Crear carpeta", "Create folder"), x -> {
            List<App> m = marcadas();
            salirSeleccion();
            if (m.size() >= 1 && !bloqueado()) crearCarpeta(m, m.get(0), enCajon);
        }));
        barraSel.addView(pildora("ic_close", Idioma.t("Listo", "Done"), x -> salirSeleccion()));
        // Abajo, sobre el dock: arriba tapaba la primera fila de iconos (17/11).
        FrameLayout.LayoutParams l = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(52), Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        l.bottomMargin = dp(120);
        raiz.addView(barraSel, l);
        barraSel.setAlpha(0f);
        barraSel.setTranslationY(dp(16));
        barraSel.animate().alpha(1f).translationY(0).setDuration(200).setInterpolator(DESACELERA);
        notificarRejillas();
    }

    List<App> marcadas() {
        List<App> r = new ArrayList<>();
        for (App a : apps) if (seleccion.contains(clave(a))) r.add(a);
        return r;
    }

    void salirSeleccion() {
        seleccionando = false;
        seleccion.clear();
        if (barraSel != null) { View b = barraSel; b.animate().alpha(0f).setDuration(140).withEndAction(() -> raiz.removeView(b)); barraSel = null; }
        notificarRejillas();
    }

    void notificarRejillas() {
        if (adaptador != null) adaptador.notifyDataSetChanged();
        if (adaptadorCajon != null) adaptadorCajon.notifyDataSetChanged();
    }

    View pildora(String icono, String texto, View.OnClickListener al) {
        LinearLayout p = new LinearLayout(this);
        p.setGravity(Gravity.CENTER_VERTICAL);
        p.setPadding(dp(16), 0, dp(18), 0);
        p.setBackground(seleccionable());
        ImageView i = new ImageView(this);
        i.setImageDrawable(ico(icono));
        p.addView(i, new LinearLayout.LayoutParams(dp(20), dp(20)));
        TextView t = new TextView(this);
        estilo(t, 14, true, Color.WHITE);
        t.setText(texto);
        t.setPadding(dp(10), 0, 0, 0);
        p.addView(t);
        p.setOnClickListener(al);
        p.setLayoutParams(new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(44)));
        return p;
    }

    // ---------- carpetas (preferencia "carpetas": JSON nombre -> [paquetes]) ----------

    Map<String, List<String>> carpetas() {
        Map<String, List<String>> m = new LinkedHashMap<>();
        try {
            JSONObject o = new JSONObject(prefs.getString("carpetas", "{}"));
            for (Iterator<String> k = o.keys(); k.hasNext(); ) {
                String n = k.next();
                JSONArray ja = o.getJSONArray(n);
                List<String> l = new ArrayList<>();
                for (int i = 0; i < ja.length(); i++) {
                    String id = ja.getString(i);
                    boolean expandido = false;
                    // El formato antiguo por paquete incluye todas sus actividades instaladas.
                    if (!id.contains("/")) for (App a : apps) if (a.pkg.equals(id)) {
                        if (!l.contains(clave(a))) l.add(clave(a));
                        expandido = true;
                    }
                    if (!expandido && !l.contains(id)) l.add(id);
                }
                m.put(n, l);
            }
        } catch (JSONException e) { m.clear(); }   // preferencia corrupta: se empieza de cero
        return m;
    }

    void guardarCarpetas(Map<String, List<String>> m) {
        prefs.edit().putString("carpetas", new JSONObject(m).toString()).apply();
        Set<String> s = enInicio();   // una carpeta que se vació no queda como fantasma en el inicio
        if (s.removeIf(id -> id.startsWith("carpeta:") && !m.containsKey(id.substring(8)))) guardarEnInicio(s);
    }

    // Las carpetas guardan ComponentName.flattenToString(); las entradas viejas (solo paquete) se siguen reconociendo.
    static String clave(App a) { return a.comp.flattenToString(); }

    String carpetaDe(App a) {
        for (Map.Entry<String, List<String>> e : carpetas().entrySet()) if (e.getValue().contains(clave(a)) || e.getValue().contains(a.pkg)) return e.getKey();
        return null;
    }

    /** Inicio: solo lo que el usuario puso (apps y carpetas). Cajón: todas las carpetas y las apps sueltas. Cada uno con su orden. */
    void armarItems() {
        items.clear();
        itemsCajon.clear();
        visibles.clear();
        Set<String> dentro = new HashSet<>(), en = enInicio();
        for (Map.Entry<String, List<String>> e : carpetas().entrySet()) {
            List<App> l = new ArrayList<>();
            for (App a : apps) if (e.getValue().contains(clave(a)) || e.getValue().contains(a.pkg)) { l.add(a); dentro.add(clave(a)); }
            if (l.isEmpty()) continue;
            visibles.put(e.getKey(), l);
            itemsCajon.add(e.getKey());
            if (en.contains("carpeta:" + e.getKey())) items.add(e.getKey());
        }
        for (App a : apps) if (!dentro.contains(clave(a))) {
            itemsCajon.add(a);
            if (en.contains(clave(a))) items.add(a);
        }
        ordenar(items, "orden");
        ordenar(itemsCajon, "orden_cajon");
        for (Map.Entry<String, List<App>> e : visibles.entrySet()) {
            // Alfabético hasta que el usuario reordene esa carpeta; luego, su orden.
            List<String> ids = Arrays.asList(prefs.getString("orden_c:" + e.getKey(), "").split("\n"));
            e.getValue().sort(Comparator.comparingInt(a -> { int i = ids.indexOf(clave(a)); return i < 0 ? Integer.MAX_VALUE : i; }));
        }
    }

    // Orden elegido por el usuario; lo nuevo queda al final en su orden por omisión.
    void ordenar(List<Object> l, String pref) {
        List<String> orden = Arrays.asList(prefs.getString(pref, "").split("\n"));
        Map<String, Integer> posicion = new java.util.HashMap<>();
        for (int i = 0; i < orden.size(); i++) posicion.putIfAbsent(orden.get(i), i);
        l.sort(Comparator.comparingInt(o -> posicion.getOrDefault(id(o), Integer.MAX_VALUE)));
    }

    static String id(Object o) { return o instanceof App ? clave((App) o) : "carpeta:" + o; }

    /** Coloca la app antes o después de otro elemento (sacándola de su carpeta si estaba en una). Soltada en el inicio, queda en él. */
    void mover(App a, Object destino, boolean despues, boolean enCajon) {
        String carpeta = carpetaDe(a);
        if (carpeta != null) {
            Map<String, List<String>> cs = carpetas();
            List<String> l = cs.get(carpeta);
            if ((l.remove(clave(a)) | l.remove(a.pkg)) && l.isEmpty()) cs.remove(carpeta);
            guardarCarpetas(cs);
        }
        if (!enCajon) ponerEnInicio(clave(a));
        armarItems();
        List<String> ids = new ArrayList<>();
        for (Object o : enCajon ? itemsCajon : items) if (!id(o).equals(clave(a))) ids.add(id(o));
        int i = ids.indexOf(id(destino));
        ids.add(i < 0 ? ids.size() : i + (despues ? 1 : 0), clave(a));
        prefs.edit().putString(enCajon ? "orden_cajon" : "orden", String.join("\n", ids)).apply();
        refrescar();
    }

    /** Reordena dentro de una carpeta y la deja abierta. */
    void reordenarCarpeta(String carpeta, App a, App destino, boolean despues) {
        List<String> l = new ArrayList<>();
        for (App x : visibles.get(carpeta)) if (!clave(x).equals(clave(a))) l.add(clave(x));   // el orden que el usuario ve
        int i = l.indexOf(clave(destino));
        l.add(i < 0 ? l.size() : i + (despues ? 1 : 0), clave(a));
        prefs.edit().putString("orden_c:" + carpeta, String.join("\n", l)).apply();
        refrescar();
        abrirCarpeta(carpeta);
    }

    /** Cambio de apps o carpetas: solo las rejillas; los widgets no se vuelven a crear. */
    void refrescar() {
        cerrarCarpeta();
        if (menuActual != null) menuActual.dismiss();
        armarItems();
        if (adaptador != null) adaptador.notifyDataSetChanged();
        if (adaptadorCajon != null) filtrarCajon(buscarCajon == null ? "" : buscarCajon.getText().toString());
    }

    void pulsar(View v, Object o, boolean enCajon) {
        if (seleccionando && seleccionEnCajon == enCajon) {
            if (o instanceof App) {
                String k = clave((App) o);
                if (!seleccion.remove(k)) seleccion.add(k);
                rebotar(v);
                notificarRejillas();
            }
            return;
        }
        if (o instanceof App) { abrir((App) o); if (enCajon) cerrarCajon(); }
        else abrirCarpeta((String) o);
    }

    void pulsarLargo(View v, Object o, boolean enCajon) {
        if (o instanceof App) { ofrecerArrastre(v, (App) o); return; }
        String n = (String) o;
        rebotar(v);
        PopupMenu m = new PopupMenu(this, v);
        m.getMenu().add(0, 1, 0, Idioma.t("Cambiar nombre", "Rename"));
        m.getMenu().add(0, 2, 1, Idioma.t("Deshacer carpeta", "Ungroup folder"));
        if (!enCajon) m.getMenu().add(0, 3, 2, Idioma.t("Quitar del inicio", "Remove from Home"));
        m.setOnMenuItemClickListener(it -> {
            if (it.getItemId() == 1) { renombrar(n); return true; }
            if (bloqueado()) return true;
            if (it.getItemId() == 3) { Set<String> s = enInicio(); s.remove("carpeta:" + n); guardarEnInicio(s); refrescar(); return true; }
            Map<String, List<String>> cs = carpetas();
            List<String> dentroDe = cs.remove(n);
            guardarCarpetas(cs);
            Set<String> s = enInicio();   // al deshacerla en el inicio, sus apps se quedan en el inicio
            if (s.remove("carpeta:" + n) && dentroDe != null) s.addAll(dentroDe);
            guardarEnInicio(s);
            refrescar();
            return true;
        });
        mostrarMenu(m, v);
    }

    void renombrar(String n) {
        EditText e = new EditText(this);
        e.setSingleLine();
        e.setText(n);
        new AlertDialog.Builder(this).setTitle(Idioma.t("Nombre de la carpeta", "Folder name")).setView(e)
                .setPositiveButton(Idioma.t("Aceptar", "OK"), (d, i) -> {
                    String nuevo = e.getText().toString().trim();
                    Map<String, List<String>> cs = carpetas();
                    if (nuevo.isEmpty() || cs.containsKey(nuevo)) return;
                    Map<String, List<String>> r = new LinkedHashMap<>();
                    for (Map.Entry<String, List<String>> x : cs.entrySet()) r.put(x.getKey().equals(n) ? nuevo : x.getKey(), x.getValue());
                    guardarCarpetas(r);
                    Set<String> s = enInicio();
                    if (s.remove("carpeta:" + n)) { s.add("carpeta:" + nuevo); guardarEnInicio(s); }
                    SharedPreferences.Editor ed = prefs.edit();
                    for (String pref : new String[]{"orden", "orden_cajon"}) {
                        List<String> l = new ArrayList<>(Arrays.asList(prefs.getString(pref, "").split("\n")));
                        l.replaceAll(x -> x.equals("carpeta:" + n) ? "carpeta:" + nuevo : x);
                        ed.putString(pref, String.join("\n", l));
                    }
                    ed.putString("orden_c:" + nuevo, prefs.getString("orden_c:" + n, "")).remove("orden_c:" + n).apply();
                    refrescar();
                })
                .setNegativeButton(Idioma.t("Cancelar", "Cancel"), null).show();
    }

    void cerrarCarpeta() {
        if (carpetaAbierta == null) return;
        raiz.removeView(carpetaAbierta);
        carpetaAbierta = null;
        nombreCarpetaAbierta = null;
    }

    void abrirCarpeta(String nombre) {
        List<App> dentroDe = visibles.get(nombre);
        if (dentroDe == null) return;
        cerrarCarpeta();
        boolean enCajon = cajon != null;
        GridView g = new GridView(this);
        g.setOnTouchListener(this::tocarApp);
        g.setNumColumns(4);
        g.setVerticalSpacing(dp(8));
        g.setPadding(dp(20), dp(4), dp(20), dp(20));
        g.setSelector(android.R.color.transparent);
        g.setAdapter(new Rejilla(new ArrayList<Object>(dentroDe), enCajon));
        int alto = Math.min(((dentroDe.size() + 3) / 4) * dp(icono() + 58) + dp(28), (int) (getResources().getDisplayMetrics().heightPixels * 0.7f));
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setBackground(new Vidrio.Placa(panel, dp(28), 0xB3202026));
        panel.setElevation(dp(16));
        panel.setClickable(true);
        TextView titulo = new TextView(this);
        titulo.setText(nombre);
        estilo(titulo, 20, true, Color.WHITE);
        titulo.setGravity(Gravity.CENTER);
        titulo.setPadding(dp(24), dp(20), dp(24), dp(12));
        titulo.setOnClickListener(v -> renombrar(nombre));
        panel.addView(titulo);
        panel.addView(g, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, alto));
        carpetaAbierta = new FrameLayout(this);
        carpetaAbierta.setBackgroundColor(0x66000000);
        carpetaAbierta.setOnClickListener(v -> cerrarCarpeta());
        // Arrastrar dentro del panel reordena; al salir de él, la carpeta se oculta y se puede soltar fuera.
        carpetaAbierta.setOnDragListener((v, e) -> {
            if (e.getAction() == DragEvent.ACTION_DRAG_LOCATION && v.getVisibility() == View.VISIBLE) {
                Rect r = new Rect();
                panel.getHitRect(r);
                // Fuera: se quita (no basta ocultarla: invisible seguía recibiendo el DROP y soltar fuera no hacía nada, 17/11).
                if (!r.contains((int) e.getX(), (int) e.getY())) { v.setVisibility(View.INVISIBLE); v.post(this::cerrarCarpeta); }
            }
            return e.getAction() != DragEvent.ACTION_DROP && e.getLocalState() instanceof App;
        });
        carpetaAbierta.addView(panel, new FrameLayout.LayoutParams(
                Math.min(dp(520), getResources().getDisplayMetrics().widthPixels - dp(48)),
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        raiz.addView(carpetaAbierta, lleno());
        panel.setScaleX(.9f); panel.setScaleY(.9f); panel.setAlpha(0f);
        panel.animate().scaleX(1f).scaleY(1f).alpha(1f).setDuration(200).setInterpolator(DESACELERA);
        nombreCarpetaAbierta = nombre;
        g.setOnItemClickListener((p, v, i, id) -> {
            if (seleccionando) { pulsar(v, dentroDe.get(i), enCajon); return; }
            cerrarCarpeta(); abrir(dentroDe.get(i)); if (enCajon) cerrarCajon();
        });
        g.setOnItemLongClickListener((p, v, i, id) -> { ofrecerArrastre(v, dentroDe.get(i)); return true; });
    }

    void meter(String nombre, App... agregar) {
        Map<String, List<String>> cs = carpetas();
        for (App a : agregar) {
            for (Iterator<Map.Entry<String, List<String>>> it = cs.entrySet().iterator(); it.hasNext(); ) {
                Map.Entry<String, List<String>> e = it.next();
                if (e.getKey().equals(nombre)) continue;
                e.getValue().remove(clave(a));
                e.getValue().remove(a.pkg);
                if (e.getValue().isEmpty()) it.remove();
            }
            List<String> l = cs.computeIfAbsent(nombre, k -> new ArrayList<>());
            if (!l.contains(clave(a))) l.add(clave(a));
            assert cs.values().stream().filter(c -> c.contains(clave(a)) || c.contains(a.pkg)).count() == 1 : "App en varias carpetas";
        }
        guardarCarpetas(cs);
        refrescar();
    }

    void sacar(App a) {
        Map<String, List<String>> cs = carpetas();
        String c = carpetaDe(a);
        for (Iterator<List<String>> it = cs.values().iterator(); it.hasNext(); ) {
            List<String> l = it.next();
            if ((l.remove(clave(a)) | l.remove(a.pkg)) && l.isEmpty()) it.remove();   // sin apps, la carpeta desaparece
        }
        guardarCarpetas(cs);
        if (c != null && enInicio().contains("carpeta:" + c)) ponerEnInicio(clave(a));   // de una carpeta del inicio, al inicio
        refrescar();
    }

    /** Mosaico 3x3 con las primeras apps de la carpeta (como One UI). Copias de los iconos: un Drawable compartido no puede tener dos tamaños. */
    Drawable iconoCarpeta(String nombre) {
        List<App> l = visibles.get(nombre);
        int t = dp(64), m = dp(6), c = (t - 4 * m) / 3, n = l == null ? 0 : Math.min(9, l.size());
        Drawable[] capas = new Drawable[1 + n];
        GradientDrawable f = new GradientDrawable();
        f.setColor(0x8C1E1E24);
        f.setCornerRadius(t * .3f);
        f.setStroke(dp(1), 0x26FFFFFF);
        capas[0] = f;
        for (int i = 0; i < n; i++) {
            Drawable.ConstantState cs = l.get(i).icono.getConstantState();
            capas[i + 1] = cs != null ? cs.newDrawable().mutate() : l.get(i).icono;
        }
        LayerDrawable ld = new LayerDrawable(capas);
        ld.setLayerSize(0, t, t);
        for (int i = 0; i < n; i++) {
            ld.setLayerGravity(i + 1, Gravity.TOP | Gravity.LEFT);
            ld.setLayerSize(i + 1, c, c);
            ld.setLayerInsetLeft(i + 1, m + (i % 3) * (c + m));
            ld.setLayerInsetTop(i + 1, m + (i / 3) * (c + m));
        }
        return ld;
    }

    // ---------- cajón de apps (foto R): deslizar hacia arriba sobre el inicio ----------

    void abrirCajon() {
        if (cajon != null) return;
        long t0 = SystemClock.uptimeMillis();
        cerrarCarpeta();
        if (menuActual != null) menuActual.dismiss();
        boolean vertical = getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
        FrameLayout c = cajon = new FrameLayout(this);
        c.setClickable(true);
        Vidrio.Fondo fondoCajon = new Vidrio.Fondo(this, 0x4D000000);
        c.addView(fondoCajon, lleno());
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setGravity(Gravity.CENTER_HORIZONTAL);
        rejillaCajon = new GridView(this);   // deslizar hacia abajo lo cierra: dispatchTouchEvent
        rejillaCajon.setOnTouchListener(this::tocarApp);
        rejillaCajon.setNumColumns(prefs.getInt("columnas_cajon", vertical ? 5 : 8));
        rejillaCajon.setVerticalSpacing(dp(6));
        rejillaCajon.setSelector(android.R.color.transparent);
        rejillaCajon.setVerticalScrollBarEnabled(false);
        rejillaCajon.setOverScrollMode(View.OVER_SCROLL_NEVER);
        vistaCajon.clear();
        vistaCajon.addAll(itemsCajon);
        adaptadorCajon = new Rejilla(vistaCajon, true);
        rejillaCajon.setAdapter(adaptadorCajon);
        rejillaCajon.setOnItemClickListener((p, v, i, id) -> pulsar(v, vistaCajon.get(i), true));
        rejillaCajon.setOnItemLongClickListener((p, v, i, id) -> { pulsarLargo(v, vistaCajon.get(i), true); return true; });
        recibir(rejillaCajon, AL_FINAL, true);
        int ancho = Math.min(dp(1180), getResources().getDisplayMetrics().widthPixels - dp(64));
        LinearLayout.LayoutParams lg = new LinearLayout.LayoutParams(ancho, 0, 1);
        lg.topMargin = barraEstado() + dp(40);
        col.addView(rejillaCajon, lg);
        resultadosAjustes = new LinearLayout(this);
        resultadosAjustes.setOrientation(LinearLayout.VERTICAL);
        resultadosAjustes.setBackground(new Vidrio.Placa(resultadosAjustes, dp(20), 0xB31E1E24));
        LinearLayout.LayoutParams lra = new LinearLayout.LayoutParams(Math.min(dp(520), ancho), ViewGroup.LayoutParams.WRAP_CONTENT);
        col.addView(resultadosAjustes, lra);

        LinearLayout barra = new LinearLayout(this);   // buscador abajo, con micrófono
        barra.setGravity(Gravity.CENTER_VERTICAL);
        barra.setPadding(dp(16), 0, dp(6), 0);
        barra.setBackground(new Vidrio.Placa(barra, dp(26), 0xCC26262C));
        barra.setElevation(dp(10));
        ImageView lupa = new ImageView(this);
        lupa.setImageDrawable(Paneles.de(this).icono("ic_search", TEXTO2));
        barra.addView(lupa, new LinearLayout.LayoutParams(dp(20), dp(20)));
        buscarCajon = new EditText(this);
        buscarCajon.setHint(Idioma.t("Buscar apps y ajustes", "Search apps and settings"));
        estilo(buscarCajon, 15, false, Color.WHITE);
        buscarCajon.setHintTextColor(TEXTO3);
        buscarCajon.setSingleLine(true);
        buscarCajon.setBackground(null);
        buscarCajon.setImeOptions(EditorInfo.IME_ACTION_GO);
        buscarCajon.setPadding(dp(12), 0, dp(8), 0);
        barra.addView(buscarCajon, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1));
        ImageView mic = iconoBoton("ic_mic", 40, v -> {
            try { startActivityForResult(new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
                    .putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM), PEDIR_VOZ); }
            catch (ActivityNotFoundException e) { Toast.makeText(this, Idioma.t("No hay reconocimiento de voz instalado", "Voice recognition isn't installed"), Toast.LENGTH_SHORT).show(); }
        });
        barra.addView(mic, new LinearLayout.LayoutParams(dp(40), dp(40)));
        LinearLayout.LayoutParams lb = new LinearLayout.LayoutParams(Math.min(dp(440), ancho), dp(52));
        lb.topMargin = dp(10);
        lb.bottomMargin = dp(36);
        col.addView(barra, lb);
        c.addView(col, lleno());
        ImageView mas = iconoBoton("ic_menu_moreoverflow_material", 44, v -> {
            PopupMenu m = new PopupMenu(this, v);
            m.getMenu().add(0, 1, 0, Idioma.t("Ordenar de la A a la Z", "Sort A to Z"));
            m.getMenu().add(0, 2, 1, Idioma.t("Ajustes de inicio", "Home settings"));
            m.setOnMenuItemClickListener(it -> {
                if (it.getItemId() == 1) { prefs.edit().remove("orden_cajon").apply(); refrescar(); }
                else Hojas.ajustes(this);
                return true;
            });
            mostrarMenu(m, v);
        });
        FrameLayout.LayoutParams lm = new FrameLayout.LayoutParams(dp(44), dp(44), Gravity.TOP | Gravity.END);
        lm.topMargin = barraEstado() + dp(8);
        lm.rightMargin = dp(24);
        c.addView(mas, lm);
        buscarCajon.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int d) { }
            @Override public void onTextChanged(CharSequence s, int a, int b, int d) { }
            @Override public void afterTextChanged(Editable s) { filtrarCajon(s.toString()); }
        });
        buscarCajon.setOnEditorActionListener((v, accion, e) -> {   // Intro abre lo primero, como Inicio de Windows
            for (Object o : vistaCajon) if (o instanceof App) { abrir((App) o); cerrarCajon(); return true; }
            if (resultadosAjustes.getChildCount() > 0) { resultadosAjustes.getChildAt(0).performClick(); return true; }
            return false;
        });
        raiz.addView(c, raiz.indexOfChild(zonasArrastre), lleno());   // debajo de las zonas de soltar y de lo que se abra encima
        fondoCajon.setAlpha(0f);
        col.setTranslationY(dp(48));
        fondoCajon.animate().alpha(1f).setDuration(167).setInterpolator(DESACELERA).withEndAction(() -> {
            if (cajon == c && arrastrada == null) { carril.setVisibility(View.INVISIBLE); raiz.setBackground(null); }   // lo de debajo (y el velo) no se dibuja de más
        });
        col.animate().translationY(0).setDuration(220).setInterpolator(DESACELERA);
        dockVista.animate().alpha(0f).setDuration(120);
        indicador.setVisibility(View.INVISIBLE);
        c.getViewTreeObserver().addOnPreDrawListener(new android.view.ViewTreeObserver.OnPreDrawListener() {
            @Override public boolean onPreDraw() {
                c.getViewTreeObserver().removeOnPreDrawListener(this);
                Log.i("inicio", "cajon dibujado a los " + (SystemClock.uptimeMillis() - t0) + " ms");
                return true;
            }
        });
    }

    void cerrarCajon() {
        FrameLayout c = cajon;
        if (c == null) return;
        if (seleccionando && seleccionEnCajon) salirSeleccion();
        ocultarTeclado();
        carril.setVisibility(View.VISIBLE);
        atenuar();
        dockVista.animate().alpha(1f).setDuration(160);
        indicador.setVisibility(View.VISIBLE);
        cajon = null;
        c.getChildAt(0).animate().alpha(0f).setDuration(140).setInterpolator(ACELERA);
        for (int i = 1; i < c.getChildCount(); i++) c.getChildAt(i).animate().translationY(dp(48)).alpha(0f).setDuration(120).setInterpolator(ACELERA);
        c.postDelayed(() -> raiz.removeView(c), 150);
        rejillaCajon = null; adaptadorCajon = null; buscarCajon = null; resultadosAjustes = null;
    }

    /** Arrastre desde el cajón al borde de arriba: el cajón se aparta (sigue en la ventana hasta soltar) y se ve el inicio. */
    void dejarVerInicio() {
        carril.setVisibility(View.VISIBLE);   // ViewGroup avisa del arrastre en curso a lo que se vuelve visible
        atenuar();
        dockVista.setAlpha(1f);
        cajon.setVisibility(View.INVISIBLE);
    }

    void quitarCajon() {
        FrameLayout c = cajon;
        if (c == null) return;
        cajon = null;
        raiz.removeView(c);
        carril.setVisibility(View.VISIBLE);
        atenuar();
        rejillaCajon = null; adaptadorCajon = null; buscarCajon = null; resultadosAjustes = null;
        indicador.setVisibility(View.VISIBLE);
    }

    void ocultarTeclado() {
        if (buscarCajon != null) getSystemService(InputMethodManager.class).hideSoftInputFromWindow(buscarCajon.getWindowToken(), 0);
    }

    static String normalizar(String s) {
        return Normalizer.normalize(s, Normalizer.Form.NFD).replaceAll("\\p{M}", "").toLowerCase(Locale.ROOT);
    }

    /** Busca apps (también las de dentro de carpetas) y opciones de Ajustes (lista local: sin esperas). */
    void filtrarCajon(String texto) {
        if (adaptadorCajon == null) return;
        String q = normalizar(texto.trim());
        vistaCajon.clear();
        if (q.isEmpty()) vistaCajon.addAll(itemsCajon);
        else for (App a : apps) if (normalizar(a.nombre).contains(q)) vistaCajon.add(a);
        adaptadorCajon.notifyDataSetChanged();
        resultadosAjustes.removeAllViews();
        if (q.isEmpty()) return;
        int n = 0;
        for (String[] a : AJUSTES) {
            String nom = Idioma.t(a[0], a[3]);
            if (n == 4 || !(normalizar(nom).contains(q) || normalizar(a[1]).contains(q))) continue;
            n++;
            resultadosAjustes.addView(filaMenu(ico("ic_settings_24dp"), nom + "  ·  " + Idioma.t("Ajustes", "Settings"), false, v -> abrirAjuste(a[2], null)));
        }
        resultadosAjustes.addView(filaMenu(ico("ic_search"), Idioma.t("Buscar «" + texto.trim() + "» en Ajustes", "Search Settings for “" + texto.trim() + "”"), false,
                v -> abrirAjuste("android.settings.APP_SEARCH_SETTINGS", texto.trim())));
    }

    void abrirAjuste(String accion, String consulta) {
        if (accion.equals("fondo")) { cerrarCajon(); Hojas.fondo(this); return; }
        // shortcut: la búsqueda de Ajustes (SettingsIntelligence) no recibe el texto por un extra público; si falla se abre Ajustes.
        Intent i = new Intent(accion).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (consulta != null) i.putExtra(android.app.SearchManager.QUERY, consulta);
        try { startActivity(i); }
        catch (ActivityNotFoundException e) {
            try { startActivity(new Intent("com.android.settings.action.SETTINGS_SEARCH").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
            catch (ActivityNotFoundException e2) { startActivity(new Intent(Settings.ACTION_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
        }
        cerrarCajon();
    }

    // Opciones de Ajustes que se encuentran desde el cajón: nombre, palabras clave (en los dos idiomas), acción, nombre en inglés.
    static final String[][] AJUSTES = {
            {"Wi-Fi", "internet red inalambrica wifi network wireless", Settings.ACTION_WIFI_SETTINGS, "Wi-Fi"},
            {"Bluetooth", "auriculares dispositivos conectados headphones connected devices", Settings.ACTION_BLUETOOTH_SETTINGS, "Bluetooth"},
            {"Red e Internet", "datos movil avion vpn mobile data airplane", Settings.ACTION_WIRELESS_SETTINGS, "Network & internet"},
            {"Pantalla", "brillo tamano fuente resolucion tiempo de espera brightness font size resolution timeout", Settings.ACTION_DISPLAY_SETTINGS, "Display"},
            {"Modo oscuro", "tema noche claro night light mode", "android.settings.DARK_THEME_SETTINGS", "Dark theme"},
            {"Fondo de pantalla y estilo", "fondo pantalla wallpaper estilo paleta background palette", "fondo", "Wallpaper & style"},
            {"Sonido y vibración", "volumen tono silencio volume ringtone silent", Settings.ACTION_SOUND_SETTINGS, "Sound & vibration"},
            {"Notificaciones", "avisos alertas alerts", "android.settings.NOTIFICATION_SETTINGS", "Notifications"},
            {"No molestar", "silencio concentracion silent focus", "android.settings.ZEN_MODE_SETTINGS", "Do Not Disturb"},
            {"Batería", "energia ahorro consumo carga power saver usage charge", "android.intent.action.POWER_USAGE_SUMMARY", "Battery"},
            {"Almacenamiento", "espacio memoria archivos space memory files", Settings.ACTION_INTERNAL_STORAGE_SETTINGS, "Storage"},
            {"Aplicaciones", "apps permisos predeterminadas permissions default", Settings.ACTION_APPLICATION_SETTINGS, "Apps"},
            {"Ubicación", "gps localizacion", Settings.ACTION_LOCATION_SOURCE_SETTINGS, "Location"},
            {"Seguridad", "bloqueo pantalla contrasena pin huella screen lock password fingerprint", Settings.ACTION_SECURITY_SETTINGS, "Security"},
            {"Privacidad", "permisos datos permissions data", Settings.ACTION_PRIVACY_SETTINGS, "Privacy"},
            {"Cuentas", "google sincronizar usuarios sync", Settings.ACTION_SYNC_SETTINGS, "Accounts"},
            {"Accesibilidad", "lector zoom texto grande reader large text", Settings.ACTION_ACCESSIBILITY_SETTINGS, "Accessibility"},
            {"Idioma", "idiomas region espanol languages english", Settings.ACTION_LOCALE_SETTINGS, "Language"},
            {"Teclado", "metodo de entrada escritura input method typing", Settings.ACTION_INPUT_METHOD_SETTINGS, "Keyboard"},
            {"Fecha y hora", "zona horaria reloj time zone clock", Settings.ACTION_DATE_SETTINGS, "Date & time"},
            {"Acerca de la tableta", "version android informacion modelo information model", Settings.ACTION_DEVICE_INFO_SETTINGS, "About tablet"},
            {"Opciones para desarrolladores", "depuracion usb adb debugging", Settings.ACTION_APPLICATION_DEVELOPMENT_SETTINGS, "Developer options"},
            {"Usuarios", "perfiles invitado profiles guest", "android.settings.USER_SETTINGS", "Users"},
            {"Sistema", "actualizacion copia restablecer update backup reset", "android.settings.SYSTEM_UPDATE_SETTINGS", "System"},
    };

    // ---------- hojas a pantalla completa (ajustes del inicio, fondo y estilo, selector de widgets) ----------

    /** Hoja sobre el fondo difuminado, con título y flecha atrás; en esta ventana: se abre al instante. */
    FrameLayout abrirHoja(String titulo, View contenido, boolean desplazable) {
        cerrarHoja();
        cerrarCarpeta();
        FrameLayout h = hoja = new FrameLayout(this);
        h.setClickable(true);
        Vidrio.Fondo fondoHoja = new Vidrio.Fondo(this, 0xD90B0B0F);
        h.addView(fondoHoja, lleno());
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(dp(32), barraEstado() + dp(12), dp(32), 0);
        LinearLayout cab = new LinearLayout(this);
        cab.setGravity(Gravity.CENTER_VERTICAL);
        ImageView volver = iconoBoton("ic_ab_back_material", 44, v -> cerrarHoja());
        volver.setColorFilter(ACENTO);
        cab.addView(volver, new LinearLayout.LayoutParams(dp(44), dp(44)));
        TextView t = new TextView(this);
        estilo(t, 22, true, ACENTO);
        t.setText(titulo);
        t.setPadding(dp(8), 0, 0, 0);
        cab.addView(t);
        col.addView(cab);
        if (desplazable) {
            ScrollView sv = new ScrollView(this);
            sv.setVerticalScrollBarEnabled(false);
            FrameLayout centro = new FrameLayout(this);
            centro.setPadding(0, dp(8), 0, dp(40));
            centro.addView(contenido, new FrameLayout.LayoutParams(Math.min(dp(760), getResources().getDisplayMetrics().widthPixels - dp(64)),
                    ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER_HORIZONTAL));
            sv.addView(centro);
            col.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        } else col.addView(contenido, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        h.addView(col, lleno());
        raiz.addView(h, lleno());
        fondoHoja.setAlpha(0f);
        col.setTranslationY(dp(32));
        fondoHoja.animate().alpha(1f).setDuration(167).setInterpolator(DESACELERA);
        col.animate().translationY(0).setDuration(220).setInterpolator(DESACELERA);
        return h;
    }

    void cerrarHoja() {
        FrameLayout h = hoja;
        if (h == null) return;
        hoja = null;
        if (rehacerAlCerrar) { rehacerAlCerrar = false; raiz.removeView(h); mostrar(); return; }   // cambió el tamaño o la cuadrícula
        getSystemService(InputMethodManager.class).hideSoftInputFromWindow(h.getWindowToken(), 0);
        h.getChildAt(0).animate().alpha(0f).setDuration(140).setInterpolator(ACELERA);
        h.getChildAt(1).animate().translationY(dp(32)).alpha(0f).setDuration(120).setInterpolator(ACELERA);
        h.postDelayed(() -> raiz.removeView(h), 150);
    }

    // ---------- modo edición (pellizco) ----------

    void abrirEdicion() {
        if (edicion != null || carril == null) return;
        cerrarTodo();
        edicion = new Edicion(this);
    }

    // ---------- estilo ----------

    /** Se rasteriza una vez al cargar apps; no depende de la máscara circular del sistema. */
    Drawable iconoUniforme(Drawable original) {
        int t = dp(64);
        Bitmap b = Bitmap.createBitmap(t, t, Bitmap.Config.ARGB_8888);
        Canvas c = new Canvas(b);
        Path mascara = new Path();
        mascara.addRoundRect(new RectF(0, 0, t, t), t * .22f, t * .22f, Path.Direction.CW);
        c.clipPath(mascara);
        Rect anterior = new Rect(original.getBounds());
        if (original instanceof AdaptiveIconDrawable) {
            AdaptiveIconDrawable a = (AdaptiveIconDrawable) original;
            Drawable fondo = a.getBackground(), frente = a.getForeground();
            for (Drawable capa : new Drawable[]{fondo, frente}) {
                if (capa == null) continue;
                Rect bounds = new Rect(capa.getBounds());
                capa.setBounds(-t / 4, -t / 4, t + t / 4, t + t / 4);
                capa.draw(c);
                capa.setBounds(bounds);
            }
        } else {
            c.drawColor(Color.WHITE);
            int w = original.getIntrinsicWidth(), h = original.getIntrinsicHeight();
            float escala = t * .72f / Math.max(1, Math.max(w, h));
            int iw = w > 0 && h > 0 ? Math.round(w * escala) : Math.round(t * .72f);
            int ih = w > 0 && h > 0 ? Math.round(h * escala) : Math.round(t * .72f);
            original.setBounds((t - iw) / 2, (t - ih) / 2, (t + iw) / 2, (t + ih) / 2);
            original.draw(c);
        }
        original.setBounds(anterior);
        assert b.getWidth() == b.getHeight() && Color.alpha(b.getPixel(0, 0)) == 0 : "Máscara de icono inválida";
        return new BitmapDrawable(getResources(), b);
    }

    void mostrarMenu(PopupMenu menu, View ancla) {
        LinearLayout opciones = new LinearLayout(this);
        opciones.setOrientation(LinearLayout.VERTICAL);
        opciones.setPadding(dp(6), dp(8), dp(6), dp(8));
        opciones.setMinimumWidth(dp(220));
        PopupWindow[] v = new PopupWindow[1];
        for (int i = 0; i < menu.getMenu().size(); i++) {
            android.view.MenuItem item = menu.getMenu().getItem(i);
            TextView fila = new TextView(this);
            estilo(fila, 15, false, Color.WHITE);
            fila.setText(item.getTitle());
            fila.setGravity(Gravity.CENTER_VERTICAL);
            fila.setMinHeight(dp(46));
            fila.setFocusable(true);
            fila.setBackground(seleccionable());
            fila.setPadding(dp(18), dp(8), dp(18), dp(8));
            fila.setOnClickListener(x -> {
                if (v[0] != null) v[0].dismiss();
                menu.getMenu().performIdentifierAction(item.getItemId(), 0);
            });
            opciones.addView(fila);
        }
        v[0] = mostrarVentana(opciones, ancla);
    }

    /** Ventana flotante de vidrio junto al ancla. Con el dedo aún puesto no es modal: moverlo inicia el arrastre (dispatchTouchEvent). */
    PopupWindow mostrarVentana(View contenido, View ancla) {
        contenido.setBackground(new Vidrio.Placa(contenido, dp(24), 0xD91E1E24));
        ScrollView scroll = new ScrollView(this);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.addView(contenido);
        PopupWindow ventana = new PopupWindow(scroll, ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, !tocando);
        if (menuActual != null) menuActual.dismiss();
        menuActual = ventana;
        ventana.setOnDismissListener(() -> {
            if (menuActual == ventana) menuActual = null;
            pendienteArrastre = null; anclaPendiente = null;
        });
        ventana.setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(Color.TRANSPARENT));
        ventana.setElevation(dp(12));
        ventana.setAnimationStyle(android.R.style.Animation_Dialog);
        // Conserva la secuencia original hasta soltar o iniciar el arrastre.
        ventana.setTouchModal(!tocando);
        ventana.setOutsideTouchable(true);
        // Sin sitio debajo (iconos del dock): encima del icono. Antes el menú del dock salía cortado al pie (17/11).
        scroll.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        int[] p = new int[2];
        ancla.getLocationOnScreen(p);
        int alto = scroll.getMeasuredHeight(), debajo = getResources().getDisplayMetrics().heightPixels - p[1] - ancla.getHeight();
        ventana.showAsDropDown(ancla, 0, debajo < alto + dp(16) ? -(ancla.getHeight() + alto + dp(8)) : -dp(8));
        return ventana;
    }

    static boolean gestoNotificaciones(float dx, float dy, int umbral) {
        return dy > umbral && dy > Math.abs(dx) * 2;
    }

    static boolean sobreControl(View v, int x, int y) {
        if (v.getVisibility() != View.VISIBLE) return false;
        int[] p = new int[2];
        v.getLocationOnScreen(p);
        if (x < p[0] || y < p[1] || x >= p[0] + v.getWidth() || y >= p[1] + v.getHeight()) return false;
        if (v instanceof GridView) return ((GridView) v).pointToPosition(x - p[0], y - p[1]) != GridView.INVALID_POSITION;
        // raiz tiene pulsación larga (agregar widget): en contenedores solo cuenta el clic, no la pulsación larga.
        if (v.isClickable() || (v.isLongClickable() && !(v instanceof ViewGroup)) || v instanceof android.appwidget.AppWidgetHostView) return true;
        if (v instanceof ViewGroup)
            for (int i = ((ViewGroup) v).getChildCount() - 1; i >= 0; i--)
                if (sobreControl(((ViewGroup) v).getChildAt(i), x, y)) return true;
        return false;
    }

    static boolean superaSlop(float dx, float dy, int slop) {
        return dx * dx + dy * dy > slop * slop;
    }

    static float distancia(MotionEvent e) {
        float dx = e.getX(0) - e.getX(1), dy = e.getY(0) - e.getY(1);
        return (float) Math.sqrt(dx * dx + dy * dy);
    }

    /**
     * Gestos del inicio: deslizar de lado = páginas (y Discover), hacia arriba = cajón, hacia abajo = notificaciones,
     * pellizcar = modo edición, doble toque en vacío = apagar pantalla. Mantener pulsado y mover = arrastrar.
     */
    boolean cortina;   // el gesto de bajar sobre el inicio lleva el panel de avisos con el dedo (Paneles.tiron)
    @Override public boolean dispatchTouchEvent(MotionEvent e) {
        int accion = e.getActionMasked();
        toqueX = e.getRawX(); toqueY = e.getRawY();
        int slop = ViewConfiguration.get(this).getScaledTouchSlop();
        if (accion == MotionEvent.ACTION_DOWN) {
            vacioX = toqueX; vacioY = toqueY;
            libre = carpetaAbierta == null && arrastrada == null && menuActual == null && hoja == null && edicion == null && !seleccionando;
            gestoVacio = libre && cajon == null;
            gestoConsumido = false; paginando = false; pellizco0 = 0; cortina = false;
            if (velocidad == null) velocidad = VelocityTracker.obtain(); else velocidad.clear();
        } else if (accion == MotionEvent.ACTION_POINTER_DOWN) {
            gestoVacio = false;
            pendienteArrastre = null; anclaPendiente = null;
            if (e.getPointerCount() == 2 && libre && cajon == null && !paginando) pellizco0 = distancia(e);
        }
        if (velocidad != null) velocidad.addMovement(e);
        if (gestoConsumido) {
            if (cortina) {
                // otro dedo: se suelta aquí (getRawY pasaría a ser de otro dedo y la tarjeta saltaría)
                boolean fin = accion == MotionEvent.ACTION_UP || accion == MotionEvent.ACTION_CANCEL || accion == MotionEvent.ACTION_POINTER_DOWN;
                if (fin) velocidad.computeCurrentVelocity(1000);
                Paneles.de(this).tiron("noti", (int) (toqueY - vacioY), fin, accion == MotionEvent.ACTION_UP ? (int) velocidad.getYVelocity() : 0);
                if (fin) cortina = false;
            }
            if (accion == MotionEvent.ACTION_UP || accion == MotionEvent.ACTION_CANCEL) {
                gestoConsumido = false; gestoVacio = false; tocando = false;
            }
            return true;
        }
        if (accion == MotionEvent.ACTION_MOVE && pellizco0 > 0 && e.getPointerCount() >= 2 && distancia(e) < pellizco0 * .72f) {
            pellizco0 = 0;
            cancelarHijos(e);
            gestoConsumido = true;
            abrirEdicion();
            return true;
        }
        if (paginando) {
            carril.mover(toqueX - vacioX);
            if (accion == MotionEvent.ACTION_UP || accion == MotionEvent.ACTION_CANCEL) {
                velocidad.computeCurrentVelocity(1000);
                carril.soltar(accion == MotionEvent.ACTION_UP ? velocidad.getXVelocity() : 0);
                paginando = false; tocando = false; gestoVacio = false; pendienteArrastre = null; anclaPendiente = null;
            }
            return true;
        }
        if (accion == MotionEvent.ACTION_MOVE && pendienteArrastre != null
                && superaSlop(toqueX - largoX, toqueY - largoY, slop)) {
            App a = pendienteArrastre;
            View ancla = anclaPendiente;
            if (menuActual != null) menuActual.dismiss();
            prepararArrastre(ancla, a);
            cancelarHijos(e);
            gestoConsumido = true;
            return true;
        }
        if (accion == MotionEvent.ACTION_MOVE && libre && cajon == null && e.getPointerCount() == 1 && pendienteArrastre == null && carril != null) {
            float dx = toqueX - vacioX, dy = toqueY - vacioY;
            if (Math.abs(dx) > slop * 2 && Math.abs(dx) > Math.abs(dy) * 2 && carril.puede(dx)) {
                paginando = true;
                cancelarHijos(e);
                carril.empezar(dx);
                return true;
            }
            // Cajón: hacia arriba sobre el inicio. El borde inferior es del visor (inicio/Recientes): ese no cuenta.
            // En Discover lo vertical es desplazar las noticias (17/11: abría el cajón): solo quedan los bordes del visor.
            if (!enDiscover() && dy < -dp(64) && -dy > Math.abs(dx) * 2 && vacioY < getResources().getDisplayMetrics().heightPixels - dp(48)
                    && !desplazaHaciaAbajo(paginaVisible(), vacioX, vacioY)) {
                cancelarHijos(e);
                gestoConsumido = true;
                abrirCajon();
                return true;
            }
        }
        // Cajón abierto: hacia abajo vuelve al inicio, desde cualquier sitio (17/11: solo valía sobre los iconos) si la lista
        // está arriba del todo o el dedo empezó fuera de ella.
        if (accion == MotionEvent.ACTION_MOVE && libre && cajon != null && e.getPointerCount() == 1 && pendienteArrastre == null) {
            float dx = toqueX - vacioX, dy = toqueY - vacioY;
            if (dy > dp(56) && dy > Math.abs(dx) * 2
                    && (rejillaCajon == null || !rejillaCajon.canScrollVertically(-1) || !dentroDe(rejillaCajon, vacioX, vacioY))) {
                cancelarHijos(e);
                gestoConsumido = true;
                cerrarCajon();
                return true;
            }
        }
        if (accion == MotionEvent.ACTION_MOVE && gestoVacio && carpetaAbierta == null && !enDiscover()
                && arrastrada == null && menuActual == null && rejilla != null && prefs.getBoolean("deslizar_noti", true)
                && !rejilla.canScrollVertically(-1)
                && gestoNotificaciones(toqueX - vacioX, toqueY - vacioY, dp(48))) {
            cancelarHijos(e);
            gestoConsumido = true;
            // nuestro panel de avisos (foto N), que baja con el dedo como la cortina (18/11: antes salía entero de golpe)
            cortina = true;
            Paneles.de(this).tiron("noti", (int) (toqueY - vacioY), false, 0);
            return true;
        }
        if (accion == MotionEvent.ACTION_MOVE && gestoVacio && superaSlop(toqueX - vacioX, toqueY - vacioY, slop)) {
            toqueFondo = false;   // ya no es un toque: no cuenta para el doble toque (dormir) aunque el dedo vuelva
            if (!gestoNotificaciones(toqueX - vacioX, toqueY - vacioY, 0)) gestoVacio = false;   // hacia abajo aún puede abrir el panel
        }
        boolean resultado = super.dispatchTouchEvent(e);
        // "tocando" también se marca en los huecos de la rejilla (tocarApp está en la GridView): se mira qué hay bajo el dedo.
        if (accion == MotionEvent.ACTION_DOWN)
            toqueFondo = gestoVacio && !sobreControl(getWindow().getDecorView(), (int) toqueX, (int) toqueY);
        if (accion == MotionEvent.ACTION_UP || accion == MotionEvent.ACTION_CANCEL) {
            if (gestoVacio && toqueFondo && accion == MotionEvent.ACTION_UP && prefs.getBoolean("doble_toque", true)) {
                long ahora = System.currentTimeMillis();
                int s2 = slop * 2;
                if (ahora - ultimoToqueVacio < 300 && !superaSlop(toqueX - ultimoVacioX, toqueY - ultimoVacioY, s2)) {
                    dormir();
                    ultimoToqueVacio = 0;
                } else {
                    ultimoToqueVacio = ahora;
                    ultimoVacioX = toqueX; ultimoVacioY = toqueY;
                }
            }
            tocando = false;
            gestoVacio = false;
            pendienteArrastre = null; anclaPendiente = null;
            if (menuActual != null) {
                if (accion == MotionEvent.ACTION_CANCEL) menuActual.dismiss();
                else { menuActual.setFocusable(true); menuActual.setTouchModal(true); menuActual.update(); }
            }
        }
        return resultado;
    }

    boolean enDiscover() { return carril != null && carril.pagina < carril.base(); }

    View paginaVisible() { return carril == null || carril.getChildCount() == 0 ? null : carril.getChildAt(Math.min(carril.pagina, carril.getChildCount() - 1)); }

    /** ¿Hay bajo el dedo algo que aún se desplace hacia abajo (página de widgets, columna o rejilla)? Entonces no es el cajón. */
    static boolean desplazaHaciaAbajo(View v, float x, float y) {
        if (v == null || v.getVisibility() != View.VISIBLE || !dentroDe(v, x, y)) return false;
        if ((v instanceof ScrollView || v instanceof GridView) && v.canScrollVertically(1)) return true;
        if (v instanceof ViewGroup) for (int i = 0; i < ((ViewGroup) v).getChildCount(); i++)
            if (desplazaHaciaAbajo(((ViewGroup) v).getChildAt(i), x, y)) return true;
        return false;
    }

    static boolean dentroDe(View v, float x, float y) {
        int[] p = new int[2];
        v.getLocationOnScreen(p);
        return x >= p[0] && y >= p[1] && x < p[0] + v.getWidth() && y < p[1] + v.getHeight();
    }

    boolean gestoConsumido;

    void cancelarHijos(MotionEvent e) {
        MotionEvent cancel = MotionEvent.obtain(e);
        cancel.setAction(MotionEvent.ACTION_CANCEL);
        super.dispatchTouchEvent(cancel);
        cancel.recycle();
    }

    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        // Escribir con el cajón cerrado lo abre y busca (como Inicio de Windows).
        if (e.getAction() == KeyEvent.ACTION_DOWN && cajon == null && hoja == null && e.isPrintingKey() && !e.isCtrlPressed()) {
            abrirCajon();
            buscarCajon.requestFocus();
        }
        return super.dispatchKeyEvent(e);
    }

    int dp(float v) { return (int) (v * getResources().getDisplayMetrics().density + 0.5f); }

    int barraEstado() {
        int id = getResources().getIdentifier("status_bar_height", "dimen", "android");
        return id > 0 ? getResources().getDimensionPixelSize(id) : dp(24);
    }

    LinearLayout.LayoutParams margenArriba(int m) {
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        l.topMargin = m;
        return l;
    }

    GradientDrawable fondo(int color, int borde, int radio) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(color);
        g.setCornerRadius(dp(radio));
        if (borde != 0) g.setStroke(dp(1), borde);
        return g;
    }

    void estilo(TextView t, float sp, boolean seminegrita, int color) {
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        Typeface f = seminegrita ? semi : normal;
        t.setTypeface(f != null ? f : Typeface.create(Typeface.DEFAULT, seminegrita ? 600 : 400, false));
    }

    Button boton(String texto, int colorTexto, int fondoColor, int borde) {
        Button b = new Button(this);
        b.setText(texto);
        b.setAllCaps(false);
        estilo(b, 16, true, colorTexto);
        b.setMinHeight(dp(48));
        b.setPadding(dp(24), 0, dp(24), 0);
        b.setBackground(fondo(fondoColor, borde, 24));
        b.setStateListAnimator(null);
        return b;
    }
}
