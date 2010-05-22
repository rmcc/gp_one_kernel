#ifdef SLEEP_REG_SUPPORT
#include <asm/arch/sleep.h>
#endif
#include <linux/dma-mapping.h>
#include "sdio_qc_hcd.h"
#include "linux/sdio_std_hcd_linux.h"

/* global variable */
static spinlock_t           sd_hcd_drv_lock = SPIN_LOCK_UNLOCKED;
static BOOL                 isPreCMD53W = FALSE;
BOOL                        chk_manuf_id_ok = FALSE;
#ifdef SLEEP_REG_SUPPORT
extern sleep_okts_handle    sd_hcd_sleep_handle;
#endif

#define SDIO_CIS_AREA_BEGIN   0x00001000
#define SDIO_CIS_AREA_END     0x00017fff
#define CISTPL_END            0xff
#define CISTPL_LINK_END       0xff
#define CISTPL_MANFID         0x20

struct SDIO_MANFID_TPL {
    UINT16  ManufacturerCode;   /* jedec code */
    UINT16  ManufacturerInfo;   /* manufacturer specific code */
} CT_PACK_STRUCT;

static int validate_dma(PSDHCD_INSTANCE   pHcInstance,
                        PSDREQUEST        pReq)
{
#ifdef DMA_SUPPORT
	if (pHcInstance->OsSpecific.dma.channel == -1) {
		DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: invalid DMA channel\n"));
		return SDIO_STATUS_INVALID_PARAMETER;
	}
	if ((pReq->BlockLen * pReq->BlockCount) < 32) {
		DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: data len < 32\n"));
		return SDIO_STATUS_INVALID_PARAMETER;
	}
	if ((pReq->BlockLen * pReq->BlockCount) % 32) {
		DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: ! data len % 32\n"));
		return SDIO_STATUS_INVALID_PARAMETER;
	}
#endif
	return 0;
}

SDIO_STATUS get_tuple(PSDDEVICE  pDevice,
                      UINT8      Tuple,
                      UINT32     *pTupleScanAddress,
                      PUINT8     pBuffer,
                      UINT8      *pLength)
{
    SDIO_STATUS       status = SDIO_STATUS_SUCCESS;
    UINT32            scanStart = *pTupleScanAddress;
    UINT8             tupleCode;
    UINT8             tupleLink;
    
    /* sanity check */
    if (scanStart < SDIO_CIS_AREA_BEGIN) {
        return SDIO_STATUS_CIS_OUT_OF_RANGE; 
    }
   
    while (TRUE) {           
        /* check for end */
        if (scanStart > SDIO_CIS_AREA_END) {
            status = SDIO_STATUS_TUPLE_NOT_FOUND;
            break;   
        }          

        /* get the code */
        status = SDLIB_IssueCMD52(pDevice, 0, scanStart, &tupleCode, 1, FALSE);

        if (!SDIO_SUCCESS(status)) {
            break;   
        } 

        if (CISTPL_END == tupleCode) {
            /* found the end */
            status = SDIO_STATUS_TUPLE_NOT_FOUND;
            break; 
        }

        /* bump past tuple code */
        scanStart++;
        /* get the tuple link value */
        status = SDLIB_IssueCMD52(pDevice, 0, scanStart, &tupleLink, 1, FALSE);

        if (!SDIO_SUCCESS(status)) {
            break;   
        }

        /* bump past tuple link*/
        scanStart++;           

        /* check tuple we just found */
        if (tupleCode == Tuple) {

//             printk("%s: Tuple:0x%2.2X Found at Address:0x%X, TupleLink:0x%X \n",
//                    __FUNCTION__,
//                    Tuple,
//                    (scanStart - 2),
//                    tupleLink);

            if (tupleLink != CISTPL_LINK_END) {
                /* return the next scan address to the caller */
                *pTupleScanAddress = scanStart + tupleLink; 
            } else {
                /* the tuple link is an end marker */ 
                *pTupleScanAddress = 0xFFFFFFFF;
            }

            /* go get the tuple */
            status = SDLIB_IssueCMD52(pDevice, 0, scanStart,pBuffer,min(*pLength,tupleLink), FALSE);

            if (SDIO_SUCCESS(status)) {
                /* set the actual return length */
                *pLength = min(*pLength,tupleLink); 
            }
            /* break out of loop */
            break;
        }

        /*increment past this entire tuple */
        scanStart += tupleLink;
    }
    
    return status;
}

BOOL check_manuf_id(PSDHCD pHcd)
{
    struct SDIO_MANFID_TPL          manfid;
    UINT8                           temp;
    UINT32                          tplAddr;
    SDIO_STATUS                     status = SDIO_STATUS_ERROR;

    /* variable init */
    temp = sizeof(manfid);
    tplAddr = pHcd->CardProperties.CommonCISPtr;

    /* get manuf ID */
    status = get_tuple(
                pHcd->pPseudoDev,
                CISTPL_MANFID,
                &tplAddr, 
                (PUINT8)&manfid,
                &temp);

    /* is ID correct? */
    if (!SDIO_SUCCESS(status)) {
//        printk("%s: SDIO Bus Driver: Failed to get MANFID tuple err:%d \n", __FUNCTION__, status);
        status = SDIO_STATUS_ERROR;
    } else {  
//        printk("%s: SDIO MANFID:0x%X, MANFINFO:0x%X \n",
//                __FUNCTION__,
//                CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerCode),
//                CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerInfo));

        if (((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerCode) & 0x000f) == (0x1 << 0x0))
         && ((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerCode) & 0x00f0) == (0x7 << 0x4))
         && ((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerCode) & 0x0f00) == (0x2 << 0x8))
         && ((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerCode) & 0xf000) == (0x0 << 0xc))
         && ((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerInfo) & 0x000f) == (0x1 << 0x0))
         && ((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerInfo) & 0x00f0) == (0x0 << 0x4))
         && ((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerInfo) & 0x0f00) == (0x2 << 0x8))
         && ((CT_LE16_TO_CPU_ENDIAN(manfid.ManufacturerInfo) & 0xf000) == (0x0 << 0xc))) {

            status = SDIO_STATUS_SUCCESS;
        } else {
            status = SDIO_STATUS_ERROR;
        }
    }

    return status;
}

/*
 * GetResponseData - get the response data 
 * Input:    pHcInstance - device context
 *           pReq - the request
 * Output: 
 * Return:
 * Notes:  
 */
void GetResponseData(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq)
{
    INT     byteCount;
    UINT32 resp;
    UINT8  readBuffer[16];
    UINT8  *pBuf;

    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP) {
        return;    
    }

    byteCount = SD_DEFAULT_RESPONSE_BYTES;        
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
        byteCount = SD_R2_RESPONSE_BYTES;         
    } 

    pBuf = &readBuffer[0];
    resp = READ_MMC_REG(pHcInstance, MCI_RESP_REG);
    //printk("[%s] resp=0x%x\n", __FUNCTION__, resp);
    *(pBuf + 5) = 0;
    *(pBuf + 4) = (UINT8)(resp >> 24);
    *(pBuf + 3) = (UINT8)(resp >> 16);
    *(pBuf + 2) = (UINT8)(resp >> 8);
    *(pBuf + 1) = (UINT8)(resp);
    *(pBuf) = 0;

    /* handle normal SD/MMC responses */        
    /* the standard host strips the CRC for all responses and puts them in 
     * a nice linear order */
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
	pReq->Response[0] = 0x00;
        memcpy(&pReq->Response[1],readBuffer,byteCount);
    } else {
        memcpy(pReq->Response,readBuffer,byteCount);
    }
}

#define POLL_TIMEOUT 1000000
SDIO_STATUS sdcc_cmd_send_verify(PSDHCD_INSTANCE pHcInstance)
{
    UINT32   status = 0;
    int      timeout = POLL_TIMEOUT;
 
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: command send verify\n"));

    while (timeout > 0) {
        status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
        DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: command send verify: status=[0x%x]\n", status));

        if (status & MCI_STATUS_CmdSent)
            break;
        timeout--;
    }
 
    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: command send verify: clear\n"));

    if (timeout > 0) {
        WRITE_MMC_REG(pHcInstance, MCI_CLEAR_REG, MCI_STATUS_CmdSent);
        timeout = POLL_TIMEOUT;
 
        while (timeout > 0) {
            /* make sure cmd_sent is cleared */
            status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: command send verify: status=[0x%x]\n", status));
            if (!(status & MCI_STATUS_CmdSent)) {
                DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: command send verify\n"));
                return SDIO_STATUS_SUCCESS;
            }
            timeout--;
        }
    }
 
    DBG_PRINT(SDDBG_ERROR, ("-SDIO Qualcomm HCD: command send verify: error\n"));
    return SDIO_STATUS_ERROR;
}

SDIO_STATUS sdcc_poll_cmd_response(PSDHCD_INSTANCE pHcInstance, UINT32 cmd)
{
   UINT32           i      = 0;
   UINT32           status = 0;
   SDIO_STATUS      rc     = SDIO_STATUS_SUCCESS;

   /* polling for status */
   while (i++ < POLL_TIMEOUT)
   {
      status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);

      /* response comes back */
      if (status & MCI_STATUS_CmdRespEnd) {
         WRITE_MMC_REG(pHcInstance, MCI_CLEAR_REG, MCI_STATUS_CmdRespEnd);
         rc = SDIO_STATUS_SUCCESS;
         break;
      }

      /* timedout on response */
      if (status & MCI_STATUS_CmdTimeOut) {
         DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: cmd resp timeout: status=[0x%x]\n", status));
         WRITE_MMC_REG(pHcInstance, MCI_CLEAR_REG, MCI_STATUS_CmdTimeOut);
         rc = SDIO_STATUS_BUS_RESP_TIMEOUT;
         break;
      }

      /* check CRC error */
      if (status & MCI_STATUS_CmdCrcFail) {
         /* the following cmd response doesn't have CRC. */
#define SD_ACMD41_SD_APP_OP_COND		41
#define SD_CMD1_SEND_OP_COND			1
#define SD_CMD5_IO_SEND_OP_COND			5
         if ((SD_ACMD41_SD_APP_OP_COND == cmd) ||
             (SD_CMD1_SEND_OP_COND     == cmd) ||
             (SD_CMD5_IO_SEND_OP_COND  == cmd)) {

            rc = SDIO_STATUS_SUCCESS;
         } else {
            rc = SDIO_STATUS_BUS_RESP_CRC_ERR;
         }

         WRITE_MMC_REG(pHcInstance, MCI_CLEAR_REG, MCI_STATUS_CmdCrcFail);
         break;
      }
   }

   return rc;
}

SDIO_STATUS ProcessCommandDone(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq, BOOL FromIsr)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;

    do {       
        if (pHcInstance->Cancel) {
            status = SDIO_STATUS_CANCELED;   
            break;
        }
                
        /* get the response data for the command */
        GetResponseData(pHcInstance, pReq);
        
        /* check for data */
        if (!(pReq->Flags & SDREQ_FLAGS_DATA_TRANS)) {
            /* no data phase, we're done */
            status = SDIO_STATUS_SUCCESS;
            break;
        }                   

        /* check with the bus driver if it is okay to continue with data */
        status = SDIO_CheckResponse(&pHcInstance->Hcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: Response for Data transfer error=[%d]\n", status));
            break;   
        }
    } while (FALSE); 

    return status;    
}

/*
 * SD Receive Data Operations  
 * @param pRequest: Bus Request Transaction
 * @return: BOOL
 */
static BOOL SDIPollingReceive(PSDHCD_INSTANCE pHcInstance, UINT8 *pBuff, UINT32 dwLen)
{
    UINT32      count     = 0;
    UINT32      fifo_word = 0;
    UINT32      status    = 0;
    int         timeout   = 10000;
    
    while (timeout > 0) {
    	
        status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
    
        while ((status & MCI_STATUS_RxDataAvlbl)) {	
        //if ((status & MCI_STATUS_RxDataAvlbl)) {	

            if(count < dwLen) {	
    
                fifo_word = READ_MMC_REG(pHcInstance, MCI_FIFO_REG);
                *pBuff       = (UINT8)(fifo_word);
                *(pBuff + 1) = (UINT8)(fifo_word >> 8);
                *(pBuff + 2) = (UINT8)(fifo_word >> 16);
                *(pBuff + 3) = (UINT8)(fifo_word >> 24);
                pBuff+=4;
                count+=4;
                timeout = 10000;
            }
        
            if(count >= dwLen) {
                goto READ_DONE;
            }

            //udelay(1);
            status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
        }

        timeout--;
        status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);

        if (status & MCI_STATUS_StartBitErr) {
            printk(" SDIO Qualcomm HCD: start bit error\n");
            goto READ_ERROR;
        }
        if (status & MCI_STATUS_RxOverrun) {
            printk(" SDIO Qualcomm HCD: RX over run error\n");
            goto READ_ERROR;
        }
        if (status & MCI_STATUS_DataTimeOut) {
            printk(" SDIO Qualcomm HCD: data timeout\n");
            goto READ_ERROR;
        }
        if (status & MCI_STATUS_DataCmdCrcFail) {
            printk(" SDIO Qualcomm HCD: CRC failed \n");
            goto READ_ERROR;
        }
    }
 
    if (timeout <=0 ) {
        printk(" SDIO Qualcomm HCD: polling RX timeout\n");
        goto READ_ERROR;
    }
    
READ_DONE:
    return TRUE;  

READ_ERROR:
    return FALSE;
}

/*
 * SD Transmit Data Operations  
 * @param pRequest: Bus Request Transaction
 * @return: BOOL
 */
static BOOL SDIPollingTransmit(PSDHCD_INSTANCE pHcInstance, char *pBuff, UINT32 dwLen)
{
    UINT32    count = 0;
    UINT32    fifo_word = 0;
    UINT32    status = 0;
    int       timeout = 10000;

    if (pHcInstance == NULL) {
        printk(" SDIO Qualcomm HCD: pHct==NULL\n");
        return FALSE;
    }
    if (pBuff == NULL) {
        printk(" SDIO Qualcomm HCD: pBuff==NULL\n");
        return FALSE;
    }
    if ((dwLen % 4) != 0) {
        printk(" SDIO Qualcomm HCD: len is not the multiple of 4\n");
        return FALSE;
    }

    status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);

    while (1) {
        if (!(status & MCI_STATUS_TxFifoFull)) {
            if (count < dwLen) {
                fifo_word  = ((UINT32)pBuff[0])          |
                              (((UINT32)pBuff[1]) << 8)  |
                              (((UINT32)pBuff[2]) << 16) |
                              (((UINT32)pBuff[3]) << 24);

                WRITE_MMC_REG(pHcInstance, MCI_FIFO_REG, fifo_word);
                pBuff += 4;
                count += 4;	 
                timeout = 10000;   	
            } else {
                break;
            }
        }

        status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
        timeout--;
    }

    if (timeout <= 0 ) {
        printk(" SDIO Qualcomm HCD: polling TX timeout\n");
        return FALSE;
    }

    return TRUE;
}

static BOOL hcd_waitPrgDone(PSDHCD pHcd)
{
    volatile UINT32     status = 0;
    UINT32              i = 0;
    PSDHCD_INSTANCE     pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext;
    
    while ((++i) < 10000) {
        status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
        if (status & MCI_STATUS_CmdSent) {
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: command sent\n"));
        }
        if (status & MCI_STATUS_ProgDone ) {
            break;
        } 
    } 

    if (i >= 10000) {
        printk(" SDIO Qualcomm HCD: wait program down failed\n");
        return FALSE;
    }
    return TRUE;
}


/* SDIO COMMAND 52 Definitions */
#define CMD52_READ                                            0
#define CMD52_WRITE                                           1
#define CMD52_READ_AFTER_WRITE                                1
#define CMD52_NORMAL_WRITE                                    0
#define SDIO_SET_CMD52_ARG(arg,rw,func,raw,address,writedata) \
                       (arg) = (((rw) & 1) << 31)           | \
                               (((func) & 0x7) << 28)       | \
                               (((raw) & 1) << 27)          | \
                               (1 << 26)                    | \
                               (((address) & 0x1FFFF) << 9) | \
                               (1 << 8)                     | \
                               ((writedata) & 0xFF)

#define SDIO_SET_CMD52_READ_ARG(arg,func,address)             \
    SDIO_SET_CMD52_ARG(arg,CMD52_READ,(func),0,address,0x00)

static void hcd_sendCMD52(PSDHCD pHcd)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext;
    SDREQUEST req;
    PSDREQUEST preq = &req;
    UINT32 temp=0;

    WRITE_MMC_REG(pHcInstance, MCI_CLEAR_REG, MCI_STATUS_ProgDone);
    preq->Command = 52;
    preq->Flags = SDREQ_FLAGS_RESP_SDIO_R5;
    preq->pCompleteContext = (PVOID)pHcd;
    temp |= MCI_CMD_ENABLED;
    temp |= MCI_CMD_PROG_ENABLED;
    temp |= MCI_CMD_RESPONSE;

    SDIO_SET_CMD52_READ_ARG(preq->Argument, 0, 0x0);
    WRITE_MMC_REG(pHcInstance, MCI_ARG_REG, preq->Argument);
    WRITE_MMC_REG(pHcInstance, MCI_CMD_REG, preq->Command|temp);
    hcd_waitPrgDone(pHcd);    
    status = sdcc_poll_cmd_response(pHcInstance, preq->Command);
    if (SDIO_SUCCESS(status)) {
       /* process the command completion */
	GetResponseData(pHcInstance, preq);
    }
}

static int hcd_wait_for_dataend(PSDHCD_INSTANCE pHcInstance, unsigned int retries)
{
	uint32_t reg_status;

	while(retries) {
		reg_status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
		if (reg_status & MCI_STATUS_DataEnd)
			break;
		retries--;
	}
	if (!retries)
		return -ETIMEDOUT;
	return 0;
}

static void hcd_stop_data(PSDHCD_INSTANCE pHcInstance)
{
	WRITE_MMC_REG(pHcInstance, MCI_DATA_CTL_REG, 0);
}

static void hcd_dma_complete_func(
	struct msm_dmov_cmd		*cmd,
	unsigned int			result,
	struct msm_dmov_errdata		*err)
{
#ifdef DMA_SUPPORT
	struct dma_data		*dma_data = container_of(cmd, struct dma_data, hdr);
	PSDHCD_INSTANCE		pHcInstance = dma_data->pHost;
	unsigned long		flags;
	uint32_t		reg_datacnt, reg_status;
	PSDREQUEST		pReq;
	int			rc = 0;

	spin_lock_irqsave(&sd_hcd_drv_lock, flags);

	pReq = pHcInstance->pReq;
	BUG_ON(!pReq);

	if (!(result & DMOV_RSLT_VALID))
		printk( KERN_ERR " SDIO Qualcomm HCD: DMA result is not valid\n");
	else if (!(result & DMOV_RSLT_DONE)) {
		/* Either an error or a flush occurred */
		if (result & DMOV_RSLT_ERROR)
			printk( KERN_ERR " SDIO Qualcomm HCD: DMA error[0x:%x]\n", result);
		if (result & DMOV_RSLT_FLUSH)
			printk( KERN_ERR " SDIO Qualcomm HCD: DMA chan flushed[0x:%x]\n", result);
		if (err) {
			printk( KERN_ERR "flush data: [%x, %x, %x, %x, %x, %x]\n",
				err->flush[0],
				err->flush[1],
				err->flush[2],
				err->flush[3],
				err->flush[4],
				err->flush[5]);
		}
	}

	reg_status = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);

	if ((result & DMOV_RSLT_DONE) && !(reg_status & MCI_STATUS_DataEnd)) {
		DBG_PRINT(SDDBG_TRACE,
			(" SDIO Qualcomm HCD: DMA DMOV_RSLT_DONE[0x%x] but wait for DATAEND[0x%x]\n",
			result,
			reg_status));

		if (result & DMOV_RSLT_VALID) {
			rc = hcd_wait_for_dataend(pHcInstance, MMC_MAX_DATAEND_WAIT_CNT);
			if (rc) {
				printk( KERN_ERR " SDIO Qualcomm HCD: time out for waiting DATAEND\n");
			} 
		}
	}

	hcd_stop_data(pHcInstance);

	/*
	 * According to QCT it is only ok to read datacnt
	 * after the transfer is complete (or otherwise stopped)
	 */
	reg_datacnt = READ_MMC_REG(pHcInstance, MCI_DATA_COUNT_REG);

	/*
	 * If we timed out on DMA then output some debugging
	 */
	if (rc) {
		printk( KERN_ERR " SDIO Qualcomm HCD: "
			"remaining data count[%d], blk size%d], blk num[%d]\n",
			reg_datacnt,
			pReq->BlockLen,
			pReq->BlockCount);
	}

	dma_unmap_sg(
		pHcInstance->Hcd.pDevice,
		&pHcInstance->OsSpecific.dma.sg,
		pHcInstance->OsSpecific.dma.num_ents,
		pHcInstance->OsSpecific.dma.dir);

	if (reg_datacnt) {
		printk( KERN_ERR " SDIO Qualcomm HCD: data isn't sent fully by DMA\n");
	}
	memset(&pHcInstance->OsSpecific.dma.sg, 0, sizeof(struct scatterlist));
	pHcInstance->pReq = NULL;

	spin_unlock_irqrestore(&sd_hcd_drv_lock, flags);
#endif
	return;
}

SDIO_STATUS SetUpHCDDMA(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq)
{
#ifdef DMA_SUPPORT
	struct nc_dmadata     *nc;
	dmov_box              *box;
	uint32_t              rows;
	uint32_t              crci;
	unsigned int          n;
	int                   i, rc;
	struct scatterlist    *sg = &pHcInstance->OsSpecific.dma.sg;

	rc = validate_dma(pHcInstance, pReq);
	if (rc) {
		DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: not valid dma\n"));
		return SDIO_STATUS_INVALID_PARAMETER;  
	}

	pHcInstance->OsSpecific.dma.num_ents = 1;
	sg_init_one(sg, pReq->pDataBuffer, pReq->BlockLen * pReq->BlockCount);

	nc = pHcInstance->OsSpecific.dma.nc;
	crci = SDIO_WLAN_SLOT_DMA_CRCI;

	if (IS_SDREQ_WRITE_DATA(pReq->Flags))
		pHcInstance->OsSpecific.dma.dir = DMA_TO_DEVICE;
	else
		pHcInstance->OsSpecific.dma.dir = DMA_FROM_DEVICE;

	n = dma_map_sg(
		pHcInstance->Hcd.pDevice,
		&pHcInstance->OsSpecific.dma.sg,
		pHcInstance->OsSpecific.dma.num_ents,
		pHcInstance->OsSpecific.dma.dir);

	if (n != pHcInstance->OsSpecific.dma.num_ents) {
		printk( KERN_ERR " SDIO Qualcomm HCD: mapping all sg elements failed\n");
		pHcInstance->OsSpecific.dma.num_ents = 0;
		return SDIO_STATUS_INVALID_PARAMETER;  
	}

	box = &nc->cmd[0];
	for (i = 0; i < pHcInstance->OsSpecific.dma.num_ents; i++) {
		box->cmd = CMD_MODE_BOX;

		if (i == (pHcInstance->OsSpecific.dma.num_ents - 1))
			box->cmd |= CMD_LC;

		rows = (sg_dma_len(sg) % MCI_FIFO_SIZE)?
				(sg_dma_len(sg) / MCI_FIFO_SIZE) + 1:
				(sg_dma_len(sg) / MCI_FIFO_SIZE);

		if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
			box->src_row_addr = sg_dma_address(sg);
			box->dst_row_addr = MSM_SDC_BASE_ADDR + MCI_FIFO_REG;

			box->src_dst_len = (MCI_FIFO_SIZE << 16) | (MCI_FIFO_SIZE);
			box->row_offset = (MCI_FIFO_SIZE << 16);

			box->num_rows = rows * ((1 << 16) + 1);
			box->cmd |= CMD_DST_CRCI(crci);
		} else {
			box->src_row_addr = MSM_SDC_BASE_ADDR + MCI_FIFO_REG;
			box->dst_row_addr = sg_dma_address(sg);

			box->src_dst_len = (MCI_FIFO_SIZE << 16) | (MCI_FIFO_SIZE);
			box->row_offset = MCI_FIFO_SIZE;

			box->num_rows = rows * ((1 << 16) + 1);
			box->cmd |= CMD_SRC_CRCI(crci);
		}
		box++;
		sg++;
	}

	/* location of command block must be 64 bit aligned */
	if (pHcInstance->OsSpecific.dma.cmd_busaddr & 0x07) {
		printk( KERN_ERR " SDIO Qualcomm HCD: cmd blk must be 64-bit aligned\n");
	}

	nc->cmdptr = (pHcInstance->OsSpecific.dma.cmd_busaddr >> 3) | CMD_PTR_LP;
	pHcInstance->OsSpecific.dma.hdr.cmdptr = DMOV_CMD_PTR_LIST |
	DMOV_CMD_ADDR(pHcInstance->OsSpecific.dma.cmdptr_busaddr);
	pHcInstance->OsSpecific.dma.hdr.complete_func = hcd_dma_complete_func;
#endif

	return SDIO_STATUS_PENDING;
}

/*
 * HcdRequest - SD request handler
 * Input:  pHcd - HCD object
 * Output: 
 * Return: 
 * Notes: 
 */
SDIO_STATUS HcdRequest(PSDHCD pHcd) 
{
    SDIO_STATUS     status = SDIO_STATUS_SUCCESS;
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)pHcd->pContext;
    UINT32	    temp;
    PSDREQUEST      pReq;
    UINT32	    dat_ctrl_tmp = 0;
    unsigned long   flags;
    unsigned int    dma_mode = 0;
    
    ///DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: HcdRequest\n"));
    
    spin_lock_irqsave(&sd_hcd_drv_lock, flags);

//    WARN_ON(pHcInstance->pReq != NULL);

    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
    pHcInstance->pReq = pReq;

    do {
        
#ifdef SLEEP_REG_SUPPORT
        sleep_negate_okts(sd_hcd_sleep_handle);
#endif

        if (pHcInstance->ShuttingDown) {
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: HcdRequest - returning canceled\n"));
            status = SDIO_STATUS_CANCELED;
            break;
        }

      	temp = MCI_CMD_ENABLED;

	/* PROG_DONE */
        if (!(GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP)) {
            if ( TRUE == isPreCMD53W ) {
                if ( (pReq->Command != 52) ) {
                    hcd_sendCMD52(pHcd);
                    isPreCMD53W = FALSE;
                } else {
                    WRITE_MMC_REG(pHcInstance, MCI_CLEAR_REG, MCI_STATUS_ProgDone);
                    temp |= MCI_CMD_PROG_ENABLED;
                }
            }
        }

        switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) {    
            case SDREQ_FLAGS_NO_RESP:
                break;
            case SDREQ_FLAGS_RESP_R1:
            case SDREQ_FLAGS_RESP_MMC_R4:        
            case SDREQ_FLAGS_RESP_MMC_R5:
            case SDREQ_FLAGS_RESP_R6:
                temp |= MCI_CMD_RESPONSE;
                break;
            case SDREQ_FLAGS_RESP_R1B:
                temp |= MCI_CMD_RESPONSE;
                break;
            case SDREQ_FLAGS_RESP_R2:
                temp |= MCI_CMD_RESPONSE;
                temp |= MCI_CMD_LONG_RESP;
                break;
            case SDREQ_FLAGS_RESP_SDIO_R5:
                temp |= MCI_CMD_RESPONSE;
                break;
            case SDREQ_FLAGS_RESP_R3:
            case SDREQ_FLAGS_RESP_SDIO_R4:
                temp |= MCI_CMD_RESPONSE;
                break;
        }   

        if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS){
            temp |= MCI_CMD_DAT_CMD; 
        }

        //printk("[%s] write ARG=0x%x\n", __FUNCTION__, pReq->Argument);
        WRITE_MMC_REG(pHcInstance, MCI_ARG_REG, (pReq->Argument));
        
        if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS){

            //printk("[%s] DATA_TRANS\n", __FUNCTION__);

#ifdef DMA_SUPPORT
            /* dma mode? */
            if (pHcInstance->OsSpecific.dma.nc_busaddr) {
                /* DMA mode */
                status = SetUpHCDDMA(pHcInstance, pReq);
                if (!SDIO_SUCCESS(status)) {
                    /* use PIO */
                    printk(" SDIO Qualcomm HCD: setting up DMA failed, use PIO\n");
                    pReq->Flags &=~ SDREQ_FLAGS_DATA_DMA;
                    /* use the context to hold where we are in the buffer */
                    pReq->pHcdContext = pReq->pDataBuffer;   
                } else {
                    /* use DMA indeed */
                    pReq->Flags |= SDREQ_FLAGS_DATA_DMA;
                    dma_mode = 1;
                }
            } else {
#endif
                /* PIO mode */
                pReq->Flags &= ~SDREQ_FLAGS_DATA_DMA;
                /* use the context to hold where we are in the buffer */
                pReq->pHcdContext = pReq->pDataBuffer;   
#ifdef DMA_SUPPORT
            }
#endif

            WRITE_MMC_REG(pHcInstance, MCI_CLEAR_REG, MCI_STATUS_STATIC_MASK);
            WRITE_MMC_REG(pHcInstance, MCI_DATA_TIMER_REG, 0xffffffff);
            WRITE_MMC_REG(pHcInstance, MCI_DATA_LENGTH_REG, pReq->BlockCount * pReq->BlockLen);

            /* blk size */
            dat_ctrl_tmp = (pReq->BlockLen << MCI_DATA_CTL_BLK_SIZE_SHIFT) |
                           MCI_DATA_CTL_ENABLE;

            /* direction */
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                dat_ctrl_tmp |= MCI_DATA_CTL_WR_DIR;
            } else {
                dat_ctrl_tmp |= MCI_DATA_CTL_RD_DIR;
            }

            /* dma? */
            if (dma_mode) {
                dat_ctrl_tmp |= MCI_DATA_CTL_DMA_EN;
            }

            WRITE_MMC_REG(pHcInstance,
                          MCI_DATA_CTL_REG,
                          dat_ctrl_tmp);
	}

        WRITE_MMC_REG(pHcInstance, MCI_CMD_REG, pReq->Command | temp);
	
        if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP) {
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: SDREQ_FLAGS_NO_RESP Cmd=[0x%d]\n", pReq->Command));
            status = sdcc_cmd_send_verify(pHcInstance);
            isPreCMD53W = FALSE;
            break;
	} else {
            /* PROG_DONE */
            if ( TRUE == isPreCMD53W ) {
                if ( FALSE == hcd_waitPrgDone(pHcd) ) {
                    temp = READ_MMC_REG(pHcInstance, MCI_STATUS_REG);
                    DBG_PRINT(SDDBG_TRACE, (
                              " SDIO Qualcomm HCD: wait prgDone [1] finished "
                              "cmd=%d arg=%x status=0x%x\n",
                              pReq->Command,
                              pReq->Argument,
                              temp));
                }
            }
        }

        isPreCMD53W = FALSE;
        status = sdcc_poll_cmd_response(pHcInstance, pReq->Command);

        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: fail to poll cmd resp: status=[0x%x]\n", status));
            pHcInstance->pReq = NULL;
            spin_unlock_irqrestore(&sd_hcd_drv_lock, flags);
            return status;
        }

        //printk(" SDIO Qualcomm HCD: Command Response success, cmd=[0x%x]\n", pReq->Command);

        /* process the command */
        status = ProcessCommandDone(pHcInstance, pReq, FALSE);

	if ((!dma_mode) &&
            (pReq->Flags & SDREQ_FLAGS_DATA_TRANS)) {

            //printk(" SDIO Qualcomm HCD: DATA_TRANS\n");
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                if (chk_manuf_id_ok) {
                    SDIPollingTransmit(pHcInstance, pReq->pDataBuffer, pReq->BlockCount * pReq->BlockLen);
                    isPreCMD53W = TRUE;
                } else {
                    unsigned int            i;
		    for (i = 0; i < (pReq->BlockCount * pReq->BlockLen - 1); i++)
                        *((char *)pReq->pDataBuffer + i) ^= *((char *)pReq->pDataBuffer + 1 + i);
                    SDIPollingTransmit(pHcInstance, pReq->pDataBuffer, pReq->BlockCount * pReq->BlockLen);
                    isPreCMD53W = TRUE;
		}
            } else {
                SDIPollingReceive(pHcInstance, pReq->pDataBuffer, pReq->BlockCount * pReq->BlockLen);
            }
            pHcInstance->pReq = NULL;
        }
    } while (FALSE);

    ///DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: HcdRequest\n"));
#ifdef SLEEP_REG_SUPPORT
    sleep_assert_okts(sd_hcd_sleep_handle);
#endif
    spin_unlock_irqrestore(&sd_hcd_drv_lock, flags);
    return status;
} 

void hcd_data_trans_en(PSDHCD pHcd)
{
//    printk("%s: SDCONFIG_ENABLE case\n", __FUNCTION__);

    if (pHcd == NULL)
        return;

    if (check_manuf_id(pHcd) == SDIO_STATUS_SUCCESS) {
//    printk("%s: enable\n", __FUNCTION__);
        chk_manuf_id_ok = TRUE;
    } else {
//    printk("%s: disable\n", __FUNCTION__);
        chk_manuf_id_ok = FALSE;
    }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  EnableDisableSDIOIRQ - enable SDIO interrupt detection
  Input:    pHcInstance - host controller
            Enable - enable SDIO IRQ detection
            FromIsr - called from ISR
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void EnableDisableSDIOIRQ(PSDHCD_INSTANCE pHcInstance, BOOL Enable, BOOL FromIsr)
{
    ULONG    flags = 0;
    UINT32   intsEnables;

    /* protected read-modify-write */
    if (FromIsr) {
        spin_lock(&pHcInstance->OsSpecific.RegAccessLock);
    } else {
        spin_lock_irqsave(&pHcInstance->OsSpecific.RegAccessLock, flags);
    }

//    intsEnables = READ_MMC_REG(pHcInstance, MCI_INT_MASK_REG);
    if (Enable) {
//        intsEnables |=  MMC_MASK_SDIOIntOper;
        intsEnables = MMC_MASK_SDIOIntOper;
    } else { 
//        intsEnables &= ~((UINT32)MMC_MASK_SDIOIntOper);
        intsEnables = 0;
    }

    WRITE_MMC_REG(pHcInstance, MCI_INT_MASK_REG, intsEnables);

    if (FromIsr) {
        spin_unlock(&pHcInstance->OsSpecific.RegAccessLock);
    } else {
        spin_unlock_irqrestore(&pHcInstance->OsSpecific.RegAccessLock, flags);
    }
}

