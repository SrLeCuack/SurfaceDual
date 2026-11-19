// Codec2 de SurfaceDual (servicio android.hardware.media.c2 "default" del vendor): decodifica H.264, HEVC y VP9
// de 8 bits en la GPU DENTRO de la VM (AV1 no: el UMD de Qualcomm cae en DecodeAV1Frame; sigue en c2.android.av1). FFmpeg separa el flujo y lleva el DPB; VA-API (Mesa d3d12, RutaA/fase4/video)
// manda cada cuadro a la cola VIDEO_DECODE de D3D12 por /dev/dxg. Salida:
//  - a una Surface (pool IGBA/BufferQueue): el procesador de vídeo de la GPU (VA VPP) convierte NV12 -> RGBA directo en
//    el búfer de gralloc (textura D3D12 compartida, T_D3D12_GPU), sin copias por CPU;
//  - a ByteBuffer (pool GRALLOC): se baja el NV12 a CPU y se copia al YUV que pida el cliente (lento: miniaturas).
// Si la GPU no puede con el flujo (perfil sin soporte), FFmpeg decodifica por software en este mismo proceso.
// Compila con el libc++ de la plataforma (std::__1): enlaza libcodec2_vndk/libcodec2_aidl_V2 del vendor (compilar.sh).
#define LOG_TAG "c2.surfacedual"
#include <log/log.h>
#include <cutils/properties.h>

#include <C2Config.h>
#include <C2PlatformSupport.h>
#include <SimpleC2Component.h>
#include <SimpleC2Interface.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <codec2/aidl/ComponentStore.h>
#include <media/stagefright/foundation/MediaDefs.h>
#include <system/graphics.h>

#include <map>
#include <mutex>
#include <set>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <va/va_vpp.h>
#include <va/va_drmcommon.h>
}
#include "../../video/va_sin_drm.h"

using namespace android;

namespace {

#ifdef SD_DRI_PRUEBA
constexpr char kDri[] = "/vendor/lib64/wsl:/data/local/tmp";   // solo para la prueba manual (compilar.sh prueba)
#else
constexpr char kDri[] = "/vendor/lib64/wsl";   // d3d12_drv_video.so (partición sdgraficos)
#endif
// Cuadros retenidos como máximo: DPB de H.264/HEVC (16) + los hilos de FFmpeg por software (frame threading: hilos - 1).
constexpr uint32_t kMaxLado = 4096, kHilosSw = 4, kRetraso = 16 + kHilosSw - 1;

struct Codec { const char *nombre, *mime; AVCodecID id; };
const Codec kCodecs[] = {
    {"c2.surfacedual.avc.decoder", MEDIA_MIMETYPE_VIDEO_AVC, AV_CODEC_ID_H264},
    {"c2.surfacedual.hevc.decoder", MEDIA_MIMETYPE_VIDEO_HEVC, AV_CODEC_ID_HEVC},
    {"c2.surfacedual.vp9.decoder", MEDIA_MIMETYPE_VIDEO_VP9, AV_CODEC_ID_VP9},
};

// Pantalla VA y dispositivo FFmpeg del proceso (perezosos; nullptr si no hay GPU -> FFmpeg por software).
AVBufferRef *dispositivoVa() {
    static AVBufferRef *d = [] () -> AVBufferRef * {
        VADisplay dpy = sd_va_abrir(kDri);
        if (!dpy) { ALOGE("sin VA-API: se decodifica por software"); return nullptr; }
        AVBufferRef *r = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
        ((AVVAAPIDeviceContext *)((AVHWDeviceContext *)r->data)->hwctx)->display = dpy;
        if (av_hwdevice_ctx_init(r) < 0) { av_buffer_unref(&r); return nullptr; }
        ALOGI("VA: %s", vaQueryVendorString(dpy));
        return r;
    }();
    return d;
}
VADisplay pantalla() { return ((AVVAAPIDeviceContext *)((AVHWDeviceContext *)dispositivoVa()->data)->hwctx)->display; }

// ---------------------------------------------------------------------------------------------------- interfaz
class Interfaz : public SimpleInterface<void>::BaseParams {
public:
    Interfaz(const std::shared_ptr<C2ReflectorHelper> &h, const Codec &c)
        : SimpleInterface<void>::BaseParams(h, c.nombre, C2Component::KIND_DECODER, C2Component::DOMAIN_VIDEO, c.mime) {
        noPrivateBuffers(); noInputReferences(); noOutputReferences(); noInputLatency(); noTimeStretch();
        addParameter(DefineParam(mRetraso, C2_PARAMKEY_OUTPUT_DELAY)
            .withConstValue(new C2PortActualDelayTuning::output(kRetraso)).build());
        addParameter(DefineParam(mAtributos, C2_PARAMKEY_COMPONENT_ATTRIBUTES)
            .withConstValue(new C2ComponentAttributesSetting(C2Component::ATTRIB_IS_TEMPORAL)).build());
        addParameter(DefineParam(mTam, C2_PARAMKEY_PICTURE_SIZE)
            .withDefault(new C2StreamPictureSizeInfo::output(0u, 320, 240))
            .withFields({C2F(mTam, width).inRange(2, kMaxLado, 2), C2F(mTam, height).inRange(2, kMaxLado, 2)})
            .withSetter(TamSetter).build());
        addParameter(DefineParam(mTamMax, C2_PARAMKEY_MAX_PICTURE_SIZE)
            .withDefault(new C2StreamMaxPictureSizeTuning::output(0u, 320, 240))
            .withFields({C2F(mTam, width).inRange(2, kMaxLado, 2), C2F(mTam, height).inRange(2, kMaxLado, 2)})
            .withSetter(TamMaxSetter, mTam).build());
        addParameter(DefineParam(mEntradaMax, C2_PARAMKEY_INPUT_MAX_BUFFER_SIZE)
            .withDefault(new C2StreamMaxBufferSizeInfo::input(0u, 2 << 20))
            .withFields({C2F(mEntradaMax, value).any()})
            .calculatedAs(EntradaMaxSetter, mTamMax).build());
        perfiles(c.id);
        addParameter(DefineParam(mAspectosDef, C2_PARAMKEY_DEFAULT_COLOR_ASPECTS)
            .withDefault(new C2StreamColorAspectsTuning::output(0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                         C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
            .withFields({C2F(mAspectosDef, range).inRange(C2Color::RANGE_UNSPECIFIED, C2Color::RANGE_OTHER),
                         C2F(mAspectosDef, primaries).inRange(C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                         C2F(mAspectosDef, transfer).inRange(C2Color::TRANSFER_UNSPECIFIED, C2Color::TRANSFER_OTHER),
                         C2F(mAspectosDef, matrix).inRange(C2Color::MATRIX_UNSPECIFIED, C2Color::MATRIX_OTHER)})
            .withSetter(AspectosDefSetter).build());
        // Formato para ByteBuffer; con Surface se pide RGBA_8888 de GPU aparte.
        addParameter(DefineParam(mFormato, C2_PARAMKEY_PIXEL_FORMAT)
            .withConstValue(new C2StreamPixelFormatInfo::output(0u, HAL_PIXEL_FORMAT_YCBCR_420_888)).build());
    }

    static C2R TamSetter(bool, const C2P<C2StreamPictureSizeInfo::output> &viejo, C2P<C2StreamPictureSizeInfo::output> &me) {
        C2R r = C2R::Ok();
        if (!me.F(me.v.width).supportsAtAll(me.v.width)) { r = r.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width))); me.set().width = viejo.v.width; }
        if (!me.F(me.v.height).supportsAtAll(me.v.height)) { r = r.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height))); me.set().height = viejo.v.height; }
        return r;
    }
    static C2R TamMaxSetter(bool, C2P<C2StreamMaxPictureSizeTuning::output> &me, const C2P<C2StreamPictureSizeInfo::output> &tam) {
        me.set().width = c2_min(c2_max(me.v.width, tam.v.width), kMaxLado);
        me.set().height = c2_min(c2_max(me.v.height, tam.v.height), kMaxLado);
        return C2R::Ok();
    }
    static C2R EntradaMaxSetter(bool, C2P<C2StreamMaxBufferSizeInfo::input> &me, const C2P<C2StreamMaxPictureSizeTuning::output> &m) {
        me.set().value = c2_max((m.v.width + 15) / 16 * ((m.v.height + 15) / 16) * 192, 2u << 20);   // compresión >= 2
        return C2R::Ok();
    }
    static C2R PerfilSetter(bool, C2P<C2StreamProfileLevelInfo::input> &) { return C2R::Ok(); }
    static C2R AspectosDefSetter(bool, C2P<C2StreamColorAspectsTuning::output> &me) {
        if (me.v.range > C2Color::RANGE_OTHER) me.set().range = C2Color::RANGE_OTHER;
        if (me.v.primaries > C2Color::PRIMARIES_OTHER) me.set().primaries = C2Color::PRIMARIES_OTHER;
        if (me.v.transfer > C2Color::TRANSFER_OTHER) me.set().transfer = C2Color::TRANSFER_OTHER;
        if (me.v.matrix > C2Color::MATRIX_OTHER) me.set().matrix = C2Color::MATRIX_OTHER;
        return C2R::Ok();
    }

private:
    // Solo perfiles de 8 bits: los de 10 bits (salida P010) siguen yendo a c2.android.*.
    void perfiles(AVCodecID id) {
        std::vector<uint32_t> p, n; C2Config::profile_t p0; C2Config::level_t n0;
        if (id == AV_CODEC_ID_H264) {
            p = {PROFILE_AVC_CONSTRAINED_BASELINE, PROFILE_AVC_BASELINE, PROFILE_AVC_MAIN, PROFILE_AVC_CONSTRAINED_HIGH,
                 PROFILE_AVC_PROGRESSIVE_HIGH, PROFILE_AVC_HIGH};
            for (int l = LEVEL_AVC_1; l <= LEVEL_AVC_5_2; l++) n.push_back(l);
            p0 = PROFILE_AVC_HIGH; n0 = LEVEL_AVC_5_2;
        } else if (id == AV_CODEC_ID_HEVC) {
            p = {PROFILE_HEVC_MAIN, PROFILE_HEVC_MAIN_STILL};
            for (int l = LEVEL_HEVC_MAIN_1; l <= LEVEL_HEVC_HIGH_5_2; l++) n.push_back(l);
            p0 = PROFILE_HEVC_MAIN; n0 = LEVEL_HEVC_MAIN_5_1;
        } else {
            p = {PROFILE_VP9_0};
            for (int l = LEVEL_VP9_1; l <= LEVEL_VP9_5_2; l++) n.push_back(l);
            p0 = PROFILE_VP9_0; n0 = LEVEL_VP9_5;
        }
        addParameter(DefineParam(mPerfil, C2_PARAMKEY_PROFILE_LEVEL)
            .withDefault(new C2StreamProfileLevelInfo::input(0u, p0, n0))
            .withFields({C2F(mPerfil, profile).oneOf(p), C2F(mPerfil, level).oneOf(n)})
            .withSetter(PerfilSetter).build());
    }

    std::shared_ptr<C2PortActualDelayTuning::output> mRetraso;
    std::shared_ptr<C2ComponentAttributesSetting> mAtributos;
    std::shared_ptr<C2StreamPictureSizeInfo::output> mTam;
    std::shared_ptr<C2StreamMaxPictureSizeTuning::output> mTamMax;
    std::shared_ptr<C2StreamMaxBufferSizeInfo::input> mEntradaMax;
    std::shared_ptr<C2StreamProfileLevelInfo::input> mPerfil;
    std::shared_ptr<C2StreamColorAspectsTuning::output> mAspectosDef;
    std::shared_ptr<C2StreamPixelFormatInfo::output> mFormato;
};

// -------------------------------------------------------------------------------------------------- componente
class Decodificador : public SimpleC2Component {
public:
    Decodificador(c2_node_id_t id, const Codec &c, const std::shared_ptr<Interfaz> &intf)
        : SimpleC2Component(std::make_shared<SimpleInterface<Interfaz>>(c.nombre, id, intf)), mIntf(intf), mCodec(c) {}
    ~Decodificador() override { cerrar(); }

protected:
    c2_status_t onInit() override { return abrir() ? C2_OK : C2_CORRUPTED; }
    c2_status_t onStop() override { reiniciar(); return C2_OK; }
    void onReset() override { reiniciar(); }
    void onRelease() override { cerrar(); }
    c2_status_t onFlush_sm() override { reiniciar(); return C2_OK; }

    void process(const std::unique_ptr<C2Work> &work, const std::shared_ptr<C2BlockPool> &pool) override {
        work->result = C2_OK; work->workletsProcessed = 0u; work->worklets.front()->output.flags = work->input.flags;
        uint64_t indice = work->input.ordinal.frameIndex.peeku();
        bool eos = work->input.flags & C2FrameData::FLAG_END_OF_STREAM, csd = work->input.flags & C2FrameData::FLAG_CODEC_CONFIG;
        if (!mCtx && !abrir()) { work->result = C2_CORRUPTED; work->workletsProcessed = 1u; return; }
        bool enviado = false;
        if (!work->input.buffers.empty()) {   // frontera de confianza: el búfer lo manda la app
            const C2BufferData &bd = work->input.buffers[0]->data();
            if (bd.type() != C2BufferData::LINEAR || bd.linearBlocks().empty()) {
                ALOGE("entrada no lineal"); work->result = C2_BAD_VALUE; work->workletsProcessed = 1u; return;
            }
            C2ReadView v = bd.linearBlocks().front().map().get();
            if (v.error()) { ALOGE("no se pudo leer la entrada: %d", v.error()); work->result = C2_CORRUPTED; work->workletsProcessed = 1u; return; }
            const uint8_t *d = v.data(); size_t n = v.capacity();
            if (csd && mCodec.id == AV_CODEC_ID_VP9) n = 0;   // vpcC no es flujo
            if (csd && n) mCsd.insert(mCsd.end(), d, d + n);   // para reabrir sin GPU a mitad del flujo
            enviado = n && enviar(d, n, csd ? -1 : (int64_t)indice, work, pool);
            if (!csd && enviado) mPendientes.insert(indice);
        }
        recibir(work, pool);
        if (eos) vaciar(work, pool);
        if (work->workletsProcessed) {   // su propio cuadro ya salió (también si era el último)
            if (eos) work->worklets.front()->output.flags = C2FrameData::FLAG_END_OF_STREAM;
        } else if (csd || !enviado || eos) completarVacio(work);
    }

    c2_status_t drain(uint32_t modo, const std::shared_ptr<C2BlockPool> &pool) override {
        if (modo == NO_DRAIN) return C2_OK;
        if (modo == DRAIN_CHAIN) return C2_OMITTED;
        std::unique_ptr<C2Work> nada;
        vaciar(nada, pool);
        return C2_OK;
    }

private:
    bool abrir() {
        const AVCodec *c = avcodec_find_decoder(mCodec.id);
        mCtx = c ? avcodec_alloc_context3(c) : nullptr;
        if (!mCtx) return false;
        mCtx->max_pixels = kMaxLado * kMaxLado;   // un SPS enorme no llega a reservar su DPB
        // Prueba del paso a software: debug.sd.c2.fallo_gpu=N simula un fallo de la GPU tras N cuadros.
        mFalloPrueba = property_get_int32("debug.sd.c2.fallo_gpu", 0); mCuadrosGpu = 0;
        if (AVBufferRef *d = mSinGpu ? nullptr : dispositivoVa()) {
            mCtx->hw_device_ctx = av_buffer_ref(d);
            mCtx->get_format = [] (AVCodecContext *, const AVPixelFormat *f) {
                const AVPixelFormat *p = f;
                for (; *p != AV_PIX_FMT_NONE; p++) if (*p == AV_PIX_FMT_VAAPI) return *p;
                ALOGW("la GPU no admite este flujo: software");
                return f[0];
            };
            mCtx->thread_count = 1;
        } else mCtx->thread_count = kHilosSw;
        if (!mPaquete) { mPaquete = av_packet_alloc(); mCuadro = av_frame_alloc(); mCpu = av_frame_alloc(); }
        return avcodec_open2(mCtx, c, nullptr) == 0;
    }
    void cerrar() {
        reiniciarVpp();
        avcodec_free_context(&mCtx); av_packet_free(&mPaquete); av_frame_free(&mCuadro); av_frame_free(&mCpu);
        mPendientes.clear(); mCsd.clear(); mSinGpu = mEsperarClave = mSalidaRgba = false;
    }
    // La GPU falló a mitad: los cuadros pendientes salen vacíos (sin error: el reproductor sigue), contexto nuevo por
    // software y se descarta todo hasta el próximo cuadro clave (sin referencias, saldría basura).
    void pasarASoftware(const std::unique_ptr<C2Work> &work, int r) {
        ALOGW("la GPU falló (%d): se sigue por software desde el próximo cuadro clave", r);
        for (uint64_t i : mPendientes)
            if (work && i == work->input.ordinal.frameIndex.peeku()) completarVacio(work);
            else finish(i, [] (const std::unique_ptr<C2Work> &x) { completarVacio(x); });
        mPendientes.clear();
        avcodec_free_context(&mCtx); mSinGpu = mEsperarClave = true;
        if (abrir() && empaquetar(mCsd.data(), mCsd.size(), -1)) { avcodec_send_packet(mCtx, mPaquete); av_packet_unref(mPaquete); }
    }
    // ¿El paquete empieza un cuadro clave? H.264: NAL IDR (5); HEVC: IRAP (16-21); VP9: frame_type 0 (perfil 0).
    bool esClave(const uint8_t *d, size_t n) const {
        if (mCodec.id == AV_CODEC_ID_VP9) return n && (d[0] & 0xcc) == 0x80;
        for (size_t i = 0; i + 3 < n; i++)
            if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
                int t = mCodec.id == AV_CODEC_ID_H264 ? d[i + 3] & 0x1f : (d[i + 3] >> 1) & 0x3f;
                if (mCodec.id == AV_CODEC_ID_H264 ? t == 5 : t >= 16 && t <= 21) return true;
            }
        return false;
    }
    // stop/reset/flush: también suelta las superficies importadas (búferes de una Surface que puede desaparecer).
    // El framework reenvía el CSD tras flush: se vacía mCsd para no acumularlo.
    void reiniciar() { if (mCtx) avcodec_flush_buffers(mCtx); mPendientes.clear(); mCsd.clear(); reiniciarVpp(); }

    // true si FFmpeg aceptó el paquete. EAGAIN: saca cuadros y reintenta. Error de la GPU: reabre por software.
    bool enviar(const uint8_t *d, size_t n, int64_t pts, const std::unique_ptr<C2Work> &work,
                const std::shared_ptr<C2BlockPool> &pool) {
        if (mEsperarClave && pts >= 0) {
            if (!esClave(d, n)) return false;   // el trabajo sale vacío
            mEsperarClave = false;
        }
        if (!empaquetar(d, n, pts)) return false;
        int r;
        AVCodecContext *c0 = mCtx;
        for (int k = 0; (r = avcodec_send_packet(mCtx, mPaquete)) == AVERROR(EAGAIN) && k < 64; k++) {
            recibir(work, pool);
            if (mCtx != c0) break;   // recibir() pasó a software: el paquete se vuelve a evaluar abajo
        }
        av_packet_unref(mPaquete);
        if (mCtx != c0) return mCtx && pts >= 0 && enviar(d, n, pts, work, pool);
        if (r < 0 && r != AVERROR_INVALIDDATA && r != AVERROR(EAGAIN) && mCtx->hw_device_ctx) {
            pasarASoftware(work, r);
            return mCtx && pts >= 0 && enviar(d, n, pts, work, pool);   // si es clave entra ya en el contexto nuevo
        }
        if (r == AVERROR(EAGAIN)) { ALOGE("FFmpeg no acepta el paquete"); work->result = C2_CORRUPTED; }
        else if (r < 0) ALOGW("send_packet: %d", r);
        return r >= 0;
    }
    bool empaquetar(const uint8_t *d, size_t n, int64_t pts) {   // FFmpeg exige relleno tras los datos: copia
        if (!n || n > (size_t)INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE || av_new_packet(mPaquete, (int)n) < 0) return false;
        memcpy(mPaquete->data, d, n); mPaquete->pts = pts;
        return true;
    }

    // Saca todos los cuadros listos y los entrega en el trabajo de su índice (pts = frameIndex).
    void recibir(const std::unique_ptr<C2Work> &work, const std::shared_ptr<C2BlockPool> &pool) {
        int r = AVERROR(EAGAIN);
        while (mCtx && (r = avcodec_receive_frame(mCtx, mCuadro)) == 0) {   // mCtx nulo si falló reabrir por software
            if (mCuadro->format == AV_PIX_FMT_VAAPI && mFalloPrueba && ++mCuadrosGpu >= mFalloPrueba) {
                av_frame_unref(mCuadro); r = AVERROR(EIO); break;
            }
            entregar(work, pool);
            av_frame_unref(mCuadro);
        }
        // Con VA la GPU decodifica al pedir el cuadro: sus errores salen aquí, no en send_packet.
        if (mCtx && mCtx->hw_device_ctx && r < 0 && r != AVERROR(EAGAIN) && r != AVERROR_EOF) pasarASoftware(work, r);
    }
    void vaciar(const std::unique_ptr<C2Work> &work, const std::shared_ptr<C2BlockPool> &pool) {
        if (mCtx) avcodec_send_packet(mCtx, nullptr);
        recibir(work, pool);
        if (mCtx) avcodec_flush_buffers(mCtx);   // tras EOF FFmpeg no acepta más paquetes sin vaciar
        for (uint64_t i : mPendientes)   // cuadros que el decodificador descartó: se devuelven vacíos
            if (!work || i != work->input.ordinal.frameIndex.peeku()) finish(i, [] (const std::unique_ptr<C2Work> &w) { completarVacio(w); });
        mPendientes.clear();
    }
    static void completarVacio(const std::unique_ptr<C2Work> &w) {
        w->worklets.front()->output.flags = (C2FrameData::flags_t)(w->input.flags & C2FrameData::FLAG_END_OF_STREAM);
        w->worklets.front()->output.buffers.clear();
        w->worklets.front()->output.ordinal = w->input.ordinal;
        w->workletsProcessed = 1u;
    }

    void entregar(const std::unique_ptr<C2Work> &work, const std::shared_ptr<C2BlockPool> &pool) {
        // pts = frameIndex. Sin pts (AV_NOPTS_VALUE): el pendiente más antiguo (FIFO). Índice ya entregado (dos campos con
        // el mismo pts): se descarta el segundo; su trabajo ya terminó, nada queda colgado.
        if (mCuadro->pts < 0 && mPendientes.empty()) return;
        uint64_t indice = mCuadro->pts < 0 ? *mPendientes.begin() : (uint64_t)mCuadro->pts;
        if (!mPendientes.erase(indice)) { ALOGW("cuadro con índice desconocido %llu", (unsigned long long)indice); return; }
        // Un índice más de kRetraso entradas anterior a uno ya salido no saldrá nunca (el reordenamiento no llega a tanto).
        while (!mPendientes.empty() && *mPendientes.begin() + kRetraso < indice) {
            uint64_t i = *mPendientes.begin(); mPendientes.erase(mPendientes.begin());
            if (work && i == work->input.ordinal.frameIndex.peeku()) completarVacio(work);
            else finish(i, [] (const std::unique_ptr<C2Work> &x) { completarVacio(x); });
        }
        uint32_t w = mCuadro->width & ~1u, h = mCuadro->height & ~1u;
        std::vector<std::unique_ptr<C2Param>> cambios;
        bool error = false;
        if (w != mAncho || h != mAlto) {   // primera vez o cambio de resolución: avisar al cliente
            mAncho = w; mAlto = h;
            C2StreamPictureSizeInfo::output t(0u, w, h);
            std::vector<std::unique_ptr<C2SettingResult>> fallos;
            if (w > kMaxLado || h > kMaxLado || mIntf->config({&t}, C2_MAY_BLOCK, &fallos) != C2_OK) {
                ALOGE("tamaño no admitido %ux%u", w, h); mAncho = mAlto = 0; error = true;
            } else cambios.push_back(C2Param::Copy(t));
            reiniciarVpp();
        }
        std::shared_ptr<C2GraphicBlock> bloque;
        bool gpu = mCuadro->format == AV_PIX_FMT_VAAPI, superficie = pool->getAllocatorId() != C2PlatformAllocatorStore::GRALLOC;
        // RGBA_8888 y no IMPL_DEFINED: SurfaceFlinger aborta al importar el formato 34. Si la Surface ya recibió RGBA (GPU)
        // y luego falla la GPU o el VPP, se sigue en RGBA convertido por CPU, sin cambiar de formato a mitad.
        bool ok = false, rgbaCpu = superficie && !gpu && mSalidaRgba;
        c2_status_t e = C2_OK;
        if (!error && superficie && gpu) {
            // 1<<28 (vendor): el composer reconoce la capa de vídeo y puede dejar que la pinte el Visor (capa DEVICE).
            e = pool->fetchGraphicBlock(w, h, HAL_PIXEL_FORMAT_RGBA_8888, C2MemoryUsage(0, (1ull << 9) /* GPU_RENDER_TARGET */ | (1ull << 28)), &bloque);
            ok = e == C2_OK && convertirGpu(*bloque);
            if (ok) mSalidaRgba = true; else { ALOGW("VPP falló: RGBA por CPU"); rgbaCpu = mSalidaRgba = true; }
        }
        if (!error && rgbaCpu) {
            e = pool->fetchGraphicBlock(w, h, HAL_PIXEL_FORMAT_RGBA_8888, {C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE}, &bloque);
            ok = e == C2_OK && aRgbaCpu(*bloque);
        } else if (!error && !(superficie && gpu)) {
            e = pool->fetchGraphicBlock(w, h, HAL_PIXEL_FORMAT_YCBCR_420_888, {C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE}, &bloque);
            ok = e == C2_OK && copiarCpu(*bloque);
        }
        if (!ok) ALOGE("salida perdida (fetch %d)", e);
        auto llenar = [&] (const std::unique_ptr<C2Work> &x) {
            x->worklets.front()->output.flags = (C2FrameData::flags_t)0;
            x->worklets.front()->output.buffers.clear();
            if (ok) x->worklets.front()->output.buffers.push_back(createGraphicBuffer(bloque, C2Rect(w, h)));
            for (auto &c : cambios) x->worklets.front()->output.configUpdate.push_back(std::move(c));
            x->worklets.front()->output.ordinal = x->input.ordinal;
            if (error) x->result = C2_BAD_VALUE;   // tamaño no admitido
            x->workletsProcessed = 1u;
        };
        if (work && indice == work->input.ordinal.frameIndex.peeku()) llenar(work);
        else finish(indice, llenar);
    }

    // NV12 de la GPU -> RGBA en el búfer de gralloc (fd de dxg importado como superficie VA), con el procesador de vídeo.
    bool convertirGpu(const C2GraphicBlock &b) {
        // El C2Handle es el native_handle de nuestro gralloc con ints de Codec2 añadidos al final (gralloc o AHWB):
        // fd 0 = recurso D3D12; ints (hal/gralloc/comun.h) I_MAGIA=0, I_STRIDE=7, I_ID=10/11, I_TIPO=12 (1 = T_D3D12_GPU).
        const native_handle_t *nh = reinterpret_cast<const native_handle_t *>(b.handle());
        const int *e = nh ? nh->data + nh->numFds : nullptr;
        bool valido = nh && nh->numFds == 1 && nh->numInts >= 13 && e[0] == 0x53444856 && e[12] == 1 && nh->data[0] >= 0;
        if (!valido) { ALOGE("el búfer de salida no es una textura D3D12 de nuestro gralloc"); return false; }
        uint64_t id = (uint32_t)e[10] | ((uint64_t)(uint32_t)e[11] << 32);
        VASurfaceID dst = superficieDe(id, nh->data[0], e[7], b.width(), b.height());
        if (dst == VA_INVALID_SURFACE) return false;
        VADisplay dpy = pantalla();
        if (mVpp == VA_INVALID_ID) {
            if (vaCreateConfig(dpy, VAProfileNone, VAEntrypointVideoProc, nullptr, 0, &mVppCfg) != VA_STATUS_SUCCESS ||
                vaCreateContext(dpy, mVppCfg, mAncho, mAlto, VA_PROGRESSIVE, nullptr, 0, &mVpp) != VA_STATUS_SUCCESS) {
                ALOGE("sin procesador de vídeo VA"); mVpp = VA_INVALID_ID; reiniciarVpp(); return false;
            }
        }
        VARectangle r = {0, 0, (uint16_t)mAncho, (uint16_t)mAlto};
        VAProcPipelineParameterBuffer pp = {};
        pp.surface = (VASurfaceID)(uintptr_t)mCuadro->data[3]; pp.surface_region = &r; pp.output_region = &r;
        pp.output_background_color = 0xff000000;
        pp.surface_color_standard = mCuadro->colorspace == AVCOL_SPC_BT470BG || mCuadro->colorspace == AVCOL_SPC_SMPTE170M ||
            (mCuadro->colorspace == AVCOL_SPC_UNSPECIFIED && mAlto < 720) ? VAProcColorStandardBT601 : VAProcColorStandardBT709;
        pp.output_color_standard = VAProcColorStandardSRGB;
        pp.input_color_properties.color_range = mCuadro->color_range == AVCOL_RANGE_JPEG ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
        pp.output_color_properties.color_range = VA_SOURCE_RANGE_FULL;
        VABufferID buf;
        if (vaCreateBuffer(dpy, mVpp, VAProcPipelineParameterBufferType, sizeof pp, 1, &pp, &buf) != VA_STATUS_SUCCESS) return false;
        bool ok = vaBeginPicture(dpy, mVpp, dst) == VA_STATUS_SUCCESS && vaRenderPicture(dpy, mVpp, &buf, 1) == VA_STATUS_SUCCESS &&
                  vaEndPicture(dpy, mVpp) == VA_STATUS_SUCCESS;
        vaDestroyBuffer(dpy, buf);
        return ok && vaSyncSurface(dpy, dst) == VA_STATUS_SUCCESS;   // el consumidor lee el búfer en cuanto se entrega
    }
    // Superficie VA de un búfer de gralloc, por su id (los búferes de la Surface se reciclan: se importan una vez).
    VASurfaceID superficieDe(uint64_t id, int fd, int stride, uint32_t w, uint32_t h) {
        auto it = mImportadas.find(id);
        if (it != mImportadas.end()) return it->second;
        if (mImportadas.size() >= 48) reiniciarVpp();   // shortcut: vacía la caché entera; con LRU si alguna app rota más búferes
        VADRMPRIMESurfaceDescriptor d = {};
        d.fourcc = VA_FOURCC_RGBA; d.width = w; d.height = h; d.num_objects = 1;
        d.objects[0].fd = fd; d.objects[0].size = (uint32_t)stride * h * 4; d.objects[0].drm_format_modifier = 0;
        d.num_layers = 1; d.layers[0].drm_format = 0x34324241 /* DRM_FORMAT_ABGR8888 = RGBA en memoria */;
        d.layers[0].num_planes = 1; d.layers[0].object_index[0] = 0; d.layers[0].offset[0] = 0; d.layers[0].pitch[0] = (uint32_t)stride * 4;
        VASurfaceAttrib a[2] = {};
        a[0].type = VASurfaceAttribMemoryType; a[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
        a[0].value.type = VAGenericValueTypeInteger; a[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2;
        a[1].type = VASurfaceAttribExternalBufferDescriptor; a[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
        a[1].value.type = VAGenericValueTypePointer; a[1].value.value.p = &d;
        VASurfaceID s;
        if (vaCreateSurfaces(pantalla(), VA_RT_FORMAT_RGB32, w, h, &s, 1, a, 2) != VA_STATUS_SUCCESS) {
            ALOGE("no se pudo importar el búfer de gralloc a VA"); return VA_INVALID_SURFACE;
        }
        return mImportadas[id] = s;
    }
    void reiniciarVpp() {
        VADisplay dpy = dispositivoVa() ? pantalla() : nullptr;
        if (!dpy) return;
        for (auto &[id, s] : mImportadas) vaDestroySurfaces(dpy, &s, 1);
        mImportadas.clear();
        if (mVpp != VA_INVALID_ID) vaDestroyContext(dpy, mVpp);
        if (mVppCfg != VA_INVALID_ID) vaDestroyConfig(dpy, mVppCfg);
        mVpp = VA_INVALID_ID; mVppCfg = VA_INVALID_ID;
    }

    // Cuadro en memoria (NV12 de VA bajado a CPU, o YUV420P/NV12 de software); nullptr si el formato no se sabe copiar.
    const AVFrame *cuadroCpu() {
        if (mCuadro->format == AV_PIX_FMT_VAAPI) {
            mCpu->format = AV_PIX_FMT_NV12;
            return av_hwframe_map(mCpu, mCuadro, AV_HWFRAME_MAP_READ) < 0 ? nullptr : mCpu;
        }
        if (mCuadro->format == AV_PIX_FMT_YUV420P || mCuadro->format == AV_PIX_FMT_YUVJ420P || mCuadro->format == AV_PIX_FMT_NV12) return mCuadro;
        ALOGE("formato de software sin copia: %d", mCuadro->format);
        return nullptr;
    }
    // Cb y Cr de la fila de croma i, columna x (NV12 intercalado o planos separados).
    static void croma(const AVFrame *f, uint32_t i, uint32_t x, int *cb, int *cr) {
        if (f->format == AV_PIX_FMT_NV12) { const uint8_t *s = f->data[1] + (size_t)i * f->linesize[1] + 2 * x; *cb = s[0]; *cr = s[1]; }
        else { *cb = f->data[1][(size_t)i * f->linesize[1] + x]; *cr = f->data[2][(size_t)i * f->linesize[2] + x]; }
    }

    // Cuadro -> plano YUV del cliente, según su disposición (C2PlanarLayout).
    bool copiarCpu(C2GraphicBlock &b) {
        const AVFrame *f = cuadroCpu();
        if (!f) return false;
        C2GraphicView v = b.map().get();
        bool ok = !v.error() && v.layout().numPlanes >= 3;
        if (ok) {
            const C2PlanarLayout &l = v.layout();
            uint8_t *y = v.data()[C2PlanarLayout::PLANE_Y], *u = v.data()[C2PlanarLayout::PLANE_U], *vv = v.data()[C2PlanarLayout::PLANE_V];
            for (uint32_t i = 0; i < mAlto; i++) memcpy(y + i * l.planes[0].rowInc, f->data[0] + (size_t)i * f->linesize[0], mAncho);
            int cu = l.planes[1].colInc, cv = l.planes[2].colInc;
            for (uint32_t i = 0; i < mAlto / 2; i++) {
                uint8_t *du = u + i * l.planes[1].rowInc, *dv = vv + i * l.planes[2].rowInc;
                for (uint32_t x = 0; x < mAncho / 2; x++) { int cb, cr; croma(f, i, x, &cb, &cr); du[x * cu] = cb; dv[x * cv] = cr; }
            }
        }
        av_frame_unref(mCpu);
        return ok;
    }

    // Cuadro -> RGBA por CPU (solo si la GPU falló con la Surface ya en RGBA). BT.601 o BT.709, rango limitado o completo,
    // con el mismo criterio que el VPP. shortcut: C escalar (~1080p a 60 fps cabe en un núcleo); libyuv si hiciera falta.
    bool aRgbaCpu(C2GraphicBlock &b) {
        const AVFrame *f = cuadroCpu();
        if (!f) return false;
        C2GraphicView v = b.map().get();
        const C2PlanarLayout &l = v.layout();
        // C2AllocatorGralloc describe RGBA_8888 como TYPE_RGB de 3 planos (el alfa no lo cuenta): basta con R a paso 4.
        bool ok = !v.error() && (l.type == C2PlanarLayout::TYPE_RGB || l.type == C2PlanarLayout::TYPE_RGBA) && l.numPlanes >= 3 &&
                  l.planes[0].colInc == 4;
        if (!ok) ALOGE("RGBA por CPU: map %d, tipo %d, planos %u", v.error(), (int)l.type, l.numPlanes);
        if (ok) {
            bool b709 = !(f->colorspace == AVCOL_SPC_BT470BG || f->colorspace == AVCOL_SPC_SMPTE170M ||
                          (f->colorspace == AVCOL_SPC_UNSPECIFIED && mAlto < 720)), lleno = f->color_range == AVCOL_RANGE_JPEG;
            // Coeficientes x256: Y, Cr->R, Cb->G, Cr->G, Cb->B.
            int ky = lleno ? 256 : 298, y0 = lleno ? 0 : 16;
            int rv = b709 ? (lleno ? 403 : 459) : (lleno ? 359 : 409), gu = b709 ? (lleno ? 48 : 55) : (lleno ? 88 : 100),
                gv = b709 ? (lleno ? 120 : 136) : (lleno ? 183 : 208), bu = b709 ? (lleno ? 475 : 541) : (lleno ? 454 : 516);
            auto sat = [] (int x) { return (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x); };
            uint8_t *d = v.data()[C2PlanarLayout::PLANE_R];
            for (uint32_t i = 0; i < mAlto; i++) {
                uint8_t *fila = d + (size_t)i * l.planes[0].rowInc;
                const uint8_t *py = f->data[0] + (size_t)i * f->linesize[0];
                for (uint32_t x = 0; x < mAncho; x++) {
                    int cb, cr; croma(f, i / 2, x / 2, &cb, &cr); cb -= 128; cr -= 128;
                    int yy = ky * (py[x] - y0);
                    fila[4 * x] = sat((yy + rv * cr + 128) >> 8);
                    fila[4 * x + 1] = sat((yy - gu * cb - gv * cr + 128) >> 8);
                    fila[4 * x + 2] = sat((yy + bu * cb + 128) >> 8);
                    fila[4 * x + 3] = 255;
                }
            }
        }
        av_frame_unref(mCpu);
        return ok;
    }

    std::shared_ptr<Interfaz> mIntf;
    const Codec &mCodec;
    AVCodecContext *mCtx = nullptr;
    AVPacket *mPaquete = nullptr;
    AVFrame *mCuadro = nullptr, *mCpu = nullptr;
    std::set<uint64_t> mPendientes;   // índices enviados sin cuadro todavía
    std::vector<uint8_t> mCsd;   // SPS/PPS/VPS recibidos (para reabrir por software a mitad del flujo)
    bool mSinGpu = false, mEsperarClave = false, mSalidaRgba = false;   // software tras fallo; Surface ya en RGBA
    int mFalloPrueba = 0, mCuadrosGpu = 0;
    uint32_t mAncho = 0, mAlto = 0;
    VAConfigID mVppCfg = VA_INVALID_ID;
    VAContextID mVpp = VA_INVALID_ID;
    std::map<uint64_t, VASurfaceID> mImportadas;
};

// ---------------------------------------------------------------------------------------------------- almacén
class Almacen : public C2ComponentStore {
public:
    Almacen() : mReflector(std::make_shared<C2ReflectorHelper>()), mIntfAlmacen(mReflector) {
        for (const Codec &c : kCodecs) {
            auto t = std::make_shared<C2Component::Traits>();
            t->name = c.nombre; t->domain = C2Component::DOMAIN_VIDEO; t->kind = C2Component::KIND_DECODER;
            t->rank = 0x100; t->mediaType = c.mime; t->owner = "default";
            mRasgos.push_back(t);
        }
    }
    C2String getName() const override { return "default"; }
    c2_status_t createComponent(C2String nombre, std::shared_ptr<C2Component> *const out) override {
        const Codec *c = buscar(nombre);
        if (!c) return C2_NOT_FOUND;
        *out = std::make_shared<Decodificador>(++mId, *c, std::make_shared<Interfaz>(mReflector, *c));
        return C2_OK;
    }
    c2_status_t createInterface(C2String nombre, std::shared_ptr<C2ComponentInterface> *const out) override {
        const Codec *c = buscar(nombre);
        if (!c) return C2_NOT_FOUND;
        *out = std::make_shared<SimpleInterface<Interfaz>>(c->nombre, ++mId, std::make_shared<Interfaz>(mReflector, *c));
        return C2_OK;
    }
    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override { return mRasgos; }
    c2_status_t copyBuffer(std::shared_ptr<C2GraphicBuffer>, std::shared_ptr<C2GraphicBuffer>) override { return C2_OMITTED; }
    c2_status_t query_sm(const std::vector<C2Param *> &s, const std::vector<C2Param::Index> &h,
                         std::vector<std::unique_ptr<C2Param>> *const out) const override {
        return mIntfAlmacen.query(s, h, C2_MAY_BLOCK, out);
    }
    c2_status_t config_sm(const std::vector<C2Param *> &p, std::vector<std::unique_ptr<C2SettingResult>> *const f) override {
        return mIntfAlmacen.config(p, C2_MAY_BLOCK, f);
    }
    std::shared_ptr<C2ParamReflector> getParamReflector() const override { return mReflector; }
    c2_status_t querySupportedParams_nb(std::vector<std::shared_ptr<C2ParamDescriptor>> *const p) const override {
        return mIntfAlmacen.querySupportedParams(p);
    }
    c2_status_t querySupportedValues_sm(std::vector<C2FieldSupportedValuesQuery> &q) const override {
        return mIntfAlmacen.querySupportedValues(q, C2_MAY_BLOCK);
    }

private:
    const Codec *buscar(const C2String &n) {
        for (const Codec &c : kCodecs) if (n == c.nombre) return &c;
        return nullptr;
    }
    // Parámetros del almacén: dma-buf del montón "system" para los búferes lineales (entrada).
    struct IntfAlmacen : C2InterfaceHelper {
        explicit IntfAlmacen(const std::shared_ptr<C2ReflectorHelper> &h) : C2InterfaceHelper(h) {
            setDerivedInstance(this);
            addParameter(DefineParam(mDmaBuf, "dmabuf-usage")
                .withDefault(C2StoreDmaBufUsageInfo::AllocShared(128))
                .withFields({C2F(mDmaBuf, m.usage).flags({C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE}),
                             C2F(mDmaBuf, m.capacity).inRange(0, UINT32_MAX, 1024), C2F(mDmaBuf, m.allocFlags).flags({}),
                             C2F(mDmaBuf, m.heapName).any()})
                .withSetter(DmaBuf).build());
        }
        static C2R DmaBuf(bool, C2P<C2StoreDmaBufUsageInfo> &me) {
            strncpy(me.set().m.heapName, "system", me.v.flexCount()); me.set().m.allocFlags = 0;
            return C2R::Ok();
        }
        std::shared_ptr<C2StoreDmaBufUsageInfo> mDmaBuf;
    };
    std::shared_ptr<C2ReflectorHelper> mReflector;
    IntfAlmacen mIntfAlmacen;
    std::vector<std::shared_ptr<const C2Component::Traits>> mRasgos;
    std::atomic<c2_node_id_t> mId{0};
};

}  // namespace

namespace android { void SetUpMinijail(const std::string &base, const std::string &extra); }   // libavservices_minijail

int main() {
    signal(SIGPIPE, SIG_IGN);
    // seccomp antes de crear hilos (los heredan): base de Codec2 del vendor + lo medido para FFmpeg, VA, Mesa y D3D12
    // (seccomp.policy, medido con video/rastreo). Una llamada fuera de la lista mata el proceso con SIGSYS.
    ALOGI("arranque");   // abre el socket de logd antes de seccomp
    android::SetUpMinijail("/vendor/etc/seccomp_policy/android.hardware.media.c2-default-seccomp_policy",
                           "/vendor/etc/seccomp_policy/surfacedual-c2.policy");
    using namespace ::aidl::android::hardware::media::c2;
    ABinderProcess_setThreadPoolMaxThreadCount(8);
    std::shared_ptr<IComponentStore> s = ::ndk::SharedRefBase::make<utils::ComponentStore>(std::make_shared<Almacen>());
    const std::string n = std::string(IComponentStore::descriptor) + "/default";
    if (AServiceManager_addService(s->asBinder().get(), n.c_str()) != EX_NONE) { ALOGE("no se pudo registrar %s", n.c_str()); return 1; }
    ALOGI("%s listo", n.c_str());
    ABinderProcess_joinThreadPool();
    return 0;
}
