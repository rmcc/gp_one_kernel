//------------------------------------------------------------------------------
// <copyright file="dksdio_drv.c" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
// 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
static const char athId[] __attribute__ ((unused)) = "$Id: //depot/sw/releases/olca2.1-RC/host/art/sdio_driver/dksdio_drv.c#1 $";

#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/wireless.h>

#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#include "htc_api.h"
#include "htc_internal.h"
#include "bmi.h"
#include "athdrv.h"
#include "dksdio.h"

MODULE_LICENSE("Dual BSD/GPL");

//#define MAX_RX_BUFFERS 33  // 8.25KB -- one extra buffer than max size in application
#define MAX_RX_BUFFERS 16  //
#define SEND_ENDPOINT   0
#define RECV_ENDPOINT   1
#define	MAX_ENDPOINTS		3
#define MBOX_MAX_MSG_SIZE  (256-HTC_HEADER_LEN)
#define DK_SDIO_TRANSFER_SIZE 256
#define MAX_RX_BUF_SIZE	(MAX_RX_BUFFERS * DK_SDIO_TRANSFER_SIZE)

int enablehtc = 0;
unsigned int debughif = 1;
unsigned int debugbmi = 1;
 //unsigned int debughtc = 255;
  //int debuglevel = 255;
  //int debugdriver = 1;
int debughtc = 128;
int debuglevel = 0;
int debugdriver = 0;
int dk_htc_start = 0;
int bypasswmi = 0;
//int sdio1bitmode = 0;
int onebitmode = 0;
//int forcereset = 1;
int resetok = 1;

module_param(bypasswmi, int, 0644);
module_param(debugbmi, int, 0644);
module_param(debughif, int, 0644);
module_param(debuglevel, int, 0644);
module_param(debugdriver, int, 0644);
module_param(onebitmode, int, 0644);
module_param(debughtc, int, 0644);
module_param(enablehtc, int, 0644);
//module_param(forcereset, int, 0644);
module_param(resetok, int, 0644);
#ifdef DEBUG
int mboxnum = HTC_MAILBOX_NUM_MAX;
int hangcount[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
int txcreditsavailable[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
int txcreditsconsumed[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
int txcreditintrenable[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 1};
int txcreditintrenableaggregate[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 1};
int txcount[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
int txcompletecount[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
//module_param_array(hangcount, int, mboxnum, 0644);
//module_param_array(txcreditsavailable, int, mboxnum, 0644);
//module_param_array(txcreditsconsumed, int, mboxnum, 0644);
//module_param_array(txcreditintrenable, int, mboxnum, 0644);
//module_param_array(txcreditintrenableaggregate, int, mboxnum, 0644);
//module_param_array(txcount, int, mboxnum, 0644);
//module_param_array(txcompletecount, int, mboxnum, 0644);

unsigned int tx_attempt[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
unsigned int tx_post[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
//unsigned int tx_complete[HTC_MAILBOX_NUM_MAX] = {[0 ... HTC_MAILBOX_NUM_MAX - 1] = 0};
//module_param_array(tx_attempt, int, mboxnum, 0644);
//module_param_array(tx_post, int, mboxnum, 0644);
//module_param_array(tx_complete, int, mboxnum, 0644);

#endif /* DEBUG */

//#unsigned int sdiobusspeedlow = 0;
unsigned int busspeedlow = 0;
module_param(busspeedlow, int, 0644);

int sRxBufIndex, eRxBufIndex;
unsigned char rxBuffer[MAX_RX_BUF_SIZE];
int  rRxBuffers[MAX_ENDPOINTS];
unsigned char *rxBuffers;

int rx_complete = 0;
int tx_complete = 0;

#ifdef DEBUG
#define AR_DEBUG_PRINTF(args...)        if (debugdriver) printk(args);
#define AR_DEBUG2_PRINTF(args...)        if (debugdriver >= 2) printk(args);
#else
#define AR_DEBUG_PRINTF(args...)
#define AR_DEBUG2_PRINTF(args...)
#endif


/*
#undef DEBUG
#define DEBUG
#ifdef DEBUG
#define AR_DEBUG_PRINTF(args...)        if (debugdriver) printk(args);
#else
#define AR_DEBUG_PRINTF(args...)
#endif
*/


/* Function declarations */
static int dk_sdio_init_module(void);
static void dk_sdio_cleanup_module(void);
static void dkDestroy(void);
static void DK_HTCStop( HTC_TARGET *htcTarget );
DECLARE_WAIT_QUEUE_HEAD (txq);
DECLARE_WAIT_QUEUE_HEAD (rxq);

static int dbg_print = 0;


struct dk_dev {
   HTC_TARGET *htcTarget;
   HIF_DEVICE *hifDevice;
};


struct dk_dev dkdev;

/*
 * HTC event handlers
 */
static void dk_sdio_avail_ev(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                            HTC_EVENT_ID id, HTC_EVENT_INFO *evInfo, void *arg);

static void dk_sdio_unavail_ev(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                            HTC_EVENT_ID id, HTC_EVENT_INFO *evInfo, void *arg);

static void dk_sdio_rx(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                      HTC_EVENT_ID id, HTC_EVENT_INFO *evInfo, void *arg);

static void dk_sdio_rx_refill(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                      HTC_EVENT_ID evId, HTC_EVENT_INFO *evInfo, void *arg);

static void dk_sdio_tx_complete(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                      HTC_EVENT_ID id, HTC_EVENT_INFO *evInfo, void *arg);

/*
 * Static variables
 */

static int __init
dk_sdio_init_module(void)
{
    static int probed = 0;

AR_DEBUG_PRINTF("Init module 0 \n");
//printk(KERN_EMERG "Init module  :%x: \n", onebitmode);

    if (probed) {
        return -ENODEV;
    }
    probed++;

AR_DEBUG_PRINTF("Init module probe=%d \n", probed);
    HTCInit();

    HTCEventReg(NULL, ENDPOINT_UNUSED, HTC_TARGET_AVAILABLE,
		dk_sdio_avail_ev, NULL);
    HTCEventReg(NULL, ENDPOINT_UNUSED, HTC_TARGET_UNAVAILABLE,
		dk_sdio_unavail_ev, NULL);

    return 0;
}

static void __exit
dk_sdio_cleanup_module(void)
{
    AR_DEBUG_PRINTF( "dk_sdio_cleanup: success\n");
	dkDestroy();
    HTCShutDown(NULL);
	dkdev.htcTarget = NULL;
}


static int dk_sdio_open
(
    struct inode *inode,
    struct file *file
)
{
    int minor;
    int major;
    A_STATUS status;

#ifdef DK_DEBUG
        AR_DEBUG_PRINTF("DK:: dk_sdio_open \n");
#endif
    major = MAJOR(inode->i_rdev);
    minor = MINOR(inode->i_rdev);
    minor = minor & 0x0f;

	dk_htc_start = 0;
    A_MEMZERO(rRxBuffers, sizeof(rRxBuffers));

    AR_DEBUG_PRINTF( "dk_sdio_open: enter\n");


    return 0;
}

void DK_HTCStop( HTC_TARGET *htcTarget ) {

    A_STATUS status;

    	status = HTCEventReg(dkdev.htcTarget, RECV_ENDPOINT, HTC_BUFFER_RECEIVED, NULL, NULL);
    	if (status == A_OK) {
        	status = HTCEventReg(dkdev.htcTarget, RECV_ENDPOINT, HTC_DATA_AVAILABLE, NULL, NULL);
    	}
    	if (status == A_OK) {
        	status = HTCEventReg(dkdev.htcTarget, SEND_ENDPOINT, HTC_BUFFER_SENT, NULL, NULL);
    	}

    	if (status != A_OK) {
			printk(KERN_ALERT "DK_HTCStop::Event un register failure\n");
        	return ;
    	}

        HTCStop(dkdev.htcTarget);

}

static int
dk_sdio_release(struct inode *inode, struct file *file)
{
    AR_DEBUG_PRINTF( "dk_sdio_release: enter\n");
	if (dkdev.htcTarget) {
	 if (dk_htc_start) {
       DK_HTCStop(dkdev.htcTarget);
       dk_htc_start  = 0;
	 }
	}

    AR_DEBUG_PRINTF( "dk_sdio_release: exit\n");
    return 0;
}


ssize_t
dk_sdio_write(struct file *file, const char *buf, size_t length, loff_t *offset)
{
    unsigned char *buffer, *bufpos;
    A_STATUS status;
	A_INT32 bytes, bIndex, cnt;

	sRxBufIndex = 0;
	eRxBufIndex = 0;
    bytes = length;

	AR_DEBUG_PRINTF("dk_sdio_write start of function\n");
	if (!dk_htc_start) {
		dk_htc_start = 1;


    	rxBuffers = (unsigned char *)kmalloc(MAX_RX_BUFFERS * DK_SDIO_TRANSFER_SIZE, GFP_KERNEL);
        if (rxBuffers == NULL)
        {
			printk(KERN_ALERT "rx buffers allocation fail\n");
            return -ENOMEM;
        }

    	status = HTCEventReg(dkdev.htcTarget, RECV_ENDPOINT, HTC_BUFFER_RECEIVED, dk_sdio_rx, NULL);
    	if (status == A_OK) {
        	status = HTCEventReg(dkdev.htcTarget, RECV_ENDPOINT, HTC_DATA_AVAILABLE, dk_sdio_rx_refill, NULL);
    	}

    	if (status == A_OK) {
        	status = HTCEventReg(dkdev.htcTarget, SEND_ENDPOINT, HTC_BUFFER_SENT, dk_sdio_tx_complete, NULL);
    	}

    	if (status != A_OK) {
			printk(KERN_ALERT "dk_sdio_write::Event register failure\n");
        	return -EIO;
    	}

    	status = HTCStart(dkdev.htcTarget);
		if (status != A_OK) {
			printk(KERN_ALERT "HTCStart failure\n");
        	return -EIO;
		}
	}

	AR_DEBUG_PRINTF("dk_sdio_write\n");
	AR_DEBUG_PRINTF("dk_sdio_write:[0x%x:%d]\n", (int)buf, length);
    if ((buffer = (unsigned char *)kmalloc(length+HTC_HEADER_LEN, GFP_KERNEL)) != NULL)
    {
			AR_DEBUG_PRINTF("dk_sdio_write:buffer[0x%x:%d]\n", (int)buffer, length);
			if (dbg_print) printk(KERN_EMERG "dk_sdio_write:buffer[0x%x:%d]\n", (int)buffer, length);

            memset(buffer, '\0', length);

			AR_DEBUG_PRINTF("dk_sdio_write:call cfu\n");
			AR_DEBUG_PRINTF("dk_sdio_write:call cfu:[%x]\n", (int)&buffer[0]);
            if (copy_from_user(&buffer[0+HTC_HEADER_LEN],
                                   (unsigned char *)buf, length))
            {
                	kfree(buffer);
				    printk(KERN_ALERT "write:Copy buffer from user space: fail\n");
                    return -EFAULT;
            }
            else
            {
              bIndex = 0;
    	      bufpos = buffer+HTC_HEADER_LEN;
    	      while (bytes>0) {

		        if (bytes > MBOX_MAX_MSG_SIZE)
			        cnt = MBOX_MAX_MSG_SIZE;
		        else
			        cnt = bytes;

#ifdef DEBUG
				AR_DEBUG_PRINTF("dk_sdio_write:kb[0x%x]:\n", (int)bufpos);
	  			dumpBytes(bufpos, cnt);
	  			AR_DEBUG_PRINTF("call HTCBufferSend");
#endif
				tx_complete = 0;

        		status = HTCBufferSend(dkdev.htcTarget, SEND_ENDPOINT, bufpos, cnt, NULL);
                if (status != A_OK)
                {
				    printk(KERN_ALERT "write:HTC Buffer send : fail\n");
                    return -EFAULT;
                }

                AR_DEBUG_PRINTF( "Wait for tx_complete\n");
	            wait_event_interruptible(txq, tx_complete);
                AR_DEBUG_PRINTF( "txq event signalled\n");

                bIndex += cnt;
		        bytes -= cnt;
		        bufpos += cnt;
			  }
			}
    }
    else
    {
				printk(KERN_ALERT "write:Buffer alloc : fail\n");
                return -ENOMEM;
    }
    if (dbg_print) printk("Free up buffer %x\n", (unsigned int) buffer);
    kfree(buffer);
    return length;
}

ssize_t dk_sdio_read(struct file *file, char *buf, size_t length, loff_t *offset)
{
	unsigned char *rx_buf = NULL;

    AR_DEBUG_PRINTF( "dk_sdio_read:len=%d:sR=%d:eR=%d", length, sRxBufIndex, eRxBufIndex);
    //printk(KERN_EMERG "dk_sdio_read:len=%d:sR=%d:eR=%d", length, sRxBufIndex, eRxBufIndex);
	rx_complete = 0;

	if ( ((eRxBufIndex - sRxBufIndex)  < length) && ((eRxBufIndex - sRxBufIndex) == 0)) {
       AR_DEBUG_PRINTF( "dk_sdio_read:wait for rxq");
	   wait_event_interruptible(rxq, rx_complete);
       AR_DEBUG_PRINTF( "dk_sdio_read:rxq event triggered");
	}


	rx_buf = (rxBuffer + sRxBufIndex);
#ifdef DEBUG
    AR_DEBUG_PRINTF("copy to user::dk_sdio_read:len=%d:sR=%d:eR=%d", length, sRxBufIndex, eRxBufIndex);
	dumpBytes(rx_buf, length);
#endif
    if (copy_to_user(buf, rx_buf, length))
    {
		printk(KERN_ALERT "read:HTC Buffer receive : fail\n");
        return -EFAULT;
    }
	sRxBufIndex += length;
	if (sRxBufIndex > MAX_RX_BUF_SIZE) {
				sRxBufIndex= 0;
				eRxBufIndex= 0;
	}

    return length;

}



static void
dk_sdio_tx_complete(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                      HTC_EVENT_ID evId, HTC_EVENT_INFO *evInfo, void *arg)
{

    AR_DEBUG_PRINTF( "dk_sdio_tx_complete\n");
    A_ASSERT(evId == HTC_BUFFER_SENT);
	tx_complete = 1;
	wake_up_interruptible_sync(&txq);
    AR_DEBUG_PRINTF( "dk_sdio_tx:wakeup txq");
}

/*
 * Receive event handler.  This is called by HTC when a packet is received
 */
int pktcount;
static void
dk_sdio_rx(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                      HTC_EVENT_ID evId, HTC_EVENT_INFO *evInfo, void *arg)
{

    A_ASSERT(evId == HTC_BUFFER_RECEIVED);


    AR_DEBUG_PRINTF( "dk_sdio_rx ep=%d - data=0x%x, len=0x%x ",
			eid, (A_UINT32)evInfo->buffer, evInfo->actualLength);
    if (dbg_print) printk(KERN_EMERG "dk_sdio_rx ep=%d - data=0x%x, len=0x%x ", eid, (A_UINT32)evInfo->buffer, evInfo->actualLength);

	if (evInfo->actualLength > HTC_HEADER_LEN) {
	   if (eRxBufIndex >= MAX_RX_BUF_SIZE)  {
            printk(KERN_ALERT  "dksdio_rx::exceeding buffer size\n" );
            printk(KERN_EMERG  "dk_sdio_rx:len=%d:sR=%d:eR=%d", evInfo->actualLength, sRxBufIndex, eRxBufIndex );
            return;
       }

    if (dbg_print) printk(KERN_EMERG "copy data from %x/%dB\n", (unsigned int)evInfo->buffer, evInfo->actualLength-HTC_HEADER_LEN);
	   A_MEMCPY( &rxBuffer[eRxBufIndex], evInfo->buffer, evInfo->actualLength);
	   eRxBufIndex += (evInfo->actualLength);

	   if (eRxBufIndex > MAX_RX_BUF_SIZE) eRxBufIndex= MAX_RX_BUF_SIZE;
	}
    rRxBuffers[eid]--;

//if (dbg_print) printk(KERN_EMERG "free up buffer %x\n", evInfo->buffer);
//	kfree(evInfo->buffer);
    AR_DEBUG_PRINTF( "dk_sdio_rx:%d:rxBeId=%d: freed up rx buffer\n", eid, rRxBuffers[eid]);

	rx_complete = 1;
	wake_up_interruptible_sync(&rxq);
    AR_DEBUG_PRINTF( "dk_sdio_rx:wakeup rxq");

}

static void
dk_sdio_rx_refill(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                      HTC_EVENT_ID evId, HTC_EVENT_INFO *evInfo, void *arg)
{
		unsigned char *rxBuf;
    AR_DEBUG_PRINTF( "dk_sdio_rx_refill:%d:rxBeId=%d: \n", eid, rRxBuffers[eid]);
    while (rRxBuffers[eid] < MAX_RX_BUFFERS) {
        //rxBuf = (unsigned char *)kmalloc(DK_SDIO_TRANSFER_SIZE, GFP_KERNEL);
        rxBuf = (unsigned char *)(rxBuffers+(rRxBuffers[eid] * DK_SDIO_TRANSFER_SIZE));
            if (rxBuf == NULL)
            {
				printk(KERN_ALERT "rx_refill:rxBuf alloc : fail\n");
                return;
            }
	if (dbg_print) printk(KERN_ALERT "rx_refill:rxBuf alloc @%x: \n", (unsigned int)rxBuf);
    AR_DEBUG_PRINTF( "dk_sdio_rx_refill:%d:rxBeId=%d: providing htc with buffer\n", eid, rRxBuffers[eid]);
	   HTCBufferReceive(htcTarget, eid, (void *)rxBuf, DK_SDIO_TRANSFER_SIZE, NULL);
       rRxBuffers[eid]++;
	}
}

/* This would basically hold all the private ioctls that are not related to
   WLAN operation */
static int
dk_sdio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    HTC_TARGET *htcTarget = dkdev.htcTarget;
    HIF_DEVICE *hifDevice = dkdev.hifDevice;
    int ret, i, size, param;
    unsigned int address;
    unsigned int length;
    unsigned int count;
    unsigned char *buffer;
    DK_IDATA dkrq;
    DK_IDATA *rq;
    char *userdata;

    if (copy_from_user(&dkrq, (void *)arg, sizeof(DK_IDATA))) {
        ret = -EFAULT;
		return ret;
    }
    rq = &dkrq;

    if (cmd == AR6000_IOCTL_EXTENDED)
    {
        /*
         * This allows for many more wireless ioctls than would otherwise
         * be available.  Applications embed the actual ioctl command in
         * the first word of the parameter block, and use the command
         * AR6000_IOCTL_EXTENDED_CMD on the ioctl call.
         */
        get_user(cmd, (int *)rq->ifr_data);
        userdata = (char *)(((unsigned int *)rq->ifr_data)+1);
    }
    else
    {
        userdata = (char *)rq->ifr_data;
    }

    switch(cmd)
    {
        case AR6000_XIOCTL_BMI_DONE:
            ret = BMIDone(hifDevice);
            break;

        case AR6000_XIOCTL_BMI_READ_MEMORY:
            get_user(address, (unsigned int *)userdata);
            get_user(length, (unsigned int *)userdata + 1);
            AR_DEBUG_PRINTF("Read Memory (address: 0x%x, length: %d)\n",
                             address, length);
            if ((buffer = (unsigned char *)A_MALLOC(length)) != NULL) {
                A_MEMZERO(buffer, length);
                ret = BMIReadMemory(hifDevice, address, buffer, length);
                if (copy_to_user(rq->ifr_data, buffer, length)) {
                    ret = -EFAULT;
                }
                A_FREE(buffer);
            } else {
                ret = -ENOMEM;
            }
            break;

        case AR6000_XIOCTL_BMI_WRITE_MEMORY:
            get_user(address, (unsigned int *)userdata);
            get_user(length, (unsigned int *)userdata + 1);
            AR_DEBUG_PRINTF("Write Memory (address: 0x%x, length: %d)\n",
                             address, length);
            if ((buffer = (unsigned char *)A_MALLOC(length)) != NULL) {
                A_MEMZERO(buffer, length);
                if (copy_from_user(buffer, &userdata[sizeof(address) +
                                   sizeof(length)], length))
                {
                    ret = -EFAULT;
                } else {
                    ret = BMIWriteMemory(hifDevice, address, buffer, length);
                }
                A_FREE(buffer);
            } else {
                ret = -ENOMEM;
            }
            break;

   case AR6000_XIOCTL_BMI_TEST:
           AR_DEBUG_PRINTF("No longer supported\n");
           ret = -EOPNOTSUPP;
           break;

        case AR6000_XIOCTL_BMI_EXECUTE:
            get_user(address, (unsigned int *)userdata);
            get_user(param, (unsigned int *)userdata + 1);
            AR_DEBUG_PRINTF("Execute (address: 0x%x, param: %d)\n",
                             address, param);
            ret = BMIExecute(hifDevice, address, &param);
            put_user(param, (unsigned int *)rq->ifr_data); /* return value */
            break;

        case AR6000_XIOCTL_BMI_SET_APP_START:
            get_user(address, (unsigned int *)userdata);
            AR_DEBUG_PRINTF("Set App Start (address: 0x%x)\n", address);
            ret = BMISetAppStart(hifDevice, address);
            break;

        case AR6000_XIOCTL_BMI_READ_SOC_REGISTER:
            get_user(address, (unsigned int *)userdata);
            ret = BMIReadSOCRegister(hifDevice, address, &param);
            put_user(param, (unsigned int *)rq->ifr_data); /* return value */
            break;

        case AR6000_XIOCTL_BMI_WRITE_SOC_REGISTER:
            get_user(address, (unsigned int *)userdata);
            get_user(param, (unsigned int *)userdata + 1);
            ret = BMIWriteSOCRegister(hifDevice, address, param);
            break;


        default:
            ret = -EOPNOTSUPP;
    }

    return ret;
}

static struct file_operations dk_sdio_fops = {
    owner:  THIS_MODULE,
    open:   dk_sdio_open,
    read:   dk_sdio_read,
    write:   dk_sdio_write,
    ioctl:   dk_sdio_ioctl,
    release: dk_sdio_release
};


/*
 * HTC Event handlers
 */
static void
dk_sdio_avail_ev(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid, HTC_EVENT_ID event,
                HTC_EVENT_INFO *evInfo,
                void *arg)
{
    A_STATUS status;

    AR_DEBUG_PRINTF( "dk_sdio_available\n");

    A_ASSERT(event == HTC_TARGET_AVAILABLE);

    dkdev.htcTarget = NULL;

    dkdev.htcTarget = htcTarget;
    dkdev.hifDevice = evInfo->buffer;
	BMIInit();

    status = register_chrdev(DK_SDIO_MAJOR_NUMBER, THIS_DEV_NAME,&dk_sdio_fops);
    if (status < 0) {
    	AR_DEBUG_PRINTF( "dk_sdio_avail: register char device fail %x\n", (A_UINT32)status);
    }

    AR_DEBUG_PRINTF( "dk_sdio_avail: htcTarget=0x%x\n", (A_UINT32)htcTarget);
}

void dkDestroy(void) {

	if (dkdev.htcTarget) {
	   if (dk_htc_start)  {
	        printk(KERN_EMERG "dkDestroy:Check HTC Stop\n");
            DK_HTCStop(dkdev.htcTarget);
	   }
	   //printk(KERN_EMERG "dkDestroy:HTCShutDown\n");
       HTCShutDown(dkdev.htcTarget);	/* frees up any buffers */
	   kfree(rxBuffers);
	   dk_htc_start = 0;
	   //printk(KERN_EMERG "dkDestroy:HTCShutDown done\n");
		dkdev.htcTarget = NULL;
		//printk(KERN_EMERG "dkDestroy:unregister \n");
    	unregister_chrdev(DK_SDIO_MAJOR_NUMBER, THIS_DEV_NAME);
		//printk(KERN_EMERG "dkDestroy:unregister done\n");
	}

}

static void
dk_sdio_unavail_ev(HTC_TARGET *htcTarget, HTC_ENDPOINT_ID eid,
                  HTC_EVENT_ID event, HTC_EVENT_INFO *evInfo, void *arg)
{
		//printk(KERN_EMERG "dk_sdio_unavail_ev\n");
	    //dk_htc_start = 0;
		dkDestroy();

}


module_init(dk_sdio_init_module);
module_exit(dk_sdio_cleanup_module);


