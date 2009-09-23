/*
 * drivers/mmc/host/omap_hsmmc.c
 *
 * Driver for OMAP2430/3430 MMC controller.
 *
 * Copyright (C) 2007 Texas Instruments.
 *
 * Authors:
 *	Syed Mohammed Khasim	<x0khasim@ti.com>
 *	Madhusudhan		<madhu.cr@ti.com>
 *	Mohit Jalori		<mjalori@ti.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <mach/dma.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/mmc.h>
#include <mach/cpu.h>

/* OMAP HSMMC Host Controller Registers */
#define OMAP_HSMMC_SYSCONFIG	0x0010
#define OMAP_HSMMC_SYSSTATUS	0x0014
#define OMAP_HSMMC_CON		0x002C
#define OMAP_HSMMC_BLK		0x0104
#define OMAP_HSMMC_ARG		0x0108
#define OMAP_HSMMC_CMD		0x010C
#define OMAP_HSMMC_RSP10	0x0110
#define OMAP_HSMMC_RSP32	0x0114
#define OMAP_HSMMC_RSP54	0x0118
#define OMAP_HSMMC_RSP76	0x011C
#define OMAP_HSMMC_DATA		0x0120
#define OMAP_HSMMC_HCTL		0x0128
#define OMAP_HSMMC_SYSCTL	0x012C
#define OMAP_HSMMC_STAT		0x0130
#define OMAP_HSMMC_IE		0x0134
#define OMAP_HSMMC_ISE		0x0138
#define OMAP_HSMMC_CAPA		0x0140

#define VS18			(1 << 26)
#define VS30			(1 << 25)
#define SDVS18			(0x5 << 9)
#define SDVS30			(0x6 << 9)
#define SDVS33			(0x7 << 9)
#define SDVS_MASK		0x00000E00
#define SDVSCLR			0xFFFFF1FF
#define SDVSDET			0x00000400
#define AUTOIDLE		0x1
#define SDBP			(1 << 8)
#define DTO			0xe
#define ICE			0x1
#define ICS			0x2
#define CEN			(1 << 2)
#define CLKD_MASK		0x0000FFC0
#define CLKD_SHIFT		6
#define DTO_MASK		0x000F0000
#define DTO_SHIFT		16
#define INT_EN_MASK		0x307F0033
#define BWR_ENABLE		(1 << 4)
#define BRR_ENABLE		(1 << 5)
#define INIT_STREAM		(1 << 1)
#define DP_SELECT		(1 << 21)
#define DDIR			(1 << 4)
#define DMA_EN			0x1
#define MSBS			(1 << 5)
#define BCE			(1 << 1)
#define FOUR_BIT		(1 << 1)
#define DW8			(1 << 5)
#define CC			0x1
#define TC			0x02
#define OD			0x1
#define ERR			(1 << 15)
#define CMD_TIMEOUT		(1 << 16)
#define DATA_TIMEOUT		(1 << 20)
#define CMD_CRC			(1 << 17)
#define DATA_CRC		(1 << 21)
#define CARD_ERR		(1 << 28)
#define STAT_CLEAR		0xFFFFFFFF
#define INIT_STREAM_CMD		0x00000000
#define DUAL_VOLT_OCR_BIT	7
#define SRC			(1 << 25)
#define SRD			(1 << 26)
#define SOFTRESET		(1 << 1)
#define RESETDONE		(1 << 0)

/*
 * FIXME: Most likely all the data using these _DEVID defines should come
 * from the platform_data, or implemented in controller and slot specific
 * functions.
 */
#define OMAP_MMC1_DEVID		0
#define OMAP_MMC2_DEVID		1
#define OMAP_MMC3_DEVID		2

#define MMC_TIMEOUT_MS		20
#define OMAP_MMC_MASTER_CLOCK	96000000
#define DRIVER_NAME		"mmci-omap-hs"

/* Timeouts for entering power saving states on inactivity, msec */
#define OMAP_MMC_DISABLED_TIMEOUT	100
#define OMAP_MMC_SLEEP_TIMEOUT		1000
#define OMAP_MMC_OFF_TIMEOUT		8000

/*
 * One controller can have multiple slots, like on some omap boards using
 * omap.c controller driver. Luckily this is not currently done on any known
 * omap_hsmmc.c device.
 */
#define mmc_slot(host)		(host->pdata->slots[host->slot_id])

/*
 * MMC Host controller read/write API's
 */
#define OMAP_HSMMC_READ(base, reg)	\
	__raw_readl((base) + OMAP_HSMMC_##reg)

#define OMAP_HSMMC_WRITE(base, reg, val) \
	__raw_writel((val), (base) + OMAP_HSMMC_##reg)

struct mmc_omap_host {
	struct	device		*dev;
	struct	mmc_host	*mmc;
	struct	mmc_request	*mrq;
	struct	mmc_command	*cmd;
	struct	mmc_data	*data;
	struct	clk		*fclk;
	struct	clk		*iclk;
	struct	clk		*dbclk;
	struct	semaphore	sem;
	struct	work_struct	mmc_carddetect_work;
	void	__iomem		*base;
	resource_size_t		mapbase;
	unsigned int		id;
	unsigned int		dma_len;
	unsigned int		dma_sg_idx;
	unsigned char		bus_mode;
	unsigned char		power_mode;
	u32			*buffer;
	u32			bytesleft;
	int			suspended;
	int			irq;
	int			use_dma, dma_ch;
	int			dma_line_tx, dma_line_rx;
	int			slot_id;
	int			dbclk_enabled;
	int			response_busy;
	int			context_loss;
	int			dpm_state;
	int			vdd;

	struct	omap_mmc_platform_data	*pdata;
};

/*
 * Stop clock to the card
 */
static void omap_mmc_stop_clock(struct mmc_omap_host *host)
{
	OMAP_HSMMC_WRITE(host->base, SYSCTL,
		OMAP_HSMMC_READ(host->base, SYSCTL) & ~CEN);
	if ((OMAP_HSMMC_READ(host->base, SYSCTL) & CEN) != 0x0)
		dev_dbg(mmc_dev(host->mmc), "MMC Clock is not stoped\n");
}

#ifdef CONFIG_PM

/*
 * Restore the MMC host context, if it was lost as result of a
 * power state change.
 */
static int omap_mmc_restore_ctx(struct mmc_omap_host *host)
{
	struct mmc_ios *ios = &host->mmc->ios;
	struct omap_mmc_platform_data *pdata = host->pdata;
	int context_loss = 0;
	u32 hctl, capa, con;
	u16 dsor = 0;
	unsigned long timeout;

	if (pdata->get_context_loss_count) {
		context_loss = pdata->get_context_loss_count(host->dev);
		if (context_loss < 0)
			return 1;
	}

	dev_dbg(mmc_dev(host->mmc), "context was %slost\n",
		context_loss == host->context_loss ? "not " : "");
	if (host->context_loss == context_loss)
		return 1;

	/* Wait for hardware reset */
	timeout = jiffies + msecs_to_jiffies(MMC_TIMEOUT_MS);
	while ((OMAP_HSMMC_READ(host->base, SYSSTATUS) & RESETDONE) != RESETDONE
		&& time_before(jiffies, timeout))
		;

	/* Do software reset */
	OMAP_HSMMC_WRITE(host->base, SYSCONFIG, SOFTRESET);
	timeout = jiffies + msecs_to_jiffies(MMC_TIMEOUT_MS);
	while ((OMAP_HSMMC_READ(host->base, SYSSTATUS) & RESETDONE) != RESETDONE
		&& time_before(jiffies, timeout))
		;

	OMAP_HSMMC_WRITE(host->base, SYSCONFIG,
			OMAP_HSMMC_READ(host->base, SYSCONFIG) | AUTOIDLE);

	if (host->id == OMAP_MMC1_DEVID) {
		if (host->power_mode != MMC_POWER_OFF &&
		    (1 << ios->vdd) <= MMC_VDD_23_24)
			hctl = SDVS18;
		else
			hctl = SDVS30;
		capa = VS30 | VS18;
	} else {
		hctl = SDVS18;
		capa = VS18;
	}

	OMAP_HSMMC_WRITE(host->base, HCTL,
			OMAP_HSMMC_READ(host->base, HCTL) | hctl);

	OMAP_HSMMC_WRITE(host->base, CAPA,
			OMAP_HSMMC_READ(host->base, CAPA) | capa);

	OMAP_HSMMC_WRITE(host->base, HCTL,
			OMAP_HSMMC_READ(host->base, HCTL) | SDBP);

	timeout = jiffies + msecs_to_jiffies(MMC_TIMEOUT_MS);
	while ((OMAP_HSMMC_READ(host->base, HCTL) & SDBP) != SDBP
		&& time_before(jiffies, timeout))
		;

	OMAP_HSMMC_WRITE(host->base, STAT, STAT_CLEAR);
	OMAP_HSMMC_WRITE(host->base, ISE, INT_EN_MASK);
	OMAP_HSMMC_WRITE(host->base, IE, INT_EN_MASK);

	/* Do not initialize card-specific things if the power is off */
	if (host->power_mode == MMC_POWER_OFF)
		goto out;

	con = OMAP_HSMMC_READ(host->base, CON);
	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_8:
		OMAP_HSMMC_WRITE(host->base, CON, con | DW8);
		break;
	case MMC_BUS_WIDTH_4:
		OMAP_HSMMC_WRITE(host->base, CON, con & ~DW8);
		OMAP_HSMMC_WRITE(host->base, HCTL,
			OMAP_HSMMC_READ(host->base, HCTL) | FOUR_BIT);
		break;
	case MMC_BUS_WIDTH_1:
		OMAP_HSMMC_WRITE(host->base, CON, con & ~DW8);
		OMAP_HSMMC_WRITE(host->base, HCTL,
			OMAP_HSMMC_READ(host->base, HCTL) & ~FOUR_BIT);
		break;
	}

	if (ios->clock) {
		dsor = OMAP_MMC_MASTER_CLOCK / ios->clock;
		if (dsor < 1)
			dsor = 1;

		if (OMAP_MMC_MASTER_CLOCK / dsor > ios->clock)
			dsor++;

		if (dsor > 250)
			dsor = 250;
	}

	OMAP_HSMMC_WRITE(host->base, SYSCTL,
		OMAP_HSMMC_READ(host->base, SYSCTL) & ~CEN);
	OMAP_HSMMC_WRITE(host->base, SYSCTL, (dsor << 6) | (DTO << 16));
	OMAP_HSMMC_WRITE(host->base, SYSCTL,
		OMAP_HSMMC_READ(host->base, SYSCTL) | ICE);

	timeout = jiffies + msecs_to_jiffies(MMC_TIMEOUT_MS);
	while ((OMAP_HSMMC_READ(host->base, SYSCTL) & ICS) != ICS
		&& time_before(jiffies, timeout))
		;

	OMAP_HSMMC_WRITE(host->base, SYSCTL,
		OMAP_HSMMC_READ(host->base, SYSCTL) | CEN);

	con = OMAP_HSMMC_READ(host->base, CON);
	if (ios->bus_mode == MMC_BUSMODE_OPENDRAIN)
		OMAP_HSMMC_WRITE(host->base, CON, con | OD);
	else
		OMAP_HSMMC_WRITE(host->base, CON, con & ~OD);
out:
	host->context_loss = context_loss;

	dev_dbg(mmc_dev(host->mmc), "context is restored\n");
	return 0;
}

/*
 * Save the MMC host context (store the number of power state changes so far).
 */
static void omap_mmc_save_ctx(struct mmc_omap_host *host)
{
	struct omap_mmc_platform_data *pdata = host->pdata;
	int context_loss;

	if (pdata->get_context_loss_count) {
		context_loss = pdata->get_context_loss_count(host->dev);
		if (context_loss < 0)
			return;
		host->context_loss = context_loss;
	}
}

#else

static int omap_mmc_restore_ctx(struct mmc_omap_host *host)
{
	return 0;
}

static void omap_mmc_save_ctx(struct mmc_omap_host *host)
{
}

#endif

/*
 * Send init stream sequence to card
 * before sending IDLE command
 */
static void send_init_stream(struct mmc_omap_host *host)
{
	int reg = 0;
	unsigned long timeout;

	disable_irq(host->irq);
	OMAP_HSMMC_WRITE(host->base, CON,
		OMAP_HSMMC_READ(host->base, CON) | INIT_STREAM);
	OMAP_HSMMC_WRITE(host->base, CMD, INIT_STREAM_CMD);

	timeout = jiffies + msecs_to_jiffies(MMC_TIMEOUT_MS);
	while ((reg != CC) && time_before(jiffies, timeout))
		reg = OMAP_HSMMC_READ(host->base, STAT) & CC;

	OMAP_HSMMC_WRITE(host->base, CON,
		OMAP_HSMMC_READ(host->base, CON) & ~INIT_STREAM);

	OMAP_HSMMC_WRITE(host->base, STAT, STAT_CLEAR);
	OMAP_HSMMC_READ(host->base, STAT);

	enable_irq(host->irq);
}

static inline
int mmc_omap_cover_is_closed(struct mmc_omap_host *host)
{
	int r = 1;

	if (mmc_slot(host).get_cover_state)
		r = mmc_slot(host).get_cover_state(host->dev, host->slot_id);
	return r;
}

static ssize_t
mmc_omap_show_cover_switch(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct mmc_omap_host *host = mmc_priv(mmc);

	return sprintf(buf, "%s\n", mmc_omap_cover_is_closed(host) ? "closed" :
		       "open");
}

static DEVICE_ATTR(cover_switch, S_IRUGO, mmc_omap_show_cover_switch, NULL);

static ssize_t
mmc_omap_show_slot_name(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct mmc_omap_host *host = mmc_priv(mmc);

	return sprintf(buf, "%s\n", mmc_slot(host).name);
}

static DEVICE_ATTR(slot_name, S_IRUGO, mmc_omap_show_slot_name, NULL);

/*
 * Configure the response type and send the cmd.
 */
static void
mmc_omap_start_command(struct mmc_omap_host *host, struct mmc_command *cmd,
	struct mmc_data *data)
{
	int cmdreg = 0, resptype = 0, cmdtype = 0;

	dev_dbg(mmc_dev(host->mmc), "%s: CMD%d, argument 0x%08x\n",
		mmc_hostname(host->mmc), cmd->opcode, cmd->arg);
	host->cmd = cmd;

	/*
	 * Clear status bits and enable interrupts
	 */
	OMAP_HSMMC_WRITE(host->base, STAT, STAT_CLEAR);
	OMAP_HSMMC_WRITE(host->base, ISE, INT_EN_MASK);

	if (host->use_dma)
		OMAP_HSMMC_WRITE(host->base, IE,
				 INT_EN_MASK & ~(BRR_ENABLE | BWR_ENABLE));
	else
		OMAP_HSMMC_WRITE(host->base, IE, INT_EN_MASK);

	host->response_busy = 0;
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136)
			resptype = 1;
		else if (cmd->flags & MMC_RSP_BUSY) {
			resptype = 3;
			host->response_busy = 1;
		} else
			resptype = 2;
	}

	/*
	 * Unlike OMAP1 controller, the cmdtype does not seem to be based on
	 * ac, bc, adtc, bcr. Only commands ending an open ended transfer need
	 * a val of 0x3, rest 0x0.
	 */
	if (cmd == host->mrq->stop)
		cmdtype = 0x3;

	cmdreg = (cmd->opcode << 24) | (resptype << 16) | (cmdtype << 22);

	if (data) {
		cmdreg |= DP_SELECT | MSBS | BCE;
		if (data->flags & MMC_DATA_READ)
			cmdreg |= DDIR;
		else
			cmdreg &= ~(DDIR);
	}

	if (host->use_dma)
		cmdreg |= DMA_EN;

	OMAP_HSMMC_WRITE(host->base, ARG, cmd->arg);
	OMAP_HSMMC_WRITE(host->base, CMD, cmdreg);
}

static int
mmc_omap_get_dma_dir(struct mmc_omap_host *host, struct mmc_data *data)
{
	if (data->flags & MMC_DATA_WRITE)
		return DMA_TO_DEVICE;
	else
		return DMA_FROM_DEVICE;
}

/*
 * Notify the transfer complete to MMC core
 */
static void
mmc_omap_xfer_done(struct mmc_omap_host *host, struct mmc_data *data)
{
	if (!data) {
		struct mmc_request *mrq = host->mrq;

		/* TC before CC from CMD6 - don't know why, but it happens */
		if (host->cmd && host->cmd->opcode == 6 &&
		    host->response_busy) {
			host->response_busy = 0;
			return;
		}

		host->mrq = NULL;
		mmc_request_done(host->mmc, mrq);
		return;
	}

	host->data = NULL;

	if (host->use_dma && host->dma_ch != -1)
		dma_unmap_sg(mmc_dev(host->mmc), data->sg, host->dma_len,
			mmc_omap_get_dma_dir(host, data));

	if (!data->error)
		data->bytes_xfered += data->blocks * (data->blksz);
	else
		data->bytes_xfered = 0;

	if (!data->stop) {
		host->mrq = NULL;
		mmc_request_done(host->mmc, data->mrq);
		return;
	}
	mmc_omap_start_command(host, data->stop, NULL);
}

/*
 * Notify the core about command completion
 */
static void
mmc_omap_cmd_done(struct mmc_omap_host *host, struct mmc_command *cmd)
{
	host->cmd = NULL;

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			/* response type 2 */
			cmd->resp[3] = OMAP_HSMMC_READ(host->base, RSP10);
			cmd->resp[2] = OMAP_HSMMC_READ(host->base, RSP32);
			cmd->resp[1] = OMAP_HSMMC_READ(host->base, RSP54);
			cmd->resp[0] = OMAP_HSMMC_READ(host->base, RSP76);
		} else {
			/* response types 1, 1b, 3, 4, 5, 6 */
			cmd->resp[0] = OMAP_HSMMC_READ(host->base, RSP10);
		}
	}
	if ((host->data == NULL && !host->response_busy) || cmd->error) {
		host->mrq = NULL;
		mmc_request_done(host->mmc, cmd->mrq);
	}
}

/*
 * DMA clean up for command errors
 */
static void mmc_dma_cleanup(struct mmc_omap_host *host, int errno)
{
	host->data->error = errno;

	if (host->use_dma && host->dma_ch != -1) {
		dma_unmap_sg(mmc_dev(host->mmc), host->data->sg, host->dma_len,
			mmc_omap_get_dma_dir(host, host->data));
		omap_free_dma(host->dma_ch);
		host->dma_ch = -1;
		up(&host->sem);
	}
	host->data = NULL;
}

/*
 * Readable error output
 */
#ifdef CONFIG_MMC_DEBUG
static void mmc_omap_report_irq(struct mmc_omap_host *host, u32 status)
{
	/* --- means reserved bit without definition at documentation */
	static const char *mmc_omap_status_bits[] = {
		"CC", "TC", "BGE", "---", "BWR", "BRR", "---", "---", "CIRQ",
		"OBI", "---", "---", "---", "---", "---", "ERRI", "CTO", "CCRC",
		"CEB", "CIE", "DTO", "DCRC", "DEB", "---", "ACE", "---",
		"---", "---", "---", "CERR", "CERR", "BADA", "---", "---", "---"
	};
	char res[256];
	char *buf = res;
	int len, i;

	len = sprintf(buf, "MMC IRQ 0x%x :", status);
	buf += len;

	for (i = 0; i < ARRAY_SIZE(mmc_omap_status_bits); i++)
		if (status & (1 << i)) {
			len = sprintf(buf, " %s", mmc_omap_status_bits[i]);
			buf += len;
		}

	dev_dbg(mmc_dev(host->mmc), "%s\n", res);
}
#endif  /* CONFIG_MMC_DEBUG */

/*
 * MMC controller internal state machines reset
 *
 * Used to reset command or data internal state machines, using respectively
 *  SRC or SRD bit of SYSCTL register
 * Can be called from interrupt context
 */
static inline void mmc_omap_reset_controller_fsm(struct mmc_omap_host *host,
		unsigned long bit)
{
	unsigned long i = 0;
	unsigned long limit = (loops_per_jiffy *
				msecs_to_jiffies(MMC_TIMEOUT_MS));

	OMAP_HSMMC_WRITE(host->base, SYSCTL,
			 OMAP_HSMMC_READ(host->base, SYSCTL) | bit);

	while ((OMAP_HSMMC_READ(host->base, SYSCTL) & bit) &&
		(i++ < limit))
		cpu_relax();

	if (OMAP_HSMMC_READ(host->base, SYSCTL) & bit)
		dev_err(mmc_dev(host->mmc),
			"Timeout waiting on controller reset in %s\n",
			__func__);
}

/*
 * MMC controller IRQ handler
 */
static irqreturn_t mmc_omap_irq(int irq, void *dev_id)
{
	struct mmc_omap_host *host = dev_id;
	struct mmc_data *data;
	int end_cmd = 0, end_trans = 0, status;

	if (host->mrq == NULL) {
		OMAP_HSMMC_WRITE(host->base, STAT,
			OMAP_HSMMC_READ(host->base, STAT));
		/* Flush posted write */
		OMAP_HSMMC_READ(host->base, STAT);
		return IRQ_HANDLED;
	}

	data = host->data;
	status = OMAP_HSMMC_READ(host->base, STAT);
	dev_dbg(mmc_dev(host->mmc), "IRQ Status is %x\n", status);

	if (status & ERR) {
#ifdef CONFIG_MMC_DEBUG
		mmc_omap_report_irq(host, status);
#endif
		if ((status & CMD_TIMEOUT) ||
			(status & CMD_CRC)) {
			if (host->cmd) {
				if (status & CMD_TIMEOUT) {
					mmc_omap_reset_controller_fsm(host,
								      SRC);
					host->cmd->error = -ETIMEDOUT;
				} else {
					host->cmd->error = -EILSEQ;
				}
				end_cmd = 1;
			}
			if (host->data || host->response_busy) {
				if (host->data)
					mmc_dma_cleanup(host, -ETIMEDOUT);
				host->response_busy = 0;
				mmc_omap_reset_controller_fsm(host, SRD);
			}
		}
		if ((status & DATA_TIMEOUT) ||
			(status & DATA_CRC)) {
			if (host->data || host->response_busy) {
				int err = (status & DATA_TIMEOUT) ?
						-ETIMEDOUT : -EILSEQ;

				if (host->data)
					mmc_dma_cleanup(host, err);
				else
					host->mrq->cmd->error = err;
				host->response_busy = 0;
				mmc_omap_reset_controller_fsm(host, SRD);
				end_trans = 1;
			}
		}
		if (status & CARD_ERR) {
			dev_dbg(mmc_dev(host->mmc),
				"Ignoring card err CMD%d\n", host->cmd->opcode);
			if (host->cmd)
				end_cmd = 1;
			if (host->data)
				end_trans = 1;
		}
	}

	OMAP_HSMMC_WRITE(host->base, STAT, status);
	/* Flush posted write */
	OMAP_HSMMC_READ(host->base, STAT);

	if (end_cmd || ((status & CC) && host->cmd))
		mmc_omap_cmd_done(host, host->cmd);
	if ((end_trans || (status & TC)) && host->mrq)
		mmc_omap_xfer_done(host, data);

	return IRQ_HANDLED;
}

static void set_sd_bus_power(struct mmc_omap_host *host)
{
	unsigned long i;

	OMAP_HSMMC_WRITE(host->base, HCTL,
			 OMAP_HSMMC_READ(host->base, HCTL) | SDBP);
	for (i = 0; i < loops_per_jiffy; i++) {
		if (OMAP_HSMMC_READ(host->base, HCTL) & SDBP)
			break;
		cpu_relax();
	}
}

/*
 * Switch MMC interface voltage ... only relevant for MMC1.
 *
 * MMC2 and MMC3 use fixed 1.8V levels, and maybe a transceiver.
 * The MMC2 transceiver controls are used instead of DAT4..DAT7.
 * Some chips, like eMMC ones, use internal transceivers.
 */
static int omap_mmc_switch_opcond(struct mmc_omap_host *host, int vdd)
{
	u32 reg_val = 0;
	int ret;

	/* Disable the clocks */
	clk_disable(host->fclk);
	clk_disable(host->iclk);
	clk_disable(host->dbclk);

	/* Turn the power off */
	ret = mmc_slot(host).set_power(host->dev, host->slot_id, 0, 0);
	if (ret != 0)
		goto err;

	/* Turn the power ON with given VDD 1.8 or 3.0v */
	ret = mmc_slot(host).set_power(host->dev, host->slot_id, 1, vdd);
	if (ret != 0)
		goto err;

	clk_enable(host->fclk);
	clk_enable(host->iclk);
	clk_enable(host->dbclk);

	OMAP_HSMMC_WRITE(host->base, HCTL,
		OMAP_HSMMC_READ(host->base, HCTL) & SDVSCLR);
	reg_val = OMAP_HSMMC_READ(host->base, HCTL);

	/*
	 * If a MMC dual voltage card is detected, the set_ios fn calls
	 * this fn with VDD bit set for 1.8V. Upon card removal from the
	 * slot, omap_mmc_set_ios sets the VDD back to 3V on MMC_POWER_OFF.
	 *
	 * Cope with a bit of slop in the range ... per data sheets:
	 *  - "1.8V" for vdds_mmc1/vdds_mmc1a can be up to 2.45V max,
	 *    but recommended values are 1.71V to 1.89V
	 *  - "3.0V" for vdds_mmc1/vdds_mmc1a can be up to 3.5V max,
	 *    but recommended values are 2.7V to 3.3V
	 *
	 * Board setup code shouldn't permit anything very out-of-range.
	 * TWL4030-family VMMC1 and VSIM regulators are fine (avoiding the
	 * middle range) but VSIM can't power DAT4..DAT7 at more than 3V.
	 */
	if ((1 << vdd) <= MMC_VDD_23_24)
		reg_val |= SDVS18;
	else
		reg_val |= SDVS30;

	OMAP_HSMMC_WRITE(host->base, HCTL, reg_val);
	set_sd_bus_power(host);

	return 0;
err:
	dev_dbg(mmc_dev(host->mmc), "Unable to switch operating voltage\n");
	return ret;
}

/*
 * Work Item to notify the core about card insertion/removal
 */
static void mmc_omap_detect(struct work_struct *work)
{
	struct mmc_omap_host *host = container_of(work, struct mmc_omap_host,
						mmc_carddetect_work);
	struct omap_mmc_slot_data *slot = &mmc_slot(host);
	int carddetect;

	if (host->suspended)
		return;

	sysfs_notify(&host->mmc->class_dev.kobj, NULL, "cover_switch");

	if (slot->card_detect)
		carddetect = slot->card_detect(slot->card_detect_irq);
	else
		carddetect = -ENOSYS;

	if (carddetect) {
		mmc_detect_change(host->mmc, (HZ * 200) / 1000);
	} else {
		mmc_host_enable(host->mmc);
		mmc_omap_reset_controller_fsm(host, SRD);
		mmc_host_lazy_disable(host->mmc);
		mmc_detect_change(host->mmc, (HZ * 50) / 1000);
	}
}

/*
 * ISR for handling card insertion and removal
 */
static irqreturn_t omap_mmc_cd_handler(int irq, void *dev_id)
{
	struct mmc_omap_host *host = (struct mmc_omap_host *)dev_id;

	if (host->suspended)
		return IRQ_HANDLED;
	schedule_work(&host->mmc_carddetect_work);

	return IRQ_HANDLED;
}

static int mmc_omap_get_dma_sync_dev(struct mmc_omap_host *host,
				     struct mmc_data *data)
{
	int sync_dev;

	if (data->flags & MMC_DATA_WRITE)
		sync_dev = host->dma_line_tx;
	else
		sync_dev = host->dma_line_rx;
	return sync_dev;
}

static void mmc_omap_config_dma_params(struct mmc_omap_host *host,
				       struct mmc_data *data,
				       struct scatterlist *sgl)
{
	int blksz, nblk, dma_ch;

	dma_ch = host->dma_ch;
	if (data->flags & MMC_DATA_WRITE) {
		omap_set_dma_dest_params(dma_ch, 0, OMAP_DMA_AMODE_CONSTANT,
			(host->mapbase + OMAP_HSMMC_DATA), 0, 0);
		omap_set_dma_src_params(dma_ch, 0, OMAP_DMA_AMODE_POST_INC,
			sg_dma_address(sgl), 0, 0);
	} else {
		omap_set_dma_src_params(dma_ch, 0, OMAP_DMA_AMODE_CONSTANT,
			(host->mapbase + OMAP_HSMMC_DATA), 0, 0);
		omap_set_dma_dest_params(dma_ch, 0, OMAP_DMA_AMODE_POST_INC,
			sg_dma_address(sgl), 0, 0);
	}

	blksz = host->data->blksz;
	nblk = sg_dma_len(sgl) / blksz;

	omap_set_dma_transfer_params(dma_ch, OMAP_DMA_DATA_TYPE_S32,
			blksz / 4, nblk, OMAP_DMA_SYNC_FRAME,
			mmc_omap_get_dma_sync_dev(host, data),
			!(data->flags & MMC_DATA_WRITE));

	omap_start_dma(dma_ch);
}

/*
 * DMA call back function
 */
static void mmc_omap_dma_cb(int lch, u16 ch_status, void *data)
{
	struct mmc_omap_host *host = data;

	if (ch_status & OMAP2_DMA_MISALIGNED_ERR_IRQ)
		dev_dbg(mmc_dev(host->mmc), "MISALIGNED_ADRS_ERR\n");

	if (host->dma_ch < 0)
		return;

	host->dma_sg_idx++;
	if (host->dma_sg_idx < host->dma_len) {
		/* Fire up the next transfer. */
		mmc_omap_config_dma_params(host, host->data,
					   host->data->sg + host->dma_sg_idx);
		return;
	}

	omap_free_dma(host->dma_ch);
	host->dma_ch = -1;
	/*
	 * DMA Callback: run in interrupt context.
	 * mutex_unlock will throw a kernel warning if used.
	 */
	up(&host->sem);
}

/*
 * Routine to configure and start DMA for the MMC card
 */
static int
mmc_omap_start_dma_transfer(struct mmc_omap_host *host, struct mmc_request *req)
{
	int dma_ch = 0, ret = 0, err = 1, i;
	struct mmc_data *data = req->data;

	/* Sanity check: all the SG entries must be aligned by block size. */
	for (i = 0; i < data->sg_len; i++) {
		struct scatterlist *sgl;

		sgl = data->sg + i;
		if (sgl->length % data->blksz)
			return -EINVAL;
	}
	if ((data->blksz % 4) != 0)
		/* REVISIT: The MMC buffer increments only when MSB is written.
		 * Return error for blksz which is non multiple of four.
		 */
		return -EINVAL;

	/*
	 * If for some reason the DMA transfer is still active,
	 * we wait for timeout period and free the dma
	 */
	if (host->dma_ch != -1) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(100);
		if (down_trylock(&host->sem)) {
			omap_free_dma(host->dma_ch);
			host->dma_ch = -1;
			up(&host->sem);
			return err;
		}
	} else {
		if (down_trylock(&host->sem))
			return err;
	}

	ret = omap_request_dma(mmc_omap_get_dma_sync_dev(host, data), "MMC/SD",
			       mmc_omap_dma_cb, host, &dma_ch);
	if (ret != 0) {
		dev_err(mmc_dev(host->mmc),
			"%s: omap_request_dma() failed with %d\n",
			mmc_hostname(host->mmc), ret);
		return ret;
	}

	host->dma_len = dma_map_sg(mmc_dev(host->mmc), data->sg,
			data->sg_len, mmc_omap_get_dma_dir(host, data));
	host->dma_ch = dma_ch;
	host->dma_sg_idx = 0;

	mmc_omap_config_dma_params(host, data, data->sg);

	return 0;
}

static void set_data_timeout(struct mmc_omap_host *host,
			     struct mmc_request *req)
{
	unsigned int timeout, cycle_ns;
	uint32_t reg, clkd, dto = 0;

	reg = OMAP_HSMMC_READ(host->base, SYSCTL);
	clkd = (reg & CLKD_MASK) >> CLKD_SHIFT;
	if (clkd == 0)
		clkd = 1;

	cycle_ns = 1000000000 / (clk_get_rate(host->fclk) / clkd);
	timeout = req->data->timeout_ns / cycle_ns;
	timeout += req->data->timeout_clks;
	if (timeout) {
		while ((timeout & 0x80000000) == 0) {
			dto += 1;
			timeout <<= 1;
		}
		dto = 31 - dto;
		timeout <<= 1;
		if (timeout && dto)
			dto += 1;
		if (dto >= 13)
			dto -= 13;
		else
			dto = 0;
		if (dto > 14)
			dto = 14;
	}

	reg &= ~DTO_MASK;
	reg |= dto << DTO_SHIFT;
	OMAP_HSMMC_WRITE(host->base, SYSCTL, reg);
}

/*
 * Configure block length for MMC/SD cards and initiate the transfer.
 */
static int
mmc_omap_prepare_data(struct mmc_omap_host *host, struct mmc_request *req)
{
	int ret;
	host->data = req->data;

	if (req->data == NULL) {
		OMAP_HSMMC_WRITE(host->base, BLK, 0);
		return 0;
	}

	OMAP_HSMMC_WRITE(host->base, BLK, (req->data->blksz)
					| (req->data->blocks << 16));
	set_data_timeout(host, req);

	if (host->use_dma) {
		ret = mmc_omap_start_dma_transfer(host, req);
		if (ret != 0) {
			dev_dbg(mmc_dev(host->mmc), "MMC start dma failure\n");
			return ret;
		}
	}
	return 0;
}

/*
 * Request function. for read/write operation
 */
static void omap_mmc_request(struct mmc_host *mmc, struct mmc_request *req)
{
	struct mmc_omap_host *host = mmc_priv(mmc);
	int err;

	WARN_ON(host->mrq != NULL);
	host->mrq = req;
	err = mmc_omap_prepare_data(host, req);
	if (err) {
		req->cmd->error = err;
		if (req->data)
			req->data->error = err;
		host->mrq = NULL;
		mmc_request_done(mmc, req);
		return;
	}

	mmc_omap_start_command(host, req->cmd, req->data);
}


/* Routine to configure clock values. Exposed API to core */
static void omap_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct mmc_omap_host *host = mmc_priv(mmc);
	u16 dsor = 0;
	unsigned long regval;
	unsigned long timeout;
	u32 con;
	int do_send_init_stream = 0;

	mmc_host_enable(host->mmc);

	if (ios->power_mode != host->power_mode) {
		switch (ios->power_mode) {
		case MMC_POWER_OFF:
			mmc_slot(host).set_power(host->dev, host->slot_id,
						 0, 0);
			host->vdd = 0;
			break;
		case MMC_POWER_UP:
			mmc_slot(host).set_power(host->dev, host->slot_id,
						 1, ios->vdd);
			host->vdd = ios->vdd;
			break;
		case MMC_POWER_ON:
			do_send_init_stream = 1;
			break;
		}
		host->power_mode = ios->power_mode;
	}

	/* FIXME: set registers based only on changes to ios */

	con = OMAP_HSMMC_READ(host->base, CON);
	switch (mmc->ios.bus_width) {
	case MMC_BUS_WIDTH_8:
		OMAP_HSMMC_WRITE(host->base, CON, con | DW8);
		break;
	case MMC_BUS_WIDTH_4:
		OMAP_HSMMC_WRITE(host->base, CON, con & ~DW8);
		OMAP_HSMMC_WRITE(host->base, HCTL,
			OMAP_HSMMC_READ(host->base, HCTL) | FOUR_BIT);
		break;
	case MMC_BUS_WIDTH_1:
		OMAP_HSMMC_WRITE(host->base, CON, con & ~DW8);
		OMAP_HSMMC_WRITE(host->base, HCTL,
			OMAP_HSMMC_READ(host->base, HCTL) & ~FOUR_BIT);
		break;
	}

	if (host->id == OMAP_MMC1_DEVID) {
		/* Only MMC1 can interface at 3V without some flavor
		 * of external transceiver; but they all handle 1.8V.
		 */
		if ((OMAP_HSMMC_READ(host->base, HCTL) & SDVSDET) &&
			(ios->vdd == DUAL_VOLT_OCR_BIT)) {
				/*
				 * The mmc_select_voltage fn of the core does
				 * not seem to set the power_mode to
				 * MMC_POWER_UP upon recalculating the voltage.
				 * vdd 1.8v.
				 */
				if (omap_mmc_switch_opcond(host, ios->vdd) != 0)
					dev_dbg(mmc_dev(host->mmc),
						"Switch operation failed\n");
		}
	}

	if (ios->clock) {
		dsor = OMAP_MMC_MASTER_CLOCK / ios->clock;
		if (dsor < 1)
			dsor = 1;

		if (OMAP_MMC_MASTER_CLOCK / dsor > ios->clock)
			dsor++;

		if (dsor > 250)
			dsor = 250;
	}
	omap_mmc_stop_clock(host);
	regval = OMAP_HSMMC_READ(host->base, SYSCTL);
	regval = regval & ~(CLKD_MASK);
	regval = regval | (dsor << 6) | (DTO << 16);
	OMAP_HSMMC_WRITE(host->base, SYSCTL, regval);
	OMAP_HSMMC_WRITE(host->base, SYSCTL,
		OMAP_HSMMC_READ(host->base, SYSCTL) | ICE);

	/* Wait till the ICS bit is set */
	timeout = jiffies + msecs_to_jiffies(MMC_TIMEOUT_MS);
	while ((OMAP_HSMMC_READ(host->base, SYSCTL) & ICS) != ICS
		&& time_before(jiffies, timeout))
		msleep(1);

	OMAP_HSMMC_WRITE(host->base, SYSCTL,
		OMAP_HSMMC_READ(host->base, SYSCTL) | CEN);

	if (do_send_init_stream)
		send_init_stream(host);

	con = OMAP_HSMMC_READ(host->base, CON);
	if (ios->bus_mode == MMC_BUSMODE_OPENDRAIN)
		OMAP_HSMMC_WRITE(host->base, CON, con | OD);
	else
		OMAP_HSMMC_WRITE(host->base, CON, con & ~OD);

	if (host->power_mode == MMC_POWER_OFF)
		mmc_host_disable(host->mmc);
	else
		mmc_host_lazy_disable(host->mmc);
}

static int omap_hsmmc_get_cd(struct mmc_host *mmc)
{
	struct mmc_omap_host *host = mmc_priv(mmc);

	if (!mmc_slot(host).card_detect)
		return -ENOSYS;
	return mmc_slot(host).card_detect(mmc_slot(host).card_detect_irq);
}

static int omap_hsmmc_get_ro(struct mmc_host *mmc)
{
	struct mmc_omap_host *host = mmc_priv(mmc);

	if (!mmc_slot(host).get_ro)
		return -ENOSYS;
	return mmc_slot(host).get_ro(host->dev, 0);
}

static void omap_hsmmc_init(struct mmc_omap_host *host)
{
	u32 hctl, capa, value;

	/* Only MMC1 supports 3.0V */
	if (host->id == OMAP_MMC1_DEVID) {
		hctl = SDVS30;
		capa = VS30 | VS18;
	} else {
		hctl = SDVS18;
		capa = VS18;
	}

	value = OMAP_HSMMC_READ(host->base, HCTL) & ~SDVS_MASK;
	OMAP_HSMMC_WRITE(host->base, HCTL, value | hctl);

	value = OMAP_HSMMC_READ(host->base, CAPA);
	OMAP_HSMMC_WRITE(host->base, CAPA, value | capa);

	/* Set the controller to AUTO IDLE mode */
	value = OMAP_HSMMC_READ(host->base, SYSCONFIG);
	OMAP_HSMMC_WRITE(host->base, SYSCONFIG, value | AUTOIDLE);

	/* Set SD bus power bit */
	set_sd_bus_power(host);
}

/*
 * Dynamic power saving handling, FSM:
 *   ENABLED -> DISABLED -> CARDSLEEP / REGSLEEP -> OFF
 *     ^___________|          |                      |
 *     |______________________|______________________|
 *
 * ENABLED:   mmc host is fully functional
 * DISABLED:  fclk is off
 * CARDSLEEP: fclk is off, card is asleep, voltage regulator is asleep
 * REGSLEEP:  fclk is off, voltage regulator is asleep
 * OFF:       fclk is off, voltage regulator is off
 *
 * Transition handlers return the timeout for the next state transition
 * or negative error.
 */

enum {ENABLED = 0, DISABLED, CARDSLEEP, REGSLEEP, OFF};

/* Handler for [ENABLED -> DISABLED] transition */
static int omap_mmc_enabled_to_disabled(struct mmc_omap_host *host)
{
	omap_mmc_save_ctx(host);
	clk_disable(host->fclk);
	host->dpm_state = DISABLED;

	dev_dbg(mmc_dev(host->mmc), "ENABLED -> DISABLED\n");

	if (host->power_mode == MMC_POWER_OFF)
		return 0;

	return msecs_to_jiffies(OMAP_MMC_SLEEP_TIMEOUT);
}

/* Handler for [DISABLED -> REGSLEEP / CARDSLEEP] transition */
static int omap_mmc_disabled_to_sleep(struct mmc_omap_host *host)
{
	int err, new_state;

	if (!mmc_try_claim_host(host->mmc))
		return 0;

	clk_enable(host->fclk);
	omap_mmc_restore_ctx(host);
	if (mmc_card_can_sleep(host->mmc)) {
		err = mmc_card_sleep(host->mmc);
		if (err < 0) {
			clk_disable(host->fclk);
			mmc_release_host(host->mmc);
			return err;
		}
		new_state = CARDSLEEP;
	} else
		new_state = REGSLEEP;
	if (mmc_slot(host).set_sleep)
		mmc_slot(host).set_sleep(host->dev, host->slot_id, 1, 0,
					 new_state == CARDSLEEP);
	/* FIXME: turn off bus power and perhaps interrupts too */
	clk_disable(host->fclk);
	host->dpm_state = new_state;

	mmc_release_host(host->mmc);

	dev_dbg(mmc_dev(host->mmc), "DISABLED -> %s\n",
		host->dpm_state == CARDSLEEP ? "CARDSLEEP" : "REGSLEEP");

	if ((host->mmc->caps & MMC_CAP_NONREMOVABLE) ||
	    mmc_slot(host).card_detect ||
	    (mmc_slot(host).get_cover_state &&
	     mmc_slot(host).get_cover_state(host->dev, host->slot_id)))
		return msecs_to_jiffies(OMAP_MMC_OFF_TIMEOUT);

	return 0;
}

/* Handler for [REGSLEEP / CARDSLEEP -> OFF] transition */
static int omap_mmc_sleep_to_off(struct mmc_omap_host *host)
{
	if (!mmc_try_claim_host(host->mmc))
		return 0;

	if (!((host->mmc->caps & MMC_CAP_NONREMOVABLE) ||
	      mmc_slot(host).card_detect ||
	      (mmc_slot(host).get_cover_state &&
	       mmc_slot(host).get_cover_state(host->dev, host->slot_id)))) {
		mmc_release_host(host->mmc);
		return 0;
	}

	mmc_slot(host).set_power(host->dev, host->slot_id, 0, 0);
	host->vdd = 0;
	host->power_mode = MMC_POWER_OFF;

	dev_dbg(mmc_dev(host->mmc), "%s -> OFF\n",
		host->dpm_state == CARDSLEEP ? "CARDSLEEP" : "REGSLEEP");

	host->dpm_state = OFF;

	mmc_release_host(host->mmc);

	return 0;
}

/* Handler for [DISABLED -> ENABLED] transition */
static int omap_mmc_disabled_to_enabled(struct mmc_omap_host *host)
{
	int err;

	err = clk_enable(host->fclk);
	if (err < 0)
		return err;

	omap_mmc_restore_ctx(host);

	host->dpm_state = ENABLED;

	dev_dbg(mmc_dev(host->mmc), "DISABLED -> ENABLED\n");

	return 0;
}

/* Handler for [SLEEP -> ENABLED] transition */
static int omap_mmc_sleep_to_enabled(struct mmc_omap_host *host)
{
	if (!mmc_try_claim_host(host->mmc))
		return 0;

	clk_enable(host->fclk);
	omap_mmc_restore_ctx(host);
	if (mmc_slot(host).set_sleep)
		mmc_slot(host).set_sleep(host->dev, host->slot_id, 0,
			 host->vdd, host->dpm_state == CARDSLEEP);
	if (mmc_card_can_sleep(host->mmc))
		mmc_card_awake(host->mmc);

	dev_dbg(mmc_dev(host->mmc), "%s -> ENABLED\n",
		host->dpm_state == CARDSLEEP ? "CARDSLEEP" : "REGSLEEP");

	host->dpm_state = ENABLED;

	mmc_release_host(host->mmc);

	return 0;
}

/* Handler for [OFF -> ENABLED] transition */
static int omap_mmc_off_to_enabled(struct mmc_omap_host *host)
{
	clk_enable(host->fclk);

	omap_mmc_restore_ctx(host);
	omap_hsmmc_init(host);
	mmc_power_restore_host(host->mmc);

	host->dpm_state = ENABLED;

	dev_dbg(mmc_dev(host->mmc), "OFF -> ENABLED\n");

	return 0;
}

/*
 * Bring MMC host to ENABLED from any other PM state.
 */
static int omap_mmc_enable(struct mmc_host *mmc)
{
	struct mmc_omap_host *host = mmc_priv(mmc);

	switch (host->dpm_state) {
	case DISABLED:
		return omap_mmc_disabled_to_enabled(host);
	case CARDSLEEP:
	case REGSLEEP:
		return omap_mmc_sleep_to_enabled(host);
	case OFF:
		return omap_mmc_off_to_enabled(host);
	default:
		dev_dbg(mmc_dev(host->mmc), "UNKNOWN state\n");
		return -EINVAL;
	}
}

/*
 * Bring MMC host in PM state (one level deeper).
 */
static int omap_mmc_disable(struct mmc_host *mmc, int lazy)
{
	struct mmc_omap_host *host = mmc_priv(mmc);

	switch (host->dpm_state) {
	case ENABLED: {
		int delay;

		delay = omap_mmc_enabled_to_disabled(host);
		if (lazy || delay < 0)
			return delay;
		return 0;
	}
	case DISABLED:
		return omap_mmc_disabled_to_sleep(host);
	case CARDSLEEP:
	case REGSLEEP:
		return omap_mmc_sleep_to_off(host);
	default:
		dev_dbg(mmc_dev(host->mmc), "UNKNOWN state\n");
		return -EINVAL;
	}
}

static int omap_mmc_enable_fclk(struct mmc_host *mmc)
{
	struct mmc_omap_host *host = mmc_priv(mmc);
	int err;

	err = clk_enable(host->fclk);
	if (err)
		return err;
	dev_dbg(mmc_dev(host->mmc), "mmc_fclk: enabled\n");
	omap_mmc_restore_ctx(host);
	return 0;
}

static int omap_mmc_disable_fclk(struct mmc_host *mmc, int lazy)
{
	struct mmc_omap_host *host = mmc_priv(mmc);

	omap_mmc_save_ctx(host);
	clk_disable(host->fclk);
	dev_dbg(mmc_dev(host->mmc), "mmc_fclk: disabled\n");
	return 0;
}

static const struct mmc_host_ops mmc_omap_ops = {
	.enable = omap_mmc_enable_fclk,
	.disable = omap_mmc_disable_fclk,
	.request = omap_mmc_request,
	.set_ios = omap_mmc_set_ios,
	.get_cd = omap_hsmmc_get_cd,
	.get_ro = omap_hsmmc_get_ro,
	/* NYET -- enable_sdio_irq */
};

static const struct mmc_host_ops mmc_omap_ps_ops = {
	.enable = omap_mmc_enable,
	.disable = omap_mmc_disable,
	.request = omap_mmc_request,
	.set_ios = omap_mmc_set_ios,
	.get_cd = omap_hsmmc_get_cd,
	.get_ro = omap_hsmmc_get_ro,
	/* NYET -- enable_sdio_irq */
};

#ifdef CONFIG_DEBUG_FS

static int mmc_regs_show(struct seq_file *s, void *data)
{
	struct mmc_host *mmc = s->private;
	struct mmc_omap_host *host = mmc_priv(mmc);
	struct omap_mmc_platform_data *pdata = host->pdata;
	int context_loss = 0;

	if (pdata->get_context_loss_count)
		context_loss = pdata->get_context_loss_count(host->dev);

	seq_printf(s, "mmc%d:\n"
			" enabled:\t%d\n"
			" dpm_state:\t%d\n"
			" nesting_cnt:\t%d\n"
			" ctx_loss:\t%d:%d\n"
			"\nregs:\n",
			mmc->index, mmc->enabled ? 1 : 0,
			host->dpm_state, mmc->nesting_cnt,
			host->context_loss, context_loss);

	if (host->suspended || host->dpm_state == OFF) {
		seq_printf(s, "host suspended, can't read registers\n");
		return 0;
	}

	if (clk_enable(host->fclk) != 0) {
		seq_printf(s, "can't read the regs\n");
		return 0;
	}

	seq_printf(s, "SYSCONFIG:\t0x%08x\n",
			OMAP_HSMMC_READ(host->base, SYSCONFIG));
	seq_printf(s, "CON:\t\t0x%08x\n",
			OMAP_HSMMC_READ(host->base, CON));
	seq_printf(s, "HCTL:\t\t0x%08x\n",
			OMAP_HSMMC_READ(host->base, HCTL));
	seq_printf(s, "SYSCTL:\t\t0x%08x\n",
			OMAP_HSMMC_READ(host->base, SYSCTL));
	seq_printf(s, "IE:\t\t0x%08x\n",
			OMAP_HSMMC_READ(host->base, IE));
	seq_printf(s, "ISE:\t\t0x%08x\n",
			OMAP_HSMMC_READ(host->base, ISE));
	seq_printf(s, "CAPA:\t\t0x%08x\n",
			OMAP_HSMMC_READ(host->base, CAPA));

	clk_disable(host->fclk);

	return 0;
}

static int mmc_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_regs_show, inode->i_private);
}

static const struct file_operations mmc_regs_fops = {
	.open           = mmc_regs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void omap_mmc_debugfs(struct mmc_host *mmc)
{
	if (mmc->debugfs_root)
		debugfs_create_file("regs", S_IRUSR, mmc->debugfs_root,
			mmc, &mmc_regs_fops);
}

#else

static void omap_mmc_debugfs(struct mmc_host *mmc)
{
}

#endif

static int __init omap_mmc_probe(struct platform_device *pdev)
{
	struct omap_mmc_platform_data *pdata = pdev->dev.platform_data;
	struct mmc_host *mmc;
	struct mmc_omap_host *host = NULL;
	struct resource *res;
	int ret = 0, irq;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "Platform Data is missing\n");
		return -ENXIO;
	}

	if (pdata->nr_slots == 0) {
		dev_err(&pdev->dev, "No Slots\n");
		return -ENXIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (res == NULL || irq < 0)
		return -ENXIO;

	res = request_mem_region(res->start, res->end - res->start + 1,
							pdev->name);
	if (res == NULL)
		return -EBUSY;

	mmc = mmc_alloc_host(sizeof(struct mmc_omap_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto err;
	}

	host		= mmc_priv(mmc);
	host->mmc	= mmc;
	host->pdata	= pdata;
	host->dev	= &pdev->dev;
	host->use_dma	= 1;
	host->dev->dma_mask = &pdata->dma_mask;
	host->dma_ch	= -1;
	host->irq	= irq;
	host->id	= pdev->id;
	host->slot_id	= 0;
	host->mapbase	= res->start;
	host->base	= ioremap(host->mapbase, SZ_4K);
	host->power_mode = -1;

	platform_set_drvdata(pdev, host);
	INIT_WORK(&host->mmc_carddetect_work, mmc_omap_detect);

	if (mmc_slot(host).power_saving)
		mmc->ops	= &mmc_omap_ps_ops;
	else
		mmc->ops	= &mmc_omap_ops;

	mmc->f_min	= 400000;
	mmc->f_max	= 52000000;

	sema_init(&host->sem, 1);

	host->iclk = clk_get(&pdev->dev, "ick");
	if (IS_ERR(host->iclk)) {
		ret = PTR_ERR(host->iclk);
		host->iclk = NULL;
		goto err1;
	}
	host->fclk = clk_get(&pdev->dev, "fck");
	if (IS_ERR(host->fclk)) {
		ret = PTR_ERR(host->fclk);
		host->fclk = NULL;
		clk_put(host->iclk);
		goto err1;
	}

	omap_mmc_save_ctx(host);

	mmc->caps |= MMC_CAP_DISABLE;
	mmc_set_disable_delay(mmc, OMAP_MMC_DISABLED_TIMEOUT);
	/* we start off in DISABLED state */
	host->dpm_state = DISABLED;

	if (mmc_host_enable(host->mmc) != 0) {
		clk_put(host->iclk);
		clk_put(host->fclk);
		goto err1;
	}

	if (clk_enable(host->iclk) != 0) {
		mmc_host_disable(host->mmc);
		clk_put(host->iclk);
		clk_put(host->fclk);
		goto err1;
	}

	host->dbclk = clk_get(&pdev->dev, "mmchsdb_fck");
	/*
	 * MMC can still work without debounce clock.
	 */
	if (IS_ERR(host->dbclk))
		dev_warn(mmc_dev(host->mmc), "Failed to get debounce clock\n");
	else
		if (clk_enable(host->dbclk) != 0)
			dev_dbg(mmc_dev(host->mmc), "Enabling debounce"
							" clk failed\n");
		else
			host->dbclk_enabled = 1;

	/* Since we do only SG emulation, we can have as many segs
	 * as we want. */
	mmc->max_phys_segs = 1024;
	mmc->max_hw_segs = 1024;

	mmc->max_blk_size = 512;       /* Block Length at max can be 1024 */
	mmc->max_blk_count = 0xFFFF;    /* No. of Blocks is 16 bits */
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->max_seg_size = mmc->max_req_size;

	mmc->caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED |
		     MMC_CAP_WAIT_WHILE_BUSY;

	if (mmc_slot(host).wires >= 8)
		mmc->caps |= MMC_CAP_8_BIT_DATA;
	else if (mmc_slot(host).wires >= 4)
		mmc->caps |= MMC_CAP_4_BIT_DATA;

	if (mmc_slot(host).nonremovable)
		mmc->caps |= MMC_CAP_NONREMOVABLE;

	omap_hsmmc_init(host);

	/* Select DMA lines */
	switch (host->id) {
	case OMAP_MMC1_DEVID:
		host->dma_line_tx = OMAP24XX_DMA_MMC1_TX;
		host->dma_line_rx = OMAP24XX_DMA_MMC1_RX;
		break;
	case OMAP_MMC2_DEVID:
		host->dma_line_tx = OMAP24XX_DMA_MMC2_TX;
		host->dma_line_rx = OMAP24XX_DMA_MMC2_RX;
		break;
	case OMAP_MMC3_DEVID:
		host->dma_line_tx = OMAP34XX_DMA_MMC3_TX;
		host->dma_line_rx = OMAP34XX_DMA_MMC3_RX;
		break;
	default:
		dev_err(mmc_dev(host->mmc), "Invalid MMC id\n");
		goto err_irq;
	}

	/* Request IRQ for MMC operations */
	ret = request_irq(host->irq, mmc_omap_irq, IRQF_DISABLED,
			mmc_hostname(mmc), host);
	if (ret) {
		dev_dbg(mmc_dev(host->mmc), "Unable to grab HSMMC IRQ\n");
		goto err_irq;
	}

	/* initialize power supplies, gpios, etc */
	if (pdata->init != NULL) {
		if (pdata->init(&pdev->dev) != 0) {
			dev_dbg(mmc_dev(host->mmc), "late init error\n");
			goto err_irq_cd_init;
		}
	}
	mmc->ocr_avail = mmc_slot(host).ocr_mask;

	/* Request IRQ for card detect */
	if ((mmc_slot(host).card_detect_irq)) {
		ret = request_irq(mmc_slot(host).card_detect_irq,
				  omap_mmc_cd_handler,
				  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
					  | IRQF_DISABLED,
				  mmc_hostname(mmc), host);
		if (ret) {
			dev_dbg(mmc_dev(host->mmc),
				"Unable to grab MMC CD IRQ\n");
			goto err_irq_cd;
		}
	}

	OMAP_HSMMC_WRITE(host->base, ISE, INT_EN_MASK);
	OMAP_HSMMC_WRITE(host->base, IE, INT_EN_MASK);

	mmc_host_lazy_disable(host->mmc);

	mmc_add_host(mmc);

	if (mmc_slot(host).name != NULL) {
		ret = device_create_file(&mmc->class_dev, &dev_attr_slot_name);
		if (ret < 0)
			goto err_slot_name;
	}
	if (mmc_slot(host).card_detect_irq && mmc_slot(host).get_cover_state) {
		ret = device_create_file(&mmc->class_dev,
					&dev_attr_cover_switch);
		if (ret < 0)
			goto err_cover_switch;
	}

	omap_mmc_debugfs(mmc);

	return 0;

err_cover_switch:
	device_remove_file(&mmc->class_dev, &dev_attr_cover_switch);
err_slot_name:
	mmc_remove_host(mmc);
err_irq_cd:
	free_irq(mmc_slot(host).card_detect_irq, host);
err_irq_cd_init:
	free_irq(host->irq, host);
err_irq:
	mmc_host_disable(host->mmc);
	clk_disable(host->iclk);
	clk_put(host->fclk);
	clk_put(host->iclk);
	if (host->dbclk_enabled) {
		clk_disable(host->dbclk);
		clk_put(host->dbclk);
	}

err1:
	iounmap(host->base);
err:
	dev_dbg(mmc_dev(host->mmc), "Probe Failed\n");
	release_mem_region(res->start, res->end - res->start + 1);
	if (host)
		mmc_free_host(mmc);
	return ret;
}

static int omap_mmc_remove(struct platform_device *pdev)
{
	struct mmc_omap_host *host = platform_get_drvdata(pdev);
	struct resource *res;

	if (host) {
		mmc_host_enable(host->mmc);
		mmc_remove_host(host->mmc);
		if (host->pdata->cleanup)
			host->pdata->cleanup(&pdev->dev);
		free_irq(host->irq, host);
		if (mmc_slot(host).card_detect_irq)
			free_irq(mmc_slot(host).card_detect_irq, host);
		flush_scheduled_work();

		mmc_host_disable(host->mmc);
		clk_disable(host->iclk);
		clk_put(host->fclk);
		clk_put(host->iclk);
		if (host->dbclk_enabled) {
			clk_disable(host->dbclk);
			clk_put(host->dbclk);
		}

		mmc_free_host(host->mmc);
		iounmap(host->base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, res->end - res->start + 1);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int omap_mmc_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct mmc_omap_host *host = platform_get_drvdata(pdev);

	if (host && host->suspended)
		return 0;

	if (host) {
		host->suspended = 1;
		if (host->pdata->suspend) {
			ret = host->pdata->suspend(&pdev->dev,
							host->slot_id);
			if (ret) {
				dev_dbg(mmc_dev(host->mmc),
					"Unable to handle MMC board"
					" level suspend\n");
				host->suspended = 0;
				return ret;
			}
		}
		cancel_work_sync(&host->mmc_carddetect_work);
		mmc_host_enable(host->mmc);
		ret = mmc_suspend_host(host->mmc, state);
		if (ret == 0) {
			OMAP_HSMMC_WRITE(host->base, ISE, 0);
			OMAP_HSMMC_WRITE(host->base, IE, 0);


			OMAP_HSMMC_WRITE(host->base, HCTL,
				OMAP_HSMMC_READ(host->base, HCTL) & ~SDBP);
			mmc_host_disable(host->mmc);
			clk_disable(host->iclk);
			clk_disable(host->dbclk);
		} else {
			host->suspended = 0;
			if (host->pdata->resume) {
				ret = host->pdata->resume(&pdev->dev,
							  host->slot_id);
				if (ret)
					dev_dbg(mmc_dev(host->mmc),
						"Unmask interrupt failed\n");
			}
			mmc_host_disable(host->mmc);
		}

	}
	return ret;
}

/* Routine to resume the MMC device */
static int omap_mmc_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct mmc_omap_host *host = platform_get_drvdata(pdev);

	if (host && !host->suspended)
		return 0;

	if (host) {
		ret = clk_enable(host->iclk);
		if (ret)
			goto clk_en_err;

		if (clk_enable(host->dbclk) != 0)
			dev_dbg(mmc_dev(host->mmc),
					"Enabling debounce clk failed\n");

		if (mmc_host_enable(host->mmc) != 0) {
			clk_disable(host->iclk);
			goto clk_en_err;
		}

		omap_hsmmc_init(host);

		if (host->pdata->resume) {
			ret = host->pdata->resume(&pdev->dev, host->slot_id);
			if (ret)
				dev_dbg(mmc_dev(host->mmc),
					"Unmask interrupt failed\n");
		}

		/* Notify the core to resume the host */
		ret = mmc_resume_host(host->mmc);
		if (ret == 0)
			host->suspended = 0;
		mmc_host_lazy_disable(host->mmc);
	}

	return ret;

clk_en_err:
	dev_dbg(mmc_dev(host->mmc),
		"Failed to enable MMC clocks during resume\n");
	return ret;
}

#else
#define omap_mmc_suspend	NULL
#define omap_mmc_resume		NULL
#endif

static struct platform_driver omap_mmc_driver = {
	.remove		= omap_mmc_remove,
	.suspend	= omap_mmc_suspend,
	.resume		= omap_mmc_resume,
	.driver		= {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init omap_mmc_init(void)
{
	/* Register the MMC driver */
	return platform_driver_probe(&omap_mmc_driver, omap_mmc_probe);
}

static void __exit omap_mmc_cleanup(void)
{
	/* Unregister MMC driver */
	platform_driver_unregister(&omap_mmc_driver);
}

module_init(omap_mmc_init);
module_exit(omap_mmc_cleanup);

MODULE_DESCRIPTION("OMAP High Speed Multimedia Card driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");
