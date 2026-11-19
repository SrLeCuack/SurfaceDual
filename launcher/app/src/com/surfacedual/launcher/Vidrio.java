package com.surfacedual.launcher;

import android.app.WallpaperColors;
import android.app.WallpaperManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Fondo de pantalla difuminado barato (como Mica de Windows 11, que también toma el fondo y no lo que hay detrás):
 * se reduce UNA vez a ~1/12, se difumina en la CPU (3 pasadas de caja sobre ~240x160 px) y se guarda en files/vidrio.png.
 * Dibujarlo es estirar ese mapa de bits con filtro bilineal: sin RenderEffect (0,6–1,2 s de GPU por cuadro en esta VM, medido
 * 17/11) ni el desenfoque de SurfaceFlinger (aquí pinta negro).
 */
final class Vidrio {
    static final String TAG = "vidrio";
    static Bitmap chico;     // difuminado (paneles, tarjetas, cajón)
    static Bitmap nitido;    // sin difuminar, ~640 px de ancho (vistas previas de "Fondo de pantalla y estilo")
    static int version;
    static final List<Runnable> avisar = new ArrayList<>();
    static final ExecutorService hilo = Executors.newSingleThreadExecutor();
    static final Handler principal = new Handler(Looper.getMainLooper());

    /** Carga lo guardado (al instante) y, si hace falta, recalcula desde el fondo del sistema en otro hilo. */
    static void preparar(Context c, boolean recalcular) {
        Context app = c.getApplicationContext();
        if (chico == null) {
            Bitmap b = BitmapFactory.decodeFile(new File(app.getFilesDir(), "vidrio.png").getPath());
            Bitmap n = BitmapFactory.decodeFile(new File(app.getFilesDir(), "nitido.jpg").getPath());
            if (b != null) { chico = b; nitido = n; version++; }
            else recalcular = true;
        }
        if (recalcular) hilo.execute(() -> {
            try { calcular(app, null); } catch (RuntimeException e) { Log.w(TAG, "difuminar", e); }
        });
    }

    /** original: la imagen recién puesta como fondo (no hace falta permiso para leerla); null = leer la del sistema. */
    static void calcular(Context c, Bitmap dada) {
        long t0 = System.currentTimeMillis();
        int[] p = pantalla(c);
        Bitmap original = dada != null ? dada : leerFondo(c, p);
        Bitmap n, b;
        if (original != null) {
            n = recortar(original, 640, p);
            b = recortar(original, 240, p);
            difuminar(b, 6);
            difuminar(b, 6);
            difuminar(b, 6);
        } else {   // sin permiso para leer el fondo: degradado con sus colores (WallpaperColors no pide permiso)
            n = null;
            b = desdeColores(c, p);
        }
        try (FileOutputStream o = new FileOutputStream(new File(c.getFilesDir(), "vidrio.png"))) { b.compress(Bitmap.CompressFormat.PNG, 100, o); }
        catch (Exception e) { Log.w(TAG, "guardar", e); }
        if (n != null) try (FileOutputStream o = new FileOutputStream(new File(c.getFilesDir(), "nitido.jpg"))) { n.compress(Bitmap.CompressFormat.JPEG, 88, o); }
        catch (Exception e) { Log.w(TAG, "guardar", e); }
        Log.i(TAG, "fondo difuminado en " + (System.currentTimeMillis() - t0) + " ms (" + (original != null ? "imagen" : "colores") + ")");
        principal.post(() -> {
            chico = b; nitido = n; version++;
            for (Runnable r : new ArrayList<>(avisar)) r.run();
        });
    }

    /** El fondo del sistema reducido al leerlo (inSampleSize): nunca se decodifica la imagen entera. */
    static Bitmap leerFondo(Context c, int[] p) {
        WallpaperManager wm = WallpaperManager.getInstance(c);
        try (ParcelFileDescriptor f = wm.getWallpaperFile(WallpaperManager.FLAG_SYSTEM)) {
            if (f != null) {
                BitmapFactory.Options o = new BitmapFactory.Options();
                o.inJustDecodeBounds = true;
                BitmapFactory.decodeFileDescriptor(f.getFileDescriptor(), null, o);
                o.inSampleSize = Math.max(1, Math.min(o.outWidth / 1280, o.outHeight / 860));
                o.inJustDecodeBounds = false;
                android.system.Os.lseek(f.getFileDescriptor(), 0, android.system.OsConstants.SEEK_SET);
                Bitmap b = BitmapFactory.decodeFileDescriptor(f.getFileDescriptor(), null, o);
                if (b != null) return b;
            }
        } catch (Exception e) { Log.i(TAG, "sin acceso al archivo del fondo: " + e); }
        try {   // fondo de fábrica (sin archivo) o sin permiso de archivo
            Drawable d = wm.getDrawable();
            if (d == null) d = wm.getBuiltInDrawable();
            if (d instanceof BitmapDrawable) return ((BitmapDrawable) d).getBitmap();
        } catch (Exception e) { Log.i(TAG, "sin acceso al fondo: " + e); }
        return null;
    }

    /** Recorte centrado con la forma de la pantalla, a "ancho" px. */
    static Bitmap recortar(Bitmap src, int ancho, int[] p) {
        int alto = Math.max(1, ancho * p[1] / Math.max(1, p[0]));
        Bitmap b = Bitmap.createBitmap(ancho, alto, Bitmap.Config.ARGB_8888);
        float esc = Math.max(ancho / (float) src.getWidth(), alto / (float) src.getHeight());
        float w = src.getWidth() * esc, h = src.getHeight() * esc;
        new Canvas(b).drawBitmap(src, null, new RectF((ancho - w) / 2, (alto - h) / 2, (ancho + w) / 2, (alto + h) / 2), new Paint(Paint.FILTER_BITMAP_FLAG));
        return b;
    }

    static Bitmap desdeColores(Context c, int[] p) {
        int cw = 240, ch = Math.max(1, 240 * p[1] / Math.max(1, p[0]));
        int a = 0xFF3769C8, b2 = 0xFF4A35A8, t = 0xFF1E9BC8;
        try {
            WallpaperColors w = WallpaperManager.getInstance(c).getWallpaperColors(WallpaperManager.FLAG_SYSTEM);
            if (w != null) {
                a = w.getPrimaryColor().toArgb();
                if (w.getSecondaryColor() != null) b2 = w.getSecondaryColor().toArgb();
                if (w.getTertiaryColor() != null) t = w.getTertiaryColor().toArgb();
            }
        } catch (RuntimeException ignored) { }
        Bitmap bm = Bitmap.createBitmap(cw, ch, Bitmap.Config.ARGB_8888);
        Canvas cv = new Canvas(bm);
        cv.drawColor(0xFF06070C);
        Paint pt = new Paint(Paint.DITHER_FLAG);
        pt.setShader(new RadialGradient(cw * .62f, ch * 1.05f, cw * .7f, (a & 0xFFFFFF) | 0xB0000000, 0, Shader.TileMode.CLAMP));
        cv.drawPaint(pt);
        pt.setShader(new RadialGradient(cw * .16f, ch * .2f, cw * .45f, (b2 & 0xFFFFFF) | 0x8A000000, 0, Shader.TileMode.CLAMP));
        cv.drawPaint(pt);
        pt.setShader(new RadialGradient(cw * .9f, ch * .15f, cw * .36f, (t & 0xFFFFFF) | 0x5C000000, 0, Shader.TileMode.CLAMP));
        cv.drawPaint(pt);
        return bm;
    }

    /** Difuminado de caja separable (horizontal y vertical), en el sitio. */
    static void difuminar(Bitmap b, int r) {
        int w = b.getWidth(), h = b.getHeight();
        int[] px = new int[w * h], tmp = new int[w * h];
        b.getPixels(px, 0, w, 0, 0, w, h);
        caja(px, tmp, w, h, r, 1, w);
        caja(tmp, px, h, w, r, w, 1);
        b.setPixels(px, 0, w, 0, 0, w, h);
    }

    // Una pasada: "n" líneas de "largo" píxeles; paso entre píxeles "dp" y entre líneas "dl".
    static void caja(int[] in, int[] out, int largo, int n, int r, int dp, int dl) {
        int div = 2 * r + 1;
        for (int l = 0; l < n; l++) {
            int base = l * dl, sa = 0, sr = 0, sg = 0, sb = 0;
            for (int i = -r; i <= r; i++) {
                int c = in[base + Math.max(0, Math.min(largo - 1, i)) * dp];
                sa += c >>> 24; sr += (c >> 16) & 255; sg += (c >> 8) & 255; sb += c & 255;
            }
            for (int i = 0; i < largo; i++) {
                out[base + i * dp] = (sa / div) << 24 | (sr / div) << 16 | (sg / div) << 8 | (sb / div);
                int c1 = in[base + Math.min(largo - 1, i + r + 1) * dp], c0 = in[base + Math.max(0, i - r) * dp];
                sa += (c1 >>> 24) - (c0 >>> 24); sr += ((c1 >> 16) & 255) - ((c0 >> 16) & 255);
                sg += ((c1 >> 8) & 255) - ((c0 >> 8) & 255); sb += (c1 & 255) - (c0 & 255);
            }
        }
    }

    static int[] pantalla(Context c) {
        Rect r = c.getSystemService(WindowManager.class).getMaximumWindowMetrics().getBounds();
        return new int[]{Math.max(1, r.width()), Math.max(1, r.height())};
    }

    /** Pantalla completa: el fondo difuminado y oscurecido (Recientes, cajón, selector de widgets, ajustes). */
    static class Fondo extends View {
        final Paint filtro = new Paint(Paint.FILTER_BITMAP_FLAG);
        final Rect destino = new Rect();
        final int velo;
        // El velo va como filtro de color del mismo dibujo: una pasada a pantalla completa en vez de dos (17/11, la GPU
        // virtual va justa de relleno: 6 ms de mediana al abrir el cajón).
        Fondo(Context c, int velo) { super(c); this.velo = velo; filtro.setColorFilter(new android.graphics.PorterDuffColorFilter(velo, android.graphics.PorterDuff.Mode.SRC_ATOP)); }
        @Override public boolean hasOverlappingRendering() { return false; }   // fundido sin capa de 2880x1920 (medido: era el tirón)
        @Override protected void onSizeChanged(int w, int h, int ow, int oh) { destino.set(0, 0, w, h); }
        @Override protected void onDraw(Canvas c) {
            if (chico != null) c.drawBitmap(chico, null, destino, filtro);
            else { c.drawColor(0xFF06070C); c.drawColor(velo); }
        }
    }

    /**
     * Placa de vidrio: la parte del fondo difuminado que queda detrás de la vista (en coordenadas de pantalla), un tinte y
     * un borde fino. Un sombreador con matriz: sin recortes ni capas al dibujar.
     */
    static class Placa extends Drawable {
        final View duena;
        final Paint img = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG), tinte = new Paint(Paint.ANTI_ALIAS_FLAG),
                borde = new Paint(Paint.ANTI_ALIAS_FLAG);
        final float radio;
        final Matrix m = new Matrix();
        final int[] pos = new int[2];
        final RectF r = new RectF();
        int versionVista = -1, px = Integer.MIN_VALUE, py;
        float w0, h0;

        Placa(View duena, float radioPx, int colorTinte) {
            this.duena = duena;
            radio = radioPx;
            tinte.setColor(colorTinte);
            img.setColorFilter(new android.graphics.PorterDuffColorFilter(colorTinte, android.graphics.PorterDuff.Mode.SRC_ATOP));   // tinte en la misma pasada
            borde.setStyle(Paint.Style.STROKE);
            borde.setStrokeWidth(Math.max(1f, duena.getResources().getDisplayMetrics().density));
            borde.setColor(0x24FFFFFF);
        }

        @Override public void draw(Canvas c) {
            r.set(getBounds());
            if (chico != null) {
                duena.getLocationOnScreen(pos);
                if (versionVista != version) {
                    img.setShader(new BitmapShader(chico, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP));
                    int[] p = pantalla(duena.getContext());
                    w0 = p[0] / (float) chico.getWidth(); h0 = p[1] / (float) chico.getHeight();
                    versionVista = version; px = Integer.MIN_VALUE;
                }
                if (pos[0] != px || pos[1] != py) {
                    px = pos[0]; py = pos[1];
                    m.setScale(w0, h0);
                    m.postTranslate(-px, -py);
                    img.getShader().setLocalMatrix(m);
                }
                c.drawRoundRect(r, radio, radio, img);
            } else c.drawRoundRect(r, radio, radio, tinte);
            r.inset(.5f, .5f);
            c.drawRoundRect(r, radio, radio, borde);
        }

        @Override public void getOutline(android.graphics.Outline o) { o.setRoundRect(getBounds(), radio); }
        @Override public void setAlpha(int a) { }
        @Override public void setColorFilter(ColorFilter f) { }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    static boolean oscuro(Context c) {
        return (c.getResources().getConfiguration().uiMode & android.content.res.Configuration.UI_MODE_NIGHT_MASK) == android.content.res.Configuration.UI_MODE_NIGHT_YES;
    }

    static { assert Color.alpha(0x80FFFFFF) == 0x80; }
}
