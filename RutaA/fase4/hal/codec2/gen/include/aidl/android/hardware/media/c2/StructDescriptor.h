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
#include <aidl/android/hardware/media/c2/FieldDescriptor.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::media::c2 {
class FieldDescriptor;
}  // namespace aidl::android::hardware::media::c2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
class StructDescriptor {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  int32_t type = 0;
  std::vector<::aidl::android::hardware::media::c2::FieldDescriptor> fields;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const StructDescriptor& _rhs) const {
    return std::tie(type, fields) == std::tie(_rhs.type, _rhs.fields);
  }
  inline bool operator<(const StructDescriptor& _rhs) const {
    return std::tie(type, fields) < std::tie(_rhs.type, _rhs.fields);
  }
  inline bool operator!=(const StructDescriptor& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const StructDescriptor& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const StructDescriptor& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const StructDescriptor& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "StructDescriptor{";
    _aidl_os << "type: " << ::android::internal::ToString(type);
    _aidl_os << ", fields: " << ::android::internal::ToString(fields);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
