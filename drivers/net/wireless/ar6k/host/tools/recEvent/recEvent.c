/*
 * Copyright (c) 2006 Atheros Communications Inc.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <a_config.h>
#include <a_osapi.h>
#include <a_types.h>
#include <athdefs.h>
#include <ieee80211.h>
#include <wmi.h>
#include <athdrv_linux.h>
#include <dbglog_api.h>

#undef DEBUG
#undef DBGLOG_DEBUG

#define ID_LEN                         2
#define FILENAME_LENGTH_MAX            128
#define RESTORE_FILE                   "restore.sh"
#define DBGLOG_FILE                    "dbglog.h"
#define DBGLOGID_FILE                  "dbglog_id.h"
#define DBGLOG_OUTPUT_FILE             "dbglog.out"

#define GET_CURRENT_TIME(s) do { \
    time_t t; \
    t = time(NULL); \
    s = strtok(ctime(&t), "\n"); \
} while (0);

const char *progname;
char restorefile[FILENAME_LENGTH_MAX];
char dbglogfile[FILENAME_LENGTH_MAX];
char dbglogidfile[FILENAME_LENGTH_MAX];
char dbglogoutfile[FILENAME_LENGTH_MAX];
FILE *fpout;
int dbgRecLimit=0;

char dbglog_id_tag[DBGLOG_MODULEID_NUM_MAX][DBGLOG_DBGID_NUM_MAX][DBGLOG_DBGID_DEFINITION_LEN_MAX];

#ifdef DEBUG
int debugRecEvent = 0;
#define RECEVENT_DEBUG_PRINTF(args...)        if (debugRecEvent) printf(args);
#else
#define RECEVENT_DEBUG_PRINTF(args...)
#endif

static A_STATUS app_wmiready_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_connect_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_disconnect_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_bssInfo_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_pstream_timeout_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_reportError_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_rssi_threshold_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_scan_complete_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_challenge_resp_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_target_debug_event_rx(A_INT8 *datap, int len);

static void
event_rtm_newlink(struct nlmsghdr *h, int len);

static void
event_wireless(char *data, int len);

int
string_search(FILE *fp, char *string)
{
    char str[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    rewind(fp);
    memset(str, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
    while (!feof(fp)) {
        fscanf(fp, "%s", str);
        if (strstr(str, string)) return 1;
    }

    return 0;
}

void
get_module_name(char *string, char *dest)
{
    char *str1, *str2;
    char str[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    memset(str, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
    strcpy(str, string);
    str1 = strtok(str, "_");
    while ((str2 = strtok(NULL, "_"))) {
        str1 = str2;
    }

    strcpy(dest, str1);
}

#ifdef DBGLOG_DEBUG
void
dbglog_print_id_tags(void)
{
    int i, j;

    for (i = 0; i < DBGLOG_MODULEID_NUM_MAX; i++) {
        for (j = 0; j < DBGLOG_DBGID_NUM_MAX; j++) {
            printf("[%d][%d]: %s\n", i, j, dbglog_id_tag[i][j]);
        }
    }
}
#endif /* DBGLOG_DEBUG */

int
dbglog_generate_id_tags(void)
{
    int id1, id2;
    FILE *fp1, *fp2;
    char str1[DBGLOG_DBGID_DEFINITION_LEN_MAX];
    char str2[DBGLOG_DBGID_DEFINITION_LEN_MAX];
    char str3[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    if (!(fp1 = fopen(dbglogfile, "r"))) {
        perror(dbglogfile);
        return -1;
    }

    if (!(fp2 = fopen(dbglogidfile, "r"))) {
        perror(dbglogidfile);
        return -1;
    }

    memset(dbglog_id_tag, 0, sizeof(dbglog_id_tag));
    if (string_search(fp1, "DBGLOG_MODULEID_START")) {
        fscanf(fp1, "%s %s %d", str1, str2, &id1);
        do {
            memset(str3, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
            get_module_name(str2, str3);
            strcat(str3, "_DBGID_DEFINITION_START");
            if (string_search(fp2, str3)) {
                memset(str3, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
                get_module_name(str2, str3);
                strcat(str3, "_DBGID_DEFINITION_END");
                fscanf(fp2, "%s %s %d", str1, str2, &id2);
                while (!(strstr(str2, str3))) {
                    strcpy((char *)&dbglog_id_tag[id1][id2], str2);
                    fscanf(fp2, "%s %s %d", str1, str2, &id2);
                }
            }
            fscanf(fp1, "%s %s %d", str1, str2, &id1);
        } while (!(strstr(str2, "DBGLOG_MODULEID_END")));
    }

    fclose(fp2);
    fclose(fp1);

    return 0;
}

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s <output log file>\n", progname);
    exit(-1);
}

int main(int argc, char** argv)
{
    int s;
    struct sockaddr_nl local;
    struct sockaddr_nl from;
    socklen_t fromlen;
    struct nlmsghdr *h;
    char buf[8192];
    int left;
    char *workarea;
    char *platform, *dbgRecLimitStr;

    progname = argv[0];
    if (argc == 1) {
        usage();
    }

    if ((workarea = getenv("WORKAREA")) == NULL) {
        printf("export WORKAREA\n");
        return -1;
    }

    if ((platform = getenv("ATH_PLATFORM")) == NULL) {
        printf("export ATH_PLATFORM\n");
        return -1;
    }

    if((dbgRecLimitStr = getenv("ATH_DBG_REC_LIMIT")) == NULL) {
        printf("No debug record limit set\n");
    }

    /* Get the file name for the restore script */
    memset(restorefile, 0, FILENAME_LENGTH_MAX);
    strcpy(restorefile, workarea);
    strcat(restorefile, "/host/.output/");
    strcat(restorefile, platform);
    strcat(restorefile, "/image/");
    strcat(restorefile, RESTORE_FILE);

    /* Get the file name for dbglog header file */
    memset(dbglogfile, 0, FILENAME_LENGTH_MAX);
    strcpy(dbglogfile, workarea);
    strcat(dbglogfile, "/include/");
    strcat(dbglogfile, DBGLOG_FILE);

    /* Get the file name for dbglog id header file */
    memset(dbglogidfile, 0, FILENAME_LENGTH_MAX);
    strcpy(dbglogidfile, workarea);
    strcat(dbglogidfile, "/include/");
    strcat(dbglogidfile, DBGLOGID_FILE);

    /* Get the file name for dbglog output file */
    memset(dbglogoutfile, 0, FILENAME_LENGTH_MAX);
    strcpy(dbglogoutfile, argv[1]);
    if (!(fpout = fopen(dbglogoutfile, "w+"))) {
        perror(dbglogoutfile);
        return -1;
    }

    /* first 8 bytes are to indicate the last record */
    fseek(fpout, 8, SEEK_SET);
    fprintf(fpout, "\n");
    /* What's the dbglog record size? Did user set it? */
    if(dbgRecLimitStr) {
        dbgRecLimit = atoi(dbgRecLimitStr);
    } else {
        dbgRecLimit = 1000000; /* Million records is a good default */
    }

    s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (s < 0) {
        perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
        return -1;
    }

    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = RTMGRP_LINK;
    if (bind(s, (struct sockaddr *) &local, sizeof(local)) < 0) {
        perror("bind(netlink)");
        close(s);
        return -1;
    }

    dbglog_generate_id_tags();
#ifdef DBGLOG_DEBUG
    dbglog_print_id_tags();
#endif /* DBGLOG_DEBUG */

    while (1) {
        fromlen = sizeof(from);
        left = recvfrom(s, buf, sizeof(buf), 0,
                        (struct sockaddr *) &from, &fromlen);
        if (left < 0) {
            if (errno != EINTR && errno != EAGAIN)
                perror("recvfrom(netlink)");
            break;
        }

        h = (struct nlmsghdr *) buf;

        while (left >= sizeof(*h)) {
            int len, plen;

            len = h->nlmsg_len;
            plen = len - sizeof(*h);
            if (len > left || plen < 0) {
                perror("Malformed netlink message: ");
                break;
            }

            switch (h->nlmsg_type) {
            case RTM_NEWLINK:
                event_rtm_newlink(h, plen);
                break;
            case RTM_DELLINK:
                RECEVENT_DEBUG_PRINTF("DELLINK\n");
                break;
            default:
                RECEVENT_DEBUG_PRINTF("OTHERS\n");
            }

            len = NLMSG_ALIGN(len);
            left -= len;
            h = (struct nlmsghdr *) ((char *) h + len);
        }
    }

    fclose(fpout);
    close(s);
    return 0;
}

static void
event_rtm_newlink(struct nlmsghdr *h, int len)
{
    struct ifinfomsg *ifi;
    int attrlen, nlmsg_len, rta_len;
    struct rtattr * attr;

    if (len < sizeof(*ifi)) {
        perror("too short\n");
        return;
    }

    ifi = NLMSG_DATA(h);

    nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

    attrlen = h->nlmsg_len - nlmsg_len;
    if (attrlen < 0) {
        perror("bad attren\n");
        return;
    }

    attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

    rta_len = RTA_ALIGN(sizeof(struct rtattr));
    while (RTA_OK(attr, attrlen)) {
        if (attr->rta_type == IFLA_WIRELESS) {
            event_wireless( ((char*)attr) + rta_len, attr->rta_len - rta_len);
        } else if (attr->rta_type == IFLA_IFNAME) {

        }
        attr = RTA_NEXT(attr, attrlen);
    }
}

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

static void
event_wireless(char *data, int len)
{
    struct iw_event iwe_buf, *iwe = &iwe_buf;
    char *pos, *end, *custom, *buf;
    A_UINT16 eventid;
    A_STATUS status;

    pos = data;
    end = data + len;

    while (pos + IW_EV_LCP_LEN <= end) {
        /* Event data may be unaligned, so make a local, aligned copy
         * before processing. */
        memcpy(&iwe_buf, pos, sizeof(struct iw_event));
#if 0
        RECEVENT_DEBUG_PRINTF("Wireless event: cmd=0x%x len=%d\n",
               iwe->cmd, iwe->len);
#endif
        if (iwe->len <= IW_EV_LCP_LEN)
            return;
        switch (iwe->cmd) {
        case SIOCGIWAP:
            RECEVENT_DEBUG_PRINTF("event = new AP: "
                   "%02x:%02x:%02x:%02x:%02x:%02x" ,
                   MAC2STR((__u8 *) iwe->u.ap_addr.sa_data));
            if (memcmp(iwe->u.ap_addr.sa_data,
                   "\x00\x00\x00\x00\x00\x00", 6) == 0
                ||
                memcmp(iwe->u.ap_addr.sa_data,
                   "\x44\x44\x44\x44\x44\x44", 6) == 0)
            {
                RECEVENT_DEBUG_PRINTF(" Disassociated\n");
            }
            else
            {
                RECEVENT_DEBUG_PRINTF(" Associated\n");
            }
            break;
        case IWEVCUSTOM:
            custom = pos + IW_EV_POINT_LEN;
            if (custom + iwe->u.data.length > end)
                return;
            buf = malloc(iwe->u.data.length + 1);
            if (buf == NULL) return;
            memcpy(buf, custom, iwe->u.data.length);
            #if 0
            buf[iwe->u.data.length] = '\0';
            RECEVENT_DEBUG_PRINTF("%s\n", buf);
            #endif
            /* we send all the event content to the APP, the first two bytes is
             * event ID, then is the content of the event.
             */
            {
                eventid = *((A_UINT16*)buf);
                switch (eventid) {
                case (WMI_READY_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Ready, len = %d\n", iwe->u.data.length);
                    status = app_wmiready_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;    
                case (WMI_CONNECT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Connect, len = %d\n", iwe->u.data.length);
                    status = app_connect_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_DISCONNECT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Disconnect, len = %d\n", iwe->u.data.length);
                    status = app_disconnect_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_BSSINFO_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Bss Info, len = %d\n", iwe->u.data.length);
                    status = app_bssInfo_event_rx((A_UINT8 *)buf + ID_LEN, iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_PSTREAM_TIMEOUT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Pstream Timeout, len = %d\n", iwe->u.data.length);
                    status = app_pstream_timeout_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_ERROR_REPORT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Error Report, len = %d\n", iwe->u.data.length);
                    status = app_reportError_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_RSSI_THRESHOLD_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Rssi Threshold, len = %d\n", iwe->u.data.length);
                    status = app_rssi_threshold_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_SCAN_COMPLETE_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Scan Complete, len = %d\n", iwe->u.data.length);
                    status = app_scan_complete_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_TX_RETRY_ERR_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Tx Retry Err, len = %d\n", iwe->u.data.length);
                    break;
                case (WMIX_HB_CHALLENGE_RESP_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Challenge Resp, len = %d\n", iwe->u.data.length);
                    status = app_challenge_resp_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMIX_DBGLOG_EVENTID):
                    status = app_target_debug_event_rx((A_INT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                default:
                    RECEVENT_DEBUG_PRINTF("Host received other event with id 0x%x\n",
                                     eventid);
                    break;
                }
            }
            free(buf);
            break;
        case SIOCGIWSCAN:
            RECEVENT_DEBUG_PRINTF("event = SCAN: \n");
            break;
        case SIOCSIWESSID:
            RECEVENT_DEBUG_PRINTF("event = ESSID: ");
            custom = pos + IW_EV_POINT_LEN;
            if (custom + iwe->u.data.length > end)
                return;
            buf = malloc(iwe->u.data.length + 1);
            if (buf == NULL) return;
            memcpy(buf, custom, iwe->u.data.length);
            buf[iwe->u.data.length] = '\0';
            RECEVENT_DEBUG_PRINTF("%s\n", buf);
            free(buf);
            break;
        default:
            RECEVENT_DEBUG_PRINTF("event = Others\n");
        }

        pos += iwe->len;
    }
}

static A_STATUS app_wmiready_event_rx(A_UINT8 *datap, int len)
{
    WMI_READY_EVENT *ev;
    
    if (len < sizeof(WMI_READY_EVENT)) {
        return A_EINVAL;
    }
    ev = (WMI_READY_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive wmi ready event:\n");  
    RECEVENT_DEBUG_PRINTF("mac address =  %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x ",
              ev->macaddr[0], ev->macaddr[1], ev->macaddr[2], ev->macaddr[3], 
              ev->macaddr[4], ev->macaddr[5]);      
    RECEVENT_DEBUG_PRINTF("Physical capability = %d\n",ev->phyCapability);             
    return A_OK;
}

static A_STATUS app_connect_event_rx(A_UINT8 *datap, int len)
{
    WMI_CONNECT_EVENT *ev;
    A_UINT16 i, assoc_resp_ie_pos, assoc_req_ie_pos;;
    
    if (len < sizeof(WMI_CONNECT_EVENT)) {
        return A_EINVAL;
    }
    ev = (WMI_CONNECT_EVENT *)datap;

    RECEVENT_DEBUG_PRINTF("\nApplication receive connected event on freq %d \n", ev->channel);
    RECEVENT_DEBUG_PRINTF("with bssid %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x "
            " listenInterval=%d, assocReqLen=%d assocRespLen =%d\n",
             ev->bssid[0], ev->bssid[1], ev->bssid[2],
             ev->bssid[3], ev->bssid[4], ev->bssid[5],
             ev->listenInterval, ev->assocReqLen, ev->assocRespLen);

    /*
     * The complete Association Response Frame is delivered to the host
     */
    RECEVENT_DEBUG_PRINTF("Association Request frame: ");   
    for (i = 0; i < ev->assocReqLen; i++) 
    {
        if (!(i % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");
    assoc_req_ie_pos = sizeof(A_UINT16)  +  /* capinfo*/
                       sizeof(A_UINT16);    /* listen interval */
    RECEVENT_DEBUG_PRINTF("AssocReqIEs: ");          
    for (i = assoc_req_ie_pos; i < ev->assocReqLen; i++) 
    {
        if (!((i- assoc_req_ie_pos) % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");   
             
    RECEVENT_DEBUG_PRINTF("Association Response frame: ");     
    for (i = ev->assocReqLen; i < (ev->assocReqLen + ev->assocRespLen); i++) 
    {
        if (!((i-ev->assocReqLen) % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");
    
    assoc_resp_ie_pos = sizeof(struct ieee80211_frame) +
                        sizeof(A_UINT16)  +  /* capinfo*/
                        sizeof(A_UINT16)  +  /* status Code */
                        sizeof(A_UINT16)  ;  /* associd */
    RECEVENT_DEBUG_PRINTF("AssocRespIEs: ");                    
    for (i = ev->assocReqLen + assoc_resp_ie_pos; 
             i < (ev->assocReqLen + ev->assocRespLen); i++) 
    {
        if (!((i- ev->assocReqLen- assoc_resp_ie_pos) % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");
        
    return A_OK;
}

static A_STATUS app_disconnect_event_rx(A_UINT8 *datap, int len)
{
    WMI_DISCONNECT_EVENT *ev;
    A_UINT16 i;
    
    if (len < sizeof(WMI_DISCONNECT_EVENT)) {
        return A_EINVAL;
    }
    
    ev = (WMI_DISCONNECT_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive disconnected event: reason is %d protocol reason/status code is %d\n",
            ev->disconnectReason, ev->protocolReasonStatus);
    RECEVENT_DEBUG_PRINTF("Disconnect from %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x ",
              ev->bssid[0], ev->bssid[1], ev->bssid[2], ev->bssid[3], 
              ev->bssid[4], ev->bssid[5]);
    
    RECEVENT_DEBUG_PRINTF("\nAssocResp Frame = %s", 
                    ev->assocRespLen ? " " : "NULL");
    for (i = 0; i < ev->assocRespLen; i++) {
        if (!(i % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", datap[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");
    return A_OK;
}

static A_STATUS app_bssInfo_event_rx(A_UINT8 *datap, int len)
{
    WMI_BSS_INFO_HDR *bih;

    if (len <= sizeof(WMI_BSS_INFO_HDR)) {
        return A_EINVAL;
    }
    bih = (WMI_BSS_INFO_HDR *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive BSS info event:\n");
    RECEVENT_DEBUG_PRINTF("channel = %d, frame type = %d, snr = %d rssi = %d.\n",
            bih->channel, bih->frameType, bih->snr, bih->rssi);
    RECEVENT_DEBUG_PRINTF("BSSID is: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x \n",
              bih->bssid[0], bih->bssid[1], bih->bssid[2], bih->bssid[3], 
              bih->bssid[4], bih->bssid[5]);
    return A_OK;
}

static A_STATUS app_pstream_timeout_event_rx(A_UINT8 *datap, int len)
{
    WMI_PSTREAM_TIMEOUT_EVENT *ev;
        
    if (len < sizeof(WMI_PSTREAM_TIMEOUT_EVENT)) {
        return A_EINVAL;
    }
    ev = (WMI_PSTREAM_TIMEOUT_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive pstream timeout event:\n");
    RECEVENT_DEBUG_PRINTF("streamID= %d\n", ev->trafficClass);
    return A_OK;
}

static A_STATUS app_reportError_event_rx(A_UINT8 *datap, int len)
{
    WMI_TARGET_ERROR_REPORT_EVENT *reply;
    
    if (len < sizeof(WMI_TARGET_ERROR_REPORT_EVENT)) {
        return A_EINVAL;
    }
    reply = (WMI_TARGET_ERROR_REPORT_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive report error event\n");
    RECEVENT_DEBUG_PRINTF("error value is %d\n",reply->errorVal);

    /* Initiate recovery if its a fatal error */
    if (reply->errorVal & WMI_TARGET_FATAL_ERR) {
        /* Reset the ar6000 module in the driver */
        printf("Executing script: %s\n", restorefile);
        system(restorefile);
    }

    return A_OK;
}

static A_STATUS
app_rssi_threshold_event_rx(A_UINT8 *datap, int len)
{
    USER_RSSI_THOLD *evt;

    if (len < sizeof(USER_RSSI_THOLD)) {
        return A_EINVAL;
    }
    evt = (USER_RSSI_THOLD*)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive rssi threshold event\n");
    RECEVENT_DEBUG_PRINTF("tag is %d, rssi is %d\n", evt->tag, evt->rssi);

    return A_OK;
}

static A_STATUS
app_scan_complete_event_rx(A_UINT8 *datap, int len)
{
    RECEVENT_DEBUG_PRINTF("\nApplication receive scan complete event\n");

    return A_OK;
}

static A_STATUS
app_challenge_resp_event_rx(A_UINT8 *datap, int len)
{
    A_UINT32 cookie;

    memcpy(&cookie, datap, len);
    RECEVENT_DEBUG_PRINTF("\nApplication receive challenge response event: 0x%x\n", cookie);

    return A_OK;
}

static A_STATUS
app_target_debug_event_rx(A_INT8 *datap, int len)
{
#define BUF_SIZE    120
    A_UINT32 count;
    A_UINT32 timestamp;
    A_UINT32 debugid;
    A_UINT32 numargs;
    A_UINT32 moduleid;
    A_INT32 *buffer;
    A_UINT32 length;
    char *tm, buf[BUF_SIZE];
    long curpos;
    static int numOfRec = 0;

#ifdef DBGLOG_DEBUG
    RECEVENT_DEBUG_PRINTF("Application received target debug event: %d\n", len);
#endif /* DBGLOG_DEBUG */
    count = 0;
    buffer = (A_INT32 *)datap;
    length = (len >> 2);
    while (count < length) {
        debugid = DBGLOG_GET_DBGID(buffer[count]);
        moduleid = DBGLOG_GET_MODULEID(buffer[count]);
        numargs = DBGLOG_GET_NUMARGS(buffer[count]);
        timestamp = DBGLOG_GET_TIMESTAMP(buffer[count]);
        GET_CURRENT_TIME(tm);
        switch (numargs) {
            case 0:
            fprintf(fpout, "%s: %s (%d)\n", tm,
                    dbglog_id_tag[moduleid][debugid],
                    timestamp);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d)\n",
                                  dbglog_id_tag[moduleid][debugid], 
                                  timestamp);
#endif /* DBGLOG_DEBUG */
            break;

            case 1:
            fprintf(fpout, "%s: %s (%d): 0x%x\n", tm,
                    dbglog_id_tag[moduleid][debugid], 
                    timestamp, buffer[count+1]);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d): 0x%x\n", 
                                  dbglog_id_tag[moduleid][debugid], 
                                  timestamp, buffer[count+1]);
#endif /* DBGLOG_DEBUG */
            break;

            case 2:
            fprintf(fpout, "%s: %s (%d): 0x%x, 0x%x\n", tm,
                    dbglog_id_tag[moduleid][debugid], 
                    timestamp, buffer[count+1],
                    buffer[count+2]);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d): 0x%x, 0x%x\n",
                                  dbglog_id_tag[moduleid][debugid], 
                                  timestamp, buffer[count+1],
                                  buffer[count+2]);
#endif /* DBGLOG_DEBUG */
            break;

            default:
            RECEVENT_DEBUG_PRINTF("Invalid args: %d\n", numargs);
        }
        count += (numargs + 1);

        numOfRec++;
        if(dbgRecLimit && (numOfRec % dbgRecLimit == 0)) {
            /* Once record limit is hit, rewind to start
             * after 8 bytes from start 
             */
            numOfRec = 0;
            curpos = ftell(fpout);
            truncate(dbglogoutfile, curpos);
            rewind(fpout);
            fseek(fpout, 8, SEEK_SET);
            fprintf(fpout, "\n");
        }
    }

    /* Update the last rec at the top of file */
    curpos = ftell(fpout);
    if( fgets(buf, BUF_SIZE, fpout) ) {
        buf[BUF_SIZE - 1] = 0;  /* In case string is longer from logs */
        length = strlen(buf);
        memset(buf, ' ', length-1);
        buf[length] = 0;
        fseek(fpout, curpos, SEEK_SET);
        fprintf(fpout, "%s", buf);
    }

    rewind(fpout);
    /* Update last record */
    fprintf(fpout, "%08d\n", numOfRec);
    fseek(fpout, curpos, SEEK_SET);
    fflush(fpout);

#undef BUF_SIZE
    return A_OK;
}
