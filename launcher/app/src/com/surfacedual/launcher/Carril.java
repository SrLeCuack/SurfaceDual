package com.surfacedual.launcher;

import android.animation.ValueAnimator;
import android.widget.FrameLayout;

/**
 * Páginas del inicio una al lado de otra; se mueven con scrollX (solo desplaza las listas de dibujo ya grabadas: barato).
 * A la izquierda de la primera puede ir Discover: si el de Google responde, lo pinta Google por encima (no es una página).
 */
class Carril extends FrameLayout {
    final Inicio ini;
    int pagina;
    float origen;   // scrollX al empezar el gesto
    boolean google;      // este gesto lo lleva el Discover de Google
    float hecho;         // fracción del ancho recorrida hacia Discover (gesto de Google)
    ValueAnimator anim;
    java.util.function.IntConsumer alCambiar;

    Carril(Inicio i) { super(i); ini = i; }

    int base;   // páginas antes de la primera del inicio (la de Discover propia): se fija al armar el carril
    int base() { return base; }

    @Override protected void onLayout(boolean cambio, int l, int t, int r, int b) {
        int w = r - l;
        for (int i = 0; i < getChildCount(); i++) getChildAt(i).layout(i * w, 0, (i + 1) * w, b - t);
        if (cambio && (anim == null || !anim.isRunning())) scrollTo(pagina * w, 0);
    }

    boolean puede(float dx) {
        if (dx < 0) return pagina < getChildCount() - 1;
        return pagina > 0 || (pagina == 0 && ini.discover.googleListo());
    }

    void empezar(float dx) {
        if (anim != null) anim.cancel();
        origen = getScrollX();
        google = dx > 0 && pagina == 0 && ini.discover.googleListo() && base() == 0;   // solo hacia la derecha (hacia la izquierda: página 1)
        if (google) ini.discover.empezar();
    }

    void mover(float dx) {
        int w = Math.max(1, getWidth());
        if (google) { hecho = Math.max(0f, Math.min(1f, dx / w)); ini.discover.desplazar(hecho); return; }
        float x = origen - dx, max = (getChildCount() - 1) * w;
        if (x < 0) x = x / 3;          // resistencia en los extremos
        else if (x > max) x = max + (x - max) / 3;
        scrollTo(Math.round(x), 0);
    }

    // Como Samsung/Pixel: cambia de página con un gesto corto. Un tirón (> 1000 dp/s) decide por su sentido; si no, basta
    // pasar del 25 % del ancho (antes hacía falta más de la mitad, y para Discover casi toda la pantalla).
    void soltar(float vx) {
        float v = ini.dp(1000);
        if (google) {
            google = false;
            boolean abrir = vx > v || hecho > .25f && vx > -v;
            ini.discover.terminar();
            if (abrir) ini.discover.abrir(); else ini.discover.cerrarSuperposicion();   // la decisión es nuestra, no de Google
            return;
        }
        float rel = getScrollX() / (float) Math.max(1, getWidth()) - pagina;
        ir(vx < -v ? pagina + 1 : vx > v ? pagina - 1 : rel > .25f ? pagina + 1 : rel < -.25f ? pagina - 1 : pagina, true);
    }

    void ir(int i, boolean animar) {
        i = Math.max(0, Math.min(getChildCount() - 1, i));
        if (anim != null) anim.cancel();
        int destino = i * getWidth(), desde = getScrollX();
        boolean cambia = i != pagina;
        pagina = i;
        if (!animar || desde == destino || getWidth() == 0) scrollTo(destino, 0);
        else {
            anim = ValueAnimator.ofInt(desde, destino);
            anim.setDuration(Math.max(140, Math.min(260, Math.abs(destino - desde) * 260L / Math.max(1, getWidth()))));
            anim.setInterpolator(Inicio.DESACELERA);
            anim.addUpdateListener(a -> scrollTo((int) a.getAnimatedValue(), 0));
            anim.start();
        }
        if (alCambiar != null && (cambia || !animar)) alCambiar.accept(i);
    }
}
