//------------------------------------------------------------------------------
// <copyright file="mboxping.c" company="Atheros">
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
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <a_config.h>
#include <a_osapi.h>
#include <a_types.h>
#include <athdrv_linux.h>

#define MBOXTX                 0x01
#define MBOXRX                 0x02
#define PRELOAD                0x04
#define PKTSIZE                0x08
#define NUMPKTS                0x10
#define DURATION               0x20

#define MAXWAIT                10
#define MAX_PACKET_LEN         1536
#define MIN_PACKET_LEN         32

void catcher(int signo);
void pinger(void);
void finish(int signo);
void tvsub(register struct timeval *out, register struct timeval *in);
void pr_pack(unsigned char *buffer, int length);
void bindnetif(void);
void IndicateTrafficActivity(int StreamID, A_BOOL Active);

const char *progname;
const char commands[] =
"commands:\n\
--transmit=<mbox> --receive=<mbox> --size=<size> --count=<num> --preload=<num> --duration=<seconds> --verify --quiet \n\
--delay --random  --dumpcreditstates --wait --usepattern \n\
The 'delay' switch will flag each ping packet to trigger the target to delay the packet completion.  The delay \n\
is specified on the target and is usually some ratio of the length of the packet. \n\
The 'random' switch will randomize packets from the minimum packet size to the maximum of 'size' \n\
The 'dumpcreditstates' triggers the HTC layer to dump credit state information to the debugger console \n\
the driver must be compiled with debug prints enabled and the proper debug zone turned on \n\
The 'usepattern' switch forces test to use a known pattern in 'verify' mode \n\
The options can also be given in the abbreviated form --option=x or -o x. The options can be given in any order";
int sockfd;
pid_t ident;
unsigned int numTransmitted = 0;
unsigned int numReceived = 0;
struct sockaddr_ll my_addr;
unsigned int utmin = 999999999;
unsigned int utmax = 0;
unsigned int utsum = 0;
unsigned int numPkts = 0;
unsigned int duration = 10;
unsigned int mboxRx, mboxTx, g_Size, preload = 0;

#define TRUE 1
#define FALSE 0
int verifyMode = FALSE;
int crcErrors = 0;
int quiet = FALSE;
int dumpdata = FALSE;
int flagDelay = FALSE;
int randomLength = FALSE;
int dumpCreditState = FALSE;
int waitresources = FALSE;
int verfRandomData = TRUE;

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}


void NotifyError(char *description)
{
    printf("** Error stream test, send stream: %d loopback on: %d  reason: ",mboxTx, mboxRx);
    perror(description);
    printf("\n");
}


unsigned char ifname[IFNAMSIZ];

int
main (int argc, char **argv) {
    int options, c, i;
    unsigned char rxpacket[MAX_PACKET_LEN];
    unsigned int length;

    progname = argv[0];
    if (argc == 1) usage();

    memset(ifname, '\0', IFNAMSIZ);
    strcpy((char *)ifname, "eth1");

    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"transmit", required_argument, NULL, 't'},
            {"receive", required_argument, NULL, 'r'},
            {"size", required_argument, NULL, 's'},
            {"count", required_argument, NULL, 'c'},
            {"preload", required_argument, NULL, 'p'},
            {"interface", required_argument, NULL, 'i'},
            {"duration", required_argument, NULL, 'd'},
            {"verify", no_argument, NULL, 'v'},
            {"quiet", no_argument, NULL, 'q'},
            {"bufferdump", no_argument, NULL, 'b'},
            {"delay", no_argument, &flagDelay, TRUE},
            {"random", no_argument, &randomLength, TRUE},
            {"dumpcreditstates",no_argument,&dumpCreditState,TRUE},
            {"wait", no_argument, NULL, 'w'},
            {"usepattern",no_argument,&verfRandomData,FALSE},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "t:r:s:c:p:i:d:vqbw",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 't':
            mboxTx = atoi(optarg);
            options |= MBOXTX;
            break;

        case 'r':
            mboxRx = atoi(optarg);
            options |= MBOXRX;
            break;

        case 's':
            g_Size = atoi(optarg);
            options |= PKTSIZE;
            break;

        case 'c':
            numPkts = atoi(optarg);
            options |= NUMPKTS;
            break;

        case 'p':
            preload = atoi(optarg);
            options |= PRELOAD;
            break;

        case 'i':
            memset(ifname, '\0', IFNAMSIZ);
            strcpy((char *)ifname, optarg);
            break;

        case 'd':
            duration = atoi(optarg);
            options |= DURATION;
            break;

        case 'v':
            verifyMode = TRUE;
            printf("data-verify mode selected\n");
            break;
        case 'q':
            quiet = TRUE;
            printf("running quiet...\n");
            break;
        case 'b':
            dumpdata = TRUE;
            break;
        case 'y':
            flagDelay = TRUE;
            printf("packet loopback delay enabled...this will slow down throughput results...\n");
            break;
        case 'w':
            waitresources = TRUE;
            break;
        case 0:
            /* for options that are just simple flags */
            break;
        default:
            printf("invalid option : %d \n",c);
            usage();
        }
    }

    printf("netif : %s\n",ifname);

    if (flagDelay) {
        printf("packet loopback delay enabled...this will slow down throughput results...\n");
    }

    if (dumpCreditState) {
        struct ifreq ifr;
        unsigned int command;

        bindnetif();
        memset(&ifr,0,sizeof(ifr));
        strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
        command = AR6000_XIOCTL_DUMP_HTC_CREDIT_STATE;
        ifr.ifr_data = (char *)&command;

        printf("Sending command to %s to dump credit states \n",ifname);
        if (ioctl(sockfd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
        {
            perror("ioctl");
            exit(1);
        }

        exit(0);
    }

    if ((options & (MBOXTX | MBOXRX | PKTSIZE)) == (MBOXTX | MBOXRX | PKTSIZE))
    {

        if (randomLength) {
            printf("randomized packet lengths have been selected, range : 32 to %d bytes \n",
                g_Size);
        }

        if (verifyMode) {
            if (verfRandomData) {
                printf("verify mode uses random data\n");
            } else {
                printf("verify mode will use known pattern data\n");
            }
        }

        bindnetif();

        ident = getpid();
        setlinebuf(stdout);

            /* indicate that this stream is now active */
        IndicateTrafficActivity(mboxTx, TRUE);

        signal(SIGINT, finish);
        if (preload)
        {
            signal(SIGALRM, finish);
            for (i=0; i<preload; i++) pinger();
            alarm(duration);
        }
        else
        {
            signal(SIGALRM, catcher);
            catcher(0);
        }

        for (;;) {
            memset(rxpacket, '\0', MAX_PACKET_LEN);
            if ((length = recvfrom(sockfd, rxpacket, MAX_PACKET_LEN, 0, NULL, NULL)) < 0) {
                NotifyError("recvfrom");
                exit(1);
            }
            pr_pack(rxpacket, length);
            if (preload) {
                pinger();
            }
            else {
                if (numPkts && numReceived >= numPkts) {
                    finish(0);
                }
            }
        }
    }
    else usage();

    exit(0);
}

typedef struct _TRAFFIC_IOCTL_DATA {
    A_UINT32                       Command;
    struct ar6000_traffic_activity_change Data;
} TRAFFIC_IOCTL_DATA;

void IndicateTrafficActivity(int StreamID, A_BOOL Active)
{
    struct             ifreq ifr;
    TRAFFIC_IOCTL_DATA ioctlData;

    memset(&ifr,0,sizeof(ifr));
    memset(&ioctlData,0,sizeof(ioctlData));

    strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
    ioctlData.Command = AR6000_XIOCTL_TRAFFIC_ACTIVITY_CHANGE;
    ioctlData.Data.StreamID = StreamID;
    ioctlData.Data.Active = Active ? 1 : 0;

    ifr.ifr_data = (char *)&ioctlData;

    //printf("Sending command to %s, stream %d is now %s \n",
    //        ifname, StreamID, Active ? "Active" : "Inactive");

    if (ioctl(sockfd, AR6000_IOCTL_EXTENDED, &ifr) < 0)
    {
        perror("ioctl - activity");
        exit(1);
    }
}


void bindnetif(void)
{
    struct ifreq ifr;

    if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        NotifyError("socket");
        exit(1);
    }

    memset(&ifr, '\0', sizeof(struct ifreq));
    strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        NotifyError("SIOCGIFINDEX");
        exit(1);
    }

    memset(&my_addr, '\0', sizeof(struct sockaddr_ll));
    my_addr.sll_family = AF_PACKET;
    my_addr.sll_protocol = htons(ETH_P_ALL);
    my_addr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_ll)) < 0) {
        NotifyError("bind");
        exit(1);
    }
}

void
catcher(int signo)
{
    int waittime;

    pinger();
    if (numPkts == 0 || numTransmitted < numPkts) {
        alarm(1);
    }
    else {
        if (numReceived) {
            waittime = 2 * utmax / 1000;
            waittime = (waittime > 0) ? waittime : 1;
        }
        else {
            waittime = MAXWAIT;
        }
        signal(SIGALRM, finish);
        alarm(waittime);
    }
}



static unsigned int crc16table[256] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

unsigned short CalcCRC16(unsigned char *pBuffer, int length)
{
    unsigned int ii;
    unsigned int index;
    unsigned short crc16 = 0x0;

    for(ii = 0; ii < length; ii++) {
        index = ((crc16 >> 8) ^ pBuffer[ii]) & 0xff;
        crc16 = ((crc16 << 8) ^ crc16table[index]) & 0xffff;
    }
    return crc16;
}

/* simple dump buffer code */
void DumpBuffer(unsigned char *pBuffer, int Length, char *description)
{
    char  line[49];
    char  address[5];
    char  ascii[17];
    char  temp[5];
    int   i;
    unsigned char num;
    int offset = 0;

    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    printf("Description:%s  Length :%d \n",description, Length);
    printf("Offset                   Data                               ASCII        \n");
    printf("--------------------------------------------------------------------------\n");

    while (Length) {
        line[0] = (char)0;
        ascii[0] = (char)0;
        address[0] = (char)0;
        sprintf(address,"%4.4X",offset);
        for (i = 0; i < 16; i++) {
            if (Length != 0) {
                num = *pBuffer;
                sprintf(temp,"%2.2X ",num);
                strcat(line,temp);
                if ((num >= 0x20) && (num <= 0x7E)) {
                    sprintf(temp,"%c",*pBuffer);
                } else {
                    sprintf(temp,"%c",0x2e);
                }
                strcat(ascii,temp);
                pBuffer++;
                Length--;
            } else {
                    /* pad partial line with spaces */
                strcat(line,"   ");
                strcat(ascii," ");
            }
        }
        printf("%s    %s   %s\n", address, line, ascii);
        offset += 16;
    }
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

}

#define HDR_SIZE          20
#define HDR_CRC_START     3
#define HDR_BYTES_CRC    (HDR_SIZE - HDR_CRC_START)
#define DATA_OFFSET      (HDR_SIZE + 2)

void
pinger(void)
{
    unsigned char txpacket[MAX_PACKET_LEN];
    struct timeval tv, socktimeout;
    unsigned int length;
    int ret;
    int size;
    fd_set  writeset;
    unsigned short CRC16;

    if (randomLength) {
        float value;
            /* scale the size */
        value = ((float)g_Size * (float)rand()) / (float)RAND_MAX;
        size = (int)value;
    } else {
        size = g_Size;
    }

        /* min value */
    size = (size < MIN_PACKET_LEN) ? MIN_PACKET_LEN : size;

    /*
    0         1         2         3     7      15        16    20         22  .....
    | mboxRxH | mboxTxT | mboxRxT | PID | TxTS | mboxTxH | Seq | hdr CRC  |   DATA
    */

    /* Fill in the required book keeping informnation */
    length = 0;
    memset(txpacket, '\0', MAX_PACKET_LEN);
    txpacket[length++] = mboxRx; /* Receive mailbox number */
    if (flagDelay) {
            /* tell the target to delay this packet, on reception the target
             * checks this field, if its 0xaa55 then it is marked as requiring delayed
             * loopback, this provides more asynchronous processing */
        txpacket[length++] = 0xaa; /* mboxTx, filled by target */
        txpacket[length++] = 0x55; /* mboxRx, filled by target */
    } else {
        txpacket[length++] = 0xDE; /* mboxTx, filled by target */
        txpacket[length++] = 0xAD; /* mboxRx, filled by target */
    }
    memcpy(&txpacket[length], &ident, sizeof(pid_t)); /* Process ID */
    length += sizeof(pid_t);
    timerclear(&tv);
    if (gettimeofday(&tv, NULL) < 0) {
        NotifyError("gettimeofday");
        exit(1);
    }
    memcpy(&txpacket[length], &tv, sizeof(struct timeval)); /* Insert the timestamp */
    length += sizeof(struct timeval);
    txpacket[length++] = (mboxTx << 1); /* IP TOS Offset */
    memcpy(&txpacket[length], &numTransmitted, sizeof(numTransmitted));
    length += sizeof(numTransmitted); /* Sequence counter. Unique for every txpacket sent */
    numTransmitted += 1;

    CRC16 = CalcCRC16(&txpacket[HDR_CRC_START],HDR_BYTES_CRC);

        /* save hdr CRC */
    txpacket[length++] = (unsigned char)CRC16;
    txpacket[length++] = (unsigned char)(CRC16 >> 8);

    if (verifyMode) {
        unsigned char* pBuffer;
        int verifylength = 0;

            /* save start of buffer */
        pBuffer = &txpacket[length];
            /* seed the random number generator */
        srand((tv.tv_usec + numTransmitted));
            /* fill the remainder with data */
        while (length < (size - 2)) {
                /* stick in some random data */
            if (verfRandomData) {
                txpacket[length] = (unsigned char)rand() + length;
            } else {
                txpacket[length] = length;
            }
            length++;
            verifylength++;
        }

        CRC16 = CalcCRC16(pBuffer,verifylength);
            /* append 16 bit CRC at the end */
        txpacket[length++] = ((unsigned char *)(&CRC16))[1];
        txpacket[length++] = ((unsigned char *)(&CRC16))[0];

    } else {
        /* Fill the rest of the txpacket */
        while (length < size) {
            txpacket[length] = length;
            length += 1;
        }
    }

    if (dumpdata) {
        DumpBuffer(txpacket,size,"TX Data Dump");
    }

    FD_ZERO(&writeset);
    FD_SET(sockfd,&writeset);
    memset(&socktimeout,0,sizeof(socktimeout));
    socktimeout.tv_sec = 0;
    socktimeout.tv_usec = 100000;  /* 100 MS */

    while (1) {

            /* wait for socket to be writeable */
        if ((ret = select(sockfd + 1, NULL, &writeset, NULL, &socktimeout)) < 0) {
            NotifyError("select");
            exit(1);
        }

        if (!FD_ISSET(sockfd,&writeset)) {
            if (randomLength || flagDelay || waitresources) {
                /* if the randomizer or packet delay setting is enabled, it is possible to
                 * exhaust socket resources, we can try to be more persistent here */
                continue;
            } else {
                printf("socket write could not get buffer space \n");
                NotifyError("select");
                exit(1);
            }
        }

        break;
    }

    /* Send the packet */
    if ((ret = sendto(sockfd, txpacket, size, 0, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_ll))) < 0) {
        NotifyError("sendto");
        exit(1);
    }

    if (ret < size) {
        printf("stream ping: wrote %d out of %d chars\n", ret, size);
        fflush(stdout);
    }
}

void
finish(int signo)
{
    putchar('\n');
    fflush(stdout);
    printf("\n---- Stream PING Statistics ----\n");
    printf("Stream test complete. Send stream: %d loopback stream: %d \n",mboxTx, mboxRx);
    printf("Packets transmitted: %d\n", numTransmitted);
    printf("Packets received: %d\n", numReceived);
    if (verifyMode) {
        printf("Packet CRC errors: %d\n", crcErrors);
    }

    if (numTransmitted) {
       if (numReceived > numTransmitted) {
           printf("-- somebody's printing up packets!");
       }
       else {
           printf("%d%% Packet loss",
                  (int) (((numTransmitted-numReceived)*100) / numTransmitted));
       }
    }
    printf("\n");
    if (preload)
    {
        if (numReceived) {
            if (randomLength) {
                printf("Throughput = %f pkts/sec \n", (float)numReceived/duration);
            } else {
                printf("Throughput = %f pkts/sec, %f Mbps\n", (float)numReceived/duration,
                    (2.0*(float)numReceived/(float)duration)*(float)g_Size*8.0/1000000.0);
            }
        }
    }
    else
    {
        if (numReceived) {
            printf("User round-trip (us)  min/avg/max = %d/%d/%d\n",
                   utmin, utsum/numReceived, utmax);
#if 0
            printf("Kernel round-trip (us)  min/avg/max = %d/%d/%d\n",
                   ktmin, ktsum/numReceived, ktmax);
#endif
        }
    }
    fflush(stdout);

        /* we are done, mark the traffic on this stream as inactive */
    IndicateTrafficActivity(mboxTx, FALSE);

    exit(0);
}

void
tvsub(register struct timeval *out, register struct timeval *in)
{
    if((out->tv_usec -= in->tv_usec) < 0) {
        out->tv_sec--;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}

void
pr_pack(unsigned char *buffer, int length)
{
    struct timeval tv, tp;
    int triptime;
    pid_t pid;
    int seq;
    int txh, rxh, txt, rxt;
    unsigned short CRC16,verfCRC16;

    gettimeofday(&tv, NULL);

    if (length < MIN_PACKET_LEN) {
        printf(" Got invalid packet length of %d \n",length);
        exit(1);
    }
        /* CRC check only the fixed portion of the header, the target alters some of the fields */
    CRC16 = CalcCRC16(&buffer[HDR_CRC_START],HDR_BYTES_CRC);

        /* save hdr CRC */
    verfCRC16  = buffer[HDR_SIZE];
    verfCRC16 |= ((unsigned short)buffer[HDR_SIZE + 1]) << 8;

    txh = buffer[15];
    rxh = buffer[0];
    txt = buffer[1];
    rxt = buffer[2];
    memcpy(&pid, &buffer[3], sizeof(pid_t));

    if (CRC16 != verfCRC16) {
        printf(" Mbox ping header corrupted! CRC is: 0x%X, should be 0x%X, stream TX ? :%d, stream RX ? :%d , pid ? :0x%X\n",
            CRC16, verfCRC16, txt,rxt,pid);
        DumpBuffer(buffer,length,"Bad header");
        crcErrors++;
        return;
    }



//    if ((pid != ident) || (txh != rxt) || (rxh != txt)) {
    if (pid != ident) {
            /* not our packet, could be another instance running */
        return;
    }

    /* the target is suppose to echo these back */

    if (txt != mboxTx) {
        printf(" Target did not echo send stream correctly, was:%d  should be %d\n",
            txt,mboxTx);
        DumpBuffer(buffer,length,"Bad header");
        exit(1);
    }

    if (rxt != mboxRx) {
        printf(" Target did not echo recv stream correctly, was:%d  should be %d\n",
            rxt,mboxRx);
        DumpBuffer(buffer,length,"Bad header");
        exit(1);

    }

    memcpy(&tp, &buffer[7], sizeof(struct timeval));
    tvsub(&tv, &tp);
    triptime = tv.tv_sec*1000000 + tv.tv_usec;
    utsum += triptime;
    utmin = (triptime < utmin) ? triptime : utmin;
    utmax = (triptime > utmax) ? triptime : utmax;

    memcpy(&seq, &buffer[16], sizeof(seq));
    if (preload) {
        if (!quiet) {
            printf(".");
        }
    } else {
        printf("Sequence Number: %d, Triptime: %d\n", seq, triptime);
    }

    if (seq < numReceived) {
        printf("** Sequence Number: %d, should be >= %d.  Send stream: %d loopback on: %d \n",
                seq, numReceived, mboxTx, mboxRx);

    }

    numReceived += 1;

    if (dumpdata) {
        DumpBuffer(buffer,length,"RX dump");
    }

    if (verifyMode) {
        unsigned char *pBuffer = &buffer[DATA_OFFSET];
        unsigned short crc16,verifyCrc16;
        int crcLength;

            /* last 2 bytes is the CRC */
        crc16 = ((unsigned short)buffer[length - 1]) | (((unsigned short)buffer[length - 2]) << 8);

            /* compute CRC , do not include all the header info and do not include CRC bytes*/
        crcLength = length - DATA_OFFSET - 2;
        if (crcLength > 0) {
            verifyCrc16 = CalcCRC16(pBuffer,crcLength);
            if (crc16 != verifyCrc16) {
                int i;
                printf("Sequence Number: %d, buffer CRC error (got:0x%4.4X expecting:0x%4.4X) (stream TX:%d: stream RX:%d)\n",
                     seq, verifyCrc16, crc16, mboxTx, mboxRx);
                crcErrors++;

                if (!verfRandomData) {
                    /* if the data is a known pattern, walk the pattern and find mismatches */
                    for (i = DATA_OFFSET; i < (length - 2); i++) {
                        if (buffer[i] != (unsigned char)i) {
                            printf("offset:0x%4.4X got:0x%2.2X, expecting: 0x%2.2X \n",
                                i, buffer[i], (unsigned char)i);
                        }
                    }
                }
                DumpBuffer(buffer,length,"buffer CRC error");
            }

        } else {
            printf("*** not enough bytes to calculate CRC!! \n");
        }
    }



}
