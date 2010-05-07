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
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/mfd/pmic8058.h>

#include <linux/i2c.h>
#include <linux/smsc911x.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>

#include <mach/board.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <asm/mach/mmc.h>
#include <mach/tlmm.h>

#include "devices.h"
#include "timer.h"

/* Macros assume PMIC GPIOs start at 0 */
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio + NR_GPIO_IRQS)
#define PM8058_GPIO_SYS_TO_PM(sys_gpio)		(sys_gpio - NR_GPIO_IRQS)
#define PM8058_IRQ_BASE				(NR_MSM_IRQS + NR_GPIO_IRQS)

void __iomem *gic_cpu_base_addr;

/*
 * The smc91x configuration varies depending on platform.
 * The resources data structure is filled in at runtime.
 */
static struct resource smc91x_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name          = "smc91x",
	.id            = 0,
	.num_resources = ARRAY_SIZE(smc91x_resources),
	.resource      = smc91x_resources,
};

static struct resource smsc911x_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = 0x1b800000,
		.end   = 0x1b8000ff
	},
	[1] = {
		.flags = IORESOURCE_IRQ,
		.start = TLMM_SCSS_DIR_CONN_IRQ_0,
		.end   = TLMM_SCSS_DIR_CONN_IRQ_0
	},
};

static struct smsc911x_platform_config smsc911x_config = {
	.irq_polarity = SMSC911X_IRQ_POLARITY_ACTIVE_HIGH,
	.irq_type     = SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags        = SMSC911X_USE_16BIT
};

static struct platform_device smsc911x_device = {
	.name          = "smsc911x",
	.id            = 0,
	.num_resources = ARRAY_SIZE(smsc911x_resources),
	.resource      = smsc911x_resources,
	.dev           = {
		.platform_data = &smsc911x_config
	}
};

#ifdef CONFIG_I2C_QUP
static void gsbi_qup_i2c_gpio_config(int adap_id, int config_type)
{
}

static struct msm_i2c_platform_data msm_gsbi3_qup_i2c_pdata = {
	.clk_freq = 100000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi4_qup_i2c_pdata = {
	.clk_freq = 100000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi7_qup_i2c_pdata = {
	.clk_freq = 100000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi8_qup_i2c_pdata = {
	.clk_freq = 100000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi9_qup_i2c_pdata = {
	.clk_freq = 100000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};
#endif

#ifdef CONFIG_I2C_SSBI
/* PMIC SSBI */
static struct msm_ssbi_platform_data msm_ssbi1_pdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
};

/* PMIC SSBI */
static struct msm_ssbi_platform_data msm_ssbi2_pdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
};

/* CODEC/TSSC SSBI */
static struct msm_ssbi_platform_data msm_ssbi3_pdata = {
	.controller_type = MSM_SBI_CTRL_SSBI,
};
#endif

static struct platform_device *rumi_sim_devices[] __initdata = {
	&smc91x_device,
#ifdef CONFIG_I2C_QUP
	&msm_gsbi3_qup_i2c_device,
	&msm_gsbi4_qup_i2c_device,
	&msm_gsbi7_qup_i2c_device,
	&msm_gsbi8_qup_i2c_device,
	&msm_gsbi9_qup_i2c_device,
#endif
#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi1,
	&msm_device_ssbi2,
	&msm_device_ssbi3,
#endif
};

static struct platform_device *surf_devices[] __initdata = {
	&smsc911x_device,
#ifdef CONFIG_I2C_QUP
	&msm_gsbi3_qup_i2c_device,
	&msm_gsbi4_qup_i2c_device,
	&msm_gsbi7_qup_i2c_device,
	&msm_gsbi8_qup_i2c_device,
	&msm_gsbi9_qup_i2c_device,
#endif
#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi1,
	&msm_device_ssbi2,
	&msm_device_ssbi3,
#endif
};

#ifdef CONFIG_I2C
static struct i2c_board_info __initdata msm8x60_i2c_gsbi8_info[] = {
};
#endif

#ifdef CONFIG_PMIC8058

int pm8058_gpios_init(struct pm8058_chip *pm_chip)
{
	return 0;
}

#define PM8058_GPIO_INT           88

static struct pm8058_gpio_platform_data pm8058_gpio_data = {
	.gpio_base	= PM8058_GPIO_PM_TO_SYS(0),
	.irq_base	= PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 0)
};

static struct pm8058_gpio_platform_data pm8058_mpp_data = {
	.gpio_base	= PM8058_GPIO_PM_TO_SYS(PM8058_GPIOS),
	.irq_base	= PM8058_MPP_IRQ(PM8058_IRQ_BASE, 0),
};

static struct mfd_cell pm8058_subdevs[] = {
	{	.name = "pm8058-gpio",
		.platform_data	= &pm8058_gpio_data,
		.data_size	= sizeof(pm8058_gpio_data),
	},
	{	.name = "pm8058-mpp",
		.platform_data	= &pm8058_mpp_data,
		.data_size	= sizeof(pm8058_mpp_data),
	},
};

static struct pm8058_platform_data pm8058_platform_data = {
	.irq_base = PM8058_IRQ_BASE,
	.init = pm8058_gpios_init,

	.num_subdevs = ARRAY_SIZE(pm8058_subdevs),
	.sub_devices = pm8058_subdevs,
};

static struct i2c_board_info pm8058_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("pm8058-core", 0),
		.irq = MSM_GPIO_TO_INT(PM8058_GPIO_INT),
		.platform_data = &pm8058_platform_data,
	},
};

#endif /* CONFIG_PMIC8058 */


unsigned long clk_get_max_axi_khz(void)
{
	return 0;
}

static void __init msm8x60_init_buses(void)
{
#ifdef CONFIG_I2C_QUP
	msm_gsbi3_qup_i2c_device.dev.platform_data = &msm_gsbi3_qup_i2c_pdata;
	msm_gsbi4_qup_i2c_device.dev.platform_data = &msm_gsbi4_qup_i2c_pdata;
	msm_gsbi7_qup_i2c_device.dev.platform_data = &msm_gsbi7_qup_i2c_pdata;
	msm_gsbi8_qup_i2c_device.dev.platform_data = &msm_gsbi8_qup_i2c_pdata;
	msm_gsbi9_qup_i2c_device.dev.platform_data = &msm_gsbi9_qup_i2c_pdata;
#endif
#ifdef CONFIG_I2C_SSBI
	msm_device_ssbi1.dev.platform_data = &msm_ssbi1_pdata;
	msm_device_ssbi2.dev.platform_data = &msm_ssbi2_pdata;
	msm_device_ssbi3.dev.platform_data = &msm_ssbi3_pdata;
#endif
}

static void __init msm8x60_map_io(void)
{
	msm_map_msm8x60_io();
	msm_clock_init(msm_clocks_8x60, msm_num_clocks_8x60);
}

static void __init msm8x60_init_irq(void)
{
	unsigned int i;

	gic_dist_init(0, MSM_QGIC_DIST_BASE, GIC_PPI_START);
	gic_cpu_base_addr = (void *)MSM_QGIC_CPU_BASE;
	gic_cpu_init(0, MSM_QGIC_CPU_BASE);

	/* Edge trigger PPIs except AVS_SVICINT and AVS_SVICINTSWDONE */
	writel(0xFFFFD7FF, MSM_QGIC_DIST_BASE + GIC_DIST_CONFIG + 4);

	/* RUMI does not adhere to GIC spec by enabling STIs by default.
	 * Enable/clear is supposed to be RO for STIs, but is RW on RUMI.
	 */
	if (machine_is_msm8x60_surf() ||
	    machine_is_msm8x60_ffa()  ||
	    machine_is_msm8x60_rumi3())
		writel(0x0000FFFF, MSM_QGIC_DIST_BASE + GIC_DIST_ENABLE_SET);

	/* FIXME: Not installing AVS_SVICINT and AVS_SVICINTSWDONE yet
	 * as they are configured as level, which does not play nice with
	 * handle_percpu_irq.
	 */
	for (i = GIC_PPI_START; i < GIC_SPI_START; i++) {
		if (i != AVS_SVICINT && i != AVS_SVICINTSWDONE)
			set_irq_handler(i, handle_percpu_irq);
	}
}

/*
 * Most segments of the EBI2 bus are disabled by default.
 */
static void __init msm8x60_init_ebi2(void)
{
	uint32_t ebi2_cfg;
	void *ebi2_cfg_ptr;

	ebi2_cfg_ptr = ioremap_nocache(0x1a100000, sizeof(uint32_t));
	if (ebi2_cfg_ptr != 0) {
		ebi2_cfg = readl(ebi2_cfg_ptr);

		if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa())
			ebi2_cfg |= (1 << 4) | (1 << 5); /* CS2, CS3 */
		else if (machine_is_msm8x60_sim())
			ebi2_cfg |= (1 << 4); /* CS2 */
		else if (machine_is_msm8x60_rumi3())
			ebi2_cfg |= (1 << 5); /* CS3 */

		writel(ebi2_cfg, ebi2_cfg_ptr);
		iounmap(ebi2_cfg_ptr);
	}

	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa()) {
		ebi2_cfg_ptr = ioremap_nocache(0x1a110000, SZ_4K);
		if (ebi2_cfg_ptr != 0) {
			/* EBI2_XMEM_CFG:PWRSAVE_MODE off */
			writel(0UL, ebi2_cfg_ptr);

			/* CS2: Delay 9 cycles (140ns@64MHz) between SMSC
			 * LAN9221 Ethernet controller reads and writes.
			 * The lowest 4 bits are the read delay, the next
			 * 4 are the write delay. */
			writel(0x031F1C99, ebi2_cfg_ptr + 0x10);

			/* EBI2 CS3 muxed address/data,
			 * two cyc addr enable */
			writel(0xA3030020, ebi2_cfg_ptr + 0x34);
			iounmap(ebi2_cfg_ptr);
		}
	}
}

static void __init msm8x60_configure_smc91x(void)
{
	if (machine_is_msm8x60_sim()) {

		smc91x_resources[0].start = 0x1b800300;
		smc91x_resources[0].end   = 0x1b8003ff;

		smc91x_resources[1].start = (NR_MSM_IRQS + 40);
		smc91x_resources[1].end   = (NR_MSM_IRQS + 40);

	} else if (machine_is_msm8x60_rumi3()) {

		smc91x_resources[0].start = 0x1d000300;
		smc91x_resources[0].end   = 0x1d0003ff;

		smc91x_resources[1].start = TLMM_SCSS_DIR_CONN_IRQ_0;
		smc91x_resources[1].end   = TLMM_SCSS_DIR_CONN_IRQ_0;
	}
}

struct msm8x60_tlmm_cfg_struct {
	unsigned gpio;
	u32      flags;
};

static uint32_t msm8x60_tlmm_cfgs[] = {
	/*
	 * GSBI8
	 */
	GPIO_CFG(64, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(65, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	/*
	 * EBI2
	 */
	/* address lines */
	GPIO_CFG(123, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(124, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(125, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(126, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(127, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(128, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(129, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(130, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	/* A_D lines */
	GPIO_CFG(135, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(136, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(137, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(138, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(139, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(140, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(141, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(142, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(143, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(144, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(145, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(146, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(147, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(148, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(149, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	GPIO_CFG(150, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	/* OE */
	GPIO_CFG(151, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	/* WE */
	GPIO_CFG(157, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	/* CS2 */
	GPIO_CFG(40, 2, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	/* CS3 */
	GPIO_CFG(133, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),
	/* ADV */
	GPIO_CFG(153, 1, GPIO_INPUT, GPIO_PULL_UP, GPIO_8MA),

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	/*
	 * SD/MMC Slot-1 (CLK, CMD, D0-D7)
	 */
	GPIO_CFG(167, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA),
	GPIO_CFG(168, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(159, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(160, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(161, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(162, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	GPIO_CFG(163, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(164, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(165, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(166, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
#endif
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
	/*
	 * SD/MMC Slot-5 (CLK, CMD, D0-D3)
	 */
	GPIO_CFG(97,  2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA),
	GPIO_CFG(95,  2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(100, 2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(99,  2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(98,  2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
	GPIO_CFG(96,  2, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_10MA),
#endif

#ifdef CONFIG_I2C_QUP
	/* GSBI7 QUP I2C (Marimba) */
	GPIO_CFG(59, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
	GPIO_CFG(60, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA),
#endif

#ifdef CONFIG_PMIC8058
	/* PMIC8058 */
	GPIO_CFG(PM8058_GPIO_INT, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
#endif
};

static uint32_t msm8x60_hdrive_cfgs[][2] = {
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	{TLMM_HDRV_SDC3_CLK, GPIO_8MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_8MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_8MA},
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	{TLMM_HDRV_SDC4_CLK, GPIO_8MA},
	{TLMM_HDRV_SDC4_CMD, GPIO_8MA},
	{TLMM_HDRV_SDC4_DATA, GPIO_8MA},
#endif
};

static uint32_t msm8x60_pull_cfgs[][2] = {
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	{TLMM_PULL_SDC3_CMD, GPIO_PULL_UP},
	{TLMM_PULL_SDC3_DATA, GPIO_PULL_UP},
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	{TLMM_PULL_SDC4_CMD, GPIO_PULL_UP},
	{TLMM_PULL_SDC4_DATA, GPIO_PULL_UP},
#endif
};

static void __init msm8x60_init_tlmm(void)
{
	unsigned n;

	if (machine_is_msm8x60_rumi3())
		msm_gpio_install_direct_irq(0, 0);
	else if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa()) {
		msm_gpio_install_direct_irq(62, 0);

		for (n = 0; n < ARRAY_SIZE(msm8x60_tlmm_cfgs); ++n)
			gpio_tlmm_config(msm8x60_tlmm_cfgs[n], 0);
		for (n = 0; n < ARRAY_SIZE(msm8x60_hdrive_cfgs); ++n)
			msm_tlmm_set_hdrive(msm8x60_hdrive_cfgs[n][0],
					msm8x60_hdrive_cfgs[n][1]);
		for (n = 0; n < ARRAY_SIZE(msm8x60_pull_cfgs); ++n)
			msm_tlmm_set_pull(msm8x60_pull_cfgs[n][0],
					msm8x60_pull_cfgs[n][1]);
	}
}

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC2_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC3_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC4_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC5_SUPPORT))

static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);

	/* Handle VREGs here once VREG support is available */

	return rc;
}
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct mmc_platform_data msm8x60_sdc1_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
#else
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#endif
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static struct mmc_platform_data msm8x60_sdc2_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct mmc_platform_data msm8x60_sdc3_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct mmc_platform_data msm8x60_sdc4_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
static struct mmc_platform_data msm8x60_sdc5_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
};
#endif

static void __init msm8x60_init_mmc(void)
{
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	msm_add_sdcc(1, &msm8x60_sdc1_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	msm_add_sdcc(2, &msm8x60_sdc2_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	msm_add_sdcc(3, &msm8x60_sdc3_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	msm_add_sdcc(4, &msm8x60_sdc4_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
	msm_add_sdcc(5, &msm8x60_sdc5_data);
#endif
}

static void __init msm8x60_init(void)
{
	msm8x60_init_ebi2();
	msm8x60_init_tlmm();
	msm8x60_init_mmc();
	msm8x60_init_buses();
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa())
		platform_add_devices(surf_devices,
				     ARRAY_SIZE(surf_devices));
	else {
		msm8x60_configure_smc91x();
		platform_add_devices(rumi_sim_devices,
				     ARRAY_SIZE(rumi_sim_devices));
	}
#ifdef CONFIG_I2C
	i2c_register_board_info(msm_gsbi8_qup_i2c_device.id,
				msm8x60_i2c_gsbi8_info,
				ARRAY_SIZE(msm8x60_i2c_gsbi8_info));
#endif

#ifdef CONFIG_PMIC8058
	i2c_register_board_info(msm_device_ssbi1.id,
				pm8058_boardinfo,
				ARRAY_SIZE(pm8058_boardinfo));
#endif
}

MACHINE_START(MSM8X60_RUMI3, "QCT MSM8X60 RUMI3")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.map_io = msm8x60_map_io,
	.init_irq = msm8x60_init_irq,
	.init_machine = msm8x60_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(MSM8X60_SIM, "QCT MSM8X60 SIMULATOR")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.map_io = msm8x60_map_io,
	.init_irq = msm8x60_init_irq,
	.init_machine = msm8x60_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(MSM8X60_SURF, "QCT MSM8X60 SURF")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io = MSM_DEBUG_UART_PHYS
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.map_io = msm8x60_map_io,
	.init_irq = msm8x60_init_irq,
	.init_machine = msm8x60_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(MSM8X60_FFA, "QCT MSM8X60 FFA")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io = MSM_DEBUG_UART_PHYS
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.map_io = msm8x60_map_io,
	.init_irq = msm8x60_init_irq,
	.init_machine = msm8x60_init,
	.timer = &msm_timer,
MACHINE_END
