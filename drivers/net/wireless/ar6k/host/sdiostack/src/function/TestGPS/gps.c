/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: gps.c

@abstract: OS independent GPS class SDIO function driver

#notes: 
 
@notice: Copyright (c), 2004 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "../../include/ctsystem.h"
#include "../../include/sdio_busdriver.h"
#include "gps.h"
#include "../../include/_sdio_defs.h"
#include "../../include/sdio_lib.h"

#include <linux/random.h>

void GpsIRQHandler(PVOID pContext);
void InitTestTimer(PSDGPS_DEVICE pGpsDevice);
void CleanupTestTimer(void);
void QueueTestTimer(UINT32 TimeOut); 


//??
SDIO_STATUS ReadRegisterAsynch(PSDGPS_DEVICE pDEviice, UINT reg, void (*pComp)(PSDREQUEST pReq));
static void TestCompletion4(PSDREQUEST pReq) {
    if (!SDIO_SUCCESS(pReq->Status)) {
DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: TestCompletion4 failed, status: 0x%x\n", pReq->Status));
        while(TRUE);
    }
    SDDeviceFreeRequest(((PSDGPS_DEVICE)pReq->pCompleteContext)->pSDDevice, pReq);
}
 
static void TestCompletion3(PSDREQUEST pReq) {
    static int count = 0;
DBG_PRINT(SDDBG_TRACE, ("+SDIO GPS Function: TC3\n"));

    if (!SDIO_SUCCESS(pReq->Status)) {
DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: TestCompletion3 failed, status: 0x%x\n", pReq->Status));
        while(TRUE);
    }
    count++;
    if (count > 1000) {
        count = 0;
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: TC3\n"));
    }
    if (SD_R5_GET_RESP_FLAGS(pReq->Response) & SD_R5_ERRORS) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: TC3 error, 0x%X\n", SD_R5_GET_RESP_FLAGS(pReq->Response)));
    }    
    SDDeviceFreeRequest(((PSDGPS_DEVICE)pReq->pCompleteContext)->pSDDevice, pReq);

    if (!SDIO_SUCCESS(ReadRegisterAsynch((PSDGPS_DEVICE)pReq->pCompleteContext, 4,
         TestCompletion4))) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: TC3, failed asynch CMD52\n"));
    }
 
DBG_PRINT(SDDBG_TRACE, ("-SDIO GPS Function: TC3\n"));
    
}
static void TestCompletion2(PSDREQUEST pReq) {
    PSDGPS_DEVICE pDevice = (PSDGPS_DEVICE)pReq->pCompleteContext;
#define LENGTH 500    
    static UINT8 buffer[LENGTH] = {1};
    static int count = 0;
DBG_PRINT(SDDBG_TRACE, ("+SDIO GPS Function: TC2\n"));
    if (!SDIO_SUCCESS(pReq->Status)) {
DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: TestCompletion2 failed, status: 0x%x\n", pReq->Status));
        while(TRUE);
    }
    count++;
    if (count > 100) {
        count = 0;
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: TC2\n"));
    }
    SDDeviceFreeRequest(pDevice->pSDDevice, pReq);
DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: TC2 1\n"));
//??    if (1) return;
    /* test asynch cmd3 */
    /* allocate request to send to host controller */
    pReq = SDDeviceAllocRequest(pDevice->pSDDevice);
    if (NULL == pReq) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: TestComp2 - can't allocate\n"));
        return;
    }
   
    /* initialize the command argument bits, see CMD53 SDIO spec. */
    SDIO_SET_CMD53_ARG(pReq->Argument,
                       CMD53_READ,
                       SDDEVICE_GET_SDIO_FUNCNO(pDevice->pSDDevice), /* function number */
                       CMD53_BYTE_BASIS,       /* set to byte mode */
                       CMD53_INCR_ADDRESS,    
                       UART_SCRATCH_REG, /* 17-bit register address */
                       CMD53_CONVERT_BYTE_BASIS_BLK_LENGTH_PARAM(LENGTH)  /* bytes */
                       );
    pReq->pDataBuffer = buffer;
    pReq->Command = CMD53;
    pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS | 
                 /* (SDREQ_FLAGS_DATA_WRITE) |*/
                   SDREQ_FLAGS_TRANS_ASYNC;
    pReq->BlockCount = 1;    /* byte mode is always 1 block */
    pReq->BlockLen = LENGTH;
    pReq->pCompletion = TestCompletion3;
    pReq->pCompleteContext = (PVOID)pDevice;
DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: TC2 2\n"));
            
    SDDEVICE_CALL_REQUEST_FUNC(pDevice->pSDDevice,pReq);
DBG_PRINT(SDDBG_TRACE, ("-SDIO GPS Function: TC2\n"));

    /* force an error on re-use of request */
//??    SDDEVICE_CALL_REQUEST_FUNC(pDevice->pSDDevice,pReq);
        
}
static void TestCompletion(PSDREQUEST pReq) 
{
    PSDGPS_DEVICE pDevice = (PSDGPS_DEVICE)pReq->pCompleteContext;
  
    if (!SDIO_SUCCESS(pReq->Status)) {
DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: TestCompletion failed, status: 0x%x\n", pReq->Status));
        while(TRUE);
    }
    SDDeviceFreeRequest(pDevice->pSDDevice, pReq);
    
    if (!SDIO_SUCCESS(ReadRegisterAsynch(pDevice, 4, TestCompletion2))) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: async read fail, comp\n"));
        while(TRUE);
    }
    
}
SDIO_STATUS ReadRegisterAsynch(PSDGPS_DEVICE pDevice, UINT reg, void (*pComp)(PSDREQUEST pReq) ) {
    
    PSDREQUEST  pReq = NULL;

    pReq = SDDeviceAllocRequest(pDevice->pSDDevice);
    
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
                             
    SDLIB_SetupCMD52Request(SDDEVICE_GET_SDIO_FUNCNO(pDevice->pSDDevice), 
                            ((pDevice)->UartRegOffset + (reg)),
                            TRUE,
                            0, 
                            pReq);
    pReq->Flags |= SDREQ_FLAGS_TRANS_ASYNC;   
    pReq->pCompletion =pComp;
    pReq->pCompleteContext = (PVOID)pDevice;
                            
    return SDDEVICE_CALL_REQUEST_FUNC(pDevice->pSDDevice, pReq);
}

//??





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
                                  
        if (!SDIO_SUCCESS(status)) {
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
            /* lookSDDBG_TRACE foDBG_PRPSDGPS_DEVICEINTr the GPS TPL */
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
                /* if zerPSDGPS_DEVICEo, its not setup, guess */
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
        
        InitTestTimer(pDevice);
        
        if (!SDIO_SUCCESS(status)) {
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
                
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: enabling InterruptEnable \n"));
        pDevice->InterruptEnable = UART_ERBFI | UART_ELSI;
        WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
        
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
    
    CleanupTestTimer();
    
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
    
    pDevice = (PSDGPS_DEVICE)pContext;
    DBG_PRINT(SDIO_GPS_TRACE_INT, ("+I\n"));
            
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
                    DBG_PRINT(SDIO_GPS_TRACE_INT, ("+LS 0x%X\n", (UINT)temp));
                    if (temp & UART_LSR_DR) {
                        GpsReceive(pDevice);
                    }
                    break; 
                case UART_IID_RDA: 
                      /* handle receive */        
                    GpsReceive(pDevice);
//??
                    if (!SDIO_SUCCESS(ReadRegisterAsynch(pDevice, UART_MODEM_CONTROL_REG, TestCompletion))) {
                        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: async read fail\n"));
                    }
                    {
                      UCHAR byte;
                      get_random_bytes(&byte, 1);
                      QueueTestTimer(byte & 0x3F);
                    }
//??        
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
SDIO_STATUS WriteRegisterAsynch(PSDGPS_DEVICE pDevice, PSDREQUEST  pReq, void (*pComp)(PSDREQUEST pReq), UINT reg, UINT8 Data) {

    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
                             
    SDLIB_SetupCMD52Request(SDDEVICE_GET_SDIO_FUNCNO(pDevice->pSDDevice), 
                            ((pDevice)->UartRegOffset + (reg)),
                            TRUE,
                            Data, 
                            pReq);
    pReq->Flags |= SDREQ_FLAGS_TRANS_ASYNC;   
    pReq->pCompletion = pComp;
    pReq->pCompleteContext = (PVOID)pDevice;
                            
    return SDDEVICE_CALL_REQUEST_FUNC(pDevice->pSDDevice, pReq);
}


static struct timer_list TestTimer; 
static struct work_struct TestWork;
static INT g_UseWorkItem = 0;   

static void DoTest(PSDGPS_DEVICE pGpsDevice, BOOL Where)
{
    static int tcount = 0;
    tcount++;
    if (tcount > 30) {
        tcount = 0; 
        if (Where) {
          DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: Timer - work item\n"));
        } else {
          DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: Timer - timer func\n"));  
        }
    }
  
    if (!SDIO_SUCCESS(ReadRegisterAsynch(pGpsDevice, 
                      UART_MODEM_CONTROL_REG, TestCompletion))) {
         DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: async read fail - timer\n"));
    }  
}

static void Test_WorkItemFunc(void *context)
{
   PSDGPS_DEVICE pGpsDevice = (PSDGPS_DEVICE)context;
   DoTest(pGpsDevice, TRUE);
}

static void TimerFunc(unsigned long Context)
{
    PSDGPS_DEVICE pGpsDevice = (PSDGPS_DEVICE)Context;
  
    if (g_UseWorkItem) {
        schedule_work(&TestWork);
    } else {
        DoTest(pGpsDevice, FALSE);
    }
    
   
}

void QueueTestTimer(UINT32 TimeOut)
{
    UINT32 delta;
    
        /* convert timeout to ticks */
    delta = (TimeOut * HZ)/1000;
    if (delta == 0) {
        delta = 1;  
    }
    TestTimer.expires = jiffies + delta;
    add_timer(&TestTimer);           
}

void InitTestTimer(PSDGPS_DEVICE pGpsDevice)
{
    init_timer(&TestTimer);       
    TestTimer.function = TimerFunc;
    TestTimer.data = (unsigned long)pGpsDevice;  
    INIT_WORK(&TestWork,Test_WorkItemFunc, pGpsDevice);  
}

void CleanupTestTimer()
{
    del_timer(&TestTimer); 
    flush_scheduled_work();
}
    

