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
#include <aidl/android/hardware/common/fmq/MQDescriptor.h>
#include <aidl/android/hardware/common/fmq/SynchronizedReadWrite.h>
#include <aidl/android/hardware/common/fmq/UnsynchronizedWrite.h>
#include <aidl/android/hardware/media/bufferpool2/BufferInvalidationMessage.h>
#include <aidl/android/hardware/media/bufferpool2/BufferStatusMessage.h>
#include <aidl/android/hardware/media/bufferpool2/IAccessor.h>
#include <aidl/android/hardware/media/bufferpool2/IConnection.h>
#include <aidl/android/hardware/media/bufferpool2/IObserver.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::common::fmq {
template <typename T, typename Flavor>
class MQDescriptor;
}  // namespace aidl::android::hardware::common::fmq
namespace aidl::android::hardware::media::bufferpool2 {
class BufferInvalidationMessage;
class BufferStatusMessage;
class IConnection;
class IObserver;
}  // namespace aidl::android::hardware::media::bufferpool2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
class IAccessorDelegator;

class IAccessor : public ::ndk::ICInterface {
public:
  typedef IAccessorDelegator DefaultDelegator;
  static const char* descriptor;
  IAccessor();
  virtual ~IAccessor();

  class ConnectionInfo {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IConnection> connection;
    int64_t connectionId = 0L;
    int32_t msgId = 0;
    ::aidl::android::hardware::common::fmq::MQDescriptor<::aidl::android::hardware::media::bufferpool2::BufferStatusMessage, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite> toFmqDesc;
    ::aidl::android::hardware::common::fmq::MQDescriptor<::aidl::android::hardware::media::bufferpool2::BufferInvalidationMessage, ::aidl::android::hardware::common::fmq::UnsynchronizedWrite> fromFmqDesc;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const ConnectionInfo& _rhs) const {
      return std::tie(connection, connectionId, msgId, toFmqDesc, fromFmqDesc) == std::tie(_rhs.connection, _rhs.connectionId, _rhs.msgId, _rhs.toFmqDesc, _rhs.fromFmqDesc);
    }
    inline bool operator<(const ConnectionInfo& _rhs) const {
      return std::tie(connection, connectionId, msgId, toFmqDesc, fromFmqDesc) < std::tie(_rhs.connection, _rhs.connectionId, _rhs.msgId, _rhs.toFmqDesc, _rhs.fromFmqDesc);
    }
    inline bool operator!=(const ConnectionInfo& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const ConnectionInfo& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const ConnectionInfo& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const ConnectionInfo& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "ConnectionInfo{";
      _aidl_os << "connection: " << ::android::internal::ToString(connection);
      _aidl_os << ", connectionId: " << ::android::internal::ToString(connectionId);
      _aidl_os << ", msgId: " << ::android::internal::ToString(msgId);
      _aidl_os << ", toFmqDesc: " << ::android::internal::ToString(toFmqDesc);
      _aidl_os << ", fromFmqDesc: " << ::android::internal::ToString(fromFmqDesc);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  static inline const int32_t version = 2;
  static inline const std::string hash = "c12818c51418fbfc76f1b81eba6678d75cf526c1";
  static constexpr uint32_t TRANSACTION_connect = FIRST_CALL_TRANSACTION + 0;

  static std::shared_ptr<IAccessor> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IAccessor>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IAccessor>* instance);
  virtual ::ndk::ScopedAStatus connect(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IObserver>& in_observer, ::aidl::android::hardware::media::bufferpool2::IAccessor::ConnectionInfo* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IAccessorDefault : public IAccessor {
public:
  ::ndk::ScopedAStatus connect(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IObserver>& in_observer, ::aidl::android::hardware::media::bufferpool2::IAccessor::ConnectionInfo* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace bufferpool2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
