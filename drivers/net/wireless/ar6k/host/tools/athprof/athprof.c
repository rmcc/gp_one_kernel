
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
#include <wmi.h>
#include <wmix.h>

/* Support for Target-side profiling */

const char *prog_name;
struct ifreq ifr;
int sock;
int debug;

#define DPRINTF(args...) if (debug) {printf(args);}

const char cmd_args[] =
"arguments:\n\
  --addr=address\n\
  --bins=number-of-bins --period=sampling-period (in 1/32768 second units)\n\
  --counts  (show address/count pairs after profiling is done)\n\
  --Addresses (use before --counts to display addresses as well as counts)\n\
  --interface=interface_name (e.g. eth1)\n\
  --start\n\
  --stop\n\
\n";

void
usage(void)
{
    fprintf(stderr, "usage:\n%s arguments...\n", prog_name);
    fprintf(stderr, "%s\n", cmd_args);
    exit(1);
}

void
do_ioctl(void *ioctl_data)
{
    ifr.ifr_data = (char *)ioctl_data;

    if (ioctl(sock, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
        err(1, ifr.ifr_name);
        exit (1);
    }
}

/*
 * Send a WMI message to start target-side profiling.
 */
void
athprof_start(void)
{
    int prof_start_cmd = AR6000_XIOCTL_PROF_START;

    DPRINTF("athprof_start\n");
    do_ioctl(&prof_start_cmd);
}

/*
 * Send a WMI message to stop target-side profiling.
 */
void
athprof_stop(void)
{
    int prof_stop_cmd = AR6000_XIOCTL_PROF_STOP;

    DPRINTF("athprof_stop\n");
    do_ioctl(&prof_stop_cmd);
}

/*
 * Send a WMI message to configure target-side profiling.
 */
void
athprof_cfg(unsigned int period, unsigned int nbins)
{
    struct {
        unsigned int cmdid;
        WMIX_PROF_CFG_CMD wmix;
    } prof_cfg_cmd;

    DPRINTF("athprof_cfg: period=0x%x  nbins=%d\n", period, nbins);

    prof_cfg_cmd.cmdid       = AR6000_XIOCTL_PROF_CFG;
    prof_cfg_cmd.wmix.period = period;
    prof_cfg_cmd.wmix.nbins  = nbins;

    do_ioctl(&prof_cfg_cmd);
}

/*
 * Send a WMI message to set the start address for the next
 * bin for target-side profiling.
 */
void
athprof_addr_set(unsigned int addr)
{
    struct {
        unsigned int cmdid;
        WMIX_PROF_ADDR_SET_CMD wmix;
    } prof_addr_set_cmd;

    DPRINTF("athprof_addr_set: addr=0x%x\n", addr);

    prof_addr_set_cmd.cmdid      = AR6000_XIOCTL_PROF_ADDR_SET;
    prof_addr_set_cmd.wmix.addr  = addr;

    do_ioctl(&prof_addr_set_cmd);
}

/*
 * Send a WMI message to retrieve the address and histogram
 * count for the next target-side profiling bin.
 */
void
athprof_count_get(unsigned int *addr, unsigned int *count)
{
    struct {
        unsigned int cmdid;
        WMIX_PROF_COUNT_EVENT wmix;
    } prof_count_get_cmd;

    DPRINTF("athprof_count_get\n");

    prof_count_get_cmd.cmdid      = AR6000_XIOCTL_PROF_COUNT_GET;

    do_ioctl(&prof_count_get_cmd);

    *addr  = (unsigned int)prof_count_get_cmd.wmix.addr;
    *count = (unsigned int)prof_count_get_cmd.wmix.count;
}

int
main (int argc, char **argv)
{
    unsigned int period = 0;
    unsigned int nbins = 0;
    int c;
    int noaddr_flag = 1; /* By default, don't show addresses */

    prog_name = argv[0];

    if (argc == 1) {
        usage();
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        err(1, "socket");
    }

    memset(ifr.ifr_name, '\0', sizeof(ifr.ifr_name));
    strcpy(ifr.ifr_name, "eth1"); /* default */

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"addr", 1, NULL, 'a'},
            {"Addresses", 0, NULL, 'A'},
            {"bins", 1, NULL, 'b'},
            {"counts", 0, NULL, 'c'},
            {"debug", 0, NULL, 'd'},
            {"interface", 1, NULL, 'i'},
            {"period", 1, NULL, 'p'},
            {"stop", 0, NULL, 'q'},
            {"start", 0, NULL, 's'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "Aa:b:cdi:p:sq",
                         long_options, &option_index);
        if (c == -1) {
            break;
        }

        /*
         * NB: Some command options are acted upon immediately,
         * so order of options may matter.
         */

        switch (c) {

        case 'A':
        {
            noaddr_flag = 0;
            break;
        }
        case 'a':
        {
            unsigned int addr = strtoul(optarg, NULL, 0);
            athprof_addr_set(addr);
            break;
        }

        case 'b':
        {
            nbins = strtoul(optarg, NULL, 0);
            if (period) {
                athprof_cfg(period, nbins);
            }
            break;
        }

        case 'c':
        {
            unsigned int addr;
            unsigned int count;

            /*
             * Stop (just in case it's still profiling).  Also this
             * has a side-effect of resetting the "get position".
             */
            athprof_stop();
            athprof_count_get(&addr, &count);
            if (count != 0) {
                fprintf(stderr, "Warning: Some LOW PC hits (%d)\n", count);
            }
            for (;;) {
                athprof_count_get(&addr, &count);
                if (addr == 0xffffffff) {
                    if (count != 0) {
                        fprintf(stderr, "Warning: Some HIGH PC hits (%d)\n", count);
                    }
                    break;
                }

                if (noaddr_flag) {
                    printf("%d\n", count);
                } else {
                    printf("%d 0x%x\n", count, addr);
                }
            }
            break;
        }

        case 'd':
        {
            debug = 1;
            break;
        }

        case 'i':
        {
            memset(ifr.ifr_name, '\0', sizeof(ifr.ifr_name));
            strncpy(ifr.ifr_name, optarg, sizeof(ifr.ifr_name));
            break;
        }

        case 'p':
        {
            period = strtoul(optarg, NULL, 0);
            if (nbins) {
                athprof_cfg(period, nbins);
            }
            break;
        }

        case 'q':
        {
            athprof_stop();
            break;
        }

        case 's':
        {
            athprof_start();
            break;
        }


        default:
        {
            usage();
            break;
        }
        }
    }

    exit(0);
}
