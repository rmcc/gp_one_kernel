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
@file: sdio_omapmcbsp_hcd.h

@abstract: include file for MPC5200 SPI host controller, OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_MPC5200SPI_HCD_H___
#define __SDIO_MPC5200SPI_HCD_H___

#include "../../include/ctsystem.h"

#include "../../include/sdio_busdriver.h"
#include "../../include/sdio_lib.h"
#include "../../include/_sdio_defs.h"

#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/sdio_hcd_vxworks.h"
#endif /* VXWORKS */

/* QNX Neutrino support */
#ifdef QNX
#include "nto/sdio_hcd_nto.h"
#endif /* QNX */

#if defined(LINUX) || defined (__linux__)
#include "linux/sdio_hcd_linux.h"
#endif /* LINUX */

#ifdef INTEGRITY
#include "integrity/sdio_hcd_integrity.h"
#endif /* INTEGRITY */


enum PXA_TRACE_ENUM {
    PXA_TRACE_CARD_INSERT = (SDDBG_TRACE + 1),
    PXA_TRACE_SDIO_INT = (SDDBG_TRACE + 2),
    PXA_TRACE_DATA,       
    PXA_TRACE_REQUESTS,   
    PXA_TRACE_CONFIG,     
    PXA_TRACE_MMC_INT,    
    PXA_TRACE_CLOCK,
    PXA_TRACE_LAST
};


/* driver wide data, this driver only supports one device, 
 * so we include the per device data here also */
typedef struct _SDHCD_DRIVER_CONTEXT {
    PTEXT        pDescription;       /* human readable device decsription */
    SDLIST       DeviceList;         /* the list of current devices handled by this driver */
    OS_SEMAPHORE DeviceListSem;      /* protection for the DeviceList */
    UINT         DeviceCount;        /* number of devices currently installed */   
    SDHCD_DRIVER Driver;             /* OS dependent driver specific info */  
}SDHCD_DRIVER_CONTEXT, *PSDHCD_DRIVER_CONTEXT;

#define MPC5200SPI_MAX_BYTES_PER_BLOCK 2048
#define MPC5200SPI_DEFAULT_SPI_MAX_BLOCKS  0x1  /* only single block trabnsfers supported in SPI currently */
#define MPC5200SPI_MAX_BUSY_TIMEOUT    0 /* use default */ 
#define MPC5200SPI_DEFAULT_VOLTAGE     (SLOT_POWER_3_0V | SLOT_POWER_3_3V)
#define MPC5200SPI_DEFAULT_CURRENT     1000 
#define MPC5200SPI_MAX_CLOCK_DIVIDE    253
#define MPC5200SPI_DEFAULT_MIN_CLOCK_DIVIDE 10

#define CLOCK_ON    TRUE
#define CLOCK_OFF   FALSE


/* prototypes */
SDIO_STATUS HcdRequest(PSDHCD pHcd);
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pReq);
SDIO_STATUS HcdInitialize(PSDHCD_DEVICE pDeviceContext);
void HcdDeinitialize(PSDHCD_DEVICE pDeviceContext);
SDIO_STATUS QueueEventResponse(PSDHCD_DEVICE pDeviceContext, INT WorkItemID);
SDIO_STATUS SDPutBuffer(PSDHCD_DEVICE pDevice, PUINT8 pData, DMA_ADDRESS DmaAddress, UINT Length);
SDIO_STATUS SDGetBuffer(PSDHCD_DEVICE pDevice, PUINT8 pData, DMA_ADDRESS DmaAddress, UINT Length);
SDIO_STATUS SDSetClock(PSDHCD_DEVICE pDevice, PSDCONFIG_BUS_MODE_DATA pMode);
SDIO_STATUS SDStartStopClock(PSDHCD_DEVICE pDevice, BOOL Start);
SDIO_STATUS SDTransferBuffers(PSDHCD_DEVICE pDevice, 
                              PUINT8 pData, DMA_ADDRESS DmaAddress,
                              UINT Length,
                              void (*pCompletion)(PVOID pContext), PVOID pContext);
SDIO_STATUS AckSDIOIrq(PSDHCD_DEVICE pDevice);
SDIO_STATUS EnableDisableSDIOIrq(PSDHCD_DEVICE pDevice, BOOL Enable);


#endif /* __SDIO_MPC5200SPI_HCD_H___ */
