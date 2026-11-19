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
#include <aidl/android/hardware/camera/common/VendorTag.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::camera::common {
class VendorTag;
}  // namespace aidl::android::hardware::camera::common
namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace common {
class VendorTagSection {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  std::string sectionName;
  std::vector<::aidl::android::hardware::camera::common::VendorTag> tags;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const VendorTagSection& _rhs) const {
    return std::tie(sectionName, tags) == std::tie(_rhs.sectionName, _rhs.tags);
  }
  inline bool operator<(const VendorTagSection& _rhs) const {
    return std::tie(sectionName, tags) < std::tie(_rhs.sectionName, _rhs.tags);
  }
  inline bool operator!=(const VendorTagSection& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const VendorTagSection& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const VendorTagSection& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const VendorTagSection& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "VendorTagSection{";
    _aidl_os << "sectionName: " << ::android::internal::ToString(sectionName);
    _aidl_os << ", tags: " << ::android::internal::ToString(tags);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
