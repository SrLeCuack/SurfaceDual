/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=1 --hash=6a780550f6e6965d6969fd7964c3ca81b6b0ccdf -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\common\\fmq\\GrantorDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\common\\fmq\\MQDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\common\\fmq\\SynchronizedReadWrite.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\common\\fmq\\UnsynchronizedWrite.aidl
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
namespace common {
namespace fmq {
enum class UnsynchronizedWrite : int8_t {
  EMPTY = 0,
};

}  // namespace fmq
}  // namespace common
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace common {
namespace fmq {
[[nodiscard]] static inline std::string toString(UnsynchronizedWrite val) {
  switch(val) {
  case UnsynchronizedWrite::EMPTY:
    return "EMPTY";
  default:
    return std::to_string(static_cast<int8_t>(val));
  }
}
}  // namespace fmq
}  // namespace common
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::common::fmq::UnsynchronizedWrite, 1> enum_values<aidl::android::hardware::common::fmq::UnsynchronizedWrite> = {
  aidl::android::hardware::common::fmq::UnsynchronizedWrite::EMPTY,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
