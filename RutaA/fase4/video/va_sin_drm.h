// Pantalla VA sin DRM: en la VM no hay /dev/dri (d3d12 va por /dev/dxg), así que vaGetDisplayDRM no sirve (exige un
// nodo DRM). Se arma el contexto a mano con el driver "d3d12"; Mesa (parche en compilar_android.sh) cae al winsys nulo.
#pragma once
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>

#ifdef __cplusplus
extern "C" {
#endif
VADisplayContextP va_newDisplayContext(void);                 // exportadas por libva.so (va_internal.h no se instala)
VADriverContextP va_newDriverContext(VADisplayContextP dctx);
#ifdef __cplusplus
}
#endif

static VAStatus sd_nombres(VADisplayContextP dc, char **drivers, unsigned *n) {
    (void)dc;
    if (*n < 1) return VA_STATUS_ERROR_INVALID_PARAMETER;
    drivers[0] = strdup("d3d12"); *n = 1;
    return VA_STATUS_SUCCESS;
}
static void sd_destruir(VADisplayContextP dc) { (void)dc; }   // la pantalla vive lo que el proceso

// Abre e inicializa la pantalla VA del proceso. dri = carpeta de d3d12_drv_video.so. NULL si falla.
static VADisplay sd_va_abrir(const char *dri) {
    setenv("LIBVA_DRIVERS_PATH", dri, 1);
    setenv("GALLIUM_DRIVER", "d3d12", 1);
    VADisplayContextP dc = va_newDisplayContext();
    VADriverContextP ctx = dc ? va_newDriverContext(dc) : NULL;
    struct drm_state *st = (struct drm_state *)calloc(1, sizeof *st);
    if (!ctx || !st) return NULL;
    st->fd = open("/dev/null", O_RDWR | O_CLOEXEC);   // Mesa lo prueba como DRM, falla y usa el winsys nulo
    dc->vaGetDriverNames = sd_nombres;
    dc->vaDestroy = sd_destruir;
    ctx->native_dpy = NULL;
    ctx->display_type = VA_DISPLAY_DRM;
    ctx->drm_state = st;
    int ma, mi;
    return vaInitialize((VADisplay)dc, &ma, &mi) == VA_STATUS_SUCCESS ? (VADisplay)dc : NULL;
}
