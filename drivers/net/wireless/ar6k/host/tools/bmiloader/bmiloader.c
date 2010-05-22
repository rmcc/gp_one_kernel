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
#include <errno.h>
#if defined(DWSIM)
#include <sched.h>
#endif

#include <a_config.h>
#include <a_osapi.h>
#include <athdefs.h>
#include <a_types.h>
#include "athdrv_linux.h"
#include "bmi_msg.h"

#ifndef PATH_MAX
#define PATH_MAX (255)
#endif

#define ADDRESS_FLAG                    0x001
#define LENGTH_FLAG                     0x002
#define PARAM_FLAG                      0x004
#define FILE_FLAG                       0x008
#define COUNT_FLAG                      0x010
#define AND_OP_FLAG                     0x020
#define BITWISE_OP_FLAG                 0x040
#define QUIET_FLAG                      0x080
#define UNCOMPRESS_FLAG                 0x100
#define TIMEOUT_FLAG                    0x200

#define BMI_TEST                        BMI_NO_COMMAND

/* Limit malloc size when reading/writing file */
#define MAX_BUF                         (8*1024)

const char *progname;
const char commands[] = 
"commands and options:\n\
--get --address=<register address>\n\
--set --address=<register address> --param=<register value>\n\
--set --address=<register address> --or=<Or-ing mask value>\n\
--set --address=<register address> --and=<And-ing mask value>\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
--write --address=<target address> [--file=<filename> | --param=<value>] [--uncompress]\n\
--execute --address=<function start address> --param=<input param>\n\
--begin --address=<function start address>\n\
--info\n\
--test --address=<target address> --length=<cmd size> --count=<iterations>\n\
--quiet\n\
--done\n\
--timeout=<time to wait for command completion in seconds>\n\
The options can also be given in the abbreviated form --option=x or -o x. The options can be given in any order";

#define A_ROUND_UP(x, y)             ((((x) + ((y) - 1)) / (y)) * (y))

#define quiet() (flag & QUIET_FLAG)
#define nqprintf(args...) if (!quiet()) {printf(args);}

INLINE void *
MALLOC(int nbytes)
{
    void *p= malloc(nbytes);

    if (!p)
    {
        err(1, "Cannot allocate memory\n");
    }

    return p;
}

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
    char filename[PATH_MAX], ifname[IFNAMSIZ];
    unsigned int cmd;
    struct ifreq ifr;
    char *buffer;
    struct stat filestat;
    int flag;
    int target_version = -1;
    int target_type = -1;
    unsigned int bitwise_mask;
    unsigned int timeout;
    
    progname = argv[0];
    if (argc == 1) usage();

    flag = 0;
    memset(filename, '\0', sizeof(filename));
    memset(ifname, '\0', IFNAMSIZ);
    strcpy(ifname, "eth1");
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) err(1, "socket");

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"get", 0, NULL, 'g'},
            {"set", 0, NULL, 's'},
            {"read", 0, NULL, 'r'},
            {"test", 0, NULL, 't'},
            {"timeout", 1, NULL, 'T'},
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
            {"info", 0, NULL, 'I'},
            {"and", 1, NULL, 'n'},
            {"or", 1, NULL, 'o'},
            {"quiet", 0, NULL, 'q'},
            {"uncompress", 0, NULL, 'u'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "rwtebdgsIqf:l:a:p:i:c:n:o:",
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
            memset(filename, '\0', sizeof(filename));
            strncpy(filename, optarg, sizeof(filename));
            flag |= FILE_FLAG;
            break;

        case 'l':
            length = atoi(optarg);
            flag |= LENGTH_FLAG;
            break;

        case 'a':
            address = strtoul(optarg, NULL, 0);
            flag |= ADDRESS_FLAG;
            break;

        case 'p':
            param = strtoul(optarg, NULL, 0);
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

        case 'I':
            cmd = BMI_GET_TARGET_INFO;
            break;
            
        case 'n':
            flag |= PARAM_FLAG | AND_OP_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;
            
        case 'o':                
            flag |= PARAM_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;

        case 'q':
            flag |= QUIET_FLAG;
            break;
            
        case 'u':
            flag |= UNCOMPRESS_FLAG;
            break;

        case 'T':
            timeout = strtoul(optarg, NULL, 0);
            timeout = timeout * 10; // convert seconds to 100ms units
            flag |= TIMEOUT_FLAG;
            break;
            
        default:
            usage();
        }
    }

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    /* Verify that the Target is alive.  If not, wait for it. */
    {
        int rv;
        static int waiting_msg_printed = 0;

        buffer = (char *)MALLOC(sizeof(struct bmi_target_info));
        ((int *)buffer)[0] = AR6000_XIOCTL_TARGET_INFO;
        ifr.ifr_data = buffer;
        while ((rv=ioctl(s, AR6000_IOCTL_EXTENDED, &ifr)) < 0)
        {
            if (errno == ENODEV) {
                /* 
                 * Give the Target device a chance to start.
                 * Then loop back and see if it's alive till the specified
                 * timeout
                 */
                if (flag & TIMEOUT_FLAG) {
                    if (!timeout) {
                        err(1, ifr.ifr_name);
                        exit(1);
                    }
                    timeout--;
                }
                if (!waiting_msg_printed) {
                    nqprintf("bmiloader is waiting for Target....\n");
                    waiting_msg_printed = 1;
                }
                usleep(100000); /* Wait for 100ms */
            } else {
                printf("Unexpected error on AR6000_XIOCTL_TARGET_INFO: %d\n", rv);
                exit(1);
            }
        }
        target_version = ((int *)buffer)[0];
        target_type = ((int *)buffer)[1];
        free(buffer);
    }
    switch(cmd)
    {
    case BMI_DONE:
        nqprintf("BMI Done\n");
        buffer = (char *)MALLOC(4);
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
            nqprintf("BMI Test (address: 0x%x, length: %d, count: %d)\n", 
                    address, length, count);
            buffer = (char *)MALLOC(16);
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
            nqprintf(
                 "BMI Read Memory (address: 0x%x, length: %d, filename: %s)\n",
                  address, length, filename);

            if ((fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 00644)) < 0)
            {
                perror("Could not create a file");
                exit(1);
            }
            buffer = (char *)MALLOC(MAX_BUF + 12);

            {
                unsigned int remaining = length;

                while (remaining)
                {
                    length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                    ((int *)buffer)[0] = AR6000_XIOCTL_BMI_READ_MEMORY;
                    ((int *)buffer)[1] = address;

                    /*
                     * We round up the requested length because some
                     * SDIO Host controllers can't handle other lengths;
                     * but we still only write the requested number of
                     * bytes to the file.
                     */
                    ((int *)buffer)[2] = A_ROUND_UP(length, 4);
                    ifr.ifr_data = buffer;
                    if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
                    {
                        err(1, ifr.ifr_name);
                    }
                    else
                    {
                        write(fd, buffer, length);
                    }

                    remaining -= length;
                    address += length;
                }
            }

            close(fd);
            free(buffer);
        }
        else usage();
        break;

    case BMI_WRITE_MEMORY:
        if (!(flag & ADDRESS_FLAG))
        {
            usage(); /* no address specified */
        }
        if (!(flag & (FILE_FLAG | PARAM_FLAG)))
        {
            usage(); /* no data specified */
        }
        if ((flag & FILE_FLAG) && (flag & PARAM_FLAG))
        {
            usage(); /* too much data specified */
        }
        if ((flag & UNCOMPRESS_FLAG) && !(flag & FILE_FLAG))
        {
            usage(); /* uncompress only works with a file */
        }

        if (flag & FILE_FLAG)
        {
            nqprintf(
                 "BMI Write %sMemory (address: 0x%x, filename: %s)\n",
                  ((flag & UNCOMPRESS_FLAG) ? "compressed " : ""),
                  address, filename);
            if ((fd = open(filename, O_RDONLY)) < 0)
            {
                perror("Could not open file");
                exit(1);
            }
            memset(&filestat, '\0', sizeof(struct stat));
            buffer = (char *)MALLOC(MAX_BUF + 12);
            fstat(fd, &filestat);
            length = filestat.st_size;

            if (flag & UNCOMPRESS_FLAG) {
                /* Initiate compressed stream */
                ((int *)buffer)[0] = AR6000_XIOCTL_BMI_LZ_STREAM_START;
                ((int *)buffer)[1] = address;
                if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
                {
                    err(1, ifr.ifr_name);
                }
            }
        }
        else
        { /* PARAM_FLAG */
            nqprintf(
                 "BMI Write Memory (address: 0x%x, value: 0x%x)\n",
                  address, param);
            length = sizeof(param);
            buffer = (char *)MALLOC(length + 12);
            *(unsigned int *)(&buffer[12]) = param;
            fd = -1;
        }

        /*
         * Write length bytes of data to memory.
         * Data is either present in buffer OR
         * needs to be read from fd in MAX_BUF chunks.
         *
         * Within the kernel, the implementation of
         * AR6000_XIOCTL_BMI_WRITE_MEMORY further
         * limits the size of each transfer over the
         * interconnect according to BMI protocol
         * limitations.
         */ 
        {
            unsigned int remaining = length;
            int *pLength;

            while (remaining)
            {
                length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
#if defined(DWSIM)
                if ((address & 0xf0000000) == 0) {
                    /*
                     * TBDXXX: 
                     * If it looks like we're writing to an XTENSA address,
                     * write at most 20 bytes at a time.  If it looks like
                     * we're talking MIPS, do the whole thing.  Currently,
                     * we can't write too much at once through BMI to XT
                     * because the Host driver internally divides the request
                     * into multiple BMI transactions and it waits for each
                     * transaction to complete before starting the next one.
                     * But the first one never completes because the ISS
                     * never gets an opportunity to execute properly because
                     * the kernel locks it out when it tries to access
                     * simulated target registers through diag window.
                     */
                    if (remaining > 20) {
                        length = 20;
                    } else {
                        length = remaining;
                    }
                }
#endif
                if (flag & UNCOMPRESS_FLAG) {
                    /* 0 pad last word of data to avoid messy uncompression */
                    ((A_UINT32 *)buffer)[2+((length-1)/4)] = 0;

                    if (read(fd, &buffer[8], length) != length)
                    {
                        perror("read from compressed file failed");
                        exit(1);
                    }
                    ((int *)buffer)[0] = AR6000_XIOCTL_BMI_LZ_DATA;
                    pLength = &((int *)buffer)[1];
                } else {
                    if (fd > 0)
                    {
                        if (read(fd, &buffer[12], length) != length)
                        {
                            perror("read from file failed");
                            exit(1);
                        }
                    }

                    ((int *)buffer)[0] = AR6000_XIOCTL_BMI_WRITE_MEMORY;
                    ((int *)buffer)[1] = address;
                    pLength = &((int *)buffer)[2];
                }

                /*
                 * We round up the requested length because some
                 * SDIO Host controllers can't handle other lengths.
                 * This generally isn't a problem for users, but it's
                 * something to be aware of.
                 */
                *pLength = A_ROUND_UP(length, 4);
                ifr.ifr_data = buffer;
                while (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
                {
#if defined(DWSIM)
                    printf("Retry BMI write...address=0x%x length=0x%d\n", address, length);
#else
                    err(1, ifr.ifr_name);
#endif
                }

                remaining -= length;
                address += length;
            }
        }

        free(buffer);
        if (fd > 0)
        {
            close(fd);
            if (flag & UNCOMPRESS_FLAG) {
                /*
                 * Close compressed stream and open a new (fake)
                 * one.  This serves mainly to flush Target caches.
                 */
                ((int *)buffer)[0] = AR6000_XIOCTL_BMI_LZ_STREAM_START;
                ((int *)buffer)[1] = 0;
                if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
                {
                    err(1, ifr.ifr_name);
                }
            }
        }

        break;

    case BMI_READ_SOC_REGISTER:
        if ((flag & (ADDRESS_FLAG)) == (ADDRESS_FLAG))
        {
            nqprintf("BMI Read Register (address: 0x%x)\n", address);
            buffer = (char *)MALLOC(8);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_READ_SOC_REGISTER;
            ((int *)buffer)[1] = address;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            param = ((int *)buffer)[0];
            if (quiet()) {
                printf("0x%x\n", param);
            } else {
                printf("Return Value from target: 0x%x\n", param);
            }
            free(buffer);
        }
        else usage();
        break;

    case BMI_WRITE_SOC_REGISTER:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            int origvalue = 0;
            
            if (flag & BITWISE_OP_FLAG) {
                /* first read */    
                buffer = (char *)MALLOC(8);
                ((int *)buffer)[0] = AR6000_XIOCTL_BMI_READ_SOC_REGISTER;
                ((int *)buffer)[1] = address;
                ifr.ifr_data = buffer;
                if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
                {
                    err(1, ifr.ifr_name);
                }
                param = ((int *)buffer)[0];
                origvalue = param;
                free(buffer);
                
                /* now modify */
                if (flag & AND_OP_FLAG) {
                    param &= bitwise_mask;        
                } else {
                    param |= bitwise_mask;
                }               
            
                /* fall through to write out the parameter */
            }
            
            if (flag & BITWISE_OP_FLAG) {
                if (quiet()) {
                    printf("0x%x\n", origvalue);
                } else {
                    printf("BMI Bit-Wise (%s) modify Register (address: 0x%x, orig:0x%x, new: 0x%x,  mask:0x%X)\n", 
                       (flag & AND_OP_FLAG) ? "AND" : "OR", address, origvalue, param, bitwise_mask );   
                }
            } else{ 
                nqprintf("BMI Write Register (address: 0x%x, param: 0x%x)\n", address, param);
            }
            
            buffer = (char *)MALLOC(12);
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
            nqprintf("BMI Execute (address: 0x%x, param: 0x%x)\n", address, param);
            buffer = (char *)MALLOC(12);
            ((int *)buffer)[0] = AR6000_XIOCTL_BMI_EXECUTE;
            ((int *)buffer)[1] = address;
            ((int *)buffer)[2] = param;
            ifr.ifr_data = buffer;
            if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
            param = ((int *)buffer)[0];
            if (quiet()) {
                printf("0x%x\n", param);
            } else {
                printf("Return Value from target: 0x%x\n", param);
            }
            free(buffer);
        }
        else usage();
        break;

    case BMI_SET_APP_START:
        if ((flag & ADDRESS_FLAG) == ADDRESS_FLAG)
        {
            nqprintf("BMI Set App Start (address: 0x%x)\n", address);
            buffer = (char *)MALLOC(8);
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
    case BMI_GET_TARGET_INFO:
        nqprintf("BMI Target Info:\n");
        printf("TARGET_TYPE=%s\n",
                (target_type == TARGET_TYPE_AR6001) ? "AR6001" :
                ((target_type == TARGET_TYPE_AR6002) ? "AR6002" : "unknown"));
        printf("TARGET_VERSION=0x%x\n", target_version);
        break;

    default:
        usage();
    }

    exit (0);
}
