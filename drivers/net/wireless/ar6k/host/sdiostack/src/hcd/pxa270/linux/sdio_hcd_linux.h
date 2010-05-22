// Copyright (c) 2005, 2006 Atheros Communications Inc.
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

@abstract: include file for PXA270 local bus host controller, linux dependent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_HCD_LINUX_H___
#define __SDIO_HCD_LINUX_H___


#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/device.h>

#include <asm/irq.h>

#define SDIO_CARD_INSERT_GPIO  11  /* card insertion switch */
//#define SDIO_CARD_INSERT_GPIO  29
#define SDIO_CARD_WP_GPIO      22  /* write protect switch position */
#define CARD_INSERT_POLARITY   FALSE
#define WP_POLARITY            TRUE

/* interrupt assignments */
#define SDIO_PXA_CONTROLLER_IRQ  IRQ_MMC  
#define SDIO_PXA_CARD_INSERT_IRQ IRQ_GPIO(SDIO_CARD_INSERT_GPIO) 

/* debounce delay for slot */
#define PXA_SLOT_DEBOUNCE_MS  2000

/* major number, zero indicates assign dynamically */
#define SDIO_BD_MAJOR_INIT 0

/* device base name */
#define SDIO_BD_BASE "sdiobd"

/* mapped memory address */
typedef struct _SDHCD_MEMORY {
    ULONG Raw;      /* start of address range */
    ULONG Length;   /* length of range */
    PVOID pMapped;  /* the mapped address */
}SDHCD_MEMORY, *PSDHCD_MEMORY;

typedef void (*PDMA_TRANSFER_COMPLETION)(PVOID,SDIO_STATUS,BOOL);

#define SDHCD_MAX_DEVICE_NAME 12

#define IS_SDHCD_SHUTTING_DOWN(pC) ((pC)->Device.ShutDown)

/* device data*/
typedef struct _SDHCD_DEVICE {
    OS_PNPDEVICE   HcdDevice;     /* the OS device for this HCD */
    OS_PNPDRIVER   HcdDriver;     /* the OS driver for this HCD */ 
    SDDMA_DESCRIPTION Dma;        /* driver DMA description */
    POS_PNPDEVICE pBusDevice;      /* our device registered with bus driver */
    UINT    MMCInterrupt;          /* controller interrupt */
    UINT    CardInsertInterrupt;   /* card insert interrupt */
    UINT    CardRemoveInterrupt;   /* card remove interrupt */
    UINT    SDIOInterrupt;         /* sdio interrupt interrupt */
    UINT8   InitStateMask;
    BOOL    ShutDown;
#define MMC_INTERRUPT_INIT         0x01
#define CARD_DETECT_INSERT_INTERRUPT_INIT 0x02
#define CARD_DETECT_REMOVE_INTERRUPT_INIT 0x04
#define SDHC_REGISTERED            0x10
#define SDHC_HW_INIT               0x20
#define TIMER_INIT                 0x40
    SDHCD_MEMORY ControlRegs;    /* memory addresses of device */
    SDHCD_MEMORY GpioRegs;       /* memory addresses of GPIO regs */
    spinlock_t   Lock;           /* lock against the ISR */
    BOOL         StartUpCheck;
    PDMA_TRANSFER_COMPLETION pDmaCompletion;
    PVOID        pContext;
    PUINT32      pDmaDescriptorBuffer;  /* buffer for descriptors */
    DMA_ADDRESS  DmaDescriptorPhys;     /* physical address for descriptor buffer */       
    PUINT8       pDmaCommonBuffer;      /* common buffer for DMA */
    DMA_ADDRESS  DmaCommonBufferPhys;   /* physical address for common buffer */
    INT          DmaChannel;
    BOOL         DmaSgMapped;
    INT          LastRxCopy;
}SDHCD_DEVICE, *PSDHCD_DEVICE;

#define GET_MMC_BASE(pC)(pC)->Device.ControlRegs.pMapped 
#define READ_GPIO_REG(pC, OFFSET) \
    _READ_DWORD_REG(((UINT32)(pC)->Device.GpioRegs.pMapped) + (OFFSET))
#define WRITE_GPIO_REG(pC, OFFSET,VALUE) \
    _WRITE_DWORD_REG(((UINT32)(pC)->Device.GpioRegs.pMapped) + (OFFSET),(VALUE))
#define READ_MMC_REG(pC, OFFSET)  \
    _READ_DWORD_REG(((UINT32)(pC)->Device.ControlRegs.pMapped) + (OFFSET))
#define WRITE_MMC_REG(pC, OFFSET, VALUE) \
    _WRITE_DWORD_REG(((UINT32)(pC)->Device.ControlRegs.pMapped) + (OFFSET),(VALUE))

//#define ENABLE_TEST_PIN 1

#ifdef ENABLE_TEST_PIN /* used for scope capture */  
#define TEST_PIN_GPIO GPIO59_LDD_1
#define TEST_PIN_GPSR GPIO_GPSR1
#define TEST_PIN_GPCR GPIO_GPCR1
#define SET_TEST_PIN(pHct) {                                      \
    if (IS_HCD_BUS_MODE_SPI(&(pHct)->Hcd)) {                      \
        WRITE_GPIO_REG((pHct),TEST_PIN_GPSR,GPIO_bit(TEST_PIN_GPIO)); \
    }                                                             \
} 
 
#define CLEAR_TEST_PIN(pHct) {                                    \
    if (IS_HCD_BUS_MODE_SPI(&(pHct)->Hcd)) {                      \
        WRITE_GPIO_REG((pHct),TEST_PIN_GPCR,GPIO_bit(TEST_PIN_GPIO)); \
    }                                                             \
}
#else
#define SET_TEST_PIN(pHct)
#define CLEAR_TEST_PIN(pHct)
#endif
/* prototypes */
#endif /* __SDIO_HCD_LINUX_H___ */
