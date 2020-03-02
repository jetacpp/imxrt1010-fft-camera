/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"

#include "video_data.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/* mjpeg data of the video */
const unsigned char g_UsbDeviceVideoMjpegData[] =
{
   
};
const uint32_t g_UsbDeviceVideoMjpegLength = sizeof(g_UsbDeviceVideoMjpegData)/sizeof(g_UsbDeviceVideoMjpegData[0]);


uint8_t g_frame1 [0] = {

};
const uint32_t g_frame1_len = sizeof(g_frame1);

uint8_t g_frame2 [0] = { 

};

const uint32_t g_frame2_len = sizeof(g_frame2);

uint8_t* g_frames [] = {g_frame1, g_frame2};
const uint32_t  g_frames_len [] = {g_frame1_len, g_frame2_len};

uint8_t g_cur_frame;
    

