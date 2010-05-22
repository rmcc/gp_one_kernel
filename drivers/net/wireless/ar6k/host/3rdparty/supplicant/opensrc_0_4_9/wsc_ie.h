#ifndef _WSC_IE_H_
#define _WSC_IE_H_


#ifdef __GNUC__
#define __ATTRIB_PACK           __attribute__ ((packed))
#else
#pragma pack(push, 1)
#define __ATTRIB_PACK 
#endif

typedef struct wsc_ie_data {
	struct wpa_supplicant *wpa_s;
    int udpFdCom;
	int sendCounter;
	int sendUp;
} __ATTRIB_PACK WSC_IE_DATA;

#define WSC_WLAN_UDP_PORT       38000
#define WSC_WLAN_UDP_ADDR       "127.0.0.1"

#define WSC_WLAN_DATA_MAX_LENGTH         1024

#define WSC_IE_TYPE_SET_BEACON_IE     			1
#define WSC_IE_TYPE_SET_PROBE_REQUEST_IE     	2
#define WSC_IE_TYPE_SET_PROBE_RESPONSE_IE     	3
#define WSC_IE_TYPE_BEACON_IE_DATA     			4
#define WSC_IE_TYPE_PROBE_REQUEST_IE_DATA     	5
#define WSC_IE_TYPE_PROBE_RESPONSE_IE_DATA     	6
#define WSC_IE_TYPE_SEND_BEACONS_UP				7
#define WSC_IE_TYPE_SEND_PR_RESPS_UP			8
#define WSC_IE_TYPE_SEND_PROBE_REQUEST			9
#define WSC_IE_TYPE_MAX                         10

typedef struct wsc_ie_command_data {
    u8 type;
	u32 length;
	u8 data[];
} __ATTRIB_PACK WSC_IE_COMMAND_DATA;

typedef struct wsc_ie_beacon_data
{
	char        ssid[32];
    u8          macAddr[6];
    u8          data[];
} __ATTRIB_PACK WSC_IE_BEACON_DATA;


int wsc_ie_init(struct wpa_supplicant *wpa_s);
int wsc_ie_deinit(struct wpa_supplicant *wpa_s);
 
#ifndef __GNUC__
#pragma pack(pop)
#endif

#endif // _WSC_IE_H_

