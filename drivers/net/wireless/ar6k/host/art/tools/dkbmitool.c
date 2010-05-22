//------------------------------------------------------------------------------
// <copyright file="dkbmitool.c" company="Atheros">
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

#include "../sdio_driver/dksdio.h"

typedef enum
{
    BMI_DONE,
    BMI_WRITE_MEMORY,
    BMI_READ_MEMORY,
    BMI_EXECUTE,
    BMI_SET_APP_START,
    BMI_TEST,
} BMI_COMMAND;

#define ADDRESS_FLAG                    0x01
#define LENGTH_FLAG                     0x02
#define PARAM_FLAG                      0x04
#define FILE_FLAG                       0x08
#define COUNT_FLAG                      0x10

const char *progname;
const char commands[] =
"commands:\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
--write --address=<target address> --file=<filename>\n\
--execute --address=<function start address> --param=<input param>\n\
--start --address=<function start address>\n\
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
    BMI_COMMAND cmd;
    progname = argv[0];
    DK_IDATA ifr;
    unsigned char *buffer;
    struct stat filestat;
    int flag;

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
            {"read", 0, NULL, 'r'},
            {"test", 0, NULL, 't'},
            {"file", 1, NULL, 'f'},
            {"done", 0, NULL, 'd'},
            {"write", 0, NULL, 'w'},
            {"start", 0, NULL, 's'},
            {"count", 1, NULL, 'c'},
            {"param", 1, NULL, 'p'},
            {"length", 1, NULL, 'l'},
            {"execute", 0, NULL, 'e'},
            {"address", 1, NULL, 'a'},
            {"interface", 1, NULL, 'i'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "rwtesdf:l:a:p:i:c:",
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

        case 's':
            cmd = BMI_SET_APP_START;
            break;

        case 'd':
            cmd = BMI_DONE;
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
            param = atoi(optarg);
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
        if (ioctl(s, AR6000_IOCTL_BMI_DONE, &ifr) < 0)
        {
            err(1, ifr.ifr_name);
        }
        break;

    case BMI_TEST:
        if ((flag & (COUNT_FLAG | LENGTH_FLAG | ADDRESS_FLAG)) ==
            (COUNT_FLAG | LENGTH_FLAG | ADDRESS_FLAG))
        {
            printf("BMI Test (address: 0x%x, length: %d, count: %d)\n",
                    address, length, count);
            buffer = (unsigned char *)malloc(12);
            ((int *)buffer)[0] = address;
            ((int *)buffer)[1] = length;
            ((int *)buffer)[2] = count;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_BMI_TEST, &ifr) < 0)
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
            buffer = (unsigned char *)malloc(length + 8);
            ((int *)buffer)[0] = address;
            ((int *)buffer)[1] = length;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_BMI_READ_MEMORY, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            else
            {
                fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC);
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
            fd = open(filename, O_RDONLY);
            memset(&filestat, '\0', sizeof(struct stat));
            fstat(fd, &filestat);
            length = filestat.st_size;
            printf("length = %d\n", length);
            buffer = (unsigned char *)malloc(length + 8);
            ((int *)buffer)[0] = address;
            ((int *)buffer)[1] = length;
            read(fd, &buffer[8], length);
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_BMI_WRITE_MEMORY, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            free(buffer);
            close(fd);
        }
        else usage();
        break;

    case BMI_EXECUTE:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            printf("BMI Execute (address: 0x%x, param: %d)\n", address, param);
            buffer = (unsigned char *)malloc(8);
            ((int *)buffer)[0] = address;
            ((int *)buffer)[1] = param;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_BMI_EXECUTE, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            param = ((int *)buffer)[0];
            printf("Return Value from target: %d\n", param);
            free(buffer);
        }
        else usage();
        break;

    case BMI_SET_APP_START:
        if ((flag & ADDRESS_FLAG) == ADDRESS_FLAG)
        {
            printf("BMI Set App Start (address: 0x%x)\n", address);
            buffer = (unsigned char *)malloc(4);
            ((int *)buffer)[0] = address;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_BMI_SET_APP_START, &ifr) < 0)
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
