/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=261437c3d7d6ad09d5560eee5196a28c0b27106e -I C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\CameraIdAndStreamCombination.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\ConcurrentCameraIdCombination.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\ICameraProvider.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\provider\\ICameraProviderCallback.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/camera/provider/ICameraProviderCallback.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace provider {
class BpCameraProviderCallback : public ::ndk::BpCInterface<ICameraProviderCallback> {
public:
  explicit BpCameraProviderCallback(const ::ndk::SpAIBinder& binder);
  virtual ~BpCameraProviderCallback();

  ::ndk::ScopedAStatus cameraDeviceStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) override;
  ::ndk::ScopedAStatus torchModeStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::TorchModeStatus in_newStatus) override;
  ::ndk::ScopedAStatus physicalCameraDeviceStatusChange(const std::string& in_cameraDeviceName, const std::string& in_physicalCameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  int32_t _aidl_cached_version = -1;
  std::string _aidl_cached_hash = "-1";
  std::mutex _aidl_cached_hash_mutex;
};
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
