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

#include "fsl_device_registers.h"

#include "board.h"
#include "clock_config.h"
#include "pin_mux.h"
#include "fsl_debug_console.h"


#include "video_camera.h"
#include "app_sai.h"
#include "fft_camera.h"

#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

/*******************************************************************************
 * Variables
 ******************************************************************************/
uint32_t FrameBufferPointer1, FrameBufferPointer2;


extern usb_video_camera_struct_t g_UsbDeviceVideoCamera;

extern void USB_DeviceApplicationInit(void);

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief The main function.
 */
void main(void)
{
    BOARD_ConfigMPU();

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    app_sai_init();
    USB_DeviceApplicationInit();

    PRINTF("This is a USB video demo appliation.\r\n");

    FFT_CAMERA_DoCapture(NULL, NULL, 0);

    /* Wait to get the full frame buffer to show. */
    while (kStatus_Success != FFT_CAMERA_GetFullBuffer(NULL, &FrameBufferPointer1))
    {
    }


    USB_DeviceRun(g_UsbDeviceVideoCamera.deviceHandle);

    while (1U)
    {
#if USB_DEVICE_CONFIG_USE_TASK
        USB_DeviceTaskFn(g_UsbDeviceVideoVirtualCamera.deviceHandle);
#endif

        if (emptyBlock > 0)
        {
            xfer.data     = Buffer + rx_index * BUFFER_SIZE;
            xfer.dataSize = BUFFER_SIZE;
            if (kStatus_Success == SAI_TransferReceiveEDMA(DEMO_SAI, &rxHandle, &xfer))
            {
                rx_index++;
            }
            if (rx_index == BUFFER_NUMBER)
            {
                rx_index = 0U;
            }
        }
        if (emptyBlock < BUFFER_NUMBER)
        {
            xfer.data     = Buffer + tx_index * BUFFER_SIZE;
            xfer.dataSize = BUFFER_SIZE;

            if (kStatus_Success == SAI_TransferSendEDMA(DEMO_SAI, &txHandle, &xfer))
            {
                //tx_index++;
            }

            FFT_CAMERA_DoCapture(NULL, xfer.data, xfer.dataSize);

            tx_index++;

            if (tx_index == BUFFER_NUMBER)
            {
                tx_index = 0U;
            }

        }
    }
}
