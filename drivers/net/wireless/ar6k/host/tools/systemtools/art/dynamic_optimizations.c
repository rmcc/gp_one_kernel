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
#endif	// #ifdef __ATH_DJGPPDOS__

#ifdef _WINDOWS
#include <windows.h>
#endif 
#include <stdio.h>
#ifndef LINUX
#include <conio.h>
#endif
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include "wlantype.h"   /* typedefs for A_UINT16 etc.. */
#include "wlanproto.h"
#include "athreg.h"
#include "manlib.h"     /* The Manufacturing Library */
#include "MLIBif.h"     /* Manufacturing Library low level driver support functions */
#ifdef JUNGO
#include "mld.h"        /* Low level driver information */
#endif
#include "common_hw.h"        /* Low level driver information */
#ifdef __ATH_DJGPPDOS__
#include "mlibif_dos.h"
#endif

#include "manlibInst.h" /* The Manufacturing Library Instrument Library extension */
#include "mEeprom.h"    /* Definitions for the data structure */
//#include "maui_cal.h"   /* Definitions for the Calibration Library */
#include "test.h"
#include "dk_cmds.h"
#include "dynamic_optimizations.h"

#if defined(LINUX) || defined(__linux__)
#include "linux_ansi.h"
#endif

#include "art_if.h"

extern A_UINT32 swDeviceID;

static A_UCHAR  rxStation[6] = {0x10, 0x11, 0x11, 0x11, 0x11, 0x01};	// DUT
static A_UCHAR  txStation[6] = {0x20, 0x22, 0x22, 0x22, 0x22, 0x02};	// Golden
static A_UCHAR  pattern[2]	 = {0xaa, 0x55};	


GAIN_OPTIMIZATION_LADDER gainLadder = { 11, //numStepsInLadder
					NUM_PARAMS_IN_GAIN_OPTIMIZATION_STEP, //numParamsInStep
					4, //defaultStepNum
					4, //currStepNum
					0, //currGain
					0, //gainFCorrection
					0, //targetGain
					20,
					35,
					FALSE, //gainLadderActive
					{"bb_tx_clip", "rf_pwd_90", "rf_pwd_84", "rf_rfgainsel", "bb_tx_dac_scale_cck", "rf_b_ob", "rf_b_db"}, //paramName
					{ { {4, 1, 1, 1, 0, 3, 3},   6, "FG10"},
					  { {4, 0, 1, 1, 0, 3, 3},   4, "FG9"},
					  { {3, 1, 1, 1, 0, 3, 3},   3, "FG8"},
					  { {4, 0, 0, 1, 0, 3, 3},   1, "FG7"},
					  { {4, 1, 1, 0, 0, 3, 3},   0, "FG6"}, // noJack
					  { {4, 0, 1, 0, 0, 3, 3},  -2, "FG5"}, //halfJack
					  { {3, 1, 1, 0, 0, 3, 3},  -3, "FG4"}, //clip3
					  { {4, 0, 0, 0, 0, 3, 3},  -4, "FG3"}, //noJack
					  { {2, 1, 1, 0, 1, 3, 3},  -6, "FG2"},  //clip2
					  { {2, 1, 1, 0, 1, 1, 1}, -10, "FG1"},  //b_obdb
					  { {2, 1, 1, 0, 2, 1, 1}, -12, "FG0"}  //scale_cck2
					}
} ; 

GAIN_OPTIMIZATION_LADDER gainLadder_derby2 = { 8, //numStepsInLadder
					NUM_PARAMS_IN_DERBY2_GAIN_OPTIMIZATION_STEP, //numParamsInStep
					1, //defaultStepNum
					0, //currStepNum
					0, //currGain
					0, //gainFCorrection
					0, //targetGain
					25,
					85,
					FALSE, //gainLadderActive
					{"rf_mixgain_ovr",
					 "rf_pwd_138", "rf_pwd_137", "rf_pwd_136", 
					 "rf_pwd_132", "rf_pwd_131", "rf_pwd_130"}, //paramName
					{ { {3, 0,0,0, 0,0,0},   6, "FG7"},
					  { {2, 0,0,0, 0,0,0},   0, "FG6"},
					  { {1, 0,0,0, 0,0,0},   -3, "FG5"},
					  { {0, 0,0,0, 0,0,0},   -6, "FG4"},
					  { {0, 1,1,0, 0,0,0},   -8, "FG3"},
					  { {0, 1,1,0, 1,1,0},   -10, "FG2"},
					  { {0, 1,0,1, 1,1,0},   -13, "FG1"},
					  { {0, 1,0,1, 1,0,1},   -16, "FG0"},
					}
} ; 

/*
GAIN_OPTIMIZATION_LADDER gainLadder_derby2 = { 9, //numStepsInLadder
					NUM_PARAMS_IN_DERBY2_GAIN_OPTIMIZATION_STEP, //numParamsInStep
					0, //defaultStepNum
					0, //currStepNum
					0, //currGain
					0, //gainFCorrection
					0, //targetGain
					30,
					85,
					FALSE, //gainLadderActive
					{"rf_Acas_sel", "rf_Bcas_sel", "rf_mixgain_ovr",
					 "rf_pwd_138", "rf_pwd_137", "rf_pwd_136", 
					 "rf_pwd_132", "rf_pwd_131", "rf_pwd_130"}, //paramName
					{ { {3, 3, 3, 0,0,0, 0,0,0},   0, "FG8"},
					  { {3, 3, 3, 1,1,0, 0,0,0},   -2, "FG7"},
					  { {3, 3, 3, 1,1,0, 1,1,0},   -4, "FG6"},
					  { {2, 2, 2, 0,0,0, 0,0,0},   -6, "FG5"},
					  { {2, 2, 2, 1,1,0, 0,0,0},   -8, "FG4"},
					  { {2, 2, 2, 1,1,0, 1,1,0},   -10, "FG3"},
					  { {2, 2, 1, 1,1,0, 1,1,0},   -12, "FG2"},
					  { {1, 1, 1, 1,1,0, 1,1,0},   -15, "FG1"},
					  { {1, 1, 1, 1,0,1, 1,1,0},   -18, "FG0"}
					}
} ; 
*/
GAIN_OPTIMIZATION_LADDER gainLadder_derby1 = { 9, //numStepsInLadder
					NUM_PARAMS_IN_DERBY2_GAIN_OPTIMIZATION_STEP, //numParamsInStep
					0, //defaultStepNum
					0, //currStepNum
					0, //currGain
					0, //gainFCorrection
					0, //targetGain
					30,
					85,
					FALSE, //gainLadderActive
					{"rf_mixgain_ovr",
					 "rf_pwd_138", "rf_pwd_137", "rf_pwd_136", 
					 "rf_pwd_132", "rf_pwd_131", "rf_pwd_130"}, //paramName
					{ { { 3, 0,0,0, 0,0,0},   0, "FG8"},
					  { { 3, 1,1,0, 0,0,0},   -2, "FG7"},
					  { { 3, 1,1,0, 1,1,0},   -4, "FG6"},
					  { { 2, 1,1,0, 0,0,0},   -6, "FG5"},
					  { { 2, 1,1,0, 1,1,0},   -8, "FG4"},
					  { { 2, 1,0,1, 1,1,0},   -10, "FG3"},
					  { { 1, 1,1,0, 1,1,0},   -12, "FG2"},
					  { { 1, 1,0,1, 1,1,0},   -15, "FG1"},
					  { { 1, 1,0,1, 1,0,1},   -18, "FG0"}
					}
} ; 

void  initializeGainLadder(GAIN_OPTIMIZATION_LADDER *ladder)
{

	ladder->currStepNum = ladder->defaultStepNum;
	ladder->currStep    = &(ladder->optStep[ladder->defaultStepNum]);
//	ladder->active		= ((swDeviceID & 0xff) >= 0x14) ? FALSE : TRUE;
	ladder->active		= TRUE;
//++JC++
	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) {
		ladder->active		= FALSE;
	}
	if (isPredator(swDeviceID)) {
		ladder->active		= FALSE;
	}
//++JC++
}

void  setGainLadderForMaxGain(GAIN_OPTIMIZATION_LADDER *ladder)
{
	ladder->currStepNum = 0;
	ladder->currStep    = &(ladder->optStep[ladder->currStepNum]);
//	ladder->active		= ((swDeviceID & 0xff) >= 0x14) ? FALSE : TRUE;
	ladder->active		= TRUE;
}

void  setGainLadderForMinGain(GAIN_OPTIMIZATION_LADDER *ladder)
{
	ladder->currStepNum = ladder->numStepsInLadder-1;
	ladder->currStep    = &(ladder->optStep[ladder->currStepNum]);
//	ladder->active		= ((swDeviceID & 0xff) >= 0x14) ? FALSE : TRUE;
	ladder->active		= TRUE;
}

void setGainLadderForIndex(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 index)
{
//++JC++
	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) return;
//++JC++
	ladder->currStepNum = index;
	ladder->currStep    = &(ladder->optStep[ladder->currStepNum]);
//	ladder->active		= ((swDeviceID & 0xff) >= 0x14) ? FALSE : TRUE;
	ladder->active		= TRUE;
}

void  loopIncrementFixedGain(GAIN_OPTIMIZATION_LADDER *ladder)
{
//++JC++
	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) return;
//++JC++
//	if (!(ladder->active)) return;
	if (ladder->currStepNum == 0) {
	ladder->currStepNum = ladder->numStepsInLadder -1 ; 
	} else {
	ladder->currStepNum--;
	}
	ladder->currStep    = &(ladder->optStep[ladder->currStepNum]);
}

A_BOOL  invalidGainReadback(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum)
{
	A_UINT32	gStep, mix_ovr, g;
	A_UINT32	L1, L2, L3, L4;

	if (!(ladder->active)) return (FALSE);
	
	if(((swDeviceID & 0xff) == 0x14) || ((swDeviceID & 0xff) >= 0x16)) {
		mix_ovr = art_getFieldForMode(devNum, "rf_mixvga_ovr", configSetup.mode, configSetup.turbo);
		L1 = 0;
		L2 = 107 ;
		L3 = 0;
		L4 = 107 ;
		if (mix_ovr == 1) {
			L2 = 83;
			L4 = 83;
			ladder->hiTrig = 55;
		}
	} else {
		gStep = art_getFieldForMode(devNum, "rf_rfgain_step", configSetup.mode, configSetup.turbo);
		L1 = 0;
		L2 = (gStep == 0x3f) ? 50 : gStep + 4 ;
		L3 = (gStep != 0x3f) ? 0x40 : L1 ;
		L4 = L3 + 50;
		ladder->loTrig = L1 + ((gStep == 0x3f) ? DYN_ADJ_LO_MARGIN : 0);
		ladder->hiTrig = L4 - ((gStep == 0x3f) ? DYN_ADJ_UP_MARGIN : -5); // never adjust if != 0x3f
	}

	g = ladder->currGain;

	return ( ! ( ( (g>=L1) && (g<=L2) ) || ( (g>=L3) && (g<=L4) ) ) );
}

A_INT32 setupGainFRead(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 power)
{
//	if (!(ladder->active)) return (-1);
	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) return (-1);

	art_writeField(devNum, "bb_probe_powertx", power);
	art_writeField(devNum, "bb_probe_next_tx", 1);
	return(0);
}

A_INT32 computeGainFCorrection(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum)
{
	A_UINT32    mix_ovr = 0;
	A_UINT32	gainStep;
	A_UINT32	mix_gain;
	

	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) return (-1);

	if ((swDeviceID & 0xFF) >= 0x16) {
		mix_ovr = art_getFieldForMode(devNum, "rf_mixvga_ovr", configSetup.mode, configSetup.turbo);
		mix_gain = art_getFieldForMode(devNum, "rf_mixgain_ovr", configSetup.mode, configSetup.turbo);
		ladder->gainFCorrection = 0;
		if (mix_ovr == 1) {
			gainStep = art_getFieldForMode(devNum, "rf_mixgain_step", configSetup.mode, configSetup.turbo);
			switch (mix_gain) {
			case 0 :
				ladder->gainFCorrection = 0;
				break;
			case 1 :
				ladder->gainFCorrection = gainStep;
				break;
			case 2 :
				ladder->gainFCorrection = 2*gainStep - 5;
				break;
			case 3 :
				ladder->gainFCorrection = 2*gainStep;
				break;
			}
		}
	}
	return (0);
}

A_INT32 readGainF(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 power)
{
	A_UINT32	rddata;

//	if (!(ladder->active)) return (-1);
//++JC++
	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) return (-1);
//++JC++

	setupGainFRead(ladder, devNum, power);
	art_txDataSetup(devNum, RATE_54, rxStation, 2, 30, pattern, 2, 0, configSetup.antenna, 1|PROBE_PKT);
    art_txDataBegin(devNum, 5000, 0);			
	//Sleep(1);
	rddata = art_regRead(devNum, 0x9930);
	ladder->currGain = (rddata >> 25) ;
	
	computeGainFCorrection(ladder, devNum);
	ladder->currGain -= ladder->gainFCorrection;

	return (0);
}

A_INT32 readGainFInContTx(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 power)
{
	A_UINT32	rddata, j=0;
	A_BOOL		invalidGainF = TRUE;
//	A_UINT32 i, gainF;

//	if (!(ladder->active)) return (-1);
//++JC++
	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) return (-1);
//++JC++

	while (invalidGainF && (j<20))
	{
		j++;
		setupGainFRead(ladder, devNum, power);
		Sleep(100);
		rddata = art_regRead(devNum, 0x9930);
		ladder->currGain = (rddata >> 25) ;
		computeGainFCorrection(ladder, devNum);
		ladder->currGain -= ladder->gainFCorrection;
		invalidGainF = invalidGainReadback(ladder, devNum);
	}
	return (0);
}

A_BOOL  needGainAdjustment(GAIN_OPTIMIZATION_LADDER *ladder)
{
	if (!(ladder->active)) return(FALSE);
	return ((ladder->currGain <= ladder->loTrig) || (ladder->currGain >= ladder->hiTrig));
}


A_INT32 adjustGain(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 debug)
{
// return > 0 for valid adjustments.
	if (!(ladder->active)) return (-1);

	if (ladder->currGain >= ladder->hiTrig)
	{
		if (ladder->currStepNum == 0)
		{
			if (debug) uiPrintf("Max gain limit.\n");
			return -1;
		}

		if (debug) uiPrintf("Adding gain: currG=%d [%s] --> ", ladder->currGain, ladder->currStep->stepName);

		ladder->targetGain = ladder->currGain;
		while ((ladder->targetGain >= ladder->hiTrig) && (ladder->currStepNum > 0))
		{
			ladder->targetGain -= 2*(ladder->optStep[--(ladder->currStepNum)].stepGain - ladder->currStep->stepGain);
			ladder->currStep = &(ladder->optStep[ladder->currStepNum]);
		}

		if (debug) uiPrintf("targG=%d [%s]\n", ladder->targetGain, ladder->currStep->stepName);

		return 1;
	}

	if (ladder->currGain <= ladder->loTrig)
	{
		if (ladder->currStepNum == (ladder->numStepsInLadder-1))
		{
			if (debug) uiPrintf("Min gain limit.\n");
			return -2;
		}

		if (debug) uiPrintf("Deducting gain: currG=%d [%s] --> ", ladder->currGain, ladder->currStep->stepName);

		ladder->targetGain = ladder->currGain;
		while ((ladder->targetGain <= ladder->loTrig) && (ladder->currStepNum < (ladder->numStepsInLadder-1)))
		{
			ladder->targetGain -= 2*(ladder->optStep[++(ladder->currStepNum)].stepGain - ladder->currStep->stepGain);
			ladder->currStep = &(ladder->optStep[ladder->currStepNum]);
		}

		if (debug) uiPrintf("targG=%d [%s]\n", ladder->targetGain, ladder->currStep->stepName);

		return 2;
	}

	if (debug) uiPrintf("GainF did not need tweaking.\n");
	return (0); // should've called needAdjGain before calling this.

}


void programNewGain(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 debug)
{

	A_UINT32 ii;

//	if (!(ladder->active)) return ;
	if (isGriffin(swDeviceID) || isEagle(swDeviceID)) return;

	for (ii=0; ii<ladder->numParamsInStep; ii++)
	{
		if (debug) uiPrintf("ii=%d:%s = %d, ", ii, ladder->paramName[ii], ladder->currStep->paramVal[ii]);
		art_writeField(devNum, ladder->paramName[ii], ladder->currStep->paramVal[ii]);
	}

	if ((swDeviceID & 0xFF) >= 0x16) {
		art_writeField(devNum, "rf_mixvga_ovr", 1);
	}
	if (debug) uiPrintf("\n");
}

void tweakGainF (GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 debug)
{
//  if((swDeviceID & 0xff) >= 0x14) return;

	if (!(ladder->active)) return;
	if (invalidGainReadback(ladder, devNum)) return;

	if (needGainAdjustment(ladder)) 
	{
		if (adjustGain(ladder, debug) > 0)
		{
			programNewGain(ladder, devNum, debug);
		}
	}
}

