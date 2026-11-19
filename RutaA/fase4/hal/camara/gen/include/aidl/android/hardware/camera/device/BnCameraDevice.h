/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=ac7c7f74993871015f73daff1ea0c7f4d4f85ca7 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferCache.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferRequestStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraBlob.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraBlobId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraMetadata.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraOfflineSessionInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CaptureRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CaptureResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ConfigureStreamsRet.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ErrorCode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ErrorMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\HalStream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDevice.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDeviceCallback.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDeviceSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraInjectionSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraOfflineSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\NotifyMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\OfflineRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\OfflineStream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\PhysicalCameraMetadata.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\PhysicalCameraSetting.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\RequestTemplate.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ShutterMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\Stream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBufferRequestError.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBufferRet.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBuffersVal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamConfiguration.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamConfigurationMode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamRotation.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamType.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/camera/device/ICameraDevice.h"

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
namespace device {
class BnCameraDevice : public ::ndk::BnCInterface<ICameraDevice> {
public:
  BnCameraDevice();
  virtual ~BnCameraDevice();
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) final;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) final;
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class ICameraDeviceDelegator : public BnCameraDevice {
public:
  explicit ICameraDeviceDelegator(const std::shared_ptr<ICameraDevice> &impl) : _impl(impl) {
     int32_t _impl_ver = 0;
     if (!impl->getInterfaceVersion(&_impl_ver).isOk()) {;
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Delegator failed to get version of the implementation.");
     }
     if (_impl_ver != ICameraDevice::version) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Mismatched versions of delegator and implementation is not allowed.");
     }
  }

  ::ndk::ScopedAStatus getCameraCharacteristics(::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override {
    return _impl->getCameraCharacteristics(_aidl_return);
  }
  ::ndk::ScopedAStatus getPhysicalCameraCharacteristics(const std::string& in_physicalCameraId, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override {
    return _impl->getPhysicalCameraCharacteristics(in_physicalCameraId, _aidl_return);
  }
  ::ndk::ScopedAStatus getResourceCost(::aidl::android::hardware::camera::common::CameraResourceCost* _aidl_return) override {
    return _impl->getResourceCost(_aidl_return);
  }
  ::ndk::ScopedAStatus isStreamCombinationSupported(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_streams, bool* _aidl_return) override {
    return _impl->isStreamCombinationSupported(in_streams, _aidl_return);
  }
  ::ndk::ScopedAStatus open(const std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceCallback>& in_callback, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceSession>* _aidl_return) override {
    return _impl->open(in_callback, _aidl_return);
  }
  ::ndk::ScopedAStatus openInjectionSession(const std::shared_ptr<::aidl::android::hardware::camera::device::ICameraDeviceCallback>& in_callback, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraInjectionSession>* _aidl_return) override {
    return _impl->openInjectionSession(in_callback, _aidl_return);
  }
  ::ndk::ScopedAStatus setTorchMode(bool in_on) override {
    return _impl->setTorchMode(in_on);
  }
  ::ndk::ScopedAStatus turnOnTorchWithStrengthLevel(int32_t in_torchStrength) override {
    return _impl->turnOnTorchWithStrengthLevel(in_torchStrength);
  }
  ::ndk::ScopedAStatus getTorchStrengthLevel(int32_t* _aidl_return) override {
    return _impl->getTorchStrengthLevel(_aidl_return);
  }
  ::ndk::ScopedAStatus constructDefaultRequestSettings(::aidl::android::hardware::camera::device::RequestTemplate in_type, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override {
    return _impl->constructDefaultRequestSettings(in_type, _aidl_return);
  }
  ::ndk::ScopedAStatus isStreamCombinationWithSettingsSupported(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_streams, bool* _aidl_return) override {
    return _impl->isStreamCombinationWithSettingsSupported(in_streams, _aidl_return);
  }
  ::ndk::ScopedAStatus getSessionCharacteristics(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_sessionConfig, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override {
    return _impl->getSessionCharacteristics(in_sessionConfig, _aidl_return);
  }
protected:
private:
  std::shared_ptr<ICameraDevice> _impl;
};

}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
