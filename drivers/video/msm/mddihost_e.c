/* drivers/video/msm/src/drv/mddi/mddihost_e.c
 *
 * Copyright (c) 2008 QUALCOMM USA, INC.
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"

#include <linux/clk.h>
#include <mach/clk.h>

extern struct semaphore mddi_host_mutex;
static boolean mddi_host_ext_powered = FALSE;

void mddi_host_start_ext_display(void)
{
	down(&mddi_host_mutex);

	if (!mddi_host_ext_powered) {
		mddi_host_init(MDDI_HOST_EXT);

		mddi_host_ext_powered = TRUE;
	}

	up(&mddi_host_mutex);
}

void mddi_host_stop_ext_display(void)
{
	down(&mddi_host_mutex);

	if (mddi_host_ext_powered) {
		mddi_host_powerdown(MDDI_HOST_EXT);

		mddi_host_ext_powered = FALSE;
	}

	up(&mddi_host_mutex);
}

void mddi_ext_host_clock_config(struct msm_fb_data_type *mfd,
				struct clk *this_clk)
{
	static boolean mddi_ext_host_ioclock_rate_set = FALSE;

	if (!mddi_ext_host_ioclock_rate_set) {
		if (clk_set_max_rate(this_clk, mfd->panel_info.clk_max) < 0) {
			if (clk_set_rate(this_clk,
						mfd->panel_info.clk_rate) < 0)
				goto err_mddi_clk_set;
		} else if (clk_set_min_rate(this_clk,
						mfd->panel_info.clk_min) < 0)
				goto err_mddi_clk_set;

		mddi_ext_host_ioclock_rate_set = TRUE;
	}

	return;

err_mddi_clk_set:
	printk(KERN_ERR
		"%s: can't set mddi_e io clk targate rate = %d min=%d max=%d\n",
		__func__, mfd->panel_info.clk_rate,
		mfd->panel_info.clk_min,
		mfd->panel_info.clk_max);
}
