/*
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *  Copyright (C) 2002 Shane Nay (shane@minirl.com)
 *  Copyright 2005-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memory.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/mach/map.h>
#include <mach/common.h>
#include <asm/page.h>
#include <asm/setup.h>
#include <mach/board-mx31lite.h>
#include <mach/imx-uart.h>
#include <mach/iomux-mx3.h>
#include "devices.h"

/*
 * This file contains the board-specific initialization routines.
 */

static unsigned int mx31lite_pins[] = {
	/* UART1 */
	MX31_PIN_CTS1__CTS1,
	MX31_PIN_RTS1__RTS1,
	MX31_PIN_TXD1__TXD1,
	MX31_PIN_RXD1__RXD1,
};

static struct imxuart_platform_data uart_pdata = {
	.flags = IMXUART_HAVE_RTSCTS,
};

/*
 * This structure defines the MX31 memory map.
 */
static struct map_desc mx31lite_io_desc[] __initdata = {
	{
		.virtual = SPBA0_BASE_ADDR_VIRT,
		.pfn = __phys_to_pfn(SPBA0_BASE_ADDR),
		.length = SPBA0_SIZE,
		.type = MT_DEVICE_NONSHARED
	}, {
		.virtual = CS4_BASE_ADDR_VIRT,
		.pfn = __phys_to_pfn(CS4_BASE_ADDR),
		.length = CS4_SIZE,
		.type = MT_DEVICE
	}
};

/*
 * Set up static virtual mappings.
 */
void __init mx31lite_map_io(void)
{
	mx31_map_io();
	iotable_init(mx31lite_io_desc, ARRAY_SIZE(mx31lite_io_desc));
}

/*
 * Board specific initialization.
 */
static void __init mxc_board_init(void)
{
	mxc_iomux_setup_multiple_pins(mx31lite_pins, ARRAY_SIZE(mx31lite_pins),
				      "mx31lite");

	mxc_register_device(&mxc_uart_device0, &uart_pdata);
}

static void __init mx31lite_timer_init(void)
{
	mx31_clocks_init(26000000);
}

struct sys_timer mx31lite_timer = {
	.init	= mx31lite_timer_init,
};

/*
 * The following uses standard kernel macros defined in arch.h in order to
 * initialize __mach_desc_MX31LITE data structure.
 */

MACHINE_START(MX31LITE, "LogicPD MX31 LITEKIT")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.phys_io        = AIPS1_BASE_ADDR,
	.io_pg_offst    = ((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.boot_params    = PHYS_OFFSET + 0x100,
	.map_io         = mx31lite_map_io,
	.init_irq       = mxc_init_irq,
	.init_machine   = mxc_board_init,
	.timer          = &mx31lite_timer,
MACHINE_END
