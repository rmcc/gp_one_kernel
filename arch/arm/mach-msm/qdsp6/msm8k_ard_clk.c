/*
 *
 * Copyright (c) 2009 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 *
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/init.h>
#include "../clock.h"
#include <mach/qdsp6/msm8k_ard_clk.h>

static struct clk	*rx_clk;
static struct clk	*tx_clk;
static struct clk	*ecodec_clk;


void ard_clk_set_ecodec_clk(void)
{

	s32 rc = CAD_RES_SUCCESS;
	/* Frequency in Hz - 8Khz for AUX PCM now, later need to change to
	 * support I2S
	 */
	rc = clk_set_rate(ecodec_clk, 2048000);
	if (rc != CAD_RES_SUCCESS)
		pr_err("ard_clk: Rate on ECODEC clk not set!\n");

}


void ard_clk_enable_internal_codec_clk_tx(void)
{
	s32 rc = CAD_RES_SUCCESS;

	if (tx_clk != NULL)
		return;

	tx_clk = clk_get(NULL, "icodec_tx_clk");
	if (tx_clk == NULL) {
		pr_err("ard_clk: Invalid TX clk!\n");
		return;
	}

	ard_clk_set_icodec_tx_clk();

	rc = clk_enable(tx_clk);
	if (rc != CAD_RES_SUCCESS)
		pr_err("ard_clk: RX clk not enabled!\n");
}


void ard_clk_enable_internal_codec_clk_rx(void)
{
	s32 rc = CAD_RES_SUCCESS;

	if (rx_clk != NULL)
		return;

	rx_clk = clk_get(NULL, "icodec_rx_clk");

	if (rx_clk == NULL) {
		pr_err("ard_clk: Invalid RX clk!\n");
		return;
	}

	ard_clk_set_icodec_rx_clk();

	rc = clk_enable(rx_clk);
	if (rc != CAD_RES_SUCCESS)
		pr_err("ard_clk: RX clk not enabled!\n");
}


void ard_clk_enable_external_codec_clk(void)
{
	s32 rc = CAD_RES_SUCCESS;

	if (ecodec_clk != NULL)
		return;

	ecodec_clk = clk_get(NULL, "ecodec_clk");
	if (ecodec_clk == NULL) {
		pr_err("ard_clk: Invalid ECODEC clk!\n");
		return;
	}

	ard_clk_set_ecodec_clk();

	rc = clk_enable(ecodec_clk);
	if (rc != CAD_RES_SUCCESS)
		pr_err("ard_clk: ECODEC clk not enabled!\n");
}


void ard_clk_disable_internal_codec_clk_tx(void)
{
	clk_disable(tx_clk);
	clk_put(tx_clk);
	tx_clk = NULL;
}

void ard_clk_disable_internal_codec_clk_rx(void)
{
	clk_disable(rx_clk);
	clk_put(rx_clk);
	rx_clk = NULL;
}


void ard_clk_disable_external_codec_clk(void)
{
	clk_disable(ecodec_clk);
	clk_put(ecodec_clk);
	ecodec_clk = NULL;
}


void ard_clk_set_icodec_rx_clk(void)
{
	s32 rc = CAD_RES_SUCCESS;
	/* Frequency in Hz - 48KHz */
	rc = clk_set_rate(rx_clk, 12288000);

	if (rc != CAD_RES_SUCCESS)
		pr_err("ard_clk: Rate on RX clk not set!\n");
}


void ard_clk_set_icodec_tx_clk(void)
{
	s32 rc = CAD_RES_SUCCESS;

	/* Frequency in Hz - 8KHz */
	rc = clk_set_rate(tx_clk, 2048000);

	if (rc != CAD_RES_SUCCESS)
		pr_err("ard_clk: Rate on TX clk not set!\n");
}

void ard_clk_enable(u32 dev_id)
{
	switch (dev_id) {
	case 0:
		ard_clk_enable_internal_codec_clk_rx();
		pr_err("ENABLE RX INT CLK, dev_id %d\n", dev_id);
		break;
	case 1:
		ard_clk_enable_internal_codec_clk_tx();
		pr_err("ENABLE TX INT CLK, dev_id %d\n", dev_id);
		break;
	case 2:
	case 3:
		/* No seperate TX and RX clocks for external codec */
		ard_clk_enable_external_codec_clk();
		pr_err("ENABLE EXT CLK, dev_id %d\n", dev_id);
		break;
	default:
		pr_err("unsupported clk\n");
	}
}

void ard_clk_disable(u32 dev_id)
{
	switch (dev_id) {
	case 0:
		ard_clk_disable_internal_codec_clk_rx();
		pr_err("DISABLE RX INT CLK, dev_id %d\n", dev_id);
		break;
	case 1:
		ard_clk_disable_internal_codec_clk_tx();
		pr_err("DISABLE TX INT CLK, dev_id %d\n", dev_id);
		break;
	case 2:
	case 3:
		/* No seperate TX and RX clocks for external codec */
		ard_clk_disable_external_codec_clk();
		pr_err("DISABLE EXT CLK, dev_id %d\n", dev_id);
		break;
	default:
		pr_err("unsupported clk setting\n");
	}
}
