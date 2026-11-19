/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=c12818c51418fbfc76f1b81eba6678d75cf526c1 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferInvalidationMessage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\BufferStatusMessage.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IAccessor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IClientManager.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\IObserver.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\bufferpool2\\ResultStatus.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/media/bufferpool2/IClientManager.h"

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
namespace bufferpool2 {
class BnClientManager : public ::ndk::BnCInterface<IClientManager> {
public:
  BnClientManager();
  virtual ~BnClientManager();
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) final;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) final;
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class IClientManagerDelegator : public BnClientManager {
public:
  explicit IClientManagerDelegator(const std::shared_ptr<IClientManager> &impl) : _impl(impl) {
     int32_t _impl_ver = 0;
     if (!impl->getInterfaceVersion(&_impl_ver).isOk()) {;
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Delegator failed to get version of the implementation.");
     }
     if (_impl_ver != IClientManager::version) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Mismatched versions of delegator and implementation is not allowed.");
     }
  }

  ::ndk::ScopedAStatus registerSender(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IAccessor>& in_bufferPool, ::aidl::android::hardware::media::bufferpool2::IClientManager::Registration* _aidl_return) override {
    return _impl->registerSender(in_bufferPool, _aidl_return);
  }
  ::ndk::ScopedAStatus registerPassiveSender(const std::shared_ptr<::aidl::android::hardware::media::bufferpool2::IAccessor>& in_bufferPool, ::aidl::android::hardware::media::bufferpool2::IClientManager::Registration* _aidl_return) override {
    return _impl->registerPassiveSender(in_bufferPool, _aidl_return);
  }
protected:
private:
  std::shared_ptr<IClientManager> _impl;
};

}  // namespace bufferpool2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
