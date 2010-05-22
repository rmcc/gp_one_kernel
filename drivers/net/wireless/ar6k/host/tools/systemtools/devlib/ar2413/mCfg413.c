/*
 *  Copyright © 2003 Atheros Communications, Inc.,  All Rights Reserved.
 *
 */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar2413/mCfg413.c#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar2413/mCfg413.c#1 $"

#ifdef VXWORKS
#include "vxworks.h"
#endif

#ifdef __ATH_DJGPPDOS__
#define __int64	long long
typedef unsigned long DWORD;
#define Sleep	delay
#endif	// #ifdef __ATH_DJGPPDOS__

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifndef VXWORKS
#include <malloc.h>
#endif
#include "wlantype.h"

#include "mCfg413.h"
#include "mCfg211.h"

#include "athreg.h"
#include "manlib.h"
#include "mEeprom.h"
#include "mConfig.h"

#include "ar5211reg.h"

/**************************************************************************
 * setChannelAr2413 - Perform the algorithm to change the channel
 *					  for AR2413 and AR5413 based adapters
 *
 */
A_BOOL setChannelAr2413
(
 A_UINT32 devNum,
 A_UINT32 freq		// New channel
)
{
	A_UINT32 channelSel = 0;
	A_UINT32 bModeSynth = 0;
	A_UINT32 aModeRefSel = 0;
	A_UINT32 regVal = 0;
    LIB_DEV_INFO	*pLibDev = gLibInfo.pLibDevArray[devNum];

	if(freq < 4800) {
		if(((freq - 2272) % 5) == 0) {
			channelSel = (16 + (freq-2272)/5);

			bModeSynth = 0;
			writeField(devNum, "rf_DerbyChanSelMode", 1);
    		channelSel = (channelSel << 2) & 0xff;
		}
		else if (((freq - 2274) % 5) == 0) {
			channelSel = (10 + (freq-2274)/5);

			bModeSynth = 1;
			writeField(devNum, "rf_DerbyChanSelMode", 1);
    		channelSel = (channelSel << 2) & 0xff;
		}
		else if ((freq >= 2272) && (freq <= 2527)) {
			channelSel = freq - 2272;
			bModeSynth = 0;    //this is a don't care

			writeField(devNum, "rf_DerbyChanSelMode", 0);
		}
		else {
			mError(devNum, EINVAL, "%d is an illegal derby driven channel\n", freq);
			return(0);
		}
	
		channelSel = reverseBits(channelSel, 8);
		if(freq == 2484) {
			REGW(devNum, 0xa204, REGR(devNum, 0xa204) | 0x10);
		}
	}
	else {
		switch(pLibDev->libCfgParams.refClock) {
		case REF_CLK_DYNAMIC:
			if (pLibDev->channelMasks & QUARTER_CHANNEL_MASK) {
				channelSel = reverseBits((A_UINT32)((freq-4800)/2.5 + 1), 8);
				aModeRefSel = reverseBits(0, 2);				
			}
			else if ( (freq % 20) == 0 ){
				channelSel = reverseBits(((freq-4800)/20 << 2), 8);
//				aModeRefSel = reverseBits(3, 2);
				aModeRefSel = reverseBits(1, 2);
			} else if ( (freq % 10) == 0) {
				channelSel = reverseBits(((freq-4800)/10 << 1), 8);
//				aModeRefSel = reverseBits(2, 2);
				aModeRefSel = reverseBits(1, 2);
			} else if ((freq % 5) == 0) {
				channelSel = reverseBits((freq-4800)/5, 8);
				aModeRefSel = reverseBits(1, 2);
			} 

			break;

		case REF_CLK_2_5:
			channelSel = (A_UINT32)((freq-4800)/2.5);
			if (pLibDev->channelMasks & QUARTER_CHANNEL_MASK) {
				channelSel += 1;
			}
			channelSel = reverseBits(channelSel, 8);
			aModeRefSel = reverseBits(0, 2);
			break;

		case REF_CLK_5:
			channelSel = reverseBits((freq-4800)/5, 8);
			aModeRefSel = reverseBits(1, 2);
			break;

		case REF_CLK_10:
			channelSel = reverseBits(((freq-4800)/10 << 1), 8);
			aModeRefSel = reverseBits(2, 2);
			break;

		case REF_CLK_20:
			channelSel = reverseBits(((freq-4800)/20 << 2), 8);
			aModeRefSel = reverseBits(3, 2);
			break;


		default:
			if (pLibDev->channelMasks & QUARTER_CHANNEL_MASK) {
				channelSel = reverseBits((A_UINT32)((freq-4800)/2.5 + 1), 8);
				aModeRefSel = reverseBits(0, 2);				
			}
			else if ( (freq % 20) == 0 ){
				channelSel = reverseBits(((freq-4800)/20 << 2), 8);
//				aModeRefSel = reverseBits(3, 2);
				aModeRefSel = reverseBits(1, 2);
			} else if ( (freq % 10) == 0) {
				channelSel = reverseBits(((freq-4800)/10 << 1), 8);
//				aModeRefSel = reverseBits(2, 2);
				aModeRefSel = reverseBits(1, 2);
			} else if ((freq % 5) == 0) {
				channelSel = reverseBits((freq-4800)/5, 8);
				aModeRefSel = reverseBits(1, 2);
			} 

			break;
		}
		writeField(devNum, "rf_DerbyChanSelMode", 0);
	}
//printf("SNOOP: aModeRefSel = %d\n", reverseBits(aModeRefSel, 2));
//printf("SNOOP: channelSel (after reverse) = %x (%x)\n", channelSel, reverseBits(channelSel, 8));

	regVal = (channelSel << 4) | (aModeRefSel << 2) | (bModeSynth << 1)| (1 << 12) | 0x1;
	REGW(devNum, PHY_BASE+(0x27<<2), (regVal & 0xff));
    regVal = (regVal >> 8) & 0x7f;
	REGW(devNum, PHY_BASE+(0x36<<2), regVal);		
	mSleep(1);
	return(1);
}


/**************************************************************************
* initPowerAr5211 - Set the power for the AR2413 chips
*
*/
void initPowerAr2413
(
	A_UINT32 devNum,
	A_UINT32 freq,
	A_UINT32  override,
	A_UCHAR  *pwrSettings
)
{
    LIB_DEV_INFO		*pLibDev = gLibInfo.pLibDevArray[devNum];
	A_INT16				ratesArray[NUM_RATES];

	memset(ratesArray, 0, NUM_RATES * sizeof(A_INT16));
	//only override the power if the eeprom has been read
	if((!pLibDev->eePromLoad) || (!pLibDev->eepData.infoValid)) {
		return;
	}

	if((override) || (pwrSettings != NULL)) {
		mError(devNum, EINVAL, "Override of power not supported.  Disable eeprom load and change config file instead\n");
		return;
	}

	if(((pLibDev->eepData.version >> 12) & 0xF) >= 5) {
		programHeaderInfo(devNum, pLibDev->p16kEepHeader, (A_UINT16)freq, pLibDev->mode);

		//get the max power values
		getMaxRDPowerlistForFreq(devNum, (A_UINT16)freq, ratesArray);
        
		//set the max power and pdadc values
		forcePowerTxMax(devNum, ratesArray);
		
		return;
	}
	else {
		mError(devNum, EINVAL, "initPowerAr2413: Illegal eeprom version\n");
		return;
	}
	
	return;
}

void pllProgramAr5413
(
 	A_UINT32 devNum,
 	A_UINT32 turbo
)
{
	LIB_DEV_INFO	*pLibDev = gLibInfo.pLibDevArray[devNum];
	A_UINT32 reset;

	reset = 0;

	REGW(devNum, 0xa200, 0);
	if((pLibDev->mode == MODE_11A) || (turbo == TURBO_ENABLE) || (pLibDev->mode == MODE_11O))	{
//		REGW(devNum, 0x987c, 0xea) ; 
		REGW(devNum, 0x987c, 0x4) ; 
	}
	else {
		REGW(devNum, 0x987c, 0xeb);  // clk_ref * 22 / 5
	} 		
	reset = 1;

	if (reset) {
		hwResetAr5211(devNum, MAC_RESET | BB_RESET);
	}
}
