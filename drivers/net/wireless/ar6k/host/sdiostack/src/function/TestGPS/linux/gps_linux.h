/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: gps_linux.h

@abstract: OS dedependent include for GPS SDIO function driver

#notes: 
 
@notice: Copyright (c), 2004 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __GPS_LINUX_H___
#define __GPS_LINUX_H___
#include <linux/serial_reg.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>

typedef struct _SDGPS_DEVICE {
    PSDDEVICE   pSDDevice;       /* the devices we are supporting */
    BOOL        HwReady;         /* hardware is setup and ready */
    struct uart_port Port;       /* uart port support */
    UINT32      UartRegOffset;   /* UART register offset */
    UINT32      UartMaxBaud;     /* maximum allowable baud rate */
    UINT16      UartDivisor;     /* base divisor at 1200 baud*/
    UINT8       InterruptEnable; /* state of IER register */
    OS_SEMAPHORE DeviceSem;      /* semaphore to protect the shadow registers and interrupts */
}SDGPS_DEVICE, *PSDGPS_DEVICE;


#endif /* __GPS_LINUX_H___*/
