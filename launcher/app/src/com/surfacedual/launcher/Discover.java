package com.surfacedual.launcher;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Process;
import android.os.RemoteException;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Log;
import android.util.Xml;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.xmlpull.v1.XmlPullParser;

/**
 * Página -1 del inicio (foto M). Primero intenta el Discover de verdad: el servicio de superposición de la app de Google
 * (com.android.launcher3.WINDOW_OVERLAY, protocolo ILauncherOverlay que usan Lawnchair/Pixel Launcher, transacciones a mano).
 * Google solo lo acepta de launchers de sistema o depurables (Bridge/Lawnfeed existen por eso). Si no responde, nuestra
 * propia página con las noticias destacadas de Google Noticias (RSS) para el idioma y país del sistema.
 */
class Discover {
    static final String TAG = "discover", GSA = "com.google.android.googlequicksearchbox", PUENTE = "com.surfacedual.feed";
    static final String DESC = "com.google.android.libraries.launcherclient.ILauncherOverlay",
            DESC_CB = "com.google.android.libraries.launcherclient.ILauncherOverlayCallback";
    static final int START_SCROLL = 1, ON_SCROLL = 2, END_SCROLL = 3, WINDOW_ATTACHED = 4, WINDOW_DETACHED = 5, CLOSE_OVERLAY = 6,
            ON_PAUSE = 7, ON_RESUME = 8, OPEN_OVERLAY = 9, WINDOW_ATTACHED2 = 14, SET_ACTIVITY_STATE = 16;

    static class Noticia { String titulo, fuente, enlace; long fecha; }
    static final List<Noticia> noticias = new ArrayList<>();
    static long cargadas;

    final Inicio ini;
    IBinder remoto;
    int api = 1, estadoGoogle, estadoActividad;
    boolean enlazado, cargando;
    LinearLayout[] columnas;
    TextView aviso;

    Discover(Inicio i) { ini = i; }

    boolean activo() { return ini.prefs.getBoolean("discover", true); }
    // El de Google por omisión (medido 17/11: con la cuenta del usuario y el puente, Google lo acepta y pinta su feed); si no
    // responde, queda la página propia de Google Noticias.
    boolean usarGoogle() { return activo() && ini.prefs.getBoolean("discover_google", true); }
    boolean googleListo() { return usarGoogle() && remoto != null && (estadoGoogle & 1) != 0; }
    boolean paginaPropia() { return activo() && !googleListo(); }

    // ---------- Discover de Google (superposición) ----------

    final ServiceConnection conexion = new ServiceConnection() {
        @Override public void onServiceConnected(ComponentName n, IBinder b) { remoto = b; Log.i(TAG, "conectado a " + n + " api " + api); adjuntar(0); }
        @Override public void onServiceDisconnected(ComponentName n) { remoto = null; estadoCambio(0); }
        @Override public void onNullBinding(ComponentName n) { Log.i(TAG, "Google rechazó este launcher (sin binder): página propia"); }
    };

    final Binder retorno = new Binder() {
        { attachInterface(null, DESC_CB); }
        @Override protected boolean onTransact(int codigo, Parcel d, Parcel r, int flags) throws RemoteException {
            if (codigo == 1) { d.enforceInterface(DESC_CB); float p = d.readFloat(); Vidrio.principal.post(() -> progreso(p)); return true; }
            if (codigo == 2) { d.enforceInterface(DESC_CB); int s = d.readInt(); Vidrio.principal.post(() -> estadoCambio(s)); return true; }
            return super.onTransact(codigo, d, r, flags);
        }
    };

    void alIniciar() {
        estadoActividad |= 1;
        if (!usarGoogle()) { if (enlazado) cerrar(); return; }
        if (enlazado) { if (remoto != null) adjuntar(0); return; }
        Uri uri = Uri.parse("app://" + ini.getPackageName() + ":" + Process.myUid()).buildUpon()
                .appendQueryParameter("v", "7").appendQueryParameter("cv", "9").build();
        Intent i = new Intent("com.android.launcher3.WINDOW_OVERLAY").setPackage(GSA).setData(uri);
        ResolveInfo ri = ini.getPackageManager().resolveService(i, PackageManager.GET_META_DATA);
        if (ri == null) { Log.i(TAG, "la app de Google no ofrece la superposición: página propia"); return; }
        api = ri.serviceInfo.metaData == null ? 1 : ri.serviceInfo.metaData.getInt("service.api.version", 1);
        // Con el puente instalado (launcher/feed, depurable como Lawnfeed) se habla con Google a través de él: a un launcher
        // que no es de sistema ni depurable Google no le da el Discover (medido 16/11: onNullBinding).
        Intent puente = new Intent(i).setPackage(PUENTE);
        if (ini.getPackageManager().resolveService(puente, 0) != null) i = puente;
        try { enlazado = ini.bindService(i, conexion, Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT); Log.i(TAG, "enlazando con " + i.getPackage() + " (api " + api + "): " + enlazado); }
        catch (SecurityException e) { Log.i(TAG, "sin permiso para enlazar: " + e); }
    }

    void alParar() { estadoActividad &= ~1; estado(0); }

    /** 1 = iniciada, 2 = en primer plano (setActivityState). */
    void estado(int s) {
        estadoActividad = (estadoActividad & 1) | (s & 2) | (s & 1);
        if (api >= 4) llamar(SET_ACTIVITY_STATE, d -> d.writeInt(estadoActividad));
        else llamar((estadoActividad & 2) != 0 ? ON_RESUME : ON_PAUSE, null);
    }

    void adjuntar(int intento) {
        if (remoto == null) return;
        WindowManager.LayoutParams lp = ini.getWindow().getAttributes();
        if (lp.token == null) { if (intento < 20) ini.getWindow().getDecorView().postDelayed(() -> adjuntar(intento + 1), 100); return; }
        int opciones = 1 | 2 | 4 | 8;   // las de Lawnchair: Discover activado y lo demás de Pixel Launcher
        if (api < 3) llamar(WINDOW_ATTACHED, d -> { d.writeInt(1); lp.writeToParcel(d, 0); d.writeStrongBinder(retorno); d.writeInt(opciones); });
        else {
            Bundle b = new Bundle();
            b.putParcelable("layout_params", lp);
            b.putParcelable("configuration", ini.getResources().getConfiguration());
            b.putInt("client_options", opciones);
            llamar(WINDOW_ATTACHED2, d -> { d.writeInt(1); b.writeToParcel(d, 0); d.writeStrongBinder(retorno); });
        }
        estado(estadoActividad);
    }

    void llamar(int codigo, java.util.function.Consumer<Parcel> args) {
        IBinder r = remoto;
        if (r == null) return;
        Parcel d = Parcel.obtain();
        try {
            d.writeInterfaceToken(DESC);
            if (args != null) args.accept(d);
            r.transact(codigo, d, null, IBinder.FLAG_ONEWAY);
        } catch (RemoteException | RuntimeException e) { Log.w(TAG, "transacción " + codigo, e); }
        finally { d.recycle(); }
    }

    void estadoCambio(int s) {
        boolean antes = googleListo();
        estadoGoogle = s;
        Log.i(TAG, "estado de Google = " + s + (googleListo() ? " (Discover de Google activo)" : " (no acepta este launcher: página propia)"));
        if (antes != googleListo() && ini.carril != null && (ini.carril.base() > 0) != paginaPropia()) ini.mostrar();
    }

    void progreso(float p) {   // la superposición de Google se abre: el inicio se aparta un poco
        if (ini.carril == null) return;
        ini.carril.setTranslationX(p * ini.dp(56));
        ini.carril.setAlpha(1f - p * .7f);
        // Nunca a medias: si se queda entre 0 y 1 sin un dedo que la lleve (gesto cortado, reinicio, giro), se asienta.
        ini.carril.removeCallbacks(asentar);
        if (p > 0f && p < 1f) ini.carril.postDelayed(asentar, 400);
        ultimo = p;
    }
    float ultimo;
    final Runnable asentar = this::asentar;
    void asentar() {
        if (ini.carril == null || ini.carril.google || ultimo <= 0f || ultimo >= 1f) return;
        if (ultimo >= .5f) abrir(); else cerrarSuperposicion();
    }

    void empezar() { llamar(START_SCROLL, null); }
    void desplazar(float p) { llamar(ON_SCROLL, d -> d.writeFloat(p)); }
    void terminar() { llamar(END_SCROLL, null); }
    void abrir() { llamar(OPEN_OVERLAY, d -> d.writeInt(1)); }
    void cerrarSuperposicion() { llamar(CLOSE_OVERLAY, d -> d.writeInt(1)); }   // 1 = con animación

    void cerrar() {
        if (remoto != null) llamar(WINDOW_DETACHED, d -> d.writeInt(ini.isChangingConfigurations() ? 1 : 0));
        if (enlazado) try { ini.unbindService(conexion); } catch (IllegalArgumentException ignored) { }
        enlazado = false;
        remoto = null;
    }

    // ---------- página propia (Google Noticias) ----------

    View pagina() {
        FrameLayout raiz = new FrameLayout(ini);
        raiz.setBackgroundColor(0xE6000000);   // como el Discover oscuro de la foto M
        ScrollView sv = new ScrollView(ini);
        sv.setVerticalScrollBarEnabled(false);
        LinearLayout col = new LinearLayout(ini);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(0, ini.barraEstado() + ini.dp(28), 0, ini.dp(48));
        LinearLayout cab = new LinearLayout(ini);
        cab.setGravity(Gravity.CENTER_VERTICAL);
        TextView g = new TextView(ini);
        ini.estilo(g, 34, true, Color.WHITE);
        g.setText("Google");
        cab.addView(g, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        TextView buscar = new TextView(ini);
        ini.estilo(buscar, 15, false, Inicio.TEXTO2);
        buscar.setText("Buscar en Google");
        buscar.setGravity(Gravity.CENTER_VERTICAL);
        buscar.setPadding(ini.dp(20), 0, ini.dp(40), 0);
        buscar.setBackground(ini.fondo(0xFF202124, 0x26FFFFFF, 24));
        buscar.setOnClickListener(v -> buscarEnGoogle());
        cab.addView(buscar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ini.dp(48)));
        col.addView(cab);
        LinearLayout chips = new LinearLayout(ini);
        chips.addView(chip(new SimpleDateFormat("EEEE d 'de' MMMM", new Locale("es")).format(new java.util.Date()), "Hoy"));
        chips.addView(chip("Noticias destacadas", "Google Noticias · " + region()[1]));
        LinearLayout.LayoutParams lch = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lch.topMargin = ini.dp(20);
        col.addView(chips, lch);
        aviso = new TextView(ini);
        ini.estilo(aviso, 14, false, Inicio.TEXTO3);
        aviso.setPadding(0, ini.dp(20), 0, 0);
        col.addView(aviso);
        LinearLayout tarjetas = new LinearLayout(ini);
        columnas = new LinearLayout[2];
        for (int k = 0; k < 2; k++) {
            columnas[k] = new LinearLayout(ini);
            columnas[k].setOrientation(LinearLayout.VERTICAL);
            LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1);
            if (k == 1) l.leftMargin = ini.dp(16);
            tarjetas.addView(columnas[k], l);
        }
        LinearLayout.LayoutParams lt = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lt.topMargin = ini.dp(16);
        col.addView(tarjetas, lt);
        FrameLayout centro = new FrameLayout(ini);
        centro.addView(col, new FrameLayout.LayoutParams(Math.min(ini.dp(1000), ini.getResources().getDisplayMetrics().widthPixels - ini.dp(64)),
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER_HORIZONTAL));
        sv.addView(centro);
        raiz.addView(sv, Inicio.lleno());
        pintar();
        return raiz;
    }

    View chip(String grande, String chico) {
        LinearLayout c = new LinearLayout(ini);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setPadding(ini.dp(16), ini.dp(10), ini.dp(16), ini.dp(10));
        c.setBackground(ini.fondo(0xFF17171B, 0x26FFFFFF, 16));
        TextView a = new TextView(ini);
        ini.estilo(a, 12, false, Inicio.TEXTO2);
        a.setText(chico);
        TextView b = new TextView(ini);
        ini.estilo(b, 16, true, Color.WHITE);
        b.setText(grande);
        c.addView(a);
        c.addView(b);
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        l.rightMargin = ini.dp(12);
        c.setLayoutParams(l);
        return c;
    }

    /** La página se ve: si las noticias tienen más de 20 min, se piden otra vez (en otro hilo). */
    void visible() {
        if (System.currentTimeMillis() - cargadas < 20 * 60_000 || cargando) return;
        cargando = true;
        File cache = new File(ini.getFilesDir(), "discover.xml");
        new Thread(() -> {
            List<Noticia> l = null;
            if (noticias.isEmpty() && cache.exists()) try (InputStream in = new FileInputStream(cache)) { l = leer(in); } catch (Exception ignored) { }
            if (l != null) { List<Noticia> f = l; Vidrio.principal.post(() -> { noticias.clear(); noticias.addAll(f); pintar(); }); }
            String[] r = region();
            try {
                HttpURLConnection c = (HttpURLConnection) new URL("https://news.google.com/rss?hl=" + r[0] + "&gl=" + r[1] + "&ceid=" + r[1] + ":" + r[0]).openConnection();
                c.setConnectTimeout(6000);
                c.setReadTimeout(8000);
                byte[] datos;
                try (InputStream in = c.getInputStream()) { datos = in.readAllBytes(); } finally { c.disconnect(); }
                List<Noticia> nuevas = leer(new java.io.ByteArrayInputStream(datos));
                try (FileOutputStream o = new FileOutputStream(cache)) { o.write(datos); }
                Vidrio.principal.post(() -> { noticias.clear(); noticias.addAll(nuevas); cargadas = System.currentTimeMillis(); cargando = false; pintar(); });
            } catch (Exception e) {
                Log.w(TAG, "noticias: " + e);
                Vidrio.principal.post(() -> { cargando = false; if (noticias.isEmpty() && aviso != null) aviso.setText("Sin conexión: no se pudieron cargar las noticias."); });
            }
        }).start();
    }

    /** {hl, gl}: español de América (es-419) salvo España; sin país, México. */
    static String[] region() {
        Locale l = Locale.getDefault();
        String idioma = l.getLanguage().isEmpty() ? "es" : l.getLanguage(), pais = l.getCountry();
        if (pais.isEmpty()) pais = idioma.equals("es") ? "MX" : "US";
        String hl = idioma.equals("es") && !pais.equals("ES") ? "es-419" : idioma.equals("es") ? "es" : idioma + "-" + pais;
        return new String[]{hl, pais};
    }

    static List<Noticia> leer(InputStream in) throws Exception {
        List<Noticia> l = new ArrayList<>();
        XmlPullParser p = Xml.newPullParser();
        p.setInput(in, "UTF-8");
        SimpleDateFormat f = new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss zzz", Locale.US);
        Noticia n = null;
        for (int e = p.getEventType(); e != XmlPullParser.END_DOCUMENT && l.size() < 40; e = p.next()) {
            if (e == XmlPullParser.START_TAG) {
                String t = p.getName();
                if (t.equals("item")) n = new Noticia();
                else if (n != null && t.equals("title")) n.titulo = p.nextText();
                else if (n != null && t.equals("link")) n.enlace = p.nextText();
                else if (n != null && t.equals("source")) n.fuente = p.nextText();
                else if (n != null && t.equals("pubDate")) try { n.fecha = f.parse(p.nextText()).getTime(); } catch (Exception ignored) { }
            } else if (e == XmlPullParser.END_TAG && p.getName().equals("item") && n != null) {
                if (n.titulo != null && n.enlace != null) {
                    if (n.fuente != null && n.titulo.endsWith(" - " + n.fuente)) n.titulo = n.titulo.substring(0, n.titulo.length() - n.fuente.length() - 3);
                    l.add(n);
                }
                n = null;
            }
        }
        return l;
    }

    // shortcut: el RSS de Google Noticias no trae imágenes (los enlaces son redirecciones de Google); tarjetas de texto.
    // Mejora: instalar el launcher como app de sistema (priv-app en la imagen) para que Google acepte su Discover real.
    void pintar() {
        if (columnas == null) return;
        for (LinearLayout c : columnas) c.removeAllViews();
        if (aviso != null) aviso.setText(noticias.isEmpty() ? "Cargando noticias…" : "");
        int[] alto = new int[2];
        long ahora = System.currentTimeMillis();
        for (Noticia n : noticias) {
            int k = alto[0] <= alto[1] ? 0 : 1;
            LinearLayout t = new LinearLayout(ini);
            t.setOrientation(LinearLayout.VERTICAL);
            t.setPadding(ini.dp(20), ini.dp(18), ini.dp(20), ini.dp(16));
            t.setBackground(ini.fondo(0xFF17171B, 0x1AFFFFFF, 24));
            TextView tit = new TextView(ini);
            ini.estilo(tit, n.titulo.length() > 90 ? 17 : 20, true, Color.WHITE);
            tit.setText(n.titulo);
            tit.setMaxLines(5);
            tit.setEllipsize(TextUtils.TruncateAt.END);
            tit.setLineSpacing(0, 1.1f);
            t.addView(tit);
            LinearLayout pie = new LinearLayout(ini);
            pie.setGravity(Gravity.CENTER_VERTICAL);
            pie.setPadding(0, ini.dp(14), 0, 0);
            String fuente = n.fuente == null || n.fuente.trim().isEmpty() ? "Google Noticias" : n.fuente.trim();
            TextView letra = new TextView(ini);
            ini.estilo(letra, 11, true, Color.WHITE);
            letra.setText(fuente.substring(0, 1).toUpperCase(Locale.ROOT));
            letra.setGravity(Gravity.CENTER);
            GradientDrawable gd = new GradientDrawable();
            gd.setShape(GradientDrawable.OVAL);
            gd.setColor(Color.HSVToColor(new float[]{(fuente.hashCode() & 0x7FFFFFFF) % 360, .55f, .75f}));
            letra.setBackground(gd);
            pie.addView(letra, new LinearLayout.LayoutParams(ini.dp(20), ini.dp(20)));
            TextView src = new TextView(ini);
            ini.estilo(src, 13, false, Inicio.TEXTO2);
            src.setSingleLine(true);
            src.setEllipsize(TextUtils.TruncateAt.END);
            CharSequence hace = n.fecha > 0 ? DateUtils.getRelativeTimeSpanString(n.fecha, ahora, DateUtils.MINUTE_IN_MILLIS, DateUtils.FORMAT_ABBREV_RELATIVE) : "";
            src.setText(fuente + (hace.length() > 0 ? " · " + hace : ""));
            src.setPadding(ini.dp(8), 0, ini.dp(8), 0);
            pie.addView(src, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            TextView compartir = new TextView(ini);
            ini.estilo(compartir, 13, true, Inicio.ACENTO);
            compartir.setText("Compartir");
            compartir.setPadding(ini.dp(8), ini.dp(6), ini.dp(4), ini.dp(6));
            compartir.setOnClickListener(v -> abrirIntent(Intent.createChooser(new Intent(Intent.ACTION_SEND).setType("text/plain")
                    .putExtra(Intent.EXTRA_TEXT, n.titulo + "\n" + n.enlace), null)));
            pie.addView(compartir);
            t.addView(pie);
            t.setForeground(ini.seleccionable());
            t.setOnClickListener(v -> abrirIntent(new Intent(Intent.ACTION_VIEW, Uri.parse(n.enlace))));
            LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            l.bottomMargin = ini.dp(16);
            columnas[k].addView(t, l);
            alto[k] += 60 + n.titulo.length();   // estimación: el título manda en la altura
        }
    }

    void buscarEnGoogle() {
        try { ini.startActivity(new Intent(Intent.ACTION_WEB_SEARCH).setPackage(GSA).putExtra(android.app.SearchManager.QUERY, "").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
        catch (ActivityNotFoundException e) { abrirIntent(new Intent(Intent.ACTION_VIEW, Uri.parse("https://www.google.com"))); }
    }

    void abrirIntent(Intent i) {
        try { ini.startActivity(i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
        catch (ActivityNotFoundException e) { android.widget.Toast.makeText(ini, "No hay app para abrirlo", android.widget.Toast.LENGTH_SHORT).show(); }
    }
}
