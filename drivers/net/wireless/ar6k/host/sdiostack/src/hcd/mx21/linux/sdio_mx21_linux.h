// Copyright (c) 2006 Atheros Communications Inc.
// 
//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_mx21_linux.h

@abstract: include file for MX21 local bus host controller, linux dependent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_HCD_LINUX_H___
#define __SDIO_HCD_LINUX_H___

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <asm/irq.h>

#define SDHC_DMA_COMMON_BUFFER_SIZE       16*1024 
#define SDHC_MAX_BYTES_PER_DMA_DESCRIPTOR 16*1024*1024 /* 16MB per descriptor */

/* mapped memory address */
typedef struct _SDHCD_MEMORY {
    ULONG Raw;      /* start of address range */
    ULONG Length;   /* length of range */
    PVOID pMapped;  /* the mapped address */
}SDHCD_MEMORY, *PSDHCD_MEMORY;

typedef void (*PDMA_TRANSFER_COMPLETION)(PVOID,SDIO_STATUS,BOOL);

#define SDHCD_MAX_DEVICE_NAME 12

/* device data*/
typedef struct _SDHCD_DEVICE {
    OS_PNPDEVICE   HcdDevice;     /* the OS device for this HCD */
    OS_PNPDRIVER   HcdDriver;     /* the OS driver for this HCD */ 
    SDDMA_DESCRIPTION Dma;        /* driver DMA description */
    POS_PNPDEVICE pBusDevice;     /* our device registered with bus driver */
    UINT    ControllerInterrupt;  /* controller interrupt */
    UINT8   InitStateMask;
#define SDHC_INTERRUPT_INIT        0x01
#define SDHC_REGISTERED            0x10
#define SDHC_HW_INIT               0x20
#define SDHC_DMA_ALLOCATED         0x40
    SDHCD_MEMORY ControlRegs;    /* memory addresses of device */
    spinlock_t   Lock;           /* lock against the ISR */
    BOOL         StartUpCheck;
    PDMA_TRANSFER_COMPLETION pDmaCompletion;
    PVOID        pContext;     
    PUINT8       pDmaCommonBuffer;      /* common buffer for DMA */
    DMA_ADDRESS  DmaCommonBufferPhys;   /* physical address for common buffer */
    UINT32       DmaCommonBufferSize;   /* size of DMA common buffer */
    INT          DmaChannel;            /* DMA channel to use */
    BOOL         DmaSgMapped;
    INT          LastRxCopy;
    UINT32       PeripheralClockRate;
    UINT32       DmaHclkErrata;         /* see errata 13 in MX21 chip errata doc */
    BOOL         LocalIrqDisabled;
}SDHCD_DEVICE, *PSDHCD_DEVICE;

#define OsMicroDelay(x) udelay((x))
 
#define READ_HC_REG(pC, OFFSET)  \
    _READ_DWORD_REG(((UINT32)(pC)->Device.ControlRegs.pMapped) + (OFFSET))
#define WRITE_HC_REG(pC, OFFSET, VALUE) \
    _WRITE_DWORD_REG(((UINT32)(pC)->Device.ControlRegs.pMapped) + (OFFSET),(VALUE))

#define WRITE_HC_REG_D(pC, OFFSET, VALUE) \
    { WRITE_HC_REG(pC, OFFSET, VALUE);OsMicroDelay(1);}
    
#define GET_HC_REG_BASE(pC) ((UINT32)((pC)->Device.ControlRegs.pMapped))

#endif
