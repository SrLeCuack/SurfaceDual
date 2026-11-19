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
#include <cassert>

#ifndef __BIONIC__
#ifndef __assert2
#define __assert2(a,b,c,d) ((void)0)
#endif
#endif

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace provider {
class BnCameraProviderCallback : public ::ndk::BnCInterface<ICameraProviderCallback> {
public:
  BnCameraProviderCallback();
  virtual ~BnCameraProviderCallback();
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) final;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) final;
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class ICameraProviderCallbackDelegator : public BnCameraProviderCallback {
public:
  explicit ICameraProviderCallbackDelegator(const std::shared_ptr<ICameraProviderCallback> &impl) : _impl(impl) {
     int32_t _impl_ver = 0;
     if (!impl->getInterfaceVersion(&_impl_ver).isOk()) {;
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Delegator failed to get version of the implementation.");
     }
     if (_impl_ver != ICameraProviderCallback::version) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Mismatched versions of delegator and implementation is not allowed.");
     }
  }

  ::ndk::ScopedAStatus cameraDeviceStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) override {
    return _impl->cameraDeviceStatusChange(in_cameraDeviceName, in_newStatus);
  }
  ::ndk::ScopedAStatus torchModeStatusChange(const std::string& in_cameraDeviceName, ::aidl::android::hardware::camera::common::TorchModeStatus in_newStatus) override {
    return _impl->torchModeStatusChange(in_cameraDeviceName, in_newStatus);
  }
  ::ndk::ScopedAStatus physicalCameraDeviceStatusChange(const std::string& in_cameraDeviceName, const std::string& in_physicalCameraDeviceName, ::aidl::android::hardware::camera::common::CameraDeviceStatus in_newStatus) override {
    return _impl->physicalCameraDeviceStatusChange(in_cameraDeviceName, in_physicalCameraDeviceName, in_newStatus);
  }
protected:
private:
  std::shared_ptr<ICameraProviderCallback> _impl;
};

}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
