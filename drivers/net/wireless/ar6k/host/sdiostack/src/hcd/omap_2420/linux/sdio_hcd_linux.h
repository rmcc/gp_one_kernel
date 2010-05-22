// Copyright (c) 2004-2006 Atheros Communications Inc.
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
@file: sdio_hcd_linux.h

@abstract: include file for Texas Instruments OMAP host controller, linux dependent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_HCD_LINUX_H___
#define __SDIO_HCD_LINUX_H___


#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/errno.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/device.h>
#endif
#include <asm/arch/dma.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <asm/hardware/clock.h>
#endif
#include <asm/irq.h>
#include <linux/delay.h>


#define SDHCD_MAX_DEVICE_NAME     12

#define OMAP_BASE_ADDRESS1        0x4809C000
#define OMAP_BASE_LENGTH          0x6A

#define OMAP_MODULE_CLOCK         96000000
#define OMAP_MAX_DEVICE_COUNT     2 
#define OMAP_DEFAULT_DEVICE_COUNT 1
#define OMAP_DEFAULT_FIRST_DEVICE 0
#define OMAP_DMA_MASK             0xFFFFFFFF

#define HCD_COMMAND_MIN_POLLING_CLOCK 5000000

/* debounce delay for slot */
#define SD_SLOT_DEBOUNCE_MS  500


/* device base name */
#define SDIO_BD_BASE "sdiobd"

/* mapped memory address */
typedef struct _SDHCD_MEMORY {
    ULONG Raw;      /* start of address range */
    ULONG Length;   /* length of range */
    PVOID pMapped;  /* the mapped address */
}SDHCD_MEMORY, *PSDHCD_MEMORY;

typedef void (*PDMA_TRANSFER_COMPLETION)(PVOID,SDIO_STATUS,BOOL);

/* device data*/
typedef struct _HCD_OS_INFO {
    POS_PNPDEVICE pBusDevice;      /* our device registered with bus driver */
    SDHCD_MEMORY  Address;          /* memory address of this device */ 
    spinlock_t    AddressSpinlock;   /* use to protect reghisters when needed */
    UINT8         InitStateMask;
#define SDIO_BASE_MAPPED           0x01
#define SDIO_IRQ_INTERRUPT_INIT    0x04
#define SDHC_REGISTERED            0x10
#define SDHC_HW_INIT               0x40
#define SDHC_TIMER_INIT            0x80
    spinlock_t   Lock;            /* lock against the ISR */
    BOOL         CardInserted;    /* card inserted flag */
    BOOL         Cancel;
    BOOL         ShuttingDown;    /* indicates shut down of HCD) */
    struct work_struct iocomplete_work; /* work item definintions */
    struct work_struct carddetect_work; /* work item definintions */
    struct work_struct sdioirq_work; /* work item definintions */
    DMA_ADDRESS hDmaBuffer;       /* handle for data buffer */
    PUINT8      pDmaBuffer;       /* virtual address of command buffer */
    UINT32      CommonBufferSize; /* size of CommonBuffer */
    int         Interrupt;
    int         DmaRxId;
    int         DmaTxId;
    int         DmaRxChannel;      /* receive DMA channel */
    int         DmaTxChannel;      /* transmit DMA channel */   
    int         LastTransfer;      /* length of last transfer */
    PSDDMA_DESCRIPTOR pDmaList;    /* in use scatter-gather list */
    UINT        SGcount;           /* count of in-use scatter gather list */
    PVOID       TransferContext;   /* context passed to TransferCompletion routine */ 
    PDMA_TRANSFER_COMPLETION pTransferCompletion; /* transfer completion routine */
    int         DAT0_gpio_pin;      /* gpio pin tied to dat 0 pin (polling operation) */
    int         DAT0_gpio_conf_offset;
    int         DAT0_gpio_pad_conf_byte;
    int         DAT0_gpio_pad_mode_value;
}HCD_OS_INFO, *PHCD_OS_INFO;

typedef struct _SDHCD_DRIVER {
    OS_PNPDEVICE   HcdDevice;     /* the OS device for this HCD */
    OS_PNPDRIVER   HcdDriver;     /* the OS driver for this HCD */ 
    SDDMA_DESCRIPTION Dma;        /* driver DMA description */
}SDHCD_DRIVER, *PSDHCD_DRIVER;


#define WORK_ITEM_IO_COMPLETE  0
#define WORK_ITEM_CARD_DETECT  1
#define WORK_ITEM_SDIO_IRQ     2

 
#define READ_HOST_REG32(pDevice, OFFSET)  \
    _READ_DWORD_REG((((UINT32)((pDevice)->OSInfo.Address.pMapped))) + (OFFSET))
#define WRITE_HOST_REG32(pDevice, OFFSET, VALUE) \
    _WRITE_DWORD_REG((((UINT32)((pDevice)->OSInfo.Address.pMapped))) + (OFFSET),(VALUE))
#define READ_HOST_REG16(pDevice, OFFSET)  \
    _READ_WORD_REG((((UINT32)((pDevice)->OSInfo.Address.pMapped))) + (OFFSET))
#define WRITE_HOST_REG16(pDevice, OFFSET, VALUE) \
    _WRITE_WORD_REG((((UINT32)((pDevice)->OSInfo.Address.pMapped))) + (OFFSET),(VALUE))

#define GET_HC_REG_BASE(pDevice) (pDevice)->OSInfo.Address.pMapped

#define OMAP_USE_DBG_GPIO
/* prototypes */
#endif /* __SDIO_HCD_LINUX_H___ */
