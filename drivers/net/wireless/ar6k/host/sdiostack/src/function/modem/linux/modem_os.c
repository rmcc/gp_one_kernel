/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: modem_os.c

@abstract: OS dependent Modem SDIO function driver

#notes: 
 
@notice: Copyright (c), 2004-2005 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 3;
#include "../../../include/ctsystem.h"
#include "../../../include/sdio_busdriver.h"
#include "../../../include/sdio_lib.h"
#include "../modem.h"

#include <linux/fs.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>

#define DESCRIPTION "SDIO MODEM Function Driver"
#define AUTHOR "Atheros Communications, Inc."


/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");
int fixedbaud = 1;
module_param(fixedbaud, int, 0644);
MODULE_PARM_DESC(fixedbaud, "fixedbaud, if non-zero then no baud rate changes will be processed");


static BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static unsigned int modem_tx_empty(struct uart_port *port);
static void modem_set_mctrl(struct uart_port *port, unsigned int mctrl);
static unsigned int modem_get_mctrl(struct uart_port *port);
static void modem_stop_tx(struct uart_port *port, unsigned int tty_stop);
static void modem_start_tx(struct uart_port *port, unsigned int tty_start);
static void modem_stop_rx(struct uart_port *port);
static void modem_enable_ms(struct uart_port *port);
static void modem_break_ctl(struct uart_port *port, int break_state);
static int modem_startup(struct uart_port *port);
static void modem_shutdown(struct uart_port *port);
static void modem_set_termios(struct uart_port *port, struct termios *termios,
                            struct termios *old);
static void modem_pm(struct uart_port *port, unsigned int state, unsigned int oldstate);
static const char *modem_type(struct uart_port *port);
static void modem_release_port(struct uart_port *port);
static int modem_request_port(struct uart_port *port);
static void modem_config_port(struct uart_port *port, int flags);
static int modem_verify_port(struct uart_port *port, struct serial_struct *ser);


/* devices we support, null terminated */
static SD_PNP_INFO Ids[] = {
   {.SDIO_ManufacturerID = 0x22E,  
     .SDIO_ManufacturerCode = 0x104, 
    }, 
   {}
};

static struct uart_ops sops = {
    .tx_empty       = modem_tx_empty,
    .set_mctrl      = modem_set_mctrl,
    .get_mctrl      = modem_get_mctrl,
    .stop_tx        = modem_stop_tx,
    .start_tx       = modem_start_tx,
    .stop_rx        = modem_stop_rx,
    .enable_ms      = modem_enable_ms,
    .break_ctl      = modem_break_ctl,
    .startup        = modem_startup,
    .shutdown       = modem_shutdown,
    .set_termios    = modem_set_termios,
    .pm             = modem_pm,
    .type           = modem_type,
    .release_port   = modem_release_port,
    .request_port   = modem_request_port,
    .config_port    = modem_config_port,
    .verify_port    = modem_verify_port,
};

/* the driver context data */
static SDMODEM_DRIVER_CONTEXT ModemContext = {
    .Function.pName                 = "sdio_modem",
    .Function.Version               = CT_SDIO_STACK_VERSION_CODE,
    .Function.MaxDevices            = 1,
    .Function.NumDevices            = 0,
    .Function.pIds                  = Ids,
    .Function.pProbe                = Probe,
    .Function.pRemove               = Remove,
    .Function.pSuspend              = NULL,
    .Function.pResume               = NULL,
    .Function.pWake                 = NULL,
    .Function.pContext              = &ModemContext, 
    .ModemDevice.Port.type            = PORT_16550,
    .ModemDevice.Port.uartclk         = MODEM_MAX_BAUD_RATE*16,
    .ModemDevice.Port.fifosize        = MODEM_FIFO_SIZE,
    .ModemDevice.Port.line            = 0,
    .ModemDevice.Port.ops             = &sops,
};

static struct uart_driver modem_uart = {
    .owner          =       THIS_MODULE,
    .driver_name    =       "ttyMODEM",     /* not sure if this is proper naming convention */
    .dev_name       =       "ttyMODEM",
    .major          =       0, /*TTY_MAJOR,*/
    .minor          =       0, /*64,*/
    .nr             =       1,
    
};

    

/*
 * Probe - a device potentially for us
*/
static BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PSDMODEM_DRIVER_CONTEXT pFunctionContext = 
                                (PSDMODEM_DRIVER_CONTEXT)pFunction->pContext;
    SYSTEM_STATUS err = 0;
    SDIO_STATUS status;
      
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Probe - enter\n"));

    /* make sure this is a device we can handle */
    if (pDevice->pId[0].SDIO_FunctionClass == pFunctionContext->Function.pIds[0].SDIO_FunctionClass) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Probe - card matched (0x%X/0x%X/0x%X)\n",
                                pDevice->pId[0].SDIO_ManufacturerID,
                                pDevice->pId[0].SDIO_ManufacturerCode,
                                pDevice->pId[0].SDIO_FunctionNo));
            /* connect to the serial port driver */
        pFunctionContext->pSDDevice = pDevice;
        pFunctionContext->ModemDevice.pSDDevice = pDevice;
        pFunctionContext->ModemDevice.HwReady = FALSE;

        if (!SDIO_SUCCESS((status = ModemInitialize(&pFunctionContext->ModemDevice)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: Probe - could not initialize, %d\n", status));
            return FALSE;
        }  
        /* the port structure must be reset each time it is re-used */
        ZERO_OBJECT(pFunctionContext->ModemDevice.Port);
        pFunctionContext->ModemDevice.Port.type            = PORT_16550;
        pFunctionContext->ModemDevice.Port.uartclk         = MODEM_MAX_BAUD_RATE*16;
        pFunctionContext->ModemDevice.Port.fifosize        = MODEM_FIFO_SIZE;
        pFunctionContext->ModemDevice.Port.line            = 0;
        pFunctionContext->ModemDevice.Port.ops             = &sops;
        pFunctionContext->ModemDevice.Port.dev = &pDevice->Device; 
        pFunctionContext->ModemDevice.Port.uartclk = pFunctionContext->ModemDevice.UartMaxBaud * 16;
        err = uart_add_one_port(&modem_uart, &pFunctionContext->ModemDevice.Port);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: Probe - could not add uart port, %d\n", err));
            return FALSE;
        }
        return TRUE;
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Probe - not our card (0x%X/0x%X/0x%X)\n",
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
    PSDMODEM_DRIVER_CONTEXT pFunctionContext = 
                             (PSDMODEM_DRIVER_CONTEXT)pFunction->pContext;
    SYSTEM_STATUS err;
                                 
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MODEM Function: Remove - enter\n"));
    
    ModemDeinitialize(&pFunctionContext->ModemDevice);

    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: Remove - calling uart_remove_one_port\n"));
    err = uart_remove_one_port(&modem_uart, &pFunctionContext->ModemDevice.Port);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: Remove - could not remove uart port, %d\n", err));
    }
    pFunctionContext->pSDDevice = NULL;
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MODEM Function: Remove - exit\n"));
}

/*
 *  modem_tx_empty - transmitter fifo and shifter is empty
*/
static unsigned int modem_tx_empty(struct uart_port *port)
{
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    UINT8 data;
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_tx_empty - enter\n"));
    ReadRegister(pDevice, UART_LINE_STATUS_REG, &data);
    return (data & UART_LSR_TEMT)? TIOCSER_TEMT : 0;
}

/*
 * modem_set_mctrl - set modem control line
*/
static void modem_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_set_mctrl - enter\n"));
    return; /* fixed values */
}

/*
 * modem_get_mctrl - get modem control line
*/
static unsigned int modem_get_mctrl(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_get_mctrl - enter\n"));
    return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS; /* fixed */
}

/*
 * modem_stop_tx - stop output
*/
static void modem_stop_tx(struct uart_port *port, unsigned int tty_stop)
{
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_stop_tx - enter\n"));
    if (pDevice->InterruptEnable & UART_ETBEI) {
        /* called with local ints disabled if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
            return;
        }  */
        pDevice->InterruptEnable &= ~UART_ETBEI;
        WriteRegisterAsynch(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
        /*SemaphorePost(&pDevice->DeviceSem);*/
    }
}

/*
 * modem_start_tx - start output
*/
static void modem_start_tx(struct uart_port *port, unsigned int tty_start)
{
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MODEM Function: modem_start_tx - enter\n"));
    if (!(pDevice->InterruptEnable & UART_ETBEI)) {
        /* called with local ints disabledif (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
            return;
        }  */
        pDevice->InterruptEnable |= UART_ETBEI;
        WriteRegisterAsynch(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
        /*SemaphorePost(&pDevice->DeviceSem);*/
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MODEM Function: modem_start_tx\n"));
}

/*
 * modem_stop_rx - stop output
*/
static void modem_stop_rx(struct uart_port *port)
{
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_stop_rx - enter\n"));
    /* called with local ints disabled if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
        return;
    }  */
    pDevice->InterruptEnable &= ~UART_ELSI;
    WriteRegisterAsynch(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
    /*SemaphorePost(&pDevice->DeviceSem);*/
}

/*
 * modem_enable_ms - enable modem status interrupts
*/
static void modem_enable_ms(struct uart_port *port)
{
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_enable_ms - enter\n"));
    /* called with local ints disabledif (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
        return;
    } */ 
    pDevice->InterruptEnable |= UART_EDSSI;
    WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
    /*SemaphorePost(&pDevice->DeviceSem);*/
}

/*
 * modem_break_ctl - enable/disable breaks
*/
static void modem_break_ctl(struct uart_port *port, int break_state)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_break_ctl - enter\n"));
}

/*
 * modem_startup - initialization
*/
static int modem_startup(struct uart_port *port) 
{
    UINT8               temp;
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_startup - enter\n"));
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
 * modem_shutdown - 
*/
static void modem_shutdown(struct uart_port *port)
{
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MODEM Function: modem_shutdown - enter\n"));
    //??if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pDevice->DeviceSem))) {
    //??    return;
    //??}  
    pDevice->InterruptEnable = 0;
    WriteRegister(pDevice, UART_INT_ENABLE_REG, pDevice->InterruptEnable);
    //??SemaphorePost(&pDevice->DeviceSem);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MODEM Function: modem_shutdown\n"));
}

/*
 * modem_set_termios - set data parameters
*/
static void modem_set_termios(struct uart_port *port, struct termios *termios,
                            struct termios *old)        
{
    PSDMODEM_DEVICE pDevice = CONTAINING_STRUCT(port, SDMODEM_DEVICE, Port);
    unsigned char cval = 0;
    unsigned int baudrate;

    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_set_termios - enter\n"));

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
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: LCR 0x%X\n", cval));
   
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
    modem_set_mctrl(&pDevice->Port, pDevice->Port.mctrl);
    
   /* SemaphorePost(&pDevice->DeviceSem);*/
}

/*
 * modem_pm - power management
*/
static void modem_pm(struct uart_port *port, unsigned int state, unsigned int oldstate)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_pm - state: %d\n", state));
    return;
}

/*
 * modem_type - retrieve type string
*/
static const char *modem_type(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_type - enter\n"));
    return MODEM_TYPE;
}

/*
 * modem_release_port - release resources
*/
static void modem_release_port(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_release_port - enter\n"));
    return;
}

/*
 * modem_request_port - request resources
*/
static int modem_request_port(struct uart_port *port)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_request_port - enter\n"));
    return 0;
}

/*
 * modem_config_port - auto configuration
*/
static void modem_config_port(struct uart_port *port, int flags)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_config_port - enter\n"));
    return;
}

/*
 * modem_verify_port - verify configuration of port
*/
static int modem_verify_port(struct uart_port *port, struct serial_struct *ser)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: modem_verify_port - enter\n"));
    return 0;
}

/*
 *  ModemReceive
*/
SDIO_STATUS ModemReceive(PSDMODEM_DEVICE pDevice)
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
        DBG_PRINT(SDIO_MODEM_TRACE_INT, ("+RX%X\n", (UINT)inChar));
        return status;
    }
    for (status = ReadRegister(pDevice, UART_LINE_STATUS_REG, &statusReg);
         SDIO_SUCCESS(status) && (statusReg & UART_LSR_DR) && (maxCount-- > 0);
         status = ReadRegister(pDevice, UART_LINE_STATUS_REG, &statusReg)) {
        if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
            tty->flip.work.func((void *)tty);
            if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
                DBG_PRINT(SDDBG_WARN, ("SDIO MODEM Function: modem_receive - tty buffer problem\n"));
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
            DBG_PRINT(SDDBG_WARN, ("SDIO MODEM Function: ModemReceive overrun\n"));
        }
        DBG_PRINT(SDIO_MODEM_TRACE_INT, ("%c|,", inChar));
    }
    tty_flip_buffer_push(tty);
    return status;
}

/*
 *  ModemTransmit
*/
SDIO_STATUS ModemTransmit(PSDMODEM_DEVICE pDevice)
{
    UINT ii;
    struct uart_port *port = &pDevice->Port;
    struct circ_buf *xmit;

    xmit = ((port == NULL) || (port->info == NULL)) ? NULL : &port->info->xmit;
    /* if we aren't ready, just clear interrupt */
    if (xmit == NULL) {
       modem_stop_tx(port, 0);
       return SDIO_STATUS_SUCCESS;
    }

     if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
        modem_stop_tx(port, 0);
        return SDIO_STATUS_SUCCESS;
    }
    
    for(ii = 0; ii < MODEM_FIFO_SIZE; ii++) {
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
        modem_stop_tx(port, 0);
    }
    
    return SDIO_STATUS_SUCCESS;
}

/*
 * module init
*/
static int __init sdio_modem_init(void) {
    SDIO_STATUS status; 
    SYSTEM_STATUS err;
    
    REL_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: init\n"));
    /* register with the serial driver core */
    err = uart_register_driver(&modem_uart);
    if (err < 0) { 
        DBG_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: failed to register with uart driver, %d\n", err));
        return err;
    }
    /* register with bus driver core */
    if (!SDIO_SUCCESS((status = SDIO_RegisterFunction(&ModemContext.Function)))) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MODEM Function: failed to register with bus driver, %d\n", status));
        uart_unregister_driver(&modem_uart);
        return SDIOErrorToOSError(status);
    }
    return err;
}

/*
 * module cleanup
*/
static void __exit sdio_modem_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("SDIO MODEM Function: : cleanup\n"));
    SDIO_UnregisterFunction(&ModemContext.Function);
    /* unregister with the serial driver core */
    uart_unregister_driver(&modem_uart);
}


MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_modem_init);
module_exit(sdio_modem_cleanup);
