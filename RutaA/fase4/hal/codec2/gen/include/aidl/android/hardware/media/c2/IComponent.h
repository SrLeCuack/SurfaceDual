/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\BaseBlock.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Block.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQuery.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQueryResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FrameData.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentInterface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentListener.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentStore.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IConfigurable.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSink.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurfaceConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IPooledGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\InfoBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamField.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamFieldValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Params.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\SettingResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\StructDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ValueRange.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Work.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkBundle.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkOrdinal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Worklet.aidl
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
#include <aidl/android/hardware/common/NativeHandle.h>
#include <aidl/android/hardware/media/c2/IComponent.h>
#include <aidl/android/hardware/media/c2/IComponentInterface.h>
#include <aidl/android/hardware/media/c2/IConfigurable.h>
#include <aidl/android/hardware/media/c2/IGraphicBufferAllocator.h>
#include <aidl/android/hardware/media/c2/IInputSink.h>
#include <aidl/android/hardware/media/c2/IInputSurface.h>
#include <aidl/android/hardware/media/c2/IInputSurfaceConnection.h>
#include <aidl/android/hardware/media/c2/IPooledGraphicBufferAllocator.h>
#include <aidl/android/hardware/media/c2/WorkBundle.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::common {
class NativeHandle;
}  // namespace aidl::android::hardware::common
namespace aidl::android::hardware::media::c2 {
class IComponentInterface;
class IConfigurable;
class IGraphicBufferAllocator;
class IInputSink;
class IInputSurface;
class IInputSurfaceConnection;
class IPooledGraphicBufferAllocator;
class WorkBundle;
}  // namespace aidl::android::hardware::media::c2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
class IComponentDelegator;

class IComponent : public ::ndk::ICInterface {
public:
  typedef IComponentDelegator DefaultDelegator;
  static const char* descriptor;
  IComponent();
  virtual ~IComponent();

  class BlockPool {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int64_t blockPoolId = 0L;
    std::shared_ptr<::aidl::android::hardware::media::c2::IConfigurable> configurable;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const BlockPool& _rhs) const {
      return std::tie(blockPoolId, configurable) == std::tie(_rhs.blockPoolId, _rhs.configurable);
    }
    inline bool operator<(const BlockPool& _rhs) const {
      return std::tie(blockPoolId, configurable) < std::tie(_rhs.blockPoolId, _rhs.configurable);
    }
    inline bool operator!=(const BlockPool& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const BlockPool& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const BlockPool& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const BlockPool& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "BlockPool{";
      _aidl_os << "blockPoolId: " << ::android::internal::ToString(blockPoolId);
      _aidl_os << ", configurable: " << ::android::internal::ToString(configurable);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class GbAllocator {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    ::ndk::ScopedFileDescriptor waitableFd;
    std::shared_ptr<::aidl::android::hardware::media::c2::IGraphicBufferAllocator> igba;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const GbAllocator& _rhs) const {
      return std::tie(waitableFd, igba) == std::tie(_rhs.waitableFd, _rhs.igba);
    }
    inline bool operator<(const GbAllocator& _rhs) const {
      return std::tie(waitableFd, igba) < std::tie(_rhs.waitableFd, _rhs.igba);
    }
    inline bool operator!=(const GbAllocator& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const GbAllocator& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const GbAllocator& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const GbAllocator& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "GbAllocator{";
      _aidl_os << "waitableFd: " << ::android::internal::ToString(waitableFd);
      _aidl_os << ", igba: " << ::android::internal::ToString(igba);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class PooledGbAllocator {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    ::ndk::ScopedFileDescriptor waitableFd;
    int64_t receiverId = 0L;
    std::shared_ptr<::aidl::android::hardware::media::c2::IPooledGraphicBufferAllocator> ipgba;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const PooledGbAllocator& _rhs) const {
      return std::tie(waitableFd, receiverId, ipgba) == std::tie(_rhs.waitableFd, _rhs.receiverId, _rhs.ipgba);
    }
    inline bool operator<(const PooledGbAllocator& _rhs) const {
      return std::tie(waitableFd, receiverId, ipgba) < std::tie(_rhs.waitableFd, _rhs.receiverId, _rhs.ipgba);
    }
    inline bool operator!=(const PooledGbAllocator& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const PooledGbAllocator& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const PooledGbAllocator& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const PooledGbAllocator& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "PooledGbAllocator{";
      _aidl_os << "waitableFd: " << ::android::internal::ToString(waitableFd);
      _aidl_os << ", receiverId: " << ::android::internal::ToString(receiverId);
      _aidl_os << ", ipgba: " << ::android::internal::ToString(ipgba);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class BlockPoolAllocator {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t allocatorId = 0;
    std::optional<::aidl::android::hardware::media::c2::IComponent::GbAllocator> gbAllocator;
    std::optional<::aidl::android::hardware::media::c2::IComponent::PooledGbAllocator> pooledGbAllocator;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const BlockPoolAllocator& _rhs) const {
      return std::tie(allocatorId, gbAllocator, pooledGbAllocator) == std::tie(_rhs.allocatorId, _rhs.gbAllocator, _rhs.pooledGbAllocator);
    }
    inline bool operator<(const BlockPoolAllocator& _rhs) const {
      return std::tie(allocatorId, gbAllocator, pooledGbAllocator) < std::tie(_rhs.allocatorId, _rhs.gbAllocator, _rhs.pooledGbAllocator);
    }
    inline bool operator!=(const BlockPoolAllocator& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const BlockPoolAllocator& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const BlockPoolAllocator& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const BlockPoolAllocator& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "BlockPoolAllocator{";
      _aidl_os << "allocatorId: " << ::android::internal::ToString(allocatorId);
      _aidl_os << ", gbAllocator: " << ::android::internal::ToString(gbAllocator);
      _aidl_os << ", pooledGbAllocator: " << ::android::internal::ToString(pooledGbAllocator);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  static inline const int32_t version = 2;
  static inline const std::string hash = "2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe";
  static constexpr uint32_t TRANSACTION_configureVideoTunnel = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_createBlockPool = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_destroyBlockPool = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_drain = FIRST_CALL_TRANSACTION + 3;
  static constexpr uint32_t TRANSACTION_flush = FIRST_CALL_TRANSACTION + 4;
  static constexpr uint32_t TRANSACTION_getInterface = FIRST_CALL_TRANSACTION + 5;
  static constexpr uint32_t TRANSACTION_queue = FIRST_CALL_TRANSACTION + 6;
  static constexpr uint32_t TRANSACTION_release = FIRST_CALL_TRANSACTION + 7;
  static constexpr uint32_t TRANSACTION_reset = FIRST_CALL_TRANSACTION + 8;
  static constexpr uint32_t TRANSACTION_start = FIRST_CALL_TRANSACTION + 9;
  static constexpr uint32_t TRANSACTION_stop = FIRST_CALL_TRANSACTION + 10;
  static constexpr uint32_t TRANSACTION_connectToInputSurface = FIRST_CALL_TRANSACTION + 11;
  static constexpr uint32_t TRANSACTION_asInputSink = FIRST_CALL_TRANSACTION + 12;

  static std::shared_ptr<IComponent> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IComponent>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IComponent>* instance);
  virtual ::ndk::ScopedAStatus configureVideoTunnel(int32_t in_avSyncHwId, ::aidl::android::hardware::common::NativeHandle* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus createBlockPool(const ::aidl::android::hardware::media::c2::IComponent::BlockPoolAllocator& in_allocator, ::aidl::android::hardware::media::c2::IComponent::BlockPool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus destroyBlockPool(int64_t in_blockPoolId) = 0;
  virtual ::ndk::ScopedAStatus drain(bool in_withEos) = 0;
  virtual ::ndk::ScopedAStatus flush(::aidl::android::hardware::media::c2::WorkBundle* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterface(std::shared_ptr<::aidl::android::hardware::media::c2::IComponentInterface>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus queue(const ::aidl::android::hardware::media::c2::WorkBundle& in_workBundle) = 0;
  virtual ::ndk::ScopedAStatus release() = 0;
  virtual ::ndk::ScopedAStatus reset() = 0;
  virtual ::ndk::ScopedAStatus start() = 0;
  virtual ::ndk::ScopedAStatus stop() = 0;
  virtual ::ndk::ScopedAStatus connectToInputSurface(const std::shared_ptr<::aidl::android::hardware::media::c2::IInputSurface>& in_inputSurface, std::shared_ptr<::aidl::android::hardware::media::c2::IInputSurfaceConnection>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus asInputSink(std::shared_ptr<::aidl::android::hardware::media::c2::IInputSink>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IComponentDefault : public IComponent {
public:
  ::ndk::ScopedAStatus configureVideoTunnel(int32_t in_avSyncHwId, ::aidl::android::hardware::common::NativeHandle* _aidl_return) override;
  ::ndk::ScopedAStatus createBlockPool(const ::aidl::android::hardware::media::c2::IComponent::BlockPoolAllocator& in_allocator, ::aidl::android::hardware::media::c2::IComponent::BlockPool* _aidl_return) override;
  ::ndk::ScopedAStatus destroyBlockPool(int64_t in_blockPoolId) override;
  ::ndk::ScopedAStatus drain(bool in_withEos) override;
  ::ndk::ScopedAStatus flush(::aidl::android::hardware::media::c2::WorkBundle* _aidl_return) override;
  ::ndk::ScopedAStatus getInterface(std::shared_ptr<::aidl::android::hardware::media::c2::IComponentInterface>* _aidl_return) override;
  ::ndk::ScopedAStatus queue(const ::aidl::android::hardware::media::c2::WorkBundle& in_workBundle) override;
  ::ndk::ScopedAStatus release() override;
  ::ndk::ScopedAStatus reset() override;
  ::ndk::ScopedAStatus start() override;
  ::ndk::ScopedAStatus stop() override;
  ::ndk::ScopedAStatus connectToInputSurface(const std::shared_ptr<::aidl::android::hardware::media::c2::IInputSurface>& in_inputSurface, std::shared_ptr<::aidl::android::hardware::media::c2::IInputSurfaceConnection>* _aidl_return) override;
  ::ndk::ScopedAStatus asInputSink(std::shared_ptr<::aidl::android::hardware::media::c2::IInputSink>* _aidl_return) override;
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
