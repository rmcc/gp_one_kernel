/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: gps.h

@abstract: OS independent include for GPS SDIO function driver

#notes: 
 
@notice: Copyright (c), 2004 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __GPS_H___
#define __GPS_H___

#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/gps_vxworks.h"
#endif /* VXWORKS */

#if defined(LINUX) || defined(__linux__)
#include "linux/gps_linux.h"
#endif /* LINUX */

#ifdef QNX
#include "nto/gps_qnx.h"
#endif /* QNX Neutrino */

typedef struct _SDGPS_DRIVER_CONTEXT {
    SDFUNCTION   Function;  /* function description for bus driver */ 
    PSDDEVICE    pSDDevice; /* bus driver's device we are supporting (only 1) */
    SDGPS_DEVICE GpsDevice; /* per device info, we only support one device right now */
}SDGPS_DRIVER_CONTEXT, *PSDGPS_DRIVER_CONTEXT;

#define SDIO_GPS_TRACE_INT  (SDDBG_TRACE+1)

#define GPS_CMD_52                   52
#define GPS_MAX_BAUD_RATE            115200
#define GPS_FIFO_SIZE                16
#define GPS_TYPE                     "SDIO GPS 16650"
#define GPS_TUPLE                    0x91

struct SDIO_GPS_SUBTPL_SIOREG {
    UINT8   RegisterID;   
    UINT8   RegisterExpID;
    UINT8   RegisterOffset[3];
    UINT8   MaxBaudRateCode;
    UINT8   DRL_4800;
    UINT8   DRM_4800;
}__attribute__ ((packed));

struct SDIO_GPS_SUBTPL_RCVCAPS {
    UINT8   Junk;  /* defined in some other doc */   
}__attribute__ ((packed));

union GPS_TPL_DATA {
    struct SDIO_GPS_SUBTPL_SIOREG  AsSIOReg; 
    struct SDIO_GPS_SUBTPL_RCVCAPS AsRcvCaps;
}; 
    
    /* GPS tuple */
struct SDIO_GPS_TPL {
    UINT8   InterfaceCode;        
    UINT8   StdTupleNumber;
#define STD_GPS_TUPLE_SIOREG 0x00
#define STD_GPS_TUPLE_RCVCAP 0x01
    union GPS_TPL_DATA  Tpd;
}__attribute__ ((packed));

    /* 166550 UART register offsets */
#define UART_RECEIVE_REG             0x00
#define UART_SEND_REG                0x00
#define UART_BAUD_LOW_REG            0x00
#define UART_BAUD_HIGH_REG           0x01
#define UART_INT_ENABLE_REG          0x01
#define UART_INT_IDENT_REG           0x02
#define UART_FIFO_CNTRL_REG          0x02
#define UART_LINE_CNTRL_REG          0x03
#define UART_MODEM_CONTROL_REG       0x04
#define UART_LINE_STATUS_REG         0x05
#define UART_MODEM_STATUS_REG        0x06
#define UART_SCRATCH_REG             0x07

    //INTERRUPT_STATUS_REG
#define UART_UARTINT                 0x80    
#define UART_RXWKINT                 0x02    

    //INT_ENABLE_REG
#define UART_EDSSI                   0x08    /* enable modem status interrupt */
#define UART_ELSI                    0x04    /* enable receiver line status interrupt */
#define UART_ETBEI                   0x02    /* enable transmitter holding register empty int. */
#define UART_ERBFI                   0x01    /* enable received data available interrupt */

    //INT_IDENT_REG
#define UART_FIFO_ENABLED            0xC0  
#define UART_IID_RLS                 0x06  
#define UART_IID_RDA                 0x04  
#define UART_IID_CTI                 0x0C  
#define UART_IID_THRE                0x02  
#define UART_IID_MS                  0x00  
#define UART_INTPEND                 0x01  
#define UART_IID_MASK                0x0F    

    //FIFO_CNTRL_REG
#define UART_FIFO_ENABLE             0x01    
#define UART_FIFO_TRIGGER_1          0x00    
#define UART_FIFO_TRIGGER_4          0x40
#define UART_FIFO_TRIGGER_8          0x80
#define UART_FIFO_TRIGGER_14         0xC0
#define UART_FIFO_XMIT_RESET         0x04    
#define UART_FIFO_RCV_RESET          0x02    

    //LINE_CNTRL_REG
#define UART_CLOCK_ENABLE            0x80    
#define UART_SET_BREAK               0x40    
#define UART_EVEN_PARITY_SELECT      0x10    
#define UART_PARITY_ENABLE           0x08    
#define UART_NO_PARITY               0x00
#define UART_NUM_STOP_BITS           0x04    
#define UART_ONE_STOP                0x00
#define UART_DATA_8_BITS             0x03


    //MODEM_CONTROL_REG
#define UART_RTS_AUTO_FLOW           0x40    
#define UART_CTS_AUTO_FLOW           0x20    
#define UART_LOOP                    0x10
#define UART_TOUT2                   0x08
#define UART_TOUT1                   0x04    
#define UART_RTS_ON                  0x02    
#define UART_DTR_ON                  0x01    

    //LINE_STATUS_REG
#define UART_LSR_EIRF                0x80    /* error receive fifo */
#define UART_LSR_TEMT                0x40   
#define UART_LSR_THRE                0x20    /* transmitter holding register empty */
#define UART_LSR_BI                  0x10 
#define UART_LSR_FE                  0x08 
#define UART_LSR_PE                  0x04 
#define UART_LSR_OE                  0x02 
#define UART_LSR_DR                  0x01 

    //MODEM_STATUS_REG
#define UART_MSR_DCD                 0x80
#define UART_MSR_RI                  0x40
#define UART_MSR_DSR                 0x20
#define UART_MSR_CTS                 0x10    
#define UART_MSR_DCTS                0x01    

    //UART_INIT_REG
#define UART_UART_INIT               0xC7    

#define GPS_FIFO_SIZE                  16

/* prototypes */
SDIO_STATUS IssueCMD52(PSDDEVICE     pDevice,
                       UINT8         FuncNo,
                       UINT32        Address,
                       PUCHAR        pData,
                       INT           ByteCount,
                       BOOL          Write);
#define ReadRegister(pDev,reg,pData) \
        SDLIB_IssueCMD52((pDev)->pSDDevice,SDDEVICE_GET_SDIO_FUNCNO((pDev)->pSDDevice),\
                          ((pDev)->UartRegOffset + (reg)),(pData),1,FALSE)
SDIO_STATUS WriteRegister(PSDGPS_DEVICE pDevice, UINT reg, UINT8 Data);
SDIO_STATUS WriteRegisterAsynch(PSDGPS_DEVICE pDevice, PSDREQUEST  pReq, void (*pComp)(PSDREQUEST pReq), UINT reg, UINT8 Data);
SDIO_STATUS GpsInitialize(PSDGPS_DEVICE pDevice);
void GpsDeinitialize(PSDGPS_DEVICE pDevice);
SDIO_STATUS GpsReceive(PSDGPS_DEVICE pDevice);
SDIO_STATUS GpsTransmit(PSDGPS_DEVICE pDevice);
SDIO_STATUS SetBaudRate(PSDGPS_DEVICE pDevice, UINT32 BaudRate);


#endif /* __GPS_H___*/

