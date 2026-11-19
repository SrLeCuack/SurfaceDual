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
#include <aidl/android/hardware/camera/common/CameraDeviceStatus.h>
#include <aidl/android/hardware/camera/common/TorchModeStatus.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace provider {
class ICameraProviderCallbackDelegator;

class ICameraProviderCallback : public ::ndk::ICInterface {
public:
  typedef ICameraProviderCallbackDelegator DefaultDelegator;
  static const char* descriptor;
  ICameraProviderCallback();
  virtual ~ICameraProviderCallback();

  static inline const int32_t version = 3;
  static inline const std::string hash = "261437c3d7d6ad09d5560eee5196a28c0b27106e";
  static constexpr uint32_t TRANSACTION_cameraDeviceStatusChange = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_torchModeStatusChange = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_physicalCameraDeviceStatusChange = FIRST_CALL_TRANSACTION + 2;

  static std::shared_ptr<ICameraProviderCallback> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<ICameraProviderCallback>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<ICameraProviderCallback>* instance);
  virtual ::ndk::ScopedAStatus cameraDeviceStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) = 0;
  virtual ::ndk::ScopedAStatus torchModeStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::TorchModeStatus in_newStatus) = 0;
  virtual ::ndk::ScopedAStatus physicalCameraDeviceStatusChange(const std::string& in_cameraDeviceName, const std::string& in_physicalCameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class ICameraProviderCallbackDefault : public ICameraProviderCallback {
public:
  ::ndk::ScopedAStatus cameraDeviceStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) override;
  ::ndk::ScopedAStatus torchModeStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::TorchModeStatus in_newStatus) override;
  ::ndk::ScopedAStatus physicalCameraDeviceStatusChange(const std::string& in_cameraDeviceName, const std::string& in_physicalCameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
