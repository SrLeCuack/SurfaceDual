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
#include <aidl/android/hardware/media/bufferpool2/IAccessor.h>
#include <aidl/android/hardware/media/bufferpool2/IClientManager.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::media::bufferpool2 {
class IAccessor;
}  // namespace aidl::android::hardware::media::bufferpool2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
class IClientManagerDelegator;

class IClientManager : public ::ndk::ICInterface {
public:
  typedef IClientManagerDelegator DefaultDelegator;
  static const char* descriptor;
  IClientManager();
  virtual ~IClientManager();

  class Registration {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int64_t connectionId = 0L;
    bool isNew = true;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const Registration& _rhs) const {
      return std::tie(connectionId, isNew) == std::tie(_rhs.connectionId, _rhs.isNew);
    }
    inline bool operator<(const Registration& _rhs) const {
      return std::tie(connectionId, isNew) < std::tie(_rhs.connectionId, _rhs.isNew);
    }
    inline bool operator!=(const Registration& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const Registration& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const Registration& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const Registration& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "Registration{";
      _aidl_os << "connectionId: " << ::android::internal::ToString(connectionId);
      _aidl_os << ", isNew: " << ::android::internal::ToString(isNew);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  static inline const int32_t version = 2;
  static inline const std::string hash = "c12818c51418fbfc76f1b81eba6678d75cf526c1";
  static constexpr uint32_t TRANSACTION_registerSender = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_registerPassiveSender = FIRST_CALL_TRANSACTION + 1;

  static std::shared_ptr<IClientManager> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IClientManager>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IClientManager>* instance);
  virtual ::ndk::ScopedAStatus registerSender(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IAccessor>& in_bufferPool, ::aidl::android::hardware::media::bufferpool2::IClientManager::Registration* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus registerPassiveSender(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IAccessor>& in_bufferPool, ::aidl::android::hardware::media::bufferpool2::IClientManager::Registration* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IClientManagerDefault : public IClientManager {
public:
  ::ndk::ScopedAStatus registerSender(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IAccessor>& in_bufferPool, ::aidl::android::hardware::media::bufferpool2::IClientManager::Registration* _aidl_return) override;
  ::ndk::ScopedAStatus registerPassiveSender(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IAccessor>& in_bufferPool, ::aidl::android::hardware::media::bufferpool2::IClientManager::Registration* _aidl_return) override;
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
