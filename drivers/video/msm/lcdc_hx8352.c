/* drivers/video/msm/src/panel/lcdc/lcdc_hx8352.c
 * 
 * All source code in this file is licensed under the following license
 * except where indicated.
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
#include "msm_fb.h"

//T_FIH,ADQ,JOE HSU -----
#if 0
static int __init lcdc_hx8352_init(void)
{
	int ret;
	struct msm_panel_info pinfo;

	pinfo.xres = 240;
	pinfo.yres = 400;
	pinfo.type = LCDC_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.fb_num = 2;
	pinfo.clk_rate = 38400000;

//	pinfo.lcdc.pixclk_in_ns = 226;	/* 1/38.4 Mhz x 1000 */
	pinfo.lcdc.h_back_porch = 5;
	pinfo.lcdc.h_front_porch = 5;
	pinfo.lcdc.h_pulse_width = 5;
	pinfo.lcdc.v_back_porch = 2;
	pinfo.lcdc.v_front_porch = 0;
	pinfo.lcdc.v_pulse_width = 2;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;	

	ret = lcdc_device_register(&pinfo);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);
	return ret;
}

module_init(lcdc_hx8352_init);
#endif
//T_FIH,ADQ,JOE HSU +++++