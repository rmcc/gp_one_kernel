/*
 * Copyright (c) 2006 Atheros Communications Inc.
 * All rights reserved.
 */

/*
 * This application allows the Host to install patches from
 * a ROM Patch Distribution File in order to override sections
 * of Target ROM firmware with RAM.
 *
 * See rpdf.txt for an explanation of RPDF.
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

#define RPDF_CMD_INSTALL        1
#define RPDF_CMD_INST_ACT       2
#define MAX_NUM_PATCHES         32

#define MAX_FILENAME_LEN        1024

const char *progname;
const char commands[] = 
"\nCommands:\n\
--file=RPDF file name\n\
--help\n\
--interface=interface name (such as eth1)\n\
--notarg (verify only -- no target)\n\
--verbose\n\
";

const char examples[] =
"\nExample:\n\
    --file=rompatch1.rpdf\n\
";

int sock_fd;
struct ifreq ifr;

unsigned int filename_specified = 0;
char ifname[IFNAMSIZ];           /* Interface name */
unsigned int num_installed_patches = 0;
unsigned int use_target          = 1;

int verbose_enabled = 0;
#define DBGMSG(args...)                 \
do {                                    \
    if (verbose_enabled) printf(args);    \
} while (0)                             \

#define min(x, y)                  (((x) < (y)) ? (x) : (y))

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

/* Wrapper for read -- exits if anything goes wrong with the read */
void
doread(int fd, void *buf, size_t count)
{
    if (read(fd, buf, count) != count) {
        fprintf(stderr, "Could not read %d bytes from RPDF file.\n", count);
        exit(-1);
    }
}

/*
 * Tracks patches which have been installed but not yet activated.
 * Element 0 is the patchId of the oldest installed patch.
 */
static unsigned int installed_patch_list[MAX_NUM_PATCHES];

/* Add a patch_id to the installed_patch_list. */
void
add_patch_id(unsigned int patch_id)
{
    if (num_installed_patches == MAX_NUM_PATCHES) {
        /* Invalid RPDF?! */
        fprintf(stderr, "Cannot support more than %d patches.\n", MAX_NUM_PATCHES);
        usage();
    }

    installed_patch_list[num_installed_patches] = patch_id;
    num_installed_patches++;
}

/* Activate all patches on the installed_patch_list. */
void
activate_patches(void)
{
    int i;
    A_UINT32 activate_bmicmd[2+MAX_NUM_PATCHES];

    DBGMSG("Activate installed patches\n");

    if (!use_target) {
        return;
    }

    if (num_installed_patches == 0) {
        return; /* nothing to activate */
    }

    activate_bmicmd[0] = AR6000_XIOCTL_BMI_ROMPATCH_ACTIVATE;
    activate_bmicmd[1] = num_installed_patches;
    for (i=0; i<num_installed_patches; i++) {
        activate_bmicmd[2+i] = installed_patch_list[i];
    }
    ifr.ifr_data = (char *)activate_bmicmd;
    if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
    {
        err(1, ifr.ifr_name);
    }

    /* Clear  installed_patch_list. */
    num_installed_patches = 0;
}


/* Write a portion of a patch to RAM via BMI. */
void
commit_chunk_to_RAM(unsigned char *buf, A_UINT32 ramaddr, unsigned int length)
{
    A_UINT32 writemem_bmicmd[3+MAX_NUM_PATCHES];

    (writemem_bmicmd)[0] = AR6000_XIOCTL_BMI_WRITE_MEMORY;
    (writemem_bmicmd)[1] = ramaddr;
    (writemem_bmicmd)[2] = length;
    memcpy(&writemem_bmicmd[3], buf, (size_t)length);

    ifr.ifr_data = (char *)writemem_bmicmd;
    if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
    {
        err(1, ifr.ifr_name);
    }
}

/*
 * Decompose length bytes of buf into smaller chunks that can be
 * processed by BMI.  Write each chunk into Target RAM starting
 * at ramaddr.
 */
void
commit_patch_buffer_to_RAM(unsigned char *buf,
                           A_UINT32 ramaddr,
                           A_UINT32 length)
{
    unsigned int remaining;
    unsigned int position;
    unsigned int chunk_size;

    DBGMSG("Write %d bytes to Target RAM: 0x%x\n", length, ramaddr);

    if (!use_target) {
        return;
    }

    position = 0;
    remaining = length;
    while (remaining) {
        chunk_size = min(remaining, BMI_DATASZ_MAX);

        commit_chunk_to_RAM(&buf[position],
                            ramaddr+position,
                            chunk_size);

        remaining -= chunk_size;
        position += chunk_size;
    }
}


/*
 * Set up a ROM to RAM remapping in Target hardware,
 * but don't yet mark it valid.
 */
void
install_patch(A_UINT32 romaddr, A_UINT32 ramaddr, A_UINT32 hwsz)
{
    A_UINT32 rompatch_installed_id;
    A_UINT32 install_bmicmd[5+MAX_NUM_PATCHES];

    DBGMSG("Install patch: ROM: 0x%x RAM: 0x%x hwsz: 0x%x\n",
        romaddr, ramaddr, hwsz);

    if (!use_target) {
        return;
    }

    install_bmicmd[0] = AR6000_XIOCTL_BMI_ROMPATCH_INSTALL;
    install_bmicmd[1] = romaddr;
    install_bmicmd[2] = ramaddr;
    install_bmicmd[3] = hwsz;
    install_bmicmd[4] = 0; /* Activate separately */
    ifr.ifr_data = (char *)install_bmicmd;
    if (ioctl(sock_fd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
    {
        err(1, ifr.ifr_name);
    }

    rompatch_installed_id = install_bmicmd[0];
    DBGMSG("Installed ID: 0x%x\n", rompatch_installed_id);

    add_patch_id(rompatch_installed_id);
}

/*
 * Verify that we can open the RPDF file, that it has a valid checksum.
 */
int
access_rpdf_file(char *filename)
{
    A_UINT32 rpdf_id;
    A_UINT32 rpdf_cksum;
    int rpdf_fd;

    if ((rpdf_fd = open(filename, O_RDONLY)) < 0) {
        fprintf(stderr, "Cannot open RPDF file: %s.\n", filename);
        exit(-1);
    }

    doread(rpdf_fd, &rpdf_id, sizeof(rpdf_id));
    DBGMSG("Patch ID is 0x%08x\n", rpdf_id);

    /*
     * Skip over customer checksum field.
     * NB: ROM Patch Distributions Files will arrive with md5sums
     * so they can be externally validated when customers receive
     * a patches.  The cksum field within the RPDF file may be
     * used by customers for end-user validation.
     */
    doread(rpdf_fd, &rpdf_cksum, sizeof(rpdf_cksum));

    return rpdf_fd;
}

void
sanity_check_params(void)
{
    if (!filename_specified) {
        fprintf(stderr, "Must specify an RPDF file\n");
        confused_params();
    }
}

/* Verify that the Target is alive.  If not, wait for it. */
void
wait_for_target(void)
{
    int rv;
    static int waiting_msg_printed = 0;
    unsigned char *check_ready_buffer;

    if (!use_target) {
        return;
    }

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

/* Handle the next Patch Specification from the specified RPDF file */
void
handle_patch_specification(int rpdf_fd)
{
    A_UINT32 rpdf_cmd;
    A_UINT32 hw_remapsz;
    A_UINT32 ROM_addr;
    A_UINT32 RAM_addr;
    A_UINT32 patch_length;

    doread(rpdf_fd, &rpdf_cmd, sizeof(rpdf_cmd));

    if ((rpdf_cmd == RPDF_CMD_INSTALL) ||
        (rpdf_cmd == RPDF_CMD_INST_ACT))
    {
        /*
         * NB: For an embedded kernel port of this code, this
         * buffer may be too large for the stack.  Decrease
         * PATCH_BUF_SZ as needed, though it's probably best
         * to keep PATCH_BUF_SZ a multiple of BMI_DATASZ_MAX.
         */
#define PATCH_BUF_SZ            2048
        unsigned char patch_buffer[PATCH_BUF_SZ];

        doread(rpdf_fd, &hw_remapsz, sizeof(hw_remapsz));
        if (hw_remapsz & (hw_remapsz-1))
        {
            fprintf(stderr, "Invalid hardware remap size (0x%x)\n", hw_remapsz);
            exit(-1);
        }
    
        doread(rpdf_fd, &ROM_addr, sizeof(ROM_addr));
        doread(rpdf_fd, &RAM_addr, sizeof(RAM_addr));
    
        doread(rpdf_fd, &patch_length, sizeof(patch_length));
    
        DBGMSG("Spec: cmd=%d remapsz=0x%x ROM=0x%x RAM=0x%x length=0x%x\n",
                    rpdf_cmd, hw_remapsz, ROM_addr, RAM_addr, patch_length);
    
        /* Commit patch_length bytes of data from file to Target RAM. */
        {
            A_UINT32 RAM_segment_addr;
            A_UINT32 patch_segment_length;
    
            /*
             * Read data from file and commit to Target RAM in segments
             * of at most PATCH_BUF_SZ bytes each.
             */
            RAM_segment_addr = RAM_addr;
            while (patch_length > 0) {
                patch_segment_length = min(patch_length, PATCH_BUF_SZ);
                doread(rpdf_fd, patch_buffer, patch_segment_length);
                commit_patch_buffer_to_RAM(patch_buffer, RAM_segment_addr, patch_segment_length);
    
                patch_length -= patch_segment_length;
                RAM_segment_addr += patch_segment_length;
            }
        }
    
        if (hw_remapsz > 0) {
            install_patch(ROM_addr, RAM_addr, hw_remapsz);
        }
    
        if (rpdf_cmd == RPDF_CMD_INST_ACT) {
            activate_patches();
        }
    } else {
        fprintf(stderr, "Unknown RPDF command (0x%x) in RPDF file\n", rpdf_cmd);
        exit(-1);
    }
}

/* Loop through each Patch Specification in a Patch Distribution */
void
handle_patch_distribution(int rpdf_fd)
{
    int i;
    A_UINT32 patch_spec_count;

    doread(rpdf_fd, &patch_spec_count, sizeof(patch_spec_count));
    DBGMSG("Number of patch specifications : %d\n", patch_spec_count);

    for (i=1; i<=patch_spec_count; i++) {
        DBGMSG("Handle patch specification %d of %d\n", i, patch_spec_count);
        handle_patch_specification(rpdf_fd);
    }
}

int
main(int argc, char **argv)
{
    char filename[MAX_FILENAME_LEN]; /* RPDF file name */
    int rpdf_fd;
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
            {"file", 1, NULL, 0},
            {"help", 0, NULL, 0},
            {"interface", 1, NULL, 0},
            {"notarg", 0, NULL, 0},
            {"verbose", 0, NULL, 0},
            {0, 0, 0, 0}
        };
#define OPTION_FILE                     0
#define OPTION_HELP                     1
#define OPTION_IFNAME                   2
#define OPTION_NOTARG                   3
#define OPTION_VERBOSE                  4

        c = getopt_long (argc, argv, "+",
                         long_options, &option_index);

        if (c == -1) { /* done */
            break;
        }

        if (c != 0) { /* unrecognized option */
            usage();
        }

        switch(option_index) {
            case OPTION_FILE:
                if (filename_specified) {
                    fprintf(stderr, "ROM Patch Distribution File re-specified.\n");
                    confused_params();
                }
                filename_specified = 1;
                memset(filename, '\0', MAX_FILENAME_LEN);
                strncpy(filename, optarg, MAX_FILENAME_LEN-1);
                break;

            case OPTION_HELP:
                usage();
                break;

            case OPTION_IFNAME:
                memset(ifname, '\0', IFNAMSIZ);
                strncpy(ifname, optarg, IFNAMSIZ-1);
                break;

            case OPTION_NOTARG:
                use_target = 0;
                break;

            case OPTION_VERBOSE:
                verbose_enabled = 1;
                break;
        }
    }

    if (argc != optind) {
        fprintf(stderr, "Cannot understand '%s'.\n", argv[optind]);
        usage();
    }

    DBGMSG("interface=%s\n", ifname);
    DBGMSG("filename=%s\n", filename);

    if (!use_target) {
        printf("Dry run.  Will not touch Target.\n");
    }

    sanity_check_params();

    rpdf_fd = access_rpdf_file(filename);

    wait_for_target();

    handle_patch_distribution(rpdf_fd);

    DBGMSG("All patches have been installed\n");

    exit(0);
}
