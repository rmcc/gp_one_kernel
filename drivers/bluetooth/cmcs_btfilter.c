/*
 *
 *  Bluetooth FILTER driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"
#include "cmcs_btfilter.h"


static COEX_CONNECTION_INFO   *pCoex_Info = NULL;
static struct work_struct	    coex_task;
static DEFINE_SPINLOCK(bt_coex_lock);

extern void COEX_SET_BT_STATE(u8 streamType,u8 status,WMI_SET_BT_PARAMS_CMD *pParam);

static void
do_coex_work(struct work_struct *work)
{
    u8 scoSlots;

    spin_lock(&bt_coex_lock);

    if(pCoex_Info == NULL){
        spin_unlock(&bt_coex_lock);
        return;
    }
    
    switch (pCoex_Info->streamType) {
    case BT_STREAM_SCAN:
        COEX_SET_BT_STATE(pCoex_Info->streamType,pCoex_Info->status,NULL);
        break;
    case BT_STREAM_SCO:
        if(pCoex_Info->status == BT_STATUS_START) {
            memset(&pCoex_Info->paramCmd, 0, sizeof(WMI_SET_BT_PARAMS_CMD));
            pCoex_Info->paramCmd.paramType = BT_PARAM_SCO;
            pCoex_Info->paramCmd.info.scoParams.numScoCyclesForceTrigger =10;
            pCoex_Info->paramCmd.info.scoParams.dataResponseTimeout = 20;
            pCoex_Info->paramCmd.info.scoParams.stompScoRules =2;
            pCoex_Info->paramCmd.info.scoParams.stompDutyCyleVal =2;
            pCoex_Info->paramCmd.info.scoParams.psPollLatencyFraction =1;
            pCoex_Info->paramCmd.info.scoParams.noSCOSlots = 2;
            pCoex_Info->paramCmd.info.scoParams.noIdleSlots = 4; 

            /*
            printk("          LinkType=%d, TransmissionInterval=%d, RetransmissionInterval=%d.\n"
                ,pCoex_Info->LinkType
               ,pCoex_Info->TransmissionInterval
               ,pCoex_Info->RetransmissionInterval);
            printk("          RxPacketLength=%d, TxPacketLength=%d, Valid=%s.\n"
                ,pCoex_Info->RxPacketLength
               ,pCoex_Info->TxPacketLength
               ,pCoex_Info->Valid?"true":"false");
            printk("    numScoCyclesForceTrigger : %d \n",pCoex_Info->paramCmd.info.scoParams.numScoCyclesForceTrigger);
            printk("    dataResponseTimeout      : %d \n",pCoex_Info->paramCmd.info.scoParams.dataResponseTimeout);
            printk("    stompScoRules            : %d \n",pCoex_Info->paramCmd.info.scoParams.stompScoRules);                
            printk("    stompDutyCyleVal         : %d \n",pCoex_Info->paramCmd.info.scoParams.stompDutyCyleVal);
            printk("    psPollLatencyFraction    : %d \n",pCoex_Info->paramCmd.info.scoParams.psPollLatencyFraction);
            printk("    noSCOSlots               : %d \n",pCoex_Info->paramCmd.info.scoParams.noSCOSlots);
            printk("    noIdleSlots              : %d \n",pCoex_Info->paramCmd.info.scoParams.noIdleSlots);     
            */
        }

        COEX_SET_BT_STATE(pCoex_Info->streamType,pCoex_Info->status,&pCoex_Info->paramCmd);
        break;
    case BT_STREAM_ESCO:
        if(pCoex_Info->status == BT_STATUS_START) {
            memset(&pCoex_Info->paramCmd, 0, sizeof(WMI_SET_BT_PARAMS_CMD));
            pCoex_Info->paramCmd.paramType = BT_PARAM_SCO;
            pCoex_Info->paramCmd.info.scoParams.numScoCyclesForceTrigger =1;
            pCoex_Info->paramCmd.info.scoParams.dataResponseTimeout = 20;
            pCoex_Info->paramCmd.info.scoParams.stompScoRules =2;
            pCoex_Info->paramCmd.info.scoParams.stompDutyCyleVal =2;
            pCoex_Info->paramCmd.info.scoParams.psPollLatencyFraction =3;
            if (pCoex_Info->TxPacketLength <= 90) {   // EV3:<=30, 2-EV3:<=60, 3-EV3:<=90
                        scoSlots = 1;
            } else {    // EV4:<=120, EV5:<=180, 2-EV5:<=360 3-EV5  
                        scoSlots = 3; 
            }
            // account for RX/TX 
            scoSlots *= 2;
            pCoex_Info->paramCmd.info.scoParams.noSCOSlots =  scoSlots;
            if (pCoex_Info->TransmissionInterval >= scoSlots) {
                 pCoex_Info->paramCmd.info.scoParams.noIdleSlots = pCoex_Info->TransmissionInterval - scoSlots;
            } else {
                 pCoex_Info->paramCmd.info.scoParams.noIdleSlots = 0;
                 printk("Invalid scoSlot,  got:%d, transInt: %d\n",scoSlots,pCoex_Info->TransmissionInterval);
            }

            /*
            printk("          LinkType=%d, TransmissionInterval=%d, RetransmissionInterval=%d.\n"
                    ,pCoex_Info->LinkType
                   ,pCoex_Info->TransmissionInterval
                   ,pCoex_Info->RetransmissionInterval);
            printk("          RxPacketLength=%d, TxPacketLength=%d, Valid=%s.\n"
                    ,pCoex_Info->RxPacketLength
                   ,pCoex_Info->TxPacketLength
                   ,pCoex_Info->Valid?"true":"false");
            printk("    numScoCyclesForceTrigger : %d \n",pCoex_Info->paramCmd.info.scoParams.numScoCyclesForceTrigger);
            printk("    dataResponseTimeout      : %d \n",pCoex_Info->paramCmd.info.scoParams.dataResponseTimeout);
            printk("    stompScoRules            : %d \n",pCoex_Info->paramCmd.info.scoParams.stompScoRules);                
            printk("    stompDutyCyleVal         : %d \n",pCoex_Info->paramCmd.info.scoParams.stompDutyCyleVal);
            printk("    psPollLatencyFraction    : %d \n",pCoex_Info->paramCmd.info.scoParams.psPollLatencyFraction);
            printk("    noSCOSlots               : %d \n",pCoex_Info->paramCmd.info.scoParams.noSCOSlots);
            printk("    noIdleSlots              : %d \n",pCoex_Info->paramCmd.info.scoParams.noIdleSlots);     
            */
        }

        COEX_SET_BT_STATE(pCoex_Info->streamType,pCoex_Info->status,&pCoex_Info->paramCmd);
        break;
    default:
        printk("unknow streamType=%d status=%d.\n",pCoex_Info->streamType,pCoex_Info->status);
    }        

    spin_unlock(&bt_coex_lock);
}

void BTFilter_Init(void)
{

    pCoex_Info = (COEX_CONNECTION_INFO *)kmalloc(sizeof(COEX_CONNECTION_INFO), GFP_KERNEL);
    if(pCoex_Info == NULL){
        printk("BTFilter_Init alloc memory fail.\n");
        return;
    }

	memset(pCoex_Info, 0, sizeof(COEX_CONNECTION_INFO));

    INIT_WORK(&coex_task, do_coex_work);
}


void BTFilter_Deinit(void)
{

    if(pCoex_Info) {
        spin_lock(&bt_coex_lock);

        COEX_SET_BT_STATE(BT_STREAM_SCAN,BT_STATUS_STOP,NULL);
        kfree(pCoex_Info);
        pCoex_Info = NULL;

        spin_unlock(&bt_coex_lock);
    }
}


void BTFilter_HCIEvent(unsigned char *pBuffer, int len)
{

    if(pCoex_Info == NULL)
        return;

    switch( HCI_GET_EVENT_CODE(pBuffer)) {
        case HCI_EVT_INQUIRY_COMPLETE :
            pCoex_Info->streamType = BT_STREAM_SCAN;
            pCoex_Info->status = BT_STATUS_STOP;
            schedule_work(&coex_task);
            break;

        case HCI_EVT_SCO_CONNECT_COMPLETE:
        case HCI_EVT_CONNECT_COMPLETE :                         
            if (BT_CONN_EVENT_STATUS_SUCCESS(pBuffer)) {
                switch(GET_BT_CONN_LINK_TYPE(pBuffer)){
                case BT_LINK_TYPE_SCO:
                    pCoex_Info->streamType = BT_STREAM_SCO;
                    pCoex_Info->status = BT_STATUS_START;
                    pCoex_Info->hScoConnect = GET_BT_CONN_HANDLE(pBuffer);
                    pCoex_Info->LinkType = GET_BT_CONN_LINK_TYPE(pBuffer);
                    pCoex_Info->TransmissionInterval = GET_TRANS_INTERVAL(pBuffer);
                    pCoex_Info->RetransmissionInterval = GET_RETRANS_INTERVAL(pBuffer);
                    pCoex_Info->RxPacketLength = GET_RX_PKT_LEN(pBuffer);
                    pCoex_Info->TxPacketLength = GET_TX_PKT_LEN(pBuffer);
                    pCoex_Info->Valid = true;
                    schedule_work(&coex_task);
                    break;
                case BT_LINK_TYPE_ESCO:
                    pCoex_Info->streamType = BT_STREAM_ESCO;
                    pCoex_Info->status = BT_STATUS_START;
                    pCoex_Info->hEscoConnect = GET_BT_CONN_HANDLE(pBuffer);
                    pCoex_Info->LinkType = GET_BT_CONN_LINK_TYPE(pBuffer);
                    pCoex_Info->TransmissionInterval = GET_TRANS_INTERVAL(pBuffer);
                    pCoex_Info->RetransmissionInterval = GET_RETRANS_INTERVAL(pBuffer);
                    pCoex_Info->RxPacketLength = GET_RX_PKT_LEN(pBuffer);
                    pCoex_Info->TxPacketLength = GET_TX_PKT_LEN(pBuffer);
                    pCoex_Info->Valid = true;
                    schedule_work(&coex_task);
                    break;
                case BT_LINK_TYPE_ACL:
                default:
                    break;
                }
             }                  
            break;

        case HCI_EVT_DISCONNECT :
            if(GET_BT_CONN_HANDLE(pBuffer) == pCoex_Info->hScoConnect) {
                pCoex_Info->streamType = BT_STREAM_SCO;
                pCoex_Info->status = BT_STATUS_STOP;
                pCoex_Info->hScoConnect = 0;
                pCoex_Info->Valid = false;
                schedule_work(&coex_task);
            }else if(GET_BT_CONN_HANDLE(pBuffer) == pCoex_Info->hEscoConnect) {
                pCoex_Info->streamType = BT_STREAM_ESCO;
                pCoex_Info->status = BT_STATUS_STOP;
                pCoex_Info->hEscoConnect = 0;
                pCoex_Info->Valid = false;
                schedule_work(&coex_task);
            }
            break;

        case HCI_EVT_CONNECT_REQUEST :
        case HCI_EVT_REMOTE_NAME_REQ :
        case HCI_EVT_ROLE_CHANGE :
            /* TODO */
            break;
        default:
            break;
    }


}

void BTFilter_HCICommand(unsigned char *pBuffer, int len)
{

    if(pCoex_Info == NULL)
        return;

    if (!IS_LINK_CONTROL_CMD(pBuffer)) {  // we only filter link control commands 
        return;
    }

    switch (HCI_GET_OP_CODE(pBuffer)) {
        case HCI_INQUIRY :
            pCoex_Info->streamType = BT_STREAM_SCAN;
            pCoex_Info->status = BT_STATUS_START;
            schedule_work(&coex_task);
            break;

        case HCI_INQUIRY_CANCEL :
            pCoex_Info->streamType = BT_STREAM_SCAN;
            pCoex_Info->status = BT_STATUS_STOP;
            schedule_work(&coex_task);
            break;

        default :
            break;
    }

}







