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
#include <aidl/android/hardware/media/bufferpool2/BufferStatus.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
class BufferStatusMessage {
public:
  typedef std::true_type fixed_size;
  static const char* descriptor;

  int64_t transactionId __attribute__((aligned (8))) = 0L;
  int32_t bufferId __attribute__((aligned (4))) = 0;
  ::aidl::android::hardware::media::bufferpool2::BufferStatus status __attribute__((aligned (4))) = ::aidl::android::hardware::media::bufferpool2::BufferStatus(0);
  int64_t connectionId __attribute__((aligned (8))) = 0L;
  int64_t targetConnectionId __attribute__((aligned (8))) = 0L;
  int64_t timestampUs __attribute__((aligned (8))) = 0L;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const BufferStatusMessage& _rhs) const {
    return std::tie(transactionId, bufferId, status, connectionId, targetConnectionId, timestampUs) == std::tie(_rhs.transactionId, _rhs.bufferId, _rhs.status, _rhs.connectionId, _rhs.targetConnectionId, _rhs.timestampUs);
  }
  inline bool operator<(const BufferStatusMessage& _rhs) const {
    return std::tie(transactionId, bufferId, status, connectionId, targetConnectionId, timestampUs) < std::tie(_rhs.transactionId, _rhs.bufferId, _rhs.status, _rhs.connectionId, _rhs.targetConnectionId, _rhs.timestampUs);
  }
  inline bool operator!=(const BufferStatusMessage& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const BufferStatusMessage& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const BufferStatusMessage& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const BufferStatusMessage& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "BufferStatusMessage{";
    _aidl_os << "transactionId: " << ::android::internal::ToString(transactionId);
    _aidl_os << ", bufferId: " << ::android::internal::ToString(bufferId);
    _aidl_os << ", status: " << ::android::internal::ToString(status);
    _aidl_os << ", connectionId: " << ::android::internal::ToString(connectionId);
    _aidl_os << ", targetConnectionId: " << ::android::internal::ToString(targetConnectionId);
    _aidl_os << ", timestampUs: " << ::android::internal::ToString(timestampUs);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
static_assert(offsetof(BufferStatusMessage, transactionId) == 0);
static_assert(sizeof(int64_t) == 8);
static_assert(offsetof(BufferStatusMessage, bufferId) == 8);
static_assert(sizeof(int32_t) == 4);
static_assert(offsetof(BufferStatusMessage, status) == 12);
static_assert(sizeof(::aidl::android::hardware::media::bufferpool2::BufferStatus) == 4);
static_assert(offsetof(BufferStatusMessage, connectionId) == 16);
static_assert(sizeof(int64_t) == 8);
static_assert(offsetof(BufferStatusMessage, targetConnectionId) == 24);
static_assert(sizeof(int64_t) == 8);
static_assert(offsetof(BufferStatusMessage, timestampUs) == 32);
static_assert(sizeof(int64_t) == 8);
static_assert(alignof(BufferStatusMessage) == 8);
static_assert(sizeof(BufferStatusMessage) == 40);
}  // namespace bufferpool2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
