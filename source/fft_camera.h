#ifndef EVKMIMXRT1010_DEV_VIDEO_VIRTUAL_CAMERA_LITE_BM_FFT_CAMERA_H
#define EVKMIMXRT1010_DEV_VIDEO_VIRTUAL_CAMERA_LITE_BM_FFT_CAMERA_H

#include <stdint.h>
#include "usb_device_descriptor.h"

/* Camera definition. */
#define CAMERA_HORIZONTAL_POINTS    USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_WIDTH
#define CAMERA_VERTICAL_POINTS      USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_HEIGHT

#define FRAME_BUFFER_ALIGN          64      /* Frame buffer data alignment. */
#define CAMERA_FRAME_BUFFER_COUNT   1

void FFT_CAMERA_SubmitEmptyBuffer(void* instance, uint32_t buffer);
int FFT_CAMERA_GetFullBuffer(void* instance, uint32_t* buffer);
int FFT_CAMERA_DoCapture(void* instance, uint8_t* data, uint32_t data_len);

#endif //EVKMIMXRT1010_DEV_VIDEO_VIRTUAL_CAMERA_LITE_BM_FFT_CAMERA_H
