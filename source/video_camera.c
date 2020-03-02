/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017 NXP
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "video_camera.h"
#include "fft_camera.h"


#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
#include "usb_phy.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_InitHardware(void);
void USB_DeviceClockInit(void);
void USB_DeviceIsrEnable(void);
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle);
#endif

static void USB_DeviceVideoPrepareVideoData(void);
static usb_status_t USB_DeviceVideoRequest(class_handle_t handle, uint32_t event, void *param);
static usb_status_t USB_DeviceVideoCallback(class_handle_t handle, uint32_t event, void *param);
static void USB_DeviceVideoApplicationSetDefault(void);
static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param);
void USB_DeviceApplicationInit(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

extern usb_device_class_struct_t g_UsbDeviceVideoCameraConfig;

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static usb_device_video_probe_and_commit_controls_struct_t s_ProbeStruct;
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static usb_device_video_probe_and_commit_controls_struct_t s_CommitStruct;
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static usb_device_video_still_probe_and_commit_controls_struct_t s_StillProbeStruct;
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static usb_device_video_still_probe_and_commit_controls_struct_t s_StillCommitStruct;
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint32_t s_ClassRequestBuffer[(sizeof(usb_device_video_probe_and_commit_controls_struct_t) >> 2U) + 1U];
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_ImageBuffer[HS_STREAM_IN_PACKET_SIZE];
usb_video_camera_struct_t g_UsbDeviceVideoCamera;

usb_device_class_config_struct_t g_UsbDeviceVideoConfig[1] = {{
    USB_DeviceVideoCallback, (class_handle_t)NULL, &g_UsbDeviceVideoCameraConfig,
}};

usb_device_class_config_list_struct_t g_UsbDeviceVideoConfigList = {
    g_UsbDeviceVideoConfig, USB_DeviceCallback, 1U,
};
//extern camera_receiver_handle_t cameraReceiver;
extern uint32_t FrameBufferPointer1, FrameBufferPointer2;

/*******************************************************************************
 * Code
 ******************************************************************************/
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U))
void USB_OTG1_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(g_UsbDeviceVideoCamera.deviceHandle);
}
#endif
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U))
void USB_OTG2_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(g_UsbDeviceVideoCamera.deviceHandle);
}
#endif
void USB_DeviceClockInit(void)
{
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL, BOARD_USB_PHY_TXCAL45DP, BOARD_USB_PHY_TXCAL45DM,
    };
#endif
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    if (CONTROLLER_ID == kUSB_ControllerEhci0)
    {
        CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
        CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);
    }
    else
    {
        CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
        CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);
    }
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
#endif
}
void USB_DeviceIsrEnable(void)
{
    uint8_t irqNumber;
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    uint8_t usbDeviceEhciIrq[] = USBHS_IRQS;
    irqNumber = usbDeviceEhciIrq[CONTROLLER_ID - kUSB_ControllerEhci0];
#endif
/* Install isr, set priority, and enable IRQ. */
#if defined(__GIC_PRIO_BITS)
    GIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
#else
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
#endif
    EnableIRQ((IRQn_Type)irqNumber);
}
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle)
{
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    USB_DeviceEhciTaskFunction(deviceHandle);
#endif
}
#endif

/* Prepare next transfer payload */
static void USB_DeviceVideoPrepareVideoData(void)
{
    usb_device_video_mjpeg_payload_header_struct_t *payloadHeader;
    uint32_t maxPacketSize;
    uint32_t i;
    uint32_t sendLength;

    g_UsbDeviceVideoCamera.currentTime += 10000U;

    if (g_UsbDeviceVideoCamera.imagePosition < CAMERA_FRAME_BYTES)
    {
        //usb_echo("start, less than full frame\r\n");
        g_UsbDeviceVideoCamera.imageBuffer = (uint8_t *)(FrameBufferPointer1 + g_UsbDeviceVideoCamera.imagePosition -
                                                 sizeof(usb_device_video_mjpeg_payload_header_struct_t));
    }
    else
    {
        //usb_echo("start, more than full frame\r\n");
//        g_UsbDeviceVideoCamera.imageBuffer = (uint8_t *)&g_UsbDeviceVideoCamera.image_header[0];
        g_UsbDeviceVideoCamera.imageBuffer = (uint8_t *)(FrameBufferPointer1 - sizeof(usb_device_video_mjpeg_payload_header_struct_t));
    }
    payloadHeader = (usb_device_video_mjpeg_payload_header_struct_t *)&g_UsbDeviceVideoCamera.imageBuffer[0];
    memset(payloadHeader, 0, sizeof(usb_device_video_mjpeg_payload_header_struct_t));

    payloadHeader->bHeaderLength = sizeof(usb_device_video_mjpeg_payload_header_struct_t);
    payloadHeader->headerInfoUnion.bmheaderInfo = 0U;
    payloadHeader->headerInfoUnion.headerInfoBits.frameIdentifier = g_UsbDeviceVideoCamera.currentFrameId;
    g_UsbDeviceVideoCamera.imageBufferLength = sizeof(usb_device_video_mjpeg_payload_header_struct_t);

    if (g_UsbDeviceVideoCamera.stillImageTransmission)
    {
        //usb_echo("still frame\r\n");
        payloadHeader->headerInfoUnion.headerInfoBits.stillImage = 1U;
        maxPacketSize = USB_LONG_FROM_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.stillCommitStruct->dwMaxPayloadTransferSize);
    }
    else
    {
        //usb_echo("streaming\r\n");
        maxPacketSize = USB_LONG_FROM_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.commitStruct->dwMaxPayloadTransferSize);
    }
    maxPacketSize = maxPacketSize - sizeof(usb_device_video_mjpeg_payload_header_struct_t);
    if (g_UsbDeviceVideoCamera.waitForNewInterval)
    {
        if (g_UsbDeviceVideoCamera.currentTime < USB_LONG_FROM_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.commitStruct->dwFrameInterval))
        {
            return;
        }
        else
        {
            //usb_echo("interval ok, set image pos 0, return\r\n");
            g_UsbDeviceVideoCamera.imagePosition = 0U;
            g_UsbDeviceVideoCamera.currentTime = 0U;
            g_UsbDeviceVideoCamera.waitForNewInterval = 0U;
            payloadHeader->headerInfoUnion.headerInfoBits.endOfFrame = 1U;
            g_UsbDeviceVideoCamera.stillImageTransmission = 0U;
            g_UsbDeviceVideoCamera.currentFrameId ^= 1U;
            if (USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_TRANSMIT_STILL_IMAGE == g_UsbDeviceVideoCamera.stillImageTriggerControl)
            {
                g_UsbDeviceVideoCamera.stillImageTriggerControl = USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_NORMAL_OPERATION;
                g_UsbDeviceVideoCamera.stillImageTransmission = 1U;
            }
            return;
        }
    }

    if (g_UsbDeviceVideoCamera.imagePosition < CAMERA_FRAME_BYTES)
    {
        sendLength = CAMERA_FRAME_BYTES - g_UsbDeviceVideoCamera.imagePosition;
        if (sendLength > maxPacketSize)
        {
            sendLength = maxPacketSize;
        }
        g_UsbDeviceVideoCamera.imagePosition += sendLength;

        if (g_UsbDeviceVideoCamera.imagePosition >= CAMERA_FRAME_BYTES)
        {

            g_UsbDeviceVideoCamera.waitCameraCapture = 1;

            if (g_UsbDeviceVideoCamera.currentTime < USB_LONG_FROM_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.commitStruct->dwFrameInterval))
            {
               //usb_echo("frame completed, wait\r\n");
                g_UsbDeviceVideoCamera.waitForNewInterval = 1U;
            }
            else
            {
                //usb_echo("frame completed, set pos 0\r\n");
                g_UsbDeviceVideoCamera.imagePosition = 0U;
                g_UsbDeviceVideoCamera.currentTime = 0U;
                payloadHeader->headerInfoUnion.headerInfoBits.endOfFrame = 1U;
                g_UsbDeviceVideoCamera.stillImageTransmission = 0U;
                g_UsbDeviceVideoCamera.currentFrameId ^= 1U;
                if (USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_TRANSMIT_STILL_IMAGE == g_UsbDeviceVideoCamera.stillImageTriggerControl)
                {
                    g_UsbDeviceVideoCamera.stillImageTriggerControl = USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_NORMAL_OPERATION;
                    g_UsbDeviceVideoCamera.stillImageTransmission = 1U;
                }
            }
        }
        g_UsbDeviceVideoCamera.imageBufferLength += sendLength;
    }
}

static usb_status_t USB_DeviceVideoRequest(class_handle_t handle, uint32_t event, void *param)
{
    usb_device_control_request_struct_t *request = (usb_device_control_request_struct_t *)param;
    usb_device_video_probe_and_commit_controls_struct_t *probe =
        (usb_device_video_probe_and_commit_controls_struct_t *)(request->buffer);
    usb_device_video_probe_and_commit_controls_struct_t *commit =
        (usb_device_video_probe_and_commit_controls_struct_t *)(request->buffer);
    usb_device_video_still_probe_and_commit_controls_struct_t *still_probe =
        (usb_device_video_still_probe_and_commit_controls_struct_t *)(request->buffer);
    usb_device_video_still_probe_and_commit_controls_struct_t *still_commit =
        (usb_device_video_still_probe_and_commit_controls_struct_t *)(request->buffer);
    uint32_t temp32;
    usb_status_t error = kStatus_USB_Success;

    switch (event)
    {
        /* probe request */
        case USB_DEVICE_VIDEO_SET_CUR_VS_PROBE_CONTROL:
            if ((request->buffer == NULL) || (request->length == 0U))
            {
                return kStatus_USB_InvalidRequest;
            }
            temp32 = USB_LONG_FROM_LITTLE_ENDIAN_DATA(probe->dwFrameInterval);
            if ((temp32 >= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MIN_INTERVAL) &&
                (temp32 <= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_INTERVAL))
            {
                USB_LONG_TO_LITTLE_ENDIAN_DATA(temp32, g_UsbDeviceVideoCamera.probeStruct->dwFrameInterval);
            }
            temp32 = USB_LONG_FROM_LITTLE_ENDIAN_DATA(probe->dwMaxPayloadTransferSize);
            if ((temp32) && (temp32 < g_UsbDeviceVideoCamera.currentMaxPacketSize))
            {
                USB_LONG_TO_LITTLE_ENDIAN_DATA(temp32,
                                               g_UsbDeviceVideoCamera.probeStruct->dwMaxPayloadTransferSize);
            }
            g_UsbDeviceVideoCamera.probeStruct->bFormatIndex = probe->bFormatIndex;
            g_UsbDeviceVideoCamera.probeStruct->bFrameIndex = probe->bFrameIndex;
            break;
        case USB_DEVICE_VIDEO_GET_CUR_VS_PROBE_CONTROL:
            request->buffer = (uint8_t *)g_UsbDeviceVideoCamera.probeStruct;
            request->length = g_UsbDeviceVideoCamera.probeLength;
            break;
        case USB_DEVICE_VIDEO_GET_LEN_VS_PROBE_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.probeLength;
            request->length = sizeof(g_UsbDeviceVideoCamera.probeLength);
            break;
        case USB_DEVICE_VIDEO_GET_INFO_VS_PROBE_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.probeInfo;
            request->length = sizeof(g_UsbDeviceVideoCamera.probeInfo);
            break;
        /* commit request */
        case USB_DEVICE_VIDEO_SET_CUR_VS_COMMIT_CONTROL:
            if ((request->buffer == NULL) || (request->length == 0U))
            {
                return kStatus_USB_InvalidRequest;
            }
            temp32 = USB_LONG_FROM_LITTLE_ENDIAN_DATA(commit->dwFrameInterval);
            if ((temp32 >= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MIN_INTERVAL) &&
                (temp32 <= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_INTERVAL))
            {
                USB_LONG_TO_LITTLE_ENDIAN_DATA(temp32, g_UsbDeviceVideoCamera.commitStruct->dwFrameInterval);
            }

            temp32 = USB_LONG_FROM_LITTLE_ENDIAN_DATA(commit->dwMaxPayloadTransferSize);
            if ((temp32) && (temp32 < g_UsbDeviceVideoCamera.currentMaxPacketSize))
            {
                USB_LONG_TO_LITTLE_ENDIAN_DATA(temp32,
                                               g_UsbDeviceVideoCamera.commitStruct->dwMaxPayloadTransferSize);
            }
            g_UsbDeviceVideoCamera.commitStruct->bFormatIndex = commit->bFormatIndex;
            g_UsbDeviceVideoCamera.commitStruct->bFrameIndex = commit->bFrameIndex;
            break;
        case USB_DEVICE_VIDEO_GET_CUR_VS_COMMIT_CONTROL:
            request->buffer = (uint8_t *)g_UsbDeviceVideoCamera.commitStruct;
            request->length = g_UsbDeviceVideoCamera.commitLength;
            break;
        case USB_DEVICE_VIDEO_GET_LEN_VS_COMMIT_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.commitLength;
            request->length = sizeof(g_UsbDeviceVideoCamera.commitLength);
            break;
        case USB_DEVICE_VIDEO_GET_INFO_VS_COMMIT_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.commitInfo;
            request->length = sizeof(g_UsbDeviceVideoCamera.commitInfo);
            break;
        /* still probe request */
        case USB_DEVICE_VIDEO_SET_CUR_VS_STILL_PROBE_CONTROL:
            if ((request->buffer == NULL) || (request->length == 0U))
            {
                return kStatus_USB_InvalidRequest;
            }
            temp32 = USB_LONG_FROM_LITTLE_ENDIAN_DATA(still_probe->dwMaxPayloadTransferSize);
            if ((temp32) && (temp32 < g_UsbDeviceVideoCamera.currentMaxPacketSize))
            {
                USB_LONG_TO_LITTLE_ENDIAN_DATA(
                    temp32, g_UsbDeviceVideoCamera.stillProbeStruct->dwMaxPayloadTransferSize);
            }

            g_UsbDeviceVideoCamera.stillProbeStruct->bFormatIndex = still_probe->bFormatIndex;
            g_UsbDeviceVideoCamera.stillProbeStruct->bFrameIndex = still_probe->bFrameIndex;
            break;
        case USB_DEVICE_VIDEO_GET_CUR_VS_STILL_PROBE_CONTROL:
            request->buffer = (uint8_t *)g_UsbDeviceVideoCamera.stillProbeStruct;
            request->length = g_UsbDeviceVideoCamera.stillProbeLength;
            break;
        case USB_DEVICE_VIDEO_GET_LEN_VS_STILL_PROBE_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.stillProbeLength;
            request->length = sizeof(g_UsbDeviceVideoCamera.stillProbeLength);
            break;
        case USB_DEVICE_VIDEO_GET_INFO_VS_STILL_PROBE_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.stillProbeInfo;
            request->length = sizeof(g_UsbDeviceVideoCamera.stillProbeInfo);
            break;
        /* still commit request */
        case USB_DEVICE_VIDEO_SET_CUR_VS_STILL_COMMIT_CONTROL:
            if ((request->buffer == NULL) || (request->length == 0U))
            {
                return kStatus_USB_InvalidRequest;
            }
            temp32 = USB_LONG_FROM_LITTLE_ENDIAN_DATA(still_commit->dwMaxPayloadTransferSize);
            if ((temp32) && (temp32 < g_UsbDeviceVideoCamera.currentMaxPacketSize))
            {
                USB_LONG_TO_LITTLE_ENDIAN_DATA(
                    temp32, g_UsbDeviceVideoCamera.stillCommitStruct->dwMaxPayloadTransferSize);
            }

            g_UsbDeviceVideoCamera.stillCommitStruct->bFormatIndex = still_commit->bFormatIndex;
            g_UsbDeviceVideoCamera.stillCommitStruct->bFrameIndex = still_commit->bFrameIndex;
            break;
        case USB_DEVICE_VIDEO_GET_CUR_VS_STILL_COMMIT_CONTROL:
            request->buffer = (uint8_t *)g_UsbDeviceVideoCamera.stillCommitStruct;
            request->length = g_UsbDeviceVideoCamera.stillCommitLength;
            break;
        case USB_DEVICE_VIDEO_GET_LEN_VS_STILL_COMMIT_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.stillCommitLength;
            request->length = sizeof(g_UsbDeviceVideoCamera.stillCommitLength);
            break;
        case USB_DEVICE_VIDEO_GET_INFO_VS_STILL_COMMIT_CONTROL:
            request->buffer = &g_UsbDeviceVideoCamera.stillCommitInfo;
            request->length = sizeof(g_UsbDeviceVideoCamera.stillCommitInfo);
            break;
        /* still image trigger request */
        case USB_DEVICE_VIDEO_SET_CUR_VS_STILL_IMAGE_TRIGGER_CONTROL:
            g_UsbDeviceVideoCamera.stillImageTriggerControl = *(request->buffer);
            break;
        default:
            error = kStatus_USB_InvalidRequest;
            break;
    }
    return error;
}

void hex_dump(uint8_t* buf, uint32_t buf_len)
{
    for (int i = 0; i < buf_len; i++)
        usb_echo("%02X ", buf[i]);
    usb_echo("\r\n");
}

static uint32_t bytes_cnt;

/* USB device Video class callback */
static usb_status_t USB_DeviceVideoCallback(class_handle_t handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_Error;

    switch (event)
    {
        case kUSB_DeviceVideoEventStreamSendResponse:
            /* Stream data dent */
            if (g_UsbDeviceVideoCamera.attach)
            {
                /* Prepare the next stream data */
                USB_DeviceVideoPrepareVideoData();
                if (g_UsbDeviceVideoCamera.imageBufferLength > 25)
                    hex_dump(g_UsbDeviceVideoCamera.imageBuffer, 25);
                else
                    hex_dump(g_UsbDeviceVideoCamera.imageBuffer, g_UsbDeviceVideoCamera.imageBufferLength);
                error = USB_DeviceVideoSend(
                    g_UsbDeviceVideoCamera.videoHandle, USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN,
                    g_UsbDeviceVideoCamera.imageBuffer, g_UsbDeviceVideoCamera.imageBufferLength);
                bytes_cnt += g_UsbDeviceVideoCamera.imageBufferLength;
                usb_echo("sent from @%p, %d, total %d\r\n", g_UsbDeviceVideoCamera.imageBuffer, g_UsbDeviceVideoCamera.imageBufferLength, bytes_cnt);
            }
            break;
        case kUSB_DeviceVideoEventClassRequestBuffer:
            if (param && (g_UsbDeviceVideoCamera.attach))
            {
                /* Get the class-specific OUT buffer */
                usb_device_control_request_struct_t *request = (usb_device_control_request_struct_t *)param;

                if (request->length <= sizeof(usb_device_video_probe_and_commit_controls_struct_t))
                {
                    request->buffer = (uint8_t *)g_UsbDeviceVideoCamera.classRequestBuffer;
                    error = kStatus_USB_Success;
                }
            }
            break;
        default:
            if (param && (event > 0xFFU))
            {
                /* If the event is the class-specific request(Event > 0xFFU), handle the class-specific request */
                error = USB_DeviceVideoRequest(handle, event, param);
            }
            break;
    }

    return error;
}

/* Set to default state */
static void USB_DeviceVideoApplicationSetDefault(void)
{
    g_UsbDeviceVideoCamera.speed = USB_SPEED_FULL;
    g_UsbDeviceVideoCamera.attach = 0U;
    g_UsbDeviceVideoCamera.currentMaxPacketSize = HS_STREAM_IN_PACKET_SIZE;
    g_UsbDeviceVideoCamera.imageBuffer = s_ImageBuffer;
    g_UsbDeviceVideoCamera.probeStruct = &s_ProbeStruct;
    g_UsbDeviceVideoCamera.commitStruct = &s_CommitStruct;
    g_UsbDeviceVideoCamera.stillProbeStruct = &s_StillProbeStruct;
    g_UsbDeviceVideoCamera.stillCommitStruct = &s_StillCommitStruct;
    g_UsbDeviceVideoCamera.classRequestBuffer = &s_ClassRequestBuffer[0];

    g_UsbDeviceVideoCamera.probeStruct->bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoCamera.probeStruct->bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    USB_LONG_TO_LITTLE_ENDIAN_DATA(USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_DEFAULT_INTERVAL,
                                   g_UsbDeviceVideoCamera.probeStruct->dwFrameInterval);
    USB_LONG_TO_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.currentMaxPacketSize,
                                   g_UsbDeviceVideoCamera.probeStruct->dwMaxPayloadTransferSize);
    USB_LONG_TO_LITTLE_ENDIAN_DATA(USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE,
                                   g_UsbDeviceVideoCamera.probeStruct->dwMaxVideoFrameSize);

    g_UsbDeviceVideoCamera.commitStruct->bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoCamera.commitStruct->bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    USB_LONG_TO_LITTLE_ENDIAN_DATA(USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_DEFAULT_INTERVAL,
                                   g_UsbDeviceVideoCamera.commitStruct->dwFrameInterval);
    USB_LONG_TO_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.currentMaxPacketSize,
                                   g_UsbDeviceVideoCamera.commitStruct->dwMaxPayloadTransferSize);
    USB_LONG_TO_LITTLE_ENDIAN_DATA(USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE,
                                   g_UsbDeviceVideoCamera.commitStruct->dwMaxVideoFrameSize);

    g_UsbDeviceVideoCamera.probeInfo = 0x03U;
    g_UsbDeviceVideoCamera.probeLength = 26U;
    g_UsbDeviceVideoCamera.commitInfo = 0x03U;
    g_UsbDeviceVideoCamera.commitLength = 26U;

    g_UsbDeviceVideoCamera.stillProbeStruct->bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoCamera.stillProbeStruct->bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    g_UsbDeviceVideoCamera.stillProbeStruct->bCompressionIndex = 0x01U;
    USB_LONG_TO_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.currentMaxPacketSize,
                                   g_UsbDeviceVideoCamera.stillProbeStruct->dwMaxPayloadTransferSize);
    USB_LONG_TO_LITTLE_ENDIAN_DATA(USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE,
                                   g_UsbDeviceVideoCamera.stillProbeStruct->dwMaxVideoFrameSize);

    g_UsbDeviceVideoCamera.stillCommitStruct->bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoCamera.stillCommitStruct->bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    g_UsbDeviceVideoCamera.stillCommitStruct->bCompressionIndex = 0x01U;
    USB_LONG_TO_LITTLE_ENDIAN_DATA(g_UsbDeviceVideoCamera.currentMaxPacketSize,
                                   g_UsbDeviceVideoCamera.stillCommitStruct->dwMaxPayloadTransferSize);
    USB_LONG_TO_LITTLE_ENDIAN_DATA(USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE,
                                   g_UsbDeviceVideoCamera.stillCommitStruct->dwMaxVideoFrameSize);

    g_UsbDeviceVideoCamera.stillProbeInfo = 0x03U;
    g_UsbDeviceVideoCamera.stillProbeLength = sizeof(s_StillProbeStruct);
    g_UsbDeviceVideoCamera.stillCommitInfo = 0x03U;
    g_UsbDeviceVideoCamera.stillCommitLength = sizeof(s_StillCommitStruct);

    g_UsbDeviceVideoCamera.currentTime = 0U;
    g_UsbDeviceVideoCamera.currentFrameId = 0U;
    g_UsbDeviceVideoCamera.currentStreamInterfaceAlternateSetting = 0U;
    g_UsbDeviceVideoCamera.imageBufferLength = 0U;
    g_UsbDeviceVideoCamera.imageIndex = 0U;
    g_UsbDeviceVideoCamera.waitForNewInterval = 0U;
    g_UsbDeviceVideoCamera.waitCameraCapture = 0U;
    g_UsbDeviceVideoCamera.stillImageTransmission = 0U;
    g_UsbDeviceVideoCamera.stillImageTriggerControl = USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_NORMAL_OPERATION;
}

/* The device callback */
static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_Success;
    uint8_t *temp8 = (uint8_t *)param;
    uint16_t *temp16 = (uint16_t *)param;

    switch (event)
    {
        case kUSB_DeviceEventBusReset:
        {
            /* The device BUS reset signal detected */
            USB_DeviceVideoApplicationSetDefault();
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)) || \
    (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
            /* Get USB speed to configure the device, including max packet size and interval of the endpoints. */
            if (kStatus_USB_Success == USB_DeviceGetStatus(g_UsbDeviceVideoCamera.deviceHandle,
                                                           kUSB_DeviceStatusSpeed,
                                                           &g_UsbDeviceVideoCamera.speed))
            {
                USB_DeviceSetSpeed(g_UsbDeviceVideoCamera.deviceHandle, g_UsbDeviceVideoCamera.speed);
            }

            if (USB_SPEED_HIGH == g_UsbDeviceVideoCamera.speed)
            {
                g_UsbDeviceVideoCamera.currentMaxPacketSize = HS_STREAM_IN_PACKET_SIZE;
            }
#endif
        }
        break;
        case kUSB_DeviceEventSetConfiguration:
            if (USB_VIDEO_CAMERA_CONFIGURE_INDEX == (*temp8))
            {
                /* Set the configuration request */
                g_UsbDeviceVideoCamera.attach = 1U;
                g_UsbDeviceVideoCamera.currentConfiguration = *temp8;
            }
            break;
        case kUSB_DeviceEventSetInterface:
            if ((g_UsbDeviceVideoCamera.attach) && param)
            {
                /* Set alternateSetting of the interface request */
                uint8_t interface = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                uint8_t alternateSetting = (uint8_t)(*temp16 & 0x00FFU);

                if (g_UsbDeviceVideoCamera.currentInterfaceAlternateSetting[interface] != alternateSetting)
                {
                    if (!g_UsbDeviceVideoCamera.currentInterfaceAlternateSetting[interface])
                    {
                        if (USB_VIDEO_CAMERA_STREAM_INTERFACE_INDEX == interface)
                        {
                            USB_DeviceVideoPrepareVideoData();

                             if (g_UsbDeviceVideoCamera.imageBufferLength > 25)
                                hex_dump(g_UsbDeviceVideoCamera.imageBuffer, 25);
                            else
                                hex_dump(g_UsbDeviceVideoCamera.imageBuffer, g_UsbDeviceVideoCamera.imageBufferLength);
                            error = USB_DeviceVideoSend(
                                g_UsbDeviceVideoCamera.videoHandle, USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN,
                                g_UsbDeviceVideoCamera.imageBuffer, g_UsbDeviceVideoCamera.imageBufferLength);
                            bytes_cnt += g_UsbDeviceVideoCamera.imageBufferLength;
                            usb_echo("sent from @%p, %d, total %d\r\n", g_UsbDeviceVideoCamera.imageBuffer, g_UsbDeviceVideoCamera.imageBufferLength, bytes_cnt);
                        }
                    }
                    g_UsbDeviceVideoCamera.currentInterfaceAlternateSetting[interface] = alternateSetting;
                }
            }
            break;
        case kUSB_DeviceEventGetConfiguration:
            if (param)
            {
                /* Get the current configuration request */
                *temp8 = g_UsbDeviceVideoCamera.currentConfiguration;
                error = kStatus_USB_Success;
            }
            break;
        case kUSB_DeviceEventGetInterface:
            if (param)
            {
                /* Set the alternateSetting of the interface request */
                uint8_t interface = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                if (interface < USB_VIDEO_CAMERA_INTERFACE_COUNT)
                {
                    *temp16 =
                        (*temp16 & 0xFF00U) | g_UsbDeviceVideoCamera.currentInterfaceAlternateSetting[interface];
                    error = kStatus_USB_Success;
                }
                else
                {
                    error = kStatus_USB_InvalidRequest;
                }
            }
            break;
        case kUSB_DeviceEventGetDeviceDescriptor:
            if (param)
            {
                /* Get the device descriptor request */
                error = USB_DeviceGetDeviceDescriptor(handle, (usb_device_get_device_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetConfigurationDescriptor:
            if (param)
            {
                /* Get the configuration descriptor request */
                error = USB_DeviceGetConfigurationDescriptor(handle,
                                                             (usb_device_get_configuration_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetStringDescriptor:
            if (param)
            {
                /* Get the string descriptor request */
                error = USB_DeviceGetStringDescriptor(handle, (usb_device_get_string_descriptor_struct_t *)param);
            }
            break;
        default:
            break;
    }

    return error;
}

void USB_DeviceApplicationInit(void)
{
    USB_DeviceClockInit();
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
    SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

    USB_DeviceVideoApplicationSetDefault();

    if (kStatus_USB_Success !=
        USB_DeviceClassInit(CONTROLLER_ID, &g_UsbDeviceVideoConfigList, &g_UsbDeviceVideoCamera.deviceHandle))
    {
        usb_echo("USB device video camera failed\r\n");
        return;
    }
    else
    {
        g_UsbDeviceVideoCamera.videoHandle = g_UsbDeviceVideoConfigList.config->classHandle;
    }

    USB_DeviceIsrEnable();
}
