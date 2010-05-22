/*
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
 */

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/bootmem.h>
#include <linux/i2c.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/sirc.h>
#include <mach/msm_touchpad.h>
#include <mach/msm_i2ckbd.h>
#include <linux/spi/spi.h>

#include "devices.h"
#include "timer.h"

#define MSM_PMEM_MDP_SIZE	0x800000
#define MSM_FB_SIZE             0x600000

static struct resource smc911x_resources[] = {
	[0] = {
		.start  = 0x90000000,
		.end    = 0x90000100,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = MSM_GPIO_TO_INT(156),
		.end    = 156,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device smc911x_device = {
	.name           = "smc911x",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(smc911x_resources),
	.resource       = smc911x_resources,
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.size = MSM_PMEM_MDP_SIZE,
	.no_allocator = 0,
	.cached = 1,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct resource qsd_spi_resources[] = {
	{
		.name   = "spi_irq_in",
		.start	= INT_SPI_INPUT,
		.end	= INT_SPI_INPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_out",
		.start	= INT_SPI_OUTPUT,
		.end	= INT_SPI_OUTPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_err",
		.start	= INT_SPI_ERROR,
		.end	= INT_SPI_ERROR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_base",
		.start	= 0xA1200000,
		.end	= 0xA1200000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "spi_clk",
		.start	= 17,
		.end	= 1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_mosi",
		.start	= 18,
		.end	= 1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_miso",
		.start	= 19,
		.end	= 1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_cs0",
		.start	= 20,
		.end	= 1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_cs0",
		.start	= 106,
		.end	= 0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device qsd_device_spi = {
	.name	        = "spi_qsd",
	.id	        = 0,
	.num_resources	= ARRAY_SIZE(qsd_spi_resources),
	.resource	= qsd_spi_resources,
};

static struct spi_board_info msm_spi_board_info[] __initdata = {
	{
		.modalias	= "bma150",
		.mode		= SPI_MODE_3,
		.irq		= MSM_GPIO_TO_INT(106),
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 10000000,
	}
};

static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
};

static u32 mddi_clk_list[] = {
	61440,
	81920,
	96000,
	122880,
	153600,
	192000,
	245760,
	288000,
	384000
};

static int msm_fb_mddi_sel_clk(u32 *clk_rate)
{
	u32 rate = *clk_rate;
	int i;
	int last_idx = ARRAY_SIZE(mddi_clk_list)-1;

	rate = (rate/1000)*2;

	for (i = 0 ; i < ARRAY_SIZE(mddi_clk_list) ; i++)
		if (mddi_clk_list[i] == rate)
			return 0;

	if (rate > mddi_clk_list[last_idx]) {
		*clk_rate = mddi_clk_list[last_idx]*1000;
		return 0;
	}

	for (i = last_idx ; i > 0 ; i--)
		if ((rate < mddi_clk_list[i]) && (rate > mddi_clk_list[i-1])) {
			*clk_rate = mddi_clk_list[i-1]*1000;
	return 0;
		}

	return -1;
}

static struct mddi_platform_data mddi_pdata = {
	.mddi_sel_clk = msm_fb_mddi_sel_clk,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", 0);
	msm_fb_register_device("pmdh", &mddi_pdata);
	msm_fb_register_device("emdh", &mddi_pdata);
	msm_fb_register_device("tvenc", 0);
	msm_fb_register_device("lcdc", 0);
}

#ifdef CONFIG_BT
static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};

enum {
	BT_SYSRST,
	BT_WAKE,
	BT_HOST_WAKE,
	BT_PWR_EN,
	BT_RFR,
	BT_CTS,
	BT_RX,
	BT_TX,
	BT_PCM_DOUT,
	BT_PCM_DIN,
	BT_PCM_SYNC,
	BT_PCM_CLK,
};

static unsigned bt_config_power_on[] = {
	GPIO_CFG(18, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* SYSRST */
	GPIO_CFG(19, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* WAKE */
	GPIO_CFG(21, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* HOST_WAKE */
	GPIO_CFG(22, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PWR_EN */
	GPIO_CFG(43, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 2, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* Tx */
	GPIO_CFG(68, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(70, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),	/* PCM_CLK */
};
static unsigned bt_config_power_off[] = {
	GPIO_CFG(18, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* SYSRST */
	GPIO_CFG(19, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* WAKE */
	GPIO_CFG(21, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* HOST_WAKE */
	GPIO_CFG(22, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PWR_EN */
	GPIO_CFG(43, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* RFR */
	GPIO_CFG(44, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* CTS */
	GPIO_CFG(45, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Rx */
	GPIO_CFG(46, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* Tx */
	GPIO_CFG(68, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DOUT */
	GPIO_CFG(69, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_DIN */
	GPIO_CFG(70, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_SYNC */
	GPIO_CFG(71, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),	/* PCM_CLK */
};

static int bluetooth_power(int on)
{
	int pin, rc;

	printk(KERN_DEBUG "%s\n", __func__);

	if (on) {
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on); pin++) {
			rc = gpio_tlmm_config(bt_config_power_on[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_on[pin], rc);
				return -EIO;
			}
		}

		gpio_set_value(22, on); /* PWR_EN */
		gpio_set_value(18, on); /* SYSRST */

	} else {
		gpio_set_value(18, on); /* SYSRST */
		gpio_set_value(22, on); /* PWR_EN */

		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off); pin++) {
			rc = gpio_tlmm_config(bt_config_power_off[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_off[pin], rc);
				return -EIO;
			}
		}

	}

	return 0;
}

static void __init bt_power_init(void)
{
	msm_bt_power_device.dev.platform_data = &bluetooth_power;
}
#else
#define bt_power_init(x) do {} while (0)
#endif

static struct platform_device *devices[] __initdata = {
	&msm_fb_device,
	&msm_device_smd,
	&smc911x_device,
	&android_pmem_device,
	&msm_device_nand,
#ifdef CONFIG_BT
	&msm_bt_power_device,
#endif
	&msm_device_i2c,
	&qsd_device_spi,
	&msm_device_tssc,
};

static struct msm_acpu_clock_platform_data comet_clock_data = {
	.acpu_switch_time_us = 20,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.power_collapse_khz = 128000000,
	.wait_for_irq_khz = 128000000,
};

static struct msm_touchpad_platform_data msm_touchpad_data = {
	.gpioirq     = 42,
	.gpiosuspend = 34,
};

static struct msm_i2ckbd_platform_data msm_kybd_data = {
	.hwrepeat = 0,
	.scanset1 = 1,
	.gpioreset = 35,
	.gpioirq = 144,
};

static struct i2c_board_info msm_i2c_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("glidesensor", 0x2A),
		.irq           =  MSM_GPIO_TO_INT(42),
		.platform_data = &msm_touchpad_data
	},
	{
		I2C_BOARD_INFO("msm-i2ckbd", 0x3A),
		.type           = "msm-i2ckbd",
		.irq            = MSM_GPIO_TO_INT(144),
		.platform_data  = &msm_kybd_data
	},
};

static void __init comet_init_irq(void)
{
	msm_init_irq();
	msm_init_sirc();
}

static void sdcc_gpio_init(void)
{
	/* SDC1 GPIOs */
	if (gpio_request(51, "sdc1_data_3"))
		pr_err("failed to request gpio sdc1_data_3\n");
	if (gpio_request(52, "sdc1_data_2"))
		pr_err("failed to request gpio sdc1_data_2\n");
	if (gpio_request(53, "sdc1_data_1"))
		pr_err("failed to request gpio sdc1_data_1\n");
	if (gpio_request(54, "sdc1_data_0"))
		pr_err("failed to request gpio sdc1_data_0\n");
	if (gpio_request(55, "sdc1_cmd"))
		pr_err("failed to request gpio sdc1_cmd\n");
	if (gpio_request(56, "sdc1_clk"))
		pr_err("failed to request gpio sdc1_clk\n");

	/* SDC2 GPIOs */
	if (gpio_request(62, "sdc2_clk"))
		pr_err("failed to request gpio sdc2_clk\n");
	if (gpio_request(63, "sdc2_cmd"))
		pr_err("failed to request gpio sdc2_cmd\n");
	if (gpio_request(64, "sdc2_data_3"))
		pr_err("failed to request gpio sdc2_data_3\n");
	if (gpio_request(65, "sdc2_data_2"))
		pr_err("failed to request gpio sdc2_data_2\n");
	if (gpio_request(66, "sdc2_data_1"))
		pr_err("failed to request gpio sdc2_data_1\n");
	if (gpio_request(67, "sdc2_data_0"))
		pr_err("failed to request gpio sdc2_data_0\n");

	/* SDC3 GPIOs */
	if (gpio_request(88, "sdc3_clk"))
		pr_err("failed to request gpio sdc3_clk\n");
	if (gpio_request(89, "sdc3_cmd"))
		pr_err("failed to request gpio sdc3_cmd\n");
	if (gpio_request(90, "sdc3_data_3"))
		pr_err("failed to request gpio sdc3_data_3\n");
	if (gpio_request(91, "sdc3_data_2"))
		pr_err("failed to request gpio sdc3_data_2\n");
	if (gpio_request(92, "sdc3_data_1"))
		pr_err("failed to request gpio sdc3_data_1\n");
	if (gpio_request(93, "sdc3_data_0"))
		pr_err("failed to request gpio sdc3_data_0\n");

}

static unsigned int vreg_enabled;
static unsigned sdcc_cfg_data[][6] = {
	/* SDC1 configs */
	{
	GPIO_CFG(51, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(52, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(53, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(54, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(55, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(56, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	},
	/* SDC2 configs */
	{
	GPIO_CFG(62, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	GPIO_CFG(63, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(64, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(65, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(66, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(67, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
	/* SDC3 configs */
	{
	GPIO_CFG(88, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	GPIO_CFG(89, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(90, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(91, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(92, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(93, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	},
};

static int msm_sdcc_setup_gpio(int dev_id, unsigned enable)
{
	int i, rc;
	for (i = 0; i < ARRAY_SIZE(sdcc_cfg_data[dev_id - 1]); i++) {
		rc = gpio_tlmm_config(sdcc_cfg_data[dev_id - 1][i],
			enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc)
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, sdcc_cfg_data[dev_id - 1][i], rc);
	}
	return rc;
}

static int msm_sdcc_setup_power(int dev_id, int on)
{
	int rc = 0;
	struct vreg *vreg_mmc;

	rc = msm_sdcc_setup_gpio(dev_id, on);
	if (rc)
		return -EIO;

	if (on == vreg_enabled)
			return 0;

	vreg_mmc = vreg_get(NULL, "gp5");
	if (IS_ERR(vreg_mmc)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_mmc));
		return PTR_ERR(vreg_mmc);
	}

	rc = on ? vreg_enable(vreg_mmc) : vreg_disable(vreg_mmc);
	if (rc) {
		printk(KERN_ERR "%s: Failed to configure vreg (%d)\n",
					__func__, rc);
		return rc;
	}

	vreg_enabled = on;
	return 0;
}

static struct mmc_platform_data comet_sdcc_data = {
	.ocr_mask	= MMC_VDD_27_28 | MMC_VDD_28_29,
	.setup_power	= msm_sdcc_setup_power,
};

static void __init comet_init_mmc(void)
{
	sdcc_gpio_init();
	msm_add_sdcc(1, &comet_sdcc_data);
	msm_add_sdcc(3, &comet_sdcc_data);
}

static void __init comet_init(void)
{
	msm_acpu_clock_init(&comet_clock_data);

	platform_add_devices(devices, ARRAY_SIZE(devices));
	bt_power_init();
	msm_fb_add_devices();
	comet_init_mmc();
	i2c_register_board_info(0, msm_i2c_board_info,
				ARRAY_SIZE(msm_i2c_board_info));
	spi_register_board_info(msm_spi_board_info,
				ARRAY_SIZE(msm_spi_board_info));
}

static void __init comet_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;

	addr = alloc_bootmem(android_pmem_pdata.size);
	android_pmem_pdata.start = __pa(addr);

	size = MSM_FB_SIZE;
	addr = alloc_bootmem(size);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical) for fb\n",
		size, addr, __pa(addr));
}

static void __init comet_map_io(void)
{
	msm_map_comet_io();
	comet_allocate_memory_regions();
	msm_clock_init(msm_clocks_8x50, msm_num_clocks_8x50);
}

MACHINE_START(COMET, "Comet Board (QCT 8x50)")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = 0x0,
	.map_io = comet_map_io,
	.init_irq = comet_init_irq,
	.init_machine = comet_init,
	.timer = &msm_timer,
MACHINE_END
