/*
	Copyright (C) 2004 - 2008 rt2x00 SourceForge Project
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	Module: rt2x00lib
	Abstract: rt2x00 generic link tuning routines.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "rt2x00.h"
#include "rt2x00lib.h"

static int rt2x00link_antenna_get_link_rssi(struct rt2x00_dev *rt2x00dev)
{
	struct link_ant *ant = &rt2x00dev->link.ant;

	if (ant->rssi_ant && rt2x00dev->link.qual.rx_success)
		return ant->rssi_ant;
	return DEFAULT_RSSI;
}

static int rt2x00link_antenna_get_rssi_history(struct rt2x00_dev *rt2x00dev,
					       enum antenna antenna)
{
	struct link_ant *ant = &rt2x00dev->link.ant;

	if (ant->rssi_history[antenna - ANTENNA_A])
		return ant->rssi_history[antenna - ANTENNA_A];
	return DEFAULT_RSSI;
}
/* Small wrapper for rt2x00link_antenna_get_rssi_history() */
#define rt2x00link_antenna_get_rssi_rx_history(__dev) \
	rt2x00link_antenna_get_rssi_history((__dev), \
					    (__dev)->link.ant.active.rx)
#define rt2x00link_antenna_get_rssi_tx_history(__dev) \
	rt2x00link_antenna_get_rssi_history((__dev), \
					    (__dev)->link.ant.active.tx)

static void rt2x00link_antenna_update_rssi_history(struct rt2x00_dev *rt2x00dev,
						   enum antenna antenna,
						   int rssi)
{
	struct link_ant *ant = &rt2x00dev->link.ant;
	ant->rssi_history[ant->active.rx - ANTENNA_A] = rssi;
}
/* Small wrapper for rt2x00link_antenna_get_rssi_history() */
#define rt2x00link_antenna_update_rssi_rx_history(__dev, __rssi) \
	rt2x00link_antenna_update_rssi_history((__dev), \
					       (__dev)->link.ant.active.rx, \
					       (__rssi))
#define rt2x00link_antenna_update_rssi_tx_history(__dev, __rssi) \
	rt2x00link_antenna_update_rssi_history((__dev), \
					       (__dev)->link.ant.active.tx, \
					       (__rssi))

static void rt2x00link_antenna_reset(struct rt2x00_dev *rt2x00dev)
{
	rt2x00dev->link.ant.rssi_ant = 0;
}

static void rt2x00lib_antenna_diversity_sample(struct rt2x00_dev *rt2x00dev)
{
	struct link_ant *ant = &rt2x00dev->link.ant;
	struct antenna_setup new_ant;
	int sample_a = rt2x00link_antenna_get_rssi_history(rt2x00dev, ANTENNA_A);
	int sample_b = rt2x00link_antenna_get_rssi_history(rt2x00dev, ANTENNA_B);

	memcpy(&new_ant, &ant->active, sizeof(new_ant));

	/*
	 * We are done sampling. Now we should evaluate the results.
	 */
	ant->flags &= ~ANTENNA_MODE_SAMPLE;

	/*
	 * During the last period we have sampled the RSSI
	 * from both antenna's. It now is time to determine
	 * which antenna demonstrated the best performance.
	 * When we are already on the antenna with the best
	 * performance, then there really is nothing for us
	 * left to do.
	 */
	if (sample_a == sample_b)
		return;

	if (ant->flags & ANTENNA_RX_DIVERSITY)
		new_ant.rx = (sample_a > sample_b) ? ANTENNA_A : ANTENNA_B;

	if (ant->flags & ANTENNA_TX_DIVERSITY)
		new_ant.tx = (sample_a > sample_b) ? ANTENNA_A : ANTENNA_B;

	rt2x00lib_config_antenna(rt2x00dev, &new_ant);
}

static void rt2x00lib_antenna_diversity_eval(struct rt2x00_dev *rt2x00dev)
{
	struct link_ant *ant = &rt2x00dev->link.ant;
	struct antenna_setup new_ant;
	int rssi_curr;
	int rssi_old;

	memcpy(&new_ant, &ant->active, sizeof(new_ant));

	/*
	 * Get current RSSI value along with the historical value,
	 * after that update the history with the current value.
	 */
	rssi_curr = rt2x00link_antenna_get_link_rssi(rt2x00dev);
	rssi_old = rt2x00link_antenna_get_rssi_rx_history(rt2x00dev);
	rt2x00link_antenna_update_rssi_rx_history(rt2x00dev, rssi_curr);

	/*
	 * Legacy driver indicates that we should swap antenna's
	 * when the difference in RSSI is greater that 5. This
	 * also should be done when the RSSI was actually better
	 * then the previous sample.
	 * When the difference exceeds the threshold we should
	 * sample the rssi from the other antenna to make a valid
	 * comparison between the 2 antennas.
	 */
	if (abs(rssi_curr - rssi_old) < 5)
		return;

	ant->flags |= ANTENNA_MODE_SAMPLE;

	if (ant->flags & ANTENNA_RX_DIVERSITY)
		new_ant.rx = (new_ant.rx == ANTENNA_A) ? ANTENNA_B : ANTENNA_A;

	if (ant->flags & ANTENNA_TX_DIVERSITY)
		new_ant.tx = (new_ant.tx == ANTENNA_A) ? ANTENNA_B : ANTENNA_A;

	rt2x00lib_config_antenna(rt2x00dev, &new_ant);
}

static void rt2x00lib_antenna_diversity(struct rt2x00_dev *rt2x00dev)
{
	struct link_ant *ant = &rt2x00dev->link.ant;

	/*
	 * Determine if software diversity is enabled for
	 * either the TX or RX antenna (or both).
	 * Always perform this check since within the link
	 * tuner interval the configuration might have changed.
	 */
	ant->flags &= ~ANTENNA_RX_DIVERSITY;
	ant->flags &= ~ANTENNA_TX_DIVERSITY;

	if (rt2x00dev->default_ant.rx == ANTENNA_SW_DIVERSITY)
		ant->flags |= ANTENNA_RX_DIVERSITY;
	if (rt2x00dev->default_ant.tx == ANTENNA_SW_DIVERSITY)
		ant->flags |= ANTENNA_TX_DIVERSITY;

	if (!(ant->flags & ANTENNA_RX_DIVERSITY) &&
	    !(ant->flags & ANTENNA_TX_DIVERSITY)) {
		ant->flags = 0;
		return;
	}

	/*
	 * If we have only sampled the data over the last period
	 * we should now harvest the data. Otherwise just evaluate
	 * the data. The latter should only be performed once
	 * every 2 seconds.
	 */
	if (ant->flags & ANTENNA_MODE_SAMPLE)
		rt2x00lib_antenna_diversity_sample(rt2x00dev);
	else if (rt2x00dev->link.count & 1)
		rt2x00lib_antenna_diversity_eval(rt2x00dev);
}

void rt2x00link_update_stats(struct rt2x00_dev *rt2x00dev,
			     struct sk_buff *skb,
			     struct rxdone_entry_desc *rxdesc)
{
	struct link_qual *qual = &rt2x00dev->link.qual;
	struct link_ant *ant = &rt2x00dev->link.ant;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	int avg_rssi = rxdesc->rssi;
	int ant_rssi = rxdesc->rssi;

	/*
	 * Frame was received successfully since non-succesfull
	 * frames would have been dropped by the hardware.
	 */
	qual->rx_success++;

	/*
	 * We are only interested in quality statistics from
	 * beacons which came from the BSS which we are
	 * associated with.
	 */
	if (!ieee80211_is_beacon(hdr->frame_control) ||
	    !(rxdesc->dev_flags & RXDONE_MY_BSS))
		return;

	/*
	 * Update global RSSI
	 */
	if (qual->avg_rssi)
		avg_rssi = MOVING_AVERAGE(qual->avg_rssi, rxdesc->rssi, 8);
	qual->avg_rssi = avg_rssi;

	/*
	 * Update antenna RSSI
	 */
	if (ant->rssi_ant)
		ant_rssi = MOVING_AVERAGE(ant->rssi_ant, rxdesc->rssi, 8);
	ant->rssi_ant = ant_rssi;
}

static void rt2x00link_precalculate_signal(struct rt2x00_dev *rt2x00dev)
{
	struct link_qual *qual = &rt2x00dev->link.qual;

	if (qual->rx_failed || qual->rx_success)
		qual->rx_percentage =
		    (qual->rx_success * 100) /
		    (qual->rx_failed + qual->rx_success);
	else
		qual->rx_percentage = 50;

	if (qual->tx_failed || qual->tx_success)
		qual->tx_percentage =
		    (qual->tx_success * 100) /
		    (qual->tx_failed + qual->tx_success);
	else
		qual->tx_percentage = 50;

	qual->rx_success = 0;
	qual->rx_failed = 0;
	qual->tx_success = 0;
	qual->tx_failed = 0;
}

int rt2x00link_calculate_signal(struct rt2x00_dev *rt2x00dev, int rssi)
{
	struct link_qual *qual = &rt2x00dev->link.qual;
	int rssi_percentage = 0;
	int signal;

	/*
	 * We need a positive value for the RSSI.
	 */
	if (rssi < 0)
		rssi += rt2x00dev->rssi_offset;

	/*
	 * Calculate the different percentages,
	 * which will be used for the signal.
	 */
	rssi_percentage = (rssi * 100) / rt2x00dev->rssi_offset;

	/*
	 * Add the individual percentages and use the WEIGHT
	 * defines to calculate the current link signal.
	 */
	signal = ((WEIGHT_RSSI * rssi_percentage) +
		  (WEIGHT_TX * qual->tx_percentage) +
		  (WEIGHT_RX * qual->rx_percentage)) / 100;

	return max_t(int, signal, 100);
}

void rt2x00link_start_tuner(struct rt2x00_dev *rt2x00dev)
{
	struct link_qual *qual = &rt2x00dev->link.qual;

	/*
	 * Link tuning should only be performed when
	 * an active sta or master interface exists.
	 * Single monitor mode interfaces should never have
	 * work with link tuners.
	 */
	if (!rt2x00dev->intf_ap_count && !rt2x00dev->intf_sta_count)
		return;

	/*
	 * Clear all (possibly) pre-existing quality statistics.
	 */
	memset(qual, 0, sizeof(*qual));

	/*
	 * The RX and TX percentage should start at 50%
	 * this will assure we will get at least get some
	 * decent value when the link tuner starts.
	 * The value will be dropped and overwritten with
	 * the correct (measured) value anyway during the
	 * first run of the link tuner.
	 */
	qual->rx_percentage = 50;
	qual->tx_percentage = 50;

	rt2x00link_reset_tuner(rt2x00dev, false);

	queue_delayed_work(rt2x00dev->hw->workqueue,
			   &rt2x00dev->link.work, LINK_TUNE_INTERVAL);
}

void rt2x00link_stop_tuner(struct rt2x00_dev *rt2x00dev)
{
	cancel_delayed_work_sync(&rt2x00dev->link.work);
}

void rt2x00link_reset_tuner(struct rt2x00_dev *rt2x00dev, bool antenna)
{
	if (!test_bit(DEVICE_STATE_ENABLED_RADIO, &rt2x00dev->flags))
		return;

	/*
	 * Reset link information.
	 * Both the currently active vgc level as well as
	 * the link tuner counter should be reset. Resetting
	 * the counter is important for devices where the
	 * device should only perform link tuning during the
	 * first minute after being enabled.
	 */
	rt2x00dev->link.count = 0;
	rt2x00dev->link.vgc_level = 0;

	/*
	 * Reset the link tuner.
	 */
	rt2x00dev->ops->lib->reset_tuner(rt2x00dev);

	if (antenna)
		rt2x00link_antenna_reset(rt2x00dev);
}

static void rt2x00link_tuner(struct work_struct *work)
{
	struct rt2x00_dev *rt2x00dev =
	    container_of(work, struct rt2x00_dev, link.work.work);
	struct link_qual *qual = &rt2x00dev->link.qual;

	/*
	 * When the radio is shutting down we should
	 * immediately cease all link tuning.
	 */
	if (!test_bit(DEVICE_STATE_ENABLED_RADIO, &rt2x00dev->flags))
		return;

	/*
	 * Update statistics.
	 */
	rt2x00dev->ops->lib->link_stats(rt2x00dev, qual);
	rt2x00dev->low_level_stats.dot11FCSErrorCount += qual->rx_failed;

	/*
	 * Only perform the link tuning when Link tuning
	 * has been enabled (This could have been disabled from the EEPROM).
	 */
	if (!test_bit(CONFIG_DISABLE_LINK_TUNING, &rt2x00dev->flags))
		rt2x00dev->ops->lib->link_tuner(rt2x00dev);

	/*
	 * Precalculate a portion of the link signal which is
	 * in based on the tx/rx success/failure counters.
	 */
	rt2x00link_precalculate_signal(rt2x00dev);

	/*
	 * Send a signal to the led to update the led signal strength.
	 */
	rt2x00leds_led_quality(rt2x00dev, qual->avg_rssi);

	/*
	 * Evaluate antenna setup, make this the last step since this could
	 * possibly reset some statistics.
	 */
	rt2x00lib_antenna_diversity(rt2x00dev);

	/*
	 * Increase tuner counter, and reschedule the next link tuner run.
	 */
	rt2x00dev->link.count++;
	queue_delayed_work(rt2x00dev->hw->workqueue,
			   &rt2x00dev->link.work, LINK_TUNE_INTERVAL);
}

void rt2x00link_register(struct rt2x00_dev *rt2x00dev)
{
	INIT_DELAYED_WORK(&rt2x00dev->link.work, rt2x00link_tuner);
}
