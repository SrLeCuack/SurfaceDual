/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=ac7c7f74993871015f73daff1ea0c7f4d4f85ca7 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferCache.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferRequestStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraBlob.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraBlobId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraMetadata.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraOfflineSessionInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CaptureRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CaptureResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ConfigureStreamsRet.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ErrorCode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ErrorMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\HalStream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDevice.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDeviceCallback.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDeviceSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraInjectionSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraOfflineSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\NotifyMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\OfflineRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\OfflineStream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\PhysicalCameraMetadata.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\PhysicalCameraSetting.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\RequestTemplate.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ShutterMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\Stream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBufferRequestError.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBufferRet.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBuffersVal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamConfiguration.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamConfigurationMode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamRotation.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamType.aidl
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
#include <aidl/android/hardware/camera/device/StreamRotation.h>
#include <aidl/android/hardware/camera/device/StreamType.h>
#include <aidl/android/hardware/camera/metadata/RequestAvailableDynamicRangeProfilesMap.h>
#include <aidl/android/hardware/camera/metadata/ScalerAvailableStreamUseCases.h>
#include <aidl/android/hardware/camera/metadata/SensorPixelMode.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {
class Stream {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  int32_t id = 0;
  ::aidl::android::hardware::camera::device::StreamType streamType = ::aidl::android::hardware::camera::device::StreamType(0);
  int32_t width = 0;
  int32_t height = 0;
  ::aidl::android::hardware::graphics::common::PixelFormat format = ::aidl::android::hardware::graphics::common::PixelFormat(0);
  ::aidl::android::hardware::graphics::common::BufferUsage usage = ::aidl::android::hardware::graphics::common::BufferUsage(0);
  ::aidl::android::hardware::graphics::common::Dataspace dataSpace = ::aidl::android::hardware::graphics::common::Dataspace(0);
  ::aidl::android::hardware::camera::device::StreamRotation rotation = ::aidl::android::hardware::camera::device::StreamRotation(0);
  std::string physicalCameraId;
  int32_t bufferSize = 0;
  int32_t groupId = 0;
  std::vector<::aidl::android::hardware::camera::metadata::SensorPixelMode> sensorPixelModesUsed;
  ::aidl::android::hardware::camera::metadata::RequestAvailableDynamicRangeProfilesMap dynamicRangeProfile = ::aidl::android::hardware::camera::metadata::RequestAvailableDynamicRangeProfilesMap(0);
  ::aidl::android::hardware::camera::metadata::ScalerAvailableStreamUseCases useCase = ::aidl::android::hardware::camera::metadata::ScalerAvailableStreamUseCases(0);
  int32_t colorSpace = 0;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const Stream& _rhs) const {
    return std::tie(id, streamType, width, height, format, usage, dataSpace, rotation, physicalCameraId, bufferSize, groupId, sensorPixelModesUsed, dynamicRangeProfile, useCase, colorSpace) == std::tie(_rhs.id, _rhs.streamType, _rhs.width, _rhs.height, _rhs.format, _rhs.usage, _rhs.dataSpace, _rhs.rotation, _rhs.physicalCameraId, _rhs.bufferSize, _rhs.groupId, _rhs.sensorPixelModesUsed, _rhs.dynamicRangeProfile, _rhs.useCase, _rhs.colorSpace);
  }
  inline bool operator<(const Stream& _rhs) const {
    return std::tie(id, streamType, width, height, format, usage, dataSpace, rotation, physicalCameraId, bufferSize, groupId, sensorPixelModesUsed, dynamicRangeProfile, useCase, colorSpace) < std::tie(_rhs.id, _rhs.streamType, _rhs.width, _rhs.height, _rhs.format, _rhs.usage, _rhs.dataSpace, _rhs.rotation, _rhs.physicalCameraId, _rhs.bufferSize, _rhs.groupId, _rhs.sensorPixelModesUsed, _rhs.dynamicRangeProfile, _rhs.useCase, _rhs.colorSpace);
  }
  inline bool operator!=(const Stream& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const Stream& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const Stream& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const Stream& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "Stream{";
    _aidl_os << "id: " << ::android::internal::ToString(id);
    _aidl_os << ", streamType: " << ::android::internal::ToString(streamType);
    _aidl_os << ", width: " << ::android::internal::ToString(width);
    _aidl_os << ", height: " << ::android::internal::ToString(height);
    _aidl_os << ", format: " << ::android::internal::ToString(format);
    _aidl_os << ", usage: " << ::android::internal::ToString(usage);
    _aidl_os << ", dataSpace: " << ::android::internal::ToString(dataSpace);
    _aidl_os << ", rotation: " << ::android::internal::ToString(rotation);
    _aidl_os << ", physicalCameraId: " << ::android::internal::ToString(physicalCameraId);
    _aidl_os << ", bufferSize: " << ::android::internal::ToString(bufferSize);
    _aidl_os << ", groupId: " << ::android::internal::ToString(groupId);
    _aidl_os << ", sensorPixelModesUsed: " << ::android::internal::ToString(sensorPixelModesUsed);
    _aidl_os << ", dynamicRangeProfile: " << ::android::internal::ToString(dynamicRangeProfile);
    _aidl_os << ", useCase: " << ::android::internal::ToString(useCase);
    _aidl_os << ", colorSpace: " << ::android::internal::ToString(colorSpace);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
