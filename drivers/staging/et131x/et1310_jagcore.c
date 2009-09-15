/*
 * Agere Systems Inc.
 * 10/100/1000 Base-T Ethernet Driver for the ET1301 and ET131x series MACs
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *   http://www.agere.com
 *
 *------------------------------------------------------------------------------
 *
 * et1310_jagcore.c - All code pertaining to the ET1301/ET131x's JAGcore
 *
 *------------------------------------------------------------------------------
 *
 * SOFTWARE LICENSE
 *
 * This software is provided subject to the following terms and conditions,
 * which you should read carefully before using the software.  Using this
 * software indicates your acceptance of these terms and conditions.  If you do
 * not agree with these terms and conditions, do not use the software.
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source or binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 * . Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following Disclaimer as comments in the code as
 *    well as in the documentation and/or other materials provided with the
 *    distribution.
 *
 * . Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following Disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * . Neither the name of Agere Systems Inc. nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Disclaimer
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
 * USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
 * RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include "et131x_version.h"
#include "et131x_debug.h"
#include "et131x_defs.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>

#include "et1310_phy.h"
#include "et1310_pm.h"
#include "et1310_jagcore.h"

#include "et131x_adapter.h"
#include "et131x_initpci.h"

/* Data for debugging facilities */
#ifdef CONFIG_ET131X_DEBUG
extern dbg_info_t *et131x_dbginfo;
#endif /* CONFIG_ET131X_DEBUG */

/**
 * ConfigGlobalRegs - Used to configure the global registers on the JAGCore
 * @pAdpater: pointer to our adapter structure
 */
void ConfigGlobalRegs(struct et131x_adapter *etdev)
{
	struct _GLOBAL_t __iomem *pGbl = &etdev->regs->global;

	DBG_ENTER(et131x_dbginfo);

	if (etdev->RegistryPhyLoopbk == false) {
		if (etdev->RegistryJumboPacket < 2048) {
			/* Tx / RxDMA and Tx/Rx MAC interfaces have a 1k word
			 * block of RAM that the driver can split between Tx
			 * and Rx as it desires.  Our default is to split it
			 * 50/50:
			 */
			writel(0, &pGbl->rxq_start_addr);
			writel(PARM_RX_MEM_END_DEF, &pGbl->rxq_end_addr);
			writel(PARM_RX_MEM_END_DEF + 1, &pGbl->txq_start_addr);
			writel(INTERNAL_MEM_SIZE - 1, &pGbl->txq_end_addr);
		} else if (etdev->RegistryJumboPacket < 8192) {
			/* For jumbo packets > 2k but < 8k, split 50-50. */
			writel(0, &pGbl->rxq_start_addr);
			writel(INTERNAL_MEM_RX_OFFSET, &pGbl->rxq_end_addr);
			writel(INTERNAL_MEM_RX_OFFSET + 1, &pGbl->txq_start_addr);
			writel(INTERNAL_MEM_SIZE - 1, &pGbl->txq_end_addr);
		} else {
			/* 9216 is the only packet size greater than 8k that
			 * is available. The Tx buffer has to be big enough
			 * for one whole packet on the Tx side. We'll make
			 * the Tx 9408, and give the rest to Rx
			 */
			writel(0x0000, &pGbl->rxq_start_addr);
			writel(0x01b3, &pGbl->rxq_end_addr);
			writel(0x01b4, &pGbl->txq_start_addr);
			writel(INTERNAL_MEM_SIZE - 1,&pGbl->txq_end_addr);
		}

		/* Initialize the loopback register. Disable all loopbacks. */
		writel(0, &pGbl->loopback.value);
	} else {
		/* For PHY Line loopback, the memory is configured as if Tx
		 * and Rx both have all the memory.  This is because the
		 * RxMAC will write data into the space, and the TxMAC will
		 * read it out.
		 */
		writel(0, &pGbl->rxq_start_addr);
		writel(INTERNAL_MEM_SIZE - 1, &pGbl->rxq_end_addr);
		writel(0, &pGbl->txq_start_addr);
		writel(INTERNAL_MEM_SIZE - 1, &pGbl->txq_end_addr);

		/* Initialize the loopback register (MAC loopback). */
		writel(1, &pGbl->loopback);
	}

	/* MSI Register */
	writel(0, &pGbl->msi_config.value);

	/* By default, disable the watchdog timer.  It will be enabled when
	 * a packet is queued.
	 */
	writel(0, &pGbl->watchdog_timer);

	DBG_LEAVE(et131x_dbginfo);
}

/**
 * ConfigMMCRegs - Used to configure the main memory registers in the JAGCore
 * @etdev: pointer to our adapter structure
 */
void ConfigMMCRegs(struct et131x_adapter *etdev)
{
	DBG_ENTER(et131x_dbginfo);
	/* All we need to do is initialize the Memory Control Register */
	writel(ET_MMC_ENABLE, &etdev->regs->mmc.mmc_ctrl);
	DBG_LEAVE(et131x_dbginfo);
}

/**
 *	et131x_enable_interrupts	-	enable interrupt
 *	@adapter: et131x device
 *
 *	Enable the appropriate interrupts on the ET131x according to our
 *	configuration
 */

void et131x_enable_interrupts(struct et131x_adapter *adapter)
{
	u32 mask;

	/* Enable all global interrupts */
	if (adapter->FlowControl == TxOnly || adapter->FlowControl == Both)
		mask = INT_MASK_ENABLE;
	else
		mask = INT_MASK_ENABLE_NO_FLOW;

	if (adapter->DriverNoPhyAccess)
		mask |= ET_INTR_PHY;

	adapter->CachedMaskValue = mask;
	writel(mask, &adapter->regs->global.int_mask);
}

/**
 *	et131x_disable_interrupts	-	interrupt disable
 *	@adapter: et131x device
 *
 *	Block all interrupts from the et131x device at the device itself
 */

void et131x_disable_interrupts(struct et131x_adapter *adapter)
{
	/* Disable all global interrupts */
	adapter->CachedMaskValue = INT_MASK_DISABLE;
	writel(INT_MASK_DISABLE, &adapter->regs->global.int_mask);
}
