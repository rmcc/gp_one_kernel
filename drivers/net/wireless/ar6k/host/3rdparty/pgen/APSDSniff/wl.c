//////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2005  Andy Patti, Kevin Yu, Greg Chesson,
//    Atheros Communications
//
// License is granted to Wi-Fi Alliance members and designated
// contractors for exclusive use in testing of Wi-Fi equipment.
// This license is not transferable and does not extend to non-Wi-Fi
// applications.
//
// Derivative works are authorized and are limited by the
// same restrictions.
//
// Derivatives in binary form must reproduce the
// above copyright notice, the name of the authors "Andy Patti", "Kevin Yu"
// and "Greg Chesson",
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// The name of the authors may not be used to endorse or promote
// products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////
#include <string.h>
#include <pcap.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
// bpf.h or pcap-bpf.f
//#include <net/bpf.h>
#include <pcap-bpf.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>

#include "prism.h"
#include "wl.h"
#include "zlib.h"
#include "wlcommon.h"
#include "timing.h"

char *frameType[64] = {
    "Assoc Req",  //0
    "Reserved",
    "Data",
    "Reserved",
    "Assoc Resp",
    "Reserved",
    "Data+CF-Ack",
    "Reserved",
    "Reassoc Req",
    "Reserved",
    "Data+CF-Poll",  // 10
    "Reserved",
    "Reassoc Resp",
    "Reserved",
    "Data+CF-Ack+CF-Poll",
    "Reserved",
    "Probe Req",
    "Reserved",
    "Null (no Data)",
    "Reserved",
    "Probe Resp",  // 20
    "Reserved",
    "CF-Ack (no data)",
    "Reserved",
    "Reserved",
    "Reserved",
    "CF-Poll (no data)",
    "Reserved",
    "Reserved",
    "Reserved",
    "CF-ACK+CF-Poll (no data)",  // 30
    "Reserved",
    "Beacon",
    "Block Ack Req",
    "QoS Data",
    "Reserved",
    "ATIM",
    "Block Ack",
    "QoS Data+CF-Ack",
    "Reserved",
    "Disassoc",   // 40
    "PS Poll",    
    "QoS Data+CF-Poll",
    "Reserved",
    "Auth",
    "RTS",
    "QoS Data+CF-Ack+CF-Poll",
    "Reserved",
    "Deauth",
    "CTS",
    "QoS Null",    // 50
    "Reserved",
    "Action",
    "Ack",
    "QoS Null+CF-Ack",
    "Reserved",
    "Reserved",
    "CF-End",
    "Qos Null+CF-Poll",
    "Reserved",
    "Reserved",   //60
    "CF-End+CF-Ack",  
    "QoS NULL+CF-Ack+CF-Poll",   
    "Reserved"
};

struct packet_filter g_filter_spec;

char dump;
u_char init_done = 0;

u_long pktcnt = 0;
u_long donePacketCnt = 0;

int nwanted;                    // num packets to capture
int nSeconds = 0;               // time duration of capturing
int timing=0;                   // print timing info
int dlt;                        // datalink type from libpcap
int pTypeWanted=-1;             // Type of packet to filter on
int countFiltered=0;            // Turn on/off count of filtered packets
int countQoS=0;

long long total_payload;
long long total_load;
unsigned long long numRetries=0;  // Retry counter
unsigned long packetCount=0;
unsigned long numFiltered=0;
unsigned long qosCount=0;
unsigned long filterWDS=0;

key_t shm_key = 0x08888888;

struct cap_info *shmptr = NULL; 
struct cap_info *shm_hdptr = NULL;
struct cap_info *shm_tailptr = NULL;
struct cap_info *shm_nxtptr = NULL;
struct cap_info *shm_doneptr = NULL;
struct cap_info *shm_currptr = NULL;

// histogram structure
#define NHBUCKETS	10	// max num buckets in hist struct
struct histogram {
	long nsamples;
	int nbuckets;
	int *tabledef;
	int *table;
	long buckets[NHBUCKETS];
};

struct histogram lengths;	// all packet sizes
struct histogram smaller;	// ones reduced by compression
struct histogram bigger;	// ones increased by compression
struct histogram percents;	// reduction percentage

char ebuf[PCAP_ERRBUF_SIZE];	// err buf for pcap
pcap_t *P;			// pcap handle

// lookup table, indexes payload size into bucket table
#define	HIST_LEN_TABLE_SIZE	(2048)
int hist_len_table[HIST_LEN_TABLE_SIZE];
int hist_len_table_def[] = {40, 60, 100, 128, 256, 512, 1024, 1500, 2048, 0};

// a little ether header stuff
#define ETHER_HDRLEN	14
#define ETHERMTU	1500
struct ether_header {
	u_char	ether_dst[6];
	u_char	ether_src[6];
	u_short	ether_type;
};
char zbuf[ETHERMTU+100];	// compression dest buffer

int
phex(u_char *s, int len)
{
    u_char c0, c1;

	while (len) {
		c0 = *s++; len--;
		printf("%x:", c0);
	}
}


void
pr(u_char *s, int len)
{
    unsigned long long val;

	while (len--) {
		printf("%u ", *s++);
	}
	printf("\n");
}

void
pr_ether_header(struct ether_header *p)
{

	//printf("%Lx:%Lx:%d\n", src, dst, type);
	phex((u_char *)&p->ether_dst, 6);
	printf("  ");
	phex((u_char *)&p->ether_src, 6);
	printf("\n");
}

unsigned long long parse_fc_field(u_char **packetPtr,
                                  struct packet_info *pInfo,
                                  int *pLen)
{
    u_short val;
    u_char *ptr;
    unsigned long long error=0;

    ptr = *packetPtr;
    if (*pLen > 1) {
        *pLen -= 2;
        val = (u_short) *ptr++;
        pInfo->fc.rawData = val;
        val |= ((u_short) *ptr++) << 8;
        pInfo->fc.rawData = val;
        pInfo->fc.protoVers = val & (0x0003);
        pInfo->fc.type = (val & (0x000C)) >> 2;
        pInfo->fc.subtype = (val & (0x00F0)) >> 4;
        pInfo->fc.typeIndex = (val & (0xFC)) >> 2;
        pInfo->fc.toDS = (val >> 8) & (0x0001);
        pInfo->fc.fromDS = (val >> 9) & (0x0001);
        pInfo->fc.moreFrag = (val >> 10) & (0x0001);
        pInfo->fc.retry = (val >> 11) & (0x0001);
        pInfo->fc.pwrMgt = (val >> 12) & (0x0001);
        pInfo->fc.moreData = (val >> 13) & (0x0001);
        pInfo->fc.WEP = (val >> 14) & (0x0001);
        pInfo->fc.order = (val >> 15) & (0x0001);
        if ((pInfo->fc.type != 2) &&
            (pInfo->fc.fromDS == 1)) {
            error |= DSBITS_FCERR;
        }
        if ((pInfo->fc.type != 2) &&
            (pInfo->fc.order == 1)) {
            error |= ORDER_FCERR;
        }
    } else {
        error = PARSE_FCERR;
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long parse_duration_field(u_char **packetPtr,
                                        struct packet_info *pInfo,
                                        int *pLen)
{
    unsigned long long error=0;
    u_short val;
    u_char *ptr;

    ptr = *packetPtr;
    if (*pLen > 0) {
        val = (u_short) *ptr++;
        *pLen -= 1;
        pInfo->duration.rawData=val;
    } else {
        error = PARSE_DURERR;
    }
    if (*pLen > 0) {
        val |= ((u_short) *ptr++) << 8;
        *pLen -= 1;
        pInfo->duration.rawData = val;
        if (pInfo->fc.typeIndex == 41) {
            pInfo->duration.AID = val & (0x3FFF);
            if ((val & 0xC000) != 0xC000) {
                error = PARSE_DURERR;
            }
            if ((pInfo->duration.AID > 2007) ||
                (pInfo->duration.AID == 0)) {
                error = PARSE_DURERR;
            }
        } else {
            if ((val & 0x8000) == 0) {
                pInfo->duration.duration = val & 0x7FFF;
            } else {
                if ((val & 0x4000) == 0x4000) {
                    error = PARSE_DURERR;
                } else {
                    if (val & 0x2000) {
                        error = PARSE_DURERR;
                    }
                }
            }
        }
    } else {
        error = PARSE_DURERR;
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long parse_fcs_field (u_char *ptr,
                                    struct packet_info *pInfo)
{
    int i,shift;

    pInfo->fcs = 0;
    for (i=0,shift=0;i<4; i++,shift += 8) {
        pInfo->fcs |= ((unsigned long) *ptr++) << shift;
    }
}
    
unsigned long long get_sequence_control(u_char **packetPtr,
                                        struct packet_info *pInfo,
                                        int *pLen)
{
    u_char *ptr;
    u_short val;
    unsigned long long error=0;

    ptr = *packetPtr;
    if (*pLen > 0) {
        val = (u_short) *ptr++;
        *pLen -= 1;
        pInfo->seqCtrl.rawData = val;
    } else {
        error = SEQ_ERR;
    }
    if (*pLen > 0) {
        val |= ((u_short) *ptr++) << 8;
        *pLen -= 1;
        pInfo->seqCtrl.rawData = val;
        pInfo->seqCtrl.fragNum = val & (0x000F);
        pInfo->seqCtrl.seqNum = (val & (0xFFF0)) >> 4;
    } else {
        error = SEQ_ERR;
    }
    *packetPtr = ptr;
    return(error);
}


unsigned long long parse_qos_control(u_char **packetPtr, 
                                     struct packet_info *pInfo,
                                     int *pLen)
{

    unsigned long long error=0;
    u_char *tptr=*packetPtr;
    
    memset(&(pInfo->qosControl),0,sizeof(struct qos_control_field));
    get_u_short(packetPtr,&(pInfo->qosControl.rawData),pLen);
    pInfo->qosControl.ackPolicy = (pInfo->qosControl.rawData >> 5) & (0x0003);
    pInfo->qosControl.eosp = (pInfo->qosControl.rawData >> 4) & (0x0001);
    pInfo->qosControl.up = (pInfo->qosControl.rawData) & (0x0007);
    return(error);
}
    
	

unsigned long long parse_mac_header(u_char **packetPtr,
                                    struct packet_info *pInfo, 
                                    int *pLen)
{
    unsigned long long error=0;

    error = parse_fc_field(packetPtr, pInfo, pLen);
    error |= parse_duration_field(packetPtr, pInfo, pLen);
    error |= get_hw_address(packetPtr, &(pInfo->addr1[0]), pLen);
    if ( (pInfo->fc.type == 1) && ( (pInfo->fc.subtype == 0x1100) ||  (pInfo->fc.subtype == 0x1101))  ) { // Control Frame (CTS/ACK)  // palm
        /* quick hack to allow control frame print */
        return error;
    }
    error |= get_hw_address(packetPtr, &(pInfo->addr2[0]), pLen); // Control Frame (PS poll / RTS / CF-end) // palm

    if ( (pInfo->fc.type == 1)   ) {
        /* quick hack to allow control frame print */
        return error;
    }
    error |= get_hw_address(packetPtr, &(pInfo->addr3[0]), pLen);
    error |= get_sequence_control(packetPtr, pInfo, pLen);
    if ((pInfo->fc.toDS == 1) && (pInfo->fc.fromDS == 1)) {
        error |= get_hw_address(packetPtr, &(pInfo->addr4[0]), pLen);
    }
    if ((pInfo->fc.type == 2) &&
        ((pInfo->fc.subtype &(0x08)) != 0)) {
        // qos data type
        error |= parse_qos_control(packetPtr, pInfo, pLen);
    }
    return(error);
}

void print_mac_header(struct packet_info *pInfo)
{
    printf ("raw: (0x%04x) Vers: (%d) Type: (%s) fromDS: (%d) toDS: (%d)\n",
            pInfo->fc.rawData, pInfo->fc.protoVers, 
            frameType[pInfo->fc.typeIndex], pInfo->fc.fromDS, pInfo->fc.toDS);
    printf ("moreFrag: (%d) retry: (%d) pwrMgt: (%d) moreData: (%d) WEP: (%d) order: (%d)\n",
            pInfo->fc.moreFrag, pInfo->fc.retry, pInfo->fc.pwrMgt,
            pInfo->fc.moreData, pInfo->fc.WEP, pInfo->fc.order);
    if (pInfo->fc.typeIndex == 41) {
        printf ("AID: (%d) ",pInfo->duration.AID);
    } else {
        printf ("Dur: (%d) ",pInfo->duration.duration);
    }
    if ( (pInfo->fc.type == 1) && ( (pInfo->fc.subtype == 0x1100) ||  (pInfo->fc.subtype == 0x1101))  ) { // Control Frame (CTS/ACK)  // palm
        /* quick hack to allow control frame print */
        printf("\n");
        return;
    }

    if ( (pInfo->fc.type == 1)   ) {  // Control Frame (PS poll / RTS / CF-end) // palm
        /* quick hack to allow control frame print */
        printf("\n");
        return;
    }
    printf ("Seq Num: (0x%x) Frag Num: (0x%x)\n",
            pInfo->seqCtrl.seqNum, pInfo->seqCtrl.fragNum);
    if ((pInfo->fc.type == 2) &&
        ((pInfo->fc.subtype &(0x08)) != 0)) {
        // qos data type
        printf ("Qos AckPolicy: (%d) EOSP: (%d) UP: (%d)\n",
                pInfo->qosControl.ackPolicy,
                pInfo->qosControl.eosp,
                pInfo->qosControl.up);
    }
}

void print_mac_header_error(unsigned long error)
{
    if (error & DSBITS_FCERR) {
        printf ("ERROR in FC: DS bits\n");
    }
    if (error & AID_FCERR) {
        printf ("ERROR in FC: PS-POLL AID value\n");
    }
    if (error & ORDER_FCERR) {
        printf ("ERROR in FC: order bit setting\n");
    }
    if (error & PARSE_FCERR) {
        printf ("ERROR in FC: parse error\n");
    }
    if (error & PARSE_DURERR) {
        printf ("ERROR parsing duration field\n");
    }
    if (error & ADDR_ERR) {
        printf ("ERROR parsing address fields\n");
    }
    if (error & SEQ_ERR) {
        printf ("Error parsing sequence control field\n");
    }
}

void print_packet_info(struct packet_info *pInfo)
{
    switch(pInfo->fc.type) {
    case 0: // Management frame
        print_management_frame(pInfo);
        break;
    case 1:   // palm
        print_control_frame(pInfo);
        break;
    case 2:
        print_data_frame(pInfo);
        break;
    }
}

void
pr_wlan_header(struct wlan_header *p,int phLen, int totalLen, u_int32_t tsfTick)
{
    int pLen,diff,reserve,okToPrint=1,val=0;
    struct packet_info pInfo;
    unsigned long long error=0;
    u_char *ptr=0;

    memset(&pInfo,0,sizeof(struct packet_info));

    if (phLen < totalLen) {
        pInfo.partialCap = 1;
    }
    reserve = 4 - (totalLen-phLen);
    if (reserve < 0)
        reserve=0;
    pLen = phLen - reserve;
    ptr = (u_char *) p;
    if (pLen < 0) {
        return;
    }

    error = parse_mac_header(&ptr,&pInfo,&pLen);

    if ((filterWDS) &&
        ((pInfo.fc.toDS != 1) || (pInfo.fc.fromDS != 1)))
	    return;
    if (g_filter_spec.staFilter) {
        okToPrint = 0;
        if ((memcmp(&g_filter_spec.staAddr[0],&(pInfo.addr1[0]),6) == 0) ||
            (memcmp(&g_filter_spec.staAddr[0],&(pInfo.addr2[0]),6) == 0)) {
            okToPrint = 1;   
        }
    }
    else {
        val = (g_filter_spec.sourceFilter << 1)|(g_filter_spec.destFilter);
        switch (val) {
        case 1:
            if (memcmp(&g_filter_spec.destAddr[0],&(pInfo.addr1[0]),6) != 0) {
                okToPrint = 0;
            }
            break;
        case 2:
            if (memcmp(&g_filter_spec.sourceAddr[0],&(pInfo.addr2[0]),6) != 0) {
                okToPrint = 0;
            }
            break;
        case 3:
            if ((memcmp(&g_filter_spec.destAddr[0],&(pInfo.addr1[0]),6) != 0) &&
                (memcmp(&g_filter_spec.sourceAddr[0],&(pInfo.addr2[0]),6) != 0)) {
                okToPrint = 0;
            }
            break;
        default:
            okToPrint = 1;
        }
    }
    
    if ((pTypeWanted > -1) &&
        (pInfo.fc.typeIndex != pTypeWanted)) {
        okToPrint = 0;
    }
	    
    if ((error == 0) && (okToPrint)) {
        if (pInfo.fc.retry != 0) {
            numRetries++;
        }
        error = 0;
        switch (pInfo.fc.type) {
        case 0 : // Management frame
            // filtering done in parser
            if (parse_management_frame(&ptr,&pInfo,&pLen))
                return;
            break;
        case 1: // Control frame
            if (!g_filter_spec.printCtlFrms) {
                return;
            }
            break;
        case 2: // Data frame
            if (parse_data_frame(&ptr,&pInfo,&pLen))
                return;
            break;
        case 3 :
            break;
        default: // ERROR - this shouldn't happen
            printf ("ERROR parsing field control type (%d)\n",
                    pInfo.fc.type);
        }
        if (pLen > 0) {
            printf ("ERROR - packet length exceeded parsing by (%d) bytes.  Total Packet len: (%d)\n",pLen,totalLen);
        }
        if (!(pInfo.partialCap)) {
            parse_fcs_field ((((u_char *) p)+totalLen-4),&pInfo);
            pInfo.crc = crc32(0L,Z_NULL,0);
            pInfo.crc = crc32(pInfo.crc,(u_char *) p,totalLen-reserve);
            printf ("fcs: (0x%x) Count: (%u) tsfTick: (%u) ",pInfo.fcs, g_filter_spec.totalPrinted++, tsfTick);
            if (pInfo.fcs != pInfo.crc) {
                printf ("CRC ERROR - crc:(0x%x)\n",
                        pInfo.crc);
            } else {
                printf ("\n");
            }
        }
    }
    pInfo.error = error;
    if (okToPrint) {
        if (countFiltered) {
            numFiltered++;
        } 
        print_mac_header(&pInfo);
        fflush(stdout);
        print_mac_header_error(error);
        print_packet_info(&pInfo);
        if (dump) {
            pr_ether_data((u_char *)p, phLen);
        }
        printf ("\n");
    }
}

void
pr_prism_header(wlan_ng_prism2_header *ph, int capLen, int totalLen)
{
    p80211item_uint32_t *ip;
    struct wlan_header *wh;
    int framelen;

//	printf("msgcode(%x)\n", ph->msgcode);
//	printf("frm %x\n", DIDmsg_lnxind_wlansniffrm);
	ip = &ph->frmlen;
//	printf("did(%x) len(%d) status(%x) data(%x)\n", ip->did, ip->len, ip->status, ip->data);
	if (timing) {
	    printf ("hosttime: (%u)  mactime: (%u)\n",
                ph->hosttime.data, ph->mactime.data);
	}   
	wh = (struct wlan_header *)&ph[1];
	pr_wlan_header(wh, capLen-sizeof(wlan_ng_prism2_header),
                   totalLen-sizeof(wlan_ng_prism2_header), 
                   (u_int32_t)ph->mactime.data);
}

__inline__
hist_incr(struct histogram *h, int n)
{
    int bucket;
    
    h->nsamples++;
    bucket = h->table[n];
    h->buckets[bucket]++;
}


unsigned long diffTime(unsigned long ftime, unsigned long stime)
{
    unsigned long long finishTime=ftime;

    if (stime > ftime) {
        finishTime = (PRE_WRAP_VALUE + 1) + ((unsigned long long) ftime);
    }
    return((unsigned long) (finishTime-(unsigned long long) stime));
}

#ifdef TIMING
void pr_timing (wlan_ng_prism2_header *pheader)
{
    float avTsf, avCycles, avJiffie;
    float varTsf, varCycles, varJiffie;
    float count;
    unsigned long diffTsf, diffCycles, diffJiffie;
    struct ath_timing *timeStats;

    timeStats = &(pheader->timing);
    count = (float) timeStats->count;
    avTsf = ((float) timeStats->diffSumTSF)/count;
    avCycles = ((double) timeStats->diffSumCycles)/((double)count);
    avJiffie = ((float) timeStats->diffSumJiffie)/count;
    varTsf = (((float) timeStats->diffSumTSF2)/count) - avTsf*avTsf;
    varCycles = (((float) timeStats->diffSumCycles2)/count) -
        avCycles* avCycles;
    varJiffie = (((float) timeStats->diffSumJiffie2)/count) - 
        avJiffie*avJiffie;
    
    diffTsf = diffTime(timeStats->lastTSF, timeStats->firstTSF);
    diffCycles = diffTime(timeStats->lastCycles, timeStats->firstCycles);
    diffJiffie = diffTime(timeStats->lastJiffie, timeStats->firstJiffie);
    
    printf ("Packet Count: (%lu)\n",timeStats->count);
    printf ("Av. Proc. time: (%.2f) Cycles  (%.2f) Jiffie  (%.2f) TSF\n",
            avCycles, avJiffie, avTsf);
    printf ("Var. of Proc. time: (%.2f) Cycles  (%.2f) Jiffie  (%.2f) TSF\n",
            varCycles, varJiffie, varTsf);
    printf ("Last packet:\n arrival time: (%lu) Cycles  (%lu) Jiffie  (%lu) TSF\n",
            timeStats->firstCycles, timeStats->firstJiffie,
            timeStats->firstTSF);
    printf ("finish time: (%lu) Cycles  (%lu) Jiffie  (%lu) TSF\n",
            timeStats->lastCycles, timeStats->lastJiffie,
            timeStats->lastTSF);
    printf ("time to process: (%lu) Cycles  (%lu) Jiffie  (%lu) TSF\n",
            diffCycles, diffJiffie, diffTsf);
}
#endif    
	
// analyze a packet here
void
analyzeframe(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
    int del;
    u_char *packetp, *snapend;
    u_char *pdata;
    u_char *pheader;
    int pdatalen;
    struct histogram *hist;
    int bucket;
    int destlen;
    struct ifreq ifReq;
#ifdef TIMING
    struct ath_timing timeStats;
#endif

    packetp = (u_char *)p;
    pheader = packetp;
    snapend = packetp + h->caplen;
    pdata = packetp + ETHER_HDRLEN;
    pdatalen = h->len - ETHER_HDRLEN;
    total_payload += pdatalen;
    total_load += h->caplen;
    
    hist = &lengths;
    hist->nsamples++;
    bucket = hist->table[pdatalen];
    hist->buckets[bucket]++;
    //hist_incr(&lengths, pdatalen);
    
    if (dlt==DLT_EN10MB) {
        pr_ether_header((struct ether_header *)pheader);
    } else
        if (dlt==DLT_PRISM_HEADER) {
#ifdef TIMING
            if (timing) {
                /* 
                 * XXX: shouldn't print here since filtering occurs later! 
                 */
                pr_timing((wlan_ng_prism2_header *) pheader);
            }
#endif
            packetCount++;
            pr_prism_header((wlan_ng_prism2_header *)pheader,
                            h->caplen,h->len);
        } else {
            fprintf(stderr, "unknown datalink type(%d)\n", dlt);
            exit(1);
        }
    fflush(stdout);
}

void
pbuckets(char *m, struct histogram *h)
{
    int b;

	printf("%s", m);
	printf("nsamples (%d)\n", h->nsamples);
	for(b=0; b<h->nbuckets; b++) {
		printf("%d\t", h->tabledef[b]);
	}
	printf("\n");
	for(b=0; b<h->nbuckets; b++) {
		printf("%d\t", h->buckets[b]);
	}
	printf("\n");
}


void
cleanup()
{
	pbuckets("packet length histogram\n", &lengths);
	printf("total packet bytes\t%Ld\n", total_load);
	printf("total payload bytes\t%Ld\n", total_payload);
	if (countFiltered) {
	    printf ("Total # of filtered packets:\t%llu\n",numFiltered);
	}
	if (countQoS) {
	    printf ("Total # of QoS Data packets:\t%llu\n",qosCount);
	}
	printf("Total # of retries:\t%llu\n", numRetries);
//	printf("Total # of wireless packets:\t%llu\n", packetCount);
	printf("Total # of wireless packets:\t%llu\n", donePacketCnt);

        if(shm_hdptr!=NULL)
        {
           shmdt(shm_hdptr);

           shm_hdptr = NULL;
        }

	exit(0);
}

void
make_table(int *table, int size, int *buckets, struct histogram *h)
{
    int b, t, prev, hi;

	for(prev=b=0; buckets[b]; b++) {
		hi = buckets[b];
		for(t=prev; t<=hi && t<size; t++) {
			table[t] = b;
		}
		prev = t;
	}
	while (t<size) {
		table[t++] = b;
	}
	h->nbuckets = b;
	h->tabledef = buckets;
	h->table = table;
#ifdef DEBUG_TABLE
	for(t=0; t<size; t++)
		printf("%d\t%d\n", t, table[t]);
	exit(0);
#endif
}

int get_filter_address(char *inAddr, unsigned char *addr)
{
    char addrCopy[40],*ptr=NULL;
    int count=0,retVal=0;
    
    ptr = &addrCopy[0];
    strncpy(ptr,inAddr,39);
    ptr[39]='\0';
    ptr = strtok(ptr,":");
    while (ptr != NULL) {
        sscanf(ptr,"%x",(addr+count++));
        ptr = strtok(NULL,":");
    }
    if (count == 6) {
        retVal = 1;
    }
    return(retVal);
}


void
dump_pcappkt(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
   u_char *pktptr = (u_char *)p;
   int del;
   u_char *packetp, *snapend;
   u_char *pdata;
   u_char *pheader;
   int pdatalen;
   struct histogram *hist;
   int bucket;
   int destlen;
   struct ifreq ifReq;

   packetp = (u_char *)p;
   pheader = packetp;
   snapend = packetp + h->caplen;
   pdata = packetp + ETHER_HDRLEN;
   pdatalen = h->len - ETHER_HDRLEN;
   total_payload += pdatalen;
   total_load += h->caplen;
    
   hist = &lengths;
   hist->nsamples++;
   bucket = hist->table[pdatalen];
   hist->buckets[bucket]++;

   if(shm_currptr == shm_doneptr && pktcnt != 0)
      printf("Buff overflow, I have to overwrite an oldest entry\n");

   if(p == NULL)
   {
      printf("got a null pcap packet\n");
      return;
   }

   shm_currptr = shm_nxtptr;
   shm_currptr->cap_len = h->caplen;
   shm_currptr->len = h->len;

   memcpy(shm_currptr->packet, pktptr, (h->caplen < SHM_MAX_PACKET_SIZE)?h->caplen:SHM_MAX_PACKET_SIZE);

   if(shm_currptr == shm_tailptr)
       shm_nxtptr = shm_hdptr;
   else
       shm_nxtptr++;

   init_done = 1;
}
	
char device[100];

void *pcap_thread (void *arg)
{
    int r;
    int snaplen;
    struct timeval time_start, time_stop;

    snaplen = SHM_MAX_PACKET_SIZE;

    P = pcap_open_live(device, snaplen, 1, 1000, ebuf);
    if (!P) {
        fprintf(stderr, "pcap open failure\n");
        if (ebuf[0])  
            fprintf(stderr, "%s\n", ebuf);
        exit(1);
    }

    dlt = pcap_datalink(P);
    
    r = pcap_snapshot(P);
    if (r != snaplen) {
        fprintf(stderr, "snaplen(%d) changed\n", snaplen);
        exit(1);
    }
    
    signal(SIGINT, &cleanup);
    
    if(nSeconds <= 0)
      r = pcap_loop(P, nwanted, dump_pcappkt, 0);
    else
      {
	gettimeofday(&time_start, 0);
	time_start.tv_sec = time_start.tv_sec + nSeconds;
	
	do{
	    r = pcap_loop(P, 10, dump_pcappkt, 0);
	    gettimeofday(&time_stop, 0);
	}while(time_stop.tv_sec < time_start.tv_sec);
       }
	    
    fprintf(stderr, "pcap_loop returned %d\n", r);
    fflush(stdout);

    if(shm_hdptr != NULL)
    {
      shmdt(shm_hdptr);
      shm_hdptr = NULL;
    }

    exit(0);

}

void *process_thread(void *arg)
{
    u_char *packetp;
    u_char *pheader;
    struct cap_info *tmppktp = NULL, *nxttmppktp = NULL;

    u_int caplen, pktlen;

    printf("Process Thread\n");

    for(;;)
    {
       if(init_done == 0)
         continue;

       if(shm_doneptr == shm_currptr )
           continue;

       tmppktp = shm_doneptr;
       if(shm_doneptr == shm_tailptr)
       {
            nxttmppktp = shm_hdptr;
       }
       else
       {
            nxttmppktp = shm_doneptr+1;
       }

             
       caplen = tmppktp->cap_len;
       pktlen = tmppktp->len;

       pheader = tmppktp->packet;

       if (dlt==DLT_EN10MB) 
       {
           pr_ether_header((struct ether_header *)pheader);
       } 
       else
       {
           if (dlt==DLT_PRISM_HEADER) 
           {
#ifdef TIMING
               if (timing) 
               {
                /* 
                 * XXX: shouldn't print here since filtering occurs later! 
                 */
                   pr_timing((wlan_ng_prism2_header *) pheader);
               }
#endif
               donePacketCnt++;
               pr_prism_header((wlan_ng_prism2_header *)pheader,
                            caplen,pktlen);
            } 
            else 
            {
                fprintf(stderr, "unknown datalink type(%d)\n", dlt);
                exit(1);
            }
            fflush(stdout);
      }
      memset(tmppktp, 0, SHM_BUCKET_SIZE);

      shm_doneptr = nxttmppktp; 
   }
}


main(int argc, char **argv)
{
    extern char *optarg;
    int cmd;
    int status;
    int shmem_size = SHM_MAX_BUCKET * sizeof(struct cap_info); 

    printf("shmem_size %d\n", shmem_size);
    printf("bucket size %i\n", SHM_BUCKET_SIZE);

    // shared memory
    struct cap_info *shmp = NULL;
    int shmid;

    // pthreads
    pthread_attr_t attr1, attr2;
    pthread_t thread1, thread2;
    struct sched_param sched1, sched2;
    int pri;
    
    nwanted = -1;
    dump = 0;
    strcpy(&device[0],"ath0");

#ifdef TIMING
    while ((cmd = getopt(argc, argv, "a:b:r:x:S:s:q:hw:dcfeym")) != EOF) {
#else
    while ((cmd = getopt(argc, argv, "a:b:r:x:S:s:q:hw:dcfey")) != EOF) {
#endif

	    switch(cmd) {
	    case 'a':
            strncpy(&device[0],optarg,100);
            break;
	    case 'x': 
            g_filter_spec.sourceFilter = get_filter_address(optarg,&(g_filter_spec.sourceAddr[0]));
            break;
	    case 'r':
            g_filter_spec.destFilter = get_filter_address(optarg,&(g_filter_spec.destAddr[0]));
            break;
	    case 'S':
            g_filter_spec.staFilter = get_filter_address(optarg,&(g_filter_spec.staAddr[0]));
            break;
	    case 'c': g_filter_spec.printCtlFrms = 1; break;
	    case 'd': dump = 1;break;
#ifdef TIMING
	    case 'm': timing=1;break;
#endif
	    case 'w': 
            nwanted = strtol(optarg, 0, 10);
            break;
	    case 't': 
            nSeconds = strtol(optarg, 0, 10);
            break;
	    case 'b': 
            g_filter_spec.beaconLimit = strtol(optarg, 0, 10);
            break;
	    case 'q':
            pTypeWanted = strtol(optarg,0,10);
            break;
	    case 'f':
            countFiltered = 1;
            numFiltered=0;
            break;
	    case 'e':
            countQoS = 1;
            qosCount = 0;
            break;
	    case 'y':
		    filterWDS=1;
		    break;
	    case 's':
            g_filter_spec.ssid=1;
            strncpy(&(g_filter_spec.filtSsid[0]) ,optarg, sizeof(g_filter_spec.filtSsid));
            break;
	    case 'h':
	    default: 
            printf("usage:\n");
            printf("\t-a dev    Use device (def=ath0)\n");
            printf("\t-r addr   Receive hw addr\n");
            printf("\t-x addr   Xmit hw addr\n");
            printf("\t-p        print mac addresses and initial payload\n");
            printf("\t-z	  disable compression\n");
            printf("\t-t	  print before:after packet sizes\n");
            printf("\t-w %%d  	  set number of packets to capture\n");
            printf("\t-t %%d  	  set duration of time to capture\n");            
	    printf("\t-d        dump raw data at end of parse\n");
#ifdef TIMING
            printf("\t-m        print timing info\n");
#endif
            printf("\t-q %%d          parse only packets of type %%d\n");
            printf("\t-f      count filtered packets\n");
            printf("\t-e      count QoS Data packets (no qos null packets are counted)\n");
            printf("\t-y	  filter WDS\n");
            printf("\t-h	  print usage\n");
            exit(0);
            break;
	    }
    }

#if 0    
    if (g_filter_spec.destFilter) {
        printf ("Filtering for dest addr: ");
        print_hw_address(&g_filter_spec.destAddr[0]);
        printf("\n\n");
    }
    if (g_filter_spec.sourceFilter) {
        printf ("\nFiltering for source addr: ");
        print_hw_address(&g_filter_spec.sourceAddr[0]);
        printf ("\n\n");
    }
    if (g_filter_spec.staFilter) {
        printf ("Filtering for STA tx/rx addr: ");
        print_hw_address(&g_filter_spec.staAddr[0]);
        printf("\n\n");
    }
#endif
    ebuf[0] = 0;
    make_table(hist_len_table, HIST_LEN_TABLE_SIZE, hist_len_table_def, &lengths);
    make_table(hist_len_table, HIST_LEN_TABLE_SIZE, hist_len_table_def, &smaller);
    make_table(hist_len_table, HIST_LEN_TABLE_SIZE, hist_len_table_def, &bigger);


    shmid = shmget(shm_key, shmem_size, 0666 | IPC_CREAT);

    if( shmid == -1)
    {
       if( ENOENT != errno)
       {
           printf("Bad Error getting shared memory\n");
           exit (errno);
       }
    }

    shmp = (struct cap_info *)shmat(shmid, NULL, 0); 

    if(shmp == NULL)
    {
         printf("Unable to attach the shm\n");
         exit(1);
    }

    memset(shmp, 0, shmem_size);
      
    shm_hdptr = (struct cap_info *)shmp;

    printf("shm_doneptr %x\n", shm_doneptr);

    shm_doneptr = shm_nxtptr = shm_hdptr;
    printf("shm_doneptr %x\n", shm_doneptr);

    shm_tailptr = shm_hdptr + (SHM_MAX_BUCKET-1); 

    printf("hdr %x %i, tail %x %ul %i\n", shm_hdptr, shm_hdptr, shm_tailptr, shm_tailptr, (shm_tailptr-shm_hdptr)/sizeof(struct cap_info));
    printf("hdr->len %d tail->len %d\n", shm_hdptr->len, shm_tailptr->len);

    pthread_attr_init(&attr1);  
    pthread_attr_init(&attr2);  

    pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_JOINABLE);

    pthread_attr_setschedpolicy(&attr1, SCHED_OTHER);
    pthread_attr_setschedpolicy(&attr2, SCHED_OTHER);

    pri = sched_get_priority_min(SCHED_OTHER);

    sched2.sched_priority = pri;
    sched1.sched_priority = pri+16;
    pthread_attr_setschedparam(&attr1, &sched1);
    pthread_attr_setschedparam(&attr2, &sched2);

    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);

    pthread_create(&thread2, &attr2, process_thread, NULL);
    pthread_create(&thread1, &attr1, pcap_thread, NULL);

    pthread_join(thread2, (void **) &status);
    pthread_join(thread1, (void **) &status);
    
}
