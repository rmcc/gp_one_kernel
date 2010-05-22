/* 
 * @file: hif_internal.h
 * 
 * @abstract: spi mode HIF layer definitions
 * 
 * 
 * @notice: Copyright (c) 2004-2006 Atheros Communications Inc.
 * 
 * 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
 * 
 */

#include "a_config.h"
#include "ctsystem.h"
#include "sdio_busdriver.h"
#include "sdio_lib.h"
#include "ath_spi_hcd_if.h"
#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#include "hif.h"
#include "hw/mbox_host_reg.h"

#define HIF_MBOX_BLOCK_SIZE                4  /* pad bytes to a full WORD on SPI */
#define HIF_MBOX_BASE_ADDR                 0x800
#define HIF_MBOX_WIDTH                     0x800
#define HIF_MBOX0_BLOCK_SIZE               HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX1_BLOCK_SIZE               HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX2_BLOCK_SIZE               HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX3_BLOCK_SIZE               HIF_MBOX_BLOCK_SIZE

#define SPI_CLOCK_FREQUENCY_DEFAULT        24000000
#define SPI_CLOCK_FREQUENCY_REDUCED        12000000

#define HIF_MBOX_START_ADDR(mbox)                        \
    HIF_MBOX_BASE_ADDR + mbox * HIF_MBOX_WIDTH

#define HIF_MBOX_END_ADDR(mbox)	                         \
    HIF_MBOX_START_ADDR(mbox) + HIF_MBOX_WIDTH - 1

#define BUS_REQUEST_MAX_NUM_FORDATA                32
#define BUS_REQUEST_MAX_NUM_TOTAL                  40


typedef struct target_function_context {
    SDFUNCTION           function; /* function description of the bus driver */
    OS_SEMAPHORE         instanceSem; /* instance lock. Unused */
    SDLIST               instanceList; /* list of instances. Unused */
} TARGET_FUNCTION_CONTEXT;

typedef struct bus_request {
    struct bus_request *next;
    SDREQUEST          *request;
    struct hif_device  *device;
    void               *context;
} BUS_REQUEST;

struct hif_device {
    SDDEVICE    *handle;
    void        *htc_handle;
    BUS_REQUEST *busrequestfreelist;
    BUS_REQUEST  busrequestblob[BUS_REQUEST_MAX_NUM_TOTAL];
    A_BOOL       shutdownInProgress;
    OS_CRITICALSECTION lock;
    A_UINT16     curBlockSize;
    A_UINT16     enabledSpiInts;
};


void 
hifRWCompletionHandler(SDREQUEST *request);

void
hifIRQHandler(void *context);

BOOL
hifDeviceInserted(SDFUNCTION *function, SDDEVICE *device);

A_STATUS
hifConfigureSPI(HIF_DEVICE *device);

void
hifDeviceRemoved(SDFUNCTION *function, SDDEVICE *device);


BUS_REQUEST *
hifAllocateBusRequest(HIF_DEVICE *device);

void
hifFreeBusRequest(HIF_DEVICE *device, BUS_REQUEST *request);


void HIFSpiDumpRegs(HIF_DEVICE *device);
                            
#define SPIReadInternal(d,a,p) SPIReadWriteInternal((d),(a),(p),TRUE) 
#define SPIWriteInternal(d,a,p) SPIReadWriteInternal((d),(a),(p),FALSE)      

                         
