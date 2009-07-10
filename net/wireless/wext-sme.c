/*
 * cfg80211 wext compat for managed mode.
 *
 * Copyright 2009	Johannes Berg <johannes@sipsolutions.net>
 * Copyright (C) 2009   Intel Corporation. All rights reserved.
 */

#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <net/cfg80211.h>
#include "nl80211.h"

static int cfg80211_mgd_wext_connect(struct cfg80211_registered_device *rdev,
				     struct wireless_dev *wdev)
{
	int err;

	ASSERT_RDEV_LOCK(rdev);
	ASSERT_WDEV_LOCK(wdev);

	if (!netif_running(wdev->netdev))
		return 0;

	wdev->wext.connect.ie = wdev->wext.ie;
	wdev->wext.connect.ie_len = wdev->wext.ie_len;
	wdev->wext.connect.privacy = wdev->wext.default_key != -1;

	err = 0;
	if (wdev->wext.connect.ssid_len != 0)
		err = __cfg80211_connect(rdev, wdev->netdev,
					 &wdev->wext.connect);

	return err;
}

int cfg80211_mgd_wext_siwfreq(struct net_device *dev,
			      struct iw_request_info *info,
			      struct iw_freq *freq, char *extra)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_dev(wdev->wiphy);
	struct ieee80211_channel *chan;
	int err;

	/* call only for station! */
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION))
		return -EINVAL;

	chan = cfg80211_wext_freq(wdev->wiphy, freq);
	if (chan && IS_ERR(chan))
		return PTR_ERR(chan);

	if (chan && (chan->flags & IEEE80211_CHAN_DISABLED))
		return -EINVAL;

	cfg80211_lock_rdev(rdev);
	wdev_lock(wdev);

	if (wdev->wext.connect.channel == chan) {
		err = 0;
		goto out;
	}

	if (wdev->sme_state != CFG80211_SME_IDLE) {
		bool event = true;
		/* if SSID set, we'll try right again, avoid event */
		if (wdev->wext.connect.ssid_len)
			event = false;
		err = __cfg80211_disconnect(wiphy_to_dev(wdev->wiphy),
					    dev, WLAN_REASON_DEAUTH_LEAVING,
					    event);
		if (err)
			goto out;
	}


	wdev->wext.connect.channel = chan;

	/* SSID is not set, we just want to switch channel */
	if (wdev->wext.connect.ssid_len && chan) {
		err = -EOPNOTSUPP;
		if (rdev->ops->set_channel)
			err = rdev->ops->set_channel(wdev->wiphy, chan,
						     NL80211_CHAN_NO_HT);
		goto out;
	}

	err = cfg80211_mgd_wext_connect(wiphy_to_dev(wdev->wiphy), wdev);
 out:
	wdev_unlock(wdev);
	cfg80211_unlock_rdev(rdev);
	return err;
}
/* temporary symbol - mark GPL - in the future the handler won't be */
EXPORT_SYMBOL_GPL(cfg80211_mgd_wext_siwfreq);

int cfg80211_mgd_wext_giwfreq(struct net_device *dev,
			      struct iw_request_info *info,
			      struct iw_freq *freq, char *extra)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct ieee80211_channel *chan = NULL;

	/* call only for station! */
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION))
		return -EINVAL;

	wdev_lock(wdev);
	if (wdev->current_bss)
		chan = wdev->current_bss->pub.channel;
	else if (wdev->wext.connect.channel)
		chan = wdev->wext.connect.channel;
	wdev_unlock(wdev);

	if (chan) {
		freq->m = chan->center_freq;
		freq->e = 6;
		return 0;
	}

	/* no channel if not joining */
	return -EINVAL;
}
/* temporary symbol - mark GPL - in the future the handler won't be */
EXPORT_SYMBOL_GPL(cfg80211_mgd_wext_giwfreq);

int cfg80211_mgd_wext_siwessid(struct net_device *dev,
			       struct iw_request_info *info,
			       struct iw_point *data, char *ssid)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	size_t len = data->length;
	int err;

	/* call only for station! */
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION))
		return -EINVAL;

	if (!data->flags)
		len = 0;

	/* iwconfig uses nul termination in SSID.. */
	if (len > 0 && ssid[len - 1] == '\0')
		len--;

	cfg80211_lock_rdev(wiphy_to_dev(wdev->wiphy));
	wdev_lock(wdev);

	err = 0;

	if (wdev->wext.connect.ssid && len &&
	    len == wdev->wext.connect.ssid_len &&
	    memcmp(wdev->wext.connect.ssid, ssid, len))
		goto out;

	if (wdev->sme_state != CFG80211_SME_IDLE) {
		bool event = true;
		/* if SSID set now, we'll try to connect, avoid event */
		if (len)
			event = false;
		err = __cfg80211_disconnect(wiphy_to_dev(wdev->wiphy),
					    dev, WLAN_REASON_DEAUTH_LEAVING,
					    event);
		if (err)
			goto out;
	}

	wdev->wext.connect.ssid = wdev->wext.ssid;
	memcpy(wdev->wext.ssid, ssid, len);
	wdev->wext.connect.ssid_len = len;

	wdev->wext.connect.crypto.control_port = false;

	err = cfg80211_mgd_wext_connect(wiphy_to_dev(wdev->wiphy), wdev);
 out:
	wdev_unlock(wdev);
	cfg80211_unlock_rdev(wiphy_to_dev(wdev->wiphy));
	return err;
}
/* temporary symbol - mark GPL - in the future the handler won't be */
EXPORT_SYMBOL_GPL(cfg80211_mgd_wext_siwessid);

int cfg80211_mgd_wext_giwessid(struct net_device *dev,
			       struct iw_request_info *info,
			       struct iw_point *data, char *ssid)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	/* call only for station! */
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION))
		return -EINVAL;

	data->flags = 0;

	wdev_lock(wdev);
	if (wdev->ssid_len) {
		data->flags = 1;
		data->length = wdev->ssid_len;
		memcpy(ssid, wdev->ssid, data->length);
	} else if (wdev->wext.connect.ssid && wdev->wext.connect.ssid_len) {
		data->flags = 1;
		data->length = wdev->wext.connect.ssid_len;
		memcpy(ssid, wdev->wext.connect.ssid, data->length);
	} else
		data->flags = 0;
	wdev_unlock(wdev);

	return 0;
}
/* temporary symbol - mark GPL - in the future the handler won't be */
EXPORT_SYMBOL_GPL(cfg80211_mgd_wext_giwessid);

int cfg80211_mgd_wext_siwap(struct net_device *dev,
			    struct iw_request_info *info,
			    struct sockaddr *ap_addr, char *extra)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	u8 *bssid = ap_addr->sa_data;
	int err;

	/* call only for station! */
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION))
		return -EINVAL;

	if (ap_addr->sa_family != ARPHRD_ETHER)
		return -EINVAL;

	/* automatic mode */
	if (is_zero_ether_addr(bssid) || is_broadcast_ether_addr(bssid))
		bssid = NULL;

	cfg80211_lock_rdev(wiphy_to_dev(wdev->wiphy));
	wdev_lock(wdev);

	err = 0;
	/* both automatic */
	if (!bssid && !wdev->wext.connect.bssid)
		goto out;

	/* fixed already - and no change */
	if (wdev->wext.connect.bssid && bssid &&
	    compare_ether_addr(bssid, wdev->wext.connect.bssid) == 0)
		goto out;

	if (wdev->sme_state != CFG80211_SME_IDLE) {
		err = __cfg80211_disconnect(wiphy_to_dev(wdev->wiphy),
					    dev, WLAN_REASON_DEAUTH_LEAVING,
					    false);
		if (err)
			goto out;
	}

	if (bssid) {
		memcpy(wdev->wext.bssid, bssid, ETH_ALEN);
		wdev->wext.connect.bssid = wdev->wext.bssid;
	} else
		wdev->wext.connect.bssid = NULL;

	err = cfg80211_mgd_wext_connect(wiphy_to_dev(wdev->wiphy), wdev);
 out:
	wdev_unlock(wdev);
	cfg80211_unlock_rdev(wiphy_to_dev(wdev->wiphy));
	return err;
}
/* temporary symbol - mark GPL - in the future the handler won't be */
EXPORT_SYMBOL_GPL(cfg80211_mgd_wext_siwap);

int cfg80211_mgd_wext_giwap(struct net_device *dev,
			    struct iw_request_info *info,
			    struct sockaddr *ap_addr, char *extra)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	/* call only for station! */
	if (WARN_ON(wdev->iftype != NL80211_IFTYPE_STATION))
		return -EINVAL;

	ap_addr->sa_family = ARPHRD_ETHER;

	wdev_lock(wdev);
	if (wdev->current_bss)
		memcpy(ap_addr->sa_data, wdev->current_bss->pub.bssid, ETH_ALEN);
	else if (wdev->wext.connect.bssid)
		memcpy(ap_addr->sa_data, wdev->wext.connect.bssid, ETH_ALEN);
	else
		memset(ap_addr->sa_data, 0, ETH_ALEN);
	wdev_unlock(wdev);

	return 0;
}
/* temporary symbol - mark GPL - in the future the handler won't be */
EXPORT_SYMBOL_GPL(cfg80211_mgd_wext_giwap);

int cfg80211_wext_siwgenie(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *data, char *extra)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_dev(wdev->wiphy);
	u8 *ie = extra;
	int ie_len = data->length, err;

	if (wdev->iftype != NL80211_IFTYPE_STATION)
		return -EOPNOTSUPP;

	if (!ie_len)
		ie = NULL;

	wdev_lock(wdev);

	/* no change */
	err = 0;
	if (wdev->wext.ie_len == ie_len &&
	    memcmp(wdev->wext.ie, ie, ie_len) == 0)
		goto out;

	if (ie_len) {
		ie = kmemdup(extra, ie_len, GFP_KERNEL);
		if (!ie) {
			err = -ENOMEM;
			goto out;
		}
	} else
		ie = NULL;

	kfree(wdev->wext.ie);
	wdev->wext.ie = ie;
	wdev->wext.ie_len = ie_len;

	if (wdev->sme_state != CFG80211_SME_IDLE) {
		err = __cfg80211_disconnect(rdev, dev,
					    WLAN_REASON_DEAUTH_LEAVING, false);
		if (err)
			goto out;
	}

	/* userspace better not think we'll reconnect */
	err = 0;
 out:
	wdev_unlock(wdev);
	return err;
}
EXPORT_SYMBOL_GPL(cfg80211_wext_siwgenie);

int cfg80211_wext_siwmlme(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *data, char *extra)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct iw_mlme *mlme = (struct iw_mlme *)extra;
	struct cfg80211_registered_device *rdev;
	int err;

	if (!wdev)
		return -EOPNOTSUPP;

	rdev = wiphy_to_dev(wdev->wiphy);

	if (wdev->iftype != NL80211_IFTYPE_STATION)
		return -EINVAL;

	if (mlme->addr.sa_family != ARPHRD_ETHER)
		return -EINVAL;

	wdev_lock(wdev);
	switch (mlme->cmd) {
	case IW_MLME_DEAUTH:
	case IW_MLME_DISASSOC:
		err = __cfg80211_disconnect(rdev, dev, mlme->reason_code,
					    true);
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}
	wdev_unlock(wdev);

	return err;
}
EXPORT_SYMBOL_GPL(cfg80211_wext_siwmlme);
