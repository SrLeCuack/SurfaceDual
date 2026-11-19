/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=1 --hash=d1a423213d80e15de2b10e54d908ac5c29644fef -I C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\CameraDeviceStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\CameraMetadataType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\CameraResourceCost.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\TagBoundaryId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\TorchModeStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\VendorTag.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\VendorTagSection.aidl
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
namespace camera {
namespace common {
enum class TagBoundaryId : int64_t {
  AOSP = 0L,
  VENDOR = 2147483648L,
};

}  // namespace common
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace common {
[[nodiscard]] static inline std::string toString(TagBoundaryId val) {
  switch(val) {
  case TagBoundaryId::AOSP:
    return "AOSP";
  case TagBoundaryId::VENDOR:
    return "VENDOR";
  default:
    return std::to_string(static_cast<int64_t>(val));
  }
}
}  // namespace common
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::camera::common::TagBoundaryId, 2> enum_values<aidl::android::hardware::camera::common::TagBoundaryId> = {
  aidl::android::hardware::camera::common::TagBoundaryId::AOSP,
  aidl::android::hardware::camera::common::TagBoundaryId::VENDOR,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
