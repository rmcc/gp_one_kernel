/*
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
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
#include <linux/usb/mass_storage_function.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/mpp.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/sirc.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_touchpad.h>
#include <mach/msm_i2ckbd.h>

#include "devices.h"
#include "timer.h"
#include "keypad-surf-ffa.h"
#include "socinfo.h"

#define MSM_PMEM_MDP_SIZE	0x800000
#define MSM_PMEM_ADSP_SIZE	0x1100000
#define MSM_PMEM_GPU1_SIZE	0x800000
#define MSM_FB_SIZE             0x500000
#define MSM_AUDIO_SIZE		0x200000

#define MSM_SMI_BASE		0x2b00000
#define MSM_SMI_SIZE		0x1500000

#define MSM_FB_BASE		MSM_SMI_BASE
#define MSM_PMEM_GPU0_BASE	(MSM_FB_BASE + MSM_FB_SIZE)
#define MSM_PMEM_GPU0_SIZE	(MSM_SMI_SIZE - MSM_FB_SIZE)

static struct resource smc91x_resources[] = {
	[0] = {
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.flags  = IORESOURCE_IRQ,
	},
};

static struct usb_mass_storage_platform_data usb_mass_storage_pdata = {
	.nluns          = 0x02,
	.buf_size       = 16384,
	.vendor         = "GOOGLE",
	.product        = "Mass storage",
	.release        = 0xffff,
};

static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
		.platform_data          = &usb_mass_storage_pdata,
	},
};

static struct platform_device smc91x_device = {
	.name           = "smc91x",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(smc91x_resources),
	.resource       = smc91x_resources,
};
static struct usb_function_map usb_functions_map[] = {
	{"diag", 0},
	{"adb", 1},
	{"modem", 2},
	{"nmea", 3},
	{"mass_storage", 4},
	{"ethernet", 5},
};

/* dynamic composition */
static struct usb_composition usb_func_composition[] = {
	{
		.product_id         = 0x9012,
		.functions	    = 0x5, /* 0101 */
	},

	{
		.product_id         = 0x9013,
		.functions	    = 0x15, /* 10101 */
	},

	{
		.product_id         = 0x9014,
		.functions	    = 0x30, /* 110000 */
	},

	{
		.product_id         = 0x9016,
		.functions	    = 0xD, /* 01101 */
	},

	{
		.product_id         = 0x9017,
		.functions	    = 0x1D, /* 11101 */
	},

	{
		.product_id         = 0xF000,
		.functions	    = 0x10, /* 10000 */
	},

	{
		.product_id         = 0xF009,
		.functions	    = 0x20, /* 100000 */
	},

	{
		.product_id         = 0x9018,
		.functions	    = 0x1F, /* 011111 */
	},

};
static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.version	= 0x0100,
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_180NM),
	.vendor_id          = 0x5c6,
	.product_name       = "Qualcomm HSUSB Device",
	.serial_number      = "1234567890ABCDEF",
	.manufacturer_name  = "Qualcomm Incorporated",
	.compositions	= usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.function_map   = usb_functions_map,
	.num_functions	= ARRAY_SIZE(usb_functions_map),
	.config_gpio    = NULL,
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.size = MSM_PMEM_MDP_SIZE,
	.no_allocator = 0,
	.cached = 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.size = MSM_PMEM_ADSP_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_gpu0_pdata = {
	.name = "pmem_gpu0",
	.start = MSM_PMEM_GPU0_BASE,
	.size = MSM_PMEM_GPU0_SIZE,
	.no_allocator = 1,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_gpu1_pdata = {
	.name = "pmem_gpu1",
	.no_allocator = 1,
	.cached = 0,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct platform_device android_pmem_gpu0_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_gpu0_pdata },
};

static struct platform_device android_pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 3,
	.dev = { .platform_data = &android_pmem_gpu1_pdata },
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
		.name   = "spi_pwr",
		.start	= 21,
		.end	= 0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_cs0",
		.start	= 22,
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
		.irq		= MSM_GPIO_TO_INT(22),
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 10000000,
	}
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
		if (mddi_clk_list[i] == rate) {
			*clk_rate = mddi_clk_list[i]*1000;
			return 0;
	}

	if (rate > mddi_clk_list[last_idx]) {
		*clk_rate = mddi_clk_list[last_idx]*1000;
		return 0;
	}

	for (i = last_idx ; i > 0 ; i--)
		if ((rate < mddi_clk_list[i]) && (rate > mddi_clk_list[i-1])) {
			*clk_rate = mddi_clk_list[i-1]*1000;
	return 0;
		}

	return -EINVAL;
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

static struct resource msm_audio_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "aux_pcm_dout",
		.start  = 68,
		.end    = 68,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_din",
		.start  = 69,
		.end    = 69,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_syncout",
		.start  = 70,
		.end    = 70,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_clkin_a",
		.start  = 71,
		.end    = 71,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_audio_device = {
	.name   = "msm_audio",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_audio_resources),
	.resource       = msm_audio_resources,
};

static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.wakeup_irq = MSM_GPIO_TO_INT(45),
	.inject_rx_on_wakeup = 1,
	.rx_to_inject = 0x32,
};

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= 21,
		.end	= 21,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= 19,
		.end	= 19,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(21),
		.end	= MSM_GPIO_TO_INT(21),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

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
	&smc91x_device,
	&msm_device_smd,
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_gpu0_device,
	&android_pmem_gpu1_device,
	&msm_device_nand,
	&msm_device_i2c,
	&qsd_device_spi,
	&msm_device_hsusb_otg,
	&msm_device_hsusb_host,
	&msm_device_hsusb_peripheral,
	&mass_storage_device,
	&msm_device_tssc,
	&msm_audio_device,
	&msm_device_uart_dm1,
	&msm_bluesleep_device,
#ifdef CONFIG_BT
	&msm_bt_power_device,
#endif
#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
	&msm_device_uart3,
#endif
};

static struct msm_acpu_clock_platform_data qsd8x50_clock_data = {
	.acpu_switch_time_us = 20,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.power_collapse_khz = 128000000,
	.wait_for_irq_khz = 128000000,
};

static struct msm_touchpad_platform_data msm_touchpad_data = {
	.gpioirq     = 38,
	.gpiosuspend = 34,
};

static struct msm_i2ckbd_platform_data msm_kybd_data = {
	.hwrepeat = 0,
	.scanset1 = 1,
	.gpioreset = 35,
	.gpioirq = 36,
};

static struct i2c_board_info msm_i2c_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("glidesensor", 0x2A),
		.irq           =  MSM_GPIO_TO_INT(38),
		.platform_data = &msm_touchpad_data
	},
	{
		I2C_BOARD_INFO("msm-i2ckbd", 0x3A),
		.type           = "msm-i2ckbd",
		.irq           =  MSM_GPIO_TO_INT(36),
		.platform_data  = &msm_kybd_data
	},
	{
		I2C_BOARD_INFO("mt9d112", 0x78 >> 1),
	},
	{
		I2C_BOARD_INFO("mt9t013", 0x6C),
	},
};

static uint32_t camera_off_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(0,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(2,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	GPIO_CFG(3,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
	GPIO_CFG(4,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(5,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(6,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(7,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(8,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
	GPIO_CFG(9,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
	GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
	GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
	GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
	GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
	GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), /* MCLK */
};

static uint32_t camera_on_gpio_table[] = {
   /* parallel CAMERA interfaces */
   GPIO_CFG(0,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
   GPIO_CFG(1,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
   GPIO_CFG(2,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
   GPIO_CFG(3,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
   GPIO_CFG(4,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
   GPIO_CFG(5,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
   GPIO_CFG(6,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
   GPIO_CFG(7,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
   GPIO_CFG(8,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
   GPIO_CFG(9,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
   GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT10 */
   GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT11 */
   GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
   GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
   GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
   GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA), /* MCLK */
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static void config_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
}

static void config_camera_off_gpios(void)
{
  config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

static struct msm_camera_sensor_info msm_camera_sensor[2] = {
	{
		.sensor_reset   = 17,
		.sensor_pwd	  = 85,
		.vcm_pwd      = 0,
		.sensor_name  = "mt9d112",
	},
	{
		.sensor_reset   = 17,
		.sensor_pwd	= 85,
		.vcm_pwd      = 0,
		.sensor_name  = "mt9t013",
	},
};

static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.snum = 2,
	.sinfo = &msm_camera_sensor[0],
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
};

static struct resource msm_camera_resources[] = {
	{
		.start	= 0xA0F00000,
		.end	= 0xA0F00000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VFE,
		.end	= INT_VFE,
		.flags	= IORESOURCE_IRQ,
	},
};

static void __init msm_camera_add_device(void)
{
	msm_camera_register_device(&msm_camera_resources,
		ARRAY_SIZE(msm_camera_resources), &msm_camera_device_data);

	config_camera_off_gpios();
}

static void __init qsd8x50_init_irq(void)
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

	/* SDC4 GPIOs */
	if (gpio_request(142, "sdc4_clk"))
		pr_err("failed to request gpio sdc4_clk\n");
	if (gpio_request(143, "sdc4_cmd"))
		pr_err("failed to request gpio sdc4_cmd\n");
	if (gpio_request(144, "sdc4_data_0"))
		pr_err("failed to request gpio sdc4_data_0\n");
	if (gpio_request(145, "sdc4_data_1"))
		pr_err("failed to request gpio sdc4_data_1\n");
	if (gpio_request(146, "sdc4_data_2"))
		pr_err("failed to request gpio sdc4_data_2\n");
	if (gpio_request(147, "sdc4_data_3"))
		pr_err("failed to request gpio sdc4_data_3\n");
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
	/* SDC4 configs */
	{
	GPIO_CFG(142, 3, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	GPIO_CFG(143, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(144, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(145, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(146, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(147, 3, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_8MA),
	}
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
	struct mpp *mpp_mmc;

	rc = msm_sdcc_setup_gpio(dev_id, on);
	if (rc)
		return -EIO;

	if (on == vreg_enabled)
			return 0;

	if (machine_is_qsd8x50_ffa()) {
		mpp_mmc = mpp_get(NULL, "mpp3");
		if (!mpp_mmc) {
			printk(KERN_ERR "%s: mpp get failed\n", __func__);
			return -EIO;
		}

		rc = mpp_config_digital_out(mpp_mmc,
		     MPP_CFG(MPP_DLOGIC_LVL_MSMP,
		     on ? MPP_DLOGIC_OUT_CTRL_HIGH : MPP_DLOGIC_OUT_CTRL_LOW));
		if (rc) {
			printk(KERN_ERR
				"%s: Failed to configure mpp (%d)\n",
					__func__, rc);
			return rc;
		}
	} else {
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
	}

	vreg_enabled = on;
	return 0;
}

static struct mmc_platform_data qsd8x50_sdcc_data = {
	.ocr_mask	= MMC_VDD_27_28 | MMC_VDD_28_29,
	.setup_power	= msm_sdcc_setup_power,
};

static void __init qsd8x50_init_mmc(void)
{
	sdcc_gpio_init();
	msm_add_sdcc(1, &qsd8x50_sdcc_data);

	if (machine_is_qsd8x50_surf()) {
		msm_add_sdcc(2, &qsd8x50_sdcc_data);
		msm_add_sdcc(3, &qsd8x50_sdcc_data);
		msm_add_sdcc(4, &qsd8x50_sdcc_data);
	}

}

static void __init qsd8x50_cfg_smc91x(void)
{
	int rc = 0;

	if (machine_is_qsd8x50_surf()) {
		smc91x_resources[0].start = 0x70000300;
		smc91x_resources[0].end = 0x70000400;
		smc91x_resources[1].start = MSM_GPIO_TO_INT(156);
		smc91x_resources[1].end = MSM_GPIO_TO_INT(156);
	}
	else if (machine_is_qsd8x50_ffa()) {
		smc91x_resources[0].start = 0x84000300;
		smc91x_resources[0].end = 0x84000400;
		smc91x_resources[1].start = MSM_GPIO_TO_INT(87);
		smc91x_resources[1].end = MSM_GPIO_TO_INT(87);

		rc = gpio_tlmm_config(GPIO_CFG(87, 0, GPIO_INPUT,
					       GPIO_PULL_DOWN, GPIO_2MA),
					       GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config=%d\n",
					__func__, rc);
		}
	}
	else printk(KERN_ERR "%s: running on invalid machine type\n", __func__);
}

static void __init qsd8x50_init(void)
{
	socinfo_init();
	qsd8x50_cfg_smc91x();
	msm_acpu_clock_init(&qsd8x50_clock_data);
	msm_device_hsusb_peripheral.dev.platform_data = &msm_hsusb_pdata;
	msm_device_hsusb_host.dev.platform_data = &msm_hsusb_pdata;
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
	platform_add_devices(devices, ARRAY_SIZE(devices));
	msm_fb_add_devices();
	msm_camera_add_device();
#if defined(CONFIG_QSD_GPIO_KEYPAD)
	init_surf_ffa_keypad(machine_is_qsd8x50_ffa());
#endif
	qsd8x50_init_mmc();
	bt_power_init();
	i2c_register_board_info(0, msm_i2c_board_info,
				ARRAY_SIZE(msm_i2c_board_info));
	spi_register_board_info(msm_spi_board_info,
				ARRAY_SIZE(msm_spi_board_info));
}

static void __init qsd8x50_allocate_memory_regions(void)
{
	void *addr, *addr_1m_aligned;
	unsigned long size;

	size = MSM_PMEM_MDP_SIZE;
	addr = alloc_bootmem(size);
	android_pmem_pdata.start = __pa(addr);
	android_pmem_pdata.size = size;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	       "for pmem\n", size, addr, __pa(addr));

	size = MSM_PMEM_ADSP_SIZE;
	addr = alloc_bootmem(size);
	android_pmem_adsp_pdata.start = __pa(addr);
	android_pmem_adsp_pdata.size = size;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	       "for adsp pmem\n", size, addr, __pa(addr));

	/* The GPU1 area must be aligned to a 1M boundary */
	/* XXX For now allocate an extra 1M, use the aligned part, */
	/* waste the extra memory */
	size = MSM_PMEM_GPU1_SIZE + 0x100000 - PAGE_SIZE;
	addr = alloc_bootmem(size);
	addr_1m_aligned =
		(void *)(((unsigned int)addr + 0x100000) & 0xfff00000);
	size = MSM_PMEM_GPU1_SIZE;
	android_pmem_gpu1_pdata.start = __pa(addr_1m_aligned);
	android_pmem_gpu1_pdata.size = size;
	printk(KERN_INFO
		"allocating %lu bytes at %p (%lx physical) for gpu1 pmem\n",
		size, addr_1m_aligned, __pa(addr_1m_aligned));

	size = MSM_FB_SIZE;
	addr = MSM_FB_BASE;
	msm_fb_resources[0].start = addr;
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	printk(KERN_INFO "using %lu bytes of SMI at %lx physical for fb\n",
		size, addr);

	size = MSM_AUDIO_SIZE;
	addr = alloc_bootmem(size);
	msm_audio_resources[0].start = __pa(addr);
	msm_audio_resources[0].end = __pa(addr) + MSM_AUDIO_SIZE;
	printk(KERN_INFO "allocating %lu bytes at %p (%lx physical)"
	       "for audio\n", size, addr, __pa(addr));
}

static void __init qsd8x50_map_io(void)
{
	msm_map_qsd8x50_io();
	qsd8x50_allocate_memory_regions();
	msm_clock_init(msm_clocks_8x50, msm_num_clocks_8x50);
}

MACHINE_START(SWORDFISH, "swordfish")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = 0x16000100,
	.map_io = qsd8x50_map_io,
	.init_irq = qsd8x50_init_irq,
	.init_machine = qsd8x50_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(QSD8X50_SURF, "QCT QSD8X50 SURF")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = 0x16000100,
	.map_io = qsd8x50_map_io,
	.init_irq = qsd8x50_init_irq,
	.init_machine = qsd8x50_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(QSD8X50_FFA, "QCT QSD8X50 FFA")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = 0x16000100,
	.map_io = qsd8x50_map_io,
	.init_irq = qsd8x50_init_irq,
	.init_machine = qsd8x50_init,
	.timer = &msm_timer,
MACHINE_END
