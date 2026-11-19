/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=c12818c51418fbfc76f1b81eba6678d75cf526c1 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferInvalidationMessage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferStatusMessage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IAccessor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IClientManager.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IObserver.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\ResultStatus.aidl
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
namespace media {
namespace bufferpool2 {
enum class BufferStatus : int32_t {
  NOT_USED = 0,
  USED = 1,
  TRANSFER_TO = 2,
  TRANSFER_FROM = 3,
  TRANSFER_TIMEOUT = 4,
  TRANSFER_LOST = 5,
  TRANSFER_FETCH = 6,
  TRANSFER_OK = 7,
  TRANSFER_ERROR = 8,
  INVALIDATION_ACK = 9,
};

}  // namespace bufferpool2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
[[nodiscard]] static inline std::string toString(BufferStatus val) {
  switch(val) {
  case BufferStatus::NOT_USED:
    return "NOT_USED";
  case BufferStatus::USED:
    return "USED";
  case BufferStatus::TRANSFER_TO:
    return "TRANSFER_TO";
  case BufferStatus::TRANSFER_FROM:
    return "TRANSFER_FROM";
  case BufferStatus::TRANSFER_TIMEOUT:
    return "TRANSFER_TIMEOUT";
  case BufferStatus::TRANSFER_LOST:
    return "TRANSFER_LOST";
  case BufferStatus::TRANSFER_FETCH:
    return "TRANSFER_FETCH";
  case BufferStatus::TRANSFER_OK:
    return "TRANSFER_OK";
  case BufferStatus::TRANSFER_ERROR:
    return "TRANSFER_ERROR";
  case BufferStatus::INVALIDATION_ACK:
    return "INVALIDATION_ACK";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace bufferpool2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::media::bufferpool2::BufferStatus, 10> enum_values<aidl::android::hardware::media::bufferpool2::BufferStatus> = {
  aidl::android::hardware::media::bufferpool2::BufferStatus::NOT_USED,
  aidl::android::hardware::media::bufferpool2::BufferStatus::USED,
  aidl::android::hardware::media::bufferpool2::BufferStatus::TRANSFER_TO,
  aidl::android::hardware::media::bufferpool2::BufferStatus::TRANSFER_FROM,
  aidl::android::hardware::media::bufferpool2::BufferStatus::TRANSFER_TIMEOUT,
  aidl::android::hardware::media::bufferpool2::BufferStatus::TRANSFER_LOST,
  aidl::android::hardware::media::bufferpool2::BufferStatus::TRANSFER_FETCH,
  aidl::android::hardware::media::bufferpool2::BufferStatus::TRANSFER_OK,
  aidl::android::hardware::media::bufferpool2::BufferStatus::TRANSFER_ERROR,
  aidl::android::hardware::media::bufferpool2::BufferStatus::INVALIDATION_ACK,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
