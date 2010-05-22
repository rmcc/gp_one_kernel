/*
 * Copyright (c) 2006 Atheros Communications Inc.
 * All rights reserved.
 */

/*
 * This application allows the user to specific rompatch
 * actions in order to override sections of Target firmware
 * in ROM with a same-length section of firmware in RAM.
 *
 * The patch contents can be downloaded to the Target with
 * this application or independently.
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

#include <a_config.h>
#include <a_osapi.h>
#include <athdefs.h>
#include <a_types.h>
#include "athdrv_linux.h"
#include "bmi_msg.h"

#define MAX_FILENAME_LEN        1024
#define MAX_NUM_PATCHES         32
#define BMI_WRITE_MEMORY_HEADER 12 /* cmd, addr, length */
#define BOGUS_ADDRESS           0xffffffff

const char *progname;
const char commands[] = 
"\nCommands:\n\
--activate\n\
--deactivate\n\
--file=patch file name\n\
--help\n\
--install\n\
--interface=interace name (such as eth1)\n\
--length=length of patch\n\
--notarg (verify only -- no target)\n\
--patchid=patch id returned from previous install\n\
--ram=Target RAM address\n\
--rom=Target ROM address\n\
--uninstall\n\
";

const char examples[] =
"\nExamples:\n\
Install a patch from file rompatch1 into RAM locations\n\
starting at 0x5678, basing the patch length on the length\n\
of the file, then establish a remapping from ROM addresses\n\
starting at 0x1234 to the installed patch, and finally\n\
activate the patch:\n\
    --install --file=rompatch1 --rom=0x1234 --ram=0x5678 --activate\n\
\n\
Activate two patches with IDs 24 and 25 that were installed earlier:\n\
    --activate --patchid=24 --patchid=25\n\
";

int sock_fd;
struct ifreq ifr;

int patch_fd;
char filename[MAX_FILENAME_LEN]; /* Patch file name */
unsigned int filename_specified;
unsigned int file_length;
char ifname[IFNAMSIZ];           /* Interface name */
unsigned char *patch_buffer;     /* holds contents of patch file (to install) */
unsigned int patch_list[MAX_NUM_PATCHES];
unsigned int num_patches         = 0;
unsigned int rompatch_installed_id = 0;
unsigned int patch_length        = 0;
unsigned int do_activate         = 0;
unsigned int do_deactivate       = 0;
unsigned int do_install          = 0;
unsigned int do_uninstall        = 0;
unsigned int RAM_addr            = BOGUS_ADDRESS;
unsigned int ROM_addr            = BOGUS_ADDRESS;
unsigned int use_target          = 1;

static void
usage(void)
{
    fprintf(stderr, "Usage:\n%s commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    fprintf(stderr, "%s\n", examples);
    exit(-1);
}

static void
confused_params(void)
{
    fprintf(stderr, "Illegal combination of commands or arguments.\n");
    usage();
}

void
add_patch_id(unsigned int patch_id)
{
    if (num_patches == MAX_NUM_PATCHES) {
        fprintf(stderr, "Cannot support more than %d patches.\n", MAX_NUM_PATCHES);
        usage();
    }

    patch_list[num_patches] = patch_id;
    num_patches++;
}

void
access_patch_file(void)
{
    struct stat filestat;

    if (filename_specified)  {
        if ((RAM_addr == BOGUS_ADDRESS) ||
            (ROM_addr == BOGUS_ADDRESS))
        {
            fprintf(stderr, "Must specify RAM and ROM addresses for installation.\n");
            confused_params();
        }

        if ((patch_fd = open(filename, O_RDONLY)) < 0) {
            fprintf(stderr, "Cannot open file: %s.\n", filename);
            exit(-1);
        }

        memset(&filestat, 0, sizeof(struct stat));
        if (fstat(patch_fd, &filestat) < 0) {
            fprintf(stderr, "Cannot find length of patch file: %s.\n", filename);
            exit(-1);
        }

        file_length = filestat.st_size;

        if (patch_length == 0) {
            patch_length = file_length;
        }

        if (patch_length < file_length) {
            fprintf(stderr, "Specified patch length (%d) less than actual length (%d).\n",
                    patch_length, file_length);
            confused_params();
        }

        patch_buffer = (unsigned char *)malloc(file_length+BMI_WRITE_MEMORY_HEADER);
        if (patch_buffer == NULL) {
            fprintf(stderr, "Cannot allocate space (%d bytes) for patch file.\n", file_length);
            exit(-1);
        }

        if (read(patch_fd, patch_buffer+BMI_WRITE_MEMORY_HEADER, file_length) != file_length) {
            fprintf(stderr, "Cannot read patch file (length=%d).\n", file_length);
            exit(-1);
        }

        close(patch_fd);
    } else { /* No patch file specified */
        if ((patch_length == 0) ||
            (ROM_addr == BOGUS_ADDRESS) ||
            (RAM_addr == BOGUS_ADDRESS))
        {
            fprintf(stderr, "Must describe WHAT to install.\n");
            confused_params();
        }
    }
}

void
sanity_check_params(void)
{
    if (do_activate) {
        if (do_deactivate || do_uninstall) {
            confused_params();
        }

        if ((!do_install) && (num_patches == 0)) {
            confused_params();
        }
    }

    if (do_deactivate) {
        if (do_install || do_activate) {
            confused_params();
        }

        if (num_patches == 0) {
            confused_params();
        }
    }

    if (do_install) {
        if (do_uninstall || do_deactivate) {
            confused_params();
        }
    }

    if (do_uninstall) {
        if (do_install || do_activate) {
            confused_params();
        }

        if (num_patches != 1) {
            /* A semi-arbitrary restriction to at most 1 patch. */
            fprintf(stderr, "Uninstall exactly one patch at a time.\n");
            confused_params();
        }
    }

    if ((!do_install) &&
        (!do_uninstall) &&
        (!do_activate) &&
        (!do_deactivate))
    {
        fprintf(stderr, "No patch action specified.\n");
        confused_params();
    }
}

void
handle_patches(void)
{
    A_UINT32 rompatch_cmd[5+MAX_NUM_PATCHES];
    int i;

    if (do_install) {

        access_patch_file();

        if (use_target) {
            /* 
             * If a patch file was specified, then
             * use BMI to write the contents of that
             * file to RAM_addr.
             */
            if (file_length != 0) {
                ((int *)patch_buffer)[0] = AR6000_XIOCTL_BMI_WRITE_MEMORY;
                ((int *)patch_buffer)[1] = RAM_addr;
                ((int *)patch_buffer)[2] = file_length;
                ifr.ifr_data = (char *)patch_buffer;
                if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
                {
                    err(1, ifr.ifr_name);
                }
                free(patch_buffer);
            }
    
            /* Now perform the rompatch installation */
            rompatch_cmd[0] = AR6000_XIOCTL_BMI_ROMPATCH_INSTALL;
            rompatch_cmd[1] = ROM_addr;
            rompatch_cmd[2] = RAM_addr;
            rompatch_cmd[3] = patch_length;
            rompatch_cmd[4] = 0; /* Activate separately */
            ifr.ifr_data = (char *)rompatch_cmd;
            if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
    
            rompatch_installed_id = rompatch_cmd[0];
            add_patch_id(rompatch_installed_id);
    
            /* User will need this for later activation/deactivation */
            printf("%d\n", rompatch_installed_id);
        }
    }

#if defined(DEBUG)
    {
        int i;

        printf("Patch list:");
        for (i=0; i<num_patches; i++) {
            printf(" %d", patch_list[i]);
        }
        printf("\n");
    }
#endif /* DEBUG */

    if (do_activate) {
        if (use_target) {
            rompatch_cmd[0] = AR6000_XIOCTL_BMI_ROMPATCH_ACTIVATE;
            rompatch_cmd[1] = num_patches;
            for (i=0; i<num_patches; i++) {
                rompatch_cmd[2+i] = patch_list[i];
            }
            ifr.ifr_data = (char *)rompatch_cmd;
            if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
        }
    }

    if (do_deactivate) {
        if (use_target) {
            rompatch_cmd[0] = AR6000_XIOCTL_BMI_ROMPATCH_DEACTIVATE;
            rompatch_cmd[1] = num_patches;
            for (i=0; i<num_patches; i++) {
                rompatch_cmd[2+i] = patch_list[i];
            }
            ifr.ifr_data = (char *)rompatch_cmd;
            if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
        }
    }

    if (do_uninstall) {
        if (use_target) {
            rompatch_cmd[0] = AR6000_XIOCTL_BMI_ROMPATCH_UNINSTALL;
            rompatch_cmd[1] = patch_list[0];
            ifr.ifr_data = (char *)rompatch_cmd;
            if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
            {
                err(1, ifr.ifr_name);
            }
        }
    }
}

/* Verify that the Target is alive.  If not, wait for it. */
void
wait_for_target(void)
{
    int rv;
    static int waiting_msg_printed = 0;
    unsigned char *check_ready_buffer;

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    check_ready_buffer = (unsigned char *)malloc(4);
    ((int *)check_ready_buffer)[0] = AR6000_XIOCTL_CHECK_TARGET_READY;
    ifr.ifr_data = (char *)check_ready_buffer;
    while ((rv=ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr)) < 0)
    {
        if (errno == ENODEV) {
            /* 
             * Give the Target device a chance to start.
             * Then loop back and see if it's alive.
             */
            if (!waiting_msg_printed) {
                printf("rompatcher is waiting for Target....\n");
                waiting_msg_printed = 1;
            }
            usleep(100000);  /* Sleep 100ms */
        } else {
            fprintf(stderr, "Cannot check Target Ready (errno=0x%x).\n", errno);
            exit(-1);
        }
    }
    free(check_ready_buffer);
}

int
main (int argc, char **argv)
{
    progname = argv[0];

    memset(ifname, '\0', IFNAMSIZ);
    strcpy(ifname, "eth1");

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        err(1, "socket");
    }

    for(;;) {
        int c;
        int option_index = 0;
        struct option long_options[] = {
            {"activate", 0, NULL, 0},
            {"deactivate", 0, NULL, 0}, 
            {"file", 1, NULL, 0},
            {"help", 0, NULL, 0},
            {"install", 0, NULL, 0},
            {"interface", 1, NULL, 0},
            {"length", 1, NULL, 0},
            {"notarg", 0, NULL, 0},
            {"patchid", 1, NULL, 0},
            {"ram", 1, NULL, 0},
            {"rom", 1, NULL, 0},
            {"uninstall", 0, NULL, 0},
            {0, 0, 0, 0}
        };
#define OPTION_ACTIVATE                 0
#define OPTION_DEACTIVATE               1
#define OPTION_FILE                     2
#define OPTION_HELP                     3
#define OPTION_INSTALL                  4
#define OPTION_IFNAME                   5
#define OPTION_LENGTH                   6
#define OPTION_NOTARG                   7
#define OPTION_PATCHID                  8
#define OPTION_RAM                      9
#define OPTION_ROM                      10
#define OPTION_UNINSTALL                11

        c = getopt_long (argc, argv, "+",
                         long_options, &option_index);

        if (c == -1) { /* done */
            break;
        }

        if (c != 0) { /* unrecognized option */
            usage();
        }

        switch(option_index) {
            case OPTION_ACTIVATE:
                do_activate=1;
                break;

            case OPTION_DEACTIVATE:
                do_deactivate=1;
                break;

            case OPTION_FILE:
                if (filename_specified) {
                    fprintf(stderr, "Patch file re-specified.\n");
                    confused_params();
                }
                filename_specified = 1;
                memset(filename, '\0', MAX_FILENAME_LEN);
                strncpy(filename, optarg, MAX_FILENAME_LEN-1);
                break;

            case OPTION_HELP:
                usage();

            case OPTION_INSTALL:
                do_install = 1;
                break;

            case OPTION_IFNAME:
                memset(ifname, '\0', IFNAMSIZ);
                strncpy(ifname, optarg, IFNAMSIZ-1);
                break;

            case OPTION_LENGTH:
                if (patch_length != 0) {
                    fprintf(stderr, "Patch length re-specified.\n");
                    confused_params();
                }
                patch_length=strtoul(optarg, NULL, 0);
                break;

            case OPTION_NOTARG:
                use_target = 0;
                break;

            case OPTION_PATCHID:
                add_patch_id(strtoul(optarg, NULL, 0));
                break;

            case OPTION_RAM:
                if (RAM_addr != BOGUS_ADDRESS) {
                    fprintf(stderr, "RAM address re-specified.\n");
                    confused_params();
                }
                RAM_addr = strtoul(optarg, NULL, 0);
                break;

            case OPTION_ROM:
                if (ROM_addr != BOGUS_ADDRESS) {
                    fprintf(stderr, "ROM address re-specified.\n");
                    confused_params();
                }
                ROM_addr = strtoul(optarg, NULL, 0);
                break;

            case OPTION_UNINSTALL:
                do_uninstall = 1;
                break;
        }
    }

    if (argc != optind) {
        fprintf(stderr, "Cannot understand '%s'.\n", argv[optind]);
        usage();
    }

#if defined(DEBUG)
    printf("interface=%s\n", ifname);
    printf("filename=%s\n", filename);
    printf("length=%d\n", patch_length);
    printf("activate=%d\n", do_activate);
    printf("deactivate=%d\n", do_deactivate);
    printf("install=%d\n", do_install);
    printf("uninstall=%d\n", do_uninstall);
    printf("RAMaddr=0x%x\n", RAM_addr);
    printf("ROMaddr=0x%x\n", ROM_addr);
    if (num_patches > 0) {
        int i;

        printf("Initial patch list:");
        for (i=0; i<num_patches; i++) {
            printf(" %d", patch_list[i]);
        }
        printf("\n");
    }
#endif /* DEBUG */

    sanity_check_params();

    if (use_target) {
        wait_for_target();
    }

    handle_patches();

    exit(0);
}
