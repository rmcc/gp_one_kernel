/* mData.c - contains frame transmit functions */
/* Copyright (c) 2001 Atheros Communications, Inc., All Rights Reserved */

#ident	"ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/mData.c#2 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/mData.c#2 $"

/* 
Revsision history
--------------------
1.0 	  Created.
*/
 
//#define DEBUG_MEMORY  

#ifdef VXWORKS
#include "timers.h"
#include "vxdrv.h"
#endif

#ifdef __ATH_DJGPPDOS__
#include <unistd.h>
#ifndef EILSEQ  
    #define EILSEQ EIO
#endif	// EILSEQ

#define __int64	long long
typedef unsigned long DWORD;
#define HANDLE long
#define Sleep	delay
#endif	// #ifdef __ATH_DJGPPDOS__

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "mdata.h"
#include "stats_routines.h"
#include "mEeprom.h"
#include "mConfig.h"
#ifndef VXWORKS
#include <malloc.h>
#if defined(LINUX) || defined(__linux__)
#include "linuxdrv.h"
#else
#include "ntdrv.h"
#endif
#endif // VXWorks
#include "mData210.h"
#include "mData211.h"
#include "mData212.h"
#include "mData513.h"
#include "mDevtbl.h"
#include "mIds.h"

//#if defined(LINUX) || defined(__linux__)
//#undef ARCH_BIG_ENDIAN
//#endif

#ifdef ARCH_BIG_ENDIAN 
#include "endian_func.h"
#endif

#if defined(COBRA_AP) && defined(PCI_INTERFACE)
#include "ar531xPlusreg.h"
#endif

static void mdkExtractAddrAndSequence(A_UINT32 devNum, RX_STATS_TEMP_INFO *pStatsInfo);
static void mdkCountDuplicatePackets(A_UINT32	devNum, 
				RX_STATS_TEMP_INFO	*pStatsInfo, A_BOOL *pIsDuplicate);
static void mdkGetSignalStrengthStats(SIG_STRENGTH_STATS *pStats, A_INT8	signalStrength);
static void txAccumulateStats(A_UINT32 devNum, A_UINT32 txTime, A_UINT16 queueIndex);
static void mdkExtractTxStats(A_UINT32	devNum, TX_STATS_TEMP_INFO	*pStatsInfo, A_UINT16 queueIndex);
static void mdkExtractRxStats(A_UINT32 devNum, RX_STATS_TEMP_INFO	*pStatsInfo);
static void sendStatsPkt(A_UINT32 devNum, A_UINT32 rate, A_UINT16 StatsType, A_UCHAR *dest);
static A_BOOL mdkExtractRemoteStats(A_UINT32 devNum, RX_STATS_TEMP_INFO *pStatsInfo);
static void comparePktData(A_UINT32 devNum, RX_STATS_TEMP_INFO	*pStatsInfo);
static void extractPPM(A_UINT32 devNum, RX_STATS_TEMP_INFO *pStatsInfo);
static A_UINT32 countBits(A_UINT32 mismatchBits);
static void fillCompareBuffer(A_UCHAR *pBuffer, A_UINT32 compareBufferSize, 
				A_UCHAR *pDataPattern, A_UINT32 dataPatternLength);
static void fillRxDescAndFrame(A_UINT32 devNum, RX_STATS_TEMP_INFO *statsInfo);

#if defined(__ATH_DJGPPDOS__)
static A_UINT32 milliTime(void);
#endif

////////////////////// __TODO__   ////////////////////////////////////////////////////////////////////////////////
MANLIB_API void txDataStart(A_UINT32 devNum);
MANLIB_API void txDataComplete(A_UINT32 devNum, A_UINT32 timeout, A_UINT32 remoteStats);

A_UINT32 buf_ptr;
A_UCHAR *tmp_pktDataPtr;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static char bitCount[256] = {
//	0  1  2  3	4  5  6  7	8  9  a  b	c  d  e  f
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 	// 0X
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 	// 1X
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 	// 2X
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 	// 3X
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 	// 4X
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 	// 5X
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 	// 6X
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 	// 7X
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 	// 8X
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 	// 9X
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 	// aX
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 	// bX
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 	// cX
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 	// dX
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 	// eX
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8		// fX
};

// A quick lookup translating from rate 6, 9, 12... to the stats_struct array index
//static const A_UCHAR StatsRateArray[19] = 
//	{0, 0, 1, 2, 3, 0, 4, 0, 5, 0,	0,	0,	6,	0,	0,	0,	7,	0,	8};
//#define rate2bin(x) (StatsRateArray[((x)/3)])

// A quick lookup translating from IEEE rate field to the stats bin
//static const A_UCHAR IEEErateArray[8] = {7, 5, 3, 1, 8, 6, 4, 2};
//#define descRate2bin(x) (IEEErateArray[(x)-8])

/**************************************************************************
* txDataSetupNoEndPacket - create packet and descriptors for transmission
*                          with no end packet. added to be used for falcon
*                          11g synthesizer phase offset.
*/
MANLIB_API void txDataSetupNoEndPacket
(
 A_UINT32 devNum, 
 A_UINT32 rateMask, 
 A_UCHAR *dest, 
 A_UINT32 numDescPerRate, 
 A_UINT32 dataBodyLength, 
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength, 
 A_UINT32 retries, 
 A_UINT32 antenna, 
 A_UINT32 broadcast
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	pLibDev->noEndPacket = TRUE;

	txDataSetup(devNum, rateMask, dest, numDescPerRate, dataBodyLength,
				dataPattern, dataPatternLength, retries, antenna, broadcast);
}

/**************************************************************************
* txDataSetup - create packet and descriptors for transmission
*
*/
MANLIB_API void txDataSetup
(
 A_UINT32 devNum, 
 A_UINT32 rateMask, 
 A_UCHAR *dest, 
 A_UINT32 numDescPerRate, 
 A_UINT32 dataBodyLength, 
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength, 
 A_UINT32 retries, 
 A_UINT32 antenna, 
 A_UINT32 broadcast
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UCHAR 	rates[NUM_RATES];
	A_UINT32	mask = 0x01;
	A_UINT16	i, j, numRates, rateIndex, prevRateIndex, rateValue;
	A_UINT32	descAddress, dIndex, descOp;
	A_UINT32	antMode = 0;
	A_UINT32	pktSize;
	A_UINT32	pktAddress;
	MDK_ATHEROS_DESC	*localDescPtr;		   //pointer to current descriptor being filled
	MDK_ATHEROS_DESC    *localDescBuffer;      //create a local buffer to create descriptors
	A_UINT16	 queueIndex;
	A_BOOL		probePkt = FALSE;
	A_UINT16    mdkPktType = MDK_NORMAL_PKT;
	A_UINT32 *dPtr, lastDesc = 0, intrBit ;
	A_UINT32    falconAddrMod = 0;
#ifdef _TIME_PROFILE
    A_UINT32 txDataSetup_start, txDataSetup_end;
    txDataSetup_start=milliTime();
printf("txDataSetup_start=%u\n", txDataSetup_start);
#endif

	//overloading the broadcast param with additional flags
	if(broadcast & PROBE_PKT) {
		probePkt = TRUE;
		mdkPktType = MDK_PROBE_PKT;

		//setup the probe packets on a different queue
		pLibDev->txProbePacketNext = TRUE;
		queueIndex = PROBE_QUEUE;
		if(pLibDev->backupSelectQueueIndex != PROBE_QUEUE) { //safety check so we don't backup probe_queue index
			pLibDev->backupSelectQueueIndex = pLibDev->selQueueIndex;
		}
		pLibDev->selQueueIndex = PROBE_QUEUE;
	}
	else {
		pLibDev->selQueueIndex = 0;
		queueIndex = pLibDev->selQueueIndex;
	}

	//cleanup broadcast flag after cleaning up overloaded flags
	broadcast = broadcast & 0x1;

#ifdef DEBUG_MEMORY
printf("SNOOP::txDataSetup::broadcast=%x\n", broadcast);
printf("SNOOP::dest %x:%x:%x:%x:%x:%x\n", dest[6], dest[5], dest[4], dest[3], dest[2], dest[1], dest[0]);
#endif

	pLibDev->tx[queueIndex].dcuIndex = 0;


	pLibDev->tx[queueIndex].retryValue = retries;		
	//verify some of the arguments
	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:txDataSetup\n", devNum);
		return;
	}
	
	if(pLibDev->devState < RESET_STATE) {
		mError(devNum, EILSEQ, "Device Number %d:txDataSetup: device not in reset state - resetDevice must be run first\n", devNum);
		return;
	}

	if (0 == numDescPerRate) {
		mError(devNum, EINVAL, "Device Number %d:txDataSetup: must create at least 1 descriptor per rate\n", devNum);
		return;
	}

	/* we must take the MDK pkt header into consideration here otherwise it could make the 
	 * body size greater than an 802.11 pkt body
	 */
	if(dataBodyLength > (MAX_PKT_BODY_SIZE - sizeof(MDK_PACKET_HEADER) - sizeof(WLAN_DATA_MAC_HEADER3) - FCS_FIELD)) {
		mError(devNum, EINVAL, "Device Number %d:txDataSetup: packet body size must be less than %d [%d - (%d + %d + %d)]\n", devNum, 
								(MAX_PKT_BODY_SIZE - sizeof(MDK_PACKET_HEADER) - sizeof(WLAN_DATA_MAC_HEADER3) - FCS_FIELD), 
								MAX_PKT_BODY_SIZE,
								sizeof(MDK_PACKET_HEADER),
								sizeof(WLAN_DATA_MAC_HEADER3),
								FCS_FIELD);
		return;
	}

	// reset the txEnable. Only one queue is supported at a time for now 
	//FJC removed this now that am using another queue for probe packets
	//txEnable gets reset in cleanup below
//	for( i = 0; i < MAX_TX_QUEUE; i++ )
//	{
//	 	pLibDev->tx[i].txEnable=0;
//	}

	//cleanup any stuff from previous go round
	if (pLibDev->tx[queueIndex].pktAddress) {
		memFree(devNum, pLibDev->tx[queueIndex].pktAddress);
		pLibDev->tx[queueIndex].pktAddress = 0;
		memFree(devNum, pLibDev->tx[queueIndex].descAddress);
		pLibDev->tx[queueIndex].descAddress = 0;
		pLibDev->tx[queueIndex].txEnable = 0;
	}
	if(pLibDev->tx[queueIndex].endPktAddr) {
		memFree(devNum, pLibDev->tx[queueIndex].endPktAddr);
		memFree(devNum, pLibDev->tx[queueIndex].endPktDesc);
		pLibDev->tx[queueIndex].endPktAddr = 0;
		pLibDev->tx[queueIndex].endPktDesc = 0;
	}

	//create rate array and findout how many rates there are
	for (i = 0, numRates = 0; i < NUM_RATES; i++)
	{
		if (rateMask & mask) {
			rates[numRates] = rateValues[i];
			numRates++;
		}
		mask = (mask << 1) & 0xffffff;			
	}

#ifdef DEBUG_MEMORY
	printf("SNOOP::txDataSetup::mask=%x:rateMask=%x\n", mask, rateMask);
	printf("SNOOP::txDataSetup::numRates=%d:numDescPerRate=%d\n", numRates, numDescPerRate);
#endif


	//create the required number of descriptors
	pLibDev->tx[queueIndex].numDesc = numDescPerRate * numRates;
	pLibDev->tx[queueIndex].descAddress = memAlloc( devNum, 
					pLibDev->tx[queueIndex].numDesc * sizeof(MDK_ATHEROS_DESC));

	if (0 == pLibDev->tx[queueIndex].descAddress) {
		mError(devNum, ENOMEM, "Device Number %d:txDataSetup: unable to allocate client memory for %d descriptors\n", devNum, pLibDev->tx[queueIndex].numDesc);
		return;
	}

	//setup the transmit packets
	if (pLibDev->noEndPacket) {
		createTransmitPacket(devNum, MDK_SKIP_STATS_PKT, dest, pLibDev->tx[queueIndex].numDesc, 
			dataBodyLength, dataPattern, dataPatternLength, broadcast, queueIndex,
			&pktSize, &(pLibDev->tx[queueIndex].pktAddress));
	} else {
        	createTransmitPacket(devNum, mdkPktType, dest, pLibDev->tx[queueIndex].numDesc,  \
	        	dataBodyLength, dataPattern, dataPatternLength, broadcast, queueIndex, \
		        &pktSize, &(pLibDev->tx[queueIndex].pktAddress));
	}
	//take a copy of the dest address
	memcpy(pLibDev->tx[queueIndex].destAddr.octets, dest, WLAN_MAC_ADDR_SIZE);
	pLibDev->tx[queueIndex].dataBodyLen = dataBodyLength;

	//setupAntennaAr5210( devNum, antenna, &antMode );
	if (!ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setupAntenna(devNum, antenna, &antMode))
	{
		return;
	}

	rateIndex = 0;
	prevRateIndex = rateIndex;
	descAddress = pLibDev->tx[queueIndex].descAddress;
	pktAddress = pLibDev->tx[queueIndex].pktAddress;

	if (isFalcon(devNum) || isDragon(devNum)) {
		falconAddrMod = FALCON_MEM_ADDR_MASK ;
	}

	localDescBuffer = (MDK_ATHEROS_DESC *)malloc(sizeof(MDK_ATHEROS_DESC) * pLibDev->tx[queueIndex].numDesc);
	if (!localDescBuffer) {
		mError(devNum, ENOMEM, "Device Number %d:txDataSetup: unable to allocate host memory for %d tx descriptors\n", devNum, pLibDev->tx[queueIndex].numDesc);
		return;
	}
	dIndex = 0;
	localDescPtr = localDescBuffer;
	for (i = 0, j = 0; i < pLibDev->tx[queueIndex].numDesc; i++)
	{
		rateValue = rates[rateIndex];

		//write buffer ptr to descriptor
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
	    if(isCobra(pLibDev->swDevID)) {
			localDescPtr->bufferPhysPtr = pktAddress;
		}
		else {
			localDescPtr->bufferPhysPtr = pktAddress | HOST_PCI_SDRAM_BASEADDR;
		}
#else
		localDescPtr->bufferPhysPtr = pktAddress;
#endif
		
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setDescriptor( devNum, localDescPtr, pktSize, 
							antMode, i, rateValue, broadcast );


		//write link pointer
		if (i == (pLibDev->tx[queueIndex].numDesc - 1)) { //ie its the last descriptor 
			localDescPtr->nextPhysPtr = 0;
		//	localDescPtr->nextPhysPtr = pLibDev->tx[queueIndex].descAddress;
		}
		else {
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
		    if(isCobra(pLibDev->swDevID)) {
			    localDescPtr->nextPhysPtr = (descAddress + sizeof(MDK_ATHEROS_DESC));
			}
		    else {
			    localDescPtr->nextPhysPtr = (descAddress + sizeof(MDK_ATHEROS_DESC)) | HOST_PCI_SDRAM_BASEADDR;
			}
#else
			localDescPtr->nextPhysPtr = falconAddrMod | (descAddress + sizeof(MDK_ATHEROS_DESC));
#endif
		}

		//writeDescriptor(devNum, descAddress, &localDesc);
//		zeroDescriptorStatus(devNum, localDescPtr, pLibDev->swDevID);
		//increment rate index for next time round
		if(!(rateMask & RATE_GROUP)) {
			(rateIndex == numRates - 1) ? rateIndex = 0 : rateIndex++;
		}
		else {
			if (j == (numDescPerRate - 1)) {
				j = 0; 
				rateIndex++;
    			if (pLibDev->devMap.remoteLib) {
					dPtr = (A_UINT32 *)localDescPtr;

					lastDesc = LAST_DESC_NEXT << DESC_INFO_LAST_DESC_BIT_START;
					localDescPtr->hwControl[0] &= ~DESC_TX_INTER_REQ;
					intrBit = 0;
					descOp = ((sizeof(MDK_ATHEROS_DESC)/sizeof(A_UINT32))+1) << DESC_OP_WORD_OFFSET_BIT_START;

					if (i == (pLibDev->tx[queueIndex].numDesc-1)) {
					   	lastDesc = LAST_DESC_NULL << DESC_INFO_LAST_DESC_BIT_START;
						intrBit = DESC_TX_INTER_REQ_START <<  DESC_OP_INTR_BIT_START;
						descOp = intrBit | \
								 ((numDescPerRate-1)  << DESC_OP_NDESC_OFFSET_BIT_START) | \
								 (2 << DESC_OP_WORD_OFFSET_BIT_START);
					}

					pLibDev->devMap.r_createDescriptors(devNum, (pLibDev->tx[queueIndex].descAddress + \
							(dIndex * sizeof(MDK_ATHEROS_DESC))),  \
							lastDesc | numDescPerRate |  \
							(sizeof(MDK_ATHEROS_DESC)/sizeof(A_UINT32) << DESC_INFO_NUM_DESC_WORDS_BIT_START), \
							0, \
							descOp, \
							(A_UINT32 *)localDescPtr);

				    dIndex = i+1;
				}
			}
			else {
				j++;
			}
		}


		//increment descriptor address
		descAddress += sizeof(MDK_ATHEROS_DESC);
		localDescPtr ++;
	}

	//write all the descriptors in one shot
    if (!pLibDev->devMap.remoteLib || !(rateMask & RATE_GROUP)) {
		writeDescriptors(devNum, pLibDev->tx[queueIndex].descAddress, localDescBuffer, pLibDev->tx[queueIndex].numDesc);
#ifdef DEBUG_MEMORY
printf("SNOOP::Desc contents are\n");
memDisplay(devNum, pLibDev->tx[queueIndex].descAddress, 16);
#endif
	}
	free(localDescBuffer);


#ifdef DEBUG_MEMORY
printf("SNOOP::createEndPacket to \n");
printf("SNOOP::dest %x:%x:%x:%x:%x:%x\n", dest[6], dest[5], dest[4], dest[3], dest[2], dest[1], dest[0]);
#endif

	//create the end packet
	createEndPacket(devNum, queueIndex, dest, antMode, probePkt);

	// Set broadcast for Begin
	if(broadcast) {
		pLibDev->tx[queueIndex].broadcast = 1;
	}
	else {
		pLibDev->tx[queueIndex].broadcast = 0;
	}

	pLibDev->tx[queueIndex].txEnable = 1;
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setRetryLimit( devNum, queueIndex );
#ifdef _TIME_PROFILE
    txDataSetup_end=milliTime();
printf(".................SNOOP::exit txDataSetup:Time taken = %dms\n", (txDataSetup_end-txDataSetup_start));
#endif
	return;
}

MANLIB_API void cleanupTxRxMemory
(
 A_UINT32 devNum,
 A_UINT32 flags
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT16	 queueIndex;



	pLibDev->selQueueIndex = 0;
	pLibDev->tx[0].dcuIndex = 0;

	queueIndex = pLibDev->selQueueIndex;

	if(flags & TX_CLEAN) {
		if (pLibDev->tx[queueIndex].pktAddress) {
			memFree(devNum, pLibDev->tx[queueIndex].pktAddress);
			pLibDev->tx[queueIndex].pktAddress = 0;
			memFree(devNum, pLibDev->tx[queueIndex].descAddress);
			pLibDev->tx[queueIndex].descAddress = 0;
			pLibDev->tx[queueIndex].txEnable = 0;
		}
		if(pLibDev->tx[queueIndex].endPktAddr) {
			memFree(devNum, pLibDev->tx[queueIndex].endPktAddr);
			memFree(devNum, pLibDev->tx[queueIndex].endPktDesc);
			pLibDev->tx[queueIndex].endPktAddr = 0;
			pLibDev->tx[queueIndex].endPktDesc = 0;
		}
		if(pLibDev->tx[PROBE_QUEUE].pktAddress) {
			memFree(devNum, pLibDev->tx[PROBE_QUEUE].pktAddress);
			pLibDev->tx[PROBE_QUEUE].pktAddress = 0;
			memFree(devNum, pLibDev->tx[PROBE_QUEUE].descAddress);
			pLibDev->tx[PROBE_QUEUE].descAddress = 0;
			pLibDev->tx[PROBE_QUEUE].txEnable = 0;
		}
	}
	if(flags & RX_CLEAN) {
		if (pLibDev->rx.rxEnable || pLibDev->rx.bufferAddress) {
			memFree(devNum, pLibDev->rx.bufferAddress);
			pLibDev->rx.bufferAddress = 0;
			memFree(devNum, pLibDev->rx.descAddress);
			pLibDev->rx.descAddress = 0;
			pLibDev->rx.rxEnable = 0;
		}
	}

}

void
createEndPacket
(
 A_UINT32 devNum,
 A_UINT16 queueIndex,
 A_UCHAR  *dest,
 A_UINT32 antMode,
 A_BOOL   probePkt
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 pktSize;
	MDK_ATHEROS_DESC	localDesc;
	A_UINT32			retryValue;
	A_UINT16			endPktType = MDK_LAST_PKT;

	if (pLibDev->noEndPacket) {
		return;
	}

	pLibDev->tx[queueIndex].endPktDesc = 
			memAlloc( devNum, 1 * sizeof(MDK_ATHEROS_DESC));
	if(probePkt) {
		endPktType = MDK_PROBE_LAST_PKT;
	}

	// this is the special end descriptor
	createTransmitPacket(devNum, endPktType, dest, 1, 0, NULL, 0, 0,
		queueIndex, &pktSize, &(pLibDev->tx[queueIndex].endPktAddr));

	//write buffer ptr to descriptor
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
	    if(isCobra(pLibDev->swDevID)) {
			localDesc.bufferPhysPtr = pLibDev->tx[queueIndex].endPktAddr;
		}
		else {
			localDesc.bufferPhysPtr = pLibDev->tx[queueIndex].endPktAddr | HOST_PCI_SDRAM_BASEADDR;
		}
#else
	localDesc.bufferPhysPtr = pLibDev->tx[queueIndex].endPktAddr;
#endif
	
	//Venice needs to have the retries set for end packets
	//so take a copy and artificially set it
	retryValue = pLibDev->tx[queueIndex].retryValue;
	if(probePkt) {
		pLibDev->tx[queueIndex].retryValue = 0;
	}
	else {
		pLibDev->tx[queueIndex].retryValue = 0xf;
	}


	if (pLibDev->libCfgParams.enableXR) {
		if (isFalcon(devNum) || isDragon(devNum)) {
			setDescriptorEndPacketAr5513( devNum, &localDesc, pktSize, 
									       antMode, 0, rateValues[15], 0);
		} else {
		        setDescriptorEndPacketAr5212( devNum, &localDesc, pktSize, 
								       antMode, 0, rateValues[15], 0);
		}
	}
	else if (((pLibDev->swDevID & 0x00ff) >= 0x0013) && (pLibDev->mode == MODE_11G) && (!pLibDev->turbo)) {
		if (isFalcon(devNum) || isDragon(devNum)) {
			setDescriptorEndPacketAr5513( devNum, &localDesc, pktSize, 
									       antMode, 0, rateValues[8], 0);
		} else {
		        setDescriptorEndPacketAr5212( devNum, &localDesc, pktSize, 
								       antMode, 0, rateValues[8], 0);
		}
	} else {
	  	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setDescriptor( devNum, &localDesc, pktSize, 
						antMode, 0, rateValues[0], 0);
	}
	//restore the retry value
	pLibDev->tx[queueIndex].retryValue = retryValue;
	
	localDesc.nextPhysPtr = 0;
	
	writeDescriptor(devNum, pLibDev->tx[queueIndex].endPktDesc, &localDesc);
}

void
sendEndPacket
(
 A_UINT32 devNum,
 A_UINT16 queueIndex
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	if (pLibDev->noEndPacket) {
		return;
	}

#ifdef DEBUG_MEMORY
printf("SNOOP:sendEndPacket::start Send end packet\n");
#endif
    
	//successful transmission of frames, so send special end packet
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->sendTxEndPacket(devNum, queueIndex );

	//cleanup end packet memory allocation
//	if(pLibDev->tx[queueIndex].endPktAddr) {
//		memFree(devNum, pLibDev->tx[queueIndex].endPktAddr);
//		memFree(devNum, pLibDev->tx[queueIndex].endPktDesc);
//		pLibDev->tx[queueIndex].endPktAddr = 0;
//		pLibDev->tx[queueIndex].endPktDesc = 0;
//	}
}


/**************************************************************************
* enableWep - setup the keycache with default keys and set an internel
*			  wep enable flag
*			  For now the software will hardcode some keys in here.
*			  eventually I will change the interface on here to allow
*			  keycache to be setup.
*
*/
MANLIB_API void enableWep
(
 A_UINT32 devNum,
 A_UCHAR key	   //which of the default keys to use
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:enableWep\n", devNum);
		return;
	}

	if(pLibDev->devState < RESET_STATE) {
		mError(devNum, EILSEQ, "Device Number %d:enableWep: device not in reset state - resetDevice must be run first\n", devNum);
		return;
	}

	//program keys 1-3 in the key cache.
	REGW(devNum, 0x9000, 0x22222222);
	REGW(devNum, 0x9004, 0x11);
	REGW(devNum, 0x9014, 0x00); 		//40 bit key
	REGW(devNum, 0x901c, 0x8000);		//valid bit
	REGW(devNum, 0x9010, 0x22222222);
	REGW(devNum, 0x9024, 0x11);
	REGW(devNum, 0x9034, 0x00); 		//40 bit key
	REGW(devNum, 0x903c, 0x8000);		//valid bit
	REGW(devNum, 0x9040, 0x22222222);
	REGW(devNum, 0x9044, 0x11);
	REGW(devNum, 0x9054, 0x00); 		//40 bit key
	REGW(devNum, 0x905c, 0x8000);		//valid bit
	REGW(devNum, 0x9060, 0x22222222);
	REGW(devNum, 0x9064, 0x11);
	REGW(devNum, 0x9074, 0x00); 		//40 bit key
	REGW(devNum, 0x907c, 0x8000);		//valid bit

	REGW(devNum, 0x8800, 0x22222222);
	REGW(devNum, 0x8804, 0x11);
	REGW(devNum, 0x8814, 0x00); 		//40 bit key
	REGW(devNum, 0x881c, 0x8000);		//valid bit
	REGW(devNum, 0x8810, 0x22222222);
	REGW(devNum, 0x8824, 0x11);
	REGW(devNum, 0x8834, 0x00); 		//40 bit key
	REGW(devNum, 0x883c, 0x8000);		//valid bit
	REGW(devNum, 0x8840, 0x22222222);
	REGW(devNum, 0x8844, 0x11);
	REGW(devNum, 0x8854, 0x00); 		//40 bit key
	REGW(devNum, 0x885c, 0x8000);		//valid bit
	REGW(devNum, 0x8860, 0x22222222);
	REGW(devNum, 0x8864, 0x11);
	REGW(devNum, 0x8874, 0x00); 		//40 bit key
	REGW(devNum, 0x887c, 0x8000);		//valid bit
	pLibDev->wepEnable = 1;
	pLibDev->wepKey = key;
}

/**************************************************************************
* txDataBegin - start transmission
*
*/
MANLIB_API void txDataBegin
(
 A_UINT32 devNum, 
 A_UINT32 timeout,
 A_UINT32 remoteStats
)
{
	txDataStart( devNum );
#ifndef _IQV
	txDataComplete( devNum, timeout, remoteStats );
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
////
////

/**************************************************************************
* txDataBegin - start transmission
*
*/
MANLIB_API void txDataStart
(
 A_UINT32 devNum
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	MDK_ATHEROS_DESC	localDesc;
	static A_UINT16 activeQueues[MAX_TX_QUEUE] = {0};	// Index of the active queue
	A_UINT16	activeQueueCount = 0;					// active queues count
	int i = 0;

	//if we are sending probe packets, then only enable the probe queue
	if((pLibDev->txProbePacketNext) && (pLibDev->tx[PROBE_QUEUE].txEnable)) {
		activeQueues[activeQueueCount] = (A_UINT16)PROBE_QUEUE;
		activeQueueCount++;
	}
	else {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			if ( pLibDev->tx[i].txEnable )
			{
				activeQueues[activeQueueCount] = (A_UINT16)i;
				activeQueueCount++;
			}
		}
	}

	for( i = 0; i < activeQueueCount; i++ )
	{
		if (checkDevNum(devNum) == FALSE) {
			mError(devNum, EINVAL, "Device Number %d:txDataStart\n", devNum);
			return;
		}
 
		if(pLibDev->devState < RESET_STATE) {
			mError(devNum, EILSEQ, "Device Number %d:txDataStart: device not in reset state - resetDevice must be run first\n", devNum);
			return;
		}

		if (!pLibDev->tx[activeQueues[i]].txEnable) {
			mError(devNum, EILSEQ, 
				"Device Number %d:txDataStart: txDataSetup must successfully complete before running txDataBegin\n", devNum);
			return; 
		}

		//zero out the local tx stats structures
		memset(&(pLibDev->tx[activeQueues[i]].txStats[0]), 0, sizeof(TX_STATS_STRUCT) * STATS_BINS);
		pLibDev->tx[activeQueues[i]].haveStats = 0;
	}
	
	// cleanup descriptors created by the last begin
	if (pLibDev->rx.rxEnable || pLibDev->rx.bufferAddress) {
		memFree(devNum, pLibDev->rx.bufferAddress);
		pLibDev->rx.bufferAddress = 0;
		memFree(devNum, pLibDev->rx.descAddress);
		pLibDev->rx.descAddress = 0;
		pLibDev->rx.rxEnable = 0;
		pLibDev->rx.numDesc = 0;
		pLibDev->rx.bufferSize = 0;
	}
  
	// Add a local self-linked rx descriptor and buffer to stop receive overrun
	pLibDev->rx.descAddress = memAlloc( devNum, sizeof(MDK_ATHEROS_DESC));
	if (0 == pLibDev->rx.descAddress) {
		mError(devNum, ENOMEM, "Device Number %d:txDataStart: unable to allocate memory for rx-descriptor to prevent overrun\n", devNum);
		return;
	}
	pLibDev->rx.bufferSize = 512;
	pLibDev->rx.bufferAddress = memAlloc(devNum, pLibDev->rx.bufferSize);
	if (0 == pLibDev->rx.bufferAddress) {
		mError(devNum, ENOMEM, "Device Number %d:txDataStart: unable to allocate memory for rx-buffer to prevent overrun\n", devNum);
		return;
	}
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
	    if(isCobra(pLibDev->swDevID)) {
		localDesc.bufferPhysPtr = pLibDev->rx.bufferAddress;
	    localDesc.nextPhysPtr = pLibDev->rx.descAddress;
	}
	else {
		localDesc.bufferPhysPtr = pLibDev->rx.bufferAddress | HOST_PCI_SDRAM_BASEADDR;
	    localDesc.nextPhysPtr = pLibDev->rx.descAddress | HOST_PCI_SDRAM_BASEADDR;
	}
#else
	localDesc.bufferPhysPtr = pLibDev->rx.bufferAddress;
	localDesc.nextPhysPtr = pLibDev->rx.descAddress;
#endif
	localDesc.hwControl[1] = pLibDev->rx.bufferSize;
	localDesc.hwControl[0] = 0;
	writeDescriptor(devNum, pLibDev->rx.descAddress, &localDesc);


	//write RXDP
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
	    if(isCobra(pLibDev->swDevID)) {
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->writeRxDescriptor(devNum, pLibDev->rx.descAddress);
	}
	else {
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->writeRxDescriptor(devNum, pLibDev->rx.descAddress | HOST_PCI_SDRAM_BASEADDR);
	}
#else
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->writeRxDescriptor(devNum, pLibDev->rx.descAddress);
#endif

	//program registers
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->txBeginConfig(devNum, 1);  

	pLibDev->start=milliTime();

}	// txDataStart

MANLIB_API void txDataComplete
(
 A_UINT32 devNum, 
 A_UINT32 timeout,
 A_UINT32 remoteStats
)
{	
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	ISR_EVENT	event;
	A_UINT32 	finish;
	A_UINT32	i = 0;
	A_UINT32	statsLoop = 0;
	static A_UINT32 deltaTimeArray[MAX_TX_QUEUE] = {0}; // array to store the delta time
	static A_UINT16 activeQueues[MAX_TX_QUEUE] = {0};	// Index of the active queue
	A_UINT16	activeQueueCount = 0;	// active queues count
	A_UINT16	wfqCount = 0;			// wait for queues count
	A_UINT32 	startTime;
	A_UINT32 	curTime;

	//if we are sending probe packets, then only enable the probe queue
	if((pLibDev->txProbePacketNext) && (pLibDev->tx[PROBE_QUEUE].txEnable)) {
		activeQueues[activeQueueCount] = (A_UINT16)PROBE_QUEUE;
		activeQueueCount++;
	}
	else {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			if ( pLibDev->tx[i].txEnable )
			{
				activeQueues[activeQueueCount] = (A_UINT16)i;
				activeQueueCount++;
			}
		}
	}
	wfqCount = activeQueueCount;

	startTime = milliTime();


#ifdef DEBUG_MEMORY
printf("SNOOP::txDataComplete::TXDP=%x:TXE=%x\n", REGR(devNum, 0x800), REGR(devNum, 0x840));
printf("SNOOP::txDataComplete::Memory at TXDP %x location is \n", REGR(devNum, 0x800));
memDisplay(devNum, REGR(devNum, 0x800), 12);
printf("SNOOP::txDataComplete::Frame contents at %x pointed location is \n", REGR(devNum, 0x800));
pLibDev->devMap.OSmemRead(devNum, REGR(devNum, 0x800)+4, (A_UCHAR*)&buf_ptr, sizeof(buf_ptr));
memDisplay(devNum, buf_ptr, 10);
printf("SNOOP::txDataComplete::Memory at baseaddress %x location is \n", pLibDev->tx[0].descAddress);
memDisplay(devNum, pLibDev->tx[0].descAddress, 12);
printf("SNOOP::txDataComplete::Frame contents at %x pointed location is \n", pLibDev->tx[0].descAddress);
pLibDev->devMap.OSmemRead(devNum, (pLibDev->tx[0].descAddress+4), (A_UCHAR*)&buf_ptr, sizeof(buf_ptr));
memDisplay(devNum, buf_ptr, 10);
printf("reg8000=%x:reg8004=%x:reg8008=%x:reg800c=%x:reg8014=%x\n", REGR(devNum, 0x8000), REGR(devNum, 0x8004), REGR(devNum, 0x8008), REGR(devNum, 0x800c), REGR(devNum, 0x8014));
#endif





	//wait for event
	for (i = 0; i < timeout && wfqCount; i++)
	{
		event = pLibDev->devMap.getISREvent(devNum);
		
		if (event.valid)
		{
#ifdef DEBUG_MEMORY
printf("SNOOP::txDataComplete::Got event\n");
#endif
			//see if it is the TX_EOL interrupt
			if ( ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->isTxdescEvent(event.ISRValue) 
				/*event.ISRValue & F2_ISR_TXDESC*/) {
				//This is the event we are waiting for
				//stop the clock
				finish=milliTime();
				
				// __TODO__
				// THE "EVENT" WILL HAVE THE INFO. ABOUT THE QUEUE INDEX
				// SO WE COULD STORE THE TIME DELTA AND PROCESS THE 
				// STATS AFTER WE GET ALL THE EOL OR TIMESOUT (i.e. AFTER
				// WE GET OUT OF THIS LOOP
				//
				// deltaTimeArray[event.queueIndex] = finish - start;
				//
				{
//				DWORD dwReg1 = REGR( devNum, 0x00c4 );
//				DWORD dwReg2 = REGR( devNum, 0x00c8 );
//				DWORD dwReg3 = REGR( devNum, 0x00cc );
//				DWORD dwReg4 = REGR( devNum, 0x00d0 );
//				DWORD dwReg5 = REGR( devNum, 0x00d4 );
//				DWORD dwQ = dwReg1 >> 16;
				}
				deltaTimeArray[pLibDev->selQueueIndex] = finish - pLibDev->start;	// __TODO__  GET THE CORRECT INDEX

				//get the stats
				//txAccumulateStats(devNum, finish - start);	// MOVED OUT OF THIS LOOP

				wfqCount--;
				if ( !wfqCount )
				{
					break;
				}
				continue;	// skip the sleep; we got an interrupt, maybe there is more in the event queue
			}
		}
		curTime = milliTime();
		if (curTime > (startTime+timeout)) {
			i = timeout;
			break;
		}
		mSleep(1);		
	}

	if (i == timeout)
	{
		mError(devNum, EIO, "Device Number %d:txDataComplete: timeout reached before all frames transmitted\n", devNum);
		finish=milliTime();
		//return;
	}

	// Get the stats for all active queues and send end packets
	//
	for( i = 0; i < activeQueueCount; i++ )
	{
		if( !(remoteStats & SKIP_STATS_COLLECTION)) {
			txAccumulateStats(devNum, deltaTimeArray[activeQueues[i]], activeQueues[i] );
		}

		//successful transmission of frames, so send special end packet
		//ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->sendTxEndPacket(devNum, activeQueues[i] );
		if(!pLibDev->txProbePacketNext) {
			sendEndPacket(devNum, activeQueues[i]);
		}

	}

	//send stats packets if enabled
	if((remoteStats == ENABLE_STATS_SEND) && (!pLibDev->txProbePacketNext)){
#ifdef DEBUG_MEMORY
printf("SNOOP::txDataComplete::enable stats send\n");
#endif
		
		// send the stats for all the active queues
		for( i = 0; i < activeQueueCount; i++ )
		{
			for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) 
			{
				if((pLibDev->tx[activeQueues[i]].txStats[statsLoop].excessiveRetries > 0) ||
				(pLibDev->tx[activeQueues[i]].txStats[statsLoop].goodPackets > 0)) {
					sendStatsPkt(devNum, statsLoop, MDK_TX_STATS_PKT, pLibDev->tx[activeQueues[i]].destAddr.octets);
				}
			}
			sendStatsPkt(devNum, 0, MDK_TX_STATS_PKT, pLibDev->tx[activeQueues[i]].destAddr.octets);
		}
	}
	
	//cleanup
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->txCleanupConfig(devNum);	
	
	// reset "noEndPacket" flag
	pLibDev->noEndPacket = FALSE;
	
	// __TODO__
	// WRITE TO EACH DCU FOR MAUI, WILL CALL THE FNPTR HERE 
	//
	//write back the retry value
	//REGW(devNum, F2_RETRY_LMT, pLibDev->tx[queueIndex].retryValue);
	//setRetryLimitAr5210( devNum, 0 ); 	// 5210
	//gMdataFnTable[pLibDev->ar5kInitIndex].setRetryLimit( devNum, 0 );
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setRetryLimit( devNum, 0 );

	if(pLibDev->txProbePacketNext) {
		//clear probe packet next flag for next time.
		pLibDev->txProbePacketNext = 0;
		pLibDev->tx[PROBE_QUEUE].txEnable = 0;
		pLibDev->selQueueIndex = pLibDev->backupSelectQueueIndex;
	}
   return;
}

////
////
////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


/**************************************************************************
* rxDataSetup - create packet and descriptors for reception
*
*/
MANLIB_API void rxDataSetup
(
 A_UINT32 devNum, 
 A_UINT32 numDesc, 
 A_UINT32 dataBodyLength,
 A_UINT32 enablePPM 		
)
{
#ifdef _TIME_PROFILE
    A_UINT32 rxDataSetup_start, rxDataSetup_end;
    rxDataSetup_start=milliTime();
#endif
	internalRxDataSetup(devNum, numDesc, dataBodyLength, enablePPM, RX_NORMAL);
#ifdef _TIME_PROFILE
    rxDataSetup_end=milliTime();
printf(".................SNOOP::exit rxDataSetup:Time taken = %dms\n", (rxDataSetup_end-rxDataSetup_start));
#endif
	return;
}
	
MANLIB_API void rxDataSetupFixedNumber
(
 A_UINT32 devNum, 
 A_UINT32 numDesc, 
 A_UINT32 dataBodyLength,
 A_UINT32 enablePPM 		
)
{
	internalRxDataSetup(devNum, numDesc, dataBodyLength, enablePPM, RX_FIXED_NUMBER);
	return;
}


void internalRxDataSetup
(
 A_UINT32 devNum, 
 A_UINT32 numDesc, 
 A_UINT32 dataBodyLength,
 A_UINT32 enablePPM,
 A_UINT32 mode
)
{
	A_UINT32	i;
	A_UINT32	bufferAddress;
	A_UINT32	buffAddrIncrement=0, lastDesc;
	A_UINT32	descAddress;
	MDK_ATHEROS_DESC	*localDescPtr;		   //pointer to current descriptor being filled
	MDK_ATHEROS_DESC    *localDescBuffer;      //create a local buffer to create descriptors
	A_UINT32    sizeBufferMem, intrBit, descOp;
	A_UINT32    falconAddrMod = 0;
	A_BOOL      falconTrue = FALSE;
	A_UINT32    ppm_data_padding = PPM_DATA_SIZE;


	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:rxDataSetup\n", devNum);
		return;
	}
	if(pLibDev->devState < RESET_STATE) {
		mError(devNum, EILSEQ, "Device Number %d:rxDataSetup: device not in reset state - resetDevice must be run first\n", devNum);
		return;
	}

	falconTrue = isFalcon(devNum) || isDragon(devNum);
	if (falconTrue) {
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} 

	if((pLibDev->mode == MODE_11B) && (enablePPM)) {
		//not supported for 11b mode.
		enablePPM = 0;
	}
	
	// cleanup descriptors created last time
	if (pLibDev->rx.rxEnable || pLibDev->rx.bufferAddress) {
		memFree(devNum, pLibDev->rx.bufferAddress);
		pLibDev->rx.bufferAddress = 0;
		memFree(devNum, pLibDev->rx.descAddress);
		pLibDev->rx.descAddress = 0;
		pLibDev->rx.rxEnable = 0;
	}
	
	pLibDev->rx.overlappingDesc = FALSE;
	pLibDev->rx.numDesc = numDesc + 11;
	pLibDev->rx.numExpectedPackets = numDesc;
	pLibDev->rx.rxMode = mode;

	/*
	 * create descriptors, create eleven extra descriptors and buffers: 
	 * - one to receive the special "last packet"
	 * - up to 9 to receive the stats packet (incase this is enabled)
	 * - one linked to itself to prevent overruns
	 */
	pLibDev->rx.descAddress = memAlloc( devNum, pLibDev->rx.numDesc * sizeof(MDK_ATHEROS_DESC));
	if (0 == pLibDev->rx.descAddress) {
		mError(devNum, ENOMEM, "Device Number %d:rxDataSetup: unable to allocate memory for %d descriptors\n", devNum, pLibDev->rx.numDesc);
		return;
	}

	// If getting the PPM data - increase the data body length before upsizing for stats
	if(enablePPM) {
		dataBodyLength += ppm_data_padding;
	}

	//create buffers
	//make sure dataBody length is large enough to take a stats packet, if not then
	//pad it out.  The 8 accounts for the rateBin labels at the front of the packets
	if (dataBodyLength < (sizeof(TX_STATS_STRUCT) + sizeof(RX_STATS_STRUCT) + 8)) {
		dataBodyLength = sizeof(TX_STATS_STRUCT) + sizeof(RX_STATS_STRUCT) + 8;
	}
	pLibDev->rx.bufferSize = dataBodyLength + sizeof(MDK_PACKET_HEADER) 
					+ sizeof(WLAN_DATA_MAC_HEADER3) + FCS_FIELD;
	/* The above calculation makes the receive buffer the exact size.  Want to
	 * add some extra bytes to end and also make sure buffers and 32 byte aligned.
	 * will have at least 0x20 spare bytes	
	 */
	if ((pLibDev->rx.bufferSize & (cache_line_size - 1)) > 0) {
		pLibDev->rx.bufferSize += (0x20 - (pLibDev->rx.bufferSize & (cache_line_size - 1))); 
	}

	if ((pLibDev->rx.bufferSize + 0x20) < 4096) {
		pLibDev->rx.bufferSize += 0x20;
	}
	
	sizeBufferMem = pLibDev->rx.numDesc * pLibDev->rx.bufferSize;

	//check to see if should try overlapping buffers 
	if(sizeBufferMem > pLibDev->devMap.DEV_MEMORY_RANGE) {
		//allocate 1 full buffer (as spare) then add headers + 32 bytes for each required descriptor
		sizeBufferMem = pLibDev->rx.bufferSize +
			(pLibDev->rx.numDesc * (sizeof(MDK_PACKET_HEADER) + sizeof(WLAN_DATA_MAC_HEADER3) + 0x22
		                            + (enablePPM ? ppm_data_padding : 0) ));
		if(sizeBufferMem > pLibDev->devMap.DEV_MEMORY_RANGE) {
			mError(devNum, ENOMEM, "Device Number %d:rxDataSetup: unable to allocate memory for buffers (even overlapped):%d\n", devNum, sizeBufferMem);
			return;
		}
		pLibDev->rx.overlappingDesc = TRUE;
	}

	pLibDev->rx.bufferAddress = memAlloc(devNum, sizeBufferMem);
	
	if (0 == pLibDev->rx.bufferAddress) {
		mError(devNum, ENOMEM, "Device Number %d:rxDataSetup: unable to allocate memory for buffers:%d\n", devNum, sizeBufferMem);
		return;
	}

	//initialize descriptors
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
    if(isCobra(pLibDev->swDevID)) {
		descAddress = pLibDev->rx.descAddress;
	    bufferAddress = pLibDev->rx.bufferAddress;

	if (falconTrue) {
		falconAddrMod = FALCON_MEM_ADDR_MASK ;
	}

	else {
		descAddress = pLibDev->rx.descAddress | HOST_PCI_SDRAM_BASEADDR;
	    bufferAddress = pLibDev->rx.bufferAddress | HOST_PCI_SDRAM_BASEADDR;
	}
#else
	descAddress = pLibDev->rx.descAddress;
	bufferAddress = pLibDev->rx.bufferAddress;
#endif
	localDescBuffer = (MDK_ATHEROS_DESC *)malloc(sizeof(MDK_ATHEROS_DESC) * pLibDev->rx.numDesc);
	if(localDescBuffer == NULL) {
		mError(devNum, ENOMEM, "Device Number %d:txDataSetup: unable to allocate local memory for descriptors\n", devNum);
		return;		
	}
	localDescPtr = localDescBuffer;
	for (i = 0; i < pLibDev->rx.numDesc; i++)
	{
		localDescPtr->bufferPhysPtr = falconAddrMod | bufferAddress;
		
		//update the link pointer
		if (i == pLibDev->rx.numDesc - 1) { //ie its the last descriptor
			//link it to itself
			localDescPtr->nextPhysPtr = falconAddrMod | descAddress;
			pLibDev->rx.lastDescAddress = descAddress;
		}
		else {
			localDescPtr->nextPhysPtr = falconAddrMod | (descAddress + sizeof(MDK_ATHEROS_DESC));
		}
		//update the buffer size, and set interrupt for first descriptor
		localDescPtr->hwControl[1] = pLibDev->rx.bufferSize;
		if (i == 0) {	
			 localDescPtr->hwControl[1] |= DESC_RX_INTER_REQ;
		}
//		else if ((i == (numDesc  - 1)) && (mode == RX_FIXED_NUMBER))
//		{
//			 localDescPtr->hwControl[1] |= DESC_RX_INTER_REQ;
//		}
		
		localDescPtr->hwControl[0] = 0;		

		localDescPtr->hwStatus[0] = 0;
		localDescPtr->hwStatus[1] = 0;

		localDescPtr->hwExtra[0] = 0; // falcon rx status word 3

		//writeDescriptor(devNum, descAddress, localDescPtr);
		zeroDescriptorStatus(devNum, localDescPtr, pLibDev->swDevID);

		//increment descriptor address
		descAddress += sizeof(MDK_ATHEROS_DESC);
		localDescPtr ++;

		if(pLibDev->rx.overlappingDesc) {
			bufferAddress += (sizeof(MDK_PACKET_HEADER) + sizeof(WLAN_DATA_MAC_HEADER3) + 0x22  + (enablePPM ? ppm_data_padding : 0));
			buffAddrIncrement = (sizeof(MDK_PACKET_HEADER) + sizeof(WLAN_DATA_MAC_HEADER3) + 0x22  + (enablePPM ? ppm_data_padding : 0));
		} else {
			bufferAddress += pLibDev->rx.bufferSize;
			buffAddrIncrement = pLibDev->rx.bufferSize;
		}
	}

    if (pLibDev->devMap.remoteLib) {
		lastDesc = LAST_DESC_LOOP << DESC_INFO_LAST_DESC_BIT_START;
		intrBit = DESC_RX_INTER_REQ_START <<  DESC_OP_INTR_BIT_START;

        if (!isFalcon(devNum) && !isDragon(devNum)) {
		   descOp = intrBit | 0 | (2 << DESC_OP_WORD_OFFSET_BIT_START);
        }
        else{
		   descOp = intrBit | 0 | (3 << DESC_OP_WORD_OFFSET_BIT_START);
        }

		pLibDev->devMap.r_createDescriptors(devNum, pLibDev->rx.descAddress, \
							lastDesc |  pLibDev->rx.numDesc |  \
							(sizeof(MDK_ATHEROS_DESC)/sizeof(A_UINT32) << DESC_INFO_NUM_DESC_WORDS_BIT_START), \
							buffAddrIncrement | (1 << BUF_ADDR_INC_CLEAR_BUF_BIT_START), \
							descOp, \
							(A_UINT32 *)localDescBuffer);
	}
	else {
	    //write all the descriptors in one shot
	    writeDescriptors(devNum, pLibDev->rx.descAddress, localDescBuffer, pLibDev->rx.numDesc);
	}
	free(localDescBuffer);

	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setPPM(devNum, enablePPM);

	pLibDev->rx.rxEnable = 1;

	return;
}


/**************************************************************************
* rxDataBegin - Start and complete reception
*
*/
MANLIB_API void rxDataBegin
(
 A_UINT32 devNum, 
 A_UINT32 waitTime, 
 A_UINT32 timeout, 
 A_UINT32 remoteStats,
 A_UINT32 enableCompare,
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	pLibDev->mdkErrno = 0;
	rxDataStart(devNum);
	if (pLibDev->mdkErrno == 0) {
		rxDataComplete(devNum, waitTime, timeout, remoteStats, enableCompare, dataPattern, dataPatternLength);
	}
}

/**************************************************************************
* rxDataBeginSG - Start and complete reception - signal generator accomodation
*
*/
MANLIB_API void rxDataBeginSG
(
 A_UINT32 devNum, 
 A_UINT32 waitTime, 
 A_UINT32 timeout, 
 A_UINT32 remoteStats,
 A_UINT32 enableCompare,
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength,
 A_UINT32 sgpacketnumber
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	pLibDev->mdkErrno = 0;
	rxDataStart(devNum);
	if (pLibDev->mdkErrno == 0) {
		rxDataCompleteSG(devNum, waitTime, timeout, remoteStats, enableCompare, dataPattern, dataPatternLength, sgpacketnumber);
	}
}

/**************************************************************************
* rxDataBegin - Start and complete reception
*
*/
MANLIB_API void rxDataBeginFixedNumber
(
 A_UINT32 devNum, 
 A_UINT32 timeout, 
 A_UINT32 remoteStats,
 A_UINT32 enableCompare,
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength
)
{
	rxDataStart(devNum);
	rxDataCompleteFixedNumber(devNum, timeout, remoteStats, enableCompare, dataPattern, dataPatternLength);
}


/**************************************************************************
* rxDataStart - Start reception
*
*/
MANLIB_API void rxDataStart
(
 A_UINT32 devNum
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	
	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:rxDataBegin\n", devNum);
		return;
	}

	if(pLibDev->devState < RESET_STATE) {
		mError(devNum, EILSEQ, "Device Number %d:rxDataBegin: device not in reset state - resetDevice must be run first\n", devNum);
		return;
	}

	if (!pLibDev->rx.rxEnable) {
		mError(devNum, EILSEQ, 
			"Device Number %d:rxDataBegin: rxDataSetup must successfully complete before running rxDataBegin\n", devNum);
		return; 
	}
	
	// zero out the stats structure
	memset(&(pLibDev->rx.rxStats[0][0]), 0, sizeof(RX_STATS_STRUCT) * STATS_BINS * MAX_TX_QUEUE);
	pLibDev->rx.haveStats = 0;

	// zero out remote transmit stats
//	if(remoteStats & ENABLE_STATS_RECEIVE) {
//		memset(&(pLibDev->txRemoteStats[0][0]), 0, sizeof(TX_STATS_STRUCT) * STATS_BINS * MAX_TX_QUEUE);
//	}

	// program registers
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxBeginConfig(devNum);
}


/**************************************************************************
* rxDataComplete - Start reception
*
*/
MANLIB_API void rxDataComplete
(
 A_UINT32 devNum, 
 A_UINT32 waitTime, 
 A_UINT32 timeout, 
 A_UINT32 remoteStats,
 A_UINT32 enableCompare,
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength
)
{
	ISR_EVENT	event;
	A_UINT32	i, statsLoop, jj;
	A_UINT32	numDesc = 0;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	RX_STATS_TEMP_INFO	statsInfo;
	A_BOOL		statsSent = FALSE;
	A_BOOL		rxComplete = FALSE;
	A_UINT16	queueIndex = pLibDev->selQueueIndex;
	A_UINT32	startTime;
	A_UINT32	curTime;
	A_BOOL		skipStats = FALSE;
	A_UINT16    numStatsToSkip = 0;		
	A_UINT32    tempBuff[2] = {0, 0};
//A_UINT16  junkSeqCounter;
//A_UINT16  junkErrCounter;
	A_BOOL      falconTrue = FALSE;
	A_UINT32    ppm_data_padding = PPM_DATA_SIZE;
	A_UINT32    status1_offset, status2_offset, status3_offset; // legacy status1 - datalen/rate, 
	                                                            // legacy status2 - done/error/timestamp
	                                                            // new with falcon status3 - rssi for 4 ant
#ifdef _IQV
	FILE *fp;
	FILE *fpStart;
#endif		


	falconTrue = isFalcon(devNum) || isDragon(devNum);

	if (falconTrue) {
		status1_offset = FIRST_FALCON_RX_STATUS_WORD;
		status2_offset = SECOND_FALCON_RX_STATUS_WORD;
		status3_offset = FALCON_ANT_RSSI_RX_STATUS_WORD;
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} else {
		status1_offset = FIRST_STATUS_WORD;
		status2_offset = SECOND_STATUS_WORD;
		status3_offset = status2_offset; // to avoid illegal value. should never be used besides multAnt chips
	}

	startTime = milliTime();
    timeout = timeout * 10;

#ifdef DEBUG_MEMORY
printf("Desc Base address = %x:RXDP=%x:RXE/RXD=%x\n", pLibDev->rx.descAddress, REGR(devNum, 0xc), REGR(devNum, 0x8));
memDisplay(devNum, pLibDev->rx.descAddress, 12);
printf("reg8000=%x:reg8004=%x:reg8008=%x:reg800c=%x:reg8014=%x\n", REGR(devNum, 0x8000), REGR(devNum, 0x8004), REGR(devNum, 0x8008), REGR(devNum, 0x800c), REGR(devNum, 0x8014));

#endif

#ifdef _IQV
//  to inform IQFact that ready for sending packets
	fpStart = fopen("log/PerStart.txt", "w");
	if (fpStart)
	{
		fprintf(fpStart, "start!");    // for sync with descriptor read.
		fclose(fpStart);
	}
//	printf("I am waiting....\n");		// _IQV	
#endif

	// wait for event
	event.valid = 0;
	event.ISRValue = 0;
	for (i = 0; i < waitTime; i++)
	{
		event = pLibDev->devMap.getISREvent(devNum);
		if (event.valid) {
			if(ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->isRxdescEvent(event.ISRValue)) {
	//printf("I detected event....\n");		// _IQV	
	//			mSleep(150);				// _IQV	
				break;
			}
		}

		curTime = milliTime();
		if (curTime > (startTime+waitTime)) {
			i = waitTime;
			break;
		}
		mSleep(1);	
	}

#ifdef _IQV
// wait for packets all be sent
//	printf("I am out of event waiting ....\n");		// _IQV	
		fp = fopen("log/PerDone.txt", "r");
		while (!fp)
		{
			Sleep(100);
			fp = fopen("log/PerDone.txt", "r");
		}
		if (fp){
			fclose(fp);
			DeleteFile("log/PerDone.txt");
		}
//	printf("I am out of waiting....\n");		// _IQV	
#endif

#ifdef DEBUG_MEMORY
printf("waitTime = %d\n", waitTime);
printf("Desc Base address = %x:RXDP=%x:RXE/RXD=%x\n", pLibDev->rx.descAddress, REGR(devNum, 0xc), REGR(devNum, 0x8));
memDisplay(devNum, pLibDev->rx.descAddress, 12);
printf("reg8000=%x:reg8004=%x:reg8008=%x:reg800c=%x:reg8014=%x\n", REGR(devNum, 0x8000), REGR(devNum, 0x8004), REGR(devNum, 0x8008), REGR(devNum, 0x800c), REGR(devNum, 0x8014));

#endif
	
	// This is a special case to allow the script multitest.pl to send and receive between
	// two cards in the same machine.  A wait time of 1 will setup RX and exit
	// A second rxDataBegin will collect the statistics.
	if (waitTime == 1) {
		return;
	}
	else if ((i == waitTime) && (waitTime !=0)) {
		mError(devNum, EIO, "Device Number %d:rxDataBegin: nothing received within %d millisecs (waitTime)\n", devNum, waitTime);
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
		return;
	}

	i = 0;
	memset(&statsInfo, 0, sizeof(RX_STATS_TEMP_INFO));
	for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) {
		statsInfo.sigStrength[statsLoop].rxMinSigStrength = 127;
		if (falconTrue) {
			for (jj = 0; jj < 4; jj++ ) {
				statsInfo.multAntSigStrength[statsLoop].rxMinSigStrengthAnt[jj] = 127;
				statsInfo.multAntSigStrength[statsLoop].rxMaxSigStrengthAnt[jj] = -127;
			}
		}
	}

	if(remoteStats & SKIP_SOME_STATS) {
		//extract and set number to skip
		skipStats = 1;
		numStatsToSkip = (A_UINT16)((remoteStats & NUM_TO_SKIP_M) >> NUM_TO_SKIP_S);
	}

	if(pLibDev->rx.enablePPM) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) 
			{
				pLibDev->rx.rxStats[i][statsLoop].ppmMax = -1000;
				pLibDev->rx.rxStats[i][statsLoop].ppmMin = 1000;
			}
		}
	}

	statsInfo.descToRead = pLibDev->rx.descAddress;
	if (falconTrue) {
		statsInfo.descToRead &= FALCON_DESC_ADDR_MASK;
	}

	if (enableCompare) {
		//create a max size compare buffer for easy compare
		statsInfo.pCompareData = (A_UCHAR *)malloc(FRAME_BODY_SIZE);

		if(!statsInfo.pCompareData) {
			mError(devNum, ENOMEM, "Device Number %d:rxDataBegin: Unable to allocate memory for compare buffer\n", devNum);
			ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
			return;
		}

		fillCompareBuffer(statsInfo.pCompareData, FRAME_BODY_SIZE, dataPattern, dataPatternLength);
	}	

	startTime=milliTime();
//	junkSeqCounter = 0;
//	junkErrCounter = 0;
	//sit in polling loop, looking for descriptors to be done.
	do {
		//read descriptor status words

	   	// Read descriptor at once
		fillRxDescAndFrame(devNum, &statsInfo);
	   	//



/* Siva
		pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status2_offset, 
					(A_UCHAR *)&statsInfo.status2, sizeof(statsInfo.status2));
*/

		if (statsInfo.status2 & DESC_DONE) {
/* Siva
			pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status1_offset, 
					(A_UCHAR *)&statsInfo.status1, sizeof(statsInfo.status1));
			if (falconTrue) {
					pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status3_offset, 
						(A_UCHAR *)&statsInfo.status3, sizeof(statsInfo.status3));
			}
			pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + BUFFER_POINTER, 
					(A_UCHAR *)&statsInfo.bufferAddr, sizeof(statsInfo.bufferAddr));
*/
			if (falconTrue) {
				statsInfo.bufferAddr &= FALCON_DESC_ADDR_MASK;
			}

			if (!(remoteStats & LEAVE_DESC_STATUS)) {
			    //zero out status
			    pLibDev->devMap.OSmemWrite(devNum, statsInfo.descToRead + status1_offset, 
						(A_UCHAR *)tempBuff, 8);
			}

//pLibDev->devMap.OSmemWrite(devNum, statsInfo.descToRead + status2_offset, 
//			(A_UCHAR *)&tempBuff, 4);


			statsInfo.descRate = descRate2bin((statsInfo.status1 >> BITS_TO_RX_DATA_RATE) & pLibDev->rxDataRateMsk);
			// Initialize loop variables
			statsInfo.badPacket = 0;
			statsInfo.gotHeaderInfo = FALSE; // TODO: FIX for multi buffer packet support
			statsInfo.illegalBuffSize = 0;
			statsInfo.controlFrameReceived = 0;
			statsInfo.mdkPktType = 0;

			// Increase buffer address for PPM
			if(pLibDev->rx.enablePPM) {
				statsInfo.bufferAddr += ppm_data_padding;
			}

			//reset our timeout counter
			i = 0;
						
			if (numDesc == pLibDev->rx.numDesc - 1) {
				//This is the final looped rx desc and done bit is set, we ran out of receive descriptors, 
				//so return with an error
				mError(devNum, ENOMEM, "Device Number %d:rxDataComplete:  ran out of receive descriptors\n", devNum);
				break;
			}

			mdkExtractAddrAndSequence(devNum, &statsInfo);

			
#ifdef DEBUG_MEMORY
printf("SNOOP::mdkPkt s1=%X:s2=%X:Type = %x\n", statsInfo.status1, statsInfo.status2, statsInfo.mdkPktType);
printf("SNOOP::Process packet:bP=%d:cFR=%d:iBS=%d\n", statsInfo.badPacket, statsInfo.controlFrameReceived, statsInfo.illegalBuffSize);
#endif
			//only process packets if things are good
			if ( !((statsInfo.status1 & DESC_MORE) || statsInfo.badPacket || statsInfo.controlFrameReceived || 
				statsInfo.illegalBuffSize) ) {
#ifdef DEBUG_MEMORY
printf("SNOOP::Processing packet\n");
#endif
				//check for this being "last packet" or stats packet
				//mdkExtractAddrAndSequence also pulled mdkPkt type info from packet
				if ((statsInfo.status2 & DESC_FRM_RCV_OK) 
				 && (statsInfo.mdkPktType == MDK_LAST_PKT)){
					//if were not expecting remote stats then we are done
					if (!(remoteStats & ENABLE_STATS_RECEIVE)) {
						rxComplete = TRUE;
					}

					//we have done with receive so can send stats
					if(remoteStats & ENABLE_STATS_SEND) {
						for (i = 0; i < MAX_TX_QUEUE; i++ )
						{
							for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) {
								if((pLibDev->rx.rxStats[i][statsLoop].crcPackets > 0) ||
									(pLibDev->rx.rxStats[i][statsLoop].goodPackets > 0)) {
									sendStatsPkt(devNum, statsLoop, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
								}
							}
						}
						sendStatsPkt(devNum, 0, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
						statsSent = TRUE;
					}
				}
				else if((statsInfo.status2 & DESC_FRM_RCV_OK) 
					 && (statsInfo.mdkPktType >= MDK_TX_STATS_PKT) 
					 && (statsInfo.mdkPktType <= MDK_TXRX_STATS_PKT)) {
					rxComplete = mdkExtractRemoteStats(devNum, &statsInfo);
				}
				else if (statsInfo.mdkPktType == MDK_NORMAL_PKT) {
					if (enableCompare) {
						comparePktData(devNum, &statsInfo);
					}
					if (pLibDev->rx.enablePPM) {
						extractPPM(devNum, &statsInfo);
					}

					if(skipStats && (numStatsToSkip != 0)) {
						numStatsToSkip --;
					}
					else {
						mdkExtractRxStats(devNum, &statsInfo);
					}
 				}

				else if ((statsInfo.mdkPktType == MDK_PROBE_PKT) ||
						(statsInfo.mdkPktType == MDK_PROBE_LAST_PKT)) {
					//Want to ignore probe packets, do nothing

				}
				else {
					if(statsInfo.status2 & DESC_FRM_RCV_OK) {
						mError(devNum, EIO, "Device Number %d:A good matching packet with an unknown MDK_PKT type detected MDK_PKT: %d\n", devNum, 
							statsInfo.mdkPktType);
						{
							A_UINT32 iIndex;
							printf("Frame Info\n");
							for(iIndex=0; iIndex<statsInfo.frame_info_len; iIndex++) {
								printf("%X ", statsInfo.frame_info[iIndex]);
							}
							printf("\n");
							printf("PPM Data Info\n");
							for(iIndex=0; iIndex<statsInfo.ppm_data_len; iIndex++) {
								printf("%X ", statsInfo.ppm_data[iIndex]);
							}
							printf("\n");
						}
					}
					if (statsInfo.status2 & DESC_CRC_ERR)
					{
						pLibDev->rx.rxStats[statsInfo.qcuIndex][0].crcPackets++;
						pLibDev->rx.rxStats[statsInfo.qcuIndex][statsInfo.descRate].crcPackets++;
						pLibDev->rx.haveStats = 1;
					}
					else if (statsInfo.status2 & pLibDev->decryptErrMsk)
					{
						pLibDev->rx.rxStats[statsInfo.qcuIndex][0].decrypErrors++;
						pLibDev->rx.rxStats[statsInfo.qcuIndex][statsInfo.descRate].decrypErrors++;
						pLibDev->rx.haveStats = 1;
					}

					
				}
			}

			if(rxComplete) {
				break;
			}

			//get next descriptor to process
			statsInfo.descToRead += sizeof(MDK_ATHEROS_DESC);
			numDesc ++;
		}
		else {
#ifndef _IQV
			curTime = milliTime();
			if (curTime > (startTime+timeout)) {
				i = timeout;
				break;
			}
			mSleep(1);
			i++;
#else
			rxComplete = TRUE;	
			break;				
#endif
		}

	} while (i < timeout);

	milliTime();
	
	if((remoteStats & ENABLE_STATS_SEND) && !statsSent) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) {
				if((pLibDev->rx.rxStats[i][statsLoop].crcPackets > 0) ||
					(pLibDev->rx.rxStats[i][statsLoop].goodPackets > 0)) {
					sendStatsPkt(devNum, statsLoop, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
				}
			}
			sendStatsPkt(devNum, 0, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
		}
	}

	//cleanup
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);

	if(pLibDev->tx[queueIndex].txEnable) {
		//write back the retry value
		//REGW(devNum, F2_RETRY_LMT, pLibDev->tx[queueIndex].retryValue);	__TODO__
	}

	//do any other register cleanup required
	if(enableCompare) {
		free(statsInfo.pCompareData);
	}

	if ((i == timeout) && (rxComplete != TRUE))
	{
		mError(devNum, EIO, 
			"Device Number %d:rxDataBegin: timeout reached, without receiving all packets.  Number received = %lu\n", devNum,
			numDesc);
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
	}

	return;
}

/**************************************************************************
* rxDataCompleteSG - Start reception : Adjustment for Signal Generators
*
*/
MANLIB_API void rxDataCompleteSG
(
 A_UINT32 devNum, 
 A_UINT32 waitTime, 
 A_UINT32 timeout, 
 A_UINT32 remoteStats,
 A_UINT32 enableCompare,
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength,
 A_UINT32 sgpacketnumber
)
{
	ISR_EVENT	event;
	A_UINT32	i, statsLoop, jj;
	A_UINT32	numDesc = 0;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	RX_STATS_TEMP_INFO	statsInfo;
	A_BOOL		statsSent = FALSE;
	A_BOOL		rxComplete = FALSE;
	A_UINT16	queueIndex = pLibDev->selQueueIndex;
	A_UINT32	startTime;
	A_UINT32	curTime;
	A_BOOL		skipStats = FALSE;
	A_UINT16    numStatsToSkip = 0;		
	A_UINT32    tempBuff[2] = {0, 0};
//A_UINT16  junkSeqCounter;
//A_UINT16  junkErrCounter;
	A_BOOL      falconTrue = FALSE;
	A_UINT32    ppm_data_padding = PPM_DATA_SIZE;
	A_UINT32    status1_offset, status2_offset, status3_offset; // legacy status1 - datalen/rate, 
	                                                            // legacy status2 - done/error/timestamp
	                                                            // new with falcon status3 - rssi for 4 ant

        A_UINT32     temp_buff_ind;
//	A_UINT32     temp_reg_rd;
        A_UINT32     pktcount = 0;
        A_UINT32     pktmon = 0;
        A_UINT32     storVar;
// 

	falconTrue = isFalcon(devNum) || isDragon(devNum);

	if (falconTrue) {
		status1_offset = FIRST_FALCON_RX_STATUS_WORD;
		status2_offset = SECOND_FALCON_RX_STATUS_WORD;
		status3_offset = FALCON_ANT_RSSI_RX_STATUS_WORD;
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} else {
		status1_offset = FIRST_STATUS_WORD;
		status2_offset = SECOND_STATUS_WORD;
		status3_offset = status2_offset; // to avoid illegal value. should never be used besides multAnt chips
	}
     
	 //store register to restore later
	storVar = REGR(devNum, 0x803C);

	REGW(devNum, 0x803C, storVar | 0x00000005); 

	startTime = milliTime();
    timeout = timeout * 10;

#ifdef DEBUG_MEMORY
printf("Desc Base address = %x:RXDP=%x:RXE/RXD=%x\n", pLibDev->rx.descAddress, REGR(devNum, 0xc), REGR(devNum, 0x8));
memDisplay(devNum, pLibDev->rx.descAddress, 12);
printf("reg8000=%x:reg8004=%x:reg8008=%x:reg800c=%x:reg8014=%x\n", REGR(devNum, 0x8000), REGR(devNum, 0x8004), REGR(devNum, 0x8008), REGR(devNum, 0x800c), REGR(devNum, 0x8014));

#endif

	// wait for event
	event.valid = 0;
	event.ISRValue = 0;
	for (i = 0; i < waitTime; i++)
	{
		event = pLibDev->devMap.getISREvent(devNum);
		if (event.valid) {
			if(ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->isRxdescEvent(event.ISRValue)) {
				break;
			}
		}

		curTime = milliTime();
		if (curTime > (startTime+waitTime)) {
			i = waitTime;
			break;
		}
		mSleep(1);	
	}

#ifdef DEBUG_MEMORY
printf("waitTime = %d\n", waitTime);
printf("Desc Base address = %x:RXDP=%x:RXE/RXD=%x\n", pLibDev->rx.descAddress, REGR(devNum, 0xc), REGR(devNum, 0x8));
memDisplay(devNum, pLibDev->rx.descAddress, 12);
printf("reg8000=%x:reg8004=%x:reg8008=%x:reg800c=%x:reg8014=%x\n", REGR(devNum, 0x8000), REGR(devNum, 0x8004), REGR(devNum, 0x8008), REGR(devNum, 0x800c), REGR(devNum, 0x8014));

#endif
	
	// This is a special case to allow the script multitest.pl to send and receive between
	// two cards in the same machine.  A wait time of 1 will setup RX and exit
	// A second rxDataBegin will collect the statistics.
	if (waitTime == 1) {
		return;
	}
	else if ((i == waitTime) && (waitTime !=0)) {
		mError(devNum, EIO, "Device Number %d:rxDataBegin: nothing received within %d millisecs (waitTime)\n", devNum, waitTime);
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
		return;
	}

	i = 0;
	memset(&statsInfo, 0, sizeof(RX_STATS_TEMP_INFO));
	for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) {
		statsInfo.sigStrength[statsLoop].rxMinSigStrength = 127;
		if (falconTrue) {
			for (jj = 0; jj < 4; jj++ ) {
				statsInfo.multAntSigStrength[statsLoop].rxMinSigStrengthAnt[jj] = 127;
				statsInfo.multAntSigStrength[statsLoop].rxMaxSigStrengthAnt[jj] = -127;
			}
		}
	}

	if(remoteStats & SKIP_SOME_STATS) {
		//extract and set number to skip
		skipStats = 1;
		numStatsToSkip = (A_UINT16)((remoteStats & NUM_TO_SKIP_M) >> NUM_TO_SKIP_S);
	}

	if(pLibDev->rx.enablePPM) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) 
			{
				pLibDev->rx.rxStats[i][statsLoop].ppmMax = -1000;
				pLibDev->rx.rxStats[i][statsLoop].ppmMin = 1000;
			}
		}
	}

	statsInfo.descToRead = pLibDev->rx.descAddress;
	if (falconTrue) {
		statsInfo.descToRead &= FALCON_DESC_ADDR_MASK;
	}

	if (enableCompare) {
		//create a max size compare buffer for easy compare
		statsInfo.pCompareData = (A_UCHAR *)malloc(FRAME_BODY_SIZE);

		if(!statsInfo.pCompareData) {
			mError(devNum, ENOMEM, "Device Number %d:rxDataBegin: Unable to allocate memory for compare buffer\n", devNum);
			ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
			return;
		}

		fillCompareBuffer(statsInfo.pCompareData, FRAME_BODY_SIZE, dataPattern, dataPatternLength);
	}	

	startTime=milliTime();
//	junkSeqCounter = 0;
//	junkErrCounter = 0;
	//sit in polling loop, looking for descriptors to be done.
	do {
		//read descriptor status words

	   	// Read descriptor at once
		fillRxDescAndFrame(devNum, &statsInfo);


// Ignore 2 descriptors for the setup delay
	if(numDesc == 0) {
		//extract and set number to skip
		skipStats = 1;
		numStatsToSkip = 2;
	}

/* Siva
		pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status2_offset, 
					(A_UCHAR *)&statsInfo.status2, sizeof(statsInfo.status2));
*/

		if (statsInfo.status2 & DESC_DONE) {
/* Siva
			pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status1_offset, 
					(A_UCHAR *)&statsInfo.status1, sizeof(statsInfo.status1));
			if (falconTrue) {
					pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status3_offset, 
						(A_UCHAR *)&statsInfo.status3, sizeof(statsInfo.status3));
			}
			pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + BUFFER_POINTER, 
					(A_UCHAR *)&statsInfo.bufferAddr, sizeof(statsInfo.bufferAddr));
*/
			if (falconTrue) {
				statsInfo.bufferAddr &= FALCON_DESC_ADDR_MASK;
			}

			if (!(remoteStats & LEAVE_DESC_STATUS)) {
			    //zero out status
			    pLibDev->devMap.OSmemWrite(devNum, statsInfo.descToRead + status1_offset, 
						(A_UCHAR *)tempBuff, 8);
			}

//pLibDev->devMap.OSmemWrite(devNum, statsInfo.descToRead + status2_offset, 
//			(A_UCHAR *)&tempBuff, 4);


			statsInfo.descRate = descRate2bin((statsInfo.status1 >> BITS_TO_RX_DATA_RATE) & pLibDev->rxDataRateMsk);
			// Initialize loop variables
			statsInfo.badPacket = 0;
			statsInfo.gotHeaderInfo = FALSE; // TODO: FIX for multi buffer packet support
			statsInfo.illegalBuffSize = 0;
			statsInfo.controlFrameReceived = 0;
			statsInfo.mdkPktType = 0;

			// Increase buffer address for PPM
			if(pLibDev->rx.enablePPM) {
				statsInfo.bufferAddr += ppm_data_padding;
			}

			//reset our timeout counter
			i = 0;

			if (numDesc == pLibDev->rx.numDesc - 1) {
				//This is the final looped rx desc and done bit is set, we ran out of receive descriptors, 
				//so return with an error
				mError(devNum, ENOMEM, "Device Number %d:rxDataCompleteSG:  ran out of receive descriptors\n", devNum);
				break;
			}

			mdkExtractAddrAndSequence(devNum, &statsInfo);

// Reciving Packets count
	if ((statsInfo.status2 & DESC_DONE)) {
		pktcount = pktcount + 1;
	}

#ifdef DEBUG_MEMORY
printf("SNOOP::mdkPkt s1=%X:s2=%X:Type = %x\n", statsInfo.status1, statsInfo.status2, statsInfo.mdkPktType);
printf("SNOOP::Process packet:bP=%d:cFR=%d:iBS=%d\n", statsInfo.badPacket, statsInfo.controlFrameReceived, statsInfo.illegalBuffSize);
#endif
			//only process packets if things are good
			if ( !((statsInfo.status1 & DESC_MORE) || statsInfo.badPacket || statsInfo.controlFrameReceived || 
				statsInfo.illegalBuffSize) ) {
#ifdef DEBUG_MEMORY
printf("SNOOP::Processing packet\n");
#endif
				//check for this being "last packet" or stats packet
				//mdkExtractAddrAndSequence also pulled mdkPkt type info from packet

// Ignore 2 descriptors for the setup delay
				if ((statsInfo.status2 & DESC_DONE)  && (pktcount > sgpacketnumber + 2 )){
//
					//if were not expecting remote stats then we are done
					if (!(remoteStats & ENABLE_STATS_RECEIVE)) {
						rxComplete = TRUE;
					}

					//we have done with receive so can send stats
					if(remoteStats & ENABLE_STATS_SEND) {
						for (i = 0; i < MAX_TX_QUEUE; i++ )
						{
							for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) {
								if((pLibDev->rx.rxStats[i][statsLoop].crcPackets > 0) ||
									(pLibDev->rx.rxStats[i][statsLoop].goodPackets > 0)) {
									sendStatsPkt(devNum, statsLoop, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
								}
							}
						}
						sendStatsPkt(devNum, 0, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
						statsSent = TRUE;
					}
				}
				else if((statsInfo.status2 & DESC_FRM_RCV_OK) 
					 && (statsInfo.mdkPktType >= MDK_TX_STATS_PKT) 
					 && (statsInfo.mdkPktType <= MDK_TXRX_STATS_PKT)) {
					rxComplete = mdkExtractRemoteStats(devNum, &statsInfo);
				}
				else if (statsInfo.mdkPktType == MDK_NORMAL_PKT) {
					if (enableCompare) {
						comparePktData(devNum, &statsInfo);
					}
					if (pLibDev->rx.enablePPM) {
						extractPPM(devNum, &statsInfo);
					}

					if(skipStats && (numStatsToSkip != 0)) {
						numStatsToSkip --;
					}
					else {
						mdkExtractRxStats(devNum, &statsInfo);
					}
 				}

				else if ((statsInfo.mdkPktType == MDK_PROBE_PKT) ||
						(statsInfo.mdkPktType == MDK_PROBE_LAST_PKT)) {
					//Want to ignore probe packets, do nothing

				}
				else {
					if(statsInfo.status2 & DESC_FRM_RCV_OK) {
						mError(devNum, EIO, "Device Number %d:A good matching packet with an unknown MDK_PKT type detected MDK_PKT: %d\n", devNum, 
							statsInfo.mdkPktType);
						{
							A_UINT32 iIndex;
							printf("Frame Info\n");
							for(iIndex=0; iIndex<statsInfo.frame_info_len; iIndex++) {
								printf("%X ", statsInfo.frame_info[iIndex]);
							}
							printf("\n");
							printf("PPM Data Info\n");
							for(iIndex=0; iIndex<statsInfo.ppm_data_len; iIndex++) {
								printf("%X ", statsInfo.ppm_data[iIndex]);
							}
							printf("\n");
						}
					}
					if (statsInfo.status2 & DESC_CRC_ERR)
					{
						pLibDev->rx.rxStats[statsInfo.qcuIndex][0].crcPackets++;
						pLibDev->rx.rxStats[statsInfo.qcuIndex][statsInfo.descRate].crcPackets++;
						pLibDev->rx.haveStats = 1;
					}
					else if (statsInfo.status2 & pLibDev->decryptErrMsk)
					{
						pLibDev->rx.rxStats[statsInfo.qcuIndex][0].decrypErrors++;
						pLibDev->rx.rxStats[statsInfo.qcuIndex][statsInfo.descRate].decrypErrors++;
						pLibDev->rx.haveStats = 1;
					}

					
				}
			}


			if(rxComplete) {
				break;
			}
//  logic end SG

			//get next descriptor to process
			statsInfo.descToRead += sizeof(MDK_ATHEROS_DESC);
			numDesc ++;
		}
		else {
			curTime = milliTime();
			if (curTime > (startTime+timeout)) {
				i = timeout;
				break;
			}
			mSleep(1);
			i++;
		}

	} while (i < timeout);

	milliTime();
	
	if((remoteStats & ENABLE_STATS_SEND) && !statsSent) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) {
				if((pLibDev->rx.rxStats[i][statsLoop].crcPackets > 0) ||
					(pLibDev->rx.rxStats[i][statsLoop].goodPackets > 0)) {
					sendStatsPkt(devNum, statsLoop, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
				}
			}
			sendStatsPkt(devNum, 0, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
		}
	}

	//cleanup
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);

	if(pLibDev->tx[queueIndex].txEnable) {
		//write back the retry value
		//REGW(devNum, F2_RETRY_LMT, pLibDev->tx[queueIndex].retryValue);	__TODO__
	}

	//do any other register cleanup required
	if(enableCompare) {
		free(statsInfo.pCompareData);
	}

	if ((i == timeout) && (rxComplete != TRUE))
	{
		mError(devNum, EIO, 
			"Device Number %d:rxDataBegin: timeout reached, without receiving all packets.  Number received = %lu\n", devNum,
			numDesc);
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
	}
	REGW(devNum, 0x803C, storVar); //revert to the way it was before this function.
	return;
}


/**************************************************************************
* rxDataCompleteFixedNumber - receive until get required amount of frames
*
*/
MANLIB_API void rxDataCompleteFixedNumber
(
 A_UINT32 devNum, 
 A_UINT32 timeout, 
 A_UINT32 remoteStats,
 A_UINT32 enableCompare,
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength
)
{
	ISR_EVENT	event;
	A_UINT32	i, statsLoop;
	A_UINT32	numDesc = 0;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	RX_STATS_TEMP_INFO	statsInfo;
//	A_BOOL		statsSent = FALSE;
//	A_BOOL		rxComplete = FALSE;
//	A_UINT16	queueIndex = pLibDev->selQueueIndex;
	A_UINT32	startTime = 0;
	A_UINT32	curTime;
	A_BOOL		skipStats = FALSE;
	A_UINT16     numStatsToSkip = 0;		
	A_UINT32     numReceived = 0;
	A_BOOL	     done = FALSE;

	startTime = milliTime();
	if(remoteStats & ENABLE_STATS_RECEIVE) {
		mError(devNum, EINVAL, "Device Number %d:Stats receive is not supported in this mode.  Returning...\n", devNum);
		return;
	}

	// wait for event
	event.valid = 0;
	event.ISRValue = 0;
	for (i = 0; i < timeout; i++)
	{
		event = pLibDev->devMap.getISREvent(devNum);
		if (event.valid) {
			if(ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->isRxdescEvent(event.ISRValue)) {
				//reset the starttime clock so we timeout from first received
				startTime = milliTime();
				break;
			}
		}

		curTime = milliTime();
		if (curTime > (startTime+timeout)) {
			i = timeout;
			break;
		}
		mSleep(1);	
	}

	
	if (i == timeout) {
		mError(devNum, EIO, "Device Number %d:rxDataBeginFixedNumber: did not recieved required number within %d millisecs (timeout)\n", devNum, timeout);
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
		//fall through and gather stats anyway incase we received some
//		return;
	}

	i = 0;
	memset(&statsInfo, 0, sizeof(RX_STATS_TEMP_INFO));
	for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) {
		statsInfo.sigStrength[statsLoop].rxMinSigStrength = 127;
	}

	if(remoteStats & SKIP_SOME_STATS) {
		//extract and set number to skip
		skipStats = 1;
		numStatsToSkip = (A_UINT16)((remoteStats & NUM_TO_SKIP_M) >> NUM_TO_SKIP_S);
	}

	if(pLibDev->rx.enablePPM) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) 
			{
				pLibDev->rx.rxStats[i][statsLoop].ppmMax = -1000;
				pLibDev->rx.rxStats[i][statsLoop].ppmMin = 1000;
			}
		}
	}

	statsInfo.descToRead = pLibDev->rx.descAddress;
	if (enableCompare) {
		//create a max size compare buffer for easy compare
		statsInfo.pCompareData = (A_UCHAR *)malloc(FRAME_BODY_SIZE);

		if(!statsInfo.pCompareData) {
			mError(devNum, ENOMEM, "Device Number %d:rxDataBegin: Unable to allocate memory for compare buffer\n", devNum);
			ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
			return;
		}

		fillCompareBuffer(statsInfo.pCompareData, FRAME_BODY_SIZE, dataPattern, dataPatternLength);
	}	

	//sit in polling loop, looking for descriptors to be done.
	do {
		fillRxDescAndFrame(devNum, &statsInfo);
		//read descriptor status words
//		pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status2_offset, 
//					(A_UCHAR *)&statsInfo.status2, sizeof(statsInfo.status2));

		if (statsInfo.status2 & DESC_DONE) {
//			pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + status1_offset, 
//					(A_UCHAR *)&statsInfo.status1, sizeof(statsInfo.status1));
//			pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + BUFFER_POINTER, 
//					(A_UCHAR *)&statsInfo.bufferAddr, sizeof(statsInfo.bufferAddr));
			statsInfo.descRate = descRate2bin((statsInfo.status1 >> BITS_TO_RX_DATA_RATE) & pLibDev->rxDataRateMsk);

			// Initialize loop variables
			statsInfo.badPacket = 0;
			statsInfo.gotHeaderInfo = FALSE; // TODO: FIX for multi buffer packet support
			statsInfo.illegalBuffSize = 0;
			statsInfo.controlFrameReceived = 0;
			statsInfo.mdkPktType = 0;

			//just gather simple statistics on the packets received.
			// Increase buffer address for PPM
			if(pLibDev->rx.enablePPM) {
				statsInfo.bufferAddr += PPM_DATA_SIZE;
			}

			//reset our timeout counter
			i = 0;
						
			if (numDesc == pLibDev->rx.numDesc - 1) {
				//This is the final looped rx desc we are done, don't count this one
				printf("Device Number %d:got to looped descriptor\n", devNum);
				done = TRUE;
			}
			//only process packets if things are good
			if ( !(statsInfo.status1 & DESC_MORE) ) {
				if (enableCompare) {
					comparePktData(devNum, &statsInfo);
				}
				if (pLibDev->rx.enablePPM) {
					extractPPM(devNum, &statsInfo);
				}

				if(skipStats && (numStatsToSkip != 0)) {
					numStatsToSkip --;
				}
				else {
					mdkExtractAddrAndSequence(devNum, &statsInfo);
					if(!statsInfo.badPacket) {
						mdkExtractRxStats(devNum, &statsInfo);
						numReceived++;

					}
				}
				if(numReceived == pLibDev->rx.numExpectedPackets) {
					done = TRUE;
				}
			}

			//get next descriptor to process
			statsInfo.descToRead += sizeof(MDK_ATHEROS_DESC);
			numDesc ++;
		}
		else {
			if(i != timeout) {
				curTime = milliTime();
				if (curTime > (startTime+timeout)) {
					i = timeout;
					ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
					continue;
				}
				mSleep(1);
				i++;
			}
			else {
				if(!statsInfo.status2) {
					//timeout and no more descriptors to gather stats from
					done = TRUE;
				}
			}
		}

	} while (!done);
	
	if(remoteStats & ENABLE_STATS_SEND) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) {
				if((pLibDev->rx.rxStats[i][statsLoop].crcPackets > 0) ||
					(pLibDev->rx.rxStats[i][statsLoop].goodPackets > 0)) {
					sendStatsPkt(devNum, statsLoop, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
				}
			}
			sendStatsPkt(devNum, 0, MDK_RX_STATS_PKT, statsInfo.addrPacket.octets);
		}
	}


	//do any other register cleanup required
	if(enableCompare) {
		free(statsInfo.pCompareData);
	}
	return;
}


MANLIB_API A_BOOL rxLastDescStatsSnapshot
(
 A_UINT32 devNum, 
 RX_STATS_SNAPSHOT *pRxStats
)
{
	A_UINT32  compStatus[2];
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT16  i;
	A_BOOL      falconTrue = FALSE;
	A_UINT32    ppm_data_padding = PPM_DATA_SIZE;
	A_UINT32    ant_rssi;
	A_UINT32    status1_offset, status2_offset, status3_offset; // legacy status1 - datalen/rate, 
	                                                            // legacy status2 - done/error/timestamp
	                                                            // new with falcon status3 - rssi for 4 ant

	falconTrue = isFalcon(devNum) || isDragon(devNum);

	if (falconTrue) {
		status1_offset = FIRST_FALCON_RX_STATUS_WORD;
		status2_offset = SECOND_FALCON_RX_STATUS_WORD;
		status3_offset = FALCON_ANT_RSSI_RX_STATUS_WORD;
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} else {
		status1_offset = FIRST_STATUS_WORD;
		status2_offset = SECOND_STATUS_WORD;
		status3_offset = status2_offset; // to avoid illegal value. should never be used besides multAnt chips
	}


	memset(pRxStats, 0, sizeof(RX_STATS_SNAPSHOT));
	//read the complete status of the last descriptor
	pLibDev->devMap.OSmemRead(devNum, pLibDev->rx.lastDescAddress + status1_offset, 
				(A_UCHAR *)&compStatus, 8);

	//extract the stats if the done bit is set
	if(!compStatus[1] & DESC_DONE) {
		//descriptor is not done
		return FALSE;
	}

	//check in case we only got half the status written
	if(!compStatus[0] || !compStatus[1]) {
		//we read an incomplete descriptor
		printf("Device Number %d:SNOOP: read an incomplete descriptor\n", devNum);
		return FALSE;
	}

	if(compStatus[0] & DESC_MORE) {
		//more bit is set, ignore
		printf("Device Number %d:SNOOP: more bit set on descriptor, ignoring\n", devNum);
		return FALSE;
	}

	//read stats
	if(compStatus[1] & DESC_FRM_RCV_OK) {
		pRxStats->goodPackets = 1;

		pRxStats->DataSigStrength = (A_INT8)((compStatus[0] >> pLibDev->bitsToRxSigStrength) & SIG_STRENGTH_MASK);
		if (falconTrue) {
			pRxStats->chStr  = (compStatus[0] >> 14) & 0x1;
			pRxStats->ch0Sel = (compStatus[0] >> 28) & 0x1;
			pRxStats->ch1Sel = (compStatus[0] >> 29) & 0x1;
			pRxStats->ch0Req = (compStatus[0] >> 30) & 0x1;
			pRxStats->ch1Req = (compStatus[0] >> 31) & 0x1;

			pLibDev->devMap.OSmemRead(devNum, pLibDev->rx.lastDescAddress + status3_offset, 
						(A_UCHAR *)&ant_rssi, 4);
			pRxStats->DataSigStrengthPerAnt[0] = (A_INT8)((ant_rssi >>  0) & SIG_STRENGTH_MASK);
			pRxStats->DataSigStrengthPerAnt[1] = (A_INT8)((ant_rssi >>  8) & SIG_STRENGTH_MASK);
			pRxStats->DataSigStrengthPerAnt[2] = (A_INT8)((ant_rssi >> 16) & SIG_STRENGTH_MASK);
			pRxStats->DataSigStrengthPerAnt[3] = (A_INT8)((ant_rssi >> 24) & SIG_STRENGTH_MASK);
		}
		pRxStats->dataRate = (A_UINT16)((compStatus[0] >> BITS_TO_RX_DATA_RATE) & pLibDev->rxDataRateMsk);
		//convert the rate
		for(i = 0; i < numRateCodes; i++) {
			if((A_UCHAR)pRxStats->dataRate == rateValues[i]) {
				pRxStats->dataRate = i;
				break;
			}
		}
		pRxStats->bodySize = (A_UINT16)(compStatus[0] & DESC_DATA_LENGTH_FIELDS);

		//remove the 802.11 header, FCS and mdk packet header from this length
		pRxStats->bodySize = pRxStats->bodySize - (sizeof(WLAN_DATA_MAC_HEADER3) + sizeof(MDK_PACKET_HEADER) + FCS_FIELD);
	} 
	else {
		if (compStatus[1] & DESC_CRC_ERR)
		{
			pRxStats->crcPackets = 1;
		}
		else if (compStatus[1] & pLibDev->decryptErrMsk)
		{
			pRxStats->decrypErrors = 1;
		}
		
	}

	//zero out the completion status
	compStatus[0] = 0;
	compStatus[1] = 0;
	pLibDev->devMap.OSmemWrite(devNum, pLibDev->rx.lastDescAddress + status1_offset, 
				(A_UCHAR *)&compStatus, 8);

	return TRUE;	


}

////
////
////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
///////////////////////  RX DATA BEGIN - SPLIT //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


/**************************************************************************
* txrxDataBegin - Start transmit and reception
*
*/
MANLIB_API void txrxDataBegin
(
 A_UINT32 devNum, 
 A_UINT32 waitTime, 
 A_UINT32 timeout, 
 A_UINT32 remoteStats,
 A_UINT32 enableCompare,
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength
)
{
	ISR_EVENT	event;
	A_UINT32	i, statsLoop;
	A_BOOL		txComplete = FALSE;
	A_BOOL		rxComplete = FALSE;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32	numDesc = 0;
	RX_STATS_TEMP_INFO	rxStatsInfo;
	A_BOOL		statsSent = FALSE;
	A_UINT32 start, finish;
//	A_UINT32	status1, status2;
	A_BOOL		lastPktReceived = FALSE;
	A_BOOL		statsPktReceived = FALSE;
	A_UINT16	queueIndex = pLibDev->selQueueIndex;
	A_UINT32 startTime;
	A_UINT32 curTime;
	A_BOOL		skipStats = FALSE;
	A_UINT16     numStatsToSkip = 0;		
	A_BOOL      falconTrue = FALSE;
	A_UINT32    ppm_data_padding = PPM_DATA_SIZE;
	A_UINT32    status1_offset, status2_offset, status3_offset; // legacy status1 - datalen/rate, 
	                                                            // legacy status2 - done/error/timestamp
	                                                            // new with falcon status3 - rssi for 4 ant
#ifdef _TIME_PROFILE
    A_UINT32 txrxDataBegin_start, txrxDataBegin_end;
    txrxDataBegin_start=milliTime();
printf("txrxDataBegin_start::TIME-:::%u:%u\n", txrxDataBegin_start, milliTime());
#endif


	falconTrue = isFalcon(devNum) || isDragon(devNum);

	if (falconTrue) {
		status1_offset = FIRST_FALCON_RX_STATUS_WORD;
		status2_offset = SECOND_FALCON_RX_STATUS_WORD;
		status3_offset = FALCON_ANT_RSSI_RX_STATUS_WORD;
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} else {
		status1_offset = FIRST_STATUS_WORD;
		status2_offset = SECOND_STATUS_WORD;
		status3_offset = status2_offset; // to avoid illegal value. should never be used besides multAnt chips
	}

	timeout = timeout * 10;

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:txrxDataBegin\n", devNum);
		return;
	}

	if(pLibDev->devState < RESET_STATE) {
		mError(devNum, EILSEQ, "Device Number %d:txrxDataBegin: device not in reset state - resetDevice must be run first\n", devNum);
		return;
	}

	if (!pLibDev->rx.rxEnable || !pLibDev->tx[queueIndex].txEnable) {
		mError(devNum, EILSEQ, 
			"Device Number %d:txrxDataBegin: txDataSetup and rxDataSetup must complete before running txrxDataBegin\n", devNum);
		return; 
	}
	
	//zero out the stats structure
	memset(&(pLibDev->tx[queueIndex].txStats[0]), 0, sizeof(TX_STATS_STRUCT) * STATS_BINS);
	pLibDev->tx[queueIndex].haveStats = 0;
	memset(&(pLibDev->rx.rxStats[0][0]), 0, sizeof(RX_STATS_STRUCT) * STATS_BINS * MAX_TX_QUEUE);
	pLibDev->rx.haveStats = 0;
	if(remoteStats & ENABLE_STATS_RECEIVE) {
		memset(&pLibDev->txRemoteStats[0], 0, sizeof(TX_STATS_STRUCT) * STATS_BINS);	
		memset(&(pLibDev->rxRemoteStats[0][0]), 0, sizeof(RX_STATS_STRUCT) * STATS_BINS * MAX_TX_QUEUE);	
	}

	// start receive first
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxBeginConfig(devNum);

#ifdef DEBUG_MEMORY
printf("SNOOP::waitTime for receive=%d\n", waitTime);
printf("SNOOP::RX desc contents of %X\n", pLibDev->rx.descAddress);
memDisplay(devNum, pLibDev->rx.descAddress, 12);
pLibDev->devMap.OSmemRead(devNum, (pLibDev->rx.descAddress+4), (A_UCHAR*)&buf_ptr, sizeof(buf_ptr));
printf("SNOOP::RX desc pointed frame contents at %X\n", buf_ptr);
memDisplay(devNum, buf_ptr, 30);
#endif

	//wait for event
	startTime = milliTime();
	event.valid = 0;
	event.ISRValue = 0;
	for (i = 0; i < waitTime; i++)
	{
		event = pLibDev->devMap.getISREvent(devNum);
		if (event.valid) {
			if(ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->isRxdescEvent(event.ISRValue)) {
				break;
			}
		}
		curTime = milliTime();
		if (curTime > (startTime+waitTime)) {
			i = waitTime;
			break;
		}
		mSleep(1);		
	}

	
	if ((i == waitTime) && (waitTime !=0)) {
		mError(devNum, EIO, "Device Number %d:txrxDataBegin: nothing received within %d millisecs (waitTime)\n", devNum, waitTime);
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
		return;
	}

	i = 0;
	memset(&rxStatsInfo, 0, sizeof(RX_STATS_TEMP_INFO));

	if(remoteStats & SKIP_SOME_STATS) {
		//extract and set number to skip
		skipStats = 1;
		numStatsToSkip = (A_UINT16)((remoteStats & NUM_TO_SKIP_M) >> NUM_TO_SKIP_S);
	}

	for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) {
		rxStatsInfo.sigStrength[statsLoop].rxMinSigStrength = 127;
	}

	if(pLibDev->rx.enablePPM) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) {
				pLibDev->rx.rxStats[i][statsLoop].ppmMax = -1000;
				pLibDev->rx.rxStats[i][statsLoop].ppmMin = 1000;
			}
		}
	}

	rxStatsInfo.descToRead = pLibDev->rx.descAddress;

	if (enableCompare) {
		//create a max size compare buffer for easy compare
		rxStatsInfo.pCompareData = (A_UCHAR *)malloc(FRAME_BODY_SIZE);

		if(!rxStatsInfo.pCompareData) {
			mError(devNum, ENOMEM, "Device Number %d:txrxDataBegin: Unable to allocate memory for compare buffer\n", devNum);
			ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);
			return;
		}

		fillCompareBuffer(rxStatsInfo.pCompareData, FRAME_BODY_SIZE, dataPattern, dataPatternLength);
	}	

	//start transmit
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->txBeginConfig(devNum, 1);
	start= milliTime();

	startTime = milliTime();
	do {
		if (!txComplete) {
			event = pLibDev->devMap.getISREvent(devNum);
		}

		//check for transmit
		if ((event.valid) && 
			(ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->isTxdescEvent(event.ISRValue)) && 
			!txComplete) {

			finish= milliTime();



#ifdef DEBUG_MEMORY
printf("SNOOP::txrxDataBegin::Got txdesc event\n");
printf("SNOOP::Status of frames sent \n");
printf("SNOOP::txrxDataBegin::TXDP=%x:TXE=%x\n", REGR(devNum, 0x800), REGR(devNum, 0x840));
printf("SNOOP::txrxDataBegin::Memory at TXDP %x location is \n", REGR(devNum, 0x800));
memDisplay(devNum, REGR(devNum, 0x800), 12);
printf("SNOOP::txrxDataBegin::Frame contents at %x pointed location is \n", REGR(devNum, 0x800));
pLibDev->devMap.OSmemRead(devNum, REGR(devNum, 0x800)+4, (A_UCHAR*)&buf_ptr, sizeof(buf_ptr));
memDisplay(devNum, buf_ptr, 30);
printf("SNOOP::txrxDataBegin::Memory at baseaddress %x location is \n", pLibDev->tx[0].descAddress);
memDisplay(devNum, pLibDev->tx[0].descAddress, 12);
printf("SNOOP::txrxDataBegin::Frame contents at %x pointed location is \n", pLibDev->tx[0].descAddress);
pLibDev->devMap.OSmemRead(devNum, (pLibDev->tx[0].descAddress+4), (A_UCHAR*)&buf_ptr, sizeof(buf_ptr));
memDisplay(devNum, buf_ptr, 30);
printf("SNOOP::Register contents are\n");
printf("reg8000=%x:reg8004=%x:reg8008=%x:reg800c=%x:reg8014=%x\n", REGR(devNum, 0x8000), REGR(devNum, 0x8004), REGR(devNum, 0x8008), REGR(devNum, 0x800c), REGR(devNum, 0x8014));
#endif



			//ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->sendTxEndPacket(devNum, queueIndex );
			sendEndPacket(devNum, queueIndex);

			if(pLibDev->libCfgParams.enableXR) {
				REGW(devNum, XRMODE_REG, REGR(devNum, XRMODE_REG) | (0x1 << BITS_TO_XR_WAIT_FOR_POLL));
			}

			txComplete = TRUE;
			txAccumulateStats(devNum, finish - start, queueIndex);
		}

		//read descriptor status words
		fillRxDescAndFrame(devNum, &rxStatsInfo);
/*  Siva
		pLibDev->devMap.OSmemRead(devNum, rxStatsInfo.descToRead + status2_offset, 
					(A_UCHAR *)&rxStatsInfo.status2, sizeof(rxStatsInfo.status2));
*/

		if (rxStatsInfo.status2 & DESC_DONE) {
/*
			pLibDev->devMap.OSmemRead(devNum, rxStatsInfo.descToRead + status1_offset, 
					(A_UCHAR *)&rxStatsInfo.status1, sizeof(rxStatsInfo.status1));
			pLibDev->devMap.OSmemRead(devNum, rxStatsInfo.descToRead + BUFFER_POINTER, 
					(A_UCHAR *)&rxStatsInfo.bufferAddr, sizeof(rxStatsInfo.bufferAddr));
*/
			rxStatsInfo.descRate = descRate2bin((rxStatsInfo.status1 >> BITS_TO_RX_DATA_RATE) & pLibDev->rxDataRateMsk);

			if (falconTrue) {
				rxStatsInfo.bufferAddr &= FALCON_DESC_ADDR_MASK;
			}

			// Initialize loop variables
			rxStatsInfo.badPacket = 0;
			rxStatsInfo.gotHeaderInfo = FALSE; // TODO: FIX for multi buffer packet support
			rxStatsInfo.illegalBuffSize = 0;
			rxStatsInfo.controlFrameReceived = 0;
			rxStatsInfo.mdkPktType = 0;
			//reset our timeout counter
			i = 0;

			// Increase buffer address for PPM
			if(pLibDev->rx.enablePPM) {
				rxStatsInfo.bufferAddr += ppm_data_padding;
			}
						
			if (numDesc == pLibDev->rx.numDesc - 1) {
				//This is the final looped rx desc and done bit is set, we ran out of receive descriptors, 
				//so return with an error
				mError(devNum, ENOMEM, "Device Number %d:txrxDataBegin:  ran out of receive descriptors\n", devNum);
				break;
			}
			mdkExtractAddrAndSequence(devNum, &rxStatsInfo);

#ifdef DEBUG_MEMORY
printf("SNOOP::rxStatsInfo structure\n");
printf("SNOOP::descToRead=%X\n", rxStatsInfo.descToRead);
printf("SNOOP::descRate=%X\n", rxStatsInfo.descRate);
printf("SNOOP::status1=%X\n", rxStatsInfo.status1);
printf("SNOOP::status2=%X\n", rxStatsInfo.status2);
printf("SNOOP::bufferAddr=%X\n", rxStatsInfo.bufferAddr);
printf("SNOOP::totalBytes=%X\n", rxStatsInfo.totalBytes);
printf("SNOOP::mdkPktType=%X\n", rxStatsInfo.mdkPktType);
printf("SNOOP::frame_info_len=%X\n", rxStatsInfo.frame_info_len);
printf("SNOOP::ppm_data_len=%X\n", rxStatsInfo.ppm_data_len);
printf("SNOOP::RX desc contents of %X\n", pLibDev->rx.descAddress);
memDisplay(devNum, pLibDev->rx.descAddress, 12);
pLibDev->devMap.OSmemRead(devNum, (pLibDev->rx.descAddress+4), (A_UCHAR*)&buf_ptr, sizeof(buf_ptr));
printf("SNOOP::RX desc pointed frame contents at %X\n", buf_ptr);
memDisplay(devNum, buf_ptr, 30);
#endif

			//only process packets if things are good
			if ( !((rxStatsInfo.status1 & DESC_MORE) || rxStatsInfo.badPacket || rxStatsInfo.controlFrameReceived || 
				rxStatsInfo.illegalBuffSize) ) {
				
				//check for this being "last packet" or stats packet
				//mdkExtractAddrAndSequence also pulled mdkPkt type info from packet
				if ((rxStatsInfo.status2 & DESC_FRM_RCV_OK) 
				 && (rxStatsInfo.mdkPktType == MDK_LAST_PKT)){
					lastPktReceived = TRUE;
					
					//if were not expecting remote stats then we are done
					if (!(remoteStats & ENABLE_STATS_RECEIVE)) {
						rxComplete = TRUE;
					}

					//we have done with receive so can send stats
					if((remoteStats & ENABLE_STATS_SEND) && txComplete) {
						for (i = 0; i < 1 /*MAX_TX_QUEUE*/; i++ )
						{
							for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) {
								// Need to check if we have transmit OR receive stats for this rate
								if((pLibDev->rx.rxStats[i][statsLoop].crcPackets > 0) ||
									(pLibDev->rx.rxStats[i][statsLoop].goodPackets > 0) ||
									(pLibDev->tx[queueIndex].txStats[statsLoop].excessiveRetries > 0) ||
									(pLibDev->tx[queueIndex].txStats[statsLoop].goodPackets > 0)) {
									sendStatsPkt(devNum, statsLoop, MDK_TXRX_STATS_PKT, pLibDev->tx[queueIndex].destAddr.octets);
								}
							}
							sendStatsPkt(devNum, 0, MDK_TXRX_STATS_PKT, pLibDev->tx[queueIndex].destAddr.octets);
							statsSent = TRUE;
						}
					}
				}
				else if((rxStatsInfo.status2 & DESC_FRM_RCV_OK) 
					 && (rxStatsInfo.mdkPktType >= MDK_TX_STATS_PKT)
					 && (rxStatsInfo.mdkPktType <= MDK_TXRX_STATS_PKT)) {
					statsPktReceived = mdkExtractRemoteStats(devNum, &rxStatsInfo);
					rxComplete = statsPktReceived;
				}
				else if (rxStatsInfo.mdkPktType == MDK_NORMAL_PKT){
					if (enableCompare) {
						comparePktData(devNum, &rxStatsInfo);
					}
					if (pLibDev->rx.enablePPM) {
						extractPPM(devNum, &rxStatsInfo);
					}

					if(skipStats && (numStatsToSkip != 0)) {
						numStatsToSkip --;
					}
					else {
						mdkExtractRxStats(devNum, &rxStatsInfo);
					}

				}
				else {
					if(rxStatsInfo.status2 & DESC_FRM_RCV_OK) {
						mError(devNum, EIO, "Device Number %d:A good matching packet with an unknown MDK_PKT type detected MDK_PKT: %d\n", devNum, 
							rxStatsInfo.mdkPktType);
						{
							A_UINT32 iIndex;
							printf("Frame Info\n");
							for(iIndex=0; iIndex<rxStatsInfo.frame_info_len; iIndex++) {
								printf("%X ", rxStatsInfo.frame_info[iIndex]);
							}
							printf("\n");
							printf("PPM Data Info\n");
							for(iIndex=0; iIndex<rxStatsInfo.ppm_data_len; iIndex++) {
								printf("%X ", rxStatsInfo.ppm_data[iIndex]);
							}
							printf("\n");
						}
					}
					if (rxStatsInfo.status2 & DESC_CRC_ERR)
					{
						pLibDev->rx.rxStats[rxStatsInfo.qcuIndex][0].crcPackets++;
						pLibDev->rx.rxStats[rxStatsInfo.qcuIndex][rxStatsInfo.descRate].crcPackets++;
						pLibDev->rx.haveStats = 1;
					}
					else if (rxStatsInfo.status2 & pLibDev->decryptErrMsk)
					{
						pLibDev->rx.rxStats[rxStatsInfo.qcuIndex][0].decrypErrors++;
						pLibDev->rx.rxStats[rxStatsInfo.qcuIndex][rxStatsInfo.descRate].decrypErrors++;
						pLibDev->rx.haveStats = 1;
					}

				}
			}

			if(txComplete && rxComplete) {
				break;
			}

			//get next descriptor to process
			rxStatsInfo.descToRead += sizeof(MDK_ATHEROS_DESC);
			numDesc ++;
		}
		else {
			curTime = milliTime();
			if (curTime > (startTime+timeout)) {
				i = timeout;
				break;
			}
			mSleep(1);
			i++;
		}
	} while (i < timeout);

	if (i == timeout)
	{
		if (!txComplete) {
			mError(devNum, EIO, "Device Number %d:txrxDataBegin: timeout reached before all frames transmitted\n", devNum);
		}
		else {
			mError(devNum, EIO, 
			"Device Number %d:txrxDataBegin: timeout reached, without receiving all packets.  Number received = %lu\n", devNum,
			numDesc);

			if (!lastPktReceived) {
				mError(devNum, EIO, 
				"Device Number %d:txrxDataBegin: timeout reached, without receiving last packet\n", devNum);
			}

			if (!statsPktReceived && (remoteStats == ENABLE_STATS_RECEIVE)) {
				mError(devNum, EIO, 
				"Device Number %d:txrxDataBegin: timeout reached, without receiving stats packet\n", devNum);
			} 
			   
		}
	}



	if((remoteStats & ENABLE_STATS_SEND) && !statsSent) {
		for( i = 0; i < MAX_TX_QUEUE; i++ )
		{
			for(statsLoop = 1; statsLoop < STATS_BINS; statsLoop++) {
				// Need to check if we have transmit OR receive stats for this rate
				if((pLibDev->rx.rxStats[i][statsLoop].crcPackets > 0) ||
					(pLibDev->rx.rxStats[i][statsLoop].goodPackets > 0) ||
					(pLibDev->tx[queueIndex].txStats[statsLoop].excessiveRetries > 0) ||
					(pLibDev->tx[queueIndex].txStats[statsLoop].goodPackets > 0)) {
					sendStatsPkt(devNum, statsLoop, MDK_TXRX_STATS_PKT, pLibDev->tx[queueIndex].destAddr.octets);
				}
			}
			sendStatsPkt(devNum, 0, MDK_TXRX_STATS_PKT, pLibDev->tx[queueIndex].destAddr.octets);
		}
	}

	//cleanup
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->txCleanupConfig(devNum);
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->rxCleanupConfig(devNum);

	//write back the retry value
	//REGW(devNum, F2_RETRY_LMT, pLibDev->tx[queueIndex].retryValue);
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setRetryLimit( devNum, 0 );	//__TODO__

	if(enableCompare) {
		free(rxStatsInfo.pCompareData);
	}

#ifdef _TIME_PROFILE
    txrxDataBegin_end=milliTime();
printf(".................SNOOP::exit txrxDataBegin:Time taken = %dms\n", (txrxDataBegin_end-txrxDataBegin_start));
#endif

	return;
}

/**************************************************************************
* txGetStats - Read and return the tx stats values
*
* remote: set to 1 if want to get stats that were sent from a remote stn
*/
MANLIB_API void txGetStats
(
 A_UINT32 devNum, 
 A_UINT32 rateInMb,
 A_UINT32 remote,
 TX_STATS_STRUCT *pReturnStats 
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT16	queueIndex = pLibDev->selQueueIndex;
	A_UINT32  rateBinIndex = rate2bin(rateInMb);
	
	memset(pReturnStats, 0, sizeof(TX_STATS_STRUCT));

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:txGetStats\n", devNum);
		return;
	}
//	if ( (rateInMb > 54) || ((rateBinIndex == 0) && (rateInMb)) ) {
//		mError(devNum, EINVAL, "Device Number %d:txGetStats: Invalid rateInMb value %x - use 0 for all rates\n", devNum,
//			rateInMb);
//		return;
//	}
	if (remote) {
		//check to see if we received remote stats
		if ((pLibDev->remoteStats == MDK_TX_STATS_PKT) || 
			(pLibDev->remoteStats == MDK_TXRX_STATS_PKT)) {
			memcpy(pReturnStats, &(pLibDev->txRemoteStats[rateBinIndex]), sizeof(TX_STATS_STRUCT)); 
		}
		else {
			mError(devNum, EIO, "Device Number %d:txGetStats: no remote stats were received\n", devNum);
			return;
		}
	}
	else {
		if(pLibDev->tx[queueIndex].haveStats) {
			memcpy(pReturnStats, &(pLibDev->tx[queueIndex].txStats[rateBinIndex]), sizeof(TX_STATS_STRUCT)); 
		}
		else {
			mError(devNum, EIO, "Device Number %d:txGetStats: no tx stats have been collected\n", devNum);
			return;
		}

	}
	return;
}

/**************************************************************************
* rxGetStats - Read and return the rx stats values
*
* remote: set to 1 if want to get stats that were sent from a remote stn
*/
MANLIB_API void rxGetStats
(
 A_UINT32 devNum,
 A_UINT32 rateInMb,
 A_UINT32 remote,
 RX_STATS_STRUCT *pReturnStats 
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 rateBinIndex = rate2bin(rateInMb);
#ifdef _TIME_PROFILE
    A_UINT32 rxGetStats_start, rxGetStats_end;
    rxGetStats_start=milliTime();
#endif

	//work around for venice to get to the correct CCK rate bins
	if(((pLibDev->swDevID & 0xff) >= 0x0013) && (pLibDev->mode == MODE_11B)){
		if((rateBinIndex > 0) && (rateBinIndex < 9)) {
			if(rateBinIndex == 1){
				rateBinIndex += 8;
			}
			else {
				rateBinIndex += 7;
			}
		}
	}

	memset(pReturnStats, 0, sizeof(RX_STATS_STRUCT));

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:rxGetStats\n", devNum);
		return;
	}
	if ( /*(rateInMb > 54) ||*/ ((rateBinIndex == 0) && (rateInMb)) ) {
		mError(devNum, EINVAL, "Device Number %d:rxGetStats: Invalid rateInMb value %d - use 0 for all rates\n", devNum,
			rateInMb);
		return;
	}

	if (remote) {
		//check to see if we received remote stats
		if ((pLibDev->remoteStats == MDK_RX_STATS_PKT) || 
			(pLibDev->remoteStats == MDK_TXRX_STATS_PKT)) {
			memcpy(pReturnStats, &(pLibDev->rxRemoteStats[0][rateBinIndex]), sizeof(RX_STATS_STRUCT)); 
		}
		else {
			mError(devNum, EIO, "Device Number %d:rxGetStats: no remote stats were received\n", devNum);
			return;
		}
	}
	else {
		if(pLibDev->rx.haveStats) { // __TODO__ 
			memcpy(pReturnStats, &(pLibDev->rx.rxStats[0][rateBinIndex]), sizeof(RX_STATS_STRUCT)); 
		}
		else {
			mError(devNum, EIO, "Device Number %d:rxGetStats: no rx stats have been collected\n", devNum);
			return;
		}
	}
#ifdef _TIME_PROFILE
    rxGetStats_end=milliTime();
printf(".................SNOOP::exit rxGetStats:Time taken = %dms\n", (rxGetStats_end-rxGetStats_start));
#endif
	return;
}

/**************************************************************************
* rxGetData - Get data from a particular descriptor
*
* 
*/
MANLIB_API void rxGetData
(
 A_UINT32 devNum, 
 A_UINT32 bufferNum, 
 A_UCHAR *pReturnBuffer, 
 A_UINT32 sizeBuffer
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 readSize, readAddr;
	A_UINT32 status;
	A_UINT32 descAddr;

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:rxGetData\n", devNum);
		return;
	}
	if(pLibDev->devState < RESET_STATE) {
		mError(devNum, EILSEQ, "Device Number %d:rxGetData: device not out of reset state - resetDevice must be run first\n", devNum);
	return;
	}
	if(pLibDev->rx.bufferAddress == 0) {
		mError(devNum, EILSEQ, "Device Number %d:rxGetData: receive buffer is empty - no valid data to read\n", devNum);
		return;
	}
	if(bufferNum > pLibDev->rx.numDesc) {
		mError(devNum, EINVAL, "Device Number %d:rxGetData: Asking for a buffer beyond the number setup for receive\n", devNum);
		return;
	}

	readSize = A_MIN(pLibDev->rx.bufferSize, sizeBuffer);
	readAddr = pLibDev->rx.bufferAddress + (pLibDev->rx.bufferSize * bufferNum);
	//read only if the descriptor is good, otherwise, move onto next descriptor
	descAddr = pLibDev->rx.descAddress + (sizeof(MDK_ATHEROS_DESC) * bufferNum);
	while(bufferNum < pLibDev->rx.numDesc) {
		pLibDev->devMap.OSmemRead(devNum, descAddr + SECOND_STATUS_WORD, 
					(A_UCHAR *)&status, sizeof(status));

		if (status & DESC_DONE) {
			if(status & DESC_FRM_RCV_OK) { 		
				pLibDev->devMap.OSmemRead(devNum, readAddr, pReturnBuffer, readSize);
				return;
			}
		} else {
			//found a descriptor that's not done, assume all others are same
			mError(devNum, EIO, "Device Number %d:rxGetData: Found descriptor not done before valid receive descriptor", devNum);
			return;
		}
		descAddr += sizeof(MDK_ATHEROS_DESC);
		readAddr += pLibDev->rx.bufferSize;
		bufferNum++;
	}
	//if got to here then ran out of descriptors
	mError(devNum, EIO, "Device Number %d:rxGetData: Unable to find a descriptor with RX_OK\n", devNum);
}


/**************************************************************************
* createTransmitPacket - create packet for transmission
*
*/
void createTransmitPacket
(
 A_UINT32 devNum, 
 A_UINT16 mdkType,
 A_UCHAR *dest, 
 A_UINT32 numDesc,
 A_UINT32 dataBodyLength, 
 A_UCHAR *dataPattern, 
 A_UINT32 dataPatternLength, 
 A_UINT32 broadcast,
 A_UINT16 queueIndex,			// QCU index
 A_UINT32 *pPktSize,			//return size
 A_UINT32 *pPktAddr 			//return address
)
{
	WLAN_DATA_MAC_HEADER3  *pPktHeader;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 amountToWrite; 	
	A_UCHAR broadcastAddr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	MDK_PACKET_HEADER *pMdkHeader;
	A_UCHAR *pTransmitPkt;
	A_UCHAR *pPkt;
	A_UINT32 *pIV;
	
	//allocate enough memory for the packet
	*pPktSize = dataBodyLength;
	
	if (!pLibDev->specialTx100Pkt) {
		*pPktSize += sizeof(WLAN_DATA_MAC_HEADER3) + sizeof(MDK_PACKET_HEADER);
		if(pLibDev->wepEnable && (mdkType == MDK_NORMAL_PKT)) {
			*pPktSize += (WEP_IV_FIELD + 2);
		}
	}

	*pPktAddr = memAlloc(devNum, *pPktSize);
	if (0 == *pPktAddr) {
		mError(devNum, ENOMEM, "Device Number %d:createTransmitPacket: unable to allocate memory for transmit buffer\n", devNum);
		return;
	}

	//allocate local memory to create the packet in, so only do one block write of packet
	pTransmitPkt = pPkt = (A_UCHAR *)malloc(*pPktSize);
	if(!pPkt) {
		mError(devNum, ENOMEM, "Device Number %d:Unable to allocate local memory for packet\n", devNum);
		return;
	}

	pPktHeader = (WLAN_DATA_MAC_HEADER3 *)pPkt;

	//create the packet Header, all fields are listed here to enable easy changing	
	pPktHeader->frameControl.protoVer = 0;
	pPktHeader->frameControl.fType = FRAME_DATA;
	pPktHeader->frameControl.fSubtype = SUBT_DATA;
	pPktHeader->frameControl.ToDS = 0;
	pPktHeader->frameControl.FromDS = 0;
	pPktHeader->frameControl.moreFrag = 0;
	pPktHeader->frameControl.retry = 0;
	pPktHeader->frameControl.pwrMgt = 0;
	pPktHeader->frameControl.moreData = 0;
	pPktHeader->frameControl.wep = ((pLibDev->wepEnable) && (mdkType == MDK_NORMAL_PKT)) ? 1 : 0;
	pPktHeader->frameControl.order = 0;
// Swap the bytes of the frameControl
#if 0
//#ifdef ARCH_BIG_ENDIAN
	{
		A_UINT16* ptr;
	
		ptr = (A_UINT16 *)(&(pPktHeader->frameControl));
		*ptr = btol_s(*ptr);
	}
#endif
//	pPktHeader->durationNav.clearBit = 0;
//	pPktHeader->durationNav.duration = 0;
	pPktHeader->durationNav = 0;  // aman redefined the struct to be just A_UINT16
// Swap the bytes of the durationNav
#if 0
//#ifdef ARCH_BIG_ENDIAN
	{
		A_UINT16* ptr;

		ptr = (A_UINT16 *)(&(pPktHeader->durationNav));	
		*ptr = btol_s(*ptr);
	}
#endif
	memcpy(pPktHeader->address1.octets, broadcast ? broadcastAddr : dest, WLAN_MAC_ADDR_SIZE);
	memcpy(pPktHeader->address2.octets, pLibDev->macAddr.octets, WLAN_MAC_ADDR_SIZE);
	memcpy(pPktHeader->address3.octets, pLibDev->bssAddr.octets, WLAN_MAC_ADDR_SIZE);
//printf("SNOOP:: frame with\n");
//if (!broadcast) printf("SNOOP::dest %x:%x:%x:%x:%x:%x\n", dest[6], dest[5], dest[4], dest[3], dest[2], dest[1], dest[0]);
//printf("SNOOP::macaddr %x:%x:%x:%x:%x:%x\n", pLibDev->macAddr.octets[6], pLibDev->macAddr.octets[5], pLibDev->macAddr.octets[4], pLibDev->macAddr.octets[3], pLibDev->macAddr.octets[2], pLibDev->macAddr.octets[1], pLibDev->macAddr.octets[0]);
//printf("SNOOP::bssAddr %x:%x:%x:%x:%x:%x\n", pLibDev->bssAddr.octets[6], pLibDev->bssAddr.octets[5], pLibDev->bssAddr.octets[4], pLibDev->bssAddr.octets[3], pLibDev->bssAddr.octets[2], pLibDev->bssAddr.octets[1], pLibDev->bssAddr.octets[0]);
	WLAN_SET_FRAGNUM(pPktHeader->seqControl, 0);
	WLAN_SET_SEQNUM(pPktHeader->seqControl, 0xfed);
#if 0
//#ifdef ARCH_BIG_ENDIAN
	{
		A_UINT16* ptr;
	
		ptr = (A_UINT16 *)(&(pPktHeader->seqControl));
		*ptr = btol_s(*ptr);
	}
#endif




	//fill in the packet body, if special Tx100 packet, don't move on the pointer
	if(!pLibDev->specialTx100Pkt) {
		pPkt += sizeof(WLAN_DATA_MAC_HEADER3);
	}

	//fill in the IV if required
	if (pLibDev->wepEnable && (mdkType == MDK_NORMAL_PKT)) {
		pIV = (A_UINT32 *)pPkt;

		*pIV = 0;
//		  *pIV = (pLibDev->wepKey << 30) | 0x123456;  //fixed IV for now
		*pIV = 0xffffffff;
		pPkt += 4;
		*pPkt = 0xff;
		pPkt++;
		if (pLibDev->wepKey > 3) {
			*pPkt = 0x0;
		}
		else {
			*pPkt = (A_UCHAR)(pLibDev->wepKey << 6);
		}
		pPkt++;
	}

	//fill in packet type
	pMdkHeader = (MDK_PACKET_HEADER *)pPkt;
#ifdef ARCH_BIG_ENDIAN
	pMdkHeader->pktType = btol_s(mdkType);
	pMdkHeader->numPackets = btol_l(numDesc);
#else
	pMdkHeader->pktType = mdkType;
	pMdkHeader->numPackets = numDesc;
#endif
	//pMdkHeader->qcuIndex = queueIndex;		// __TODO__

	//fill in the repeating pattern, if special Tx100 packet, don't move on the pointer
	if(!pLibDev->specialTx100Pkt) {
		pPkt += sizeof(MDK_PACKET_HEADER);
	}
	while (dataBodyLength) {
		if(dataBodyLength > dataPatternLength) {
			amountToWrite = dataPatternLength;
			dataBodyLength -= dataPatternLength;
		} 
		else {
			amountToWrite = dataBodyLength;
			dataBodyLength = 0;
		}

		memcpy(pPkt, dataPattern, amountToWrite);
		pPkt += amountToWrite;
	}

	/* write the packet to physical memory
	 * need to check to see if the packet will be greater than 2000 bytes if so
	 * need to perform the write in 2 steps
	 */
	if (*pPktSize > 2000) {
		pLibDev->devMap.OSmemWrite(devNum, *pPktAddr, (A_UCHAR *)pTransmitPkt, 2000);
		if (*pPktSize > 4000) {
			pLibDev->devMap.OSmemWrite(devNum, (*pPktAddr + 2000), (A_UCHAR *)(pTransmitPkt + 2000), 2000); 	   
			pLibDev->devMap.OSmemWrite(devNum, (*pPktAddr + 4000), (A_UCHAR *)(pTransmitPkt + 4000), *pPktSize - 4000); 
		} else {
			pLibDev->devMap.OSmemWrite(devNum, (*pPktAddr + 2000), (A_UCHAR *)(pTransmitPkt + 2000), *pPktSize - 2000); 	   
		}
	}
	else {
		pLibDev->devMap.OSmemWrite(devNum, *pPktAddr, (A_UCHAR *)pTransmitPkt, *pPktSize);
	}
#ifdef DEBUG_MEMORY
printf("SNOOP::Frame contents are\n");
memDisplay(devNum, *pPktAddr, 20);
#endif

	//free the local memory
	free(pTransmitPkt);
	queueIndex = 0; //this is not used, quieting warnings
	return;
}

/**************************************************************************
* writeDescriptor - write descriptor to physical memory
*
*/

/*  Old Definition
void writeDescriptor
(
 A_UINT32	devNum,
 A_UINT32	descAddress,
 MDK_ATHEROS_DESC *pDesc
)
{
	A_UINT32 ii;

	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	//zero out the status values
	if (isFalcon(devNum)) {
		pDesc->hwExtraBuffer[0] = 0;
		pDesc->hwExtraBuffer[1] = 0;
		pDesc->hwExtraBuffer[2] = 0;
	} else if((pLibDev->swDevID & 0x00ff) >= 0x13) {
		pDesc->hwExtra[0] = 0;
		pDesc->hwExtra[1] = 0;
	} else {
		pDesc->hwStatus[0] = 0;
		pDesc->hwStatus[1] = 0;
	}

	pLibDev->devMap.OSmemWrite(devNum, descAddress, (A_UCHAR *)pDesc, sizeof(MDK_ATHEROS_DESC));
}
*/

/**************************************************************************
* calculateRateThroughput - For the given rate, calculate throughput
*
*/
void calculateRateThroughput
(
 A_UINT32 devNum,
 A_UINT32 descRate, 
 A_UINT32 queueIndex
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 descStartTime;
	A_UINT32 descEndTime;
	A_UINT32 bitsReceived;
	float newTxTime = 0;
	
	descStartTime = pLibDev->tx[queueIndex].txStats[descRate].startTime;
	descEndTime = pLibDev->tx[queueIndex].txStats[descRate].endTime;


	if(descStartTime < descEndTime) {
		bitsReceived = (pLibDev->tx[queueIndex].txStats[descRate].goodPackets - pLibDev->tx[queueIndex].txStats[descRate].firstPktGood) \
			* (pLibDev->tx[queueIndex].dataBodyLen + sizeof(MDK_PACKET_HEADER)) * 8;
		newTxTime = (float)((descEndTime - descStartTime) * 1024)/1000;  //puts the time in millisecs
		pLibDev->tx[queueIndex].txStats[descRate].newThroughput = (A_UINT32)((float)bitsReceived / newTxTime);
	}
}

/**************************************************************************
* txAccumulateStats - walk through all descriptors and accumulate the stats
*
*/
void txAccumulateStats
(
 A_UINT32 devNum,
 A_UINT32 txTime,
 A_UINT16 queueIndex
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
#ifdef _TIME_PROFILE
    A_UINT32 tasStart, tasEnd;

    tasStart=milliTime();
#endif
//	TX_STATS_TEMP_INFO	statsInfo;
//	A_UINT32 i, statsLoop;
//	A_UINT32 bitsReceived;
//	A_UINT32 descStartTime = 0;
//	A_UINT32 descTempTime = 0;
//	A_UINT32 descMidTime = 0;
//	A_UINT32 descEndTime = 0;
//	float    newTxTime = 0;
//	A_UINT16 firstPktGood = 0;
//	A_UINT32 lastDescRate = 0;




#ifdef DEBUG_MEMORY
printf("SNOOP::Collecting stats:txTime=%d\n", txTime);
#endif
/*********************** moved to a new function 'fill_txStats' *****
 ************************* by Siva ***************************

	// traverse the descriptor list and get the stats from each one
	memset(&statsInfo, 0, sizeof(TX_STATS_TEMP_INFO));

	for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) {
		statsInfo.sigStrength[statsLoop].rxMinSigStrength = 127;
	}

	statsInfo.descToRead = pLibDev->tx[queueIndex].descAddress;
	for (i = 0; i < pLibDev->tx[queueIndex].numDesc; i++) {
		statsInfo.descRate = ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->txGetDescRate(devNum, statsInfo.descToRead);
//		pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + FIRST_CONTROL_WORD, 
//						(A_UCHAR *)&(statsInfo.descRate), sizeof(statsInfo.descRate));
//		statsInfo.descRate = (statsInfo.descRate >> BITS_TO_TX_XMIT_RATE) & 0xF;
		statsInfo.descRate = descRate2bin(statsInfo.descRate);
		pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + pLibDev->txDescStatus1, 
					(A_UCHAR *)&(statsInfo.status1), sizeof(statsInfo.status1));
		pLibDev->devMap.OSmemRead(devNum, statsInfo.descToRead + pLibDev->txDescStatus2, 
						(A_UCHAR *)&(statsInfo.status2), sizeof(statsInfo.status2));

		if (statsInfo.status2 & DESC_DONE) {
	
			mdkExtractTxStats(devNum, &statsInfo, queueIndex);
			if(i == 0) {
				descStartTime = (statsInfo.status1 >> 16) & 0xffff;
				descMidTime = descStartTime;
				if(statsInfo.status1 & DESC_FRM_XMIT_OK) {
					firstPktGood = 1; //need to remove the 1st packet from new throughput calculation
					pLibDev->tx[queueIndex].txStats[statsInfo.descRate].firstPktGood = 1;
				}

				//update the rate start time
				pLibDev->tx[queueIndex].txStats[statsInfo.descRate].startTime = descStartTime;
				pLibDev->tx[queueIndex].txStats[statsInfo.descRate].endTime = descStartTime;
			}
			else if ( i == pLibDev->tx[queueIndex].numDesc - 1) {
				descEndTime = descEndTime + ((statsInfo.status1 >> 16) & 0xffff);
				if(descEndTime < descMidTime) {
					descEndTime += 0x10000;
				}
				//update the end time for the current rate
				pLibDev->tx[queueIndex].txStats[statsInfo.descRate].endTime = descEndTime;
				calculateRateThroughput(devNum, lastDescRate, queueIndex);
			}
			else {
				descTempTime = (statsInfo.status1 >> 16) & 0xffff;
				if (descTempTime < descMidTime) {
					descMidTime = 0x10000 + descTempTime;
				}
				else {
					descMidTime = descTempTime;
				}
				pLibDev->tx[queueIndex].txStats[statsInfo.descRate].endTime = descTempTime;
				if(lastDescRate != statsInfo.descRate) {
					//this is the next rate so update the time
					pLibDev->tx[queueIndex].txStats[statsInfo.descRate].startTime = descTempTime;

					//update the throughput on the last rate
					if(statsInfo.status1 & DESC_FRM_XMIT_OK) {
						//need to remove the 1st packet from new throughput calculation
						pLibDev->tx[queueIndex].txStats[statsInfo.descRate].firstPktGood = 1;
					}

					calculateRateThroughput(devNum, lastDescRate, queueIndex);
				}
			}
				
		}
		else {
			//assume that if find a descriptor that is not done, then
			//none of the rest of the descriptors will be done, since assume
			//tx has completed by time get to here
			mError(devNum, EIO, 
				"Device Number %d:txGetStats: found a descriptor that is not done, stopped gathering stats %08lx %08lx\n", devNum,
				statsInfo.status1, statsInfo.status2);
			break;
		}

		lastDescRate = statsInfo.descRate;
		statsInfo.descToRead += sizeof(MDK_ATHEROS_DESC);		
	}

	if (txTime == 0) {
//		mError(devNum, EINVAL, "Device Number %d:transmit time is zero, not calculating throughput\n", devNum);
	}
	else {
		//calculate the throughput
		bitsReceived = pLibDev->tx[queueIndex].txStats[0].goodPackets * (pLibDev->tx[queueIndex].dataBodyLen + sizeof(MDK_PACKET_HEADER)) * 8;
		pLibDev->tx[queueIndex].txStats[0].throughput = bitsReceived / txTime;
	}

	if(descStartTime < descEndTime) {
		bitsReceived = (pLibDev->tx[queueIndex].txStats[0].goodPackets - firstPktGood) * (pLibDev->tx[queueIndex].dataBodyLen + sizeof(MDK_PACKET_HEADER)) * 8;
		newTxTime = (float)((descEndTime - descStartTime) * 1024)/1000;  //puts the time in millisecs
		pLibDev->tx[queueIndex].txStats[0].newThroughput = (A_UINT32)((float)bitsReceived / newTxTime);
	}


********************** by Siva *******************
*********************** moved to a new function 'fill_txStats' *****/


    if (pLibDev->devMap.remoteLib && !isDragon(devNum)) {
	    pLibDev->devMap.r_fillTxStats(devNum, pLibDev->tx[queueIndex].descAddress, pLibDev->tx[queueIndex].numDesc, pLibDev->tx[queueIndex].dataBodyLen, txTime, &(pLibDev->tx[queueIndex].txStats[0]));
	}
	else {

       fillTxStats(devNum, pLibDev->tx[queueIndex].descAddress, pLibDev->tx[queueIndex].numDesc, pLibDev->tx[queueIndex].dataBodyLen, txTime, &(pLibDev->tx[queueIndex].txStats[0]));
	}

	if (isFalcon(devNum) || isDragon(devNum)) {
		// transmit descriptor does not fill txTimeStamp for falcon. 		
		pLibDev->tx[queueIndex].txStats[0].newThroughput = pLibDev->tx[queueIndex].txStats[0].throughput;
	}

	pLibDev->tx[queueIndex].haveStats = 1;
		
	if(pLibDev->tx[queueIndex].txStats[0].underruns > 0) {
		printf("Device Number %d:SNOOP: Getting underruns %d\n", devNum, pLibDev->tx[queueIndex].txStats[0].underruns);
//FJC_DEBUG, take this back out when put the prefetch back in again
		pLibDev->adjustTxThresh = 1;
	}
#ifdef DEBUG_MEMORY
printf("SNOOP::Collecting stats done\n");
#endif
#ifdef _TIME_PROFILE
    tasEnd=milliTime();
printf("!!!!!!!!!!!!!!!!!SNOOP::exit txAccumulateStats:Time taken = %dms\n", (tasEnd-tasStart));
#endif
	return;
}

/**************************************************************************
* sendStatsPkt - Create and send a stats packet to other station
*
*/
void sendStatsPkt
(
 A_UINT32 devNum, 
 A_UINT32 rate, 				 // rate of bin to send
 A_UINT16 StatsType,			 // which type of stats packet to send
 A_UCHAR *dest					 // address to send to
)
{
	A_UCHAR *pPktData = NULL;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 PktSize;
	A_UINT32 PktAddress;
	A_UINT32 DescAddress;
	A_UINT32 DataBodyLength = 0;
	A_UINT32 status1;
	A_UINT32 status2;
	A_UINT32 i;
	MDK_ATHEROS_DESC localDesc;
	A_UINT16	queueIndex = pLibDev->selQueueIndex;

	// error check statsType 
	if ((StatsType > MDK_TXRX_STATS_PKT) || (StatsType < MDK_TX_STATS_PKT)) {
		mError(devNum, EINVAL, "Device Number %d:sendStatsPkt: illegal StatsType passed in\n", devNum);
		return;
	}
	if (rate > STATS_BINS - 1) {
		mError(devNum, EINVAL, "Device Number %d:sendStatsPkt: illegal rate bin for send packet %d\n", devNum, rate);
		return;
	}


	//create the packet
	if(StatsType == MDK_TXRX_STATS_PKT) {
		/* check to see what stats have been gathered.	If neither, then don't
		 * send the stats, if only one set of stats have been gathered then send that
		 * if both sets have been gathered then send both
		 */
		if(!pLibDev->tx[queueIndex].haveStats && !pLibDev->rx.haveStats) {
			mError(devNum, EINVAL, "Device Number %d:sendStatsPkt: neither tx nor rx stats have been collected\n", devNum);
			return;
		}
		else if (!pLibDev->tx[queueIndex].haveStats) {
			//dont have tx stats, send only rx, change stats type to RX and handle later
			StatsType = MDK_RX_STATS_PKT;
		}
		else if (!pLibDev->rx.haveStats) {
			//dont have rx stats, send only tx, change stats type to RX and handle later
			StatsType = MDK_TX_STATS_PKT;
		}
		else {
			//send both tx and rx stats
			DataBodyLength = sizeof(TX_STATS_STRUCT) + sizeof(RX_STATS_STRUCT) + 2 * sizeof(rate);
			pPktData = (A_UCHAR *)malloc(DataBodyLength);
			if(!pPktData) {
				mError(devNum, ENOMEM, "Device Number %d:sendStatsPkt: unable to allocate memory for TX and RX stats structs\n", devNum);
				return;
			}
	#ifdef ARCH_BIG_ENDIAN
			*(A_UINT32 *)(pPktData) = btol_l(rate);
			swapAndCopyBlock_l((void *)(pPktData + sizeof(rate)),(void *)(&(pLibDev->tx[queueIndex].txStats[rate])), sizeof(TX_STATS_STRUCT));
			*(A_UINT32 *)(pPktData + sizeof(TX_STATS_STRUCT) + sizeof(rate)) = btol_l(rate);
			//#####FJC - rxStats queue info is hardcoded to 0.  We don't currently send que info in mdkpkt
			swapAndCopyBlock_l((void *)(pPktData + sizeof(TX_STATS_STRUCT) + 2 * sizeof(rate)),(void *)(&(pLibDev->rx.rxStats[0][rate])), sizeof(RX_STATS_STRUCT));
	#else
			*(A_UINT32 *)(pPktData) = rate;
			memcpy(pPktData + sizeof(rate), &(pLibDev->tx[queueIndex].txStats[rate]), sizeof(TX_STATS_STRUCT));
			*(A_UINT32 *)(pPktData + sizeof(TX_STATS_STRUCT) + sizeof(rate)) = rate;
			//#####FJC - rxStats queue info is hardcoded to 0.  We don't currently send que info in mdkpkt
			memcpy(pPktData + sizeof(TX_STATS_STRUCT) + 2 * sizeof(rate), &(pLibDev->rx.rxStats[0][rate]), sizeof(RX_STATS_STRUCT));
	#endif
		}
	}

	if(StatsType == MDK_TX_STATS_PKT) {
		if(!pLibDev->tx[queueIndex].haveStats) {
			mError(devNum, EINVAL, "Device Number %d:sendStatsPkt: no tx stats have been collected\n", devNum);
			return;
		}
		DataBodyLength = sizeof(TX_STATS_STRUCT) + sizeof(rate);
		pPktData = (A_UCHAR *)malloc(DataBodyLength);
		if(!pPktData) {
			mError(devNum, ENOMEM, "Device Number %d:sendStatsPkt: unable to allocate memory for TX stats structs\n", devNum);
			return;
		}
	#ifdef ARCH_BIG_ENDIAN
		*(A_UINT32 *)(pPktData) = btol_l(rate);
		swapAndCopyBlock_l((void *)(pPktData + sizeof(rate)),(void *)(&(pLibDev->tx[queueIndex].txStats[rate])), sizeof(TX_STATS_STRUCT));
	#else
		*(A_UINT32 *)(pPktData) = rate;
		memcpy(pPktData + sizeof(rate), &(pLibDev->tx[queueIndex].txStats[rate]), sizeof(TX_STATS_STRUCT));
	#endif
	}

	if (StatsType == MDK_RX_STATS_PKT) {
		if(!pLibDev->rx.haveStats) {
			mError(devNum, EINVAL, "Device Number %d:sendStatsPkt: no rx stats have been collected\n", devNum);
			return;
		}
		DataBodyLength = sizeof(RX_STATS_STRUCT) + sizeof(rate);
		pPktData = (A_UCHAR *)malloc(DataBodyLength);
		if(!pPktData) {
			mError(devNum, ENOMEM, "Device Number %d:sendStatsPkt: unable to allocate memory for TX stats structs\n", devNum);
			return;
		}
	#ifdef ARCH_BIG_ENDIAN
		*(A_UINT32 *)(pPktData) = btol_l(rate);
		//#####FJC - rxStats queue info is hardcoded to 0.  We don't currently send que info in mdkpkt
		swapAndCopyBlock_l((void *)(pPktData + sizeof(rate)),(void *)(&(pLibDev->rx.rxStats[0][rate])), sizeof(RX_STATS_STRUCT));

	#else
		*(A_UINT32 *)(pPktData) = rate;
		//#####FJC - rxStats queue info is hardcoded to 0.  We don't currently send que info in mdkpkt
		memcpy(pPktData + sizeof(rate), &(pLibDev->rx.rxStats[0][rate]), sizeof(RX_STATS_STRUCT));

	#endif
	}

	createTransmitPacket(devNum, StatsType, dest, 1, DataBodyLength, pPktData, 
				DataBodyLength, 0, 0, &PktSize, &PktAddress);

	//data should be copied to the packet memory, so can free TXRX memory
	if (pPktData) 
		free(pPktData);

	//create the descriptor
	DescAddress = memAlloc( devNum, sizeof(MDK_ATHEROS_DESC));
	if (0 == DescAddress) {
		mError(devNum, ENOMEM, "Device Number %d:sendStatsPkt: unable to allocate memory for descriptor\n", devNum);
		return;
	}
	
	//create descriptor
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
    if(isCobra(pLibDev->swDevID)) {
		localDesc.bufferPhysPtr = PktAddress;
	}
	else {
		localDesc.bufferPhysPtr = PktAddress | HOST_PCI_SDRAM_BASEADDR;
	}
#else
	localDesc.bufferPhysPtr = PktAddress;
#endif
	
//	localDesc.hwControl[0] = ((rateValues[0] << BITS_TO_TX_XMIT_RATE) | 
//				 (sizeof(WLAN_DATA_MAC_HEADER3) << BITS_TO_TX_HDR_LEN) |
//				 (PktSize + 4));

//	localDesc.hwControl[1] = PktSize;
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setStatsPktDesc(devNum, &localDesc, PktSize, rateValues[0]);
	
	localDesc.nextPhysPtr = 0;

	writeDescriptor(devNum, DescAddress, &localDesc);
#ifdef DEBUG_MEMORY
printf("SNOOP::Desc contents are\n");
memDisplay(devNum, DescAddress, 12);
#endif

	//send the packet
	//set max retries
#if defined(COBRA_AP) && defined(PCI_INTERFACE)
    if(isCobra(pLibDev->swDevID)) {
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->beginSendStatsPkt(devNum, DescAddress);
	}
	else {
		ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->beginSendStatsPkt(devNum, DescAddress | HOST_PCI_SDRAM_BASEADDR);
	}
#else
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->beginSendStatsPkt(devNum, DescAddress);
#endif

	//poll done bit, waiting for descriptor to be complete
	for(i = 0; i < MDK_PKT_TIMEOUT; i++) {
	   pLibDev->devMap.OSmemRead(devNum,  DescAddress + pLibDev->txDescStatus2,
				(A_UCHAR *)&status2, sizeof(status2));
		
	   if(status2 & DESC_DONE) {
		   pLibDev->devMap.OSmemRead(devNum,  DescAddress + pLibDev->txDescStatus1,
				(A_UCHAR *)&status1, sizeof(status1));

		   if(!(status1 & DESC_FRM_XMIT_OK)) {
				mError(devNum, EIO, "Device Number %d:sendStsPkt: remote stats packet not successfully sent, status = %08lx\n", devNum, status1);
				memFree(devNum, DescAddress);
				memFree(devNum, PktAddress);
				return;
			}
#ifdef DEBUG_MEMORY
printf("SNOOP::sendStatsPkt for rate %d done\n", rate);
#endif
			break;
		}
		mSleep(1);
	}

	if (i == MDK_PKT_TIMEOUT) {
		mError(devNum, EIO, "Device Number %d:sendStsPkt: timed out waiting for stats packet done\n", devNum);

	}
	memFree(devNum, DescAddress);
	memFree(devNum, PktAddress);


	return;
}

/**************************************************************************
* zeroDescriptorStatus - zero out the descriptor status words
*
*/
void zeroDescriptorStatus
(
 A_UINT32	devNumIndex,
 MDK_ATHEROS_DESC *pDesc,
 A_UINT32 swDevID 
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNumIndex];

	if (isFalcon(devNumIndex) || isDragon(devNumIndex)) {
		pDesc->hwExtraBuffer[0] = 0;
		pDesc->hwExtraBuffer[1] = 0;
		pDesc->hwExtraBuffer[2] = 0;
	} else if((pLibDev->swDevID & 0x00ff) >= 0x13) {
		pDesc->hwExtra[0] = 0;
		pDesc->hwExtra[1] = 0;
	} else {
		pDesc->hwStatus[0] = 0;
		pDesc->hwStatus[1] = 0;
	}

}
/**************************************************************************
* writeDescriptor - write descriptor to physical memory
*
*/
void writeDescriptor
(
 A_UINT32	devNum,
 A_UINT32	descAddress,
 MDK_ATHEROS_DESC *pDesc
)
{
#ifndef THIN_CLIENT_BUILD
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	zeroDescriptorStatus(devNum, pDesc, pLibDev->swDevID);
	pLibDev->devMap.OSmemWrite(devNum, descAddress, (A_UCHAR *)pDesc, sizeof(MDK_ATHEROS_DESC));
#endif
}



/**************************************************************************
* writeDescriptors - write a group of descriptors to physical memory
*
*/
#define MAX_MEMREAD_BYTES       2048  
void writeDescriptors
(
 A_UINT32	devNumIndex,
 A_UINT32	descAddress,
 MDK_ATHEROS_DESC *pDesc,
 A_UINT32   numDescriptors
)
{
#ifdef THIN_CLIENT_BUILD
   hwMemWriteBlock(devNumIndex, pDesc, sizeof(MDK_ATHEROS_DESC), descAddress);
#else
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNumIndex];
	A_UINT32 sizeToWrite;
	A_UCHAR *blockPtr;
	
	//need to check for block of descriptors being greater than the write size per cmd,
	//if so need to split the block
	sizeToWrite = sizeof(MDK_ATHEROS_DESC) * numDescriptors;
	blockPtr = (A_UCHAR *)pDesc;
	while(sizeToWrite > MAX_MEMREAD_BYTES) {
		pLibDev->devMap.OSmemWrite(devNumIndex, descAddress, blockPtr, MAX_MEMREAD_BYTES);	
		blockPtr += MAX_MEMREAD_BYTES;
		descAddress += MAX_MEMREAD_BYTES;
		sizeToWrite -= MAX_MEMREAD_BYTES;
	}

	pLibDev->devMap.OSmemWrite(devNumIndex, descAddress, (A_UCHAR *)blockPtr, sizeToWrite);
#endif
}




/**************************************************************************
* mdkExtractAddrAndSequence() - Extract address and sequence from packets
*
* Checks to see if we still need to get the address and sequence control
* for the current packet, if so then try to extract from this buffer.  
* Make the assumption that the header is contained in one buffer. May add
* support for multiple buffers later if needed.
*
* RETURNS: 
*/
void mdkExtractAddrAndSequence 
(
 A_UINT32			devNum,
 RX_STATS_TEMP_INFO *pStatsInfo
)
{
	A_UINT32			 i;
	A_INT32 			 pktBodySize;
	FRAME_CONTROL       *pFrameControl;
	A_UINT16			 frameControlValue;
	LIB_DEV_INFO		*pLibDev = gLibInfo.pLibDevArray[devNum];
	WLAN_MACADDR		 frameMacAddr = {0,0,0,0,0,0};	
	MDK_PACKET_HEADER	 mdkPktInfo;
	A_BOOL				 falconTrue = FALSE;
	A_UINT32			 ppm_data_padding = PPM_DATA_SIZE;

	if(pStatsInfo->gotHeaderInfo)
	{
		/* we've already done this for this packet, get out */
		return;
	}

	falconTrue = isFalcon(devNum) || isDragon(devNum);
	if (falconTrue) {
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	}

	/* if got to here then assume that this is the first buffer 
	 * of the packet.  For now are assuming that the header is 
	 * fully contained in this first buffer 
	 */

	/* do some error checking */

	/* calculate pkt body size before reading any buffer contents */
	pktBodySize = (pStatsInfo->status1 & DESC_DATA_LENGTH_FIELDS) - sizeof(WLAN_DATA_MAC_HEADER3) 
					- FCS_FIELD;
	if(pLibDev->rx.enablePPM) {
		pktBodySize -= ppm_data_padding;
	}

	/* A non-MDK packet has snuck into our receive */
	//Only need this filter on for crete, don't expect to come across it with maui, want to know about it if do
	if(pktBodySize < (A_INT32)(sizeof(MDK_PACKET_HEADER))) {
		pStatsInfo->badPacket = TRUE;	
		pStatsInfo->gotHeaderInfo = TRUE;
		//Only need this filter on for crete, don't expect to come across it with maui, 
		//want to know about it if do
		if (pLibDev->swDevID != 0x0007) {
			if(pStatsInfo->status2 & DESC_FRM_RCV_OK) { 
//				mError(devNum, EIO, "\nDevice Number %d:mdkExtractAddrAndSequence: A packet smaller than a valid MDK packet was received\n", devNum);
			}
		}
		return;
	}


	/* first check to see if this is a control packet, is so don't
	 * have any sequence num checking to do 
	 */
	 memcpy((A_UCHAR *)&frameControlValue, (A_UCHAR*)&(pStatsInfo->frame_info[0]), sizeof(frameControlValue));
/*   Siva
	pLibDev->devMap.OSmemRead(devNum, pStatsInfo->bufferAddr, 
				(A_UCHAR *)&frameControlValue, sizeof(frameControlValue));
*/
// need to swap the frame control for spirit
#if 0
//#ifdef ARCH_BIG_ENDIAN
	frameControlValue = ltob_s(frameControlValue);
#endif	
	pFrameControl = (FRAME_CONTROL	*)&frameControlValue;
	if ( ((pFrameControl->fType == FRAME_CTRL) || (pFrameControl->fType == FRAME_MGT)) && (pLibDev->rx.rxMode != RX_FIXED_NUMBER))
	{
		pStatsInfo->controlFrameReceived = TRUE;
		pStatsInfo->gotHeaderInfo = TRUE;
//		mError(devNum, EIO, "Device Number %d:mdkExtractAddrAndSequence: Not extracting address info, control frame received\n", devNum);
		return;
	}

	// Recheck pkt body size now that we know we've read the frame control
	pktBodySize -= (pFrameControl->wep ?  (WEP_IV_FIELD + 2 + WEP_ICV_FIELD) : 0);
	if(pktBodySize < (A_INT32)(sizeof(MDK_PACKET_HEADER))) {
		pStatsInfo->badPacket = TRUE;	
		pStatsInfo->gotHeaderInfo = TRUE;
		if(pStatsInfo->status2 & DESC_FRM_RCV_OK) {    	    
//			mError(devNum, EIO, "Device Number %d:mdkExtractAddrAndSequence: A packet smaller than a valid MDK packet was received, second check\n", devNum);
		}
		return;
	}

	//get the MDK packet info from the start of the packet
	memcpy( (A_UCHAR *)&mdkPktInfo, &(pStatsInfo->frame_info[0]) + MAC_HDR_DATA3_SIZE + (pFrameControl->wep ? (WEP_IV_FIELD + 2) : 0), sizeof(mdkPktInfo));
/*  Siva
	pLibDev->devMap.OSmemRead(devNum, pStatsInfo->bufferAddr + MAC_HDR_DATA3_SIZE + 
					(pFrameControl->wep ? (WEP_IV_FIELD + 2) : 0), 
					(A_UCHAR *)&mdkPktInfo, sizeof(mdkPktInfo));
*/

#ifdef ARCH_BIG_ENDIAN
	mdkPktInfo.pktType = ltob_s(mdkPktInfo.pktType);
	mdkPktInfo.numPackets = ltob_l(mdkPktInfo.numPackets);
#endif	
	pStatsInfo->mdkPktType = mdkPktInfo.pktType;
	pStatsInfo->qcuIndex = 0; //mdkPktInfo.qcuIndex;		// __TODO__

#ifdef DEBUG_MEMORY
printf("SNOOP::Frame contents are\n");
memDisplay(devNum, pStatsInfo->bufferAddr, 20*4);
printf("SNOOP::Address 2 offset = %x\n", ADDRESS2_START);
#endif

	/* should now be able to copy the mac address and seq control */
	for (i = 0; i < WLAN_MAC_ADDR_SIZE; i++) 
	{
		memcpy( &(frameMacAddr.octets[i]), &(pStatsInfo->frame_info[0]) + ADDRESS2_START + i, sizeof(frameMacAddr.octets[i]));

/*  Siva
		pLibDev->devMap.OSmemRead(devNum, pStatsInfo->bufferAddr + ADDRESS2_START + i, 
					&(frameMacAddr.octets[i]), sizeof(frameMacAddr.octets[i]));
*/
	}

	// see if this is the mac address we expect.  If we don't already have a mac address for
	//compare then grab this one, otherwise compare it against the one we hold
	if(pLibDev->rx.rxMode == RX_FIXED_NUMBER) {
		//need to compare against our BSSID
		if(A_MACADDR_COMP(&pLibDev->bssAddr, &frameMacAddr)) {
			pStatsInfo->badPacket = TRUE;
			pStatsInfo->gotHeaderInfo = TRUE;
			return;
		}
	}
	else if (pStatsInfo->gotMacAddr) {
		if (A_MACADDR_COMP(&(pStatsInfo->addrPacket), &frameMacAddr)) {
			//mac addresses don't match, need to flag this as a bad packet
			//we need to traverse the descriptors to get to the end of it,
			//but we don't want to gather stats on it
			pStatsInfo->badPacket = TRUE;
			pStatsInfo->gotHeaderInfo = TRUE;
			return;
		}
	}
	else {
		//we don't have mac address of received packets. Assume this is the address to 
		//receive from so take a copy of it if this is an MDK style PACKET
		if((pStatsInfo->mdkPktType >= MDK_NORMAL_PKT) && (pStatsInfo->mdkPktType <= MDK_TXRX_STATS_PKT)) {
			A_MACADDR_COPY(&frameMacAddr, &pStatsInfo->addrPacket);
			pStatsInfo->gotMacAddr = TRUE;
		}
		else {
			//don't mark it bad if its a decrypt error.  We need to let these through
			if ((pStatsInfo->status2 & 0x1f) != 0x11) {
				pStatsInfo->badPacket = TRUE;
			}
			pStatsInfo->gotHeaderInfo = TRUE;
#ifdef _DEBUG
//			  mError(devNum, EIO, "Device Number %d:mdkExtractAddrAndSequence: A non-MDK packet was received before any MDK packets - ignoring\n", devNum);
#endif
			return;
		}
	}
	
	memcpy( (A_UCHAR *)&(pStatsInfo->seqCurrentPacket), &(pStatsInfo->frame_info[0]) + SEQ_CONTROL_START, sizeof(pStatsInfo->seqCurrentPacket));
/*  Siva
	pLibDev->devMap.OSmemRead(devNum, pStatsInfo->bufferAddr + SEQ_CONTROL_START, 
					(A_UCHAR *)&(pStatsInfo->seqCurrentPacket), sizeof(pStatsInfo->seqCurrentPacket));
*/
	pStatsInfo->retry = (A_BOOL)(pFrameControl->retry);
	pStatsInfo->gotHeaderInfo = TRUE;


	return;
}

/**************************************************************************
* extractRxStats() - Extract and save receive descriptor statistics
*
* Extract and save required statistics.  
*
* RETURNS: 
*/
void mdkExtractRxStats
(
 A_UINT32 devNum,
 RX_STATS_TEMP_INFO *pStatsInfo
)
{
	A_BOOL			isDuplicate;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	RX_STATS_STRUCT  *rxStatsTemp;
	MULT_ANT_SIG_STRENGTH_STATS  *multAntStatsTemp;
	A_UINT32          ii, jj, kk, ll;
	A_INT32           rssi_max = -128;


	/* first check for good packets */
	if (pStatsInfo->status2 & DESC_FRM_RCV_OK) 
	{
		pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].goodPackets ++;
		pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].goodPackets ++;

		/* do any duplicate packet processing */
		mdkCountDuplicatePackets(devNum, pStatsInfo, &isDuplicate);

		/* add to total number of bytes count if not duplicate */
		if (!isDuplicate)
		{
			pStatsInfo->totalBytes += (pStatsInfo->status1 & 0xfff);
		}


		/* do signal strength processig */
		mdkGetSignalStrengthStats(&(pStatsInfo->sigStrength[pStatsInfo->descRate]), 
			(A_INT8)((pStatsInfo->status1 >> pLibDev->bitsToRxSigStrength) & SIG_STRENGTH_MASK));
		mdkGetSignalStrengthStats(&(pStatsInfo->sigStrength[0]), 
			(A_INT8)((pStatsInfo->status1 >> pLibDev->bitsToRxSigStrength) & SIG_STRENGTH_MASK));

		/* do multi antenna signal strength and chainSel/Req processing for falcon */
		mdkProcessMultiAntSigStrength(devNum, pStatsInfo);

		if (isFalcon(devNum) || isDragon(devNum)) {
			for (kk=0; kk<2; kk++) {
				ll = (kk>0) ? pStatsInfo->descRate : 0;
				rxStatsTemp = &(pLibDev->rx.rxStats[pStatsInfo->qcuIndex][ll]);
				multAntStatsTemp = &(pStatsInfo->multAntSigStrength[ll]);

				rssi_max = -128;
				jj = 0;

				for (ii=0; ii<4; ii++) {
					rxStatsTemp->RSSIPerAntAvg[ii] = (A_INT32) multAntStatsTemp->rxAvrgSignalStrengthAnt[ii];
					rxStatsTemp->RSSIPerAntMax[ii] = (A_INT32) multAntStatsTemp->rxMaxSigStrengthAnt[ii];
					rxStatsTemp->RSSIPerAntMin[ii] = (A_INT32) multAntStatsTemp->rxMinSigStrengthAnt[ii];
					if (rxStatsTemp->RSSIPerAntAvg[ii] > rssi_max) {
						rssi_max = rxStatsTemp->RSSIPerAntAvg[ii];
						jj = ii; // pick the ant with max rssi_avg to report
					}
				}

				rxStatsTemp->maxRSSIAnt = jj;
				rxStatsTemp->DataSigStrengthAvg = rxStatsTemp->RSSIPerAntAvg[jj];
				rxStatsTemp->DataSigStrengthMax = rxStatsTemp->RSSIPerAntMax[jj];
				rxStatsTemp->DataSigStrengthMin = rxStatsTemp->RSSIPerAntMin[jj];
				for (ii=0; ii<2; ii++) {
					rxStatsTemp->Chain0AntReq[ii] = pStatsInfo->Chain0AntReq[ll].Count[ii];
					rxStatsTemp->Chain1AntReq[ii] = pStatsInfo->Chain1AntReq[ll].Count[ii];
					rxStatsTemp->Chain0AntSel[ii] = pStatsInfo->Chain0AntSel[ll].Count[ii];
					rxStatsTemp->Chain1AntSel[ii] = pStatsInfo->Chain1AntSel[ll].Count[ii];
					rxStatsTemp->ChainStrong[ii]  = pStatsInfo->ChainStrong[ll].Count[ii];
				}
			}
		} else {
        		pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].DataSigStrengthAvg = pStatsInfo->sigStrength[pStatsInfo->descRate].rxAvrgSignalStrength;
		        pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].DataSigStrengthMax = pStatsInfo->sigStrength[pStatsInfo->descRate].rxMaxSigStrength;
		        pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].DataSigStrengthMin = pStatsInfo->sigStrength[pStatsInfo->descRate].rxMinSigStrength;
		        pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].DataSigStrengthAvg = pStatsInfo->sigStrength[0].rxAvrgSignalStrength;
		        pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].DataSigStrengthMax = pStatsInfo->sigStrength[0].rxMaxSigStrength;
		        pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].DataSigStrengthMin = pStatsInfo->sigStrength[0].rxMinSigStrength;
		}
	}
	else
	{
		if (pStatsInfo->status2 & DESC_CRC_ERR)
		{
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].crcPackets++;
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].crcPackets++;
		}
		else if (pStatsInfo->status2 & pLibDev->decryptErrMsk)
		{
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].decrypErrors++;
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].decrypErrors++;
		}
	}

	pLibDev->rx.haveStats = 1;
	return;
}


/**************************************************************************
* mdkExtractRemoteStats() - Extract and save statistics sent from remote stn
*
*
* RETURNS: TRUE if the last stats packet or error - otherwise FALSE
*/
A_BOOL mdkExtractRemoteStats
(
 A_UINT32 devNum,
 RX_STATS_TEMP_INFO *pStatsInfo
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32	 addressStats;			//physical address of where stats are in packet
	A_UINT32	 locBin = 0;
#ifdef DEBUG_MEMORY
printf("SNOOP::mdkExtractRemoteStats enter\n");
#endif

	if ((pStatsInfo->mdkPktType < MDK_TX_STATS_PKT) || (pStatsInfo->mdkPktType > MDK_TXRX_STATS_PKT)) {
		mError(devNum, EINVAL, "Device Number %d:mdkExtractRemoteStats: Illegal mdk packet type for stats extraction\n", devNum);
		return TRUE;
	}

	pLibDev->remoteStats = pStatsInfo->mdkPktType;
	
	addressStats = pStatsInfo->bufferAddr + sizeof(WLAN_DATA_MAC_HEADER3) + sizeof(MDK_PACKET_HEADER);
	if ((pStatsInfo->mdkPktType == MDK_TX_STATS_PKT) || 
		(pStatsInfo->mdkPktType == MDK_TXRX_STATS_PKT)) {
		//figure out which bin to put these remote stats in
		pLibDev->devMap.OSmemRead(devNum, addressStats, 
					(A_UCHAR *)&locBin, sizeof(locBin));

#ifdef ARCH_BIG_ENDIAN
	    locBin = ltob_l(locBin);
#endif
#ifdef DEBUG_MEMORY
printf("SNOOP::mdkExtractRemoteStats::locBin=%d\n", locBin);
#endif
	
		if(locBin > STATS_BINS - 1) {
			mError(devNum, EINVAL, "Device Number %d:mdkExtractRemoteStats: TXSTATS:Illegal stats bin value received: %d\n", devNum, locBin);
			return TRUE;
		}

		addressStats += sizeof(locBin);

		//copy over stats info from packet
		pLibDev->devMap.OSmemRead(devNum, addressStats, 
					(A_UCHAR *)&pLibDev->txRemoteStats[locBin], sizeof(TX_STATS_STRUCT));
#ifdef ARCH_BIG_ENDIAN
		swapBlock_l((void *)(&pLibDev->txRemoteStats[locBin]),sizeof(TX_STATS_STRUCT));
#endif

		
		//increment our address value so that would point to RX stats if any
		addressStats += sizeof(TX_STATS_STRUCT);
	}

	if ((pStatsInfo->mdkPktType == MDK_RX_STATS_PKT) || 
		(pStatsInfo->mdkPktType == MDK_TXRX_STATS_PKT)) {

		pLibDev->devMap.OSmemRead(devNum, addressStats, 
					(A_UCHAR *)&locBin, sizeof(locBin));
#ifdef ARCH_BIG_ENDIAN
	locBin = ltob_l(locBin);
#endif

		//if this is venice, then need to put the stats in the correct CCK bins
		if(((pLibDev->swDevID & 0xff) >= 0x0013) && (pLibDev->mode == MODE_11B)){
			if((locBin > 0) && (locBin < 9)) {
				if(locBin == 1){
					locBin += 8;
				}
				else {
					locBin += 7;
				}
			}
		}

		if(locBin > STATS_BINS - 1) {
			mError(devNum, EINVAL, "Device Number %d:mdkExtractRemoteStats: RXSTATS:Illegal stats bin value received: %x\n", devNum, locBin);
			return TRUE;
		}


		addressStats += sizeof(locBin);

		//copy over stats info from packet
		pLibDev->devMap.OSmemRead(devNum, addressStats, 
					(A_UCHAR *)&pLibDev->rxRemoteStats[0][locBin], sizeof(RX_STATS_STRUCT));

#ifdef ARCH_BIG_ENDIAN
		swapBlock_l((void *)(&pLibDev->rxRemoteStats[0][locBin]),sizeof(RX_STATS_STRUCT));
#endif
	}
#ifdef DEBUG_MEMORY
printf("SNOOP::return from mdkExtractRemoteStats:locBin=%d\n", locBin);
#endif
	return (A_UCHAR)(locBin ? FALSE : TRUE );
}

/**************************************************************************
* countDuplicatePackets() - Count duplicate packets
*
* Uses the address and sequence number already extracted for the packet
* and determines if this is a duplicate packet.  Updates statistics if
* it is a duplicate packet
*
* RETURNS: 
*/
void mdkCountDuplicatePackets
(
 A_UINT32	devNum,
 RX_STATS_TEMP_INFO *pStatsInfo,
 A_BOOL 	*pIsDuplicate
)
{

	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	*pIsDuplicate = FALSE;

	// don't need to do any address checking, we did that already
	if (pStatsInfo->retry)
	{
		if((WLAN_GET_SEQNUM(pStatsInfo->seqCurrentPacket) == pStatsInfo->lastRxSeqNo) &&
		   (WLAN_GET_FRAGNUM(pStatsInfo->seqCurrentPacket) == pStatsInfo->lastRxFragNo))
		{
			*pIsDuplicate = TRUE;
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].singleDups++;
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].singleDups++;

			if(WLAN_GET_SEQNUM(pStatsInfo->seqCurrentPacket) == pStatsInfo->oneBeforeLastRxSeqNo)
			{
				pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].multipleDups++;
				pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].singleDups++;
			}
			pStatsInfo->oneBeforeLastRxSeqNo = pStatsInfo->lastRxSeqNo;
		}
	}	

	pStatsInfo->lastRxSeqNo = WLAN_GET_SEQNUM(pStatsInfo->seqCurrentPacket);
	pStatsInfo->lastRxFragNo = WLAN_GET_FRAGNUM(pStatsInfo->seqCurrentPacket);

	return;
}


/**************************************************************************
* mdkGetSignalStrengthStats() - Update signal strength stats
*
* Given the value of signal strength from the descriptor, update the 
* relevant signal strength statistics
*
* RETURNS: 
*/
void mdkGetSignalStrengthStats
(
 SIG_STRENGTH_STATS *pStats,
 A_INT8			signalStrength
)
{
	//workaround -128 RSSI
	if(signalStrength == -128) {
		signalStrength = 0;
	}
	pStats->rxAccumSignalStrength += signalStrength;
	pStats->rxNumSigStrengthSamples++;
	pStats->rxAvrgSignalStrength = (A_INT8)
	(pStats->rxAccumSignalStrength/(A_INT32)pStats->rxNumSigStrengthSamples);
	
	/* max and min */
	if(signalStrength > pStats->rxMaxSigStrength)
		pStats->rxMaxSigStrength = signalStrength;
	
	if (signalStrength < pStats->rxMinSigStrength)
		pStats->rxMinSigStrength = signalStrength;
	return;
}

/**************************-************************************************
* extractTxStats() - Extract and save transmit descriptor statistics
*
* 
*
* RETURNS: 
*/
#if 0
void mdkExtractTxStats
(
 A_UINT32	devNum,
 TX_STATS_TEMP_INFO *pStatsInfo,
 A_UINT16	queueIndex
)
{
	A_UINT16 numRetries;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
//	A_UINT16	queueIndex = pLibDev->selQueueIndex;

	// first check for good packets 
	if (pStatsInfo->status1 & DESC_FRM_XMIT_OK) 
	{
		pLibDev->tx[queueIndex].txStats[0].goodPackets ++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].goodPackets ++;

		if (isFalcon(devNum)) {
			pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].BFCount += (pStatsInfo->status2 >> 13) & 0x1;
			pLibDev->tx[queueIndex].txStats[0].BFCount += (pStatsInfo->status2 >> 13) & 0x1;
		}

		/* do any signal strength processig */
		mdkGetSignalStrengthStats(&(pStatsInfo->sigStrength[0]), 
			(A_INT8)((pStatsInfo->status2 >> BITS_TO_TX_SIG_STRENGTH) & SIG_STRENGTH_MASK));
		pLibDev->tx[queueIndex].txStats[0].ackSigStrengthAvg = pStatsInfo->sigStrength[0].rxAvrgSignalStrength;
		pLibDev->tx[queueIndex].txStats[0].ackSigStrengthMax = pStatsInfo->sigStrength[0].rxMaxSigStrength;
		pLibDev->tx[queueIndex].txStats[0].ackSigStrengthMin = pStatsInfo->sigStrength[0].rxMinSigStrength;

		mdkGetSignalStrengthStats(&(pStatsInfo->sigStrength[pStatsInfo->descRate]), 
			(A_INT8)((pStatsInfo->status2 >> BITS_TO_TX_SIG_STRENGTH) & SIG_STRENGTH_MASK));
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].ackSigStrengthAvg = pStatsInfo->sigStrength[pStatsInfo->descRate].rxAvrgSignalStrength;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].ackSigStrengthMax = pStatsInfo->sigStrength[pStatsInfo->descRate].rxMaxSigStrength;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].ackSigStrengthMin = pStatsInfo->sigStrength[pStatsInfo->descRate].rxMinSigStrength;
	}
	else
	{
		/* processess error statistics */
		if (pStatsInfo->status1 & DESC_FIFO_UNDERRUN)
		{
			pLibDev->tx[queueIndex].txStats[0].underruns ++;
			pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].underruns ++;
		}
		else if (pStatsInfo->status1 & DESC_EXCESS_RETIES)
		{
			pLibDev->tx[queueIndex].txStats[0].excessiveRetries ++;
			pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].excessiveRetries ++;
		}
	}

	/* count the number of retries */
	numRetries = (A_UINT16)((pStatsInfo->status1 >> BITS_TO_SHORT_RETRIES) & RETRIES_MASK);
	if (numRetries == 1) {
		pLibDev->tx[queueIndex].txStats[0].shortRetry1++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].shortRetry1++;
	}
	else if (numRetries == 2) {
		pLibDev->tx[queueIndex].txStats[0].shortRetry2++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].shortRetry2++;
	}
	else if (numRetries == 3) {
		pLibDev->tx[queueIndex].txStats[0].shortRetry3++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].shortRetry3++;
	}
	else if (numRetries == 4) {
		pLibDev->tx[queueIndex].txStats[0].shortRetry4++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].shortRetry4++;
	}
	else if (numRetries == 5) {
		pLibDev->tx[queueIndex].txStats[0].shortRetry5++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].shortRetry5++;
	}
	else if ((numRetries >= 6) && (numRetries <= 10)) {
		pLibDev->tx[queueIndex].txStats[0].shortRetry6to10++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].shortRetry6to10++;
	}
	else if ((numRetries >=11) && (numRetries <= 15)) {
		pLibDev->tx[queueIndex].txStats[0].shortRetry11to15++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].shortRetry11to15++;
	}
	else;

	numRetries = (A_UINT16)((pStatsInfo->status1 >> BITS_TO_LONG_RETRIES) & RETRIES_MASK);
	if (numRetries == 1) {
		pLibDev->tx[queueIndex].txStats[0].longRetry1++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].longRetry1++;
	}
	else if (numRetries == 2) {
		pLibDev->tx[queueIndex].txStats[0].longRetry2++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].longRetry2++;
	}
	else if (numRetries == 3) {
		pLibDev->tx[queueIndex].txStats[0].longRetry3++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].longRetry3++;
	}
	else if (numRetries == 4) {
		pLibDev->tx[queueIndex].txStats[0].longRetry4++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].longRetry4++;
	}
	else if (numRetries == 5) {
		pLibDev->tx[queueIndex].txStats[0].longRetry5++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].longRetry5++;
	}
	else if ((numRetries >= 6) && (numRetries <= 10)) {
		pLibDev->tx[queueIndex].txStats[0].longRetry6to10++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].longRetry6to10++;
	}
	else if ((numRetries >=11) && (numRetries <= 15)) {
		pLibDev->tx[queueIndex].txStats[0].longRetry11to15++;
		pLibDev->tx[queueIndex].txStats[pStatsInfo->descRate].longRetry11to15++;
	}
	else;

	return;
}
#endif

/**************************************************************************
* extractPPM() - Pull PPM value from the packet body
*
*/
void extractPPM
(
 A_UINT32 devNum, 
 RX_STATS_TEMP_INFO *pStatsInfo
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 pktData;
	A_INT32 ppm;
	double f_ppm;
	double scale_factor;
	A_BOOL        falconTrue = FALSE;
	A_UINT32      ppm_data_padding = PPM_DATA_SIZE;
	A_UINT32      ppm_byte_offset = 0;

	falconTrue = isFalcon(devNum) || isDragon(devNum);
	if (falconTrue) {
		ppm_byte_offset = 4;
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} 

	// Only pull PPM from good packets
	if (pStatsInfo->status2 & DESC_FRM_RCV_OK) {
		//get to start of packet body
		memcpy((A_UCHAR*)&pktData, &(pStatsInfo->ppm_data[0]), sizeof(pktData));
/*  Siva
		pLibDev->devMap.OSmemRead(devNum, pStatsInfo->bufferAddr - ppm_data_padding, (A_UCHAR *)&pktData, sizeof(pktData));
*/
	#ifdef ARCH_BIG_ENDIAN
		pktData = ltob_l(pktData);
	#endif
		ppm = pktData & 0xffff;
		// Sign extend ppm
		if((ppm >> 15) == 1) {
			ppm = ppm | 0xffff0000;
		}
		
		// scale ppm to parts per million
		scale_factor = pLibDev->freqForResetDevice*3.2768/1000;
		//scale ppm appropriately for turbo, half and quarter modes
		if(pLibDev->turbo == TURBO_ENABLE) {
			scale_factor = scale_factor / 2;
		}
		else if (pLibDev->turbo == HALF_SPEED_MODE) {
			scale_factor = scale_factor * 2;
		}
		else if (pLibDev->turbo == QUARTER_SPEED_MODE) {
			scale_factor = scale_factor * 4;
		}
		f_ppm = ppm/scale_factor;
		ppm = (A_INT32)(ppm /scale_factor);

		pStatsInfo->ppmAccum[0] += f_ppm;
		pStatsInfo->ppmSamples[0]++;
		pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].ppmAvg = (A_INT32)(pStatsInfo->ppmAccum[0] /
			pStatsInfo->ppmSamples[0]);
	
		/* max and min */
		if(ppm > pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].ppmMax)
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].ppmMax = ppm;
		if (ppm < pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].ppmMin)
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].ppmMin = ppm;

		pStatsInfo->ppmAccum[pStatsInfo->descRate] += f_ppm;
		pStatsInfo->ppmSamples[pStatsInfo->descRate]++;
		pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].ppmAvg = (A_INT32)(pStatsInfo->ppmAccum[pStatsInfo->descRate] /
			pStatsInfo->ppmSamples[pStatsInfo->descRate]);
	
		/* max and min */
		if(ppm > pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].ppmMax)
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].ppmMax = ppm;
		if (ppm < pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].ppmMin)
			pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].ppmMin = ppm;
		return;
	}
}

/**************************************************************************
* comparePktData() - Do a bit comparison on pkt data to find bit errors
*
* Note compare only compares the packet body.  It does not compare 
* the header, since most of the header had to be correct to get to here
* ie all 3 addresses would already by correct
*
* RETURNS: 
*/
void comparePktData
(
 A_UINT32	devNum,
 RX_STATS_TEMP_INFO *pStatsInfo
)
{
	A_UINT32 pktCompareAddr;
	A_UINT32 pktBodySize, initSize;
	A_UINT32 pktData;
	A_UINT32 compareWord;
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 *pTempPtr;
	A_UINT32 mask;
	A_UINT32 mismatchBits;
	A_UINT32 *tmp_pktData;
	A_UCHAR  *intermediatePtr;
	A_BOOL   falconTrue = FALSE;
	A_UINT32 ppm_data_padding = PPM_DATA_SIZE;

	falconTrue = isFalcon(devNum) || isDragon(devNum);
	if (falconTrue) {
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} 

	/* check to see if more bit is set in descriptor, compare does not
	 * support this at the moment
	 */
	if (pStatsInfo->status1 & DESC_MORE) {
		mError(devNum, EINVAL, 
			"Device Number %d:comparePktData: Packet is contained in more than one descriptor, compare does not support this\n", devNum);
		return;
	}

	//get to start of packet body
	pktCompareAddr = pStatsInfo->bufferAddr + sizeof(WLAN_DATA_MAC_HEADER3) + 
					 (pLibDev->wepEnable ? WEP_IV_FIELD : 0)
					 + sizeof(MDK_PACKET_HEADER);


	//calculate pkt body size
	pktBodySize = (pStatsInfo->status1 & DESC_DATA_LENGTH_FIELDS) - sizeof(WLAN_DATA_MAC_HEADER3)
					-  (pLibDev->wepEnable ? WEP_IV_FIELD : 0) 
					- sizeof(MDK_PACKET_HEADER) - FCS_FIELD;
	initSize = pktBodySize;

	if(pLibDev->rx.enablePPM) {
		pktBodySize -= ppm_data_padding;
	}

	pTempPtr = (A_UINT32 *)pStatsInfo->pCompareData;
	//temp work around for predator, skip the first 4 bytes of buffer
//	pTempPtr++;
//	pktBodySize -=4;
	tmp_pktDataPtr = (A_UCHAR *) malloc(pktBodySize * sizeof(A_UCHAR));
	if (tmp_pktDataPtr) {
		if(pktBodySize > 2000) {
			pLibDev->devMap.OSmemRead(devNum, pktCompareAddr, (A_UCHAR *)tmp_pktDataPtr, 2000);
			intermediatePtr = (A_UCHAR *)tmp_pktDataPtr + 2000;
			pLibDev->devMap.OSmemRead(devNum, pktCompareAddr + 2000, intermediatePtr, pktBodySize - 2000);
		} else {
			pLibDev->devMap.OSmemRead(devNum, pktCompareAddr, (A_UCHAR *)tmp_pktDataPtr, pktBodySize);
		}
	}
	else {
		printf("Unable to allocate memory for reading pkt data\n");
		return;
	}
	
	tmp_pktData = (A_UINT32 *)tmp_pktDataPtr;
	while (pktBodySize) {
		//pLibDev->devMap.OSmemRead(devNum, pktCompareAddr, (A_UCHAR *)&pktData, sizeof(pktData));
		pktData = (A_UINT32)(*tmp_pktData);	
		tmp_pktData++;
		compareWord = *pTempPtr;
		if(pktBodySize < 4) {
			//mask off bits won't compare
			mask = 0xffffffff >> ((4 - pktBodySize) * 8);
			pktData &= mask;
			compareWord &= mask;
			pktBodySize = 0;
		}
		else {
			pktBodySize -= 4;
		}

		mismatchBits = compareWord ^ pktData;
		if(mismatchBits) {
			/* there is a compare mismatch so count number of bits mismatch
			 * and increment correct stats counter based on frame OK or not
			 */
			if (pStatsInfo->status2 & DESC_FRM_RCV_OK) {
				pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].bitErrorCompares += countBits(mismatchBits);
				pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].bitErrorCompares += countBits(mismatchBits);
//				printf("snoop: error at %d/%d : mismatch bits = %d [0x%x==0x%x]\n", pktBodySize, initSize, countBits(mismatchBits), compareWord, pktData);
			}
			else {
				pLibDev->rx.rxStats[pStatsInfo->qcuIndex][0].bitMiscompares += countBits(mismatchBits);
				pLibDev->rx.rxStats[pStatsInfo->qcuIndex][pStatsInfo->descRate].bitMiscompares += countBits(mismatchBits);
			}
		}
		pTempPtr ++;
		pktCompareAddr += sizeof(pktCompareAddr);
	}
	free(tmp_pktDataPtr);

	return;
}

/**************************************************************************
* countBits() - Count number of bits set in 32 bit value
* 
* Uses a byte wide lookup table
* 
* RETURNS: Number of bits set
*/
A_UINT32 countBits
(
 A_UINT32 mismatchBits
)
{
	A_UINT32 count;

	count = bitCount[mismatchBits >> 24] + bitCount[(mismatchBits >> 16) & 0xff]
			+ bitCount[(mismatchBits >> 8) & 0xff] + bitCount[mismatchBits & 0xff];

	return(count);
}

/**************************************************************************
* fillCompareBuffer() - Fill buffer with repeating input pattern 
* 
* 
* RETURNS: 
*/
void fillCompareBuffer
(
 A_UCHAR *pBuffer,
 A_UINT32 compareBufferSize,
 A_UCHAR *pDataPattern,
 A_UINT32 dataPatternLength
)
{
	A_UINT32 amountToWrite;

	//fill the buffer with the pattern
	while (compareBufferSize) {
		if(compareBufferSize > dataPatternLength) {
			amountToWrite = dataPatternLength;
			compareBufferSize -= dataPatternLength;
		} 
		else {
			amountToWrite = compareBufferSize;
			compareBufferSize = 0;
		}
		memcpy(pBuffer, pDataPattern, amountToWrite);
		pBuffer += amountToWrite;
	}
}

/****************************************************************************
* setQueue() - set the Transmit queue number 
*
*/

MANLIB_API void setQueue
(
	A_UINT32 devNum, 
	A_UINT32 qcuNumber
)
{
    	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:setQueue\n", devNum);
		return;
	}
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->setQueue(devNum, qcuNumber);
}

/****************************************************************************
* mapQueue() - map the Transmit QCU to a DCU 
*
*/

MANLIB_API void mapQueue
(
	A_UINT32 devNum, 
	A_UINT32 qcuNumber,
	A_UINT32 dcuNumber

)
{
    	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:setQueue\n", devNum);
		return;
	}
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->mapQueue(devNum, qcuNumber,dcuNumber);
}

/****************************************************************************
* clearKeyCache() - Clear the Key Cache Table 
*
*/

MANLIB_API void clearKeyCache
(
	A_UINT32 devNum
)
{
    	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:setQueue\n", devNum);
		return;
	}
	ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->clearKeyCache(devNum);
}

/**************************************************************************
* mdkProcessMultiAntSigStrength() - Process receive descriptor statistics
*                                   for multi-antenna related MIMO stats
*
* Extract and save required statistics.  
*
* RETURNS: 
*/
void mdkProcessMultiAntSigStrength
(
 A_UINT32 devNum,
 RX_STATS_TEMP_INFO *pStatsInfo
)
{
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32      ii, jj, kk;
	A_INT8        rssi;
	MULT_ANT_SIG_STRENGTH_STATS *pStatsMult;

	for (jj = 0; jj < 2; jj++) {

		kk = (jj>0) ? pStatsInfo->descRate : 0 ;
		pStatsMult = &(pStatsInfo->multAntSigStrength[kk]);

		for (ii = 0; ii < 4; ii++) {
			rssi = (A_INT8) (( (pStatsInfo->status3) >> (8*ii) ) & SIG_STRENGTH_MASK);

			// skip rssi = -128, they're invalid
			if (rssi > -128) {
				pStatsMult->rxAccumSignalStrengthAnt[ii] += rssi;
				pStatsMult->rxNumSigStrengthSamplesAnt[ii]++;
				pStatsMult->rxAvrgSignalStrengthAnt[ii] = (A_INT8) (pStatsMult->rxAccumSignalStrengthAnt[ii] /
																 (A_INT32)(pStatsMult->rxNumSigStrengthSamplesAnt[ii]) );

				if (rssi > pStatsMult->rxMaxSigStrengthAnt[ii]) {
					pStatsMult->rxMaxSigStrengthAnt[ii] = rssi;
				}

				if (rssi < pStatsMult->rxMinSigStrengthAnt[ii]) {
					pStatsMult->rxMinSigStrengthAnt[ii] = rssi;
				}
			}
		}			

	    // Chain0AntSel
		pStatsInfo->Chain0AntSel[kk].Count[( pStatsInfo->status2 >> CHAIN_0_ANT_SEL_S) & 0x1];
		pStatsInfo->Chain0AntSel[kk].totalNum++;

	    // Chain1AntSel
		pStatsInfo->Chain1AntSel[kk].Count[( pStatsInfo->status2 >> CHAIN_1_ANT_SEL_S) & 0x1];
		pStatsInfo->Chain1AntSel[kk].totalNum++;

	    // Chain0AntReq
		pStatsInfo->Chain0AntReq[kk].Count[( pStatsInfo->status2 >> CHAIN_0_ANT_REQ_S) & 0x1];
		pStatsInfo->Chain0AntReq[kk].totalNum++;

	    // Chain1AntReq
		pStatsInfo->Chain1AntReq[kk].Count[( pStatsInfo->status2 >> CHAIN_1_ANT_REQ_S) & 0x1];
		pStatsInfo->Chain1AntReq[kk].totalNum++;

	    // Chain_Strong
		pStatsInfo->ChainStrong[kk].Count[( pStatsInfo->status2 >> CHAIN_STRONG_S) & 0x1];
		pStatsInfo->ChainStrong[kk].totalNum++;
	}
}

void fillRxDescAndFrame(A_UINT32 devNum, RX_STATS_TEMP_INFO *statsInfo) {

	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 frame_info_len;
	A_UCHAR tmpFrameInfo[MAX_FRAME_INFO_SIZE];
	A_BOOL      falconTrue = FALSE;
	A_UINT32    ppm_data_padding = PPM_DATA_SIZE;
	A_UINT32    status1_offset, status2_offset, status3_offset; // legacy status1 - datalen/rate, 
	                                                            // legacy status2 - done/error/timestamp
	                                                            // new with falcon status3 - rssi for 4 ant



	falconTrue = isFalcon(devNum) || isDragon(devNum);

	if (falconTrue) {
		status1_offset = FIRST_FALCON_RX_STATUS_WORD;
		status2_offset = SECOND_FALCON_RX_STATUS_WORD;
		status3_offset = FALCON_ANT_RSSI_RX_STATUS_WORD;
		if (pLibDev->libCfgParams.chainSelect == 2) {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_DUAL_CHAIN;
		} else {
			ppm_data_padding = PPM_DATA_SIZE_FALCON_SINGLE_CHAIN;
		}
	} else {
		status1_offset = FIRST_STATUS_WORD;
		status2_offset = SECOND_STATUS_WORD;
		status3_offset = status2_offset; // to avoid illegal value. should never be used besides multAnt chips
	}

		pLibDev->devMap.OSmemRead(devNum, statsInfo->descToRead, 
					(A_UCHAR *)&(statsInfo->desc), sizeof(statsInfo->desc));

		statsInfo->bufferAddr = (statsInfo->desc).bufferPhysPtr;
//		statsInfo->status1 = (statsInfo->desc).hwStatus[0]; 
//		statsInfo->status2 = (statsInfo->desc).hwStatus[1]; 
		statsInfo->status1 = *( (A_UINT32 *)((A_UCHAR *)(&(statsInfo->desc)) + status1_offset) ); 
		statsInfo->status2 = *( (A_UINT32 *)((A_UCHAR *)(&(statsInfo->desc)) + status2_offset) ); 
		statsInfo->status3 = *( (A_UINT32 *)((A_UCHAR *)(&(statsInfo->desc)) + status3_offset) ); 
		statsInfo->totalBytes = (statsInfo->status1 & 0xfff);

		if (statsInfo->status2 & DESC_DONE) {
			frame_info_len = sizeof(WLAN_DATA_MAC_HEADER3) + sizeof(MDK_PACKET_HEADER);
			statsInfo->ppm_data_len = 0;
			if (pLibDev->rx.enablePPM) {
			  frame_info_len += ppm_data_padding;
			  statsInfo->ppm_data_len = ppm_data_padding;
			}

			pLibDev->devMap.OSmemRead(devNum, statsInfo->bufferAddr, 
						(A_UCHAR *)&(tmpFrameInfo), frame_info_len);
			
	//printf("SNOOP::fillRxDescAndFrame:enablePPM=%d:frame_info_len=%d\n", pLibDev->rx.enablePPM, frame_info_len);
			if (pLibDev->rx.enablePPM) {
			  memcpy( &(statsInfo->ppm_data[0]), &(tmpFrameInfo[0]), ppm_data_padding);
			  memcpy( &(statsInfo->frame_info[0]), &(tmpFrameInfo[0])+ppm_data_padding, (frame_info_len - ppm_data_padding));
			}
			else {
			  memcpy( &(statsInfo->frame_info[0]), &(tmpFrameInfo[0]), frame_info_len);
			}
			statsInfo->frame_info_len = frame_info_len;
		}

}

#if defined(__ATH_DJGPPDOS__)
/**************************************************************************
* milliTime - return time in milliSeconds 
*
* This routine calls a OS specific routine for gettting the time
* 
* RETURNS: time in milliSeconds
*/

A_UINT32 milliTime
(
	void
)
{
	A_UINT32 timeMilliSeconds;
	A_UINT64 tempTime = 0;
		
	tempTime = uclock() * 1000;
	timeMilliSeconds = (tempTime)/UCLOCKS_PER_SEC;
	return timeMilliSeconds;

}
#endif
