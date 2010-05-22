// Copyright (c) 2004, 2005 Atheros Communications Inc.
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

@abstract: include file for Tokyo Electron PCI Ellen host controller, linux dependent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_HCD_LINUX_H___
#define __SDIO_HCD_LINUX_H___


#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/device.h>


#include <asm/irq.h>


#define SDHCD_MAX_DEVICE_NAME 12

#define CARD_INSERT_POLARITY   FALSE
#define WP_POLARITY            TRUE
#define HCD_COMMAND_MIN_POLLING_CLOCK 5000000

/* debounce delay for slot */
#define SD_SLOT_DEBOUNCE_MS  1000

/* the config space slot number and start for SD host */
#define PCI_CONFIG_SLOT   0x40
#define GET_SLOT_COUNT(config)\
    ((((config)>>4)& 0x7) +1)
#define GET_SLOT_FIRST(config)\
    ((config) & 0x7)

/* device base name */
#define SDIO_BD_BASE "sdiobd"

/* mapped memory address */
typedef struct _SDHCD_MEMORY {
    ULONG Raw;      /* start of address range */
    ULONG Length;   /* length of range */
    PVOID pMapped;  /* the mapped address */
}SDHCD_MEMORY, *PSDHCD_MEMORY;

typedef enum _SDHCD_TYPE {
    TYPE_CLASS,     /* standard class device */
    TYPE_PCIELLEN,  /* Tokuo Electron PCI Ellen card */
}SDHCD_TYPE, *PSDHCD_TYPE;

/* device data*/
typedef struct _SDHCD_DEVICE {
    struct pci_dev *pBusDevice;    /* our device registered with bus driver */
    SDLIST  List;                  /* linked list */
    SDHCD   Hcd;                   /* HCD description for bus driver */
    char    DeviceName[SDHCD_MAX_DEVICE_NAME]; /* our chr device name */
    SDHCD_MEMORY Address;          /* memory address of this device */ 
    spinlock_t AddressSpinlock;    /* use to protect reghisters when needed */
    SDHCD_MEMORY ControlRegs;      /* memory address of shared control registers */ 
    SDHCD_TYPE Type;               /* type of this device */
    UINT8   InitStateMask;
#define SDIO_BAR_MAPPED            0x01
#define SDIO_LAST_CONTROL_BAR_MAPPED 0x02 /* set on device that will unmap the shared control registers */
#define SDIO_IRQ_INTERRUPT_INIT    0x04
#define SDHC_REGISTERED            0x10
#define SDHC_HW_INIT               0x40
#define TIMER_INIT                 0x80
    spinlock_t   Lock;            /* lock against the ISR */
    BOOL         CardInserted;    /* card inserted flag */
    BOOL         Cancel;
    BOOL         ShuttingDown;    /* indicates shut down of HCD) */
    BOOL         HighSpeed;       /* device supports high speed, 25-50 Mhz */
    UINT32       BaseClock;       /* base clock in hz */ 
    UINT32       TimeOut;         /* timeout setting */ 
    UINT32       ClockSpinLimit;  /* clock limit for command spin loops */
    BOOL         KeepClockOn;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    struct work_struct iocomplete_work; /* work item definintions */
    struct work_struct carddetect_work; /* work item definintions */
    struct work_struct sdioirq_work; /* work item definintions */
#else
    struct delayed_work iocomplete_work; /* work item definintions */
    struct delayed_work carddetect_work; /* work item definintions */
    struct delayed_work sdioirq_work; /* work item definintions */
#endif
}SDHCD_DEVICE, *PSDHCD_DEVICE;


#define WORK_ITEM_IO_COMPLETE  0
#define WORK_ITEM_CARD_DETECT  1
#define WORK_ITEM_SDIO_IRQ     2

 
#define READ_HOST_REG32(pDevice, OFFSET)  \
    _READ_DWORD_REG((((UINT32)((pDevice)->Address.pMapped))) + (OFFSET))
#define WRITE_HOST_REG32(pDevice, OFFSET, VALUE) \
    _WRITE_DWORD_REG((((UINT32)((pDevice)->Address.pMapped))) + (OFFSET),(VALUE))
#define READ_HOST_REG16(pDevice, OFFSET)  \
    _READ_WORD_REG((((UINT32)((pDevice)->Address.pMapped))) + (OFFSET))
#define WRITE_HOST_REG16(pDevice, OFFSET, VALUE) \
    _WRITE_WORD_REG((((UINT32)((pDevice)->Address.pMapped))) + (OFFSET),(VALUE))
#define READ_HOST_REG8(pDevice, OFFSET)  \
    _READ_BYTE_REG((((UINT32)((pDevice)->Address.pMapped))) + (OFFSET))
#define WRITE_HOST_REG8(pDevice, OFFSET, VALUE) \
    _WRITE_BYTE_REG((((UINT32)((pDevice)->Address.pMapped))) + (OFFSET),(VALUE))

#define READ_CONTROL_REG32(pDevice, OFFSET)  \
    _READ_DWORD_REG((((UINT32)((pDevice)->ControlRegs.pMapped))) + (OFFSET))
#define WRITE_CONTROL_REG32(pDevice, OFFSET, VALUE) \
    _WRITE_DWORD_REG((((UINT32)((pDevice)->ControlRegs.pMapped))) + (OFFSET),(VALUE))
#define READ_CONTROL_REG16(pDevice, OFFSET)  \
    _READ_WORD_REG((((UINT32)((pDevice)->ControlRegs.pMapped))) + (OFFSET))
#define WRITE_CONTROL_REG16(pDevice, OFFSET, VALUE) \
    _WRITE_WORD_REG((((UINT32)((pDevice)->ControlRegs.pMapped))) + (OFFSET),(VALUE))

/* PLX 9030 control registers */
#define INTCSR 0x4C
#define INTCSR_LINTi1ENABLE         (1 << 0)
#define INTCSR_LINTi1STATUS         (1 << 2)
#define INTCSR_LINTi2ENABLE         (1 << 3)
#define INTCSR_LINTi2STATUS         (1 << 5)
#define INTCSR_PCIINTENABLE         (1 << 6)

#define GPIOCTRL 0x54
#define GPIO8_PIN_DIRECTION     (1 << 25)
#define GPIO8_DATA_MASK         (1 << 26)
#define GPIO3_PIN_SELECT        (1 << 9)
#define GPIO3_PIN_DIRECTION     (1 << 10)
#define GPIO3_DATA_MASK         (1 << 11)
#define GPIO2_PIN_SELECT        (1 << 6)
#define GPIO2_PIN_DIRECTION     (1 << 7)
#define GPIO2_DATA_MASK         (1 << 8)
#define GPIO4_PIN_SELECT        (1 << 12)
#define GPIO4_PIN_DIRECTION     (1 << 13)
#define GPIO4_DATA_MASK         (1 << 14)

#define GPIO_CONTROL(pDevice, on,  GpioMask)   \
{                                   \
     UINT32 temp;                    \
     temp = READ_CONTROL_REG32((pDevice),GPIOCTRL);   \
     if (on) temp |= (GpioMask); else temp &= ~(GpioMask);   \
     WRITE_CONTROL_REG32((pDevice),GPIOCTRL, temp);   \
}

//??#define TRACE_SIGNAL_DATA_WRITE(pDevice, on) GPIO_CONTROL((pDevice),(on),GPIO8_DATA_MASK)
//??#define TRACE_SIGNAL_DATA_READ(pDevice, on) GPIO_CONTROL((pDevice),(on),GPIO2_DATA_MASK)
//??#define TRACE_SIGNAL_DATA_ISR(pDevice, on) GPIO_CONTROL((pDevice),(on),GPIO4_DATA_MASK)
//??#define TRACE_SIGNAL_DATA_IOCOMP(pDevice, on) GPIO_CONTROL((pDevice),(on),GPIO3_DATA_MASK)
#define TRACE_SIGNAL_DATA_WRITE(pDevice, on) 
#define TRACE_SIGNAL_DATA_READ(pDevice, on) 
#define TRACE_SIGNAL_DATA_ISR(pDevice, on) 
#define TRACE_SIGNAL_DATA_IOCOMP(pDevice, on) 
#define TRACE_SIGNAL_DATA_TIMEOUT(pDevice, on) GPIO_CONTROL((pDevice),(on),GPIO3_DATA_MASK)

/* prototypes */
#endif /* __SDIO_HCD_LINUX_H___ */
