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
enum class DisplayHotplugEvent : int32_t {
  CONNECTED = 0,
  DISCONNECTED = 1,
  ERROR_UNKNOWN = -1,
  ERROR_INCOMPATIBLE_CABLE = -2,
  ERROR_TOO_MANY_DISPLAYS = -3,
  ERROR_LINK_UNSTABLE = -4,
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
[[nodiscard]] static inline std::string toString(DisplayHotplugEvent val) {
  switch(val) {
  case DisplayHotplugEvent::CONNECTED:
    return "CONNECTED";
  case DisplayHotplugEvent::DISCONNECTED:
    return "DISCONNECTED";
  case DisplayHotplugEvent::ERROR_UNKNOWN:
    return "ERROR_UNKNOWN";
  case DisplayHotplugEvent::ERROR_INCOMPATIBLE_CABLE:
    return "ERROR_INCOMPATIBLE_CABLE";
  case DisplayHotplugEvent::ERROR_TOO_MANY_DISPLAYS:
    return "ERROR_TOO_MANY_DISPLAYS";
  case DisplayHotplugEvent::ERROR_LINK_UNSTABLE:
    return "ERROR_LINK_UNSTABLE";
  default:
    return std::to_string(static_cast<int32_t>(val));
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
constexpr inline std::array<aidl::android::hardware::graphics::common::DisplayHotplugEvent, 6> enum_values<aidl::android::hardware::graphics::common::DisplayHotplugEvent> = {
  aidl::android::hardware::graphics::common::DisplayHotplugEvent::CONNECTED,
  aidl::android::hardware::graphics::common::DisplayHotplugEvent::DISCONNECTED,
  aidl::android::hardware::graphics::common::DisplayHotplugEvent::ERROR_UNKNOWN,
  aidl::android::hardware::graphics::common::DisplayHotplugEvent::ERROR_INCOMPATIBLE_CABLE,
  aidl::android::hardware::graphics::common::DisplayHotplugEvent::ERROR_TOO_MANY_DISPLAYS,
  aidl::android::hardware::graphics::common::DisplayHotplugEvent::ERROR_LINK_UNSTABLE,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
