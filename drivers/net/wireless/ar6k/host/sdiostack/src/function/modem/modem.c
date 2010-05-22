/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: modem.c

@abstract: OS independent Modem SDIO function driver

#notes: 
 
@notice: Copyright (c), 2004-2005 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "../../include/ctsystem.h"
#include "../../include/sdio_busdriver.h"
#include "modem.h"
#include "../../include/_sdio_defs.h"
#include "../../include/sdio_lib.h"

void ModemIRQHandler(PVOID pContext);
static void TxCompletion(PSDREQUEST pReq);

/*
 *  ModemInitialize - initialize new device
*/
SDIO_STATUS ModemInitialize(PSDMODEM_DEVICE pDevice)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    struct SDIO_MODEM_TPL modemTpl;
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
{
    UINT32              nextTpl;
    UINT8               tplLength;
    struct SDIO_FUNC_EXT_FUNCTION_TPL_1_1 funcTuple;
    SDIO_STATUS         status;
    
      /* get the FUNCE tuple */
    nextTpl = SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice->pSDDevice);
    tplLength = sizeof(funcTuple); 
        /* go get the function Extension tuple */
    status = SDLIB_FindTuple(pDevice->pSDDevice,
                              CISTPL_FUNCE,
                              &nextTpl,
                              (PUINT8)&funcTuple,
                              &tplLength);
    
    if (!SDIO_SUCCESS(status)) {
        DBG_PRINT(SDDBG_ERROR, ("SDLIB_GetDefaultOpCurrent: Failed to get FuncE Tuple: %d \n", status));
        return status;  
    }   
    DBG_PRINT(SDDBG_TRACE, ("SDIO Modem Function: length: %d\n",tplLength));
    DBG_PRINT(SDDBG_TRACE, ("           Common    Type: %d\n",(INT)funcTuple.CommonInfo.Type));
    DBG_PRINT(SDDBG_TRACE, ("                     FunctionInfo: %d\n",(INT)funcTuple.CommonInfo.FunctionInfo));
    DBG_PRINT(SDDBG_TRACE, ("                     SDIORev: %d\n",(INT)funcTuple.CommonInfo.SDIORev));
    DBG_PRINT(SDDBG_TRACE, ("                     CardPSN: %d\n",(INT)funcTuple.CommonInfo.CardPSN));
    DBG_PRINT(SDDBG_TRACE, ("                     CSASize: %d\n",(INT)funcTuple.CommonInfo.CSASize));
    DBG_PRINT(SDDBG_TRACE, ("                     CSAProperties: %d\n",(INT)funcTuple.CommonInfo.CSAProperties));
    DBG_PRINT(SDDBG_TRACE, ("                     MaxBlockSize: %d\n",(INT)funcTuple.CommonInfo.MaxBlockSize));
    DBG_PRINT(SDDBG_TRACE, ("                     FunctionOCR: %d\n",(INT)funcTuple.CommonInfo.FunctionOCR));
    DBG_PRINT(SDDBG_TRACE, ("                     OpMinPwr: %d\n",(INT)funcTuple.CommonInfo.OpMinPwr));
    DBG_PRINT(SDDBG_TRACE, ("                     OpAvgPwr: %d\n",(INT)funcTuple.CommonInfo.OpAvgPwr));
    DBG_PRINT(SDDBG_TRACE, ("                     OpMaxPwr: %d\n",(INT)funcTuple.CommonInfo.OpMaxPwr));
    DBG_PRINT(SDDBG_TRACE, ("                     SbMinPwr: %d\n",(INT)funcTuple.CommonInfo.SbMinPwr));
    DBG_PRINT(SDDBG_TRACE, ("                     SbAvgPwr: %d\n",(INT)funcTuple.CommonInfo.SbAvgPwr));
    DBG_PRINT(SDDBG_TRACE, ("                     SbMaxPwr: %d\n",(INT)funcTuple.CommonInfo.SbMaxPwr));
    DBG_PRINT(SDDBG_TRACE, ("                     MinBandWidth: %d\n",(INT)funcTuple.CommonInfo.MinBandWidth));
    DBG_PRINT(SDDBG_TRACE, ("                     OptBandWidth: %d\n",(INT)funcTuple.CommonInfo.OptBandWidth));


    DBG_PRINT(SDDBG_TRACE, ("            1.1      EnableTimeOut: %d\n",(INT)funcTuple.EnableTimeOut));             
    DBG_PRINT(SDDBG_TRACE, ("                     OperPwrMaxPwr: %d\n",(INT)funcTuple.OperPwrMaxPwr));  
    DBG_PRINT(SDDBG_TRACE, ("                     OperPwrAvgPwr: %d\n",(INT)funcTuple.OperPwrAvgPwr));
    DBG_PRINT(SDDBG_TRACE, ("                     HiPwrMaxPwr: %d\n",(INT)funcTuple.HiPwrMaxPwr));  
    DBG_PRINT(SDDBG_TRACE, ("                     HiPwrAvgPwr: %d\n",(INT)funcTuple.HiPwrAvgPwr));  
    DBG_PRINT(SDDBG_TRACE, ("                     LowPwrMaxPwr: %d\n",(INT)funcTuple.LowPwrMaxPwr));  
    DBG_PRINT(SDDBG_TRACE, ("                     LowPwrAvgPwr: %d\n",(INT)funcTuple.LowPwrAvgPwr));  
    SDLIB_PrintBuffer((PUCHAR)&funcTuple, sizeof(funcTuple), "funcTuple");
}

        
        status = SDLIB_GetDefaultOpCurrent(pDevice->pSDDevice,&slotCurrent.SlotCurrent);
        if (!SDIO_SUCCESS(status)) {
            break;
        }   
         
        DBG_PRINT(SDDBG_TRACE, ("SDIO Modem Function: Allocating Slot current: %d mA\n", slotCurrent.SlotCurrent));         
        status = SDLIB_IssueConfig(pDevice->pSDDevice,
                                   SDCONFIG_FUNC_ALLOC_SLOT_CURRENT,
                                   &slotCurrent,
                                   sizeof(slotCurrent));
                                   
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Modem Function: failed to allocate slot current %d\n",
                                    status));
            if (status == SDIO_STATUS_NO_RESOURCES) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Modem Function: Remaining Slot Current: %d mA\n",
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
            DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: ModemInitialize, failed to enable function %d\n",
                                    status));
            break;
        }
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function Ready!\n"));
        pDevice->HwReady = TRUE;  
            /* setup starting CIS scan */ 
        nextTpl = SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice->pSDDevice);
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Function CIS starts at :0x%X \n",
                                SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice->pSDDevice)));
            /* look for the MODEM TPL */
        while (1) {
                /* reset max buffer length */  
            tplLength = sizeof(modemTpl); 
                /* go get the MODEM tuple */
            status = SDLIB_FindTuple(pDevice->pSDDevice,
                                     MODEM_TUPLE,
                                     &nextTpl,
                                     (PUINT8)&modemTpl,
                                     &tplLength);
            
            if (!SDIO_SUCCESS(status)){
                DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: Failed to get MODEM Tuple: %d \n",status));
                break;    
            }
            
            if (modemTpl.StdTupleNumber == STD_MODEM_TUPLE_SIOREG) {
                DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Found SDIOREG Tuple \n"));
                DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: RegisterID:%d,RegisterExtID:%d \n",
                          modemTpl.Tpd.AsSIOReg.RegisterID, modemTpl.Tpd.AsSIOReg.RegisterExpID));
                DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: MaxBaudCode:%d, DRL:%d , DRM:%d\n",
                          modemTpl.Tpd.AsSIOReg.MaxBaudRateCode,
                          modemTpl.Tpd.AsSIOReg.DRL_4800,modemTpl.Tpd.AsSIOReg.DRM_4800));
                pDevice->UartRegOffset =  modemTpl.Tpd.AsSIOReg.RegisterOffset[0];  
                pDevice->UartRegOffset |=  modemTpl.Tpd.AsSIOReg.RegisterOffset[1] << 8; 
                pDevice->UartRegOffset |=  modemTpl.Tpd.AsSIOReg.RegisterOffset[2] << 16;  
                pDevice->UartMaxBaud = (modemTpl.Tpd.AsSIOReg.MaxBaudRateCode > 0) ?
                            modemTpl.Tpd.AsSIOReg.MaxBaudRateCode * 115200 : 115200;
                pDevice->UartDivisor = 4 *(modemTpl.Tpd.AsSIOReg.DRL_4800 | 
                                       (modemTpl.Tpd.AsSIOReg.DRM_4800 << 8) );
                /* if zero, its not setup, guess */
                if (pDevice->UartDivisor == 0) {
                    pDevice->UartDivisor = 6666;
                    DBG_PRINT(SDDBG_WARN, ("SDIO MODEM Function: no UART divisor, using 1\n"));
                }
                            
                DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: RegisterOffset:0x%X \n",
                                        pDevice->UartRegOffset));
                break;
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Found Sub-Tuple %d .. continuing search\n",
                                        modemTpl.StdTupleNumber));
                continue;   
            }
        }
        
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
            /* make sure interrupts are off and this also tests to see if we can see the
             * hardware */          
        pDevice->InterruptEnable = 0;
        status = WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: ModemInitialize, failed register write, %d\n",
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
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Registering ModemIrqHandler \n"));
        SDDEVICE_SET_IRQ_HANDLER(pDevice->pSDDevice,ModemIRQHandler,pDevice);
            /* unmask our interrupt on the card */
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: unmasking IRQ \n"));
        status = SDLIB_IssueConfig(pDevice->pSDDevice,SDCONFIG_FUNC_UNMASK_IRQ,NULL,0);  
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: ModemInitialize, failed to unmask IRQ %d\n",
                                    status));
            break;
        }       
                
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: enabling InterruptEnable \n"));
        pDevice->InterruptEnable = UART_ERBFI | UART_ELSI;
        WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
        
    } while (FALSE);
 
    if (!SDIO_SUCCESS(status)) {
        ModemDeinitialize(pDevice);
    }
    SemaphorePost(&pDevice->DeviceSem);
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: exiting \n"));
    
    return status;
}

/*
 *  ModemDeinitialize - initialize new device
*/
void ModemDeinitialize(PSDMODEM_DEVICE pDevice)
{
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    
    if (!(SDDEVICE_IS_CARD_REMOVED(pDevice->pSDDevice))) {
        if (pDevice->HwReady) {
                /* try masking our IRQ */
            SDLIB_IssueConfig(pDevice->pSDDevice,SDCONFIG_FUNC_MASK_IRQ,NULL,0); 
            DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Unregistering ModemIrqHandler \n"));
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
                      
    SemaphoreDelete(&pDevice->DeviceSem);  
}
/*
 * SetBaudRate - set the requested baud rate
 */
SDIO_STATUS SetBaudRate(PSDMODEM_DEVICE pDevice, UINT32 BaudRate)
{
    UINT8   lineControl;
    UINT8   saveInterrupt;
    UINT32  divisor;
    
        /* disable device interrupts */
    ReadRegister(pDevice, UART_INT_ENABLE_REG, &saveInterrupt);
    WriteRegister(pDevice, UART_INT_ENABLE_REG, 0);
        /* check for a valid baud rate */
    if ((BaudRate % 1200) || (BaudRate > pDevice->UartMaxBaud)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: SetBaudRate, invalid rate %d\n", BaudRate));
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
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: SetBaudRate, rate %d, divisor %d\n", 
                            BaudRate, divisor));
    return SDIO_STATUS_SUCCESS;
}

/*
 * ModemIRQHandler - hamdle interrupts
*/
void ModemIRQHandler(PVOID pContext) 
{
    PSDMODEM_DEVICE pDevice;
    SDIO_STATUS   status = SDIO_STATUS_DEVICE_ERROR;
    UINT8         temp;
    
    pDevice = (PSDMODEM_DEVICE)pContext;
    DBG_PRINT(SDIO_MODEM_TRACE_INT, ("+I\n"));
            
    while(1) { 
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
                    DBG_PRINT(SDIO_MODEM_TRACE_INT, ("+LS 0x%X\n", (UINT)temp));
                    if (temp & UART_LSR_DR) {
                        ModemReceive(pDevice);
                    }
                    break; 
                case UART_IID_RDA: 
                      /* handle receive */        
                    ModemReceive(pDevice);
                    break;
                case UART_IID_CTI:
                        /* receiver timeout? */
                    ModemReceive(pDevice);
                    DBG_PRINT(SDIO_MODEM_TRACE_INT, ("+CTI\n"));
                    break;
                case UART_IID_THRE:
                        /* transmitter empty */
                    ModemTransmit(pDevice);    
                    break;
                case UART_IID_MS:
                        /* modem status */
                    DBG_PRINT(SDIO_MODEM_TRACE_INT, ("SDIO MODEM Function: modem status change int \n")); 
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
        
    DBG_PRINT(SDIO_MODEM_TRACE_INT, ("-I\n"));

}

/*
 * WriteRegister - write an 8-bit register
*/
SDIO_STATUS WriteRegister(PSDMODEM_DEVICE pDevice, UINT reg, UINT8 Data) {
    return SDLIB_IssueCMD52(pDevice->pSDDevice,SDDEVICE_GET_SDIO_FUNCNO(pDevice->pSDDevice),
                    ((pDevice)->UartRegOffset + (reg)),&Data,1,TRUE);
}

/*
 * WriteRegister - write an 8-bit register aysnchronously
*/
SDIO_STATUS WriteRegisterAsynch(PSDMODEM_DEVICE pDevice, UINT reg, UINT8 Data) {
    
    PSDREQUEST  pReq = NULL;

    pReq = SDDeviceAllocRequest(pDevice->pSDDevice);
    
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
    /* nothing to do */
}
