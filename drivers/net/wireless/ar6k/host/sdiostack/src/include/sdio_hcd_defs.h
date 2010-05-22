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
@file: sdio_hcd_defs.h

@abstract: host controller driver definitions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_HCD_DEFS_H___
#define __SDIO_HCD_DEFS_H___

    /* write protect switch position data */
typedef UINT8 SDCONFIG_WP_VALUE;

    /* HC commands */
#define SDCONFIG_SEND_INIT_CLOCKS  (SDCONFIG_FLAGS_HC_CONFIG | SDCONFIG_FLAGS_DATA_PUT  | 1)
#define SDCONFIG_SDIO_INT_CTRL     (SDCONFIG_FLAGS_HC_CONFIG | SDCONFIG_FLAGS_DATA_PUT  | 2)
#define SDCONFIG_SDIO_REARM_INT    (SDCONFIG_FLAGS_HC_CONFIG | SDCONFIG_FLAGS_DATA_NONE | 3)
#define SDCONFIG_BUS_MODE_CTRL     (SDCONFIG_FLAGS_HC_CONFIG | SDCONFIG_FLAGS_DATA_BOTH | 4)
#define SDCONFIG_POWER_CTRL        (SDCONFIG_FLAGS_HC_CONFIG | SDCONFIG_FLAGS_DATA_PUT  | 5)
#define SDCONFIG_GET_WP            (SDCONFIG_FLAGS_HC_CONFIG | SDCONFIG_FLAGS_DATA_GET  | 6)
/* ATHENV */
#define SDCONFIG_ENABLE            0xFFFF
/* ATHENV */

    /* slot init clocks control */
typedef struct _SDCONFIG_INIT_CLOCKS_DATA  {
    UINT16  NumberOfClocks;  /* number of clocks to issue in the current bus mode*/
}SDCONFIG_INIT_CLOCKS_DATA, *PSDCONFIG_INIT_CLOCKS_DATA;

/* slot power control */
typedef struct _SDCONFIG_POWER_CTRL_DATA  {
    BOOL                SlotPowerEnable;            /* turn on/off slot power */
    SLOT_VOLTAGE_MASK   SlotPowerVoltageMask;       /* slot power voltage mask */
}SDCONFIG_POWER_CTRL_DATA, *PSDCONFIG_POWER_CTRL_DATA;

typedef UINT8 SDIO_IRQ_MODE_FLAGS;
/* SDIO Interrupt control */
typedef struct _SDCONFIG_SDIO_INT_CTRL_DATA  {
    BOOL                  SlotIRQEnable;      /* turn on/off Slot IRQ detection */
    SDIO_IRQ_MODE_FLAGS   IRQDetectMode;      /* slot IRQ detect mode , only valid if Enabled = TRUE */
#define IRQ_DETECT_RAW       0x00
#define IRQ_DETECT_MULTI_BLK 0x01
#define IRQ_DETECT_4_BIT     0x02
#define IRQ_DETECT_1_BIT     0x04
#define IRQ_DETECT_SPI       0x08
}SDCONFIG_SDIO_INT_CTRL_DATA, *PSDCONFIG_SDIO_INT_CTRL_DATA;

/* card insert */
#define EVENT_HCD_ATTACH               1
/* card remove */
#define EVENT_HCD_DETACH               2
/* card slot interrupt */
#define EVENT_HCD_SDIO_IRQ_PENDING     3
/* transfer done */
#define EVENT_HCD_TRANSFER_DONE        4
/* (internal use only) */
#define EVENT_HCD_CD_POLLING           5
/* NOP */
#define EVENT_HCD_NOP                  0

/* attrib_flags */
#define SDHCD_ATTRIB_SUPPORTS_POWER   0x0001  /* host controller driver supports power managment */
#define SDHCD_ATTRIB_BUS_1BIT         0x0002  /* SD Native 1 - bit mode */
#define SDHCD_ATTRIB_BUS_4BIT         0x0004  /* SD Native 4 - bit mode */
#define SDHCD_ATTRIB_BUS_SPI          0x0008  /* SPI mode capable */
#define SDHCD_ATTRIB_READ_WAIT        0x0010  /* read wait supported (SD-only) */
#define SDHCD_ATTRIB_MULTI_BLK_IRQ    0x0020  /* interrupts between multi-block capable (SD-only) */
#define SDHCD_ATTRIB_BUS_MMC8BIT      0x0040  /* MMC  8-bit */
#define SDHCD_ATTRIB_SLOT_POLLING     0x0080  /* requires slot polling for Card Detect */
#define SDHCD_ATTRIB_POWER_SWITCH     0x0100  /* host has power switch control, must be set if SPI
                                                 mode can be switched to 1 or 4 bit mode */
#define SDHCD_ATTRIB_NO_SPI_CRC       0x0200  /* when in SPI mode, 
                                                 host wants to run without SPI CRC */
#define SDHCD_ATTRIB_AUTO_CMD12       0x0400  /* host controller supports auto CMD12 */
#define SDHCD_ATTRIB_NO_4BIT_IRQ      0x0800  /* host controller does not support 4 bit IRQ mode*/
#define SDHCD_ATTRIB_RAW_MODE         0x1000  /* host controller is a raw mode hcd*/
#define SDHCD_ATTRIB_SD_HIGH_SPEED    0x2000  /* host controller supports SD high speed interface */
#define SDHCD_ATTRIB_MMC_HIGH_SPEED   0x4000  /* host controller supports MMC high speed interface */

#define IS_CARD_PRESENT(pHcd)         ((pHcd)->CardProperties.Flags & CARD_TYPE_MASK)
#define SET_CURRENT_REQUEST(pHcd,Req) (pHcd)->pCurrentRequest = (Req)
#define IS_HCD_RAW(pHcd)              ((pHcd)->Attributes & SDHCD_ATTRIB_RAW_MODE)
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: Get a pointer to the current bus request for a host controller

  @function name: GET_CURRENT_REQUEST
  @prototype: PSDREQUEST GET_CURRENT_REQUEST (PSDHCD pHcd) 
  @category: HD_Reference
 
  @input:  pHcd - host structure
           
  @return: current SD/SDIO bus request being worked on
 
  @notes: Implemented as a macro. This macro returns the current SD request that is
          being worked on.
           
  @example: getting the current request: 
          pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
        
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define GET_CURRENT_REQUEST(pHcd)     (pHcd)->pCurrentRequest 
#define GET_CURRENT_BUS_WIDTH(pHcd) SDCONFIG_GET_BUSWIDTH((pHcd)->CardProperties.BusMode)

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: Get host controller's current operational bus clock 
  
  @function name: SDHCD_GET_OPER_CLOCK
  @prototype: SD_BUSCLOCK_RATE SDHCD_GET_OPER_CLOCK(PSDHCD pHcd)
  @category: HD_Reference
  
  @input:  pHcd   - the registered host structure
 
  @output: none

  @return: clock rate
 
  @notes: Implemented as a macro. Returns the current bus clock rate. 
         
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define SDHCD_GET_OPER_CLOCK(pHcd)      (pHcd)->CardProperties.OperBusClock
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: Is host controller operating in SPI mode

  @function name: IS_HCD_BUS_MODE_SPI
  @prototype: BOOL IS_HCD_BUS_MODE_SPI (PSDHCD pHcd) 
  @category: HD_Reference
 
  @input:  pHcd - host structure
           
  @return: TRUE if in SPI mode
 
  @notes: Implemented as a macro. Host controllers that operate in SPI mode
          dynamically can use this macro to check for SPI operation.
           
  @example: testing for SPI mode: 
          if (IS_HCD_BUS_MODE_SPI(&pHct->Hcd)) {
             .. in spi mode
          }
        
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define IS_HCD_BUS_MODE_SPI(pHcd)   (GET_CURRENT_BUS_WIDTH(pHcd) == SDCONFIG_BUS_WIDTH_SPI)

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: Is host controller using SPI in non-CRC mode

  @function name: IS_HCD_BUS_MODE_SPI_NO_CRC
  @prototype: BOOL IS_HCD_BUS_MODE_SPI_NO_CRC(PSDHCD pHcd) 
  @category: HD_Reference
 
  @input:  pHcd - host structure
           
  @return: TRUE if CRC mode is off
 
  @notes: Implemented as a macro. SPI-capable cards and systems can operate in 
          non-CRC protected mode.  In this mode the host controller should ignore
          CRC fields and/or disable CRC generation when issuing command or data
          packets.  This option is useful for software based SPI mode where CRC
          should be turned off in order to reduce processing overhead.
           
  @example: test for non-CRC SPI mode: 
          if (IS_HCD_BUS_MODE_SPI_NO_CRC(&pHct->Hcd)) {
             .. disable CRC checking in hardware.
          }
        
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define IS_HCD_BUS_MODE_SPI_NO_CRC(pHcd)   ((pHcd)->CardProperties.BusMode & \
                                                        SDCONFIG_BUS_MODE_SPI_NO_CRC)

typedef UINT8 SDHCD_RESPONSE_CHECK_MODE;
/* have SDIO core check the response token and see if it is okay to continue with
   * the data portion */
#define SDHCD_CHECK_DATA_TRANS_OK   0x01
/* have SDIO core check the SPI token received */
#define SDHCD_CHECK_SPI_TOKEN       0x02
 
/* prototypes */
/* for HCD use */
SDIO_STATUS SDIO_RegisterHostController(PSDHCD pHcd);
SDIO_STATUS SDIO_UnregisterHostController(PSDHCD pHcd);
SDIO_STATUS SDIO_HandleHcdEvent(PSDHCD pHcd, HCD_EVENT Event);
SDIO_STATUS SDIO_CheckResponse(PSDHCD pHcd, PSDREQUEST pReq, SDHCD_RESPONSE_CHECK_MODE CheckMode);
SDIO_STATUS SDIO_BusAddOSDevice(PSDDMA_DESCRIPTION pDma, POS_PNPDRIVER pDriver, POS_PNPDEVICE pDevice);
void SDIO_BusRemoveOSDevice(POS_PNPDRIVER pDriver, POS_PNPDEVICE pDevice);
 
#endif /* __SDIO_BUSDRIVER_H___ */
