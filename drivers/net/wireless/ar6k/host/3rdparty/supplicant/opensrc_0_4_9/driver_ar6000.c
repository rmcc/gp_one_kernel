/*
 * WPA Supplicant - driver interaction with Atheros AR6000 driver
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2005, Atheros Communications, Inc.
 * Copyright (c) 2005, Devicescape Software, Inc.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/socket.h>

#include "common.h"
#include "driver.h"
#include "driver_wext.h"
#include "eloop.h"
#include "wpa_supplicant.h"
#include "wpa.h"

#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#include <netinet/in.h>
#include "l2_packet.h"

#include <athdefs.h>
#include <a_types.h>
#include <a_osapi.h>
#include <athdrv_linux.h>
#include <athtypes_linux.h>
#include <wmi.h>
#include <ieee80211.h>
#include <ieee80211_ioctl.h>
#include <net/if_arp.h>
#include <linux/if.h>
#include <linux/wireless.h>

#ifdef CONFIG_CCX
/* TODO: Remove this once ieee80211.h includes define for KRK. */
#ifndef IEEE80211_CIPHER_CCKM_KRK
#define IEEE80211_CIPHER_CCKM_KRK IEEE80211_CIPHER_MAX
#endif
#endif /* CONFIG_CCX */

#ifdef EAP_WSC
static int wpa_driver_madwifi_set_wsc_status(void *priv, int status);
#endif

struct wpa_driver_madwifi_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
	struct l2_packet_data * l2_sock;
};

static int
set80211priv(struct wpa_driver_madwifi_data *drv, int op, void *data, int len,
	     int show_err)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	if ((len < IFNAMSIZ) && (op != AR6000_IOCTL_EXTENDED)) {
		/*
		 * Argument data fits inline; put it there.
		 */
		memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(drv->sock, op, &iwr) < 0) {
		if (show_err) {
			static const char *opnames[] = {
				"ioctl[IEEE80211_IOCTL_SETPARAM]",
				"ioctl[IEEE80211_IOCTL_GETPARAM]",
				"ioctl[IEEE80211_IOCTL_SETKEY]",
				"ioctl[IEEE80211_IOCTL_GETKEY]",
				"ioctl[IEEE80211_IOCTL_DELKEY]",
				NULL,
				NULL,
				NULL,
				"ioctl[IEEE80211_IOCTL_ADDPMKID]",
				NULL,
			};
			if (IEEE80211_IOCTL_SETPARAM <= op &&
			    op <= IEEE80211_IOCTL_LASTONE)
				wpa_printf(MSG_DEBUG, "IOCTL Error: %s", opnames[op - SIOCIWFIRSTPRIV]);
			else
				wpa_printf(MSG_DEBUG, "unknown IOCTL");
		}
		return -1;
	}
	return 0;
}

static int
set80211param(struct wpa_driver_madwifi_data *drv, int op, int arg,
	      int show_err)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.mode = op;
	memcpy(iwr.u.name+sizeof(__u32), &arg, sizeof(arg));

	if (ioctl(drv->sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		if (show_err)
			perror("ioctl[IEEE80211_IOCTL_SETPARAM]");
		return -1;
	}
	return 0;
}

static int
wpa_driver_madwifi_set_wpa_ie(struct wpa_driver_madwifi_data *drv,
			      const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	/* NB: SETOPTIE is not fixed-size so must not be inlined */
	iwr.u.data.pointer = (void *) wpa_ie;
	iwr.u.data.length = wpa_ie_len;

	if (ioctl(drv->sock, IEEE80211_IOCTL_SETOPTIE, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "ioctl[IEEE80211_IOCTL_SETOPTIE] failed");
		return -1;
	}
	return 0;
}

static int
wpa_driver_madwifi_del_key(struct wpa_driver_madwifi_data *drv, int key_idx,
			   const u8 *addr)
{
	struct ieee80211req_del_key wk;

	wpa_printf(MSG_DEBUG, "%s: keyidx=%d", __FUNCTION__, key_idx);
	memset(&wk, 0, sizeof(wk));
	wk.idk_keyix = key_idx;
	if (addr != NULL)
		memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);

	return set80211priv(drv, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk), 1);
}

static int
wpa_driver_madwifi_set_key(void *priv, wpa_alg alg,
			   const u8 *addr, int key_idx, int set_tx,
			   const u8 *seq, size_t seq_len,
			   const u8 *key, size_t key_len)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_key wk;
	char *alg_name;
	u_int8_t cipher;

	if (alg == WPA_ALG_NONE)
		return wpa_driver_madwifi_del_key(drv, key_idx, addr);

	switch (alg) {
	case WPA_ALG_WEP:
		alg_name = "WEP";
		cipher = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
#ifdef CONFIG_CCX
	case WPA_ALG_CCKM_KRK:
		alg_name = "CCKM KRK";
		cipher = IEEE80211_CIPHER_CCKM_KRK;
		break;
#endif /* CONFIG_CCX */
	default:
		wpa_printf(MSG_DEBUG, "%s: unknown/unsupported algorithm %d",
			__FUNCTION__, alg);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: alg=%s key_idx=%d set_tx=%d seq_len=%lu "
		   "key_len=%lu", __FUNCTION__, alg_name, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);

	if (seq_len > sizeof(u_int64_t)) {
		wpa_printf(MSG_DEBUG, "%s: seq_len %lu too big",
			   __FUNCTION__, (unsigned long) seq_len);
		return -2;
	}
	if (key_len > sizeof(wk.ik_keydata)) {
		wpa_printf(MSG_DEBUG, "%s: key length %lu too big",
			   __FUNCTION__, (unsigned long) key_len);
		return -3;
	}

	memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV;
	if (set_tx) {
		wk.ik_flags |= IEEE80211_KEY_XMIT | IEEE80211_KEY_DEFAULT;
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	} else
		memset(wk.ik_macaddr, 0, IEEE80211_ADDR_LEN);
	wk.ik_keyix = key_idx;
	wk.ik_keylen = key_len;
	memcpy(&wk.ik_keyrsc, seq, seq_len);
	memcpy(wk.ik_keydata, key, key_len);

	wpa_printf(MSG_DEBUG, "%s: key_ix is %d key_len is %d key_flags is %d",
		   __FUNCTION__, wk.ik_keyix, wk.ik_keylen, wk.ik_flags);
	if (seq_len)
		wpa_hexdump(MSG_DEBUG, "AR6000: set_key - seq", seq, seq_len);

	wpa_printf(MSG_DEBUG, "%s: sizeof wk is %d ieee80211req_key is %d"
		   " IEEE80211_IOCTL_SETKEY val is %d",
		   __FUNCTION__,
		   sizeof(wk),
		   sizeof(struct ieee80211req_key),
		   IEEE80211_IOCTL_SETKEY);
	return set80211priv(drv, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk), 1);
}

static int
wpa_driver_madwifi_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_madwifi_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_COUNTERMEASURES, enabled, 1);
}


static int
wpa_driver_madwifi_set_drop_unencrypted(void *priv, int enabled)
{
	struct wpa_driver_madwifi_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_DROPUNENCRYPTED, enabled, 1);
}

static int
wpa_driver_madwifi_deauthenticate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme),
			    1);
}

static int
wpa_driver_madwifi_disassociate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_mlme mlme;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme),
			    1);
}


static u32 ar6000_cipher(int cipher, int *cipher_len)
{
	switch (cipher) {
	case CIPHER_WEP40:
		*cipher_len = 5;
		return IEEE80211_CIPHER_WEP;
	case CIPHER_WEP104:
		*cipher_len = 13;
		return IEEE80211_CIPHER_WEP;
	case CIPHER_TKIP:
		return IEEE80211_CIPHER_TKIP;
	case CIPHER_CCMP:
		return IEEE80211_CIPHER_AES_CCM;
	default:
		return IEEE80211_CIPHER_NONE;
	}
}


static u32 ar6000_key_mgmt(int key_mgmt, int auth_alg)
{
	switch (key_mgmt) {
	case KEY_MGMT_802_1X:
		return IEEE80211_AUTH_WPA;
	case KEY_MGMT_PSK:
		return IEEE80211_AUTH_WPA_PSK;
	case KEY_MGMT_NONE:
		if ((auth_alg & (AUTH_ALG_OPEN_SYSTEM | AUTH_ALG_SHARED_KEY))
		    == (AUTH_ALG_OPEN_SYSTEM | AUTH_ALG_SHARED_KEY))
			return IEEE80211_AUTH_AUTO;
		if (auth_alg & AUTH_ALG_SHARED_KEY)
			return IEEE80211_AUTH_SHARED;
		else
			return IEEE80211_AUTH_OPEN;
	case KEY_MGMT_802_1X_NO_WPA:
		return IEEE80211_AUTH_OPEN;
	case KEY_MGMT_WPA_NONE:
		return IEEE80211_AUTH_NONE; /* FIX: adhoc mode? */
#ifdef CONFIG_CCX
	case KEY_MGMT_CCKM:
		return IEEE80211_AUTH_WPA_CCKM;
#endif /* CONFIG_CCX */
	default:
		return IEEE80211_AUTH_AUTO;
	}
}


static int
wpa_driver_madwifi_associate(void *priv,
			     struct wpa_driver_associate_params *params)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret = 0, privacy = 1;
	int cipher_len;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	wpa_printf(MSG_DEBUG, "AR6000 - wpa_ie_len=%d", params->wpa_ie_len);

	/*
	 * NB: Don't need to set the freq or cipher-related state as
	 *     this is implied by the bssid which is used to locate
	 *     the scanned node state which holds it.  The ssid is
	 *     needed to disambiguate an AP that broadcasts multiple
	 *     ssid's but uses the same bssid.
	 */
	/* XXX error handling is wrong but unclear what to do... */

#ifdef EAP_WSC
if(!g_wsc) {
	wpa_driver_madwifi_set_wsc_status(priv, WSC_REG_INACTIVE);
#endif
	if (wpa_driver_madwifi_set_wpa_ie(drv, params->wpa_ie,
		  	params->wpa_ie_len) < 0 && params->wpa_ie_len)
		ret = -1;

	if (params->pairwise_suite == CIPHER_NONE &&
	    params->group_suite == CIPHER_NONE &&
	    params->key_mgmt_suite == KEY_MGMT_NONE &&
	    params->wpa_ie_len == 0)
		privacy = 0;

	wpa_printf(MSG_DEBUG, "AR6000 - IEEE80211_PARAM_PRIVACY=%d", privacy);
	if (set80211param(drv, IEEE80211_PARAM_PRIVACY, privacy, 1) < 0) {
		wpa_printf(MSG_DEBUG, "AR6000 - IEEE80211_PARAM_PRIVACY=%d", privacy);
		ret = -1;
	}

	if (params->wpa_ie_len &&
	    set80211param(drv, IEEE80211_PARAM_WPA,
			  params->wpa_ie[0] == RSN_INFO_ELEM ? 2 : 1, 1) < 0)
		ret = -1;

	if (params->bssid == NULL) {
		/* ap_scan=2 mode - driver takes care of AP selection and
		 * roaming */
		u32 auth_mode, cipher_type, wpa_mode;

		if (params->wpa_ie_len == 0)
			wpa_mode = WPA_MODE_NONE;
		else if (params->wpa_ie[0] == RSN_INFO_ELEM)
			wpa_mode = WPA_MODE_WPA2;
		else
			wpa_mode = WPA_MODE_WPA1;
		wpa_printf(MSG_DEBUG, "AR6000 - IEEE80211_PARAM_WPA=%d",
			   wpa_mode);
		if (set80211param(drv, IEEE80211_PARAM_WPA, wpa_mode, 1) < 0) {
			wpa_printf(MSG_DEBUG, "AR6000 - Setting "
				   "IEEE80211_PARAM_WPA=%d failed", wpa_mode);
			ret = -1;
		}

		auth_mode = ar6000_key_mgmt(params->key_mgmt_suite,
					    params->auth_alg);
		wpa_printf(MSG_DEBUG, "AR6000 - IEEE80211_PARAM_AUTHMODE=%d",
			   auth_mode);
		if (set80211param(drv, IEEE80211_PARAM_AUTHMODE, auth_mode, 1)
		    < 0) {
			wpa_printf(MSG_DEBUG, "AR6000 - Setting "
				   "IEEE80211_PARAM_AUTHMODE=%d", auth_mode);
			ret = -1;
		}
		cipher_type = ar6000_cipher(params->group_suite, &cipher_len);
		wpa_printf(MSG_DEBUG, "AR6000 - "
			   "IEEE80211_PARAM_MCASTCIPHER=%d", cipher_type);
		if (set80211param(drv, IEEE80211_PARAM_MCASTCIPHER,
				  cipher_type, 1) < 0) {
			wpa_printf(MSG_DEBUG, "AR6000 - Setting "
				   "IEEE80211_PARAM_MCASTCIPHER=%d",
				   cipher_type);
			ret = -1;
		}
		if (cipher_type == IEEE80211_CIPHER_WEP) {
			wpa_printf(MSG_DEBUG, "AR6000 - "
					"IEEE80211_PARAM_MCASTKEYLEN=%d",
					cipher_len);
			if (set80211param(drv, IEEE80211_PARAM_MCASTKEYLEN,
						cipher_len, 1) < 0) {
				wpa_printf(MSG_DEBUG, "AR6000 - Setting "
						"IEEE80211_PARAM_MCASTKEYLEN=%d",
						cipher_len);
				ret = -1;
			}
		}
		cipher_type = ar6000_cipher(params->pairwise_suite,
                                    &cipher_len);
		wpa_printf(MSG_DEBUG, "AR6000 - "
			   "IEEE80211_PARAM_UCASTCIPHER=%d", cipher_type);
		if (set80211param(drv, IEEE80211_PARAM_UCASTCIPHER,
				  cipher_type, 1) < 0) {
			wpa_printf(MSG_DEBUG, "AR6000 - Setting "
				   "IEEE80211_PARAM_UCASTCIPHER=%d",
				   cipher_type);
			ret = -1;
		}
		if (cipher_type == IEEE80211_CIPHER_WEP) {
			wpa_printf(MSG_DEBUG, "AR6000 - "
					"IEEE80211_PARAM_UCASTKEYLEN=%d",
					cipher_len);
			if (set80211param(drv, IEEE80211_PARAM_UCASTKEYLEN,
						cipher_len, 1) < 0) {
				wpa_printf(MSG_DEBUG, "AR6000 - Setting "
						"IEEE80211_PARAM_UCASTKEYLEN=%d",
						cipher_len);
				ret = -1;
			}
		}
		wpa_printf(MSG_DEBUG, "AR6000 - IEEE80211_PARAM_ROAMING=0");
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 0, 1) < 0) {
			wpa_printf(MSG_DEBUG, "AR6000 - Setting "
				   "IEEE80211_PARAM_ROAMING=0");
			ret = -1;
		}
	}
#ifdef EAP_WSC
}
else
{
	wpa_driver_madwifi_set_wsc_status(priv, WSC_REG_ACTIVE);
}
#endif
	if (wpa_driver_wext_set_ssid(drv->wext, params->ssid,
				     params->ssid_len) < 0) {
		wpa_printf(MSG_DEBUG, "FAILED IOCTL: wpa_driver_wext_set_ssid \n");
		ret = -1;
	}
	if (params->bssid) {
		wpa_printf(MSG_DEBUG, "AR6000 - IEEE80211_PARAM_ROAMING=2");
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0) {
			wpa_printf(MSG_DEBUG, "FAILED IOCTL: IEEE80211_PARAM_ROAMING \n");
			ret = -1;
		}
		memset(&mlme, 0, sizeof(mlme));
		mlme.im_op = IEEE80211_MLME_ASSOC;
		memcpy(mlme.im_macaddr, params->bssid, IEEE80211_ADDR_LEN);
		if (set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme,
				 sizeof(mlme), 1) < 0) {
			wpa_printf(MSG_DEBUG, "FAILED IOCTL: IEEE80211_IOCTL_SETMLME \n");
			ret = -1;
		}
	}
	return ret;
}

static int
wpa_driver_madwifi_scan(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

	/*
	 * Avoid set_ssid for scaning.
	 * if len is > 80H, (actual len's 8th bit is set, which means connect from WSC app
         * if it is lesser than that value, do only scan
         */
//	if(ssid_len > 0x80)
	{
		// take only 6 bits for ssid len
		if(wpa_driver_wext_set_ssid(drv->wext, ssid, (ssid_len)&(0x3F)) < 0)
			return -1;
	}

	if (ioctl(drv->sock, SIOCSIWSCAN, &iwr) < 0) {
		perror("ioctl[SIOCSIWSCAN]");
		return -1;
	}

	/*
	 * Add a timeout for scan just in case the driver does not report scan
	 * completion events.
	 */
	eloop_register_timeout(10, 0, wpa_driver_wext_scan_timeout, drv->wext,
			       drv->ctx);

	return 0;
}

static int wpa_driver_madwifi_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_madwifi_data *drv = priv;
	return wpa_driver_wext_get_bssid(drv->wext, bssid);
}


static int wpa_driver_madwifi_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_madwifi_data *drv = priv;
	return wpa_driver_wext_get_ssid(drv->wext, ssid);
}


static int wpa_driver_madwifi_get_scan_results(void *priv,
					    struct wpa_scan_result *results,
					    size_t max_size)
{
	struct wpa_driver_madwifi_data *drv = priv;
	return wpa_driver_wext_get_scan_results(drv->wext, results, max_size);
}

#ifdef WSC_NEW_IE
static int
wpa_driver_madwifi_set_wsc_probe_request_ie(void *priv, u8 *iebuf, int iebuflen)
{
	struct wpa_driver_madwifi_data *drv = priv;
	u8 buf[256];
	//WMI_SET_APPIE_CMD *pr_req_ie;
	struct ieee80211req_getset_appiebuf  *pr_req_ie;

	wpa_printf(MSG_DEBUG, "%s buflen = %d\n", __FUNCTION__, iebuflen);

	((int *)buf)[0] = AR6000_XIOCTL_WMI_SET_APPIE;
	//pr_req_ie = (WMI_SET_APPIE_CMD *) &buf[4];
	//pr_req_ie->mgmtFrmType = IEEE80211_APPIE_FRAME_PROBE_REQ;
	//pr_req_ie->ieLen = iebuflen;
	//memcpy(pr_req_ie->ieInfo, iebuf, iebuflen);
	pr_req_ie = (struct ieee80211req_getset_appiebuf *) &buf[4];
	pr_req_ie->app_frmtype = IEEE80211_APPIE_FRAME_PROBE_REQ;
	pr_req_ie->app_buflen = iebuflen;
	memcpy(&(pr_req_ie->app_buf[0]), iebuf, iebuflen);

	return set80211priv(drv, AR6000_IOCTL_EXTENDED, buf,
            2 + iebuflen + sizeof(int), 1);
}

static int
wpa_driver_madwifi_start_receive_beacons(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;
	u8 filt[16];

    	wpa_printf(MSG_DEBUG, "%s Enter\n", __FUNCTION__);

	((int *)filt)[0] = AR6000_XIOCTL_WMI_SET_MGMT_FRM_RX_FILTER;
	((int *)filt)[1] = IEEE80211_FILTER_TYPE_BEACON;

	return set80211priv(drv, AR6000_IOCTL_EXTENDED, filt, 8, 1);
}

static int
wpa_driver_madwifi_stop_receive_beacons(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;
	u8 filt[16];

    	wpa_printf(MSG_DEBUG, "%s Enter\n", __FUNCTION__);

	((int *)filt)[0] = AR6000_XIOCTL_WMI_SET_MGMT_FRM_RX_FILTER;
	((int *)filt)[1] = 0;

	//return 0; //RAM TODO
    	return set80211priv(drv, AR6000_IOCTL_EXTENDED, filt, 8, 1);
}

static int
wpa_driver_madwifi_process_frame(void *driver, void *ctx, const unsigned char *src_addr, const unsigned char *buf, size_t len, unsigned char *newbuf, int *newlen, u8 *frameType)
{
	static int j;
	u8 * frm;
	u8 * endfrm;
	char ssid[32];
	u8 bssid[6];
	u8 * tmpptr;
	u8 subtype;
	WMI_BSS_INFO_HDR *biHdr;

	wpa_printf(MSG_DEBUG, "Received %d bytes; beacon or pr-resp number %d\n", len, j++);

	biHdr = (WMI_BSS_INFO_HDR *) buf;

	wpa_printf(MSG_DEBUG, " %02X:%02X:%02X:%02X:%02X:%02X \n" , biHdr->bssid[0],
		biHdr->bssid[1], biHdr->bssid[2], biHdr->bssid[3], biHdr->bssid[4], biHdr->bssid[5]);

	frm = ((u8 *) buf + sizeof(WMI_BSS_INFO_HDR));
	endfrm = (u8 *) (buf + len - sizeof(WMI_BSS_INFO_HDR));

	subtype = biHdr->frameType;
	if (subtype == BEACON_FTYPE)
		*frameType = 1;
	else if (subtype == PROBERESP_FTYPE)
		*frameType = 3;
	else
		*frameType = 0;

	wpa_printf(MSG_DEBUG, "frameType = %d\n", *frameType);
	memcpy(bssid, biHdr->bssid, IEEE80211_ADDR_LEN);

	// skip timestamp(8), beac interval(2), cap info (2)
	frm += 12;
	// get ssid
	while (frm < endfrm)
	{
		switch (*frm)
		{
		case IEEE80211_ELEMID_SSID:
			if (frm[1] > 1)
			{
				memset(ssid, 0, 32);
				memcpy(ssid, &frm[2], frm[1]);
				wpa_printf(MSG_DEBUG, "ssid: %s\n", &frm[2]);

			}
			else
			{
				wpa_printf(MSG_DEBUG, "ssid: <hidden>\n");
			}

			break;
		case IEEE80211_ELEMID_VENDOR:
			// check for WFA OUI
			if (frm[1] > 4 && frm[2] == 0x00 && frm[3] == 0x50 && frm[4] == 0xF2 && frm[5]==0x04)
			{
				tmpptr = newbuf;
				memcpy(tmpptr, ssid, 32);
				tmpptr += 32;
				memcpy(tmpptr, bssid, 6);
				tmpptr += 6;
				memcpy(tmpptr, &frm[6], frm[1] - 4);
				tmpptr += (frm[1] - 4);
				*newlen = (int) (tmpptr - newbuf);
				wpa_printf(MSG_DEBUG, "Got WSC IE; ssid = %s, IE len = %d\n", ssid, frm[1]);
			}
		default:
			break;
		} // switch

		frm += frm[1] + 2;
	} // while
	return 0;
}

static int
wpa_driver_madwifi_start_receive_pr_resps(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;
	u8 filt[16];

    	wpa_printf(MSG_DEBUG, "%s Enter\n", __FUNCTION__);

	((int *)filt)[0] = AR6000_XIOCTL_WMI_SET_MGMT_FRM_RX_FILTER;
	((int *)filt)[1] = IEEE80211_FILTER_TYPE_PROBE_RESP;

	return set80211priv(drv, AR6000_IOCTL_EXTENDED, filt, 8, 1);
}

static int
wpa_driver_madwifi_stop_receive_pr_resps(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;
	u8 filt[16];

    	wpa_printf(MSG_DEBUG, "%s Enter\n", __FUNCTION__);

	((int *)filt)[0] = AR6000_XIOCTL_WMI_SET_MGMT_FRM_RX_FILTER;
	((int *)filt)[1] = 0;

	//return 0; // RAM TODO
    	return set80211priv(drv, AR6000_IOCTL_EXTENDED, filt, 8, 1);
}

static int
wpa_driver_madwifi_set_wsc_status(void *priv, int status)
{
	struct wpa_driver_madwifi_data *drv = priv;
	u8 filt[16];

    wpa_printf(MSG_DEBUG, "%s Enter\n", __FUNCTION__);

	((int *)filt)[0] = AR6000_XIOCTL_WMI_SET_WSC_STATUS;
	((int *)filt)[1] = status;

    	return set80211priv(drv, AR6000_IOCTL_EXTENDED, filt, 8, 1);
}

#endif /* WSC_NEW_IE */

static void * wpa_driver_madwifi_init(void *ctx, const char *ifname)
{
	struct wpa_driver_madwifi_data *drv;

	drv = malloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	memset(drv, 0, sizeof(*drv));
	drv->wext = wpa_driver_wext_init(ctx, ifname);
	if (drv->wext == NULL)
		goto fail;

	drv->ctx = ctx;
	strncpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0)
		goto fail2;

	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to set wpa_supplicant-based "
			   "roaming", __FUNCTION__);
		goto fail3;
	}

	if (set80211param(drv, IEEE80211_PARAM_WPA, 3, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable WPA support",
			   __FUNCTION__);
		goto fail3;
	}

	return drv;

fail3:
	close(drv->sock);
fail2:
	wpa_driver_wext_deinit(drv->wext);
fail:
	free(drv);
	return NULL;
}


static void wpa_driver_madwifi_deinit(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;

	if (wpa_driver_madwifi_set_wpa_ie(drv, NULL, 0) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to clear WPA IE",
			   __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable driver-based "
			   "roaming", __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_PRIVACY, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to disable forced Privacy "
			   "flag", __FUNCTION__);
	}
	if (set80211param(drv, IEEE80211_PARAM_WPA, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to disable WPA",
			   __FUNCTION__);
	}

	wpa_driver_wext_deinit(drv->wext);

	close(drv->sock);
	free(drv);
}

static int wpa_driver_madwifi_set_auth_alg(void *priv, int auth_alg)
{
   struct ifreq ifr;
   char buf[256];
   struct ieee80211req_authalg *alg;
   struct wpa_driver_madwifi_data *drv = priv;
   int cmd = IEEE80211_IOCTL_SETAUTHALG, res;

   memcpy(buf, &cmd, sizeof(cmd));
   alg = (struct ieee80211req_authalg *) (buf + sizeof(cmd));
   alg->auth_alg = auth_alg;
   strncpy(ifr.ifr_name, drv->ifname, sizeof(ifr.ifr_name));
   ifr.ifr_data = buf;
   res = ioctl(drv->sock, AR6000_IOCTL_EXTENDED, &ifr);
   if (res < 0) {
       wpa_printf(MSG_DEBUG, "%s: AR6000_IOCTL_EXTENDED[SETAUTHALG] "
              "failed: %s", __FUNCTION__, strerror(errno));
       return -1;
   }

   return 0;
}

#ifdef WSC_NEW_IE
int wpa_driver_madwifi_init_l2_packet(void *priv, void (*handler)(void *ctx, const unsigned char *src_addr, const unsigned char *buf, size_t len))
{
	struct wpa_driver_madwifi_data *drv = priv;
	u8 own_addr[ETH_ALEN + 1];

	drv->l2_sock = l2_packet_init(drv->ifname, NULL, 0x0019/*ETH_P_EAPOL*/,
				handler, drv, 1);
	if (drv->l2_sock == NULL)
	{
		wpa_printf(MSG_ERROR, "l2_packet_init failed\n");
		return -1;
	}

	if (l2_packet_get_own_addr(drv->l2_sock, own_addr))
		return -1;

	wpa_printf(MSG_INFO, "l2_packet_init Successful\n");
	return 0;
}

int wpa_driver_madwifi_deinit_l2_packet(void *priv)
{
	struct wpa_driver_madwifi_data *drv = priv;

	l2_packet_deinit(drv->l2_sock);

	return 0;
}
#endif /*  WSC_NEW_IE */

static int wpa_driver_madwifi_add_pmkid(void *priv, const u8 *bssid,
					const u8 *pmkid)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_addpmkid cmd;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

	memcpy(cmd.pi_bssid, bssid, IEEE80211_ADDR_LEN);
	memcpy(cmd.pi_pmkid, pmkid, sizeof(cmd.pi_pmkid));
	cmd.pi_enable = 1;

	return set80211priv(drv, IEEE80211_IOCTL_ADDPMKID, &cmd, sizeof(cmd),
			    1);
}


static int wpa_driver_madwifi_remove_pmkid(void *priv, const u8 *bssid,
					   const u8 *pmkid)
{
	struct wpa_driver_madwifi_data *drv = priv;
	struct ieee80211req_addpmkid cmd;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

	memcpy(cmd.pi_bssid, bssid, IEEE80211_ADDR_LEN);
	cmd.pi_enable = 0;

	return set80211priv(drv, IEEE80211_IOCTL_ADDPMKID, &cmd, sizeof(cmd),
			    1);
}


struct wpa_driver_ops wpa_driver_ar6000_ops = {
	.name			= "ar6000",
	.desc			= "Atheros AR6000 support",
	.get_bssid		= wpa_driver_madwifi_get_bssid,
	.get_ssid		= wpa_driver_madwifi_get_ssid,
	.set_key		= wpa_driver_madwifi_set_key,
	.init			= wpa_driver_madwifi_init,
	.deinit			= wpa_driver_madwifi_deinit,
	.set_countermeasures	= wpa_driver_madwifi_set_countermeasures,
	.set_drop_unencrypted	= wpa_driver_madwifi_set_drop_unencrypted,
	.scan			= wpa_driver_madwifi_scan,
	.get_scan_results	= wpa_driver_madwifi_get_scan_results,
	.deauthenticate		= wpa_driver_madwifi_deauthenticate,
	.disassociate		= wpa_driver_madwifi_disassociate,
	.associate		= wpa_driver_madwifi_associate,
	.set_auth_alg   = wpa_driver_madwifi_set_auth_alg,
	.add_pmkid		= wpa_driver_madwifi_add_pmkid,
	.remove_pmkid		= wpa_driver_madwifi_remove_pmkid,
#ifdef WSC_NEW_IE
	.set_wsc_probe_request_ie= wpa_driver_madwifi_set_wsc_probe_request_ie,
	.start_receive_beacons 	= wpa_driver_madwifi_start_receive_beacons,
	.stop_receive_beacons 	= wpa_driver_madwifi_stop_receive_beacons,
	.init_l2_packet 	= wpa_driver_madwifi_init_l2_packet,
	.deinit_l2_packet 	= wpa_driver_madwifi_deinit_l2_packet,
	.process_frame 		= wpa_driver_madwifi_process_frame,
	.start_receive_pr_resps = wpa_driver_madwifi_start_receive_pr_resps,
	.stop_receive_pr_resps 	= wpa_driver_madwifi_stop_receive_pr_resps,
#endif

};
