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
@file: gps.c

@abstract: OS independent GPS class SDIO function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MODULE_NAME  SD_GPS_FD
#include "../../include/ctsystem.h"
#include "../../include/sdio_busdriver.h"
#include "gps.h"
#include "../../include/_sdio_defs.h"
#include "../../include/sdio_lib.h"

void GpsIRQHandler(PVOID pContext);
static void TxCompletion(PSDREQUEST pReq);


/*
 *  GpsInitialize - initialize new device
*/
SDIO_STATUS GpsInitialize(PSDGPS_DEVICE pDevice)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    struct SDIO_GPS_TPL gpsTpl;
    UINT32              nextTpl;
    UINT8               tplLength;
    UINT8               temp;
    SDCONFIG_FUNC_SLOT_CURRENT_DATA   slotCurrent;
    
    ZERO_OBJECT(fData);
    ZERO_OBJECT(slotCurrent);
    
    do {
        status = SemaphoreInitialize(&pDevice->DeviceSem, 0);
        if (!SDIO_SUCCESS(status)) {
            break;   
        }           
        
        status = SDLIB_GetDefaultOpCurrent(pDevice->pSDDevice,&slotCurrent.SlotCurrent);
        if (!SDIO_SUCCESS(status)) {
            break;
        }   
         
        DBG_PRINT(SDDBG_TRACE, ("SDIO Gps Function: Allocating Slot current: %d mA\n", slotCurrent.SlotCurrent));         
        status = SDLIB_IssueConfig(pDevice->pSDDevice,
                                   SDCONFIG_FUNC_ALLOC_SLOT_CURRENT,
                                   &slotCurrent,
                                   sizeof(slotCurrent));
                                   
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Gps Function: failed to allocate slot current %d\n",
                                    status));
            if (status == SDIO_STATUS_NO_RESOURCES) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Gps Function: Remaining Slot Current: %d mA\n",
                                    slotCurrent.SlotCurrent));  
            }
            break;
        }        
        
        fData.EnableFlags = SDCONFIG_ENABLE_FUNC;
        fData.TimeOut = 500;
        status = SDLIB_IssueConfig(pDevice->pSDDevice,
                                   SDCONFIG_FUNC_ENABLE_DISABLE,
                                   &fData,
                                   sizeof(fData));
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: GpsInitialize, failed to enable function %d\n",
                                    status));
            break;
        }
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function Ready!\n"));
        pDevice->HwReady = TRUE;  
            /* setup starting CIS scan */ 
        nextTpl = SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice->pSDDevice);
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Function CIS starts at :0x%X \n",
                                SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice->pSDDevice)));
            /* look for the GPS TPL */
        while (1) {
                /* reset max buffer length */  
            tplLength = sizeof(gpsTpl); 
                /* go get the GPS tuple */
            status = SDLIB_FindTuple(pDevice->pSDDevice,
                                     GPS_TUPLE,
                                     &nextTpl,
                                     (PUINT8)&gpsTpl,
                                     &tplLength);
            
            if (!SDIO_SUCCESS(status)){
                DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: Failed to get GPS Tuple: %d \n",status));
                break;    
            }
            
            if (gpsTpl.StdTupleNumber == STD_GPS_TUPLE_SIOREG) {
                DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Found SDIOREG Tuple \n"));
                DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: RegisterID:%d,RegisterExtID:%d \n",
                          gpsTpl.Tpd.AsSIOReg.RegisterID, gpsTpl.Tpd.AsSIOReg.RegisterExpID));
                DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: MaxBaudCode:%d, DRL:%d , DRM:%d\n",
                          gpsTpl.Tpd.AsSIOReg.MaxBaudRateCode,
                          gpsTpl.Tpd.AsSIOReg.DRL_4800,gpsTpl.Tpd.AsSIOReg.DRM_4800));
                pDevice->UartRegOffset =  gpsTpl.Tpd.AsSIOReg.RegisterOffset[0];  
                pDevice->UartRegOffset |=  gpsTpl.Tpd.AsSIOReg.RegisterOffset[1] << 8; 
                pDevice->UartRegOffset |=  gpsTpl.Tpd.AsSIOReg.RegisterOffset[2] << 16;  
                pDevice->UartMaxBaud = (gpsTpl.Tpd.AsSIOReg.MaxBaudRateCode > 0) ?
                            gpsTpl.Tpd.AsSIOReg.MaxBaudRateCode * 115200 : 115200;
                pDevice->UartDivisor = 4 *(gpsTpl.Tpd.AsSIOReg.DRL_4800 | 
                                       (gpsTpl.Tpd.AsSIOReg.DRM_4800 << 8) );
                /* if zero, its not setup, guess */
                if (pDevice->UartDivisor == 0) {
                    pDevice->UartDivisor = 6666;
                    DBG_PRINT(SDDBG_WARN, ("SDIO GPS Function: no UART divisor, using 1\n"));
                }
                            
                DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: RegisterOffset:0x%X \n",
                                        pDevice->UartRegOffset));
                break;
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Found Sub-Tuple %d .. continuing search\n",
                                        gpsTpl.StdTupleNumber));
                continue;   
            }
        }
        
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
        
            /* allocate a single request for asynch call usage */
        pDevice->pRequest = SDDeviceAllocRequest(pDevice->pSDDevice);
        
        if (NULL == pDevice->pRequest) {
            status =  SDIO_STATUS_NO_RESOURCES;
            break;
        }
            
            /* make sure interrupts are off and this also tests to see if we can see the
             * hardware */          
        pDevice->InterruptEnable = 0;
        status = WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: GpsInitialize, failed register write, %d\n",
                                    status));
            break;
        }
            /* read registers to clear status */
        ReadRegister(pDevice, UART_LINE_STATUS_REG, &temp);
        ReadRegister(pDevice, UART_RECEIVE_REG, &temp);
        ReadRegister(pDevice, UART_INT_IDENT_REG, &temp);
        ReadRegister(pDevice, UART_MODEM_STATUS_REG, &temp);
        WriteRegister(pDevice, UART_FIFO_CNTRL_REG, UART_DATA_8_BITS | UART_FIFO_ENABLE | UART_FIFO_RCV_RESET);
        WriteRegister(pDevice, UART_FIFO_CNTRL_REG, UART_DATA_8_BITS | UART_FIFO_ENABLE);
        SetBaudRate(pDevice, 4800);
        WriteRegister(pDevice, UART_LINE_CNTRL_REG, (UART_ONE_STOP | UART_NO_PARITY | UART_DATA_8_BITS));

            /* set our IRQ handler */        
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Registering GpsIrqHandler \n"));
        SDDEVICE_SET_IRQ_HANDLER(pDevice->pSDDevice,GpsIRQHandler,pDevice);
            /* unmask our interrupt on the card */
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: unmasking IRQ \n"));
        status = SDLIB_IssueConfig(pDevice->pSDDevice,SDCONFIG_FUNC_UNMASK_IRQ,NULL,0);  
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: GpsInitialize, failed to unmask IRQ %d\n",
                                    status));
            break;
        }       
                
//??        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: enabling InterruptEnable \n"));
//??        pDevice->InterruptEnable = UART_ERBFI | UART_ELSI;
//??        WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
        
    } while (FALSE);
 
    if (!SDIO_SUCCESS(status)) {
        GpsDeinitialize(pDevice);
    }
    SemaphorePost(&pDevice->DeviceSem);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: exiting \n"));
    
    return status;
}

/*
 *  GpsDeinitialize - initialize new device
*/
void GpsDeinitialize(PSDGPS_DEVICE pDevice)
{
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    
    if (!(SDDEVICE_IS_CARD_REMOVED(pDevice->pSDDevice))) {
        if (pDevice->HwReady) {
                /* try masking our IRQ */
            SDLIB_IssueConfig(pDevice->pSDDevice,SDCONFIG_FUNC_MASK_IRQ,NULL,0); 
            DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Unregistering GpsIrqHandler \n"));
            SDDEVICE_SET_IRQ_HANDLER(pDevice->pSDDevice, NULL, NULL);
            
                /* power down the hardware */
            ZERO_OBJECT(fData);
            fData.EnableFlags = SDCONFIG_DISABLE_FUNC;
            fData.TimeOut = 500;
            SDLIB_IssueConfig(pDevice->pSDDevice,
                        SDCONFIG_FUNC_ENABLE_DISABLE,
                        &fData,
                        sizeof(fData));   
        }
    }
    pDevice->HwReady = FALSE;
        
    SDLIB_IssueConfig(pDevice->pSDDevice,
                      SDCONFIG_FUNC_FREE_SLOT_CURRENT,
                      NULL,
                      0);

    if (pDevice->pRequest != NULL) {
        SDDeviceFreeRequest(pDevice->pSDDevice, pDevice->pRequest);
        pDevice->pRequest = NULL;
    }
    SemaphoreDelete(&pDevice->DeviceSem);  
}
/*
 * SetBaudRate - set the requested baud rate
 */
SDIO_STATUS SetBaudRate(PSDGPS_DEVICE pDevice, UINT32 BaudRate)
{
    UINT8   lineControl;
    UINT8   saveInterrupt;
    UINT32  divisor;
    
        /* disable device interrupts */
    ReadRegister(pDevice, UART_INT_ENABLE_REG, &saveInterrupt);
    WriteRegister(pDevice, UART_INT_ENABLE_REG, 0);
        /* check for a valid baud rate */
    if ((BaudRate % 1200) || (BaudRate > pDevice->UartMaxBaud)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: SetBaudRate, invalid rate %d\n", BaudRate));
            WriteRegister(pDevice, UART_INT_ENABLE_REG, saveInterrupt);
        return SDIO_STATUS_INVALID_PARAMETER;
    }

        /* calculate new divisor based on 1200 baud divisor */
    divisor = pDevice->UartDivisor / (BaudRate / 1200);

    ReadRegister(pDevice, UART_LINE_CNTRL_REG, &lineControl);

    WriteRegister(pDevice, UART_LINE_CNTRL_REG, lineControl | UART_CLOCK_ENABLE);
    WriteRegister(pDevice, UART_BAUD_HIGH_REG, (UINT8)((divisor >> 8) & 0xFF));
    WriteRegister(pDevice, UART_BAUD_LOW_REG, (UINT8)(divisor & 0xFF));
    WriteRegister(pDevice, UART_LINE_CNTRL_REG, lineControl);

    WriteRegister(pDevice, UART_INT_ENABLE_REG, saveInterrupt);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: SetBaudRate, rate %d, divisor %d\n", 
                            BaudRate, divisor));
    return SDIO_STATUS_SUCCESS;
}

/*
 * GpsIRQHandler - hamdle interrupts
*/
void GpsIRQHandler(PVOID pContext) 
{
    PSDGPS_DEVICE pDevice;
    SDIO_STATUS   status = SDIO_STATUS_DEVICE_ERROR;
    UINT8         temp;
    int           max = 4;
    pDevice = (PSDGPS_DEVICE)pContext;
    DBG_PRINT(SDIO_GPS_TRACE_INT, ("+I\n"));
            
    while(1) {
        if (max-- < 0) break;
            /* read the ident register */
        status = ReadRegister(pDevice, UART_INT_IDENT_REG, &temp);
        if (!SDIO_SUCCESS(status)) {
            break;       
        }
            /* INTPEND bit is zero indicates a pending interrupt */
        if (!(temp & UART_INTPEND)) { 
                /* get the encoded interrupt value */        
            switch (temp & UART_IID_MASK) {
                case UART_IID_RLS:
                        /* line status changed */  
                    ReadRegister(pDevice, UART_LINE_STATUS_REG, &temp);
                    DBG_PRINT(SDIO_GPS_TRACE_INT, ("+LS 0x%X\n", (UINT)temp));
                    if (temp & UART_LSR_DR) {
                        GpsReceive(pDevice);
                    }
                    break; 
                case UART_IID_RDA: 
                      /* handle receive */        
                    GpsReceive(pDevice);
                    break;
                case UART_IID_CTI:
                        /* receiver timeout? */
                    GpsReceive(pDevice);
                    DBG_PRINT(SDIO_GPS_TRACE_INT, ("+CTI\n"));
                    break;
                case UART_IID_THRE:
                        /* transmitter empty */
                    GpsTransmit(pDevice);    
                    break;
                case UART_IID_MS:
                        /* modem status */
                    DBG_PRINT(SDIO_GPS_TRACE_INT, ("SDIO GPS Function: modem status change int \n")); 
                    break;
                default:   
                    break;
            }
        } else {
                /* no interrupts pending */
            break;   
        }
    } 
    
        /* ack the interrupt */
    status = SDLIB_IssueConfig(pDevice->pSDDevice,SDCONFIG_FUNC_ACK_IRQ,NULL,0);  
        
    DBG_PRINT(SDIO_GPS_TRACE_INT, ("-I\n"));

}

/*
 * WriteRegister - write an 8-bit register
*/
SDIO_STATUS WriteRegister(PSDGPS_DEVICE pDevice, UINT reg, UINT8 Data) {
    return SDLIB_IssueCMD52(pDevice->pSDDevice,SDDEVICE_GET_SDIO_FUNCNO(pDevice->pSDDevice),
                    ((pDevice)->UartRegOffset + (reg)),&Data,1,TRUE);
}

/*
 * WriteRegister - write an 8-bit register aysnchronously
*/
SDIO_STATUS WriteRegisterAsynch(PSDGPS_DEVICE pDevice, UINT reg, UINT8 Data, PSDREQUEST pReq) 
{
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
                             
    SDLIB_SetupCMD52Request(SDDEVICE_GET_SDIO_FUNCNO(pDevice->pSDDevice), 
                            ((pDevice)->UartRegOffset + (reg)),
                            TRUE,
                            Data, 
                            pReq);
    pReq->Flags |= SDREQ_FLAGS_TRANS_ASYNC;   
    pReq->pCompletion = TxCompletion;
    pReq->pCompleteContext = (PVOID)pDevice;
                            
    return SDDEVICE_CALL_REQUEST_FUNC(pDevice->pSDDevice, pReq);
}

/*
 * TxCompletion - completion routine for WriteAsynch
*/
static void TxCompletion(PSDREQUEST pReq) 
{
     /*  nothing to do
    PSDGPS_DEVICE pDevice;
    pDevice = (PSDGPS_DEVICE)pReq->pCompleteContext;
    */
}
