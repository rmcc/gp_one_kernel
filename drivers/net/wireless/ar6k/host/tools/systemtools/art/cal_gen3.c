#ifdef _WINDOWS
#include <windows.h>
#endif 
#include <stdio.h>
#ifndef LINUX
#include <conio.h>
#endif
#include <assert.h>
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
#include "common_hw.h"
#include "manlibInst.h" /* The Manufacturing Library Instrument Library extension */
#include "mEeprom.h"    /* Definitions for the data structure */
#include "dynamic_optimizations.h"
#include "maui_cal.h"   /* Definitions for the Calibration Library */
#include "rssi_power.h"
#include "test.h"
#include "parse.h"
#include "dk_cmds.h"
#include "dk_ver.h"

#if defined(LINUX) || defined(__linux__)
#include "linux_ansi.h"
#include "unistd.h"
#endif	

#include "art_if.h"

#include "ar5212/mEEPROM_d.h"
#include "cal_gen3.h"

#ifdef _IQV
#include "../../../../common/IQmeasure.h"
#endif // _IQV

extern  A_UINT32	devlibModeFor[3];
extern  A_UINT32	calModeFor[3];

extern  char		modeName[3][122]  ;
extern  A_INT32		devPM, devSA, devATT;
extern  A_BOOL		REWIND_TEST;
extern  char		ackRecvStr[1024];

extern A_UINT16		RAW_CHAN_LIST_2p4[2][3];

 
static A_UCHAR  bssID[6]     = {0x50, 0x55, 0x5A, 0x50, 0x00, 0x00};
static A_UCHAR  rxStation[6] = {0x10, 0x11, 0x12, 0x13, 0x00, 0x00};	// DUT
static A_UCHAR  txStation[6] = {0x20, 0x22, 0x24, 0x26, 0x00, 0x00};	// Golden
static A_UCHAR  NullID[6]    = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
static A_UCHAR  pattern[2] = {0xaa, 0x55};

static A_UINT16 rates[MAX_RATES] = {6,9,12,18,24,36,48,54};


 RAW_DATA_STRUCT_GEN3 RawDatasetGen3_11a ; // raw power measurements for 11a
 RAW_DATA_STRUCT_GEN3 RawDatasetGen3_11g ; // raw power measurements for 11g
 RAW_DATA_STRUCT_GEN3 RawDatasetGen3_11b ; // raw power measurements for 11b
 RAW_DATA_STRUCT_GEN3 *pRawDataset_gen3[3] = {&RawDatasetGen3_11g, &RawDatasetGen3_11b, &RawDatasetGen3_11a} ; // raw power measurements

 EEPROM_DATA_STRUCT_GEN3 CalDatasetGen3_11a ; // calibration dataset
 EEPROM_DATA_STRUCT_GEN3 CalDatasetGen3_11g ; // calibration dataset
 EEPROM_DATA_STRUCT_GEN3 CalDatasetGen3_11b ; // calibration dataset
 EEPROM_DATA_STRUCT_GEN3 *pCalDataset_gen3[3] = {&CalDatasetGen3_11g, &CalDatasetGen3_11b, &CalDatasetGen3_11a} ; // calibration dataset

 RAW_DATA_STRUCT_GEN3 RawGainDatasetGen3_11a ; // raw gainF measurements for 11a
 RAW_DATA_STRUCT_GEN3 RawGainDatasetGen3_11g ; // raw gainF measurements for 11g
 RAW_DATA_STRUCT_GEN3 RawGainDatasetGen3_11b ; // raw gainF measurements for 11b
 RAW_DATA_STRUCT_GEN3 *pRawGainDataset_gen3[3] = {&RawGainDatasetGen3_11g, &RawGainDatasetGen3_11b, &RawGainDatasetGen3_11a} ; // raw gainF measurements

 char calPowerLogFile_gen3[3][122] = {"cal_AR5212_Power_11g.log", "cal_AR5212_Power_11b.log", "cal_AR5212_Power_11a.log"};


void dutCalibration_gen3(A_UINT32 devNum, A_UINT32 mode)
{

	A_BOOL		read_from_file = CalSetup.readFromFile;
	char		*fileName = CalSetup.rawDataFilename;
//	A_UINT16	myNumRawChannels = numRAWChannels; 

	if (mode != MODE_11a) {
		read_from_file = CalSetup.readFromFile_2p4[mode];
		fileName       = CalSetup.rawDataFilename_2p4[mode];
//		myNumRawChannels = numRAWChannels_2p4; 
	}

	// power and gainf datasets setup and measurement
	if (!setup_datasets_for_cal_gen3(devNum, mode)) { 
		uiPrintf("Could not setup gen3 raw datasets for mode %d. Exiting...\n", mode);
        closeEnvironment();
        exit(0);
	}

	if (read_from_file)
	{
		read_dataset_from_file_gen3(pRawDataset_gen3[mode], fileName);
	} else
	{
		if ((mode == MODE_11b) && (CalSetup.useOneCal)){
			uiPrintf("\nUsing 11g calibration data for 11b mode\n");
//			uiPrintf("Re-using 11g calibration data for 11b mode not supported for gen3 yet\n");
//			exit(0);
			copy_11g_cal_to_11b_gen3(pRawDataset_gen3[MODE_11g], pRawDataset_gen3[MODE_11b]);
		} else {
			getCalData(devNum, mode, CalSetup.customerDebug);
		}
		dump_raw_data_to_file_gen3(pRawDataset_gen3[mode], calPowerLogFile_gen3[mode]) ;
//		if (CalSetup.customerDebug) 
//			dump_raw_data_to_file_gen3(pRawGainDataset_gen3[mode], calGainfLogFile_gen3[mode]) ;
	}
	raw_to_eeprom_dataset_gen3(pRawDataset_gen3[mode], pCalDataset_gen3[mode]);
}

void copy_11g_cal_to_11b_gen3(RAW_DATA_STRUCT_GEN3 *pRawDataSrc, RAW_DATA_STRUCT_GEN3 *pRawDataDest) {

	A_UINT32 ii, jj, kk, idxL, idxR;
	RAW_DATA_PER_CHANNEL_GEN3	*pChDest;
	RAW_DATA_PER_CHANNEL_GEN3	*pChSrcL;
	RAW_DATA_PER_CHANNEL_GEN3	*pChSrcR;
	RAW_DATA_PER_XPD_GEN3		*pXpdDest;
	RAW_DATA_PER_XPD_GEN3		*pXpdSrcL;
	RAW_DATA_PER_XPD_GEN3		*pXpdSrcR;

	assert(pRawDataDest != NULL);
	assert(pRawDataSrc  != NULL);

	// map the 11g data to 2412, 2472, 2484 for 11b to give 2484 its
	// dedicated data.
	pRawDataDest->numChannels = 3;
	CalSetup.TrgtPwrStartAddr += 5*pRawDataDest->numChannels;
	CalSetup.numForcedPiers_2p4[MODE_11b] = pRawDataDest->numChannels;

	pRawDataDest->xpd_mask    = pRawDataSrc->xpd_mask;

	idxL = 0;
	idxR = 0;
	for (ii = 0; ii < pRawDataDest->numChannels ; ii++) {
		CalSetup.piersList_2p4[MODE_11b][ii] = RAW_CHAN_LIST_2p4[MODE_11b][ii];
		pRawDataDest->pChannels[ii] = RAW_CHAN_LIST_2p4[MODE_11b][ii];
		mdk_GetLowerUpperIndex(pRawDataDest->pChannels[ii], pRawDataSrc->pChannels, pRawDataSrc->numChannels, &(idxL), &(idxR));
		pChDest = &(pRawDataDest->pDataPerChannel[ii]);
		pChSrcL  = &(pRawDataSrc->pDataPerChannel[idxL]);
		pChSrcR  = &(pRawDataSrc->pDataPerChannel[idxR]);

		pChDest->channelValue = pRawDataDest->pChannels[ii];
		pChDest->maxPower_t4  = mdk_GetInterpolatedValue_Signed16(
									pChDest->channelValue,
									pChSrcL->channelValue,
									pChSrcR->channelValue,
									pChSrcL->maxPower_t4,
									pChSrcR->maxPower_t4
								);
		pChDest->maxPower_t4  += (A_UINT16)(4*CalSetup.cck_ofdm_delta);
		
		if (pChDest->channelValue == 2484) {
			pChDest->maxPower_t4 -= (A_UINT16)(4*CalSetup.ch14_filter_cck_delta);
		}

		for (jj = 0; jj < NUM_XPD_PER_CHANNEL; jj++) {
			if ( ((1 << jj) & pRawDataDest->xpd_mask) == 0 ) {
				continue;
			}
			pXpdDest  = &(pChDest->pDataPerXPD[jj]);
			pXpdSrcL  = &(pChSrcL->pDataPerXPD[jj]);
			pXpdSrcR  = &(pChSrcR->pDataPerXPD[jj]);
			// following quantities expected to remain the same for idxL and idxR
			assert(pXpdSrcL->numPcdacs == pXpdSrcR->numPcdacs);
			assert(pXpdSrcL->xpd_gain  == pXpdSrcR->xpd_gain);
			pXpdDest->numPcdacs = pXpdSrcL->numPcdacs;
			pXpdDest->xpd_gain  = pXpdSrcL->xpd_gain;
			for (kk = 0; kk < pXpdDest->numPcdacs; kk++) {
				pXpdDest->pcdac[kk] = getInterpolatedValue(	pChDest->channelValue,
											pChSrcL->channelValue,
											pChSrcR->channelValue,
											pXpdSrcL->pcdac[kk],
											pXpdSrcR->pcdac[kk],
											FALSE // don't scale_up
										);

				pXpdDest->pwr_t4[kk] = mdk_GetInterpolatedValue_Signed16(
											pChDest->channelValue,
											pChSrcL->channelValue,
											pChSrcR->channelValue,
											pXpdSrcL->pwr_t4[kk],
											pXpdSrcR->pwr_t4[kk]
										);
				
				pXpdDest->pwr_t4[kk] += (A_UINT16)(4*CalSetup.cck_ofdm_delta);

				if (pChDest->channelValue == 2484) {
					pXpdDest->pwr_t4[kk] -= (A_UINT16)(4*CalSetup.ch14_filter_cck_delta);
				}
			}
		}
	}
} // copy_11g_cal_to_11b_gen3

A_BOOL setup_datasets_for_cal_gen3(A_UINT32 devNum, A_UINT32 mode) {
	
//	A_UINT16	myNumRawChannels = numRAWChannels; // needed to handle forced piers case.
	A_UINT16	myNumRawChannels ; 
	A_UINT16	*pMyRawChanList ;
	A_BOOL		read_from_file = CalSetup.readFromFile;
	A_BOOL		force_piers = CalSetup.forcePiers;

//	pMyRawChanList	 = RAW_CHAN_LIST ;

	if (mode != MODE_11a) {
		read_from_file		= CalSetup.readFromFile_2p4[mode];
//		myNumRawChannels	= numRAWChannels_2p4; 
		myNumRawChannels	= 3; 

		force_piers			= CalSetup.forcePiers_2p4[mode];
//		pMyRawChanList = &(RAW_CHAN_LIST_2p4[mode][0]) ;
	}

	
	// handle forcePierList case here
	if(force_piers && !read_from_file) // 'cause read from file supercedes
	{
		myNumRawChannels = (A_UINT16) ((mode == MODE_11a) ? CalSetup.numForcedPiers : CalSetup.numForcedPiers_2p4[mode]);
		if ((mode == MODE_11b) && (CalSetup.useOneCal)) {
			myNumRawChannels = 3; // 2412, 2472, 2484
		}
		pMyRawChanList	 = ((mode == MODE_11a) ? CalSetup.piersList : CalSetup.piersList_2p4[mode]);
	} else {
		uiPrintf("Automatic pier computation for gen3 not supported yet. Please specify the forced_piers_list\n");
	}

	if (!setup_raw_dataset_gen3(devNum, pRawDataset_gen3[mode], myNumRawChannels, pMyRawChanList)) {
		uiPrintf("Could not setup raw dataset for gen3 cal for mode %d\n", mode);
		return(0);
	}

	if (!setup_raw_dataset_gen3(devNum, pRawGainDataset_gen3[mode], myNumRawChannels, pMyRawChanList)) {
		uiPrintf("Could not setup raw gainF dataset for gen3 cal for mode %d\n", mode);
		return(0);
	}

	if (!setup_EEPROM_dataset_gen3(devNum, pCalDataset_gen3[mode], myNumRawChannels, pMyRawChanList)) {
		uiPrintf("Could not setup cal dataset for gen3 cal for mode %d\n", mode);
		return(0);
	}

	return(1);
}


void measure_all_channels_gen3(A_UINT32 devNum, A_UINT32 debug, A_UINT32 mode) {

	A_UINT32	xpd_gain, xgain_list[2]; // handle upto two xpd_gains
	A_INT32		ii, jj, kk, ll;
	A_UINT16	turbo = 0;
	A_UINT16	channel;
	A_UINT16	pcdac ;
	A_UINT16	reset = 0;
	double		power;
	A_UCHAR		devlibMode ; // use setResetParams to communicate appropriate mode to devlib

	double		maxPower;
	double		myAtten;
	A_INT16		lo_pcd, max_pcdac;
	A_INT16		PCDL = 11;
	A_INT16		PCDH = 41;
	A_UINT16	curr_intercept_estimate ;
	A_UINT32	gainF;
	A_UINT32    mixvga_ovr, mixgain_ovr;
	A_UINT32    gain1, gain25, intp_gain, prev_gain;
	double		pwr1, pwr25 ;
	A_INT16     dirxn;
	A_UINT16    sleep_interval = 150;
	A_UINT16    pcd1, pcd2, pcd_interp;
	double      pwrL, pwrH;
	double      attemptPower = 0;
	A_UINT16    tempPcdac, copyPcdac;
	
	mixvga_ovr = art_getFieldForMode(devNum, "rf_mixvga_ovr", mode, turbo);
	mixgain_ovr = art_getFieldForMode(devNum, "rf_mixgain_ovr", mode, turbo);
	art_writeField(devNum, "rf_mixvga_ovr", 0);


	switch (mode) {
	case MODE_11a :
		devlibMode = MODE_11A;
		max_pcdac = CalSetup.max_pcdac_11a;
		xpd_gain   = CalSetup.xgain;
		myAtten	   = CalSetup.attenDutPM;
		curr_intercept_estimate = IDEAL_10dB_INTERCEPT_5G;
		CalSetup.TrgtPwrStartAddr += 5; // upto 10 piers
		attemptPower = CalSetup.attempt_pcdac_11a;
		break;
	case MODE_11g :
		devlibMode = MODE_11G;
		max_pcdac = CalSetup.max_pcdac_11g;
		xpd_gain   = CalSetup.xgain_2p4[mode];
		myAtten	   = CalSetup.attenDutPM_2p4[mode];
		curr_intercept_estimate = IDEAL_10dB_INTERCEPT_2G;
		attemptPower = CalSetup.attempt_pcdac_11g;
		break;
	case MODE_11b :
		devlibMode = MODE_11B;
		max_pcdac = CalSetup.max_pcdac_11b;
		xpd_gain   = CalSetup.xgain_2p4[mode];
		myAtten	   = CalSetup.attenDutPM_2p4[mode];
		curr_intercept_estimate = IDEAL_10dB_INTERCEPT_2G;
		attemptPower = CalSetup.attempt_pcdac_11b;
//		if (CalSetup.pmModel == PM_436A) {
		if (CalSetup.pmModel == NRP_Z11) {
			sleep_interval = 50;
		} else {
//		if (1) {
			sleep_interval = 250;
		}
		break;
	default:
		uiPrintf("Unknown mode supplied to measure_all_channels_gen3 : %d \n", mode);
		break;
	}
	CalSetup.TrgtPwrStartAddr += 5*pRawDataset_gen3[mode]->numChannels;

	configSetup.eepromLoad = 0;
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)devlibMode, configSetup.use_init);		
						
	uiPrintf("\nCollecting raw data for the adapter for mode %s\n", modeName[mode]);

	art_changeField(devNum, "rf_ovr", 0);
	art_changeField(devNum, "rf_xpdsel", 1);

	copyPcdac = max_pcdac;
	for (ii=0; ii<pRawDataset_gen3[mode]->numChannels; ii++)
	{
		channel = pRawDataset_gen3[mode]->pDataPerChannel[ii].channelValue;

		if (ii == 0)
		{
			art_resetDevice(devNum, txStation, NullID, channel, 0);
		} else
		{
			art_changeChannel(devNum, channel); // for efficiency
		}

		uiPrintf("ch: %d  --> ", channel);
		
		if (CalSetup.cal_mult_xpd_gain_mask[mode] == 0) {			
			if((swDeviceID & 0xff) >= 0x16) { // derby2
				jj = art_getFieldForMode(devNum,"rf_pdgain_lo", devlibMode, BASE);
				kk = art_getFieldForMode(devNum,"rf_pdgain_hi", devlibMode, BASE);
				pRawDataset_gen3[mode]->xpd_mask = ((1 << jj) | (1 << kk));
				xgain_list[0] = jj;
				xgain_list[1] = kk;
			} else { // derby1
				jj = art_getFieldForMode(devNum,"rf_xpd_gain", devlibMode, BASE);
				kk = jj;
				pRawDataset_gen3[mode]->xpd_mask = (1 << jj);
				xgain_list[0] = jj;
				xgain_list[1] = kk;
			}
		} else {
			jj = 0;
			pRawDataset_gen3[mode]->xpd_mask = (A_UINT16) CalSetup.cal_mult_xpd_gain_mask[mode];
			xpd_gain = CalSetup.cal_mult_xpd_gain_mask[mode];
			kk = 0;
			ll = 0;
			while (ll < 4) {
				if ((xpd_gain & 0x1) == 1) {
					if (kk > 2) {
						uiPrintf("ERROR: A maximum of 2 xpd_gains allowed to be specified in the rf_xpd_gain parameter in the eep file.\n");
						exit(0);
					}
					xgain_list[kk++] = ll;
				}
				ll++;
				xpd_gain = (xpd_gain >> 1);
			}
			if (kk == 1) {
				xgain_list[1] = xgain_list[0];
			}
		}

		jj = xgain_list[0];
		if((swDeviceID & 0xff) >= 0x16) {
			art_writeField(devNum, "rf_pdgain_lo", jj);
			art_writeField(devNum, "rf_pdgain_hi", jj);
		} else {
			uiPrintf("SNOOP: xgain0 = %d[%d]\n", xgain_list[0], jj);
			art_writeField(devNum, "rf_xpd_gain", jj);
		}
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].xpd_gain = (A_UINT16)jj;
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].numPcdacs = NUM_POINTS_XPD0;

		pcdac = copyPcdac;
		art_ForceSinglePCDACTable(devNum, pcdac);
		art_forceSinglePowerTxMax(devNum, 0);
		art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
					    rates[0], DESC_ANT_A | USE_DESC_ANT);
		Sleep(50); // sleep 20 ms

		art_ForceSinglePCDACTable(devNum, pcdac);
		Sleep(sleep_interval);
		maxPower = pmMeasAvgPower(devPM, reset) + myAtten;				
		if (maxPower < 10) {
			Sleep(sleep_interval);
			maxPower = pmMeasAvgPower(devPM, reset) + myAtten;				
		}
		if (maxPower < 10) {
			uiPrintf("The max transmit Power is too small (%3.2f) at ch = %d. Giving up.\n", maxPower, channel);
			REWIND_TEST = TRUE;
			return;
		}
		
		//if max power is less than the desired attemptPower, try to achieve it slowly
		if (maxPower < attemptPower) {
			tempPcdac = pcdac;
//printf("SNOOP: attempting to achieve power of %6.2f\n", attemptPower);
			while ((maxPower < attemptPower) && (tempPcdac < 62)) {
				art_ForceSinglePCDACTable(devNum, ++tempPcdac);
				Sleep(sleep_interval);
				maxPower = pmMeasAvgPower(devPM, reset) + myAtten;
//printf("SNOOP: got power of %6.2f for pcdac %d\n", maxPower, tempPcdac);				
			}
			max_pcdac = tempPcdac;
		}

		if (maxPower > CalSetup.maxPowerCap[mode]) {
			pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4 = (A_UINT16)(4*CalSetup.maxPowerCap[mode] + 0.5);				
		} else {
			pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4 = (A_UINT16)(4*maxPower + 0.5);				
		}

		if (CalSetup.customerDebug) {
			uiPrintf("maxPower = %3.2f", ((double)pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4/4.0));
		} else {
			if(max_pcdac < 63) {
				uiPrintf("pcdac limited pwr is %2.1f dBm (pcdac %d)\n", ((double)pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4/4.0), max_pcdac) ;
			} else {
			uiPrintf("max pwr is %2.1f dBm\n", ((double)pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4/4.0)) ;
		}
		}

		lo_pcd = 1;
		art_ForceSinglePCDACTable(devNum, lo_pcd);
		Sleep(sleep_interval);
		power = pmMeasAvgPower(devPM, reset) + myAtten ;
		if (fabs(power - maxPower) < 10.0) {
			art_ForceSinglePCDACTable(devNum, 1);
			Sleep(sleep_interval);
			power = pmMeasAvgPower(devPM, reset) + myAtten ;
		}
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[0] = (A_UINT16)(4*power + 0.5);				
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[0]  = lo_pcd;
		gain1 = read_gainf_twice(devNum) ; 
		if (gain1 < 5) {  // possibly a bad reading
			gain1 = read_gainf_twice(devNum) ;
		}
		pwr1 = power;
		if (CalSetup.customerDebug) {
			uiPrintf(", %3.2f[%d]", power, lo_pcd);
		}

		lo_pcd = 25;
		art_ForceSinglePCDACTable(devNum, lo_pcd);
		Sleep(sleep_interval);
		power = pmMeasAvgPower(devPM, reset) + myAtten ;
		gain25 = read_gainf_twice(devNum) ;
		if (gain25 < 5) {  // possibly a bad reading
			gain25 = read_gainf_twice(devNum) ;
		}
		pwr25 = power;
		if (CalSetup.customerDebug) {
			uiPrintf(", %3.2f[%d]", power, lo_pcd);
		}

		// start search for highest linear power at pcdac for (maxPower - 3dB)
		lo_pcd = (A_UINT16)((((maxPower - 3) - pwr1)/(pwr25 - pwr1))*(25 - 1) + 1);
		if ((lo_pcd < 0) || (lo_pcd > 56)) {
			lo_pcd = 56;
		}
		art_ForceSinglePCDACTable(devNum, lo_pcd);
		Sleep(10);
		gainF = read_gainf_twice(devNum) ;
		intp_gain = (A_UINT16)(((lo_pcd - 1)/(25 - 1))*(gain25 - gain1) + gain1);

		dirxn = (gainF > (intp_gain + 5)) ? -1 : 1;

		prev_gain = gainF;
		if (dirxn < 0) {
			while ((abs(gainF - prev_gain) < 4) && (lo_pcd < max_pcdac)){
				lo_pcd -= dirxn ;
				prev_gain = gainF;
				if (CalSetup.customerDebug) printf("SNOOP: loop 1 dirxn-1 = %d : pcd=%d, prev_gF=%d", dirxn, lo_pcd, prev_gain);
				art_ForceSinglePCDACTable(devNum, lo_pcd);
				Sleep(10);				
				gainF = read_gainf_twice(devNum) ;
				if (CalSetup.customerDebug) printf(", gF=%d\n", gainF);
			}
					
			while ((abs(gainF - prev_gain) >= 4) && (lo_pcd < max_pcdac)){
				lo_pcd += dirxn ;
				prev_gain = gainF;
				if (CalSetup.customerDebug) printf("SNOOP: loop 2 dirxn-1 = %d : pcd=%d, prev_gF=%d", dirxn, lo_pcd, prev_gain);
				art_ForceSinglePCDACTable(devNum, lo_pcd);
				Sleep(10);
				gainF = read_gainf_twice(devNum) ;		
				if (CalSetup.customerDebug) printf(", gF=%d\n", gainF);
			}
		} else {  // dirxn > 0
			prev_gain = 107;
			while ((abs(gainF - prev_gain) >= 4) && (lo_pcd < max_pcdac)){
				lo_pcd -= dirxn ;
				prev_gain = gainF;
				if (CalSetup.customerDebug) printf("SNOOP: loop 1 dirxn1 = %d : pcd=%d, prev_gF=%d", dirxn, lo_pcd, prev_gain);
				art_ForceSinglePCDACTable(devNum, lo_pcd);
				Sleep(10);
				gainF = read_gainf_twice(devNum) ;	
				if (CalSetup.customerDebug) printf(", gF=%d\n", gainF);
			}
					
			while ((abs(gainF - prev_gain) < 4) && (lo_pcd < max_pcdac)) {
				lo_pcd += dirxn ;
				prev_gain = gainF;
				if (CalSetup.customerDebug) printf("SNOOP: loop 2 dirxn1 = %d : pcd=%d, prev_gF=%d", dirxn, lo_pcd, prev_gain);
				art_ForceSinglePCDACTable(devNum, lo_pcd);
				Sleep(10);
				gainF = read_gainf_twice(devNum) ;	
				if (CalSetup.customerDebug) printf(", gF=%d\n", gainF);
			}
			lo_pcd -= dirxn;
		}
		lo_pcd -= 1; // back off 1 into linear range

		if (lo_pcd > max_pcdac) {
			lo_pcd = max_pcdac;
		}
		art_ForceSinglePCDACTable(devNum, lo_pcd);
		Sleep(sleep_interval);
		power = pmMeasAvgPower(devPM, reset) + myAtten ;
		
		// highest linear power
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2] = (A_UINT16)(4*power + 0.5);				
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2]  = lo_pcd;

		// if highest linear pcdac > 40, use pcdacs (25, 70% intrcpt[25,pcdMaxLin], pcdMaxLin, pcdMaxPwr) instead
		// of (1, 25, pcdMaxLin, pcdMaxPwr).


		if (CalSetup.customerDebug) printf("SNOOP: pcd2=%d, pwr2=%f\n", lo_pcd, power);
		if (lo_pcd > 40) {
			if (CalSetup.customerDebug) printf("SNOOP: lo_pcd is > 40 : %d\n", lo_pcd);
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[0] = (A_UINT16)(4*pwr25 + 0.5);				
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[0]  = 25;
			if (CalSetup.customerDebug) printf("SNOOP: pcd0=%d, pwr0=%f\n", 25, pwr25);
//			lo_pcd = (A_UINT16)(0.75*(lo_pcd-1) + 1);
//			if (lo_pcd > 32) {
//				lo_pcd = 32;
//			}
			lo_pcd = (A_UINT16)(0.7*(lo_pcd-25) + 25);
			if (lo_pcd > 56) {
				lo_pcd = 56;
			}
			art_ForceSinglePCDACTable(devNum, lo_pcd);
			Sleep(sleep_interval);
			power = pmMeasAvgPower(devPM, reset) + myAtten ;
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[1] = (A_UINT16)(4*power + 0.5);				
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[1]  = lo_pcd;
			if (CalSetup.customerDebug) printf("SNOOP: pcd1=%d, pwr1=%f\n", lo_pcd, power);
		} else {
			if (CalSetup.customerDebug) printf("SNOOP: lo_pcd is < 40 : %d\n", lo_pcd);
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[1] = (A_UINT16)(4*pwr25 + 0.5);				
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[1]  = 25;
			if (CalSetup.customerDebug) printf("SNOOP: pcd1=%d, pwr1=%f\n", 25, pwr25);
		}
		
		power = ((double)pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2])/4.0;

		// limit power[2] to maxPowerCap if highest linear power > maxPower
		if (power >= CalSetup.maxPowerCap[mode]) {
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2] = pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4;				
			pcd1 = pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[1];
			pcd2 = pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2];
			pwrL = (double) ((double)pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[1]/4.0);
			pwrH = power;
			pcd_interp = (A_INT16)( ( (CalSetup.maxPowerCap[mode] - pwrL)*pcd2 + (pwrH - CalSetup.maxPowerCap[mode])*pcd1)/(pwrH - pwrL));
			//uiPrintf("SNOOP: maxpwr2 : pcdI = %d, pcd1 = %d, pcd2 = %d, pwr1 = %3.1f, pwr2 = %3.1f, pwrT = %3.1f\n", pcd_interp, pcd1, pcd2, pwrL, pwrH, CalSetup.maxPowerCap[mode]);
			if ((pcd_interp >= pcd1) && (pcd_interp <= pcd2)) {
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2]  = pcd_interp;
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3]  = pcd_interp;
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] = pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4;				
			} else {
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2]  = pcd2;
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2] = (A_UINT16) (4*pwrH);
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3]  = pcd2;
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] = (A_UINT16) (4*pwrH);
			}
		} else {

			// search for min pcdac corresponding to maxPower

			lo_pcd = pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2];
			if (lo_pcd == max_pcdac) { // not hit saturation yet
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] = pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2];				
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3]  = pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2];
				if (CalSetup.customerDebug) printf("SNOOP: pcd3=%d, pwr3=%f\n", lo_pcd, (pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] - 0.5)/4);
				lo_pcd = 47;
				art_ForceSinglePCDACTable(devNum, lo_pcd);
				Sleep(sleep_interval);
				power = pmMeasAvgPower(devPM, reset) + myAtten ;
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2] = (A_UINT16)(4*power + 0.5);				
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2]  = lo_pcd;
				if (CalSetup.customerDebug) printf("SNOOP: pcd2=%d, pwr2=%f\n", lo_pcd, (pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2] - 0.5)/4);
			} else {
				art_ForceSinglePCDACTable(devNum, lo_pcd);
				Sleep(10);
				gainF = read_gainf_twice(devNum) ;
				prev_gain = 107;
				while (((gainF < 100) || (gainF == prev_gain))&& (lo_pcd < max_pcdac)){
					lo_pcd++;
					art_ForceSinglePCDACTable(devNum, lo_pcd);
					Sleep(10);
					prev_gain = gainF;
					gainF = read_gainf_twice(devNum) ;
					if (CalSetup.customerDebug) printf("SNOOP: gainF = %d, pcd = %d\n", gainF, lo_pcd);
				} 
				Sleep(sleep_interval);
				power = pmMeasAvgPower(devPM, reset) + myAtten ;
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] = (A_UINT16)(4*power + 0.5);				
				pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3]  = lo_pcd;
				if (CalSetup.customerDebug) printf("SNOOP: pcd3=%d, pwr3=%f\n", lo_pcd, power);
				
				// limit power[3] to maxPowerCap if highest power > maxPower
				if (power >= CalSetup.maxPowerCap[mode]) {
					pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] = pRawDataset_gen3[mode]->pDataPerChannel[ii].maxPower_t4;
					pcd1 = pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2];
					pcd2 = pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3];
					pwrL = (double) ((double)pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2]/4.0);
					pwrH = power;
					if (pwrH == pwrL) {
						pcd_interp = pcd2;
					} else {
						pcd_interp = (A_INT16)( ( (CalSetup.maxPowerCap[mode] - pwrL)*pcd2 + (pwrH - CalSetup.maxPowerCap[mode])*pcd1)/(pwrH - pwrL));
					}
					//uiPrintf("SNOOP: maxpwr3 : pcdI = %d, pcd1 = %d, pcd2 = %d, pwr1 = %3.1f, pwr2 = %3.1f, pwrT = %3.1f\n", pcd_interp, pcd1, pcd2, pwrL, pwrH, CalSetup.maxPowerCap[mode]);
					if ((pcd_interp >= pcd1) && (pcd_interp <= pcd2)) {
						pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3]  = pcd_interp;
					} else {
						pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3]  = pcd2;
						pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] = (A_UINT16) (4*pwrH);
					}
				}
			}
		}
			
/*
//		hi_pcd = (A_UINT16)(lo_pcd + (maxPower - power)*curr_intercept_estimate/10.0 - 5);		
		hi_pcd = 55;
//		if (hi_pcd > 63) hi_pcd = 63;
//		if (hi_pcd < 0) {
//			uiPrintf("The max transmit Power (%3.2f) is too close to power at pcdac = 1 (%3.2f) at ch = %d. Try again.\n", maxPower, power, channel);
//			REWIND_TEST = TRUE;
//			return;
//			hi_pcd = 45;
//		}
		art_ForceSinglePCDACTable(devNum, hi_pcd);
		Sleep(sleep_interval);
		power = pmMeasAvgPower(devPM, reset) + myAtten ;
		gainF = read_gainf_twice(devNum) ;
		if (CalSetup.customerDebug) {
			uiPrintf(", %3.2f[%d, %d]", power, hi_pcd, gainF);
		}
		tmpPower = power;
		tmpGainF = gainF;
		if (power < (maxPower - 1.5)) {
			hi_pcd += 5;
			art_ForceSinglePCDACTable(devNum, hi_pcd);
			Sleep(sleep_interval);
			power = pmMeasAvgPower(devPM, reset) + myAtten ;
			gainF = read_gainf_twice(devNum) ;
			if (CalSetup.customerDebug) {
				uiPrintf(", %3.2f[%d, %d]", power, hi_pcd, gainF);
			}
			if ((gainF > 105) && (tmpGainF <= 105)) {
				gainF = tmpGainF;
				power = tmpPower;
				hi_pcd -= 5;
			}
		}
//		if (power > (maxPower - 0.5)) {
		while ((gainF > 99) && (hi_pcd > 32)){
			hi_pcd -= 4;
			art_ForceSinglePCDACTable(devNum, hi_pcd);
			Sleep(sleep_interval);
			power = pmMeasAvgPower(devPM, reset) + myAtten ;
			gainF = read_gainf_twice(devNum) ;
			if (CalSetup.customerDebug) {
				uiPrintf(", %3.2f[%d, %d]", power, hi_pcd, gainF);
			}
		}
//		if ((power > (maxPower - 0.5)) || (power < (maxPower - 1.5))) {
//			art_ForceSinglePCDACTable(devNum, hi_pcd);
//			Sleep(sleep_interval);
//			power = pmMeasAvgPower(devPM, reset) + myAtten ;
//			uiPrintf(", %3.2f[%d]", power, hi_pcd);
//		}
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[3] = (A_UINT16)(4*power + 0.5);
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[3] = hi_pcd;
		
		hi_pcd = lo_pcd + (A_UINT16)((hi_pcd - lo_pcd)*0.9);
		art_ForceSinglePCDACTable(devNum, hi_pcd);
		Sleep(sleep_interval);
		power = pmMeasAvgPower(devPM, reset) + myAtten ;
		if (CalSetup.customerDebug) {
			uiPrintf(", %3.2f[%d]", power, hi_pcd);
		}
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2] = (A_UINT16)(4*power + 0.5);
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2] = hi_pcd;
		
		hi_pcd = lo_pcd + (A_UINT16)((hi_pcd - lo_pcd)*0.7);
		if ((hi_pcd - lo_pcd) > 31) {
			hi_pcd = lo_pcd + 31; // because pcd_delta mask is only 5 bits
		}
		art_ForceSinglePCDACTable(devNum, hi_pcd);
		Sleep(sleep_interval);
		power = pmMeasAvgPower(devPM, reset) + myAtten ;
		if (CalSetup.customerDebug) {
			uiPrintf(", %3.2f[%d]", power, hi_pcd);
		}
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[1] = (A_UINT16)(4*power + 0.5);
		pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[1] = hi_pcd;
*/
//		if (CalSetup.cal_mult_xpd_gain_mask[mode] > 0) {
		if ( xgain_list[1] != xgain_list[0] ) {
			art_txContEnd(devNum);
			jj = xgain_list[1];
			if((swDeviceID & 0xff) >= 0x16) {
				art_writeField(devNum, "rf_pdgain_lo", jj);
				art_writeField(devNum, "rf_pdgain_hi", jj);
			} else {
				art_writeField(devNum, "rf_xpd_gain", jj);
			}
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].xpd_gain = (A_UINT16)jj;
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].numPcdacs = NUM_POINTS_XPD3;
			art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
							rates[0], DESC_ANT_A | USE_DESC_ANT);

			lo_pcd = 20;
			art_ForceSinglePCDACTable(devNum, lo_pcd);
			Sleep(sleep_interval);
			power = pmMeasAvgPower(devPM, reset) + myAtten ;
			if (CalSetup.customerDebug) {
				uiPrintf(", %3.2f[%d]", power, lo_pcd);
			}
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[0] = (A_UINT16)(4*power + 0.5);				
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[0]  = lo_pcd;

			lo_pcd = 35;
			art_ForceSinglePCDACTable(devNum, lo_pcd);
			Sleep(sleep_interval);
			power = pmMeasAvgPower(devPM, reset) + myAtten ;
			if (CalSetup.customerDebug) {
				uiPrintf(", %3.2f[%d]", power, lo_pcd);
			}
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[1] = (A_UINT16)(4*power + 0.5);				
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[1]  = lo_pcd;

			lo_pcd = max_pcdac;
			art_ForceSinglePCDACTable(devNum, lo_pcd);
			Sleep(sleep_interval);
			power = pmMeasAvgPower(devPM, reset) + myAtten ;
			if (CalSetup.customerDebug) {
				uiPrintf(", %3.2f[%d]", power, lo_pcd);
			}
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pwr_t4[2] = (A_UINT16)(4*power + 0.5);				
			pRawDataset_gen3[mode]->pDataPerChannel[ii].pDataPerXPD[jj].pcdac[2]  = lo_pcd;
		}
		
		if (CalSetup.customerDebug) {
			uiPrintf("\n");
		}
		art_txContEnd(devNum);

	} // end channel loop

	art_writeField(devNum, "rf_mixvga_ovr", mixvga_ovr);
	art_writeField(devNum, "rf_mixgain_ovr", mixgain_ovr);
}

void dump_raw_data_to_file_gen3( RAW_DATA_STRUCT_GEN3 *pRawData, char *filename )
{
	A_UINT16		i, j, kk;
    FILE			*fStream;
//	A_UINT16		single_xpd = 0xDEAD;
	A_UINT16		xgain_list[2];
	A_UINT16		xpd_mask;

	xgain_list[0] = 0xDEAD;
	xgain_list[1] = 0xDEAD;
 
	kk = 0;
	xpd_mask = pRawData->xpd_mask;

	for (j = 0; j < NUM_XPD_PER_CHANNEL; j++) {
		if (((xpd_mask >> j) & 1) > 0) {
			if (kk > 1) {
				uiPrintf("ERROR: A maximum of 2 xpd_gains supported in raw data\n");
				exit(0);
			}
			xgain_list[kk++] = (A_UINT16) j;			
		}
	}

	if (CalSetup.customerDebug)
		uiPrintf("\nWriting raw data to file %s\n", filename);
	
    if( (fStream = fopen( filename, "w")) == NULL ) {
        uiPrintf("Failed to open %s for writing - giving up\n", filename);
        return;
    }

	fprintf(fStream, "XPD_Gain_mask = 0x%x\n\n", pRawData->xpd_mask);

	//print the frequency values
	fprintf(fStream, "freq    pwr1    pwr2    pwr3    pwr4");
	if (xgain_list[1] != 0xDEAD) {
		fprintf(fStream, "   pwr1_x3  pwr2_x3   pwr3_x3\n");
		fprintf(fStream, "        [pcd]    [pcd]    [pcd]    [pcd]   [pcd]    [pcd]      [pcd]\n");
	} else {
		fprintf(fStream, "\n        [pcd]    [pcd]    [pcd]    [pcd]    \n");
	}

	for (i = 0; i < pRawData->numChannels; i++) {
		fprintf(fStream, "%d\t", pRawData->pChannels[i]);
		j = xgain_list[0];
		for (kk=0; kk<pRawData->pDataPerChannel[i].pDataPerXPD[j].numPcdacs; kk++) {
			fprintf(fStream, "%3.2f    ", (double)(pRawData->pDataPerChannel[i].pDataPerXPD[j].pwr_t4[kk]/4.0));
		}
		if (xgain_list[1] != 0xDEAD) {
			j = xgain_list[1];
			for (kk=0; kk<pRawData->pDataPerChannel[i].pDataPerXPD[j].numPcdacs; kk++) {
				fprintf(fStream, "%3.2f    ", (double)(pRawData->pDataPerChannel[i].pDataPerXPD[j].pwr_t4[kk]/4.0));
			}
		}
		fprintf(fStream, "%3.2f    ", (double)(pRawData->pDataPerChannel[i].maxPower_t4/4.0));
		fprintf(fStream,"\n\t");		
		for (kk=0; kk<pRawData->pDataPerChannel[i].pDataPerXPD[j].numPcdacs; kk++) {
			fprintf(fStream, "[%2d]    ", pRawData->pDataPerChannel[i].pDataPerXPD[j].pcdac[kk]);
		}
		if (xgain_list[1] != 0xDEAD) {
			j = xgain_list[1];
			for (kk=0; kk<pRawData->pDataPerChannel[i].pDataPerXPD[j].numPcdacs; kk++) {
				fprintf(fStream, "[%2d]    ", pRawData->pDataPerChannel[i].pDataPerXPD[j].pcdac[kk]);
			}
		}
		fprintf(fStream,"\n");		
	}
	
	fclose(fStream);
}

void raw_to_eeprom_dataset_gen3(RAW_DATA_STRUCT_GEN3 *pRawDataset, EEPROM_DATA_STRUCT_GEN3 *pCalDataset) {

	A_UINT16	ii, jj, kk;
	EEPROM_DATA_PER_CHANNEL_GEN3	*pCalCh;
	RAW_DATA_PER_CHANNEL_GEN3		*pRawCh;
	A_UINT16		xgain_list[2];
	A_UINT16		xpd_mask;

	xgain_list[0] = 0xDEAD;
	xgain_list[1] = 0xDEAD;
 
	pCalDataset->xpd_mask  = pRawDataset->xpd_mask;

	kk = 0;
	xpd_mask = pRawDataset->xpd_mask;

	for (jj = 0; jj < NUM_XPD_PER_CHANNEL; jj++) {
		if (((xpd_mask >> jj) & 1) > 0) {
			if (kk > 1) {
				uiPrintf("A maximum of 2 xpd_gains supported in raw data\n");
				exit(0);
			}
			xgain_list[kk++] = (A_UINT16) jj;			
		}
	}

	pCalDataset->numChannels = pRawDataset->numChannels;
	for (ii = 0; ii < pCalDataset->numChannels; ii++) {
		pCalCh = &(pCalDataset->pDataPerChannel[ii]);
		pRawCh = &(pRawDataset->pDataPerChannel[ii]);
		pCalCh->channelValue = pRawCh->channelValue;
		pCalCh->maxPower_t4  = pRawCh->maxPower_t4;

		jj = xgain_list[0];
		pCalCh->pcd1_xg0	   = (pRawCh->pDataPerXPD[jj].pcdac[0]);
		pCalCh->pcd2_delta_xg0 = (pRawCh->pDataPerXPD[jj].pcdac[1] - pRawCh->pDataPerXPD[jj].pcdac[0]);
		pCalCh->pcd3_delta_xg0 = (pRawCh->pDataPerXPD[jj].pcdac[2] - pRawCh->pDataPerXPD[jj].pcdac[1]);
		pCalCh->pcd4_delta_xg0 = (pRawCh->pDataPerXPD[jj].pcdac[3] - pRawCh->pDataPerXPD[jj].pcdac[2]);

		pCalCh->pwr1_xg0    = pRawCh->pDataPerXPD[jj].pwr_t4[0];
		pCalCh->pwr2_xg0    = pRawCh->pDataPerXPD[jj].pwr_t4[1];
		pCalCh->pwr3_xg0    = pRawCh->pDataPerXPD[jj].pwr_t4[2];
		pCalCh->pwr4_xg0    = pRawCh->pDataPerXPD[jj].pwr_t4[3];
		
		if (xgain_list[1] != 0xDEAD) {
			jj = xgain_list[1];
			pCalCh->pwr1_xg3    = pRawCh->pDataPerXPD[jj].pwr_t4[0];
			pCalCh->pwr2_xg3    = pRawCh->pDataPerXPD[jj].pwr_t4[1];
			pCalCh->pwr3_xg3    = pRawCh->pDataPerXPD[jj].pwr_t4[2];
		} else {
			pCalCh->pwr1_xg3    = 0;
			pCalCh->pwr2_xg3    = 0;
			pCalCh->pwr3_xg3    = 0;
		}
	}
}


void fill_words_for_eeprom_gen3(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask)
{

	A_UINT16 idx = 0;
	EEPROM_DATA_PER_CHANNEL_GEN3 *pCalCh;
	A_UINT16	ii;
	A_UINT16	pcdac_delta_mask = 0x1F;
	A_UINT16	pcdac_mask = 0x3F;
	A_UINT16	numPiers;

	// Group 1. 11a Frequency pier locations
	if(CalSetup.calPower && CalSetup.Amode)
	{
		word[idx++] = ( (fbin[1] & freqmask) << 8) | (fbin[0] & freqmask)  ;
		word[idx++] = ( (fbin[3] & freqmask) << 8) | (fbin[2] & freqmask)  ;
		word[idx++] = ( (fbin[5] & freqmask) << 8) | (fbin[4] & freqmask)  ;
		word[idx++] = ( (fbin[7] & freqmask) << 8) | (fbin[6] & freqmask)  ;
		word[idx++] = ( (fbin[9] & freqmask) << 8) | (fbin[8] & freqmask)  ;
	} else {
	}

	//Group 2. 11a calibration data for all frequency piers
	if(CalSetup.calPower && CalSetup.Amode)
	{
		
		for (ii=0; ii<pCalDataset_gen3[MODE_11a]->numChannels; ii++)
		{
			pCalCh = &(pCalDataset_gen3[MODE_11a]->pDataPerChannel[ii]);

			word[idx++] = ( ( (pCalCh->pwr1_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr4_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pcd2_delta_xg0 & pcdac_delta_mask) << 0) |
							( (pCalCh->pcd3_delta_xg0 & pcdac_delta_mask) << 5) |
							( (pCalCh->pcd4_delta_xg0 & pcdac_delta_mask) << 10) );

			word[idx++] = ( ( (pCalCh->pwr1_xg3 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg3 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg3 & dbmmask) << 0) |
//							( (pCalCh->maxPower_t4 & dbmmask) << 8) );
							( (pCalCh->pcd1_xg0 & pcdac_mask) << 8) ); // starting eeprom 4.3
		}
	} else
	{
	}


	//Group 3. 11b Calibration Piers and Calibration Information 

	if(CalSetup.calPower && CalSetup.Bmode)
	{

		numPiers = pCalDataset_gen3[MODE_11b]->numChannels;
		for (ii=0; ii<numPiers; ii++)
		{
			fbin[ii] = freq2fbin(pCalDataset_gen3[MODE_11b]->pChannels[ii]) ;
		}
		if (numPiers < NUM_PIERS_2p4) {
			for (ii=numPiers; ii<NUM_PIERS_2p4; ii++) {
				fbin[ii] = 0;
			}
		}

		for (ii=0; ii<pCalDataset_gen3[MODE_11b]->numChannels; ii++)
		{
			pCalCh = &(pCalDataset_gen3[MODE_11b]->pDataPerChannel[ii]);

			word[idx++] = ( ( (pCalCh->pwr1_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr4_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pcd2_delta_xg0 & pcdac_delta_mask) << 0) |
							( (pCalCh->pcd3_delta_xg0 & pcdac_delta_mask) << 5) |
							( (pCalCh->pcd4_delta_xg0 & pcdac_delta_mask) << 10) );

			word[idx++] = ( ( (pCalCh->pwr1_xg3 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg3 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg3 & dbmmask) << 0) |
//							( (pCalCh->maxPower_t4 & dbmmask) << 8) );
							( (pCalCh->pcd1_xg0 & pcdac_mask) << 8) ); // starting eeprom 4.3
		}
	} else
	{
	}


	//Group 4. 11g Calibration Piers and Calibration Information 

	if(CalSetup.calPower && CalSetup.Gmode)
	{
		numPiers = pCalDataset_gen3[MODE_11g]->numChannels;
		for (ii=0; ii<numPiers; ii++)
		{
			fbin[ii] = freq2fbin(pCalDataset_gen3[MODE_11g]->pChannels[ii]) ;
		}
		if (numPiers < NUM_PIERS_2p4) {
			for (ii=numPiers; ii<NUM_PIERS_2p4; ii++) {
				fbin[ii] = 0;
			}
		}

		for (ii=0; ii<pCalDataset_gen3[MODE_11g]->numChannels; ii++)
		{
			pCalCh = &(pCalDataset_gen3[MODE_11g]->pDataPerChannel[ii]);

			word[idx++] = ( ( (pCalCh->pwr1_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr4_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pcd2_delta_xg0 & pcdac_delta_mask) << 0) |
							( (pCalCh->pcd3_delta_xg0 & pcdac_delta_mask) << 5) |
							( (pCalCh->pcd4_delta_xg0 & pcdac_delta_mask) << 10) );

			word[idx++] = ( ( (pCalCh->pwr1_xg3 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg3 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg3 & dbmmask) << 0) |
//							( (pCalCh->maxPower_t4 & dbmmask) << 8) );
							( (pCalCh->pcd1_xg0 & pcdac_mask) << 8) ); // starting eeprom 4.3
		}
	} else
	{
	}

	fill_Target_Power_and_Test_Groups(&(word[idx]), (A_UINT16)(numWords - idx + 1), dbmmask, pcdmask, freqmask);
}

void read_dataset_from_file_gen3(RAW_DATA_STRUCT_GEN3 *pDataSet, char *filename) {

	uiPrintf("Reading raw data from file not supported for gen3 yet. Please provide force_piers_list\n");
	exit(0);

}

void golden_iq_cal(A_UINT32 devNum, A_UINT32 mode, A_UINT32 channel) {

	A_UINT32  maxIter = 40;
	A_UINT32  iter = 0;
	A_UINT32  complete_timeout = 4000;
	A_BOOL    TransmitOn = FALSE;

	configSetup.eepromLoad = 1;
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
						(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)mode, configSetup.use_init);		

	art_resetDevice(devNum, txStation, NullID, channel, 0);
	art_forceSinglePowerTxMax(devNum, 10);

	waitForAck(CalSetup.customerDebug);
	while (!verifyAckStr("Done with iq_cal for mode") && (iter++ < (maxIter + 1))) {
		if (REWIND_TEST) {
			if (TransmitOn) {
				art_txContEnd(devNum);
				TransmitOn = FALSE;
			}
			return;
		}
		//art_txDataSetup(devNum, IQ_CAL_RATEMASK, rxStation, 4*IQ_CAL_NUMPACKETS, 100, pattern, 2, 0, 2, 1);
		if (!TransmitOn) {
			art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, IQ_CAL_RATE, DESC_ANT_A | USE_DESC_ANT);
			TransmitOn = TRUE;
		}

		Sleep(100);
		sendAck(devNum, "Ready to TX for iq_cal at", mode, channel, 0,CalSetup.customerDebug);
//		Sleep(30);
//		art_txDataBegin(devNum, complete_timeout, 0);
		waitForAck(CalSetup.customerDebug);
		if (verifyAckStr("Failed iq_cal. Rewind Test")) {				
			uiPrintf("\n\n*************** Failed iq_cal for mode %s. Retry. ********************\n",modeName[calModeFor[mode]]);
			uiPrintf("recv Str = %s\n", ackRecvStr);
			REWIND_TEST = TRUE;
		}
	}
	if (TransmitOn) {
		art_txContEnd(devNum);
		TransmitOn = FALSE;
	}
	sendAck(devNum, "Consent to quit iq_cal", 0, 0, 0, CalSetup.customerDebug);
}

A_UINT16 dut_iq_cal(A_UINT32 devNum, A_UINT32 mode, A_UINT32 channel) {
	
// mode is the devlibmode here

	A_UINT32 iter = 0;
	A_UINT32 maxIter = 40;
	A_UINT32 iterRX = 0;
	A_UINT32 maxIterRX = 200;
//	double pwr_meas_i = 0;
//	double pwr_meas_q = 0;
	A_UINT32 pwr_meas_i = 0;
	A_UINT32 pwr_meas_q = 0;
	A_UINT32 iq_corr_meas = 0, iq_corr_neg = 0;
	A_UINT32 i_coeff, q_coeff;
	A_UINT32 rddata;

#ifdef _IQV
	double	power;
#endif // _IQV
	uiPrintf("\nPerforming iq_cal for mode %s :", modeName[calModeFor[mode]]);

#ifndef _IQV
	sendAck(devNum, "Initiate iq_cal at mode", mode, channel, 0, CalSetup.customerDebug);
#else	
	IQ_dut_freq_hz(((double) (channel))*1000000);
//	IQ_dut_mode (iq_mode);
	power = IQ_get_IQ_cal_power();
	if (!IQPER_preset( ((double) (channel))*1000000, power, "6_iq_cal.mod", 0, 0))
		// chann, power, wave, turbo, TxNoStop
		return FALSE;
#endif // _IQV

//	art_resetDevice(devNum, rxStation, NullID, channel, 0);

	while ((iter++ < maxIter) && 
		((pwr_meas_i < 0x0FFFFFFF) || (pwr_meas_q < 0x0FFFFFFF) || (iq_corr_meas == 0)) ){

		if (!art_resetDevice(devNum, rxStation, NullID, channel, 0))
			return FALSE;
		
#ifndef _IQV
		sendAck(devNum, "Ready to RX for iq_cal at", mode, channel, iter, CalSetup.customerDebug);
		waitForAck(CalSetup.customerDebug) ;
		if (!verifyAckStr("Ready to TX for iq_cal at")) {
			uiPrintf("ERROR: Got out of sync in iq_cal [ackStr: %s]\n", ackRecvStr);
			return FALSE;
		}
#endif // _IQV
		Sleep(10);

		art_regWrite(devNum, 0x9920, 0x1f000) ; // enable iq_cal and set sample size to 11
		rddata = art_regRead(devNum, 0x9920);
		rddata = (rddata >> 16) & 0x1;

		iterRX = 0;
		while ((rddata == 0x1) && (iterRX++ < maxIterRX)){
			rddata = art_regRead(devNum, 0x9920);
			rddata = (rddata >> 16) & 0x1;
			Sleep(10);
//			uiPrintf("[%d] rddata = %d\n", iterRX, rddata);
			
		}

		if (iterRX == maxIterRX) {
			uiPrintf("iq_cal iter : %d.\n", iter);
		}

		pwr_meas_i = 0;
		pwr_meas_q = 0;
		iq_corr_meas = 0;

		pwr_meas_i   = art_regRead(devNum, (0x9800 + (260 << 2))) ;
		pwr_meas_q   = art_regRead(devNum, (0x9800 + (261 << 2))) ;
		iq_corr_meas = art_regRead(devNum, (0x9800 + (262 << 2))) ;

//	uiPrintf("SNOOP: pwr_I = 0x%x, pwr_Q = 0x%x, IQ_Corr = 0x%x\n", pwr_meas_i, pwr_meas_q, iq_corr_meas);

		iq_corr_neg = 0;


	}

	if ((iter > maxIter) && 
		((pwr_meas_i < 0x0FFFFFFF) || (pwr_meas_q < 0x0FFFFFFF) || (iq_corr_meas == 0)) ){
			uiPrintf("\n\n*************** Failed iq_cal for mode %s. Retry. ********************\n",modeName[calModeFor[mode]]);
			REWIND_TEST = TRUE;
			sendAck(devNum, "Failed iq_cal. Rewind Test", 0, 0, 0, CalSetup.customerDebug);
			return FALSE;
	}

	if (iq_corr_meas >= 0x80000000) {
		iq_corr_meas = 0xFFFFFFFF - iq_corr_meas;
		iq_corr_neg = 1;
	}


    i_coeff	= (A_UINT32)floor((128.0*(double)iq_corr_meas)/(((double)pwr_meas_i+(double)pwr_meas_q)/2.0) + 0.5);
    q_coeff = (A_UINT32)floor((((double)pwr_meas_i/(double)pwr_meas_q) - 1.0)*128.0  + 0.5);

//	uiPrintf("SNOOP: interm i_coeff = %d, q_coeff = %d\n", i_coeff, q_coeff);


	if (CalSetup.customerDebug > 0) {
		uiPrintf("pwr_I = %d, pwr_Q = %d, IQ_Corr = %d\n", pwr_meas_i, pwr_meas_q, iq_corr_meas);
	}

	// print i_coeff/q_coeff before truncation.
	uiPrintf("      i_coeff = %d, q_coeff = %d\n", i_coeff, q_coeff);

    i_coeff = (i_coeff & 0x3f);
    if (iq_corr_neg == 0x0) {
        i_coeff = 0x40 - i_coeff;
    }
    q_coeff = q_coeff & 0x1f;

//	uiPrintf("      i_coeff = %d, q_coeff = %d\n", i_coeff, q_coeff);
	
    rddata = art_regRead(devNum, 0x9920);
    rddata = rddata | (1 << 11) | (i_coeff << 5) | q_coeff;
    art_regWrite(devNum, 0x9920, rddata);

	CalSetup.iqcal_i_corr[calModeFor[mode]] = i_coeff;
	CalSetup.iqcal_q_corr[calModeFor[mode]] = q_coeff;
#ifndef _IQV
	sendAck(devNum, "Done with iq_cal for mode", mode, channel, 0, CalSetup.customerDebug);
	waitForAck(CalSetup.customerDebug);
#endif
	return TRUE;
}

// returns numWords and eeprom cal data for desired mode : MODE_11a, _11b, _11g
// starts filling array "word" from idx=0, so feed the pointer to the beginning of where you need to start filling
void get_cal_info_for_mode_gen3(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask, A_UINT32 mode)
{

	A_UINT16    idx = 0;
	EEPROM_DATA_PER_CHANNEL_GEN3 *pCalCh;
	A_UINT16	ii;
	A_UINT16	pcdac_delta_mask = 0x1F;
	A_UINT16	pcdac_mask = 0x3F;
	A_UINT32 length;

	length = checkSumLength;

	// ideally want a more stringent check to ensure not to walk off the end of array
	// but don't know how many words remain in "word" array. this is the absolute upper limit.
	//assert(numWords < 0x400) ;
	assert(numWords < length) ;

	if(mode == MODE_11a)
	{
		// Group 1. 11a Frequency pier locations	
		word[idx++] = ( (fbin[1] & freqmask) << 8) | (fbin[0] & freqmask)  ;
		word[idx++] = ( (fbin[3] & freqmask) << 8) | (fbin[2] & freqmask)  ;
		word[idx++] = ( (fbin[5] & freqmask) << 8) | (fbin[4] & freqmask)  ;
		word[idx++] = ( (fbin[7] & freqmask) << 8) | (fbin[6] & freqmask)  ;
		word[idx++] = ( (fbin[9] & freqmask) << 8) | (fbin[8] & freqmask)  ;

		//Group 2. 11a calibration data for all frequency piers		
		for (ii=0; ii<pCalDataset_gen3[MODE_11a]->numChannels; ii++)
		{
			pCalCh = &(pCalDataset_gen3[MODE_11a]->pDataPerChannel[ii]);

			word[idx++] = ( ( (pCalCh->pwr1_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr4_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pcd2_delta_xg0 & pcdac_delta_mask) << 0) |
							( (pCalCh->pcd3_delta_xg0 & pcdac_delta_mask) << 5) |
							( (pCalCh->pcd4_delta_xg0 & pcdac_delta_mask) << 10) );

			word[idx++] = ( ( (pCalCh->pwr1_xg3 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg3 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg3 & dbmmask) << 0) |
//							( (pCalCh->maxPower_t4 & dbmmask) << 8) );
							( (pCalCh->pcd1_xg0 & pcdac_mask) << 8) ); // starting eeprom 4.3
		}

		if (idx > numWords) {
			uiPrintf("ERROR: get_cal_info_for_mode_gen3: ran over expected numwords [%d] in 11a cal data : %d (actual)\n", idx, numWords);
			exit(0);
		}
	} 

	
	if(mode == MODE_11b)
	{
		//Group 3. 11b Calibration Piers and Calibration Information 
		for (ii=0; ii<pCalDataset_gen3[MODE_11b]->numChannels; ii++)
		{
			pCalCh = &(pCalDataset_gen3[MODE_11b]->pDataPerChannel[ii]);

			word[idx++] = ( ( (pCalCh->pwr1_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr4_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pcd2_delta_xg0 & pcdac_delta_mask) << 0) |
							( (pCalCh->pcd3_delta_xg0 & pcdac_delta_mask) << 5) |
							( (pCalCh->pcd4_delta_xg0 & pcdac_delta_mask) << 10) );

			word[idx++] = ( ( (pCalCh->pwr1_xg3 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg3 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg3 & dbmmask) << 0) |
//							( (pCalCh->maxPower_t4 & dbmmask) << 8) );
							( (pCalCh->pcd1_xg0 & pcdac_mask) << 8) ); // starting eeprom 4.3
		}

		if (idx > numWords) {
			uiPrintf("ERROR: get_cal_info_for_mode_gen3: ran over expected numwords [%d] in 11b cal data : %d (actual)\n", idx, numWords);
			exit(0);
		}
	} 	

	if(mode == MODE_11g)
	{
		//Group 4. 11g Calibration Piers and Calibration Information 
		for (ii=0; ii<pCalDataset_gen3[MODE_11g]->numChannels; ii++)
		{
			pCalCh = &(pCalDataset_gen3[MODE_11g]->pDataPerChannel[ii]);

			word[idx++] = ( ( (pCalCh->pwr1_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg0 & dbmmask) << 0) |
							( (pCalCh->pwr4_xg0 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pcd2_delta_xg0 & pcdac_delta_mask) << 0) |
							( (pCalCh->pcd3_delta_xg0 & pcdac_delta_mask) << 5) |
							( (pCalCh->pcd4_delta_xg0 & pcdac_delta_mask) << 10) );

			word[idx++] = ( ( (pCalCh->pwr1_xg3 & dbmmask) << 0) |
							( (pCalCh->pwr2_xg3 & dbmmask) << 8) );

			word[idx++] = ( ( (pCalCh->pwr3_xg3 & dbmmask) << 0) |
//							( (pCalCh->maxPower_t4 & dbmmask) << 8) );
							( (pCalCh->pcd1_xg0 & pcdac_mask) << 8) ); // starting eeprom 4.3
		}

		if (idx > numWords) {
			uiPrintf("ERROR: get_cal_info_for_mode_gen3: ran over expected numwords [%d] in 11g cal data : %d (actual)\n", idx, numWords);
			exit(0);
		}
	} 
}
