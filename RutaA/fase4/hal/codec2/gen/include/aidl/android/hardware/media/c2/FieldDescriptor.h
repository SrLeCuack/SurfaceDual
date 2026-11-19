/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\BaseBlock.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Block.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQuery.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQueryResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FrameData.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentInterface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentListener.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentStore.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IConfigurable.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSink.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurfaceConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IPooledGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\InfoBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamField.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamFieldValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Params.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\SettingResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\StructDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ValueRange.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Work.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkBundle.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkOrdinal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Worklet.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_enums.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#include <aidl/android/hardware/media/c2/FieldDescriptor.h>
#include <aidl/android/hardware/media/c2/FieldId.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::media::c2 {
class FieldId;
}  // namespace aidl::android::hardware::media::c2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
class FieldDescriptor {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  enum class Type : int32_t {
    NO_INIT = 0,
    INT32 = 1,
    UINT32 = 2,
    CNTR32 = 3,
    INT64 = 4,
    UINT64 = 5,
    CNTR64 = 6,
    FLOAT = 7,
    STRING = 256,
    BLOB = 257,
    STRUCT = 131072,
  };

  class NamedValue {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    std::string name;
    int64_t value = 0L;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const NamedValue& _rhs) const {
      return std::tie(name, value) == std::tie(_rhs.name, _rhs.value);
    }
    inline bool operator<(const NamedValue& _rhs) const {
      return std::tie(name, value) < std::tie(_rhs.name, _rhs.value);
    }
    inline bool operator!=(const NamedValue& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const NamedValue& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const NamedValue& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const NamedValue& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "NamedValue{";
      _aidl_os << "name: " << ::android::internal::ToString(name);
      _aidl_os << ", value: " << ::android::internal::ToString(value);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  ::aidl::android::hardware::media::c2::FieldId fieldId;
  ::aidl::android::hardware::media::c2::FieldDescriptor::Type type = ::aidl::android::hardware::media::c2::FieldDescriptor::Type(0);
  int32_t structIndex = 0;
  int32_t extent = 0;
  std::string name;
  std::vector<::aidl::android::hardware::media::c2::FieldDescriptor::NamedValue> namedValues;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const FieldDescriptor& _rhs) const {
    return std::tie(fieldId, type, structIndex, extent, name, namedValues) == std::tie(_rhs.fieldId, _rhs.type, _rhs.structIndex, _rhs.extent, _rhs.name, _rhs.namedValues);
  }
  inline bool operator<(const FieldDescriptor& _rhs) const {
    return std::tie(fieldId, type, structIndex, extent, name, namedValues) < std::tie(_rhs.fieldId, _rhs.type, _rhs.structIndex, _rhs.extent, _rhs.name, _rhs.namedValues);
  }
  inline bool operator!=(const FieldDescriptor& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const FieldDescriptor& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const FieldDescriptor& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const FieldDescriptor& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "FieldDescriptor{";
    _aidl_os << "fieldId: " << ::android::internal::ToString(fieldId);
    _aidl_os << ", type: " << ::android::internal::ToString(type);
    _aidl_os << ", structIndex: " << ::android::internal::ToString(structIndex);
    _aidl_os << ", extent: " << ::android::internal::ToString(extent);
    _aidl_os << ", name: " << ::android::internal::ToString(name);
    _aidl_os << ", namedValues: " << ::android::internal::ToString(namedValues);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
[[nodiscard]] static inline std::string toString(FieldDescriptor::Type val) {
  switch(val) {
  case FieldDescriptor::Type::NO_INIT:
    return "NO_INIT";
  case FieldDescriptor::Type::INT32:
    return "INT32";
  case FieldDescriptor::Type::UINT32:
    return "UINT32";
  case FieldDescriptor::Type::CNTR32:
    return "CNTR32";
  case FieldDescriptor::Type::INT64:
    return "INT64";
  case FieldDescriptor::Type::UINT64:
    return "UINT64";
  case FieldDescriptor::Type::CNTR64:
    return "CNTR64";
  case FieldDescriptor::Type::FLOAT:
    return "FLOAT";
  case FieldDescriptor::Type::STRING:
    return "STRING";
  case FieldDescriptor::Type::BLOB:
    return "BLOB";
  case FieldDescriptor::Type::STRUCT:
    return "STRUCT";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::media::c2::FieldDescriptor::Type, 11> enum_values<aidl::android::hardware::media::c2::FieldDescriptor::Type> = {
  aidl::android::hardware::media::c2::FieldDescriptor::Type::NO_INIT,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::INT32,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::UINT32,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::CNTR32,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::INT64,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::UINT64,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::CNTR64,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::FLOAT,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::STRING,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::BLOB,
  aidl::android::hardware::media::c2::FieldDescriptor::Type::STRUCT,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
