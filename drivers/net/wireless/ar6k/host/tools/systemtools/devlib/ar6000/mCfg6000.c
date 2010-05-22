/*
 *  Copyright © 2005 Atheros Communications, Inc.,  All Rights Reserved.
 *
 */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mCfg6000.c#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mCfg6000.c#1 $"

#ifdef VXWORKS
#include "vxworks.h"
#endif

#ifdef __ATH_DJGPPDOS__
#define __int64 long long
typedef unsigned long DWORD;
#define Sleep   delay
#endif  // #ifdef __ATH_DJGPPDOS__

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifndef VXWORKS
#include <malloc.h>
#endif
#include "wlantype.h"

#include "mCfg6000.h"
#include "mEep6000.h"
#include "mCfg211.h"

#include "athreg.h"
#include "manlib.h"
#include "mEeprom.h"
#include "mEepStruct6000.h"
#include "mConfig.h"

#include "ar5211reg.h"

/* Refrence clock table, [XTAL][ChannelSpacing] */
static const 
A_UINT16 gRefClock[5][5] = {{8,  4,  4,  8,  16},
                            {20, 20, 20, 20, 20},
                            {40, 50, 100,200,400},
                            {40, 20, 20, 40, 40},
                            {8,  4,  4,  8,  16}};

#if defined(AR6001)
static const A_UINT16 gXtalFreq[] = { 192,260,400,520,384,240};
static A_UINT32 xTalFreq    = 2; // 40 MHz crystalclk
#elif defined(AR6002)
static const A_UINT16 gXtalFreq[] = { 192,260,400,260,384,240};
static A_UINT32 xTalFreq    = 1; // 26 MHz crystalclk
#endif

/* Amode Selection table, [XTAL][Channel Spacing] */
static const 
A_UINT16 gAmodeRefSel[5][4] = {{0,0,1,2},
                               {0,0,0,0},
                               {0,1,2,3},
                               {0,0,1,1},
                               {0,0,1,2}};


#define DIV_2_MASK  0x4000
#define FRACTIONAL_MODE  0x8000
#define XTAL_MASK        0xfff

/* Need a proper header file ?*/
#define PHY_SYNTH_CONTROL 0x9874
#define PHY_CCK_TX_CTRL   0xa204
#define PHY_CCK_TX_CTRL_JAPAN    0x00000010

/**************************************************************************
 * setChannelFractionalNAr6000 - Perform the algorithm to change the channel
 *                    for AR6000 - currently using override Integer mode to match Bank6 INI setting.
 *
 */
A_BOOL setChannelFractionalNAr6000
(
 A_UINT32 devNum,
 A_UINT32 freq
)
{
    A_UINT32 channelSel  = 0;
    A_UINT32 channelFrac = 0;  
    A_UINT32 bModeSynth  = 1;
    A_UINT32 aModeRefSel = 0;
    A_UINT32 reg32       = 0;
    A_UINT32 chanFreq = freq * 10;
    A_UINT32 refClock = (A_UINT32)gXtalFreq[xTalFreq];
    A_UINT32 temp;
    LIB_DEV_INFO	*pLibDev = gLibInfo.pLibDevArray[devNum];

    if (freq < 4800) {
        chanFreq *= 4;
        refClock *= 3;
    } else  {
        chanFreq *= 2;
        refClock *= 3;
    }
    if(freq == 2484) {
        REGW(devNum, 0xa204, REGR(devNum, 0xa204) | 0x10);
    }
    
    if (pLibDev->libCfgParams.refClock & DIV_2_MASK) {
		refClock = refClock / 2;
	}
	
//printf("SNOOP: chanFreq = %d, refClock = %d\n", chanFreq, refClock); 
    channelSel = chanFreq / refClock;
//printf("SNOOP: channelSel = %x\n", channelSel);
    channelSel = reverseBits(channelSel, 8);

    channelFrac = chanFreq % refClock;
    channelFrac = temp = channelFrac << 17;
    channelFrac = channelFrac / refClock;
    temp = (temp % refClock) * 2;
    if (temp >= refClock) channelFrac += 1;
//printf("SNOOP: channelFrac = %x\n", channelFrac);
    channelFrac = reverseBits(channelFrac, 17);

    reg32 = (channelFrac << 12) | (channelSel << 4) | (aModeRefSel << 2) | 
            (bModeSynth << 1) | (1 << 29) | 0x1;
    REGW(devNum, PHY_BASE + (0x27 << 2), reg32 & 0xff);

    reg32 >>= 8;
    REGW(devNum, PHY_BASE + (0x27 << 2), reg32 & 0xff);

    reg32 >>= 8;
    REGW(devNum, PHY_BASE + (0x27 << 2), reg32 & 0xff);

    reg32 >>= 8;
    REGW(devNum, PHY_BASE + (0x37 << 2), reg32 & 0xff);

    mSleep(1);
    return TRUE;
}

void setChannelFractionalNAr6002
(
    A_UINT32 devNum,
    A_UINT32 freq
)
{
    A_UINT32 channelSel  = 0;
    A_UINT32 channelFrac = 0;  
    A_UINT32 revChannelSel  = 0;
    A_UINT32 revChannelFrac = 0;  
    A_UINT32 fracMode  = 1;
    A_UINT32 aModeRefSel = 0;
    A_UINT32 reg32       = 0;
    A_UINT32 chanFreq = freq * 10;
    A_UINT32 refClock = (A_UINT32)gXtalFreq[xTalFreq];
    A_UINT32 temp;
    A_UINT32 synthSettle;
    A_UINT32 bMode;
    LIB_DEV_INFO    *pLibDev = gLibInfo.pLibDevArray[devNum];

    /* Synthesizer settle will be vary depend on XTAL and Operation mode*/
    synthSettle = 100;
    /* Convert to 4 base band clock units */
    if (pLibDev->mode == MODE_11G)
        synthSettle *= 11; 
    else
        synthSettle *= 10; 
    REGW(devNum, PHY_RX_DELAY, synthSettle);

    if (freq < 4800) {
        chanFreq *= 4;
        refClock *= 3;
        bMode = 1;
    } else  {
        chanFreq *= 2;
        refClock *= 3;
        bMode = 0;
    }

    /*if (pLibDev->libCfgParams.refClock & DIV_2_MASK): Mercury DIV_2_MASK is not supported following s/w?  */

    channelSel = chanFreq / refClock;
    revChannelSel = reverseBits(channelSel, 8);

    channelFrac = chanFreq % refClock;
    channelFrac = temp = channelFrac << 17;
    channelFrac = channelFrac / refClock;
    temp = (temp % refClock) * 2;
    if (temp >= refClock) channelFrac += 1;
    revChannelFrac = reverseBits(channelFrac, 17);

/* This is a new register in .13 radio
reg 29: synth control (.13 radio interface)
bmode                   rw      1'b0    1'b0            # [29]    2.4GHz band select
Fracmode                rw      1'b0    1'b0            # [28]    (reserved) band selection (japan mode)
AmodeRefSel             rw      2'h0    2'h0            # [27:26] (reserved) A mode channel spacing sel
channel                 rw      9'h0    9'h0            # [25:17] synthesizer channel select
chanFrac                rw      17'h0   17'h0           # [16:0]  synthesizer fractional channel input
*/
    reg32 = (bMode << 29) | (fracMode << 28) | (aModeRefSel << 26) | (channelSel << 17) | (channelFrac);
    REGW(devNum, PHY_SYNTH_CONTROL, reg32);

    mSleep(1);
    return;
}

A_BOOL setChannelAr6002
(
 A_UINT32 devNum,
 A_UINT32 freq      // New channel
)
{
    A_UINT32        synthDelay, testReg;
    A_UINT32        i;
    A_UINT32        txctl;
    LIB_DEV_INFO    *pLibDev = gLibInfo.pLibDevArray[devNum];

    if(pLibDev->libCfgParams.refClock == REF_CLK_DYNAMIC) {
        //this is default, so default to 40MHz xtal and integer mode
        xTalFreq = 1;
    }
    else {
        //fractionalN mode, extract the xtal freq index
        xTalFreq = pLibDev->libCfgParams.refClock & XTAL_MASK;
        if(xTalFreq > 5) {
            mError(devNum, EIO, "Illegal crystal freq index, valid values are 0 - 5 (19.2, 26, 40, 52, 38.4, 24\n");
            return(FALSE);
        }
    }
    //printf("... setChannelAr6002: refClock %d xTalFreq %d\n", pLibDev->libCfgParams.refClock, xTalFreq); 

    setChannelFractionalNAr6002(devNum, freq);

    txctl = REGR(devNum, PHY_CCK_TX_CTRL);
    if (freq == 2484) {
        /* Enable channel spreading for channel 14 */
        REGW(devNum, PHY_CCK_TX_CTRL, txctl | PHY_CCK_TX_CTRL_JAPAN);
    } else {
        REGW(devNum, PHY_CCK_TX_CTRL, txctl & ~PHY_CCK_TX_CTRL_JAPAN);
    }

    /*
     * Wait for the frequency synth to settle (synth goes on
     * via AR_PHY_ACTIVE_EN).  Read the phy active delay register.
     * Value is in 4 BB clock increments.
     */
    synthDelay = REGR(devNum, PHY_RX_DELAY) & PHY_RX_DELAY_M;
    if (pLibDev->mode == MODE_11G)
        synthDelay /= 11; 
    else
        synthDelay /= 10; 
    //printf("...synthDelay %d\n", synthDelay);

    /* 
     * There is an issue if the calibration starts before
     * the base band timeout completes.  This could result in the
     * rx_clear false triggering.  As a workaround we add delay an
     * extra BASE_ACTIVATE_DELAY usecs to ensure this condition
     * does not happen.
     */
    mSleep(1);
    //A_DELAY_USECS(synthDelay + 100);

    return TRUE;
}


/**************************************************************************
 * setChannelAr6000 - Perform the algorithm to change the channel
 *                    for AR6000 - currently using override Integer mode to match Bank6 INI setting.
 *
 */
extern A_BOOL setChannelAr2413 ( A_UINT32 devNum, A_UINT32 freq);
A_BOOL setChannelAr6000
(
 A_UINT32 devNum,
 A_UINT32 freq      // New channel
)
{
#if defined(MERCURY_EMULATION)   // Mercury emulation, copied setChannelAr2413
    setChannelAr2413(devNum, freq);
#elif defined(AR6001)
    A_UINT32 channelSel  = 0;
    A_UINT32 channelFrac = 0;
    A_UINT32 bModeSynth  = 0;
    A_UINT32 aModeRefSel = 0;
    A_UINT32 chanFreq    = freq * 10;
    A_UINT32 reg32       = 0;
    A_UINT32 refClock;
    A_UINT32 temp;
    LIB_DEV_INFO	*pLibDev = gLibInfo.pLibDevArray[devNum];

    if(pLibDev->libCfgParams.refClock == REF_CLK_DYNAMIC) {
        //this is default, so default to 40MHz xtal and integer mode
        xTalFreq = 2;
    } 
    else if (pLibDev->libCfgParams.refClock & FRACTIONAL_MODE) {
        //fractionalN mode, extract the xtal freq index
        xTalFreq = pLibDev->libCfgParams.refClock & XTAL_MASK;
        if(xTalFreq > 5) {
	    mError(devNum, EIO, "Illegal crystal freq index, valid values are 0 - 5 (19.2, 26, 40, 52, 38.4, 24\n");
	    return(FALSE);
	}
		
	//make the call to fractionalN mode function
	return(setChannelFractionalNAr6000(devNum, freq));
    }
    else {
	//continue with integer mode, but use the supplied xtal freq
	xTalFreq = pLibDev->libCfgParams.refClock;
	if(xTalFreq > 4) {
	    mError(devNum, EIO, "Illegal crystal freq index, valid values are 0 - 4 (19.2, 26, 40, 52, 38.4\n");
	    return(FALSE);
	}
    }
		
    if (freq < 4800) {
        chanFreq *= 4;
        refClock = (A_UINT32) gRefClock[xTalFreq][0];
    } else {
        refClock = (A_UINT32) gRefClock[xTalFreq][1];
        aModeRefSel = (A_UINT32) gAmodeRefSel[xTalFreq][0];
        aModeRefSel = reverseBits(aModeRefSel, 2);
        chanFreq *= 2;
    }
    if(freq == 2484) {
        REGW(devNum, 0xa204, REGR(devNum, 0xa204) | 0x10);
    }

    temp = chanFreq / refClock;
    channelSel = reverseBits(temp & 0xff, 8);
    channelFrac = reverseBits(((temp >> 8) & 0x7f) << 10, 17);

    reg32 = (channelFrac << 12) | (channelSel << 4) | (aModeRefSel << 2) | (bModeSynth << 1) | (1 << 29) | 0x1;
    REGW(devNum, PHY_BASE + (0x27 << 2), reg32 & 0xff);

    reg32 >>= 8;
    REGW(devNum, PHY_BASE + (0x27 << 2), reg32 & 0xff);

    reg32 >>= 8;
    REGW(devNum, PHY_BASE + (0x27 << 2), reg32 & 0xff);

    reg32 >>= 8;
    REGW(devNum, PHY_BASE + (0x37 << 2), reg32 & 0xff);

    mSleep(1);
    return TRUE;
#elif defined(AR6002)
    return(setChannelAr6002(devNum, freq));
#endif
}

/**************************************************************************
* initPowerAr6000 - Set the power for the AR2413 chips
*
*/
void initPowerAr6000
(
    A_UINT32 devNum,
    A_UINT32 freq,
    A_UINT32  override,
    A_UCHAR  *pwrSettings
)
{
    LIB_DEV_INFO        *pLibDev = gLibInfo.pLibDevArray[devNum];
    A_UINT32 flag_0dBm;
    A_INT8  __power_offset;

    //only override the power if the eeprom has been read
    if((!pLibDev->eePromLoad) || (!pLibDev->eepData.infoValid)) {
        return;
    }

#if 1
    //after we load the eeprom data, we should use the is0dBm flag in eeprom to tell isZerodBm, +ddt
    //printf("--->opFlags %x\n",pLibDev->ar6kEep->baseEepHeader.opFlags);
    flag_0dBm = pLibDev->ar6kEep->baseEepHeader.opFlags & AR6000_OPFLAGS_0dBm;
    __power_offset = pLibDev->ar6kEep->baseEepHeader.negPwrOffset;
    setZerodBm(flag_0dBm);
    setPowerRef(__power_offset);
#endif

    if((override) || (pwrSettings != NULL)) {
        mError(devNum, EINVAL, "Override of power not supported.  Disable eeprom load and change config file instead\n");
        return;
    }

    if (((pLibDev->eepData.version >> 12) & 0xF) == AR6000_EEP_VER) {
        /* AR6000 specific struct and function programs eeprom header writes (board data overrides) */
        ar6000EepromSetBoardValues(devNum, freq);

        /* AR6000 specific struct and function performs all tx power writes */      
        ar6000EepromSetTransmitPower(devNum, freq, NULL);
        return;
    } else {
        mError(devNum, EINVAL, "initPowerAr6000: Illegal eeprom version %d\n", pLibDev->eepData.version);
        return;
    }
    
    return;
}
