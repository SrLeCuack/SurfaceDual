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
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#include <aidl/android/hardware/sensors/DynamicSensorInfo.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
class DynamicSensorInfo {
public:
  typedef std::true_type fixed_size;
  static const char* descriptor;

  class Uuid {
  public:
    typedef std::true_type fixed_size;
    static const char* descriptor;

    std::array<uint8_t, 16> values __attribute__((aligned (1))) = {{}};

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const Uuid& _rhs) const {
      return std::tie(values) == std::tie(_rhs.values);
    }
    inline bool operator<(const Uuid& _rhs) const {
      return std::tie(values) < std::tie(_rhs.values);
    }
    inline bool operator!=(const Uuid& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const Uuid& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const Uuid& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const Uuid& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "Uuid{";
      _aidl_os << "values: " << ::android::internal::ToString(values);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  static_assert(offsetof(Uuid, values) == 0);
  static_assert(sizeof(std::array<uint8_t, 16>) == 16);
  static_assert(alignof(Uuid) == 1);
  static_assert(sizeof(Uuid) == 16);
  bool connected __attribute__((aligned (1))) = false;
  int32_t sensorHandle __attribute__((aligned (4))) = 0;
  ::aidl::android::hardware::sensors::DynamicSensorInfo::Uuid uuid __attribute__((aligned (1)));

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const DynamicSensorInfo& _rhs) const {
    return std::tie(connected, sensorHandle, uuid) == std::tie(_rhs.connected, _rhs.sensorHandle, _rhs.uuid);
  }
  inline bool operator<(const DynamicSensorInfo& _rhs) const {
    return std::tie(connected, sensorHandle, uuid) < std::tie(_rhs.connected, _rhs.sensorHandle, _rhs.uuid);
  }
  inline bool operator!=(const DynamicSensorInfo& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const DynamicSensorInfo& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const DynamicSensorInfo& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const DynamicSensorInfo& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "DynamicSensorInfo{";
    _aidl_os << "connected: " << ::android::internal::ToString(connected);
    _aidl_os << ", sensorHandle: " << ::android::internal::ToString(sensorHandle);
    _aidl_os << ", uuid: " << ::android::internal::ToString(uuid);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
static_assert(offsetof(DynamicSensorInfo, connected) == 0);
static_assert(sizeof(bool) == 1);
static_assert(offsetof(DynamicSensorInfo, sensorHandle) == 4);
static_assert(sizeof(int32_t) == 4);
static_assert(offsetof(DynamicSensorInfo, uuid) == 8);
static_assert(sizeof(::aidl::android::hardware::sensors::DynamicSensorInfo::Uuid) == 16);
static_assert(alignof(DynamicSensorInfo) == 4);
static_assert(sizeof(DynamicSensorInfo) == 24);
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
