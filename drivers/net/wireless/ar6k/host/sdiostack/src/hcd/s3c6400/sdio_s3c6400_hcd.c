/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_std_hcd.c

@abstract: SDIO standard host controller implementation

#notes: OS independent code
 
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
#include "sdio_s3c6400_hcd.h"

#define HOST_REG_CONTROL2 (0x80)
#define CMD8 8
#define CMD12 12
#define CMD_TYPE_ABORT 0x00c0
#define CMD52 52

#define CLOCK_ON  TRUE
#define CLOCK_OFF FALSE
         
SDIO_STATUS SetPowerLevel(PSDHCD_INSTANCE pHcInstance, BOOL On, SLOT_VOLTAGE_MASK Level); 
SDIO_STATUS ProcessCommandDone(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq, BOOL FromIsr);

    /* clock divisor table */
SD_CLOCK_TBL_ENTRY SDClockDivisorTable[SD_CLOCK_MAX_ENTRIES] =
{   /* clock rate divisor, divisor setting */
    {1, 0x0000},
    {2, 0x0100},
    {4, 0x0200},
    {8, 0x0400},
    {16,0x0800},
    {32,0x1000},
    {64,0x2000},
    {128,0x4000},
    {256,0x8000}, 
};

/* ULF debug */
dbghold dbg[2];
int dbgidx = 0;
int running = 0;

    /* register polling change macro */
#define WAIT_REGISTER32_CHANGE(pHcInstance, pStatus, reg,mask,cmp,timout) \
    {\
        if (!WaitRegisterBitsChange((pHcInstance),    \
                                    (pStatus),    \
                                    (reg),        \
                                    (mask),       \
                                    (cmp),        \
                                    (timout))) {  \
           DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - Reg Change Timeout : 0x%X src:%s, line:%d \n",\
           (reg),__FILE__, __LINE__));        \
        }                                     \
    }

#define WAIT_REGISTER32_CHANGE_OR(pHcInstance, pStatus, reg,mask,ormask,timout) \
    {\
        if (!WaitRegisterBitsChangeOR((pHcInstance),    \
                                    (pStatus),    \
                                    (reg),        \
                                    (mask),       \
                                    (ormask),     \
                                    (timout))) {  \
           DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - Reg Change Timeout : 0x%X src:%s, line:%d \n",\
           (reg),__FILE__, __LINE__));        \
        }                                     \
    }


    /* command data ready polling macro */ 
#define WAIT_FOR_DAT_CMD_DAT_READY(pHcInstance, pStatus) \
        WAIT_REGISTER32_CHANGE(pHcInstance,            \
                             pStatus,            \
                             HOST_REG_PRESENT_STATE,(HOST_REG_PRESENT_STATE_BUFFER_COMMAND_INHIBIT_DAT | \
                             HOST_REG_PRESENT_STATE_BUFFER_COMMAND_INHIBIT_CMD), \
                             0, pHcInstance->PresentStateWaitLimit)
                             
    /* function to wait for a register bit(s) change */
static BOOL WaitRegisterBitsChange(PSDHCD_INSTANCE pHcInstance, 
                            SDIO_STATUS   *pStatus,
                            UINT32         Reg, 
                            UINT32         Mask, 
                            UINT32         CompareMask, 
                            UINT32         Count)
{
    while (Count) {
                
        if ((READ_HOST_REG32(pHcInstance, Reg) & Mask) == CompareMask) {
            break;
        }
                
        Count--;    
    }    
    
    if (0 == Count) {
        if (pStatus != NULL) {
            *pStatus = SDIO_STATUS_ERROR;
        }
        return FALSE;  
    }
    
    if (pStatus != NULL) {
        *pStatus = SDIO_STATUS_SUCCESS;
    }
        
    return TRUE;
}

static BOOL WaitRegisterBitsChangeOR(PSDHCD_INSTANCE pHcInstance, 
		                            SDIO_STATUS   *pStatus,
		                            UINT32         Reg, 
		                            UINT32         Mask, 
		                            UINT32         OrMask, 
		                            UINT32         Count)
{
    while (Count) {
                
        if ((READ_HOST_REG32(pHcInstance, Reg) & Mask) & OrMask) {
            break;
        }
                
        Count--;    
    }    
    
    if (0 == Count) {
        if (pStatus != NULL) {
            *pStatus = SDIO_STATUS_ERROR;
        }
        return FALSE;  
    }
    
    if (pStatus != NULL) {
        *pStatus = SDIO_STATUS_SUCCESS;
    }
        
    return TRUE;
}

    /* reset command data line state machines */
void _DoResetCmdDatLine(PSDHCD_INSTANCE pHcInstance)
{        /* issue reset */

    unsigned int temp;
    
    WRITE_HOST_REG32(pHcInstance, HOST_REG_SW_RESET, 
            (HOST_REG_SW_RST_CMD_LINE | HOST_REG_SW_RST_DAT_LINE));
        /* wait for bits to clear */
    WAIT_REGISTER32_CHANGE(pHcInstance, 
                           NULL,
                           HOST_REG_SW_RESET,
                           HOST_REG_SW_RST_CMD_LINE | HOST_REG_SW_RST_DAT_LINE,
                           0, 
                           pHcInstance->ResetWaitLimit);
    temp = READ_HOST_REG32(pHcInstance, HOST_REG_CONTROL2);
    WRITE_HOST_REG32(pHcInstance,  HOST_REG_CONTROL2, temp | (0x1<<30));
}

#define ResetCmdDatLine(pHc) \
{                            \
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST - **** reseting cmd data at line:%d \n",\
           __LINE__));        \
    _DoResetCmdDatLine((pHc));         \
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  GetResponseData - get the response data 
  Input:    pHcInstance - device context
            pReq - the request
  Output: 
  Return:
  Notes:  
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void GetResponseData(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq)
{
    INT     dwordCount;
    INT     byteCount;
    UINT32  readBuffer[4];
    INT     ii;
    UINT16  cmdreg;
    
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP) {
        return;    
    }
    
    cmdreg = READ_HOST_REG16(pHcInstance, HOST_REG_COMMAND_REGISTER);
    
    if( (cmdreg & (0x18|0x3)) == (0x18|0x3) )
        _DoResetCmdDatLine(pHcInstance);

    byteCount = SD_DEFAULT_RESPONSE_BYTES;        
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
        byteCount = SD_R2_RESPONSE_BYTES;         
    } 
    dwordCount = (byteCount + 3) / 4;      

    /* move data into read buffer */
    for (ii = 0; ii < dwordCount; ii++) {
        readBuffer[ii] = READ_HOST_REG32(pHcInstance, HOST_REG_RESPONSE+(ii*4));
    }

    /* handle normal SD/MMC responses */        
  
    /* the standard host strips the CRC for all responses and puts them in 
     * a nice linear order */
    memcpy(&pReq->Response[1],readBuffer,byteCount);
   
    if (DBG_GET_DEBUG_LEVEL() >= STD_HOST_TRACE_REQUESTS) { 
        if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
            byteCount = 17; 
        }
        SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO S3C6400 HOST - Response Dump");
    }
    
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  DumpCurrentRequestInfo - debug dump  
  Input:    pHcInstance - device context
  Output: 
  Return: 
  Notes: This function debug prints the current request  
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void DumpCurrentRequestInfo(PSDHCD_INSTANCE pHcInstance)
{
    PSDREQUEST pRequest = GET_CURRENT_REQUEST(&pHcInstance->Hcd);
    if (pRequest != NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - Current Request Command:%d, ARG:0x%8.8X\n",
                 pRequest->Command, pRequest->Argument));
        if (IS_SDREQ_DATA_TRANS(pRequest->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - Data %s, Blocks: %d, BlockLen:%d Remaining: %d \n",
                      IS_SDREQ_WRITE_DATA(pRequest->Flags) ? "WRITE":"READ",
                      pRequest->BlockCount,
                      pRequest->BlockLen,
                      pRequest->DataRemaining));
        }
    }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  TranslateSDError - check for an SD error 
  Input:    pHcInstance - device context
            Status -  error interrupt status register value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS TranslateSDError(PSDHCD_INSTANCE pHcInstance, UINT16 ErrorMask)
{
    SDIO_STATUS status = SDIO_STATUS_DEVICE_ERROR;
    
    DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - TranslateSDError :0x%X \n",ErrorMask));
    
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_CRCERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - RESP CRC ERROR \n"));
        status = SDIO_STATUS_BUS_RESP_CRC_ERR;
    }
    
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_DATATIMEOUTERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - DATA TIMEOUT ERROR \n"));
        status = SDIO_STATUS_BUS_READ_TIMEOUT;
    } 
    
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_DATACRCERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - READ or WRITE DATA CRC ERROR \n"));
        status = SDIO_STATUS_BUS_READ_CRC_ERR;
    } 
    
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_CMDTIMEOUTERR) {
        if (pHcInstance->CardInserted) {
                /* hide error if we are polling an empty slot */
            DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST - RESPONSE TIMEOUT \n"));
        }
        status = SDIO_STATUS_BUS_RESP_TIMEOUT;
    } 
        
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_VENDOR_MASK) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt vendor error 0x%X: \n", 
                    (UINT)((ErrorMask & HOST_REG_ERROR_INT_STATUS_VENDOR_MASK) >> 
                            HOST_REG_ERROR_INT_STATUS_VENDOR_SHIFT)));
    } 
    
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_AUTOCMD12ERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt auto cmd12 error\n"));
    }
    
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_CURRENTLIMITERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt current limit error\n"));
    }
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_DATAENDBITERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt data end bit error\n"));
    }
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_DATATIMEOUTERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt data timeout error\n"));
    }
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_CMDINDEXERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt CMD index error\n"));
    }
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_CMDENDBITERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt CMD end bit error\n")); 
    }
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_CRCERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt CRC error\n"));
    }
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_SDMAERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt SDMA error (system address:0x%X\n",
            READ_HOST_REG32(pHcInstance, HOST_REG_SYSTEM_ADDRESS))); 
    }
    if (ErrorMask & HOST_REG_ERROR_INT_STATUS_ADMAERR) {
        UINT32 dmaErrStatus;
        dmaErrStatus = READ_HOST_REG32(pHcInstance, HOST_REG_ADMA_ERR_STATUS);
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST HcdSDInterrupt ADMA error status: 0x%X \n",
        dmaErrStatus));  
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST, ADMA Descriptor Start ADMA %s Address: 0x%X\n",
                 (dmaErrStatus & HOST_REG_ADMA_STATE_MASK) == HOST_REG_ADMA_STATE_FDS ? "Bad" : "Current",
                 READ_HOST_REG32(pHcInstance, HOST_REG_ADMA_ADDRESS)));   
        DumpDMADescriptorsInfo(pHcInstance);
    }   
    return status;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  ClockStartStop - SD clock control
  Input:  pHcInstance - device object
          On - turn on or off (TRUE/FALSE)
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void ClockStartStop(PSDHCD_INSTANCE pHcInstance, BOOL On) 
{

    DBG_PRINT(STD_HOST_TRACE_CLOCK, ("SDIO S3C6400 HOST ClockStartStop, %d\n", (UINT)On));
    
    if (On) {
        DBG_PRINT(STD_HOST_TRACE_CLOCK, ("SDIO S3C6400 HOST ClockControl <- 0x%x\n", pHcInstance->ClockControlState));
        pHcInstance->ClockControlState |= HOST_REG_CLOCK_CONTROL_CLOCK_ENABLE;  // Just in case, this should be on anyway
        WRITE_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL, pHcInstance->ClockControlState);
        
        /* wait for internal clock to stabilize */
        while(!(READ_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL) & 
                    (UINT16)HOST_REG_CLOCK_CONTROL_CLOCK_STABLE)) {
          ;
        }
        WRITE_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL, pHcInstance->ClockControlState | 
                        (UINT16)HOST_REG_CLOCK_CONTROL_SD_ENABLE);

        DBG_PRINT(STD_HOST_TRACE_CLOCK, ("SDIO S3C6400 Final ClockControl is 0x%x\n", 
                                        READ_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL)));
        DBG_PRINT(STD_HOST_TRACE_CLOCK, ("SDIO S3C6400 And Power Control is 0x%x\n", 
                                        READ_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL)));

        DBG_PRINT(STD_HOST_TRACE_CLOCK, ("SDIO S3C6400 Present State is is 0x%x\n", 
                                        READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE)));
        

    } else {
        pHcInstance->ClockControlState &= ~HOST_REG_CLOCK_CONTROL_SD_ENABLE;  // Just in case, this should be off anyway
        WRITE_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL, pHcInstance->ClockControlState);
    }  
}

#define ADJUST_BUS_IDLE      TRUE
#define ADJUST_BUS_NORMAL    FALSE

/* special adjustments made when the bus is idle or switched on to normal  */
static void IdleBusAdjustment(PSDHCD_INSTANCE pHcInstance, BOOL Idle)
{
    UINT16 clockControlValue;
    UINT16 clockControlCompareVal;
    UINT8  control;
    
    do {
        
        if (pHcInstance->Idle1BitIRQ) {
                /* the host controller requires a switch to 1 bit mode to detect interrupts
                 * when the bus is idle.  This is a work around for certain std hosts that
                 * miss interrupts in 4-bit mode */
                 
            control = READ_HOST_REG8(pHcInstance, HOST_REG_CONTROL);
            control &= ~HOST_REG_CONTROL_BUSWIDTH_BITS;     
            
            if (Idle) {       
                    /* switch controller to 1 bit mode for interrupt detection */
                control |= HOST_REG_CONTROL_1BIT_WIDTH;
            } else {
                    /* check currently configured bus mode to restore */
                if (GET_CURRENT_BUS_WIDTH(&pHcInstance->Hcd) == SDCONFIG_BUS_WIDTH_4_BIT) {
                    control |= HOST_REG_CONTROL_4BIT_WIDTH;    
                } else {
                    control |= HOST_REG_CONTROL_1BIT_WIDTH; 
                }
            }            
            
            WRITE_HOST_REG8(pHcInstance, HOST_REG_CONTROL, control);   
        }
        
        if (0 == pHcInstance->IdleBusClockRate) {
                /* no rate control in effect */
            break; 
        }
        
        if (Idle) {
                /* idle mode value */
            clockControlValue = pHcInstance->ClockConfigIdle;
        } else {
                /* normal mode value */
            clockControlValue = pHcInstance->ClockConfigNormal; 
        }
        
        clockControlValue |= (UINT16)HOST_REG_CLOCK_CONTROL_CLOCK_ENABLE;
        
            /* read current settings and compare */
        clockControlCompareVal = READ_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL);
        clockControlCompareVal &= HOST_REG_CLOCK_CONTROL_FREQ_SELECT_MASK | 
                                        HOST_REG_CLOCK_CONTROL_CLOCK_ENABLE;
        
        if (clockControlValue == clockControlCompareVal) {
                /* no update required, we can skip the clock stabilization polling because the
                 * clock rate is already set */
            break;    
        }
        
        WRITE_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL, clockControlValue);
        
            /* wait for clock to stabilize */
        while(!(READ_HOST_REG16(pHcInstance, HOST_REG_CLOCK_CONTROL) & 
                    (UINT16)HOST_REG_CLOCK_CONTROL_CLOCK_STABLE)) {
          ;
        }
        
        if (Idle) {
                /* when requested for IDLE mode, we leave the clock on after we adjust the rate */
            WRITE_HOST_REG16(pHcInstance, 
                             HOST_REG_CLOCK_CONTROL,
                             clockControlValue | HOST_REG_CLOCK_CONTROL_SD_ENABLE);        
        } 
        
    } while (FALSE);
               
}

int GetClockTableIndex(PSDHCD_INSTANCE pHcInstance, UINT32 DesiredRate, UINT32 *pActualRate)
{
    int     clockIndex,ii;   
    UINT32  rate;
    
    clockIndex = SD_CLOCK_MAX_ENTRIES - 1;
    
    *pActualRate = (pHcInstance->BaseClock) / SDClockDivisorTable[clockIndex].ClockRateDivisor;
    for (ii = 0; ii < SD_CLOCK_MAX_ENTRIES; ii++) {
        rate = pHcInstance->BaseClock / SDClockDivisorTable[ii].ClockRateDivisor;
        if (DesiredRate >= rate) {
            *pActualRate = rate;
            clockIndex = ii;
            break; 
        }   
    }
    
    return clockIndex;
}

int SetClockRate(PSDHCD_INSTANCE pHcInstance, PSDCONFIG_BUS_MODE_DATA pMode) 
{
    int          clockIndex;
    unsigned int control2, maxclock;

    DBG_PRINT(SDDBG_TRACE, ("SDIO s3c6400 - SetClockCtl clock: %d\n", pMode->ClockRate));
    
    clockIndex = GetClockTableIndex(pHcInstance, pMode->ClockRate, &pMode->ActualClockRate);
    control2 = READ_HOST_REG8(pHcInstance, HOST_REG_CONTROL2) |(0x1<<30);
    
    if( pMode->ClockRate <= 400000 ) {
      maxclock = 96000000;                     // Is not maxclock dependent on a clk source ?
      control2 &= ~(0x3<<14);
#ifdef SRCCLK_48MHZ
      control2 |= ((0x3<<9)|(0x1<<8)|(0x3<<4));
#else
      control2 |= ((0x3<<9)|(0x1<<8)|(CLKSRC<<4)); // EPLL 
#endif
      
    } else if ( pMode->ClockRate <= 25000000) {
      maxclock = 48000000;
      control2 &= ~(0x3<<14);
#ifdef SRCCLK_48MHZ
      control2 |= ((0x3<<9)|(0x1<<8)|(0x3<<4));
#else
      control2 |= ((0x3<<9)|(0x1<<8)|(CLKSRC<<4));
#endif      
    } else {
      control2 &= ~(0x3<<14);
#ifdef SRCCLK_48MHZ
      control2 |= ((0x3<<9)|(0x1<<8)|(0x3/*USB PHY*/<<4));
#else
      control2 |= ((0x3<<9)|(0x1<<8)|(CLKSRC/*EPLL*/<<4));
#endif
    }
//    control2 &= ~0x700;
    DBG_PRINT(SDDBG_TRACE, ("SDIO s3c6400 - Set Control2 to: 0x%x\n", control2));
    
    
    WRITE_HOST_REG32(pHcInstance, HOST_REG_CONTROL2, control2);

    DBG_PRINT(STD_HOST_TRACE_CONFIG , ("SDIO S3C6400 HOST - Clock: %d Hz, ClockRate %d (%d) state:0x%X \n", 
                                   pMode->ActualClockRate, pMode->ClockRate, clockIndex, 
                                   (UINT)pHcInstance->ClockControlState));
    return clockIndex;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  SetBusMode - Set Bus mode
  Input:  pHcInstance - device object
          pMode - mode
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void SetBusMode(PSDHCD_INSTANCE pHcInstance, PSDCONFIG_BUS_MODE_DATA pMode) 
{
    int clockIndex;
    UINT8  control;    
    
    DBG_PRINT(STD_HOST_TRACE_CONFIG , ("SDIO S3C6400 HOST - SetMode\n"));

    control = 0;

    switch (SDCONFIG_GET_BUSWIDTH(pMode->BusModeFlags)) {
        case SDCONFIG_BUS_WIDTH_1_BIT:
            printk( KERN_ERR "SDIO S3C6400 HOST bus set to 1 bit mode\n");
            break;        
        case SDCONFIG_BUS_WIDTH_4_BIT:
            printk( KERN_ERR "SDIO S3C6400 HOST bus set to 4 bit mode\n");
            control |=  HOST_REG_CONTROL_4BIT_WIDTH;
            break;
        case SDCONFIG_BUS_WIDTH_MMC8_BIT:
            control |=  HOST_REG_CONTROL_EXTENDED_DATA;
            DBG_PRINT(SDDBG_TRACE , ("SDIO S3C6400 HOST - SetMode, 8 bit bus width requested 0x%X\n", pMode->BusModeFlags));
            break;    
        default:
            DBG_PRINT(SDDBG_TRACE , ("SDIO S3C6400 HOST - SetMode, unknown bus width requested 0x%X\n", pMode->BusModeFlags));
            break;
    }
#if 0
    if (pMode->BusModeFlags & SDCONFIG_BUS_MODE_SD_HS) { 
        control |= HOST_REG_CONTROL_HI_SPEED;
    }
#endif
   
    clockIndex = SetClockRate(pHcInstance, pMode);

        /* set the clock divisor, unprotected read modify write */
    pHcInstance->ClockControlState = SDClockDivisorTable[clockIndex].RegisterValue | 
                      (UINT16)HOST_REG_CLOCK_CONTROL_CLOCK_ENABLE;
    
        /* save the normal operational value for dynamic clock control */
    pHcInstance->ClockConfigNormal = SDClockDivisorTable[clockIndex].RegisterValue;
    
    WRITE_HOST_REG8(pHcInstance, HOST_REG_CONTROL, control);                 
    mmiowb();
    WRITE_HOST_REG8(pHcInstance, HOST_REG_CONTROL, control);                 

}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdTransferTxData - data transmit transfer
  Input:  pHcInstance - device object
          pReq    - transfer request
  Output: 
  Return: 
  Notes: writes request data
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL HcdTransferTxData(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq)
{
    INT     dataCopy;
    PUINT8  pBuf;
    
    dataCopy = min(pReq->DataRemaining, (UINT)pReq->BlockLen);   
    pBuf = (PUINT8)pReq->pHcdContext;
   
    /* update remaining count */
    pReq->DataRemaining -= dataCopy;
    /* set the block data */
    while(dataCopy) { 
        UINT32 outData = 0;
        UINT   count = 0;
        if (dataCopy > 4) {
            outData = ((UINT32)(*(pBuf+0))) | 
                      (((UINT32)(*(pBuf+1))) << 8) | 
                      (((UINT32)(*(pBuf+2))) << 16) | 
                      (((UINT32)(*(pBuf+3))) << 24);
            dataCopy -= 4; 
            pBuf += 4;
        } else {
            for(count = 0; (dataCopy > 0) && (count < 4); count++) {
               outData |= (*pBuf) << (count*8); 
               pBuf++;
               dataCopy--;
            }
        }
        WRITE_HOST_REG32(pHcInstance, HOST_REG_BUFFER_DATA_PORT, outData);
    }
    
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
    if (pReq->DataRemaining) {
        return FALSE; 
    }
    return TRUE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdTransferRxData - data receive transfer
  Input:  pHcInstance - device object
          pReq    - transfer request
  Output: 
  Return: 
  Notes: reads request data
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void HcdTransferRxData(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq)
{
    INT     dataCopy;
    PUINT8  pBuf;
    
    dataCopy = min(pReq->DataRemaining, (UINT)pReq->BlockLen);   
    pBuf = (PUINT8)pReq->pHcdContext;
   
    /* update remaining count */
    pReq->DataRemaining -= dataCopy;
    /* set the block data */
    while(dataCopy) { 
        UINT32 inData;
        UINT   count = 0;
        inData = READ_HOST_REG32(pHcInstance, HOST_REG_BUFFER_DATA_PORT);
        for(count = 0; (dataCopy > 0) && (count < 4); count++) {
            *pBuf = (inData >> (count*8)) & 0xFF;
            dataCopy--;
            pBuf++;
        }
    }
    
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdRequest - SD request handler
  Input:  pHcd - HCD object
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdRequest(PSDHCD pHcd) 
{
    SDIO_STATUS     status = SDIO_STATUS_SUCCESS;
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext;
    UINT16          cmd_reg, temp, nint,errint;
    PSDREQUEST      pReq;
    UINT32		pstate = READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE);
    
    if (pstate & 0x307)
      printk(KERN_ERR "Request entered with present state at 0x%x\n", pstate);
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdRequest Enter\n"));
    
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
    
    nint = READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
    errint = READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS);
    
    if( running ) {
      printk( KERN_ERR "Request entered without previous completion\n");
      mdelay(10);
    }
    running = 1;
    
    pHcInstance->ErrIntSignalEn = READ_HOST_REG8(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE );
    pHcInstance->ErrIntStatusEn = READ_HOST_REG8(pHcInstance, HOST_REG_ERR_STATUS_ENABLE );
    pHcInstance->IntStatusEn    = READ_HOST_REG8(pHcInstance, HOST_REG_INT_STATUS_ENABLE );
    pHcInstance->IntSignalEn    = READ_HOST_REG8(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE );
        /* make sure clock is off */
    ClockStartStop(pHcInstance, CLOCK_OFF);
           
        /* make sure error ints are disabled */
    WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE,0);
    
        /* mask the remove while we are spinning on the CMD ready bits */
    MaskIrq(pHcInstance, HOST_REG_INT_STATUS_ALLOW_INSERT_REMOVE_ONLY,FALSE);

    do {
        
        if (pHcInstance->ShuttingDown) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdRequest returning canceled\n"));
            status = SDIO_STATUS_CANCELED;
            break;
        }
                
        switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) { 
            case SDREQ_FLAGS_NO_RESP:
                cmd_reg = 0x00;
                break;
            case SDREQ_FLAGS_RESP_R2:
                cmd_reg = 0x01 |
                        HOST_REG_COMMAND_REGISTER_CRC_CHECK_ENABLE;
                break;
            case SDREQ_FLAGS_RESP_R3:
            case SDREQ_FLAGS_RESP_SDIO_R4:
                cmd_reg = 0x02;
                break;
            case SDREQ_FLAGS_RESP_R1:
            case SDREQ_FLAGS_RESP_SDIO_R5:
            case SDREQ_FLAGS_RESP_R6:
                cmd_reg = 0x02 | HOST_REG_COMMAND_REGISTER_CRC_CHECK_ENABLE 
                            | HOST_REG_COMMAND_REGISTER_CMD_INDEX_CHECK_ENABLE;
                break;
            case SDREQ_FLAGS_RESP_R1B:   
                cmd_reg = 0x03 | HOST_REG_COMMAND_REGISTER_CRC_CHECK_ENABLE
                            | HOST_REG_COMMAND_REGISTER_CMD_INDEX_CHECK_ENABLE;
                break;
            default:
                cmd_reg = 0x00;
                DBG_ASSERT(FALSE);
                status = SDIO_STATUS_INVALID_PARAMETER;
                break;
        }   

        if (!SDIO_SUCCESS(status)) {
            break;
        }
        
            /* check and see if the card is still there... on some
             * host controller implementations card removal seems to prevent the
             * controller from actually starting the request */      
        WAIT_REGISTER32_CHANGE(pHcInstance, 
                               &status, 
                               HOST_REG_PRESENT_STATE,
                               HOST_REG_PRESENT_STATE_CARD_STATE_STABLE,
                               HOST_REG_PRESENT_STATE_CARD_STATE_STABLE,
                               pHcInstance->PresentStateWaitLimit);
                               
        if (!SDIO_SUCCESS(status)) {
            /* card detect could not stabilize, card might be ejecting */
            status = SDIO_STATUS_CANCELED;
            break;  
        }   
                             
        if (!(READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE) &  
                HOST_REG_PRESENT_STATE_CARD_INSERTED)) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST Card Removed! \n"));
            status = SDIO_STATUS_CANCELED;
            break; 
        }    
            
        /* make any clock adjustments or bus adjustments before we turn on the clock */
        IdleBusAdjustment(pHcInstance, ADJUST_BUS_NORMAL);
        
        pHcInstance->IntStatusEn |= HOST_REG_INT_STATUS_CMD_COMPLETE_ENABLE | HOST_REG_INT_STATUS_TRANSFER_COMPLETE_ENABLE;
        pHcInstance->IntSignalEn |= HOST_REG_INT_STATUS_CMD_COMPLETE_ENABLE | HOST_REG_INT_STATUS_TRANSFER_COMPLETE_ENABLE;
        
        WAIT_FOR_DAT_CMD_DAT_READY(pHcInstance, &status);
    
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 Timeout waiting for CMD Inhibit \n"));
            ResetCmdDatLine(pHcInstance); 
            break; 
        }
        
        /* clear any error statuses */
        temp = 10000;
        do {
            WRITE_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS, HOST_REG_ERROR_INT_STATUS_ALL_ERR);
            if( ! --temp ) break;
        } while ( READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS) & HOST_REG_ERROR_INT_STATUS_ALL_ERR );
        
        temp = 10000;
        do {
            WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_CLEAR_ALL);
            if( ! --temp ) break;
        } while ( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & 
                                               HOST_REG_NORMAL_INT_STATUS_CLEAR_ALL & 
                                               ~(HOST_REG_NORMAL_INT_STATUS_CARD_INTERRUPT) );
        
        if( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & ~(HOST_REG_NORMAL_INT_STATUS_CARD_INTERRUPT) )
          printk( KERN_ERR "Failed to clear Normal Int Status register: 0x%x\n", 
                READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) );
    
            /* set the argument register */
        if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS){
            DBG_PRINT(STD_HOST_TRACE_DATA, ("SDIO S3C6400 HOST HcdRequest %s Data Transfer, Blocks:%d, BlockLen:%d \n",
                      IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                      pReq->BlockCount, pReq->BlockLen));
            
            if (pReq->Command == CMD8)
              WRITE_HOST_REG8(pHcInstance, HOST_REG_TIMEOUT_CONTROL, 0x3);
            else
              WRITE_HOST_REG8(pHcInstance, HOST_REG_TIMEOUT_CONTROL, 0xE);
              
                /* set flag in command register */
            cmd_reg |= HOST_REG_COMMAND_REGISTER_DATA_PRESENT; 
            	
            pReq->DataRemaining = pReq->BlockLen * pReq->BlockCount;
            
#if 1
            if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) { /* setup DMA , note, for SDMA, this routine could modify HOST_REG_BLOCK_SIZE*/
#else
            if ( 1 ) { /* do dma only */
#endif
                pReq->Flags |= SDREQ_FLAGS_DATA_DMA;
                status = SetUpHCDDMA(pHcInstance, pReq);                  
                if (!SDIO_SUCCESS(status)) {
                    break;  
                }                 
            } else {                    
                pReq->Flags &=~ SDREQ_FLAGS_DATA_DMA;
                    /* use the context to hold where we are in the buffer */
                pReq->pHcdContext = pReq->pDataBuffer;   
                if (IS_SDREQ_WRITE_DATA(pReq->Flags)) 
                  pHcInstance->IntStatusEn |= HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY;
                else
                  pHcInstance->IntStatusEn |= HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY;
            }
            
            if( ! ((pReq->Flags & SDREQ_FLAGS_DATA_DMA) && (!IS_HCD_ADMA(pHcInstance))) ) {
                
                UINT16 foo = (0x7<<12) | pReq->BlockLen;
                /* set the block size register */
                WRITE_HOST_REG16(pHcInstance, HOST_REG_BLOCK_SIZE, foo);
            }
            
                /* set block count register */
            WRITE_HOST_REG16(pHcInstance, HOST_REG_BLOCK_COUNT, pReq->BlockCount);

                /* set transfer mode register */
            WRITE_HOST_REG16(pHcInstance, HOST_REG_TRANSFER_MODE, 
                    ((pReq->BlockCount >= 1) ? HOST_REG_TRANSFER_MODE_MULTI_BLOCK:0) |
                    ((pReq->BlockCount >= 1) ? HOST_REG_TRANSFER_MODE_BLOCKCOUNT_ENABLE:0) |
                    ((pReq->Flags & SDREQ_FLAGS_AUTO_CMD12) ? HOST_REG_TRANSFER_MODE_AUTOCMD12 : 0) |
                    ((IS_SDREQ_WRITE_DATA(pReq->Flags))? 0 : HOST_REG_TRANSFER_MODE_READ) |
                    ((pReq->Flags & SDREQ_FLAGS_DATA_DMA)? HOST_REG_TRANSFER_MODE_DMA_ENABLE : 0));
        }  
        else
        {
            if (pReq->Command == CMD12)
                cmd_reg |= CMD_TYPE_ABORT;
            else if (pReq->Command == CMD52) {
                if( !(((pReq->Argument)>>28) & 0x7) ) {
                    if( (((pReq->Argument)>>9) & 0x1FFFF) == 6 )
                    {
                        printk( KERN_ERR "CMD52 corr\n");
                        cmd_reg |=  CMD_TYPE_ABORT | HOST_REG_COMMAND_REGISTER_CRC_CHECK_ENABLE
                                                       | HOST_REG_COMMAND_REGISTER_CMD_INDEX_CHECK_ENABLE;
                    }
                }
            }
            if (pReq->Command == 23)
                cmd_reg |= HOST_REG_COMMAND_REGISTER_CMD_INDEX_CHECK_ENABLE |
                           HOST_REG_COMMAND_REGISTER_CRC_CHECK_ENABLE |
                           0x02;
        }
   
        /* set the argument register */
        WRITE_HOST_REG32(pHcInstance, HOST_REG_ARGUMENT, pReq->Argument);
       
        /* set command register, make sure it is clear to write */
        cmd_reg |= (pReq->Command << HOST_REG_COMMAND_REGISTER_CMD_SHIFT);
        DBG_PRINT(STD_HOST_TRACE_REQUESTS, ("SDIO S3C6400 HOST HcdRequest - CMDDAT:0x%X (RespType:%d, Command:0x%X , Arg:0x%X) \n",
                  cmd_reg, GET_SDREQ_RESP_TYPE(pReq->Flags), pReq->Command, pReq->Argument));
        
            /* enable error status */
        WRITE_HOST_REG16(pHcInstance, HOST_REG_ERR_STATUS_ENABLE, HOST_REG_ERROR_INT_STATUS_ALL_ERR);
                  
#if 1
        if (SDHCD_GET_OPER_CLOCK(pHcd) < pHcInstance->ClockSpinLimit) { 
                /* clock rate is very low, need to use interrupts here */
#else
        if( 0 ){
#endif
                
                /* enable error interrupts */  
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE, 
                    HOST_REG_ERROR_INT_STATUS_ALL_ERR);
            
                /* enable command complete IRQ */           
            UnmaskIrq(pHcInstance, HOST_REG_INT_STATUS_CMD_COMPLETE_ENABLE,FALSE); 
            
                /* enable error signal - hit it again */
            WRITE_HOST_REG16(pHcInstance, 
                             HOST_REG_INT_ERR_SIGNAL_ENABLE, 
                             pHcInstance->IntSignalEn); 
                             
                /* enable error status - hit it again */
            WRITE_HOST_REG16(pHcInstance, 
                             HOST_REG_ERR_STATUS_ENABLE, 
                             pHcInstance->IntStatusEn);
            
            /* start the clock */
            ClockStartStop(pHcInstance, CLOCK_ON);

            DBG_PRINT(STD_HOST_TRACE_REQUESTS, ("SDIO S3C6400 HOST HcdRequest using interrupt for command done (%s). (clock:%d, ref:%d)\n",
                    (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) ? "command only" : "with data",
                    SDHCD_GET_OPER_CLOCK(pHcd),
                    pHcInstance->ClockSpinLimit));    
#if 0
/* ULF dbg */
            dbg[dbgidx].mode = 1;      /* interrupt mode */
            dbg[dbgidx].sysaddr =       READ_HOST_REG32(pHcInstance, HOST_REG_SYSTEM_ADDRESS);
            dbg[dbgidx].blksize =       READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].blkcount =      READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].argument =      READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].trnmode =       READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].command =       READ_HOST_REG16(pHcInstance, HOST_REG_COMMAND_REGISTER); 
            dbg[dbgidx].resp0 =         READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].resp1 =         READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].resp2 =         READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].resp3 =         READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].present_state = READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].host_control = READ_HOST_REG8(pHcInstance, HOST_REG_
            dbg[dbgidx].pwr_control =     READ_HOST_REG8(pHcInstance, HOST_REG_
            dbg[dbgidx].blk_gap_control = READ_HOST_REG8(pHcInstance, HOST_REG_
            dbg[dbgidx].wakeup_control =  READ_HOST_REG8(pHcInstance, HOST_REG_
            dbg[dbgidx].clock_control =   READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].timeout_control = READ_HOST_REG8(pHcInstance, HOST_REG_
            dbg[dbgidx].sw_reset =        READ_HOST_REG8(pHcInstance, HOST_REG_
            dbg[dbgidx].norm_int_status = READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].errr_int_status = READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].norm_int_status_enable = READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].errr_int_status_enable = READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].norm_int_signal_enable = READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].errr_int_signal_enable = READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].autocmd12_error_status = READ_HOST_REG16(pHcInstance, HOST_REG_
            dbg[dbgidx].capabilities =           READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].max_current_caps =       READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].ctrl2 =                  READ_HOST_REG32(pHcInstance, HOST_REG_
            dbg[dbgidx].ctrl3 =                  READ_HOST_REG32(pHcInstance, HOST_REG_
/* ! ULF dbg */                                            
#endif
                /* start the command */        
            WRITE_HOST_REG16(pHcInstance, HOST_REG_COMMAND_REGISTER, cmd_reg); 
            status = SDIO_STATUS_PENDING;
            break;            
        }
        
            /* if we get here we are doing this inline using polling */
                                 
            /* enable error status - hit it again */
        WRITE_HOST_REG16(pHcInstance, 
                         HOST_REG_ERR_STATUS_ENABLE, 
                         pHcInstance->IntStatusEn);
        
            /* enable error signal - hit it again */
        WRITE_HOST_REG16(pHcInstance, 
                         HOST_REG_INT_ERR_SIGNAL_ENABLE, 
                         pHcInstance->IntSignalEn & ~( HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE |
                                                       HOST_REG_NORMAL_INT_STATUS_ERROR |
                                                       HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY |
                                                       HOST_REG_NORMAL_INT_STATUS_BLOCK_GAP )
                        ); 
        
        /* start the clock */
        ClockStartStop(pHcInstance, CLOCK_ON);
                             
            /* start command */
        WRITE_HOST_REG16(pHcInstance, HOST_REG_COMMAND_REGISTER, cmd_reg);
       
            /* wait for command to finish */
        if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) {
            WAIT_REGISTER32_CHANGE(pHcInstance,            
                                   &status,            
                                   HOST_REG_PRESENT_STATE,
                                   HOST_REG_PRESENT_STATE_BUFFER_COMMAND_INHIBIT_CMD,
                                   0, pHcInstance->PresentStateWaitLimit);
        } else  {
            WAIT_FOR_DAT_CMD_DAT_READY(pHcInstance, &status);            
        }
 
        if (!SDIO_SUCCESS(status)) {
            ResetCmdDatLine(pHcInstance); 
            break;   
        }
                
        temp = READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
        
        if( temp & HOST_REG_NORMAL_INT_STATUS_ERROR ) {
        
            UINT16 temp2;
            /* get errors */
            temp2 = READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS);
            
            if (temp2 != 0) {
                status = TranslateSDError(pHcInstance, temp2);
                    /* clear any existing errors - non-synchronized clear */
                WRITE_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS, HOST_REG_ERROR_INT_STATUS_ALL_ERR);        
                WRITE_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS, HOST_REG_ERROR_INT_STATUS_ALL_ERR);        
                    /* reset command dat , just in case */
                ResetCmdDatLine(pHcInstance);    
                break;           
            } 
        }
        if (temp & HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE ) {
            do {
                WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE);
            } while ( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE );
        }
                
            /* process the command */    
        status = ProcessCommandDone(pHcInstance, pReq, FALSE);
        
    } while (FALSE);
   
    if (status != SDIO_STATUS_PENDING) {  
        if (!pHcInstance->KeepClockOn) {   
            ClockStartStop(pHcInstance, CLOCK_OFF);
        } else {
                /* clock has to be left on for interrupts, adjust clock rate and any additional settings 
                 * for an idle bus  */
            IdleBusAdjustment(pHcInstance, ADJUST_BUS_IDLE);
        }
        
        pReq->Status = status;  
            /* cleanup DMA */
        if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) {
              
              UINT16 temp2;
              
                /* cleanup DMA if it was setup */
                temp2 = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp2 & ~HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                temp2 = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp2 & ~HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                temp2 = 10000;
                do {
                    WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                    READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
                    mmiowb();
                    if( ! --temp2 ) break;
                } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & HOST_REG_NORMAL_INT_STATUS_DMA_INT); 
                temp2 = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp2 | HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                temp2 = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp2 | HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                
            HcdTransferDataDMAEnd(pHcInstance,pReq);
        }                  
        if (IS_SDREQ_FORCE_DEFERRED_COMPLETE(pReq->Flags)) {
            DBG_PRINT(STD_HOST_TRACE_REQUESTS, ("SDIO S3C6400 HOST deferring completion to work item \n"));
                /* the HCD must do the indication in a separate context and return status pending */
            QueueEventResponse(pHcInstance, WORK_ITEM_IO_COMPLETE); 
                /* return pending */
            status = SDIO_STATUS_PENDING;
        } else {        
                /* complete the request */
            DBG_PRINT(STD_HOST_TRACE_REQUESTS, ("SDIO S3C6400 HOST Command Done, status:%d \n", status));
        }
        pHcInstance->Cancel = FALSE;  
    } else {
        DBG_PRINT(STD_HOST_TRACE_REQUESTS, ("SDIO S3C6400 HOST Bus Request Pending.... \n"));
    }    

        /* now allow removal again */
    UnmaskIrq(pHcInstance, HOST_REG_INT_STATUS_ALLOW_INSERT_REMOVE_ONLY,FALSE); 
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdRequest Exit\n"));
    return status;
} 


SDIO_STATUS ProcessCommandDone(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq, BOOL FromIsr)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    BOOL        shortTransfer = FALSE;
    UINT16      errors = 0;
    
    /* Can get here when command is not done actually. So wait for it */
    while( READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE) & 0x1 );


    do {       
        running = 0;
           
        if (pHcInstance->Cancel) {
            status = SDIO_STATUS_CANCELED;   
            break;
        }
                
            /* get the response data for the command */
        GetResponseData(pHcInstance, pReq);
        
            /* check for data */
        if (!(pReq->Flags & SDREQ_FLAGS_DATA_TRANS)) {
            /* no data phase, we're done */
            status = SDIO_STATUS_SUCCESS;
            break;
        }                   
            /* check with the bus driver if it is okay to continue with data */
        status = SDIO_CheckResponse(&pHcInstance->Hcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST : Response for Data transfer error :%d n",status));
            break;   
        }

            /* check for short transfer */
        if ((pReq->Flags & SDREQ_FLAGS_DATA_SHORT_TRANSFER) &&
            (pReq->DataRemaining <= STD_HOST_SHORT_TRANSFER_THRESHOLD) &&
            (SDHCD_GET_OPER_CLOCK(&pHcInstance->Hcd) >= pHcInstance->ClockSpinLimit)) {
                /* we will do a short transfer */
            shortTransfer = TRUE;              
            break;
        }
            
            /* normal data transfers involve interrupts */  
        status = SDIO_STATUS_PENDING; 
            /* re-enable all errors for data transfers  */
        WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE, 
                HOST_REG_ERROR_INT_STATUS_ALL_ERR);
        if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) {
                /* handle DMA case */
                /* expecting interrupt */  
            UnmaskIrq(pHcInstance, HOST_REG_INT_STATUS_TRANSFER_COMPLETE_ENABLE,FromIsr); 
            DBG_PRINT(STD_HOST_TRACE_DATA, ("SDIO S3C6400 HOST Pending DMA %s transfer \n",
                                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
            break;
        } 

        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            UINT16 ints;
                /* write data, see if the buffer is ready, it should be */
            ints = READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);            
            if (ints & HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY) {
                /* acknowledge it */
                do {
                WRITE_HOST_REG16(pHcInstance, 
                                HOST_REG_NORMAL_INT_STATUS, 
                                HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);                
                } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & 
                                                    HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);
                /* send the initial buffer */
                /* transfer data */
                if (HcdTransferTxData(pHcInstance, pReq)) {
                        /* data fits in buffer */
                        /* wait for transfer complete */
                    UnmaskIrqFromIsr(pHcInstance, 
                                 HOST_REG_INT_STATUS_TRANSFER_COMPLETE_ENABLE);          
                    break;
                }
                
                /* fall through and enable write buffer interrupts */
            }
            
                /* expecting write buffer ready interrupt */  
            UnmaskIrq(pHcInstance, 
                     HOST_REG_INT_STATUS_BUFFER_WRITE_RDY_ENABLE,
                     FromIsr);    
            
        } else {
                /* expecting read buffer ready data */
            UnmaskIrq(pHcInstance, 
                     HOST_REG_INT_STATUS_BUFFER_READ_RDY_ENABLE,
                     FromIsr);    
        }        
           
        DBG_PRINT(STD_HOST_TRACE_DATA, ("SDIO S3C6400 HOST Pending %s transfer \n",
                                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
        
    } while (FALSE); 
   
        /* check short transfer */
    while (shortTransfer) {
        DBG_PRINT(STD_HOST_TRACE_DATA, ("SDIO STD Using Short Transfer (%d bytes) %s \n",
            pReq->DataRemaining, IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
        
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
	           /* wait for buffer ready */
            WAIT_REGISTER32_CHANGE(pHcInstance,            
                                   &status,            
                                   HOST_REG_NORMAL_INT_STATUS,
                                   HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY,
                                   HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY, 
                                   pHcInstance->BufferReadyWaitLimit);
            
            if (!SDIO_SUCCESS(status)) {
                break;    
            }                       
            /* acknowledge it */

            do {
              WRITE_HOST_REG16(pHcInstance, 
                             HOST_REG_NORMAL_INT_STATUS, 
                             HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);  
            } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & 
                                                HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);

                /* transfer data */
            HcdTransferTxData(pHcInstance, pReq);
            DBG_ASSERT(pReq->DataRemaining == 0);          
                
                /* fall through for completion */              
        } else { 
            
                /* wait for read buffer ready */
            WAIT_REGISTER32_CHANGE_OR(pHcInstance,            
                                      &status,            
                                      HOST_REG_NORMAL_INT_STATUS,
                                      HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY |
                                          HOST_REG_NORMAL_INT_STATUS_ERROR,
                                      HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY |
                                          HOST_REG_NORMAL_INT_STATUS_ERROR,
                                      pHcInstance->BufferReadyWaitLimit);
            
            if (!SDIO_SUCCESS(status)) {
                break;    
            }
            
            errors = READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS);
            
            if (errors != 0) {
                break;    
            }                     
            /* acknowledge it */
            do {
              WRITE_HOST_REG16(pHcInstance, 
                             HOST_REG_NORMAL_INT_STATUS, 
                             HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY);   
            } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & 
                                                HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY);
                                      
                /* unload buffer */
            HcdTransferRxData(pHcInstance, pReq);
            DBG_ASSERT(pReq->DataRemaining == 0);
              
            /* fall through for completion */ 
        }  
        
            /* wait for transfer complete */
        WAIT_REGISTER32_CHANGE_OR(pHcInstance,            
                                  &status,            
                                  HOST_REG_NORMAL_INT_STATUS,
                                  HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE |
                                      HOST_REG_NORMAL_INT_STATUS_ERROR,
                                  HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE |
                                      HOST_REG_NORMAL_INT_STATUS_ERROR, 
                                  pHcInstance->TransferCompleteWaitLimit);
                               
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
            /* get final error status */    
        errors = READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS);
               
        break;
    }
    
    if (errors != 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO STD Using Short Transfer Errors! :0x%X \n",errors));
        status = TranslateSDError(pHcInstance, errors);
        WRITE_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS, HOST_REG_ERROR_INT_STATUS_ALL_ERR);        
    } 
    
    if (!SDIO_SUCCESS(status)) {
        ResetCmdDatLine(pHcInstance);      
    }
    
    return status;    
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdConfig - HCD configuration handler
  Input:  pHcd - HCD object
          pConfig - configuration setting
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pConfig) 
{
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext; 
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT16      command;
    UINT32 temp;
    
    if(pHcInstance->ShuttingDown) {
        DBG_PRINT(STD_HOST_TRACE_REQUESTS, ("SDIO S3C6400 HOST HcdConfig returning canceled\n"));
        return SDIO_STATUS_CANCELED;
    }

    command = GET_SDCONFIG_CMD(pConfig);
        
    switch (command){
        case SDCONFIG_GET_WP:
            /* get write protect */
            temp = READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE);
            /* if write enabled, set WP value to zero */
            *((SDCONFIG_WP_VALUE *)pConfig->pData) = 
                    (temp & HOST_REG_PRESENT_STATE_WRITE_ENABLED )? 0 : 1;
            break;
        case SDCONFIG_SEND_INIT_CLOCKS:
            ClockStartStop(pHcInstance,CLOCK_ON);
                /* should be at least 80 clocks at our lowest clock setting */
            status = OSSleep(100);
            ClockStartStop(pHcInstance,CLOCK_OFF);          
            break;
        case SDCONFIG_SDIO_INT_CTRL:
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {
                {
                    SDIO_IRQ_MODE_FLAGS irqModeFlags;
                    UINT8               blockGapControl;
                    
                    irqModeFlags = GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->IRQDetectMode;
                    if (irqModeFlags & IRQ_DETECT_4_BIT) {
                        DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST: 4 Bit IRQ mode \r\n")); 
                            /* in 4 bit mode, the clock needs to be left on */
                        pHcInstance->KeepClockOn = TRUE;
                        blockGapControl = READ_HOST_REG8(pHcInstance,HOST_REG_BLOCK_GAP);
                        if (irqModeFlags & IRQ_DETECT_MULTI_BLK) {
                            blockGapControl |= HOST_REG_INT_DETECT_AT_BLOCK_GAP;
                            DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST: 4 Bit Multi-block IRQ detection enabled \r\n")); 
                        } else {
                                // no interrupts between blocks
                            blockGapControl &= ~HOST_REG_INT_DETECT_AT_BLOCK_GAP;   
                        }  
                        WRITE_HOST_REG8(pHcInstance,HOST_REG_BLOCK_GAP,blockGapControl);                         
                    } else {
                            /* in 1 bit mode, the clock can be left off */
                        pHcInstance->KeepClockOn = FALSE;   
                    }                   
                }
                    /* enable detection */
                EnableDisableSDIOIRQ(pHcInstance,TRUE,FALSE); 
            } else {
                pHcInstance->KeepClockOn = FALSE; 
                EnableDisableSDIOIRQ(pHcInstance,FALSE,FALSE);
            }
            break;
        case SDCONFIG_SDIO_REARM_INT:
                /* re-enable IRQ detection */
            EnableDisableSDIOIRQ(pHcInstance,TRUE,FALSE);
            break;
        case SDCONFIG_BUS_MODE_CTRL:
            SetBusMode(pHcInstance, (PSDCONFIG_BUS_MODE_DATA)(pConfig->pData));
            break;
        case SDCONFIG_POWER_CTRL:
            DBG_PRINT(STD_HOST_TRACE_CONFIG, ("SDIO S3C6400 HOST PwrControl: En:%d, VCC:0x%X \n",
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask));
            status = SetPowerLevel(pHcInstance, 
                     GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                     GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask);
            break;
        case SDCONFIG_GET_HCD_DEBUG:
            *((CT_DEBUG_LEVEL *)pConfig->pData) = DBG_GET_DEBUG_LEVEL();
            break;
        case SDCONFIG_SET_HCD_DEBUG:
            DBG_SET_DEBUG_LEVEL(*((CT_DEBUG_LEVEL *)pConfig->pData));
            break;
        default:
            /* invalid request */
            DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST Local HCD: HcdConfig - bad command: 0x%X\n",
                                    command));
            status = SDIO_STATUS_INVALID_PARAMETER;
    }
    
    return status;
} 


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  SetPowerLevel - Set power level of board
  Input:  pHcInstance - device context
          On - if true turns power on, else off
          Level - SLOT_VOLTAGE_MASK level
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS SetPowerLevel(PSDHCD_INSTANCE pHcInstance, BOOL On, SLOT_VOLTAGE_MASK Level) 
{
    UINT8 out, old;
    UINT32 capCurrent;
    
    capCurrent = READ_HOST_REG32(pHcInstance, HOST_REG_MAX_CURRENT_CAPABILITIES);
       
    switch (Level) {
      case SLOT_POWER_3_3V:
        out = HOST_REG_POWER_CONTROL_VOLT_3_3;
            /* extract */
        capCurrent = (capCurrent & HOST_REG_MAX_CURRENT_CAPABILITIES_3_3_MASK) >>
                        HOST_REG_MAX_CURRENT_CAPABILITIES_3_3_SHIFT;
        break;
      case SLOT_POWER_3_0V:
        out = HOST_REG_POWER_CONTROL_VOLT_3_0;            
            /* extract */
        capCurrent = (capCurrent & HOST_REG_MAX_CURRENT_CAPABILITIES_3_0_MASK) >>
                        HOST_REG_MAX_CURRENT_CAPABILITIES_3_0_SHIFT;
        break;
      case SLOT_POWER_1_8V:
        out = HOST_REG_POWER_CONTROL_VOLT_1_8;            
            /* extract */
        capCurrent = (capCurrent & HOST_REG_MAX_CURRENT_CAPABILITIES_1_8_MASK) >>
                        HOST_REG_MAX_CURRENT_CAPABILITIES_1_8_SHIFT;       
        break;
      default:
        DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST SetPowerLevel - illegal power level %d\n",
                                (UINT)Level));
        return SDIO_STATUS_INVALID_PARAMETER;                        
    }
    
    if (capCurrent != 0) {
            /* convert to mA and set max current */
        pHcInstance->Hcd.MaxSlotCurrent = capCurrent * HOST_REG_MAX_CURRENT_CAPABILITIES_SCALER;
    } else {
        DBG_PRINT(SDDBG_WARN, ("SDIO S3C6400 HOST No Current Caps value for VMask:0x%X, using 400mA \n",
                  Level));  
            /* set a value */          
        pHcInstance->Hcd.MaxSlotCurrent = 400;  
    }
        
    if (On) {
        out |= HOST_REG_POWER_CONTROL_ON;
    }
    old = READ_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL);
    if( old != out ) {
        DBG_PRINT(SDDBG_WARN, ("SDIO S3C6400 HOST Setting power control register at 0x%x\n", out));
        WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, 0);
        
        WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, out);
        WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, out);
    }
    mdelay(1);
    return SDIO_STATUS_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  SetPowerOn - Set power on or off for card
  Input:  pHcInstance - device context
          On - if true turns power on, else off
  Output: 
  Return: 
  Notes: leavse the level alone
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void SetPowerOn(PSDHCD_INSTANCE pHcInstance, BOOL On) 
{
    /* non-synchronized read modify write */
    UINT8 out = READ_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL);
    WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, 0);
    if (On) {
        out |= HOST_REG_POWER_CONTROL_ON;
    } else {
        out &= ~HOST_REG_POWER_CONTROL_ON;
    }
    WRITE_HOST_REG8(pHcInstance, HOST_REG_POWER_CONTROL, out);
    mdelay(1);
    return;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdInitialize - Initialize MMC controller
  Input:  pHcInstance - device context
  Output: 
  Return: 
  Notes: I/O resources must be mapped before calling this function
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdInitialize(PSDHCD_INSTANCE pHcInstance) 
{
    UINT32 caps;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT32 clockValue;
    PTEXT  pSpecVer;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO S3C6400 HOST HcdInitialize\n"));
    
    if (0 == pHcInstance->BufferReadyWaitLimit) {
            /* initialize all these to defaults */
	    pHcInstance->BufferReadyWaitLimit = 50000;
	    pHcInstance->TransferCompleteWaitLimit = 100000;
	    pHcInstance->PresentStateWaitLimit = 30000;
	    pHcInstance->ResetWaitLimit = 30000;
    }

        /* reset the device */
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdInitialize, resetting\n"));
    WRITE_HOST_REG8(pHcInstance, HOST_REG_SW_RESET, HOST_REG_SW_RESET_ALL);
        /* wait for done */
    while(READ_HOST_REG8(pHcInstance, HOST_REG_SW_RESET) &  HOST_REG_SW_RESET_ALL)
        ;
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdInitialize, reset\n"));
            
        /* turn off clock */
    ClockStartStop(pHcInstance, CLOCK_OFF);
        /* display version info */
    switch( READ_HOST_REG16(pHcInstance, HOST_REG_VERSION) & (UINT16)HOST_REG_VERSION_SPEC_VERSION_MASK) {
      case 0:
        pSpecVer = "SD Host Spec. 1.0";
        break;
      case 1:
        pSpecVer = "SD Host Spec. 2.0";
        break;
      default:
        pSpecVer = "SD Host Spec. **UNKNOWN**";
    }    
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdInitialize: Spec version: %s, Vendor version: %d\n",
              pSpecVer,
        (READ_HOST_REG16(pHcInstance, HOST_REG_VERSION) & HOST_REG_VERSION_VENDOR_VERSION_MASK) >> 
        HOST_REG_VERSION_VENDOR_VERSION_SHIFT) );
        
        /* get capabilities */
    caps = READ_HOST_REG32(pHcInstance, HOST_REG_CAPABILITIES);
        /* save these */
    pHcInstance->Caps = caps;
       
    switch((caps & HOST_REG_CAPABILITIES_MAX_BLOCK_LEN_MASK) >> HOST_REG_CAPABILITIES_MAX_BLOCK_LEN_SHIFT) {
        case 0x00:
            pHcInstance->Hcd.MaxBytesPerBlock = 512;
            break;
        case 0x01:
            pHcInstance->Hcd.MaxBytesPerBlock = 1024;
            break;
        case 0x02:
            pHcInstance->Hcd.MaxBytesPerBlock = 2048;
            break;
        case 0x03:
            pHcInstance->Hcd.MaxBytesPerBlock = 512;
            DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST invalid buffer length\n"));
            status = SDIO_STATUS_DEVICE_ERROR;
            break;
    }
    
    clockValue = (caps & HOST_REG_CAPABILITIES_CLOCK_MASK) >> HOST_REG_CAPABILITIES_CLOCK_SHIFT;
    if (clockValue != 0) {
            /* convert to Hz */
        pHcInstance->BaseClock = clockValue*1000*1000;   
    } else {
        DBG_PRINT(SDDBG_WARN, ("SDIO S3C6400 HOST base clock is zero! (caps:0x%X) \n",caps));
            /* fall through and see if a default was setup */   
    }                                               
    if (pHcInstance->BaseClock == 0) {
         DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST invalid base clock setting\n"));
         status = SDIO_STATUS_DEVICE_ERROR;
         return status;
    }
            
    pHcInstance->Hcd.MaxClockRate =  pHcInstance->BaseClock;
    DBG_PRINT(SDDBG_WARN, ("SDIO S3C6400 HOST Using clock %dHz, max. block %d, high speed %s, %s, %s, %s\n",
                            pHcInstance->BaseClock, pHcInstance->Hcd.MaxBytesPerBlock, 
                            (caps & HOST_REG_CAPABILITIES_HIGH_SPEED)? "supported" : "not supported",
                            (caps & HOST_REG_CAPABILITIES_DMA)? "Std. DMA" : "",
                            (caps & HOST_REG_CAPABILITIES_ADMA)? "Adv. DMA" : "",
                            (caps & HOST_REG_CAPABILITIES_MMC8)? "MMC8bit" : ""));
                            
    /* setup the supported voltages and max current */
    pHcInstance->Hcd.SlotVoltageCaps = 0;
    /* max current is dynamically set based on the desired voltage, see SetPowerLevel() */
    pHcInstance->Hcd.MaxSlotCurrent = 0;

    if (caps & HOST_REG_CAPABILITIES_VOLT_1_8) {
        pHcInstance->Hcd.SlotVoltageCaps |= SLOT_POWER_1_8V;
        pHcInstance->Hcd.SlotVoltagePreferred = SLOT_POWER_1_8V;
    } 
    if(caps & HOST_REG_CAPABILITIES_VOLT_3_0) {
        pHcInstance->Hcd.SlotVoltageCaps |= SLOT_POWER_3_0V;
        pHcInstance->Hcd.SlotVoltagePreferred = SLOT_POWER_3_0V;
    }
    if(caps & HOST_REG_CAPABILITIES_VOLT_3_3) {
        pHcInstance->Hcd.SlotVoltageCaps |= SLOT_POWER_3_3V;
        pHcInstance->Hcd.SlotVoltagePreferred = SLOT_POWER_3_3V;
    }
    
         /* check host capabilities and back off some features */
        pHcInstance->Hcd.Attributes &= ~SDHCD_ATTRIB_SD_HIGH_SPEED;    
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdInitialize: caps: 0x%X, SlotVoltageCaps: 0x%X\n",
                        (UINT)caps, (UINT)pHcInstance->Hcd.SlotVoltageCaps));
              
        /* set the default timeout */
    WRITE_HOST_REG8(pHcInstance, HOST_REG_TIMEOUT_CONTROL, (pHcInstance->TimeOut = 0xE));

    /* clear any existing errors and status */
    WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_CLEAR_ALL);
    WRITE_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS, HOST_REG_ERROR_INT_STATUS_ALL_ERR);
    /* enable error interrupts */
    WRITE_HOST_REG16(pHcInstance, HOST_REG_ERR_STATUS_ENABLE, HOST_REG_ERROR_INT_STATUS_ALL_ERR);
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdInitialize - error enable 16bit: 0x%X\n", READ_HOST_REG16(pHcInstance, HOST_REG_ERR_STATUS_ENABLE)));
  
    WRITE_HOST_REG32(pHcInstance, HOST_REG_ERR_STATUS_ENABLE, HOST_REG_ERROR_INT_STATUS_ALL_ERR);
    DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST HcdInitialize - error enable 32 bit: 0x%X\n", READ_HOST_REG32(pHcInstance, HOST_REG_ERR_STATUS_ENABLE)));
  
    /* leave disabled for now */
    WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE, (UINT16)0);
                
    /* enable statuses */
    WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, HOST_REG_INT_STATUS_ALL);
        
    if (pHcInstance->IdleBusClockRate != 0) {
        int    clockIndex;
       
        /* user wants idle bus clock control,
         * get the idle bus clock control value for reducing the clock rate when the bus
         * is idle in 4-bit mode */
         
        clockIndex = GetClockTableIndex(pHcInstance, pHcInstance->IdleBusClockRate, &clockValue);
        pHcInstance->ClockConfigIdle = SDClockDivisorTable[clockIndex].RegisterValue;
    
        DBG_PRINT(SDDBG_TRACE, ("SDIO S3C6400 HOST Idle Bus Clock Rate: %d Actual Rate: %d Control:0x%2.2X\n",
            pHcInstance->IdleBusClockRate, clockValue, pHcInstance->ClockConfigIdle));
    }
    
    
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO S3C6400 HOST HcdInitialize\n"));
    return status;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdDeinitialize - deactivate controller
  Input:  pHcInstance - context
  Output: 
  Return: 
  Notes:
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void HcdDeinitialize(PSDHCD_INSTANCE pHcInstance)
{
    DBG_PRINT(SDDBG_TRACE, ("+SDIO S3C6400 HOST HcdDeinitialize\n"));
    pHcInstance->KeepClockOn = FALSE;
    MaskIrq(pHcInstance, HOST_REG_INT_STATUS_ALL,FALSE);
    pHcInstance->ShuttingDown = TRUE;
    /* disable error interrupts */
    /* clear any existing errors */
    WRITE_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS, HOST_REG_ERROR_INT_STATUS_ALL_ERR);
    /* disable error interrupts */
    WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE, 0);
    WRITE_HOST_REG16(pHcInstance, HOST_REG_ERR_STATUS_ENABLE, 0);
    ClockStartStop(pHcInstance, CLOCK_OFF);
    SetPowerOn(pHcInstance, FALSE);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO S3C6400 HOST HcdDeinitialize\n"));
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdSDInterrupt - process controller interrupt
  Input:  pHcInstance - context
  Output: 
  Return: TRUE if interrupt was handled
  Notes:
               
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL HcdSDInterrupt(PSDHCD_INSTANCE pHcInstance) 
{
    UINT16              ints;
    UINT16              errors;
    UINT16 		enables;
    UINT16 		statenables;
    UINT16 		errorenables;
    int                 temp;
    PSDREQUEST          pReq;
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    
    DBG_PRINT(STD_HOST_TRACE_INT, ("+SDIO S3C6400 HOST HcdSDInterrupt Int handler \n"));
    
    pReq = GET_CURRENT_REQUEST(&pHcInstance->Hcd);
     
    while (1) { 
        /* s3c6400 might update NORMAL_INT_STATUS with delay */
        while( !(ints = READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) ) );
        
        errors = READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS);
    	enables = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
        statenables = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
        errorenables = READ_HOST_REG16(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE);

        DBG_PRINT(STD_HOST_TRACE_INT, 
        	("SDIO S3C6400 HOST HcdSDInterrupt, ints: 0x%X errors: 0x%x, sigenables: 0x%X, statenable: 0x%X errorenables:0x%X\n", 
                (UINT)ints, (UINT)errors, (UINT)enables, (UINT)statenables, (UINT)errorenables));
        
        	/* only look at ints and error ints that are enabled */
        ints &= enables;
        errors &= errorenables;
        
        if ((ints == 0) && (errors == 0)) {
            break;  
        }
        
        /* clear any error statuses */
        temp = 10000;
        do {
          WRITE_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS, errors);
          if( ! --temp ) break;
        } while( READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS) & errors );
              
        if (ints & HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE) {
            DBG_PRINT(STD_HOST_TRACE_INT, ("SDIO S3C6400 HOST HcdSDInterrupt clearing possible data timeout errors: 0x%X \n",
                                          errors));
            errors &= ~HOST_REG_ERROR_INT_STATUS_DATATIMEOUTERR;
        }         
        
        if ( (ints & HOST_REG_NORMAL_INT_STATUS_ERROR) && (errors != 0) ) {
            status = TranslateSDError(pHcInstance, errors);  
            break;            
        }
        
            /* handle insert/removal */
        if (ints & 
            (HOST_REG_INT_STATUS_CARD_INSERT_ENABLE | HOST_REG_INT_STATUS_CARD_REMOVAL_ENABLE)){
                /* card was inserted or removed, clear interrupt */

            WRITE_HOST_REG16(pHcInstance, 
                             HOST_REG_NORMAL_INT_STATUS, 
                             HOST_REG_INT_STATUS_CARD_INSERT_ENABLE | 
                             HOST_REG_INT_STATUS_CARD_REMOVAL_ENABLE);   
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
            mmiowb();
            WRITE_HOST_REG16(pHcInstance, 
                             HOST_REG_NORMAL_INT_STATUS, 
                             HOST_REG_INT_STATUS_CARD_INSERT_ENABLE | 
                             HOST_REG_INT_STATUS_CARD_REMOVAL_ENABLE);   
                /* mask card insert */                        
            MaskIrqFromIsr(pHcInstance, HOST_REG_INT_STATUS_ALLOW_INSERT_REMOVE_ONLY);
            QueueEventResponse(pHcInstance, WORK_ITEM_CARD_DETECT);
                /* we don't need to cancel any requests, every SD transaction is protected
                   by a timeout, we just let the timeout occur */
            /* continue and process interrupts */
        } 
    
            /* deal with card interrupts */
        if ((pHcInstance->CardInserted) && 
            (ints & HOST_REG_INT_STATUS_CARD_INT_STAT_ENABLE)) {     
            DBG_PRINT(STD_HOST_TRACE_SDIO_INT, ("SDIO S3C6400 HOST: SDIO Card Interrupt Detected \n"));           
              /* SD card interrupt*/
              /* disable the interrupt, the user must clear the interrupt */
            EnableDisableSDIOIRQ(pHcInstance,FALSE,TRUE);          
            QueueEventResponse(pHcInstance, WORK_ITEM_SDIO_IRQ);
            /* continue looking for other interrupt causes */
        } else if (ints & HOST_REG_INT_STATUS_CARD_INT_STAT_ENABLE) {
              /* disable bogus interrupt */
            EnableDisableSDIOIRQ(pHcInstance,FALSE,TRUE);
        }
        
        if (NULL == pReq) {
            break;
        }
                
        if (ints & HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE) {
                /* clear interrupt */
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE);
            temp = 10000;
            do {
                WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE);
                READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
                mmiowb();
                if( ! --temp ) break;
            } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE); 
                /* disable this interrupt */
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_CMD_COMPLETE);

            MaskIrqFromIsr(pHcInstance, HOST_REG_INT_STATUS_CMD_COMPLETE_ENABLE);
            status = ProcessCommandDone(pHcInstance,pReq,TRUE);           
            if (status != SDIO_STATUS_PENDING) {
                break;    
            }            
            continue;
        }

        if (ints & HOST_REG_NORMAL_INT_STATUS_DMA_INT) {
                /* we should NOT get these, the descriptors should not have the INTERRUPT bit set */
            DBG_ASSERT(FALSE);
            break;
        }
                
            /* check TX buffer ready */
        if (ints & HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY) {
            DBG_ASSERT(IS_SDREQ_WRITE_DATA(pReq->Flags));
                /* clear interrupt */
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);
            temp = 10000;
            do {
                WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);
                READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
                mmiowb();
                if( ! --temp ) break;
            } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY); 
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_BUFFER_WRITE_RDY);

            if (pReq->DataRemaining > 0) {
                    /* transfer data */
                if (!HcdTransferTxData(pHcInstance, pReq)) {
                        /* still more data to go... we'll get more write RDY interrupts */
                    continue;
                }                
                
                /* fall through if this is the last block */
            } 
                /* transfer is done */            
                /* disable write rdy */
            MaskIrqFromIsr(pHcInstance, 
                           HOST_REG_INT_STATUS_BUFFER_WRITE_RDY_ENABLE);
                /* all data transfered, wait for transfer complete */
            UnmaskIrqFromIsr(pHcInstance, 
                             HOST_REG_INT_STATUS_TRANSFER_COMPLETE_ENABLE); 
            continue;
        }
        
            /* check RX buffer ready */
        if (ints & (HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY)) {
            DBG_ASSERT(!IS_SDREQ_WRITE_DATA(pReq->Flags));
                /* clear interrupt */
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY);
            temp = 10000;
            do {
                WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY);
                READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
                mmiowb();
                if( ! --temp ) break;
            } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY); 
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_BUFFER_READ_RDY);

                /* unload fifo */
            HcdTransferRxData(pHcInstance, pReq);
            if (pReq->DataRemaining > 0) {
                    /* more to do.. */
            } else {
                    /* turn off read ready interrupts */
                MaskIrqFromIsr(pHcInstance, 
                               HOST_REG_INT_STATUS_BUFFER_READ_RDY_ENABLE);
                    /* all data transfered, wait for transfer complete */
                UnmaskIrqFromIsr(pHcInstance, 
                                 HOST_REG_INT_STATUS_TRANSFER_COMPLETE_ENABLE);  
            }
            continue;
        }
                
        if (ints & HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE) {            
            DBG_ASSERT(IS_SDREQ_DATA_TRANS(pReq->Flags));            
            DBG_PRINT(STD_HOST_TRACE_INT, ("SDIO S3C6400 HOST HcdSDInterrupt Transfer done \n"));
           
                /* clear interrupt */
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE);
            temp = 10000;
            do {
                WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE);
                READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
                mmiowb();
                if( ! --temp ) break;
            } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE); 
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE);
            temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
            WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_TRANSFER_COMPLETE);

                /* if we get here without an error, we are done with the data
                 * data operation */
            status = SDIO_STATUS_SUCCESS; 
            break;             
        }
        
    }
       
    if (status != SDIO_STATUS_PENDING) {
            /* turn off interrupts and clock */
        MaskIrqFromIsr(pHcInstance, 
                    ~(HOST_REG_INT_STATUS_ALLOW_INSERT_REMOVE_ONLY | 
                      HOST_REG_INT_STATUS_CARD_INT_STAT_ENABLE) );
                      
        if (errors) {
                /* reset statemachine */
            ResetCmdDatLine(pHcInstance);          
        }
        
        if (!pHcInstance->KeepClockOn) {
            ClockStartStop(pHcInstance, CLOCK_OFF);
        } else {
                /* adjust for IDLE control */
            IdleBusAdjustment(pHcInstance, ADJUST_BUS_IDLE);    
        }
                
        if (pReq != NULL) {                        
            if (IS_SDREQ_DATA_TRANS(pReq->Flags)) {
                if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    TRACE_SIGNAL_DATA_WRITE(pHcInstance, FALSE);
                } else {
                    TRACE_SIGNAL_DATA_READ(pHcInstance, FALSE);
                } 
            }         
                /* set the status */
            pReq->Status = status;
            
                /* cleanup DMA if used */
            if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) {

                temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp & ~HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                temp = 10000;
                do {
                    WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                    READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
                    mmiowb();
                    if( ! --temp ) break;
                } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & HOST_REG_NORMAL_INT_STATUS_DMA_INT); 
                temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_DMA_INT);
                temp = READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE);
                WRITE_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE, temp | HOST_REG_NORMAL_INT_STATUS_DMA_INT);
            
                HcdTransferDataDMAEnd(pHcInstance,pReq);
            }
            
            if (IS_SDREQ_DATA_TRANS(pReq->Flags)) {
                DBG_PRINT(STD_HOST_TRACE_DATA, ("SDIO S3C6400 HOST - %s Data Transfer Complete with status:%d\n",
                                     IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                                     pReq->Status));
            }
            
            if ((DBG_GET_DEBUG_LEVEL() >= STD_HOST_TRACE_DATA_DUMP) && SDIO_SUCCESS(status) &&
                IS_SDREQ_DATA_TRANS(pReq->Flags) && !IS_SDREQ_WRITE_DATA(pReq->Flags) &&
                !(pReq->Flags & SDREQ_FLAGS_DATA_DMA)) {     
                SDLIB_PrintBuffer(pReq->pDataBuffer,(pReq->BlockLen*pReq->BlockCount),"SDIO S3C6400 HOST - RX DataDump");    
            }      
                /* queue work item to notify bus driver of I/O completion */
            QueueEventResponse(pHcInstance, WORK_ITEM_IO_COMPLETE);
        }
    }
    
    DBG_PRINT(STD_HOST_TRACE_INT, ("-SDIO S3C6400 HOST HcdSDInterrupt Int handler \n"));
    return TRUE;
}


/* card detect callback from a deferred (non-ISR) context */
void ProcessDeferredCardDetect(PSDHCD_INSTANCE pHcInstance)
{

    HCD_EVENT       event;
    volatile UINT32 temp;
    int             temp2;
    
    event = EVENT_HCD_NOP;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO S3C6400 HOST Card Detect Processing \n"));
    if (pHcInstance->ShuttingDown) {
        return;
    }

    DBG_PRINT(SDDBG_TRACE, ("SDIO STD Host Card Detect Delaying to debounce card (%d Milliseconds)... \n",
            pHcInstance->CardDetectDebounceMS));
        
    OSSleep(pHcInstance->CardDetectDebounceMS);
    
    /* wait for stable */
    while(!(temp = READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE))& 
            HOST_REG_PRESENT_STATE_CARD_STATE_STABLE) {
        ;
    }

        /* look for removal */
    if (!(temp & HOST_REG_PRESENT_STATE_CARD_INSERTED)) {        
        pHcInstance->CardInserted = FALSE; 
        pHcInstance->KeepClockOn = FALSE;   
            /* turn the power off */
        SetPowerOn(pHcInstance, FALSE); 
        if (pHcInstance->StartUpCardCheckDone) {
            DBG_PRINT(STD_HOST_TRACE_CARD_INSERT, ("SDIO S3C6400 HOST Card Detect REMOVE\n"));
            /* card not present */
            event = EVENT_HCD_DETACH;
        }
    } else {
        /* card present */
        event = EVENT_HCD_ATTACH;
        pHcInstance->CardInserted = TRUE; 
        DBG_PRINT(STD_HOST_TRACE_CARD_INSERT, ("SDIO S3C6400 HOST Card Detect INSERT\n"));
    }
    
    if (!pHcInstance->StartUpCardCheckDone) {
            /* startup check is now done */
        pHcInstance->StartUpCardCheckDone = TRUE;    
    }
         /* clear interrupt */
    temp2 = 10000;
    do {
        WRITE_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS, 
                         HOST_REG_INT_STATUS_CARD_INSERT_ENABLE | 
                         HOST_REG_INT_STATUS_CARD_REMOVAL_ENABLE);
        READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS);
        mmiowb();
        if( ! --temp2 ) break;
    } while( READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS) & 
                                (HOST_REG_INT_STATUS_CARD_INSERT_ENABLE | 
                                 HOST_REG_INT_STATUS_CARD_REMOVAL_ENABLE));
     

        /* re-enable insertion/removal */                 
    UnmaskIrq(pHcInstance, HOST_REG_INT_STATUS_ALLOW_INSERT_REMOVE_ONLY,FALSE);

    if (event != EVENT_HCD_NOP) { 
        SDIO_HandleHcdEvent(&pHcInstance->Hcd, event);
    }
    
    if (!pHcInstance->CardInserted && !pHcInstance->RequestCompleteQueued) {
            /* check for a stuck request */
        PSDREQUEST  pReq = GET_CURRENT_REQUEST(&pHcInstance->Hcd);
        if (pReq != NULL) {    
            DBG_PRINT(SDDBG_ERROR, ("SDIO S3C6400 HOST , Stuck Request! 0x%X\n",(UINT)pReq));   
            DumpStdHcdRegisters(pHcInstance);     
            DumpCurrentRequestInfo(pHcInstance);
        }
    }
    
    DBG_PRINT(STD_HOST_TRACE_CARD_INSERT, ("- SDIO S3C6400 HOST Card Detect Processing \n"));
}


void DumpStdHcdRegisters(PSDHCD_INSTANCE pHcInstance)
{
    DBG_PRINT(SDDBG_TRACE, ("---------------- SDIO S3C6400 HOST, Register Dump ----------------- \n"));
    
    DBG_PRINT(SDDBG_TRACE,("    NORMAL INT STATUS    : 0x%X \n",
                READ_HOST_REG16(pHcInstance, HOST_REG_NORMAL_INT_STATUS)));
    DBG_PRINT(SDDBG_TRACE,("    ERROR INT STATUS     : 0x%X \n",
                READ_HOST_REG16(pHcInstance, HOST_REG_ERROR_INT_STATUS)));
    DBG_PRINT(SDDBG_TRACE,("    INT SIGNAL ENABLE    : 0x%X \n",
                READ_HOST_REG16(pHcInstance, HOST_REG_INT_SIGNAL_ENABLE)));
    DBG_PRINT(SDDBG_TRACE,("    ERROR SIGNAL ENABLES : 0x%X \n",
                READ_HOST_REG16(pHcInstance, HOST_REG_INT_ERR_SIGNAL_ENABLE)));    
    DBG_PRINT(SDDBG_TRACE,("    STATUS ENABLES       : 0x%X \n",
                READ_HOST_REG16(pHcInstance, HOST_REG_INT_STATUS_ENABLE)));
    DBG_PRINT(SDDBG_TRACE,("    ERROR STATUS ENABLES : 0x%X \n",
                READ_HOST_REG16(pHcInstance, HOST_REG_ERR_STATUS_ENABLE)));        
         
    DBG_PRINT(SDDBG_TRACE,("    HOST PRESENT_STATE   : 0x%X \n",
                READ_HOST_REG32(pHcInstance, HOST_REG_PRESENT_STATE)));         
         
                
    DBG_PRINT(SDDBG_TRACE, ("------------------------------------------------------------------"));
}


