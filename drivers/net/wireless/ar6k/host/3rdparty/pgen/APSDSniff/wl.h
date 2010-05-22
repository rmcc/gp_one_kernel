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
#ifndef _WL_H_
#define _WL_H_

#include <pcap.h>

#define GET_BIT_FIELD(_data, _type) (((_data)& _type ## _MASK) >> (_type ## _SHIFT))

#define DSBITS_FCERR 0x0000000000000001
#define AID_FCERR    0x0000000000000002
#define ORDER_FCERR  0x0000000000000004
#define PARSE_FCERR  0x0000000000000008
#define PARSE_DURERR 0x0000000000000010
#define ADDR_ERR     0x0000000000000020
#define SEQ_ERR      0x0000000000000040


struct wlan_header {
	u_char fc[0];
	u_char dur[2];
	u_char addr1[6];
	u_char addr2[6];
	u_char addr3[6];
	u_short seq;
};

struct wlan_qos_header {
	u_char fc[0];
	u_char dur[2];
	u_char addr1[6];
	u_char addr2[6];
	u_char addr3[6];
	u_short seq;
        u_short qos;
};

struct wlan_qos_long_header {
	u_char fc[0];
	u_char dur[2];
	u_char addr1[6];
	u_char addr2[6];
	u_char addr3[6];
	u_short seq;
	u_char addr4[6];
        u_short qos;
};

struct field_control {
        u_short rawData;
        u_char type;
        //u_char subtype;
        u_short subtype;
        u_char typeIndex;
        u_char protoVers;
        u_char toDS;
        u_char fromDS;
        u_char moreFrag;
        u_char retry;
        u_char pwrMgt;
        u_char moreData;
        u_char WEP;
        u_char order;
};

struct duration_field {
        u_short rawData;
        u_short duration;
        u_short AID;
};

struct sequence_control {
        u_short rawData;
        u_char fragNum;
        u_short seqNum;
};

struct capability_info {
        u_short rawData;
        u_char ess;
        u_char ibss;
        u_char cfPoll;
        u_char cfPollReq;
        u_char privacy;
        u_char shortPreamble;
        u_char pbcc;
        u_char chanAgil;
        u_char specMgmt;
        u_char qos;
        u_char shortSlotTime;
        u_char apsd;
        u_char dsssOfdm;
        u_char blockAck;
};

struct ssid_ielem {
        u_char length;
        u_char ssid[33];
};

struct supp_rates_ielem {
        u_char length;
        u_char rates[8];
};

struct fh_param_set_ielem {
        u_char length;
        u_short dwellTime;
        u_char hopSet;
        u_char hopPattern;
        u_char hopIndex;
};

struct ds_param_set_ielem {
        u_char length;
        u_char currChannel;
};

struct country_ielem {
	u_char length;
    char country_str[2];
	char usage;
	u_char triplets[256*3];
};

struct power_constraint_ielem {
	u_char length;
	u_char constraint;
};

struct power_capability_ielem {
	u_char length;
	u_char minTx;
	u_char maxTx;
};

struct chan_switch_ielem {
	u_char length;
	u_char mode;
	u_char newChan;
	u_char count;
};

struct supported_chan_ielem {
	u_char length;
	u_char channelBitmap[26];
};

struct cf_param_set_ielem {
        u_char length;
        u_char cfpCount;
        u_char cfpPeriod;
        u_short cfpMaxDur;
        u_short cfpDurRemain;
};

struct tim_ielem {
        u_char length;
        u_char dtimCount;
        u_char dtimPeriod;
        u_char bitmapControl;
        u_char virtualBitmap[251];
};

struct ibss_param_set_ielem {
        u_char length;
        u_short atimWindow;
};

struct chall_text_ielem {
        u_char length;
        u_char text[254];
};

struct qos_control_field {
        u_short rawData;
        u_char eosp;        // End of Service Period
        u_char ackPolicy;   // Ack policy
        u_char up;          // UserPriority field  - low 3 bits
};

/*
 * AP QOSINFO defs
 */
#define WME_QOSINFO_PARAM_SET_COUNT_MASK 	0xf
#define WME_QOSINFO_PARAM_SET_COUNT_SHIFT 	0
#define WME_QOSINFO_UAPSD_EN_MASK 		0x80
#define WME_QOSINFO_UAPSD_EN_SHIFT 		7
/*
 * STA QOSINFO defs
 */
#define WME_QOSINFO_UAPSD_VO_MASK	1
#define WME_QOSINFO_UAPSD_VO_SHIFT	0
#define WME_QOSINFO_UAPSD_VI_MASK	2
#define WME_QOSINFO_UAPSD_VI_SHIFT	1
#define WME_QOSINFO_UAPSD_BK_MASK	4
#define WME_QOSINFO_UAPSD_BK_SHIFT	2
#define WME_QOSINFO_UAPSD_BE_MASK	8
#define WME_QOSINFO_UAPSD_BE_SHIFT	3
#define WME_QOSINFO_UAPSD_MAXSP_MASK	0x60
#define WME_QOSINFO_UAPSD_MAXSP_SHIFT	5

struct wme_ielem {
        u_char length;
        u_char oui[3];
        u_char ouiType;
        u_char ouiSubtype;
        u_char version;
        u_char qosInfo;
};

struct ath_advCap_ielem {
	u_char length;
	u_char oui[3];
	u_char ouiType;
	u_char ouiSubtype;
	u_char version;
	u_short capability;
	u_short defKeyIndex;
};

struct ac_param_record {
        u_char aciByte;
        u_char ecwByte;
        u_short txop;
        u_char aci;
        u_char acm;
        u_char aifsn;
        u_char ecwMax;
        u_char ecwMin;
};

struct wme_param_ielem {
        u_char length;
        u_char oui[3];
        u_char ouiType;
        u_char ouiSubtype;
        u_char version;
        u_char qosInfo;
        u_char reserved;
        struct ac_param_record AC_BE;
        struct ac_param_record AC_BK;
        struct ac_param_record AC_VI;
        struct ac_param_record AC_VO;
};

struct mgmt_action_elem {
        u_char catCode;
        u_char actionCode;
        u_char dialogToken;
        u_char statusCode;
};

struct wme_tspec_ielem {
        u_char length;
        u_char oui[3];
        u_char ouiType;
        u_char ouiSubtype;
        u_char version;
        u_char tsInfo[3];
        u_short nomMSDUsize;
        u_short maxMSDUsize;
        u_long minServInt;
        u_long maxServInt;
        u_long inactivInt;
        u_long suspensionInt;
        u_long servStartTime;
        u_long minDataRate;
        u_long meanDataRate;
        u_long peakDataRate;
        u_long maxBurstSize;
        u_long delayBound;
        u_long minPhyRate;
        u_short surplusBWallow;
        u_short mediumTime;

        u_char tid;
        u_char direction;
        u_char psb;
        u_char up;
};

struct qbss_load_ielem {
        u_char length;
        u_short stationCount;
        u_char chanUtil;
        u_short availAdmCap;
};

struct edca_param_set_ielem {
        u_char length;
        u_char qosInfo;
        u_char qAck;
        u_char qReq;
        u_char txopReq;
        u_char moreDataAck;
        u_char edcaParamUpdateCnt;
        u_char reserved;
        struct ac_param_record AC_BE;
        struct ac_param_record AC_BK;
        struct ac_param_record AC_VI;
        struct ac_param_record AC_VO;
};

struct qos_capability_ielem {
        u_char length;
        u_char qosInfo;
        u_char qAck;
        u_char qReq;
        u_char txopReq;
        u_char moreDataAck;
        u_char edcaParamUpdateCnt;
};

struct type0_field {
        u_char sourceAddr[6];
        u_char destAddr[6];
        u_short type;
};

struct type1_ipv4_field {
        u_char sourceIPAddr[4];
        u_char destIPAddr[4];
        u_short sourcePort;
        u_short destPort;
        u_char dscp;
        u_char protocol;
        u_char reserved;
};

struct type1_ipv6_field {
        u_char sourceIPAddr[16];
        u_char destIPAddr[16];
        u_short sourcePort;
        u_short destPort;
        u_char flowLabel[3];
};
  
struct type1_field {
        u_char version;
        struct type1_ipv4_field ipv4;
        struct type1_ipv6_field ipv6;
};

struct tclas_ielem {
        u_char length;
        u_char userPriority;
        u_char clasType;
        u_char clasMask;
        struct type0_field type0Field;
        struct type1_field type1Field;
        u_short qTagType8021;  // This is the only field in a type 2 field
};

struct mgmt_frame {
	unsigned long long error;
	u_short authAlgNum;
	u_short authTransSeqNum;
	u_short beaconInt;
	struct capability_info capInfo;
	u_char currAPaddr[6];
	u_short listenInt;
	u_short reasonCode;
	u_short aid;
	u_short statusCode;
	unsigned long long timestamp;
	struct ssid_ielem ssid;
	struct supp_rates_ielem suppRates;
	struct fh_param_set_ielem fhParamSet;
	struct ds_param_set_ielem dsParamSet;
	struct cf_param_set_ielem cfParamSet;
	struct country_ielem country;
	struct power_constraint_ielem powerConstraint;
	struct power_capability_ielem powerCapability;
	struct supported_chan_ielem supportedChan;
	struct chan_switch_ielem chanSwitch;
	struct tim_ielem tim;
	struct ibss_param_set_ielem ibssParamSet;
	struct chall_text_ielem challText;
	struct wme_ielem wme;
	struct wme_param_ielem wmeParam;
	struct mgmt_action_elem mgmtAction;
	struct wme_tspec_ielem wmeTspec;
	struct qbss_load_ielem qbssLoad;
	struct edca_param_set_ielem edcaParamSet;
	struct qos_capability_ielem qosCap;
	struct tclas_ielem tclas[2];
	struct ath_advCap_ielem athAdvCap;
	int tclasCount;
	u_char ielemParsed;
	u_char isAp;
};

struct data_frame {
        unsigned long long error;
        int payLen;
        u_char *payPtr;
};

struct packet_info{
        u_char partialCap;
        u_char isAp;
        unsigned long long error;
        struct field_control fc;
        struct duration_field duration;
        u_char addr1[6];
        u_char addr2[6];
        u_char addr3[6];
        u_char addr4[6];
        struct sequence_control seqCtrl;
        struct mgmt_frame mgmt;
        struct data_frame data;
        unsigned long fcs;
        unsigned long crc;
        struct qos_control_field qosControl;
};


// Filter specification
struct packet_filter {
	int                   ssid;
	char                  filtSsid[128];
	int                   bssid;
	char                  filtBssid[8]; 
	int                   printCtlFrms;
	int                   beaconLimit;
	int                   beaconCount;
    int                   sourceFilter;
    unsigned char         sourceAddr[6];
    int                   destFilter;
    unsigned char         destAddr[6];
    int                   staFilter;
    unsigned char         staAddr[6];
    u_int32_t             totalPrinted;
};

extern struct packet_filter g_filter_spec;

#define SHM_MAX_PACKET_SIZE 2346
//#define SHM_BUCKET_SIZE  (2*sizeof(u_int) + SHM_MAX_PACKET_SIZE)
#define SHM_MAX_BUCKET  2048
#define SHM_BUF_HEAD 0
#define SHM_BUF_TAIL (SHM_MAX_BUCKET-1)
#define SHM_LENS_SIZE sizeof(u_int)

struct cap_info
{
   u_int cap_len;
   u_int len;
   u_char packet[SHM_MAX_PACKET_SIZE];
};

#define SHM_BUCKET_SIZE sizeof(struct cap_info)

#endif
