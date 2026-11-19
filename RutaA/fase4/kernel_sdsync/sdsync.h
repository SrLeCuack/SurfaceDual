/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_SDSYNC_H
#define _UAPI_SDSYNC_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct sdsync_desde_eventfd {
	__s32 eventfd;
	__s32 fence;
};

#define SDSYNC_IOC_DESDE_EVENTFD _IOWR('S', 1, struct sdsync_desde_eventfd)

#endif
