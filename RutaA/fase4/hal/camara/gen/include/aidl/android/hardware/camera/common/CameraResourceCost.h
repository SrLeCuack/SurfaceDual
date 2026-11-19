/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=1 --hash=d1a423213d80e15de2b10e54d908ac5c29644fef -I C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\CameraDeviceStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\CameraMetadataType.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\CameraResourceCost.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\TagBoundaryId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\TorchModeStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\VendorTag.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\common\\VendorTagSection.aidl
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
namespace common {
class CameraResourceCost {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  int32_t resourceCost = 0;
  std::vector<std::string> conflictingDevices;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const CameraResourceCost& _rhs) const {
    return std::tie(resourceCost, conflictingDevices) == std::tie(_rhs.resourceCost, _rhs.conflictingDevices);
  }
  inline bool operator<(const CameraResourceCost& _rhs) const {
    return std::tie(resourceCost, conflictingDevices) < std::tie(_rhs.resourceCost, _rhs.conflictingDevices);
  }
  inline bool operator!=(const CameraResourceCost& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const CameraResourceCost& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const CameraResourceCost& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const CameraResourceCost& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "CameraResourceCost{";
    _aidl_os << "resourceCost: " << ::android::internal::ToString(resourceCost);
    _aidl_os << ", conflictingDevices: " << ::android::internal::ToString(conflictingDevices);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
