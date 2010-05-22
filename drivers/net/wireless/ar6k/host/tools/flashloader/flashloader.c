/*
 * Copyright (c) 2005 Atheros Communications Inc.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <err.h>

#include "a_config.h"
#include "a_osapi.h"
#include "athdefs.h"
#include "a_types.h"
#include <athdrv_linux.h>
#include "wmi.h"
#include "flash.h"
#include "htc_api.h"
#include "targaddrs.h"

/*
 * FlashLoader Exit Codes.
 * These offer scripts an opportunity to use flashloader more intelligently.
 */
#define FLEC_SUCCESS                       0
#define FLEC_USAGE                         1
#define FLEC_BD_OVERWRITE                  2
#define FLEC_COMM_FAIL                     3
#define FLEC_FILE                          4
#define FLEC_UNUSED5                       5
#define FLEC_UNUSED6                       6
#define FLEC_FLASH_NOT_EMPTY               7
#define FLEC_ADDR_RANGE                    8
#define FLEC_UNUSED9                       9
#define FLEC_INTERNAL_ERROR               10
#define FLEC_VERIFY_FAILED                11

/* Flashloader commands */
#define FLASHLOADER_READ        1
#define FLASHLOADER_WRITE       2
#define FLASHLOADER_ERASE       3
#define FLASHLOADER_DONE        4
#define FLASHLOADER_VERIFY      5

/* Options to flashloader command */
#define ADDRESS_FLAG                    0x01
#define LENGTH_FLAG                     0x02
#define FILE_FLAG                       0x04
#define OVERRIDE_FLAG                   0x08
#define BOARDDATA_FLAG                  0x10
#define UNUSED_FLAG_0x20                0x20
#define VERSION_FLAG                    0x40
#define IMAGE_FLAG                      0x80
#define COMMIT_FLAG                    0x100
#define FLASHPART_FLAG                 0x200
#define PARAM_FLAG                     0x400

#define PAYLOAD_OFFSET                  64
#define MAX_PACKET_LEN                  254

const char *progname;
const char commands[] = 
"commands:\n\
--erase --address=<target address> --length=<bytes>\n\
--write --address=<target address> --file=<filename>\n\
        --address=<target address> --param=<32-bit value> --image=<filename>\n\
        --boarddata --file=<filename>\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
       --boarddata --file=<filename>\n\
--commit --image=filename\n\
--verify\n\
\n\
Additional options can be added:\n\
--override       override safety checks, and allows operations that may\n\
                 be destructive (i.e. erase Board Data)\n\
\n\
--image=filename operate on a local file that contains a complete flash image\n\
                 rather than on flash itself\n\
--part=address   Address in Target RAM of flashpart initialization function\n\
--done\n\
\n\
Most options can also be given in the abbreviated form --option=x or -o x.\n\
Options can be given in any order";

const char cal_data_erase_warning[] = "\
WARNING: This command may destroy a special area of flash that\n\
contains data that is specific to your hardware!  This data\n\
was programmed into your flash during the manufacture process.\n\
Wireless hardware will not function without this data.  If you\n\
really want to risk overwriting this data, use the --override option.\n\
First use the options --read --boarddata --file=<filename> to save\n\
the existing board data to a file.\n";

int sockfd;
int filefd = -1;
int imagefd = -1;
struct sockaddr_ll my_addr;
unsigned char packet[MAX_PACKET_LEN+PAYLOAD_OFFSET];
int flag;
struct ifreq ifr;
unsigned char ifname[IFNAMSIZ];
unsigned int flashpart;

void check_erased(unsigned int address, unsigned int length);
int imageop(unsigned char *buf);

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    exit(FLEC_USAGE);
}

/*
 * This acts as an interceptor.  If IMAGE_FLAG, it passes
 * args through to sendto so that a flash operation occurs.  If
 * IMAGE_FLAG is set, the request is diverted to a local "flash
 * image file".
 */
#ifndef HTC_RAW_INTERFACE
#define SENDTO(sockfd, packet, length, flags, sockaddr, socklen)               \
((flag & IMAGE_FLAG) ?                                                         \
        imageop((packet)) :                                                    \
        sendto((sockfd), (packet), (length), (flags), (sockaddr), (socklen)))
#else
int SENDTO(int sockfd, char *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) 
{
    int ret;

    if (flag & IMAGE_FLAG) {
        ret = imageop(((unsigned char *)buf));
    } else {
        memset(&ifr, '\0', sizeof(struct ifreq));
        strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
        ifr.ifr_data = (char *)malloc(12 + len);
        ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_WRITE;
        ((int *)ifr.ifr_data)[1] = 0;
        ((int *)ifr.ifr_data)[2] = len;
        memcpy(&(((char *)(ifr.ifr_data))[12]), (char *)buf, len);
        if (ioctl(sockfd, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            perror("write ioctl");
            exit(1);
        }
        ret = ((int *)ifr.ifr_data)[0];
        free(ifr.ifr_data);
    }

    return ret;
}
#endif /* HTC_RAW_INTERFACE */

#define FLASH_IMAGE_SZ (512*1024)

/*
 * Use an image file to simulate flash operations.
 *
 * This function interprets a command that would have been sent to the
 * Target, and performs a similar operation on a local flash image.
 * The pseudo-flash image effectively has a sector size of 1, so we
 * don't have to deal with accidental erasure that occurs with larger
 * sector sizes.
 */
int
imageop(unsigned char *buf)
{
    unsigned int command;
    unsigned int addr;
    unsigned int length;
    int i;

    buf += PAYLOAD_OFFSET;
    memcpy(&command, buf, sizeof(command));
    buf += sizeof(command);

    switch(command) {
    case FLASH_READ:
        memcpy(&addr, buf, sizeof(addr));
        buf += sizeof(addr);
        memcpy(&length, buf, sizeof(length));

        /* Make addr relative to start of image file */
        addr -= TARG_FLASH_ADDRS(0);

        if (lseek(imagefd, addr, SEEK_SET) < 0)
        {
            fprintf(stderr, "lseek in image file failed. Invalid address (0x%x)?\n", addr);
            perror("lseek");
            exit(FLEC_ADDR_RANGE);
        }
        if (read(imagefd, &packet[PAYLOAD_OFFSET], length) < 0)
        {
            fprintf(stderr, "Read from image file failed.  addr=0x%x length=0x%x\n",
                addr, length);
            fprintf(stderr, "file desc is %d\n", imagefd);
            perror("read");
            exit(FLEC_ADDR_RANGE);
        }
        break;

    case FLASH_WRITE:
        memcpy(&addr, buf, sizeof(addr));
        buf += sizeof(addr);
        memcpy(&length, buf, sizeof(length));
        buf += sizeof(length);

        /* Make addr relative to start of image file */
        addr -= TARG_FLASH_ADDRS(0);

        if (lseek(imagefd, addr, SEEK_SET) < 0)
        {
            fprintf(stderr, "lseek in image file failed. Invalid address (0x%x)?\n", addr);
            perror("lseek");
            exit(FLEC_ADDR_RANGE);
        }
        if (write(imagefd, buf, length) < 0)
        {
            fprintf(stderr, "Write to image file failed. Invalid length?\n");
            perror("write");
            exit(FLEC_ADDR_RANGE);
        }
        break;

    case FLASH_PARTIAL_ERASE:
        memcpy(&addr, buf, sizeof(addr));
        buf += sizeof(addr);
        memcpy(&length, buf, sizeof(length));

        /* Make addr relative to start of image file */
        addr -= TARG_FLASH_ADDRS(0);

        if (lseek(imagefd, addr, SEEK_SET) < 0)
        {
            fprintf(stderr, "lseek in image file failed. Invalid address (0x%x)?\n", addr);
            perror("lseek");
            exit(FLEC_ADDR_RANGE);
        }
        for (i=0; i<length; i++)
        {
            char erasebyte = 0xff;
            if (write(imagefd, &erasebyte, 1) < 0)
            {
                fprintf(stderr, "Partial erase image file failed.\n");
                perror("write");
                exit(FLEC_ADDR_RANGE);
            }
        }
        break;

    case FLASH_ERASE:
        memcpy(&addr, buf, sizeof(addr));
        if (addr == FLASH_ERASE_COOKIE)
        {
            for (i=0; i<FLASH_IMAGE_SZ; i+=4)
            {
                unsigned int eraseword = 0xffffffff;
                if (write(imagefd, &eraseword, sizeof(eraseword)) < 0)
                {
                    fprintf(stderr, "Full erase image file failed.\n");
                    perror("write");
                    exit(INTERNAL_ERROR);
                }
            }
        }
        break;

    case FLASH_PART_INIT:
        break;

    case FLASH_DONE:
        break;

    default: {
        fprintf(stderr, "Unimplemented flash command (%d)?!\n", command);
        exit(FLEC_INTERNAL_ERROR);
        break;
    }
    }

    return 0;
}

/*
 * Issue a warning and exit if it appears that the user may
 * overwrite board data.
 */
void
check_bd_overwrite(unsigned int address, unsigned int length)
{
    unsigned int start_addr;
    unsigned int end_addr;

    start_addr = AR6000_BOARD_DATA_ADDR;
    end_addr = AR6000_BOARD_DATA_ADDR + AR6000_BOARD_DATA_SZ - 1;

    if (((address + length - 1) >= start_addr) && 
        (address <= end_addr))
    {
        fprintf(stderr, "%s", cal_data_erase_warning);
        exit(FLEC_BD_OVERWRITE);
    }
}

/* This should be sufficiently conservative to work for many flash parts.  */
#define FLASH_SECTOR_SIZE (128*1024)

/*
 * Issue a warning and exit if it appears that the user may
 * erase board data.
 */
void
check_bd_erase(unsigned int address, unsigned int length)
{
    unsigned int start_addr;
    unsigned int end_addr;

    if (flag & IMAGE_FLAG)
    {
        /* Effectively, a sector size of 1 byte */
        check_bd_overwrite(address, length);
    }
    else
    { 
        /* Actually writing to Flash on Target */

        /* Adjust start/length for sector size */
        start_addr = address & ~(FLASH_SECTOR_SIZE-1);
        end_addr = (address+length+FLASH_SECTOR_SIZE-1) & ~(FLASH_SECTOR_SIZE-1);
        check_bd_overwrite(start_addr, end_addr-start_addr);
    }
}

int
valid_address_range(unsigned int address, unsigned int length)
{
    if ((address < TARG_FLASH_ADDRS(0)) || 
        ((address + length) > AR6000_FLASH_ADDR(0x100000)) ||
        ((length <= 0) || (length > 0x80000)))
    {
        fprintf(stderr, "Unexpected address(0x%x) or length(%d)\n", 
               address, length);
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
 * Use packet to read length (must be < MAX_PACKET_LEN) bytes from
 * flash address.
 *
 * Uses globals sockfd, my_addr, and packet.
 *
 * Data that is read is returned in packet.
 */
void
flash_read(unsigned int address,
           unsigned int length)
{
    unsigned int command;
    unsigned int offset;
    int ret;

    /* Frame and send the command on mailbox zero */
    memset(packet, '\0', sizeof(packet));
    packet[15] = (0 << 1);
    offset = PAYLOAD_OFFSET;
    command = FLASH_READ;
    memcpy(&packet[offset], &command, sizeof(command));
    offset += sizeof(command);
    memcpy(&packet[offset], &address, sizeof(address));
    offset += sizeof(address);
    memcpy(&packet[offset], &length, sizeof(length));
    offset += sizeof(length);
    if ((ret = SENDTO(sockfd, (char *)packet, offset, 0, 
                      (struct sockaddr *)&my_addr, 
                      sizeof(struct sockaddr_ll))) < 0) 
    {
        perror("sendto");
        exit(FLEC_COMM_FAIL);
    }

    /* Receive the requested amount of data */
    if (!(flag & IMAGE_FLAG)) { /* hack to support image file reads */
        memset(packet, '\0', sizeof(packet));
#ifndef HTC_RAW_INTERFACE
        if ((length = recvfrom(sockfd, packet, length+PAYLOAD_OFFSET, 0, 
                               NULL, NULL)) < 0) {
            perror("recvfrom");
            exit(FLEC_COMM_FAIL);
        }
#else
        memset(&ifr, '\0', sizeof(struct ifreq));
        strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
        ifr.ifr_data = (char *)malloc(12 + length + PAYLOAD_OFFSET);
        ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_READ;
        ((int *)ifr.ifr_data)[1] = 0;
        ((int *)ifr.ifr_data)[2] = length + PAYLOAD_OFFSET;
        if (ioctl(sockfd, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            perror("read ioctl");
            exit(1);
        }
        length = ((int *)ifr.ifr_data)[0];
        memcpy(packet, &(((char *)(ifr.ifr_data))[4]), length);
        free(ifr.ifr_data);
#endif /* HTC_RAW_INTERFACE */
    }
}

/*
 * Open a file and exit with a message if it can't be opened.
 */
int
file_open(char *filename,
          int flags,
          mode_t mode)
{
    int fd;

    fd = open(filename, flags, mode);
    if (fd < 0) {
        perror(filename);
        exit(FLEC_FILE);
    }

    return fd;
}

/* Return the length of an open file. */
unsigned int
file_length(int fd)
{
    struct stat filestat;
    int length;

    memset(&filestat, '\0', sizeof(struct stat));
    fstat(fd, &filestat);
    length = filestat.st_size;

    return length;
}

int
min_length(unsigned int length,
           unsigned int actual_length,
           char *filename)
{
    if (flag & LENGTH_FLAG)
    {
        if (actual_length < length)
        {
            fprintf(stderr, "Actual length (%d) of file %s is less than requested length (%d)\n",
                actual_length, filename, length);
            exit(FLEC_FILE);
        }
        return length;
    }
    else
    {
        return actual_length;
    }
}

void
check_image_file()
{
    unsigned int length;

    length = file_length(imagefd);
    if (length != FLASH_IMAGE_SZ)
    {
        fprintf(stderr, "Flash image file is not expected size (0x%x).\n",
                FLASH_IMAGE_SZ);
        exit(FLEC_FILE);
    }
}

/*
 * Read from flash to a file.
 * Read length bytes from flash into filefd.
 */
void
flash_to_file(unsigned int address,
              unsigned int length)
{
    unsigned int remaining;

    remaining = length;
    while (remaining) {
        length = (remaining <= MAX_PACKET_LEN) ? remaining : MAX_PACKET_LEN;
        flash_read(address, length);
        if (write(filefd, packet+PAYLOAD_OFFSET, length) < 0)
        {
            perror("write");
            exit(FLEC_FILE);
        }
        remaining -= length;
        address += length;
    }
}

char verify_buffer[MAX_PACKET_LEN];

void
verify_file(unsigned int address,
            unsigned int length)
{
    unsigned int remaining;

    remaining = length;
    while (remaining) {
        length = (remaining <= MAX_PACKET_LEN) ? remaining : MAX_PACKET_LEN;
        flash_read(address, length);
        if (read(filefd, verify_buffer, length) < 0)
        {
            perror("read");
            exit(FLEC_FILE);
        }
        if (memcmp(verify_buffer, packet+PAYLOAD_OFFSET, length))
        {
            printf("Verification failed [address=0x%x, length=%d]\n",
                        address, length);

            exit(FLEC_VERIFY_FAILED);
        }
        remaining -= length;
        address += length;
    }
}

/*
 * Verify that flash is empty in the range specified by address/length.
 * If erased return 1; if NOT erased, return 0.
 */
int
is_erased(unsigned int address,
          unsigned int length)
{
    unsigned int remaining;
    char *flashbyte = (char *)packet+PAYLOAD_OFFSET;
    int i;

    remaining = length;
    while (remaining) {
        length = (remaining <= MAX_PACKET_LEN) ? remaining : MAX_PACKET_LEN;
        flash_read(address, length);

        for (i=0; i<length; i++) {
            if (flashbyte[i] != (char)0xff) {
                return 0; /* Flash is dirty in this range */
            }
        }

        remaining -= length;
        address += length;
    }
    return 1; /* Flash is erased in this range */
}

/*
 * Verify that flash is empty in the range specified by address/length.
 * If not, print a warning and exit.
 */
void
check_erased(unsigned int address,
             unsigned int length)
{
    if (!is_erased(address, length))
    {
                fprintf(stderr, "Flash not empty in range 0x%x..0x%x\n",
                        address, address+length-1);
                exit(FLEC_FLASH_NOT_EMPTY);
    }
}

/*
 * Validate address range and erase flash in that range.
 * If there's anything unsafe/risky about this range,
 * print a warning message and exit.
 */
int
prepare_flash(unsigned int address,
              unsigned int length)
{
    unsigned int command;
    unsigned int offset;
    int ret;

    if (!valid_address_range(address, length))
    {
        if ((flag & OVERRIDE_FLAG) != OVERRIDE_FLAG) {
            exit(FLEC_ADDR_RANGE);
        }
    }

    if (is_erased(address, length))
    {
        /*
         * Flash is already clear in this range, so we just have
         * to avoid accidently overwriting Board Data.
         */
        if ((flag & (OVERRIDE_FLAG | BOARDDATA_FLAG)) == 0)
        {
            check_bd_overwrite(address, length);
        }
        return 0;
    }
    else
    {
        /*
         * We need to partially erase flash in order to do
         * this write; so check for accidental erasure of
         * Board Data.
         */
        if ((flag & (OVERRIDE_FLAG | BOARDDATA_FLAG)) == 0)
        {
            check_bd_erase(address, length);
        }

        /* Do a partial erase before writing */
        memset(packet, '\0', sizeof(packet));
        packet[15] = (0 << 1);
        command = FLASH_PARTIAL_ERASE;
        offset = PAYLOAD_OFFSET;
        memcpy(&packet[offset], &command, sizeof(command));
        offset += sizeof(command);
        memcpy(&packet[offset], &address, sizeof(address));
        offset += sizeof(address);
        memcpy(&packet[offset], &length, sizeof(length));
        offset += sizeof(length);
        if ((ret = SENDTO(sockfd, (char *)packet, offset, 0,
                          (struct sockaddr *)&my_addr,
                          sizeof(struct sockaddr_ll))) < 0) {
            perror("sendto");
            exit(1);
        }
        return 1;
    }
}

/* 
 * Write length bytes from a file (filefd) to flash (sockfd) at address.
 */
void
file_to_flash(unsigned int address,
              unsigned int length)
{
    unsigned int remaining;
    unsigned int command;
    unsigned int offset;
    int ret;

    /* Frame and send the WRITE command on mailbox zero */
    command = FLASH_WRITE;
    remaining = length;
    while (remaining) {
        memset(packet, '\0', sizeof(packet));
        packet[15] = (0 << 1);
        offset = PAYLOAD_OFFSET;
        memcpy(&packet[offset], &command, sizeof(command));
        offset += sizeof(command);
        memcpy(&packet[offset], &address, sizeof(address));
        offset += sizeof(address);
        length = MAX_PACKET_LEN - offset - sizeof(length);
        length = (remaining < length) ? remaining : length;
        memcpy(&packet[offset], &length, sizeof(length));
        offset += sizeof(length);
        read(filefd, &packet[offset], length);
        offset += length;
        if ((ret = SENDTO(sockfd, (char *)packet, offset, 0, 
                          (struct sockaddr *)&my_addr, 
                          sizeof(struct sockaddr_ll))) < 0)
        {
            perror("sendto");
            exit(FLEC_COMM_FAIL);
        }
        address += length;
        remaining -= length;
    }
}

/* 
 * Write length bytes from a buffer to flash (sockfd) at address.
 */
void
buf_to_flash(unsigned char *buf,
             unsigned int address,
             unsigned int length)
{
    unsigned int remaining;
    unsigned int command;
    unsigned int offset;
    int ret;

    /* Frame and send the WRITE command on mailbox zero */
    command = FLASH_WRITE;
    remaining = length;
    while (remaining) {
        memset(packet, '\0', sizeof(packet));
        packet[15] = (0 << 1);
        offset = PAYLOAD_OFFSET;
        memcpy(&packet[offset], &command, sizeof(command));
        offset += sizeof(command);
        memcpy(&packet[offset], &address, sizeof(address));
        offset += sizeof(address);
        length = MAX_PACKET_LEN - offset - sizeof(length);
        length = (remaining < length) ? remaining : length;
        memcpy(&packet[offset], &length, sizeof(length));
        offset += sizeof(length);
        memcpy(&packet[offset], buf, length);
        offset += length;
        if ((ret = SENDTO(sockfd, (char *)packet, offset, 0, 
                          (struct sockaddr *)&my_addr, 
                          sizeof(struct sockaddr_ll))) < 0)
        {
            perror("sendto");
            exit(FLEC_COMM_FAIL);
        }
        buf += length;
        address += length;
        remaining -= length;
    }
}

int
main (int argc, char **argv) {
    int c, ret, cookie;
    unsigned int address, length, version;
    char filename[128], imagename[128];
    unsigned int flashloader_command;
    unsigned int command;
    unsigned int offset;
    unsigned int param;

    progname = basename(argv[0]);

    if (argc == 1) {
        usage();
    }

    flag = 0;
    memset(ifname, '\0', IFNAMSIZ);
    strcpy((char *)ifname, "eth1");

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"read", 0, NULL, 'r'},
            {"file", 1, NULL, 'f'},
            {"done", 0, NULL, 'd'},
            {"write", 0, NULL, 'w'},
            {"erase", 0, NULL, 'e'},
            {"length", 1, NULL, 'l'},
            {"address", 1, NULL, 'a'},
            {"override", 0, NULL, 'o'},
            {"interface", 1, NULL, 'i'},
            {"boarddata", 0, NULL, 'b'},
            {"version", 1, NULL, 'v'},
            {"image", 1, NULL, 'I'},
            {"commit", 0, NULL, 'c'},
            {"verify", 0, NULL, 'V'},
            {"part", 1, NULL, 'p'},
            {"param", 1, NULL, 'P'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "rdweobf:l:a:i:v:i:c:V",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            flashloader_command = FLASHLOADER_READ;
            break;

        case 'w':
            flashloader_command = FLASHLOADER_WRITE;
            break;

        case 'e':
            flashloader_command = FLASHLOADER_ERASE;
            break;

        case 'd':
            flashloader_command = FLASHLOADER_DONE;
            break;

        case 'c':
            flashloader_command = FLASHLOADER_WRITE;
            flag |= COMMIT_FLAG;
            break;

        case 'V':
            flashloader_command = FLASHLOADER_VERIFY;
            break;

        case 'o':
            flag |= OVERRIDE_FLAG;
            break;

        case 'b':
            flag |= BOARDDATA_FLAG;
            break;

        case 'f':
            memset(filename, '\0', 128);
            strcpy(filename, optarg);
            flag |= FILE_FLAG;
            break;

        case 'l':
            length = strtoul(optarg, NULL, 0);
            flag |= LENGTH_FLAG;
            break;

        case 'a':
            address = strtoul(optarg, NULL, 0);
            flag |= ADDRESS_FLAG;
            break;

        case 'i':
            memset(ifname, '\0', 8);
            strcpy((char *)ifname, optarg);
            break;

        case 'v':
            version = strtoul(optarg, NULL, 0);
            flag |= VERSION_FLAG;
            break;

        case 'I':
            memset(imagename, '\0', 128);
            strcpy(imagename, optarg);
            flag |= IMAGE_FLAG;
            break;

        case 'p':
            flashpart=strtoul(optarg, NULL, 0);
            flag |= FLASHPART_FLAG;
            break;

        case 'P':
            param = strtoul(optarg, NULL, 0);
            flag |= PARAM_FLAG;
            break;

        default:
            usage();
        }
    }

    if ((flag & (IMAGE_FLAG | COMMIT_FLAG)) != IMAGE_FLAG)
    {
#ifndef HTC_RAW_INTERFACE
        if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
            perror("socket");
            exit(FLEC_COMM_FAIL);
        }
    
        memset(&ifr, '\0', sizeof(struct ifreq));
        strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
            perror("SIOCGIFINDEX");
            exit(FLEC_COMM_FAIL);
        }
    
        memset(&my_addr, '\0', sizeof(struct sockaddr_ll));
        my_addr.sll_family = AF_PACKET;
        my_addr.sll_protocol = htons(ETH_P_ALL);
        my_addr.sll_ifindex = ifr.ifr_ifindex;
        if (bind(sockfd, (struct sockaddr *)&my_addr, 
            sizeof(struct sockaddr_ll)) < 0)
        {
            perror("bind");
            exit(FLEC_COMM_FAIL);
        }
        strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
#else
        /* Open the raw HTC interface */
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket");
            exit(1);
        }
        memset(&ifr, '\0', sizeof(struct ifreq));
        strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
        ifr.ifr_data = (char *)malloc(4);
        ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_OPEN;
        if (ioctl(sockfd, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            perror("open ioctl");
            exit(1);
        }
        free(ifr.ifr_data);
#endif /* HTC_RAW_INTERFACE */
    }

    /*
     * If we're using a custom flashpart (not AMD16), let the
     * Target know where we've placed the flashpart code.
     */
    if (flag & FLASHPART_FLAG)
    {
        /* Frame and send the command on mailbox zero */
        memset(packet, '\0', sizeof(packet));
        packet[15] = (0 << 1);
        command = FLASH_PART_INIT;
        offset = PAYLOAD_OFFSET;
        memcpy(&packet[offset], &command, sizeof(command));
        offset += sizeof(command);
        memcpy(&packet[offset], &flashpart, sizeof(flashpart));
        offset += sizeof(flashpart);
        if ((ret = SENDTO(sockfd, (char *)packet, offset, 0, 
                          (struct sockaddr *)&my_addr, 
                          sizeof(struct sockaddr_ll))) < 0) {
            perror("sendto");
            exit(FLEC_COMM_FAIL);
        }
    }

    switch(flashloader_command)
    {
    case FLASHLOADER_DONE:
        printf("Flash Done\n");
        memset(packet, '\0', sizeof(packet));

        /* Frame and send the command on mailbox zero */
        packet[15] = (0 << 1);
        command = FLASH_DONE;
        offset = PAYLOAD_OFFSET;
        memcpy(&packet[offset], &command, sizeof(command));
        offset += sizeof(command);
        if ((ret = SENDTO(sockfd, (char *)packet, offset, 0, 
                          (struct sockaddr *)&my_addr, 
                          sizeof(struct sockaddr_ll))) < 0)
        {
            perror("sendto");
            exit(FLEC_COMM_FAIL);
        }

        /* Close the RAW HTC interface */
        memset(&ifr, '\0', sizeof(struct ifreq));
        strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
        ifr.ifr_data = (char *)malloc(4);
        ((int *)ifr.ifr_data)[0] = AR6000_XIOCTL_HTC_RAW_CLOSE;
        if (ioctl(sockfd, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            perror("close ioctl");
            exit(1);
        }
        free(ifr.ifr_data);
        break;

    case FLASHLOADER_VERIFY:
        if (flag & COMMIT_FLAG)
        {
            usage();
        }

        /* FALL THROUGH */

    case FLASHLOADER_READ:
        if (flag & IMAGE_FLAG)
        {
            imagefd = file_open(imagename, O_RDONLY, 0);
            check_image_file();
        }

        if ((flag & BOARDDATA_FLAG) == BOARDDATA_FLAG)
        {
            if (flag & (ADDRESS_FLAG | LENGTH_FLAG))
            {
                usage();
            }
            address = AR6000_BOARD_DATA_ADDR;
            length = AR6000_BOARD_DATA_SZ;
            flag |= ADDRESS_FLAG | LENGTH_FLAG;
        }

        if ((flag & LENGTH_FLAG) == 0)
        {
            /* Implicit length */
            if (flashloader_command == FLASHLOADER_VERIFY)
            {
                filefd = file_open(filename, O_RDONLY, 0);
                length = file_length(filefd);
                close(filefd);
                flag |= LENGTH_FLAG;
            }
        }


        if ((flag & (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG)) == 
            (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG))
        {
            if (!valid_address_range(address, length))
            {
                if ((flag & OVERRIDE_FLAG) != OVERRIDE_FLAG) {
                    exit(FLEC_ADDR_RANGE);
                }
            }

            if (flashloader_command == FLASHLOADER_READ)
            {
                filefd = file_open(filename, O_CREAT|O_WRONLY|O_TRUNC, 00644);

                printf("Flash Read (address: 0x%x, length: %d, filename: %s)\n",
                       address, length, filename);

                flash_to_file(address, length);
            } 
            else
            { /* FLASHLOADER_VERIFY */
                unsigned int actual_length;

                filefd = file_open(filename, O_RDONLY, 0);
                actual_length = file_length(filefd);
                length = min_length(length, actual_length, filename);

                printf("Verify flash (address: 0x%x, length: %d) against %s)\n",
                       address, length, filename);

                verify_file(address, length);
            }
        }
        else if ((flag & (ADDRESS_FLAG | FILE_FLAG)) == FILE_FLAG)
        {
            if (flashloader_command == FLASHLOADER_READ)
            {
                filefd = file_open(filename, O_CREAT|O_WRONLY|O_TRUNC, 00644);
                if ((flag & LENGTH_FLAG) == 0)
                {
                    length = FLASH_IMAGE_SZ;
                }
                printf("Read %d bytes of flash to file %s\n", length, filename);
                flash_to_file(TARG_FLASH_ADDRS(0), length);
            }
            else
            { /* FLASHLOADER_VERIFY */
                filefd = file_open(filename, O_RDONLY, 0);
                printf("Verify %d bytes of flash against %s\n", length, filename);
                verify_file(TARG_FLASH_ADDRS(0), length);
            }
        }
        else {
            usage();
        }
        break;

    case FLASHLOADER_WRITE:
        if (flag & COMMIT_FLAG)
        {
            if (!(flag & IMAGE_FLAG))
            {
                fprintf(stderr, "The --commit option requires --image.\n");
                usage();
            }
            if (flag & (ADDRESS_FLAG     |
                        LENGTH_FLAG      |
                        FILE_FLAG        |
                        OVERRIDE_FLAG    |
                        BOARDDATA_FLAG   |
                        VERSION_FLAG))
            {
                fprintf(stderr, "Unexpected flag with --commit.\n");
                usage();
            }
    
            imagefd = file_open(imagename, O_RDONLY, 0);
            check_image_file();
    
            /* Convert WRITE with COMMIT into WRITE with ADDRESS & FILE & OVERRIDE*/
            address = TARG_FLASH_ADDRS(0);
            strcpy(filename,  imagename);
            flag = (flag & ~IMAGE_FLAG) |
                        (ADDRESS_FLAG | FILE_FLAG | OVERRIDE_FLAG);
        }
        else
        {
            if (flag & IMAGE_FLAG)
            {
                imagefd = file_open(imagename, O_CREAT|O_RDWR, 00644);
                check_image_file();
            }
        }

        if ((flag & BOARDDATA_FLAG) == BOARDDATA_FLAG)
        {
            if (flag & (ADDRESS_FLAG | LENGTH_FLAG))
            {
                usage();
            }
            address = AR6000_BOARD_DATA_ADDR;
            length = AR6000_BOARD_DATA_SZ;
            flag |= ADDRESS_FLAG | LENGTH_FLAG;
        }

        if ((flag & (VERSION_FLAG | ADDRESS_FLAG | FILE_FLAG)) ==
                                                  (ADDRESS_FLAG | FILE_FLAG))
        {
            unsigned int actual_length;

            filefd = file_open(filename, O_RDONLY, 0);
            actual_length = file_length(filefd);

            if (flag & BOARDDATA_FLAG)
            {
                if (actual_length != AR6000_BOARD_DATA_SZ) {
                    fprintf(stderr, "Size of Board Data file %s incorrect.\n",
                                filename);
                    exit(FLEC_FILE);
                }
            }

            length = min_length(length, actual_length, filename);

            printf("Flash Write (address: 0x%x, filename: %s length: %d)\n",
               address, filename, length);

            prepare_flash(address, length);

            /* Write the file to flash */
            file_to_flash(address, length);

            flash_read(address, 2); /* flush */
        } else if ((flag & (VERSION_FLAG | ADDRESS_FLAG | PARAM_FLAG)) ==
                                                  (ADDRESS_FLAG | PARAM_FLAG))
        {
            if (flag & IMAGE_FLAG) {
                length = sizeof(int);

                printf("Flash Write (address: 0x%x, param: 0x%x)\n",
                   address, param);

                prepare_flash(address, length);
                buf_to_flash((unsigned char *)&param, address, length);
            } else {
                fprintf(stderr, "When using --param, you MUST also use --image.\n");
                    exit(FLEC_USAGE);
            }
        } else {
            usage();
        }
        break;

    case FLASHLOADER_ERASE:
        if (flag & IMAGE_FLAG)
        {
            imagefd = file_open(imagename, O_CREAT|O_RDWR, 00644);
            /*
             * Don't check_image_file here:
             * permit a full erase to work on non-conforming file.
             */
        }

        if ((flag & (ADDRESS_FLAG | LENGTH_FLAG)) == 
            (ADDRESS_FLAG | LENGTH_FLAG))
        {
            if (flag & IMAGE_FLAG)
            {
                check_image_file();
            }

            if (!valid_address_range(address, length))
            {
                if ((flag & OVERRIDE_FLAG) != OVERRIDE_FLAG) {
                    exit(FLEC_ADDR_RANGE);
                }
            }

            if ((flag & OVERRIDE_FLAG) != OVERRIDE_FLAG) {
                check_bd_erase(address, length);
            }

            printf("Flash Partial Erase (address: 0x%x, length: %d)\n",
                   address, length);

            /* Frame and send the command on mailbox zero */
            memset(packet, '\0', sizeof(packet));
            packet[15] = (0 << 1);
            command = FLASH_PARTIAL_ERASE;
            offset = PAYLOAD_OFFSET;
            memcpy(&packet[offset], &command, sizeof(command));
            offset += sizeof(command);
            memcpy(&packet[offset], &address, sizeof(address));
            offset += sizeof(address);
            memcpy(&packet[offset], &length, sizeof(length));
            offset += sizeof(length);
            if ((ret = SENDTO(sockfd, (char *)packet, offset, 0, 
                              (struct sockaddr *)&my_addr, 
                              sizeof(struct sockaddr_ll))) < 0) {
                perror("sendto");
                exit(FLEC_COMM_FAIL);
            }
            printf("Please wait for the flash erase to complete ...\n");

            /* Do a small read to ensure that the flash was erased */
            flash_read(address, 2);
        }
        else
        {
            if ((flag & OVERRIDE_FLAG) == OVERRIDE_FLAG)
            {
                printf("Erase entire flash\n");
                /* Frame and send the command on mailbox zero */
                memset(packet, '\0', sizeof(packet));
                packet[15] = (0 << 1);
                command = FLASH_ERASE;
                offset = PAYLOAD_OFFSET;
                memcpy(&packet[offset], &command, sizeof(command));
                offset += sizeof(command);
                cookie = FLASH_ERASE_COOKIE;
                memcpy(&packet[offset], &cookie, sizeof(cookie));
                offset += sizeof(cookie);
                if ((ret = SENDTO(sockfd, (char *)packet, offset, 0, 
                                  (struct sockaddr *)&my_addr, 
                                  sizeof(struct sockaddr_ll))) < 0) {
                    perror("sendto");
                    exit(FLEC_COMM_FAIL);
                }
                printf("Please wait for the flash erase to complete ...\n");

                /* Do a small read to ensure that the flash was erased */
                flash_read(TARG_FLASH_ADDRS(0), 2);
            }
            else
            {
                fprintf(stderr, "%s", cal_data_erase_warning);
                exit(FLEC_BD_OVERWRITE);
            }
        }
        break;

    default:
        usage();
    }


    exit (FLEC_SUCCESS);
}
