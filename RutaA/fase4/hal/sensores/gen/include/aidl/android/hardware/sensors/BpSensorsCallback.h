/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=b051523be857524c99057b69f253a7d0bd295400 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\AdditionalInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\DynamicSensorInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\Event.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\ISensors.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\ISensorsCallback.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\SensorInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\SensorStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\SensorType.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include "aidl/android/hardware/sensors/ISensorsCallback.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
class BpSensorsCallback : public ::ndk::BpCInterface<ISensorsCallback> {
public:
  explicit BpSensorsCallback(const ::ndk::SpAIBinder& binder);
  virtual ~BpSensorsCallback();

  ::ndk::ScopedAStatus onDynamicSensorsConnected(const std::vector<::aidl::android::hardware::sensors::SensorInfo>& in_sensorInfos) override;
  ::ndk::ScopedAStatus onDynamicSensorsDisconnected(const std::vector<int32_t>& in_sensorHandles) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  int32_t _aidl_cached_version = -1;
  std::string _aidl_cached_hash = "-1";
  std::mutex _aidl_cached_hash_mutex;
};
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
