/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=7 --hash=83e58c8276ecf8c32a567b3830cd4d1c0b43bfdb -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\AlphaInterpretation.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\BlendMode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\BufferUsage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\ChromaSiting.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\ColorTransform.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Compression.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Cta861_3.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Dataspace.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\DisplayDecorationSupport.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\DisplayHotplugEvent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\ExtendableType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\FRect.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HardwareBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HardwareBufferDescription.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Hdr.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HdrConversionCapability.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HdrConversionStrategy.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Interlaced.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PixelFormat.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PlaneLayout.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PlaneLayoutComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PlaneLayoutComponentType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Point.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Rect.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Smpte2086.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\StandardMetadataType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Transform.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\XyColor.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_enums.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
enum class StandardMetadataType : int64_t {
  INVALID = 0L,
  BUFFER_ID = 1L,
  NAME = 2L,
  WIDTH = 3L,
  HEIGHT = 4L,
  LAYER_COUNT = 5L,
  PIXEL_FORMAT_REQUESTED = 6L,
  PIXEL_FORMAT_FOURCC = 7L,
  PIXEL_FORMAT_MODIFIER = 8L,
  USAGE = 9L,
  ALLOCATION_SIZE = 10L,
  PROTECTED_CONTENT = 11L,
  COMPRESSION = 12L,
  INTERLACED = 13L,
  CHROMA_SITING = 14L,
  PLANE_LAYOUTS = 15L,
  CROP = 16L,
  DATASPACE = 17L,
  BLEND_MODE = 18L,
  SMPTE2086 = 19L,
  CTA861_3 = 20L,
  SMPTE2094_40 = 21L,
  SMPTE2094_10 = 22L,
  STRIDE = 23L,
  SMPTE2094_50 = 24L,
};

}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
[[nodiscard]] static inline std::string toString(StandardMetadataType val) {
  switch(val) {
  case StandardMetadataType::INVALID:
    return "INVALID";
  case StandardMetadataType::BUFFER_ID:
    return "BUFFER_ID";
  case StandardMetadataType::NAME:
    return "NAME";
  case StandardMetadataType::WIDTH:
    return "WIDTH";
  case StandardMetadataType::HEIGHT:
    return "HEIGHT";
  case StandardMetadataType::LAYER_COUNT:
    return "LAYER_COUNT";
  case StandardMetadataType::PIXEL_FORMAT_REQUESTED:
    return "PIXEL_FORMAT_REQUESTED";
  case StandardMetadataType::PIXEL_FORMAT_FOURCC:
    return "PIXEL_FORMAT_FOURCC";
  case StandardMetadataType::PIXEL_FORMAT_MODIFIER:
    return "PIXEL_FORMAT_MODIFIER";
  case StandardMetadataType::USAGE:
    return "USAGE";
  case StandardMetadataType::ALLOCATION_SIZE:
    return "ALLOCATION_SIZE";
  case StandardMetadataType::PROTECTED_CONTENT:
    return "PROTECTED_CONTENT";
  case StandardMetadataType::COMPRESSION:
    return "COMPRESSION";
  case StandardMetadataType::INTERLACED:
    return "INTERLACED";
  case StandardMetadataType::CHROMA_SITING:
    return "CHROMA_SITING";
  case StandardMetadataType::PLANE_LAYOUTS:
    return "PLANE_LAYOUTS";
  case StandardMetadataType::CROP:
    return "CROP";
  case StandardMetadataType::DATASPACE:
    return "DATASPACE";
  case StandardMetadataType::BLEND_MODE:
    return "BLEND_MODE";
  case StandardMetadataType::SMPTE2086:
    return "SMPTE2086";
  case StandardMetadataType::CTA861_3:
    return "CTA861_3";
  case StandardMetadataType::SMPTE2094_40:
    return "SMPTE2094_40";
  case StandardMetadataType::SMPTE2094_10:
    return "SMPTE2094_10";
  case StandardMetadataType::STRIDE:
    return "STRIDE";
  case StandardMetadataType::SMPTE2094_50:
    return "SMPTE2094_50";
  default:
    return std::to_string(static_cast<int64_t>(val));
  }
}
}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::graphics::common::StandardMetadataType, 25> enum_values<aidl::android::hardware::graphics::common::StandardMetadataType> = {
  aidl::android::hardware::graphics::common::StandardMetadataType::INVALID,
  aidl::android::hardware::graphics::common::StandardMetadataType::BUFFER_ID,
  aidl::android::hardware::graphics::common::StandardMetadataType::NAME,
  aidl::android::hardware::graphics::common::StandardMetadataType::WIDTH,
  aidl::android::hardware::graphics::common::StandardMetadataType::HEIGHT,
  aidl::android::hardware::graphics::common::StandardMetadataType::LAYER_COUNT,
  aidl::android::hardware::graphics::common::StandardMetadataType::PIXEL_FORMAT_REQUESTED,
  aidl::android::hardware::graphics::common::StandardMetadataType::PIXEL_FORMAT_FOURCC,
  aidl::android::hardware::graphics::common::StandardMetadataType::PIXEL_FORMAT_MODIFIER,
  aidl::android::hardware::graphics::common::StandardMetadataType::USAGE,
  aidl::android::hardware::graphics::common::StandardMetadataType::ALLOCATION_SIZE,
  aidl::android::hardware::graphics::common::StandardMetadataType::PROTECTED_CONTENT,
  aidl::android::hardware::graphics::common::StandardMetadataType::COMPRESSION,
  aidl::android::hardware::graphics::common::StandardMetadataType::INTERLACED,
  aidl::android::hardware::graphics::common::StandardMetadataType::CHROMA_SITING,
  aidl::android::hardware::graphics::common::StandardMetadataType::PLANE_LAYOUTS,
  aidl::android::hardware::graphics::common::StandardMetadataType::CROP,
  aidl::android::hardware::graphics::common::StandardMetadataType::DATASPACE,
  aidl::android::hardware::graphics::common::StandardMetadataType::BLEND_MODE,
  aidl::android::hardware::graphics::common::StandardMetadataType::SMPTE2086,
  aidl::android::hardware::graphics::common::StandardMetadataType::CTA861_3,
  aidl::android::hardware::graphics::common::StandardMetadataType::SMPTE2094_40,
  aidl::android::hardware::graphics::common::StandardMetadataType::SMPTE2094_10,
  aidl::android::hardware::graphics::common::StandardMetadataType::STRIDE,
  aidl::android::hardware::graphics::common::StandardMetadataType::SMPTE2094_50,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
