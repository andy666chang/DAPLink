/*
 * Minimal DAPLink RL-USB API for the Zephyr bootloader.
 *
 * Provides what usb/msc/usbd_msc.c (DAPLink MSC class) and
 * drag-n-drop/vfs_manager.c need, on top of the Zephyr legacy USB
 * device stack (src/usbd_msc_zephyr.c).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RL_USB_H
#define RL_USB_H

#include <stdint.h>
#include "compiler.h"

typedef uint8_t  BOOL;
typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;

#ifndef __TRUE
#define __TRUE  1
#endif
#ifndef __FALSE
#define __FALSE 0
#endif

#include "usb_msc.h"

/* USB Device events (usbd_event.h) */
#define USBD_EVT_OUT            (1 << 1)
#define USBD_EVT_IN             (1 << 2)

/* Setup packet, only wIndexL is used by the MSC class */
typedef struct {
    U8 wIndexL;
} USB_SETUP_PACKET;

extern USB_SETUP_PACKET USBD_SetupPacket;
extern U8   USBD_EP0Buf[];
extern U32  USBD_EndPointHalt;
extern U32  USBD_EndPointStall;
extern U8   USBD_HighSpeed;

/* MSC configuration (usb_lib.c) */
extern const U8  usbd_msc_ep_bulkin;
extern const U8  usbd_msc_ep_bulkout;
extern const U16 usbd_msc_maxpacketsize[2];
extern const U8 *usbd_msc_inquiry_data;
extern const U16 USBD_MSC_BulkBufSize;
extern U8   USBD_MSC_BulkBuf[];

/* MSC class state (usbd_msc.c) */
extern BOOL USBD_MSC_MediaReady;
extern BOOL USBD_MSC_ReadOnly;
extern U32  USBD_MSC_MemorySize;
extern U32  USBD_MSC_BlockSize;
extern U32  USBD_MSC_BlockGroup;
extern U32  USBD_MSC_BlockCount;
extern U8  *USBD_MSC_BlockBuf;
extern MSC_CSW USBD_MSC_CSW;

/* Hardware layer (usbd_hw.h) */
U32  USBD_ReadEP(U32 EPNum, U8 *pData, U32 size);
U32  USBD_WriteEP(U32 EPNum, U8 *pData, U32 cnt);
void USBD_SetStallEP(U32 EPNum);
void USBD_ClrStallEP(U32 EPNum);

/* MSC class (usbd_msc.h) */
void usbd_msc_init(void);
void usbd_msc_read_sect(U32 block, U8 *buf, U32 num_of_blocks);
void usbd_msc_write_sect(U32 block, U8 *buf, U32 num_of_blocks);
void usbd_msc_start_stop(BOOL start);

BOOL USBD_MSC_Reset(void);
BOOL USBD_MSC_GetMaxLUN(void);
void USBD_MSC_SetCSW(void);
void USBD_MSC_ClrStallEP(U32 EPNum);
void USBD_MSC_Reset_Event(void);
void USBD_MSC_EP_BULK_Event(U32 event);

#endif
