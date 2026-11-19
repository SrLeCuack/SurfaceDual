/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=c12818c51418fbfc76f1b81eba6678d75cf526c1 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferInvalidationMessage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferStatusMessage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IAccessor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IClientManager.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IObserver.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\ResultStatus.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_interface_utils.h>
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#include <android/hardware_buffer_aidl.h>
#include <aidl/android/hardware/common/NativeHandle.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::common {
class NativeHandle;
}  // namespace aidl::android::hardware::common
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
class Buffer {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  int32_t id = 0;
  std::optional<::aidl::android::hardware::common::NativeHandle> buffer;
  std::optional<::aidl::android::hardware::HardwareBuffer> hwbBuffer;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const Buffer& _rhs) const {
    return std::tie(id, buffer, hwbBuffer) == std::tie(_rhs.id, _rhs.buffer, _rhs.hwbBuffer);
  }
  inline bool operator<(const Buffer& _rhs) const {
    return std::tie(id, buffer, hwbBuffer) < std::tie(_rhs.id, _rhs.buffer, _rhs.hwbBuffer);
  }
  inline bool operator!=(const Buffer& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const Buffer& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const Buffer& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const Buffer& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "Buffer{";
    _aidl_os << "id: " << ::android::internal::ToString(id);
    _aidl_os << ", buffer: " << ::android::internal::ToString(buffer);
    _aidl_os << ", hwbBuffer: " << ::android::internal::ToString(hwbBuffer);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace bufferpool2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
