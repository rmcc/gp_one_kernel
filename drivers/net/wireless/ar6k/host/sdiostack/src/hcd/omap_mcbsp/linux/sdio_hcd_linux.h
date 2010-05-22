/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_hcd_linux.h

@abstract: include file for OMAP McBSP SPI host controller, linux dependent code
 
@notice: Copyright (c), 2004-2005 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_HCD_LINUX_H___
#define __SDIO_HCD_LINUX_H___


#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <asm/arch/mcbsp.h>
#include <asm/arch/dma.h>
#include <asm/hardware/clock.h>

#include "../../SpiLib/spilib.h"


/* device base name */
#define SDIO_BD_BASE "sdiobd"
#define SDHCD_MAX_DEVICE_NAME 12

#define OMAPMCBSP_DEFAULT_DMA_ID   1
#define OMAPMCBSP_COMMAND_RESPONSE_BUFFER_SIZE  (SPILIB_COMMAND_BUFFER_SIZE + SPILIB_LEADING_READ_BYTES+SPILIB_COMMAND_MAX_RESPONSE_SIZE)
#define OMAPMCBSP_DATA_BUFFER_SIZE  (OMAPMCBSP_MAX_BYTES_PER_BLOCK + SPILIB_LEADING_READ_BYTES + SPILIB_DATA_BUFFER_CMD_SPACE)
#define OMAPMCBSP_DMA_BUFFER_SIZE (OMAPMCBSP_DATA_BUFFER_SIZE + OMAPMCBSP_COMMAND_RESPONSE_BUFFER_SIZE)
//??#define OMAPMCBSP_BASE_CLOCK        60000000
#define OMAPMCBSP_BASE_CLOCK        30000000
#define OMAPMCBSP_INIT_CLOCK_BYTES  50
#define OMAPMCBSP_DMA_MASK          0xffffffff
#define OMAPMCBSP_DEFAULT_IRQ_PIN   9

#define OMAPMCBSP_DMA_TIMEOUT       50
#define OMAPMCBSP_CHIPSELECT_PIN    21

/* device data*/
typedef struct _SDHCD_DEVICE {
    struct device *pBusDevice;  /* our device registered with bus driver */
    SDLIST  List;               /* linked list */
    SDHCD   Hcd;                /* HCD description for bus driver */
    struct device_driver *pHcdPlatformDriver;   /* the OS driver for this HCD */ 
    UINT8   InitStateMask;
#define SDHC_REGISTERED            0x10
#define SDHC_HW_INIT               0x20
#define SDHC_DMA_INIT              0x40
#define SDHC_SDIRQ_INIT            0x80
    UINT32      BaseClock;          /* base clock in hz */ 
    UINT32      Clock;              /* current clock index */ 
    struct clk *pDmaClock;          /* dma clock active */
    BOOL        CardInserted;       /* card inserted flag */
    BOOL        ShuttingDown;       /* indicates shut down of HCD) */
    struct work_struct iocomplete_work; /* work item definintions */
    struct work_struct carddetect_work; /* work item definintions */
    struct work_struct sdioirq_work; /* work item definintions */
    struct work_struct dmairq_work; /* work item definintions */
    UINT        RemainingBytes;     /* remaining receive bytes in transfer */
    SDSPI_CONFIG Config;            /* SPI configuration data */
    void (*pTransferCompletion)(PVOID pContext); /* transfer completion routine */
    PVOID       TransferContext;    /* context passed to TransferCompletion routine */ 
    struct completion TransferComplete; /* completion for blocking transfer */
    PUINT8      pRxData;            /* received data ptr */
    UINT        RxLength;           /* length of pRxData */
    struct omap_mcbsp * pMcbspData; /* McBSP configuration */  
    short       TxDmaSave;          /* temp. channel number storage */      
    UINT32      Channel;            /* DMA channel for this device */
    DMA_ADDRESS hDma;               /* allocated DMA buffer */
    DMA_ADDRESS hDmaCommandBuffer;  /* handle for command buffer */
    DMA_ADDRESS hDmaDataBuffer;     /* handle for data buffer */
    PUINT8      pDmaCommandBuffer;  /* virtual address of command buffer */
    PUINT8      pDmaDataBuffer;     /* virtual address of data buffer */
    spinlock_t  IrqLock;            /* lock against the ISR */
    UINT        LastTransferLength; /* saved transfer length */
    struct timer_list DmaTimeOut;   /* timer to catch DMA timeouts */
    BOOL        SpiShift;           /* SPI data needs shifting */
    SDREQUEST   Cmd12Request;       /* request to handle auto CMD12's */
    UINT8       LastByte;           /* last byte processed */
    UINT        MinClockDiv;        /* minimum clock divisor to use */
//??temp
    int         DmaTimeoutCount;
    ULONG       LastDstAddr;
//??    
}SDHCD_DEVICE, *PSDHCD_DEVICE;

typedef struct _SDHCD_DRIVER {
    struct platform_device HcdPlatformDevice; /* the OS device for this HCD */
    struct device_driver HcdPlatformDriver;   /* the OS driver for this HCD */ 
}SDHCD_DRIVER, *PSDHCD_DRIVER;

#define OMAPMCBSP_GET_COMMAND_DMA_BUFFER(pDevice) pDevice->hDmaCommandBuffer
#define OMAPMCBSP_GET_DATA_DMA_BUFFER(pDevice) pDevice->hDmaDataBuffer
#define OMAPMCBSP_GET_COMMAND_BUFFER(pDevice) pDevice->pDmaCommandBuffer
#define OMAPMCBSP_GET_DATA_BUFFER(pDevice) pDevice->pDmaDataBuffer
#define OMAPMCBSP_GET_DATA_BUFFER_SIZE(pDevice) OMAPMCBSP_DATA_BUFFER_SIZE

#define WORK_ITEM_IO_COMPLETE  0
#define WORK_ITEM_CARD_DETECT  1
#define WORK_ITEM_SDIO_IRQ     2

/* prototypes */
#endif /* __SDIO_HCD_LINUX_H___ */
