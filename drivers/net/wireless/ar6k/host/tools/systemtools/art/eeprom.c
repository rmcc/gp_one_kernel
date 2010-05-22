/* eeprom.c - Contains the functions for printing the eeprom contents */

/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */
#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/eeprom.c#3 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/eeprom.c#3 $"

#ifdef __ATH_DJGPPDOS__
 #define __int64	long long
 #define HANDLE long
 typedef unsigned long DWORD;
 #define Sleep	delay
 #include <bios.h>
#endif	// #ifdef __ATH_DJGPPDOS__

#ifdef _WINDOWS
 #include <windows.h>
#endif

#ifdef JUNGO
#include "mld.h"       /* Low level driver information for MLD */
#endif
#include "common_hw.h"

#include <stdio.h>
 
#ifndef LINUX
#include <conio.h>
#else
#include "linux_ansi.h"
#endif
#include <string.h>
#include <stdlib.h>
#include "wlantype.h"
#include "mEeprom.h"
#include "mConfig.h"     
#include "art_if.h"
#include "test.h"
#ifdef __ATH_DJGPPDOS__
#include "mlibif_dos.h"
#endif

A_UINT16  xpdGainMapping[] = {0, 6, 9, 18};
A_UINT16  xpdGainMapping_gen5[] = {0, 1, 2, 4};
extern void printAR6000Eeprom(A_UINT32 devNum);

/* NOT_USED: in Dragon/Mercury */
void printHeaderInfo_16K
(
 MDK_EEP_HEADER_INFO	*pHeaderInfo,
 A_UINT32			mode
)
{
#if defined(AR6001) || defined(AR6002)
#else
	A_CHAR binaryString[50];
	A_CHAR modeString[10];
	A_UINT16 i, j;
	MODE_HEADER_INFO	*pModeInfo;
	A_INT16  tmpVal;

	switch(mode) {
	case MODE_11A:
		pModeInfo = &(pHeaderInfo->info11a);
		sprintf(modeString, "11a");
		break;

	case MODE_11G:
		pModeInfo = &(pHeaderInfo->info11g);
		sprintf(modeString, "11g");
		break;

	case MODE_11O:
		pModeInfo = &(pHeaderInfo->info11g);
		sprintf(modeString, "11o");
		break;

	case MODE_11B:
		pModeInfo = &(pHeaderInfo->info11b);
		sprintf(modeString, "11b");
		break;

	default:

		printf("Illegal mode passed to printHeaderInfo_16K\n");
		return;
	} //end switch

	uiPrintf("\n");
	uiPrintf(" =================Header Information for mode %s===============\n", modeString);

	uiPrintf(" |  Major Version           %2d  ", pHeaderInfo->majorVersion);
	uiPrintf("|  Minor Version           %2d  |\n", pHeaderInfo->minorVersion);
	if(pHeaderInfo->majorVersion >= 4) {
		uiPrintf(" |  EAR Start            0x%3x  ", pHeaderInfo->earStartLocation);
		uiPrintf("|  Target Power Start   0x%3x  |\n", pHeaderInfo->trgtPowerStartLocation);
		uiPrintf(" |  EEP MAP                0x%1x  ", pHeaderInfo->eepMap);
		uiPrintf("|                              |\n");
   		uiPrintf(" |  Enable 32 khz            %1d  ", pHeaderInfo->enable32khz);

		if(((pHeaderInfo->majorVersion  == 4) && (pHeaderInfo->minorVersion >= 5)) || 
		(pHeaderInfo->majorVersion >= 5)){
			uiPrintf("|  Old Enable 32 khz        %1d  |\n", pHeaderInfo->oldEnable32khz);
			uiPrintf(" |  Mask for Radio 0     %1d ", pHeaderInfo->maskRadio0);
			switch (pHeaderInfo->maskRadio0) {
			case 0:
				uiPrintf("   ");
				break;
			case 1:
				uiPrintf("  g");
				break;
			case 2:
				uiPrintf("  a");
				break;
			case 3:
				uiPrintf("a/g");
				break;
			default:
				uiPrintf("   ");
				break;
			}
			uiPrintf("  |  Mask for Radio 1        %1d",pHeaderInfo->maskRadio1);
			switch (pHeaderInfo->maskRadio1) {
			case 0:
				uiPrintf("   ");
				break;
			case 1:
				uiPrintf("  g");
				break;
			case 2:
				uiPrintf("  a");
				break;
			case 3:
				uiPrintf("a/g");
				break;
			default:
				uiPrintf("   ");
				break;
			}
			uiPrintf("|\n");
		} else {
			uiPrintf("|                              |\n");
		}

		if(((pHeaderInfo->majorVersion  == 4) && (pHeaderInfo->minorVersion >= 4)) || 
		(pHeaderInfo->majorVersion >= 5)){
			uiPrintf(" |  EEP File Version       %3d  ", pHeaderInfo->eepFileVersion);
			uiPrintf("|  ART Build Number       %3d  |\n", pHeaderInfo->artBuildNumber);
			uiPrintf(" |  EAR File Identifier    %3d  ", pHeaderInfo->earFileIdentifier);
			uiPrintf("|  EAR File Version       %3d  |\n", pHeaderInfo->earFileVersion);
		}
		if((pHeaderInfo->majorVersion  >= 5) && (pHeaderInfo->minorVersion >= 1))
		{
			uiPrintf(" |-------------------------------------------------------------|\n");

			uiPrintf(" |  calStartLocation     0x%3x  ",pHeaderInfo->calStartLocation);
			uiPrintf("|  keyCacheSize           %3d  |\n", pHeaderInfo->keyCacheSize);
			
			uiPrintf(" |  enableClip             %3d  ", pHeaderInfo->enableClip);
			uiPrintf("|  maxNumQCU		  %3d  |\n", pHeaderInfo->maxNumQCU);

			uiPrintf(" |  burstingDisable        %3d  ", pHeaderInfo->burstingDisable);
			uiPrintf("|  fastFrameDisable       %3d  |\n",  pHeaderInfo->fastFrameDisable);

			uiPrintf(" |  aesDisable		   %3d  ", pHeaderInfo->aesDisable);
			uiPrintf("|  compressionDisable     %3d  |\n", pHeaderInfo->compressionDisable);
			uiPrintf(" |  xrDisable             %3d  ", pHeaderInfo->disableXR);
			uiPrintf("|                              |\n");	

		}
		if(((pHeaderInfo->majorVersion  == 5) && (pHeaderInfo->minorVersion >= 3))
				|| (pHeaderInfo->majorVersion >= 6)){
			uiPrintf(" |-------------------------------------------------------------|\n");

			uiPrintf(" |  enable FCC Mid         %3d  ", pHeaderInfo->enableFCCMid);
			uiPrintf("|  enable Jap even Uni 1  %3d  |\n",  pHeaderInfo->enableJapanEvenU1);

			uiPrintf(" |  enable Jap Uni 2       %3d  ", pHeaderInfo->enableJapenU2);
			uiPrintf("|  enable Jap Mid         %3d  |\n",  pHeaderInfo->enableJapnMid);

			uiPrintf(" |  disable Jap odd Uni 1  %3d  ", pHeaderInfo->disableJapanOddU1);
			uiPrintf("|  enable Jap 11a new     %3d  |\n", pHeaderInfo->enableJapanMode11aNew);
		}
	}
	uiPrintf(" |-------------------------------------------------------------|\n");

	if(((pHeaderInfo->majorVersion == 3) && (pHeaderInfo->minorVersion >= 1)) 
			|| (pHeaderInfo->majorVersion >= 4)){
		uiPrintf(" |  A Mode         %1d  ", pHeaderInfo->Amode);
		uiPrintf("|  B Mode         %1d  ", pHeaderInfo->Bmode);
		uiPrintf("|  G Mode        %1d  |\n", pHeaderInfo->Gmode);
	}
	else {
		uiPrintf(" |  A Mode                   %1d  ", pHeaderInfo->Amode);
		uiPrintf("|  B Mode                   %1d  |\n", pHeaderInfo->Bmode);
	}

	if(pHeaderInfo->countryCodeFlag) {
		uiPrintf(" |  Country Code %03x  ", pHeaderInfo->countryRegCode);
	} else {
		uiPrintf(" |  Reg. Domain  %03x  ", pHeaderInfo->countryRegCode);
	}

	//uiPrintf("|  turbo Disable  %1d  ", pHeaderInfo->turboDisable);
	uiPrintf("|  turbo Disable  %1d  ", pModeInfo->turboDisable);
	uiPrintf("|  RF Silent     %1d  |\n", pHeaderInfo->RFKill);
	uiPrintf(" |-------------------------------------------------------------|\n");
	if(((pHeaderInfo->majorVersion == 3) && (pHeaderInfo->minorVersion >= 3)) 
			|| (pHeaderInfo->majorVersion >= 4)){
		uiPrintf(" |  worldwide roaming        %1x  ", pHeaderInfo->worldwideRoaming);
		uiPrintf("|  False detect backoff  0x%02x  |\n", pModeInfo->falseDetectBackoff);
	}
	uiPrintf(" |  device type              %1x  ", pHeaderInfo->deviceType);

	uiPrintf("|  Switch Settling Time  0x%02x  |\n", pModeInfo->switchSettling);
	uiPrintf(" |  ADC Desired size       %2d  ", pModeInfo->adcDesiredSize);
	uiPrintf("|  XLNA Gain             0x%02x  |\n", pModeInfo->xlnaGain);

	uiPrintf(" |  tx end to XLNA on     0x%02x  ", pModeInfo->txEndToXLNAOn);
	uiPrintf("|  Threashold 62         0x%02x  |\n", pModeInfo->thresh62);

	uiPrintf(" |  tx end to XPA off     0x%02x  ", pModeInfo->txEndToXPAOff);
	uiPrintf("|  tx end to XPA on      0x%02x  |\n", pModeInfo->txFrameToXPAOn);

	uiPrintf(" |  PGA Desired size       %2d  ", pModeInfo->pgaDesiredSize);
	uiPrintf("|  Noise Threshold        %3d  |\n", pModeInfo->noisefloorThresh);

	uiPrintf(" |  XPD Gain              0x%02x  ", pModeInfo->xgain);
	uiPrintf("|  XPD                      %1d  |\n", pModeInfo->xpd);

	uiPrintf(" |  txrx Attenuation      0x%02x  ", pModeInfo->txrxAtten);
	
	
	uiPrintf("|  Antenna control  0  ");
	itoa(pModeInfo->antennaControl[0], binaryString, 2);
	for(i = 0; i < (6 - strlen(binaryString)); i++) {
		uiPrintf("0");
	}
	uiPrintf("%s  |\n", binaryString);
	for(j = 1; j <= 5; j++) {
		uiPrintf(" |  Antenna control %2d  ", j);

		itoa(pModeInfo->antennaControl[j], binaryString, 2);
		for(i = 0; i < (6 - strlen(binaryString)); i++) {
			uiPrintf("0");
		}
		uiPrintf("%s  ", binaryString);

		uiPrintf("|  Antenna control %2d  ", j+5);
		itoa(pModeInfo->antennaControl[j+5], binaryString, 2);
		for(i = 0; i < (6 - strlen(binaryString)); i++) {
			uiPrintf("0");
		}
		uiPrintf("%s  |\n", binaryString);
	}

	
	if(((pHeaderInfo->majorVersion == 3) && (pHeaderInfo->minorVersion >= 4)) 
			|| (pHeaderInfo->majorVersion >= 4)){
		uiPrintf(" |  Init GainI            0x%02x  ", pModeInfo->initialGainI);
//		if(mode == MODE_11B) {
//			uiPrintf("|                              |\n");
//		}
		if(mode != MODE_11B) {
			uiPrintf("|  Turbo 2W Pwr Max.       %2d  |\n", pModeInfo->turbo.max2wPower);	
		}
		if((mode == MODE_11G) || (mode == MODE_11O)) {
			uiPrintf(" |  OFDM/CCK Delta        %4.1f  ", (float)(pHeaderInfo->scaledOfdmCckDelta)/10);
		}
		
		if (pHeaderInfo->majorVersion == 4) {
			if(mode != MODE_11A) {
	    		uiPrintf("|  Use Fixed dB Bias 2.4GHz %1d  |\n", pHeaderInfo->fixedBiasB);
			}
		} else {
			uiPrintf("|                              |\n");	
		}
	}

	if(pHeaderInfo->majorVersion >= 4) {
		if(mode != MODE_11B) {
			tmpVal = pModeInfo->iqCalI ;
			if ((tmpVal >> 5) == 0x1) tmpVal = tmpVal - 64 ;
			uiPrintf(" |  IQ Cal I               %3d  ", tmpVal);
			tmpVal = pModeInfo->iqCalQ ;
			if ((tmpVal >> 4) == 0x1) tmpVal = tmpVal - 32 ;
			uiPrintf("|  IQ Cal Q               %3d  |\n", tmpVal);
		}
		if(mode == MODE_11A) {
			uiPrintf(" |  Use Fixed dB Bias 5GHz   %1d  ", pHeaderInfo->fixedBiasA);
			if(pHeaderInfo->minorVersion >= 1) {
				uiPrintf("|  rxtxMargin            0x%02x  |\n", pModeInfo->rxtxMargin);
			}
			else {
				uiPrintf("|                              |\n");
			}
		}
		if(((pHeaderInfo->majorVersion == 4) && (pHeaderInfo->minorVersion >= 1)) || 
			(pHeaderInfo->majorVersion >= 5))
		{
			if(mode != MODE_11A) {
				uiPrintf(" |  rxtxMargin            0x%02x  ", pModeInfo->rxtxMargin);				
			}
		}
		if((((pHeaderInfo->majorVersion == 4) && (pHeaderInfo->minorVersion >= 2)) ||
			(pHeaderInfo->majorVersion >= 5))
			&& (mode == MODE_11G)){
			uiPrintf("|  OFDM/CCK Gain Delta   %4.1f  |\n", (float)(pHeaderInfo->ofdmCckGainDeltaX2)/2);
		} else if ((mode == MODE_11B) || (mode == MODE_11O)) {
			uiPrintf("|                              |\n");
		}
		if((((pHeaderInfo->minorVersion >= 5) && (pHeaderInfo->majorVersion == 4)) &&
			(pHeaderInfo->majorVersion >= 5))
			&& ((mode == MODE_11G)||(mode == MODE_11O))){
			uiPrintf(" |  CH14 Filter CCK Delta %4.1f  ", (float)(pHeaderInfo->scaledCh14FilterCckDelta)/10);
			uiPrintf("|                              |\n");
		}

	}

	uiPrintf(" |-------------------------------------------------------------|\n");
	if(mode == MODE_11A) {
		uiPrintf(" |   OB_1   %1d   ", pModeInfo->ob_1);
		uiPrintf("|   OB_2    %1d   ", pModeInfo->ob_2);
		uiPrintf("|   OB_3   %1d  ", pModeInfo->ob_3);
		uiPrintf("|   OB_4     %1d   |\n", pModeInfo->ob_4);
		uiPrintf(" |   DB_1   %1d   ", pModeInfo->db_1);
		uiPrintf("|   DB_2    %1d   ", pModeInfo->db_2);
		uiPrintf("|   DB_3   %1d  ", pModeInfo->db_3);
		uiPrintf("|   DB_4     %1d   |\n", pModeInfo->db_4);
	}
	else {
	if(((pHeaderInfo->majorVersion == 3) && (pHeaderInfo->minorVersion >= 1)) 
			|| (pHeaderInfo->majorVersion >= 4)){
			uiPrintf(" |   OB_1   %1d   ", pModeInfo->ob_1);
			uiPrintf("|   B_OB    %1d   ", pModeInfo->ob_4);
			uiPrintf("|   DB_1   %1d  ", pModeInfo->db_1);
			uiPrintf("|   B_DB     %1d   |\n", pModeInfo->db_4);
		}
		else {
			uiPrintf(" |  OB_1                     %1d  ", pModeInfo->ob_1);
			uiPrintf("|  DB_1                     %1d  |\n", pModeInfo->db_1);
		}
		
	}

#if 0 
	if ((mode == MODE_11A) && (pHeaderInfo->minorVersion >= 2)) {
		uiPrintf(" |-------------------------------------------------------------|\n");
		for(i = 0; i < 4; i++) {
			uiPrintf(" |  Gsel_%1d  %1d   ", i, pHeaderInfo->cornerCal[i].gSel);
			uiPrintf("|  Pd84_%1d   %1d   ", i, pHeaderInfo->cornerCal[i].pd84);
			uiPrintf("|  Pd90_%1d  %1d  ", i, pHeaderInfo->cornerCal[i].pd90);
			uiPrintf("|  clip_%1d    %1d   |\n", i, pHeaderInfo->cornerCal[i].clip);
		}
	}
#endif
	if((mode != MODE_11B) && (pHeaderInfo->majorVersion >= 5)) {
		uiPrintf(" |-------------------------------------------------------------|\n");

		uiPrintf(" |  turbo txrx atten      0x%02x  ", pModeInfo->turbo.txrxAtten);
		uiPrintf("|  turbo rxtx margin     0x%02x  |\n", pModeInfo->turbo.rxtxMargin);
		uiPrintf(" |  turbo PGA Desired sz   %2d  ", pModeInfo->turbo.pgaDesiredSize);
		uiPrintf("|  turbo ADC Desired sz   %2d  |\n", pModeInfo->turbo.adcDesiredSize);

		uiPrintf(" |  turbo switch settling 0x%02x  ", pModeInfo->turbo.switchSettling);
		uiPrintf("|                              |\n");
	}

	
	
	uiPrintf(" ===============================================================\n");

	if(((pHeaderInfo->majorVersion == 3) && (pHeaderInfo->minorVersion >= 4)) 
			 && (mode != MODE_11A)) {
		if((pModeInfo->calPier1 != 0xff) || (pModeInfo->calPier2 != 0xff)) {
			uiPrintf("\nCalibration actually performed at channels: ");
			if(pModeInfo->calPier1 != 0xff) {
				uiPrintf("%d ", pModeInfo->calPier1);
			}	

			if(pModeInfo->calPier2 != 0xff) {
				uiPrintf("%d ", pModeInfo->calPier2);
			}	

			uiPrintf("\n");
		}
	}
		
	return;
#endif
}

void printChannelInfo_16K
(
 MDK_PCDACS_ALL_MODES	*pEepromData,
 A_UINT32			mode
)
{
	A_UINT16			i, j, k = 0;
	MDK_DATA_PER_CHANNEL	*pDataPerChannel;

	switch(mode) {
	case MODE_11A:
		pDataPerChannel = pEepromData->DataPerChannel_11a;
		break;

	case MODE_11G:
	case MODE_11O:
		pDataPerChannel = pEepromData->DataPerChannel_11g;
		break;

	case MODE_11B:
		pDataPerChannel = pEepromData->DataPerChannel_11b;
		break;

	default:
		printf("Illegal mode passed to printChannelInfo_16K\n");
		return;
	}
	
	uiPrintf("\n");
	if(mode == MODE_11A) {
		uiPrintf("=========================Calibration Information============================\n");
		
		for (k = 0; k < 10; k+=5) {
			for(i = k; i < k + 5; i++) {
				uiPrintf("|     %04d     ",
					pDataPerChannel[i].channelValue);
			}
			uiPrintf("|\n");
			
			uiPrintf("|==============|==============|==============|==============|==============|\n");
			for(i = k; i < k + 5; i++) {
				uiPrintf("|pcdac pwr(dBm)");
			}
			uiPrintf("|\n");

			for(j = 0; j < pDataPerChannel[0].numPcdacValues; j++) {
				for(i = k; i < k + 5; i++) {
					uiPrintf("|  %02d    %4.1f  ",
						pDataPerChannel[i].PcdacValues[j],
						((float)(pDataPerChannel[i].PwrValues[j]))/SCALE);
				}
				uiPrintf("|\n");
			}
			
			uiPrintf("|              |              |              |              |              |\n");	
			for (i = k; i < k + 5; i++) {
				uiPrintf("| pcdac min %02d ", pDataPerChannel[i].pcdacMin);
			}
			uiPrintf("|\n");
			for (i = k; i < k + 5; i++) {
				uiPrintf("| pcdac max %02d ", pDataPerChannel[i].pcdacMax);
			}
			uiPrintf("|\n");
			uiPrintf("|==============|==============|==============|==============|==============|\n");
		}
	} else {
		uiPrintf("               ==========Calibration Information=============\n");
		
		for(i = 0; i < 3; i++) {
			if(0 == i) {
				uiPrintf("               ");
			}
			uiPrintf("|     %04d     ",
				pDataPerChannel[i].channelValue);
		}
		uiPrintf("|\n");
		
		uiPrintf("               |==============|==============|==============|\n");
		for(i = 0; i < 3; i++) {
			if(0 == i) {
				uiPrintf("               ");
			}
			uiPrintf("|pcdac pwr(dBm)");
		}
		uiPrintf("|\n");

		for(j = 0; j < pDataPerChannel[0].numPcdacValues; j++) {
			for(i = 0; i < 3; i++) {
				if(0 == i) {
					uiPrintf("               ");
				}
				uiPrintf("|  %02d    %4.1f  ",
					pDataPerChannel[i].PcdacValues[j],
					((float)(pDataPerChannel[i].PwrValues[j]))/SCALE);
			}
			uiPrintf("|\n");
		}
		
		uiPrintf("               |              |              |              |\n");	
		uiPrintf("               ");
		for (i = 0; i < 3; i++) {
			uiPrintf("| pcdac min %02d ", pDataPerChannel[i].pcdacMin);
		}
		uiPrintf("|\n");
		uiPrintf("               ");
		for (i = 0; i < 3; i++) {
			uiPrintf("| pcdac max %02d ", pDataPerChannel[i].pcdacMax);
		}
		uiPrintf("|\n");
		uiPrintf("               |==============|==============|==============|\n");

	}
}

void printTargetPowerInfo_16K
(
 MDK_TRGT_POWER_ALL_MODES	*pPowerInfoAllModes,
 A_UINT32			mode
)
{
	A_UINT16 i, k;
	MDK_TRGT_POWER_INFO		*pPowerInfo;
	A_UINT16    numTrgtPwrPiers;

	uiPrintf("\n");
	if(mode == MODE_11A) {
		pPowerInfo = pPowerInfoAllModes->trgtPwr_11a;
		uiPrintf("============================Target Power Info===============================\n");
	
		for (k = 0; k < 8; k+=4) {
			uiPrintf("|     rate     ");
			for(i = k; i < k + 4; i++) {
				uiPrintf("|     %04d     ",
					pPowerInfo[i].testChannel);
			}
			uiPrintf("|\n");
			
			uiPrintf("|==============|==============|==============|==============|==============|\n");

			uiPrintf("|     6-24     ");
			for (i = k; i < k + 4; i++) {
				uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr6_24)/2);
			}
			uiPrintf("|\n");

			uiPrintf("|      36      ");
			for (i = k; i < k + 4; i++) {
				uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr36)/2);
			}
			uiPrintf("|\n");

			uiPrintf("|      48      ");
			for (i = k; i < k + 4; i++) {
				uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr48)/2);
			}
			uiPrintf("|\n");

			uiPrintf("|      54      ");
			for (i = k; i < k + 4; i++) {
				uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr54)/2);
			}

			uiPrintf("|\n");
			uiPrintf("|==============|==============|==============|==============|==============|\n");
		}
	}
	else {
		if(mode == MODE_11B) {
			pPowerInfo = pPowerInfoAllModes->trgtPwr_11b;
			numTrgtPwrPiers = pPowerInfoAllModes->numTargetPwr_11b;
		}
		else {
			pPowerInfo = pPowerInfoAllModes->trgtPwr_11g;
			numTrgtPwrPiers = pPowerInfoAllModes->numTargetPwr_11g;
		}
		if(numTrgtPwrPiers == 2){
			uiPrintf("=============Target Power Info================\n");
		} else {
			uiPrintf("====================Target Power Info========================\n");
		}

		uiPrintf("|     rate     ");
		for(i = 0; i < numTrgtPwrPiers; i++) {
			uiPrintf("|     %04d     ",
				pPowerInfo[i].testChannel);
		}
		uiPrintf("|\n");
		
		if(numTrgtPwrPiers == 2){
			uiPrintf("|==============|==============|==============|\n");
		} else {
			uiPrintf("|==============|==============|==============|==============|\n");

		}

		if(mode == MODE_11B) {
			uiPrintf("|      1       ");
		}
		else {
			uiPrintf("|     6-24     ");
		}

		for (i = 0; i < numTrgtPwrPiers; i++) {
			uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr6_24)/2);
		}
		uiPrintf("|\n");

		if(mode == MODE_11B) {
			uiPrintf("|      2       ");
		}
		else {
			uiPrintf("|      36      ");
		}
		for (i = 0; i < numTrgtPwrPiers; i++) {
			uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr36)/2);
		}
		uiPrintf("|\n");

		if(mode == MODE_11B) {
			uiPrintf("|      5.5     ");
		}
		else {
			uiPrintf("|      48      ");
		}
		for (i = 0; i < numTrgtPwrPiers; i++) {
			uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr48)/2);
		}
		uiPrintf("|\n");

		if(mode == MODE_11B) {
			uiPrintf("|      11      ");
		}
		else {
			uiPrintf("|      54      ");
		}
		for (i = 0; i < numTrgtPwrPiers; i++) {
			uiPrintf("|     %4.1f     ", (float)(pPowerInfo[i].twicePwr54)/2);
		}

		uiPrintf("|\n");
		if(numTrgtPwrPiers == 2){
			uiPrintf("|==============|==============|==============|\n");
		} else {
			uiPrintf("|==============|==============|==============|==============|\n");

		}

	}
}

void printRDEdges_16K
(
 MDK_RD_EDGES_POWER		*pRdEdgePwrInfo,
 A_UINT16			*pTestGroups,
 A_UINT32			mode,
 A_UINT16			maxNumCtl,
 A_UINT16			majorVersion,
 A_UINT16			minorVersion
)
{
	A_UINT16	i=0, j;
	A_UINT16	ctlMode;
	char		ctlType[64];

	uiPrintf("\n");
	uiPrintf("=======================Test Group Band Edge Power========================\n");
	while ((pTestGroups[i] != 0) && (i < maxNumCtl)) {
		switch(pTestGroups[i] & 0x7) {
		case CTL_MODE_11A: 
			sprintf(ctlType, " [ 0x%x 11a base mode ] ", (pTestGroups[i]&0xf8));
			ctlMode = MODE_11A; break;
		case CTL_MODE_11A_TURBO:
			sprintf(ctlType, " [ 0x%x 11a TURBO mode ]", (pTestGroups[i]&0xf8));
			ctlMode = MODE_11A; break;
		case CTL_MODE_11B: 
			sprintf(ctlType, " [ 0x%x 11b mode ]      ", (pTestGroups[i]&0xf8));
			ctlMode = MODE_11B; break;
		case CTL_MODE_11G: 
			sprintf(ctlType, " [ 0x%x 11g mode ]      ", (pTestGroups[i]&0xf8));
			ctlMode = MODE_11G; break;
		case CTL_MODE_11G_TURBO: 
			sprintf(ctlType, " [ 0x%x 11g TURBO mode ]", (pTestGroups[i]&0xf8));
			ctlMode = MODE_11G; break;
		default: uiPrintf("Illegal mode mask in CTL (0x%x) number %d\n", pTestGroups[i], i); return;
		}
		if(mode != ctlMode) {
			i++;
			pRdEdgePwrInfo+=NUM_16K_EDGES;
			continue;
		}
		uiPrintf("|                                                                       |\n");
		uiPrintf("| CTL: 0x%02x  %s", pTestGroups[i] & 0xff, ctlType);
		
		uiPrintf("                                   |\n");
		uiPrintf("|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

		uiPrintf("| edge  ");
		for(j = 0; j < NUM_16K_EDGES; j++) {
			if (pRdEdgePwrInfo[j].rdEdge == 0)
			{
				uiPrintf("|  --   ");
			} else
			{
				uiPrintf("| %04d  ", pRdEdgePwrInfo[j].rdEdge);
			}
		}

		uiPrintf("|\n");
		uiPrintf("|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");
		uiPrintf("| power ");
		for(j = 0; j < NUM_16K_EDGES; j++) {
			if (pRdEdgePwrInfo[j].rdEdge == 0)
			{
				uiPrintf("|  --   ");
			} else
			{
				uiPrintf("| %4.1f  ", (float)(pRdEdgePwrInfo[j].twice_rdEdgePower)/2);
			}
		}

		uiPrintf("|\n");
		if(((majorVersion == 3) && (minorVersion >= 3)) || (majorVersion >= 4))  {
			uiPrintf("|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");
			uiPrintf("| flag  ");
			for(j = 0; j < NUM_16K_EDGES; j++) {
				if (pRdEdgePwrInfo[j].rdEdge == 0)
				{
					uiPrintf("|  --   ");
				} else
				{
					uiPrintf("|   %1d   ", pRdEdgePwrInfo[j].flag);
				}
			}

			uiPrintf("|\n");
		}
		uiPrintf("=========================================================================\n");
		i++;
		pRdEdgePwrInfo+=NUM_16K_EDGES;
	}
}

void
printfChannelInfo_gen3
(
 EEPROM_FULL_DATA_STRUCT_GEN3 *pChannelInfo, 
 A_UINT32 mode
)
{
	EEPROM_DATA_PER_CHANNEL_GEN3	*pDataPerChannel;
	A_UINT16 i, k;
	A_UINT16 pcdacValues[10];
	A_UINT16 xpdGainMask;
	A_UINT16 k_end, i_end;
	A_UINT16 xpdGainValues[2];
	A_UINT16 numXpdGain = 0, jj;

	switch(mode) {
	case MODE_11A:
		pDataPerChannel = pChannelInfo->pDataPerChannel11a;
		xpdGainMask = pChannelInfo->xpd_mask11a;
		break;

	case MODE_11G:
	case MODE_11O:
		pDataPerChannel = pChannelInfo->pDataPerChannel11g;
		xpdGainMask = pChannelInfo->xpd_mask11g;
		break;

	case MODE_11B:
		pDataPerChannel = pChannelInfo->pDataPerChannel11b;
		xpdGainMask = pChannelInfo->xpd_mask11b;
		break;

	default:
		printf("Illegal mode passed to printfChannelInfo_gen3\n");
		return;
	}

	//calculate the value of xpdgains
	for (jj = 0; jj < NUM_XPD_PER_CHANNEL; jj++) {
		if (((xpdGainMask >> jj) & 1) > 0) {
			xpdGainValues[numXpdGain++] = (A_UINT16) jj;			
		}
	}

	uiPrintf("\n");
	if(mode == MODE_11A) {
		uiPrintf("=========================Calibration Information============================\n");
		k_end = 10;
		i_end = 5;
	}
	else {
		uiPrintf("               ==========Calibration Information=============\n");
		k_end = 3;
		i_end = 3;
	}

	for (k = 0; k < k_end; k+=5) {
		if(mode != MODE_11A) {
			uiPrintf("               ");
		}
		for(i = k; i < (k + i_end); i++) {
			uiPrintf("|     %04d     ",
				pDataPerChannel[i].channelValue);
		}
		uiPrintf("|\n");
		
    	if(mode == MODE_11A) {
	    	uiPrintf("|==============|==============|==============|==============|==============|\n");
		}
		else {
    		uiPrintf("               |==============|==============|==============|\n");
			uiPrintf("               ");
		}
		for(i = k; i < (k + i_end); i++) {
			uiPrintf("|pcdac pwr(dBm)");
		}
		uiPrintf("|\n");

    	if(mode == MODE_11A) {
			uiPrintf("|              |              |              |              |              |\n");
			uiPrintf("| XPD_Gain %2d  |              |              |              |              |\n", 
				xpdGainMapping[xpdGainValues[0]]);	
		}
		else {
    		uiPrintf("               | XPD_Gain %2d  |              |              |\n",
				xpdGainMapping[xpdGainValues[0]]);	
    		uiPrintf("               |              |              |              |\n");
		}
		
    	if(mode != MODE_11A) {
			uiPrintf("               ");		
		}
		for(i = k; i < (k + i_end); i++) {
			pcdacValues[i] = pDataPerChannel[i].pcd1_xg0;
			uiPrintf("|  %02d  %6.2f  ", pcdacValues[i],	((float)(pDataPerChannel[i].pwr1_xg0)/4));
		}
		uiPrintf("|\n");
    	if(mode != MODE_11A) {
			uiPrintf("               ");		
		}
		for(i = k; i < (k + i_end); i++) {
			pcdacValues[i] += pDataPerChannel[i].pcd2_delta_xg0;
			uiPrintf("|  %02d  %6.2f  ", pcdacValues[i],	((float)(pDataPerChannel[i].pwr2_xg0)/4));
		}
		uiPrintf("|\n");
    	if(mode != MODE_11A) {
			uiPrintf("               ");		
		}

		for(i = k; i < (k + i_end); i++) {
			pcdacValues[i] += pDataPerChannel[i].pcd3_delta_xg0;
			uiPrintf("|  %02d  %6.2f  ", pcdacValues[i],	((float)(pDataPerChannel[i].pwr3_xg0)/4));
		}

		uiPrintf("|\n");
    	if(mode != MODE_11A) {
			uiPrintf("               ");		
		}

		for(i = k; i < (k + i_end); i++) {
			pcdacValues[i] += pDataPerChannel[i].pcd4_delta_xg0;
			uiPrintf("|  %02d  %6.2f  ", pcdacValues[i],	((float)(pDataPerChannel[i].pwr4_xg0)/4));
		}
		uiPrintf("|\n");
    	if(mode != MODE_11A) {
			uiPrintf("               ");		
		}
		
		for(i = k; i < (k + i_end); i++) {
			pcdacValues[i] = 63;
			uiPrintf("|  %02d  %6.2f  ", pcdacValues[i],	((float)(pDataPerChannel[i].maxPower_t4)/4));
		}
		uiPrintf("|\n");
		
    	if(mode == MODE_11A) {
			uiPrintf("|              |              |              |              |              |\n");	
		}
		else {
    		uiPrintf("               |              |              |              |\n");	
		}

		if(numXpdGain > 1) {
    		if(mode == MODE_11A) {
				uiPrintf("| XPD_Gain %2d  |              |              |              |              |\n", 
					xpdGainMapping[xpdGainValues[1]]);	
			}
			else {
    			uiPrintf("               | XPD_Gain %2d  |              |              |\n",
					xpdGainMapping[xpdGainValues[1]]);	
			}
	    	if(mode != MODE_11A) {
				uiPrintf("               ");		
			}
			for(i = k; i < (k + i_end); i++) {
				uiPrintf("|  %02d  %6.2f  ", 20,	((float)(pDataPerChannel[i].pwr1_xg3)/4));
			}
			uiPrintf("|\n");
	    	if(mode != MODE_11A) {
				uiPrintf("               ");		
			}
			for(i = k; i < (k + i_end); i++) {
				uiPrintf("|  %02d  %6.2f  ", 35,	((float)(pDataPerChannel[i].pwr2_xg3)/4));
			}
			uiPrintf("|\n");
	    	if(mode != MODE_11A) {
				uiPrintf("               ");		
			}
			for(i = k; i < (k + i_end); i++) {
				uiPrintf("|  %02d  %6.2f  ", 63,	((float)(pDataPerChannel[i].pwr3_xg3)/4));
			}
			uiPrintf("|\n");
		}
		if(mode == MODE_11A) {
			uiPrintf("|==============|==============|==============|==============|==============|\n");
		} else {
			uiPrintf("               |==============|==============|==============|\n");
		}
	}

}

void
printfChannelInfo_gen5
(
 EEPROM_FULL_DATA_STRUCT_GEN5 *pChannelInfo, 
 A_UINT32 mode
)
{
	EEPROM_DATA_PER_CHANNEL_GEN5	*pDataPerChannel;
	A_UINT16 channelCount, channelRowCnt, vpdCount;
	A_UINT16 pdadcValues[10];
	A_INT16  powerValues_t2[10];
	A_UINT16 xpdGainMask;
	A_UINT16 channelRowCnt_end, channelCount_end;
	A_UINT16 xpdGainValues[NUM_XPD_PER_CHANNEL];
	A_UINT16 numXpdGain = 0;
	A_UINT16 pdGainCount;

	if (!pChannelInfo) {
	   printf("printfChannelInfo_gen5::NULL channel info passed\n");
	   return;
	}

	switch(mode) {
	case MODE_11A:
		pDataPerChannel = pChannelInfo->pDataPerChannel11a;
		xpdGainMask = pChannelInfo->xpd_mask11a;
		break;

	case MODE_11G:
	case MODE_11O:
		pDataPerChannel = pChannelInfo->pDataPerChannel11g;
		xpdGainMask = pChannelInfo->xpd_mask11g;
		break;

	case MODE_11B:
		pDataPerChannel = pChannelInfo->pDataPerChannel11b;
		xpdGainMask = pChannelInfo->xpd_mask11b;
		break;

	default:
		printf("Illegal mode passed to printfChannelInfo_gen5\n");
		return;
	}

	//calculate the value of xpdgains
	for (pdGainCount = 0; pdGainCount < MAX_NUM_PDGAINS_PER_CHANNEL; pdGainCount++) {
		if (((xpdGainMask >> (MAX_NUM_PDGAINS_PER_CHANNEL-pdGainCount-1)) & 1) > 0) {
			if (numXpdGain >= MAX_NUM_PDGAINS_PER_CHANNEL) {
				uiPrintf("A maximum of 4 pd_gains supported in eep_to_raw_data for gen5\n");
				exit(0);
			}
			xpdGainValues[numXpdGain++] = (A_UINT16) (MAX_NUM_PDGAINS_PER_CHANNEL-pdGainCount-1);			
		}
	}


	uiPrintf("\n");
	if(mode == MODE_11A) {
		uiPrintf("=========================Calibration Information============================\n");
		channelRowCnt_end = 10;
		channelCount_end = 5;
	}
	else {
		uiPrintf("=========================Calibration Information=============\n");
		channelRowCnt_end = 4;
		channelCount_end = 4;
	}

	for (channelRowCnt = 0; channelRowCnt < channelRowCnt_end; channelRowCnt+=5) {
		if(mode != MODE_11A) {
//			uiPrintf("               ");
		}
		for(channelCount = channelRowCnt; channelCount < (channelRowCnt + channelCount_end); channelCount++) {
			uiPrintf("|     %04d     ",
				pDataPerChannel[channelCount].channelValue);
		}
		uiPrintf("|\n");
		
    	if(mode == MODE_11A) {
	    	uiPrintf("|==============|==============|==============|==============|==============|\n");
		}
		else {
    		uiPrintf("===============|==============|==============|==============|\n");
//			uiPrintf("               ");
		}
		for(channelCount = channelRowCnt; channelCount < (channelRowCnt + channelCount_end); channelCount++) {
			uiPrintf("|pdadc pwr(dBm)");
		}
		uiPrintf("|\n");

		for(pdGainCount = 0; pdGainCount < numXpdGain; pdGainCount++) {
			if(mode == MODE_11A) {
				uiPrintf("|              |              |              |              |              |\n");
				uiPrintf("| PD_Gain %2d   |              |              |              |              |\n", 
					xpdGainMapping_gen5[xpdGainValues[pdGainCount]]);	
			}
			else {
    			uiPrintf("| PD_Gain %2d   |              |              |              |\n",
					xpdGainMapping_gen5[xpdGainValues[pdGainCount]]);	
    			uiPrintf("|              |              |              |              |\n");
			}
			
    		if(mode != MODE_11A) {
	//			uiPrintf("               ");		
			}
			for(channelCount = channelRowCnt; channelCount < (channelRowCnt + channelCount_end); channelCount++) {
				pdadcValues[channelCount] = pDataPerChannel[channelCount].Vpd_I[pdGainCount];
				powerValues_t2[channelCount] = pDataPerChannel[channelCount].pwr_I[pdGainCount] * 2;
				uiPrintf("|  %03d %6.2f  ", pdadcValues[channelCount],	((float)(powerValues_t2[channelCount])/2));
			}
			uiPrintf("|\n");
    		if(mode != MODE_11A) {
//				uiPrintf("               ");		
			}
			for(vpdCount = 0; vpdCount < NUM_POINTS_OTHER_PDGAINS - 1; vpdCount++) {
				for(channelCount = channelRowCnt; channelCount < (channelRowCnt + channelCount_end); channelCount++) {
					pdadcValues[channelCount] += pDataPerChannel[channelCount].Vpd_delta[vpdCount][pdGainCount];
					powerValues_t2[channelCount] += pDataPerChannel[channelCount].pwr_delta_t2[vpdCount][pdGainCount];
					uiPrintf("|  %03d %6.2f  ", pdadcValues[channelCount],	((float)(powerValues_t2[channelCount])/2));
				}
				uiPrintf("|\n");
	   			if(mode != MODE_11A) {
//					uiPrintf("               ");		
				}
			}

			if(pdGainCount == numXpdGain - 1) {  //this is the last xpdgain, has an extra set
				for(channelCount = channelRowCnt; channelCount < (channelRowCnt + channelCount_end); channelCount++) {
					pdadcValues[channelCount] += pDataPerChannel[channelCount].Vpd_delta[vpdCount][pdGainCount];
					powerValues_t2[channelCount] += pDataPerChannel[channelCount].pwr_delta_t2[vpdCount][pdGainCount];
					uiPrintf("|  %03d %6.2f  ", pdadcValues[channelCount],	((float)(powerValues_t2[channelCount])/2));
				}

				uiPrintf("|\n");
	    		if(mode != MODE_11A) {
//					uiPrintf("               ");		
				}
			}

			
    		if(mode == MODE_11A) {
				uiPrintf("|              |              |              |              |              |\n");	
			}
			else {
    			uiPrintf("|              |              |              |              |\n");	
			}

		}

		if(mode == MODE_11A) {
			uiPrintf("|==============|==============|==============|==============|==============|\n");
		} else {
			uiPrintf("|==============|==============|==============|==============|\n");
		}
	}

}


void printEepromStruct_16K
(
 A_UINT32	devNum,
 A_UINT32	mode
)
{
	void				*pTempPtr;
	MDK_EEP_HEADER_INFO		*pHeaderPtr;	//need to keep a copy of header pointer for later
	A_UINT16 testGroups[MAX_NUM_CTL];
	A_UINT16 numCtl, minorRevision, majorRevision;

	if(isDragon(devNum)) {
#if defined(LINUX) || defined(_WINDOWS)
		printAR6000Eeprom(devNum);
#endif
		return;	
	}
	
	art_GetEepromStruct(devNum, EEP_HEADER_16K, &pTempPtr);
	pHeaderPtr = (MDK_EEP_HEADER_INFO *)pTempPtr; 
	printHeaderInfo_16K(pHeaderPtr, mode);
	//take a copy of the NUM_CTL test groups for later
	memcpy(testGroups, pHeaderPtr->testGroups, sizeof(A_UINT16) * pHeaderPtr->numCtl);
	numCtl = pHeaderPtr->numCtl;
	minorRevision = pHeaderPtr->minorVersion;
	majorRevision = pHeaderPtr->majorVersion;

	uiPrintf("\nPress any key to continue\n");
	while(!kbhit());
	getch();

	if((majorRevision ==3) ||
		((majorRevision >= 4) && (pHeaderPtr->eepMap == 0))){
		art_GetEepromStruct(devNum, EEP_CHANNEL_INFO_16K, &pTempPtr);
		printChannelInfo_16K((MDK_PCDACS_ALL_MODES *)pTempPtr, mode);
	}
	else if((majorRevision >=4) && pHeaderPtr->eepMap==1) {
		art_GetEepromStruct(devNum, EEP_GEN3_CHANNEL_INFO, &pTempPtr);
		printfChannelInfo_gen3((EEPROM_FULL_DATA_STRUCT_GEN3 *)pTempPtr, mode);

	}
	else if(majorRevision ==5) {
		art_GetEepromStruct(devNum, EEP_GEN5_CHANNEL_INFO, &pTempPtr);
		printfChannelInfo_gen5((EEPROM_FULL_DATA_STRUCT_GEN5 *)pTempPtr, mode);

	}
	else {
		uiPrintf("ERROR: EEPROM version number NOT supported for displaying channel info\n");
	}
	uiPrintf("\nPress any key to continue\n");
	while(!kbhit());
	getch();

	art_GetEepromStruct(devNum, EEP_TRGT_POWER_16K, &pTempPtr);
	printTargetPowerInfo_16K((MDK_TRGT_POWER_ALL_MODES *)pTempPtr, mode);
	uiPrintf("\nPress any key to continue\n");
	while(!kbhit());
	getch();

	art_GetEepromStruct(devNum, EEP_RD_POWER_16K, &pTempPtr);
	printRDEdges_16K((MDK_RD_EDGES_POWER *)pTempPtr, testGroups, mode, numCtl, majorRevision, minorRevision);
	uiPrintf("\nPress any key to continue\n");
	while(!kbhit());
	getch();
	return;
}
