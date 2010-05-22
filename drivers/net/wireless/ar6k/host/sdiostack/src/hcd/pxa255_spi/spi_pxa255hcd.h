// Copyright (c) 2004 Atheros Communications Inc.
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
@file: spi_pxa255hcd.h

@abstract: include file for PX255 local bus host controller (SPI-only)
           OS independent  code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SPI_PXA255HCD_H___
#define __SPI_PXA255HCD_H___

#include "../../include/ctsystem.h"

#include "../../include/sdio_busdriver.h"
#include "../../include/sdio_lib.h"

#if defined(LINUX) || defined(__linux__)
#include "linux/spi_hcd_linux.h"
#endif /* LINUX */

enum PXA_TRACE_ENUM {
    PXA_TRACE_CARD_INSERT = 10,
    PXA_TRACE_SDIO_INT = 11,
    PXA_TRACE_DATA,       
    PXA_TRACE_REQUESTS,   
    PXA_TRACE_CONFIG,     
    PXA_TRACE_SPI_INT,    
    PXA_TRACE_LAST
};

#define SDIO_IRQ_POLARITY      FALSE

#define PXA_SPI_CONTROLLER_BASE_ADDRESS    0x41000000
#define PXA_SPI_CONTROLLER_ADDRESS_LENGTH  0x10
#define PXA_GPIO_PIN_LVL_REGS_BASE         0x40e00000
#define PXA_GPIO_PIN_LVL_REGS_LENGTH       0x72
#define SDIO_BD_MAX_SLOTS                  1
#define SPI_PXA_MAX_BLOCKS                 512    
#define SPI_PXA_MAX_BYTES_PER_BLOCK        2048
#define SDMMC_RESP_TIMEOUT_CLOCKS          64
#define SDMMC_DATA_TIMEOUT_CLOCKS          0xFFFF

#define SPI_MAX_RXFIFO  32
#define SPI_MAX_TXFIFO  32

/* SSP register definitions */
#define SSP_SSCR0_REG           0x00
#define SSCR0_DSS_8BIT          0x07
#define SSCR0_FRAME_FORMAT_SPI  (0 << 4)
#define SSCR0_ON_CHIP_CLOCK     (0 << 6)
#define SSCR0_ENABLE            (1 << 7)
#define SSCR0_CLOCK_DIVISOR_SHIFT 8
#define SSP_SSCR0_SDIO_SPI_MODE (SSCR0_DSS_8BIT | SSCR0_FRAME_FORMAT_SPI | \
                                 SSCR0_ON_CHIP_CLOCK | SSCR0_ENABLE)

#define SSP_SSCR1_REG           0x04
#define SSCR1_RCV_FIFO_INT_ENABLE        (1 << 0)
#define SSCR1_TX_FIFO_INT_ENABLE         (1 << 1)
#define SSCR1_SPI_CLK_IDLE_HIGH          (1 << 3)
#define SSCR1_SPI_CLK_INACTIVE_1_CLK_EOF (1 << 4)
#define SSCR_TX_FIFO_THRESHOLD_SHIFT      6
#define SSCR_RX_FIFO_THRESHOLD_SHIFT      10
#define DEFAULT_TX_FIFO_THRESHOLD 8
#define DEFAULT_RX_FIFO_THRESHOLD 8
#define SSP_SSCR1_SDIO_SPI_MODE    (DEFAULT_TX_FIFO_THRESHOLD <<    \
                                    SSCR_TX_FIFO_THRESHOLD_SHIFT) | \
                                    (DEFAULT_RX_FIFO_THRESHOLD <<   \
                                    SSCR_RX_FIFO_THRESHOLD_SHIFT)

#define SSP_SSSR_REG           0x08


#define SSP_SSDR_REG           0x10
#define SSDR_TX_FIFO_NOT_FULL  (1 << 2)
#define SSDR_RX_FIFO_NOT_EMPTY (1 << 3)
#define SSDR_BUSY              (1 << 4)
#define SSDR_TX_FIFO_BELOW_THRESH  (1 << 5)
#define SSDR_RX_FIFO_ABOVE_THRESH  (1 << 6)
#define SSDR_RX_FIFO_OVERRUN       (1 << 7)
#define SSDR_GET_TX_FIFO_LEVEL(r)  ((r) >> 8) & 0x0F
#define SSDR_GET_RX_FIFO_LEVEL(r)  ((r) >> 12) & 0x0F

/* GPIO register definitions */
#define GPIO_GPLR0              0x00
#define GPIO_GPLR1              0x04
#define GPIO_GPLR2              0x08
#define GPIO_GPDR0              0x0C
#define GPIO_GPDR1              0x10
#define GPIO_GPDR2              0x14

#define GPIO_GPSR0              0x18
#define GPIO_GPSR1              0x1C
#define GPIO_GPSR2              0x20
#define GPIO_GPCR0              0x24
#define GPIO_GPCR1              0x28
#define GPIO_GPCR2              0x2C
#define GPIO_GRER0              0x30
#define GPIO_GRER1              0x34
#define GPIO_GFER0              0x3c
#define GPIO_GFER1              0x40

#define SPI_MAX_CLOCK_ENTRIES 1

typedef struct _SPI_CLOCK_TBL_ENTRY {
    SD_BUSCLOCK_RATE  ClockRate;  /* rate in Khz */
    UINT32            Divisor;    /* divisor control value*/
}SPI_CLOCK_TBL_ENTRY;

/* driver wide data, this driver only supports one device, 
 * so we include the per device data here also */
typedef struct _SDHCD_DRIVER_CONTEXT {
    PTEXT        pDescription;       /* human readable device decsription */
    SDHCD        Hcd;                /* HCD description for bus driver */
    SDHCD_DEVICE Device;             /* the single device's info */
    BOOL         CardInserted;       /* card inserted flag */
    BOOL         Cancel;
}SDHCD_DRIVER_CONTEXT, *PSDHCD_DRIVER_CONTEXT;


/* prototypes */
SDIO_STATUS HcdRequest(PSDHCD pHcd);
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pReq);
SDIO_STATUS HcdInitialize(PSDHCD_DRIVER_CONTEXT pHcdContext); 
void HcdDeinitialize(PSDHCD_DRIVER_CONTEXT pHcdContext);
BOOL HcdSPIInterrupt(PSDHCD_DRIVER_CONTEXT pHcdContext);
SDIO_STATUS QueueEventResponse(PSDHCD_DRIVER_CONTEXT pHcdContext, INT WorkItemID);
BOOL GetGpioPinLevel(PSDHCD_DRIVER_CONTEXT pHcdContext, INT Pin);

SDIO_STATUS EnableDisableSDIOIrq(PSDHCD_DRIVER_CONTEXT pHcdContext, BOOL Enable);
SDIO_STATUS AckSDIOIrq(PSDHCD_DRIVER_CONTEXT pHcdContext);
void ModifyCSForSPIIntDetection(PSDHCD_DRIVER_CONTEXT pHcdContext, BOOL Enable);

#define WORK_ITEM_IO_COMPLETE  0
#define WORK_ITEM_CARD_DETECT  1
#define WORK_ITEM_SDIO_IRQ     2

#endif /* __SPI_PXA255HCD_H___ */
