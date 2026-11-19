/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:/SurfaceDual\\herramientas\\sdk\\build-tools\\android-37.0\\aidl.exe --lang=ndk --structured --stability=vintf --version=3 --hash=ac7c7f74993871015f73daff1ea0c7f4d4f85ca7 -I C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl -I C:/SurfaceDual\\RutaA\\fase4\\hal\\aidl -o gen/src -h gen/include C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferCache.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferRequestStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\BufferStatus.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraBlob.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraBlobId.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraMetadata.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CameraOfflineSessionInfo.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CaptureRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\CaptureResult.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ConfigureStreamsRet.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ErrorCode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ErrorMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\HalStream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDevice.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDeviceCallback.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraDeviceSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraInjectionSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ICameraOfflineSession.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\NotifyMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\OfflineRequest.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\OfflineStream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\PhysicalCameraMetadata.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\PhysicalCameraSetting.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\RequestTemplate.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\ShutterMsg.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\Stream.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBuffer.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBufferRequestError.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBufferRet.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamBuffersVal.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamConfiguration.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamConfigurationMode.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamRotation.aidl C:/SurfaceDual\\RutaA\\fase4\\hal\\camara\\aidl\\android\\hardware\\camera\\device\\StreamType.aidl
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
#include <aidl/android/hardware/camera/device/BufferCache.h>
#include <aidl/android/hardware/camera/device/CameraMetadata.h>
#include <aidl/android/hardware/camera/device/CameraOfflineSessionInfo.h>
#include <aidl/android/hardware/camera/device/CaptureRequest.h>
#include <aidl/android/hardware/camera/device/ConfigureStreamsRet.h>
#include <aidl/android/hardware/camera/device/HalStream.h>
#include <aidl/android/hardware/camera/device/ICameraOfflineSession.h>
#include <aidl/android/hardware/camera/device/RequestTemplate.h>
#include <aidl/android/hardware/camera/device/StreamConfiguration.h>
#include <aidl/android/hardware/common/fmq/MQDescriptor.h>
#include <aidl/android/hardware/common/fmq/SynchronizedReadWrite.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::camera::device {
class BufferCache;
class CameraMetadata;
class CameraOfflineSessionInfo;
class CaptureRequest;
class ConfigureStreamsRet;
class HalStream;
class ICameraOfflineSession;
class StreamConfiguration;
}  // namespace aidl::android::hardware::camera::device
namespace aidl::android::hardware::common::fmq {
template <typename T, typename Flavor>
class MQDescriptor;
}  // namespace aidl::android::hardware::common::fmq
namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {
class ICameraDeviceSessionDelegator;

class ICameraDeviceSession : public ::ndk::ICInterface {
public:
  typedef ICameraDeviceSessionDelegator DefaultDelegator;
  static const char* descriptor;
  ICameraDeviceSession();
  virtual ~ICameraDeviceSession();

  static inline const int32_t version = 3;
  static inline const std::string hash = "ac7c7f74993871015f73daff1ea0c7f4d4f85ca7";
  static constexpr uint32_t TRANSACTION_close = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_configureStreams = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_constructDefaultRequestSettings = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_flush = FIRST_CALL_TRANSACTION + 3;
  static constexpr uint32_t TRANSACTION_getCaptureRequestMetadataQueue = FIRST_CALL_TRANSACTION + 4;
  static constexpr uint32_t TRANSACTION_getCaptureResultMetadataQueue = FIRST_CALL_TRANSACTION + 5;
  static constexpr uint32_t TRANSACTION_isReconfigurationRequired = FIRST_CALL_TRANSACTION + 6;
  static constexpr uint32_t TRANSACTION_processCaptureRequest = FIRST_CALL_TRANSACTION + 7;
  static constexpr uint32_t TRANSACTION_signalStreamFlush = FIRST_CALL_TRANSACTION + 8;
  static constexpr uint32_t TRANSACTION_switchToOffline = FIRST_CALL_TRANSACTION + 9;
  static constexpr uint32_t TRANSACTION_repeatingRequestEnd = FIRST_CALL_TRANSACTION + 10;
  static constexpr uint32_t TRANSACTION_configureStreamsV2 = FIRST_CALL_TRANSACTION + 11;

  static std::shared_ptr<ICameraDeviceSession> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<ICameraDeviceSession>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<ICameraDeviceSession>* instance);
  virtual ::ndk::ScopedAStatus close() = 0;
  virtual ::ndk::ScopedAStatus configureStreams(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_requestedConfiguration, std::vector<::aidl::android::hardware::camera::device::HalStream>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus constructDefaultRequestSettings(::aidl::android::hardware::camera::device::RequestTemplate in_type, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus flush() = 0;
  virtual ::ndk::ScopedAStatus getCaptureRequestMetadataQueue(::aidl::android::hardware::common::fmq::MQDescriptor<int8_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getCaptureResultMetadataQueue(::aidl::android::hardware::common::fmq::MQDescriptor<int8_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus isReconfigurationRequired(const ::aidl::android::hardware::camera::device::CameraMetadata& in_oldSessionParams, const ::aidl::android::hardware::camera::device::CameraMetadata& in_newSessionParams, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus processCaptureRequest(const std::vector<::aidl::android::hardware::camera::device::CaptureRequest>& in_requests, const std::vector<::aidl::android::hardware::camera::device::BufferCache>& in_cachesToRemove, int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus signalStreamFlush(const std::vector<int32_t>& in_streamIds, int32_t in_streamConfigCounter) = 0;
  virtual ::ndk::ScopedAStatus switchToOffline(const std::vector<int32_t>& in_streamsToKeep, ::aidl::android::hardware::camera::device::CameraOfflineSessionInfo* out_offlineSessionInfo, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraOfflineSession>* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus repeatingRequestEnd(int32_t in_frameNumber, const std::vector<int32_t>& in_streamIds) = 0;
  virtual ::ndk::ScopedAStatus configureStreamsV2(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_requestedConfiguration, ::aidl::android::hardware::camera::device::ConfigureStreamsRet* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) = 0;
};
class ICameraDeviceSessionDefault : public ICameraDeviceSession {
public:
  ::ndk::ScopedAStatus close() override;
  ::ndk::ScopedAStatus configureStreams(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_requestedConfiguration, std::vector<::aidl::android::hardware::camera::device::HalStream>* _aidl_return) override;
  ::ndk::ScopedAStatus constructDefaultRequestSettings(::aidl::android::hardware::camera::device::RequestTemplate in_type, ::aidl::android::hardware::camera::device::CameraMetadata* _aidl_return) override;
  ::ndk::ScopedAStatus flush() override;
  ::ndk::ScopedAStatus getCaptureRequestMetadataQueue(::aidl::android::hardware::common::fmq::MQDescriptor<int8_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>* _aidl_return) override;
  ::ndk::ScopedAStatus getCaptureResultMetadataQueue(::aidl::android::hardware::common::fmq::MQDescriptor<int8_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>* _aidl_return) override;
  ::ndk::ScopedAStatus isReconfigurationRequired(const ::aidl::android::hardware::camera::device::CameraMetadata& in_oldSessionParams, const ::aidl::android::hardware::camera::device::CameraMetadata& in_newSessionParams, bool* _aidl_return) override;
  ::ndk::ScopedAStatus processCaptureRequest(const std::vector<::aidl::android::hardware::camera::device::CaptureRequest>& in_requests, const std::vector<::aidl::android::hardware::camera::device::BufferCache>& in_cachesToRemove, int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus signalStreamFlush(const std::vector<int32_t>& in_streamIds, int32_t in_streamConfigCounter) override;
  ::ndk::ScopedAStatus switchToOffline(const std::vector<int32_t>& in_streamsToKeep, ::aidl::android::hardware::camera::device::CameraOfflineSessionInfo* out_offlineSessionInfo, std::shared_ptr<::aidl::android::hardware::camera::device::ICameraOfflineSession>* _aidl_return) override;
  ::ndk::ScopedAStatus repeatingRequestEnd(int32_t in_frameNumber, const std::vector<int32_t>& in_streamIds) override;
  ::ndk::ScopedAStatus configureStreamsV2(const ::aidl::android::hardware::camera::device::StreamConfiguration& in_requestedConfiguration, ::aidl::android::hardware::camera::device::ConfigureStreamsRet* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceVersion(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getInterfaceHash(std::string* _aidl_return) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
}  // namespace aidl
