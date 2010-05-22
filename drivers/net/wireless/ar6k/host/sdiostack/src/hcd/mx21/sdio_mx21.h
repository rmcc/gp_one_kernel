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
@file: sdio_mx21.h

@abstract: include file for Freescale MX21 SDIO bus host controller, OS independent  code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_MX21HCD_H___
#define __SDIO_MX21HCD_H___

#include <ctsystem.h>
#include <sdio_busdriver.h>
#include <sdio_lib.h>

#if defined(LINUX) || defined(__linux__)
#include "linux/sdio_mx21_linux.h"
#endif /* LINUX */

enum SDHC_TRACE_ENUM {
    SDHC_TRACE_CARD_INSERT = (SDDBG_TRACE + 1),
    SDHC_TRACE_SDIO_INT = (SDDBG_TRACE + 2), 
    SDHC_TRACE_DATA,
    SDHC_TRACE_DMA_DEBUG,
    SDHC_TRACE_REQUESTS,
    SDHC_TRACE_DATA_DUMP,   
    SDHC_TRACE_CONFIG,     
    SDHC_TRACE_HC_INT,    
    SDHC_TRACE_LAST
};

typedef enum _SDHC_DMA_TYPE {
    SDHC_DMA_NONE = 0,    
    SDHC_DMA_COMMON_BUFFER = 1,
    SDHC_DMA_SCATTER_GATHER
}SDHC_DMA_TYPE, *PSDHC_DMA_TYPE;

#define SDHC_MODULE_MAX_CLK      20000000
#define SDHC_CONTROLLER1_BASE_ADDRESS    0x10013000
#define SDHC_CONTROLLER2_BASE_ADDRESS    0x10014000
#define SDHC_CONTROLLER_ADDRESS_LENGTH   0x1000

#define SDIO_SDHC_MAX_BYTES_PER_BLOCK   2048 

#define SDIO_SDHC_MAX_BLOCKS            0xFFFF

#define HCD_MAX_CLOCK_ENTRIES 8
typedef struct _HCD_CLOCK_TBL_ENTRY {
    SD_BUSCLOCK_RATE  ClockRate;  /* rate */
    UINT16            RegisterValue;
}HCD_CLOCK_TBL_ENTRY;

/* register definitions */
#define SDHC_STR_STP_CLK_REG  0x00
#define SDHC_STR_STP_CLK_RESET      (1 << 3)
#define SDHC_STR_STP_CLK_ENABLE     (1 << 2)
#define SDHC_STR_STP_CLK_START      (1 << 1)
#define SDHC_STR_STP_CLK_STOP       (1 << 0)

#define SDHC_STATUS_REG       0x04
#define SDHC_STATUS_CARD_PRESENT   (1 << 15)
#define SDHC_STATUS_SDIO_INT       (1 << 14)
#define SDHC_STATUS_END_CMD        (1 << 13 )
#define SDHC_STATUS_WRITE_DONE     (1 << 12)
#define SDHC_STATUS_READ_DONE      (1 << 11)
#define SDHC_STATUS_WRITE_CRC_CODE_MASK (3 << 9)
#define SDHC_STATUS_CLK_RUN        (1 << 8)
#define SDHC_STATUS_FIFO_FULL      (1 << 7)
#define SDHC_STATUS_FIFO_EMPTY     (1 << 6)
#define SDHC_STATUS_RESP_CRC_ERROR (1 << 5)
#define SDHC_STATUS_READ_CRC_ERROR (1 << 3)
#define SDHC_STATUS_WRITE_CRC_ERROR (1 << 2)
#define SDHC_STATUS_RESP_TIMEOUT    (1 << 1)
#define SDHC_STATUS_READ_TIMEOUT    (1 << 0)

#define SDHC_STATUS_RESP_ERRORS  (SDHC_STATUS_RESP_CRC_ERROR | SDHC_STATUS_RESP_TIMEOUT)
#define SDHC_STATUS_RD_WR_ERRORS (SDHC_STATUS_READ_CRC_ERROR | SDHC_STATUS_READ_TIMEOUT |\
                                  SDHC_STATUS_WRITE_CRC_ERROR)

#define SDHC_CLK_RATE_REG            0x08
#define SDHC_CLK_RATE_PRESCALE_SHIFT 4
#define SDHC_CLK_RATE_PRESCALE_MASK  (0xFFF << SDHC_CLK_RATE_PRESCALE_SHIFT)

#define SDHC_CMD_DAT_REG             0x0C
#define SDHCD_CMD_DAT_BUS_1BIT       0x00
#define SDHCD_CMD_DAT_BUS_4BIT       (0x2 << 8)
#define SDHCD_CMD_DAT_INIT_CLKS      (1 << 7)
#define SDHCD_CMD_DAT_RESP_BUSY      (1 << 6) /* undocumented bit */
#define SDHCD_CMD_DAT_DATA_WRITE     (1 << 4)
#define SDHCD_CMD_DAT_DATA_ENABLE    (1 << 3)
#define SDHCD_CMD_DAT_RESP_NO_RESP   0x00
#define SDHCD_CMD_DAT_RESP_R1R5R6    0x01
#define SDHCD_CMD_DAT_RESP_R2        0x02
#define SDHCD_CMD_DAT_RESP_R3R4      0x03

#define SDHC_CMD_RES_TO_REG             0x10
#define SDMMC_RESP_TIMEOUT_CLOCKS       64

#define SDHC_CMD_READ_TO_REG            0x14
#define SDMMC_DATA_TIMEOUT_CLOCKS       0xFFFF

#define SDHC_BLK_LEN_REG                0x18
#define SDHC_NOB_REG                    0x1C

#define SDHC_REVISION_REG               0x20

#define SDHC_INT_MASK_REG               0x24
#define SDHC_INT_CARD_DETECT            (1 << 15)
#define SDHC_INT_SDIO_INT_WAKEUP        (1 << 14)
#define SDHC_INT_DAT0_ENABLE            (1 << 5)

#define SDHC_INT_SDIO_MASK              (1 << 4)
#define SDHC_INT_BUFF_RDY_MASK          (1 << 3)
#define SDHC_INT_END_CMD_MASK           (1 << 2)
#define SDHC_INT_WRITE_DONE_MASK        (1 << 1)
#define SDHC_INT_DATA_TRANS_DONE_MASK   (1 << 0)
#define SDHC_INT_MASK_ALL               0x1F

#define SDHC_CMD_REG                    0x28
#define SDHC_ARGH_REG                   0x2C
#define SDHC_ARGL_REG                   0x30
#define SDHC_RES_FIFO_REG               0x34
#define SDHC_BUF_ACCESS_REG             0x38

#define SDHC_MAX_FIFO_1BIT  16
#define SDHC_MAX_FIFO_4BIT  64

/* driver wide data, this driver only supports one device, 
 * so we include the per device data here also */
typedef struct _SDHCD_DRIVER_CONTEXT {
    PTEXT         pDescription;       /* human readable device decsription */
    SDHCD         Hcd;                /* HCD description for bus driver */
    SDHCD_DEVICE  Device;             /* the single device's info */
    BOOL          CardInserted;       /* card inserted flag */
    BOOL          KeepClockOn; 
    BOOL          SD4Bit;             /* 4 bit bus mode */
    BOOL          CmdProcessed;       /* command phase was processed */
    BOOL          IssueInitClocks;
    UINT32        FifoDepth;          /* FIFO depth for the bus mode */
    SDHC_DMA_TYPE DmaType;
    INT           ValidClockEntries;
    HCD_CLOCK_TBL_ENTRY ClockDivisorTable[HCD_MAX_CLOCK_ENTRIES];
    BOOL          SDIOIrqMasked;
    BOOL          SDIOIrqDetectArmed;
    BOOL          SDIOCardIrqDetectRequested;
    INT           BaseClkDivisorReg;
    SDCONFIG_BUS_MODE_DATA SavedBusMode; 
}SDHCD_DRIVER_CONTEXT, *PSDHCD_DRIVER_CONTEXT;


/* prototypes */
SDIO_STATUS HcdRequest(PSDHCD pHcd);
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pReq);
SDIO_STATUS HcdInitialize(PSDHCD_DRIVER_CONTEXT pHcdContext); 
void HcdDeinitialize(PSDHCD_DRIVER_CONTEXT pHcdContext);
BOOL HcdInterrupt(PSDHCD_DRIVER_CONTEXT pHcdContext);
SDIO_STATUS QueueEventResponse(PSDHCD_DRIVER_CONTEXT pHcdContext, INT WorkItemID);
BOOL GetGpioPinLevel(PSDHCD_DRIVER_CONTEXT pHcdContext, INT Pin); 
void ModifyCSForSPIIntDetection(PSDHCD_DRIVER_CONTEXT pHcdContext, BOOL Enable);
void UnmaskHcdIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr);
void MaskHcdIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr);
void SlotPowerOnOff(PSDHCD_DRIVER_CONTEXT pHct , BOOL On);
BOOL IsSlotWPSet(PSDHCD_DRIVER_CONTEXT pHct);
SDIO_STATUS SetUpHCDDMA(PSDHCD_DRIVER_CONTEXT pHct, 
                         PSDREQUEST pReq, 
                         PDMA_TRANSFER_COMPLETION pCompletion,
                         PVOID                    pContext);
SDIO_STATUS InitMX21(PSDHCD_DRIVER_CONTEXT pHct);
void DeinitMX21(PSDHCD_DRIVER_CONTEXT pHct);
void CompleteRequestSyncDMA(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq, BOOL FromIsr);
void DisableHcdInterrupt(PSDHCD_DRIVER_CONTEXT pHct, BOOL FromIsr);
void EnableHcdInterrupt(PSDHCD_DRIVER_CONTEXT pHct, BOOL FromIsr);
BOOL IsDMAAllowed(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq);
void DumpDmaInfo(PSDHCD_DRIVER_CONTEXT pHct);
#define WORK_ITEM_IO_COMPLETE  0
#define WORK_ITEM_CARD_DETECT  1
#define WORK_ITEM_SDIO_IRQ     2

#define HCD_COMMAND_MIN_POLLING_CLOCK 5000000

#endif 
