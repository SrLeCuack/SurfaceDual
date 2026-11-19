#pragma once
// sync_wait de libsync: una fence de Android es un fd que se vuelve legible al señalarse.
#include <errno.h>
#include <poll.h>

inline int sync_wait(int fd, int timeout) {
    pollfd p = {fd, POLLIN, 0};
    int r;
    do r = poll(&p, 1, timeout); while (r < 0 && (errno == EINTR || errno == EAGAIN));
    if (r == 0) { errno = ETIME; return -1; }
    if (r > 0 && (p.revents & (POLLERR | POLLNVAL))) { errno = EINVAL; return -1; }
    return r > 0 ? 0 : -1;
}
