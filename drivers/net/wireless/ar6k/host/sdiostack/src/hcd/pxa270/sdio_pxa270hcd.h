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
@file: sdio_pxa270hcd.h

@abstract: include file for PXA270local bus host controller, OS independent  code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_PXA270HCD_H___
#define __SDIO_PXA270HCD_H___

#include <ctsystem.h>
#include <sdio_busdriver.h>
#include <sdio_lib.h>

#if defined(LINUX) || defined(__linux__)
#include "linux/sdio_hcd_linux.h"
#endif /* LINUX */

#ifdef UNDER_CE
#include "wince/sdio_hcd_wince.h"
#endif /* LINUX */

enum PXA_TRACE_ENUM {
    PXA_TRACE_CARD_INSERT = (SDDBG_TRACE + 1),
    PXA_TRACE_SDIO_INT = (SDDBG_TRACE + 2), 
    PXA_TRACE_DATA,       
    PXA_TRACE_REQUESTS, 
    PXA_TRACE_DATA_DUMP,   
    PXA_TRACE_CONFIG,     
    PXA_TRACE_MMC_INT,    
    PXA_TRACE_LAST
};

typedef enum _PXA_DMA_TYPE {
    PXA_DMA_NONE = 0,    
    PXA_DMA_COMMON_BUFFER = 1,
    PXA_DMA_SCATTER_GATHER
}PXA_DMA_TYPE, *PPXA_DMA_TYPE;


//default device location
#define PXA_MMC_CONTROLLER_BASE_ADDRESS    0x41100000
#define PXA_MMC_CONTROLLER_ADDRESS_LENGTH  0x44
#define PXA_GPIO_PIN_LVL_REGS_BASE         0x40e00000
#define PXA_GPIO_PIN_LVL_REGS_LENGTH       0x14c
#define SDIO_BD_MAX_SLOTS                  1
#define SDIO_PXA_MAX_BYTES_PER_BLOCK       1023
#define SDIO_PXA_MAX_BLOCKS                0xFFFF
#define SPI_PXA_MAX_BLOCKS                 1   /* SPI mode only supports single block */
#define SPI_PXA_MAX_BYTES_PER_BLOCK        1023
#define SDMMC_RESP_TIMEOUT_CLOCKS          64
#define SDMMC_DATA_TIMEOUT_CLOCKS          0xFFFF

#define PXA_DMA_THRESHOLD 32
#define MMC_MAX_RXFIFO  32
#define MMC_MAX_TXFIFO  32
#define PXA_TX_PARTIAL_FIFO_MASK (MMC_MAX_TXFIFO - 1)
/* register definitions */
#define GPIO_GPLR0              0x00
#define GPIO_GPLR1              0x04
#define GPIO_GPLR2              0x08
#define GPIO_GPLR3              0x100
#define GPIO_GPDR0              0x0C
#define GPIO_GPDR1              0x10
#define GPIO_GPDR2              0x14
#define GPIO_GPDR3              0x10C

#define GPIO_GPSR0              0x18
#define GPIO_GPSR1              0x1C
#define GPIO_GPSR2              0x20
#define GPIO_GPSR3              0x118
#define GPIO_GPCR0              0x24
#define GPIO_GPCR1              0x28
#define GPIO_GPCR2              0x2C
#define GPIO_GPCR3              0x124

#define GPIO_GRER0              0x30
#define GPIO_GRER1              0x34
#define GPIO_GFER0              0x3c
#define GPIO_GFER1              0x40

#define GPIO_GAFR0_L            0x54
#define GPIO_GAFR0_U            0x58
#define GPIO_GAFR1_L            0x5C
#define GPIO_GAFR1_U            0x60
#define GPIO_GAFR2_L            0x64
#define GPIO_GAFR2_U            0x68
#define GPIO_GAFR3_L            0x6C
#define GPIO_GAFR3_U            0x70

/* clock control */
#define MMC_STRPCL_REG          0x00
#define MMC_CLOCK_START         0x02
#define MMC_CLOCK_STOP          0x01
/* mmc status */
#define MMC_STAT_REG            0x04
#define MMC_STAT_DATA_DONE      (1 << 11)
#define MMC_STAT_PRG_DONE       (1 << 12)
#define MMC_STAT_END_CMD        (1 << 13)
#define MMC_STAT_CLK_ON         (1 << 8)
#define MMC_STAT_RCV_FULL       (1 << 7)
#define MMC_STAT_XMIT_EMPTY     (1 << 6)
#define MMC_STAT_RESP_CRC_ERR   (1 << 5)
#define MMC_STAT_SPI_RDTKN_ERR  (1 << 4)
#define MMC_STAT_RDDAT_CRC_ERR  (1 << 3)
#define MMC_STAT_WR_ERROR       (1 << 2)
#define MMC_STAT_RESP_TIMEOUT   (1 << 1)
#define MMC_STAT_READ_TIMEOUT   (1 << 0)
#define MMC_STAT_ERRORS         0x0000003F
#define MMC_RESP_ERRORS         (MMC_STAT_RESP_CRC_ERR | MMC_STAT_RESP_TIMEOUT)
#define MMC_STAT_RD_ERRORS      (MMC_STAT_RDDAT_CRC_ERR | MMC_STAT_READ_TIMEOUT)
/* clock rate */
#define MMC_CLKRT_REG           0x08
/* SPI control */
#define MMC_SPI_REG             0x0c
#define MMC_SPI_SEL_CS0         (1 << 3)      
#define MMC_SPI_CS_ENABLE       (1 << 2)
#define MMC_SPI_CRC_ENABLE      (1 << 1)
#define MMC_SPI_ENABLE          (1 << 0)
#define SPI_ENABLE_WITH_CRC  (MMC_SPI_CS_ENABLE | MMC_SPI_ENABLE | \
                              MMC_SPI_CRC_ENABLE | MMC_SPI_SEL_CS0)
#define SPI_ENABLE_NO_CRC  (MMC_SPI_CS_ENABLE | MMC_SPI_ENABLE | \
                            MMC_SPI_SEL_CS0)
/* command/data control */
#define MMC_CMDAT_REG           0x10
#define MMC_CMDAT_SDIO_IRQ_DETECT  (1 << 11)
#define MMC_CMDAT_SD_4DAT       (1 << 8)
#define MMC_CMDAT_DMA_ENABLE    (1 << 7)
#define MMC_CMDAT_80_CLOCKS     (1 << 6)
#define MMC_CMDAT_RES_BUSY      (1 << 5)
#define MMC_CMDDAT_STREAM       (1 << 4)  
#define MMC_CMDDAT_DATA_WR      (1 << 3)
#define MMC_CMDDAT_DATA_EN      (1 << 2)
#define MMC_CMDDAT_RES_NONE     0x00
#define MMC_CMDDAT_RES_R1_R4_R5 0x01
#define MMC_CMDDAT_RES_R2       0x02
#define MMC_CMDDAT_RES_R3       0x03
/* response timeout control */
#define MMC_RESTO_REG           0x14
#define MMC_RESTO_MASK          0x0000007F
/* read data timeout */
#define MMC_RDTO_REG            0x18 
#define MMC_RDTO_MASK           0x0000FFFF   
/* block length */
#define MMC_BLKLEN_REG          0x1c
#define MMC_BLKLEN_MASK         0x003FF     
/* number of blocks */      
#define MMC_NOB_REG_REG         0x20
#define MMC_NOB_MASK            0x0000FFFF       
/* partial buffer */
#define MMC_PRTBUF_REG          0x24
#define MMC_PRTBUF_PARTIAL      (1 << 0)
/* interrupt mask register */
#define MMC_I_MASK_REG          0x28

#define MMC_MASK_SUSP_ACK       (1 << 12)
#define MMC_MASK_SDIO_IRQ       (1 << 11)
#define MMC_MASK_RD_STALLED     (1 << 10)
#define MMC_MASK_RES_ERR        (1 << 9)
#define MMC_MASK_DAT_ERR        (1 << 8)
#define MMC_MASK_TINT           (1 << 7)

#define MMC_MASK_TXFIFO_WR      (1 << 6)
#define MMC_MASK_RXFIFO_RD      (1 << 5)
#define MMC_MASK_CLK_OFF        (1 << 4)
#define MMC_MASK_STOP_CMD       (1 << 3)
#define MMC_MASK_END_CMD        (1 << 2)
#define MMC_MASK_PRG_DONE       (1 << 1)
#define MMC_MASK_DATA_TRANS     (1 << 0)
#define MMC_MASK_ALL_INTS       0x00001FFF
/* interrupt pending */
#define MMC_I_REG_REG          0x2c
#define MMC_INT_SDIO_IRQ       (1 << 11)
#define MMC_INT_TXFIFO_WR      (1 << 6)
#define MMC_INT_RXFIFO_RD      (1 << 5)
#define MMC_INT_CLK_OFF        (1 << 4)
#define MMC_INT_STOP_CMD       (1 << 3)
#define MMC_INT_END_CMD        (1 << 2)
#define MMC_INT_PRG_DONE       (1 << 1)
#define MMC_INT_DATA_TRANS     (1 << 0)
/* command register */
#define MMC_CMD_REG            0x30
/* argument high */
#define MMC_ARGH_REG           0x34
/* argument low */
#define MMC_ARGL_REG           0x38
/* response fifo */
#define MMC_RES_REG            0x3c
#define SD_DEFAULT_RESPONSE_BYTES 6
#define SD_R2_RESPONSE_BYTES      16
/* RX Fifo */
#define MMC_RXFIFO_REG         0x40
/* TX Fifo */
#define MMC_TXFIFO_REG         0x44

#define MMC_MAX_CLOCK_ENTRIES 7

typedef struct _MMC_CLOCK_TBL_ENTRY {
    SD_BUSCLOCK_RATE  ClockRate;  /* rate in */
    UINT8             Divisor;
}MMC_CLOCK_TBL_ENTRY;

/* driver wide data, this driver only supports one device, 
 * so we include the per device data here also */
typedef struct _SDHCD_DRIVER_CONTEXT {
    PTEXT        pDescription;       /* human readable device decsription */
    SDHCD        Hcd;                /* HCD description for bus driver */
    SDHCD_DEVICE Device;             /* the single device's info OS-Specific */
    BOOL         CardInserted;       /* card inserted flag */
    BOOL         Cancel;
    BOOL         KeepClockOn; 
    BOOL         SD4Bit;             /* 4 bit bus mode */
    BOOL         SDIrqData;          /* irq check during transfers */
    PXA_DMA_TYPE DmaType;
    BOOL         DmaCapable;         /* DMA Capable */
    BOOL         PartialTxFIFO;      /* partial TX fifo flag for DMA completion */
    BOOL         IssueInitClocks;
}SDHCD_DRIVER_CONTEXT, *PSDHCD_DRIVER_CONTEXT;


/* prototypes */
SDIO_STATUS HcdRequest(PSDHCD pHcd);
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pReq);
SDIO_STATUS HcdInitialize(PSDHCD_DRIVER_CONTEXT pHcdContext); 
void HcdDeinitialize(PSDHCD_DRIVER_CONTEXT pHcdContext);
BOOL HcdMMCInterrupt(PSDHCD_DRIVER_CONTEXT pHcdContext);
SDIO_STATUS QueueEventResponse(PSDHCD_DRIVER_CONTEXT pHcdContext, INT WorkItemID);
BOOL GetGpioPinLevel(PSDHCD_DRIVER_CONTEXT pHcdContext, INT Pin); 
void ModifyCSForSPIIntDetection(PSDHCD_DRIVER_CONTEXT pHcdContext, BOOL Enable);
void UnmaskMMCIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr);
void MaskMMCIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr);
void SlotPowerOnOff(PSDHCD_DRIVER_CONTEXT pHct , BOOL On);
BOOL IsSlotWPSet(PSDHCD_DRIVER_CONTEXT pHct);
SDIO_STATUS SetUpPXADMA(PSDHCD_DRIVER_CONTEXT pHct, 
                        PSDREQUEST pReq, 
                        PDMA_TRANSFER_COMPLETION pCompletion,
                        PVOID                    pContext);
void CompleteRequestSyncDMA(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq, BOOL FromIsr);
void SDCancelDMATransfer(PSDHCD_DRIVER_CONTEXT pHct);

#define WORK_ITEM_IO_COMPLETE  0
#define WORK_ITEM_CARD_DETECT  1
#define WORK_ITEM_SDIO_IRQ     2

#define HCD_COMMAND_MIN_POLLING_CLOCK 5000000

#endif /* __SDIO_PXA255HCD_H___ */
