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
#include <aidl/android/hardware/camera/common/CameraResourceCost.h>
#include <aidl/android/hardware/camera/device/CameraMetadata.h>
#include <aidl/android/hardware/camera/device/ICameraDeviceCallback.h>
#include <aidl/android/hardware/camera/device/ICameraDeviceSession.h>
#include <aidl/android/hardware/camera/device/ICameraInjectionSession.h>
#include <aidl/android/hardware/camera/device/RequestTemplate.h>
#include <aidl/android/hardware/camera/device/StreamConfiguration.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::camera::common {
class CameraResourceCost;
}  // namespace aidl::android::hardware::camera::common
namespace aidl::android::hardware::camera::device {
class CameraMetadata;
class ICameraDeviceCallback;
class ICameraDeviceSession;
class ICameraInjectionSession;
class StreamConfiguration;
}  // namespace aidl::android::hardware::camera::device
namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {
class ICameraDeviceDelegator;

class ICameraDevice : public ::ndk::ICInterface {
public:
  typedef ICameraDeviceDelegator DefaultDelegator;
  static const char* descriptor;
  ICameraDevice();
  virtual ~ICameraDevice();

  static inline const int32_t version = 3;
  static inline const std::string hash = "ac7c7f74993871015f73daff1ea0c7f4d4f85ca7";
  static constexpr uint32_t TRANSACTION_getCameraCharacteristics = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_getPhysicalCameraCharacteristics = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_getResourceCost = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_isStreamCombinationSupported = FIRST_CALL_TRANSACTION + 3;
  static constexpr uint32_t TRANSACTION_open = FIRST_CALL_TRANSACTION + 4;
  static constexpr uint32_t TRANSACTION_openInjectionSession = FIRST_CALL_TRANSACTION + 5;
  static constexpr uint32_t TRANSACTION_setTorchMode = FIRST_CALL_TRANSACTION + 6;
  static constexpr uint32_t TRANSACTION_turnOnTorchWithStrengthLevel = FIRST_CALL_TRANSACTION + 7;
  static constexpr uint32_t TRANSACTION_getTorchStrengthLevel = FIRST_CALL_TRANSACTION + 8;
  static constexpr uint32_t TRANSACTION_constructDefaultRequestSettings = FIRST_CALL_TRANSACTION + 9;
  static constexpr uint32_t TRANSACTION_isStreamCombinationWithSettingsSupported = FIRST_CALL_TRANSACTION + 10;
  static constexpr uint32_t TRANSACTION_getSessionCharacteristics = FIRST_CALL_TRANSACTION + 11;

  static std::shared_ptr<ICameraDevice> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<ICameraDevice>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<ICameraDevice>* instance);
  virtual ::ndk::ScopedAStatus getCameraCharacteristics(::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getPhysicalCameraCharacteristics(const std::string& in_physicalCameraId, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getResourceCost(::aidl::android::hardware::camera::common::CameraResourceCost* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus isStreamCombinationSupported(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_streams, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus open(const std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceCallback>& in_callback, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceSession>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus openInjectionSession(const std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceCallback>& in_callback, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraInjectionSession>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus setTorchMode(bool in_on) = 0;
  virtual ::ndk::ScopedAStatus turnOnTorchWithStrengthLevel(int32_t in_torchStrength) = 0;
  virtual ::ndk::ScopedAStatus getTorchStrengthLevel(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus constructDefaultRequestSettings(::aidl::android::hardware::camera::device::RequestTemplate in_type, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus isStreamCombinationWithSettingsSupported(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_streams, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getSessionCharacteristics(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_sessionConfig, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class ICameraDeviceDefault : public ICameraDevice {
public:
  ::ndk::ScopedAStatus getCameraCharacteristics(::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override;
  ::ndk::ScopedAStatus getPhysicalCameraCharacteristics(const std::string& in_physicalCameraId, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override;
  ::ndk::ScopedAStatus getResourceCost(::aidl::android::hardware::camera::common::CameraResourceCost* _aidl_return) override;
  ::ndk::ScopedAStatus isStreamCombinationSupported(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_streams, bool* _aidl_return) override;
  ::ndk::ScopedAStatus open(const std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceCallback>& in_callback, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceSession>* _aidl_return) override;
  ::ndk::ScopedAStatus openInjectionSession(const std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceCallback>& in_callback, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraInjectionSession>* _aidl_return) override;
  ::ndk::ScopedAStatus setTorchMode(bool in_on) override;
  ::ndk::ScopedAStatus turnOnTorchWithStrengthLevel(int32_t in_torchStrength) override;
  ::ndk::ScopedAStatus getTorchStrengthLevel(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus constructDefaultRequestSettings(::aidl::android::hardware::camera::device::RequestTemplate in_type, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override;
  ::ndk::ScopedAStatus isStreamCombinationWithSettingsSupported(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_streams, bool* _aidl_return) override;
  ::ndk::ScopedAStatus getSessionCharacteristics(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_sessionConfig, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
