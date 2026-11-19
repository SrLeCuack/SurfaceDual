package com.surfacedual.feed;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.net.Uri;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import java.util.ArrayList;
import java.util.List;

/**
 * Reenvía tal cual las transacciones ILauncherOverlay del launcher a la app de Google (que ve a esta app depurable como cliente).
 * Los binders de dentro (el retorno del launcher) viajan como referencias: Google le contesta directo al launcher.
 */
public class Puente extends Service {
    static final String TAG = "puente_discover", GSA = "com.google.android.googlequicksearchbox";
    IBinder google;
    final List<Object[]> pendientes = new ArrayList<>();   // llamadas que llegan antes de conectar con Google

    final ServiceConnection conexion = new ServiceConnection() {
        @Override public void onServiceConnected(ComponentName n, IBinder b) {
            synchronized (pendientes) {
                google = b;
                for (Object[] p : pendientes) enviar((int) p[0], (Parcel) p[1], (int) p[2]);
                pendientes.clear();
            }
            Log.i(TAG, "conectado con Google");
        }
        @Override public void onServiceDisconnected(ComponentName n) { synchronized (pendientes) { google = null; } }
        @Override public void onNullBinding(ComponentName n) { Log.w(TAG, "Google rechazó el puente"); }
    };

    final Binder reenvio = new Binder() {
        @Override protected boolean onTransact(int codigo, Parcel d, Parcel r, int flags) throws RemoteException {
            if (codigo == INTERFACE_TRANSACTION) return super.onTransact(codigo, d, r, flags);
            synchronized (pendientes) {
                if (google == null) {
                    if ((flags & FLAG_ONEWAY) == 0) return false;   // con respuesta: no se puede esperar a Google
                    Parcel copia = Parcel.obtain();
                    copia.appendFrom(d, 0, d.dataSize());
                    pendientes.add(new Object[]{codigo, copia, flags});
                    return true;
                }
            }
            d.setDataPosition(0);
            return google.transact(codigo, d, r, flags);
        }
    };

    void enviar(int codigo, Parcel d, int flags) {
        try { d.setDataPosition(0); google.transact(codigo, d, null, flags); }
        catch (RemoteException e) { Log.w(TAG, "reenvío " + codigo, e); }
        finally { d.recycle(); }
    }

    @Override public IBinder onBind(Intent i) { enlazar(i); return reenvio; }

    // Android guarda el binder de onBind: al volver a enlazarse el launcher sin que muera este proceso solo llama a onRebind
    // (y solo si onUnbind devolvió true). Sin esto Google quedaba desconectado para siempre (revisión Gemini 17/11).
    @Override public void onRebind(Intent i) { enlazar(i); }

    void enlazar(Intent i) {
        String v = i.getData() == null ? null : i.getData().getQueryParameter("v"), cv = i.getData() == null ? null : i.getData().getQueryParameter("cv");
        Uri uri = Uri.parse("app://" + getPackageName() + ":" + Process.myUid()).buildUpon()
                .appendQueryParameter("v", v == null ? "7" : v).appendQueryParameter("cv", cv == null ? "9" : cv).build();
        boolean ok = bindService(new Intent("com.android.launcher3.WINDOW_OVERLAY").setPackage(GSA).setData(uri), conexion,
                BIND_AUTO_CREATE | BIND_IMPORTANT);
        Log.i(TAG, "enlazando con Google: " + ok);
    }

    @Override public boolean onUnbind(Intent i) {
        try { unbindService(conexion); } catch (IllegalArgumentException ignored) { }
        synchronized (pendientes) { google = null; for (Object[] p : pendientes) ((Parcel) p[1]).recycle(); pendientes.clear(); }
        return true;
    }
}
