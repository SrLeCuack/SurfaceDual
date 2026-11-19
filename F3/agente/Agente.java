import android.content.Context;
import android.content.ContextWrapper;
import android.hardware.display.DisplayManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.util.Log;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.Proxy;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.concurrent.Executor;

// Agente de sistema de SurfaceDual: los controles de Android (volumen, brillo, Wi-Fi) mueven los reales de Windows y
// Windows los mueve a ellos. Nada duplicado: Windows es la base. Lo arranca whpx_init como root, tras boot_completed y
// tras su autoconexión del Wi-Fi (desde ahí el agente es el único que enciende o apaga el Wi-Fi de Android):
//   CLASSPATH=/data/local/tmp/agente.dex app_process / Agente [host=10.0.3.2] [puerto=4449]
// Un solo canal TCP con el Visor (red del sistema, solo root pasa el cortafuegos). Líneas: "vol 0-100", "bri 0-100",
// "wifi on|off", "bt on|off" y "listo 1". Al conectar, Windows manda su estado y "listo"; hasta entonces Android no manda
// nada (barrera por sesión). Sin sondeo: Android avisa por binder (volumen absoluto del altavoz, oyentes de la pantalla y
// del Wi-Fi), Windows por sus eventos. API ocultas por reflexión (como scrcpy).
public class Agente {
    static final String TAG = "agente";
    static final Object cerrojo = new Object();  // protege todo lo de abajo
    static Socket sesion;                        // conexión actual con el Visor (null: sin conexión)
    static boolean sincronizado;                 // Windows ya mandó su estado en esta sesión
    static final LinkedHashMap<String, String> pendientes = new LinkedHashMap<>();   // cola acotada: el último valor por clave
    static long ultimoBri;                       // a lo más 5 brillos por segundo mientras se arrastra la barra
    static int volWindows = -1, briWindows = -1; // lo último que dijo u oyó Windows: su eco no se le devuelve
    static String wifiWindows = "", btWindows = "", ahorroWindows = "", clipWindows = "";
    static final String ASA = "/data/local/tmp/surfacedual_asa";
    static String asa = "";   // franja del asa del Panel Edge ("d,y0,y1" en píxeles de Android, u "off"): el visor no la toma como "Atrás"
    static volatile String giro = "";   // rotación real de Android (0-3): el visor gira con ella sus zonas de gestos
    static volatile boolean bloqueo;    // Windows bloqueado con Android delante: Android duerme, la música sigue
    static String isla = "off";   // rectángulo de la isla compacta ("x0,y0,x1,y1" en píxeles de Android): sus toques, el visor nos los da
    static boolean cargando;                     // Windows enchufado ("bat ...,on,...")
    static boolean gpsAndroid;                   // alguna app pide el GPS: Windows enciende su ubicación mientras dure
    static final int TOPE_CLIP = 256 * 1024;     // "clip <base64>": la única línea de más de 64 (igual que Enlace.cs)
    static final String SHELL = "com.android.shell";
    static Object adm, altavoz;                  // AudioDeviceVolumeManager y el altavoz (virtio-snd sale como speaker)
    static Object dm;                            // DisplayManagerGlobal
    static Object wifi;                          // IWifiManager
    static final Executor directo = Runnable::run;   // los avisos llegan en hilos binder: se atienden ahí, sin bloquear

    public static void main(String[] a) throws Exception {
        if (a.length == 2 && a[0].equals("--traer")) { System.out.println("traer=" + traer(a[1])); return; }   // prueba desde adb
        // prueba desde adb (como shell, sin el puerto): --orden tareas | cerrar <pkg|*> | ajuste avion on ... (archivos en /data/local/tmp/files)
        if (a.length > 1 && a[0].equals("--orden")) { System.out.println("orden=" + ejecutar(java.util.Arrays.copyOfRange(a, 1, a.length))); return; }
        if (a.length == 1 && a[0].equals("--ayudante")) { ayudante(); return; }
        if (a.length == 2 && a[0].equals("--tecla")) { System.out.println("tecla=" + tecla(Integer.parseInt(a[1]))); return; }   // prueba desde adb
        if (a.length == 1 && a[0].equals("--oir")) { oirUbicacion(); Thread.sleep(120000); return; }   // prueba desde adb: mirar logcat -s agente
        boolean servidor = a.length > 0 && a[0].equals("--escuchar");
        String host = servidor ? "" : (a.length > 0 ? a[0] : "10.0.3.2");
        int puerto = a.length > 1 ? Integer.parseInt(a[1]) : 4449;
        try { String g = new String(java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(ASA)), StandardCharsets.US_ASCII).trim(); if (g.matches("off|[di],[0-9]{1,5},[0-9]{1,5}")) asa = g; }
        catch (IOException ignored) { }   // primera vez: aún no hay asa
        try { if (lectura()) lectura(false); } catch (Exception e) { Log.w(TAG, "lectura al arrancar", e); }   // murió con el modo puesto: nada se queda pegado
        // Apps compiladas a código nativo (AOT "speed"): esta imagen las trae en "verify" y la 1.ª apertura iba más lenta
        // (medido 16/11: 316 apps en 144 s). Compila solo lo nuevo (lo ya compilado se salta) y solo con Windows enchufado
        // tras 2 min seguidos con el cargador (la revisión lo marcó como gasto de batería); si no, espera al cargador.
        // App por app (no "-a"): al desenchufar se para entre una y otra y sigue donde iba al volver el cargador.
        Thread aot = new Thread(() -> { try {
            String[] apps = null;
            for (int i = 0; apps == null || i < apps.length; ) {
                for (boolean sigue = false; !sigue; ) {
                    synchronized (cerrojo) { while (!cargando) cerrojo.wait(); }
                    Thread.sleep(120000);
                    synchronized (cerrojo) { sigue = cargando; }
                }
                if (apps == null) apps = sh("cmd package list packages").split("\n");
                for (boolean sigue = true; sigue && i < apps.length; i++) {
                    String app = apps[i].trim().replaceFirst("^package:", "");
                    if (app.matches("[A-Za-z0-9_.]+"))
                        try { sh("cmd package compile -m speed " + app, 600); } catch (IOException e) { Log.w(TAG, "aot " + app + ": " + e.getMessage()); }
                    synchronized (cerrojo) { sigue = cargando; }
                }
            }
            Log.i(TAG, "aot terminado");
        } catch (Exception e) { Log.w(TAG, "aot", e); } });
        aot.setDaemon(true); aot.setPriority(Thread.MIN_PRIORITY); aot.start();
        // Ventanas libres de Android 17 (desktop windowing): sin estas dos, WM Shell responde "Not implemented". Surten efecto al reiniciar.
        // Paneles del launcher (isla, ajustes rápidos, notificaciones): superponerse, escribir brillo/giro y leer avisos.
        sh("appops set com.surfacedual.launcher SYSTEM_ALERT_WINDOW allow; appops set com.surfacedual.launcher WRITE_SETTINGS allow; cmd notification allow_listener com.surfacedual.launcher/com.surfacedual.launcher.IslaOyente; settings put secure secure_overlay_settings 1");   // último: Ajustes de Android ya no oculta los paneles (16/11)
        // Launcher (17/11): leer el fondo para difuminarlo e imágenes para elegir fondo, poner widgets sin preguntar (YouTube Music
        // bajo el reloj) y tema oscuro de Android por omisión UNA vez (después manda la opción "Tema oscuro" del launcher).
        // La isla ya no usa accesibilidad (Centro de seguridad avisaba "acceso total"): se quita la entrada que dejaron versiones
        // anteriores, sin tocar otros servicios del usuario.
        sh("appops set com.surfacedual.launcher ACCESS_RESTRICTED_SETTINGS default; s=$(settings get secure enabled_accessibility_services); "
                + "case \"$s\" in *IslaAccesibilidad*) n=$(echo \"$s\" | tr ':' '\n' | grep -v IslaAccesibilidad | paste -sd: -); "
                + "if [ -n \"$n\" ]; then settings put secure enabled_accessibility_services \"$n\"; else settings delete secure enabled_accessibility_services; settings put secure accessibility_enabled 0; fi;; esac");
        sh("appops set com.surfacedual.launcher MANAGE_EXTERNAL_STORAGE allow; appwidget grantbind --package com.surfacedual.launcher --user 0; "
                + "[ \"$(settings get secure surfacedual_tema)\" = null ] && { cmd uimode night yes; settings put secure surfacedual_tema 1; }");
        // Circle to Search (17/11): Android apaga los dos "assist" en cada arranque, y también DESPUÉS de boot_completed
        // (medido 17/11: puestos a 1 aquí, a los 30 s valían 0): se ponen ahora y otra vez a los 40 s. "panel circulo" los repone.
        Thread asistente = new Thread(() -> { try {
            for (int i = 0; i < 2; i++) {
                sh("settings put secure assist_screenshot_enabled 1; settings put secure assist_structure_enabled 1; settings put secure search_all_entrypoints_enabled 1");
                Thread.sleep(40000);
            }
        } catch (Exception e) { Log.w(TAG, "asistente", e); } });
        asistente.setDaemon(true); asistente.start();
        sh("settings put global enable_freeform_support 1 && settings put global force_resizable_activities 1 && settings put global override_desktop_mode_features 1 && setprop persist.wm.debug.desktop_mode_enforce_device_restrictions false");
        if (!servidor) {
            Thread lapiz = new Thread(() -> pluma(host));
            lapiz.setDaemon(true);
            lapiz.start();
        }
        // Si se reinicia system_server o audioserver, los registros de abajo mueren con él: salir, whpx_init relanza el agente
        for (String s : new String[] { "audio", "media.audio_policy" })
            for (int i = 0; ; i++) {   // recién arrancado, el servicio puede no estar publicado aún
                IBinder b = (IBinder) Class.forName("android.os.ServiceManager").getMethod("getService", String.class).invoke(null, s);
                if (b != null) { b.linkToDeath(() -> System.exit(1), 0); break; }
                if (i == 60) System.exit(1);
                Thread.sleep(1000);
            }
        dm = Class.forName("android.hardware.display.DisplayManagerGlobal").getMethod("getInstance").invoke(null);
        wifi = servicio("wifi", "android.net.wifi.IWifiManager");
        // Avisos de Android (los observadores de Settings no sirven aquí: ContentService exige un proceso de app)
        DisplayManager.DisplayListener brillo = new DisplayManager.DisplayListener() {
            public void onDisplayAdded(int id) { }
            public void onDisplayRemoved(int id) { }
            public void onDisplayChanged(int id) { if (id == 0) brilloCambio(); }
        };
        dm.getClass().getMethod("registerDisplayListener", DisplayManager.DisplayListener.class, Executor.class, long.class, String.class, boolean.class)
            .invoke(dm, brillo, directo, 256L /* INTERNAL_EVENT_FLAG_DISPLAY_BRIGHTNESS_CHANGED */, "com.android.shell", true);
        wifi.getClass().getMethod("addWifiStateChangedListener", Class.forName("android.net.wifi.IWifiStateChangedListener"))
            .invoke(wifi, oyente("android.net.wifi.IWifiStateChangedListener", d -> wifiCambio()));
        try { oirUbicacion(); } catch (Exception e) { Log.w(TAG, "ubicación", e); }
        try { oirPrivacidad(); } catch (Exception e) { Log.w(TAG, "privacidad", e); }
        try { oirVentanas(); } catch (Throwable e) { Log.w(TAG, "ventanas", e); }
        try { oirGiro(); } catch (Throwable e) { Log.w(TAG, "giro", e); }
        alAyudante(null);   // el oyente del portapapeles de Android vive desde ya
        Thread d = new Thread(Agente::dividir);
        d.setDaemon(true);
        d.start();
        Thread t = new Thread(Agente::escritor);
        t.setDaemon(true);
        t.start();
        if (servidor) {
            for (;;) {
                try (java.net.ServerSocket ss = new java.net.ServerSocket(puerto)) {
                    for (;;) {
                        Socket s = ss.accept();
                        String gw = puertaEnlace(), ip = s.getInetAddress().getHostAddress();
                        if (ip.startsWith("::ffff:")) ip = ip.substring(7);
                        if (gw.isEmpty() || !ip.equals(gw)) { Log.w(TAG, "rechazado " + ip + " != " + gw); s.close(); continue; }
                        Log.i(TAG, "sesión desde " + ip);
                        synchronized (cerrojo) {
                            if (sesion != null) try { sesion.close(); } catch (IOException ignored) { }
                            sesion = s; sincronizado = false; pendientes.clear();
                        }
                        new Thread(() -> atenderSesion(s)).start();
                    }
                } catch (IOException e) { Log.w(TAG, "servidor: " + e.getMessage()); }
                Thread.sleep(5000);
            }
        }
        long espera = 5000;
        for (;;) {
            Socket s = new Socket();
            try {
                s.connect(new InetSocketAddress(host, puerto), 3000);   // slirp deja el connect ~2 min en SYN_SENT si Windows no escucha
                espera = 5000;
                synchronized (cerrojo) { sesion = s; sincronizado = false; pendientes.clear(); }
                Log.i(TAG, "conectado a " + host + ":" + puerto);
                atenderSesion(s);
            } catch (IOException e) { }
            synchronized (cerrojo) { if (sesion == s) sesion = null; sincronizado = false; pendientes.clear(); }
            try { s.close(); } catch (IOException e) { }
            // sin Visor: 5, 10 y luego 15 s; en modo Windows Android está suspendido y no reintenta; al volver, sincroniza en segundos
            Thread.sleep(espera);
            espera = Math.min(espera * 2, 15000);
        }
    }

    // Lectura acotada por bytes (nada se acumula sin tope): una línea de más de 64 que no sea "clip" se descarta entera;
    // un "clip" de más de TOPE_CLIP corta la sesión. En el registro solo va la clave (ni portapapeles ni ubicación).
    static void atenderSesion(Socket s) {
        try {
            java.io.InputStream in = new java.io.BufferedInputStream(s.getInputStream());
            java.io.ByteArrayOutputStream l = new java.io.ByteArrayOutputStream();
            boolean descartar = false;
            for (int b; (b = in.read()) >= 0; ) {
                if (b == '\n') {
                    String t = l.toString("UTF-8");
                    if (!descartar) try { aplicar(t.trim()); } catch (Exception e) { Log.w(TAG, "fallo al aplicar " + t.split(" ")[0], e); }
                    l.reset(); descartar = false;
                } else if (!descartar) {
                    l.write(b);
                    if (l.size() == 65 && !l.toString("UTF-8").startsWith("clip ")) descartar = true;
                    else if (l.size() > TOPE_CLIP + 5) break;
                }
            }
        } catch (IOException e) { }
        finally {
            synchronized (cerrojo) { if (sesion == s) { sesion = null; sincronizado = false; pendientes.clear(); } }
            try { s.close(); } catch (IOException e) { }
            alAyudante("sinloc");   // sin Windows no llegan ubicaciones: fuera el proveedor de prueba
        }
    }

    // Android guarda la ruta por omisión en una tabla por red (medido: "default via 172.25.32.1 dev eth0 table 1005"), no en
    // la principal que muestra /proc/net/route: se pregunta a todas las tablas.
    static String puertaEnlace() {
        try {
            Process p = new ProcessBuilder("/system/bin/ip", "-4", "route", "show", "table", "all").redirectErrorStream(true).start();
            try (BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream(), StandardCharsets.US_ASCII))) {
                for (String l; (l = r.readLine()) != null; ) {
                    String[] c = l.trim().split("\\s+");
                    if (c.length > 4 && c[0].equals("default") && c[1].equals("via") && c[3].equals("dev") && c[4].startsWith("eth")) return c[2];
                }
            } finally { p.destroy(); }
        } catch (Exception e) { Log.w(TAG, "puertaEnlace", e); }
        return "";
    }


    static void pluma(String host) {
        long espera = 5000;
        for (;;) {
            Process puente = null;
            try (Socket s = new Socket()) {
                s.setTcpNoDelay(true);
                s.connect(new InetSocketAddress(host, 4451), 3000);
                espera = 5000;
                puente = new ProcessBuilder("/data/local/tmp/surfacedual-pen").redirectError(ProcessBuilder.Redirect.INHERIT).start();
                Process proceso = puente;
                Thread supervisor = new Thread(() -> {
                    try { proceso.waitFor(); s.close(); }
                    catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                    catch (IOException e) { }
                });
                supervisor.setDaemon(true);
                supervisor.start();
                try (java.io.OutputStream destino = puente.getOutputStream()) {
                    java.io.DataInputStream entrada = new java.io.DataInputStream(s.getInputStream());
                    byte[] muestra = new byte[16];
                    for (;;) { entrada.readFully(muestra); destino.write(muestra); destino.flush(); }
                }
            } catch (IOException e) { Log.w(TAG, "pluma: " + e.getMessage()); }
            finally {
                if (puente != null) {
                    try {
                        puente.getOutputStream().close();
                        if (!puente.waitFor(1, java.util.concurrent.TimeUnit.SECONDS)) puente.destroyForcibly();
                    } catch (IOException e) { puente.destroyForcibly(); }
                    catch (InterruptedException e) { puente.destroyForcibly(); Thread.currentThread().interrupt(); return; }
                }
            }
            try { Thread.sleep(espera); } catch (InterruptedException e) { return; }
            espera = Math.min(espera * 2, 15000);
        }
    }

    // Windows -> Android.
    static void aplicar(String l) throws Exception {
        String[] p = l.split(" ");
        if (p.length != 2) return;
        if (p[0].equals("listo")) {
            // Modo lectura: al ocultar Android Windows restaura su ahorro y al volver manda "ahorro off" (justo antes de "listo"): se repide.
            synchronized (cerrojoLectura) {   // un "lectura off" a la vez no deja el ahorro repedido sin respaldo
                boolean lec = lectura();
                synchronized (cerrojo) { sincronizado = true; encolar("gps", gpsAndroid ? "on" : "off"); if (!asa.isEmpty()) encolar("asa", asa); encolar("isla", isla); if (!giro.isEmpty()) encolar("giro", giro);
                    encolar("lectura", lec ? "on" : "off");   // el visor: 60 Hz y EcoQoS ampliado solo con el modo puesto
                    if (lec) { ahorroWindows = "on"; encolar("ahorro", "on"); } }
                if (lec) sh("cmd power set-mode 1");
            }
        } else if (p[0].equals("clip") && p[1].length() <= TOPE_CLIP && p[1].matches("[A-Za-z0-9+/=]+")
                || p[0].equals("loc") && p[1].matches("-?[0-9]{1,3}\\.[0-9]{1,8},-?[0-9]{1,3}\\.[0-9]{1,8},[0-9]{1,6}")) {
            // Los atiende el ayudante con uid shell: ClipboardService solo deja leer en segundo plano a quien tiene
            // READ_CLIPBOARD_IN_BACKGROUND, root no (desensamblado de services.jar, 17/11); la ubicación simulada, igual.
            if (p[0].equals("clip")) synchronized (cerrojo) { clipWindows = p[1]; pendientes.remove("clip"); }
            alAyudante(l);
        } else if (p[0].equals("isla") && p[1].matches("toque,[0-9]{1,5}|mantener|izq|der|arriba|abajo")) {
            alLauncher("isla", "gesto " + p[1]);   // toque en la isla compacta que tomó el visor (SystemUI no lo deja pasar)
        } else if (p[0].equals("despierta") && p[1].equals("1")) {
            // Android vuelve de la pausa de la VM: la conexión de push de Google (mtalk, 5228-5230) murió durante la pausa y GMS
            // esperaba ~60 s a su latido para notarlo (medido 17/11: avisos 61 s tarde). Cortada, GMS se reconecta al momento.
            // ss directo y no por sh: sh pasa al dominio shell, que no puede abrir sock_diag (medido 17/11: Permission denied).
            new Thread(() -> { try {
                java.io.File salida = java.io.File.createTempFile("ss_k", ".txt", new java.io.File("/data/local/tmp"));
                try {
                    Process pr = new ProcessBuilder("/system/bin/ss", "-K", "-tn", "dport", "=", ":5228", "or", "dport", "=", ":5229", "or", "dport", "=", ":5230")
                        .redirectErrorStream(true).redirectOutput(salida).start();
                    int rc = pr.waitFor(5, java.util.concurrent.TimeUnit.SECONDS) ? pr.exitValue() : -1;
                    String t = new String(java.nio.file.Files.readAllBytes(salida.toPath()), StandardCharsets.UTF_8).replaceAll("\\s+", " ").trim();
                    Log.i(TAG, "despierta: ss=" + rc + " " + t.substring(0, Math.min(300, t.length())));   // lo cortado (o el error)
                } finally { salida.delete(); }
            } catch (Throwable e) { Log.w(TAG, "despierta: " + e); } }).start();
        } else if (p[0].equals("rot") && p[1].matches("0|90|180|270")) {
            // Giro de Windows (sensor de la Surface): Android no tiene acelerómetro en la VM. Con "Girar" de Android encendido se
            // pone la rotación y se deja en libre (sin sensor, libre conserva la última); apagado, Android no gira (como un Pixel).
            int giroPedido = Integer.parseInt(p[1]) / 90;
            String res = sh("s=$(settings get system accelerometer_rotation); [ \"$s\" = 0 ] && echo fijo || { cmd window user-rotation lock " + giroPedido
                    + " && settings put system accelerometer_rotation 1; echo r" + giroPedido + "; }").trim();
            Log.i(TAG, "rot " + p[1] + " -> " + res);
        } else if (p[0].equals("vol")) {
            int v = Math.max(0, Math.min(100, Integer.parseInt(p[1])));
            synchronized (cerrojo) { volWindows = v; pendientes.remove("vol"); }   // lo de Windows manda: fuera lo viejo en cola
            alLauncher("isla", "volwin");   // la isla no enseña su aviso de volumen: Windows ya enseña el suyo
            if (adm == null) volumenAbsoluto(v);
            else adm.getClass().getMethod("notifyAbsoluteVolumeChanged", info().getClass(), altavoz.getClass()).invoke(adm, info(v), altavoz);
            // AudioService solo da escala completa a los HDMI con volumen absoluto (desensamblado de services.jar, 15/11); al
            // altavoz se lo dice este aviso directo a audioserver: la música sale a 0 dB salvo en 0 (medido: pico 0,12 en el
            // índice 5 y en el 15; sin él, 0,006 en el 5). Se repite en cada cambio: es idempotente y barato.
            int rc = (int) Class.forName("android.media.AudioSystem").getMethod("setDeviceAbsoluteVolumeEnabled", int.class, String.class, boolean.class, int.class)
                .invoke(null, 2 /* AUDIO_DEVICE_OUT_SPEAKER */, "", true, 3 /* STREAM_MUSIC */);
            if (rc != 0) Log.w(TAG, "audioserver no aceptó el volumen absoluto: " + rc);
        } else if (p[0].equals("bri")) {
            int v = Math.max(0, Math.min(100, Integer.parseInt(p[1])));
            synchronized (cerrojo) { briWindows = v; pendientes.remove("bri"); }
            Object bi = dm.getClass().getMethod("getBrightnessInfo", int.class).invoke(dm, 0);
            float min = bi.getClass().getField("brightnessMinimum").getFloat(bi), max = bi.getClass().getField("brightnessMaximum").getFloat(bi);
            float lin = (float) Class.forName("com.android.internal.display.BrightnessUtils").getMethod("convertGammaToLinear", float.class).invoke(null, v / 100f);
            dm.getClass().getMethod("setBrightness", int.class, float.class).invoke(dm, 0, min + lin * (max - min));
        } else if (p[0].equals("wifi") && (p[1].equals("on") || p[1].equals("off"))) {
            synchronized (cerrojo) { wifiWindows = p[1]; pendientes.remove("wifi"); }
            // cmd wifi queda registrado como com.android.shell: wifiLlamador sabe que no fue el usuario
            if (!p[1].equals(wifiEstado())) sh("cmd wifi set-wifi-enabled " + (p[1].equals("on") ? "enabled" : "disabled"));
        }
        else if (p[0].equals("idioma") && p[1].matches("[A-Za-z]{2,3}(-[A-Za-z0-9]{2,8})*")) {
            // Idioma de Windows: el mismo camino que Ajustes (LocalePicker -> updatePersistentConfiguration), solo si cambia
            Object am = Class.forName("android.app.ActivityManager").getMethod("getService").invoke(null);
            Object conf = am.getClass().getMethod("getConfiguration").invoke(am);
            android.os.LocaleList ls = ((android.content.res.Configuration) conf).getLocales();
            String actual = ls.isEmpty() ? "" : ls.get(0).toLanguageTag();
            if (!actual.equals(p[1])) {
                Class.forName("com.android.internal.app.LocalePicker").getMethod("updateLocales", android.os.LocaleList.class)
                    .invoke(null, android.os.LocaleList.forLanguageTags(p[1]));
                Log.i(TAG, "idioma " + actual + " -> " + p[1]);
            }
        } else if (p[0].equals("zona") && p[1].matches("[A-Za-z0-9_+/-]{1,64}")) {
            // Zona de Windows: la detección automática de Android se apaga siempre (si no, su ubicación simulada la cambia
            // a Los Ángeles después) y la zona se pone como si el usuario la eligiera en Ajustes, solo si cambia
            sh("cmd time_zone_detector set_auto_detection_enabled false");
            Object v = Class.forName("android.os.SystemProperties").getMethod("get", String.class).invoke(null, "persist.sys.timezone");
            if (!p[1].equals(v)) {
                sh("cmd time_zone_detector suggest_manual_time_zone --zone_id " + p[1]);
                Log.i(TAG, "zona " + v + " -> " + p[1]);
            }
        } else if (p[0].equals("bat") && p[1].matches("[0-9]{1,3},(on|off),(charging|discharging|not-charging|full)")) {
            // Batería de Windows (VM de Hyper-V: sin batería virtual). Como el usuario no la ve cambiar a menudo, por cmd basta.
            String[] b = p[1].split(",");
            int nivel = Math.min(100, Integer.parseInt(b[0]));
            int estado = b[2].equals("charging") ? 2 : b[2].equals("discharging") ? 3 : b[2].equals("not-charging") ? 4 : 5;
            sh("cmd battery set ac " + (b[1].equals("on") ? 1 : 0) + "; cmd battery set level " + nivel + "; cmd battery set status " + estado);
            synchronized (cerrojo) { cargando = b[1].equals("on"); cerrojo.notifyAll(); }
        } else if (p[0].equals("ahorro") && (p[1].equals("on") || p[1].equals("off"))) {
            // Ahorro de energía de Windows -> modo de ahorro de batería de Android (cmd power set-mode: 1 = ahorro).
            synchronized (cerrojo) { ahorroWindows = p[1]; pendientes.remove("ahorro"); }
            sh("cmd power set-mode " + (p[1].equals("on") ? 1 : 0));
        } else if (p[0].equals("flotar")) {
            flotar();
        } else if (p[0].equals("split")) {
            split();
        } else if (p[0].equals("linea") && p[1].matches("mantener|suelta")) {
            alLauncher("linea", p[1]);
        } else if (p[0].equals("bloqueo") && p[1].matches("[01]")) {
            // Windows bloqueado (botón de encendido) con Android delante: Android duerme (su pantalla de bloqueo, con la música)
            // y la VM sigue para que suene. Sin audio un rato, se pide al visor pausar la VM (batería).
            bloqueo = p[1].equals("1");
            if (bloqueo) vigilarReposo();
        } else if (p[0].equals("panel") && p[1].equals("circulo")) {
            alLauncher("linea", "disparo");
            // Mantener pulsada la línea de abajo (lo detecta el visor) = Circle to Search nativo.
            // Primero la tecla; los dos ajustes "assist" (Android los apaga a veces) se reponen después, en otro hilo (cts_propuesta P2).
            if (!tecla(android.view.KeyEvent.KEYCODE_CONTEXTUAL_SEARCH)) sh("input keyevent KEYCODE_CONTEXTUAL_SEARCH");   // ~0,3 s más lento
            Log.i(TAG, "circulo");
            new Thread(() -> { try { sh("settings put secure assist_screenshot_enabled 1; settings put secure assist_structure_enabled 1"); } catch (Exception e) { Log.w(TAG, "circulo: " + e); } }).start();
        } else if (p[0].equals("panel") && p[1].matches("qs|noti|recientes|cajon")) {
            // Gestos que el visor detecta (arriba-derecha = ajustes rápidos, arriba-izquierda = notificaciones, tres dedos o
            // subir y mantener = recientes, tecla Windows = cajón): los dibuja el launcher (su receptor exige DUMP: root y shell).
            // Por binder desde este proceso: sin lanzar procesos (instantáneo) y sin pasar descriptores a system_server, que
            // los rechaza de este dominio SELinux ("cmd: Failed transaction": por eso los gestos no abrían nada, 17/11).
            if (!panel(p[1])) sh("cmd activity broadcast -a com.surfacedual.launcher.PANEL -n com.surfacedual.launcher/.OrdenPanel --es cual " + p[1]);
        } else if (p[0].equals("bt") && (p[1].equals("on") || p[1].equals("off"))) {
            // La pila de Bluetooth de Android sigue apagada (sin controlador entra en un bucle de caídas): el estado de
            // Windows solo se guarda para el botón de los ajustes rápidos del launcher ("estado").
            synchronized (cerrojo) { btWindows = p[1]; }
        }
    }

    static int tareaFrente() {
        try {
            for (String t : sh("am stack list").split("\n")) {
                t = t.trim();
                if (!t.startsWith("taskId=") || t.startsWith("taskId=0:") || t.contains("com.surfacedual.launcher/")) continue;
                int i = t.indexOf(':');
                if (i > 7) return Integer.parseInt(t.substring(7, i));
            }
        } catch (Exception e) { Log.w(TAG, "tareaFrente", e); }
        return -1;
    }

    // Android 17 ya no tiene setTaskWindowingMode: relanzar la raíz de la tarea con --windowingMode 5 (como tocar su
    // ícono) pasa la tarea existente a ventana libre sin crear otra actividad. Medido: Ajustes y Edge -> mode=freeform.
    static void flotar() throws Exception {
        for (String t : sh("am stack list").split("\n")) {
            t = t.trim();
            if (!t.startsWith("taskId=") || t.startsWith("taskId=0:") || !t.contains("visible=true") || t.contains("com.surfacedual.launcher/")) continue;
            String[] c = t.split(" ");
            if (c.length < 2 || !c[1].matches("[A-Za-z0-9_.]+/[A-Za-z0-9_.$]+")) continue;
            sh("am start --windowingMode 5 -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n " + c[1].replace("$", "\\$"));
            Log.i(TAG, "flotar " + c[1]);
            return;
        }
    }

    static void split() throws Exception {
        int id = tareaFrente();
        if (id <= 0) return;
        sh("dumpsys activity service SystemUIService WMShell splitscreen moveToSideStage " + id + " 0 >/dev/null 2>&1");   // SystemUI escribe a /dev/null: sin permiso sobre /data/local/tmp
        Thread.sleep(150);
        sh("input keyevent 187");
        Log.i(TAG, "split tarea " + id);
    }

    // Volumen: el altavoz pasa a "volumen absoluto" (como un BT o un HDMI-CEC): la barra de Android solo informa a quien
    // maneja el volumen, aquí Windows. Si el agente muere, AudioService lo detecta y el altavoz vuelve al volumen normal.
    static void volumenAbsoluto(int v) throws Exception {
        Context ctx = new ContextWrapper(null) {
            @Override public Context getApplicationContext() { return this; }
            @Override public String getOpPackageName() { return "com.android.shell"; }
            @Override public String getPackageName() { return "com.android.shell"; }
        };
        Class<?> c = Class.forName("android.media.AudioDeviceVolumeManager");
        Object m = c.getConstructor(Context.class).newInstance(ctx);
        Class<?> ada = Class.forName("android.media.AudioDeviceAttributes");
        Object alt = ada.getConstructor(int.class, int.class, String.class).newInstance(2 /* ROLE_OUTPUT */, 2 /* TYPE_BUILTIN_SPEAKER */, "");
        Class<?> oyente = Class.forName("android.media.AudioDeviceVolumeManager$OnAudioDeviceVolumeChangedListener");
        Object o = Proxy.newProxyInstance(Agente.class.getClassLoader(), new Class<?>[] { oyente }, (px, mt, args) -> {
            if (mt.getName().equals("onAudioDeviceVolumeChanged")) volumenCambio(args[1]);
            return mt.getName().equals("hashCode") ? 0 : mt.getName().equals("equals") ? px == args[0] : null;
        });
        c.getMethod("setDeviceAbsoluteVolumeBehavior", ada, info().getClass(), Executor.class, oyente).invoke(m, alt, info(v), directo, o);
        altavoz = alt; adm = m;
        Log.i(TAG, "altavoz en volumen absoluto, Windows " + v);
    }

    static Object info() throws Exception { return info(0); }
    static Object info(int v) throws Exception {   // VolumeInfo de la música en la escala de Windows (0-100)
        Class<?> b = Class.forName("android.media.VolumeInfo$Builder");
        Object x = b.getConstructor(int.class).newInstance(3 /* STREAM_MUSIC */);
        x = b.getMethod("setMinVolumeIndex", int.class).invoke(x, 0);
        x = b.getMethod("setMaxVolumeIndex", int.class).invoke(x, 100);
        x = b.getMethod("setVolumeIndex", int.class).invoke(x, v);
        return b.getMethod("build").invoke(x);
    }

    // Android -> Windows. Los avisos solo encolan: nunca escriben en el socket desde un hilo binder.
    static void volumenCambio(Object vi) throws Exception {
        int v = (int) vi.getClass().getMethod("getVolumeIndex").invoke(vi);
        synchronized (cerrojo) {
            if (v == volWindows) return;   // notifyAbsoluteVolumeChanged no avisa de vuelta (medido): esto solo evita repetir
            volWindows = v;
            encolar("vol", "" + v);
        }
    }

    // Brillo actual (también el temporal mientras se arrastra la barra) como posición de la barra de Android (0-100),
    // que es lo que se muestra igual en Windows. Misma cuenta que SystemUI: normalizar entre mínimo y máximo y pasar a gamma.
    static void brilloCambio() {
        try {
            Object bi = dm.getClass().getMethod("getBrightnessInfo", int.class).invoke(dm, 0);
            Class<?> c = bi.getClass();
            float lin = c.getField("brightness").getFloat(bi), min = c.getField("brightnessMinimum").getFloat(bi), max = c.getField("brightnessMaximum").getFloat(bi);
            float g = (float) Class.forName("com.android.internal.display.BrightnessUtils").getMethod("convertLinearToGamma", float.class).invoke(null, (lin - min) / (max - min));
            int p = Math.round(g * 100);
            synchronized (cerrojo) {
                if (Math.abs(p - briWindows) <= 1) return;   // histéresis: el redondeo gamma/lineal no rebota hacia Windows
                briWindows = p;
                encolar("bri", "" + p);
            }
        } catch (Exception e) { Log.w(TAG, "brillo", e); }
    }

    // El Wi-Fi cambió: se pregunta a WifiService quién llamó por última vez a setWifiEnabled (respuesta asíncrona).
    static void wifiCambio() {
        Socket s;
        synchronized (cerrojo) { s = sesion; }
        if (s == null || wifiEstado() == null) return;   // sin Visor, o a medio cambiar
        try {
            Class<?> il = Class.forName("android.net.wifi.ILastCallerListener");
            wifi.getClass().getMethod("getLastCallerInfoForApi", int.class, il).invoke(wifi, 2 /* API_WIFI_ENABLED */,
                oyente(il.getName(), d -> wifiLlamador(s, d.readString(), d.readInt() != 0)));   // onResult(paquete, encender)
        } catch (Exception e) { Log.w(TAG, "wifi", e); }
    }

    // Apagar el Wi-Fi de Windows corta su red: solo lo mueve un gesto del usuario en Android (Ajustes o configuración
    // rápida). No cuentan lo que pidieron el agente o whpx_init (cmd wifi = com.android.shell), los fallos ni el modo avión
    // (no pasan por setWifiEnabled), ni un estado distinto de lo pedido, ni una respuesta de otra sesión. Ante la duda, nada.
    static void wifiLlamador(Socket deSesion, String paquete, boolean encender) {
        String e = wifiEstado();
        Log.i(TAG, "wifi " + e + ", último llamador " + paquete + " " + encender);
        synchronized (cerrojo) {
            if (deSesion != sesion || e == null || paquete == null || paquete.equals("com.android.shell") || !e.equals(encender ? "on" : "off") || e.equals(wifiWindows)) return;
            wifiWindows = e;
            encolar("wifi", e);
        }
    }

    static String wifiEstado() {
        try {
            int s = (int) wifi.getClass().getMethod("getWifiEnabledState").invoke(wifi);
            return s == 3 ? "on" : s == 1 ? "off" : null;   // WIFI_STATE_ENABLED / DISABLED
        } catch (Exception e) { return null; }
    }

    // Alguna app pide el GPS (por la ubicación combinada de Play o directo): se avisa a Windows para que encienda la suya
    // solo entonces. Solo "gps": la combinada deja pedidos BALANCED de intervalo enorme siempre activos (dumpsys 17/11).
    static void oirUbicacion() throws Exception {
        Object lm = servicio("location", "android.location.ILocationManager");
        Class<?> il = Class.forName("android.location.provider.IProviderRequestListener"), pr = Class.forName("android.location.provider.ProviderRequest");
        android.os.Parcelable.Creator<?> crear = (android.os.Parcelable.Creator<?>) pr.getField("CREATOR").get(null);
        java.lang.reflect.Method activo = pr.getMethod("isActive");
        lm.getClass().getMethod("addProviderRequestListener", il).invoke(lm, oyente(il.getName(), d -> {   // onProviderRequestChanged(proveedor, pedido)
            if (!"gps".equals(d.readString())) return;
            try {
                boolean on = d.readInt() != 0 && (boolean) activo.invoke(crear.createFromParcel(d));   // pedido nulo: apagado
                synchronized (cerrojo) { if (on == gpsAndroid) return; gpsAndroid = on; encolar("gps", on ? "on" : "off"); }
                if (!on) alAyudante("sinloc");   // nadie pide ubicación: fuera el proveedor de prueba
                Log.i(TAG, "gps pedido " + on);
            } catch (Exception e) { Log.w(TAG, "gps", e); }
        }));
    }

    // Ayudante con uid shell (portapapeles y ubicación simulada): lo arranca el agente y le habla por su entrada estándar.
    // Si muere, renace con la próxima línea de Windows o tras una espera que se duplica (sin bucle de arranques).
    static final Object cerrojoAyudante = new Object();
    static Process ayudante;
    static long esperaAyudante = 5000;

    static void alAyudante(String linea) {
        synchronized (cerrojoAyudante) {
            try {
                if (ayudante == null || !ayudante.isAlive()) {
                    Process p = new ProcessBuilder("/system/bin/app_process", "/", "Agente", "--ayudante").redirectError(ProcessBuilder.Redirect.INHERIT).start();
                    ayudante = p;
                    Thread t = new Thread(() -> leerAyudante(p));
                    t.setDaemon(true);
                    t.start();
                }
                if (linea != null) { ayudante.getOutputStream().write((linea + "\n").getBytes(StandardCharsets.UTF_8)); ayudante.getOutputStream().flush(); }
            } catch (IOException e) { Log.w(TAG, "ayudante", e); if (ayudante != null) ayudante.destroy(); ayudante = null; }
        }
    }

    static void leerAyudante(Process p) {
        long inicio = System.currentTimeMillis();
        try (BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream(), StandardCharsets.UTF_8))) {
            for (String l; (l = r.readLine()) != null; ) {
                String[] m = l.startsWith("mini ") ? l.split(" ") : null;   // respuesta a miniatura(): "mini id hora jpeg|=|-"
                if (m != null && m.length == 4) { java.util.concurrent.CompletableFuture<String[]> f = minis.get(Integer.parseInt(m[1])); if (f != null) f.complete(m); continue; }
                if (!l.startsWith("clip ") || l.length() > TOPE_CLIP + 5) continue;
                String v = l.substring(5);
                synchronized (cerrojo) { if (v.equals(clipWindows)) continue; clipWindows = v; encolar("clip", v); }
            }
        } catch (IOException e) { }
        Log.w(TAG, "ayudante terminó");
        try {
            synchronized (cerrojoAyudante) { esperaAyudante = System.currentTimeMillis() - inicio > 600000 ? 5000 : Math.min(esperaAyudante * 2, 600000); }
            Thread.sleep(esperaAyudante);
            synchronized (cerrojoAyudante) { if (ayudante == p || ayudante == null) alAyudante(null); }   // si otro ya lo relanzó, nada
        } catch (InterruptedException e) { }
    }

    // --ayudante: pasa a uid shell (com.android.shell tiene READ_CLIPBOARD_IN_BACKGROUND, como scrcpy). Entrada "clip <b64>"
    // y "loc lat,lon,precisión"; salida "clip <b64>" cuando se copia algo en Android. Sin ecos: recuerda el último texto.
    static String clipVisto;
    static void ayudante() throws Exception {
        if (android.system.Os.getuid() == 0) { android.system.Os.setgid(2000); android.system.Os.setuid(2000); }
        // Sin privilegios de root: uid/gid shell y ningún grupo 0 heredado (Os no expone setgroups; el servicio de init
        // del agente no declara grupos extra). Si no se cumple, no arranca.
        String estado = new String(java.nio.file.Files.readAllBytes(java.nio.file.Paths.get("/proc/self/status")), StandardCharsets.US_ASCII);
        if (!estado.matches("(?s).*\nUid:\t2000\t2000\t2000\t2000\n.*") || !estado.matches("(?s).*\nGid:\t2000\t2000\t2000\t2000\n.*")
                || java.util.Arrays.asList(estado.replaceFirst("(?s).*\nGroups:([^\n]*)\n.*", "$1").trim().split("\\s+")).contains("0"))
            throw new SecurityException("ayudante con privilegios");
        Object cb = servicio("clipboard", "android.content.IClipboard");
        java.lang.reflect.Method leer = cb.getClass().getMethod("getPrimaryClip", String.class, String.class, int.class, int.class);
        java.lang.reflect.Method poner = cb.getClass().getMethod("setPrimaryClip", android.content.ClipData.class, String.class, String.class, int.class, int.class);
        Class<?> io = Class.forName("android.content.IOnPrimaryClipChangedListener");
        cb.getClass().getMethod("addPrimaryClipChangedListener", io, String.class, String.class, int.class, int.class).invoke(cb, oyente(io.getName(), d -> {
            try {
                android.content.ClipData x = (android.content.ClipData) leer.invoke(cb, SHELL, null, 0, 0);
                CharSequence c = x == null || x.getItemCount() == 0 ? null : x.getItemAt(0).getText();
                if (c == null || c.length() == 0) return;
                String t = c.toString(), b = java.util.Base64.getEncoder().encodeToString(t.getBytes(StandardCharsets.UTF_8));
                synchronized (Agente.class) {
                    if (t.equals(clipVisto) || b.length() > TOPE_CLIP) return;
                    clipVisto = t;
                    System.out.println("clip " + b);
                    System.out.flush();
                }
            } catch (Exception e) { Log.w(TAG, "clip", e); }
        }), SHELL, null, 0, 0);
        boolean simulando = false;
        BufferedReader r = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
        for (String l; (l = r.readLine()) != null; ) {
            try {
                if (l.startsWith("clip ")) {
                    String t = new String(java.util.Base64.getDecoder().decode(l.substring(5)), StandardCharsets.UTF_8);
                    synchronized (Agente.class) { clipVisto = t; }
                    android.content.ClipData x = android.content.ClipData.newPlainText("Windows", t);
                    // Copia remota: sin la burbuja "copiado" de SystemUI (la respeta con ro.boot.qemu=1, medido 17/11)
                    android.os.PersistableBundle extra = new android.os.PersistableBundle();
                    extra.putBoolean("com.android.systemui.SUPPRESS_CLIPBOARD_OVERLAY", true);
                    x.getDescription().setExtras(extra);
                    poner.invoke(cb, x, SHELL, null, 0, 0);
                } else if (l.startsWith("loc ")) {
                    int coma = l.lastIndexOf(',');
                    if (!simulando) {   // el proveedor de prueba sustituye al GPS (la VM no tiene) mientras alguien lo pide
                        sh("appops set " + SHELL + " android:mock_location allow; cmd location providers remove-test-provider gps; cmd location providers add-test-provider gps --supportsAltitude --supportsSpeed --supportsBearing; cmd location providers set-test-provider-enabled gps true");
                        simulando = true;
                    }
                    sh("cmd location providers set-test-provider-location gps --location " + l.substring(4, coma) + " --accuracy " + l.substring(coma + 1));
                } else if (l.matches("mini [0-9]{1,9} [0-9]{1,19}")) {
                    String[] m = l.split(" "), res;
                    try { res = capturar(Integer.parseInt(m[1]), Long.parseLong(m[2])).split(" "); } catch (Exception e) { Log.w(TAG, "miniatura " + m[1] + ": " + e); res = new String[] { "0", "-" }; }
                    synchronized (Agente.class) { System.out.println("mini " + m[1] + " " + res[0] + " " + res[1]); System.out.flush(); }
                } else if (l.equals("sinloc")) {   // nadie la pide o se cerró la sesión: el GPS vuelve a ser el de Android
                    sh("cmd location providers remove-test-provider gps; appops set " + SHELL + " android:mock_location default");
                    simulando = false;
                }
            } catch (Exception e) { Log.w(TAG, "ayudante " + l.substring(0, Math.max(0, l.indexOf(' '))), e); }   // sin datos en el registro
        }
    }

    static void encolar(String k, String v) {   // con cerrojo; antes de "listo" o sin sesión se descarta (barrera)
        if (sesion == null || !sincronizado) return;
        pendientes.put(k, v);
        cerrojo.notifyAll();
    }

    // Único hilo que escribe al Visor. Si falla, cierra el socket entero: el lector se desbloquea y reconecta.
    static void escritor() {
        for (;;) {
            String k, v; Socket s;
            synchronized (cerrojo) {
                try {
                    while (sesion == null || pendientes.isEmpty()) cerrojo.wait();
                    k = pendientes.keySet().iterator().next();
                    long falta = k.equals("bri") ? ultimoBri + 200 - System.currentTimeMillis() : 0;
                    if (falta > 0) { cerrojo.wait(falta); continue; }   // entretanto, un brillo nuevo reemplaza al pendiente
                    v = pendientes.remove(k);
                    s = sesion;
                    if (k.equals("bri")) ultimoBri = System.currentTimeMillis();
                } catch (InterruptedException e) { return; }
            }
            try { s.getOutputStream().write((k + " " + v + "\n").getBytes(StandardCharsets.UTF_8)); Log.i(TAG, "-> " + k + (k.equals("clip") ? " (" + v.length() + " c)" : " " + v)); }
            catch (IOException e) { try { s.close(); } catch (IOException x) { } }
        }
    }

    // Órdenes del launcher (pantalla dividida, Recientes, isla...): Android 17 solo deja algunas a su propio launcher o al
    // shell de WM. Socket local "surfacedual.agente" (espacio abstracto): el kernel da el uid del otro extremo (SO_PEERCRED),
    // así que el agente solo atiende al uid del launcher y el launcher solo cree a uid 0 (otra app pudo quedarse el nombre).
    // Sin clave en los archivos del launcher ni /proc/net (revisión de seguridad 17/11). "orden args" -> "ok[ datos]".
    static final String SOCKET = "surfacedual.agente";

    static final java.util.concurrent.ExecutorService colaLauncher = java.util.concurrent.Executors.newSingleThreadExecutor();
    static void dividir() {
        for (;;) {
            try (android.net.LocalServerSocket ss = new android.net.LocalServerSocket(SOCKET)) {
                for (;;) {
                    android.net.LocalSocket c = ss.accept();
                    // Primero quién llama; las del launcher se atienden en orden, en un solo hilo, sin descartar ninguna.
                    boolean esLauncher;
                    try { esLauncher = c.getPeerCredentials().getUid() == uidLauncher(); }
                    catch (Exception e) { esLauncher = false; }
                    if (!esLauncher) { c.close(); continue; }
                    colaLauncher.execute(() -> { try (android.net.LocalSocket x = c) { atender(x); } catch (Exception e) { Log.w(TAG, "launcher", e); } });
                }
            } catch (IOException e) { Log.w(TAG, "sin socket " + SOCKET + ", reintento en 15 s", e); }   // otra app lo ocupa: el launcher no la cree
            try { Thread.sleep(15000); } catch (InterruptedException e) { return; }
        }
    }

    static int uidLauncher() throws Exception {   // en cada conexión: sobrevive a reinstalar el launcher
        Object pm = Class.forName("android.app.ActivityThread").getMethod("getPackageManager").invoke(null);
        return (int) pm.getClass().getMethod("getPackageUid", String.class, long.class, int.class).invoke(pm, "com.surfacedual.launcher", 0L, 0);
    }

    static void atender(android.net.LocalSocket c) throws Exception {
        c.setSoTimeout(2000);
        String l = new BufferedReader(new InputStreamReader(c.getInputStream(), StandardCharsets.US_ASCII)).readLine();
        if (l == null || l.isEmpty()) return;
        String r = ejecutar(l.split(" "));
        if (r == null) return;   // orden desconocida o fallida: sin "ok", el launcher hace lo suyo
        c.getOutputStream().write(("ok" + r + "\n").getBytes(StandardCharsets.US_ASCII));
    }

    // Órdenes del launcher. Devuelve null (sin "ok"), "" o " datos".
    static String ejecutar(String[] p) throws Exception {
        if (p[0].equals("dividir") && p.length == 2 && p[1].matches("[A-Za-z0-9_.]+/[A-Za-z0-9_.$]+")) {
            String comp = p[1], pkg = comp.substring(0, comp.indexOf('/'));
            sh("am start -W -n " + comp.replace("$", "\\$") + " -a android.intent.action.MAIN -c android.intent.category.LAUNCHER");
            // tareas de arriba abajo: la primera que no sea esta app ni el launcher es la que se usaba antes
            String otra = null;
            for (String t : sh("am stack list").split("\n")) {
                t = t.trim();
                if (!t.startsWith("taskId=") || t.startsWith("taskId=0:") || t.contains(" " + pkg + "/") || t.contains("com.surfacedual.launcher/")) continue;
                otra = t.substring(7, t.indexOf(':'));
                break;
            }
            if (otra != null) sh("dumpsys activity service SystemUIService WMShell splitscreen moveToSideStage " + otra + " 0 >/dev/null 2>&1");
            Log.i(TAG, "dividir " + comp + " junto a la tarea " + otra);
        } else if (p[0].equals("traer") && p.length == 2 && p[1].matches("[A-Za-z0-9_.]+")) {
            if (!traer(p[1])) return null;   // sin tarea de esa app: el launcher la abre como siempre
        } else if (p[0].equals("dormir") && p.length == 1) {
            sh("input keyevent 223");   // KEYCODE_SLEEP
            Log.i(TAG, "dormir");
        } else if (p[0].equals("tareas") && p.length == 1) {
            return " " + tareas();
        } else if (p[0].equals("minis") && (p.length == 1 || p.length == 2 && p[1].matches("[A-Za-z0-9_.,]+"))) {
            // "minis [paquetes que el launcher ya tiene en disco]": responde " paquete:jpeg-base64 ..." con las nuevas
            return minis(tareas(), p.length == 2 ? java.util.Arrays.asList(p[1].split(",")) : java.util.Collections.emptyList());
        } else if (p[0].equals("cerrar") && p.length == 2 && p[1].matches("[A-Za-z0-9_.]+|\\*")) {
            cerrar(p[1]);
        } else if (p[0].equals("ajuste") && p.length == 3 && p[1].matches("wifi|bt|avion|ahorro") && p[2].matches("on|off")) {
            ajuste(p[1], p[2]);
        } else if (p[0].equals("asa") && p.length == 4 && p[1].matches("[din]") && p[2].matches("[0-9]{1,5}") && p[3].matches("[0-9]{1,5}")) {
            // Panel Edge del launcher: dónde está su asa (d = borde derecho, i = izquierdo, n = sin asa). Al visor por 4449.
            synchronized (cerrojo) { asa = p[1].equals("n") ? "off" : p[1] + "," + p[2] + "," + p[3]; encolar("asa", asa); }   // formato de Enlace.cs
            // Guardada: si el agente se reinicia, la reenvía al conectar el visor (revisión Gemini/Kimi 17/11).
            try { java.nio.file.Files.write(java.nio.file.Paths.get(ASA), asa.getBytes(StandardCharsets.US_ASCII)); } catch (IOException e) { Log.w(TAG, "asa: " + e); }
        } else if (p[0].equals("isla") && (p.length == 2 && p[1].equals("off") || p.length == 5 && (p[1] + p[2] + p[3] + p[4]).matches("[0-9]{4,20}"))) {
            // Isla del launcher: dónde tomar sus toques (u "off"). Al visor por 4449, formato de Enlace.cs.
            synchronized (cerrojo) { isla = p.length == 2 ? "off" : p[1] + "," + p[2] + "," + p[3] + "," + p[4]; encolar("isla", isla); }
        } else if (p[0].equals("desinstalar") && p.length == 2 && p[1].matches("[A-Za-z0-9_.]+")) {
            // Solo apps instaladas por el usuario (pm list packages -3): una del sistema no se toca aunque la pida el launcher.
            if (!java.util.Arrays.asList(sh("pm list packages -3 " + p[1]).split("\\s+")).contains("package:" + p[1])) return null;
            String res = sh("pm uninstall " + p[1], 60).trim();
            Log.i(TAG, "desinstalar " + p[1] + ": " + res);
            if (!res.startsWith("Success")) return null;
        } else if (p[0].equals("volver") && p.length == 2 && p[1].matches("windows|apagar")) {
            // Suspender/Apagar del launcher cuando su conexión directa con el visor (4445) está caída: por la sesión 4449.
            synchronized (cerrojo) { if (!sincronizado) return null; encolar("volver", p[1]); }
        } else if (p[0].equals("ventanas") && p.length == 1) {
            return " " + (barraVista.isEmpty() ? "visible" : barraVista) + " " + (asaVista.isEmpty() ? "no" : asaVista);   // la isla al arrancar
        } else if (p[0].equals("tema") && p.length == 2 && p[1].matches("oscuro|claro")) {
            sh("cmd uimode night " + (p[1].equals("oscuro") ? "yes" : "no"));   // tema de Android (lo pide Ajustes de inicio)
            Log.i(TAG, "tema " + p[1]);
        } else if (p[0].equals("lectura") && p.length == 2 && p[1].matches("on|off")) {
            lectura(p[1].equals("on"));
        } else if (p[0].equals("estado") && p.length == 1) {
            String lec = lectura() ? "on" : "off";
            synchronized (cerrojo) { return " bt=" + btWindows + " wifi=" + wifiWindows + " lectura=" + lec; }
        } else {
            return null;
        }
        return "";
    }

    /** Orden de panel al launcher (OrdenPanel) con IActivityManager.broadcastIntentWithFeature: 3 enteros = resultCode, appOp, userId. */
    static boolean panel(String cual) { return alLauncher("cual", cual); }

    /** Aviso a OrdenPanel del launcher (extra clave=valor) por binder; su permiso DUMP solo lo tienen root y shell. */
    static boolean alLauncher(String clave, String valor) {
        try {
            android.content.Intent i = new android.content.Intent("com.surfacedual.launcher.PANEL")
                    .setClassName("com.surfacedual.launcher", "com.surfacedual.launcher.OrdenPanel").putExtra(clave, valor)
                    .addFlags(android.content.Intent.FLAG_RECEIVER_FOREGROUND);
            Object am = Class.forName("android.app.ActivityManager").getMethod("getService").invoke(null);
            for (java.lang.reflect.Method m : am.getClass().getMethods()) {
                if (!m.getName().equals("broadcastIntentWithFeature")) continue;
                Class<?>[] t = m.getParameterTypes();
                Object[] v = new Object[t.length];
                int[] enteros = { 0, -1, 0 };   // resultCode, appOp (ninguno), userId 0
                int e = 0;
                for (int k = 0; k < t.length; k++) {
                    if (t[k] == android.content.Intent.class) v[k] = i;
                    else if (t[k] == int.class) { if (e == 3) return false; v[k] = enteros[e++]; }
                    else if (t[k] == boolean.class) v[k] = false;
                }
                if (e != 3) return false;   // otra firma: que lo haga cmd
                m.invoke(am, v);
                return true;
            }
        } catch (Throwable e) { Log.w(TAG, "al launcher " + clave + ": " + e); }
        return false;
    }

    // Micrófono y cámara en uso (como el punto verde/naranja de Android): IAppOpsService.startWatchingActive desde root;
    // cada cambio se comprueba con isOperationActive (no depende del orden de los campos del aviso) y va a la isla.
    // Isla (17/11): la barra de estado oculta (vídeo a pantalla completa) y el asa de ventanas de Android (arriba en el centro
    // de las apps) no se ven desde la ventana superpuesta del launcher. SurfaceFlinger avisa de cada cambio de ventanas
    // (WindowInfosListenerForTest, sin sondeo; uid 0 tiene ACCESS_SURFACE_FLINGER): solo se manda al launcher lo que cambia.
    static volatile String barraVista = "", asaVista = "";
    static Object ventanasOyente;
    static java.lang.reflect.Field[] campos;   // de WindowInfo: displayId, isVisible, name, bounds
    // Rotación real (IRotationWatcher, por aviso): la que el visor usa para girar sus zonas de gestos.
    static void oirGiro() throws Exception {
        Object wm = servicio("window", "android.view.IWindowManager");
        Object cb = oyente("android.view.IRotationWatcher", d -> { synchronized (cerrojo) { giro = "" + d.readInt(); encolar("giro", giro); } });
        int r = (int) wm.getClass().getMethod("watchRotation", Class.forName("android.view.IRotationWatcher"), int.class).invoke(wm, cb, 0);
        synchronized (cerrojo) { giro = "" + r; encolar("giro", giro); }
    }

    // Bloqueado: cada 30 s mira si suena algo; dos veces seguidas en silencio = "reposo" (el visor pausa la VM si sigue bloqueado).
    static Thread reposo;
    static synchronized void vigilarReposo() {
        if (reposo != null && reposo.isAlive()) return;
        reposo = new Thread(() -> {
            int mudo = 0;
            try {
                java.lang.reflect.Method activo = Class.forName("android.media.AudioSystem").getMethod("isStreamActive", int.class, int.class);
                while (bloqueo) {
                    Thread.sleep(30000);
                    mudo = (Boolean) activo.invoke(null, 3 /* STREAM_MUSIC */, 0) ? 0 : mudo + 1;
                    if (mudo >= 2 && bloqueo) { synchronized (cerrojo) { encolar("reposo", "1"); } return; }
                }
            } catch (Throwable e) { Log.w(TAG, "reposo: " + e); }
        });
        reposo.setDaemon(true);
        reposo.start();
    }

    static void oirVentanas() throws Exception {
        Class<?> c = Class.forName("android.window.WindowInfosListenerForTest");
        java.util.function.Consumer<java.util.List<?>> alCambiar = lista -> {
            try {
                boolean barra = false;
                String asa = "no";
                for (Object w : lista) {
                    if (campos == null) { Class<?> k = w.getClass(); campos = new java.lang.reflect.Field[] { k.getField("displayId"), k.getField("isVisible"), k.getField("name"), k.getField("bounds") }; }
                    if (campos[0].getInt(w) != 0) continue;
                    String n = String.valueOf(campos[2].get(w));
                    android.graphics.Rect r = (android.graphics.Rect) campos[3].get(w);
                    if (r == null) continue;
                    // Oculta (pantalla completa) la barra sube fuera de la pantalla: [0,-48][2880,0] y NOT_VISIBLE (medido 17/11)
                    if (n.endsWith(" StatusBar")) { barra |= campos[1].getBoolean(w) && r.bottom > 0; continue; }
                    if (!campos[1].getBoolean(w)) continue;
                    // El asa de WM Shell: ventana pequeña arriba en el centro (nombre medido con dumpsys input en la prueba)
                    else if (n.toLowerCase(java.util.Locale.ROOT).matches(".*(caption|app.?handle).*") && r.top < 100 && r.width() < 800) asa = r.left + "," + r.right;
                }
                String b = barra ? "visible" : "oculta";
                if (!b.equals(barraVista)) { barraVista = b; alLauncher("isla", "barra " + b); Log.i(TAG, "barra de estado " + b); }
                if (!asa.equals(asaVista)) { asaVista = asa; alLauncher("isla", "manija " + asa); Log.i(TAG, "asa de Android " + asa); }
            } catch (Throwable e) { Log.w(TAG, "ventanas: " + e); }   // una excepción aquí, en un hilo de binder, tiraba el agente
        };
        ventanasOyente = c.getConstructor().newInstance();   // guardado: sin referencia el recolector lo borraba y dejaban de llegar avisos (medido 17/11)
        c.getMethod("addWindowInfosListener", java.util.function.Consumer.class).invoke(ventanasOyente, alCambiar);
        Log.i(TAG, "oyendo ventanas (barra de estado y asa de Android)");
    }

    static void oirPrivacidad() throws Exception {
        Object ops = servicio("appops", "com.android.internal.app.IAppOpsService");
        java.lang.reflect.Method activa = ops.getClass().getMethod("isOperationActive", int.class, int.class, String.class);
        Object cb = oyente("com.android.internal.app.IAppOpsActiveCallback", d -> {
            try {
                int op = d.readInt(), uid = d.readInt();
                String pkg = d.readString();
                if (pkg == null || !pkg.matches("[A-Za-z0-9_.]+")) return;
                boolean si = (Boolean) activa.invoke(ops, op, uid, pkg);
                alLauncher("isla", (op == 26 ? "cam " : "mic ") + pkg + (si ? " 1" : " 0"));
            } catch (Exception e) { Log.w(TAG, "privacidad: " + e); }
        });
        Class<?> ci = Class.forName("com.android.internal.app.IAppOpsActiveCallback");
        ops.getClass().getMethod("startWatchingActive", int[].class, ci).invoke(ops, new int[] { 26 /* OP_CAMERA */, 27 /* OP_RECORD_AUDIO */ }, cb);
        Log.i(TAG, "privacidad: oyendo micrófono y cámara");
    }

    /** Pulsa y suelta una tecla inyectándola en el sistema desde este proceso root (sin arrancar "input": instantáneo). */
    static boolean tecla(int codigo) {
        try {
            Object im = Class.forName("android.hardware.input.InputManagerGlobal").getMethod("getInstance").invoke(null);
            java.lang.reflect.Method inyectar = im.getClass().getMethod("injectInputEvent", android.view.InputEvent.class, int.class);
            long t = android.os.SystemClock.uptimeMillis();
            for (int a : new int[]{android.view.KeyEvent.ACTION_DOWN, android.view.KeyEvent.ACTION_UP}) {
                android.view.KeyEvent k = new android.view.KeyEvent(t, t, a, codigo, 0, 0, android.view.KeyCharacterMap.VIRTUAL_KEYBOARD, 0, 0,
                        android.view.InputDevice.SOURCE_KEYBOARD);
                if (!(Boolean) inyectar.invoke(im, k, 0)) return false;   // 0 = INJECT_INPUT_EVENT_MODE_ASYNC
            }
            return true;
        } catch (Throwable e) { Log.w(TAG, "tecla " + codigo + ": " + e); return false; }
    }

    // Ajustes rápidos del launcher que una app normal no puede mover. Wi-Fi y Bluetooth son los de Windows (la base):
    // el toque del usuario se le manda tal cual (cmd wifi queda como com.android.shell y wifiLlamador no lo reenviaría).
    static void ajuste(String que, String v) throws Exception {
        boolean on = v.equals("on");
        if (que.equals("wifi")) {
            if (!v.equals(wifiEstado())) sh("cmd wifi set-wifi-enabled " + (on ? "enabled" : "disabled"));
            synchronized (cerrojo) { if (!v.equals(wifiWindows)) { wifiWindows = v; encolar("wifi", v); } }
        } else if (que.equals("bt")) {
            synchronized (cerrojo) { btWindows = v; encolar("bt", v); }
        } else if (que.equals("avion")) {
            sh("cmd connectivity airplane-mode " + (on ? "enable" : "disable"));
        } else {
            sh("cmd power set-mode " + (on ? 1 : 0));   // ahorro de batería
            // Android -> Windows (punto 8). shortcut: solo desde estos ajustes rápidos; el de la app Ajustes no avisa (sin
            // proceso de app, AMS rechaza el receptor y ContentService el observador: medido 17/11). Mejora: avisar desde el launcher.
            synchronized (cerrojo) { if (!v.equals(ahorroWindows)) { ahorroWindows = v; encolar("ahorro", v); } }
        }
        Log.i(TAG, "ajuste " + que + " " + v);
    }

    // Modo lectura (ESTADO 116, F7/plan/lectura_diseno.md). El estado previo se escribe ANTES de aplicar; al quitarlo, o al
    // arrancar el agente si murió con el modo puesto, se restaura, se comprueba y solo entonces se borra (si no, se reintenta).
    // Wi-Fi no se toca: Mihon baja las páginas.
    static final String LECTURA = "/data/local/tmp/surfacedual_lectura";
    static final Object cerrojoLectura = new Object();
    // Una línea por valor: peak_refresh_rate, 3 escalas de animación, low_power, ubicación, Mihon y launcher ya exentos de Doze, Doze forzado.
    static final String LEER_LECTURA = "settings get system peak_refresh_rate; for k in window_animation_scale transition_animation_scale animator_duration_scale low_power; "
            + "do settings get global $k; done; cmd location is-location-enabled; w=$(cmd deviceidle whitelist); "
            + "for p in app.mihon com.surfacedual.launcher; do case \"$w\" in *,$p,*) echo si;; *) echo no;; esac; done; dumpsys deviceidle | grep -c mForceIdle=true";
    static final String NUM = "null|[0-9]{1,3}(\\.[0-9]{1,6})?";
    static final String[] PATRON = {NUM, NUM, NUM, NUM, "null|[01]", "true|false", "si|no", "si|no", "[01]"};
    static final String[] DEFECTO = {"null", "null", "null", "null", "0", "true", "si", "si", "0"};   // valor dañado: el de fábrica, sin quitar exenciones

    static boolean lectura() { return new java.io.File(LECTURA).exists(); }

    static String[] leerLectura() throws Exception {
        String[] v = sh(LEER_LECTURA).trim().split("\n");
        if (v.length != PATRON.length) throw new IOException("lectura: estado ilegible: " + String.join("|", v));
        for (int i = 0; i < v.length; i++) v[i] = v[i].trim();
        return v;
    }

    static void lectura(boolean on) throws Exception {
        synchronized (cerrojoLectura) {
            java.nio.file.Path f = java.nio.file.Paths.get(LECTURA);
            if (on) {
                if (!lectura()) {
                    // /data/local/tmp lo escribe shell: CREATE_NEW no sigue un enlace puesto ahí. Sin renombrar: la política SELinux del
                    // agente no da "rename" (medido 18/11); son ~40 bytes de una escritura y un archivo a medias se lee con valores de fábrica.
                    java.nio.file.Files.write(f, (String.join("\n", leerLectura()) + "\n").getBytes(StandardCharsets.US_ASCII), java.nio.file.StandardOpenOption.CREATE_NEW);
                }
                // 60 Hz: SurfaceFlinger pinta la mitad en cada cambio de página (medido 18/11: 183 cuadros contra 348 en 3 s).
                // Doze forzado con la pantalla encendida: apps de fondo sin red, trabajos (la sincronización incluida) ni alarmas;
                // el lector en primer plano no se toca. Exentos: Mihon (descargas) y el launcher (el visor le habla por TCP).
                try {   // si algo falla a medias, se deshace lo puesto (el respaldo ya está escrito)
                sh("settings put system peak_refresh_rate 60; for k in window_animation_scale transition_animation_scale animator_duration_scale; do settings put global $k 0; done; "
                        + "cmd location set-location-enabled false; cmd deviceidle whitelist +app.mihon +com.surfacedual.launcher; dumpsys deviceidle force-idle");
                ajuste("ahorro", "on");   // ahorro de Android y de Windows (Windows lo restaura solo al ocultar Android)
                // El botón dice la verdad: si algo no quedó puesto, se deshace todo y el launcher recibe el fallo.
                String[] a = leerLectura();
                if (!(a[0].equals("60") && a[1].equals("0") && a[2].equals("0") && a[3].equals("0") && a[5].equals("false") && a[8].equals("1"))) {
                    lectura(false);
                    throw new IOException("lectura: no se aplicó (" + String.join("|", a) + ")");
                }
                } catch (Exception x) { if (lectura()) try { lectura(false); } catch (Exception y) { Log.w(TAG, "lectura: deshacer", y); } throw x; }
                synchronized (cerrojo) { encolar("lectura", "on"); }
            } else if (lectura()) {
                // Archivo que shell puede cambiar: solo uno normal y pequeño (sin enlace); si no, valores de fábrica. Cada valor, validado.
                boolean sano = java.nio.file.Files.isRegularFile(f, java.nio.file.LinkOption.NOFOLLOW_LINKS) && java.nio.file.Files.size(f) < 512;
                String[] v = sano ? new String(java.nio.file.Files.readAllBytes(f), StandardCharsets.US_ASCII).split("\n") : new String[0];
                String[] e = new String[PATRON.length];
                for (int i = 0; i < e.length; i++) e[i] = i < v.length && v[i].trim().matches(PATRON[i]) ? v[i].trim() : DEFECTO[i];
                String[] k = {"system peak_refresh_rate", "global window_animation_scale", "global transition_animation_scale", "global animator_duration_scale"};
                StringBuilder o = new StringBuilder((e[8].equals("0") ? "dumpsys deviceidle unforce" : "true") + "; cmd location set-location-enabled " + e[5]);
                for (int i = 0; i < k.length; i++) o.append(e[i].equals("null") ? "; settings delete " + k[i] : "; settings put " + k[i] + " " + e[i]);
                if (e[6].equals("no")) o.append("; cmd deviceidle whitelist -app.mihon");
                if (e[7].equals("no")) o.append("; cmd deviceidle whitelist -com.surfacedual.launcher");
                sh(o.toString());
                ajuste("ahorro", e[4].equals("1") ? "on" : "off");
                String[] ahora = leerLectura();   // el ahorro no se compara: con cargador Android lo ignora (medido 18/11)
                for (int i : new int[]{0, 1, 2, 3, 5, 6, 7, 8})
                    if (!ahora[i].equals(e[i])) throw new IOException("lectura: el valor " + i + " no volvió (" + ahora[i] + " en vez de " + e[i] + ")");
                java.nio.file.Files.delete(f);
            }
            if (!on) synchronized (cerrojo) { encolar("lectura", "off"); }   // también si no había archivo: el visor vuelve a lo normal
        }
        Log.i(TAG, "lectura " + (on ? "on" : "off"));
    }

    static Object atm() throws Exception { return Class.forName("android.app.ActivityTaskManager").getMethod("getService").invoke(null); }

    static java.util.List<?> recientes(Object atm) throws Exception {
        Object lista = atm.getClass().getMethod("getRecentTasks", int.class, int.class, int.class).invoke(atm, 100, 0, 0);
        return (java.util.List<?>) lista.getClass().getMethod("getList").invoke(lista);
    }

    static String paquete(Object o) {
        android.app.ActivityManager.RecentTaskInfo t = (android.app.ActivityManager.RecentTaskInfo) o;
        android.content.ComponentName c = t.baseIntent == null ? null : t.baseIntent.getComponent();
        if (c == null) c = t.baseActivity;   // intent sin componente: la actividad raíz de la tarea
        return c == null ? null : c.getPackageName();
    }

    // Recientes propios del launcher (0.e): "tarea:paquete,..." de la más reciente a la más vieja, una por app.
    static String tareas() throws Exception {
        Object atm = atm();
        StringBuilder r = new StringBuilder();
        java.util.HashSet<String> vistas = new java.util.HashSet<>();
        for (Object o : recientes(atm)) {
            String pkg = paquete(o);
            if (pkg == null || pkg.equals("com.surfacedual.launcher") || !vistas.add(pkg)) continue;
            int id = ((android.app.ActivityManager.RecentTaskInfo) o).taskId;
            r.append(r.length() == 0 ? "" : ",").append(id).append(':').append(pkg);
            if (vistas.size() == 12) break;
        }
        return r.toString();
    }

    // "minis": las miniaturas de esas tareas, aparte de "tareas" para que las tarjetas salgan al instante (17/11).
    // Un solo plazo de 4 s para todo el lote: la cola del launcher (traer, cerrar, asa...) no espera más aunque el ayudante
    // se cuelgue (revisión Kimi/Gemini 17/11: antes hasta 3 s por tarea, 36 s con 12).
    static String minis(String lista, java.util.List<String> tiene) {
        long hasta = android.os.SystemClock.uptimeMillis() + 4000;
        StringBuilder r = new StringBuilder();
        for (String t : lista.split(",")) {
            int k = t.indexOf(':');
            long queda = hasta - android.os.SystemClock.uptimeMillis();
            if (k <= 0) continue;
            if (queda <= 0) break;
            String pkg = t.substring(k + 1);
            try {
                String b = miniatura(Integer.parseInt(t.substring(0, k)), pkg, tiene.contains(pkg), queda);
                if (b != null) r.append(' ').append(pkg).append(':').append(b);
            } catch (Exception e) { Log.w(TAG, "miniatura " + pkg + ": " + e); }
        }
        return r.toString();
    }

    // La imagen la hace el ayudante (uid shell): HWUI en el proceso root no arranca (Choreographer, EPERM de binder) y
    // abortaba el agente (17/11). Va al launcher por el socket; él la guarda en sus archivos.
    static final java.util.Map<String, Long> capturasEnviadas = new java.util.concurrent.ConcurrentHashMap<>();
    static final java.util.Map<Integer, java.util.concurrent.CompletableFuture<String[]>> minis = new java.util.concurrent.ConcurrentHashMap<>();
    static String miniatura(int id, String pkg, boolean laTiene, long msMax) throws Exception {
        // Misma captura que la última enviada y el launcher aún la tiene: nada (revisión 16/11: antes 12 JPEG en cada apertura).
        long conocida = laTiene ? capturasEnviadas.getOrDefault(pkg, 0L) : 0;
        java.util.concurrent.CompletableFuture<String[]> f = new java.util.concurrent.CompletableFuture<>();
        minis.put(id, f);
        String[] r;
        try { alAyudante("mini " + id + " " + conocida); r = f.get(Math.min(3000, msMax), java.util.concurrent.TimeUnit.MILLISECONDS); }   // "mini id hora jpeg|=|-"
        finally { minis.remove(id); }
        if (r[3].equals("-") || r[3].equals("=")) return null;
        capturasEnviadas.put(pkg, Long.parseLong(r[2]));
        return r[3];
    }

    // En el ayudante. La última imagen que guardó WindowManager de la tarea; la de delante aún no tiene: se toma ahora (medido 17/11).
    static String capturar(int id, long conocida) throws Exception {
        // Android 17: ITaskSnapshotManager.getTaskSnapshot(tarea, capturada después de (0 = cualquiera), resolución: 1 alta,
        // 2 baja (la mitad), 3 cualquiera; 0 lanza excepción). getHardwareBuffer() devuelve null fuera de system_server: mSnapshot.
        Object sm = atm().getClass().getMethod("getTaskSnapshotManager").invoke(atm());
        // 3 = cualquiera: las tareas de antes de reiniciar solo tienen la guardada en disco (con 2 no salía nada: Play Store, 17/11)
        Object s = sm.getClass().getMethod("getTaskSnapshot", int.class, long.class, int.class).invoke(sm, id, 0L, 3);
        if (s == null) s = sm.getClass().getMethod("takeTaskSnapshot", int.class, boolean.class, boolean.class, boolean.class).invoke(sm, id, false, false, false);
        if (s == null) return "0 -";
        long hora = 0;
        try { java.lang.reflect.Field fh = s.getClass().getDeclaredField("mCaptureTime"); fh.setAccessible(true); hora = fh.getLong(s); }
        catch (Exception e) { }
        if (hora != 0 && hora == conocida) return hora + " =";
        java.lang.reflect.Field f = s.getClass().getDeclaredField("mSnapshot");
        f.setAccessible(true);
        android.hardware.HardwareBuffer hb = (android.hardware.HardwareBuffer) f.get(s);
        android.graphics.Bitmap b = null, sw = null, chica = null;
        try {   // todo se libera aunque falle a medias (revisión Kimi 17/11: memoria nativa del ayudante)
            b = hb == null ? null : android.graphics.Bitmap.wrapHardwareBuffer(hb, null);
            if (b == null) return "0 -";
            sw = b.copy(android.graphics.Bitmap.Config.ARGB_8888, false);
            chica = android.graphics.Bitmap.createScaledBitmap(sw, 864, 864 * sw.getHeight() / sw.getWidth(), true);   // ~ancho de la tarjeta
            java.io.ByteArrayOutputStream o = new java.io.ByteArrayOutputStream();
            chica.compress(android.graphics.Bitmap.CompressFormat.JPEG, 85, o);
            return hora + " " + java.util.Base64.getEncoder().encodeToString(o.toByteArray());
        } finally {
            if (chica != null && chica != sw) chica.recycle();
            if (sw != null) sw.recycle();
            if (b != null) b.recycle();
            if (hb != null) hb.close();
        }
    }

    // Quita de Recientes todas las tareas de la app ("*": todas menos el launcher).
    static void cerrar(String pkg) throws Exception {
        Object atm = atm();
        java.lang.reflect.Method quitar = atm.getClass().getMethod("removeTask", int.class);
        for (Object o : recientes(atm)) {
            String p = paquete(o);
            if (p == null || p.equals("com.surfacedual.launcher") || !(pkg.equals("*") || pkg.equals(p))) continue;
            quitar.invoke(atm, ((android.app.ActivityManager.RecentTaskInfo) o).taskId);
        }
        Log.i(TAG, "cerrar " + pkg);
    }

    // Una tarjeta por app (0.b) y sin el trampolín de Archivos, que crea otra tarea y se ve negro (0.a, medido 16/11): si la
    // app ya tiene tarea (la más reciente primero), se trae al frente como desde Recientes.
    // Las demás tareas de la app se quitan de Recientes (pedido 16/11: una tarjeta por app); sin tarea viva se quitan todas
    // las muertas y el launcher abre una nueva.
    static boolean traer(String pkg) throws Exception {
        Object atm = atm();
        java.lang.reflect.Method quitar = atm.getClass().getMethod("removeTask", int.class);
        int elegida = -1;
        for (Object o : recientes(atm)) {
            android.app.ActivityManager.RecentTaskInfo t = (android.app.ActivityManager.RecentTaskInfo) o;
            if (!pkg.equals(paquete(o))) continue;
            // solo tareas vivas: las de antes de reiniciar (Archivos) "se abren" con éxito pero la app las cierra sola (medido 16/11)
            if (elegida < 0 && t.isRunning) elegida = t.taskId;
            else quitar.invoke(atm, t.taskId);
        }
        if (elegida < 0) return false;
        int r = (int) atm.getClass().getMethod("startActivityFromRecents", int.class, android.os.Bundle.class).invoke(atm, elegida, null);
        Log.i(TAG, "traer " + pkg + " tarea " + elegida + " -> " + r);
        return r >= 0;
    }

    // Solo con órdenes armadas aquí (valores validados antes). Salida a un archivo (un tubo lleno colgaría al hijo) y
    // tope de 15 s: un am/dumpsys atascado no bloquea al agente.
    static String sh(String orden) throws Exception { return sh(orden, 15); }
    static String sh(String orden, int segundos) throws Exception {
        java.io.File salida = java.io.File.createTempFile("agente", ".txt", new java.io.File("/data/local/tmp"));
        try {
            Process p = new ProcessBuilder("/system/bin/sh", "-c", orden).redirectErrorStream(true).redirectOutput(salida).start();
            if (!p.waitFor(segundos, java.util.concurrent.TimeUnit.SECONDS)) { p.destroyForcibly(); throw new IOException("sin respuesta en " + segundos + " s: " + orden.split(" ")[0]); }
            return new String(java.nio.file.Files.readAllBytes(salida.toPath()), StandardCharsets.UTF_8);
        } finally { salida.delete(); }
    }

    interface Aviso { void en(Parcel datos); }

    // Oyente binder sin la clase Stub oculta: atiende la transacción 1 (el único método de estos AIDL oneway).
    static Object oyente(String interfaz, Aviso a) throws Exception {
        return Class.forName(interfaz + "$Stub").getMethod("asInterface", IBinder.class).invoke(null, new Binder() {
            @Override protected boolean onTransact(int c, Parcel d, Parcel r, int f) {
                if (c == 1) { d.enforceInterface(interfaz); a.en(d); }
                return true;
            }
        });
    }

    static Object servicio(String nombre, String interfaz) throws Exception {
        Object b = Class.forName("android.os.ServiceManager").getMethod("getService", String.class).invoke(null, nombre);
        return Class.forName(interfaz + "$Stub").getMethod("asInterface", IBinder.class).invoke(null, b);
    }
}
