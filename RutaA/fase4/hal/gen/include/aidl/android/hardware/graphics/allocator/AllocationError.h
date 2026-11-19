/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=b061131f0ee5260aa8f779bf3569dfc861a1b89c -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\allocator\\AllocationError.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\allocator\\AllocationResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\allocator\\BufferDescriptorInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\allocator\\IAllocator.aidl
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
namespace allocator {
enum class AllocationError : int32_t {
  BAD_DESCRIPTOR = 0,
  NO_RESOURCES = 1,
  UNSUPPORTED = 2,
};

}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
[[nodiscard]] static inline std::string toString(AllocationError val) {
  switch(val) {
  case AllocationError::BAD_DESCRIPTOR:
    return "BAD_DESCRIPTOR";
  case AllocationError::NO_RESOURCES:
    return "NO_RESOURCES";
  case AllocationError::UNSUPPORTED:
    return "UNSUPPORTED";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::graphics::allocator::AllocationError, 3> enum_values<aidl::android::hardware::graphics::allocator::AllocationError> = {
  aidl::android::hardware::graphics::allocator::AllocationError::BAD_DESCRIPTOR,
  aidl::android::hardware::graphics::allocator::AllocationError::NO_RESOURCES,
  aidl::android::hardware::graphics::allocator::AllocationError::UNSUPPORTED,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
