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
@file: gps_os.c

@abstract: OS dependent GPS class SDIO function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 3;
#include "../../../include/ctsystem.h"
#include "../../../include/sdio_busdriver.h"
#include "../../../include/sdio_lib.h"
#include "../gps.h"

#include <linux/fs.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>

#define DESCRIPTION "SDIO GPS Function Driver"
#define AUTHOR "Atheros Communications, Inc."


/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");
int fixedbaud = 1;
module_param(fixedbaud, int, 0644);
MODULE_PARM_DESC(fixedbaud, "fixedbaud, if non-zero then no baud rate changes will be processed");


static BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static unsigned int gps_tx_empty(struct uart_port *port);
static void gps_set_mctrl(struct uart_port *port, unsigned int mctrl);
static unsigned int gps_get_mctrl(struct uart_port *port);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
static void gps_stop_tx(struct uart_port *port);
static void gps_start_tx(struct uart_port *port);
#else
static void gps_stop_tx(struct uart_port *port, unsigned int tty_stop);
static void gps_start_tx(struct uart_port *port, unsigned int tty_start);
#endif

static void gps_stop_rx(struct uart_port *port);
static void gps_enable_ms(struct uart_port *port);
static void gps_break_ctl(struct uart_port *port, int break_state);
static int gps_startup(struct uart_port *port);
static void gps_shutdown(struct uart_port *port);
static void gps_set_termios(struct uart_port *port, struct termios *termios,
                            struct termios *old);
static void gps_pm(struct uart_port *port, unsigned int state, unsigned int oldstate);
static const char *gps_type(struct uart_port *port);
static void gps_release_port(struct uart_port *port);
static int gps_request_port(struct uart_port *port);
static void gps_config_port(struct uart_port *port, int flags);
static int gps_verify_port(struct uart_port *port, struct serial_struct *ser);


/* devices we support, null terminated */
#define SDIO_GPS_CLASS  0x04
static SD_PNP_INFO Ids[] = {
   {.SDIO_FunctionClass = SDIO_GPS_CLASS}, /* SDIO-GPS SDIO standard interface code */
   {}
};

static struct uart_ops sops = {
    .tx_empty       = gps_tx_empty,
    .set_mctrl      = gps_set_mctrl,
    .get_mctrl      = gps_get_mctrl,
    .stop_tx        = gps_stop_tx,
    .start_tx       = gps_start_tx,
    .stop_rx        = gps_stop_rx,
    .enable_ms      = gps_enable_ms,
    .break_ctl      = gps_break_ctl,
    .startup        = gps_startup,
    .shutdown       = gps_shutdown,
    .set_termios    = gps_set_termios,
    .pm             = gps_pm,
    .type           = gps_type,
    .release_port   = gps_release_port,
    .request_port   = gps_request_port,
    .config_port    = gps_config_port,
    .verify_port    = gps_verify_port,
};

/* the driver context data */
static SDGPS_DRIVER_CONTEXT GpsContext = {
    .Function.pName                 = "sdio_gps",
    .Function.Version               = CT_SDIO_STACK_VERSION_CODE,
    .Function.MaxDevices            = 1,
    .Function.NumDevices            = 0,
    .Function.pIds                  = Ids,
    .Function.pProbe                = Probe,
    .Function.pRemove               = Remove,
    .Function.pSuspend              = NULL,
    .Function.pResume               = NULL,
    .Function.pWake                 = NULL,
    .Function.pContext              = &GpsContext, 
    .GpsDevice.Port.type            = PORT_16550,
    .GpsDevice.Port.uartclk         = GPS_MAX_BAUD_RATE*16,
    .GpsDevice.Port.fifosize        = GPS_FIFO_SIZE,
    .GpsDevice.Port.line            = 0,
    .GpsDevice.Port.ops             = &sops,
};

static struct uart_driver gps_uart = {
    .owner          =       THIS_MODULE,
    .driver_name    =       "ttyGPS",     /* not sure if this is proper naming convention */
    .dev_name       =       "ttyGPS",
    .devfs_name     =       "ttygps",
    .major          =       0, /*TTY_MAJOR,*/
    .minor          =       0, /*64,*/
    .nr             =       1,
    
    
};

    

/*
 * Probe - a device potentially for us
*/
static BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PSDGPS_DRIVER_CONTEXT pFunctionContext = 
                                (PSDGPS_DRIVER_CONTEXT)pFunction->pContext;
    SYSTEM_STATUS err = 0;
    SDIO_STATUS status;
      
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Probe - enter\n"));

    /* make sure this is a device we can handle */
    if (pDevice->pId[0].SDIO_FunctionClass == pFunctionContext->Function.pIds[0].SDIO_FunctionClass) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Probe - card matched (0x%X/0x%X/0x%X)\n",
                                pDevice->pId[0].SDIO_ManufacturerID,
                                pDevice->pId[0].SDIO_ManufacturerCode,
                                pDevice->pId[0].SDIO_FunctionNo));
            /* connect to the serial port driver */
        pFunctionContext->pSDDevice = pDevice;
        pFunctionContext->GpsDevice.pSDDevice = pDevice;
        pFunctionContext->GpsDevice.HwReady = FALSE;

        if (!SDIO_SUCCESS((status = GpsInitialize(&pFunctionContext->GpsDevice)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: Probe - could not initialize, %d\n", status));
            return FALSE;
        }  
        /* the port structure must be reset each time it is re-used */
        ZERO_OBJECT(pFunctionContext->GpsDevice.Port);
        pFunctionContext->GpsDevice.Port.type            = PORT_16550;
        pFunctionContext->GpsDevice.Port.uartclk         = GPS_MAX_BAUD_RATE*16;
        pFunctionContext->GpsDevice.Port.fifosize        = GPS_FIFO_SIZE;
        pFunctionContext->GpsDevice.Port.line            = 0;
        pFunctionContext->GpsDevice.Port.ops             = &sops;
        pFunctionContext->GpsDevice.Port.dev             = SD_GET_OS_DEVICE(pDevice); 
        pFunctionContext->GpsDevice.Port.uartclk         = pFunctionContext->GpsDevice.UartMaxBaud * 16;
//??        snprintf(gps_uart->tty_driver.devfs_name,sizeof(gps_uart->tty_driver.devfs_name), "sdgps_%s",
//??                (pFunctionContext->GpsDevice.Port.dev->bus_id);
//??        /* remove any colons */
//??        ReplaceChar(gps_uart->devfs_name, ':', '_'); 
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Probe - adding  bus_id: %s, driver name: %s\n", 
                pFunctionContext->GpsDevice.Port.dev->bus_id, pFunctionContext->GpsDevice.Port.dev->driver->name));
        err = uart_add_one_port(&gps_uart, &pFunctionContext->GpsDevice.Port);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: Probe - could not add uart port, %d\n", err));
            return FALSE;
        }
        return TRUE;
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Probe - not our card (0x%X/0x%X/0x%X)\n",
                                pDevice->pId[0].SDIO_ManufacturerID,
                                pDevice->pId[0].SDIO_ManufacturerCode,
                                pDevice->pId[0].SDIO_FunctionNo));
        return FALSE;
    }
}

/*
 * Remove - our device is being removed
*/
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PSDGPS_DRIVER_CONTEXT pFunctionContext = 
                             (PSDGPS_DRIVER_CONTEXT)pFunction->pContext;
    SYSTEM_STATUS err;
                                 
    DBG_PRINT(SDDBG_TRACE, ("+SDIO GPS Function: Remove - enter\n"));
    
    GpsDeinitialize(&pFunctionContext->GpsDevice);

    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: Remove - calling uart_remove_one_port\n"));
    err = uart_remove_one_port(&gps_uart, &pFunctionContext->GpsDevice.Port);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: Remove - could not remove uart port, %d\n", err));
    }
    pFunctionContext->pSDDevice = NULL;
    DBG_PRINT(SDDBG_TRACE, ("-SDIO GPS Function: Remove - exit\n"));
}

/*
 *  gps_tx_empty - transmitter fifo and shifter is empty
*/
static unsigned int gps_tx_empty(struct uart_port *port)
{
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    UINT8 data;
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_tx_empty - enter\n"));
    ReadRegister(pDevice, UART_LINE_STATUS_REG, &data);
    return (data & UART_LSR_TEMT)? TIOCSER_TEMT : 0;
}

/*
 * gps_set_mctrl - set modem control line
*/
static void gps_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_set_mctrl - enter\n"));
    return; /* fixed values */
}

/*
 * gps_get_mctrl - get modem control line
*/
static unsigned int gps_get_mctrl(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_get_mctrl - enter\n"));
    return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS; /* fixed */
}

/*
 * gps_stop_tx - stop output
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
static void gps_stop_tx(struct uart_port *port)
#else
static void gps_stop_tx(struct uart_port *port, unsigned int tty_stop)
#endif
{
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_stop_tx - enter\n"));
    if (pDevice->InterruptEnable & UART_ETBEI) {
        /* called with local ints disabled if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
            return;
        }  */
        pDevice->InterruptEnable &= ~UART_ETBEI;
        WriteRegisterAsynch(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable, pDevice->pRequest);
        /*SemaphorePost(&pDevice->DeviceSem);*/
    }
}

/*
 * gps_start_tx - start output
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
static void gps_start_tx(struct uart_port *port)
#else
static void gps_start_tx(struct uart_port *port, unsigned int tty_start)
#endif
{
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("+SDIO GPS Function: gps_start_tx - enter\n"));
    if (!(pDevice->InterruptEnable & UART_ETBEI)) {
        /* called with local ints disabledif (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
            return;
        }  */
        pDevice->InterruptEnable |= UART_ETBEI;
        WriteRegisterAsynch(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable, pDevice->pRequest);
        /*SemaphorePost(&pDevice->DeviceSem);*/
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO GPS Function: gps_start_tx\n"));
}

/*
 * gps_stop_rx - stop output
*/
static void gps_stop_rx(struct uart_port *port)
{
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_stop_rx - enter\n"));
    /* called with local ints disabled if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
        return;
    }  */
    pDevice->InterruptEnable &= ~UART_ELSI;
    WriteRegisterAsynch(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable, pDevice->pRequest);
    /*SemaphorePost(&pDevice->DeviceSem);*/
}

/*
 * gps_enable_ms - enable modem status interrupts
*/
static void gps_enable_ms(struct uart_port *port)
{
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_enable_ms - enter\n"));
    /* called with local ints disabledif (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
        return;
    } */ 
    pDevice->InterruptEnable |= UART_EDSSI;
    WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
    /*SemaphorePost(&pDevice->DeviceSem);*/
}

/*
 * gps_break_ctl - enable/disable breaks
*/
static void gps_break_ctl(struct uart_port *port, int break_state)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_break_ctl - enter\n"));
}

/*
 * gps_startup - initialization
*/
static int gps_startup(struct uart_port *port) 
{
    UINT8               temp;
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_startup - enter\n"));
//??    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
//??        return -EINTR;
//??    }  
            /* read registers to clear status */
    ReadRegister(pDevice, UART_LINE_STATUS_REG, &temp);
    ReadRegister(pDevice, UART_RECEIVE_REG, &temp);
    ReadRegister(pDevice, UART_INT_IDENT_REG, &temp);
    ReadRegister(pDevice, UART_MODEM_STATUS_REG, &temp);
    WriteRegister(pDevice, UART_FIFO_CNTRL_REG, UART_DATA_8_BITS | UART_FIFO_ENABLE | UART_FIFO_RCV_RESET);
    WriteRegister(pDevice, UART_FIFO_CNTRL_REG, UART_DATA_8_BITS | UART_FIFO_ENABLE);
    WriteRegister(pDevice, UART_LINE_CNTRL_REG, (UART_ONE_STOP | UART_NO_PARITY | UART_DATA_8_BITS));
    
    pDevice->InterruptEnable = UART_ERBFI | UART_ELSI;
    WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
//??    SemaphorePost(&pDevice->DeviceSem);
    return 0;
}

/*
 * gps_shutdown - 
*/
static void gps_shutdown(struct uart_port *port)
{
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("+SDIO GPS Function: gps_shutdown - enter\n"));
    //??if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
    //??    return;
    //??}  
    pDevice->InterruptEnable = 0;
    WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
    //??SemaphorePost(&pDevice->DeviceSem);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO GPS Function: gps_shutdown\n"));
}

/*
 * gps_set_termios - set data parameters
*/
static void gps_set_termios(struct uart_port *port, struct termios *termios,
                            struct termios *old)        
{
    PSDGPS_DEVICE pDevice = CONTAINING_STRUCT(port, SDGPS_DEVICE, Port);
    unsigned char cval = 0;
    unsigned int baudrate;

    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_set_termios - enter\n"));

    port->read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;//???
    
    switch (termios->c_cflag & CSIZE) {
        case CS5:
            cval = 0x00;
            break;
        case CS6:
            cval = 0x01;
            break;
        case CS7:
            cval = 0x02;
            break;
        default:
        case CS8:
            cval = 0x03;
            break;
    }
 
    cval |= (termios->c_cflag & PARENB)? UART_PARITY_ENABLE : 0;
    cval |= (termios->c_cflag & CSTOPB)? UART_NUM_STOP_BITS : 0;
    cval |= (!(termios->c_cflag & PARODD))? UART_EVEN_PARITY_SELECT : 0;
   //?? WriteRegister(pDevice, UART_LINE_CNTRL_REG, cval);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: LCR 0x%X\n", cval));
   
    /* get serial port to figure out the baud rate */
    baudrate = uart_get_baud_rate(port, termios, old, 0, port->uartclk/16); 
    DBG_ASSERT(baudrate != 0);
    if (fixedbaud == 0) {
        SetBaudRate(pDevice, baudrate);
        uart_update_timeout(port, termios->c_cflag, baudrate);
    }


    //??do something about this locking
    /* maybe called with local ints disabledif (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
        return;
    }  */
//??    pDevice->InterruptEnable &= ~UART_EDSSI;
//??    if (UART_ENABLE_MS(&port, termios->c_cflag)) {
//??        pDevice->InterruptEnable |= UART_EDSSI;
//??    }
    gps_set_mctrl(&pDevice->Port, pDevice->Port.mctrl);
    
   /* SemaphorePost(&pDevice->DeviceSem);*/
}

/*
 * gps_pm - power management
*/
static void gps_pm(struct uart_port *port, unsigned int state, unsigned int oldstate)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_pm - state: %d\n", state));
    return;
}

/*
 * gps_type - retrieve type string
*/
static const char *gps_type(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_type - enter\n"));
    return GPS_TYPE;
}

/*
 * gps_release_port - release resources
*/
static void gps_release_port(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_release_port - enter\n"));
    return;
}

/*
 * gps_request_port - request resources
*/
static int gps_request_port(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_request_port - enter\n"));
    return 0;
}

/*
 * gps_config_port - auto configuration
*/
static void gps_config_port(struct uart_port *port, int flags)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_config_port - enter\n"));
    return;
}

/*
 * gps_verify_port - verify configuration of port
*/
static int gps_verify_port(struct uart_port *port, struct serial_struct *ser)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: gps_verify_port - enter\n"));
    return 0;
}

/*
 *  GpsReceive
*/
SDIO_STATUS GpsReceive(PSDGPS_DEVICE pDevice)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT8 statusReg;
    UINT8 inChar;
    UINT maxCount = 64;
    struct uart_port *port = &pDevice->Port;
    struct tty_struct *tty;

    tty = ((port == NULL) || (port->info == NULL)) ? NULL : port->info->tty;
    /* if we aren't ready, just clear out the incoming char */
    if ((port == NULL) || (tty == NULL) || (tty->flip.char_buf_ptr == NULL)) {
        ReadRegister(pDevice, UART_RECEIVE_REG, &inChar);
        WriteRegister(pDevice, UART_FIFO_CNTRL_REG, UART_DATA_8_BITS | UART_FIFO_ENABLE | UART_FIFO_RCV_RESET);
        WriteRegister(pDevice, UART_FIFO_CNTRL_REG, UART_DATA_8_BITS | UART_FIFO_ENABLE);        
        DBG_PRINT(SDIO_GPS_TRACE_INT, ("+RX%X\n", (UINT)inChar));
        return status;
    }
    for (status = ReadRegister(pDevice, UART_LINE_STATUS_REG, &statusReg);
         SDIO_SUCCESS(status) && (statusReg & UART_LSR_DR) && (maxCount-- > 0);
         status = ReadRegister(pDevice, UART_LINE_STATUS_REG, &statusReg)) {
        if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
            tty->flip.work.func((void *)tty);
            if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
                DBG_PRINT(SDDBG_WARN, ("SDIO GPS Function: gps_receive - tty buffer problem\n"));
                return SDIO_STATUS_ERROR;
            }
        }
        ReadRegister(pDevice, UART_RECEIVE_REG, &inChar);
        *tty->flip.flag_buf_ptr = TTY_NORMAL;
        *tty->flip.char_buf_ptr = inChar;
        tty->flip.flag_buf_ptr++;
        tty->flip.char_buf_ptr++;
        tty->flip.count++;
        port->icount.rx++;
        if ((status & UART_LSR_OE) &&
            (tty->flip.count < TTY_FLIPBUF_SIZE)) {
             /* overrrun condition */
            *tty->flip.flag_buf_ptr = TTY_OVERRUN;
            tty->flip.flag_buf_ptr++;
            tty->flip.char_buf_ptr++;
            tty->flip.count++;        
            DBG_PRINT(SDDBG_WARN, ("SDIO GPS Function: GpsReceive overrun\n"));
        }
        DBG_PRINT(SDIO_GPS_TRACE_INT, ("%c|,", inChar));
    }
    tty_flip_buffer_push(tty);
    return status;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
#define GPS_STOP_TX(p)	   gps_stop_tx((p))
#else
#define GPS_STOP_TX(p)	   gps_stop_tx((p),0)
#endif

/*
 *  GpsTransmit
*/
SDIO_STATUS GpsTransmit(PSDGPS_DEVICE pDevice)
{
    UINT ii;
    struct uart_port *port = &pDevice->Port;
    struct circ_buf *xmit;

    xmit = ((port == NULL) || (port->info == NULL)) ? NULL : &port->info->xmit;
    /* if we aren't ready, just clear interrupt */
    if (xmit == NULL) {
		GPS_STOP_TX(port);
       	return SDIO_STATUS_SUCCESS;
    }

     if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
        GPS_STOP_TX(port);
        return SDIO_STATUS_SUCCESS;
    }
    
    for(ii = 0; ii < GPS_FIFO_SIZE; ii++) {
        WriteRegister(pDevice, UART_TX, xmit->buf[xmit->tail]);
        xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
        port->icount.tx++;
        if (uart_circ_empty(xmit)) {
            break;
        }
        
    }
    if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
        uart_write_wakeup(port);
    }
    if (uart_circ_empty(xmit)) {
        GPS_STOP_TX(port);
    }
    
    return SDIO_STATUS_SUCCESS;
}

/*
 * module init
*/
static int __init sdio_gps_init(void) {
    SDIO_STATUS status; 
    SYSTEM_STATUS err;
    
    REL_PRINT(SDDBG_TRACE, ("SDIO GPS Function: init\n"));
    /* register with the serial driver core */
    err = uart_register_driver(&gps_uart);
    if (err < 0) { 
        DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: failed to register with uart driver, %d\n", err));
        return err;
    }
    /* register with bus driver core */
    if (!SDIO_SUCCESS((status = SDIO_RegisterFunction(&GpsContext.Function)))) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO GPS Function: failed to register with bus driver, %d\n", status));
        uart_unregister_driver(&gps_uart);
        return SDIOErrorToOSError(status);
    }
    return err;
}

/*
 * module cleanup
*/
static void __exit sdio_gps_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("SDIO GPS Function: : cleanup\n"));
    SDIO_UnregisterFunction(&GpsContext.Function);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: SDIO unregistered\n"));
    /* unregister with the serial driver core */
    uart_unregister_driver(&gps_uart);
    DBG_PRINT(SDDBG_TRACE, ("SDIO GPS Function: UART unregistered\n"));
}


// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_gps_init);
module_exit(sdio_gps_cleanup);
