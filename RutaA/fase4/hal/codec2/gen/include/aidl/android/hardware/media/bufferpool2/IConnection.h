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
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <android/binder_enums.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#include <aidl/android/hardware/media/bufferpool2/Buffer.h>
#include <aidl/android/hardware/media/bufferpool2/IConnection.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

#ifndef __BIONIC__
#define __assert2(a,b,c,d) ((void)0)
#endif

namespace aidl::android::hardware::media::bufferpool2 {
class Buffer;
}  // namespace aidl::android::hardware::media::bufferpool2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
class IConnectionDelegator;

class IConnection : public ::ndk::ICInterface {
public:
  typedef IConnectionDelegator DefaultDelegator;
  static const char* descriptor;
  IConnection();
  virtual ~IConnection();

  class FetchInfo {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int64_t transactionId = 0L;
    int32_t bufferId = 0;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const FetchInfo& _rhs) const {
      return std::tie(transactionId, bufferId) == std::tie(_rhs.transactionId, _rhs.bufferId);
    }
    inline bool operator<(const FetchInfo& _rhs) const {
      return std::tie(transactionId, bufferId) < std::tie(_rhs.transactionId, _rhs.bufferId);
    }
    inline bool operator!=(const FetchInfo& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const FetchInfo& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const FetchInfo& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const FetchInfo& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "FetchInfo{";
      _aidl_os << "transactionId: " << ::android::internal::ToString(transactionId);
      _aidl_os << ", bufferId: " << ::android::internal::ToString(bufferId);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class FetchResult {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    enum class Tag : int32_t {
      buffer = 0,
      failure = 1,
    };

    // Expose tag symbols for legacy code
    static const inline Tag buffer = Tag::buffer;
    static const inline Tag failure = Tag::failure;

    template<typename _Tp>
    static constexpr bool _not_self = !std::is_same_v<std::remove_cv_t<std::remove_reference_t<_Tp>>, FetchResult>;

    FetchResult() : _value(std::in_place_index<static_cast<size_t>(buffer)>, ::aidl::android::hardware::media::bufferpool2::Buffer()) { }

    template <typename _Tp, typename = std::enable_if_t<
        _not_self<_Tp> &&
        std::is_constructible_v<std::variant<::aidl::android::hardware::media::bufferpool2::Buffer, int32_t>, _Tp>
      >>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr FetchResult(_Tp&& _arg)
        : _value(std::forward<_Tp>(_arg)) {}

    template <size_t _Np, typename... _Tp>
    constexpr explicit FetchResult(std::in_place_index_t<_Np>, _Tp&&... _args)
        : _value(std::in_place_index<_Np>, std::forward<_Tp>(_args)...) {}

    template <Tag _tag, typename... _Tp>
    static FetchResult make(_Tp&&... _args) {
      return FetchResult(std::in_place_index<static_cast<size_t>(_tag)>, std::forward<_Tp>(_args)...);
    }

    template <Tag _tag, typename _Tp, typename... _Up>
    static FetchResult make(std::initializer_list<_Tp> _il, _Up&&... _args) {
      return FetchResult(std::in_place_index<static_cast<size_t>(_tag)>, std::move(_il), std::forward<_Up>(_args)...);
    }

    Tag getTag() const {
      return static_cast<Tag>(_value.index());
    }

    template <Tag _tag>
    const auto& get() const {
      if (getTag() != _tag) { __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "bad access: a wrong tag"); }
      return std::get<static_cast<size_t>(_tag)>(_value);
    }

    template <Tag _tag>
    auto& get() {
      if (getTag() != _tag) { __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "bad access: a wrong tag"); }
      return std::get<static_cast<size_t>(_tag)>(_value);
    }

    template <Tag _tag, typename... _Tp>
    void set(_Tp&&... _args) {
      _value.emplace<static_cast<size_t>(_tag)>(std::forward<_Tp>(_args)...);
    }

    binder_status_t readFromParcel(const AParcel* _parcel);
    binder_status_t writeToParcel(AParcel* _parcel) const;

    inline bool operator==(const FetchResult& _rhs) const {
      return _value == _rhs._value;
    }
    inline bool operator<(const FetchResult& _rhs) const {
      return _value < _rhs._value;
    }
    inline bool operator!=(const FetchResult& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const FetchResult& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const FetchResult& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const FetchResult& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream os;
      os << "FetchResult{";
      switch (getTag()) {
      case buffer: os << "buffer: " << ::android::internal::ToString(get<buffer>()); break;
      case failure: os << "failure: " << ::android::internal::ToString(get<failure>()); break;
      }
      os << "}";
      return os.str();
    }
  private:
    std::variant<::aidl::android::hardware::media::bufferpool2::Buffer, int32_t> _value;
  };
  static inline const int32_t version = 2;
  static inline const std::string hash = "c12818c51418fbfc76f1b81eba6678d75cf526c1";
  static constexpr uint32_t TRANSACTION_fetch = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_sync = FIRST_CALL_TRANSACTION + 1;

  static std::shared_ptr<IConnection> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IConnection>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IConnection>* instance);
  virtual ::ndk::ScopedAStatus fetch(const std::vector<::aidl::android::hardware::media::bufferpool2::IConnection::FetchInfo>& in_fetchInfos, std::vector<::aidl::android::hardware::media::bufferpool2::IConnection::FetchResult>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus sync() = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IConnectionDefault : public IConnection {
public:
  ::ndk::ScopedAStatus fetch(const std::vector<::aidl::android::hardware::media::bufferpool2::IConnection::FetchInfo>& in_fetchInfos, std::vector<::aidl::android::hardware::media::bufferpool2::IConnection::FetchResult>* _aidl_return) override;
  ::ndk::ScopedAStatus sync() override;
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
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace bufferpool2 {
[[nodiscard]] static inline std::string toString(IConnection::FetchResult::Tag val) {
  switch(val) {
  case IConnection::FetchResult::Tag::buffer:
    return "buffer";
  case IConnection::FetchResult::Tag::failure:
    return "failure";
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
constexpr inline std::array<aidl::android::hardware::media::bufferpool2::IConnection::FetchResult::Tag, 2> enum_values<aidl::android::hardware::media::bufferpool2::IConnection::FetchResult::Tag> = {
  aidl::android::hardware::media::bufferpool2::IConnection::FetchResult::Tag::buffer,
  aidl::android::hardware::media::bufferpool2::IConnection::FetchResult::Tag::failure,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
