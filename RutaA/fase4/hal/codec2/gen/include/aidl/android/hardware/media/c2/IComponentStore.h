/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\BaseBlock.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Block.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQuery.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQueryResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FrameData.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentInterface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentListener.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentStore.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IConfigurable.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSink.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurfaceConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IPooledGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\InfoBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamField.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamFieldValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Params.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\SettingResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\StructDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ValueRange.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Work.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkBundle.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkOrdinal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Worklet.aidl
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
#include <android/binder_interface_utils.h>
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#include <aidl/android/hardware/media/bufferpool2/IClientManager.h>
#include <aidl/android/hardware/media/c2/Buffer.h>
#include <aidl/android/hardware/media/c2/IComponent.h>
#include <aidl/android/hardware/media/c2/IComponentInterface.h>
#include <aidl/android/hardware/media/c2/IComponentListener.h>
#include <aidl/android/hardware/media/c2/IComponentStore.h>
#include <aidl/android/hardware/media/c2/IConfigurable.h>
#include <aidl/android/hardware/media/c2/IInputSurface.h>
#include <aidl/android/hardware/media/c2/StructDescriptor.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::media::bufferpool2 {
class IClientManager;
}  // namespace aidl::android::hardware::media::bufferpool2
namespace aidl::android::hardware::media::c2 {
class Buffer;
class IComponent;
class IComponentInterface;
class IComponentListener;
class IConfigurable;
class IInputSurface;
class StructDescriptor;
}  // namespace aidl::android::hardware::media::c2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
class IComponentStoreDelegator;

class IComponentStore : public ::ndk::ICInterface {
public:
  typedef IComponentStoreDelegator DefaultDelegator;
  static const char* descriptor;
  IComponentStore();
  virtual ~IComponentStore();

  class ComponentTraits {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    enum class Kind : int32_t {
      OTHER = 0,
      DECODER = 1,
      ENCODER = 2,
    };

    enum class Domain : int32_t {
      OTHER = 0,
      VIDEO = 1,
      AUDIO = 2,
      IMAGE = 3,
    };

    std::string name;
    ::aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain domain = ::aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain(0);
    ::aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Kind kind = ::aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Kind(0);
    int32_t rank = 0;
    std::string mediaType;
    std::vector<std::string> aliases;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const ComponentTraits& _rhs) const {
      return std::tie(name, domain, kind, rank, mediaType, aliases) == std::tie(_rhs.name, _rhs.domain, _rhs.kind, _rhs.rank, _rhs.mediaType, _rhs.aliases);
    }
    inline bool operator<(const ComponentTraits& _rhs) const {
      return std::tie(name, domain, kind, rank, mediaType, aliases) < std::tie(_rhs.name, _rhs.domain, _rhs.kind, _rhs.rank, _rhs.mediaType, _rhs.aliases);
    }
    inline bool operator!=(const ComponentTraits& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const ComponentTraits& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const ComponentTraits& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const ComponentTraits& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "ComponentTraits{";
      _aidl_os << "name: " << ::android::internal::ToString(name);
      _aidl_os << ", domain: " << ::android::internal::ToString(domain);
      _aidl_os << ", kind: " << ::android::internal::ToString(kind);
      _aidl_os << ", rank: " << ::android::internal::ToString(rank);
      _aidl_os << ", mediaType: " << ::android::internal::ToString(mediaType);
      _aidl_os << ", aliases: " << ::android::internal::ToString(aliases);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  static inline const int32_t version = 2;
  static inline const std::string hash = "2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe";
  static constexpr uint32_t TRANSACTION_copyBuffer = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_createComponent = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_createInterface = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_getConfigurable = FIRST_CALL_TRANSACTION + 3;
  static constexpr uint32_t TRANSACTION_getPoolClientManager = FIRST_CALL_TRANSACTION + 4;
  static constexpr uint32_t TRANSACTION_getStructDescriptors = FIRST_CALL_TRANSACTION + 5;
  static constexpr uint32_t TRANSACTION_listComponents = FIRST_CALL_TRANSACTION + 6;
  static constexpr uint32_t TRANSACTION_createInputSurface = FIRST_CALL_TRANSACTION + 7;

  static std::shared_ptr<IComponentStore> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IComponentStore>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IComponentStore>* instance);
  virtual ::ndk::ScopedAStatus copyBuffer(const ::aidl::android::hardware::media::c2::Buffer& in_src, const ::aidl::android::hardware::media::c2::Buffer& in_dst) = 0;
  virtual ::ndk::ScopedAStatus createComponent(const std::string& in_name, const std::shared_ptr<::aidl::android::hardware::media::c2::IComponentListener>& in_listener, const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IClientManager>& in_pool, std::shared_ptr<::aidl::android::hardware::media::c2::IComponent>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus createInterface(const std::string& in_name, std::shared_ptr<::aidl::android::hardware::media::c2::IComponentInterface>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getConfigurable(std::shared_ptr<::aidl::android::hardware::media::c2::IConfigurable>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getPoolClientManager(std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IClientManager>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getStructDescriptors(const std::vector<int32_t>& in_indices, std::vector<::aidl::android::hardware::media::c2::StructDescriptor>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus listComponents(std::vector<::aidl::android::hardware::media::c2::IComponentStore::ComponentTraits>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus createInputSurface(std::shared_ptr<::aidl::android::hardware::media::c2::IInputSurface>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IComponentStoreDefault : public IComponentStore {
public:
  ::ndk::ScopedAStatus copyBuffer(const ::aidl::android::hardware::media::c2::Buffer& in_src, const ::aidl::android::hardware::media::c2::Buffer& in_dst) override;
  ::ndk::ScopedAStatus createComponent(const std::string& in_name, const std::shared_ptr<::aidl::android::hardware::media::c2::IComponentListener>& in_listener, const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IClientManager>& in_pool, std::shared_ptr<::aidl::android::hardware::media::c2::IComponent>* _aidl_return) override;
  ::ndk::ScopedAStatus createInterface(const std::string& in_name, std::shared_ptr<::aidl::android::hardware::media::c2::IComponentInterface>* _aidl_return) override;
  ::ndk::ScopedAStatus getConfigurable(std::shared_ptr<::aidl::android::hardware::media::c2::IConfigurable>* _aidl_return) override;
  ::ndk::ScopedAStatus getPoolClientManager(std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IClientManager>* _aidl_return) override;
  ::ndk::ScopedAStatus getStructDescriptors(const std::vector<int32_t>& in_indices, std::vector<::aidl::android::hardware::media::c2::StructDescriptor>* _aidl_return) override;
  ::ndk::ScopedAStatus listComponents(std::vector<::aidl::android::hardware::media::c2::IComponentStore::ComponentTraits>* _aidl_return) override;
  ::ndk::ScopedAStatus createInputSurface(std::shared_ptr<::aidl::android::hardware::media::c2::IInputSurface>* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
[[nodiscard]] static inline std::string toString(IComponentStore::ComponentTraits::Kind val) {
  switch(val) {
  case IComponentStore::ComponentTraits::Kind::OTHER:
    return "OTHER";
  case IComponentStore::ComponentTraits::Kind::DECODER:
    return "DECODER";
  case IComponentStore::ComponentTraits::Kind::ENCODER:
    return "ENCODER";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Kind, 3> enum_values<aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Kind> = {
  aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Kind::OTHER,
  aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Kind::DECODER,
  aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Kind::ENCODER,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
[[nodiscard]] static inline std::string toString(IComponentStore::ComponentTraits::Domain val) {
  switch(val) {
  case IComponentStore::ComponentTraits::Domain::OTHER:
    return "OTHER";
  case IComponentStore::ComponentTraits::Domain::VIDEO:
    return "VIDEO";
  case IComponentStore::ComponentTraits::Domain::AUDIO:
    return "AUDIO";
  case IComponentStore::ComponentTraits::Domain::IMAGE:
    return "IMAGE";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain, 4> enum_values<aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain> = {
  aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain::OTHER,
  aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain::VIDEO,
  aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain::AUDIO,
  aidl::android::hardware::media::c2::IComponentStore::ComponentTraits::Domain::IMAGE,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
