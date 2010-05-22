//------------------------------------------------------------------------------
// <copyright file="dkbmiloader.c" company="Atheros">
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
// Author(s): ="Atheros"
//==============================================================================
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
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
#include <athdefs.h>
#include <a_types.h>
#include "athdrv.h"
#include "AR6000/AR6000_bmi.h"
#include "../sdio_driver/dksdio.h"

#define ADDRESS_FLAG                    0x01
#define LENGTH_FLAG                     0x02
#define PARAM_FLAG                      0x04
#define FILE_FLAG                       0x08
#define COUNT_FLAG                      0x10

#define BMI_TEST                        BMI_NO_COMMAND

const char *progname;
const char commands[] =
"commands:\n\
--get --address=<register address>\n\
--set --address=<register address> --param=<register value>\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
--write --address=<target address> --file=<filename>\n\
--execute --address=<function start address> --param=<input param>\n\
--begin --address=<function start address>\n\
--test --address=<target address> --length=<cmd size> --count=<iterations>\n\
--done\n\
The options can also be given in the abbreviated form --option=x or -o x. The options can be given in any order";

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}

int
main (int argc, char **argv) {
    int c, s, fd;
    unsigned int address, length;
    unsigned int count, param;
    char filename[16], ifname[IFNAMSIZ];
    unsigned int cmd;
    DK_IDATA ifr;
    unsigned char *buffer;
    struct stat filestat;
    int flag;

    progname = argv[0];
    if (argc == 1) usage();

    flag = 0;
    memset(filename, '\0', 16);
    memset(ifname, '\0', IFNAMSIZ);
    strcpy(ifname, "/dev/dksdio0");
    s = open(ifname,O_RDWR);
    if (s < 0) err(1, "dksdio0");

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"get", 0, NULL, 'g'},
            {"set", 0, NULL, 's'},
            {"read", 0, NULL, 'r'},
            {"test", 0, NULL, 't'},
            {"file", 1, NULL, 'f'},
            {"done", 0, NULL, 'd'},
            {"write", 0, NULL, 'w'},
            {"begin", 0, NULL, 'b'},
            {"count", 1, NULL, 'c'},
            {"param", 1, NULL, 'p'},
            {"length", 1, NULL, 'l'},
            {"execute", 0, NULL, 'e'},
            {"address", 1, NULL, 'a'},
            {"interface", 1, NULL, 'i'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "rwtebdgsf:l:a:p:i:c:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            cmd = BMI_READ_MEMORY;
            break;

        case 'w':
            cmd = BMI_WRITE_MEMORY;
            break;

        case 'e':
            cmd = BMI_EXECUTE;
            break;

        case 'b':
            cmd = BMI_SET_APP_START;
            break;

        case 'd':
            cmd = BMI_DONE;
            break;

        case 'g':
            cmd = BMI_READ_SOC_REGISTER;
            break;

        case 's':
            cmd = BMI_WRITE_SOC_REGISTER;
            break;

        case 't':
            cmd = BMI_TEST;
            break;

        case 'f':
            strcpy(filename, optarg);
            flag |= FILE_FLAG;
            break;

        case 'l':
            length = atoi(optarg);
            flag |= LENGTH_FLAG;
            break;

        case 'a':
            address = strtoul(optarg, NULL, 16);
            flag |= ADDRESS_FLAG;
            break;

        case 'p':
            param = strtoul(optarg, NULL, 16);
            flag |= PARAM_FLAG;
            break;

        case 'c':
            count = atoi(optarg);
            flag |= COUNT_FLAG;
            break;

        case 'i':
            memset(ifname, '\0', 8);
            strcpy(ifname, optarg);
            break;

        default:
            usage();
        }
    }

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    switch(cmd)
    {
    case BMI_DONE:
        printf("BMI Done\n");
        buffer = (unsigned char *)malloc(4);
        ((int *)buffer)[0] = AR6000_XIOCTL_BMI_DONE;
        ifr.ifr_data = buffer;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
        {
            err(1, ifr.ifr_name);
        }
	free(buffer);
        break;

    case BMI_TEST:
        if ((flag & (COUNT_FLAG | LENGTH_FLAG | ADDRESS_FLAG)) ==
            (COUNT_FLAG | LENGTH_FLAG | ADDRESS_FLAG))
        {
            printf("BMI Test (address: 0x%x, length: %d, count: %d)\n",
                    address, length, count);
            buffer = (unsigned char *)malloc(16);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_TEST;
            ((int *)buffer)[1] = address;
            ((int *)buffer)[2] = length;
            ((int *)buffer)[3] = count;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            free(buffer);
        }
        else usage();
        break;

    case BMI_READ_MEMORY:
        if ((flag & (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG)) ==
            (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG))
        {
            printf(
                 "BMI Read Memory (address: 0x%x, length: %d, filename: %s)\n",
                  address, length, filename);
            buffer = (unsigned char *)malloc(length + 12);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_READ_MEMORY;
            ((int *)buffer)[1] = address;
            ((int *)buffer)[2] = length;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            else
            {
                if ((fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 00644)) < 0)
                {
                    perror("Could not create a file");
                    exit(1);
                }
                write(fd, buffer, length);
                close(fd);
            }
            free(buffer);
        }
        else usage();
        break;

    case BMI_WRITE_MEMORY:
        if ((flag & (ADDRESS_FLAG | FILE_FLAG)) == (ADDRESS_FLAG | FILE_FLAG))
        {
            printf(
                 "BMI Write Memory (address: 0x%x, filename: %s)\n",
                  address, filename);
            if ((fd = open(filename, O_RDONLY)) < 0)
            {
                perror("Could not open file");
                exit(1);
            }
            memset(&filestat, '\0', sizeof(struct stat));
            fstat(fd, &filestat);
            length = filestat.st_size;
            printf("length = %d\n", length);
            buffer = (unsigned char *)malloc(length + 12);
	    ((int *)buffer)[0] = AR6000_XIOCTL_BMI_WRITE_MEMORY;
            ((int *)buffer)[1] = address;
            ((int *)buffer)[2] = length;
            read(fd, &buffer[12], length);
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            free(buffer);
            close(fd);
        }
        else usage();
        break;

    case BMI_READ_SOC_REGISTER:
        if ((flag & (ADDRESS_FLAG)) == (ADDRESS_FLAG))
        {
            printf("BMI Read Register (address: 0x%x)\n", address);
            buffer = (unsigned char *)malloc(8);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_READ_SOC_REGISTER;
            ((int *)buffer)[1] = address;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            param = ((int *)buffer)[0];
            printf("Return Value from target: 0x%x\n", param);
            free(buffer);
        }
        else usage();
        break;

    case BMI_WRITE_SOC_REGISTER:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            printf("BMI Write Register (address: 0x%x, param: 0x%x)\n", address, param);
            buffer = (unsigned char *)malloc(12);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_WRITE_SOC_REGISTER;
            ((int *)buffer)[1] = address;
            ((int *)buffer)[2] = param;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            free(buffer);
        }
        else usage();
        break;

    case BMI_EXECUTE:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            printf("BMI Execute (address: 0x%x, param: 0x%x)\n", address, param);
            buffer = (unsigned char *)malloc(12);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_EXECUTE;
            ((int *)buffer)[1] = address;
            ((int *)buffer)[2] = param;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            param = ((int *)buffer)[0];
            printf("Return Value from target: 0x%x\n", param);
            free(buffer);
        }
        else usage();
        break;

    case BMI_SET_APP_START:
        if ((flag & ADDRESS_FLAG) == ADDRESS_FLAG)
        {
            printf("BMI Set App Start (address: 0x%x)\n", address);
            buffer = (unsigned char *)malloc(8);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_SET_APP_START;
            ((int *)buffer)[1] = address;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            free(buffer);
        }
        else usage();
        break;

    default:
        usage();
    }

    exit (0);
}
