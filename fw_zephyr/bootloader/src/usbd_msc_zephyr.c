/*
 * DAPLink MSC class (usb/msc/usbd_msc.c) on the Zephyr legacy USB stack.
 *
 * Replaces the DAPLink RL-USB core (usbd_core.c, usb_lib.c, usbd_core_msc.c
 * and hic_hal usbd_STM32F103.c):
 *   - interface / endpoint descriptors
 *   - MSC class requests (Bulk-Only Reset, Get Max LUN)
 *   - CLEAR_FEATURE(ENDPOINT_HALT) handling (USBD_ReqSetClrFeature)
 *   - USBD_ReadEP / USBD_WriteEP / USBD_SetStallEP / USBD_ClrStallEP
 *
 * Endpoint events are only flagged from the USB ISR and processed by the
 * main task with usbd_msc_process(), like DAPLink USBD_SignalHandler() ->
 * USBD_Handler(), so all vfs_manager calls run in a single thread.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "rl_usb.h"
#include "usbd_msc_zephyr.h"

#define MSC_EP_OUT          0x01
#define MSC_EP_IN           0x82
#define MSC_EP_OUT_IDX      0
#define MSC_EP_IN_IDX       1
#define MSC_EP_MPS          64

/* usb_lib.c MSC configuration */
const U8  usbd_msc_ep_bulkin = MSC_EP_IN & 0x0F;
const U8  usbd_msc_ep_bulkout = MSC_EP_OUT;
const U16 usbd_msc_maxpacketsize[2] = {MSC_EP_MPS, MSC_EP_MPS};
/* Vendor (8), Product (16), Revision (4), like USBD_MSC_INQUIRY_DATA */
const U8 *usbd_msc_inquiry_data = (const U8 *)"MBED    " "VFS             " "0.1 ";
const U16 USBD_MSC_BulkBufSize = MSC_EP_MPS;
U8 USBD_MSC_BulkBuf[MSC_EP_MPS];

/* usbd_core.c state used by the MSC class */
USB_SETUP_PACKET USBD_SetupPacket;
U8   USBD_EP0Buf[8];
U32  USBD_EndPointHalt;
U32  USBD_EndPointStall;
U8   USBD_HighSpeed;

static atomic_t ep_events;
static struct k_poll_signal *usb_signal;
static volatile bool usb_configured;

USBD_CLASS_DESCR_DEFINE(primary, 0) struct {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_in_ep;
    struct usb_ep_descriptor if0_out_ep;
} __packed msc_desc = {
    .if0 = {
        .bLength = sizeof(struct usb_if_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_BCC_MASS_STORAGE,
        .bInterfaceSubClass = MSC_SUBCLASS_SCSI,
        .bInterfaceProtocol = MSC_PROTOCOL_BULK_ONLY,
        .iInterface = 0,
    },
    .if0_in_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = MSC_EP_IN,
        .bmAttributes = USB_DC_EP_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(MSC_EP_MPS),
        .bInterval = 0,
    },
    .if0_out_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = MSC_EP_OUT,
        .bmAttributes = USB_DC_EP_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(MSC_EP_MPS),
        .bInterval = 0,
    },
};

static void msc_signal(uint32_t event)
{
    atomic_or(&ep_events, event);
    k_poll_signal_raise(usb_signal, 0);
}

static void msc_ep_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status)
{
    if (status == USB_DC_EP_DATA_OUT) {
        msc_signal(USBD_EVT_OUT);
    } else if (status == USB_DC_EP_DATA_IN) {
        msc_signal(USBD_EVT_IN);
    }
}

static struct usb_ep_cfg_data msc_ep_data[] = {
    [MSC_EP_OUT_IDX] = {
        .ep_cb = msc_ep_cb,
        .ep_addr = MSC_EP_OUT
    },
    [MSC_EP_IN_IDX] = {
        .ep_cb = msc_ep_cb,
        .ep_addr = MSC_EP_IN
    }
};

static int msc_class_handler(struct usb_setup_packet *setup,
                             int32_t *len, uint8_t **data)
{
    /* usbd_core_msc.c USBD_EndPoint0_Setup_MSC_ReqToIF() */
    if (setup->wIndex != msc_desc.if0.bInterfaceNumber) {
        return -ENOTSUP;
    }

    switch (setup->bRequest) {
    case MSC_REQUEST_RESET:
        if (setup->wValue == 0 && setup->wLength == 0 && USBD_MSC_Reset()) {
            *len = 0;
            return 0;
        }
        break;
    case MSC_REQUEST_GET_MAX_LUN:
        if (setup->wValue == 0 && setup->wLength == 1 && USBD_MSC_GetMaxLUN()) {
            *data = USBD_EP0Buf;
            *len = 1;
            return 0;
        }
        break;
    }

    return -ENOTSUP;
}

static int msc_custom_handler(struct usb_setup_packet *setup,
                              int32_t *len, uint8_t **data)
{
    uint32_t n, m;

    /* usbd_core.c USBD_ReqSetClrFeature(): endpoint CLEAR_FEATURE(HALT) */
    if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_ENDPOINT ||
        setup->bRequest != USB_SREQ_CLEAR_FEATURE ||
        setup->wValue != USB_SFS_ENDPOINT_HALT) {
        return -EINVAL;
    }

    n = setup->wIndex & 0x8F;
    m = (n & 0x80) ? ((1 << 16) << (n & 0x0F)) : (1 << n);
    if ((n & 0x0F) == 0) {
        return -EINVAL;
    }

    if ((USBD_EndPointStall & m) != 0) {
        /* EP must stay stalled */
        *len = 0;
        return 0;
    }

    USBD_ClrStallEP(n);
    USBD_SetupPacket.wIndexL = (uint8_t)setup->wIndex;
    USBD_MSC_ClrStallEP(n);
    USBD_EndPointHalt &= ~m;
    *len = 0;

    return 0;
}

static void msc_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status,
                          const uint8_t *param)
{
    ARG_UNUSED(cfg);
    ARG_UNUSED(param);

    switch (status) {
    case USB_DC_RESET:
        usb_configured = false;
        USBD_EndPointHalt = 0;
        USBD_EndPointStall = 0;
        msc_signal(USBD_MSC_EVT_RESET);
        break;
    case USB_DC_CONFIGURED:
        usb_configured = true;
        break;
    case USB_DC_DISCONNECTED:
        usb_configured = false;
        break;
    default:
        break;
    }
}

static void msc_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber)
{
    ARG_UNUSED(head);
    msc_desc.if0.bInterfaceNumber = bInterfaceNumber;
}

USBD_DEFINE_CFG_DATA(msc_config) = {
    .usb_device_description = NULL,
    .interface_config = msc_interface_config,
    .interface_descriptor = &msc_desc.if0,
    .cb_usb_status = msc_status_cb,
    .interface = {
        .class_handler = msc_class_handler,
        .custom_handler = msc_custom_handler,
    },
    .num_endpoints = ARRAY_SIZE(msc_ep_data),
    .endpoint = msc_ep_data,
};

/* usbd_hw.h -------------------------------------------------------------- */

static uint8_t ep_addr(U32 EPNum)
{
    return (EPNum & 0x80) ? (0x80 | (EPNum & 0x0F)) : (EPNum & 0x0F);
}

U32 USBD_ReadEP(U32 EPNum, U8 *pData, U32 size)
{
    uint32_t read = 0;

    usb_ep_read_wait(ep_addr(EPNum), pData, size, &read);
    usb_ep_read_continue(ep_addr(EPNum));

    return read;
}

U32 USBD_WriteEP(U32 EPNum, U8 *pData, U32 cnt)
{
    uint32_t written = 0;

    usb_write(ep_addr(EPNum), pData, cnt, &written);

    return written;
}

void USBD_SetStallEP(U32 EPNum)
{
    usb_ep_set_stall(ep_addr(EPNum));
}

void USBD_ClrStallEP(U32 EPNum)
{
    usb_ep_clear_stall(ep_addr(EPNum));
}

/* API -------------------------------------------------------------------- */

int usbd_msc_zephyr_init(struct k_poll_signal *signal)
{
    usb_signal = signal;
    return 0;
}

void usbd_msc_process(void)
{
    uint32_t events = atomic_clear(&ep_events);

    if (events & USBD_MSC_EVT_RESET) {
        USBD_MSC_Reset_Event();
    }
    if (events & (USBD_EVT_OUT | USBD_EVT_IN)) {
        USBD_MSC_EP_BULK_Event(events);
    }
}

bool usbd_configured(void)
{
    return usb_configured;
}

void usbd_connect(bool connect)
{
    if (connect) {
        usb_enable(NULL);
    } else {
        usb_disable();
        usb_configured = false;
    }
}
