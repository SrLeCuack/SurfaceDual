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
#include <aidl/android/hardware/camera/device/CameraMetadata.h>
#include <aidl/android/hardware/camera/device/PhysicalCameraMetadata.h>
#include <aidl/android/hardware/camera/device/StreamBuffer.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::camera::device {
class CameraMetadata;
class PhysicalCameraMetadata;
class StreamBuffer;
}  // namespace aidl::android::hardware::camera::device
namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {
class CaptureResult {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  int32_t frameNumber = 0;
  int64_t fmqResultSize = 0L;
  ::aidl::android::hardware::camera::device::CameraMetadata result;
  std::vector<::aidl::android::hardware::camera::device::StreamBuffer> outputBuffers;
  ::aidl::android::hardware::camera::device::StreamBuffer inputBuffer;
  int32_t partialResult = 0;
  std::vector<::aidl::android::hardware::camera::device::PhysicalCameraMetadata> physicalCameraMetadata;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const CaptureResult& _rhs) const {
    return std::tie(frameNumber, fmqResultSize, result, outputBuffers, inputBuffer, partialResult, physicalCameraMetadata) == std::tie(_rhs.frameNumber, _rhs.fmqResultSize, _rhs.result, _rhs.outputBuffers, _rhs.inputBuffer, _rhs.partialResult, _rhs.physicalCameraMetadata);
  }
  inline bool operator<(const CaptureResult& _rhs) const {
    return std::tie(frameNumber, fmqResultSize, result, outputBuffers, inputBuffer, partialResult, physicalCameraMetadata) < std::tie(_rhs.frameNumber, _rhs.fmqResultSize, _rhs.result, _rhs.outputBuffers, _rhs.inputBuffer, _rhs.partialResult, _rhs.physicalCameraMetadata);
  }
  inline bool operator!=(const CaptureResult& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const CaptureResult& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const CaptureResult& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const CaptureResult& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "CaptureResult{";
    _aidl_os << "frameNumber: " << ::android::internal::ToString(frameNumber);
    _aidl_os << ", fmqResultSize: " << ::android::internal::ToString(fmqResultSize);
    _aidl_os << ", result: " << ::android::internal::ToString(result);
    _aidl_os << ", outputBuffers: " << ::android::internal::ToString(outputBuffers);
    _aidl_os << ", inputBuffer: " << ::android::internal::ToString(inputBuffer);
    _aidl_os << ", partialResult: " << ::android::internal::ToString(partialResult);
    _aidl_os << ", physicalCameraMetadata: " << ::android::internal::ToString(physicalCameraMetadata);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
