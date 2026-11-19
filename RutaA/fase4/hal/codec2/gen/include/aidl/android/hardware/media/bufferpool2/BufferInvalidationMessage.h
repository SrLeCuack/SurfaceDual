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
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
class BufferInvalidationMessage {
public:
  typedef std::true_type fixed_size;
  static const char* descriptor;

  int32_t messageId __attribute__((aligned (4))) = 0;
  int32_t fromBufferId __attribute__((aligned (4))) = 0;
  int32_t toBufferId __attribute__((aligned (4))) = 0;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const BufferInvalidationMessage& _rhs) const {
    return std::tie(messageId, fromBufferId, toBufferId) == std::tie(_rhs.messageId, _rhs.fromBufferId, _rhs.toBufferId);
  }
  inline bool operator<(const BufferInvalidationMessage& _rhs) const {
    return std::tie(messageId, fromBufferId, toBufferId) < std::tie(_rhs.messageId, _rhs.fromBufferId, _rhs.toBufferId);
  }
  inline bool operator!=(const BufferInvalidationMessage& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const BufferInvalidationMessage& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const BufferInvalidationMessage& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const BufferInvalidationMessage& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "BufferInvalidationMessage{";
    _aidl_os << "messageId: " << ::android::internal::ToString(messageId);
    _aidl_os << ", fromBufferId: " << ::android::internal::ToString(fromBufferId);
    _aidl_os << ", toBufferId: " << ::android::internal::ToString(toBufferId);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
static_assert(offsetof(BufferInvalidationMessage, messageId) == 0);
static_assert(sizeof(int32_t) == 4);
static_assert(offsetof(BufferInvalidationMessage, fromBufferId) == 4);
static_assert(sizeof(int32_t) == 4);
static_assert(offsetof(BufferInvalidationMessage, toBufferId) == 8);
static_assert(sizeof(int32_t) == 4);
static_assert(alignof(BufferInvalidationMessage) == 4);
static_assert(sizeof(BufferInvalidationMessage) == 12);
}  // namespace bufferpool2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
