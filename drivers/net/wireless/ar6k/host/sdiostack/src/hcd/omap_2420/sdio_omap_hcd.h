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
@file: sdio_pmap_hcd.h

@abstract: include file for OMAP native MMC/SD host controller, OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_OMAP_HCD_H___
#define __SDIO_OMAP_HCD_H___

#include <ctsystem.h>

#include <sdio_busdriver.h>
#include <_sdio_defs.h>
#include <sdio_lib.h>
#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/sdio_hcd_vxworks.h"
#endif /* VXWORKS */

/* QNX Neutrino suppot */
#ifdef QNX
#include "nto/sdio_hcd_nto.h"
#endif /* QNX */

#if defined(LINUX) || defined(__linux__)
#include "linux/sdio_hcd_linux.h"
#endif /* LINUX */

#ifdef NUCLEUS_PLUS
/* Mentor Graphics Nucleus support */
#include "nucleus/sdio_hcd_nucleus.h"
#endif /* NUCLEUS_PLUS */

enum OMAP_TRACE_ENUM {
    OMAP_TRACE_CARD_INSERT = (SDDBG_TRACE + 1),
    OMAP_TRACE_SDIO_INT = (SDDBG_TRACE + 2),
    OMAP_TRACE_WORK,     
    OMAP_TRACE_REQUESTS,
    OMAP_TRACE_DATA,  
    OMAP_TRACE_DMA_DUMP,   
    OMAP_TRACE_CONFIG,     
    OMAP_TRACE_MMC_INT,    
    OMAP_TRACE_CLOCK,
    OMAP_TRACE_LAST
};
 
#define OMAP_TRACE_BUSY  OMAP_TRACE_DATA

#define OMAP_MAX_BYTES_PER_BLOCK  2048 
#define OMAP_MAX_BLOCKS           2048
#define OMAP_DEFAULT_CURRENT      500
#define OMAP_DEFAULT_CMD_TIMEOUT  64
#define OMAP_DEFAULT_DATA_TIMEOUT 400000

#define OMAP_MMC_FIFO_SIZE           64
/* almost full, for RX */
#define OMAP_MMC_AFL_FIFO_THRESH     32 //62 
 /* almost empty, for TX */
#define OMAP_MMC_AEL_FIFO_THRESH     32 // 2 
#define OMAP_MAX_SHORT_TRANSFER_SIZE 16

#define OMAP_REG_MMC_CMD                    0x00
    #define OMAP_REG_MMC_CMD_DDIR_READ           (1<<15)
    #define OMAP_REG_MMC_CMD_DDIR_WRITE          (0)
    #define OMAP_REG_MMC_CMD_STREAM_MODE_NORMAL  (0)
    #define OMAP_REG_MMC_CMD_TYPE_BC             (0)
    #define OMAP_REG_MMC_CMD_TYPE_BCR            (1<<12)
    #define OMAP_REG_MMC_CMD_TYPE_AC             (2<<12)
    #define OMAP_REG_MMC_CMD_TYPE_ADTC           (3<<12)
    #define OMAP_REG_MMC_CMD_R1BUSY              (1<<11)
    #define OMAP_REG_MMC_CMD_NORESPONSE          (0)
    #define OMAP_REG_MMC_CMD_R1                  (1<<8)
    #define OMAP_REG_MMC_CMD_R2                  (2<<8)
    #define OMAP_REG_MMC_CMD_R3                  (3<<8)
    #define OMAP_REG_MMC_CMD_R4                  (4<<8)
    #define OMAP_REG_MMC_CMD_R5                  (5<<8)
    #define OMAP_REG_MMC_CMD_R6                  (6<<8)
    #define OMAP_REG_MMC_CMD_INAB                (1<<7)
    #define OMAP_REG_MMC_CMD_CTO_DTO             (1<<6)
    #define OMAP_REG_MMC_CMD_MASK                (0x3F)
    
    
#define OMAP_REG_MMC_ARG_LOW                0x04
#define OMAP_REG_MMC_ARG_HI                 0x08

#define OMAP_REG_MMC_MODULE_CONFIG          0x0C
    #define OMAP_REG_MMC_MODULE_CONFIG_4BIT      (1<<15)
    #define OMAP_REG_MMC_MODULE_CONFIG_MODE_MMCSD (0<<12)
    #define OMAP_REG_MMC_MODULE_CONFIG_MODE_SPI  (1<<12)
    #define OMAP_REG_MMC_MODULE_CONFIG_MODE_TEST (2<<12)
    #define OMAP_REG_MMC_MODULE_CONFIG_PWRON     (1<<11)
    #define OMAP_REG_MMC_MODULE_CONFIGE_BE       (1<<10)
    #define OMAP_REG_MMC_MODULE_CONFIG_CLK_MASK  (0x3FF)
    #define OMAP_REG_MMC_MODULE_CONFIG_MODE_MASK (0x3 << 12)

#define OMAP_REG_MMC_MODULE_STATUS          0x10
    #define OMAP_REG_MMC_MODULE_STATUS_CERR      (1<<14)
    #define OMAP_REG_MMC_MODULE_STATUS_CIRQ      (1<<13)
    #define OMAP_REG_MMC_MODULE_STATUS_OCRB      (1<<12)
    #define OMAP_REG_MMC_MODULE_STATUS_AE        (1<<11)
    #define OMAP_REG_MMC_MODULE_STATUS_AF        (1<<10)
    #define OMAP_REG_MMC_MODULE_STATUS_CRW       (1<<9)
    #define OMAP_REG_MMC_MODULE_STATUS_CCRC      (1<<8)
    #define OMAP_REG_MMC_MODULE_STATUS_CTO       (1<<7)
    #define OMAP_REG_MMC_MODULE_STATUS_DCRC      (1<<6)
    #define OMAP_REG_MMC_MODULE_STATUS_DTO       (1<<5)
    #define OMAP_REG_MMC_MODULE_STATUS_EOFB      (1<<4)
    #define OMAP_REG_MMC_MODULE_STATUS_BRS       (1<<3)
    #define OMAP_REG_MMC_MODULE_STATUS_CB        (1<<2)
    #define OMAP_REG_MMC_MODULE_STATUS_CD        (1<<1)
    #define OMAP_REG_MMC_MODULE_STATUS_EOC       (1<<0)
    #define OMAP_REG_MMC_MODULE_STATUS_ALL       (0x7FFF)
    #define OMAP_REG_MMC_MODULE_STATUS_REQ_PROCESS \
          ((~(OMAP_REG_MMC_MODULE_STATUS_CIRQ | OMAP_REG_MMC_MODULE_STATUS_CD\
             )) & OMAP_REG_MMC_MODULE_STATUS_ALL)
    #define OMAP_STATUS_CMD_PROCESSING_ERRORS \
            (OMAP_REG_MMC_MODULE_STATUS_CERR | OMAP_REG_MMC_MODULE_STATUS_CTO | \
            OMAP_REG_MMC_MODULE_STATUS_CCRC)
    #define OMAP_STATUS_DATA_PROCESSING_ERRORS \
            (OMAP_REG_MMC_MODULE_STATUS_DCRC | OMAP_REG_MMC_MODULE_STATUS_DTO)
                      
#define OMAP_REG_MMC_INTERRUPT_ENABLE       0x14
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_CERR   (1<<14)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_CIRQ   (1<<13)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_OCRB   (1<<12)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_AE     (1<<11)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_AF     (1<<10)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_CRW    (1<<9)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_CCRC   (1<<8)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_CTO    (1<<7)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_DCRC   (1<<6)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_DTO    (1<<5)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_EOFB   (1<<4)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_BRS    (1<<3)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_CB     (1<<2)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_CD     (1<<1)
    #define OMAP_REG_MMC_INTERRUPT_ENABLE_EOC    (1<<0)
    #define OMAP_REG_MMC_INTERRUPT_ALL_INT       (0x7FFF)
    #define OMAP_REG_MMC_INTERRUPT_NONE_INT      (0)
    #define OMAP_REG_MMC_INTERRUPT_ERRORS        (OMAP_REG_MMC_INTERRUPT_ENABLE_CCRC | \
                                                  OMAP_REG_MMC_INTERRUPT_ENABLE_CTO  | \
                                                  OMAP_REG_MMC_INTERRUPT_ENABLE_DCRC | \
                                                  OMAP_REG_MMC_INTERRUPT_ENABLE_DTO  | \
                                                  OMAP_REG_MMC_INTERRUPT_ENABLE_CERR)
    #define OMAP_REG_MMC_INTERRUPT_REQUESTS  (OMAP_REG_MMC_INTERRUPT_ERRORS | \
                                              OMAP_REG_MMC_INTERRUPT_ENABLE_EOC | \
                                              OMAP_REG_MMC_INTERRUPT_ENABLE_AF | \
                                              OMAP_REG_MMC_INTERRUPT_ENABLE_AE | \
                                              OMAP_REG_MMC_INTERRUPT_ENABLE_EOFB )
                                              
#define OMAP_REG_MMC_CMD_TIMEOUT            0x18
    /* low 8-bit valid */
    
#define OMAP_REG_MMC_DATA_READ_TIMEOUT      0x1C
    /* 16-bit */
    
#define OMAP_REG_MMC_DATA_ACCESS            0x20
    /* 16-bit */

#define OMAP_REG_MMC_BLOCK_LENGTH           0x24
    /* low 11-bit */

#define OMAP_REG_MMC_BLOCK_COUNT            0x28
    /* low 11-bit */

#define OMAP_REG_MMC_BUFFER_CONFIG          0x2C
    #define OMAP_REG_MMC_BUFFER_CONFIG_RXDE      (1<<15)
    #define OMAP_REG_MMC_BUFFER_CONFIG_AFL_MASK  (0x1F00)
    #define OMAP_REG_MMC_BUFFER_CONFIG_AFL_SHIFT (8)
    #define OMAP_REG_MMC_BUFFER_CONFIG_TXDE      (1<<7)
    #define OMAP_REG_MMC_BUFFER_CONFIG_AEL_MASK  (0x1F)
    #define OMAP_REG_MMC_BUFFER_CONFIG_AEL_SHIFT (0)

#define OMAP_REG_MMC_SPI_CONFIG             0x30
    #define OMAP_REG_MMC_SPI_CONFIG_STR          (1<<15)
    #define OMAP_REG_MMC_SPI_CONFIG_WNR          (1<<14)
    #define OMAP_REG_MMC_SPI_CONFIG_SODV         (1<<13)
    #define OMAP_REG_MMC_SPI_CONFIG_CSTR         (1<<12)
    #define OMAP_REG_MMC_SPI_CONFIG_CSHOLD05     (0)
    #define OMAP_REG_MMC_SPI_CONFIG_CSHOLD15     (1<<10)
    #define OMAP_REG_MMC_SPI_CONFIG_CSHOLD25     (2<<10)
    #define OMAP_REG_MMC_SPI_CONFIG_CSHOLD35     (3<<10)
    #define OMAP_REG_MMC_SPI_CONFIG_TCSS1        (0)
    #define OMAP_REG_MMC_SPI_CONFIG_TCSS2        (1<<8)
    #define OMAP_REG_MMC_SPI_CONFIG_TCSS3        (2<<8)
    #define OMAP_REG_MMC_SPI_CONFIG_TCSS4        (3<<8)
    #define OMAP_REG_MMC_SPI_CONFIG_CSEL         (1<<7)
    #define OMAP_REG_MMC_SPI_CONFIG_CS1          (0)
    #define OMAP_REG_MMC_SPI_CONFIG_CS2          (1<<4)
    #define OMAP_REG_MMC_SPI_CONFIG_CS3          (2<<4)
    #define OMAP_REG_MMC_SPI_CONFIG_CS4          (3<<4)
    #define OMAP_REG_MMC_SPI_CONFIG_CSM          (1<<3)
    #define OMAP_REG_MMC_SPI_CONFIG_CSD          (1<<2)
    #define OMAP_REG_MMC_SPI_CONFIG_POL_RISE        (0)
    #define OMAP_REG_MMC_SPI_CONFIG_POL_FALL        (1)

#define OMAP_REG_MMC_SDIO_MODE_CONFIG       0x34
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_C5E    (1<<15)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_C14E   (1<<14)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_C13E   (1<<13)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_C12E   (1<<12)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_D3PS   (1<<11)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_D3ES   (1<<10)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_CDWE   (1<<9)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_IWE    (1<<8)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_DCR4   (1<<7)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_XDTS   (1<<6)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_DPE    (1<<5)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_RW     (1<<4)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_CDE    (1<<2)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_RWE    (1<<1)
    #define OMAP_REG_MMC_SDIO_MODE_CONFIG_IRQE   (1<<0)

#define OMAP_REG_MMC_SYSTEM_TEST            0x38
    #define OMAP_REG_MMC_SYSTEM_TEST_WAKD        (1<<15)
    #define OMAP_REG_MMC_SYSTEM_TEST_SSB         (1<<14)
    #define OMAP_REG_MMC_SYSTEM_TEST_RDYD        (1<<13)
    #define OMAP_REG_MMC_SYSTEM_TEST_DDIR        (1<<12)
    #define OMAP_REG_MMC_SYSTEM_TEST_D3D         (1<<11)
    #define OMAP_REG_MMC_SYSTEM_TEST_D2D         (1<<10)
    #define OMAP_REG_MMC_SYSTEM_TEST_D1D         (1<<9)
    #define OMAP_REG_MMC_SYSTEM_TEST_D0D         (1<<8)
    #define OMAP_REG_MMC_SYSTEM_TEST_CDIR        (1<<7)
    #define OMAP_REG_MMC_SYSTEM_TEST_CDAT        (1<<6)
    #define OMAP_REG_MMC_SYSTEM_TEST_MCKD        (1<<5)
    #define OMAP_REG_MMC_SYSTEM_TEST_SCKD        (1<<4)
    #define OMAP_REG_MMC_SYSTEM_TEST_CS3D        (1<<3)
    #define OMAP_REG_MMC_SYSTEM_TEST_CS2D        (1<<2)
    #define OMAP_REG_MMC_SYSTEM_TEST_CS1D        (1<<1)
    #define OMAP_REG_MMC_SYSTEM_TEST_CS0D        (1<<0)


#define OMAP_REG_MMC_MODULE_REV             0x3C
    #define OMAP_REG_MMC_MODULE_REV_MINOR_MASK   (0xF)
    #define OMAP_REG_MMC_MODULE_REV_MINOR_SHIFT  (0)
    #define OMAP_REG_MMC_MODULE_REV_MAJOR_MASK   (0xF0)
    #define OMAP_REG_MMC_MODULE_REV_MAJOR_SHIFT  (4)

#define OMAP_REG_MMC_CMD_RESPONSE0          0x40
    /* response bits 15-0 */
#define OMAP_REG_MMC_CMD_RESPONSE1          0x44
    /* response bits 31-16 */
#define OMAP_REG_MMC_CMD_RESPONSE2          0x48
    /* response bits 47-32 */
#define OMAP_REG_MMC_CMD_RESPONSE3          0x4C
    /* response bits 63-48 */
#define OMAP_REG_MMC_CMD_RESPONSE4          0x50
    /* response bits 79-64 */
#define OMAP_REG_MMC_CMD_RESPONSE5          0x54
    /* response bits 95-80 */
#define OMAP_REG_MMC_CMD_RESPONSE6          0x58
    /* response bits 111-96 */
#define OMAP_REG_MMC_CMD_RESPONSE7          0x5C
    /* response bits 127-112 */

#define OMAP_REG_MMC_SUSPEND_RESUME         0x60
    #define OMAP_REG_MMC_SUSPEND_RESUME_STOP     (1<<3)
    #define OMAP_REG_MMC_SUSPEND_RESUME_SAVE     (1<<2)
    #define OMAP_REG_MMC_SUSPEND_RESUME_RESUME   (1<<1)
    #define OMAP_REG_MMC_SUSPEND_RESUME_SUSPEND  (1<<0)

#define OMAP_REG_MMC_SYSTEM_CONTROL         0x64
    #define OMAP_REG_MMC_SYSTEM_CONTROL_SW_RESET (1<<1)

#define OMAP_REG_MMC_SYSTEM_STATUS          0x68
    #define OMAP_REG_MMC_SYSTEM_STATUS_RESET_DONE (1<<0)



#define SD_DEFAULT_RESPONSE_BYTES 6


#define CLOCK_ON  TRUE
#define CLOCK_OFF FALSE

#define OMAP_MAX_CLOCK_DIVIDE   1023

typedef struct _SD_CLOCK_TBL_ENTRY {
    INT       ClockRateDivisor;  /* divisor */
    UINT16    RegisterValue;     /* register value for clock divisor */  
}SD_CLOCK_TBL_ENTRY;

typedef enum _OMAP_DMA_MODE {
    OMAP_DMA_NONE = 0,
    OMAP_DMA_COMMON,
    OMAP_DMA_SG
}OMAP_DMA_MODE,*POMAP_DMA_MODE;

typedef struct _SDHCD_DEVICE {
    SDLIST        List;              /* linked list */
    SDHCD         Hcd;               /* HCD description for bus driver */
    OMAP_DMA_MODE DmaMode;           /* current DMA mode */
    BOOL          DmaCapable;        /* os layer supports DMA */
    UINT16        Clock;             /* current clock bit settings */
    UINT32        BaseClock;         /* base clock in hz */ 
    UINT32        TimeOut;           /* command timeout setting */ 
    UINT32        DataTimeOut;       /* data timeout setting */ 
    UINT32        ClockSpinLimit;    /* clock limit for command spin loops */
    BOOL          KeepClockOn;        
    BOOL          IrqDetectArmed;    /* IRQ detect was armed */  
    UINT8         CompletionCount;   /* used to track when both DMA and command complete are done */
    BOOL          Cancel;
    BOOL          ShuttingDown;
    BOOL          ShortTransfer;     /* do short transfer */
    SDCONFIG_BUS_MODE_DATA SavedBusMode; /* saved bus mode */
    HCD_OS_INFO   OSInfo;            /* the single device's OS-Specific */
}SDHCD_DEVICE, *PSDHCD_DEVICE;

/* driver wide data, this driver only supports one device, 
 * so we include the per device data here also */
typedef struct _SDHCD_DRIVER_CONTEXT {
    PTEXT        pDescription;       /* human readable device decsription */
    SDLIST       DeviceList;         /* the list of current devices handled by this driver */
    OS_SEMAPHORE DeviceListSem;      /* protection for the DeviceList */
    UINT         DeviceCount;        /* number of devices currently installed */     
    SDHCD_DRIVER Driver;             /* OS dependent driver specific info */  
}SDHCD_DRIVER_CONTEXT, *PSDHCD_DRIVER_CONTEXT;


/* prototypes */
SDIO_STATUS HcdRequest(PSDHCD pHcd);
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pReq);
SDIO_STATUS HcdInitialize(PSDHCD_DEVICE pDeviceContext);
void HcdDeinitialize(PSDHCD_DEVICE pDeviceContext);
BOOL HcdSDInterrupt(PSDHCD_DEVICE pDeviceContext);
BOOL HcdTransferTxData(PSDHCD_DEVICE pDevice, PSDREQUEST pReq);
BOOL HcdTransferRxData(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, BOOL Flush);


/* OS-dependent layer prototypes */
SDIO_STATUS QueueEventResponse(PSDHCD_DEVICE pDeviceContext, INT WorkItemID);
UINT16 MaskIrq(PSDHCD_DEVICE pDevice, UINT16 Mask, BOOL FromIsr);
UINT16 UnmaskIrq(PSDHCD_DEVICE pDevice, UINT16 Mask, BOOL FromIsr);
SDIO_STATUS SetUpHCDDMA(PSDHCD_DEVICE            pDevice, 
                        PSDREQUEST               pReq, 
                        PDMA_TRANSFER_COMPLETION pCompletion,
                        PVOID                    pContext);
BOOL WriteProtectSwitchOn(PSDHCD_DEVICE pDevice);
void SDCancelDMATransfer(PSDHCD_DEVICE pDevice);
SDIO_STATUS SetPowerLevel(PSDHCD_DEVICE pDeviceContext, BOOL On, SLOT_VOLTAGE_MASK Level);
void GetDefaults(PSDHCD_DEVICE pDeviceContext);
void CompleteRequestSyncDMA(PSDHCD_DEVICE pDeviceContext, PSDREQUEST pRequest, SDIO_STATUS Status);
void MicroDelay(INT Microseconds);
#define DBG_GPIO_PIN_1  1
#define DBG_GPIO_PIN_2  2

#ifdef OMAP_USE_DBG_GPIO
void ToggleGPIOPin(PSDHCD_DEVICE pDevice, INT PinNo);
#else
#define ToggleGPIOPin(p,n) 
#endif

SDIO_STATUS CheckDMA(PSDHCD_DEVICE pDevice, 
                     PSDREQUEST    pReq);
  	
SDIO_STATUS WaitDat0Busy(PSDHCD_DEVICE pDevice); 
  
/* end OS layer prototypes */
                              
#endif /* __SDIO_OMAP_HCD_H___ */
