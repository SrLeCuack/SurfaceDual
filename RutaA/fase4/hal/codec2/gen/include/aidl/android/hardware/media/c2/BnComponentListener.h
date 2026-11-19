/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\BaseBlock.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Block.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQuery.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQueryResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FrameData.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentInterface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentListener.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentStore.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IConfigurable.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSink.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurfaceConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IPooledGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\InfoBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamField.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamFieldValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Params.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\SettingResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\StructDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ValueRange.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Work.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkBundle.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkOrdinal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Worklet.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/media/c2/IComponentListener.h"

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
namespace media {
namespace c2 {
class BnComponentListener : public ::ndk::BnCInterface<IComponentListener> {
public:
  BnComponentListener();
  virtual ~BnComponentListener();
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) final;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) final;
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class IComponentListenerDelegator : public BnComponentListener {
public:
  explicit IComponentListenerDelegator(const std::shared_ptr<IComponentListener> &impl) : _impl(impl) {
     int32_t _impl_ver = 0;
     if (!impl->getInterfaceVersion(&_impl_ver).isOk()) {;
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Delegator failed to get version of the implementation.");
     }
     if (_impl_ver != IComponentListener::version) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Mismatched versions of delegator and implementation is not allowed.");
     }
  }

  ::ndk::ScopedAStatus onError(const ::aidl::android::hardware::media::c2::Status& in_status, int32_t in_errorCode) override {
    return _impl->onError(in_status, in_errorCode);
  }
  ::ndk::ScopedAStatus onFramesRendered(const std::vector<::aidl::android::hardware::media::c2::IComponentListener::RenderedFrame>& in_renderedFrames) override {
    return _impl->onFramesRendered(in_renderedFrames);
  }
  ::ndk::ScopedAStatus onInputBuffersReleased(const std::vector<::aidl::android::hardware::media::c2::IComponentListener::InputBuffer>& in_inputBuffers) override {
    return _impl->onInputBuffersReleased(in_inputBuffers);
  }
  ::ndk::ScopedAStatus onTripped(const std::vector<::aidl::android::hardware::media::c2::SettingResult>& in_settingResults) override {
    return _impl->onTripped(in_settingResults);
  }
  ::ndk::ScopedAStatus onWorkDone(const ::aidl::android::hardware::media::c2::WorkBundle& in_workBundle) override {
    return _impl->onWorkDone(in_workBundle);
  }
protected:
private:
  std::shared_ptr<IComponentListener> _impl;
};

}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
