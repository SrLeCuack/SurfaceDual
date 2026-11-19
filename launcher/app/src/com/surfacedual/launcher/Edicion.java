package com.surfacedual.launcher;

import android.graphics.Canvas;
import android.graphics.Color;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.List;

/**
 * Modo edición al pellizcar (fotos J, K, L): las páginas como tarjetas de vidrio sobre el fondo, Discover a la izquierda con su
 * interruptor, la casa para elegir la principal, "+" para otra página y abajo Fondo de pantalla y estilo / Widgets / Ajustes.
 * Las tarjetas dibujan la página real reducida (sus listas de dibujo ya grabadas: sin copias en mapas de bits).
 */
class Edicion {
    final Inicio ini;
    final FrameLayout capa;
    final HorizontalScrollView cinta;
    final LinearLayout tarjetas;
    final int cw, ch, hueco, w;
    LinearLayout pie;
    final float esc = .5f;
    boolean saliendo;

    Edicion(Inicio i) { this(i, -1); }

    /** centrar: índice de tarjeta a centrar al rehacer (sin animación de entrada); -1 = entrar desde la página actual. */
    Edicion(Inicio i, int centrar) {
        ini = i;
        int[] pt = Vidrio.pantalla(ini);   // el inicio puede no estar medido aún (al rehacer)
        w = pt[0];
        int h = pt[1];
        cw = Math.round(w * esc); ch = Math.round(h * esc); hueco = ini.dp(40);
        capa = new FrameLayout(ini);
        capa.setClickable(true);
        capa.setBackgroundColor(0x33000000);
        LinearLayout col = new LinearLayout(ini);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setGravity(Gravity.CENTER_HORIZONTAL);
        cinta = new HorizontalScrollView(ini) {
            @Override public boolean onTouchEvent(MotionEvent e) {
                boolean r = super.onTouchEvent(e);
                if (e.getActionMasked() == MotionEvent.ACTION_UP || e.getActionMasked() == MotionEvent.ACTION_CANCEL)
                    post(() -> smoothScrollTo(Math.round(getScrollX() / (float) (cw + hueco)) * (cw + hueco), 0));   // se queda en una tarjeta
                return r;
            }
        };
        cinta.setHorizontalScrollBarEnabled(false);
        cinta.setOverScrollMode(View.OVER_SCROLL_NEVER);
        tarjetas = new LinearLayout(ini);
        tarjetas.setPadding((w - cw) / 2, 0, (w - cw) / 2, 0);
        cinta.addView(tarjetas);
        LinearLayout.LayoutParams lc = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ch + ini.dp(56));
        lc.topMargin = ini.barraEstado() + ini.dp(24);
        col.addView(cinta, lc);
        armar();
        View sep = new View(ini);
        col.addView(sep, new LinearLayout.LayoutParams(1, 0, 1));
        pie = new LinearLayout(ini);
        pie.setGravity(Gravity.CENTER);
        pie.addView(botonPie("ic_menu_gallery", "Fondo de pantalla y estilo", v -> { salir(-1); Hojas.fondo(ini); }));
        pie.addView(botonPie("ic_widgets", "Widgets", v -> { int p = paginaCentrada(); salir(-1); ini.widgets.selector(Math.max(0, p)); }));
        pie.addView(botonPie("ic_settings_24dp", "Ajustes", v -> { salir(-1); Hojas.ajustes(ini); }));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = ini.dp(40);
        col.addView(pie, lp);
        capa.addView(col, Inicio.lleno());
        ini.raiz.addView(capa, Inicio.lleno());

        int actual = centrar >= 0 ? centrar : 1 + (ini.carril.pagina - ini.carril.base());   // la tarjeta 0 es Discover
        cinta.post(() -> cinta.scrollTo(actual * (cw + hueco), 0));
        ini.carril.setVisibility(View.INVISIBLE);   // la tarjeta de la página la sustituye: solo hay transformaciones, sin fundidos
        if (centrar < 0) {
            // Zum hacia atrás: las tarjetas empiezan al doble (la página actual ocupa la pantalla) y se reducen a su sitio.
            tarjetas.setScaleX(1 / esc); tarjetas.setScaleY(1 / esc);
            tarjetas.setPivotY(ini.dp(56) + ch / 2f);
            pie.setTranslationY(ini.dp(120));
            cinta.post(() -> {
                tarjetas.setPivotX(w / 2f + cinta.getScrollX());
                tarjetas.animate().scaleX(1f).scaleY(1f).setDuration(220).setInterpolator(Inicio.DESACELERA);
                pie.animate().translationY(0).setDuration(220).setInterpolator(Inicio.DESACELERA);
            });
        }
        ini.dockVista.animate().alpha(0f).setDuration(150);
        ini.indicador.setVisibility(View.INVISIBLE);
    }

    void armar() {
        tarjetas.removeAllViews();
        int base = ini.carril.base(), n = ini.carril.getChildCount() - base;
        boolean disc = ini.prefs.getBoolean("discover", true);
        // Discover (foto K): título, la página propia reducida si existe, e interruptor arriba.
        FrameLayout d = tarjeta();
        if (base > 0) d.addView(new Espejo(ini.carril.getChildAt(0)), Inicio.lleno());
        TextView t = new TextView(ini);
        ini.estilo(t, 22, true, Color.WHITE);
        t.setText("Google Discover");
        t.setGravity(Gravity.CENTER);
        t.setShadowLayer(ini.dp(8), 0, 0, 0x99000000);
        d.addView(t, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, base > 0 ? Gravity.TOP : Gravity.CENTER));
        d.setAlpha(disc ? 1f : .45f);
        d.setOnClickListener(v -> { if (base > 0) salir(0); else if (ini.discover.googleListo()) { salir(-1); ini.discover.abrir(); } });
        tarjetas.addView(columna(interruptor(disc), d, false));
        for (int k = 0; k < n; k++) {
            int pagina = k;
            FrameLayout c = tarjeta();
            c.addView(new Espejo(ini.carril.getChildAt(base + k)), Inicio.lleno());
            c.setOnClickListener(v -> salir(base + pagina));
            LinearLayout fila = new LinearLayout(ini);
            fila.setGravity(Gravity.CENTER);
            boolean casa = ini.principal() == k;
            ImageView home = boton("ic_menu_home", casa ? Inicio.ACENTO : Color.WHITE, v -> {
                ini.prefs.edit().putInt("principal", pagina).apply();
                int x = cinta.getScrollX();
                armar();
                cinta.post(() -> cinta.scrollTo(x, 0));   // sin saltar a la primera tarjeta
            });
            home.setContentDescription(casa ? "Página principal" : "Hacer principal");
            fila.addView(home);
            if (k > 0) fila.addView(boton("ic_delete", Color.WHITE, v -> {
                if (ini.bloqueado()) return;
                List<List<Integer>> p = ini.widgets.paginas();
                if (pagina >= p.size()) return;
                for (int id : new ArrayList<>(p.get(pagina))) if (id >= 0) ini.widgets.host.deleteAppWidgetId(id);
                p.remove(pagina);
                ini.widgets.guardar(p);
                int pr = ini.principal();
                if (pr >= pagina && pr > 0) ini.prefs.edit().putInt("principal", pr - 1).apply();
                rehacer(pagina);
            }));
            tarjetas.addView(columna(fila, c, false));
        }
        FrameLayout mas = tarjeta();   // "+": página nueva
        TextView m = new TextView(ini);
        ini.estilo(m, 64, false, Color.WHITE);
        m.setText("+");
        m.setGravity(Gravity.CENTER);
        mas.addView(m, Inicio.lleno());
        mas.setOnClickListener(v -> {
            if (ini.bloqueado()) return;
            List<List<Integer>> p = ini.widgets.paginas();
            p.add(new ArrayList<>());
            ini.widgets.guardar(p);
            rehacer(p.size());
        });
        tarjetas.addView(columna(new View(ini), mas, true));
    }

    /** Cambió la estructura: se rehace el inicio y se vuelve a este modo sin animar. */
    void rehacer(int centrar) {
        saliendo = true;
        ini.edicion = null;
        ini.mostrar();
        ini.edicion = new Edicion(ini, centrar);
    }

    FrameLayout tarjeta() {
        FrameLayout c = new FrameLayout(ini);
        c.setBackground(new Vidrio.Placa(c, ini.dp(24), 0x26FFFFFF));
        c.setClipToOutline(true);
        c.setElevation(ini.dp(10));
        return c;
    }

    View columna(View arriba, View tarjeta, boolean ultima) {
        LinearLayout c = new LinearLayout(ini);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setGravity(Gravity.CENTER_HORIZONTAL);
        c.addView(arriba, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ini.dp(48)));
        LinearLayout.LayoutParams lt = new LinearLayout.LayoutParams(cw, ch);
        lt.topMargin = ini.dp(8);
        c.addView(tarjeta, lt);
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(cw, ViewGroup.LayoutParams.MATCH_PARENT);
        if (!ultima) l.rightMargin = hueco;
        c.setLayoutParams(l);
        return c;
    }

    View interruptor(boolean on) {
        LinearLayout f = new LinearLayout(ini);
        f.setGravity(Gravity.CENTER_VERTICAL);
        f.setPadding(ini.dp(14), 0, ini.dp(14), 0);
        f.setBackground(ini.seleccionable());
        TextView t = new TextView(ini);
        ini.estilo(t, 14, true, Color.WHITE);
        t.setText(on ? "Activado" : "Desactivado");
        f.addView(t);
        f.addView(Hojas.interruptor(ini, on), margenIzq(ini.dp(10)));
        f.setOnClickListener(v -> {
            ini.prefs.edit().putBoolean("discover", !on).apply();
            rehacer(0);
        });
        return f;
    }

    LinearLayout.LayoutParams margenIzq(int m) {
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ini.dp(40), ini.dp(24));
        l.leftMargin = m;
        return l;
    }

    ImageView boton(String icono, int color, View.OnClickListener al) {
        ImageView b = ini.iconoBoton(icono, 44, al);
        b.setColorFilter(color);
        b.setLayoutParams(new LinearLayout.LayoutParams(ini.dp(44), ini.dp(44)));
        return b;
    }

    View botonPie(String icono, String texto, View.OnClickListener al) {
        LinearLayout b = new LinearLayout(ini);
        b.setOrientation(LinearLayout.VERTICAL);
        b.setGravity(Gravity.CENTER);
        b.setPadding(ini.dp(24), ini.dp(10), ini.dp(24), ini.dp(10));
        b.setBackground(ini.seleccionable());
        ImageView i = new ImageView(ini);
        i.setImageDrawable(ini.ico(icono));
        b.addView(i, new LinearLayout.LayoutParams(ini.dp(32), ini.dp(32)));
        TextView t = new TextView(ini);
        ini.estilo(t, 14, false, Color.WHITE);
        t.setShadowLayer(ini.dp(6), 0, 0, 0x99000000);
        t.setText(texto);
        t.setSingleLine(true);
        t.setPadding(0, ini.dp(8), 0, 0);
        // WRAP explícito: el valor por omisión en columna es MATCH_PARENT y medía el ancho del icono ("Fond", 17/11)
        b.addView(t, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        b.setOnClickListener(al);
        LinearLayout.LayoutParams l = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        l.leftMargin = l.rightMargin = ini.dp(20);
        b.setLayoutParams(l);
        return b;
    }

    /** Página normal de la tarjeta centrada (o -1 si es Discover / "+"). */
    int paginaCentrada() {
        int k = Math.round(cinta.getScrollX() / (float) (cw + hueco)) - 1;
        return k >= 0 && k < ini.carril.getChildCount() - ini.carril.base() ? k : 0;
    }

    /** Vuelve al inicio, a la página "ir" del carril (o a la que estaba con -1). */
    void salir(int ir) {
        if (saliendo) return;
        saliendo = true;
        ini.edicion = null;
        if (ir >= 0) ini.carril.ir(ir, false);
        // Zum hacia la tarjeta elegida hasta que ocupa la pantalla; entonces vuelve la página de verdad.
        int k = 1 + ini.carril.pagina - ini.carril.base();
        tarjetas.setPivotX(tarjetas.getPaddingLeft() + k * (cw + hueco) + cw / 2f);
        tarjetas.setPivotY(ini.dp(56) + ch / 2f);
        cinta.smoothScrollTo(k * (cw + hueco), 0);
        tarjetas.animate().scaleX(1 / esc).scaleY(1 / esc).setDuration(200).setInterpolator(Inicio.DESACELERA).withEndAction(() -> {
            ini.carril.setVisibility(View.VISIBLE);
            ini.raiz.removeView(capa);
        });
        ini.dockVista.animate().alpha(1f).setDuration(200);
        ini.indicador.setVisibility(View.VISIBLE);
        ini.carril.alCambiar.accept(ini.carril.pagina);
    }

    /** Dibuja otra vista reducida: graba sus listas de dibujo (las de sus hijos se reutilizan), no copia píxeles. */
    class Espejo extends View {
        final View pagina;
        Espejo(View p) { super(ini); pagina = p; setAlpha(.92f); }
        @Override protected void onDraw(Canvas c) {
            if (pagina.getWidth() == 0 || pagina.getHeight() == 0) return;
            c.save();
            c.scale(getWidth() / (float) pagina.getWidth(), getHeight() / (float) pagina.getHeight());
            pagina.draw(c);
            c.restore();
        }
    }

}
