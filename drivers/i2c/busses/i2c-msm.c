/* drivers/i2c/busses/i2c-msm.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009 QUALCOMM USA, INC.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
/* FIH_ADQ, 6360 { */
#include <mach/board.h>
/* } FIH_ADQ, 6360  */
#define DEBUG 0

enum {
	I2C_WRITE_DATA          = 0x00,
	I2C_CLK_CTL             = 0x04,
	I2C_STATUS              = 0x08,
	I2C_READ_DATA           = 0x0c,
	I2C_INTERFACE_SELECT    = 0x10,

	I2C_WRITE_DATA_DATA_BYTE            = 0xff,
	I2C_WRITE_DATA_ADDR_BYTE            = 1U << 8,
	I2C_WRITE_DATA_LAST_BYTE            = 1U << 9,

	I2C_CLK_CTL_FS_DIVIDER_VALUE        = 0xff,
	I2C_CLK_CTL_HS_DIVIDER_VALUE        = 7U << 8,

	I2C_STATUS_WR_BUFFER_FULL           = 1U << 0,
	I2C_STATUS_RD_BUFFER_FULL           = 1U << 1,
	I2C_STATUS_BUS_ERROR                = 1U << 2,
	I2C_STATUS_PACKET_NACKED            = 1U << 3,
	I2C_STATUS_ARB_LOST                 = 1U << 4,
	I2C_STATUS_INVALID_WRITE            = 1U << 5,
	I2C_STATUS_FAILED                   = 3U << 6,
	I2C_STATUS_BUS_ACTIVE               = 1U << 8,
	I2C_STATUS_BUS_MASTER               = 1U << 9,
	I2C_STATUS_ERROR_MASK               = 0xfc,

	I2C_INTERFACE_SELECT_INTF_SELECT    = 1U << 0,
	I2C_INTERFACE_SELECT_SCL            = 1U << 8,
	I2C_INTERFACE_SELECT_SDA            = 1U << 9,
	I2C_STATUS_RX_DATA_STATE            = 3U << 11,
};

struct msm_i2c_dev {
	struct device      *dev;
	void __iomem       *base;		/* virtual */
	int                 irq;
	struct clk         *clk;
	struct i2c_adapter  adapter;

	spinlock_t          lock;

	struct i2c_msg      *msg;
	int                 pos;
	int                 cnt;
	int                 err;
	int                 flush_cnt;
	void                *complete;
};

#if DEBUG
static void
dump_status(uint32_t status)
{
	printk("STATUS (0x%.8x): ", status);
	if (status & I2C_STATUS_BUS_MASTER)
		printk("MST ");
	if (status & I2C_STATUS_BUS_ACTIVE)
		printk("ACT ");
	if (status & I2C_STATUS_INVALID_WRITE)
		printk("INV_WR ");
	if (status & I2C_STATUS_ARB_LOST)
		printk("ARB_LST ");
	if (status & I2C_STATUS_PACKET_NACKED)
		printk("NAK ");
	if (status & I2C_STATUS_BUS_ERROR)
		printk("BUS_ERR ");
	if (status & I2C_STATUS_RD_BUFFER_FULL)
		printk("RD_FULL ");
	if (status & I2C_STATUS_WR_BUFFER_FULL)
		printk("WR_FULL ");
	if (status & I2C_STATUS_FAILED)
		printk("FAIL 0x%x", (status & I2C_STATUS_FAILED));
	printk("\n");
}
#endif

///+FIH_ADQ
#include <mach/gpio.h>

static void msm_i2c_reset(struct msm_i2c_dev *dev)
{
	printk(KERN_INFO "<ubh> ****<<<<< msm_i2c_reset >>>>>****\r\n");
	// (1) disabling the hardware controller
	//	I2C_SCL_SDA_HI(bus_id); /* Setup SCL & SDA register bit*/
	//	I2C_SWITCH_IO_CTRL(bus_id); /* Switch to use IO controller */
	gpio_direction_output(60, 1);
	gpio_direction_output(61, 1);

	// (2) performing the special operation manually
	//	I2C_CLK_DATA_IO(bus_id); /* Generate a clock signal */
	//gpio_direction_output(60, 1);
	//I2C_4_MICROSEC_WAIT			// I2C_CLK_BUSY_WAIT(4)
	gpio_set_value(60, 0);
	//I2C_4_7_MICROSEC_WAIT		// I2C_CLK_BUSY_WAIT(5)

	// (3) resetting and reenabling the hardware controller
	//	I2C_WRITE_SCL((bus_id), I2C_BIT_HI); /* Prepare for start */
	gpio_set_value(60, 1);
	//	I2C_THIGH_DELAY; /* Time delay */
	//I2C_4_MICROSEC_WAIT			// I2C_CLK_BUSY_WAIT(4)
	//	I2C_HW_CTRL_RESET(); /* Reset the HW controller */
	clk_disable(dev->clk);

	//	I2C_SETUP_HW_CTRL(bus_id); /* Switch to use HW controller */
	/* Diable SDA & SCL Output */
	gpio_configure(60, GPIOF_INPUT);
	gpio_configure(61, GPIOF_INPUT);
	/* Switch to I2C hardware controller */
	gpio_tlmm_config(GPIO_CFG(60, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(61, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	clk_enable(dev->clk);
}
///-FIH_ADQ
#ifdef __TRACE_I2C_FAIL__
int g_intcnt = 0;
unsigned int g_intstate[10] = {0};
int getGasgaugeState();
unsigned char g_i2cpreaddr = 0;
#endif	// __TRACE_I2C_FAIL__

static irqreturn_t
msm_i2c_interrupt(int irq, void *devid)
{
	struct msm_i2c_dev *dev = devid;
	uint32_t status;
	int err = 0;

///+FIH_ADQ
	spin_lock(&dev->lock);
///-FIH_ADQ
	status = readl(dev->base + I2C_STATUS);
#ifdef __TRACE_I2C_FAIL__
	g_intstate[g_intcnt] = status;
	g_intcnt++;
#endif	// __TRACE_I2C_FAIL__

#if DEBUG
	dump_status(status);
#endif

	if (!dev->msg) {
		printk(KERN_ERR "%s: IRQ but nothing to do! : %x\n", __func__, status);
		spin_unlock(&dev->lock);
		return IRQ_HANDLED;
	}

	if (status & I2C_STATUS_ERROR_MASK) {
#ifdef __TRACE_I2C_FAIL__
if (getGasgaugeState()) printk(KERN_ERR "<GG-%d> %d/%d_%x_%x\n", getGasgaugeState(), dev->cnt, dev->msg->len, status, g_i2cpreaddr);
else
		printk(KERN_ERR "%s: %d/%d aborted cause to the I2C_STATUS(%x)\n", __func__, dev->cnt, dev->msg->len, status);
#endif	// __TRACE_I2C_FAIL__
		err = -EIO-1;
		goto out_err;
	}

	if (dev->msg->flags & I2C_M_RD) {
		if (status & I2C_STATUS_RD_BUFFER_FULL) {

			/*
			 * Theres something in the FIFO.
			 * Are we expecting data or flush crap?
			 */
			if (dev->cnt) { /* DATA */
				uint8_t *data = &dev->msg->buf[dev->pos];

#if 0
				*data = readl(dev->base + I2C_READ_DATA);
				dev->cnt--;
				dev->pos++;
				/* This is in spin-lock. So there will be no
				 * scheduling between reading the second-last
				 * byte and writing LAST_BYTE to the controller.
				 * So extra read-cycle-clock won't be generated
				 */
				if (dev->cnt == 1)
					writel(I2C_WRITE_DATA_LAST_BYTE,
						dev->base + I2C_WRITE_DATA);
				else if (dev->cnt == 0)
					goto out_complete;
#endif

///+FIH_ADQ : solve the NAK lost issue
                if (dev->cnt == 2)
                    writel(I2C_WRITE_DATA_LAST_BYTE,
                        dev->base + I2C_WRITE_DATA);
				*data = readl(dev->base + I2C_READ_DATA);
				dev->cnt--;
				dev->pos++;

				if (dev->cnt == 0)
					goto out_complete;
///-FIH_ADQ : solve the NAK lost issue

			} else {
				/* Now that extra read-cycle-clocks aren't
				 * generated, this becomes error condition
				 */
				dev_err(dev->dev,
					"read did not stop, status - %x\n",
					status);
				err = -EIO-2;
				goto out_err;
			}
		}
	} else {
		uint16_t data;

		if (status & I2C_STATUS_WR_BUFFER_FULL) {
			dev_err(dev->dev,
				"Write buffer full in ISR on write?\n");
			err = -EIO-3;
			goto out_err;
		}

		if (dev->cnt) {
			/* Ready to take a byte */
			data = dev->msg->buf[dev->pos];
			if (dev->cnt == 1)
				data |= I2C_WRITE_DATA_LAST_BYTE;

			writel(data, dev->base + I2C_WRITE_DATA);
			dev->pos++;
			dev->cnt--;
		} else
			goto out_complete;
	}

	spin_unlock(&dev->lock);
	return IRQ_HANDLED;

 out_err:
	dev->err = err;
 out_complete:
	complete(dev->complete);
	spin_unlock(&dev->lock);
	return IRQ_HANDLED;
}

static int
msm_i2c_poll_writeready(struct msm_i2c_dev *dev)
{
	uint32_t retries = 0;

	while (retries != 2000) {
		uint32_t status = readl(dev->base + I2C_STATUS);

		if (!(status & I2C_STATUS_WR_BUFFER_FULL))
			return 0;
		if (retries++ > 1000)
			msleep(1);
	}
	return -ETIMEDOUT;
}

static int
msm_i2c_poll_notbusy(struct msm_i2c_dev *dev)
{
	uint32_t retries = 0;

	while (retries != 2000) {
		uint32_t status = readl(dev->base + I2C_STATUS);

		if (!(status & I2C_STATUS_BUS_ACTIVE))
			return 0;
		if (retries++ > 1000)
			msleep(1);
	}
	return -ETIMEDOUT;
}

#ifdef __TRACE_I2C_FAIL__
static int
msm_i2c_poll(struct msm_i2c_dev *dev, int index, uint32_t ckflage)
{
	uint32_t retries = 0;
	#define __STATE_DEEP__ 10
	uint32_t status, st[__STATE_DEEP__] = {0};
	int cnt = 0;

	st[0] = status = readl(dev->base + I2C_STATUS);
	while (retries != 2000) {
		if (!(status & ckflage))
			return 0;
		if ((cnt < __STATE_DEEP__) && (st[cnt] != status)) st[++cnt] = status;
		if (retries++ > 1000)
			msleep(1);

		status = readl(dev->base + I2C_STATUS);
	}
	if (retries) {
		printk(KERN_INFO "<i2c> %d. msm_i2c_poll : (%d) - [%x %x %x %x %x %x %x %x %x %x]\n", index, retries,
			st[0], st[1], st[2], st[3], st[4], st[5], st[6], st[7], st[8], st [9]);
	}
	else {
		if (status)
			printk(KERN_INFO "<i2c> %d. msm_i2c_poll : (0) - %d\n", index, status);
	}
	if (retries != 2000) return 0;
	return -ETIMEDOUT;
}

unsigned int g_i2chistory = 0;
char g_i2cflag = 0;
int g_count = 0;
#define __TRACE_DEEP__ 20
int g_index = 0;
unsigned int g_addstatus[__TRACE_DEEP__] = {0};

static int ckAddrSend(unsigned int st)
{
	int i = 0;
	while ((i < g_index)) {
		if (st == g_addstatus[i]) return 0;
		if (++i == __TRACE_DEEP__) return 1;
	}
	g_addstatus[g_index++] = st;
	return 1;
}
#endif	// __TRACE_I2C_FAIL__

static int
///+FIH_ADQ
_msm_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
///-FIH_ADQ
{
	DECLARE_COMPLETION_ONSTACK(complete);
	struct msm_i2c_dev *dev = i2c_get_adapdata(adap);
	int ret = -1;
	int rem = num;
	uint16_t addr;
#ifdef __TRACE_I2C_FAIL__
	uint16_t addrflage = 0;
#endif	// __TRACE_I2C_FAIL__
	long timeout;
	unsigned long flags;

///+FIH_ADQ
	if (flags = readl(dev->base + I2C_STATUS)) {
#ifdef __TRACE_I2C_FAIL__
		int i = 0;
#endif	// __TRACE_I2C_FAIL__
		printk(KERN_INFO "<i2c> msm_i2c_xfer : I2C_STATUS(%x)\n", flags);
		if ((flags = readl(dev->base + I2C_STATUS)) & I2C_STATUS_RD_BUFFER_FULL) {
			if (flags == 0xbb02) {
				unsigned long ud, tmp;
				writel(I2C_WRITE_DATA_LAST_BYTE, dev->base + I2C_WRITE_DATA);
				ud = readl(dev->base + I2C_READ_DATA);
				printk(KERN_INFO "<i2c> msm_i2c_xfer 00 : I2C_STATUS(%x -> %x) : %x\r\n", flags, readl(dev->base + I2C_STATUS), ud);
				msleep(100);
				tmp = readl(dev->base + I2C_STATUS);
				ud = readl(dev->base + I2C_READ_DATA);
				printk(KERN_INFO "<i2c> msm_i2c_xfer 01 : I2C_STATUS(%x -> %x) : %x\r\n", tmp, flags = readl(dev->base + I2C_STATUS), ud);
			}
			else {
				unsigned long ud = readl(dev->base + I2C_READ_DATA), tmp;
				printk(KERN_INFO "<ubh> msm_i2c_xfer 00 : I2C_STATUS(%x -> %x) : %x\r\n", flags, tmp = readl(dev->base + I2C_STATUS), ud);
				flags = tmp;
			}
#ifdef __TRACE_I2C_FAIL__
			i = 1;
#endif	// __TRACE_I2C_FAIL__
		}
		if (flags & (I2C_STATUS_BUS_ACTIVE | I2C_STATUS_FAILED | I2C_STATUS_BUS_ERROR | I2C_STATUS_WR_BUFFER_FULL))
				// ex. 0x2100	NOT_MASTER_STATE, I2C_STATUS_BUS_ACTIVE
				//     0xbb02	FORCED_LOW_STATE, ???, BUS_MASTER, I2C_STATUS_BUS_ACTIVE, I2C_STATUS_RD_BUFFER_FULL
				//     0x0001	I2C_STATUS_WR_BUFFER_FULL
				//     0x00C4	I2C_STATUS_FAILED(Byte n tran fail; n+1 discarded), I2C_STATUS_BUS_ERROR
				// the I2C controller is not the presenet bus master (NOT_MASTER_STATE);
				// so must reset the I2C controller to avoid block the I2C command.
		{
			msm_i2c_reset(dev);
			printk(KERN_INFO "<i2c> msm_i2c_xfer 02 : I2C_STATUS(%x -> %x)\n", flags, readl(dev->base + I2C_STATUS));
#ifdef __TRACE_I2C_FAIL__
			i = 1;
#endif	// __TRACE_I2C_FAIL__
		}
#ifdef __TRACE_I2C_FAIL__
		if (i) {
			g_count++;
			if (g_i2cflag == 0) {
				int i;
				g_i2chistory = (g_i2chistory << 8) | (g_i2cpreaddr & 0xff);
				g_i2cflag = 1;
				printk(KERN_INFO "<i2c> msm_i2c - g_addstatus list(%d) :", g_index);
				for (i = 0; i < g_index; i++)
					printk(" %x", g_addstatus[i]);
				printk("\n");
			}
			printk(KERN_INFO "<i2c> i2c trace -> [%x] - %d\r\n", g_i2chistory, g_count);
		}
	}
#else	// __TRACE_I2C_FAIL__
	}
	else g_i2cflag = 0;
#endif	// __TRACE_I2C_FAIL__
///-FIH_ADQ

	while (rem) {
		addr = msgs->addr << 1;
		if (msgs->flags & I2C_M_RD)
			addr |= 1;
#ifdef __TRACE_I2C_FAIL__
		g_intcnt = 0;
#endif	// __TRACE_I2C_FAIL__

		spin_lock_irqsave(&dev->lock, flags);
		dev->msg = msgs;
		dev->pos = 0;
		dev->err = 0;
		dev->flush_cnt = 0;
		dev->cnt = msgs->len;
		dev->complete = &complete;
		spin_unlock_irqrestore(&dev->lock, flags);

#ifdef __TRACE_I2C_FAIL__
		ret = msm_i2c_poll(dev, 0, I2C_STATUS_BUS_ACTIVE);
#else	// __TRACE_I2C_FAIL__
		ret = msm_i2c_poll_notbusy(dev);
#endif	// __TRACE_I2C_FAIL__
		if (ret) {
			dev_err(dev->dev, "Error waiting for notbusy\n");
			goto out_err;
		}

		if (rem == 1 && msgs->len == 0)
			addr |= I2C_WRITE_DATA_LAST_BYTE;

		/* Wait for WR buffer not full */
#ifdef __TRACE_I2C_FAIL__
		ret = msm_i2c_poll(dev, 1, I2C_STATUS_WR_BUFFER_FULL);
#else	// __TRACE_I2C_FAIL__
		ret = msm_i2c_poll_writeready(dev);
#endif	// __TRACE_I2C_FAIL__
		if (ret) {
			dev_err(dev->dev,
				"Error waiting for write ready before addr\n");
			goto out_err;
		}

		/* special case for doing 1 byte read.
		 * There should be no scheduling between I2C controller becoming
		 * ready to read and writing LAST-BYTE to I2C controller
		 * This will avoid potential of I2C controller starting to latch
		 * another extra byte.
		 */
		if ((msgs->len == 1) && (msgs->flags & I2C_M_RD)) {
			uint32_t retries = 0;
			spin_lock_irqsave(&dev->lock, flags);

#ifdef __TRACE_I2C_FAIL__
			writel(addr|=I2C_WRITE_DATA_ADDR_BYTE,
				dev->base + I2C_WRITE_DATA);
#else	// __TRACE_I2C_FAIL__
			writel(I2C_WRITE_DATA_ADDR_BYTE | addr,
				dev->base + I2C_WRITE_DATA);
#endif	// __TRACE_I2C_FAIL__

			/* Poll for I2C controller going into RX_DATA mode to
			 * ensure controller goes into receive mode.
			 * Just checking write_buffer_full may not work since
			 * there is delay between the write-buffer becoming
			 * empty and the slave sending ACK to ensure I2C
			 * controller goes in receive mode to receive data.
			 */
			while (retries != 2000) {
				uint32_t status = readl(dev->base + I2C_STATUS);

					if (status & I2C_STATUS_RX_DATA_STATE)
						break;
				retries++;
			}
			if (retries >= 2000) {
				spin_unlock_irqrestore(&dev->lock, flags);
				dev_err(dev->dev,
					"Error doing one byte read\n");
				goto out_err;
			}

#ifdef __TRACE_I2C_FAIL__
			writel(I2C_WRITE_DATA_LAST_BYTE,
					dev->base + I2C_WRITE_DATA);
#else	// __TRACE_I2C_FAIL__
			writel(I2C_WRITE_DATA_LAST_BYTE,
					dev->base + I2C_WRITE_DATA);
#endif	// __TRACE_I2C_FAIL__
			spin_unlock_irqrestore(&dev->lock, flags);
		} else {
			writel(I2C_WRITE_DATA_ADDR_BYTE | addr,
					 dev->base + I2C_WRITE_DATA);
		}
		/* Polling and waiting for write_buffer_empty is not necessary.
		 * Even worse, if we do, it can result in invalid status and
		 * error if interrupt(s) occur while polling.
		 */

		/*
		 * Now that we've setup the xfer, the ISR will transfer the data
		 * and wake us up with dev->err set if there was an error
		 */

#ifdef __TRACE_I2C_FAIL__
		addrflage = readl(dev->base + I2C_STATUS);
		if (ckAddrSend(addrflage)) {
				printk(KERN_INFO "<i2c> msm_i2c_xfer new g_addrstatus(%x) : addr(%x) -> rem(%d)-[%x-%x-%d-%x]\n",
					addrflage, addr, rem,
					msgs->addr, msgs->flags, msgs->len, *(msgs->buf));
		}
		else addrflage = 0;
#endif	// __TRACE_I2C_FAIL__
		timeout = wait_for_completion_timeout(&complete, HZ);
		if (!timeout) {
#ifdef __TRACE_I2C_FAIL__
			writel(I2C_WRITE_DATA_LAST_BYTE,
				dev->base + I2C_WRITE_DATA);
			msleep(100);
			/* FLUSH */
			dev_err(dev->dev, "Transaction timed out : addr(%x) -> I2C_READ_DATA(%x), I2C_STATUS(%x), rem(%d)-[%x-%x-%d-%x], addrflage(%x), INT%d-[%x %x %x %x %x %x]\n",
				addr, readl(dev->base + I2C_READ_DATA),
				readl(dev->base + I2C_STATUS), rem,
				msgs->addr, msgs->flags, msgs->len, *(msgs->buf), addrflage,
				g_intcnt, g_intstate[0], g_intstate[1], g_intstate[2], g_intstate[3], g_intstate[4], g_intstate[5]);
#else	// __TRACE_I2C_FAIL__
			dev_err(dev->dev, "Transaction timed out\n");
/* FIH_ADQ, 6370 { */			
			writel(I2C_WRITE_DATA_LAST_BYTE,
				dev->base + I2C_WRITE_DATA);
			msleep(100);
			/* FLUSH */
			readl(dev->base + I2C_READ_DATA);
			readl(dev->base + I2C_STATUS);
/* FIH_ADQ, 6370 { */			
#endif	// __TRACE_I2C_FAIL__
			ret = -ETIMEDOUT;
			goto out_err;
		}
#ifdef __TRACE_I2C_FAIL__
		if (flags = readl(dev->base + I2C_STATUS)) {
		    /* FIH_ADQ, Kenny { */
		    
		    /*
			printk(KERN_INFO "<i2c> msm_i2c_xfer : I2C completion but I2C_STATUS(%x) - rem(%d)-[%x-%x-%d-%x] [%x : %x %x %x %x %x %x %x]\r\n",
				flags, rem,
				msgs->addr, msgs->flags, msgs->len, *(msgs->buf),
				g_intcnt, g_intstate[0], g_intstate[1], g_intstate[2], g_intstate[3],
				g_intstate[4], g_intstate[5], g_intstate[6]);
			*/
			
		    /* } FIH_ADQ, Kenny */
		}
#endif	// __TRACE_I2C_FAIL__
		if (dev->err) {
if (getGasgaugeState() == 0)
			dev_err(dev->dev,
#ifdef __TRACE_I2C_FAIL__
				"Error during data xfer [%x-%x-%d-%x] : (%d)\n",
				msgs->addr, msgs->flags, msgs->len, *(msgs->buf),
#else	// __TRACE_I2C_FAIL__
				"Error during data xfer (%d)\n",
#endif	// __TRACE_I2C_FAIL__
				dev->err);
			ret = dev->err;
			goto out_err;
		}
#ifdef __TRACE_I2C_FAIL__
		if (addrflage) printk(KERN_INFO "<i2c> ------------------------------------------- %d.(%x)\n", g_index, addrflage);
#endif	// __TRACE_I2C_FAIL__

		msgs++;
		rem--;
	}

#ifdef __TRACE_I2C_FAIL__
	ret = 0;
#else	// __TRACE_I2C_FAIL__
	ret = num;
#endif	// __TRACE_I2C_FAIL__
 out_err:
#ifdef __TRACE_I2C_FAIL__
	g_i2cpreaddr = addr;
#endif	// __TRACE_I2C_FAIL__
	spin_lock_irqsave(&dev->lock, flags);
	dev->complete = NULL;
	dev->msg = NULL;
	dev->pos = 0;
	dev->err = 0;
	dev->flush_cnt = 0;
	dev->cnt = 0;
	spin_unlock_irqrestore(&dev->lock, flags);
	return ret;
}

///+FIH_ADQ
int g_irq;

static int
msm_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	int i;
	enable_irq(g_irq);
	i = _msm_i2c_xfer(adap, msgs, num);
	disable_irq(g_irq);
	return i;
}
///-FIH_ADQ

static u32
msm_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static const struct i2c_algorithm msm_i2c_algo = {
	.master_xfer	= msm_i2c_xfer,
	.functionality	= msm_i2c_func,
};

static int
msm_i2c_probe(struct platform_device *pdev)
{
	struct msm_i2c_dev	*dev;
	struct resource		*mem, *irq, *ioarea;
	int ret;
	int fs_div;
	int hs_div;
	int i2c_clk;
	int clk_ctl;
	int target_clk;
	struct clk *clk;
	struct msm_i2c_platform_data *pdata;

	printk(KERN_INFO "msm_i2c_probe\n");

	/* NOTE: driver uses the static register mapping */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -ENODEV;
	}
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return -ENODEV;
	}

	ioarea = request_mem_region(mem->start, (mem->end - mem->start) + 1,
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "I2C region already claimed\n");
		return -EBUSY;
	}
	clk = clk_get(&pdev->dev, "i2c_clk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Could not get clock\n");
		ret = PTR_ERR(clk);
		goto err_clk_get_failed;
	}
/* FIH_ADQ, 6360 { */
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "platform data not initialized\n");
		ret = -ENOSYS;
		goto err_clk_get_failed;
	}
	target_clk = pdata->clk_freq;
	/* We support frequencies upto FAST Mode(400KHz) */
	if (target_clk <= 0 || target_clk > 400000) {
		dev_err(&pdev->dev, "clock frequency not supported\n");
		ret = -EIO;
		goto err_clk_get_failed;
	}
/* } FIH_ADQ, 6360  */
	dev = kzalloc(sizeof(struct msm_i2c_dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto err_alloc_dev_failed;
	}

	dev->dev = &pdev->dev;
	dev->irq = irq->start;
	dev->clk = clk;
	dev->base = ioremap(mem->start, (mem->end - mem->start) + 1);
	if (!dev->base) {
		ret = -ENOMEM;
		goto err_ioremap_failed;
	}

	spin_lock_init(&dev->lock);
	platform_set_drvdata(pdev, dev);

	clk_enable(clk);

	/* I2C_HS_CLK = I2C_CLK/(3*(HS_DIVIDER_VALUE+1) */
	/* I2C_FS_CLK = I2C_CLK/(2*(FS_DIVIDER_VALUE+3) */
	/* FS_DIVIDER_VALUE = ((I2C_CLK / I2C_FS_CLK) / 2) - 3 */
	i2c_clk = 19200000; /* input clock */
/* FIH_ADQ, 6360 { */	
///	target_clk = 100000;
/* } FIH_ADQ, 6360  */
	/* target_clk = 200000; */
	fs_div = ((i2c_clk / target_clk) / 2) - 3;
	hs_div = 3;
	clk_ctl = ((hs_div & 0x7) << 8) | (fs_div & 0xff);
	writel(clk_ctl, dev->base + I2C_CLK_CTL);
	printk(KERN_INFO "msm_i2c_probe: clk_ctl %x, %d Hz\n",
	       clk_ctl, i2c_clk / (2 * ((clk_ctl & 0xff) + 3)));

	i2c_set_adapdata(&dev->adapter, dev);
	dev->adapter.algo = &msm_i2c_algo;
	strncpy(dev->adapter.name,
		"MSM I2C adapter",
		sizeof(dev->adapter.name));

	dev->adapter.nr = pdev->id;
	ret = i2c_add_numbered_adapter(&dev->adapter);
	if (ret) {
		dev_err(&pdev->dev, "i2c_add_adapter failed\n");
		goto err_i2c_add_adapter_failed;
	}

	ret = request_irq(dev->irq, msm_i2c_interrupt,
			IRQF_TRIGGER_RISING, pdev->name, dev);
	if (ret) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
///+FIH_ADQ
	disable_irq(g_irq = dev->irq);
///-FIH_ADQ
	return 0;

/*	free_irq(dev->irq, dev); */
err_request_irq_failed:
	i2c_del_adapter(&dev->adapter);
err_i2c_add_adapter_failed:
	clk_disable(clk);
	iounmap(dev->base);
err_ioremap_failed:
	kfree(dev);
err_alloc_dev_failed:
	clk_put(clk);
err_clk_get_failed:
	release_mem_region(mem->start, (mem->end - mem->start) + 1);
	return ret;
}

static int
msm_i2c_remove(struct platform_device *pdev)
{
	struct msm_i2c_dev	*dev = platform_get_drvdata(pdev);
	struct resource		*mem;

	platform_set_drvdata(pdev, NULL);
	free_irq(dev->irq, dev);
	i2c_del_adapter(&dev->adapter);
	clk_disable(dev->clk);
	clk_put(dev->clk);
	iounmap(dev->base);
	kfree(dev);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, (mem->end - mem->start) + 1);
	return 0;
}

static int msm_i2c_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct msm_i2c_dev *dev = platform_get_drvdata(pdev);
	if (dev) {
///+FIH_ADQ
		gpio_direction_output(60, 1);
		gpio_direction_output(61, 1);
///-FIH_ADQ
		clk_disable(dev->clk);
	}
	return 0;
}

static int msm_i2c_resume(struct platform_device *pdev)
{
	struct msm_i2c_dev *dev = platform_get_drvdata(pdev);
	if (dev) {
///+FIH_ADQ
		gpio_configure(60, GPIOF_INPUT);
		gpio_configure(61, GPIOF_INPUT);
		gpio_tlmm_config(GPIO_CFG(60, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(61, 1, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
///-FIH_ADQ
		clk_enable(dev->clk);
	}
	return 0;
}

static struct platform_driver msm_i2c_driver = {
	.probe		= msm_i2c_probe,
	.remove		= msm_i2c_remove,
	.suspend	= msm_i2c_suspend,
	.resume		= msm_i2c_resume,
	.driver		= {
		.name	= "msm_i2c",
		.owner	= THIS_MODULE,
	},
};

/* I2C may be needed to bring up other drivers */
static int __init
msm_i2c_init_driver(void)
{
	return platform_driver_register(&msm_i2c_driver);
}
subsys_initcall(msm_i2c_init_driver);

static void __exit msm_i2c_exit_driver(void)
{
	platform_driver_unregister(&msm_i2c_driver);
}
module_exit(msm_i2c_exit_driver);

