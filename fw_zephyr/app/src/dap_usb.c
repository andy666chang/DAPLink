/*
 * CMSIS-DAP v2 (WinUSB bulk) USB interface.
 *
 * Replaces DAPLink usb/bulk/usbd_bulk.c + DAP_queue.c on top of the
 * Zephyr USB device stack. Requests are received in the USB callback and
 * executed by the DAP thread (DAPLink runs them from the USB ISR).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/bos.h>
#include <zephyr/usb/msos_desc.h>

#include <usb_descriptor.h>

#include "DAP_config.h"
#include "DAP.h"
#include "dap_usb.h"
#include "led.h"

#define DAP_IFACE_STR_DESC      "CMSIS-DAP v2"

#define DAP_USB_EP_OUT          0x01
#define DAP_USB_EP_IN           0x81
#define DAP_USB_EP_OUT_IDX      0
#define DAP_USB_EP_IN_IDX       1

#define WINUSB_VENDOR_CODE      0x20

struct dap_request {
    uint16_t len;
    uint8_t data[DAP_PACKET_SIZE];
};

K_MSGQ_DEFINE(dap_rx_queue, sizeof(struct dap_request), DAP_PACKET_COUNT, 4);

static uint8_t rx_buf[DAP_PACKET_SIZE];
static uint8_t tx_buf[DAP_PACKET_SIZE];
static atomic_t rx_stalled;

struct dap_iface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bString[USB_BSTRING_LENGTH(DAP_IFACE_STR_DESC)];
} __packed;

USBD_STRING_DESCR_USER_DEFINE(primary) struct dap_iface_descriptor dap_iface_desc = {
    .bLength = USB_STRING_DESCRIPTOR_LENGTH(DAP_IFACE_STR_DESC),
    .bDescriptorType = USB_DESC_STRING,
    .bString = DAP_IFACE_STR_DESC
};

/* {CDB3B5AD-293B-4663-AA36-1AAE46463776} */
#define CMSIS_DAP_V2_DEVICE_INTERFACE_GUID \
    '{', 0x00, 'C', 0x00, 'D', 0x00, 'B', 0x00, '3', 0x00, 'B', 0x00, \
    '5', 0x00, 'A', 0x00, 'D', 0x00, '-', 0x00, '2', 0x00, '9', 0x00, \
    '3', 0x00, 'B', 0x00, '-', 0x00, '4', 0x00, '6', 0x00, '6', 0x00, \
    '3', 0x00, '-', 0x00, 'A', 0x00, 'A', 0x00, '3', 0x00, '6', 0x00, \
    '-', 0x00, '1', 0x00, 'A', 0x00, 'A', 0x00, 'E', 0x00, '4', 0x00, \
    '6', 0x00, '4', 0x00, '6', 0x00, '3', 0x00, '7', 0x00, '7', 0x00, \
    '6', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00

#define COMPATIBLE_ID_WINUSB \
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00

static struct msosv2_descriptor {
    struct msosv2_descriptor_set_header header;
#if defined(CONFIG_USB_COMPOSITE_DEVICE)
    struct msosv2_function_subset_header subset_header;
#endif
    struct msosv2_compatible_id compatible_id;
    struct msosv2_guids_property guids_property;
} __packed msosv2_cmsis_dap_desc = {
    /* Tell Windows to bind WinUSB to the CMSIS-DAP v2 interface */
    .header = {
        .wLength = sizeof(struct msosv2_descriptor_set_header),
        .wDescriptorType = MS_OS_20_SET_HEADER_DESCRIPTOR,
        .dwWindowsVersion = 0x06030000,
        .wTotalLength = sizeof(struct msosv2_descriptor),
    },
#if defined(CONFIG_USB_COMPOSITE_DEVICE)
    .subset_header = {
        .wLength = sizeof(struct msosv2_function_subset_header),
        .wDescriptorType = MS_OS_20_SUBSET_HEADER_FUNCTION,
        .wSubsetLength = sizeof(struct msosv2_function_subset_header)
            + sizeof(struct msosv2_compatible_id)
            + sizeof(struct msosv2_guids_property),
    },
#endif
    .compatible_id = {
        .wLength = sizeof(struct msosv2_compatible_id),
        .wDescriptorType = MS_OS_20_FEATURE_COMPATIBLE_ID,
        .CompatibleID = {COMPATIBLE_ID_WINUSB},
    },
    .guids_property = {
        .wLength = sizeof(struct msosv2_guids_property),
        .wDescriptorType = MS_OS_20_FEATURE_REG_PROPERTY,
        .wPropertyDataType = MS_OS_20_PROPERTY_DATA_REG_MULTI_SZ,
        .wPropertyNameLength = 42,
        .PropertyName = {DEVICE_INTERFACE_GUIDS_PROPERTY_NAME},
        .wPropertyDataLength = 80,
        .bPropertyData = {CMSIS_DAP_V2_DEVICE_INTERFACE_GUID},
    },
};

USB_DEVICE_BOS_DESC_DEFINE_CAP struct usb_bos_msosv2_desc {
    struct usb_bos_platform_descriptor platform;
    struct usb_bos_capability_msos cap;
} __packed bos_cap_msosv2 = {
    /* Microsoft OS 2.0 Platform Capability Descriptor */
    .platform = {
        .bLength = sizeof(struct usb_bos_platform_descriptor)
            + sizeof(struct usb_bos_capability_msos),
        .bDescriptorType = USB_DESC_DEVICE_CAPABILITY,
        .bDevCapabilityType = USB_BOS_CAPABILITY_PLATFORM,
        .bReserved = 0,
        /* MS OS 2.0 Platform Capability ID D8DD60DF-4589-4CC7-9CD2-659D9E648A9F */
        .PlatformCapabilityUUID = {
            0xDF, 0x60, 0xDD, 0xD8,
            0x89, 0x45,
            0xC7, 0x4C,
            0x9C, 0xD2,
            0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,
        },
    },
    .cap = {
        .dwWindowsVersion = sys_cpu_to_le32(0x06030000),
        .wMSOSDescriptorSetTotalLength = sys_cpu_to_le16(sizeof(msosv2_cmsis_dap_desc)),
        .bMS_VendorCode = WINUSB_VENDOR_CODE,
        .bAltEnumCode = 0x00
    },
};

static int msosv2_vendor_handle_req(struct usb_setup_packet *setup,
                                    int32_t *len, uint8_t **data)
{
    if (usb_reqtype_is_to_device(setup)) {
        return -ENOTSUP;
    }

    if (setup->bRequest == WINUSB_VENDOR_CODE &&
        setup->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {
        *data = (uint8_t *)(&msosv2_cmsis_dap_desc);
        *len = sizeof(msosv2_cmsis_dap_desc);
        return 0;
    }

    return -ENOTSUP;
}

USBD_CLASS_DESCR_DEFINE(primary, 1) struct {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_out_ep;
    struct usb_ep_descriptor if0_in_ep;
} __packed dapusb_desc = {
    .if0 = {
        .bLength = sizeof(struct usb_if_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_BCC_VENDOR,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,
    },
    .if0_out_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = DAP_USB_EP_OUT,
        .bmAttributes = USB_DC_EP_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(DAP_PACKET_SIZE),
        .bInterval = 0,
    },
    .if0_in_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = DAP_USB_EP_IN,
        .bmAttributes = USB_DC_EP_BULK,
        .wMaxPacketSize = sys_cpu_to_le16(DAP_PACKET_SIZE),
        .bInterval = 0,
    },
};

static struct usb_ep_cfg_data dapusb_ep_data[] = {
    [DAP_USB_EP_OUT_IDX] = {
        .ep_cb = usb_transfer_ep_callback,
        .ep_addr = DAP_USB_EP_OUT
    },
    [DAP_USB_EP_IN_IDX] = {
        .ep_cb = usb_transfer_ep_callback,
        .ep_addr = DAP_USB_EP_IN
    }
};

static void dapusb_read_cb(uint8_t ep, int size, void *priv);

static void dapusb_start_read(struct usb_cfg_data *cfg)
{
    usb_transfer(cfg->endpoint[DAP_USB_EP_OUT_IDX].ep_addr, rx_buf, sizeof(rx_buf),
                 USB_TRANS_READ, dapusb_read_cb, cfg);
}

static void dapusb_read_cb(uint8_t ep, int size, void *priv)
{
    struct usb_cfg_data *cfg = priv;
    struct dap_request req;

    ARG_UNUSED(ep);

    if (size > 0) {
        if (rx_buf[0] == ID_DAP_TransferAbort) {
            /* Handled immediately, like DAPLink/CMSIS-DAP reference */
            DAP_TransferAbort = 1U;
        } else {
            req.len = MIN(size, DAP_PACKET_SIZE);
            memcpy(req.data, rx_buf, req.len);
            k_msgq_put(&dap_rx_queue, &req, K_NO_WAIT);
        }
    }

    /* Flow control: NAK the host until a request buffer is free */
    if (k_msgq_num_free_get(&dap_rx_queue) == 0) {
        atomic_set(&rx_stalled, 1);
        return;
    }

    dapusb_start_read(cfg);
}

static void dapusb_dev_status_cb(struct usb_cfg_data *cfg,
                                 enum usb_dc_status_code status,
                                 const uint8_t *param)
{
    ARG_UNUSED(param);

    switch (status) {
    case USB_DC_CONFIGURED:
        if (!usb_transfer_is_busy(cfg->endpoint[DAP_USB_EP_OUT_IDX].ep_addr)) {
            atomic_set(&rx_stalled, 0);
            dapusb_start_read(cfg);
        }
        break;
    case USB_DC_RESET:
    case USB_DC_DISCONNECTED:
        k_msgq_purge(&dap_rx_queue);
        break;
    default:
        break;
    }
}

static void dapusb_interface_config(struct usb_desc_header *head,
                                    uint8_t bInterfaceNumber)
{
    ARG_UNUSED(head);

    dapusb_desc.if0.bInterfaceNumber = bInterfaceNumber;
#if defined(CONFIG_USB_COMPOSITE_DEVICE)
    msosv2_cmsis_dap_desc.subset_header.bFirstInterface = bInterfaceNumber;
#endif
}

USBD_DEFINE_CFG_DATA(dapusb_config) = {
    .usb_device_description = NULL,
    .interface_config = dapusb_interface_config,
    .interface_descriptor = &dapusb_desc.if0,
    .cb_usb_status = dapusb_dev_status_cb,
    .interface = {
        .class_handler = NULL,
        .custom_handler = NULL,
        .vendor_handler = msosv2_vendor_handle_req,
    },
    .num_endpoints = ARRAY_SIZE(dapusb_ep_data),
    .endpoint = dapusb_ep_data
};

int dap_usb_init(void)
{
    /* Add MS OS 2.0 BOS descriptor to BOS structure */
    usb_bos_register_cap((void *)&bos_cap_msosv2);
    /* Point interface index to string descriptor */
    dapusb_desc.if0.iInterface = usb_get_str_descriptor_idx(&dap_iface_desc);

    return 0;
}

int dap_usb_process(void)
{
    uint8_t ep = dapusb_config.endpoint[DAP_USB_EP_IN_IDX].ep_addr;
    struct dap_request req;
    uint32_t len;
    int ret;

    k_msgq_get(&dap_rx_queue, &req, K_FOREVER);

    if (atomic_cas(&rx_stalled, 1, 0)) {
        dapusb_start_read(&dapusb_config);
    }

    /* Commands shorter than declared read zeros instead of stale data */
    if (req.len < DAP_PACKET_SIZE) {
        memset(&req.data[req.len], 0, DAP_PACKET_SIZE - req.len);
    }

    len = DAP_ExecuteCommand(req.data, tx_buf) & 0xFFFFU;
    led_flash(LED_DAP);

    ret = usb_transfer_sync(ep, tx_buf, len, USB_TRANS_WRITE | USB_TRANS_NO_ZLP);
    if (ret < 0 || (uint32_t)ret != len) {
        return -EIO;
    }

    return 0;
}
