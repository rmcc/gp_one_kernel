
/*
 * Copyright (c) 2004-2006 Atheros Communications Inc.
 * All rights reserved.
 *
 *
 * 
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
 * 
 */
 
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

#include <a_config.h>
#include <a_osapi.h>
#include <a_types.h>
#include <athdefs.h>
#include <athdrv_linux.h>

const char *progname;

/* Force Target to reset */

int reset_request = AR6000_XIOCTL_FORCE_TARGET_RESET;

int
main (int argc, char **argv)
{
    int s;
    char ifname[IFNAMSIZ];
    progname = argv[0];
    struct ifreq ifr;

    if(argc == 1 )
    {
        printf("Error : specify the ethernet interface\n"
               "Usage : forcereset <intf> \n");
        exit(1);
    }

    memset(ifname, '\0', IFNAMSIZ);
    strcpy(ifname, argv[1]);
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        err(1, "socket");
    }

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_data = (char *)&reset_request;

    printf("Force Target to RESET....\n");

    if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
        err(1, ifr.ifr_name);
        exit (1);
    }

    printf("Target has been reset\n");

    exit(0);
}

