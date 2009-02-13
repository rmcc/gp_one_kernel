/*
 * PHY functions
 *
 * Copyright (c) 2004-2007 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2009 Nick Kossifidis <mickflemm@gmail.com>
 * Copyright (c) 2007-2008 Jiri Slaby <jirislaby@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#define _ATH5K_PHY

#include <linux/delay.h>

#include "ath5k.h"
#include "reg.h"
#include "base.h"
#include "rfbuffer.h"
#include "rfgain.h"

/*
 * Used to modify RF Banks before writing them to AR5K_RF_BUFFER
 */
static unsigned int ath5k_hw_rfregs_op(u32 *rf, u32 offset, u32 reg, u32 bits,
		u32 first, u32 col, bool set)
{
	u32 mask, entry, last, data, shift, position;
	s32 left;
	int i;

	data = 0;

	if (rf == NULL)
		/* should not happen */
		return 0;

	if (!(col <= 3 && bits <= 32 && first + bits <= 319)) {
		ATH5K_PRINTF("invalid values at offset %u\n", offset);
		return 0;
	}

	entry = ((first - 1) / 8) + offset;
	position = (first - 1) % 8;

	if (set)
		data = ath5k_hw_bitswap(reg, bits);

	for (i = shift = 0, left = bits; left > 0; position = 0, entry++, i++) {
		last = (position + left > 8) ? 8 : position + left;
		mask = (((1 << last) - 1) ^ ((1 << position) - 1)) << (col * 8);

		if (set) {
			rf[entry] &= ~mask;
			rf[entry] |= ((data << position) << (col * 8)) & mask;
			data >>= (8 - position);
		} else {
			data = (((rf[entry] & mask) >> (col * 8)) >> position)
				<< shift;
			shift += last - position;
		}

		left -= 8 - position;
	}

	data = set ? 1 : ath5k_hw_bitswap(data, bits);

	return data;
}

static u32 ath5k_hw_rfregs_gainf_corr(struct ath5k_hw *ah)
{
	u32 mix, step;
	u32 *rf;

	if (ah->ah_rf_banks == NULL)
		return 0;

	rf = ah->ah_rf_banks;
	ah->ah_gain.g_f_corr = 0;

	if (ath5k_hw_rfregs_op(rf, ah->ah_offset[7], 0, 1, 36, 0, false) != 1)
		return 0;

	step = ath5k_hw_rfregs_op(rf, ah->ah_offset[7], 0, 4, 32, 0, false);
	mix = ah->ah_gain.g_step->gos_param[0];

	switch (mix) {
	case 3:
		ah->ah_gain.g_f_corr = step * 2;
		break;
	case 2:
		ah->ah_gain.g_f_corr = (step - 5) * 2;
		break;
	case 1:
		ah->ah_gain.g_f_corr = step;
		break;
	default:
		ah->ah_gain.g_f_corr = 0;
		break;
	}

	return ah->ah_gain.g_f_corr;
}

static bool ath5k_hw_rfregs_gain_readback(struct ath5k_hw *ah)
{
	u32 step, mix, level[4];
	u32 *rf;

	if (ah->ah_rf_banks == NULL)
		return false;

	rf = ah->ah_rf_banks;

	if (ah->ah_radio == AR5K_RF5111) {
		step = ath5k_hw_rfregs_op(rf, ah->ah_offset[7], 0, 6, 37, 0,
				false);
		level[0] = 0;
		level[1] = (step == 0x3f) ? 0x32 : step + 4;
		level[2] = (step != 0x3f) ? 0x40 : level[0];
		level[3] = level[2] + 0x32;

		ah->ah_gain.g_high = level[3] -
			(step == 0x3f ? AR5K_GAIN_DYN_ADJUST_HI_MARGIN : -5);
		ah->ah_gain.g_low = level[0] +
			(step == 0x3f ? AR5K_GAIN_DYN_ADJUST_LO_MARGIN : 0);
	} else {
		mix = ath5k_hw_rfregs_op(rf, ah->ah_offset[7], 0, 1, 36, 0,
				false);
		level[0] = level[2] = 0;

		if (mix == 1) {
			level[1] = level[3] = 83;
		} else {
			level[1] = level[3] = 107;
			ah->ah_gain.g_high = 55;
		}
	}

	return (ah->ah_gain.g_current >= level[0] &&
			ah->ah_gain.g_current <= level[1]) ||
		(ah->ah_gain.g_current >= level[2] &&
			ah->ah_gain.g_current <= level[3]);
}

static s32 ath5k_hw_rfregs_gain_adjust(struct ath5k_hw *ah)
{
	const struct ath5k_gain_opt *go;
	int ret = 0;

	switch (ah->ah_radio) {
	case AR5K_RF5111:
		go = &rfgain_opt_5111;
		break;
	case AR5K_RF5112:
		go = &rfgain_opt_5112;
		break;
	default:
		return 0;
	}

	ah->ah_gain.g_step = &go->go_step[ah->ah_gain.g_step_idx];

	if (ah->ah_gain.g_current >= ah->ah_gain.g_high) {
		if (ah->ah_gain.g_step_idx == 0)
			return -1;
		for (ah->ah_gain.g_target = ah->ah_gain.g_current;
				ah->ah_gain.g_target >=  ah->ah_gain.g_high &&
				ah->ah_gain.g_step_idx > 0;
				ah->ah_gain.g_step =
					&go->go_step[ah->ah_gain.g_step_idx])
			ah->ah_gain.g_target -= 2 *
			    (go->go_step[--(ah->ah_gain.g_step_idx)].gos_gain -
			    ah->ah_gain.g_step->gos_gain);

		ret = 1;
		goto done;
	}

	if (ah->ah_gain.g_current <= ah->ah_gain.g_low) {
		if (ah->ah_gain.g_step_idx == (go->go_steps_count - 1))
			return -2;
		for (ah->ah_gain.g_target = ah->ah_gain.g_current;
				ah->ah_gain.g_target <= ah->ah_gain.g_low &&
				ah->ah_gain.g_step_idx < go->go_steps_count-1;
				ah->ah_gain.g_step =
					&go->go_step[ah->ah_gain.g_step_idx])
			ah->ah_gain.g_target -= 2 *
			    (go->go_step[++ah->ah_gain.g_step_idx].gos_gain -
			    ah->ah_gain.g_step->gos_gain);

		ret = 2;
		goto done;
	}

done:
	ATH5K_DBG(ah->ah_sc, ATH5K_DEBUG_CALIBRATE,
		"ret %d, gain step %u, current gain %u, target gain %u\n",
		ret, ah->ah_gain.g_step_idx, ah->ah_gain.g_current,
		ah->ah_gain.g_target);

	return ret;
}

/*
 * Read EEPROM Calibration data, modify RF Banks and Initialize RF5111
 */
static int ath5k_hw_rf5111_rfregs(struct ath5k_hw *ah,
		struct ieee80211_channel *channel, unsigned int mode)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u32 *rf;
	const unsigned int rf_size = ARRAY_SIZE(rfb_5111);
	unsigned int i;
	int obdb = -1, bank = -1;
	u32 ee_mode;

	AR5K_ASSERT_ENTRY(mode, AR5K_MODE_MAX);

	rf = ah->ah_rf_banks;

	/* Copy values to modify them */
	for (i = 0; i < rf_size; i++) {
		if (rfb_5111[i].rfb_bank >= AR5K_RF5111_INI_RF_MAX_BANKS) {
			ATH5K_ERR(ah->ah_sc, "invalid bank\n");
			return -EINVAL;
		}

		if (bank != rfb_5111[i].rfb_bank) {
			bank = rfb_5111[i].rfb_bank;
			ah->ah_offset[bank] = i;
		}

		rf[i] = rfb_5111[i].rfb_mode_data[mode];
	}

	/* Modify bank 0 */
	if (channel->hw_value & CHANNEL_2GHZ) {
		if (channel->hw_value & CHANNEL_CCK)
			ee_mode = AR5K_EEPROM_MODE_11B;
		else
			ee_mode = AR5K_EEPROM_MODE_11G;
		obdb = 0;

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[0],
				ee->ee_ob[ee_mode][obdb], 3, 119, 0, true))
			return -EINVAL;

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[0],
				ee->ee_ob[ee_mode][obdb], 3, 122, 0, true))
			return -EINVAL;

		obdb = 1;
	/* Modify bank 6 */
	} else {
		/* For 11a, Turbo and XR */
		ee_mode = AR5K_EEPROM_MODE_11A;
		obdb =	 channel->center_freq >= 5725 ? 3 :
			(channel->center_freq >= 5500 ? 2 :
			(channel->center_freq >= 5260 ? 1 :
			 (channel->center_freq > 4000 ? 0 : -1)));

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
				ee->ee_pwd_84, 1, 51, 3, true))
			return -EINVAL;

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
				ee->ee_pwd_90, 1, 45, 3, true))
			return -EINVAL;
	}

	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
			!ee->ee_xpd[ee_mode], 1, 95, 0, true))
		return -EINVAL;

	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
			ee->ee_x_gain[ee_mode], 4, 96, 0, true))
		return -EINVAL;

	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6], obdb >= 0 ?
			ee->ee_ob[ee_mode][obdb] : 0, 3, 104, 0, true))
		return -EINVAL;

	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6], obdb >= 0 ?
			ee->ee_db[ee_mode][obdb] : 0, 3, 107, 0, true))
		return -EINVAL;

	/* Modify bank 7 */
	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[7],
			ee->ee_i_gain[ee_mode], 6, 29, 0, true))
		return -EINVAL;

	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[7],
			ee->ee_xpd[ee_mode], 1, 4, 0, true))
		return -EINVAL;

	/* Write RF values */
	for (i = 0; i < rf_size; i++) {
		AR5K_REG_WAIT(i);
		ath5k_hw_reg_write(ah, rf[i], rfb_5111[i].rfb_ctrl_register);
	}

	return 0;
}

/*
 * Read EEPROM Calibration data, modify RF Banks and Initialize RF5112
 */
static int ath5k_hw_rf5112_rfregs(struct ath5k_hw *ah,
		struct ieee80211_channel *channel, unsigned int mode)
{
	const struct ath5k_ini_rfbuffer *rf_ini;
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u32 *rf;
	unsigned int rf_size, i;
	int obdb = -1, bank = -1;
	u32 ee_mode;

	AR5K_ASSERT_ENTRY(mode, AR5K_MODE_MAX);

	rf = ah->ah_rf_banks;

	if (ah->ah_radio_5ghz_revision >= AR5K_SREV_RAD_5112A) {
		rf_ini = rfb_5112a;
		rf_size = ARRAY_SIZE(rfb_5112a);
	} else {
		rf_ini = rfb_5112;
		rf_size = ARRAY_SIZE(rfb_5112);
	}

	/* Copy values to modify them */
	for (i = 0; i < rf_size; i++) {
		if (rf_ini[i].rfb_bank >= AR5K_RF5112_INI_RF_MAX_BANKS) {
			ATH5K_ERR(ah->ah_sc, "invalid bank\n");
			return -EINVAL;
		}

		if (bank != rf_ini[i].rfb_bank) {
			bank = rf_ini[i].rfb_bank;
			ah->ah_offset[bank] = i;
		}

		rf[i] = rf_ini[i].rfb_mode_data[mode];
	}

	/* Modify bank 6 */
	if (channel->hw_value & CHANNEL_2GHZ) {
		if (channel->hw_value & CHANNEL_OFDM)
			ee_mode = AR5K_EEPROM_MODE_11G;
		else
			ee_mode = AR5K_EEPROM_MODE_11B;
		obdb = 0;

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
				ee->ee_ob[ee_mode][obdb], 3, 287, 0, true))
			return -EINVAL;

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
				ee->ee_ob[ee_mode][obdb], 3, 290, 0, true))
			return -EINVAL;
	} else {
		/* For 11a, Turbo and XR */
		ee_mode = AR5K_EEPROM_MODE_11A;
		obdb = channel->center_freq >= 5725 ? 3 :
		    (channel->center_freq >= 5500 ? 2 :
			(channel->center_freq >= 5260 ? 1 :
			    (channel->center_freq > 4000 ? 0 : -1)));

		if (obdb == -1)
			return -EINVAL;

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
				ee->ee_ob[ee_mode][obdb], 3, 279, 0, true))
			return -EINVAL;

		if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
				ee->ee_ob[ee_mode][obdb], 3, 282, 0, true))
			return -EINVAL;
	}

	ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
	    ee->ee_x_gain[ee_mode], 2, 270, 0, true);
	ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
	    ee->ee_x_gain[ee_mode], 2, 257, 0, true);

	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[6],
			ee->ee_xpd[ee_mode], 1, 302, 0, true))
		return -EINVAL;

	/* Modify bank 7 */
	if (!ath5k_hw_rfregs_op(rf, ah->ah_offset[7],
			ee->ee_i_gain[ee_mode], 6, 14, 0, true))
		return -EINVAL;

	/* Write RF values */
	for (i = 0; i < rf_size; i++)
		ath5k_hw_reg_write(ah, rf[i], rf_ini[i].rfb_ctrl_register);

	return 0;
}

/*
 * Initialize RF5413/5414 and future chips
 * (until we come up with a better solution)
 */
static int ath5k_hw_rf5413_rfregs(struct ath5k_hw *ah,
		struct ieee80211_channel *channel, unsigned int mode)
{
	const struct ath5k_ini_rfbuffer *rf_ini;
	u32 *rf;
	unsigned int rf_size, i;
	int bank = -1;

	AR5K_ASSERT_ENTRY(mode, AR5K_MODE_MAX);

	rf = ah->ah_rf_banks;

	switch (ah->ah_radio) {
	case AR5K_RF5413:
		rf_ini = rfb_5413;
		rf_size = ARRAY_SIZE(rfb_5413);
		break;
	case AR5K_RF2413:
		rf_ini = rfb_2413;
		rf_size = ARRAY_SIZE(rfb_2413);

		if (mode < 2) {
			ATH5K_ERR(ah->ah_sc,
				"invalid channel mode: %i\n", mode);
			return -EINVAL;
		}

		break;
	case AR5K_RF2425:
		rf_ini = rfb_2425;
		rf_size = ARRAY_SIZE(rfb_2425);

		if (mode < 2) {
			ATH5K_ERR(ah->ah_sc,
				"invalid channel mode: %i\n", mode);
			return -EINVAL;
		}

		break;
	default:
		return -EINVAL;
	}

	/* Copy values to modify them */
	for (i = 0; i < rf_size; i++) {
		if (rf_ini[i].rfb_bank >= AR5K_RF5112_INI_RF_MAX_BANKS) {
			ATH5K_ERR(ah->ah_sc, "invalid bank\n");
			return -EINVAL;
		}

		if (bank != rf_ini[i].rfb_bank) {
			bank = rf_ini[i].rfb_bank;
			ah->ah_offset[bank] = i;
		}

		rf[i] = rf_ini[i].rfb_mode_data[mode];
	}

	/*
	 * After compairing dumps from different cards
	 * we get the same RF_BUFFER settings (diff returns
	 * 0 lines). It seems that RF_BUFFER settings are static
	 * and are written unmodified (no EEPROM stuff
	 * is used because calibration data would be
	 * different between different cards and would result
	 * different RF_BUFFER settings)
	 */

	/* Write RF values */
	for (i = 0; i < rf_size; i++)
		ath5k_hw_reg_write(ah, rf[i], rf_ini[i].rfb_ctrl_register);

	return 0;
}

/*
 * Initialize RF
 */
int ath5k_hw_rfregs(struct ath5k_hw *ah, struct ieee80211_channel *channel,
		unsigned int mode)
{
	int (*func)(struct ath5k_hw *, struct ieee80211_channel *, unsigned int);
	int ret;

	switch (ah->ah_radio) {
	case AR5K_RF5111:
		ah->ah_rf_banks_size = sizeof(rfb_5111);
		func = ath5k_hw_rf5111_rfregs;
		break;
	case AR5K_RF5112:
		if (ah->ah_radio_5ghz_revision >= AR5K_SREV_RAD_5112A)
			ah->ah_rf_banks_size = sizeof(rfb_5112a);
		else
			ah->ah_rf_banks_size = sizeof(rfb_5112);
		func = ath5k_hw_rf5112_rfregs;
		break;
	case AR5K_RF5413:
		ah->ah_rf_banks_size = sizeof(rfb_5413);
		func = ath5k_hw_rf5413_rfregs;
		break;
	case AR5K_RF2413:
		ah->ah_rf_banks_size = sizeof(rfb_2413);
		func = ath5k_hw_rf5413_rfregs;
		break;
	case AR5K_RF2425:
		ah->ah_rf_banks_size = sizeof(rfb_2425);
		func = ath5k_hw_rf5413_rfregs;
		break;
	default:
		return -EINVAL;
	}

	if (ah->ah_rf_banks == NULL) {
		/* XXX do extra checks? */
		ah->ah_rf_banks = kmalloc(ah->ah_rf_banks_size, GFP_KERNEL);
		if (ah->ah_rf_banks == NULL) {
			ATH5K_ERR(ah->ah_sc, "out of memory\n");
			return -ENOMEM;
		}
	}

	ret = func(ah, channel, mode);
	if (!ret)
		ah->ah_rf_gain = AR5K_RFGAIN_INACTIVE;

	return ret;
}

int ath5k_hw_rfgain(struct ath5k_hw *ah, unsigned int freq)
{
	const struct ath5k_ini_rfgain *ath5k_rfg;
	unsigned int i, size;

	switch (ah->ah_radio) {
	case AR5K_RF5111:
		ath5k_rfg = rfgain_5111;
		size = ARRAY_SIZE(rfgain_5111);
		break;
	case AR5K_RF5112:
		ath5k_rfg = rfgain_5112;
		size = ARRAY_SIZE(rfgain_5112);
		break;
	case AR5K_RF5413:
		ath5k_rfg = rfgain_5413;
		size = ARRAY_SIZE(rfgain_5413);
		break;
	case AR5K_RF2413:
		ath5k_rfg = rfgain_2413;
		size = ARRAY_SIZE(rfgain_2413);
		break;
	case AR5K_RF2425:
		ath5k_rfg = rfgain_2425;
		size = ARRAY_SIZE(rfgain_2425);
		break;
	default:
		return -EINVAL;
	}

	switch (freq) {
	case AR5K_INI_RFGAIN_2GHZ:
	case AR5K_INI_RFGAIN_5GHZ:
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		AR5K_REG_WAIT(i);
		ath5k_hw_reg_write(ah, ath5k_rfg[i].rfg_value[freq],
			(u32)ath5k_rfg[i].rfg_register);
	}

	return 0;
}

enum ath5k_rfgain ath5k_hw_get_rf_gain(struct ath5k_hw *ah)
{
	u32 data, type;

	ATH5K_TRACE(ah->ah_sc);

	if (ah->ah_rf_banks == NULL || !ah->ah_gain.g_active ||
			ah->ah_version <= AR5K_AR5211)
		return AR5K_RFGAIN_INACTIVE;

	if (ah->ah_rf_gain != AR5K_RFGAIN_READ_REQUESTED)
		goto done;

	data = ath5k_hw_reg_read(ah, AR5K_PHY_PAPD_PROBE);

	if (!(data & AR5K_PHY_PAPD_PROBE_TX_NEXT)) {
		ah->ah_gain.g_current = data >> AR5K_PHY_PAPD_PROBE_GAINF_S;
		type = AR5K_REG_MS(data, AR5K_PHY_PAPD_PROBE_TYPE);

		if (type == AR5K_PHY_PAPD_PROBE_TYPE_CCK)
			ah->ah_gain.g_current += AR5K_GAIN_CCK_PROBE_CORR;

		if (ah->ah_radio >= AR5K_RF5112) {
			ath5k_hw_rfregs_gainf_corr(ah);
			ah->ah_gain.g_current =
				ah->ah_gain.g_current >= ah->ah_gain.g_f_corr ?
				(ah->ah_gain.g_current-ah->ah_gain.g_f_corr) :
				0;
		}

		if (ath5k_hw_rfregs_gain_readback(ah) &&
				AR5K_GAIN_CHECK_ADJUST(&ah->ah_gain) &&
				ath5k_hw_rfregs_gain_adjust(ah))
			ah->ah_rf_gain = AR5K_RFGAIN_NEED_CHANGE;
	}

done:
	return ah->ah_rf_gain;
}

int ath5k_hw_set_rfgain_opt(struct ath5k_hw *ah)
{
	/* Initialize the gain optimization values */
	switch (ah->ah_radio) {
	case AR5K_RF5111:
		ah->ah_gain.g_step_idx = rfgain_opt_5111.go_default;
		ah->ah_gain.g_step =
		    &rfgain_opt_5111.go_step[ah->ah_gain.g_step_idx];
		ah->ah_gain.g_low = 20;
		ah->ah_gain.g_high = 35;
		ah->ah_gain.g_active = 1;
		break;
	case AR5K_RF5112:
		ah->ah_gain.g_step_idx = rfgain_opt_5112.go_default;
		ah->ah_gain.g_step =
		    &rfgain_opt_5112.go_step[ah->ah_gain.g_step_idx];
		ah->ah_gain.g_low = 20;
		ah->ah_gain.g_high = 85;
		ah->ah_gain.g_active = 1;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**************************\
  PHY/RF channel functions
\**************************/

/*
 * Check if a channel is supported
 */
bool ath5k_channel_ok(struct ath5k_hw *ah, u16 freq, unsigned int flags)
{
	/* Check if the channel is in our supported range */
	if (flags & CHANNEL_2GHZ) {
		if ((freq >= ah->ah_capabilities.cap_range.range_2ghz_min) &&
		    (freq <= ah->ah_capabilities.cap_range.range_2ghz_max))
			return true;
	} else if (flags & CHANNEL_5GHZ)
		if ((freq >= ah->ah_capabilities.cap_range.range_5ghz_min) &&
		    (freq <= ah->ah_capabilities.cap_range.range_5ghz_max))
			return true;

	return false;
}

/*
 * Convertion needed for RF5110
 */
static u32 ath5k_hw_rf5110_chan2athchan(struct ieee80211_channel *channel)
{
	u32 athchan;

	/*
	 * Convert IEEE channel/MHz to an internal channel value used
	 * by the AR5210 chipset. This has not been verified with
	 * newer chipsets like the AR5212A who have a completely
	 * different RF/PHY part.
	 */
	athchan = (ath5k_hw_bitswap(
			(ieee80211_frequency_to_channel(
				channel->center_freq) - 24) / 2, 5)
				<< 1) | (1 << 6) | 0x1;
	return athchan;
}

/*
 * Set channel on RF5110
 */
static int ath5k_hw_rf5110_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 data;

	/*
	 * Set the channel and wait
	 */
	data = ath5k_hw_rf5110_chan2athchan(channel);
	ath5k_hw_reg_write(ah, data, AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, 0, AR5K_RF_BUFFER_CONTROL_0);
	mdelay(1);

	return 0;
}

/*
 * Convertion needed for 5111
 */
static int ath5k_hw_rf5111_chan2athchan(unsigned int ieee,
		struct ath5k_athchan_2ghz *athchan)
{
	int channel;

	/* Cast this value to catch negative channel numbers (>= -19) */
	channel = (int)ieee;

	/*
	 * Map 2GHz IEEE channel to 5GHz Atheros channel
	 */
	if (channel <= 13) {
		athchan->a2_athchan = 115 + channel;
		athchan->a2_flags = 0x46;
	} else if (channel == 14) {
		athchan->a2_athchan = 124;
		athchan->a2_flags = 0x44;
	} else if (channel >= 15 && channel <= 26) {
		athchan->a2_athchan = ((channel - 14) * 4) + 132;
		athchan->a2_flags = 0x46;
	} else
		return -EINVAL;

	return 0;
}

/*
 * Set channel on 5111
 */
static int ath5k_hw_rf5111_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	struct ath5k_athchan_2ghz ath5k_channel_2ghz;
	unsigned int ath5k_channel =
		ieee80211_frequency_to_channel(channel->center_freq);
	u32 data0, data1, clock;
	int ret;

	/*
	 * Set the channel on the RF5111 radio
	 */
	data0 = data1 = 0;

	if (channel->hw_value & CHANNEL_2GHZ) {
		/* Map 2GHz channel to 5GHz Atheros channel ID */
		ret = ath5k_hw_rf5111_chan2athchan(
			ieee80211_frequency_to_channel(channel->center_freq),
			&ath5k_channel_2ghz);
		if (ret)
			return ret;

		ath5k_channel = ath5k_channel_2ghz.a2_athchan;
		data0 = ((ath5k_hw_bitswap(ath5k_channel_2ghz.a2_flags, 8) & 0xff)
		    << 5) | (1 << 4);
	}

	if (ath5k_channel < 145 || !(ath5k_channel & 1)) {
		clock = 1;
		data1 = ((ath5k_hw_bitswap(ath5k_channel - 24, 8) & 0xff) << 2) |
			(clock << 1) | (1 << 10) | 1;
	} else {
		clock = 0;
		data1 = ((ath5k_hw_bitswap((ath5k_channel - 24) / 2, 8) & 0xff)
			<< 2) | (clock << 1) | (1 << 10) | 1;
	}

	ath5k_hw_reg_write(ah, (data1 & 0xff) | ((data0 & 0xff) << 8),
			AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, ((data1 >> 8) & 0xff) | (data0 & 0xff00),
			AR5K_RF_BUFFER_CONTROL_3);

	return 0;
}

/*
 * Set channel on 5112 and newer
 */
static int ath5k_hw_rf5112_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 data, data0, data1, data2;
	u16 c;

	data = data0 = data1 = data2 = 0;
	c = channel->center_freq;

	if (c < 4800) {
		if (!((c - 2224) % 5)) {
			data0 = ((2 * (c - 704)) - 3040) / 10;
			data1 = 1;
		} else if (!((c - 2192) % 5)) {
			data0 = ((2 * (c - 672)) - 3040) / 10;
			data1 = 0;
		} else
			return -EINVAL;

		data0 = ath5k_hw_bitswap((data0 << 2) & 0xff, 8);
	} else if ((c - (c % 5)) != 2 || c > 5435) {
		if (!(c % 20) && c >= 5120) {
			data0 = ath5k_hw_bitswap(((c - 4800) / 20 << 2), 8);
			data2 = ath5k_hw_bitswap(3, 2);
		} else if (!(c % 10)) {
			data0 = ath5k_hw_bitswap(((c - 4800) / 10 << 1), 8);
			data2 = ath5k_hw_bitswap(2, 2);
		} else if (!(c % 5)) {
			data0 = ath5k_hw_bitswap((c - 4800) / 5, 8);
			data2 = ath5k_hw_bitswap(1, 2);
		} else
			return -EINVAL;
	} else {
		data0 = ath5k_hw_bitswap((10 * (c - 2) - 4800) / 25 + 1, 8);
		data2 = ath5k_hw_bitswap(0, 2);
	}

	data = (data0 << 4) | (data1 << 1) | (data2 << 2) | 0x1001;

	ath5k_hw_reg_write(ah, data & 0xff, AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, (data >> 8) & 0x7f, AR5K_RF_BUFFER_CONTROL_5);

	return 0;
}

/*
 * Set the channel on the RF2425
 */
static int ath5k_hw_rf2425_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 data, data0, data2;
	u16 c;

	data = data0 = data2 = 0;
	c = channel->center_freq;

	if (c < 4800) {
		data0 = ath5k_hw_bitswap((c - 2272), 8);
		data2 = 0;
	/* ? 5GHz ? */
	} else if ((c - (c % 5)) != 2 || c > 5435) {
		if (!(c % 20) && c < 5120)
			data0 = ath5k_hw_bitswap(((c - 4800) / 20 << 2), 8);
		else if (!(c % 10))
			data0 = ath5k_hw_bitswap(((c - 4800) / 10 << 1), 8);
		else if (!(c % 5))
			data0 = ath5k_hw_bitswap((c - 4800) / 5, 8);
		else
			return -EINVAL;
		data2 = ath5k_hw_bitswap(1, 2);
	} else {
		data0 = ath5k_hw_bitswap((10 * (c - 2) - 4800) / 25 + 1, 8);
		data2 = ath5k_hw_bitswap(0, 2);
	}

	data = (data0 << 4) | data2 << 2 | 0x1001;

	ath5k_hw_reg_write(ah, data & 0xff, AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, (data >> 8) & 0x7f, AR5K_RF_BUFFER_CONTROL_5);

	return 0;
}

/*
 * Set a channel on the radio chip
 */
int ath5k_hw_channel(struct ath5k_hw *ah, struct ieee80211_channel *channel)
{
	int ret;
	/*
	 * Check bounds supported by the PHY (we don't care about regultory
	 * restrictions at this point). Note: hw_value already has the band
	 * (CHANNEL_2GHZ, or CHANNEL_5GHZ) so we inform ath5k_channel_ok()
	 * of the band by that */
	if (!ath5k_channel_ok(ah, channel->center_freq, channel->hw_value)) {
		ATH5K_ERR(ah->ah_sc,
			"channel frequency (%u MHz) out of supported "
			"band range\n",
			channel->center_freq);
			return -EINVAL;
	}

	/*
	 * Set the channel and wait
	 */
	switch (ah->ah_radio) {
	case AR5K_RF5110:
		ret = ath5k_hw_rf5110_channel(ah, channel);
		break;
	case AR5K_RF5111:
		ret = ath5k_hw_rf5111_channel(ah, channel);
		break;
	case AR5K_RF2425:
		ret = ath5k_hw_rf2425_channel(ah, channel);
		break;
	default:
		ret = ath5k_hw_rf5112_channel(ah, channel);
		break;
	}

	if (ret)
		return ret;

	/* Set JAPAN setting for channel 14 */
	if (channel->center_freq == 2484) {
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_CCKTXCTL,
				AR5K_PHY_CCKTXCTL_JAPAN);
	} else {
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_CCKTXCTL,
				AR5K_PHY_CCKTXCTL_WORLD);
	}

	ah->ah_current_channel.center_freq = channel->center_freq;
	ah->ah_current_channel.hw_value = channel->hw_value;
	ah->ah_turbo = channel->hw_value == CHANNEL_T ? true : false;

	return 0;
}

/*****************\
  PHY calibration
\*****************/

/**
 * ath5k_hw_noise_floor_calibration - perform PHY noise floor calibration
 *
 * @ah: struct ath5k_hw pointer we are operating on
 * @freq: the channel frequency, just used for error logging
 *
 * This function performs a noise floor calibration of the PHY and waits for
 * it to complete. Then the noise floor value is compared to some maximum
 * noise floor we consider valid.
 *
 * Note that this is different from what the madwifi HAL does: it reads the
 * noise floor and afterwards initiates the calibration. Since the noise floor
 * calibration can take some time to finish, depending on the current channel
 * use, that avoids the occasional timeout warnings we are seeing now.
 *
 * See the following link for an Atheros patent on noise floor calibration:
 * http://patft.uspto.gov/netacgi/nph-Parser?Sect1=PTO1&Sect2=HITOFF&d=PALL \
 * &p=1&u=%2Fnetahtml%2FPTO%2Fsrchnum.htm&r=1&f=G&l=50&s1=7245893.PN.&OS=PN/7
 *
 * XXX: Since during noise floor calibration antennas are detached according to
 * the patent, we should stop tx queues here.
 */
int
ath5k_hw_noise_floor_calibration(struct ath5k_hw *ah, short freq)
{
	int ret;
	unsigned int i;
	s32 noise_floor;

	/*
	 * Enable noise floor calibration
	 */
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL,
				AR5K_PHY_AGCCTL_NF);

	ret = ath5k_hw_register_timeout(ah, AR5K_PHY_AGCCTL,
			AR5K_PHY_AGCCTL_NF, 0, false);
	if (ret) {
		ATH5K_ERR(ah->ah_sc,
			"noise floor calibration timeout (%uMHz)\n", freq);
		return -EAGAIN;
	}

	/* Wait until the noise floor is calibrated and read the value */
	for (i = 20; i > 0; i--) {
		mdelay(1);
		noise_floor = ath5k_hw_reg_read(ah, AR5K_PHY_NF);
		noise_floor = AR5K_PHY_NF_RVAL(noise_floor);
		if (noise_floor & AR5K_PHY_NF_ACTIVE) {
			noise_floor = AR5K_PHY_NF_AVAL(noise_floor);

			if (noise_floor <= AR5K_TUNE_NOISE_FLOOR)
				break;
		}
	}

	ATH5K_DBG_UNLIMIT(ah->ah_sc, ATH5K_DEBUG_CALIBRATE,
		"noise floor %d\n", noise_floor);

	if (noise_floor > AR5K_TUNE_NOISE_FLOOR) {
		ATH5K_ERR(ah->ah_sc,
			"noise floor calibration failed (%uMHz)\n", freq);
		return -EAGAIN;
	}

	ah->ah_noise_floor = noise_floor;

	return 0;
}

/*
 * Perform a PHY calibration on RF5110
 * -Fix BPSK/QAM Constellation (I/Q correction)
 * -Calculate Noise Floor
 */
static int ath5k_hw_rf5110_calibrate(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 phy_sig, phy_agc, phy_sat, beacon;
	int ret;

	/*
	 * Disable beacons and RX/TX queues, wait
	 */
	AR5K_REG_ENABLE_BITS(ah, AR5K_DIAG_SW_5210,
		AR5K_DIAG_SW_DIS_TX | AR5K_DIAG_SW_DIS_RX_5210);
	beacon = ath5k_hw_reg_read(ah, AR5K_BEACON_5210);
	ath5k_hw_reg_write(ah, beacon & ~AR5K_BEACON_ENABLE, AR5K_BEACON_5210);

	mdelay(2);

	/*
	 * Set the channel (with AGC turned off)
	 */
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);
	udelay(10);
	ret = ath5k_hw_channel(ah, channel);

	/*
	 * Activate PHY and wait
	 */
	ath5k_hw_reg_write(ah, AR5K_PHY_ACT_ENABLE, AR5K_PHY_ACT);
	mdelay(1);

	AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);

	if (ret)
		return ret;

	/*
	 * Calibrate the radio chip
	 */

	/* Remember normal state */
	phy_sig = ath5k_hw_reg_read(ah, AR5K_PHY_SIG);
	phy_agc = ath5k_hw_reg_read(ah, AR5K_PHY_AGCCOARSE);
	phy_sat = ath5k_hw_reg_read(ah, AR5K_PHY_ADCSAT);

	/* Update radio registers */
	ath5k_hw_reg_write(ah, (phy_sig & ~(AR5K_PHY_SIG_FIRPWR)) |
		AR5K_REG_SM(-1, AR5K_PHY_SIG_FIRPWR), AR5K_PHY_SIG);

	ath5k_hw_reg_write(ah, (phy_agc & ~(AR5K_PHY_AGCCOARSE_HI |
			AR5K_PHY_AGCCOARSE_LO)) |
		AR5K_REG_SM(-1, AR5K_PHY_AGCCOARSE_HI) |
		AR5K_REG_SM(-127, AR5K_PHY_AGCCOARSE_LO), AR5K_PHY_AGCCOARSE);

	ath5k_hw_reg_write(ah, (phy_sat & ~(AR5K_PHY_ADCSAT_ICNT |
			AR5K_PHY_ADCSAT_THR)) |
		AR5K_REG_SM(2, AR5K_PHY_ADCSAT_ICNT) |
		AR5K_REG_SM(12, AR5K_PHY_ADCSAT_THR), AR5K_PHY_ADCSAT);

	udelay(20);

	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);
	udelay(10);
	ath5k_hw_reg_write(ah, AR5K_PHY_RFSTG_DISABLE, AR5K_PHY_RFSTG);
	AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);

	mdelay(1);

	/*
	 * Enable calibration and wait until completion
	 */
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL, AR5K_PHY_AGCCTL_CAL);

	ret = ath5k_hw_register_timeout(ah, AR5K_PHY_AGCCTL,
			AR5K_PHY_AGCCTL_CAL, 0, false);

	/* Reset to normal state */
	ath5k_hw_reg_write(ah, phy_sig, AR5K_PHY_SIG);
	ath5k_hw_reg_write(ah, phy_agc, AR5K_PHY_AGCCOARSE);
	ath5k_hw_reg_write(ah, phy_sat, AR5K_PHY_ADCSAT);

	if (ret) {
		ATH5K_ERR(ah->ah_sc, "calibration timeout (%uMHz)\n",
				channel->center_freq);
		return ret;
	}

	ath5k_hw_noise_floor_calibration(ah, channel->center_freq);

	/*
	 * Re-enable RX/TX and beacons
	 */
	AR5K_REG_DISABLE_BITS(ah, AR5K_DIAG_SW_5210,
		AR5K_DIAG_SW_DIS_TX | AR5K_DIAG_SW_DIS_RX_5210);
	ath5k_hw_reg_write(ah, beacon, AR5K_BEACON_5210);

	return 0;
}

/*
 * Perform a PHY calibration on RF5111/5112 and newer chips
 */
static int ath5k_hw_rf511x_calibrate(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 i_pwr, q_pwr;
	s32 iq_corr, i_coff, i_coffd, q_coff, q_coffd;
	int i;
	ATH5K_TRACE(ah->ah_sc);

	if (!ah->ah_calibration ||
		ath5k_hw_reg_read(ah, AR5K_PHY_IQ) & AR5K_PHY_IQ_RUN)
		goto done;

	/* Calibration has finished, get the results and re-run */
	for (i = 0; i <= 10; i++) {
		iq_corr = ath5k_hw_reg_read(ah, AR5K_PHY_IQRES_CAL_CORR);
		i_pwr = ath5k_hw_reg_read(ah, AR5K_PHY_IQRES_CAL_PWR_I);
		q_pwr = ath5k_hw_reg_read(ah, AR5K_PHY_IQRES_CAL_PWR_Q);
	}

	i_coffd = ((i_pwr >> 1) + (q_pwr >> 1)) >> 7;
	q_coffd = q_pwr >> 7;

	/* No correction */
	if (i_coffd == 0 || q_coffd == 0)
		goto done;

	i_coff = ((-iq_corr) / i_coffd) & 0x3f;

	/* Boundary check */
	if (i_coff > 31)
		i_coff = 31;
	if (i_coff < -32)
		i_coff = -32;

	q_coff = (((s32)i_pwr / q_coffd) - 128) & 0x1f;

	/* Boundary check */
	if (q_coff > 15)
		q_coff = 15;
	if (q_coff < -16)
		q_coff = -16;

	/* Commit new I/Q value */
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_CORR_ENABLE |
		((u32)q_coff) | ((u32)i_coff << AR5K_PHY_IQ_CORR_Q_I_COFF_S));

	/* Re-enable calibration -if we don't we'll commit
	 * the same values again and again */
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_IQ,
			AR5K_PHY_IQ_CAL_NUM_LOG_MAX, 15);
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_RUN);

done:

	/* TODO: Separate noise floor calibration from I/Q calibration
	 * since noise floor calibration interrupts rx path while I/Q
	 * calibration doesn't. We don't need to run noise floor calibration
	 * as often as I/Q calibration.*/
	ath5k_hw_noise_floor_calibration(ah, channel->center_freq);

	/* Request RF gain */
	if (channel->hw_value & CHANNEL_5GHZ) {
		ath5k_hw_reg_write(ah, AR5K_REG_SM(ah->ah_txpower.txp_max,
			AR5K_PHY_PAPD_PROBE_TXPOWER) |
			AR5K_PHY_PAPD_PROBE_TX_NEXT, AR5K_PHY_PAPD_PROBE);
		ah->ah_rf_gain = AR5K_RFGAIN_READ_REQUESTED;
	}

	return 0;
}

/*
 * Perform a PHY calibration
 */
int ath5k_hw_phy_calibrate(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	int ret;

	if (ah->ah_radio == AR5K_RF5110)
		ret = ath5k_hw_rf5110_calibrate(ah, channel);
	else
		ret = ath5k_hw_rf511x_calibrate(ah, channel);

	return ret;
}

int ath5k_hw_phy_disable(struct ath5k_hw *ah)
{
	ATH5K_TRACE(ah->ah_sc);
	/*Just a try M.F.*/
	ath5k_hw_reg_write(ah, AR5K_PHY_ACT_DISABLE, AR5K_PHY_ACT);

	return 0;
}

/********************\
  Misc PHY functions
\********************/

/*
 * Get the PHY Chip revision
 */
u16 ath5k_hw_radio_revision(struct ath5k_hw *ah, unsigned int chan)
{
	unsigned int i;
	u32 srev;
	u16 ret;

	ATH5K_TRACE(ah->ah_sc);

	/*
	 * Set the radio chip access register
	 */
	switch (chan) {
	case CHANNEL_2GHZ:
		ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_2GHZ, AR5K_PHY(0));
		break;
	case CHANNEL_5GHZ:
		ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_5GHZ, AR5K_PHY(0));
		break;
	default:
		return 0;
	}

	mdelay(2);

	/* ...wait until PHY is ready and read the selected radio revision */
	ath5k_hw_reg_write(ah, 0x00001c16, AR5K_PHY(0x34));

	for (i = 0; i < 8; i++)
		ath5k_hw_reg_write(ah, 0x00010000, AR5K_PHY(0x20));

	if (ah->ah_version == AR5K_AR5210) {
		srev = ath5k_hw_reg_read(ah, AR5K_PHY(256) >> 28) & 0xf;
		ret = (u16)ath5k_hw_bitswap(srev, 4) + 1;
	} else {
		srev = (ath5k_hw_reg_read(ah, AR5K_PHY(0x100)) >> 24) & 0xff;
		ret = (u16)ath5k_hw_bitswap(((srev & 0xf0) >> 4) |
				((srev & 0x0f) << 4), 8);
	}

	/* Reset to the 5GHz mode */
	ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_5GHZ, AR5K_PHY(0));

	return ret;
}

void /*TODO:Boundary check*/
ath5k_hw_set_def_antenna(struct ath5k_hw *ah, unsigned int ant)
{
	ATH5K_TRACE(ah->ah_sc);
	/*Just a try M.F.*/
	if (ah->ah_version != AR5K_AR5210)
		ath5k_hw_reg_write(ah, ant, AR5K_DEFAULT_ANTENNA);
}

unsigned int ath5k_hw_get_def_antenna(struct ath5k_hw *ah)
{
	ATH5K_TRACE(ah->ah_sc);
	/*Just a try M.F.*/
	if (ah->ah_version != AR5K_AR5210)
		return ath5k_hw_reg_read(ah, AR5K_DEFAULT_ANTENNA);

	return false; /*XXX: What do we return for 5210 ?*/
}

/*
 * TX power setup
 */

/*
 * Initialize the tx power table (not fully implemented)
 */
static void ath5k_txpower_table(struct ath5k_hw *ah,
		struct ieee80211_channel *channel, s16 max_power)
{
	unsigned int i, min, max, n;
	u16 txpower, *rates;

	rates = ah->ah_txpower.txp_rates;

	txpower = AR5K_TUNE_DEFAULT_TXPOWER * 2;
	if (max_power > txpower)
		txpower = max_power > AR5K_TUNE_MAX_TXPOWER ?
		    AR5K_TUNE_MAX_TXPOWER : max_power;

	for (i = 0; i < AR5K_MAX_RATES; i++)
		rates[i] = txpower;

	/* XXX setup target powers by rate */

	ah->ah_txpower.txp_min = rates[7];
	ah->ah_txpower.txp_max = rates[0];
	ah->ah_txpower.txp_ofdm = rates[0];

	/* Calculate the power table */
	n = ARRAY_SIZE(ah->ah_txpower.txp_pcdac);
	min = AR5K_EEPROM_PCDAC_START;
	max = AR5K_EEPROM_PCDAC_STOP;
	for (i = 0; i < n; i += AR5K_EEPROM_PCDAC_STEP)
		ah->ah_txpower.txp_pcdac[i] =
#ifdef notyet
		min + ((i * (max - min)) / n);
#else
		min;
#endif
}

/*
 * Set transmition power
 */
int /*O.K. - txpower_table is unimplemented so this doesn't work*/
ath5k_hw_txpower(struct ath5k_hw *ah, struct ieee80211_channel *channel,
		unsigned int txpower)
{
	bool tpc = ah->ah_txpower.txp_tpc;
	unsigned int i;

	ATH5K_TRACE(ah->ah_sc);
	if (txpower > AR5K_TUNE_MAX_TXPOWER) {
		ATH5K_ERR(ah->ah_sc, "invalid tx power: %u\n", txpower);
		return -EINVAL;
	}

	/*
	 * RF2413 for some reason can't
	 * transmit anything if we call
	 * this funtion, so we skip it
	 * until we fix txpower.
	 *
	 * XXX: Assume same for RF2425
	 * to be safe.
	 */
	if ((ah->ah_radio == AR5K_RF2413) || (ah->ah_radio == AR5K_RF2425))
		return 0;

	/* Reset TX power values */
	memset(&ah->ah_txpower, 0, sizeof(ah->ah_txpower));
	ah->ah_txpower.txp_tpc = tpc;

	/* Initialize TX power table */
	ath5k_txpower_table(ah, channel, txpower);

	/*
	 * Write TX power values
	 */
	for (i = 0; i < (AR5K_EEPROM_POWER_TABLE_SIZE / 2); i++) {
		ath5k_hw_reg_write(ah,
			((((ah->ah_txpower.txp_pcdac[(i << 1) + 1] << 8) | 0xff) & 0xffff) << 16) |
			(((ah->ah_txpower.txp_pcdac[(i << 1)    ] << 8) | 0xff) & 0xffff),
			AR5K_PHY_PCDAC_TXPOWER(i));
	}

	ath5k_hw_reg_write(ah, AR5K_TXPOWER_OFDM(3, 24) |
		AR5K_TXPOWER_OFDM(2, 16) | AR5K_TXPOWER_OFDM(1, 8) |
		AR5K_TXPOWER_OFDM(0, 0), AR5K_PHY_TXPOWER_RATE1);

	ath5k_hw_reg_write(ah, AR5K_TXPOWER_OFDM(7, 24) |
		AR5K_TXPOWER_OFDM(6, 16) | AR5K_TXPOWER_OFDM(5, 8) |
		AR5K_TXPOWER_OFDM(4, 0), AR5K_PHY_TXPOWER_RATE2);

	ath5k_hw_reg_write(ah, AR5K_TXPOWER_CCK(10, 24) |
		AR5K_TXPOWER_CCK(9, 16) | AR5K_TXPOWER_CCK(15, 8) |
		AR5K_TXPOWER_CCK(8, 0), AR5K_PHY_TXPOWER_RATE3);

	ath5k_hw_reg_write(ah, AR5K_TXPOWER_CCK(14, 24) |
		AR5K_TXPOWER_CCK(13, 16) | AR5K_TXPOWER_CCK(12, 8) |
		AR5K_TXPOWER_CCK(11, 0), AR5K_PHY_TXPOWER_RATE4);

	if (ah->ah_txpower.txp_tpc)
		ath5k_hw_reg_write(ah, AR5K_PHY_TXPOWER_RATE_MAX_TPC_ENABLE |
			AR5K_TUNE_MAX_TXPOWER, AR5K_PHY_TXPOWER_RATE_MAX);
	else
		ath5k_hw_reg_write(ah, AR5K_PHY_TXPOWER_RATE_MAX |
			AR5K_TUNE_MAX_TXPOWER, AR5K_PHY_TXPOWER_RATE_MAX);

	return 0;
}

int ath5k_hw_set_txpower_limit(struct ath5k_hw *ah, unsigned int power)
{
	/*Just a try M.F.*/
	struct ieee80211_channel *channel = &ah->ah_current_channel;

	ATH5K_TRACE(ah->ah_sc);
	ATH5K_DBG(ah->ah_sc, ATH5K_DEBUG_TXPOWER,
		"changing txpower to %d\n", power);

	return ath5k_hw_txpower(ah, channel, power);
}

#undef _ATH5K_PHY
