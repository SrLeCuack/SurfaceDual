#pragma once
// Equivalente de libaidlcommonsupport (lo que usa el HAL): native_handle <-> NativeHandle de AIDL.
#include <aidl/android/hardware/common/NativeHandle.h>
#include <cutils/native_handle.h>
#include <fcntl.h>

namespace android {

// Duplica los fds: el native_handle sigue siendo del que llama.
inline aidl::android::hardware::common::NativeHandle makeToAidl(const native_handle_t* h) {
    aidl::android::hardware::common::NativeHandle r;
    if (!h) return r;
    for (int i = 0; i < h->numFds; i++) r.fds.emplace_back(fcntl(h->data[i], F_DUPFD_CLOEXEC, 0));
    r.ints.assign(&h->data[h->numFds], &h->data[h->numFds + h->numInts]);
    return r;
}

inline bool isAidlNativeHandleEmpty(const aidl::android::hardware::common::NativeHandle& h) {
    return h.fds.empty() && h.ints.empty();
}

}  // namespace android
