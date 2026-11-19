/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=261437c3d7d6ad09d5560eee5196a28c0b27106e -I C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\CameraIdAndStreamCombination.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\ConcurrentCameraIdCombination.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\ICameraProvider.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\ICameraProviderCallback.aidl
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
namespace camera {
namespace provider {
class ConcurrentCameraIdCombination {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  std::vector<std::string> combination;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const ConcurrentCameraIdCombination& _rhs) const {
    return std::tie(combination) == std::tie(_rhs.combination);
  }
  inline bool operator<(const ConcurrentCameraIdCombination& _rhs) const {
    return std::tie(combination) < std::tie(_rhs.combination);
  }
  inline bool operator!=(const ConcurrentCameraIdCombination& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const ConcurrentCameraIdCombination& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const ConcurrentCameraIdCombination& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const ConcurrentCameraIdCombination& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "ConcurrentCameraIdCombination{";
    _aidl_os << "combination: " << ::android::internal::ToString(combination);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
