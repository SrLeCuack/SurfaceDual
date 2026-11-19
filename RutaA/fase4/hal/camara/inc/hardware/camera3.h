#pragma once
// Solo la cola del búfer JPEG de hardware/libhardware/include/hardware/camera3.h (lo que usa HwCamera.cpp).
#include <stdint.h>
#define CAMERA3_JPEG_BLOB_ID 0x00FF
struct camera3_jpeg_blob {
    uint16_t jpeg_blob_id;
    uint32_t jpeg_size;
};
