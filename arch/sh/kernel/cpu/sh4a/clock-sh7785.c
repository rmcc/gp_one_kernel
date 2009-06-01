/*
 * arch/sh/kernel/cpu/sh4a/clock-sh7785.c
 *
 * SH7785 support for the clock framework
 *
 *  Copyright (C) 2007 - 2009  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <asm/clock.h>
#include <asm/freq.h>
#include <cpu/sh7785.h>

static unsigned int div2[] = { 1, 2, 4, 6, 8, 12, 16, 18,
			       24, 32, 36, 48 };

static struct clk_div_mult_table cpg_div = {
	.divisors = div2,
	.nr_divisors = ARRAY_SIZE(div2),
};

struct clk_priv {
	unsigned int			shift;

	/* allowable divisor bitmap */
	unsigned long			div_bitmap;

	/* Supportable frequencies + termination entry */
	struct cpufreq_frequency_table	freq_table[ARRAY_SIZE(div2)+1];
};

#define FRQMR_CLK_DATA(_name, _shift, _div_bitmap)	\
static struct clk_priv _name##_data = {			\
	.shift		= _shift,			\
	.div_bitmap	= _div_bitmap,			\
							\
	.freq_table[0]	= {				\
		.index = 0,				\
		.frequency = CPUFREQ_TABLE_END,		\
	},						\
}

FRQMR_CLK_DATA(pfc,  0, 0x0f80);
FRQMR_CLK_DATA(s3fc, 4, 0x0ff0);
FRQMR_CLK_DATA(s2fc, 8, 0x0030);
FRQMR_CLK_DATA(mfc, 12, 0x000c);
FRQMR_CLK_DATA(bfc, 16, 0x0fe0);
FRQMR_CLK_DATA(sfc, 20, 0x000c);
FRQMR_CLK_DATA(ufc, 24, 0x000c);
FRQMR_CLK_DATA(ifc, 28, 0x000e);

static unsigned long frqmr_recalc(struct clk *clk)
{
	struct clk_priv *data = clk->priv;
	unsigned int idx = (__raw_readl(FRQMR1) >> data->shift) & 0x000f;

	clk_rate_table_build(clk, data->freq_table, ARRAY_SIZE(div2),
			     &cpg_div, &data->div_bitmap);
	
	return data->freq_table[idx].frequency;
}

static long frqmr_round_rate(struct clk *clk, unsigned long rate)
{
	struct clk_priv *data = clk->priv;

	return clk_rate_table_round(clk, data->freq_table, rate);
}

static struct clk_ops frqmr_clk_ops = {
	.recalc			= frqmr_recalc,
	.round_rate		= frqmr_round_rate,
};

static unsigned long pll_recalc(struct clk *clk)
{
	int multiplier;

	multiplier = test_mode_pin(MODE_PIN_MODE4) ? 36 : 72;

	return clk->parent->rate * multiplier;
}

static struct clk_ops pll_clk_ops = {
	.recalc		= pll_recalc,
};

/*
 * Default rate for the root input clock, reset this with clk_set_rate()
 * from the platform code.
 */
static struct clk extal_clk = {
	.name		= "extal",
	.id		= -1,
	.rate		= 33333333,
};

static struct clk pll_clk = {
	.name		= "pll_clk",
	.id		= -1,
	.ops		= &pll_clk_ops,
	.parent		= &extal_clk,
	.flags		= CLK_ENABLE_ON_INIT,
};

static struct clk cpu_clk = {
	.name		= "cpu_clk",		/* Ick */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
	.priv		= &ifc_data,
};

static struct clk shyway_clk = {
	.name		= "shyway_clk",		/* SHck */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
	.priv		= &sfc_data,
};

static struct clk peripheral_clk = {
	.name		= "peripheral_clk",	/* Pck */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
	.priv		= &pfc_data,
};

static struct clk ddr_clk = {
	.name		= "ddr_clk",		/* DDRck */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
	.priv		= &mfc_data,
};

static struct clk bus_clk = {
	.name		= "bus_clk",		/* Bck */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
	.priv		= &bfc_data,
};

static struct clk ga_clk = {
	.name		= "ga_clk",		/* GAck */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.priv		= &s2fc_data,
};

static struct clk du_clk = {
	.name		= "du_clk",		/* DUck */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.priv		= &s3fc_data,
};

static struct clk umem_clk = {
	.name		= "umem_clk",		/* uck */
	.id		= -1,
	.ops		= &frqmr_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
	.priv		= &ufc_data,
};

static struct clk *clks[] = {
	&extal_clk,
	&pll_clk,
	&cpu_clk,
	&shyway_clk,
	&peripheral_clk,
	&ddr_clk,
	&bus_clk,
	&ga_clk,
	&du_clk,
	&umem_clk,
};

static int mstpcr_clk_enable(struct clk *clk)
{
	__raw_writel(__raw_readl(clk->enable_reg) & ~(1 << clk->enable_bit),
		     clk->enable_reg);
	return 0;
}

static void mstpcr_clk_disable(struct clk *clk)
{
	__raw_writel(__raw_readl(clk->enable_reg) | (1 << clk->enable_bit),
		     clk->enable_reg);
}

static struct clk_ops mstpcr_clk_ops = {
	.enable		= mstpcr_clk_enable,
	.disable	= mstpcr_clk_disable,
	.recalc		= followparent_recalc,
};

#define MSTPCR0		0xffc80030
#define MSTPCR1		0xffc80034

#define CLK(_name, _id, _parent, _enable_reg,		\
	    _enable_bit, _flags)			\
{							\
	.name		= _name,			\
	.id		= _id,				\
	.parent		= _parent,			\
	.enable_reg	= (void __iomem *)_enable_reg,	\
	.enable_bit	= _enable_bit,			\
	.flags		= _flags,			\
	.ops		= &mstpcr_clk_ops,		\
}

static struct clk mstpcr_clks[] = {
	/* MSTPCR0 */
	CLK("scif_fck", 5, &peripheral_clk, MSTPCR0, 29, 0),
	CLK("scif_fck", 4, &peripheral_clk, MSTPCR0, 28, 0),
	CLK("scif_fck", 3, &peripheral_clk, MSTPCR0, 27, 0),
	CLK("scif_fck", 2, &peripheral_clk, MSTPCR0, 26, 0),
	CLK("scif_fck", 1, &peripheral_clk, MSTPCR0, 25, 0),
	CLK("scif_fck", 0, &peripheral_clk, MSTPCR0, 24, 0),
	CLK("ssi_fck", 1, &peripheral_clk, MSTPCR0, 21, 0),
	CLK("ssi_fck", 0, &peripheral_clk, MSTPCR0, 20, 0),
	CLK("hac_fck", 1, &peripheral_clk, MSTPCR0, 17, 0),
	CLK("hac_fck", 0, &peripheral_clk, MSTPCR0, 16, 0),
	CLK("mmcif_fck", -1, &peripheral_clk, MSTPCR0, 13, 0),
	CLK("flctl_fck", -1, &peripheral_clk, MSTPCR0, 12, 0),
	CLK("tmu345_fck", -1, &peripheral_clk, MSTPCR0, 9, 0),
	CLK("tmu012_fck", -1, &peripheral_clk, MSTPCR0, 8, 0),
	CLK("siof_fck", -1, &peripheral_clk, MSTPCR0, 3, 0),
	CLK("hspi_fck", -1, &peripheral_clk, MSTPCR0, 2, 0),

	/* MSTPCR1 */
	CLK("hudi_fck", -1, NULL, MSTPCR1, 19, 0),
	CLK("ubc_fck", -1, NULL, MSTPCR1, 17, 0),
	CLK("dmac_11_6_fck", -1, NULL, MSTPCR1, 5, 0),
	CLK("dmac_5_0_fck", -1, NULL, MSTPCR1, 4, 0),
	CLK("gdta_fck", -1, NULL, MSTPCR1, 0, 0),
};

int __init arch_clk_init(void)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(clks); i++)
		ret |= clk_register(clks[i]);
	for (i = 0; i < ARRAY_SIZE(mstpcr_clks); i++)
		ret |= clk_register(&mstpcr_clks[i]);

	return ret;
}
