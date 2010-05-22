//------------------------------------------------------------------------------
// <copyright file="gpiotest.c" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
// 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// Small unit test/example for General Purpose I/O
//
// Author(s): ="Atheros"
//==============================================================================
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <err.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/wireless.h>

#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
#include "athdrv.h"

#include "gpio.h"

struct {
    unsigned int cmd;
    struct ar6000_gpio_intr_wait_cmd_s desc;
} intr_wait;

struct {
    unsigned int cmd;
    struct ar6000_gpio_intr_ack_cmd_s desc;
} intr_ack;

struct {
    unsigned int cmd;
    struct ar6000_gpio_register_cmd_s desc;
} reg_get, reg_set;

struct {
    unsigned int cmd;
    struct ar6000_gpio_output_set_cmd_s desc;
} out_set;

struct {
    unsigned int cmd;
    struct ar6000_gpio_input_get_cmd_s desc;
} in_get;

char ifname[IFNAMSIZ];
struct ifreq request_ifr;
int s;

int
main (int argc, char **argv)
{
    memset(ifname, '\0', IFNAMSIZ);
    strcpy(ifname, "eth1");
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        err(1, "socket");
    }

    strncpy(request_ifr.ifr_name, ifname, sizeof(request_ifr.ifr_name));

    intr_wait.cmd = AR6000_XIOCTL_GPIO_INTR_WAIT;
    intr_ack.cmd = AR6000_XIOCTL_GPIO_INTR_ACK;
    reg_set.cmd = AR6000_XIOCTL_GPIO_REGISTER_SET;
    reg_get.cmd = AR6000_XIOCTL_GPIO_REGISTER_GET;
    out_set.cmd = AR6000_XIOCTL_GPIO_OUTPUT_SET;
    in_get.cmd = AR6000_XIOCTL_GPIO_INPUT_GET;

    /* Read GPIO input values */
    request_ifr.ifr_data = (unsigned char *)&in_get;
    if (ioctl(s, AR6000_IOCTL_EXTENDED, &request_ifr) < 0) {
        err(1, request_ifr.ifr_name);
    }
    printf("Current input values=0x%x\n", in_get.desc.input_value);

    /* Set GPIO output values */
    request_ifr.ifr_data = (unsigned char *)&out_set;
    out_set.desc.set_mask   = 0x0000000f;
    out_set.desc.clear_mask = 0x000000f0;
    out_set.desc.enable_mask = 0x0000ffff;
    out_set.desc.disable_mask = 0x00030000;
    if (ioctl(s, AR6000_IOCTL_EXTENDED, &request_ifr) < 0) {
        err(1, request_ifr.ifr_name);
    }

    /* Read GPIO_PIN4 configuration */
    request_ifr.ifr_data = (unsigned char *)&reg_get;
    reg_get.desc.gpioreg_id = GPIO_ID_PIN(4);
    if (ioctl(s, AR6000_IOCTL_EXTENDED, &request_ifr) < 0) {
        err(1, request_ifr.ifr_name);
    }
    printf("Current value of pin4 is 0x%x\n", reg_get.desc.value);

    if (!(reg_get.desc.value & 0x00000380)) {
        /* Write GPIO_PING4 configuration */
        printf("Set pin4 to 0x00000200\n");
        request_ifr.ifr_data = (unsigned char *)&reg_set;
        reg_set.desc.gpioreg_id = GPIO_ID_PIN(4);
        reg_set.desc.value = 0x00000200;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &request_ifr) < 0) {
            err(1, request_ifr.ifr_name);
        }
    }

    for(;;) {
        /* WAIT for a GPIO interrupt */
        request_ifr.ifr_data = (unsigned char *)&intr_wait;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &request_ifr) < 0) {
            err(1, request_ifr.ifr_name);
        }
        printf("GPIO interrupt(s) received.  intr_mask=0x%x input_values=0x%x\n",
                intr_wait.desc.intr_mask, intr_wait.desc.input_values);

        /* ACK all GPIO interrupts received */
        request_ifr.ifr_data = (unsigned char *)&intr_ack;
        intr_ack.desc.ack_mask = intr_wait.desc.intr_mask;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &request_ifr) < 0) {
            err(1, request_ifr.ifr_name);
        }
    }
}
