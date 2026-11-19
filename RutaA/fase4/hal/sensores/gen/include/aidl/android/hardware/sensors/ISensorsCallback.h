/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=b051523be857524c99057b69f253a7d0bd295400 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\AdditionalInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\DynamicSensorInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\Event.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\ISensors.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\ISensorsCallback.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\SensorInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\SensorStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\sensores\\aidl\\android\\hardware\\sensors\\SensorType.aidl
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
#include <aidl/android/hardware/sensors/SensorInfo.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::sensors {
class SensorInfo;
}  // namespace aidl::android::hardware::sensors
namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
class ISensorsCallbackDelegator;

class ISensorsCallback : public ::ndk::ICInterface {
public:
  typedef ISensorsCallbackDelegator DefaultDelegator;
  static const char* descriptor;
  ISensorsCallback();
  virtual ~ISensorsCallback();

  static inline const int32_t version = 3;
  static inline const std::string hash = "b051523be857524c99057b69f253a7d0bd295400";
  static constexpr uint32_t TRANSACTION_onDynamicSensorsConnected = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_onDynamicSensorsDisconnected = FIRST_CALL_TRANSACTION + 1;

  static std::shared_ptr<ISensorsCallback> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<ISensorsCallback>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<ISensorsCallback>* instance);
  virtual ::ndk::ScopedAStatus onDynamicSensorsConnected(const std::vector<::aidl::android::hardware::sensors::SensorInfo>& in_sensorInfos) = 0;
  virtual ::ndk::ScopedAStatus onDynamicSensorsDisconnected(const std::vector<int32_t>& in_sensorHandles) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class ISensorsCallbackDefault : public ISensorsCallback {
public:
  ::ndk::ScopedAStatus onDynamicSensorsConnected(const std::vector<::aidl::android::hardware::sensors::SensorInfo>& in_sensorInfos) override;
  ::ndk::ScopedAStatus onDynamicSensorsDisconnected(const std::vector<int32_t>& in_sensorHandles) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
