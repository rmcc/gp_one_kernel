/*
 * Copyright (c) 2006 Atheros Communications Inc.
 * All rights reserved.
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
#include <athdefs.h>
#include <a_types.h>
#include <wmi.h>
#include <testcmd.h>
#include <regDb.h>

#include "athdrv_linux.h"
#include "athtestcmd.h"


const char *progname;
const char commands[] =
"commands:\n\
--tx <sine/frame/tx99/tx100/off> --txfreq <Tx channel or freq(default 2412)> --txrate <rate index> \n\
      --txpwr <frame/tx99/tx100: 0-14dBm> --txantenna <1/2/0 (auto)>\n\
      --txpktsz <pkt size, [32-1500](default 1500)>\n\
      --txpattern <tx data pattern, 0: all zeros; 1: all ones; 2: repeating 10; 3: PN7; 4: PN9; 5: PN15\n\
      --ani (Enable ANI. The ANI is disabled if this option is not specified)\n\
      --scrambleroff (Disable scrambler. The scrambler is enabled by default)\n\
      --aifsn <AIFS slots num,[0-255](Used only under '--tx frame' mode)>\n\
--rx <promis/filter/report> --rxfreq <Rx channel or freq(default 2412)> --rxantenna <1/2/0 (auto)>\n\
--pm <wakeup/sleep>\n\
--setmac <mac addr like 00:03:7f:be:ef:11>\n\
--SetAntSwitchTable <table1 in decimal value> <table2 in decimal value>  (Set table1=0 and table2=0 will restore the default AntSwitchTable)\n\
";

#define INVALID_FREQ    0

#define A_RATE_NUM      8
#define G_RATE_NUM      12

#define RATE_STR_LEN    7
typedef const char RATE_STR[RATE_STR_LEN];

const RATE_STR  bgRateStrTbl[G_RATE_NUM] = {
    { "1   Mb" },
    { "2   Mb" },
    { "5.5 Mb" },
    { "11  Mb" },
    { "6   Mb" },
    { "9   Mb" },
    { "12  Mb" },
    { "18  Mb" },
    { "24  Mb" },
    { "36  Mb" },
    { "48  Mb" },
    { "54  Mb" }
};

const RATE_STR  aRateStrTbl[A_RATE_NUM] = {
    { "6   Mb" },
    { "9   Mb" },
    { "12  Mb" },
    { "18  Mb" },
    { "24  Mb" },
    { "36  Mb" },
    { "48  Mb" },
    { "54  Mb" }
};

static A_BOOL needRxReport = FALSE;

static void rxReport(void *buf);
static A_UINT32 freqValid(A_UINT32 val);
static A_UINT16 wmic_ieee2freq(A_UINT32 chan);
static void prtRateTbl(A_UINT32 freq);
static A_UINT32 rateValid(A_UINT32 val, A_UINT32 freq);
static A_UINT32 antValid(A_UINT32 val);
static A_UINT32 txPwrValid(TCMD_CONT_TX *txCmd);
static A_STATUS wmic_ether_aton(const char *orig, A_UINT8 *eth);
static A_UINT32 pktSzValid(A_UINT32 val);

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    prtRateTbl(INVALID_FREQ);
    exit(-1);
}

int
main (int argc, char **argv)
{
    int c, s;
    char ifname[IFNAMSIZ];
    unsigned int cmd = 0;
    progname = argv[0];
    struct ifreq ifr;
    char buf[256];
    TCMD_CONT_TX *txCmd = (TCMD_CONT_TX *)((A_UINT32 *)buf + 1); /* first 32-bit is XIOCTL_CMD */
    TCMD_CONT_RX *rxCmd   = (TCMD_CONT_RX *)((A_UINT32 *)buf + 1);
    TCMD_PM *pmCmd = (TCMD_PM *)((A_UINT32 *)buf + 1);

    if (argc == 1) {
        usage();
    }

    memset(buf, 0, sizeof(buf));
    memset(ifname, '\0', IFNAMSIZ);
    strcpy(ifname, "eth1");
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        err(1, "socket");
    }

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"version", 0, NULL, 'v'},
            {"interface", 1, NULL, 'i'},
            {"tx", 1, NULL, 't'},
            {"txfreq", 1, NULL, 'f'},
            {"txrate", 1, NULL, 'g'},
            {"txpwr", 1, NULL, 'h'},
            {"txantenna", 1, NULL, 'j'},
            {"txpktsz", 1, NULL, 'z'},
            {"txpattern", 1, NULL, 'e'},
            {"rx", 1, NULL, 'r'},
            {"rxfreq", 1, NULL, 'p'},
            {"rxantenna", 1, NULL, 'q'},
            {"pm", 1, NULL, 'x'},
            {"setmac", 1, NULL, 's'},
            {"ani", 0, NULL, 'a'},
            {"scrambleroff", 0, NULL, 'o'},
            {"aifsn", 1, NULL, 'u'},
            {"SetAntSwitchTable", 1, NULL, 'S'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "vi:t:f:g:r:p:q:x:u:ao",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'i':
            memset(ifname, '\0', 8);
            strcpy(ifname, optarg);
            break;
        case 't':
            cmd = TESTMODE_CONT_TX;
			txCmd->testCmdId = TCMD_CONT_TX_ID;
            if (!strcmp(optarg, "sine")) {
                txCmd->mode = TCMD_CONT_TX_SINE;
            } else if (!strcmp(optarg, "frame")) {
                txCmd->mode = TCMD_CONT_TX_FRAME;
            } else if (!strcmp(optarg, "tx99")) {
                txCmd->mode = TCMD_CONT_TX_TX99;
            } else if (!strcmp(optarg, "tx100")) {
                txCmd->mode = TCMD_CONT_TX_TX100;
            } else if (!strcmp(optarg, "off")) {
                txCmd->mode = TCMD_CONT_TX_OFF;
            }else {
                cmd = 0;
            }
            break;
        case 'f':
            txCmd->freq = freqValid(atoi(optarg));
            break;
        case 'g':
            /* let user input index of rateTable instead of string parse */
            txCmd->dataRate = rateValid(atoi(optarg), txCmd->freq);
            break;
        case 'h':
            txCmd->txPwr = atoi(optarg);
            break;
        case 'j':
            txCmd->antenna = antValid(atoi(optarg));
            break;       
        case 'z':
            txCmd->pktSz = pktSzValid(atoi(optarg));
            break;
        case 'e':
            txCmd->txPattern = atoi(optarg);
            break;
        case 'r':
            cmd = TESTMODE_CONT_RX;
			rxCmd->testCmdId = TCMD_CONT_RX_ID;
            if (!strcmp(optarg, "promis")) {
                rxCmd->act = TCMD_CONT_RX_PROMIS;
			 	printf(" Its cont Rx promis mode \n");
            } else if (!strcmp(optarg, "filter")) {
                rxCmd->act = TCMD_CONT_RX_FILTER;
				printf(" Its cont Rx  filter  mode \n");
            } else if (!strcmp(optarg, "report")) {
				 printf(" Its cont Rx report  mode \n");
                rxCmd->act = TCMD_CONT_RX_REPORT;
                needRxReport = TRUE;
            } else {
                cmd = 0;
            }
            break;
        case 'p':
            rxCmd->u.para.freq = freqValid(atoi(optarg));
            break;
        case 'q':
            rxCmd->u.para.antenna = antValid(atoi(optarg));
            break;
        case 'x':
            cmd = TESTMODE_PM;
			pmCmd->testCmdId = TCMD_PM_ID;
            if (!strcmp(optarg, "wakeup")) {
                pmCmd->mode = TCMD_PM_WAKEUP;
            } else if (!strcmp(optarg, "sleep")) {
                pmCmd->mode = TCMD_PM_SLEEP;
            } else {
                cmd = 0;
            }
            break;
        case 's':
            {
                A_UINT8 mac[ATH_MAC_LEN];

                cmd = TESTMODE_CONT_RX;
                rxCmd->testCmdId = TCMD_CONT_RX_ID;
                rxCmd->act = TCMD_CONT_RX_SETMAC;
                if (wmic_ether_aton(optarg, mac) != A_OK) {
                    printf("Invalid mac address format! \n");
                    exit(-1);
                }
                memcpy(rxCmd->u.mac.addr, mac, ATH_MAC_LEN);
#ifdef TCMD_DEBUG
                printf("JLU: tcmd: setmac 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", 
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
                break;
            }
        case 'u':
            {
                txCmd->aifsn = atoi(optarg) & 0xff;
                printf("AIFS:%d\n", txCmd->aifsn);
            }
            break;
        case 'a':
            if(cmd == TESTMODE_CONT_TX) {
                txCmd->enANI = TRUE;
            } else if(cmd == TESTMODE_CONT_RX) {
                rxCmd->enANI = TRUE;
            }
            break;
        case 'o':
            txCmd->scramblerOff = TRUE;
            break;
        case 'S':
            if (argc < 4)
                usage();
            cmd = TESTMODE_CONT_RX;
            rxCmd->testCmdId = TCMD_CONT_RX_ID;		
            rxCmd->act = TCMD_CONT_RX_SET_ANT_SWITCH_TABLE;				
            rxCmd->u.antswitchtable.antswitch1 = (unsigned int) atoi(argv[2]);
            rxCmd->u.antswitchtable.antswitch2 = (unsigned int) atoi(argv[3]);
            break;
        default:
            usage();
        }
    }

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    switch (cmd) {
    case TESTMODE_CONT_TX:
        *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CONT_TX;

        txPwrValid(txCmd);

        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            err(1, ifr.ifr_name);
        }
        break;
    case TESTMODE_CONT_RX:
        *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_CONT_RX;

        if (rxCmd->act == TCMD_CONT_RX_PROMIS ||
             rxCmd->act == TCMD_CONT_RX_FILTER) {
            if (rxCmd->u.para.freq == 0)
                rxCmd->u.para.freq = 2412;
        }

        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            err(1, ifr.ifr_name);
        }
        if (needRxReport) {
            rxReport(ifr.ifr_data);
            needRxReport = FALSE;
        }
        break;
    case TESTMODE_PM:
        *(A_UINT32 *)buf = AR6000_XIOCTL_TCMD_PM;
        ifr.ifr_data = (void *)buf;
        if (ioctl(s, AR6000_IOCTL_EXTENDED, &ifr) < 0) {
            err(1, ifr.ifr_name);
        }
        break;

    default:
        usage();
    }

    exit (0);
}

static void
rxReport(void *buf)
{
    A_UINT32 pkt;
    A_INT32  rssi;

    pkt = *(A_UINT32 *)buf;
    rssi = *((A_INT32 *)buf + 1);

    printf("total pkt %d ; average rssi %d \n", pkt,
          (A_INT32)( pkt ? (rssi / (A_INT32)pkt) : 0));
}

static A_UINT32
freqValid(A_UINT32 val)
{
    do {
        if (val <= A_CHAN_MAX) {
            A_UINT16 freq;

            if (val < BG_CHAN_MIN)
                break;

            freq = wmic_ieee2freq(val);
            if (INVALID_FREQ == freq)
                break;
            else
                return freq;
        }

        if ((val == BG_FREQ_MAX) || 
            ((val < BG_FREQ_MAX) && (val >= BG_FREQ_MIN) && !((val - BG_FREQ_MIN) % 5)))
            return val;
        else if ((val >= A_FREQ_MIN) && (val < A_20MHZ_BAND_FREQ_MAX) && !((val - A_FREQ_MIN) % 20))
            return val;
        else if ((val >= A_20MHZ_BAND_FREQ_MAX) && (val <= A_FREQ_MAX) && !((val - A_20MHZ_BAND_FREQ_MAX) % 5))
            return val;
    } while (FALSE);

    printf("Invalid channel or freq #: %d !\n", val);
    exit(-1);
}

static A_UINT32 rateValid(A_UINT32 val, A_UINT32 freq)
{
    if (((freq >= A_FREQ_MIN) && (freq <= A_FREQ_MAX) && (val >= A_RATE_NUM)) ||
        ((freq >= BG_FREQ_MIN) && (freq <= BG_FREQ_MAX) && (val >= G_RATE_NUM))) {
        printf("Invalid rate value %d for frequency %d! \n", val, freq);
        prtRateTbl(freq);
        exit(-1);
    }

    return val;
}

static void prtRateTbl(A_UINT32 freq)
{
    int i;

    if ((INVALID_FREQ == freq) || ((freq >= BG_FREQ_MIN) && (freq <= BG_FREQ_MAX))) {
        if (INVALID_FREQ == freq)
            printf("Please choose <rate> as below table shows for 11b/g mode:\n");
        else
            printf("Please choose <rate> as below table shows for frequency %d (11b/g mode):\n", freq);

        for (i = 0; i < G_RATE_NUM; i++) {
            printf("<rate> %d \t \t %s \n", i, bgRateStrTbl[i]);
        }
        printf("\n");
    }
    
    if ((INVALID_FREQ == freq) || ((freq >= A_FREQ_MIN) && (freq <= A_FREQ_MAX))) {
        if (INVALID_FREQ == freq)
            printf("Please choose <rate> as below table shows for 11a mode:\n");
        else
            printf("Please choose <rate> as below table shows for frequency %d (11a mode):\n", freq);
        
        for (i = 0; i < A_RATE_NUM; i++) {
            printf("<rate> %d \t \t %s \n", i, aRateStrTbl[i]);
        }
    }
}

/*
 * converts ieee channel number to frequency
 */
static A_UINT16
wmic_ieee2freq(A_UINT32 chan)
{
    if (chan == BG_CHAN_MAX) {
        return BG_FREQ_MAX;
    }
    if (chan < BG_CHAN_MAX) {    /* 0-13 */
        return (BG_CHAN0_FREQ + (chan*5));
    }
    if (chan <= A_CHAN_MAX) {
        return (A_CHAN0_FREQ + (chan*5));
    }
    else {
        return INVALID_FREQ;
    }
}

static A_UINT32 antValid(A_UINT32 val)
{
    if (val > 2) {
        printf("Invalid antenna setting! <0: auto;  1/2: ant 1/2>\n");
        exit(-1);
    }

    return val;
}

static A_UINT32 txPwrValid(TCMD_CONT_TX *txCmd)
{
    if (txCmd->mode == TCMD_CONT_TX_SINE) {
        if ((txCmd->txPwr >= -15) && (txCmd->txPwr <= 11))
            return txCmd->txPwr;
    } else if (txCmd->mode != TCMD_CONT_TX_OFF) {
        if ((txCmd->txPwr >= -15) && (txCmd->txPwr <= 14))
            return txCmd->txPwr;
    } else if (txCmd->mode == TCMD_CONT_TX_OFF) {
        return 0;
    }

    printf("Invalid Tx Power value! \nTx data: [-15 - 14]dBm  \nTx sine: [-15 - 11]dBm  \n");
    exit(1);

}
static A_UINT32 pktSzValid(A_UINT32 val)
{
    if (( val < 32 )||(val > 1500)){
        printf("Invalid package size! < 32 - 1500 >\n");
        exit(-1);
    }
    return val;
}
#ifdef NOTYET

// Validate a hex character
static A_BOOL
_is_hex(char c)
{
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'A') && (c <= 'F')) ||
            ((c >= 'a') && (c <= 'f')));
}

// Convert a single hex nibble
static int
_from_hex(char c) 
{
    int ret = 0;

    if ((c >= '0') && (c <= '9')) {
        ret = (c - '0');
    } else if ((c >= 'a') && (c <= 'f')) {
        ret = (c - 'a' + 0x0a);
    } else if ((c >= 'A') && (c <= 'F')) {
        ret = (c - 'A' + 0x0A);
    }
    return ret;
}

// Convert a character to lower case
static char
_tolower(char c)
{
    if ((c >= 'A') && (c <= 'Z')) {
        c = (c - 'A') + 'a';
    }
    return c;
}

// Validate alpha
static A_BOOL
isalpha(int c)
{
    return (((c >= 'a') && (c <= 'z')) || 
            ((c >= 'A') && (c <= 'Z')));
}
#endif

// Validate digit
static A_BOOL
isdigit(int c)
{
    return ((c >= '0') && (c <= '9'));
}

#ifdef NOTYET

// Validate alphanum
static A_BOOL
isalnum(int c)
{
    return (isalpha(c) || isdigit(c));
}
#endif

/*------------------------------------------------------------------*/
/*
 * Input an Ethernet address and convert to binary.
 */
static A_STATUS
wmic_ether_aton(const char *orig, A_UINT8 *eth)
{
  const char *bufp;
  int i;

  i = 0;
  for(bufp = orig; *bufp != '\0'; ++bufp) {
	unsigned int val;
	unsigned char c = *bufp++;
	if (isdigit(c)) val = c - '0';
	else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
	else break;

	val <<= 4;
	c = *bufp++;
	if (isdigit(c)) val |= c - '0';
	else if (c >= 'a' && c <= 'f') val |= c - 'a' + 10;
	else if (c >= 'A' && c <= 'F') val |= c - 'A' + 10;
	else break;

	eth[i] = (unsigned char) (val & 0377);
	if(++i == ATH_MAC_LEN) {
		/* That's it.  Any trailing junk? */
		if (*bufp != '\0') {
#ifdef DEBUG
			fprintf(stderr, "iw_ether_aton(%s): trailing junk!\n", orig);
			return(A_EINVAL);
#endif
		}
		return(A_OK);
	}
	if (*bufp != ':')
		break;
  }

  return(A_EINVAL);
}

