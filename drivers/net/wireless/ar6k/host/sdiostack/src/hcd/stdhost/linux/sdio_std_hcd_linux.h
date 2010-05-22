/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_std_hcd_linux.h

@abstract: include file for linux dependent code
 
@notice: Copyright (c), 2005 Atheros Communications, Inc.


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
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_STD_HCD_LINUX_H___
#define __SDIO_STD_HCD_LINUX_H___

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <asm/irq.h>

#define SDHCD_MAX_DEVICE_NAME        64

/* Advance DMA parameters */
#define SDHCD_MAX_ADMA_DESCRIPTOR    32
#define SDHCD_ADMA_DESCRIPTOR_SIZE   (SDHCD_MAX_ADMA_DESCRIPTOR * sizeof(SDHCD_SGDMA_DESCRIPTOR))
#define SDHCD_MAX_ADMA_LENGTH        0x8000      /* up to 32KB per descriptor */    
#define SDHCD_ADMA_ADDRESS_MASK      0xFFFFE000  /* 4KB boundaries */
#define SDHCD_ADMA_ALIGNMENT         0xFFF       /* illegal alignment bits*/
#define SDHCD_ADMA_LENGTH_ALIGNMENT  0x0         /* any length up to the max */

/* simple DMA */
#define SDHCD_MAX_SDMA_DESCRIPTOR    1
#define SDHCD_MAX_SDMA_LENGTH        0x80000     /* up to 512KB for a single descriptor*/    
#define SDHCD_SDMA_ADDRESS_MASK      0xFFFFFFFF  /* any 32 bit address */
#define SDHCD_SDMA_ALIGNMENT         0x0         /* any 32 bit address */
#define SDHCD_SDMA_LENGTH_ALIGNMENT  0x0         /* any length up to the max */

#define HCD_COMMAND_MIN_POLLING_CLOCK 5000000

/* debounce delay for slot */
#define SD_SLOT_DEBOUNCE_MS  1000

/* mapped memory address */
typedef struct _SDHCD_MEMORY {
    ULONG Raw;      /* start of address range */
    ULONG Length;   /* length of range */
    PVOID pMapped;  /* the mapped address */
}SDHCD_MEMORY, *PSDHCD_MEMORY;

typedef struct _SDHCD_OS_SPECIFIC {
    SDHCD_MEMORY Address;               /* memory address of this device */ 
    spinlock_t   RegAccessLock;         /* use to protect registers when needed */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    struct work_struct iocomplete_work; /* work item definitions */
    struct work_struct carddetect_work; /* work item definintions */
    struct work_struct sdioirq_work;    /* work item definintions */
#else
    struct delayed_work iocomplete_work; /* work item definitions */ 
    struct delayed_work carddetect_work; /* work item definintions */
    struct delayed_work sdioirq_work;    /* work item definintions */
#endif
    spinlock_t   Lock;                  /* general purpose lock against the ISR */
    DMA_ADDRESS  hDmaBuffer;            /* handle for data buffer */
    PUINT8       pDmaBuffer;            /* virtual address of DMA command buffer */
    PSDDMA_DESCRIPTOR pDmaList;         /* in use scatter-gather list */
    UINT         SGcount;               /* count of in-use scatter gather list */
    UINT         SlotNumber;            /* the STD-host defined slot number assigned to this instance */
/* everything below this line is used by the implementation that uses this STD core */
    UINT16        InitMask;             /* implementation specific portion init mask */
    UINT32        ImpSpecific0;         /* implementation specific storage */           
    UINT32        ImpSpecific1;         /* implementation specific storage */ 
} SDHCD_OS_SPECIFIC, *PSDHCD_OS_SPECIFIC;


#define WORK_ITEM_IO_COMPLETE  0
#define WORK_ITEM_CARD_DETECT  1
#define WORK_ITEM_SDIO_IRQ     2
 
#define READ_HOST_REG32(pHcInstance, OFFSET)  \
    _READ_DWORD_REG((((UINT32)((pHcInstance)->pRegs))) + (OFFSET))
#define WRITE_HOST_REG32(pHcInstance, OFFSET, VALUE) \
    _WRITE_DWORD_REG((((UINT32)((pHcInstance)->pRegs))) + (OFFSET),(VALUE))
#define READ_HOST_REG16(pHcInstance, OFFSET)  \
    _READ_WORD_REG((((UINT32)((pHcInstance)->pRegs))) + (OFFSET))
#define WRITE_HOST_REG16(pHcInstance, OFFSET, VALUE) \
    _WRITE_WORD_REG((((UINT32)((pHcInstance)->pRegs))) + (OFFSET),(VALUE))
#define READ_HOST_REG8(pHcInstance, OFFSET)  \
    _READ_BYTE_REG((((UINT32)((pHcInstance)->pRegs))) + (OFFSET))
#define WRITE_HOST_REG8(pHcInstance, OFFSET, VALUE) \
    _WRITE_BYTE_REG((((UINT32)((pHcInstance)->pRegs))) + (OFFSET),(VALUE))

#define TRACE_SIGNAL_DATA_WRITE(pHcInstance, on) 
#define TRACE_SIGNAL_DATA_READ(pHcInstance, on) 
#define TRACE_SIGNAL_DATA_ISR(pHcInstance, on) 
#define TRACE_SIGNAL_DATA_IOCOMP(pHcInstance, on) 
#define TRACE_SIGNAL_DATA_TIMEOUT(pHcInstance, on) 



#define IS_HCD_ADMA(pHc) ((pHc)->Hcd.pDmaDescription != NULL) && \
                           ((pHc)->Hcd.pDmaDescription->Flags & SDDMA_DESCRIPTION_FLAG_SGDMA)

#define IS_HCD_SDMA(pHc) (((pHc)->Hcd.pDmaDescription != NULL) &&   \
                           (((pHc)->Hcd.pDmaDescription->Flags &   \
                             (SDDMA_DESCRIPTION_FLAG_SGDMA | SDDMA_DESCRIPTION_FLAG_DMA)) == \
                             SDDMA_DESCRIPTION_FLAG_DMA))

#endif 
