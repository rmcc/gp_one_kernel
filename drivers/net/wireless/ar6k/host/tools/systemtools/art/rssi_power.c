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

#include "manlibInst.h" /* The Manufacturing Library Instrument Library extension */
#include "mEeprom.h"    /* Definitions for the data structure */
#include "dynamic_optimizations.h"
#include "maui_cal.h"   /* Definitions for the Calibration Library */
#include "rssi_power.h" /* Definitions for the rssi to power cal */
#include "mathRoutines.h" 
#include "test.h"
#include "parse.h"
#include "dk_cmds.h"

#if defined(LINUX) || defined(__linux__)
#include "linux_ansi.h"
#endif

#include "art_if.h"
#include "ar5212/mEEPROM_d.h"
#include "cal_gen3.h"
//++JC++
#include "ar2413/mEEPROM_g.h"
#include "cal_gen5.h"
//++JC++

extern MLD_CONFIG configSetup;
extern ART_SOCK_INFO *artSockInfo;
//extern A_BOOL printLocalInfo;
	   
// extern declarations for dut-golden sync
extern ART_SOCK_INFO *pArtPrimarySock;
extern ART_SOCK_INFO *pArtSecondarySock;
extern A_INT32 ackRecvPar1, ackRecvPar2, ackRecvPar3;
extern char ackRecvStr[1024];

extern GOLDEN_PARAMS  goldenParams ;

extern A_INT32 devPM, devSA, devATT;
extern A_UINT16 numRAWChannels; //will be computed prior to measurements.
extern A_UINT16 *RAW_CHAN_LIST ; // can accomodate 5000 - 6000 in steps of 5 MHz
extern A_UINT16 *RAW_PCDACS ;
extern A_UINT16 sizeOfRawPcdacs ;

extern dPCDACS_EEPROM RawDataset ; // raw power measurements
extern dPCDACS_EEPROM *pRawDataset  ; // raw power measurements
extern dPCDACS_EEPROM RawGainDataset ; // raw gainf measurements
extern dPCDACS_EEPROM *pRawGainDataset  ; // raw gainf measurements
extern dPCDACS_EEPROM CalDataset ; // calibration dataset
extern dPCDACS_EEPROM *pCalDataset ; // calibration dataset
extern dPCDACS_EEPROM FullDataset ; // full dataset
extern dPCDACS_EEPROM *pFullDataset  ; // full dataset

extern dPCDACS_EEPROM RawDataset_11g ; // raw power measurements for 11g
extern dPCDACS_EEPROM RawDataset_11b ; // raw power measurements for 11b
extern dPCDACS_EEPROM *pRawDataset_2p4[2]  ; // raw power measurements
extern dPCDACS_EEPROM RawGainDataset_11g ; // raw power measurements for 11b
extern dPCDACS_EEPROM RawGainDataset_11b ; // raw power measurements for 11b
extern dPCDACS_EEPROM *pRawGainDataset_2p4[2] ; // raw power measurements
extern dPCDACS_EEPROM CalDataset_11g ; // calibration dataset
extern dPCDACS_EEPROM CalDataset_11b ; // calibration dataset
extern dPCDACS_EEPROM *pCalDataset_2p4[2] ; // calibration dataset

extern char calPowerLogFile_2p4[2][122] ;

extern A_UINT16 numRAWChannels_2p4 ; 
extern A_UINT16 RAW_CHAN_LIST_2p4[3] ; // Never change them. These values are NOT stored 
														   // on EEPROM, but are hardcoded in the driver instead.
extern char modeName[3][122] ;


static A_UCHAR  bssID[6]     = {0x50, 0x55, 0x5A, 0x50, 0x00, 0x00};
static A_UCHAR  rxStation[6] = {0x10, 0x11, 0x12, 0x13, 0x00, 0x00};	// DUT
static A_UCHAR  txStation[6] = {0x20, 0x22, 0x24, 0x26, 0x00, 0x00};	// Golden
static A_UCHAR  NullID[6]    = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
static A_UCHAR  pattern[2] = {0xaa, 0x55};

static FILE     *logGU;
static FILE     *refGU;
static FILE     *rawGU;
extern A_INT32 guDevPM, guDevAtt;

COEFF_DATA_STRUCT GoldenCoeffs;
COEFF_DATA_STRUCT *pGoldenCoeffs = &GoldenCoeffs;

A_UINT16 getCalData(A_UINT32 devNum, A_UINT32 mode, A_UINT32 debug) 
{
	
	if (CalSetup.useFastCal)
	{
		getCalDataUsingGU(devNum, mode, GET_GU_PWR, debug) ;
	} else if (CalSetup.eeprom_map >= CAL_FORMAT_GEN5) {
//		measure_all_channels_gen5(devNum, CalSetup.customerDebug, mode) ;
		if (!measure_all_channels_generalized_gen5(devNum, CalSetup.customerDebug, mode) )
			return FALSE;
	} else if (CalSetup.eeprom_map == CAL_FORMAT_GEN3) {
		measure_all_channels_gen3(devNum, CalSetup.customerDebug, mode) ;
	} else 
	{
		switch (mode) {
		case MODE_11a :
			if (!measure_all_channels(devNum, CalSetup.customerDebug))
				return FALSE;
			break;
		case MODE_11b :
		case MODE_11g :
			if (!measure_all_channels_2p4(devNum, CalSetup.customerDebug, mode) )
				return FALSE;
			break ;
		default :
			uiPrintf("Illegal mode specified in getCalData : %d\n", mode);
			return FALSE;
		}
	} 
	return TRUE;
} // getCalData

void getCalDataUsingGU(A_UINT32 devNum, A_UINT32 mode, A_UINT32 opcode, A_UINT32 debug) 
{

	A_UINT16 ch ;
	A_INT16 ii, jj;
	double pwrList[64];
	A_UCHAR  devlibMode ; // use setResetParams to communicate appropriate mode to devlib
	A_UINT32  time1;
		
	dPCDACS_EEPROM *pRawDatasetLocal;
	dPCDACS_EEPROM *pRawGainDatasetLocal;

	dDATA_PER_CHANNEL	*pRawChannelData;
	dDATA_PER_CHANNEL	*pGainfChannelData;

	switch (mode) {
	case MODE_11a:
		devlibMode = MODE_11A;
		break; 
	case MODE_11g :
		devlibMode = MODE_11G;
		break;
	case MODE_11b :
		devlibMode = MODE_11B;
		break;
	default:
		uiPrintf("Unknown mode : %d \n", mode);
		exit(0);
		break;
	}

	
//	gpibONL(devATT, 0); // take att offline. relinquish for golden.
//	gpibONL(devPM, 0); // take att offline. relinquish for golden.
//	gpibRSC(0, 0);  // DUT to release system control (0);

	configSetup.eepromLoad = 0;
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)devlibMode, configSetup.use_init);	

	if (mode != MODE_11a)
	{
		pRawDatasetLocal = pRawDataset_2p4[mode] ;
		pRawGainDatasetLocal = pRawGainDataset_2p4[mode] ;
	} else
	{
		pRawDatasetLocal = pRawDataset ;
		pRawGainDatasetLocal = pRawGainDataset ;
	}
	
	uiPrintf("\nCollecting raw data for the adapter for mode %s\n", modeName[mode]);

	setupDutForPwrMeas(devNum, opcode, mode, debug);
	if (debug)
		uiPrintf("numChannels=%d for mode %s\n", pRawDatasetLocal->numChannels, modeName[mode]);

	time1 = milliTime();

	for (ii=0; ii<pRawDatasetLocal->numChannels; ii++)
	{		
		ch = pRawDatasetLocal->pDataPerChannel[ii].channelValue;
		if (debug)
			uiPrintf("starting channel %d\n", ch);
		pRawChannelData = &(pRawDatasetLocal->pDataPerChannel[ii]) ;
		pGainfChannelData = &(pRawGainDatasetLocal->pDataPerChannel[ii]) ;
				
		if (mode == MODE_11a)
		{
//			setCornerFixBits(devNum, ch);
		}

		art_changeChannel(devNum, ch); // can't resetDevice else setupDutForPwrMeas gets erased.
		retrievePwrData(devNum, opcode, ch, mode, RAW_PCDACS, sizeOfRawPcdacs, &(pwrList[0]), debug);
		uiPrintf("ch: %d, time = %d\n", ch, (A_UINT16)(milliTime()-time1)/1000);

		for (jj=0; jj<sizeOfRawPcdacs; jj++)
		{
			pRawChannelData->pPwrValues[jj] = pwrList[jj] ;
			pGainfChannelData->pPwrValues[jj] = 0.0 ;
		}
	} // channels

} // getCalDataUsingGU


void setupDutForPwrMeas(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 mode, A_UINT32 debug)
{
	A_UINT32	ch;
	A_UINT32	rddata, wrdata;
//	A_INT32		minccapwr;
	A_INT32		maxccapwr  = -80;
	A_UINT16	ii, allpcdacs[64], kk=0;

	ch = ((mode == MODE_11a) ? 5170 : 2412) ;

	// disable noise cal
	art_resetDevice(devNum, txStation, bssID, ch, 0);
// /*
	Sleep(50);

	rddata = art_regRead(devNum, (0x9800 + (24<<2)));
	wrdata = rddata & ~0x8002;
	art_regWrite(devNum, (0x9800 + (24<<2)), wrdata);

	// force noise floor
//    art_regWrite(devNum, 0x9800+(25<<2), ( (maxccapwr<0) ? (abs(maxccapwr*2) ^ 0x1ff)+1 : maxccapwr*2));

//	uiPrintf("forceval=%x\n",( (maxccapwr<0) ? (abs(maxccapwr*2) ^ 0x1ff)+1 : maxccapwr*2));
	rddata = art_regRead(devNum, 0x9800+(25<<2));
	wrdata = (rddata & ~(0x1ff)) | (( (maxccapwr<0) ? (abs(maxccapwr*2) ^ 0x1ff)+1 : maxccapwr*2));
    art_regWrite(devNum, 0x9800+(25<<2), wrdata);

//	uiPrintf("rddata = %x, wrdata = %x\n", rddata, wrdata);

	rddata = art_regRead(devNum, (0x9800 + (24<<2)));
	wrdata = rddata | 0x0002;
	art_regWrite(devNum, (0x9800 + (24<<2)), wrdata);
/*
	uiPrintf("forceval=%x\n",wrdata);

 	rddata = 1;
	while ( rddata > 0)
	{
		rddata = ( art_regRead(devNum, (0x9800 + (24<<2))) & 0x2); 
		//uiPrintf("(%d) rddata = %x\n", kk++, rddata);
	}

	rddata = art_regRead(devNum, (0x9800+(25<<2)));
    minccapwr = (rddata >> 19) & 0x1ff;
    if (minccapwr & 0x100) { 
        minccapwr = -((minccapwr ^ 0x1ff) + 1);
    }
    uiPrintf( "Minimum CCA power was forced to %d dBm\n\n", minccapwr);
*/	
	for(ii=0; ii<64; ii++)
		allpcdacs[ii] = ii;

	art_ForcePCDACTable(devNum, allpcdacs);

} // setupDutForPwrMeas

void retrievePwrData(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 channel, A_UINT32 mode, 
						A_UINT16 *pcdacs, A_UINT32 numPcd, double *retVals, 
						A_UINT32 debug)
{

	A_UINT32 ii, jj, numAvg;
	A_UCHAR  datapattern[1] = {0x00};
	A_UINT32 broadcast = 1;
	A_UINT32 complete_timeout = 1000;
	A_UINT32 statsMode = 0;
	A_UINT32 dataCh;
	A_UINT32 time1;

	numAvg = (opcode == CAL_PM_PWR) ? 1 : NUM_POWER_AVERAGES ;

	for (ii=0; ii<numAvg; ii++)
	{
		if (opcode != CAL_PM_PWR)
		{	
			sendAck(devNum, "Prepare to begin receive for power meas at ", channel, ii, opcode, debug);
			waitForAck(debug);
		}

		for( jj=0; jj<numPcd; jj++)
		{
			art_forceSinglePowerTxMax(devNum, pcdacs[jj]);
			datapattern[0] = pcdacs[jj] & 0xff ;
//			art_txDataSetup(devNum, RATE_6, rxStation, NUM_POWER_PACKETS, LEN_POWER_PACKETS, datapattern, 1, 0, 
//							(USE_DESC_ANT | DESC_ANT_A), broadcast);
//			if (opcode == CAL_PM_PWR)
			if (1) // handshake in all cases.
			{	
				sendAck(devNum, "Prepare for RX at pcdac = ", channel, pcdacs[jj], opcode, debug);
				waitForAck(debug);
			}
			Sleep(10);
			//Sleep(1);
			time1 = milliTime();
			txContFrameBegin(devNum, LEN_POWER_PACKETS, IFS_IN_SLOTS, RANDOM_PATTERN, 6, (USE_DESC_ANT | DESC_ANT_A), 0, NUM_POWER_PACKETS, rxStation);
//			art_txDataBegin(devNum, complete_timeout, statsMode);
//			uiPrintf("txdatabegin time: %d\n", milliTime() - time1);
		} // for (0..numPcd)
	} // for 0..numAvg

	if (opcode == GET_GU_PWR)
	{
		sendAck(devNum, "Send Power List", numPcd, channel, numAvg, debug);
		waitForAck(debug);
		extractDoublesFromStr(ackRecvStr, retVals, numPcd, &(dataCh), debug);
		if (dataCh != channel)
		{
			uiPrintf("dataChannel (%d) is different from channel (%d)\n", dataCh, channel);
			exit(0);
		}
	}
} // retrievePwrData

void extractDoublesFromStr(char *srcStr, double *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug)
{
	char		delimiters[] = " \t" ;
	char		*tmpstr, str1[1024];
	A_UINT32	ii=0;

	strcpy(str1, srcStr);

	tmpstr = strtok(str1, delimiters);
	if (!sscanf(tmpstr, "%d", par1))
		uiPrintf("Unable to read par1 from string: %s\n", tmpstr);

	tmpstr = strtok(NULL, delimiters);

	while((tmpstr != NULL) && (ii < size))
	{
		if (!sscanf(tmpstr, "%lf", &(retVals[ii]) ))
			uiPrintf("Unable to read value %d from string: %s\n", ii, tmpstr);
		tmpstr = strtok(NULL, delimiters);
		ii++;
	}

	if (ii != size)
	{
		uiPrintf("Unable to extract all %d values from string %s\n", size, srcStr);
		exit(0);
	}
} // extractDoublesFromStr

void extractDoublesFromScientificStr(char *srcStr, double *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug)
{
	char		delimiters[] = " \t" ;
	char		*tmpstr, str1[1024];
	A_UINT32	ii=0;

	strcpy(str1, srcStr);

	tmpstr = strtok(str1, delimiters);
	if (!sscanf(tmpstr, "%d", par1))
		uiPrintf("Unable to read par1 from string: %s\n", tmpstr);

	tmpstr = strtok(NULL, delimiters);

	if (debug) uiPrintf("\nSNOOP: %s\nExtracted vals: ", srcStr);

	while((tmpstr != NULL) && (ii < size))
	{
		if (!sscanf(tmpstr, "%le", &(retVals[ii]) ))
			uiPrintf("Unable to read value %d from string: %s\n", ii, tmpstr);
		//uiPrintf("%s->%le (%d), ", tmpstr, retVals[ii], ii);
		tmpstr = strtok(NULL, delimiters);		
		ii++;
	}

	if (ii != size)
	{
		uiPrintf("Unable to extract all %d values from string %s\n", size, srcStr);
		exit(0);
	}
} // extractDoublesFromScientificStr

void extractFloatsFromStr(char *srcStr, float *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug)
{
	char		delimiters[] = " \t" ;
	char		*tmpstr, str1[1024];
	A_UINT32	ii=0;

	strcpy(str1, srcStr);

	tmpstr = strtok(str1, delimiters);
	if (!sscanf(tmpstr, "%d", par1))
		uiPrintf("Unable to read par1 from string: %s\n", tmpstr);

	tmpstr = strtok(NULL, delimiters);

	while((tmpstr != NULL) && (ii < size))
	{
		if (!sscanf(tmpstr, "%f", &(retVals[ii]) ))
			uiPrintf("Unable to read value %d from string: %s\n", ii, tmpstr);
		tmpstr = strtok(NULL, delimiters);
		ii++;
	}

	if (ii != size)
	{
		uiPrintf("Unable to extract all %d values from string %s\n", size, srcStr);
		exit(0);
	}
} // extractFloatsFromStr

void extractIntsFromStr(char *srcStr, A_UINT32 *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug)
{
	char		delimiters[] = " \t" ;
	char		*tmpstr, str1[1024];
	A_UINT32	ii=0;

	strcpy(str1, srcStr);

	tmpstr = strtok(str1, delimiters);
	if (!sscanf(tmpstr, "%d", par1))
		uiPrintf("Unable to read par1 from string: %s\n", tmpstr);

	tmpstr = strtok(NULL, delimiters);

	while((tmpstr != NULL) && (ii < size))
	{
		if (!sscanf(tmpstr, "%d", &(retVals[ii])))
			uiPrintf("Unable to read value %d from string: %s\n", ii, tmpstr);
		tmpstr = strtok(NULL, delimiters);
		ii++;
	}

	if (ii != size)
	{
		uiPrintf("Unable to extract all %d values from string %s\n", size, srcStr);
		exit(0);
	}
} // extractIntsFromStr




void setupGoldenForCal(A_UINT32 devNum, A_UINT32 debug)
{

	//11a calibration
    if(CalSetup.calPower && CalSetup.Amode) {
        sendCalDataFromGU(devNum, MODE_11a, GET_GU_PWR, debug);
    } 

	// 11g calibration
	if(CalSetup.calPower && CalSetup.Gmode){
        sendCalDataFromGU(devNum, MODE_11g, GET_GU_PWR, debug);
   } 

	// 11b calibration
	if(CalSetup.calPower && CalSetup.Bmode){
        sendCalDataFromGU(devNum, MODE_11b, GET_GU_PWR, debug);
    } 
} // setupGoldenForCal

void sendCalDataFromGU(A_UINT32 devNum, A_UINT32 mode, A_UINT32 opcode, A_UINT32 debug) 
{
	A_UINT16 ch ;
	A_INT16 ii;
	double pwrList[64];
	A_UCHAR  devlibMode ; // use setResetParams to communicate appropriate mode to devlib
	dPCDACS_EEPROM *pRawDatasetLocal;
	char cmd[1024];
	
	switch (mode) {
	case MODE_11a:
		devlibMode = MODE_11A;
		break; 
	case MODE_11g :
		devlibMode = MODE_11G;
		break;
	case MODE_11b :
		devlibMode = MODE_11B;
		break;
	default:
		uiPrintf("Unknown mode : %d \n", mode);
		exit(0);
		break;
	}

	configSetup.eepromLoad = 0;
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)devlibMode, configSetup.use_init);	

	if (mode != MODE_11a)
	{
		pRawDatasetLocal = pRawDataset_2p4[mode] ;
	} else
	{
		pRawDatasetLocal = pRawDataset ;
	}
	
	uiPrintf("\nAssisting with raw data collection for mode %s\n", modeName[mode]);
	setupGUForPwrMeas(devNum, opcode, mode, debug);
	fprintf(logGU, "%d\n", pRawDatasetLocal->numChannels);
	writeIntListToStr(cmd, sizeOfRawPcdacs, &(RAW_PCDACS[0]), sizeOfRawPcdacs);
	fprintf(logGU, cmd);

	if (debug)
		uiPrintf("numChannels=%d for mode %s\n", pRawDatasetLocal->numChannels, modeName[mode]);

	for (ii=0; ii<pRawDatasetLocal->numChannels; ii++)
	{
		ch = pRawDatasetLocal->pDataPerChannel[ii].channelValue;
		if (debug)
			uiPrintf("starting channel %d\n", ch);
		art_changeChannel(devNum, ch);
		sendPwrData(devNum, opcode, ch, mode, RAW_PCDACS, sizeOfRawPcdacs, &(pwrList[0]), debug);
	}

	fclose(logGU);
	attSet(guDevAtt, MAX_ATTENUATOR_RANGE);

} // sendCalDataFromGU



void setupGUForPwrMeas(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 mode, A_UINT32 debug)
{

	double		minAtten, maxCcaPower, pmAtten, fixedAtten;
	char		filename[64], parseFilename[64];
	A_UINT32	filetype;
	A_UINT32	atten;
	A_UINT32	ch, rddata, wrdata, kk=0;
	A_INT32		minccapwr;

	ch = ((mode == MODE_11a) ? 5170 : 2412) ;
	sprintf(filename, "%s_", modeName[mode]);
	sprintf(parseFilename, "%s_", modeName[mode]);

	minAtten = MAX_TRANSMIT_POWER - MAX_RECEIVE_POWER;
	maxCcaPower = MIN_TRANSMIT_POWER - MAX_RECEIVE_ROLLOFF - minAtten ;
	// maxccapwr_int = (A_INT32) maxCcaPower;

	if (mode == MODE_11a)
	{
		pmAtten = CalSetup.attenDutPM ;
		fixedAtten = CalSetup.attenDutGolden;
	} else
	{
		pmAtten = CalSetup.attenDutPM_2p4[mode] ;
		fixedAtten = CalSetup.attenDutGolden_2p4[mode] ;
	}

	switch (opcode) {
	case GET_GU_PWR :
		strcat(parseFilename, COEFF_GU_FILENAME);
		parseCoeffTable(parseFilename, debug);
		strcat(filename, LOG_GU_FILENAME);
		logGU = fopen(filename, "w");
		if (logGU == NULL) 
			uiPrintf("Unable to open file for logging : %s\n", filename);
		filetype = LOG_FILETYPE;
		break ;
	case CAL_PM_PWR :
		strcat(filename, CAL_PM_FILENAME);
		logGU = fopen(filename, "w");
		if (logGU == NULL) 
			uiPrintf("Unable to open file for logging : %s\n", filename);
		filetype = REF_FILETYPE;

		if (CalSetup.pmModel != PM_E4416A)
		{
			uiPrintf("fast cal is only supported with Agilent E4416A model power meters at this point.\n");
			exit(0);
		}
	
		gpibClear(guDevPM);
		gpibWrite(guDevPM, "*rcl 7\n");
		Sleep(3000);
		gpibWrite(guDevPM, "*cls;:sens:mrat doub\n");
		break ;
	case CAL_GU_PWR :
		strcat(filename, CAL_GU_FILENAME);
		logGU = fopen(filename, "w");
		if (logGU == NULL) 
			uiPrintf("Unable to open file for logging : %s\n", filename);
		filetype = RAW_FILETYPE;
		break;
	default :
		uiPrintf("Illegal opcode in setupGUForPwrMeas : %d\n", opcode);
		exit(0);
		break;
	}

	fprintf(logGU, "%f\n", FAST_CAL_FILES_VERSION);
	fprintf(logGU, "%d\n", filetype);

	uiPrintf("Verify the Following Constraints ...\n");
	uiPrintf("\tReceive input ( gu ) < %3.1f dBm\n", MAX_RECEIVE_POWER);
	uiPrintf("\tTransmit power (dut) < %3.1f dBm\n", MAX_TRANSMIT_POWER);
	uiPrintf("\tTransmit power (dut) > %3.1f dBm\n", MIN_TRANSMIT_POWER);
	atten = (A_UINT32) floor( minAtten - fixedAtten );

	if (atten > MAX_ATTENUATOR_RANGE)
	{
		uiPrintf("required attenuation (%d) > max attenutor range (%d)\n", atten, MAX_ATTENUATOR_RANGE);
		exit(0);
	} else
	{
		attSet(guDevAtt, atten);
		uiPrintf("\tMinimum Attenuation Needed: %3.1f, Prog Attenuator Set to : %d\n", minAtten, atten);
	}


	uiPrintf("\tReceive rolloff < %3.1f dBm\n", MAX_RECEIVE_ROLLOFF);
	uiPrintf("\tMin cca power   > %3.1f dBm\n", maxCcaPower);
	uiPrintf("\tDut to PM atten = %3.1f dBm\n", pmAtten);
	uiPrintf("\tPeak TX power   > %3.1f dBm\n", MIN_TRANSMIT_POWER + MAX_PEAK_TO_AVERAGE);
	uiPrintf("\tPM trig level   < %3.1f dBm\n\n", MIN_TRANSMIT_POWER + MAX_PEAK_TO_AVERAGE - pmAtten);
// SNOOP: eventually set this trig level automatically.

	// disable noise cal
	art_resetDevice(devNum, rxStation, bssID, ch, 0);

	// select rssi output format
	rddata = art_regRead(devNum, (0x9800 + (23<<2)));
	wrdata = ( rddata & ~(3<<30) )| (RSSI_OUT_SELECT<<30) ;
	art_regWrite(devNum, (0x9800 + (23<<2)), wrdata);
	uiPrintf("setting rssi format to %3.2f dB resolution\n", 1.0/pow(2,RSSI_OUT_SELECT) );


//	Sleep(50);

  
	Sleep(50);
	rddata = art_regRead(devNum, (0x9800 + (24<<2)));
	wrdata = rddata & ~0x8002;
	art_regWrite(devNum, (0x9800 + (24<<2)), wrdata);

	// force noise floor

	rddata = art_regRead(devNum, 0x9800+(25<<2));
// for read-modify-write
//	wrdata = (rddata & ~(0x1ff)) | (( (maxCcaPower<0) ? (abs((A_UINT32)(maxCcaPower*2)) ^ 0x1ff)+1 : (A_UINT32)(maxCcaPower*2)));
	wrdata =  (( (maxCcaPower<0) ? (abs((A_UINT32)(maxCcaPower*2)) ^ 0x1ff)+1 : (A_UINT32)(maxCcaPower*2)));
    art_regWrite(devNum, 0x9800+(25<<2), wrdata);

//	uiPrintf("rddata = %x, wrdata = %x\n", rddata, wrdata);

	rddata = art_regRead(devNum, (0x9800 + (24<<2)));
	wrdata = rddata | 0x0002;
	art_regWrite(devNum, (0x9800 + (24<<2)), wrdata);

	//	uiPrintf("forceval=%x\n", wrdata);

 	rddata = 1;
	while ( rddata != 0)
	{
		rddata = ( art_regRead(devNum, (0x9800 + (24<<2))) & 0x2); 
		Sleep(10);
		uiPrintf("SNOOP: (%d) rddata = %x\n", kk++, rddata); 
	}

	rddata = art_regRead(devNum, (0x9800+(25<<2)));
    minccapwr = (rddata >> 19) & 0x1ff;
    if (minccapwr & 0x100) { 
        minccapwr = -((minccapwr ^ 0x1ff) + 1);
    }
    uiPrintf( "Minimum CCA power was forced to %d dBm\n\n", minccapwr);
 
	if (debug)
		uiPrintf("finished setupGUForPwrMeas with opcode %d\n", opcode);
} // setupGUForPwrMeas

void sendPwrData(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 channel, A_UINT32 mode, 
						A_UINT16 *pcdacs, A_UINT32 numPcd, double *retVals, 
						A_UINT32 debug)
{
	A_UINT32 ii, pcdIter, numAvg;
	A_UCHAR  datapattern[1] = {0x00};
	A_UINT32 broadcast = 1;
	A_UINT32 start_timeout = 1000; //1000;
//	A_UINT32 complete_timeout_short = (mode == MODE_11b) ? 50: 20; // for one pcdac
	A_UINT32 complete_timeout_short = 500; // for one pcdac
	A_UINT32 complete_timeout_long = 3000; // for all pcdacs
	A_UINT32 statsMode = SKIP_SOME_STATS | (NUM_INITIAL_PKTS_TO_SKIP << NUM_TO_SKIP_S); 
	//A_UINT32 statsMode = NO_REMOTE_STATS;
	A_UINT32 enablePPM = 0;
	double   sum[64], avg[64];
	double	 pwr, corr;
	A_UINT32 cnt[64];
	RX_STATS_STRUCT rStats;
	A_INT32 rssi;
	char cmd[1024];
	A_UCHAR		spr; // serial poll register


	double   pmAtten, fixedAtten;

	if (mode == MODE_11a)
	{
		pmAtten = CalSetup.attenDutPM ;
		fixedAtten = CalSetup.attenDutGolden;
	} else
	{
		pmAtten = CalSetup.attenDutPM_2p4[mode] ;
		fixedAtten = CalSetup.attenDutGolden_2p4[mode] ;
	}

	numAvg = (opcode == CAL_PM_PWR) ? 1 : NUM_POWER_AVERAGES ;

	for (ii=0; ii<64; ii++)
	{
		sum[ii] = 0.0;
		avg[ii] = 0.0;
		cnt[ii]=0;
	}

	if (opcode == CAL_PM_PWR)
	{
		Sleep(1000);
		sprintf(cmd, "freq %dmhz\n", channel);
		gpibWrite(guDevPM, cmd);
		Sleep(1000);
	}

	for (ii=0; ii<numAvg; ii++)
	{
		if (opcode != CAL_PM_PWR)
		{
			// wait for transmitter
			waitForAck(debug);
			if (!verifyAckStr("Prepare to begin receive for power meas at "))
			{
				uiPrintf("acks out of sync: should sync for fastcal at ch: %d, avg_iter: %d, opcode: %d\n", channel, ii, opcode);
				uiPrintf("ack parameters found to be - par1: %d, par2: %d, par3: %d\n", ackRecvPar1, ackRecvPar2, ackRecvPar3);
				exit(0);
			}

			sendAck(devNum, "Ready to begin receive for power meas at ",  channel, ii, opcode, debug);
		}

		for (pcdIter=0; pcdIter < numPcd; pcdIter++)
		{
			if (opcode == CAL_PM_PWR)
			{
				// arm power meter
				Sleep(E4416_SLEEP_INTERVAL);
				gpibWrite(guDevPM, "init\n");
				Sleep(E4416_SLEEP_INTERVAL);

				// wait for transmitter
				waitForAck(debug);
				if (!verifyAckStr("Prepare for RX at pcdac = ") || (RAW_PCDACS[pcdIter] != ackRecvPar2))
				{
					uiPrintf("acks got out of sync in sendPwrData\n");
					exit(0);
				}

				art_rxDataSetup(devNum, NUM_POWER_PACKETS + 40, LEN_POWER_PACKETS, enablePPM);
				sendAck(devNum, "Ready for RX at pcdac = ", channel, RAW_PCDACS[pcdIter], opcode, debug);
				art_rxDataBegin(devNum, start_timeout, complete_timeout_short, statsMode, 0, datapattern, 1);					
		
				// measure power
				gpibWrite(guDevPM, "fetch?\n");
				while(!(gpibRSP(guDevPM, (A_CHAR *)&(spr)) & 16))
				{
					Sleep(E4416_SLEEP_INTERVAL);
					break;
				}

				Sleep(E4416_SLEEP_INTERVAL);
				sum[pcdIter] += atof(gpibRead(guDevPM, 17)) + pmAtten ;
				uiPrintf("Pcdac %d: %f dBm (ch: %d)\n", RAW_PCDACS[pcdIter], sum[pcdIter], channel);
				(cnt[pcdIter])++ ;
			} else 
			{

				// wait for transmitter
				waitForAck(debug);
				if (!verifyAckStr("Prepare for RX at pcdac = ") || (RAW_PCDACS[pcdIter] != ackRecvPar2))
				{
					uiPrintf("acks got out of sync in sendPwrData\n");
					exit(0);
				}

				art_rxDataSetup(devNum, NUM_POWER_PACKETS + 40, LEN_POWER_PACKETS, enablePPM);
				sendAck(devNum, "Ready for RX at pcdac = ", channel, RAW_PCDACS[pcdIter], opcode, debug);
				art_rxDataBegin(devNum, start_timeout, complete_timeout_short, statsMode, 0, datapattern, 1);					

				// stats
				art_rxGetStats(devNum, 6, 0, &rStats);
				rssi = rStats.DataSigStrengthAvg ;
				if (rssi != 0)
				{
					cnt[pcdIter]++ ;
				} else
				{
					uiPrintf("SNOOP: rssi=%d, not incrementing count\n", rssi);
				}
				if (rssi & 0x80)
					rssi = -((rssi^0xff) + 1);
				sum[pcdIter] += rssi/pow(2, RSSI_OUT_SELECT) ;				
			}

		} // for (0..numPcd)
	
	} // for (0..numAvg)


	if (opcode == GET_GU_PWR)
	{
		for (pcdIter=0; pcdIter<numPcd; pcdIter++)
		{
			pwr = sum[pcdIter]/cnt[pcdIter];
			corr = getCorrVal(channel, rssi, debug);
			avg[pcdIter] = pwr - corr;
		}
		writeDoubleListToStr(cmd, channel, &(avg[0]), numPcd);
		fprintf(logGU, cmd);
		fflush(logGU);
		waitForAck(debug);
		if (!verifyAckStr("Send Power List"))
		{
			uiPrintf("acks out of sync. expected req: %s\n", "Send Power List");
			exit(0);
		}
		sendAck(devNum, cmd, numPcd, channel, numAvg, debug);
	} else
	{
		for (pcdIter=0; pcdIter<numPcd; pcdIter++)
		{
			avg[pcdIter] = sum[pcdIter]/cnt[pcdIter];
		}
		writeDoubleListToStr(cmd, channel, &(avg[0]), numPcd);
		fprintf(logGU, cmd);
	}

	uiPrintf("done with channel: %d\n", channel);

} // sendPwrData

void parseCoeffTable(char *filename, A_UINT32 debug)
{
	FILE		*fp;
	char		linebuf[1024];
	A_BOOL		line1 = FALSE; // version #
	A_BOOL		line2 = FALSE; // fileType: 0->coeffs, 1->ref, 2->raw
	A_BOOL		line3 = FALSE; // numChannels
	A_BOOL		line4 = FALSE; // numCoeffs
	A_UINT32	dummy[NUM_COEFFS];
	A_UINT32	filetype;
	double		version;
	A_UINT16	numCh, ii, jj;
	A_UINT32	numCoeffs, channel;



    uiPrintf("\nReading from file %s\n", filename);
    if( (fp = fopen( filename, "r")) == NULL ) {
        uiPrintf("Failed to open %s to read golden coeffs\n", filename);
		exit(0);        
    }

	ii=0;
	
    while(fgets(linebuf, 1020, fp) != NULL) 
	{
		if (!line1)
		{
			if (!sscanf(linebuf, "%lf", &(version)))
			{
				uiPrintf("Could not read version from file %s, line: %s\n", filename, linebuf);
				exit(0);
			} else
			{
				line1 = TRUE;
				if (debug)
					uiPrintf("Coeff file version: %f\n", version);
				continue;
			}
		}

		if (!line2)
		{
			if (!sscanf(linebuf, "%d", &(filetype)))
			{
				uiPrintf("Could not read the file type from file %s, line: %s\n", filename, linebuf);
				exit(0);
			} else
			{
				line2 = TRUE;
				if (filetype != COEFF_FILETYPE)
				{
					uiPrintf("File %s is not a valid golden coeff file.\n", filename);
					exit(0);
				}
				if (debug)
					uiPrintf("Filetype is : %d\n", filetype);
				continue;
			}
		}

		if (!line3)
		{
			if (!sscanf(linebuf, "%d", &(numCh)))
			{
				uiPrintf("Could not read number of channels from file %s, line: %s\n", filename, linebuf);
				exit(0);
			} else
			{
				line3 = TRUE;
				if (debug)
					uiPrintf("Number of channels : %d\n", numCh);
				continue;
			}
		}

		if (!line4)
		{
			extractIntsFromStr(linebuf, dummy, NUM_COEFFS, &(numCoeffs), debug);
			line4 = TRUE;
			if (numCoeffs != NUM_COEFFS)
			{
				uiPrintf("Number of coeffs read (%d) form file (%s) is different from expected (%d)\n", numCoeffs, filename, NUM_COEFFS);
				exit(0);
			}

			if (debug)
				uiPrintf("Number of coeffs : %d\n", numCoeffs);
			break;
		}
	} // done reading first 4 lines

	pGoldenCoeffs->numChannels = numCh;
	pGoldenCoeffs->numCoeffs = (A_UINT16) numCoeffs;

	pGoldenCoeffs->pChannels = (A_UINT16 *)malloc(sizeof(A_UINT16) * numCh);
	if (NULL == pGoldenCoeffs->pChannels) {
		uiPrintf("unable to allocate golden coeffs channels\n");
		exit(0);
	}

	pGoldenCoeffs->pDataPerChannel = (COEFF_DATA_PER_CHANNEL *)malloc(numCh*sizeof(COEFF_DATA_PER_CHANNEL)) ;
	if (NULL == pGoldenCoeffs->pDataPerChannel)
	{
		uiPrintf("unable to allocate golden coeffs datastruct\n");
		A_FREE(pGoldenCoeffs->pChannels);
		exit(0);
	}
	
	for (ii=0; ii<pGoldenCoeffs->numChannels; ii++)
	{
		pGoldenCoeffs->pDataPerChannel[ii].pCoeff = (double *)malloc(pGoldenCoeffs->numCoeffs*sizeof(double)) ;
		if(NULL == pGoldenCoeffs->pDataPerChannel[ii].pCoeff)
		{
			uiPrintf("unable to allocate coeffs for channel # %d\n", ii);
			break; // will clean up outside the loop
		}
	}

	if (ii != pGoldenCoeffs->numChannels) // allocation loop must've failed
	{
		for (jj=0; jj<ii; jj++)
		{
			if (pGoldenCoeffs->pDataPerChannel[jj].pCoeff != NULL)
				A_FREE(pGoldenCoeffs->pDataPerChannel[jj].pCoeff);
		}
		A_FREE(pGoldenCoeffs->pDataPerChannel);
		A_FREE(pGoldenCoeffs->pChannels);
		uiPrintf("Unable to allocate Golden Coeffs struct. freeing everything allocated so far.\n");
		exit(0);
	}

	ii = 0;
    while(fgets(linebuf, 1020, fp) != NULL) 
	{	
		extractDoublesFromScientificStr(linebuf, pGoldenCoeffs->pDataPerChannel[ii].pCoeff, pGoldenCoeffs->numCoeffs, &(channel), debug);
		pGoldenCoeffs->pChannels[ii] = (A_UINT16) channel; 
		pGoldenCoeffs->pDataPerChannel[ii].channelValue = (A_UINT16) channel;
		ii++;
	}

	if (ii != pGoldenCoeffs->numChannels)
	{
		uiPrintf("actual # channels (%d) differes from declared # channels (%d)\n", ii, pGoldenCoeffs->numChannels);
		exit(0);
	}

	if (debug)
			uiPrintf("SNOOP: done parsing coeffs: \n");

	if (fp) fclose(fp);

} // parseCoeffTable

void writeIntListToStr(char *deststr, A_UINT32 par1, A_UINT16 *list, A_UINT32 listsize)
{
	char tmpstr[1024];
	A_UINT32 ii;

	strcpy(deststr, "");
	for (ii=0; ii < listsize; ii++)
	{
		sprintf(tmpstr, "%d\t", list[ii]);
		strcat(deststr, tmpstr);
	}
	strcpy(tmpstr, deststr);
	sprintf(deststr, "%d\t%s\n", par1, tmpstr);
} // writeIntListToStr

void writeDoubleListToStr(char *deststr, A_UINT32 par1, double *list, A_UINT32 listsize)
{
	char tmpstr[1024];
	A_UINT32 ii;

	strcpy(deststr, "");

	for (ii=0; ii<listsize; ii++)
	{
		sprintf(tmpstr, "%4.2f\t", list[ii]);
		strcat(deststr, tmpstr);
	}
	strcpy(tmpstr, deststr);
	sprintf(deststr, "%d\t%s\n", par1, tmpstr);
} // writeDoubleListToStr


double getCorrVal(A_UINT32 channel, double rssi, A_UINT32 debug)
{
	A_UINT16 chIdxL, chIdxR, ch_L, ch_R, ii;
	double	 corr_L = 0.0, corr_R = 0.0;

	iGetLowerUpperValues((A_UINT16) channel, pGoldenCoeffs->pChannels, pGoldenCoeffs->numChannels, &ch_L, &ch_R);

	if (debug)
		uiPrintf("chL=%d, chR=%d\n", ch_L, ch_R);

	chIdxL = 0;
	while (chIdxL < pGoldenCoeffs->numChannels)
	{
		if(ch_L == pGoldenCoeffs->pDataPerChannel[chIdxL].channelValue)
			break;
		chIdxL++;
	}

	if(chIdxL == pGoldenCoeffs->numChannels)
	{
		uiPrintf("desired channel %d is outside the range covered by golden coeffs: %d - %d MHz\n", channel,
			pGoldenCoeffs->pChannels[0], pGoldenCoeffs->pChannels[pGoldenCoeffs->numChannels - 1]);
		exit(0);
	}

	if (ch_R != ch_L) 
	{
		if (ch_R != pGoldenCoeffs->pDataPerChannel[chIdxL+1].channelValue)
		{
			uiPrintf("interpolation failed : chL, chR (%d, %d), golden channels: %d, %d at idx %d\n",ch_L, ch_R,
				pGoldenCoeffs->pChannels[chIdxL], pGoldenCoeffs->pChannels[chIdxL+1], chIdxL);
			exit(0);
		}
		chIdxR = chIdxL + 1;
	} else
	{
		chIdxR = chIdxL;
	}
	

	for(ii=0; ii<pGoldenCoeffs->numCoeffs; ii++)
	{
		corr_L += pGoldenCoeffs->pDataPerChannel[chIdxL].pCoeff[ii] * pow(rssi, ii);
		corr_R += pGoldenCoeffs->pDataPerChannel[chIdxR].pCoeff[ii] * pow(rssi, ii);
	}

	if (ch_L == ch_R)
	{
		return(corr_L);
	} else
	{
		return(dGetInterpolatedValue(channel, ch_L, ch_R, corr_L, corr_R));
	}
} // getCorrVal
	
void openRawRefLogFile(char *filename, A_UINT32 expectedType, A_UINT32 *numCh, A_UINT32 *numDataPoints, A_UINT32 debug)
{
	FILE		*fp;
	char		linebuf[1024];
	A_BOOL		line1 = FALSE; // version #
	A_BOOL		line2 = FALSE; // fileType: 0->coeffs, 1->ref, 2->raw
	A_BOOL		line3 = FALSE; // numChannels
	A_BOOL		line4 = FALSE; // numCoeffs
	A_UINT32	dummy[NUM_COEFFS];
	A_UINT32	filetype;
	double		version;
	A_UINT16	ii;



    uiPrintf("\nReading from file %s\n", filename);
    if( (fp = fopen( filename, "r")) == NULL ) {
        uiPrintf("Failed to open %s to read raw/ref data\n", filename);
		exit(0);        
    }

	ii=0;
	
    while(fgets(linebuf, 1020, fp) != NULL) 
	{
		if (!line1)
		{
			if (!sscanf(linebuf, "%lf", &(version)))
			{
				uiPrintf("Could not read version from file %s, line: %s\n", filename, linebuf);
				exit(0);
			} else
			{
				line1 = TRUE;
				if (debug)
					uiPrintf("Coeff file version: %f\n", version);
				continue;
			}
		}

		if (!line2)
		{
			if (!sscanf(linebuf, "%d", &(filetype)))
			{
				uiPrintf("Could not read the file type from file %s, line: %s\n", filename, linebuf);
				exit(0);
			} else
			{
				line2 = TRUE;
				if (filetype != expectedType)
				{
					uiPrintf("File %s is type (%d), not of the expected type (%d).\n", filename, filetype, expectedType);
					exit(0);
				}
				if (debug)
					uiPrintf("Filetype is : %d\n", filetype);
				continue;
			}
		}

		if (!line3)
		{
			if (!sscanf(linebuf, "%d", numCh))
			{
				uiPrintf("Could not read number of channels from file %s, line: %s\n", filename, linebuf);
				exit(0);
			} else
			{
				line3 = TRUE;
				if (debug)
					uiPrintf("Number of channels : %d\n", *numCh);
				continue;
			}
		}

		if (!line4)
		{
			extractIntsFromStr(linebuf, dummy, sizeOfRawPcdacs, numDataPoints, debug);
			line4 = TRUE;
			if (*numDataPoints != sizeOfRawPcdacs)
			{
				uiPrintf("Number of datapoints read (%d) form file (%s) is different from expected (%d)\n", *numDataPoints, filename, sizeOfRawPcdacs);
				exit(0);
			}
			if (debug)
				uiPrintf("Number of datapoints : %d\n", *numDataPoints);
			break;
		}
	} // done reading first 4 lines

	if (filetype == REF_FILETYPE)
	{
		refGU = fp;
	} else
	{
		rawGU = fp;
	}

	if (fp) fclose(fp);

} // openRawRefLogFile

	
void extractGoldenCalCoeffs(A_UINT32 mode, A_UINT32 debug)
{
	FILE		*logCal;
	A_UINT32	ii, jj, kk;
	A_UINT32	channel1, channel2;
	char		linebuf1[1024], linebuf2[1024];
	char		filenameRef[64], filenameRaw[64], filenameCoeff[64];
	float		*xdata, *ydata;
	float		coeffs[NUM_COEFFS] ;

	A_UINT32 numDataPoints1, numDataPoints2, numChannels1, numChannels2;

	sprintf(filenameRaw, "%s_%s", modeName[mode], CAL_GU_FILENAME);
	sprintf(filenameRef, "%s_%s", modeName[mode], CAL_PM_FILENAME);
	sprintf(filenameCoeff, "%s_%s", modeName[mode], COEFF_GU_FILENAME);

	openRawRefLogFile(filenameRef, REF_FILETYPE,  &(numChannels1), &(numDataPoints1), debug);
	openRawRefLogFile(filenameRaw, RAW_FILETYPE,  &(numChannels2), &(numDataPoints2), debug);

	if ( (numChannels1 != numChannels2) || (numDataPoints1 != numDataPoints2) )
	{
		uiPrintf("the raw and ref files do not match.\n");
		exit(0);
	}

	xdata = (float *) malloc(numDataPoints1*sizeof(float));
	ydata = (float *) malloc(numDataPoints1*sizeof(float));

	if ( (xdata == NULL) || (ydata == NULL) )
	{
		uiPrintf("Could not allocated for xdata, ydata.\n");
		exit(0);
	}

	uiPrintf("\nLogging Golden Coefficients in File %s\n", filenameCoeff);
    if( (logCal = fopen( filenameCoeff, "w")) == NULL ) {
        uiPrintf("Failed to open %s to write golden cal coeffs\n", filenameCoeff);
		exit(0);        
    }
	fprintf(logCal, "%f\n", FAST_CAL_FILES_VERSION);
	fprintf(logCal, "%d\n", COEFF_FILETYPE);
	fprintf(logCal, "%d\n", numChannels1);
	fprintf(logCal, "%d\t", NUM_COEFFS);
	for (ii=0; ii<NUM_COEFFS; ii++)
		fprintf(logCal, "%d\t", ii);
	fprintf(logCal, "\n");
	fflush(logCal);

	uiPrintf("Curve fit results ...\n");
	ii = 0;

    while(fgets(linebuf1, 1020, refGU) != NULL) 
	{
		if (fgets(linebuf2, 1020, rawGU) == NULL) 
		{
			uiPrintf("Problem reading the raw data file for ch %d.\n", ii);
			exit(0);
		}

		if (debug)
		{
			uiPrintf("line_ref: %s\nline_raw: %s\n", linebuf1, linebuf2);
		}

//		extractDoublesFromStr(linebuf1, ydata, numDataPoints1, &(channel1), debug);
//		extractDoublesFromStr(linebuf2, xdata, numDataPoints1, &(channel2), debug);
		extractFloatsFromStr(linebuf1, ydata, numDataPoints1, &(channel1), debug);
		extractFloatsFromStr(linebuf2, xdata, numDataPoints1, &(channel2), debug);

		if (debug) uiPrintf("ch1=%d, ch2=%d\n", channel1, channel2);

		if (channel1 != channel2)
		{
			uiPrintf("Sequence of channels is different between ref (%d) and raw (%d) files.\n", channel1, channel2);
			exit(0);
		}

		for (jj=0; jj<numDataPoints1; jj++)
		{
			ydata[jj] = xdata[jj] - ydata[jj];
			//uiPrintf("x=%f, y=%f\n", xdata[jj], ydata[jj]);
		}

		curveFit(xdata, ydata, numDataPoints1, NUM_COEFFS, coeffs);
		
		fprintf(logCal, "%d\t", channel1);
		uiPrintf("%d\t", channel1);
		for (kk=0; kk<NUM_COEFFS; kk++)
		{
			//fprintf(logCal, "%lf\t", coeffs[kk]);
			fprintf(logCal, "%e\t", coeffs[kk]);
			uiPrintf("%e\t", coeffs[kk]);
		}
		fprintf(logCal, "\n");
		uiPrintf("\n");

		ii++;
	}

	if (ii > numChannels1)
	{
		uiPrintf("data found for more channels than declared. Pl. fix the ref/raw files.\n");
		exit(0);
	}

	fclose(logCal);

} // extractGoldenCalCoeffs


void calibrateGoldenForFastCal_GU(A_UINT32 devNum, A_UINT32 mode, A_UINT32 debug)
{


	waitForSync(debug);
	sendAck(devNum, "Ready for ref data collection", mode, CAL_PM_PWR, 0, debug);
	sendCalDataFromGU(devNum, mode, CAL_PM_PWR, debug);

	waitForAck(debug);
	sendAck(devNum, "Ready for raw data collection", mode, CAL_GU_PWR, 0, debug);
	sendCalDataFromGU(devNum, mode, CAL_GU_PWR, debug);

	extractGoldenCalCoeffs(mode, debug);

	closeComms();

} // calibrateGoldenForFastCal_GU

void calibrateGoldenForFastCal_DUT(A_UINT32 devNum, A_UINT32 mode, A_UINT32 debug)
{

//	sendAck(devNum, "Prepare for ref data collection", mode, CAL_PM_PWR, 0, debug);

	sendSync(devNum, CalSetup.goldenIPAddr, debug);
	waitForAck(debug);
	Sleep(500);
	getCalDataUsingGU(devNum, mode, CAL_PM_PWR, debug);

	sendAck(devNum, "Prepare for raw data collection", mode, CAL_GU_PWR, 0, debug);
	waitForAck(debug);
	Sleep(500);
	getCalDataUsingGU(devNum, mode, CAL_GU_PWR, debug);

	closeComms();

} // calibrateGoldenForFastCal_DUT


void fastCalMenu_GU(A_UINT32 devNum) 
{
    A_BOOL exitLoop = FALSE;

//	gpibSendIFC(0); // clashes with attenuator !!?!
//	gpibSIC(0); // not necessary
//	gpibRSC(0,1); // request system controller (1)

	uiPrintf("\nSetting up Attenuator");
	guDevAtt = attInit(CalSetup.attGPIBaddr, ATT_11713A); 
//	gpibClear(guDevAtt); // not necessary
//	gpibONL(guDevAtt, 1); // not necessary
	attSet(guDevAtt, MAX_ATTENUATOR_RANGE);
	
	uiPrintf("\nSetting up Power Meter");
 	guDevPM = pmInit(CalSetup.pmGPIBaddr, PM_E4416A);


    while(exitLoop == FALSE) {
        printf("\n");
        printf("=============================================\n");
        printf("| Fastcal for mode on GU:(select before DUT)|\n");
        printf("|   a - GU Fastcal Cal for 802.11(a)        |\n");
        printf("|   b - GU Fastcal Cal for 802.11(b)        |\n");
        printf("|   g - GU Fastcal Cal for 802.11(g)        |\n");
        printf("|   q - (Q)uit                              |\n");
        printf("=============================================\n");
        switch(toupper(getch())) {
        case 'A':
			calibrateGoldenForFastCal_GU(devNum, MODE_11a, CalSetup.customerDebug);
			break;
        case 'B':
			calibrateGoldenForFastCal_GU(devNum, MODE_11b, CalSetup.customerDebug);
			break;
        case 'G':
			calibrateGoldenForFastCal_GU(devNum, MODE_11g, CalSetup.customerDebug);
			break;
        case 0x1b:
        case 'Q':
            exitLoop = TRUE;
            uiPrintf("exiting\n");
            break;
        default:
            uiPrintf("Unknown command\n");
            break;
        }
    }
} // fastCalMenu_GU

void fastCalMenu_DUT(A_UINT32 devNum) 
{
    A_BOOL exitLoop = FALSE;


    while(exitLoop == FALSE) {
        printf("\n");
        printf("=============================================\n");
        printf("| Fastcal for mode on DUT:(first select GU) |\n");
        printf("|   a - DUT Fastcal Cal for 802.11(a)       |\n");
        printf("|   b - DUT Fastcal Cal for 802.11(b)       |\n");
        printf("|   g - DUT Fastcal Cal for 802.11(g)       |\n");
        printf("|   q - (Q)uit                              |\n");
        printf("=============================================\n");
        switch(toupper(getch())) {
        case 'A':
			calibrateGoldenForFastCal_DUT(devNum, MODE_11a, CalSetup.customerDebug);
			break;
        case 'B':
			calibrateGoldenForFastCal_DUT(devNum, MODE_11b, CalSetup.customerDebug);
			break;
        case 'G':
			calibrateGoldenForFastCal_DUT(devNum, MODE_11g, CalSetup.customerDebug);
			break;
		case 0x1b:
        case 'Q':
            exitLoop = TRUE;
            uiPrintf("exiting\n");
            break;
        default:
            uiPrintf("Unknown command\n");
            break;
        }
    }
} // fastCalMenu_DUT
