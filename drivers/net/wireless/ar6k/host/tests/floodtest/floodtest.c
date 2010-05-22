//------------------------------------------------------------------------------
// <copyright file="floodtest.c" company="Atheros">
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
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <net/if.h>

#include "athdefs.h"
#include "a_types.h"
#include "floodtest.h"

#define MBOXTX                 0x01
#define MBOXRX                 0x02
#define PKTSIZE                0x04
#define DURATION               0x08

#define MAX_PACKET_LEN         1536
#define CONFIG_MSG_LEN         64

void finish(int signo);

const char *progname;
const char commands[] =
"commands:\n\
--transmit=<mbox> --size=<size> --duration=<seconds>\n\
--receive=<mbox> --size=<size> --duration=<seconds>\n\
The options can also be given in the abbreviated form --option=x or -o x. The options can be given in any order";

int numTransmitted = 0;
int numReceived = 0;
int duration = 1;
int size = 0;

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}

int
main (int argc, char **argv) {
    int sockfd, count;
    int options, c, ret;
    struct ifreq ifr;
    unsigned char packet[MAX_PACKET_LEN];
    unsigned char ifname[IFNAMSIZ];
    struct sockaddr_ll my_addr;
    unsigned int length;
    unsigned int mboxTx;
    unsigned int mboxRx;
    struct floodtest_control_s config;
    int gap = 80000;

    progname = argv[0];
    if (argc == 1) usage();

    memset(ifname, '\0', IFNAMSIZ);
    strcpy((char *)ifname, "eth1");

    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"interface", 1, NULL, 'i'},
            {"transmit", 1, NULL, 't'},
            {"receive", 1, NULL, 'r'},
            {"size", 1, NULL, 's'},
            {"duration", 1, NULL, 'd'},
            {"gap", 1, NULL, 'g'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "i:t:r:s:d:g:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'i':
            memset(ifname, '\0', IFNAMSIZ);
            strcpy((char *)ifname, optarg);
            break;

        case 't':
            mboxTx = atoi(optarg);
            options |= MBOXTX;
            break;

        case 'r':
            mboxRx = atoi(optarg);
            options |= MBOXRX;
            break;

        case 's':
            size = atoi(optarg);
            options |= PKTSIZE;
            break;

        case 'd':
            duration = atoi(optarg);
            options |= DURATION;
            break;

        case 'g':
            gap = atoi(optarg);
            break;

        default:
            usage();
        }
    }

    if ((options & PKTSIZE) == PKTSIZE)
    {
        if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
            perror("socket");
            exit(1);
        }

        memset(&ifr, '\0', sizeof(struct ifreq));
        strncpy(ifr.ifr_name, (char *)ifname, sizeof(ifr.ifr_name));
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
            perror("SIOCGIFINDEX");
            exit(1);
        }

        memset(&my_addr, '\0', sizeof(struct sockaddr_ll));
        my_addr.sll_family = AF_PACKET;
        my_addr.sll_protocol = htons(ETH_P_ALL);
        my_addr.sll_ifindex = ifr.ifr_ifindex;
        if (bind(sockfd, (struct sockaddr *)&my_addr,
                 sizeof(struct sockaddr_ll)) < 0) {
            perror("bind");
            exit(1);
        }

        setlinebuf(stdout);
        signal(SIGINT, finish);
        signal(SIGALRM, finish);

        if ((options & MBOXTX) == MBOXTX) {
            /* Tx flood */
            memset(packet, '\0', MAX_PACKET_LEN);

            /* Frame the configuration message */
            config.direction = FLOOD_TX;
            config.message_size = 0;
            config.duration = 0;
            memcpy(packet, &config, sizeof(struct floodtest_control_s));
            packet[15] = (mboxTx << 1);

            /* Send the message */
            if ((ret = sendto(sockfd, packet, CONFIG_MSG_LEN, 0,
                              (struct sockaddr *)&my_addr,
                              sizeof(struct sockaddr_ll))) < 0) {
                perror("sendto");
                exit(1);
            }

            alarm(duration);
            for (;;) {
                memset(packet, '\0', MAX_PACKET_LEN);
                memcpy(&packet[0], &numTransmitted, sizeof(numTransmitted));

                for (count = sizeof(numTransmitted); count < size; count ++) {
                    packet[count] = count;
                }
                packet[15] = (mboxTx << 1);

                /* Send the packet */
                if ((ret = sendto(sockfd, packet, size, 0,
                                  (struct sockaddr *)&my_addr,
                                  sizeof(struct sockaddr_ll))) < 0) {
                    perror("sendto");
                    exit(1);
                }

                numTransmitted += 1;
                printf(".");

                /* Introduce some delay */
                for (count = 0; count < gap; count ++) {}
            }
        } else if ((options & MBOXRX) == MBOXRX) {
            /* Rx flood */
            memset(packet, '\0', MAX_PACKET_LEN);

            /* Frame the configuration message */
            config.direction = FLOOD_RX;
            config.message_size = size;
            config.duration = duration;
            memcpy(packet, &config, sizeof(struct floodtest_control_s));
            packet[15] = (mboxRx << 1);

            /* Send the message */
            if ((ret = sendto(sockfd, packet, CONFIG_MSG_LEN, 0,
                              (struct sockaddr *)&my_addr,
                              sizeof(struct sockaddr_ll))) < 0) {
                perror("sendto");
                exit(1);
            }

            alarm(duration + 1);
            for (;;) {
                memset(packet, '\0', MAX_PACKET_LEN);
                if ((length = recvfrom(sockfd, packet, MAX_PACKET_LEN,
                                       0, NULL, NULL)) < 0) {
                    perror("recvfrom");
                    exit(1);
                }
                numReceived += 1;
                printf(".");
            }
        } else {
            usage();
        }
    }
    else usage();

    exit(0);
}


void
finish(int signo)
{
    putchar('\n');
    fflush(stdout);
    printf("\n---- FLOODTEST Statistics ----\n");

    if (numTransmitted) {
        printf("Packets transmitted: %d\n", numTransmitted);
        printf("Throughput: %f (pkts/sec), %f (Mb/s)\n",
               ((float)numTransmitted/(float)duration),
               ((float)numTransmitted/(float)(1000000*duration))*(size*8));
    } else if (numReceived) {
        printf("Packets received: %d\n", numReceived);
        printf("Throughput: %f (pkts/sec), %f (Mb/s)\n",
               ((float)numReceived/(float)duration),
               ((float)numReceived/(float)(1000000*duration))*(size*8));
    } else {
        printf("Throughput: %f (pkts/sec), %f (Mb/s)\n", 0.0, 0.0);
    }

    fflush(stdout);
    exit(0);
}
