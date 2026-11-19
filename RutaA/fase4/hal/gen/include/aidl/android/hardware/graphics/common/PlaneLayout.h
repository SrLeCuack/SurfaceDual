/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=7 --hash=83e58c8276ecf8c32a567b3830cd4d1c0b43bfdb -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\AlphaInterpretation.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\BlendMode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\BufferUsage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\ChromaSiting.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\ColorTransform.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Compression.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Cta861_3.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Dataspace.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\DisplayDecorationSupport.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\DisplayHotplugEvent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\ExtendableType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\FRect.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HardwareBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HardwareBufferDescription.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Hdr.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HdrConversionCapability.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\HdrConversionStrategy.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Interlaced.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PixelFormat.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PlaneLayout.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PlaneLayoutComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\PlaneLayoutComponentType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Point.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Rect.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Smpte2086.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\StandardMetadataType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\Transform.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\graphics\\common\\XyColor.aidl
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
#include <aidl/android/hardware/graphics/common/PlaneLayoutComponent.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::graphics::common {
class PlaneLayoutComponent;
}  // namespace aidl::android::hardware::graphics::common
namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
class PlaneLayout {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  std::vector<::aidl::android::hardware::graphics::common::PlaneLayoutComponent> components;
  int64_t offsetInBytes = 0L;
  int64_t sampleIncrementInBits = 0L;
  int64_t strideInBytes = 0L;
  int64_t widthInSamples = 0L;
  int64_t heightInSamples = 0L;
  int64_t totalSizeInBytes = 0L;
  int64_t horizontalSubsampling = 0L;
  int64_t verticalSubsampling = 0L;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const PlaneLayout& _rhs) const {
    return std::tie(components, offsetInBytes, sampleIncrementInBits, strideInBytes, widthInSamples, heightInSamples, totalSizeInBytes, horizontalSubsampling, verticalSubsampling) == std::tie(_rhs.components, _rhs.offsetInBytes, _rhs.sampleIncrementInBits, _rhs.strideInBytes, _rhs.widthInSamples, _rhs.heightInSamples, _rhs.totalSizeInBytes, _rhs.horizontalSubsampling, _rhs.verticalSubsampling);
  }
  inline bool operator<(const PlaneLayout& _rhs) const {
    return std::tie(components, offsetInBytes, sampleIncrementInBits, strideInBytes, widthInSamples, heightInSamples, totalSizeInBytes, horizontalSubsampling, verticalSubsampling) < std::tie(_rhs.components, _rhs.offsetInBytes, _rhs.sampleIncrementInBits, _rhs.strideInBytes, _rhs.widthInSamples, _rhs.heightInSamples, _rhs.totalSizeInBytes, _rhs.horizontalSubsampling, _rhs.verticalSubsampling);
  }
  inline bool operator!=(const PlaneLayout& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const PlaneLayout& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const PlaneLayout& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const PlaneLayout& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "PlaneLayout{";
    _aidl_os << "components: " << ::android::internal::ToString(components);
    _aidl_os << ", offsetInBytes: " << ::android::internal::ToString(offsetInBytes);
    _aidl_os << ", sampleIncrementInBits: " << ::android::internal::ToString(sampleIncrementInBits);
    _aidl_os << ", strideInBytes: " << ::android::internal::ToString(strideInBytes);
    _aidl_os << ", widthInSamples: " << ::android::internal::ToString(widthInSamples);
    _aidl_os << ", heightInSamples: " << ::android::internal::ToString(heightInSamples);
    _aidl_os << ", totalSizeInBytes: " << ::android::internal::ToString(totalSizeInBytes);
    _aidl_os << ", horizontalSubsampling: " << ::android::internal::ToString(horizontalSubsampling);
    _aidl_os << ", verticalSubsampling: " << ::android::internal::ToString(verticalSubsampling);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
