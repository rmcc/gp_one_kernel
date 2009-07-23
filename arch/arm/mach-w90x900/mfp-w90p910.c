/*
 * linux/arch/arm/mach-w90x900/mfp-w90p910.c
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;version 2 of the License.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/io.h>

#include <mach/hardware.h>

#define REG_MFSEL	(W90X900_VA_GCR + 0xC)

#define GPSELF		(0x01 << 1)

#define GPSELC		(0x03 << 2)
#define ENKPI		(0x02 << 2)
#define ENNAND		(0x01 << 2)

#define GPSELEI0	(0x01 << 26)
#define GPSELEI1	(0x01 << 27)

#define GPIOG0TO1	(0x03 << 14)
#define GPIOG2TO3	(0x03 << 16)
#define ENSPI		(0x0a << 14)
#define ENI2C0		(0x01 << 14)
#define ENI2C1		(0x01 << 16)

static DECLARE_MUTEX(mfp_sem);

void mfp_set_groupf(struct device *dev)
{
	unsigned long mfpen;
	const char *dev_id;

	BUG_ON(!dev);

	down(&mfp_sem);

	dev_id = dev_name(dev);

	mfpen = __raw_readl(REG_MFSEL);

	if (strcmp(dev_id, "w90p910-emc") == 0)
		mfpen |= GPSELF;/*enable mac*/
	else
		mfpen &= ~GPSELF;/*GPIOF[9:0]*/

	__raw_writel(mfpen, REG_MFSEL);

	up(&mfp_sem);
}
EXPORT_SYMBOL(mfp_set_groupf);

void mfp_set_groupc(struct device *dev)
{
	unsigned long mfpen;
	const char *dev_id;

	BUG_ON(!dev);

	down(&mfp_sem);

	dev_id = dev_name(dev);

	mfpen = __raw_readl(REG_MFSEL);

	if (strcmp(dev_id, "w90p910-lcd") == 0)
		mfpen |= GPSELC;/*enable lcd*/
	else if (strcmp(dev_id, "w90p910-kpi") == 0) {
			mfpen &= (~GPSELC);/*enable kpi*/
			mfpen |= ENKPI;
		} else if (strcmp(dev_id, "w90p910-nand") == 0) {
				mfpen &= (~GPSELC);/*enable nand*/
				mfpen |= ENNAND;
			} else
				mfpen &= (~GPSELC);/*GPIOC[14:0]*/

	__raw_writel(mfpen, REG_MFSEL);

	up(&mfp_sem);
}
EXPORT_SYMBOL(mfp_set_groupc);

void mfp_set_groupi(struct device *dev)
{
	unsigned long mfpen;
	const char *dev_id;

	BUG_ON(!dev);

	down(&mfp_sem);

	dev_id = dev_name(dev);

	mfpen = __raw_readl(REG_MFSEL);

	mfpen &= ~GPSELEI1;/*default gpio16*/

	if (strcmp(dev_id, "w90p910-wdog") == 0)
		mfpen |= GPSELEI1;/*enable wdog*/
		else if (strcmp(dev_id, "w90p910-atapi") == 0)
			mfpen |= GPSELEI0;/*enable atapi*/
			else if (strcmp(dev_id, "w90p910-keypad") == 0)
				mfpen &= ~GPSELEI0;/*enable keypad*/

	__raw_writel(mfpen, REG_MFSEL);

	up(&mfp_sem);
}
EXPORT_SYMBOL(mfp_set_groupi);

void mfp_set_groupg(struct device *dev)
{
	unsigned long mfpen;
	const char *dev_id;

	BUG_ON(!dev);

	down(&mfp_sem);

	dev_id = dev_name(dev);

	mfpen = __raw_readl(REG_MFSEL);

	if (strcmp(dev_id, "w90p910-spi") == 0) {
		mfpen &= ~(GPIOG0TO1 | GPIOG2TO3);
		mfpen |= ENSPI;/*enable spi*/
	} else if (strcmp(dev_id, "w90p910-i2c0") == 0) {
		mfpen &= ~(GPIOG0TO1);
		mfpen |= ENI2C0;/*enable i2c0*/
	} else if (strcmp(dev_id, "w90p910-i2c1") == 0) {
		mfpen &= ~(GPIOG2TO3);
		mfpen |= ENI2C1;/*enable i2c1*/
	} else {
		mfpen &= ~(GPIOG0TO1 | GPIOG2TO3);/*GPIOG[3:0]*/
	}

	__raw_writel(mfpen, REG_MFSEL);

	up(&mfp_sem);
}
EXPORT_SYMBOL(mfp_set_groupg);

