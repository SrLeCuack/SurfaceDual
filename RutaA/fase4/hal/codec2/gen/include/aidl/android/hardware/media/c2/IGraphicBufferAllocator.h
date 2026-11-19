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
#include <android/hardware_buffer_aidl.h>
#include <aidl/android/hardware/media/c2/IGraphicBufferAllocator.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
class IGraphicBufferAllocatorDelegator;

class IGraphicBufferAllocator : public ::ndk::ICInterface {
public:
  typedef IGraphicBufferAllocatorDelegator DefaultDelegator;
  static const char* descriptor;
  IGraphicBufferAllocator();
  virtual ~IGraphicBufferAllocator();

  class Allocation {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    ::aidl::android::hardware::HardwareBuffer buffer;
    ::ndk::ScopedFileDescriptor fence;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const Allocation& _rhs) const {
      return std::tie(buffer, fence) == std::tie(_rhs.buffer, _rhs.fence);
    }
    inline bool operator<(const Allocation& _rhs) const {
      return std::tie(buffer, fence) < std::tie(_rhs.buffer, _rhs.fence);
    }
    inline bool operator!=(const Allocation& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const Allocation& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const Allocation& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const Allocation& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "Allocation{";
      _aidl_os << "buffer: " << ::android::internal::ToString(buffer);
      _aidl_os << ", fence: " << ::android::internal::ToString(fence);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class Description {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    int32_t width = 0;
    int32_t height = 0;
    int32_t format = 0;
    int64_t usage = 0L;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const Description& _rhs) const {
      return std::tie(width, height, format, usage) == std::tie(_rhs.width, _rhs.height, _rhs.format, _rhs.usage);
    }
    inline bool operator<(const Description& _rhs) const {
      return std::tie(width, height, format, usage) < std::tie(_rhs.width, _rhs.height, _rhs.format, _rhs.usage);
    }
    inline bool operator!=(const Description& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const Description& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const Description& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const Description& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "Description{";
      _aidl_os << "width: " << ::android::internal::ToString(width);
      _aidl_os << ", height: " << ::android::internal::ToString(height);
      _aidl_os << ", format: " << ::android::internal::ToString(format);
      _aidl_os << ", usage: " << ::android::internal::ToString(usage);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  static inline const int32_t version = 2;
  static inline const std::string hash = "2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe";
  static constexpr uint32_t TRANSACTION_allocate = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_deallocate = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_getWaitableFd = FIRST_CALL_TRANSACTION + 2;

  static std::shared_ptr<IGraphicBufferAllocator> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IGraphicBufferAllocator>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IGraphicBufferAllocator>* instance);
  virtual ::ndk::ScopedAStatus allocate(const ::aidl::android::hardware::media::c2::IGraphicBufferAllocator::Description& in_desc, ::aidl::android::hardware::media::c2::IGraphicBufferAllocator::Allocation* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus deallocate(int64_t in_id, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getWaitableFd(::ndk::ScopedFileDescriptor* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IGraphicBufferAllocatorDefault : public IGraphicBufferAllocator {
public:
  ::ndk::ScopedAStatus allocate(const ::aidl::android::hardware::media::c2::IGraphicBufferAllocator::Description& in_desc, ::aidl::android::hardware::media::c2::IGraphicBufferAllocator::Allocation* _aidl_return) override;
  ::ndk::ScopedAStatus deallocate(int64_t in_id, bool* _aidl_return) override;
  ::ndk::ScopedAStatus getWaitableFd(::ndk::ScopedFileDescriptor* _aidl_return) override;
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
