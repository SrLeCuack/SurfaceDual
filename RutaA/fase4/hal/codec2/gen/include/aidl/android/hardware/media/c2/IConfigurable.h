/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=2 --hash=2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o C:\\Users\\usuario\\AppData\\Local\\Temp\\c2src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\BaseBlock.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Block.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Buffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQuery.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FieldSupportedValuesQueryResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\FrameData.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponent.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentInterface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentListener.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IComponentStore.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IConfigurable.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSink.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurface.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IInputSurfaceConnection.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\IPooledGraphicBufferAllocator.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\InfoBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamField.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ParamFieldValues.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Params.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\SettingResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Status.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\StructDescriptor.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\ValueRange.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Work.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkBundle.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\WorkOrdinal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl\\android\\hardware\\media\\c2\\Worklet.aidl
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
#include <aidl/android/hardware/media/c2/FieldSupportedValuesQuery.h>
#include <aidl/android/hardware/media/c2/FieldSupportedValuesQueryResult.h>
#include <aidl/android/hardware/media/c2/IConfigurable.h>
#include <aidl/android/hardware/media/c2/ParamDescriptor.h>
#include <aidl/android/hardware/media/c2/Params.h>
#include <aidl/android/hardware/media/c2/SettingResult.h>
#include <aidl/android/hardware/media/c2/Status.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::media::c2 {
class FieldSupportedValuesQuery;
class FieldSupportedValuesQueryResult;
class ParamDescriptor;
class Params;
class SettingResult;
class Status;
}  // namespace aidl::android::hardware::media::c2
namespace aidl {
namespace android {
namespace hardware {
namespace media {
namespace c2 {
class IConfigurableDelegator;

class IConfigurable : public ::ndk::ICInterface {
public:
  typedef IConfigurableDelegator DefaultDelegator;
  static const char* descriptor;
  IConfigurable();
  virtual ~IConfigurable();

  class ConfigResult {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    ::aidl::android::hardware::media::c2::Params params;
    std::vector<::aidl::android::hardware::media::c2::SettingResult> failures;
    ::aidl::android::hardware::media::c2::Status status;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const ConfigResult& _rhs) const {
      return std::tie(params, failures, status) == std::tie(_rhs.params, _rhs.failures, _rhs.status);
    }
    inline bool operator<(const ConfigResult& _rhs) const {
      return std::tie(params, failures, status) < std::tie(_rhs.params, _rhs.failures, _rhs.status);
    }
    inline bool operator!=(const ConfigResult& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const ConfigResult& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const ConfigResult& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const ConfigResult& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "ConfigResult{";
      _aidl_os << "params: " << ::android::internal::ToString(params);
      _aidl_os << ", failures: " << ::android::internal::ToString(failures);
      _aidl_os << ", status: " << ::android::internal::ToString(status);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class QueryResult {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    ::aidl::android::hardware::media::c2::Params params;
    ::aidl::android::hardware::media::c2::Status status;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const QueryResult& _rhs) const {
      return std::tie(params, status) == std::tie(_rhs.params, _rhs.status);
    }
    inline bool operator<(const QueryResult& _rhs) const {
      return std::tie(params, status) < std::tie(_rhs.params, _rhs.status);
    }
    inline bool operator!=(const QueryResult& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const QueryResult& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const QueryResult& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const QueryResult& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "QueryResult{";
      _aidl_os << "params: " << ::android::internal::ToString(params);
      _aidl_os << ", status: " << ::android::internal::ToString(status);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  class QuerySupportedValuesResult {
  public:
    typedef std::false_type fixed_size;
    static const char* descriptor;

    std::vector<::aidl::android::hardware::media::c2::FieldSupportedValuesQueryResult> values;
    ::aidl::android::hardware::media::c2::Status status;

    binder_status_t readFromParcel(const AParcel* parcel);
    binder_status_t writeToParcel(AParcel* parcel) const;

    inline bool operator==(const QuerySupportedValuesResult& _rhs) const {
      return std::tie(values, status) == std::tie(_rhs.values, _rhs.status);
    }
    inline bool operator<(const QuerySupportedValuesResult& _rhs) const {
      return std::tie(values, status) < std::tie(_rhs.values, _rhs.status);
    }
    inline bool operator!=(const QuerySupportedValuesResult& _rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const QuerySupportedValuesResult& _rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const QuerySupportedValuesResult& _rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const QuerySupportedValuesResult& _rhs) const {
      return !(_rhs < *this);
    }

    static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
    inline std::string toString() const {
      std::ostringstream _aidl_os;
      _aidl_os << "QuerySupportedValuesResult{";
      _aidl_os << "values: " << ::android::internal::ToString(values);
      _aidl_os << ", status: " << ::android::internal::ToString(status);
      _aidl_os << "}";
      return _aidl_os.str();
    }
  };
  static inline const int32_t version = 2;
  static inline const std::string hash = "2f7448ba8211c5cd1de7bd6c17fe7c5fd034ebbe";
  static constexpr uint32_t TRANSACTION_config = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_getId = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_getName = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_query = FIRST_CALL_TRANSACTION + 3;
  static constexpr uint32_t TRANSACTION_querySupportedParams = FIRST_CALL_TRANSACTION + 4;
  static constexpr uint32_t TRANSACTION_querySupportedValues = FIRST_CALL_TRANSACTION + 5;

  static std::shared_ptr<IConfigurable> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IConfigurable>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IConfigurable>* instance);
  virtual ::ndk::ScopedAStatus config(const ::aidl::android::hardware::media::c2::Params& in_inParams, bool in_mayBlock, ::aidl::android::hardware::media::c2::IConfigurable::ConfigResult* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getId(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getName(std::string* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus query(const std::vector<int32_t>& in_indices, bool in_mayBlock, ::aidl::android::hardware::media::c2::IConfigurable::QueryResult* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus querySupportedParams(int32_t in_start, int32_t in_count, std::vector<::aidl::android::hardware::media::c2::ParamDescriptor>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus querySupportedValues(const std::vector<::aidl::android::hardware::media::c2::FieldSupportedValuesQuery>& in_inFields, bool in_mayBlock, ::aidl::android::hardware::media::c2::IConfigurable::QuerySupportedValuesResult* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class IConfigurableDefault : public IConfigurable {
public:
  ::ndk::ScopedAStatus config(const ::aidl::android::hardware::media::c2::Params& in_inParams, bool in_mayBlock, ::aidl::android::hardware::media::c2::IConfigurable::ConfigResult* _aidl_return) override;
  ::ndk::ScopedAStatus getId(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getName(std::string* _aidl_return) override;
  ::ndk::ScopedAStatus query(const std::vector<int32_t>& in_indices, bool in_mayBlock, ::aidl::android::hardware::media::c2::IConfigurable::QueryResult* _aidl_return) override;
  ::ndk::ScopedAStatus querySupportedParams(int32_t in_start, int32_t in_count, std::vector<::aidl::android::hardware::media::c2::ParamDescriptor>* _aidl_return) override;
  ::ndk::ScopedAStatus querySupportedValues(const std::vector<::aidl::android::hardware::media::c2::FieldSupportedValuesQuery>& in_inFields, bool in_mayBlock, ::aidl::android::hardware::media::c2::IConfigurable::QuerySupportedValuesResult* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace c2
}  // namespace media
}  // namespace hardware
}  // namespace android
}  // namespace aidl
