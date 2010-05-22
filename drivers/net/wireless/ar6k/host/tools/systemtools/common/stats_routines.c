#ifdef __ATH_DJGPPDOS__
#include <unistd.h>
#ifndef EILSEQ  
    #define EILSEQ EIO
#endif	// EILSEQ

 #define __int64	long long
 #define HANDLE long
 typedef unsigned long DWORD;
 #define Sleep	delay
 #include <bios.h>
 #include <dir.h>
#endif	// #ifdef __ATH_DJGPPDOS__

#ifdef ECOS
#include "dk_common.h"
#endif

#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "mdata.h"
#include <string.h>
#if defined (THIN_CLIENT_BUILD) || defined (WIN_CLIENT_BUILD)
#include "hwext.h"
#else
#include "errno.h"
#include "common_defs.h"
#include "mDevtbl.h"
#endif

#include "mConfig.h"

void getSignalStrengthStats
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


void fillRateThroughput
(
 TX_STATS_STRUCT *txStats,
 A_UINT32 descRate, 
 A_UINT32 dataBodyLen
)
{
	A_UINT32 descStartTime;
	A_UINT32 descEndTime;
	A_UINT32 bitsReceived;
	float newTxTime = 0;
	
	descStartTime = txStats[descRate].startTime;
	descEndTime = txStats[descRate].endTime;

	if(descStartTime < descEndTime) {
		bitsReceived = (txStats[descRate].goodPackets - txStats[descRate].firstPktGood)  \
			* (dataBodyLen + sizeof(MDK_PACKET_HEADER)) * 8;
		newTxTime = (float)((descEndTime - descStartTime) * 1024)/1000;  //puts the time in millisecs
		txStats[descRate].newThroughput = (A_UINT32)((float)bitsReceived / newTxTime);
	}
}

void extractTxStats
(
 TX_STATS_TEMP_INFO *pStatsInfo,
 TX_STATS_STRUCT *txStats
)
{
	A_UINT16 numRetries;

	/* first check for good packets */
	if (pStatsInfo->status1 & DESC_FRM_XMIT_OK) 
	{
		txStats[0].goodPackets ++;
		txStats[pStatsInfo->descRate].goodPackets ++;

		/* do any signal strength processig */
		getSignalStrengthStats(&(pStatsInfo->sigStrength[0]), 
			(A_INT8)((pStatsInfo->status2 >> BITS_TO_TX_SIG_STRENGTH) & SIG_STRENGTH_MASK));
		txStats[0].ackSigStrengthAvg = pStatsInfo->sigStrength[0].rxAvrgSignalStrength;
		txStats[0].ackSigStrengthMax = pStatsInfo->sigStrength[0].rxMaxSigStrength;
		txStats[0].ackSigStrengthMin = pStatsInfo->sigStrength[0].rxMinSigStrength;

		getSignalStrengthStats(&(pStatsInfo->sigStrength[pStatsInfo->descRate]), 
			(A_INT8)((pStatsInfo->status2 >> BITS_TO_TX_SIG_STRENGTH) & SIG_STRENGTH_MASK));
		txStats[pStatsInfo->descRate].ackSigStrengthAvg = pStatsInfo->sigStrength[pStatsInfo->descRate].rxAvrgSignalStrength;
		txStats[pStatsInfo->descRate].ackSigStrengthMax = pStatsInfo->sigStrength[pStatsInfo->descRate].rxMaxSigStrength;
		txStats[pStatsInfo->descRate].ackSigStrengthMin = pStatsInfo->sigStrength[pStatsInfo->descRate].rxMinSigStrength;
	}
	else
	{
		/* processess error statistics */
		if (pStatsInfo->status1 & DESC_FIFO_UNDERRUN)
		{
			txStats[0].underruns ++;
			txStats[pStatsInfo->descRate].underruns ++;
		}
		else if (pStatsInfo->status1 & DESC_EXCESS_RETIES)
		{
			txStats[0].excessiveRetries ++;
			txStats[pStatsInfo->descRate].excessiveRetries ++;
		}
	}

	/* count the number of retries */
	numRetries = (A_UINT16)((pStatsInfo->status1 >> BITS_TO_SHORT_RETRIES) & RETRIES_MASK);
	if (numRetries == 1) {
		txStats[0].shortRetry1++;
		txStats[pStatsInfo->descRate].shortRetry1++;
	}
	else if (numRetries == 2) {
		txStats[0].shortRetry2++;
		txStats[pStatsInfo->descRate].shortRetry2++;
	}
	else if (numRetries == 3) {
		txStats[0].shortRetry3++;
		txStats[pStatsInfo->descRate].shortRetry3++;
	}
	else if (numRetries == 4) {
		txStats[0].shortRetry4++;
		txStats[pStatsInfo->descRate].shortRetry4++;
	}
	else if (numRetries == 5) {
		txStats[0].shortRetry5++;
		txStats[pStatsInfo->descRate].shortRetry5++;
	}
	else if ((numRetries >= 6) && (numRetries <= 10)) {
		txStats[0].shortRetry6to10++;
		txStats[pStatsInfo->descRate].shortRetry6to10++;
	}
	else if ((numRetries >=11) && (numRetries <= 15)) {
		txStats[0].shortRetry11to15++;
		txStats[pStatsInfo->descRate].shortRetry11to15++;
	}
	else;

	numRetries = (A_UINT16)((pStatsInfo->status1 >> BITS_TO_LONG_RETRIES) & RETRIES_MASK);
	if (numRetries == 1) {
		txStats[0].longRetry1++;
		txStats[pStatsInfo->descRate].longRetry1++;
	}
	else if (numRetries == 2) {
		txStats[0].longRetry2++;
		txStats[pStatsInfo->descRate].longRetry2++;
	}
	else if (numRetries == 3) {
		txStats[0].longRetry3++;
		txStats[pStatsInfo->descRate].longRetry3++;
	}
	else if (numRetries == 4) {
		txStats[0].longRetry4++;
		txStats[pStatsInfo->descRate].longRetry4++;
	}
	else if (numRetries == 5) {
		txStats[0].longRetry5++;
		txStats[pStatsInfo->descRate].longRetry5++;
	}
	else if ((numRetries >= 6) && (numRetries <= 10)) {
		txStats[0].longRetry6to10++;
		txStats[pStatsInfo->descRate].longRetry6to10++;
	}
	else if ((numRetries >=11) && (numRetries <= 15)) {
		txStats[0].longRetry11to15++;
		txStats[pStatsInfo->descRate].longRetry11to15++;
	}
	else;

	return;
}



void fillTxStats
(
 A_UINT32 devNumIndex, 
 A_UINT32 descAddress,
 A_UINT32 numDesc,
 A_UINT32 dataBodyLen,
 A_UINT32 txTime,
 TX_STATS_STRUCT *txStats
)
{
	TX_STATS_TEMP_INFO	statsInfo;
	A_UINT32 i, statsLoop, tempRate;
	A_UINT32 bitsReceived;
	A_UINT32 descStartTime = 0;
	A_UINT32 descTempTime = 0;
	A_UINT32 descMidTime = 0;
	A_UINT32 descEndTime = 0;
	float    newTxTime = 0;
	A_UINT16 firstPktGood = 0;
	A_UINT32 lastDescRate = 0;
//	A_UINT32 descRate;

#ifdef _TIME_PROFILE
A_UINT32 start,end;
    start = milliTime();
#endif
#ifdef THIN_CLIENT_BUILD
#else
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNumIndex];
#endif

#ifndef MALLOC_ABSENT
	memset(txStats, 0, sizeof(TX_STATS_STRUCT) * STATS_BINS);
	// traverse the descriptor list and get the stats from each one
	memset(&statsInfo, 0, sizeof(TX_STATS_TEMP_INFO));
#else
	A_MEMSET(txStats, 0, sizeof(TX_STATS_STRUCT) * STATS_BINS);
	// traverse the descriptor list and get the stats from each one
	A_MEMSET(&statsInfo, 0, sizeof(TX_STATS_TEMP_INFO));
#endif

	for(statsLoop = 0; statsLoop < STATS_BINS; statsLoop++) {
		statsInfo.sigStrength[statsLoop].rxMinSigStrength = 127;
	}

	statsInfo.descToRead = descAddress;
	for (i = 0; i < numDesc; i++) {
#if defined (THIN_CLIENT_BUILD) || defined (WIN_CLIENT_BUILD)

#if defined(AR5523)
	    tempRate = (hwMemRead32(devNumIndex, (statsInfo.descToRead + FOURTH_CONTROL_WORD)) >> BITS_TO_TX_DATA_RATE0) & VENICE_DATA_RATE_MASK;
		statsInfo.descRate = descRate2bin(tempRate);
	    statsInfo.status1 = hwMemRead32(devNumIndex, statsInfo.descToRead + FIRST_VENICE_STATUS_WORD);
	    statsInfo.status2 = hwMemRead32(devNumIndex, statsInfo.descToRead + SECOND_VENICE_STATUS_WORD);
#endif

#if defined(AR6000)
	    tempRate = (hwMemRead32(devNumIndex, (statsInfo.descToRead + FOURTH_CONTROL_WORD)) >> BITS_TO_TX_DATA_RATE0) & VENICE_DATA_RATE_MASK;
	    statsInfo.descRate = descRate2bin(tempRate);
	    statsInfo.status1 = hwMemRead32(devNumIndex, statsInfo.descToRead + FIRST_FALCON_TX_STATUS_WORD);
	    statsInfo.status2 = hwMemRead32(devNumIndex, statsInfo.descToRead + SECOND_FALCON_TX_STATUS_WORD);
#endif

#else
	 if (isDragon(devNumIndex)) {
        A_UINT32 words[6];
		pLibDev->devMap.OSmemRead(devNumIndex, statsInfo.descToRead + FOURTH_CONTROL_WORD, 
					(A_UCHAR *)&(words), sizeof(words));

	    tempRate = (words[0]>> BITS_TO_TX_DATA_RATE0) & VENICE_DATA_RATE_MASK;
		statsInfo.descRate = descRate2bin(statsInfo.descRate);
        memcpy(&(statsInfo.status1), &words[4], 8);
     }
	 else {
		statsInfo.descRate = ar5kInitData[pLibDev->ar5kInitIndex].pMacAPI->txGetDescRate(devNumIndex, statsInfo.descToRead);
		statsInfo.descRate = descRate2bin(statsInfo.descRate);
		pLibDev->devMap.OSmemRead(devNumIndex, statsInfo.descToRead + pLibDev->txDescStatus1, 
					(A_UCHAR *)&(statsInfo.status1), sizeof(statsInfo.status1));
		pLibDev->devMap.OSmemRead(devNumIndex, statsInfo.descToRead + pLibDev->txDescStatus2, 
						(A_UCHAR *)&(statsInfo.status2), sizeof(statsInfo.status2));
	 }
#endif


		if (statsInfo.status2 & DESC_DONE) {
	
			extractTxStats(&statsInfo, txStats);
			if(i == 0) {
				descStartTime = (statsInfo.status1 >> 16) & 0xffff;
				descMidTime = descStartTime;
				if(statsInfo.status1 & DESC_FRM_XMIT_OK) {
					firstPktGood = 1; //need to remove the 1st packet from new throughput calculation
					txStats[statsInfo.descRate].firstPktGood = 1;
				}

				//update the rate start time
				txStats[statsInfo.descRate].startTime = descStartTime;
				txStats[statsInfo.descRate].endTime = descStartTime;
			}
			else if ( i == numDesc - 1) {
				descEndTime = descEndTime + ((statsInfo.status1 >> 16) & 0xffff);
				if(descEndTime < descMidTime) {
					descEndTime += 0x10000;
				}
				//update the end time for the current rate
				txStats[statsInfo.descRate].endTime = descEndTime;
				fillRateThroughput(txStats, lastDescRate, dataBodyLen);
			}
			else {
				descTempTime = (statsInfo.status1 >> 16) & 0xffff;
				if (descTempTime < descMidTime) {
					descMidTime = 0x10000 + descTempTime;
				}
				else {
					descMidTime = descTempTime;
				}
				txStats[statsInfo.descRate].endTime = descTempTime;
				if(lastDescRate != statsInfo.descRate) {
					//this is the next rate so update the time
					txStats[statsInfo.descRate].startTime = descTempTime;

					//update the throughput on the last rate
					if(statsInfo.status1 & DESC_FRM_XMIT_OK) {
						//need to remove the 1st packet from new throughput calculation
						txStats[statsInfo.descRate].firstPktGood = 1;
					}

					fillRateThroughput(txStats, lastDescRate, dataBodyLen);
				}
			}
				
		}
		else {
			//assume that if find a descriptor that is not done, then
			//none of the rest of the descriptors will be done, since assume
			//tx has completed by time get to here
#if defined (THIN_CLIENT_BUILD) 
			uiPrintf("Device Number %d:txGetStats: found a descriptor that is not done,  \
				stopped gathering stats %08lx %08lx\n", devNumIndex, statsInfo.status1, statsInfo.status2);
#else
#if defined (WIN_CLIENT_BUILD)
			printf("Device Number %d:txGetStats: found a descriptor that is not done,  \
				stopped gathering stats %08lx %08lx\n", devNumIndex, statsInfo.status1, statsInfo.status2);
#else 
			mError(devNumIndex, EIO, 
				"Device Number %d:txGetStats: found a descriptor that is not done, stopped gathering stats %08lx %08lx\n", devNumIndex,
				statsInfo.status1, statsInfo.status2);
#endif
#endif
			break;
		}

		lastDescRate = statsInfo.descRate;
		statsInfo.descToRead += sizeof(MDK_ATHEROS_DESC);		
	}

	if (txTime == 0) {
//		mError(devNumIndex, EINVAL, "Device Number %d:transmit time is zero, not calculating throughput\n", devNumIndex);
	}
	else {
		//calculate the throughput
		bitsReceived = txStats[0].goodPackets * (dataBodyLen + sizeof(MDK_PACKET_HEADER)) * 8;
		txStats[0].throughput = bitsReceived / txTime;
	}

	if(descStartTime < descEndTime) {
		bitsReceived = (txStats[0].goodPackets - firstPktGood) * (dataBodyLen + sizeof(MDK_PACKET_HEADER)) * 8;
		newTxTime = (float)((descEndTime - descStartTime) * 1024)/1000;  //puts the time in millisecs
		txStats[0].newThroughput = (A_UINT32)((float)bitsReceived / newTxTime);
	}
		
#ifdef _TIME_PROFILE
 end = milliTime();
 printf("fill tx stats time = %d\n", (end-start));
#endif

	return;
}


