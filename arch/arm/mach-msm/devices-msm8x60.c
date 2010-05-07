/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/irqs.h>
#include <mach/dma.h>
#include <asm/mach/mmc.h>
#include "clock.h"
#include "clock-8x60.h"

/* Address of GSBI blocks */
#define MSM_GSBI1_PHYS	0x16000000
#define MSM_GSBI2_PHYS	0x16100000
#define MSM_GSBI3_PHYS	0x16200000
#define MSM_GSBI4_PHYS	0x16300000
#define MSM_GSBI5_PHYS	0x16400000
#define MSM_GSBI6_PHYS	0x16500000
#define MSM_GSBI7_PHYS	0x16600000
#define MSM_GSBI8_PHYS	0x19800000
#define MSM_GSBI9_PHYS	0x19900000
#define MSM_GSBI10_PHYS	0x19A00000
#define MSM_GSBI11_PHYS	0x19B00000
#define MSM_GSBI12_PHYS	0x19C00000

/* GSBI QUPe devices */
#define MSM_GSBI1_QUP_I2C_PHYS	0x16080000
#define MSM_GSBI2_QUP_I2C_PHYS	0x16180000
#define MSM_GSBI3_QUP_I2C_PHYS	0x16280000
#define MSM_GSBI4_QUP_I2C_PHYS	0x16380000
#define MSM_GSBI5_QUP_I2C_PHYS	0x16480000
#define MSM_GSBI6_QUP_I2C_PHYS	0x16580000
#define MSM_GSBI7_QUP_I2C_PHYS	0x16680000
#define MSM_GSBI8_QUP_I2C_PHYS	0x19880000
#define MSM_GSBI9_QUP_I2C_PHYS	0x19980000
#define MSM_GSBI10_QUP_I2C_PHYS	0x19A80000
#define MSM_GSBI11_QUP_I2C_PHYS	0x19B80000
#define MSM_GSBI12_QUP_I2C_PHYS	0x19C80000

static struct resource gsbi3_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI3_QUP_I2C_PHYS,
		.end	= MSM_GSBI3_QUP_I2C_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI3_PHYS,
		.end	= MSM_GSBI3_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI3_QUP_IRQ,
		.end	= GSBI3_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource gsbi4_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI4_QUP_I2C_PHYS,
		.end	= MSM_GSBI4_QUP_I2C_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI4_PHYS,
		.end	= MSM_GSBI4_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI4_QUP_IRQ,
		.end	= GSBI4_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource gsbi7_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI7_QUP_I2C_PHYS,
		.end	= MSM_GSBI7_QUP_I2C_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI7_PHYS,
		.end	= MSM_GSBI7_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI7_QUP_IRQ,
		.end	= GSBI7_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource gsbi8_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI8_QUP_I2C_PHYS,
		.end	= MSM_GSBI8_QUP_I2C_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI8_PHYS,
		.end	= MSM_GSBI8_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI8_QUP_IRQ,
		.end	= GSBI8_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource gsbi9_qup_i2c_resources[] = {
	{
		.name	= "qup_phys_addr",
		.start	= MSM_GSBI9_QUP_I2C_PHYS,
		.end	= MSM_GSBI9_QUP_I2C_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI9_PHYS,
		.end	= MSM_GSBI9_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "qup_err_intr",
		.start	= GSBI9_QUP_IRQ,
		.end	= GSBI9_QUP_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

/* Use GSBI3 QUP for /dev/i2c-0 */
struct platform_device msm_gsbi3_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(gsbi3_qup_i2c_resources),
	.resource	= gsbi3_qup_i2c_resources,
};

/* Use GSBI4 QUP for /dev/i2c-1 */
struct platform_device msm_gsbi4_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(gsbi4_qup_i2c_resources),
	.resource	= gsbi4_qup_i2c_resources,
};

/* Use GSBI8 QUP for /dev/i2c-3 */
struct platform_device msm_gsbi8_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(gsbi8_qup_i2c_resources),
	.resource	= gsbi8_qup_i2c_resources,
};

/* Use GSBI9 QUP for /dev/i2c-2 */
struct platform_device msm_gsbi9_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(gsbi9_qup_i2c_resources),
	.resource	= gsbi9_qup_i2c_resources,
};

/* Use GSBI7 QUP for /dev/i2c-4 (Marimba) */
struct platform_device msm_gsbi7_qup_i2c_device = {
	.name		= "qup_i2c",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(gsbi7_qup_i2c_resources),
	.resource	= gsbi7_qup_i2c_resources,
};

#ifdef CONFIG_I2C_SSBI
/* 8058 PMIC SSBI on /dev/i2c-6 */
#define MSM_SSBI1_PMIC1C_PHYS	0x00500000
static struct resource msm_ssbi1_resources[] = {
	{
		.name   = "ssbi_base",
		.start	= MSM_SSBI1_PMIC1C_PHYS,
		.end	= MSM_SSBI1_PMIC1C_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi1 = {
	.name		= "i2c_ssbi",
	.id		= 6,
	.num_resources	= ARRAY_SIZE(msm_ssbi1_resources),
	.resource	= msm_ssbi1_resources,
};

/* 8901 PMIC SSBI on /dev/i2c-7 */
#define MSM_SSBI2_PMIC2B_PHYS	0x00C00000
static struct resource msm_ssbi2_resources[] = {
	{
		.name   = "ssbi_base",
		.start	= MSM_SSBI2_PMIC2B_PHYS,
		.end	= MSM_SSBI2_PMIC2B_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi2 = {
	.name		= "i2c_ssbi",
	.id		= 7,
	.num_resources	= ARRAY_SIZE(msm_ssbi2_resources),
	.resource	= msm_ssbi2_resources,
};

/* CODEC SSBI on /dev/i2c-8 */
#define MSM_SSBI3_PHYS  0x18700000
static struct resource msm_ssbi3_resources[] = {
	{
		.name   = "ssbi_base",
		.start  = MSM_SSBI3_PHYS,
		.end    = MSM_SSBI3_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi3 = {
	.name		= "i2c_ssbi",
	.id		= 8,
	.num_resources	= ARRAY_SIZE(msm_ssbi3_resources),
	.resource	= msm_ssbi3_resources,
};
#endif /* CONFIG_I2C_SSBI */

#define MSM_SDC1_BASE         0x12400000
#define MSM_SDC2_BASE         0x12140000
#define MSM_SDC3_BASE         0x12180000
#define MSM_SDC4_BASE         0x121C0000
#define MSM_SDC5_BASE         0x12200000

static struct resource resources_sdc1[] = {
	{
		.start	= MSM_SDC1_BASE,
		.end	= MSM_SDC1_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= SDC1_IRQ_0,
		.end	= SDC1_IRQ_0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_SDC1_CHAN,
		.end	= DMOV_SDC1_CHAN,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc2[] = {
	{
		.start	= MSM_SDC2_BASE,
		.end	= MSM_SDC2_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= SDC2_IRQ_0,
		.end	= SDC2_IRQ_0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_SDC2_CHAN,
		.end	= DMOV_SDC2_CHAN,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc3[] = {
	{
		.start	= MSM_SDC3_BASE,
		.end	= MSM_SDC3_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= SDC3_IRQ_0,
		.end	= SDC3_IRQ_0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_SDC3_CHAN,
		.end	= DMOV_SDC3_CHAN,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc4[] = {
	{
		.start	= MSM_SDC4_BASE,
		.end	= MSM_SDC4_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= SDC4_IRQ_0,
		.end	= SDC4_IRQ_0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_SDC4_CHAN,
		.end	= DMOV_SDC4_CHAN,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc5[] = {
	{
		.start	= MSM_SDC5_BASE,
		.end	= MSM_SDC5_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= SDC5_IRQ_0,
		.end	= SDC5_IRQ_0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= DMOV_SDC5_CHAN,
		.end	= DMOV_SDC5_CHAN,
		.flags	= IORESOURCE_DMA,
	},
};

struct platform_device msm_device_sdc1 = {
	.name		= "msm_sdcc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resources_sdc1),
	.resource	= resources_sdc1,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc2 = {
	.name		= "msm_sdcc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(resources_sdc2),
	.resource	= resources_sdc2,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc3 = {
	.name		= "msm_sdcc",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(resources_sdc3),
	.resource	= resources_sdc3,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc4 = {
	.name		= "msm_sdcc",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(resources_sdc4),
	.resource	= resources_sdc4,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc5 = {
	.name		= "msm_sdcc",
	.id		= 5,
	.num_resources	= ARRAY_SIZE(resources_sdc5),
	.resource	= resources_sdc5,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct platform_device *msm_sdcc_devices[] __initdata = {
	&msm_device_sdc1,
	&msm_device_sdc2,
	&msm_device_sdc3,
	&msm_device_sdc4,
	&msm_device_sdc5,
};

int __init msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat)
{
	struct platform_device	*pdev;

	if (controller < 1 || controller > 5)
		return -EINVAL;

	pdev = msm_sdcc_devices[controller-1];
	pdev->dev.platform_data = plat;
	return platform_device_register(pdev);
}

static struct resource resources_otg[] = {
	{
		.start	= 0x12500000,
		.end	= 0x12500000 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= USB1_HS_IRQ,
		.end	= USB1_HS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_otg = {
	.name		= "msm_otg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_otg),
	.resource	= resources_otg,
};

static u64 dma_mask = 0xffffffffULL;
struct platform_device msm_device_gadget_peripheral = {
	.name		= "msm_hsusb",
	.id		= -1,
	.dev		= {
		.dma_mask 		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

struct clk msm_clocks_8x60[] = {
	CLK_8X60("bbrx_ssbi_clk",	BBRX_SSBI_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI1_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI2_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI3_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI4_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI5_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI6_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI7_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI8_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI9_UART_CLK,		NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI10_UART_CLK,	NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI11_UART_CLK,	NULL, 0),
	CLK_8X60("gsbi_uart_clk",	GSBI12_UART_CLK,	NULL, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI1_QUP_CLK,		NULL, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI2_QUP_CLK,		NULL, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI3_QUP_CLK,
					&msm_gsbi3_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI4_QUP_CLK,
					&msm_gsbi4_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI5_QUP_CLK,		NULL, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI6_QUP_CLK,		NULL, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI7_QUP_CLK,
					&msm_gsbi7_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI8_QUP_CLK,
					&msm_gsbi8_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI9_QUP_CLK,
					&msm_gsbi9_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI10_QUP_CLK,		NULL, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI11_QUP_CLK,		NULL, 0),
	CLK_8X60("gsbi_qup_clk",	GSBI12_QUP_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_src_clk",	GSBI_SIM_SRC_CLK,	NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI1_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI2_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI3_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI4_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI5_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI6_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI7_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI8_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI9_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI10_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI11_SIM_CLK,		NULL, 0),
	CLK_8X60("gsbi_sim_clk",	GSBI12_SIM_CLK,		NULL, 0),
	CLK_8X60("pdm_clk",		PDM_CLK,		NULL, 0),
	CLK_8X60("prng_clk",		PRNG_CLK,		NULL, 0),
	CLK_8X60("pmic_ssbi2_clk",	PMIC_SSBI2_CLK,		NULL, 0),
	CLK_8X60("sdc_clk",		SDC1_CLK,
					&msm_device_sdc1.dev, 0),
	CLK_8X60("sdc_clk",		SDC2_CLK,
					&msm_device_sdc2.dev, 0),
	CLK_8X60("sdc_clk",		SDC3_CLK,
					&msm_device_sdc3.dev, 0),
	CLK_8X60("sdc_clk",		SDC4_CLK,
					&msm_device_sdc4.dev, 0),
	CLK_8X60("sdc_clk",		SDC5_CLK,
					&msm_device_sdc5.dev, 0),
	CLK_8X60("tsif_ref_clk",	TSIF_REF_CLK,		NULL, 0),
	CLK_8X60("tssc_clk",		TSSC_CLK,		NULL, 0),
	CLK_8X60("usb_hs_clk",		USB_HS_CLK,		NULL, 0),
	CLK_8X60("usb_phy_clk",		USB_PHY0_CLK,		NULL, 0),
	CLK_8X60("usb_fs_src_clk",	USB_FS1_SRC_CLK,	NULL, 0),
	CLK_8X60("usb_fs_clk",		USB_FS1_CLK,		NULL, 0),
	CLK_8X60("usb_fs_sys_clk",	USB_FS1_SYS_CLK,	NULL, 0),
	CLK_8X60("usb_fs_src_clk",	USB_FS2_SRC_CLK,	NULL, 0),
	CLK_8X60("usb_fs_clk",		USB_FS2_CLK,		NULL, 0),
	CLK_8X60("usb_fs_sys_clk",	USB_FS2_SYS_CLK,	NULL, 0),
	CLK_8X60("gsbi_pclk",		GSBI1_P_CLK,		NULL, 0),
	CLK_8X60("gsbi_pclk",		GSBI2_P_CLK,		NULL, 0),
	CLK_8X60("gsbi_pclk",		GSBI3_P_CLK,
					&msm_gsbi3_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_pclk",		GSBI4_P_CLK,
					&msm_gsbi4_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_pclk",		GSBI5_P_CLK,		NULL, 0),
	CLK_8X60("gsbi_pclk",		GSBI6_P_CLK,		NULL, 0),
	CLK_8X60("gsbi_pclk",		GSBI7_P_CLK,
					&msm_gsbi7_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_pclk",		GSBI8_P_CLK,
					&msm_gsbi8_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_pclk",		GSBI9_P_CLK,
					&msm_gsbi9_qup_i2c_device.dev, 0),
	CLK_8X60("gsbi_pclk",		GSBI10_P_CLK,		NULL, 0),
	CLK_8X60("gsbi_pclk",		GSBI11_P_CLK,		NULL, 0),
	CLK_8X60("gsbi_pclk",		GSBI12_P_CLK,		NULL, 0),
	CLK_8X60("tsif_pclk",		TSIF_P_CLK,		NULL, 0),
	CLK_8X60("usb_fs_pclk",		USB_FS1_P_CLK,		NULL, 0),
	CLK_8X60("usb_fs_pclk",		USB_FS2_P_CLK,		NULL, 0),
	CLK_8X60("cam_clk",		CAM_CLK,		NULL, 0),
	CLK_8X60("csi_src_clk",		CSI_SRC_CLK,		NULL, 0),
	CLK_8X60("csi_clk",		CSI0_CLK,		NULL, 0),
	CLK_8X60("csi_clk",		CSI1_CLK,		NULL, 0),
	CLK_8X60("gfx2d_clk",		GFX2D0_CLK,		NULL, 0),
	CLK_8X60("gfx2d_clk",		GFX2D1_CLK,		NULL, 0),
	CLK_8X60("gfx3d_clk",		GFX3D_CLK,		NULL, 0),
	CLK_8X60("ijpeg_clk",		IJPEG_CLK,		NULL, 0),
	CLK_8X60("jpegd_clk",		JPEGD_CLK,		NULL, 0),
	CLK_8X60("mdp_clk",		MDP_CLK,		NULL, 0),
	CLK_8X60("mdp_vsync_clk",	MDP_VSYNC_CLK,		NULL, 0),
	CLK_8X60("pixel_mdp_clk",	PIXEL_MDP_CLK,		NULL, 0),
	CLK_8X60("pixel_lcdc_clk",	PIXEL_LCDC_CLK,		NULL, 0),
	CLK_8X60("rot_clk",		ROT_CLK,		NULL, 0),
	CLK_8X60("tv_src_clk",		TV_SRC_CLK,		NULL, 0),
	CLK_8X60("tv_enc_clk",		TV_ENC_CLK,		NULL, 0),
	CLK_8X60("tv_dac_clk",		TV_DAC_CLK,		NULL, 0),
	CLK_8X60("vcodec_clk",		VCODEC_CLK,		NULL, 0),
	CLK_8X60("mdp_tv_clk",		MDP_TV_CLK,		NULL, 0),
	CLK_8X60("hdmi_tv_clk",		HDMI_TV_CLK,		NULL, 0),
	CLK_8X60("dsub_tv_clk",		DSUB_TV_CLK,		NULL, 0),
	CLK_8X60("vpe_clk",		VPE_CLK,		NULL, 0),
	CLK_8X60("vfe_src_clk",		VFE_SRC_CLK,		NULL, 0),
	CLK_8X60("vfe_clk",		VFE_CLK,		NULL, 0),
	CLK_8X60("cs_vfe_clk",		CS0_VFE_CLK,		NULL, 0),
	CLK_8X60("cs_vfe_clk",		CS1_VFE_CLK,		NULL, 0),
	CLK_8X60("mi2s_src_clk",	MI2S_SRC_CLK,		NULL, 0),
	CLK_8X60("mi2s_clk",		MI2S_CLK,		NULL, 0),
	CLK_8X60("mi2s_m_clk",		MI2S_M_CLK,		NULL, 0),
	CLK_8X60("pcm_clk",		PCM_CLK,		NULL, 0),
};

unsigned msm_num_clocks_8x60 = ARRAY_SIZE(msm_clocks_8x60);
