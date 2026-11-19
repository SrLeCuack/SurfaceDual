#pragma once
// Lo poco de libbase que usa el HAL de cámara de goldfish. El HAL se compila con el NDK (libc++ estática, std::__ndk1):
// enlazar las bibliotecas C++ de la plataforma (std::__1) no casaría, así que solo se usan APIs en C del dispositivo.
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <sys/system_properties.h>

namespace android::base {

class unique_fd {
public:
    unique_fd() = default;
    explicit unique_fd(int fd) : fd_(fd) {}
    unique_fd(unique_fd&& o) noexcept : fd_(o.release()) {}
    unique_fd& operator=(unique_fd&& o) noexcept { reset(o.release()); return *this; }
    ~unique_fd() { reset(); }
    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;
    int get() const { return fd_; }
    bool ok() const { return fd_ >= 0; }
    int release() { return std::exchange(fd_, -1); }
    void reset(int fd = -1) { if (fd_ >= 0) close(fd_); fd_ = fd; }
private:
    int fd_ = -1;
};

inline std::string GetProperty(const std::string& key, const std::string& def) {
    char v[PROP_VALUE_MAX];
    return __system_property_get(key.c_str(), v) > 0 ? std::string(v) : def;
}

}  // namespace android::base
