package com.surfacedual.launcher;

import android.app.Activity;
import android.app.UiModeManager;
import android.app.WallpaperColors;
import android.app.WallpaperManager;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.GridLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextClock;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Ajustes del inicio (fotos F y G) y "Fondo de pantalla y estilo" (foto H), como hojas dentro del inicio: abren al instante. */
final class Hojas {
    static final int PEDIR_IMAGEN = 52;
    static int destino = WallpaperManager.FLAG_SYSTEM | WallpaperManager.FLAG_LOCK;   // dónde se pone el fondo elegido

    // ---------- ajustes del inicio ----------

    static void ajustes(Inicio ini) {
        LinearLayout c = columna(ini);
        boolean vertical = ini.getResources().getConfiguration().orientation == android.content.res.Configuration.ORIENTATION_PORTRAIT;

        LinearLayout g1 = grupo(ini, c);
        TextView tam = titulo(ini, "Tamaño de los iconos en el inicio y el cajón");
        tam.setPadding(ini.dp(20), ini.dp(16), ini.dp(20), 0);
        g1.addView(tam);
        g1.addView(new Paneles.Deslizador(ini, ini.ico("ic_apps"), (ini.icono() - 48) / 32f, v -> {
            ini.prefs.edit().putInt("icono", 48 + Math.round(v * 32)).apply();
            ini.rehacerAlCerrar = true;
        }), new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ini.dp(56)));
        g1.addView(interruptorFila(ini, "Mostrar los nombres de las apps en el inicio", null, "etiquetas", true, () -> ini.notificarRejillas()));

        LinearLayout g2 = grupo(ini, c);
        g2.addView(cicloFila(ini, "Cuadrícula de la pantalla de inicio", "columnas", vertical ? 4 : 6, 3, 10));
        g2.addView(cicloFila(ini, "Cuadrícula del cajón de apps", "columnas_cajon", vertical ? 5 : 8, 4, 12));

        LinearLayout g3 = grupo(ini, c);
        g3.addView(interruptorFila(ini, "Añadir página de medios al inicio", "Google Discover", "discover", true, () -> ini.rehacerAlCerrar = true));
        g3.addView(interruptorFila(ini, "Usar el Discover de Google", "Necesita tu cuenta de Google en Android; si no, noticias de Google Noticias",
                "discover_google", true, () -> {
            ini.rehacerAlCerrar = true;
            if (ini.discover.usarGoogle()) ini.discover.alIniciar(); else ini.discover.cerrar();
        }));
        g3.addView(interruptorFila(ini, "Mostrar apps recientes en el dock", "Hasta 3 apps", "dock_recientes", false, () -> ini.rehacerAlCerrar = true));
        g3.addView(interruptorFila(ini, "Bloquear el diseño del inicio", "Evita que se quiten o muevan iconos, widgets y páginas.", "bloqueo", false, null));
        g3.addView(interruptorFila(ini, "Puntos de notificación en los iconos", null, "puntos", true, () -> ini.notificarRejillas()));
        g3.addView(interruptorFila(ini, "Deslizar hacia abajo para las notificaciones", "En cualquier parte vacía del inicio", "deslizar_noti", true, null));
        g3.addView(interruptorFila(ini, "Tocar dos veces para apagar la pantalla", null, "doble_toque", true, null));
        g3.addView(interruptorFila(ini, "Paneles Edge", "Asa en el borde derecho con tus apps favoritas (mantenla para moverla)", "edge", true,
                () -> Borde.de(ini).actualizar()));

        LinearLayout g4 = grupo(ini, c);
        UiModeManager um = ini.getSystemService(UiModeManager.class);
        boolean oscuro = um != null && um.getNightMode() != UiModeManager.MODE_NIGHT_NO;
        View[] interruptor = new View[1];
        LinearLayout tema = fila(ini, "Tema oscuro", "Android y sus apps (Ajustes incluido)", null);
        interruptor[0] = interruptor(ini, oscuro);
        tema.addView(interruptor[0], new LinearLayout.LayoutParams(ini.dp(44), ini.dp(26)));
        boolean[] estado = {oscuro};
        tema.setOnClickListener(v -> {
            boolean nuevo = !estado[0];
            Inicio.peticionAgente(ini, "tema " + (nuevo ? "oscuro" : "claro"), r -> {
                estado[0] = nuevo;
                ponerInterruptor(ini, interruptor[0], nuevo);
            }, () -> Toast.makeText(ini, "Cambiar el tema necesita el agente del sistema", Toast.LENGTH_SHORT).show());
        });
        g4.addView(tema);
        g4.addView(fila(ini, "Fondo de pantalla y estilo", null, v -> fondo(ini)));

        TextView ver = new TextView(ini);
        ini.estilo(ver, 13, false, Inicio.TEXTO3);
        ver.setText("Surface Dual Launcher · gestos: deslizar de lado = páginas · hacia arriba = cajón · pellizcar = editar");
        ver.setPadding(ini.dp(20), ini.dp(16), ini.dp(20), 0);
        c.addView(ver);
        ini.abrirHoja("Ajustes de la pantalla de inicio", c, true);
    }

    static LinearLayout columna(Inicio ini) {
        LinearLayout c = new LinearLayout(ini);
        c.setOrientation(LinearLayout.VERTICAL);
        return c;
    }

    static LinearLayout grupo(Inicio ini, LinearLayout c) {
        LinearLayout g = new LinearLayout(ini);
        g.setOrientation(LinearLayout.VERTICAL);
        g.setBackground(ini.fondo(0x14FFFFFF, 0x14FFFFFF, 24));
        g.setPadding(0, ini.dp(4), 0, ini.dp(4));
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        l.topMargin = ini.dp(16);
        c.addView(g, l);
        return g;
    }

    static TextView titulo(Inicio ini, String s) {
        TextView t = new TextView(ini);
        ini.estilo(t, 16, false, Color.WHITE);
        t.setText(s);
        return t;
    }

    static LinearLayout fila(Inicio ini, String titulo, String sub, View.OnClickListener al) {
        LinearLayout f = new LinearLayout(ini);
        f.setGravity(Gravity.CENTER_VERTICAL);
        f.setPadding(ini.dp(20), ini.dp(12), ini.dp(20), ini.dp(12));
        f.setMinimumHeight(ini.dp(60));
        f.setBackground(ini.seleccionable());
        LinearLayout textos = new LinearLayout(ini);
        textos.setOrientation(LinearLayout.VERTICAL);
        textos.addView(titulo(ini, titulo));
        if (sub != null) {
            TextView s = new TextView(ini);
            ini.estilo(s, 13, false, Inicio.ACENTO);
            s.setText(sub);
            s.setTag("sub");
            textos.addView(s);
        }
        f.addView(textos, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        if (al != null) f.setOnClickListener(al);
        return f;
    }

    static View interruptorFila(Inicio ini, String titulo, String sub, String pref, boolean omision, Runnable despues) {
        LinearLayout f = fila(ini, titulo, sub, null);
        boolean on = ini.prefs.getBoolean(pref, omision);
        View i = interruptor(ini, on);
        f.addView(i, new LinearLayout.LayoutParams(ini.dp(44), ini.dp(26)));
        f.setOnClickListener(v -> {
            boolean nuevo = !ini.prefs.getBoolean(pref, omision);
            ini.prefs.edit().putBoolean(pref, nuevo).apply();
            ponerInterruptor(ini, i, nuevo);
            if (despues != null) despues.run();
        });
        return f;
    }

    /** Fila que recorre valores (columnas de la cuadrícula) a cada toque. */
    static View cicloFila(Inicio ini, String titulo, String pref, int omision, int min, int max) {
        LinearLayout f = fila(ini, titulo, ini.prefs.getInt(pref, omision) + " columnas", null);
        f.setOnClickListener(v -> {
            int n = ini.prefs.getInt(pref, omision) + 1;
            if (n > max) n = min;
            ini.prefs.edit().putInt(pref, n).apply();
            ((TextView) f.findViewWithTag("sub")).setText(n + " columnas");
            ini.rehacerAlCerrar = true;
        });
        return f;
    }

    /** Interruptor de Windows 11: pista redonda (acento si está activado) y pulgar. */
    static View interruptor(Inicio ini, boolean on) {
        FrameLayout pista = new FrameLayout(ini);
        View pulgar = new View(ini);
        pista.addView(pulgar, new FrameLayout.LayoutParams(ini.dp(14), ini.dp(14), Gravity.CENTER_VERTICAL | Gravity.START));
        ponerInterruptor(ini, pista, on);
        return pista;
    }

    static void ponerInterruptor(Inicio ini, View v, boolean on) {
        FrameLayout pista = (FrameLayout) v;
        GradientDrawable g = new GradientDrawable();
        g.setCornerRadius(ini.dp(13));
        g.setColor(on ? Inicio.ACENTO : 0x14FFFFFF);
        g.setStroke(ini.dp(1), on ? Inicio.ACENTO : 0x99FFFFFF);
        pista.setBackground(g);
        View pulgar = pista.getChildAt(0);
        GradientDrawable p = new GradientDrawable();
        p.setShape(GradientDrawable.OVAL);
        p.setColor(on ? 0xFF003049 : 0xCCFFFFFF);
        pulgar.setBackground(p);
        FrameLayout.LayoutParams l = (FrameLayout.LayoutParams) pulgar.getLayoutParams();
        l.gravity = Gravity.CENTER_VERTICAL | (on ? Gravity.END : Gravity.START);
        l.leftMargin = l.rightMargin = ini.dp(6);
        pulgar.setLayoutParams(l);
    }

    // ---------- fondo de pantalla y estilo (foto H) ----------

    static void fondo(Inicio ini) {
        LinearLayout c = columna(ini);
        LinearLayout previas = new LinearLayout(ini);
        int ancho = Math.min(ini.dp(760), ini.getResources().getDisplayMetrics().widthPixels - ini.dp(64));
        int w = (ancho - ini.dp(20)) / 2, h = w * Vidrio.pantalla(ini)[1] / Vidrio.pantalla(ini)[0];
        previas.addView(previa(ini, true), new LinearLayout.LayoutParams(w, h));
        LinearLayout.LayoutParams l2 = new LinearLayout.LayoutParams(w, h);
        l2.leftMargin = ini.dp(20);
        previas.addView(previa(ini, false), l2);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, h);
        lp.topMargin = ini.dp(12);
        c.addView(previas, lp);

        LinearLayout g1 = grupo(ini, c);
        g1.addView(fila(ini, "Cambiar fondos de pantalla", null, v -> galeria(ini)));
        LinearLayout g2 = grupo(ini, c);
        LinearLayout paleta = fila(ini, "Paleta de colores", "Colores tomados de tu fondo de pantalla", v -> {
            try { ini.startActivity(new Intent(Intent.ACTION_SET_WALLPAPER).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
            catch (ActivityNotFoundException e) { Toast.makeText(ini, "No hay app de estilo en el sistema", Toast.LENGTH_SHORT).show(); }
        });
        paleta.addView(muestras(ini));
        g2.addView(paleta);
        LinearLayout g3 = grupo(ini, c);
        g3.addView(interruptorFila(ini, "Atenuar el fondo de pantalla con el modo oscuro", null, "atenuar", true, ini::atenuar));
        ini.abrirHoja("Fondo de pantalla y estilo", c, true);
    }

    /** Vista previa del bloqueo (con la hora) o del inicio (con el dock), sobre la copia reducida del fondo. */
    static View previa(Inicio ini, boolean bloqueo) {
        FrameLayout f = new FrameLayout(ini);
        f.setBackground(ini.fondo(0xFF101014, 0x26FFFFFF, 20));
        f.setClipToOutline(true);
        ImageView img = new ImageView(ini);
        img.setScaleType(ImageView.ScaleType.CENTER_CROP);
        img.setImageBitmap(Vidrio.nitido != null ? Vidrio.nitido : Vidrio.chico);
        f.addView(img, Inicio.lleno());
        if (bloqueo) {
            TextClock hora = new TextClock(ini);
            hora.setFormat12Hour("h:mm");
            hora.setFormat24Hour("H:mm");
            ini.estilo(hora, 30, true, Color.WHITE);
            hora.setShadowLayer(ini.dp(6), 0, 0, 0x99000000);
            FrameLayout.LayoutParams l = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.CENTER_HORIZONTAL);
            l.topMargin = ini.dp(18);
            f.addView(hora, l);
        } else {
            LinearLayout dock = new LinearLayout(ini);
            dock.setGravity(Gravity.CENTER);
            dock.setPadding(ini.dp(8), ini.dp(4), ini.dp(8), ini.dp(4));
            dock.setBackground(ini.fondo(0x661A1A1E, 0, 10));
            for (String pkg : ini.dock()) {
                Inicio.App a = ini.buscar(pkg);
                if (a == null) continue;
                ImageView i = new ImageView(ini);
                i.setImageDrawable(a.icono);
                LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ini.dp(18), ini.dp(18));
                l.leftMargin = l.rightMargin = ini.dp(3);
                dock.addView(i, l);
            }
            FrameLayout.LayoutParams l = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
            l.bottomMargin = ini.dp(12);
            f.addView(dock, l);
        }
        f.setForeground(ini.seleccionable());
        f.setOnClickListener(v -> { destino = bloqueo ? WallpaperManager.FLAG_LOCK : WallpaperManager.FLAG_SYSTEM; galeria(ini); });
        return f;
    }

    static View muestras(Inicio ini) {
        LinearLayout m = new LinearLayout(ini);
        m.setOrientation(LinearLayout.VERTICAL);
        int[] colores = {0xFF60CDFF, 0xFF4A35A8, 0xFF1E9BC8};
        try {
            WallpaperColors w = WallpaperManager.getInstance(ini).getWallpaperColors(WallpaperManager.FLAG_SYSTEM);
            if (w != null) {
                colores[0] = w.getPrimaryColor().toArgb();
                if (w.getSecondaryColor() != null) colores[1] = w.getSecondaryColor().toArgb();
                if (w.getTertiaryColor() != null) colores[2] = w.getTertiaryColor().toArgb();
            }
        } catch (RuntimeException ignored) { }
        for (int k = 0; k < 3; k++) {
            View v = new View(ini);
            GradientDrawable g = new GradientDrawable();
            g.setColor(colores[k]);
            g.setCornerRadii(k == 0 ? new float[]{8, 8, 8, 8, 0, 0, 0, 0} : k == 2 ? new float[]{0, 0, 0, 0, 8, 8, 8, 8} : new float[8]);
            v.setBackground(g);
            m.addView(v, new LinearLayout.LayoutParams(ini.dp(26), ini.dp(12)));
        }
        return m;
    }

    /** Elegir fondo: imágenes de Imágenes/Descargas/DCIM (si el agente dio acceso), el selector de fotos o el del sistema. */
    static void galeria(Inicio ini) {
        LinearLayout c = columna(ini);
        LinearLayout destinos = new LinearLayout(ini);
        String[] nombres = {"Inicio y bloqueo", "Inicio", "Pantalla de bloqueo"};
        int[] valores = {WallpaperManager.FLAG_SYSTEM | WallpaperManager.FLAG_LOCK, WallpaperManager.FLAG_SYSTEM, WallpaperManager.FLAG_LOCK};
        List<TextView> pastillas = new ArrayList<>();
        for (int k = 0; k < 3; k++) {
            int val = valores[k];
            TextView p = new TextView(ini);
            ini.estilo(p, 14, true, Color.WHITE);
            p.setText(nombres[k]);
            p.setGravity(Gravity.CENTER);
            p.setPadding(ini.dp(18), ini.dp(10), ini.dp(18), ini.dp(10));
            p.setOnClickListener(v -> { destino = val; for (TextView x : pastillas) marcarPastilla(ini, x, x == v); });
            pastillas.add(p);
            marcarPastilla(ini, p, destino == val);
            LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            l.rightMargin = ini.dp(10);
            destinos.addView(p, l);
        }
        c.addView(destinos);
        LinearLayout g = grupo(ini, c);
        g.addView(fila(ini, "Elegir de Fotos", "Selector de fotos del sistema (sin permisos)", v -> {
            try { ini.startActivityForResult(new Intent(MediaStore.ACTION_PICK_IMAGES).setType("image/*"), PEDIR_IMAGEN); }
            catch (ActivityNotFoundException e) {
                try { ini.startActivityForResult(new Intent(Intent.ACTION_GET_CONTENT).setType("image/*"), PEDIR_IMAGEN); }
                catch (ActivityNotFoundException e2) { Toast.makeText(ini, "No hay selector de imágenes", Toast.LENGTH_SHORT).show(); }
            }
        }));
        g.addView(fila(ini, "Fondos del sistema", "Fondo y estilo de Android", v -> {
            try { ini.startActivity(Intent.createChooser(new Intent(Intent.ACTION_SET_WALLPAPER), "Fondo de pantalla").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)); }
            catch (ActivityNotFoundException e) { Toast.makeText(ini, "No hay selector de fondos en el sistema", Toast.LENGTH_SHORT).show(); }
        }));
        // shortcut: se listan en el hilo principal (pocas carpetas, sin subcarpetas); pasarlo a un hilo si hay miles de fotos.
        List<File> imagenes = new ArrayList<>();
        for (String d : new String[]{Environment.DIRECTORY_PICTURES, Environment.DIRECTORY_DOWNLOADS, Environment.DIRECTORY_DCIM}) {
            File[] l = Environment.getExternalStoragePublicDirectory(d).listFiles();
            if (l != null) for (File f : l) if (f.isFile() && f.getName().toLowerCase().matches(".*\\.(jpe?g|png|webp)$")) imagenes.add(f);
        }
        imagenes.sort((a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        if (!imagenes.isEmpty()) {
            TextView t = titulo(ini, "Tus imágenes");
            t.setPadding(ini.dp(4), ini.dp(24), 0, ini.dp(8));
            c.addView(t);
            GridLayout rejilla = new GridLayout(ini);
            rejilla.setColumnCount(4);
            int ancho = Math.min(ini.dp(760), ini.getResources().getDisplayMetrics().widthPixels - ini.dp(64));
            int lado = (ancho - ini.dp(12) * 3) / 4, alto = lado * 2 / 3;
            for (File f : imagenes.subList(0, Math.min(40, imagenes.size()))) {
                ImageView i = new ImageView(ini);
                i.setScaleType(ImageView.ScaleType.CENTER_CROP);
                i.setBackground(ini.fondo(0xFF1A1A1E, 0, 14));
                i.setClipToOutline(true);
                i.setForeground(ini.seleccionable());
                GridLayout.LayoutParams l = new GridLayout.LayoutParams();
                l.width = lado; l.height = alto;
                l.setMargins(0, 0, ini.dp(12), ini.dp(12));
                rejilla.addView(i, l);
                i.setOnClickListener(v -> aplicar(ini, () -> new FileInputStream(f)));
                Vidrio.hilo.execute(() -> {
                    BitmapFactory.Options o = new BitmapFactory.Options();
                    o.inJustDecodeBounds = true;
                    BitmapFactory.decodeFile(f.getPath(), o);
                    o.inSampleSize = Math.max(1, Math.min(o.outWidth / lado, o.outHeight / alto));
                    o.inJustDecodeBounds = false;
                    Bitmap b = BitmapFactory.decodeFile(f.getPath(), o);
                    if (b != null) Vidrio.principal.post(() -> i.setImageBitmap(b));
                });
            }
            c.addView(rejilla);
        }
        ini.abrirHoja("Cambiar fondos de pantalla", c, true);
    }

    static void marcarPastilla(Inicio ini, TextView p, boolean on) {
        p.setBackground(ini.fondo(on ? Inicio.ACENTO : 0x14FFFFFF, on ? Inicio.ACENTO : 0x33FFFFFF, 20));
        p.setTextColor(on ? 0xFF003049 : Color.WHITE);
    }

    interface Fuente { InputStream abrir() throws Exception; }

    static void imagenElegida(Inicio ini, int res, Intent datos) {
        Uri u = datos == null ? null : datos.getData();
        if (res == Activity.RESULT_OK && u != null) aplicar(ini, () -> ini.getContentResolver().openInputStream(u));
    }

    /** Pone la imagen como fondo (en otro hilo) y rehace el vidrio con ella misma, sin leer el fondo del sistema. */
    static void aplicar(Inicio ini, Fuente f) {
        Toast.makeText(ini, "Poniendo el fondo…", Toast.LENGTH_SHORT).show();
        int donde = destino;
        Vidrio.hilo.execute(() -> {
            try {
                try (InputStream in = f.abrir()) { WallpaperManager.getInstance(ini).setStream(in, null, true, donde); }
                if ((donde & WallpaperManager.FLAG_SYSTEM) != 0) {
                    BitmapFactory.Options o = new BitmapFactory.Options();
                    o.inSampleSize = 4;
                    Bitmap b;
                    try (InputStream in = f.abrir()) { b = BitmapFactory.decodeStream(in, null, o); }
                    if (b != null) Vidrio.calcular(ini.getApplicationContext(), b);
                }
                Vidrio.principal.post(() -> { ini.cerrarHoja(); Toast.makeText(ini, "Fondo de pantalla cambiado", Toast.LENGTH_SHORT).show(); });
            } catch (Exception e) {
                Vidrio.principal.post(() -> Toast.makeText(ini, "No se pudo poner ese fondo", Toast.LENGTH_SHORT).show());
            }
        });
    }

}
