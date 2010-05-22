#include "dk_common.h"
#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"

#if defined(WIN32) || defined(LINUX)
#include <string.h>
#include "MLIBif.h"
#include "common_defs.h"
#endif

#ifdef PREDATOR
#include "flash_ops.h"
#endif
#include "ar5211/ar5211reg.h"
#ifdef THIN_CLIENT_BUILD
#include "hwext.h"
#else
#include "errno.h"
#include "common_defs.h"
#endif

#ifdef VXWORKS
#ifdef AR5312
#include "ar531xreg.h"
#endif
#endif

#define HDR_LEN   24
#define FCS_LEN   4

#define TX_CONTROL_1_OFFSET  0x8
#define TX_CONTROL_2_OFFSET  0xc
#define TX_CONTROL_3_OFFSET  0x10
#define TX_CONTROL_4_OFFSET  0x14
#define TX_STATUS_1_OFFSET  0x18
#define TX_STATUS_2_OFFSET  0x1c

#define RX_CONTROL_1_OFFSET  0x8
#define RX_CONTROL_2_OFFSET  0xc
#define RX_STATUS_1_OFFSET  0x10
#define RX_STATUS_2_OFFSET  0x14

A_UINT32 MEM_RD(A_UINT32 devNumIndex, A_UINT32 address);
void MEM_WR(A_UINT32 devNumIndex, A_UINT32 address, A_UINT8 *data, A_UINT32 size);
A_UINT32 REG_RD(A_UINT32 devNumIndex, A_UINT32 address);
void REG_WR(A_UINT32 devNumIndex, A_UINT32 address, A_UINT32 value);

#ifdef DEBUG_MEMORY
void mem_display(A_UINT32 devNum, A_UINT32 address, A_UINT32 nWords) {
  A_UINT32 iIndex, memValue;
  for(iIndex=0; iIndex<nWords; iIndex++) {
       memValue = MEM_RD(devNum, address+(iIndex*4));
	   printf("Word %d (addr %x)=%x\n", iIndex, (address+ (iIndex*4)), memValue);
  } 

}
#endif

A_UINT32 m_send_frame_and_recv(A_UINT32 devNumIndex, A_UINT8 *pBuffer, A_UINT32 tx_desc_ptr, A_UINT32 tx_buf_ptr, A_UINT32 rx_desc_ptr, A_UINT32 rx_buf_ptr, A_UINT32 rate_code) {

   A_UINT32 *frameWords, retry_cnt=4, value;
   A_UINT32 tx_control1 ;
   A_UINT32 tx_control2 ;
   A_UINT32 tx_control3 ;
   A_UINT32 tx_control4 ;
   A_UINT32 tx_sts1 ;
   A_UINT32 tx_sts2 ;
   A_UINT32 rx_sts2 ;
   A_UINT32 frameLen, bufLen, poll_timeout = 0x1;
   A_UINT32 tx_tsf=0, rx_tsf=0, tmpR, qcu=0, dcu=0;

   REG_WR(devNumIndex, F2_STA_ID0, 0xbccddee0);
   REG_WR(devNumIndex, F2_STA_ID1, 0x00220aab);
   REG_WR(devNumIndex, F2_BSS_ID0, 0xbccddee0);
   REG_WR(devNumIndex, F2_BSS_ID1, 0x00000aab);
   REG_WR(devNumIndex, F2_DEF_ANT, 0x1);

   //enable unicast reception - needed to receive acks
   REG_WR(devNumIndex, F2_RX_FILTER, F2_RX_UCAST);
	
   //enable receive 
   REG_WR(devNumIndex, F2_DIAG_SW, 0);

   // Create frame
   frameLen = *((A_UINT32 *)pBuffer);

   bufLen = frameLen - FCS_LEN;


   while (1) {
	   poll_timeout = 0x10000;
	   value = 0x8;
       MEM_WR(devNumIndex, tx_buf_ptr, (A_UINT8 *)&value, sizeof(value));
	   value = 0x23344550;
       MEM_WR(devNumIndex, tx_buf_ptr+0x4, (A_UINT8 *)&value, sizeof(value));
	   value = 0xdee00112;
       MEM_WR(devNumIndex, tx_buf_ptr+0x8, (A_UINT8 *)&value, sizeof(value));
	   value = 0x0aabbccd;
       MEM_WR(devNumIndex, tx_buf_ptr+0xc, (A_UINT8 *)&value, sizeof(value));
	   value = 0xbccddee0;
       MEM_WR(devNumIndex, tx_buf_ptr+0x10, (A_UINT8 *)&value, sizeof(value));
	   value = 0x00000aab;
       MEM_WR(devNumIndex, tx_buf_ptr+0x14, (A_UINT8 *)&value, sizeof(value));
       MEM_WR(devNumIndex, tx_buf_ptr+HDR_LEN, (A_UINT8 *)(pBuffer+4), bufLen);

       // Create tx descriptor 

       tx_control1 = frameLen ;
       tx_control2 = bufLen ;
       tx_control3 = retry_cnt << 16 ;
       tx_control4 = rate_code;
	   value = 0;
       MEM_WR(devNumIndex, tx_desc_ptr, (A_UINT8 *)&value, sizeof(value));
	   value = tx_buf_ptr;
       MEM_WR(devNumIndex, tx_desc_ptr+0x4, (A_UINT8 *)&value, sizeof(value));   
	   value = tx_control1;
       MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_1_OFFSET, (A_UINT8 *)&value, sizeof(value));   
	   value = tx_control2;
       MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_2_OFFSET, (A_UINT8 *)&value, sizeof(value));
	   value = tx_control3;
       MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_3_OFFSET, (A_UINT8 *)&value, sizeof(value));   
	   value = tx_control4;
       MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_4_OFFSET, (A_UINT8 *)&value, sizeof(value));   
	   value = 0x0;
       MEM_WR(devNumIndex, tx_desc_ptr+TX_STATUS_1_OFFSET, (A_UINT8 *)&value, sizeof(value));
       MEM_WR(devNumIndex, tx_desc_ptr+TX_STATUS_2_OFFSET, (A_UINT8 *)&value, sizeof(value));

        // Create rx descriptor 
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr, (A_UINT8 *)&value, sizeof(value));
	   value = rx_buf_ptr;
       MEM_WR(devNumIndex, rx_desc_ptr+0x4,  (A_UINT8 *)&value, sizeof(value));   
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_CONTROL_1_OFFSET,  (A_UINT8 *)&value, sizeof(value));   
	   value = MAX_LB_FRAME_LEN;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_CONTROL_2_OFFSET, (A_UINT8 *)&value, sizeof(value));   
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_STATUS_1_OFFSET, (A_UINT8 *)&value, sizeof(value));   
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_STATUS_2_OFFSET, (A_UINT8 *)&value, sizeof(value));   

//       REG_WR(devNumIndex, F2_CR, F2_CR_RXD);
       REG_WR(devNumIndex, F2_RXDP, rx_desc_ptr);
       REG_WR(devNumIndex, F2_CR, F2_CR_RXE);


       REG_WR(devNumIndex, F2_D0_QCUMASK, 1);
       REG_WR(devNumIndex, F2_D_GBL_IFS_EIFS, REG_RD(devNumIndex, F2_D_GBL_IFS_EIFS));
       REG_WR(devNumIndex, F2_QDCKLGATE, REG_RD(devNumIndex, F2_QDCKLGATE) & ~((0x1  << qcu) | 0x10000 << dcu));
       REG_WR(devNumIndex, F2_Q0_TXDP, tx_desc_ptr );
       REG_WR(devNumIndex, F2_Q_TXE, 0x1);


       // Wait for xmit complete

       do {

		  tx_tsf = REG_RD(devNumIndex, F2_TSF_L32);
#ifdef PREDATOR
		  A_DATA_CACHE_INVAL(tx_desc_ptr + TX_STATUS_1_OFFSET, 8);
#endif
          tx_sts2 = MEM_RD(devNumIndex, tx_desc_ptr + TX_STATUS_2_OFFSET);
		  
       }
       while(!(tx_sts2 & 0x1) && (poll_timeout--));
	   poll_timeout = 0x10000;
       tx_sts1 = MEM_RD(devNumIndex, tx_desc_ptr + TX_STATUS_1_OFFSET);

#ifdef DEBUG_MEMORY
       printf("tx_sts1 = %x:tx_sts2 = %x\n", tx_sts1, tx_sts2);
       printf("TXDP %x:TXE=%x\n", REG_RD(devNumIndex, F2_Q0_TXDP), REG_RD(devNumIndex, F2_Q_TXE));
       printf("Memory contents at tx desc ptr %x\n", tx_desc_ptr);
       mem_display(devNumIndex, tx_desc_ptr, 8);
       printf("Memory contents at mac pointed txdp is \n");
       mem_display(devNumIndex, REG_RD(devNumIndex, F2_Q0_TXDP), 8);
#endif
       if (tx_sts1 & 0x1) {
            // Wait for rx complete
           do {
			  rx_tsf = REG_RD(devNumIndex, F2_TSF_L32);
#ifdef PREDATOR
		      A_DATA_CACHE_INVAL(rx_desc_ptr + RX_STATUS_1_OFFSET, 8);
#endif
              rx_sts2 = MEM_RD(devNumIndex, rx_desc_ptr + RX_STATUS_2_OFFSET);
           }
           while(!(rx_sts2 & 0x1) && (poll_timeout--));
		   if (rx_sts2 & 0x3) {
		   }
		   else {
			  rx_tsf = 0xffffffff;
			  tx_tsf = 0x0;
		   }
       }
       else {
		  rx_tsf = 0xffffffff;
		  tx_tsf = 0x0;
	   }
	   break;  
   } // while  

   return ( rx_tsf-tx_tsf);

}


A_UINT32 m_recv_frame_and_xmit(A_UINT32 devNumIndex, A_UINT32 tx_desc_ptr, A_UINT32 tx_buf_ptr, A_UINT32 rx_desc_ptr, A_UINT32 rx_buf_ptr, A_UINT32 rate_code) {

   A_UINT32 retry_cnt=4, value;
   A_UINT32 tx_control1 ;
   A_UINT32 tx_control2 ;
   A_UINT32 tx_control3 ;
   A_UINT32 tx_control4 ;
   A_UINT32 tx_sts1, tx_sts2 ;
   A_UINT32 rx_sts1 ;
   A_UINT32 rx_sts2 ;
   A_UINT32 frameLen, bufLen, poll_timeout = 0x10000, qcu=0, dcu=0, nCopyBytes, iIndex, jIndex;
   A_UINT32 pBuffer[MAX_LB_FRAME_LEN], retVal;

   REG_WR(devNumIndex, F2_STA_ID0, 0x23344550);
   REG_WR(devNumIndex, F2_STA_ID1, 0x00220112);
   REG_WR(devNumIndex, F2_BSS_ID0, 0xbccddee0);
   REG_WR(devNumIndex, F2_BSS_ID1, 0x00000aab);
   REG_WR(devNumIndex, F2_DEF_ANT, 0x1);

   //enable unicast reception - needed to receive acks
   REG_WR(devNumIndex, F2_RX_FILTER, F2_RX_UCAST);
	
   //enable receive 
   REG_WR(devNumIndex, F2_DIAG_SW, 0);

	   poll_timeout = 0x10000;
        // Create rx descriptor 
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr, (A_UINT8 *)&value, sizeof(value));
	   value = rx_buf_ptr;
       MEM_WR(devNumIndex, rx_desc_ptr+0x4,  (A_UINT8 *)&value, sizeof(value));   
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_CONTROL_1_OFFSET,  (A_UINT8 *)&value, sizeof(value));   
	   value = MAX_LB_FRAME_LEN;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_CONTROL_2_OFFSET, (A_UINT8 *)&value, sizeof(value));   
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_STATUS_1_OFFSET, (A_UINT8 *)&value, sizeof(value));   
	   value = 0x0;
       MEM_WR(devNumIndex, rx_desc_ptr+RX_STATUS_2_OFFSET, (A_UINT8 *)&value, sizeof(value));   

//       REG_WR(devNumIndex, F2_CR, F2_CR_RXD);
	   uiPrintf("Waiting for RXE clear\n");
	   while (   (REG_RD(devNumIndex, F2_CR) & F2_CR_RXE) != 0) {
		   uiPrintf("RXDP = %x:rx_desc_ptr=%x:CR=%x\n", REG_RD(devNumIndex, F2_RXDP), rx_desc_ptr, REG_RD(devNumIndex, F2_CR));
	   }
	   uiPrintf("RXE clear\n");
       REG_WR(devNumIndex, F2_RXDP, rx_desc_ptr);
       REG_WR(devNumIndex, F2_CR, F2_CR_RXE);

       // Wait for recv complete

	   retVal = 0xffffffff;
       do {
#ifdef PREDATOR
		  A_DATA_CACHE_INVAL(rx_desc_ptr + RX_STATUS_1_OFFSET, 8);
#endif
          rx_sts2 = MEM_RD(devNumIndex, rx_desc_ptr + RX_STATUS_2_OFFSET);
       }
       while(!(rx_sts2 & 0x1) && (poll_timeout--));
	   uiPrintf("polltimeout=%d:rx_sts2=%x\n", poll_timeout, rx_sts2);
 	   if (rx_sts2 & 0x3) { 

	       retVal = 0x1;
		   uiPrintf("Rx frame\n");
           rx_sts1 = MEM_RD(devNumIndex, rx_desc_ptr + RX_STATUS_1_OFFSET);
   		   frameLen = (rx_sts1 & 0xfff);
		   bufLen = frameLen - FCS_LEN;

		   value = 0x8;
		   MEM_WR(devNumIndex, tx_buf_ptr, (A_UINT8 *)&value, sizeof(value));
		   value = 0xbccddee0;
		   MEM_WR(devNumIndex, tx_buf_ptr+0x4, (A_UINT8 *)&value, sizeof(value));
		   value = 0x45500aab;
		   MEM_WR(devNumIndex, tx_buf_ptr+0x8, (A_UINT8 *)&value, sizeof(value));
		   value = 0x01122334;
		   MEM_WR(devNumIndex, tx_buf_ptr+0xc, (A_UINT8 *)&value, sizeof(value));
		   value = 0xbccddee0;
		   MEM_WR(devNumIndex, tx_buf_ptr+0x10, (A_UINT8 *)&value, sizeof(value));
		   value = 0x00000aab;
		   MEM_WR(devNumIndex, tx_buf_ptr+0x14, (A_UINT8 *)&value, sizeof(value));

		   nCopyBytes = bufLen - HDR_LEN;
		   for(iIndex=0, jIndex=0;iIndex<nCopyBytes;iIndex+=4, jIndex++) {
			   pBuffer[jIndex] = MEM_RD(devNumIndex, (rx_buf_ptr + HDR_LEN + iIndex));
		   }
		   MEM_WR(devNumIndex, tx_buf_ptr+HDR_LEN, (A_UINT8 *)(pBuffer), nCopyBytes);

		   // Create tx descriptor 
		   tx_control1 = frameLen ;
		   tx_control2 = bufLen ;
		   tx_control3 = retry_cnt << 16 ;
		   tx_control4 = rate_code;
		   value = 0x0;
		   MEM_WR(devNumIndex, tx_desc_ptr, (A_UINT8 *)&value, sizeof(value));
		   value = tx_buf_ptr;
		   MEM_WR(devNumIndex, tx_desc_ptr+0x4, (A_UINT8 *)&value, sizeof(value));   
		   value = tx_control1;
		   MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_1_OFFSET, (A_UINT8 *)&value, sizeof(value));   
		   value = tx_control2;
		   MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_2_OFFSET, (A_UINT8 *)&value, sizeof(value));
		   value = tx_control3;
		   MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_3_OFFSET, (A_UINT8 *)&value, sizeof(value));   
		   value = tx_control4;
		   MEM_WR(devNumIndex, tx_desc_ptr+TX_CONTROL_4_OFFSET, (A_UINT8 *)&value, sizeof(value));   
		   value = 0x0;
		   MEM_WR(devNumIndex, tx_desc_ptr+TX_STATUS_1_OFFSET, (A_UINT8 *)&value, sizeof(value));
		   MEM_WR(devNumIndex, tx_desc_ptr+TX_STATUS_2_OFFSET, (A_UINT8 *)&value, sizeof(value));

           REG_WR(devNumIndex, F2_D0_QCUMASK, 1);
           REG_WR(devNumIndex, F2_D_GBL_IFS_EIFS, REG_RD(devNumIndex, F2_D_GBL_IFS_EIFS));
           REG_WR(devNumIndex, F2_QDCKLGATE, REG_RD(devNumIndex, F2_QDCKLGATE) & ~((0x1  << qcu) | 0x10000 << dcu));

		   uiPrintf("Waiting for TXE clear\n");
		   while (   (REG_RD(devNumIndex, F2_Q_TXE) & 0x1) != 0);
		   uiPrintf("TXE clear\n");

           REG_WR(devNumIndex, F2_Q0_TXDP, tx_desc_ptr );
           REG_WR(devNumIndex, F2_Q_TXE, 0x1);

           do {
#ifdef PREDATOR
		  A_DATA_CACHE_INVAL(tx_desc_ptr + TX_STATUS_1_OFFSET, 8);
#endif
              tx_sts2 = MEM_RD(devNumIndex, tx_desc_ptr + TX_STATUS_2_OFFSET);
           }
           while(!(tx_sts2 & 0x1) && (poll_timeout--));
		   tx_sts1 = MEM_RD(devNumIndex, tx_desc_ptr + TX_STATUS_1_OFFSET);
		   if (tx_sts1 & 0x1) 	
				retVal = 0x0;
		   else
				retVal = 0x2;
		   uiPrintf("Tx frame sts = %x:%x\n", tx_sts1, tx_sts2);
	   }
     return (retVal);
}

A_UINT32 MEM_RD(A_UINT32 devNumIndex, A_UINT32 address) {

  A_UINT32 retValue;

#ifdef THIN_CLIENT_BUILD
        retValue = hwMemRead32(devNumIndex, address);
#else
		OSmemRead(devNumIndex, address, 
						(A_UCHAR *)&(retValue), sizeof(retValue));
#endif

   return retValue;

}



void MEM_WR(A_UINT32 devNumIndex, A_UINT32 address, A_UINT8 *data, A_UINT32 size) {

#ifdef THIN_CLIENT_BUILD
    hwMemWriteBlock(devNumIndex, data, size, &address);   
#else
	OSmemWrite(devNumIndex, address, (A_UCHAR *)data, size);
#endif

}

A_UINT32 REG_RD(A_UINT32 devNumIndex, A_UINT32 address) {

  A_UINT32 retValue;

#ifdef THIN_CLIENT_BUILD
#ifdef PREDATOR
        retValue = sysRegRead(AR5523_WMAC0_BASE_ADDRESS + address);
#endif
#ifdef FREEDOM_AP
        retValue = sysRegRead(AR531X_WLAN0+ address);
#endif

#else
		retValue = REGR(devNumIndex, address);
#endif

   return retValue;

}

void REG_WR(A_UINT32 devNumIndex, A_UINT32 address, A_UINT32 value) {

#ifdef THIN_CLIENT_BUILD
#ifdef PREDATOR
    sysRegWrite(AR5523_WMAC0_BASE_ADDRESS + address, value);
#endif
#ifdef FREEDOM_AP
    sysRegWrite(AR531X_WLAN0+ address, value);
#endif
#else
	REGW(devNumIndex, address, value);
#endif

}



