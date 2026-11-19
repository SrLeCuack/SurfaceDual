/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\BaseBlock.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Block.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQuery.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQueryResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FrameData.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentInterface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentListener.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentStore.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IConfigurable.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSink.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurfaceConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IPooledGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\InfoBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamField.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamFieldValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Params.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\SettingResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\StructDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ValueRange.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Work.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkBundle.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkOrdinal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Worklet.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/media/c2/IComponentStore.h"

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
class BnComponentStore : public ::ndk::BnCInterface<IComponentStore> {
public:
  BnComponentStore();
  virtual ~BnComponentStore();
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) final;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) final;
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class IComponentStoreDelegator : public BnComponentStore {
public:
  explicit IComponentStoreDelegator(const std::shared_ptr<IComponentStore> &impl) : _impl(impl) {
     int32_t _impl_ver = 0;
     if (!impl->getInterfaceVersion(&_impl_ver).isOk()) {;
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Delegator failed to get version of the implementation.");
     }
     if (_impl_ver != IComponentStore::version) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Mismatched versions of delegator and implementation is not allowed.");
     }
  }

  ::ndk::ScopedAStatus copyBuffer(const ::aidl::android::hardware::media::c2::Buffer& in_src, const ::aidl::android::hardware::media::c2::Buffer& in_dst) override {
    return _impl->copyBuffer(in_src, in_dst);
  }
  ::ndk::ScopedAStatus createComponent(const std::string& in_name, const std::shared_ptr<::aidl::android::hardware::media::c2::IComponentListener>& in_listener, const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IClientManager>& in_pool, std::shared_ptr<::aidl::android::hardware::media::c2::IComponent>* _aidl_return) override {
    return _impl->createComponent(in_name, in_listener, in_pool, _aidl_return);
  }
  ::ndk::ScopedAStatus createInterface(const std::string& in_name, std::shared_ptr<::aidl::android::hardware::media::c2::IComponentInterface>* _aidl_return) override {
    return _impl->createInterface(in_name, _aidl_return);
  }
  ::ndk::ScopedAStatus getConfigurable(std::shared_ptr<::aidl::android::hardware::media::c2::IConfigurable>* _aidl_return) override {
    return _impl->getConfigurable(_aidl_return);
  }
  ::ndk::ScopedAStatus getPoolClientManager(std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IClientManager>* _aidl_return) override {
    return _impl->getPoolClientManager(_aidl_return);
  }
  ::ndk::ScopedAStatus getStructDescriptors(const std::vector<int32_t>& in_indices, std::vector<::aidl::android::hardware::media::c2::StructDescriptor>* _aidl_return) override {
    return _impl->getStructDescriptors(in_indices, _aidl_return);
  }
  ::ndk::ScopedAStatus listComponents(std::vector<::aidl::android::hardware::media::c2::IComponentStore::ComponentTraits>* _aidl_return) override {
    return _impl->listComponents(_aidl_return);
  }
  ::ndk::ScopedAStatus createInputSurface(std::shared_ptr<::aidl::android::hardware::media::c2::IInputSurface>* _aidl_return) override {
    return _impl->createInputSurface(_aidl_return);
  }
protected:
private:
  std::shared_ptr<IComponentStore> _impl;
};

}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
