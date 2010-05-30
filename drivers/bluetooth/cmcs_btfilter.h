/*
 *
 *  Bluetooth FILTER driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _CMCS_BTFILTER_H_
#define _CMCS_BTFILTER_H_


#define OGF_SHIFT                   10
#define OGF_MASK                    0xFC

#define MAKE_HCI_COMMAND(ogf,ocf)   (((ogf) << OGF_SHIFT) | (ocf))
#define HCI_GET_OP_CODE(p)          (((u16)((p)[1])) << 8) | ((u16)((p)[0]))
#define HCI_TEST_OGF(p,ogf)         (((p)[1] & OGF_MASK) == ((ogf) << 2))

#define HCI_LINK_CONTROL_OGF        0x01
#define IS_LINK_CONTROL_CMD(p)      HCI_TEST_OGF(p,HCI_LINK_CONTROL_OGF)
#define HCI_INQUIRY                 MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0001)
#define HCI_INQUIRY_CANCEL          MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0002)
#define HCI_PER_INQUIRY             MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0003)
#define HCI_PER_INQUIRY_CANCEL      MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0004)
#define HCI_CREATE_CONNECTION       MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0005)
#define HCI_DISCONNECT              MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0006)
#define HCI_ADD_SCO                 MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0007)
#define HCI_ACCEPT_CONN_REQ         MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0009)
#define HCI_REJECT_CONN_REQ         MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x000A)
#define HCI_SETUP_SCO_CONN          MAKE_HCI_COMMAND(HCI_LINK_CONTROL_OGF, 0x0028) /* BT 2.0 */

#define HCI_GET_EVENT_CODE(p)       ((p)[0])
#define GET_BT_EVENT_LENGTH(p)      ((p)[1])
#define HCI_EVT_INQUIRY_COMPLETE    0x01
#define HCI_EVT_CONNECT_COMPLETE    0x03
#define HCI_EVT_CONNECT_REQUEST     0x04
#define HCI_EVT_DISCONNECT          0x05
#define HCI_EVT_REMOTE_NAME_REQ     0x07
#define HCI_EVT_ROLE_CHANGE         0x12
#define HCI_EVT_NUM_COMPLETED_PKTS  0x13
#define HCI_EVT_MODE_CHANGE         0x14
#define HCI_EVT_SCO_CONNECT_COMPLETE 0x2C  /* new to 2.0 */

/* HCI Connection Complete Event macros */
#define GET_BT_CONN_EVENT_STATUS(p) ((p)[2])
#define GET_BT_CONN_HANDLE(p)       ((u16)((p)[3]) | (((u16)((p)[4])) << 8))
#define GET_BT_CONN_LINK_TYPE(p)    ((p)[11])
#define BT_CONN_EVENT_STATUS_SUCCESS(p) (GET_BT_CONN_EVENT_STATUS(p) == 0)
#define INVALID_BT_CONN_HANDLE      0xFFFF
#define BT_LINK_TYPE_SCO            0x00
#define BT_LINK_TYPE_ACL            0x01
#define BT_LINK_TYPE_ESCO           0x02


/* SCO Connection Complete Event macros */
#define GET_TRANS_INTERVAL(p)   ((p)[12])
#define GET_RETRANS_INTERVAL(p) ((p)[13])
#define GET_RX_PKT_LEN(p)       ((u16)((p)[14]) | (((u16)((p)[15])) << 8))
#define GET_TX_PKT_LEN(p)       ((u16)((p)[16]) | (((u16)((p)[17])) << 8))



typedef struct {
    u32 numScoCyclesForceTrigger;  /* Number SCO cycles after which
                                           force a pspoll. default = 10 */
    u32 dataResponseTimeout;       /* Timeout Waiting for Downlink pkt
                                           in response for ps-poll,
                                           default = 10 msecs */
    u32  stompScoRules;
    u32 scoOptFlags;               /* SCO Options Flags :
                                            bits:     meaning:
                                             0        Allow Close Range Optimization
                                             1        Force awake during close range 
                                             2        If set use host supplied RSSI for OPT
                                             3        If set use host supplied RTS COUNT for OPT
                                             4..7     Unused
                                             8..15    Low Data Rate Min Cnt
                                             16..23   Low Data Rate Max Cnt
                                        */

    u8 stompDutyCyleVal;           /* Sco cycles to limit ps-poll queuing
                                           if stomped */
    u8 stompDutyCyleMaxVal;        /*firm ware increases stomp duty cycle
                                          gradually uptill this value on need basis*/
    u8 psPollLatencyFraction;      /* Fraction of idle
                                           period, within which
                                           additional ps-polls
                                           can be queued */
    u8 noSCOSlots;                 /* Number of SCO Tx/Rx slots.
                                           HVx, EV3, 2EV3 = 2 */
    u8 noIdleSlots;                /* Number of Bluetooth idle slots between
                                           consecutive SCO Tx/Rx slots
                                           HVx, EV3 = 4
                                           2EV3 = 10 */
	u8 scoOptOffRssi;/*RSSI value below which we go to ps poll*/
	u8 scoOptOnRssi; /*RSSI value above which we reenter opt mode*/
    u8 scoOptRtsCount;                                     
} BT_PARAMS_SCO;

typedef struct {
    u32 a2dpWlanUsageLimit; /* MAX time firmware uses the medium for
                                    wlan, after it identifies the idle time
                                    default (30 msecs) */
    u32 a2dpBurstCntMin;   /* Minimum number of bluetooth data frames
                                   to replenish Wlan Usage  limit (default 3) */
    u32 a2dpDataRespTimeout;
    u32 a2dpOptFlags;      /* A2DP Option flags:
                                       bits:    meaning:
                                        0       Allow Close Range Optimization
                                        1       Force awake during close range 
                                        2        If set use host supplied RSSI for OPT
                                        3        If set use host supplied RTS COUNT for OPT
                                        4..7    Unused
                                        8..15   Low Data Rate Min Cnt
                                        16..23  Low Data Rate Max Cnt
                                 */
    u8 isCoLocatedBtRoleMaster;
	u8 a2dpOptOffRssi;/*RSSI value below which we go to ps poll*/
	u8 a2dpOptOnRssi; /*RSSI value above which we reenter opt mode*/                                     
    u8 a2dpOptRtsCount;                                     
}BT_PARAMS_A2DP;

typedef struct {
    u32 aclWlanMediumUsageTime;  /* Wlan usage time during Acl (non-a2dp)
                                       coexistence (default 30 msecs) */
    u32 aclBtMediumUsageTime;   /* Bt usage time during acl coexistence
                                       (default 30 msecs)*/
    u32 aclDataRespTimeout;
    u32 aclDetectTimeout;      /* ACL coexistence enabled if we get 
                                       10 Pkts in X msec(default 100 msecs) */
    u32 aclmaxPktCnt;          /* No of ACL pkts to receive before
                                         enabling ACL coex */

}BT_PARAMS_ACLCOEX;

typedef struct {
    union {
        BT_PARAMS_SCO scoParams;
        BT_PARAMS_A2DP a2dpParams;
        BT_PARAMS_ACLCOEX  aclCoexParams;
        u8 antType;         /* 0 -Disabled (default)
                                     1 - BT_ANT_TYPE_DUAL
                                     2 - BT_ANT_TYPE_SPLITTER
                                     3 - BT_ANT_TYPE_SWITCH */
        u8 coLocatedBtDev;  /* 0 - BT_COLOCATED_DEV_BTS4020 (default)
                                     1 - BT_COLCATED_DEV_CSR
                                     2 - BT_COLOCATED_DEV_VALKYRIe
                                   */
    } info;
    u8 paramType ;
} WMI_SET_BT_PARAMS_CMD;


typedef enum {
    BT_STATUS_UNDEF = 0,
    BT_STATUS_START,
    BT_STATUS_STOP,
    BT_STATUS_RESUME,
    BT_STATUS_SUSPEND,
    BT_STATUS_MAX
} BT_STREAM_STATUS;

typedef enum {
    BT_PARAM_SCO = 1,         /* SCO stream parameters */
    BT_PARAM_A2DP ,
    BT_PARAM_ANTENNA_CONFIG,
    BT_PARAM_COLOCATED_BT_DEVICE,
    BT_PARAM_ACLCOEX,
    BT_PARAM_11A_SEPARATE_ANT,
    BT_PARAM_MAX
} BT_PARAM_TYPE;

typedef enum {
    BT_STREAM_UNDEF = 0,
    BT_STREAM_SCO,             /* SCO stream */
    BT_STREAM_A2DP,            /* A2DP stream */
    BT_STREAM_SCAN,            /* BT Discovery or Page */
    BT_STREAM_ESCO,
    BT_STREAM_MAX
} BT_STREAM_TYPE;

typedef struct _COEX_CONNECTION_INFO {
    u16         hScoConnect; 
    u16         hEscoConnect;
    u8          streamType;
    u8          status;
    bool        Valid;
    u8          LinkType;
    u8          TransmissionInterval;
    u8          RetransmissionInterval;
    u16         RxPacketLength;
    u16         TxPacketLength;
    WMI_SET_BT_PARAMS_CMD   paramCmd;
} COEX_CONNECTION_INFO;



void BTFilter_Init(void);
void BTFilter_Deinit(void);
void BTFilter_HCIEvent(unsigned char *pBuffer, int len);
void BTFilter_HCICommand(unsigned char *pBuffer, int len);

#endif /*_CMCS_BTFILTER_H_*/
