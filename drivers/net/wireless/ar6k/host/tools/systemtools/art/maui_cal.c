#ifdef _WINDOWS
#include <windows.h>
#endif 
#include <stdio.h>
#include <assert.h>
#include <time.h>
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
#define __ARREGH__  // This is required to avoid including ar5210reg.h
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
#include "cmdTest.h"
#if defined(LINUX) || defined(__linux__)
#include "linux_ansi.h"
#include "unistd.h"
#endif	

#include "art_if.h"
#undef __ARREGH__  // This is required to avoid including ar5210reg.h
#include "ar5212/mEEPROM_d.h"
#include "cal_gen3.h"
#include "ar5211/ar5211reg.h"  /* AR5001 register definitions */
//++JC++
#include "ar2413/mEEPROM_g.h"
#include "cal_gen5.h"
//++JC++

#include "ear_externs.h"
#if defined(LINUX)
#include "mEepStruct6000.h"
#elif defined(_WINDOWS)
#include "ar6000/mEepStruct6000.h"
#endif
#ifdef _IQV
#include "../../../../common/common.h"
#include "../../../../common/IQ_art_common.h"
extern int			ErrCode;
extern int		testType;
/*
 * \brief Claim global function readAr6000MacAddress, 
 * \brief it will be used by IQ_art_ReadMACID in IQ_test.c
 * \brief PS created it on 06/21/07 
 */
extern void readAr6000MacAddress(A_UINT32 devNum, A_UCHAR *pMacAddr_arr);
#endif // _IQV

extern void writeAr6000MacAddress(A_UINT32 devNum, A_UCHAR *pMacAddr_arr);

extern A_UINT32 subSystemID;
extern A_UCHAR DataRate[]; // declared in test.c
extern A_UCHAR calsetupFileName[];
extern char *machName;
extern A_BOOL usb_client;
extern A_BOOL sdio_client;


//extern GAIN_OPTIMIZATION_LADDER gainLadder;
extern GAIN_OPTIMIZATION_LADDER *pCurrGainLadder;

//extern MLD_CONFIG configSetup;
extern ART_SOCK_INFO *artSockInfo;
extern CAL_SETUP CalSetup;
extern YIELD_LOG_STRUCT  yldStruct;
//extern A_BOOL printLocalInfo;
	   
// extern declarations for dut-golden sync
extern ART_SOCK_INFO *pArtPrimarySock;
extern ART_SOCK_INFO *pArtSecondarySock;

extern  EEPROM_DATA_STRUCT_GEN5 *pCalDataset_gen5[] ; //++JC++
extern  EEPROM_DATA_STRUCT_GEN3 *pCalDataset_gen3[] ;
extern const A_CHAR  *DataRateStr[];
extern const A_CHAR  *DataRate_11b[];

 char ackRecvStr[1024];
 char ackSendStr[1024];
 A_INT32 ackSendPar1, ackSendPar2, ackSendPar3;
 A_INT32 ackRecvPar1, ackRecvPar2, ackRecvPar3;


static A_UINT16 SideChannel = 5120; // channel used for sending handshake.
static A_UINT16 SideChannel_2p4 = 2312; // channel used for sending handshake.
A_UINT16 eepromType = EEPROM_SIZE_16K;

extern GOLDEN_PARAMS  goldenParams;

extern MANLIB_API A_UINT8 dummyEepromWriteArea[AR6K_EEPROM_SIZE];

A_UINT32 devlibModeFor[3] = {MODE_11G, MODE_11B, MODE_11A};
A_UINT32 calModeFor[3] = {MODE_11a, MODE_11g, MODE_11b};


static A_UINT16 rates[MAX_RATES] = {6,9,12,18,24,36,48,54};

A_UINT32 VERIFY_DATA_PACKET_LEN   = 2352;

 A_INT32 devPM, devSA, devATT;
 A_INT32 guDevPM, guDevAtt;

static A_UINT16 MKK_CHAN_LIST[] = {5170, 5190, 5210, 5230}; // channels used for OBW test
A_UINT16 numRAWChannels = 0; //will be computed prior to measurements.
A_UINT16 RAW_CHAN_LIST[201] ; // can accomodate 5000 - 6000 in steps of 5 MHz
A_UINT16 PCDACS_MASTER_LIST[] = {1,4, 7,10, 12,14,15,16,18,20, 22, 25, 28,32,37, 42, 48, 54, 60, 63};
//A_UINT16 PCDACS_MASTER_LIST[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,
//								 27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,
//								 55,56,57,58,59,60,61,62,63};
A_UINT16 *RAW_PCDACS;
A_UINT16 sizeOfRawPcdacs = (A_UINT16) sizeof(PCDACS_MASTER_LIST)/sizeof(A_UINT16);

static 	A_UINT32  devNumArr[3];

static A_UCHAR  bssID[6]     = {0x50, 0x55, 0x5A, 0x50, 0x00, 0x00};
static A_UCHAR  rxStation[6] = {0x10, 0x11, 0x12, 0x13, 0x00, 0x00};	// DUT
static A_UCHAR  txStation[6] = {0x20, 0x22, 0x24, 0x26, 0x00, 0x00};	// Golden
static A_UCHAR  NullID[6]    = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
static A_UCHAR  pattern[2] = {0xaa, 0x55};


static A_UCHAR  verifyDataPattern[NUM_VERIFY_DATA_PATTERNS][LEN_VERIFY_DATA_PATTERNS]  = { 
											{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
											{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
											{0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
											{0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55},
											{0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66},
											{0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99},
											{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80},
											{0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F}
										};



static A_UINT32 PER_TEST_RATE[] = {6, 36, 48, 54};
static A_UINT32 PER_TEST_RATE_11G_CCK[] = {0xb1, 0xd5, 0xbb, 0xdb};
static A_UINT32 PER_MARGIN_TEST_RATE[] = {48, 54};
static A_UINT32 PER_RATEMASK = RATE_6|RATE_36|RATE_48|RATE_54;
static A_UINT32 PER_RATEMASK_11G_CCK = 0x7100;
static A_UINT32 PER_MARGIN_RATEMASK = RATE_48|RATE_54;
static A_UINT32 PER_RATE_COUNT = 4;
static A_UINT32 PER_MARGIN_RATE_COUNT = 2;
//static A_UINT32 PER_TEST_FRAME = 100;
static A_UINT32 PER_TEST_FRAME = 50;
static A_UINT32 PER_FRAME_LEN  = 1000;
static A_UINT32 NUM_11G_PPM_PKTS = 20;
//static A_UINT32 PER_GOOD_FRAME = 90;	// 90% of the frame send/receive

static A_UINT32 NUM_DATA_CHECK_FRAME = 4;

static A_UINT32 SEN_TEST_RATE[] = {48, 54};
static A_UINT32 SEN_RATEMASK = RATE_GROUP | RATE_48 | RATE_54;
static A_UINT32 SEN_RATEMASK_11G_CCK = RATE_GROUP | 0x6000;
static A_UINT32 LO_SEN_TEST_RATE_11G_CCK[] = {0xb1, 0xb1};
static A_UINT32 SEN_TEST_RATE_11G_CCK[] = {0xbb, 0xdb};
static A_UINT32 SEN_RATE_COUNT = 2;
//static A_UINT32 SEN_TEST_FRAME = 100;  // use deprecated by CalSetup.numSensPackets[mode] starting w/ ART_2.5b18
static A_UINT32 SEN_FRAME_LEN  = 1000;
//static A_UINT32 SEN_GOOD_FRAME = 90;	// 90% of the frame send/receive

//static A_INT32 PPM_MAX = 9;
//static A_INT32 PPM_MIN = -9;
static A_BOOL  TestFail;

 dPCDACS_EEPROM RawDataset ; // raw power measurements
 dPCDACS_EEPROM *pRawDataset = &RawDataset ; // raw power measurements
 dPCDACS_EEPROM RawGainDataset ; // raw gainf measurements
 dPCDACS_EEPROM *pRawGainDataset = &RawGainDataset ; // raw gainf measurements
 dPCDACS_EEPROM CalDataset ; // calibration dataset
 dPCDACS_EEPROM *pCalDataset = &CalDataset ; // calibration dataset
 dPCDACS_EEPROM FullDataset ; // full dataset
 dPCDACS_EEPROM *pFullDataset = &FullDataset ; // full dataset

 dPCDACS_EEPROM RawDataset_11g ; // raw power measurements for 11g
 dPCDACS_EEPROM RawDataset_11b ; // raw power measurements for 11b
 dPCDACS_EEPROM *pRawDataset_2p4[2] = {&RawDataset_11g, &RawDataset_11b} ; // raw power measurements

 dPCDACS_EEPROM CalDataset_11g ; // calibration dataset
 dPCDACS_EEPROM CalDataset_11b ; // calibration dataset
 dPCDACS_EEPROM *pCalDataset_2p4[2] = {&CalDataset_11g, &CalDataset_11b} ; // calibration dataset

 dPCDACS_EEPROM RawGainDataset_11g ; // raw power measurements for 11b
 dPCDACS_EEPROM RawGainDataset_11b ; // raw power measurements for 11b
 dPCDACS_EEPROM *pRawGainDataset_2p4[2] = {&RawGainDataset_11g, &RawGainDataset_11b} ; // raw power measurements

char calPowerLogFile_2p4[2][122] = {"cal_AR5211_Power_11g.log", "cal_AR5211_Power_11b.log"};
static char calGainfLogFile_2p4[2][122] = {"cal_AR5211_Gainf_11g.log", "cal_AR5211_Gainf_11b.log"};

extern A_UINT16 numRAWChannels_2p4;
//A_UINT16 RAW_CHAN_LIST_2p4[3] = {2412, 2447, 2484}; // Never change them. These values are NOT stored 
														   // on EEPROM, but are hardcoded in the driver instead.
// Needed to change these channels starting eeprom version 3.3 to accomodate
// Korea channels for 11g. - PD 9/02.
A_UINT16 RAW_CHAN_LIST_2p4[2][3] = {{2312, 2412, 2484}, {2412, 2472, 2484}}; // {11g, 11b}


extern TARGETS_SET	  *pTargetsSet;
extern TARGETS_SET	  *pTargetsSet_2p4[];

extern TEST_SET	  *pTestSet[3] ;
extern  char modeName[3][122]  ;

static A_UINT32		reportTiming = 1;
static char			testname[50][32] ;
static A_INT32		testtime[50];
static A_UINT32		timestart, timestop, globalteststarttime;
static A_UINT32		testnum = 0;
static char			failTest[50]; // flag if curr test has a failure.

A_BOOL		REWIND_TEST = FALSE;
static A_BOOL		SETUP_PM_MODEL2_ONCE = FALSE;
static A_BOOL		DUT_SUPPORTS_MODE[3] = {FALSE, FALSE, FALSE}; // flags used by golden
static A_BOOL     do_11g_CCK   = FALSE;

extern A_BOOL printLocalInfo;
extern A_BOOL StressTest; 

static A_UINT32	optGainLadderIndex[3];
static A_BOOL	NEED_GAIN_OPT_FOR_MODE[] = {FALSE, FALSE, FALSE};

static A_UINT32 loadEARState;
static A_UINT32 enableXRState;
static A_BOOL quarterChannelState;
static A_BOOL eepromFree = FALSE;

extern const A_CHAR  *DataRateShortStr[];
extern const A_CHAR  *DataRateShortStr_11b[];

static void dutThroughputTest(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT16 frameLen, 
					   A_UINT32 numPackets);
static void goldenThroughputTest(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT16 frameLen,
						  A_UINT32 numPackets);

#define NUM_THROUGHPUT_PACKETS		 500
#define NUM_THROUGHPUT_PACKETS_11B   100
#define THROUGHPUT_PACKET_SIZE		 1500
#define THROUGHPUT_TURBO_PACKET_SIZE 3992
//#define THROUGHPUT_TURBO_PACKET_SIZE 2992


static A_UINT32 TX_PER_STRESS_MARGIN = 0;

//A_UINT32 EEPROM_DATA[2][0x400] ;
A_UINT32 **EEPROM_DATA_BUFFER;

extern void programCompactEeprom(A_UINT32);

A_UINT16 topCalibrationEntry(A_UINT32 *pdevNum_inst1, A_UINT32 *pdevNum_inst2) 
{
	A_UINT32 devNum_inst1, devNum_inst2;
	SUB_DEV_INFO devStruct;

	devNum_inst1 = *pdevNum_inst1;
	devNum_inst2 = *pdevNum_inst2;

	loadEARState = configSetup.loadEar;
	enableXRState = configSetup.enableXR;
	quarterChannelState = configSetup.quarterChannel;

#ifdef _IQV
  if (testType != ART_EXIT)
  {
#endif // _IQV
	// force loadEAR to 0 to capture values from new config file
	// loadEAR is restored just before the dutTest
	//also turn off eeprom loading
	//also want to force off enable_xr, re-enable on exit from cal
	configSetup.loadEar = 0;
	configSetup.eepromLoad = 0;
	configSetup.enableXR = 0;
	configSetup.quarterChannel = 0;

	art_setResetParams(devNum_inst1, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
								(A_BOOL)1, MODE_11A, configSetup.use_init);		
	art_configureLibParams(devNum_inst1);
	if (!art_resetDevice(devNum_inst1, rxStation, bssID, 5220, 0))
		return FALSE;
	processEepFile(devNum_inst1, configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion);

	art_getDeviceInfo(devNum_inst1, &devStruct);
	swDeviceID = devStruct.swDevID;
	/*if(isDragon_sd(swDeviceID)) 
   	   art_writeField(devNum_inst1, "mc_eeprom_size_ovr", 3); */

    if (devNum_inst1 != devNum_inst2)  {
		art_setResetParams(devNum_inst2, configSetup.pCfgFile, \
					(A_BOOL)configSetup.eepromLoad, \
						(A_BOOL)1, MODE_11A, configSetup.use_init);		
		art_configureLibParams(devNum_inst2);
	    if (!art_resetDevice(devNum_inst2, rxStation, bssID, 2412, 0))
			return FALSE;
		processEepFile(devNum_inst2, configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion);
		art_getDeviceInfo(devNum_inst2, &devStruct);
    	swDeviceID = devStruct.swDevID;
		/*if(isDragon_sd(swDeviceID)) 
   	   		art_writeField(devNum_inst2, "mc_eeprom_size_ovr", 3); */

	}

	devNumArr[MODE_11a] = devNum_inst1;
	devNumArr[MODE_11g] = devNum_inst2;
	devNumArr[MODE_11b] = devNum_inst2;
	
	setup_raw_pcdacs();
	if (!calibrationMenu())
		return FALSE;
#ifdef _IQV
	return;
  } else {	// testType == ART_EXIT
	devNumArr[MODE_11a] = devNum_inst1;
	devNumArr[MODE_11g] = devNum_inst2;
	devNumArr[MODE_11b] = devNum_inst2;

//	setup_raw_pcdacs();
	if (!calibrationMenu())
		return FALSE;
	if (RAW_PCDACS!=NULL) {
		free(RAW_PCDACS);
		RAW_PCDACS = NULL;
	}

  }
#endif // _IQV	
	*pdevNum_inst1 = devNumArr[MODE_11a];
	*pdevNum_inst2 = devNumArr[MODE_11g];
	
	configSetup.loadEar = loadEARState;
	configSetup.enableXR = enableXRState;
	configSetup.quarterChannel = quarterChannelState;
	
	art_configureLibParams(devNum_inst1);
	art_configureLibParams(devNum_inst2);
	return TRUE;
} 
		

A_UINT16 calibrationMenu() 
{
	A_BOOL exitLoop  = FALSE;
	A_UINT32 channel = 5220;
	A_UINT32 devNum  = devNumArr[MODE_11a];
	A_UINT32 i=0,k=0;
        A_CHAR charCmd;

	if (configSetup.validInstance == 2) 
	{
		devNum = devNumArr[MODE_11g];
	}
#ifdef _IQV
	if (testType != ART_EXIT) {
#endif
	if ( !processEepFile(devNum, configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion))
	{ 
		uiPrintf("Could Not Parse the .eep file %s for SubsystemID %x.\n", configSetup.cfgTable.pCurrentElement->eepFilename, 
			configSetup.cfgTable.pCurrentElement->subsystemID);
		return FALSE;
	}
#ifndef _IQV
	parseSetup(devNum); 
#endif
	if( configSetup.force_antenna && isDragon_sd(swDeviceID))  //get here values for 11a
	{
		updateForcedAntennaVals( devNum );

	}
	//userEepromSize = (CalSetup.eepromLength*1024)/16;
	
	/*if(userEepromSize == 0x400)
	{
		checkSumLength = eepromSize = 0x400;
	}
	else
				{
		eepromSize = userEepromSize;

		uiPrintf("SNOOP: IN IN Calibaration Menu EEPSIZE= 0x%x\n", eepromSize );
		uiPrintf("SNOOP: Ivalid check sum Length Before Calibration \n ");

	}*/
			
	EEPROM_DATA_BUFFER =(A_UINT32 **) malloc(sizeof(A_UINT32 *) * NUMEEPBLK);
	
	if(EEPROM_DATA_BUFFER != NULL)	{
		for (i = 0; i < 2; i++) {
			EEPROM_DATA_BUFFER[i]= (A_UINT32 *)malloc(sizeof(A_UINT32) * eepromSize);
			if(EEPROM_DATA_BUFFER[i] == NULL){
						uiPrintf(" Memory Not allocated in calibrationMenu() \n");
						return FALSE;
				}
		}
 }
	else {
	 uiPrintf(" Memory Not allocated in calibrationMenu() \n");
		return FALSE;
 }

	for(i=0;i<NUMEEPBLK;i++)
	{
		for(k =0;k< eepromSize;k++)
	{
			EEPROM_DATA_BUFFER[i][k]=0xffff;			
		}
	}

	if (CalSetup.useFastCal)
	{
		if ( (!setup_raw_datasets()) ||
			 (!setup_raw_datasets_2p4(MODE_11g)) ||
			 (!setup_raw_datasets_2p4(MODE_11b))	 )			
		{ 
			uiPrintf("Could not setup raw datasets. Exiting...\n");
			closeEnvironment();
			return FALSE;
		}
	}

	if ((CalSetup.eeprom_map == CAL_FORMAT_GEN3) ||
		(CalSetup.eeprom_map == CAL_FORMAT_GEN5) )
	{
		CalSetup.TrgtPwrStartAddr = 0x150;
		if(CalSetup.eeprom_map == CAL_FORMAT_GEN5) 
		{
			//add extra dummy bytes to calculation of ear start
			CalSetup.TrgtPwrStartAddr += NUM_DUMMY_EEP_MAP1_LOCATIONS; 
			CalSetup.calStartAddr = CalSetup.TrgtPwrStartAddr;

		}
		if (CalSetup.cal_mult_xpd_gain_mask[MODE_11a] == 0)
		{
			CalSetup.xgain = 1 << CalSetup.xgain ;
		} 
		else 
		{
			CalSetup.xgain = CalSetup.cal_mult_xpd_gain_mask[MODE_11a];
		}
		if (CalSetup.cal_mult_xpd_gain_mask[MODE_11b] == 0) 
		{
			CalSetup.xgain_2p4[MODE_11b] = 1 << CalSetup.xgain_2p4[MODE_11b] ;
		} 
		else 
		{
			CalSetup.xgain_2p4[MODE_11b] = CalSetup.cal_mult_xpd_gain_mask[MODE_11b];			
		}
		if (CalSetup.cal_mult_xpd_gain_mask[MODE_11g] == 0) 
		{
			CalSetup.xgain_2p4[MODE_11g] = 1 << CalSetup.xgain_2p4[MODE_11g] ;
		} 
		else 
		{
			CalSetup.xgain_2p4[MODE_11g] = CalSetup.cal_mult_xpd_gain_mask[MODE_11g];
		}
	}
#ifdef _IQV
	}	//	if (testType != ART_EXIT) {
#endif
	while(exitLoop == FALSE) 
	{
#ifndef	_IQV
        printf("\n");
        printf("=============================================\n");
        printf("| Manufacturing Test & Calibration Options: |\n");
        printf("|   d - (D)evice Under Test Begin           |\n");
        printf("|   g - (G)olden Unit Test Begin            |\n");
		if (CalSetup.useFastCal)
		{
			printf("|   f - Fastcal Cal - start (f)irst on GU   |\n");
			printf("|   s - Fastcal Cal - start (s)econd on DUT |\n");
		}
    
		printf("|   q - (Q)uit                              |\n");
    printf("=============================================\n");
    if (1 == StressTest) {
        static int toggleCmd=0;
        if (toggleCmd++ %2)
            charCmd = 'Q';
        else
            charCmd = 'D';
    }
    else
        charCmd = toupper(getch()); 

    switch(charCmd) 
#else	// _IQV
    switch('D') 
#endif // _IQV
		{
      case 'D':
#ifdef _IQV
		  if (testType !=	ART_EXIT) {
			if (!dutBegin())
				return FALSE;
			return TRUE;
		  } else {
			exitLoop = TRUE;
			uiPrintf("exiting\n");
		}
#else	// _IQV
		  dutBegin();
#endif // _IQV
			break;
    
			case 'G':
			goldenTest();
			break;
      
			case 'F':
			if (CalSetup.useFastCal)
				fastCalMenu_GU(devNum);
			break;
      
			case 'S':
			if (CalSetup.useFastCal)
				fastCalMenu_DUT(devNum);
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
	if(!eepromFree)
	{
#ifndef _IQV
		for( i =0; i < NUMEEPBLK; i++){
			if(EEPROM_DATA_BUFFER[i])
				free(EEPROM_DATA_BUFFER[i]);
			}
		if(EEPROM_DATA_BUFFER)
			free(EEPROM_DATA_BUFFER);
#else
	if (EEPROM_DATA_BUFFER) {
		for( i =0; i < NUMEEPBLK; i++) { 
			if(EEPROM_DATA_BUFFER[i])
				free(EEPROM_DATA_BUFFER[i]);
				EEPROM_DATA_BUFFER[i] = NULL;
		}
		free(EEPROM_DATA_BUFFER);
		EEPROM_DATA_BUFFER = NULL;
	}
#endif		
		eepromFree = TRUE;
	}
		return TRUE;
}


A_UINT16 dutBegin() 
{
    A_BOOL		exitLoop = FALSE;
	A_UINT32	tmpVal, ii;
	A_UINT32	attenVal;
	A_UINT32    dual_11a_devNum;
	A_UINT32	i=0,k=0;
	SUB_DEV_INFO devStruct;


	testnum = 0;	

	if ( configSetup.dutSSID < 1 ) 		
	{
		uiPrintf("please specify appropriate DUT_CARD_SSID in artsetup.txt\n");
		return FALSE;
	} 
	else if (configSetup.dutSSID != configSetup.cfgTable.pCurrentElement->subsystemID)
	{
		uiPrintf("DUT_CARD_SSID specified in artsetup.txt (%x) does not match with the  SSID of this card: (%x). ",configSetup.dutSSID ,
				  configSetup.cfgTable.pCurrentElement->subsystemID);
		uiPrintf("Set the 'DUT_CARD_SSID' in artsetup.txt appropriately to calibrate the desired card.\n");
		return FALSE;
	}
	if( configSetup.force_antenna && isDragon_sd(swDeviceID))
	{
		configSetup.antenna_to_force = FORCE_ANTA;
		//get 11a mode values	
	updateForcedAntennaVals( devNumArr[MODE_11A]);
	}

	// setup the channel lists 
	setupChannelLists();
	
	if(!parseTargets()) 
	{
		uiPrintf("An error occured while parsing the file %s. Pl. check for format errors.\n", CalSetup.tgtPwrFilename);
	}

	eepromType = (A_UINT16) CalSetup.dutPromSize ;
	if (CalSetup.atherosLoggingScheme) {
		setupAtherosCalLogging();
	}
#ifndef _IQV
        if (0 == StressTest) {
	uiPrintf("\n============================================");
	uiPrintf("\nDUT (Device Under Test) calibration & test");
	uiPrintf("\nPress any key to start or <ESC> to quit");
	uiPrintf("\n============================================\n");
	while (!kbhit())
			;
        }
#endif // _IQV			
	globalteststarttime = milliTime();

#ifndef _IQV
        if (0 == StressTest) {
		if(getch() == 0x1b)
			exitLoop = TRUE;
        }
#endif // _IQV	

/*	devNumArr[MODE_11a] = devNum_def;
	if ((swDeviceID == 0xa014)||(swDeviceID == 0xa016)) {
		devNumArr[MODE_11b] = art_setupDevice(2); // get devNum for instance = 2 for freedom2 for 2.5G
		devNumArr[MODE_11g] = devNumArr[MODE_11b];
		art_resetDevice(devNumArr[MODE_11b], rxStation, bssID, 2412, 0);	
	} else {
		devNumArr[MODE_11b] = devNum_def;
		devNumArr[MODE_11g] = devNum_def;
	}
*/


	while (!exitLoop) 
	{
		REWIND_TEST = FALSE;
		TestFail = FALSE;
		for (ii=0; ii<30; ii++) failTest[ii] = 0; // initialize test fail flags

		configSetup.eepromLoad = 0;
		
#ifndef _IQV
		if ( CalSetup.useFastCal || CalSetup.testTXPER || CalSetup.testRXSEN ||
			 CalSetup.testTXPER_2p4[MODE_11b] || CalSetup.testRXSEN_2p4[MODE_11b] ||
			 CalSetup.testTXPER_2p4[MODE_11g] || CalSetup.testRXSEN_2p4[MODE_11g] ||
			 CalSetup.testDataIntegrity[MODE_11a] || CalSetup.testDataIntegrity[MODE_11g] ||
			 CalSetup.testDataIntegrity[MODE_11b] || (CalSetup.do_iq_cal) ||
			 CalSetup.testThroughput[MODE_11a] || CalSetup.testThroughput[MODE_11g] ||
			 CalSetup.testThroughput[MODE_11b])
//			 ((swDeviceID & 0xFF) >= 0x14))
		{               
			sendSync(devNumArr[MODE_11a], CalSetup.goldenIPAddr, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			//if we are using the label scheme, we need an extra sync to send the ssid to the golden
			if(configSetup.enableLabelScheme) {
				sendAck(devNumArr[MODE_11a], "Sending DUT SSID", configSetup.dutSSID, 0, 0, CalSetup.customerDebug);
			}
			if(configSetup.computeCalsetupName) {
				sendAck(devNumArr[MODE_11a], (A_CHAR *)calsetupFileName, 0, 0, 0, CalSetup.customerDebug);
			}
		}

		if (REWIND_TEST) 
		{
			exitLoop = prepare_for_next_card(&(devNumArr[0]));
			continue;
		}
#endif // _IQV		
		if(isDragon_sd(swDeviceID) && CalSetup.Bmode) {
			uiPrintf("Information: AR6000 does not support B mode.  Disabling this mode\n");
			CalSetup.Bmode = 0;
		}
		
		if(CalSetup.useInstruments && (!REWIND_TEST))
		{
			timestart = milliTime();
#ifndef _IQV
			uiPrintf("\nSetting up Power Meter");
 			devPM = pmInit(CalSetup.pmGPIBaddr, CalSetup.pmModel);
			if ((CalSetup.pmModel == PM_E4416A) && !(SETUP_PM_MODEL2_ONCE))
			{
				gpibWrite(devPM, "*rst\n");
				Sleep(100);
				//gpibWrite(devPM, "*cls;*rcl 7\n");
				pmPreset(devPM, -20, -4e-6, 860e-6, 2.6e-3, 3.43e-3);
				Sleep(100);
				gpibWrite(devPM, "*cls;:sens:det:func aver\n");
				Sleep(100);
				//exit(0);
				SETUP_PM_MODEL2_ONCE = TRUE;
			}


			uiPrintf("\nSetting up Spectrum Analyzer");
			devSA = spaInit(CalSetup.saGPIBaddr, CalSetup.saModel);

			uiPrintf("\nSetting up Attenuator\n");
#if defined(LINUX) || defined(__linux__)
			devATT = attInit(CalSetup.attGPIBaddr, ATT_11713A_110);
#else
			devATT = attInit(CalSetup.attGPIBaddr, CalSetup.attModel);
#endif
			attSet(devATT, 81); //set to max
#endif // _IQV	
			strcpy(testname[testnum],"instrument setup");
			testtime[testnum++] = milliTime() - timestart;
		} 
		
			//uiPrintf("\nManufacturing Test start ...\n");


		if (CalSetup.calPower && CalSetup.do_iq_cal && (((swDeviceID & 0xFF) == 0x14)||((swDeviceID & 0xFF) >= 0x16))) 
		{

			// Set attenuator
			if(CalSetup.useInstruments) 
			{
				attenVal = (A_INT32)(5 - CalSetup.attenDutGolden - (-35));
    			attSet(devATT, attenVal);
			}
			
			Sleep(200);

			if ((CalSetup.Amode) && (CalSetup.calPower) && (!REWIND_TEST)) 
			{
				timestart = milliTime();
 				strcpy(testname[testnum],"IQ_cal for 11a");
				configSetup.eepromLoad = 0;
				art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
								(A_BOOL)configSetup.eepromHeaderLoad, MODE_11A, configSetup.use_init);		
				if (!dut_iq_cal(devNumArr[MODE_11a], MODE_11A, 5260))
					return FALSE;
				testtime[testnum++] = milliTime() - timestart;
			}

			if ((CalSetup.Gmode) && (CalSetup.calPower)  && (!REWIND_TEST)) 
			{
				timestart = milliTime();
 				strcpy(testname[testnum],"IQ_cal for 11g");
				configSetup.eepromLoad = 0;
				art_setResetParams(devNumArr[MODE_11g], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
									(A_BOOL)configSetup.eepromHeaderLoad, MODE_11G, configSetup.use_init);		
				if (!dut_iq_cal(devNumArr[MODE_11g], MODE_11G, 2442))
					return FALSE;
				testtime[testnum++] = milliTime() - timestart;
			}

			if (REWIND_TEST) 
			{
				exitLoop = prepare_for_next_card(&(devNumArr[0]));
				continue;
			}

			sendAck(devNumArr[MODE_11a], "Done with iq_cal", 0, 0, 0, CalSetup.customerDebug);
			if(CalSetup.useInstruments) 
			{
				attSet(devATT, 81);
			}

		} 
		else 
		{
			CalSetup.iqcal_i_corr[MODE_11a] = CalSetup.i_coeff_5G;
			CalSetup.iqcal_q_corr[MODE_11a] = CalSetup.q_coeff_5G;

			CalSetup.iqcal_i_corr[MODE_11g] = CalSetup.i_coeff_2G;
			CalSetup.iqcal_q_corr[MODE_11g] = CalSetup.q_coeff_2G;

			if (CalSetup.do_iq_cal) 
			{
				sendAck(devNumArr[MODE_11a], "Done with iq_cal", 0, 0, 0, CalSetup.customerDebug);
			}
		}			

	   if ((CalSetup.cal_fixed_gain[MODE_11a] != INVALID_FG) && CalSetup.Amode)
		 {
		   if (CalSetup.cal_fixed_gain[MODE_11a] > (pCurrGainLadder->numStepsInLadder-1)) 
			 {
			   uiPrintf("ERROR: Invalid CAL_FIXED_GAIN specified in calsetup.txt for mode ");
			   uiPrintf("11a [%d] : Valid range 0..%d\n", CalSetup.cal_fixed_gain[MODE_11a], 
														  (pCurrGainLadder->numStepsInLadder-1));
				return FALSE;
		   }
		   optGainLadderIndex[MODE_11a] = (pCurrGainLadder->numStepsInLadder -1) - CalSetup.cal_fixed_gain[MODE_11a];
		   uiPrintf("\nusing cal fixed gain for mode 11a from calsetup.txt ... FG%d\n", CalSetup.cal_fixed_gain[MODE_11a]);
		   NEED_GAIN_OPT_FOR_MODE[MODE_11a] = TRUE;
	   } else if (CalSetup.calPower && CalSetup.Amode && (!REWIND_TEST)) {
			
			if ( ((swDeviceID & 0xFF) == 0x12) || ((swDeviceID & 0xFF) == 0x13) || ((swDeviceID & 0xFF) == 0x15) ) 
			{
				tmpVal = art_getFieldForMode(devNumArr[MODE_11a], "rf_rfgain_step", MODE_11A, 0);	
				if(tmpVal == 0x3f) {
					NEED_GAIN_OPT_FOR_MODE[MODE_11a] = TRUE;
				}
			} 
			else if (!isGriffin(swDeviceID) && !isEagle(swDeviceID)) 
			{ // for derby 2.0
				tmpVal = art_getFieldForMode(devNumArr[MODE_11a], "rf_mixvga_ovr", MODE_11A, 0);	
				if(tmpVal == 1) {
					NEED_GAIN_OPT_FOR_MODE[MODE_11a] = TRUE;
				}
			}

			if(NEED_GAIN_OPT_FOR_MODE[MODE_11a] && CalSetup.Amode && CalSetup.useInstruments) {
				uiPrintf("\noptimum fixed gain for mode 11a found to be ... ");
				timestart = milliTime();
 				strcpy(testname[testnum],"Optimum fixed gain for 11a");
				configSetup.eepromLoad = 0;
				art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
						(A_BOOL)configSetup.eepromHeaderLoad, MODE_11A, configSetup.use_init);
				optGainLadderIndex[MODE_11a] = optimal_fixed_gain(devNumArr[MODE_11a], pCurrGainLadder, MODE_11a);
				uiPrintf(" : %d  dB\n", pCurrGainLadder->optStep[optGainLadderIndex[MODE_11a]].stepGain);
				testtime[testnum++] = milliTime() - timestart;
			} else 
			{
				optGainLadderIndex[MODE_11a] = pCurrGainLadder->defaultStepNum;
			}
	   }

	   if ((CalSetup.cal_fixed_gain[MODE_11b] != INVALID_FG) && CalSetup.Bmode) 
		 {
		   if (CalSetup.cal_fixed_gain[MODE_11b] >= pCurrGainLadder->numStepsInLadder) 
			 {
			   uiPrintf("ERROR: Invalid CAL_FIXED_GAIN specified in calsetup.txt for mode ");
			   uiPrintf("11b [%d] : Valid range 0..%d\n", CalSetup.cal_fixed_gain[MODE_11b], 
														  (pCurrGainLadder->numStepsInLadder-1));
				return FALSE;
		   }
		   optGainLadderIndex[MODE_11b] = (pCurrGainLadder->numStepsInLadder -1) - CalSetup.cal_fixed_gain[MODE_11b];
		   NEED_GAIN_OPT_FOR_MODE[MODE_11b] = TRUE;
		   uiPrintf("\nusing cal fixed gain for mode 11b from calsetup.txt ... FG%d\n", CalSetup.cal_fixed_gain[MODE_11b]);
	   } 
		 else  if (CalSetup.calPower && CalSetup.Bmode && (!REWIND_TEST))
		 {
			
			if ( ((swDeviceID & 0xFF) == 0x12) || ((swDeviceID & 0xFF) == 0x13) || ((swDeviceID & 0xFF) == 0x15) ) 
			{
				tmpVal = art_getFieldForMode(devNumArr[MODE_11b], "rf_rfgain_step", MODE_11B, 0);	
				if(tmpVal == 0x3f) 
				{
					NEED_GAIN_OPT_FOR_MODE[MODE_11b] = TRUE;
				}
			} else if (!isGriffin(swDeviceID) && !isEagle(swDeviceID)) { // for derby 2.0
				tmpVal = art_getFieldForMode(devNumArr[MODE_11b], "rf_mixvga_ovr", MODE_11B, 0);	
				if(tmpVal == 1) {
					NEED_GAIN_OPT_FOR_MODE[MODE_11b] = TRUE;
				}
			}
			if(NEED_GAIN_OPT_FOR_MODE[MODE_11b] && CalSetup.Bmode && CalSetup.useInstruments) {
				uiPrintf("\noptimum fixed gain for mode 11b found to be ... ");
				timestart = milliTime();
 				strcpy(testname[testnum],"Optimum fixed gain for 11b");
				configSetup.eepromLoad = 0;
				art_setResetParams(devNumArr[MODE_11b], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
						(A_BOOL)configSetup.eepromHeaderLoad, MODE_11B, configSetup.use_init);
				optGainLadderIndex[MODE_11b] = optimal_fixed_gain(devNumArr[MODE_11b], pCurrGainLadder, MODE_11b);
				uiPrintf(" : %d  dB\n", pCurrGainLadder->optStep[optGainLadderIndex[MODE_11b]].stepGain);
				testtime[testnum++] = milliTime() - timestart;
			} else {
				optGainLadderIndex[MODE_11b] = pCurrGainLadder->defaultStepNum;
			}
	   }

	   if ((CalSetup.cal_fixed_gain[MODE_11g] != INVALID_FG) && CalSetup.Gmode){
		   if (CalSetup.cal_fixed_gain[MODE_11g] >= pCurrGainLadder->numStepsInLadder) {
			   uiPrintf("ERROR: Invalid CAL_FIXED_GAIN specified in calsetup.txt for mode ");
			   uiPrintf("11g [%d] : Valid range 0..%d\n", CalSetup.cal_fixed_gain[MODE_11g], 
														  (pCurrGainLadder->numStepsInLadder-1));
				return FALSE;
		   }
		   optGainLadderIndex[MODE_11g] = (pCurrGainLadder->numStepsInLadder -1) - CalSetup.cal_fixed_gain[MODE_11g];
		   NEED_GAIN_OPT_FOR_MODE[MODE_11g] = TRUE; 
		   uiPrintf("\nusing cal fixed gain for mode 11g from calsetup.txt ... FG%d\n", CalSetup.cal_fixed_gain[MODE_11g]);
	   } else  if (CalSetup.calPower && CalSetup.Gmode && (!REWIND_TEST)){
			if ( ((swDeviceID & 0xFF) == 0x12) || ((swDeviceID & 0xFF) == 0x13) || ((swDeviceID & 0xFF) == 0x15) ) {
				tmpVal = art_getFieldForMode(devNumArr[MODE_11g], "rf_rfgain_step", MODE_11G, 0);	
				if(tmpVal == 0x3f) {
					NEED_GAIN_OPT_FOR_MODE[MODE_11g] = TRUE;
				}
			} else if (!isGriffin(swDeviceID) && !isEagle(swDeviceID)) { // for derby 2.0
				tmpVal = art_getFieldForMode(devNumArr[MODE_11g], "rf_mixvga_ovr", MODE_11G, 0);	
				if(tmpVal == 1) {
					NEED_GAIN_OPT_FOR_MODE[MODE_11g] = TRUE;
				}
			}
			if(NEED_GAIN_OPT_FOR_MODE[MODE_11g] && CalSetup.Gmode && CalSetup.useInstruments) {
				uiPrintf("\noptimum fixed gain for mode 11g found to be ... ");
				timestart = milliTime();
 				strcpy(testname[testnum],"Optimum fixed gain for 11g");
				configSetup.eepromLoad = 0;
				art_setResetParams(devNumArr[MODE_11g], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
						(A_BOOL)configSetup.eepromHeaderLoad, MODE_11G, configSetup.use_init);
				optGainLadderIndex[MODE_11g] = optimal_fixed_gain(devNumArr[MODE_11g], pCurrGainLadder, MODE_11g);
				uiPrintf(" : %d  dB\n", pCurrGainLadder->optStep[optGainLadderIndex[MODE_11g]].stepGain);
				testtime[testnum++] = milliTime() - timestart;
			} else {
				optGainLadderIndex[MODE_11g] = pCurrGainLadder->defaultStepNum;
			}
	   }
	   
		if(CalSetup.Amode && CalSetup.useInstruments && (CalSetup.testTempMargin[MODE_11a]) && (!REWIND_TEST)) {
			uiPrintf("\nTesting temp margin for 11a...\n");
			timestart = milliTime();
 			strcpy(testname[testnum],"Temp margin test 11a");
			//cornerCal(devNum);
			configSetup.eepromLoad = 0;
			art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, MODE_11A, configSetup.use_init);		
			test_for_temp_margin(devNumArr[MODE_11a], MODE_11a);
			
			testtime[testnum++] = milliTime() - timestart;
		}

		if(CalSetup.Bmode && CalSetup.useInstruments && (CalSetup.testTempMargin[MODE_11b]) && (!REWIND_TEST)) {
			uiPrintf("\nTesting temp margin for 11b...\n");
			timestart = milliTime();
 			strcpy(testname[testnum],"Temp margin test 11b");
			//cornerCal(devNum);
			configSetup.eepromLoad = 0;
			art_setResetParams(devNumArr[MODE_11b], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, MODE_11B, configSetup.use_init);		
	
			test_for_temp_margin(devNumArr[MODE_11b], MODE_11b);
			testtime[testnum++] = milliTime() - timestart;
		}

		if(CalSetup.Gmode && CalSetup.useInstruments && (CalSetup.testTempMargin[MODE_11g]) && (!REWIND_TEST)) {
			uiPrintf("\nTesting temp margin for 11g...\n");
			timestart = milliTime();
 			strcpy(testname[testnum],"Temp margin test 11g");
			//cornerCal(devNum);
			configSetup.eepromLoad = 0;
			art_setResetParams(devNumArr[MODE_11g], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, MODE_11G, configSetup.use_init);		
			test_for_temp_margin(devNumArr[MODE_11g], MODE_11g);			
			testtime[testnum++] = milliTime() - timestart;
		}

		if(CalSetup.useInstruments) {
		    attSet(devATT, 81);
		}

		uiPrintf("\nManufacturing Test start ...\n");

		if (CalSetup.useFastCal && CalSetup.calPower && CalSetup.useInstruments)
		{
			gpibONL(guDevAtt, 0);	// take att offline. relinquish for GU.
			gpibONL(guDevPM, 0);	// take att offline. relinquish for GU.
			sendAck(devNumArr[MODE_11a], "DUT relinquished instrument control. Go ahead GU.", 0, 0, 0, CalSetup.customerDebug);
		} 

		// 11a calibration
        if((CalSetup.calPower && CalSetup.Amode) && (!REWIND_TEST)){
			timestart = milliTime();
			strcpy(testname[testnum],"11a cal");
			configSetup.eepromLoad = 0;
			art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, MODE_11A, configSetup.use_init);		
			if (NEED_GAIN_OPT_FOR_MODE[MODE_11a])
			{
				//setGainLadderForMaxGain(pCurrGainLadder);
				setGainLadderForIndex(pCurrGainLadder, optGainLadderIndex[MODE_11a]);
				programNewGain(pCurrGainLadder, devNumArr[MODE_11a], 0);
			}
			if (!dutCalibration(devNumArr[MODE_11a])) {
				exitLoop = TRUE;
				return FALSE;
			}
#ifdef _IQV
			if (ErrCode==0) {
				exitLoop = TRUE;
				return FALSE;
			}
#endif
			initializeGainLadder(pCurrGainLadder);
			programNewGain(pCurrGainLadder, devNumArr[MODE_11a], 0);
			testtime[testnum++] = milliTime() - timestart;
        } 

		// 11g calibration
		if(CalSetup.calPower && CalSetup.Gmode && (!REWIND_TEST)){
			configSetup.eepromLoad = 0;
			art_setResetParams(devNumArr[MODE_11g], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, MODE_11G, configSetup.use_init);		
 			timestart = milliTime();
			strcpy(testname[testnum],"11g cal");			
			if (NEED_GAIN_OPT_FOR_MODE[MODE_11g])
			{
				//initializeGainLadder(pCurrGainLadder);
				//setGainLadderForMaxGain(pCurrGainLadder); 
				setGainLadderForIndex(pCurrGainLadder, optGainLadderIndex[MODE_11g]);
				programNewGain(pCurrGainLadder, devNumArr[MODE_11g], 0);
			}
			if (!dutCalibration_2p4(devNumArr[MODE_11g], MODE_11g)) 
				return FALSE;
#ifdef _IQV
			if (ErrCode==0) {
				exitLoop = TRUE;
				return FALSE;
			}
#endif
			initializeGainLadder(pCurrGainLadder);
			programNewGain(pCurrGainLadder, devNumArr[MODE_11g], 0);
 			testtime[testnum++] = milliTime() - timestart;
       } 

		// 11b calibration
		if(CalSetup.calPower && CalSetup.Bmode && (!REWIND_TEST)){
			configSetup.eepromLoad = 0;
			art_setResetParams(devNumArr[MODE_11b], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, MODE_11B, configSetup.use_init);		
			timestart = milliTime();
			strcpy(testname[testnum],"11b cal");
			if (NEED_GAIN_OPT_FOR_MODE[MODE_11b])
			{
				//initializeGainLadder(pCurrGainLadder);
				//setGainLadderForMaxGain(pCurrGainLadder);
				setGainLadderForIndex(pCurrGainLadder, optGainLadderIndex[MODE_11b]);
				programNewGain(pCurrGainLadder, devNumArr[MODE_11b], 0);
			}
            if (!dutCalibration_2p4(devNumArr[MODE_11b], MODE_11b))
				return FALSE;
			initializeGainLadder(pCurrGainLadder);
			programNewGain(pCurrGainLadder, devNumArr[MODE_11b], 0);
			testtime[testnum++] = milliTime() - timestart;
        } 

		if(CalSetup.calPower && (!REWIND_TEST)) 
		{
			timestart = milliTime();
			strcpy(testname[testnum],"program eeprom");
			configSetup.eepromLoad = 0;
			//force the reload of eep file to ensure correct settings programmed into eeprom
			processEepFile(devNumArr[MODE_11a], configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion);
			processEepFile(devNumArr[MODE_11g], configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion);
			if (CalSetup.eeprom_map == CAL_FORMAT_GEN2) {
				CalSetup.TrgtPwrStartAddr = 0x1A5;
			}
			// ear_start_addr now points to the end of cal data. adjust it for target powers :
			// (8 11a ch + 211b ch + 3 11g ch)*2 words per ch
			CalSetup.EARStartAddr = CalSetup.TrgtPwrStartAddr + 16 + 4 + 6 + 8*pTestGroupSet->numTestGroupsDefined;
//			if(CalSetup.eeprom_map == CAL_FORMAT_GEN5) {
//				//add extra dummy bytes to calculation of ear start
//				CalSetup.EARStartAddr += NUM_DUMMY_EEP_MAP1_LOCATIONS; 
//			}
#ifdef _IQV
			if (ErrCode!=-1)
#endif
			program_eeprom((configSetup.validInstance == 1) ? devNumArr[MODE_11a] : devNumArr[MODE_11g]);  
#ifdef _IQV
			else 
				printf("	power cal failed, skip eeprom writting");
#endif
			testtime[testnum++] = milliTime() - timestart;
		}

		if(CalSetup.reprogramTargetPwr && (!REWIND_TEST)) {			
			program_Target_Power_and_Test_Groups(devNumArr[MODE_11a]);
		}

// Code added to enable dual 11a cal for ap30 :
//    + EEPROM_DATA for EEPBLK0 should already be complete at this point. 
//    + copy that as baseline for EEPROM_DATA for EEPBLK1 	
//    + modify only the cal sections for the modes that need to be modified
//    + all other information is duplicated in both EEPBLKs
		
		// dual concurrent 11a calibration
       if( configSetup.remote &&
			((CalSetup.modeMaskForRadio[0] & 0x2) == 0x2) &&
			((CalSetup.modeMaskForRadio[1] & 0x2) == 0x2) &&  // dual 11a supported
			(CalSetup.calPower && CalSetup.Amode) && (!REWIND_TEST)){
				timestart = milliTime();
				strcpy(testname[testnum],"dual 11a cal");
				configSetup.eepromLoad = 0;
				// assumption : 2nd 11a devnum should be the same as the 11g devNum. true for freedom gen chips
				dual_11a_devNum = devNumArr[MODE_11g];
		
					art_setResetParams(dual_11a_devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, MODE_11A, configSetup.use_init);		
				if (NEED_GAIN_OPT_FOR_MODE[MODE_11a])
				{
						//setGainLadderForMaxGain(pCurrGainLadder);
						setGainLadderForIndex(pCurrGainLadder, optGainLadderIndex[MODE_11g]);
						programNewGain(pCurrGainLadder, dual_11a_devNum, 0);
				}
				if (!dutCalibration(dual_11a_devNum))
					return FALSE;
				initializeGainLadder(pCurrGainLadder);
				programNewGain(pCurrGainLadder, dual_11a_devNum, 0);
				testtime[testnum++] = milliTime() - timestart;
				program_eeprom_block1(dual_11a_devNum);
      } 


		if (CalSetup.useFastCal && CalSetup.calPower && CalSetup.useInstruments && (!REWIND_TEST))
		{
				
			sendAck(devNumArr[MODE_11a], "Request Golden Unit To Relinquish Instrument Control", 0, 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			gpibRSC(0, 1);			// DUT to acquire system control (1);

			uiPrintf("\nSetting up Power Meter");
 			devPM = pmInit(CalSetup.pmGPIBaddr, CalSetup.pmModel);

			uiPrintf("\nSetting up Spectrum Analyzer");
			devSA = spaInit(CalSetup.saGPIBaddr, CalSetup.saModel);

			uiPrintf("\nSetting up Attenuator");
			devATT = attInit(CalSetup.attGPIBaddr, CalSetup.attModel);
			attSet(devATT, 81); //set to max

		}

//#ifdef _IQV
//		if (ErrCode==1 && testType != ART_CAL) {
//#endif
		if (!REWIND_TEST) {
#ifndef _IQV
			configSetup.loadEar = loadEARState;
			//art_configureLibParams(devNumArr[MODE_11a]);
			//art_resetDevice(devNumArr[MODE_11a], rxStation, bssID, 5220, 0);	

			art_configureLibParams(devNumArr[MODE_11g]);
                        art_setResetParams(devNumArr[MODE_11g], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
                            (A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);
			art_resetDevice(devNumArr[MODE_11g], rxStation, bssID, 2412, 0);

			art_getDeviceInfo(devNumArr[MODE_11a], &devStruct);
			swDeviceID = devStruct.swDevID;
#endif	//	_IQV
			if (!dutTest(&(devNumArr[0])))
				return FALSE;
		}
//#ifdef _IQV
//		}	// if (ErrCode==1 && testType != ART_CAL) {
//#endif


#ifndef _IQV			
		exitLoop = prepare_for_next_card(&(devNumArr[0]));
#else			
		exitLoop = TRUE;
#endif // _IQV
	}
	return TRUE;
}

A_UINT16 dutCalibration(A_UINT32 devNum)
{
	if(!isEagle(swDeviceID)) {
		art_changeField(devNum, "rf_ovr", 0);
	}

	if (CalSetup.eeprom_map == CAL_FORMAT_GEN3) {
		dutCalibration_gen3(devNum, MODE_11a);
		return TRUE;
	}

	if (CalSetup.eeprom_map >= CAL_FORMAT_GEN5) {
		if (!dutCalibration_gen5(devNum, MODE_11a))
			return FALSE;
		return TRUE;
	}

    // reset device in order to read eeprom content
	if (!art_resetDevice(devNum, rxStation, bssID, 5220, 0))
		return FALSE;

	// power and gainf datasets setup and measurement
	if (!setup_raw_datasets()) { 
		uiPrintf("Could not setup raw datasets. Exiting...\n");
        closeEnvironment();
        return FALSE;
	}

	if (CalSetup.readFromFile)
	{
		read_dataset_from_file(pRawDataset, CalSetup.rawDataFilename);
	} else if (CalSetup.forcePiers)
	{
		if (!getCalData(devNum, MODE_11a, CalSetup.customerDebug))
			return FALSE;
		dump_rectangular_grid_to_file(pRawDataset, "force_piers_power.log") ;
		if (CalSetup.customerDebug)
			dump_rectangular_grid_to_file(pRawGainDataset, "force_piers_gainf.log") ;
	} else
	{
		if (!getCalData(devNum, MODE_11a, CalSetup.customerDebug))
			return FALSE;
		dump_rectangular_grid_to_file(pRawDataset, "cal_AR5211_power.log") ;
		if (CalSetup.customerDebug)
			dump_rectangular_grid_to_file(pRawGainDataset, "cal_AR5211_gainf.log") ;
	}

	make_cal_dataset_from_raw_dataset() ;

	dMapGrid(pCalDataset, pFullDataset);

	if (CalSetup.customerDebug)
		dump_rectangular_grid_to_file(pFullDataset, "junkff.log");
	return TRUE;
}



void setupChannelLists()
{
	A_UINT16 lower_lab_channel, upper_lab_channel ;
	A_UINT16 ii,jj;

	lower_lab_channel = (A_UINT16) 20*(goldenParams.channelStart/20) ;
	if (lower_lab_channel < goldenParams.channelStart)
		lower_lab_channel += 20;

	upper_lab_channel = (A_UINT16) 20*(goldenParams.channelStop/20) ;


	jj=0;
	for(ii=goldenParams.channelStart; ii<=goldenParams.channelStop; ii+=goldenParams.measurementStep )
	{
		RAW_CHAN_LIST[jj] = ii;
		jj++;
	}

	if (ii < goldenParams.channelStop)
	{
		ii+=goldenParams.measurementStep ; 
		RAW_CHAN_LIST[jj] = ii;
		jj++;
	} // measurement channels need to include the channelStop

	numRAWChannels = jj;
}
		   

// Write Common Data to eeprom

A_BOOL setup_raw_datasets()
{

	A_UINT16  numPcdacs;
	A_UINT16 i, j, channelValue;
	dPCDACS_EEPROM *pEepromStruct;
	A_UINT16  myNumRawChannels; // needed to handle forced piers case.
	A_UINT16 *pMyRawChanList ;

	// handle forcePierList case here
	if(CalSetup.forcePiers && !CalSetup.readFromFile) // 'cause read from file supercedes
	{
		myNumRawChannels = (A_UINT16) CalSetup.numForcedPiers;
		pMyRawChanList = CalSetup.piersList ;
	} else
	{
		myNumRawChannels = numRAWChannels;
		pMyRawChanList = RAW_CHAN_LIST ;
	}

	numPcdacs = sizeOfRawPcdacs;

	if(!d_allocateEepromStruct( pRawDataset, myNumRawChannels, numPcdacs )) {
		uiPrintf("unable to allocate eeprom struct RawDataset for full struct\n");
		return(0);
	}

	if(!d_allocateEepromStruct( pRawGainDataset, myNumRawChannels, numPcdacs )) {
		uiPrintf("unable to allocate eeprom struct RawGainDataset for full struct\n");
		return(0);
	}

	//now fill in the channel list and pcdac lists
	for (j=0; j<2; j++) 
	{
		pEepromStruct = pRawDataset;
		if (j > 0) 
			pEepromStruct = pRawGainDataset;

		for (i = 0; i < myNumRawChannels; i++) {
			channelValue = pMyRawChanList[i];
	
			pEepromStruct->hadIdenticalPcdacs = TRUE;
			pEepromStruct->pChannels[i] = channelValue;
			
			pEepromStruct->pDataPerChannel[i].channelValue = channelValue;
			pEepromStruct->pDataPerChannel[i].numPcdacValues = numPcdacs ;
			memcpy(pEepromStruct->pDataPerChannel[i].pPcdacValues, RAW_PCDACS, numPcdacs*sizeof(A_UINT16));

			pEepromStruct->pDataPerChannel[i].pcdacMin = pEepromStruct->pDataPerChannel[i].pPcdacValues[0];
			pEepromStruct->pDataPerChannel[i].pcdacMax = pEepromStruct->pDataPerChannel[i].pPcdacValues[numPcdacs - 1];
	
		}
	}

	return(1);

}

A_BOOL	d_allocateEepromStruct(dPCDACS_EEPROM  *pEepromStruct, A_UINT16	numChannels, A_UINT16 numPcdacs )
{
	A_UINT16	i, j;

	///allocate room for the channels
	pEepromStruct->pChannels = (A_UINT16 *)malloc(sizeof(A_UINT16) * numChannels);
	if (NULL == pEepromStruct->pChannels) {
		uiPrintf("unable to allocate eeprom struct\n");
		return(0);
	}

	pEepromStruct->pDataPerChannel = (dDATA_PER_CHANNEL *)malloc(sizeof(dDATA_PER_CHANNEL) * numChannels);
	if (NULL == pEepromStruct->pDataPerChannel) {
		uiPrintf("unable to allocate eeprom struct\n");
		free(pEepromStruct->pChannels);
		return(0);
	}
	
	pEepromStruct->numChannels = numChannels;

	for(i = 0; i < numChannels; i ++) {
		pEepromStruct->pDataPerChannel[i].pPcdacValues = NULL;
		pEepromStruct->pDataPerChannel[i].pPwrValues = NULL;
		pEepromStruct->pDataPerChannel[i].numPcdacValues = numPcdacs;
		
		pEepromStruct->pDataPerChannel[i].pPcdacValues = (A_UINT16 *)malloc(sizeof(A_UINT16) * numPcdacs);
		if (NULL == pEepromStruct->pDataPerChannel[i].pPcdacValues) {
			uiPrintf("unable to allocate eeprom struct\n");
			break; //will cleanup outside loop
		}

		pEepromStruct->pDataPerChannel[i].pPwrValues = (double *)malloc(sizeof(double) * numPcdacs);
		if (NULL == pEepromStruct->pDataPerChannel[i].pPwrValues) {
			uiPrintf("unable to allocate eeprom struct\n");
			break; //will cleanup outside loop
		}

	}

	if (i != numChannels) {
		//malloc must have failed, cleanup any allocs
		for (j = 0; j < i; j++) {
			if (pEepromStruct->pDataPerChannel[j].pPcdacValues != NULL) {
				free(pEepromStruct->pDataPerChannel[j].pPcdacValues);
			}
			if (pEepromStruct->pDataPerChannel[j].pPwrValues != NULL) {
				free(pEepromStruct->pDataPerChannel[j].pPwrValues);
			}
		}

		uiPrintf("Failed to allocate eeprom struct, freeing anything already allocated\n");
		free(pEepromStruct->pDataPerChannel);
		free(pEepromStruct->pChannels);
		pEepromStruct->numChannels = 0;
		return(0);
	}
	return(1);
}

A_BOOL	d_freeEepromStruct(dPCDACS_EEPROM  *pEepromStruct)
{
	A_UINT16	j;
	A_UINT16	numChannels;

	
	numChannels = pEepromStruct->numChannels;


	for (j = 0; j < numChannels; j++) {
		if (pEepromStruct->pDataPerChannel[j].pPcdacValues != NULL) {
			free(pEepromStruct->pDataPerChannel[j].pPcdacValues);
		}
		if (pEepromStruct->pDataPerChannel[j].pPwrValues != NULL) {
			free(pEepromStruct->pDataPerChannel[j].pPwrValues);
		}
	}

	free(pEepromStruct->pDataPerChannel);
	free(pEepromStruct->pChannels);
//	free(pEepromStruct); 
	return(1);
}

A_UINT16 measure_all_channels(A_UINT32 devNum, A_UINT16 debug) 
{
	A_UINT16 fill_zero_pcdac = 40 ;
	A_BOOL fillZeros ;
	A_BOOL fillMaxPwr;
	A_UINT16 numPcdacs ;
	A_INT16 i;
	A_UINT16 *reordered_pcdacs_index ;
	A_UINT16 rr = 0; //reordered_pcdacs_index index
	A_UINT16 channel;
	A_UINT16  pcdac ;
	A_UINT16 reset = 0;
	double power;
	
	dDATA_PER_CHANNEL	*pRawChannelData;
	double				*pRawPwrValue;

	dDATA_PER_CHANNEL	*pGainfChannelData;
	double				*pGainfValue;

	uiPrintf("\nCollecting raw data for the adapter for mode 11a\n");
	numPcdacs = (A_UINT16) sizeOfRawPcdacs ;
	reordered_pcdacs_index = (A_UINT16 *)malloc(sizeof(A_UINT16) * numPcdacs) ;
	if (NULL == reordered_pcdacs_index) 
		uiPrintf("Could not allocate memory for the reordered_pcdacs_index array.\n");

	for (i=numPcdacs-1; i>=0; i--)
	{
		if (RAW_PCDACS[i] <= fill_zero_pcdac)
		{
			reordered_pcdacs_index[rr] = i;
			rr++;
		}
	}

	for (i=0; i<numPcdacs; i++)
	{
		if (RAW_PCDACS[i] > fill_zero_pcdac)
		{
			reordered_pcdacs_index[rr] = i;
			rr++;
		}
	}

	if (rr != numPcdacs)
	{
		uiPrintf("Couldn't reorder pcdacs.\n");
		return FALSE;
	}


	if(debug)
	{
		uiPrintf("Reordered pcdacs are: ");
		for (i=0; i<numPcdacs; i++)
		{
			uiPrintf("%d, ", RAW_PCDACS[reordered_pcdacs_index[i]]);
		}
		uiPrintf("\n");
	}

	for (i=0; i<pRawDataset->numChannels; i++)
	{
		channel = pRawDataset->pDataPerChannel[i].channelValue;
		pRawChannelData = &(pRawDataset->pDataPerChannel[i]) ;
		pGainfChannelData = &(pRawGainDataset->pDataPerChannel[i]) ;
				
		//setCornerFixBits(devNum, channel);

		if (i == 0)
		{
			if (!art_resetDevice(devNum, txStation, NullID, channel, 0))
				return FALSE;
		} else
		{
//			art_resetDevice(devNum, txStation, NullID, channel, 0); 
			art_changeChannel(devNum, channel); //SNOOP: get this in eventually for efficiency
		}

	
		art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
					    rates[0], DESC_ANT_A | USE_DESC_ANT);

		reset = 0; //1;
		if(CalSetup.useInstruments) 
		{	
			power = pmMeasAvgPower(devPM, reset) ;
		} else
		{
			power = 0;
		}
		reset = 0;

//		Sleep(20); // sleep 20 ms
		Sleep(50); // sleep 150 ms - based on feedback b.
		fillZeros = FALSE ;
		fillMaxPwr = FALSE ;

		for(rr=0; rr<numPcdacs; rr++)
		{
			pcdac = RAW_PCDACS[reordered_pcdacs_index[rr]] ;
			pRawPwrValue = &(pRawChannelData->pPwrValues[reordered_pcdacs_index[rr]]) ;
			pGainfValue  = &(pGainfChannelData->pPwrValues[reordered_pcdacs_index[rr]]) ;
			
			if (CalSetup.xpd < 1)
			{
				art_txContEnd(devNum);
				if (!art_resetDevice(devNum, txStation, NullID, channel, 0))
					return FALSE;
				art_ForceSinglePCDACTable(devNum, (A_UCHAR) pcdac);
				art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
							    rates[0], DESC_ANT_A | USE_DESC_ANT);
			} else
			{
				art_ForceSinglePCDACTable(devNum, pcdac);
			}

			if (pcdac > fill_zero_pcdac) 
				fillZeros = FALSE ;

			if (pcdac < fill_zero_pcdac) 
				fillMaxPwr = FALSE ;

			if (fillZeros)
			{
				*pRawPwrValue = 0;
				*pGainfValue  = 0;
				continue ;
			}

			if (fillMaxPwr)
			{
				*pRawPwrValue = CalSetup.maxPowerCap[MODE_11a];
				*pGainfValue  = 110;
				continue ;
			}

			Sleep(50) ;

			if(CalSetup.useInstruments) 
			{	
				power = pmMeasAvgPower(devPM, reset) ;
			} else
			{
				power = 0;
			}

			*pRawPwrValue = (power + CalSetup.attenDutPM) ;
			if (CalSetup.customerDebug)
			{
				art_txContEnd(devNum);
				//Sleep(100);
				*pGainfValue  = (double) dump_a2_pc_out(devNum) ;
				art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
								rates[0], DESC_ANT_A | USE_DESC_ANT);

			} else
			{
				*pGainfValue = (double) 0 ;
			}

			if ( *pRawPwrValue <= 0 )
				fillZeros = TRUE ;

			if ( *pRawPwrValue >= CalSetup.maxPowerCap[MODE_11a] )
				fillMaxPwr = TRUE ;

			if (debug)
			{
				uiPrintf("  power at pcdac=%d is %2.1f + %2.1f = %2.1f\n", pcdac, power, CalSetup.attenDutPM, *pRawPwrValue);
			}
		}

		uiPrintf("ch: %d  --> max pwr is %2.1f dBm\n", channel, *pRawPwrValue) ;

		art_txContEnd(devNum) ;
	}

	free(reordered_pcdacs_index);
	return TRUE;
}



void dump_rectangular_grid_to_file( dPCDACS_EEPROM  *pEepromData, char *filename )
{
	A_UINT16	i, j;
	dDATA_PER_CHANNEL	*pChannelData;
    FILE *fStream;
 
	if (CalSetup.customerDebug)
		uiPrintf("\nWriting to file %s\n", filename);
	
    if( (fStream = fopen( filename, "w")) == NULL ) {
        uiPrintf("Failed to open %s - using Defaults\n", filename);
        return;
    }

	//print the frequency values
	fprintf(fStream, "  \t");
	for (i = 0; i < pEepromData->numChannels; i++) {
		fprintf(fStream,"%d\t", pEepromData->pChannels[i]);
	}
	fprintf(fStream,"\n");

	//print the pcdac values for each frequency
	for(j = 0; j < pEepromData->pDataPerChannel[0].numPcdacValues ; j++) {
		fprintf(fStream,"%d\t", pEepromData->pDataPerChannel[0].pPcdacValues[j]);
		for(i = 0; i < pEepromData->numChannels; i++) {
			pChannelData = &(pEepromData->pDataPerChannel[i]);
			//fprintf(fStream,"%2.2f\t", pChannelData->pPwrValues[j]);
			fprintf(fStream,"%f\t", pChannelData->pPwrValues[j]);
		}
		fprintf(fStream,"\n");
	}

	fclose(fStream);
}

void dump_nonrectangular_grid_to_file( dPCDACS_EEPROM  *pEepromData, char *filename )
{
	A_UINT16	i, j;
    FILE *fStream;
 
    if (CalSetup.customerDebug)
		uiPrintf("\nWriting to file %s\n", filename);

    if( (fStream = fopen( filename, "w")) == NULL ) {
        uiPrintf("Failed to open %s - using Defaults\n", filename);
        return;
    }

	//print the frequency values
	fprintf(fStream, "  \t");
	for (i = 0; i < pEepromData->numChannels; i++) {
		fprintf(fStream,"%d: pcdacMin=%d, pcdacMax=%d\n", pEepromData->pChannels[i],
														  pEepromData->pDataPerChannel[i].pcdacMin,
														  pEepromData->pDataPerChannel[i].pcdacMax);
		//print the pcdac values for each frequency
		for(j = 0; j < pEepromData->pDataPerChannel[0].numPcdacValues ; j++) {
			fprintf(fStream,"%d: %2.1f\n", pEepromData->pDataPerChannel[i].pPcdacValues[j],
										   pEepromData->pDataPerChannel[i].pPwrValues[j]);
		}
		fprintf(fStream, "\n\n");
	}
	fclose(fStream);
}

void make_cal_dataset_from_raw_dataset()
{
	A_UINT16 numPiers ;
	A_UINT16 i ;
	A_UINT16 channelValue;
	A_UINT16 numAllChannels, numAllPcdacs;
	A_UINT16 stepAllChannels = 5;
	A_UINT16 *AllPcdacs ;
	A_UINT16 numPcdacs;
	A_UINT16 *turning_points ;

	A_UINT16 filter_size = 2;
	A_UINT16 data_ceiling = 30 ; // max dB in raw measured data
	A_UINT16 debug = CalSetup.customerDebug;

	dPCDACS_EEPROM SnapshotDataset;
	dPCDACS_EEPROM *pSnapshotDataset = &SnapshotDataset;


	numPcdacs = sizeOfRawPcdacs;

	if ( eepromType == EEPROM_SIZE_16K ) 
	{
		numPiers = NUM_PIERS ;
	}

	if(!d_allocateEepromStruct( pCalDataset, numPiers, goldenParams.numIntercepts  )) {
		uiPrintf("unable to allocate eeprom struct pCalDataset. Exiting...\n");
		exit(0);
	}

	pCalDataset->hadIdenticalPcdacs = FALSE ;

	
	numAllChannels = (goldenParams.channelStop - goldenParams.channelStart)/stepAllChannels + 1;
	numAllPcdacs = (goldenParams.pcdacStop - goldenParams.pcdacStart)/goldenParams.pcdacStep + 1;
	if(!d_allocateEepromStruct( pFullDataset, numAllChannels, numAllPcdacs )) {
		uiPrintf("unable to allocate eeprom struct pFullDataset. Exiting...\n");
		exit(0);
	}

	pFullDataset->hadIdenticalPcdacs = TRUE ;

	AllPcdacs = (A_UINT16 *) malloc(numAllPcdacs*sizeof(A_UINT16)) ;
	for (i = 0; i < numAllPcdacs; i++) {
		AllPcdacs[i] = goldenParams.pcdacStart + i*goldenParams.pcdacStep ;
	}

	for (i = 0; i < numAllChannels; i++) {
		channelValue = goldenParams.channelStart + i*stepAllChannels;
		pFullDataset->pChannels[i] = channelValue;
			
		pFullDataset->pDataPerChannel[i].channelValue = channelValue;
		pFullDataset->pDataPerChannel[i].numPcdacValues = numAllPcdacs ;
		memcpy(pFullDataset->pDataPerChannel[i].pPcdacValues, AllPcdacs, numAllPcdacs*sizeof(A_UINT16));

		pFullDataset->pDataPerChannel[i].pcdacMin = pFullDataset->pDataPerChannel[i].pPcdacValues[0];
		pFullDataset->pDataPerChannel[i].pcdacMax = pFullDataset->pDataPerChannel[i].pPcdacValues[numPcdacs - 1];
	}

	turning_points = (A_UINT16 *) malloc(numRAWChannels * sizeof(A_UINT16) ) ;
	if (NULL == turning_points) 
	{
		uiPrintf("Could not allocate space for turning points\n");
		exit(0);
	}

	if(CalSetup.forcePiers && !CalSetup.readFromFile) 
	{
		if(CalSetup.numForcedPiers > numPiers)
		{
			uiPrintf("ERROR: %d piers specified in FORCE_PIERS_LIST in calsetup.txt\n", CalSetup.numForcedPiers);
			uiPrintf("A maximum of %d are allowed.\n", numPiers);
			exit(0);
		}
		for (i=0; i<CalSetup.numForcedPiers; i++)
		{
			turning_points[i] = (A_UINT16) CalSetup.piersList[i];
		}
		while (i < numPiers)
		{
			turning_points[i++] = (A_UINT16) CalSetup.piersList[CalSetup.numForcedPiers-1];
		}
		truncate_hash_subzero(pRawDataset) ;
	} else 
	{
		find_optimal_pier_locations(turning_points, numPiers, filter_size, data_ceiling, debug);
	}
	


	if(!d_allocateEepromStruct( &SnapshotDataset, numPiers, numPcdacs  )) {
		uiPrintf("unable to allocate eeprom struct pSnapshotDataset. Exiting...\n");
		exit(0);
	}

	pSnapshotDataset->hadIdenticalPcdacs = TRUE ;

	for (i = 0; i < numPiers; i++) {
		channelValue = turning_points[i];
	
		pSnapshotDataset->pChannels[i] = channelValue;
			
		pSnapshotDataset->pDataPerChannel[i].channelValue = channelValue;
		memcpy(pSnapshotDataset->pDataPerChannel[i].pPcdacValues, RAW_PCDACS, sizeOfRawPcdacs );

		pSnapshotDataset->pDataPerChannel[i].pcdacMin = pSnapshotDataset->pDataPerChannel[i].pPcdacValues[0];
		pSnapshotDataset->pDataPerChannel[i].pcdacMax = pSnapshotDataset->pDataPerChannel[i].pPcdacValues[numAllPcdacs - 1];
	}

	dMapGrid(pRawDataset, pSnapshotDataset);
	if (CalSetup.customerDebug)
		dump_rectangular_grid_to_file(pSnapshotDataset, "junkcal.log");

	build_cal_dataset_skeleton(pRawDataset, pCalDataset, goldenParams.pInterceptPercentages, 
								goldenParams.numIntercepts, turning_points, numPiers);
	dMapGrid(pRawDataset, pCalDataset);

	quantize_hash(pCalDataset);
	if(CalSetup.customerDebug) {
		dump_nonrectangular_grid_to_file(pCalDataset, "junkToee.log");
	}

	d_freeEepromStruct(pSnapshotDataset);
	free(AllPcdacs);
	free(turning_points);
}

void find_optimal_pier_locations(A_UINT16 *turning_points, A_UINT16 numTPsToPick, double filter_size, A_UINT16 data_ceiling, A_UINT16 debug)
{
	A_UINT32 filter_iterations = 1;
	dPCDACS_EEPROM deriv1Struct;
	dPCDACS_EEPROM deriv2Struct ;
	dPCDACS_EEPROM *deriv1 = &deriv1Struct;
	dPCDACS_EEPROM *deriv2 = &deriv2Struct;

	A_UINT16 *allTPs;
	A_UINT16 numAllTPs ;
	A_UINT16 i;
	TP_VAL tp_totempole_Struct;
	TP_VAL *tp_totempole = &tp_totempole_Struct;

	for (i=0; i<filter_iterations; i++)
	{
		filter_hash(pRawDataset, filter_size, data_ceiling);
	}

	truncate_hash_subzero(pRawDataset) ;
	if (CalSetup.customerDebug)
		dump_rectangular_grid_to_file(pRawDataset, "junkg.log");

	if (!d_allocateEepromStruct(deriv1, pRawDataset->numChannels, pRawDataset->pDataPerChannel[0].numPcdacValues))
	{
		uiPrintf("Could not allocate deriv1 data structure. Exiting...\n");
		exit(0);
	}
	differentiate_hash(pRawDataset, deriv1);
	if (CalSetup.customerDebug)
		dump_rectangular_grid_to_file(deriv1, "junks.log");

	if (!d_allocateEepromStruct(deriv2, pRawDataset->numChannels, pRawDataset->pDataPerChannel[0].numPcdacValues))
	{
		uiPrintf("Could not allocate deriv2 data structure. Exiting...\n");
		exit(0);
	}
	differentiate_hash(deriv1, deriv2);
	if (CalSetup.customerDebug)
		dump_rectangular_grid_to_file(deriv2, "junkss.log");

	allTPs = (A_UINT16 *) malloc(numRAWChannels * sizeof(A_UINT16) ) ;
	if (NULL == allTPs) 
	{
		uiPrintf("Could not allocate space for allTPs \n");
		exit(0);
	}

	build_turning_points(pRawDataset, deriv2, allTPs, &(numAllTPs), debug);

	consolidate_turning_points(&(allTPs), &(numAllTPs), debug);
	// allTPs now points to the consolidated TPs and the numAllTPs has also been adjusted.

	if (debug)
	{
		uiPrintf("Consolidated turning points are: ");
		for (i=0; i<numAllTPs; i++)
		{
			uiPrintf("%d, ", allTPs[i]);
		}
		uiPrintf("\n");
	}

	build_turningpoint_totempole(pRawDataset, &(tp_totempole), numAllTPs, allTPs, debug);

	qsort( (void *) tp_totempole, numAllTPs, sizeof(TP_VAL), totempole_compare) ;

	for (i=0; i<numTPsToPick; i++)
	{
		if (i < numAllTPs) {
			turning_points[i] = pRawDataset->pChannels[tp_totempole[i].channel_idx] ;
		} else {
			turning_points[i] = pRawDataset->pChannels[tp_totempole[0].channel_idx] ;
		}
	}

	qsort((void *)turning_points, numTPsToPick, sizeof(A_UINT16), numerical_compare);

	d_freeEepromStruct(deriv1);
	d_freeEepromStruct(deriv2);
	free(allTPs);
	free(tp_totempole);
}

// ALGORITHM : the purpose of this routine is to filter SINGLE POINT SPIKES only.
// 1. if you've filtered the previous point - ignore the current one.
// 2. if currval > upperbound  then filter currval
// 3. if currval deviates from neighbors mean by more than the filterSize BUT the neighbors themselves
//    are within the filterSize, then filter currval.
//
// NOTE: this algorithm is NOT designed to handle extended set of deviant points.
//
void filter_hash(dPCDACS_EEPROM *pDataset, double filter_size, double data_ceiling)
{
	A_UINT16 pcd, ii, jj,  freq ;
	double avgii, diffii;
	A_UINT16 filteredOne = 0;
	dDATA_PER_CHANNEL *prevChannel ;
	dDATA_PER_CHANNEL *currChannel ;
	dDATA_PER_CHANNEL *nextChannel ;

	for (jj=0; jj<pDataset->pDataPerChannel[0].numPcdacValues; jj++)
	{
		currChannel = &(pDataset->pDataPerChannel[0]);
		nextChannel = &(pDataset->pDataPerChannel[1]);
		pcd = currChannel->pPcdacValues[jj];

		filteredOne = 0;

		// filter the 1st point
		if ( (currChannel->pPwrValues[jj] > data_ceiling) ||
			 ( fabs(currChannel->pPwrValues[jj] - nextChannel->pPwrValues[jj]) > data_ceiling) )
		{
			currChannel->pPwrValues[jj] = nextChannel->pPwrValues[jj] ;
			filteredOne = 1;
		}

		// filter elements 1..N-1
		for ( ii=1; ii<(pDataset->numChannels - 1); ii++)
		{
			freq = pDataset->pChannels[ii];
			prevChannel = &(pDataset->pDataPerChannel[ii-1]);
			currChannel = &(pDataset->pDataPerChannel[ii]);
			nextChannel = &(pDataset->pDataPerChannel[ii+1]);
			if (filteredOne > 0)
			{
				filteredOne = 0;
				continue ;
			} else
			{
				avgii = (prevChannel->pPwrValues[jj] + nextChannel->pPwrValues[jj]) / 2.0;
				diffii = fabs(prevChannel->pPwrValues[jj] - nextChannel->pPwrValues[jj]);

				if ( (currChannel->pPwrValues[jj] > data_ceiling) ||
					 ((fabs(currChannel->pPwrValues[jj] - avgii) > data_ceiling) && (diffii <= data_ceiling)) )
				{
					if (WARNINGS_ON)
						uiPrintf("Filtering %d, %d datapoint from %2.1f to %2.1f\n", freq, pcd, currChannel->pPwrValues[jj], avgii);
					currChannel->pPwrValues[jj] = avgii ;
					filteredOne = 1;
				}
			}
		}


		// filter the Nth point
		ii = pDataset->numChannels - 1;
		prevChannel = &(pDataset->pDataPerChannel[ii-1]);
		currChannel = &(pDataset->pDataPerChannel[ii]);

		if ( ( (currChannel->pPwrValues[jj] > data_ceiling) ||
			   (fabs(currChannel->pPwrValues[jj] - prevChannel->pPwrValues[jj]) > data_ceiling) )  &&
			   (filteredOne < 1) ) 
		{
			currChannel->pPwrValues[jj] = prevChannel->pPwrValues[jj] ;
			filteredOne = 1;
		}
	}		
}

void truncate_hash_subzero(dPCDACS_EEPROM *pDataset) 
{

	A_UINT16 ii, jj;
	dDATA_PER_CHANNEL *currChannel ;

	for (ii=0; ii<pDataset->numChannels; ii++)
	{
		currChannel = &(pDataset->pDataPerChannel[ii]);
		for (jj=0; jj<currChannel->numPcdacValues; jj++)
		{
			if (currChannel->pPwrValues[jj] < 0.0)
				currChannel->pPwrValues[jj] = 0.0;
		}
	}
	
}

void differentiate_hash(dPCDACS_EEPROM *srcStruct, dPCDACS_EEPROM *destStruct)
{

	A_UINT16  ii, jj   ;
	double  diffii;
	dDATA_PER_CHANNEL *currChannel ;

	diffii = (double) 2.0*(srcStruct->pChannels[1] - srcStruct->pChannels[0]) ;

	destStruct->hadIdenticalPcdacs = TRUE;	
	memcpy(destStruct->pChannels, srcStruct->pChannels, srcStruct->numChannels * sizeof(A_UINT16));

	for (ii=0; ii<destStruct->numChannels; ii++)
	{
		currChannel = &(destStruct->pDataPerChannel[ii]);
		currChannel->pcdacMax = srcStruct->pDataPerChannel[ii].pcdacMax ;
		currChannel->pcdacMin = srcStruct->pDataPerChannel[ii].pcdacMin ;
		memcpy(currChannel->pPcdacValues, srcStruct->pDataPerChannel[ii].pPcdacValues, currChannel->numPcdacValues*sizeof(A_UINT16));
	}

	for (jj=0; jj<destStruct->pDataPerChannel[0].numPcdacValues; jj++)
	{
		destStruct->pDataPerChannel[0].pPwrValues[jj] = (double)2.0*( srcStruct->pDataPerChannel[1].pPwrValues[jj] -
														    srcStruct->pDataPerChannel[0].pPwrValues[jj] )/diffii ;

		for (ii=1; ii<(destStruct->numChannels-1); ii++)
		{
			destStruct->pDataPerChannel[ii].pPwrValues[jj] = (double)( srcStruct->pDataPerChannel[ii+1].pPwrValues[jj] -
														       srcStruct->pDataPerChannel[ii-1].pPwrValues[jj] )/diffii ;
		}

		ii = destStruct->numChannels-1 ;
		destStruct->pDataPerChannel[ii].pPwrValues[jj] = (double)2.0*( srcStruct->pDataPerChannel[ii-1].pPwrValues[jj] -
														     srcStruct->pDataPerChannel[ii].pPwrValues[jj] )/diffii ;
	}

}


void build_turning_points(dPCDACS_EEPROM *pDataset, dPCDACS_EEPROM *pDerivDataset, A_UINT16 *TPlist, A_UINT16 *totalTPs, A_UINT16 debug)
{

	A_UINT16 numExtrema, numTotalExtrema ; 
	A_UINT16 *extrema ;

	A_UINT16 ii, jj;

	numTotalExtrema = 0;
	
	extrema = (A_UINT16 *)malloc(pDataset->numChannels * sizeof(A_UINT16)) ;
	if (extrema == NULL)
	{
		uiPrintf("Could not allocate memory for extrema. Exiting...\n");
		exit(0);
	}

	for (ii=0; ii<pDataset->pDataPerChannel[0].numPcdacValues; ii++)
	{
		pick_local_extrema(pDerivDataset, ii, extrema, &(numExtrema)) ; // numExtrema stores the number of extrema found
		for (jj=0; jj<numExtrema; jj++)
		{
			addUniqueElementToList(TPlist, extrema[jj], &(numTotalExtrema));
		}
	}

	*totalTPs = numTotalExtrema ;
	qsort( (void *) TPlist, numTotalExtrema, sizeof(A_UINT16), numerical_compare) ;
	
	free(extrema);
}

void addUniqueElementToList(A_UINT16 *list, A_UINT16 element, A_UINT16 *listSize)
{
	A_UINT16 ii = 0;
	A_BOOL isUnique = TRUE;
	A_UINT16 size = *listSize ;

	while ((ii<size) && (isUnique))
	{
		if (element == list[ii])
		{
			isUnique = FALSE;
		}
		ii++;
	}

	if (isUnique)
	{
 		list[size] = element ;
		(*listSize) = size + 1 ;
	}

}


int numerical_compare( const void *arg1, const void *arg2 )
{
	A_INT16 comparison = 0;
	A_UINT16 val1 = * ( A_UINT16 * ) arg1;
	A_UINT16 val2 = * ( A_UINT16 * ) arg2;

	if (val1 > val2)
		comparison = 1;
	if (val1 < val2)
		comparison = -1;

	return ( comparison );
}

void pick_local_extrema(dPCDACS_EEPROM *pDataset, A_UINT16 pcd_idx, A_UINT16 *retList, A_UINT16 *listSize)
{

	A_UINT16 ii;
	A_INT16 flatRunStartDirection, dirxn, newDirexn, midPoint ;
	A_UINT16 *flatRun ;
	A_UINT16 sizeFlatRun =0;
	A_UINT16 ch0, ch1, chN;
	double noise;
	double delta;
	A_UINT16 numTPs = 0;

	flatRun = (A_UINT16*)malloc(pDataset->numChannels * sizeof(A_UINT16)) ;
	if (flatRun == NULL)
	{
		uiPrintf("Could not allocate memory for list flatRun. Exiting...\n");
		exit(0);
	}

	ch0 = pDataset->pChannels[0];
	ch1 = pDataset->pChannels[1];
	chN = pDataset->pChannels[pDataset->numChannels-1];
	// noise calculated to ignore a change in slope causing a 1dB deviation over entire range
	// an adjustment factor of 10 seems to work better for a 30MHz measurement step
	noise = fabs(1.0/(10*(ch1-ch0)*(chN-ch0))) ;

	retList[numTPs] = ch0;
	numTPs++;

	flatRunStartDirection = 0; // implicitly indicates out of flatrun
	delta = (pDataset->pDataPerChannel[1].pPwrValues[pcd_idx] -	pDataset->pDataPerChannel[0].pPwrValues[pcd_idx]) ;
	newDirexn =  (delta >= 0) ? 1 : -1 ;

	for (ii=1; ii<(pDataset->numChannels-1); ii++)
	{
		dirxn = newDirexn;
		newDirexn = 0;
		delta = (pDataset->pDataPerChannel[ii+1].pPwrValues[pcd_idx] -	pDataset->pDataPerChannel[ii].pPwrValues[pcd_idx]) ;
		if (delta > 2*noise)
		{
			newDirexn = 1;
		}
		if (-delta > 2*noise)
		{
			newDirexn = -1;
		}
		if (abs(newDirexn - dirxn) < 1) // that is, no change in direction
		{
			if (abs(dirxn) < 1) // start populating flatRun
			{
				flatRun[sizeFlatRun] = ii ;
				sizeFlatRun++;
			}
			continue;
		}
		if (abs(newDirexn - dirxn) > 1) // a reversal in direction
		{
			// pick only the NON-ZERO extrema
			if (fabs(pDataset->pDataPerChannel[ii].pPwrValues[pcd_idx]) > noise)
			{
				retList[numTPs] = pDataset->pChannels[ii];
				numTPs++;
			}
			continue;
		}
		// at this point, either starting a flatrun or ending one
		if (abs(newDirexn) < 1) // implies start of a flatrun
		{
			flatRunStartDirection = newDirexn;
			flatRun[sizeFlatRun] = ii ;
			sizeFlatRun++;
			continue ;
		}
		if (abs(dirxn) < 1) // implies end of a flatrun
		{
			midPoint = flatRun[(A_UINT16)(sizeFlatRun-1)/2] ;
			if ((abs(newDirexn-flatRunStartDirection) > 1) &&  // pick only extrema, not plateaus
				(fabs(pDataset->pDataPerChannel[midPoint].pPwrValues[pcd_idx]) > noise)) // only non-zero extrema
			{
				retList[numTPs] = pDataset->pChannels[ii];
				numTPs++;
			}
			flatRunStartDirection = 0 ; 
			sizeFlatRun = 0; // effectively clears the list flatRun
		}
	}

	retList[numTPs] = pDataset->pChannels[pDataset->numChannels-1];
	numTPs++;

	*listSize = numTPs;	
	
	free(flatRun);
}

void consolidate_turning_points(A_UINT16 **tpList, A_UINT16 *sizeList, A_UINT16 debug)
{

	A_UINT16 *newList;
	A_UINT16 sizeNewList = 0;
	A_UINT16 binSize = 40;
	A_UINT16 ii;

	newList = (A_UINT16 *)malloc(numRAWChannels * sizeof(A_UINT16));
	if (newList == NULL)
	{
		uiPrintf("Could not allocate newList to consolidate turning points. Exiting...\n");
		exit(0);
	}

	newList[0] = (*tpList)[0] ;
	sizeNewList++;

	for (ii=1; ii<*sizeList-2; ii++)
	{
		if ( ((*tpList)[ii] - newList[sizeNewList-1]) > binSize)
		{
			newList[sizeNewList] = (*tpList)[ii];
			sizeNewList++;
		}
	}

		if ( ( ((*tpList)[*sizeList-2] - newList[sizeNewList-1]) > binSize) &&
			 ( ((*tpList)[*sizeList-1] - (*tpList)[*sizeList-2]) > binSize) )
		{
			newList[sizeNewList] = (*tpList)[*sizeList-2];
			sizeNewList++;
		}

		newList[sizeNewList] = (*tpList)[*sizeList-1];
		sizeNewList++;

		// free the memory associated with the old list that had all the TPs
		free( (void *) (*tpList));
		// point the allTP list to the consolidated list
		(*tpList) = newList ;
		*sizeList = sizeNewList ;
		
}


void build_turningpoint_totempole(dPCDACS_EEPROM *pDataset, TP_VAL **totempole, A_UINT16 sizeTotempole, A_UINT16 *tpList, A_UINT16 debug)
{

	A_INT16 ii, jj;
	double avg, cost;
	A_UINT16 left, right;


	(*totempole) = (TP_VAL *)malloc(sizeTotempole * sizeof(TP_VAL)) ;
	if ((*totempole) == NULL)
	{
		uiPrintf("Could not allocate memory for the totempole. Exiting...\n");
		exit(0);
	}

	jj = 0;
	for (ii=0; ii<pDataset->numChannels; ii++)
	{
		if (tpList[jj] == pDataset->pChannels[ii])
		{
			(*totempole)[jj].channel_idx = ii ;
			jj++ ;
		}
	}

	if (jj != sizeTotempole)
	{
		uiPrintf("Bad juju happened in assigning totempole. jj=%d, numTPs=%d. Exiting...\n",jj, sizeTotempole);
		exit(0);
	}

	for (ii=1; ii<(sizeTotempole-1); ii++)
	{
		cost = 0.0;
		left = pRawDataset->pChannels[(*totempole)[ii].channel_idx] - 
			    pRawDataset->pChannels[(*totempole)[ii-1].channel_idx];
		right= pRawDataset->pChannels[(*totempole)[ii+1].channel_idx] - 
			    pRawDataset->pChannels[(*totempole)[ii].channel_idx]; 
		
		for (jj=0; jj<pDataset->pDataPerChannel[0].numPcdacValues; jj++)
		{
			avg = (double) (right*pDataset->pDataPerChannel[(*totempole)[ii-1].channel_idx].pPwrValues[jj] +
				   left*pDataset->pDataPerChannel[(*totempole)[ii+1].channel_idx].pPwrValues[jj])/(left+right) ;
			cost += fabs(pDataset->pDataPerChannel[(*totempole)[ii].channel_idx].pPwrValues[jj] - avg);
		}
		(*totempole)[ii].val = cost ;
		if(debug > 0)
		{
			uiPrintf("Impact of TP#%d at %d = %2.1f.\n", ii, pDataset->pChannels[(*totempole)[ii].channel_idx], (*totempole)[ii].val);
		}
	}

	//guarantee inclusion of 1st and last TP
	(*totempole)[0].val = 9999.0;
	(*totempole)[sizeTotempole-1].val = 9999.0; 
}

int totempole_compare( const void *arg1, const void *arg2 )
{
	A_INT16 comparison = 0;
	TP_VAL tp1 = * ( TP_VAL * ) arg1;
	TP_VAL tp2 = * ( TP_VAL * ) arg2;

	if ( tp1.val> tp2.val )
		comparison = -1;
	if ( tp1.val < tp2.val )
		comparison = 1; 

	return ( comparison ); // for descending qsort of totempole
}

void dMapGrid(dPCDACS_EEPROM *pSrcStruct, dPCDACS_EEPROM *pDestStruct)
{
	dDATA_PER_CHANNEL	*pChannelData;
	A_UINT16			*pPcdacValue;
	double				*pPwrValue;
	A_UINT16			i, j;
	A_UINT16            channel, pcdac;

	pChannelData = pDestStruct->pDataPerChannel; 

	for(i = 0; i < pDestStruct->numChannels; i++ ) {
		pPcdacValue = pChannelData->pPcdacValues;
		pPwrValue = pChannelData->pPwrValues;
		for(j = 0; j < pChannelData->numPcdacValues; j++ ) {
			channel = pChannelData->channelValue;
			pcdac = *pPcdacValue;
			*pPwrValue = getPowerAt(channel, pcdac, pSrcStruct);
			pPcdacValue++;
			pPwrValue++;
	    }
		pChannelData++;			
	}
}

double getPowerAt( A_UINT16 channel, A_UINT16	pcdacValue, dPCDACS_EEPROM *pSrcStruct)
{
	double	powerValue;
	A_UINT16	lFreq, rFreq;			//left and right frequency values
	A_UINT16	llPcdac, ulPcdac;		//lower and upper left pcdac values
	A_UINT16	lrPcdac, urPcdac;		//lower and upper right pcdac values
	double	lPwr, uPwr;				//lower and upper temp pwr values
	double	lScaledPwr, rScaledPwr;	//left and right scaled power


	if(dFindValueInList(channel, pcdacValue, pSrcStruct, &powerValue)){
		//value was copied from srcStruct
		return(powerValue);
	}

	dGetLeftRightChannels(channel, pSrcStruct, &lFreq, &rFreq);
	dGetLowerUpperPcdacs(pcdacValue, lFreq, pSrcStruct, &llPcdac, &ulPcdac);
	dGetLowerUpperPcdacs(pcdacValue, rFreq, pSrcStruct, &lrPcdac, &urPcdac);
    
	//get the power index for the pcdac value
	dFindValueInList(lFreq, llPcdac, pSrcStruct, &lPwr);
	dFindValueInList(lFreq, ulPcdac, pSrcStruct, &uPwr);
	lScaledPwr = dGetInterpolatedValue( pcdacValue, llPcdac, ulPcdac, lPwr, uPwr);
    
	dFindValueInList(rFreq, lrPcdac, pSrcStruct, &lPwr);
	dFindValueInList(rFreq, urPcdac, pSrcStruct, &uPwr);
	rScaledPwr = dGetInterpolatedValue( pcdacValue, lrPcdac, urPcdac, lPwr, uPwr);

	return(dGetInterpolatedValue( channel, lFreq, rFreq, lScaledPwr, rScaledPwr));
} 

// the "double" returning version of findValueInList that works with scaled integer values
A_BOOL dFindValueInList( A_UINT16 channel, A_UINT16 pcdacValue, dPCDACS_EEPROM *pSrcStruct, double *powerValue)
{
	dDATA_PER_CHANNEL	*pChannelData;
	A_UINT16			*pPcdac;
	A_UINT16			i, j;

	pChannelData = pSrcStruct->pDataPerChannel; 
	for(i = 0; i < pSrcStruct->numChannels; i++ ) {
		if (pChannelData->channelValue == channel) {
			pPcdac = pChannelData->pPcdacValues;
			for(j = 0; j < pChannelData->numPcdacValues; j++ ) {
				if (*pPcdac == pcdacValue) {
					*powerValue = pChannelData->pPwrValues[j];
					return(1);
				}
				pPcdac++;
			}
	    }
		pChannelData++;			
	}
	return(0);
}

// only searches for integer values in integer lists. used for channel and pcdac lists
void iGetLowerUpperValues 
(
 A_UINT16	value,			//value to search for
 A_UINT16	*pList,			//ptr to the list to search
 A_UINT16	listSize,		//number of entries in list
 A_UINT16	*pLowerValue,	//return the lower value
 A_UINT16	*pUpperValue	//return the upper value	
)
{
	A_UINT16	i;
	A_UINT16	listEndValue = *(pList + listSize - 1);
	A_UINT16	target = value ;

	//see if value is lower than the first value in the list
	//if so return first value
	if (target <= (*pList)) {
		*pLowerValue = *pList;
		*pUpperValue = *pList;
		return;
	}
  
	//see if value is greater than last value in list
	//if so return last value
	if (target >= listEndValue) {
		*pLowerValue = listEndValue;
		*pUpperValue = listEndValue;
		return;
	}

	//look for value being near or between 2 values in list
	for(i = 0; i < listSize; i++) {
		//if value is close to the current value of the list 
		//then target is not between values, it is one of the values
		if (pList[i] == target) {
			*pLowerValue = pList[i];
			*pUpperValue = pList[i];
			return;
		}

		//look for value being between current value and next value
		//if so return these 2 values
		if (target < pList[i + 1]) {
			*pLowerValue = pList[i];
			*pUpperValue = pList[i + 1];
			return;
		}
	}
} 


void dGetLeftRightChannels(
 A_UINT16			channel,			//channel to search for
 dPCDACS_EEPROM		*pSrcStruct,		//ptr to struct to search
 A_UINT16			*pLowerChannel,		//return lower channel
 A_UINT16			*pUpperChannel		//return upper channel
)
{
	iGetLowerUpperValues(channel, pSrcStruct->pChannels, pSrcStruct->numChannels, 
						pLowerChannel, pUpperChannel);  
	return; 
}


void dGetLowerUpperPcdacs 
(
 A_UINT16			pcdac,				//pcdac to search for
 A_UINT16			channel,			//current channel
 dPCDACS_EEPROM		*pSrcStruct,		//ptr to struct to search
 A_UINT16			*pLowerPcdac,		//return lower pcdac
 A_UINT16			*pUpperPcdac		//return upper pcdac
)
{
	dDATA_PER_CHANNEL	*pChannelData;
	A_UINT16			i;
	
	//find the channel information
	pChannelData = pSrcStruct->pDataPerChannel;
	for (i = 0; i < pSrcStruct->numChannels; i++) {
		if(pChannelData->channelValue == channel) {
			break;
		}
		pChannelData++;
	}

	iGetLowerUpperValues(pcdac, pChannelData->pPcdacValues, pChannelData->numPcdacValues, 
						pLowerPcdac, pUpperPcdac);  
	return;
} 

double dGetInterpolatedValue(A_UINT32 target, A_UINT32 srcLeft, A_UINT32 srcRight, 
							 double targetLeft, double targetRight)
{
  double returnValue;
  double lRatio;


  if (abs(srcRight - srcLeft) > 0) {
    //note the ratio always need to be scaled, since it will be a fraction
    lRatio = (double) (target - srcLeft) / (srcRight - srcLeft);
    returnValue = (lRatio*targetRight + (1.0-lRatio)*targetLeft);
  } 
  else {
	  returnValue = targetLeft;
  }
  return (returnValue);
}


void build_cal_dataset_skeleton(dPCDACS_EEPROM *srcDataset, dPCDACS_EEPROM *destDataset, 
								A_UINT16 *pPercentages, A_UINT16 numIntercepts, 
								A_UINT16 *tpList, A_UINT16 numTPs)
{

	A_UINT16 ii, jj, kk;
	double maxdB;
	dDATA_PER_CHANNEL *currChannel;
	A_UINT16 intc, pcdMin, pcdMax;


	destDataset->numChannels = numTPs;
	for (ii=0; ii<numTPs; ii++)
	{
		destDataset->pChannels[ii] = tpList[ii];
		destDataset->pDataPerChannel[ii].channelValue = tpList[ii];
		destDataset->pDataPerChannel[ii].numPcdacValues = numIntercepts;
	}

	for (jj=0; jj<numTPs; jj++)
	{
		kk = 0;
		while ((destDataset->pDataPerChannel[jj].channelValue > srcDataset->pDataPerChannel[kk].channelValue) &&
			   (kk < (srcDataset->numChannels - 1)) )
		{
			kk++;
		}
		for (ii=0; ii < srcDataset->pDataPerChannel[0].numPcdacValues; ii++)
		{
//			if (srcDataset->pDataPerChannel[kk].pPwrValues[ii+1] > 0.0)
			if (((srcDataset->pDataPerChannel[kk].pPwrValues[ii+1]-srcDataset->pDataPerChannel[kk].pPwrValues[ii]) > 0.5) &&
				(srcDataset->pDataPerChannel[kk].pPwrValues[ii] > 0.0))
			{
				destDataset->pDataPerChannel[jj].pcdacMin = srcDataset->pDataPerChannel[kk].pPcdacValues[ii];
				break;
			}
		}
	
		currChannel = &(srcDataset->pDataPerChannel[kk]);
		maxdB = currChannel->pPwrValues[currChannel->numPcdacValues-1];
		for (ii=srcDataset->pDataPerChannel[0].numPcdacValues-1; ii>0; ii--)
		{
			if (currChannel->pPwrValues[ii-1] < (maxdB-0.25))
			{
				destDataset->pDataPerChannel[jj].pcdacMax = currChannel->pPcdacValues[ii];
				break;
			}
		}

		pcdMin = destDataset->pDataPerChannel[jj].pcdacMin;
		pcdMax = destDataset->pDataPerChannel[jj].pcdacMax;

		for (ii=0; ii < destDataset->pDataPerChannel[0].numPcdacValues; ii++)
		{
			intc = pPercentages[ii];
			destDataset->pDataPerChannel[jj].pPcdacValues[ii] = (A_UINT16)(intc*pcdMax + (100-intc)*pcdMin)/100;
		}
	}
}

void quantize_hash(dPCDACS_EEPROM *pDataSet)
{

	A_UINT16 ii, jj, intNum;
	double quantum = 0.5;//6 bit=>.5, 7bit=>.25

	for (ii=0; ii<pDataSet->numChannels; ii++)
	{
		for(jj=0; jj<pDataSet->pDataPerChannel[0].numPcdacValues; jj++)
		{
			if(pDataSet->pDataPerChannel[ii].pPwrValues[jj] < quantum)
			{
				pDataSet->pDataPerChannel[ii].pPwrValues[jj] = 0.0;
			} else
			{
				intNum= (A_UINT16)((pDataSet->pDataPerChannel[ii].pPwrValues[jj]/quantum) + 0.5);
				pDataSet->pDataPerChannel[ii].pPwrValues[jj] = (double) quantum*intNum;
			}
		}
	}
}


A_UINT16 dutTest(A_UINT32 *devNum)
{
	A_UINT32 Domain;
	A_UINT32 devNum_local;
	A_UINT32 mode_local;
	A_UINT32 jumbo_frame_mode_save;

#ifdef _IQV
	if (testType == ART_INIT || testType == ART_CAL  || testType == ART_MANUF)
	{
#endif // _IQV
	// Backup Regulatory Domain
	if ((eepromType == EEPROM_SIZE_16K) && !isDragon(devNumArr[MODE_11a]))
	{
		Domain = art_eepromRead(devNumArr[MODE_11a], 0xbf)&0xffff;
	} 

	if (CalSetup.test32KHzSleepCrystal && CalSetup.Enable_32khz) {
		devNum_local = (CalSetup.Amode) ? devNumArr[MODE_11a] : devNumArr[MODE_11g];
		mode_local = (CalSetup.Amode) ? MODE_11a : MODE_11g;
		timestart = milliTime();
		strcpy(testname[testnum],"32 KHz Sleep Crystal");
		setYieldParamsForTest(devNum_local, modeName[mode_local], "sleep_xtal", "delta_nav", "usec", "delta_T", "usec"); 
		uiPrintf("\n32 KHz Sleep Crystal Test : ");
		if (test_sleep_crystal(devNum_local)) {
			uiPrintf("PASS\n\n"); 
		} else {
			uiPrintf("FAIL [X]\n\n");
			TestFail = TRUE;
			if (CalSetup.endTestOnFail) {
				REWIND_TEST = TRUE;
			}
			failTest[testnum] = 1;
		}
		testtime[testnum++] = milliTime() - timestart;
		if (REWIND_TEST) {
			printTestEndSummary();
			return TRUE;
		}
	}
#ifdef _IQV
		if (testType == ART_INIT)
			return TRUE;	
	} else {	// testType == ART_MANUF  || testType == ART_CAL keep going
#endif // _IQV
/*
	if (configSetup.remote) {
		devNum_local = (CalSetup.Amode) ? devNumArr[MODE_11a] : devNumArr[MODE_11g];
		mode_local = (CalSetup.Amode) ? MODE_11a : MODE_11g;
		dutTestEthernetThroughput(devNum_local);
	}
*/
	if (CalSetup.Amode) {
		configSetup.eepromLoad = 1;
		art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
							(A_BOOL)1, (A_UCHAR)MODE_11A, configSetup.use_init);		
		art_rereadProm(devNumArr[MODE_11a]);
		
		if (!art_resetDevice(devNumArr[MODE_11a], txStation, bssID, 5220, 0))
			return FALSE;
		if(!processEepFile(devNumArr[MODE_11a], configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion))
			return FALSE;
		if (!art_resetDevice(devNumArr[MODE_11a], txStation, bssID, 5220, 0))
			return FALSE;

		if(configSetup.force_antenna && isDragon_sd(swDeviceID))
		{
			updateForcedAntennaVals(devNumArr[MODE_11A]);
			updateAntenna(devNumArr[MODE_11A]);
		}
		
		if (NEED_GAIN_OPT_FOR_MODE[MODE_11a])
		{
			//setGainLadderForMaxGain(pCurrGainLadder);
			//initializeGainLadder(pCurrGainLadder);				
			setGainLadderForIndex(pCurrGainLadder, optGainLadderIndex[MODE_11a]);
			programNewGain(pCurrGainLadder, devNumArr[MODE_11a], 0);
		}
#ifdef _IQV
			if (testType != ART_MANUF)	
				return TRUE;	// from testType == ART_CAL
			else {	// do target measurement
#endif // _IQV
		if (CalSetup.testTargetPowerControl[MODE_11a])
		{
			timestart = milliTime();
			strcpy(testname[testnum],"11a target pwr ctl");
			setYieldParamsForTest(devNumArr[MODE_11a], "11a", "tgt_pwr", "pwr", "dBm", "max_lin_pwr", "dBm"); 
			dutTestTargetPower( devNumArr[MODE_11a], MODE_11a);
			testtime[testnum++] = milliTime() - timestart;
		}
#ifdef _IQV
			}	// from testType == ART_MANUF, do target measurement
#endif // _IQV

#ifndef _IQV
		if(CalSetup.testSpecMask) {
			timestart = milliTime();
			strcpy(testname[testnum],"11a spectral mask");
			setYieldParamsForTest(devNumArr[MODE_11a], "11a", "spectral_mask", "fail_pts", "num", "measured_pwr", "dBm"); 
			dutTestSpecMask(devNumArr[MODE_11a]);
			testtime[testnum++] = milliTime() - timestart;
		}

		if(CalSetup.testOBW > 0)
		{
			timestart = milliTime();
			strcpy(testname[testnum],"11a OBW");
			setYieldParamsForTest(devNumArr[MODE_11a], "11a", "obw", "fail_pts", "num", "measured_pwr", "dBm"); 
			dutTestOBW(devNumArr[MODE_11a]);
			testtime[testnum++] = milliTime() - timestart;
		}

		if((CalSetup.testTXPER) || (CalSetup.testRXSEN))
		{
			sendAck(devNumArr[MODE_11a], "Start 11a PER and SEN test", 0, 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return TRUE;
			}

			timestart = milliTime();
			strcpy(testname[testnum],"11a PER and RXSEN");
			setYieldParamsForTest(devNumArr[MODE_11a], "11a", "tx_per", "good_pkts", "percent", "rssi", "dB"); 
			TX_PER_STRESS_MARGIN = 0; // do normal tx_per test first
			dutTestTXRX(devNumArr[MODE_11a], MODE_11a);
			if (REWIND_TEST) {
				return TRUE;
			}
			testtime[testnum++] = milliTime() - timestart;

			// test names to be used in "descriptive error reports"
			strcpy(testname[testnum],"11a PER");
			testtime[testnum++] = -1;
			strcpy(testname[testnum],"11a RXSEN");
			testtime[testnum++] = -1;
			strcpy(testname[testnum],"11a PPM");
			testtime[testnum++] = -1;

			if (CalSetup.testTXPER_margin) {
				sendAck(devNumArr[MODE_11a], "Start 11a +1dB PER MARGIN test", 0, 0, 0, CalSetup.customerDebug);
				waitForAck(CalSetup.customerDebug);
				if (REWIND_TEST) {
					return TRUE;
				}
				timestart = milliTime();
				strcpy(testname[testnum],"11a +1dB PER MARGIN");
				setYieldParamsForTest(devNumArr[MODE_11a], "11a", "1dB_tx_per", "good_pkts", "percent", "rssi", "dB"); 
				TX_PER_STRESS_MARGIN = 1; // do 1dB tx_per margin test 
				dutTestTXRX(devNumArr[MODE_11a], MODE_11a);
				TX_PER_STRESS_MARGIN = 0; // set back to default
				if (REWIND_TEST) {
					return TRUE;
				}
				testtime[testnum++] = milliTime() - timestart;

				// test names to be used in "descriptive error reports"
				strcpy(testname[testnum],"11a +1dB PER");
				testtime[testnum++] = -1;

				sendAck(devNumArr[MODE_11a], "Start 11a +2dB PER MARGIN test", 0, 0, 0, CalSetup.customerDebug);
				waitForAck(CalSetup.customerDebug);
				if (REWIND_TEST) {
					return TRUE;
				}
				timestart = milliTime();
				strcpy(testname[testnum],"11a +2dB PER MARGIN");
				setYieldParamsForTest(devNumArr[MODE_11a], "11a", "2dB_tx_per", "good_pkts", "percent", "rssi", "dB"); 
				TX_PER_STRESS_MARGIN = 2; // do 2dB tx_per margin test 
				dutTestTXRX(devNumArr[MODE_11a], MODE_11a);
				TX_PER_STRESS_MARGIN = 0; // set back to default				
				testtime[testnum++] = milliTime() - timestart;

				// test names to be used in "descriptive error reports"
				strcpy(testname[testnum],"11a +2dB PER");
				testtime[testnum++] = -1;

				if (REWIND_TEST) {
					return TRUE;
				}				
			}
		}

		if(CalSetup.testDataIntegrity[MODE_11a])
		{
			sendAck(devNumArr[MODE_11a], "Start 11a verifyDataIntegrity test", 0, 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return TRUE;
			}

			timestart = milliTime();
			strcpy(testname[testnum],"11a Data Integrity Check");
			setYieldParamsForTest(devNumArr[MODE_11a], "11a", "data_integrity", "result", "string", "iter", "num"); 
			dutVerifyDataTXRX(devNumArr[MODE_11a], MODE_11a, 0, 6, VERIFY_DATA_PACKET_LEN);
			if (REWIND_TEST) {
				return TRUE;
			}
			testtime[testnum++] = milliTime() - timestart;
		}

		if(CalSetup.testThroughput[MODE_11a])
		{
			sendAck(devNumArr[MODE_11a], "Start 11a throughput test", 0, 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return TRUE;
			}

			timestart = milliTime();
			strcpy(testname[testnum],"11a Throughput Check");
			setYieldParamsForTest(devNumArr[MODE_11a], "11a", "tx_thruput", "thruput", "mbps", "rssi", "dB"); 
			dutThroughputTest(devNumArr[MODE_11a], MODE_11a, 0, 54, THROUGHPUT_PACKET_SIZE, NUM_THROUGHPUT_PACKETS);
			if (REWIND_TEST) {
				return TRUE;
			}
			testtime[testnum++] = milliTime() - timestart;

			if(CalSetup.testTURBO) {
				sendAck(devNumArr[MODE_11a], "Start 11a throughput Turbo test", 0, 0, 0, CalSetup.customerDebug);
				waitForAck(CalSetup.customerDebug);
				if (REWIND_TEST) {
					return TRUE;
				}

				timestart = milliTime();
				strcpy(testname[testnum],"11a Throughput Turbo Check");
				setYieldParamsForTest(devNumArr[MODE_11a], "11a", "tx_thruput", "thruput", "mbps", "rssi", "dB"); 
				jumbo_frame_mode_save = art_getFieldForMode(devNumArr[MODE_11a], "mc_jumbo_frame_mode", MODE_11a, 1);
				art_writeField(devNumArr[MODE_11a], "mc_jumbo_frame_mode", 0);
				dutThroughputTest(devNumArr[MODE_11a], MODE_11a, 1, 54, THROUGHPUT_TURBO_PACKET_SIZE, NUM_THROUGHPUT_PACKETS);
				art_writeField(devNumArr[MODE_11a], "mc_jumbo_frame_mode", jumbo_frame_mode_save);
				if (REWIND_TEST) {
					return TRUE;
				}
				testtime[testnum++] = milliTime() - timestart;
			}
		
		}
#else
		} else if (testType == ART_MANUF) {
#endif // _IQV
		initializeGainLadder(pCurrGainLadder);
		programNewGain(pCurrGainLadder, devNumArr[MODE_11a], 0);
	}

#ifdef _IQV
		if (testType != ART_MANUF) // do target measurement for 11g 11b
			return TRUE;	
	}	// end of if (testType == ART_INIT || testType == ART_CAL  || testType == ART_MANUF)
		// testType == ART_CAL keep going to 11g and 11b. the other all returned.
#endif // _IQV
	if (CalSetup.Gmode) {
		if (!dutTest_2p4(devNumArr[MODE_11g], MODE_11g))
			return FALSE;
		if (REWIND_TEST) {
			return TRUE;
		}
	}

	if (CalSetup.Bmode || (CalSetup.Gmode && CalSetup.doCCKin11g)) {
		if (!dutTest_2p4(devNumArr[MODE_11b], MODE_11b))
			return FALSE;
		if (REWIND_TEST) {
			return TRUE;
		}
	}
#ifdef _IQV
	if (testType == ART_EXIT)
	{
#endif // _IQV
	// Restore Regulatory Domain
    if (eepromType == EEPROM_SIZE_16K)
	{
	   	/*art_writeField(devNumArr[MODE_11a], "mc_eeprom_size_ovr", 3); */
		//		art_eepromWrite(devNumArr[MODE_11a], 0xbf, Domain ); 
		virtual_eeprom0Write(devNumArr[MODE_11a], 0xbf, Domain ); 
	} 


	if (CalSetup.showTimingReport) {
	    report_timing_summary();
	}
//	configSetup.eepromLoad = 1;
//	art_setResetParams(devNumArr[MODE_11g], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
//							(A_BOOL)1, (A_UCHAR)MODE_11G, 0);		
	printTestEndSummary();
	art_rereadProm(devNumArr[MODE_11g]); 
//	art_resetDevice(devNumArr[MODE_11g], txStation, bssID, 2412, 0);
#ifdef _IQV
	}	// from testType == ART_EXIT
#endif // _IQV		
	return TRUE;
}


void dutTestSpecMask(A_UINT32 devNum){

    A_UCHAR  antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 cnt1;
	A_UINT16 numChannels;
	A_UINT32 *channelListTimes10 = NULL;
	A_UINT16 ii;
	A_UINT32 usePreSet=1;
	A_UINT32 fail_num;
	A_UINT32 testType;
	A_UINT32 testMask;
	A_BOOL dsrcMask;
	A_UINT32 turbo;
	A_UINT32 bandwidth;
	A_CHAR   extraTestText[100];

	double  center;
	double  span;

//#ifndef CUSTOMER_REL
	double	pwr;
//#endif

	A_UINT32 reset, verbose, plot_output; 

	A_UINT16 iter;
	A_BOOL   failedThisTest;


    span  = 60e6;	// span = 60 MHz 
    reset = 1; 
    verbose = 0; 
    plot_output = 0; 

    uiPrintf("\n\nStart Spectral Mask Test in 11a mode");

	if (eepromType == EEPROM_SIZE_16K)
	{
		numChannels = pTestSet[MODE_11a]->numTestChannelsDefined;
		channelListTimes10 = (A_UINT32 *)malloc(numChannels*sizeof(A_UINT32)) ;
		if(NULL == channelListTimes10) {
			uiPrintf("Error: unable to alloc mem for channel list in spec test\n");
			return;
		}
		for (ii=0; ii<numChannels; ii++)
		{
			channelListTimes10[ii] = pTestSet[MODE_11a]->pTestChannel[ii].channelValueTimes10;
		}
		art_rereadProm(devNum); // re-read the calibration data from eeprom
							// No need to change domain.
	} 


	uiPrintf("\n");

	for(cnt1=0; cnt1<numChannels; cnt1++) {
		if (eepromType == EEPROM_SIZE_16K)
		{
			if (!pTestSet[MODE_11a]->pTestChannel[cnt1].TEST_MASK)
			{
				continue; //skip spectral mask test at this channel
			}
		}

		testType = SPEC_MASK_20MHZ;
		testMask = pTestSet[MODE_11a]->pTestChannel[cnt1].TEST_MASK;

		while(testMask) {
			if(testMask & 0x1) {
				if(testType & SPEC_MASK_20MHZ) {
					dsrcMask = FALSE;
					turbo = 0;
					sprintf(extraTestText,"");
				}
				else if (testType & SPEC_MASK_DSRC_20MHZ) {
					dsrcMask = TRUE;
					turbo = 0;
					bandwidth = 20;
					sprintf(extraTestText, "DSRC 20Mhz");
				}
				else if (testType & SPEC_MASK_DSRC_10MHZ) {
					dsrcMask = TRUE;
					turbo = HALF_SPEED_MODE;
					bandwidth = 10;
					sprintf(extraTestText, "DSRC 10Mhz");
				}
				else if (testType & SPEC_MASK_DSRC_5MHZ) {
					dsrcMask = TRUE;
					turbo = QUARTER_SPEED_MODE;
					bandwidth = 5;
					sprintf(extraTestText, "DSRC 5Mhz");
				}
				else {
					uiPrintf("Illegal mask specified for mask Test\n");
					return;
				}


				// resetDevice loads the calibrated 64-entry pcdac table for
				// the channel. the power limits are min(targetPwr, bandEdgeMax) and hence
				// are domain independent. 
		//        art_txContEnd(devNum);
				if (cnt1 < 1)
				{
					art_resetDevice(devNum, txStation, bssID, channelListTimes10[cnt1], turbo);	// turbo=0
				} else
				{
					art_resetDevice(devNum, txStation, bssID, channelListTimes10[cnt1], turbo);	// turbo=0
		//			art_changeChannel(devNum, channelList[cnt1]);// if figure out how to stop tx100
				}
				art_txContBegin(devNum, CONT_DATA, PN9_PATTERN, 6, antenna);
				Sleep(20);		//wait 100 ms?

				iter = 0;
				failedThisTest = TRUE;

				while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
					iter++;
					if (CalSetup.customerDebug) uiPrintf("SNOOP: spec_mask repeat iter = %d \n", iter);
				
					if(CalSetup.useInstruments) {
						center = channelListTimes10[cnt1] * 1e5;
						if(dsrcMask) {
							art_writeField(devNum, "bb_window_length", 3);
							fail_num = spaMeasDSRCSpectralMask(devSA,center,span,reset,bandwidth,verbose,plot_output);
							art_writeField(devNum, "bb_window_length", 0);
						} else {
							fail_num = spaMeasSpectralMask(devSA,center,span,reset,verbose,plot_output);
						}
						reset = 0;
					} else {
						fail_num = 0;
					}

					failedThisTest = (fail_num > CalSetup.maskFailLimit) ? TRUE : FALSE;
				} //while failedThisTest

				uiPrintf("spec mask %s - chan:%5.1lf", extraTestText, 
					(double)(channelListTimes10[cnt1])/10);
				if (CalSetup.atherosLoggingScheme) {
					pwr = pmMeasAvgPower(devPM, 0) + CalSetup.attenDutPM;
				}
		//        if (fail_num > 20)	{
				if (fail_num > CalSetup.maskFailLimit)	{
					TestFail = TRUE;
					failTest[testnum] = 1;
					uiPrintf(", points failed out of 400: %4d", fail_num);
					uiPrintf("  [power=%3.1f]", pwr);
					logMeasToYield((double)(channelListTimes10[cnt1])/10, 6.0, (double)iter, (double)(CalSetup.maskFailLimit), (double)(fail_num), (double)(pwr), "FAIL");
				} else {
					logMeasToYield((double)(channelListTimes10[cnt1])/10, 6.0, (double)iter, (double)(CalSetup.maskFailLimit), (double)(fail_num), (double)(pwr), "PASS");
				}

				uiPrintf("\n");
			}
			testMask = testMask >> 1;
			testType = testType << 1;
		}
    }

    //art_txContEnd(devNum);
	art_resetDevice(devNum, txStation, bssID, channelListTimes10[0], 0);	// turbo=0

	if(channelListTimes10) {
		free(channelListTimes10);
	}
}

void ResetSA(const int ud, const double center, const double span, 
			  const double ref_level, const double scale, const double rbw, const double vbw) {

	int rsp;

	rsp = atoi(gpibQuery(ud, "*RST;*OPC?", 5L));
    gpibWrite(ud,    ":AVER 0;");								// averaging off
    gpibWrite(ud, qq(":FREQ:SPAN %f;", span));                  // span
    gpibWrite(ud, qq(":FREQ:CENT %f;", center));                // center frequency
    gpibWrite(ud, qq(":BAND:RES %f;", rbw));                    // resolution bandwidth
    gpibWrite(ud, qq(":BAND:VID %f;", vbw));                    // video bandwidth
    gpibWrite(ud, qq(":DISP:WIND:TRAC:Y:RLEV %f;", ref_level)); // reference level
    gpibWrite(ud, qq(":DISP:WIND:TRAC:Y:PDIV %f;", scale));		// scale
    gpibWrite(ud,    ":INIT:CONT 1;");                          // continuous sweep
    gpibWrite(ud,    ":DET POS;");                              // detector mode (positive peak)
    gpibWrite(ud,    ":TRAC:MODE WRIT;");                           // clear write
}


void dutTestOBW(A_UINT32 devNum) {
    A_UCHAR  antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 cnt1;
	A_UINT32 usePreSet=1;
	double   obw;
	double   OBW_LIMIT = 17.7f;	// 17.7 MHz
	double   margin;

	double  center;
	double  span;
	double	rbw, vbw;
	double  ref_level;
	double  scale;
	A_UINT32 reset=1;
	A_UINT16 numChannels, ii;
	A_UINT32 *channelListTimes10 = NULL;

	A_UINT16 iter;
	A_BOOL   failedThisTest;
	double pwr;

	center    = 5.17e9;
	span      = 40e6;		// Span = 40 MHz
	ref_level = 0;			// reference level = 10 dBm


	// Set Regulatory Domain to MKK
	uiPrintf("\n\nStart Occupied Bandwidth Test");
	
	art_rereadProm(devNum); 

	if (eepromType == EEPROM_SIZE_16K)
	{
		numChannels = pTestSet[MODE_11a]->numTestChannelsDefined;
		channelListTimes10 = (A_UINT32 *)malloc(numChannels*sizeof(A_UINT32)) ;
		if(NULL == channelListTimes10) {
			uiPrintf("Error: unable to alloc mem for channel list in spec test\n");
			return;
		}
		for (ii=0; ii<numChannels; ii++)
		{
			channelListTimes10[ii] = pTestSet[MODE_11a]->pTestChannel[ii].channelValueTimes10;
		}
		art_rereadProm(devNum); // re-read the calibration data from eeprom
							// No need to change domain.
	} 


	for(cnt1=0; cnt1<numChannels; cnt1++) {
		if (eepromType == EEPROM_SIZE_16K)
		{
			if (!pTestSet[MODE_11a]->pTestChannel[cnt1].TEST_OBW)
			{
				continue; //skip spectral mask test at this channel
			}
		}

		if (cnt1 < 1)
		{
			art_resetDevice(devNum, txStation, bssID, channelListTimes10[cnt1], 0);	// turbo=0
		} else
		{
			art_resetDevice(devNum, txStation, bssID, channelListTimes10[cnt1], 0);	// turbo=0
//			art_changeChannel(devNum, channelList[cnt1]);
		}

		art_txContBegin(devNum, CONT_DATA, PN9_PATTERN, 6, antenna);

		iter = 0;
		failedThisTest = TRUE;

		while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
			iter++;
			if (CalSetup.customerDebug) uiPrintf("SNOOP: obw repeat iter = %d \n", iter);

			if(CalSetup.useInstruments) {
				center    = channelListTimes10[cnt1] * 1e5;
				rbw       = 300e3;	// 300 KHz
				vbw       = 3e3;	// 3 KHz
				scale     = 10;		// 10 dB / scale

				obw = spaMeasOBW(devSA, center, span, ref_level, reset); 
				reset = 0;
			}
			else {
				obw = 17500000;
			}

			obw /= 1000000.0f;
			margin = OBW_LIMIT - obw;
			failedThisTest = (margin < 0) ? TRUE : FALSE;
		} //while failedThisTest

		if (CalSetup.atherosLoggingScheme) {
			pwr = pmMeasAvgPower(devPM, 0) + CalSetup.attenDutPM;		
		}

        uiPrintf("\nch:%%5.1lf, obw:%4.1f, margin:%4.2f", 
			(double)(channelListTimes10[cnt1])/10, obw, margin);
        if (margin < 0) {
			TestFail = TRUE;
			failTest[testnum] = 1;
            uiPrintf(" === OBW failed!");
			uiPrintf("  [power=%3.1f]", pwr);
			logMeasToYield((double)(channelListTimes10[cnt1])/10, 6.0, (double)iter, (double)(OBW_LIMIT), (double)(obw), (double)(pwr), "FAIL");
		} else {
			logMeasToYield((double)(channelListTimes10[cnt1])/10, 6.0, (double)iter, (double)(OBW_LIMIT), (double)(obw), (double)(pwr), "PASS");
		}
	}

	art_resetDevice(devNum, txStation, bssID, channelListTimes10[0], 0);	// turbo=0

	
	if(channelListTimes10) {
		free(channelListTimes10);
	}
}

// to be used when atheros label scheme is invoked. 
// shold be called right after completing cal, before writing eeprom data
void programMACIDFromLabel(A_UINT32 devNum)
{
	A_UCHAR     macID_arr[6], macID_arr2[6] ;
	A_UINT32    ii;
	A_UCHAR     EthernetMACID[24]; // upto 4 ethernet macIDs
    A_INT32		argList[16];
	A_UINT32	numArgs;
	

    // Write Mac Addr into EEPROM
	if ( configSetup.remote && !CalSetup.staCardOnAP)
	{

		memset(macID_arr, 0xFF, 6*sizeof(A_UCHAR));
		memset(macID_arr2, 0xFF, 6*sizeof(A_UCHAR));
		
		if (CalSetup.modeMaskForRadio[0] > 0) {
			macID_arr[0] = (A_UCHAR)((yldStruct.macID1[0] >> 8) & 0xFF);
			macID_arr[1] = (A_UCHAR)((yldStruct.macID1[0]) & 0xFF);
			macID_arr[2] = (A_UCHAR)((yldStruct.macID1[1] >> 8) & 0xFF);
			macID_arr[3] = (A_UCHAR)((yldStruct.macID1[1]) & 0xFF);
			macID_arr[4] = (A_UCHAR)((yldStruct.macID1[2] >> 8) & 0xFF);
			macID_arr[5] = (A_UCHAR)((yldStruct.macID1[2]) & 0xFF);
		}

		if (CalSetup.modeMaskForRadio[1] > 0) {		
			if (CalSetup.modeMaskForRadio[0] == 0)  {
				macID_arr2[0] = (A_UCHAR)((yldStruct.macID1[0] >> 8) & 0xFF);
				macID_arr2[1] = (A_UCHAR)((yldStruct.macID1[0]) & 0xFF);
				macID_arr2[2] = (A_UCHAR)((yldStruct.macID1[1] >> 8) & 0xFF);
				macID_arr2[3] = (A_UCHAR)((yldStruct.macID1[1]) & 0xFF);
				macID_arr2[4] = (A_UCHAR)((yldStruct.macID1[2] >> 8) & 0xFF);
				macID_arr2[5] = (A_UCHAR)((yldStruct.macID1[2]) & 0xFF);
			} else {
				macID_arr2[0] = (A_UCHAR)((yldStruct.macID2[0] >> 8) & 0xFF);
				macID_arr2[1] = (A_UCHAR)((yldStruct.macID2[0]) & 0xFF);
				macID_arr2[2] = (A_UCHAR)((yldStruct.macID2[1] >> 8) & 0xFF);
				macID_arr2[3] = (A_UCHAR)((yldStruct.macID2[1]) & 0xFF);
				macID_arr2[4] = (A_UCHAR)((yldStruct.macID2[2] >> 8) & 0xFF);
				macID_arr2[5] = (A_UCHAR)((yldStruct.macID2[2]) & 0xFF);
			}
		} 

		memset(EthernetMACID, 0xFF, 24*sizeof(A_UCHAR));

		ii = CalSetup.startEthernetPort;
//printf("SNOOP::start ethernet port = %d: numethernet ports=%d\n", CalSetup.startEthernetPort, CalSetup.numEthernetPorts);
		EthernetMACID[6*ii+0] = (A_UCHAR)((yldStruct.enetID1[0] >> 8) & 0xFF);
		EthernetMACID[6*ii+1] = (A_UCHAR)((yldStruct.enetID1[0]) & 0xFF);
		EthernetMACID[6*ii+2] = (A_UCHAR)((yldStruct.enetID1[1] >> 8) & 0xFF);
		EthernetMACID[6*ii+3] = (A_UCHAR)((yldStruct.enetID1[1]) & 0xFF);
		EthernetMACID[6*ii+4] = (A_UCHAR)((yldStruct.enetID1[2] >> 8) & 0xFF);
		EthernetMACID[6*ii+5] = (A_UCHAR)((yldStruct.enetID1[2]) & 0xFF);

		
		if (CalSetup.numEthernetPorts > 1) {
			ii++;
			EthernetMACID[6*ii+0] = (A_UCHAR)((yldStruct.enetID2[0] >> 8) & 0xFF);
			EthernetMACID[6*ii+1] = (A_UCHAR)((yldStruct.enetID2[0]) & 0xFF);
			EthernetMACID[6*ii+2] = (A_UCHAR)((yldStruct.enetID2[1] >> 8) & 0xFF);
			EthernetMACID[6*ii+3] = (A_UCHAR)((yldStruct.enetID2[1]) & 0xFF);
			EthernetMACID[6*ii+4] = (A_UCHAR)((yldStruct.enetID2[2] >> 8) & 0xFF);
			EthernetMACID[6*ii+5] = (A_UCHAR)((yldStruct.enetID2[2]) & 0xFF);
		}


		if (CalSetup.numEthernetPorts > 2) {
			ii++;
			EthernetMACID[6*ii+0] = (A_UCHAR)((yldStruct.enetID3[0] >> 8) & 0xFF);
			EthernetMACID[6*ii+1] = (A_UCHAR)((yldStruct.enetID3[0]) & 0xFF);
			EthernetMACID[6*ii+2] = (A_UCHAR)((yldStruct.enetID3[1] >> 8) & 0xFF);
			EthernetMACID[6*ii+3] = (A_UCHAR)((yldStruct.enetID3[1]) & 0xFF);
			EthernetMACID[6*ii+4] = (A_UCHAR)((yldStruct.enetID3[2] >> 8) & 0xFF);
			EthernetMACID[6*ii+5] = (A_UCHAR)((yldStruct.enetID3[2]) & 0xFF);
		}

		
		if (CalSetup.numEthernetPorts > 3) {
			ii++;
			EthernetMACID[6*ii+0] = (A_UCHAR)((yldStruct.enetID4[0] >> 8) & 0xFF);
			EthernetMACID[6*ii+1] = (A_UCHAR)((yldStruct.enetID4[0]) & 0xFF);
			EthernetMACID[6*ii+2] = (A_UCHAR)((yldStruct.enetID4[1] >> 8) & 0xFF);
			EthernetMACID[6*ii+3] = (A_UCHAR)((yldStruct.enetID4[1]) & 0xFF);
			EthernetMACID[6*ii+4] = (A_UCHAR)((yldStruct.enetID4[2] >> 8) & 0xFF);
			EthernetMACID[6*ii+5] = (A_UCHAR)((yldStruct.enetID4[2]) & 0xFF);
		}

		// Write the Ethernet MAC Addr
		art_writeProdData(devNum, macID_arr, macID_arr2, EthernetMACID, &(EthernetMACID[6]));

		// starting with viper, write radio_mask in board config info
		if ( ((swDeviceID & 0xFF) >= 0x16) && (configSetup.remote) && !(isPredator(swDeviceID))) { 
			argList[0] = CalSetup.modeMaskForRadio[0];
			argList[1] = CalSetup.modeMaskForRadio[1];
			numArgs = 2;
			art_writeNewProdData(devNum, &(argList[0]), numArgs);
		}

	} 

	virtual_eeprom0Write(devNum, 0x1f, yldStruct.macID1[0]);
	virtual_eeprom0Write(devNum, 0x1e, yldStruct.macID1[1]);
	virtual_eeprom0Write(devNum, 0x1d, yldStruct.macID1[2]);
	virtual_eeprom0Write(devNum, 0xa5, SWAP_BYTES16(yldStruct.macID1[0]));
	virtual_eeprom0Write(devNum, 0xa6, SWAP_BYTES16(yldStruct.macID1[1]));
	virtual_eeprom0Write(devNum, 0xa7, SWAP_BYTES16(yldStruct.macID1[2])); 

}

void ProgramMACID(A_UINT32 devNum)
{
    A_INT32 start = -1, limit = -1, current = -1;
	A_UINT32 existing_macID1, existing_macID2;
    A_INT32 macAddrOUI = -1;
    A_UINT32 offset;
    AR6K_EEPROM *eepromPtr = (AR6K_EEPROM *)dummyEepromWriteArea;
    FILE *fStream;
    fpos_t currentLine;
    char lineBuf[82], *pLine;
    A_BOOL append = FALSE;
    A_UINT32 macAddrMid;

    A_INT32 IPstart = -1, IPlimit = -1, IPcurrent = -1;
    A_UCHAR EthernetMACID[18]; // 3 ethernet macIDs
    A_BOOL IPappend = FALSE;
    A_UINT16 ii, jj;
    A_UCHAR macID_arr[6], macID_arr2[6] ;
    A_INT32		argList[16];
    A_UINT32	numArgs;

    offset = ((A_UINT32)&eepromPtr->baseEepHeader.macAddr - (A_UINT32)eepromPtr) /2;
    if (configSetup.enableLabelScheme) {
        if(isDragon(devNum)) {
	    // programMACIDFromLabel should've been called already
	    uiPrintf("Wrote MAC address: %04X:%04X:%04X\n", art_eepromRead(devNum, offset+2), 
 	        art_eepromRead(devNum, offset+1), art_eepromRead(devNum, offset));
	}
	else {
	    // programMACIDFromLabel should've been called already
	    uiPrintf("Wrote MAC address: %04X:%04X:%04X\n", art_eepromRead(devNum, 0x1f), 
    		art_eepromRead(devNum, 0x1e), art_eepromRead(devNum, 0x1d));
	}
    	return;
    }

    if( (fStream = fopen( CalSetup.macidFile, "r")) == NULL ) {
        uiPrintf("Failed to open %s - the MACID will not be written\n", CalSetup.macidFile);
        return;
    }
    while(fgets(lineBuf, 80, fStream) != NULL) {
        pLine = lineBuf;
        while(isspace(*pLine)) pLine++;
        if(*pLine == '#') {
            continue;
        }
        else if(strnicmp("start", pLine, strlen("start")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
            if(!sscanf(pLine, "%x", &start)) {
                uiPrintf("Unable to read the start field from %s\n", CalSetup.macidFile);
                uiPrintf(" - no new MACID will be written\n");
                fclose(fStream);
                return;
            }
        }
        else if(strnicmp("limit", pLine, strlen("limit")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
            if(!sscanf(pLine, "%x", &limit)) {
                uiPrintf("Unable to read the limit field from %s\n", CalSetup.macidFile);
                uiPrintf(" - no new MACID will be written\n");
                fclose(fStream);
                return;
            }
        }
        else if(strnicmp("current", pLine, strlen("current")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
            if(!sscanf(pLine, "%x", &current)) {
                uiPrintf("Unable to read the current field from %s\n", CalSetup.macidFile);
                uiPrintf(" - no new MACID will be written\n");
                uiPrintf(" - delete the entire \"current=\" line to set the current to start\n");
                fclose(fStream);
                return;
            }
        }        
        else if(strnicmp("MAC_ADDRESS_OUI", pLine, strlen("MAC_ADDRESS_OUI")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
            if(!sscanf(pLine, "%lx", &macAddrOUI)) {
                uiPrintf("Unable to read the MAC_ADDRESS_OUI from %s\n", CalSetup.macidFile);
                uiPrintf(" - no new MACID will be written\n");
                fclose(fStream);
                return;
            }
        }
		else if(strnicmp("IPstart", pLine, strlen("IPstart")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
            if(!sscanf(pLine, "%x", &IPstart) && configSetup.remote ) {
                uiPrintf("Unable to read the IPstart field from %s\n", CalSetup.macidFile);
                uiPrintf(" - no new Ethernet MACID will be written\n");
                fclose(fStream);
                return;
            }
        }
        else if(strnicmp("IPlimit", pLine, strlen("IPlimit")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
            if(!sscanf(pLine, "%x", &IPlimit) && configSetup.remote) {
                uiPrintf("Unable to read the IPlimit field from %s\n", CalSetup.macidFile);
                uiPrintf(" - no new Ethernet MACID will be written\n");
                fclose(fStream);
                return;
            }
        }
        else if(strnicmp("IPcurrent", pLine, strlen("IPcurrent")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
            if(!sscanf(pLine, "%x", &IPcurrent) && configSetup.remote) {
                uiPrintf("Unable to read the IPcurrent field from %s\n", CalSetup.macidFile);
                uiPrintf(" - no new Ethernet MACID will be written\n");
                uiPrintf(" - delete the entire \"IPcurrent=\" line to set the current to start\n");
                fclose(fStream);
                return;
            }
        }        
        
    } // End of line parsing
    fclose(fStream);

    if(start == -1) {
        uiPrintf("The start field is missing from %s\n", CalSetup.macidFile);
        uiPrintf(" - no new MACID will be written\n");
        return;
    }
    if(limit == -1) {
        uiPrintf("The limit field is missing from %s\n", CalSetup.macidFile);
        uiPrintf(" - no new MACID will be written\n");
        return;
    }
    if(macAddrOUI == -1) {
        uiPrintf("The MAC_ADDRESS_OUI field is missing from %s\n", CalSetup.macidFile);
        uiPrintf(" - no new MACID will be written\n");
        return;
    }

	if (configSetup.remote)
	{
		if(IPstart == -1) {
			uiPrintf("The IPstart field is missing from %s\n", CalSetup.macidFile);
			uiPrintf(" - no new Ethernet MACID will be written\n");
			return;
		}
		if(IPlimit == -1) {
			uiPrintf("The IPlimit field is missing from %s\n", CalSetup.macidFile);
			uiPrintf(" - no new Ethernet MACID will be written\n");
			return;
		}
	}
    // Read current MACID - only program if OUI's differ
	if(isDragon(devNum)) {
	    if( (art_eepromRead(devNum, offset+2) == (A_UINT32)((macAddrOUI >> 8) & 0xFFFF)) && 
    	    ((art_eepromRead(devNum, offset+1) >> 8) & 0xFF) == (A_UINT32)(macAddrOUI & 0xFF) ) {

			existing_macID1	= (art_eepromRead(devNum, offset+1) & 0x00FF);
			existing_macID1 = (existing_macID1 << 16) | art_eepromRead(devNum, offset);

			uiPrintf("MAC ADDR already present: %04X:%04X:%04X  \n", art_eepromRead(devNum, offset+2), 
				art_eepromRead(devNum, offset+1), art_eepromRead(devNum, offset));
        	uiPrintf(" - no new MACID will be assigned\n");
			return;
		}
	}

    else if( (art_eepromRead(devNum, 0x1f) == (A_UINT32)((macAddrOUI >> 8) & 0xFFFF)) && 
        ((art_eepromRead(devNum, 0x1e) >> 8) & 0xFF) == (A_UINT32)(macAddrOUI & 0xFF) ) {

		existing_macID1	= (art_eepromRead(devNum, 0x1e) & 0x00FF);
		existing_macID1 = (existing_macID1 << 16) | art_eepromRead(devNum, 0x1d);

		uiPrintf("MAC ADDR already present: %04X:%04X:%04X  \n", art_eepromRead(devNum, 0x1f), 
			art_eepromRead(devNum, 0x1e), art_eepromRead(devNum, 0x1d));
        uiPrintf(" - no new MACID will be assigned\n");

		
		// if macID not set properly in the 18th tuple - fix it.
        if (!configSetup.remote) 
		{
			existing_macID2	= SWAP_BYTES16(art_eepromRead(devNum, 0xa7));
			existing_macID2 |= ((art_eepromRead(devNum, 0xa6) & 0xFF00) << 8);

			if (existing_macID2 != existing_macID1)
			{
				uiPrintf("MAC ID in the 18th tuple found in error.(%X, %X) ", existing_macID1, existing_macID2);
			    macAddrMid = ((macAddrOUI << 8) & 0xFF00) | ((existing_macID1 >> 16) & 0x00FF);
			    //				art_eepromWrite(devNum, 0xa5, SWAP_BYTES16((macAddrOUI >> 8) & 0xFFFF));
				virtual_eeprom0Write(devNum, 0xa5, SWAP_BYTES16((macAddrOUI >> 8) & 0xFFFF));
				//				art_eepromWrite(devNum, 0xa6, SWAP_BYTES16(macAddrMid));
				virtual_eeprom0Write(devNum, 0xa6, SWAP_BYTES16(macAddrMid));
				//				art_eepromWrite(devNum, 0xa7, SWAP_BYTES16(existing_macID1 & 0xFFFF)); 
				virtual_eeprom0Write(devNum, 0xa7, SWAP_BYTES16(existing_macID1 & 0xFFFF)); 
				uiPrintf("Fixed: %04X:%04X:%04X\n", SWAP_BYTES16(art_eepromRead(devNum, 0xa5)), 
					SWAP_BYTES16(art_eepromRead(devNum, 0xa6)), SWAP_BYTES16(art_eepromRead(devNum, 0xa7)));
			}
			return;
		}
    }

	

    // Append current
    if(current == -1) {
        current = start;
        append = TRUE;
    }

    if(current >= limit) {
        uiPrintf("The current mac addres to write has reached the limit\n");
        uiPrintf(" - no new MACID will be written\n");
        return;
    }

    macAddrMid = ((macAddrOUI << 8) & 0xFF00) | ((current >> 16) & 0xFF);
    // Write Mac Addr into EEPROM
	
	if(isDragon(devNum)) {
		macID_arr[0] = (A_UCHAR)((macAddrOUI >> 16) & 0xFF);
		macID_arr[1] = (A_UCHAR)((macAddrOUI >>  8) & 0xFF);
		macID_arr[2] = (A_UCHAR)(macAddrOUI  & 0xFF);
		macID_arr[3] = (A_UCHAR)((current >> 16) & 0xFF);
		macID_arr[4] = (A_UCHAR)((current >>  8) & 0xFF);
		macID_arr[5] = (A_UCHAR)(current  & 0xFF);			
		writeAr6000MacAddress(devNum, macID_arr);
	}
	
	else if (configSetup.remote  && !CalSetup.staCardOnAP)
	{

		for (ii=0; ii<6; ii++) {
			macID_arr[ii] = 0xFF;
			macID_arr2[ii] = 0xFF;
		}
		
		if (CalSetup.modeMaskForRadio[0] > 0) {
			macID_arr[0] = (A_UCHAR)((macAddrOUI >> 16) & 0xFF);
			macID_arr[1] = (A_UCHAR)((macAddrOUI >>  8) & 0xFF);
			macID_arr[2] = (A_UCHAR)(macAddrOUI  & 0xFF);
			macID_arr[3] = (A_UCHAR)((current >> 16) & 0xFF);
			macID_arr[4] = (A_UCHAR)((current >>  8) & 0xFF);
			macID_arr[5] = (A_UCHAR)(current  & 0xFF);			
		}

		if (CalSetup.modeMaskForRadio[1] > 0) {		
			if (CalSetup.modeMaskForRadio[0] > 0) current++;
			macID_arr2[0] = (A_UCHAR)((macAddrOUI >> 16) & 0xFF);
			macID_arr2[1] = (A_UCHAR)((macAddrOUI >>  8) & 0xFF);
			macID_arr2[2] = (A_UCHAR)(macAddrOUI  & 0xFF);
			macID_arr2[3] = (A_UCHAR)((current >> 16) & 0xFF);
			macID_arr2[4] = (A_UCHAR)((current >>  8) & 0xFF);
			macID_arr2[5] = (A_UCHAR)(current  & 0xFF);
		} 

	} else
	{
		art_eepromWrite(devNum, 0x1f, (macAddrOUI >> 8) & 0xFFFF);
		art_eepromWrite(devNum, 0x1e, macAddrMid);
		art_eepromWrite(devNum, 0x1d, (current & 0xFFFF));
		art_eepromWrite(devNum, 0xa5, SWAP_BYTES16((macAddrOUI >> 8) & 0xFFFF));
		art_eepromWrite(devNum, 0xa6, SWAP_BYTES16(macAddrMid));
		art_eepromWrite(devNum, 0xa7, SWAP_BYTES16(current & 0xFFFF)); 

/*
		virtual_eeprom0Write(devNum, 0x1f, (macAddrOUI >> 8) & 0xFFFF);
		virtual_eeprom0Write(devNum, 0x1e, macAddrMid);
		virtual_eeprom0Write(devNum, 0x1d, (current & 0xFFFF));
		virtual_eeprom0Write(devNum, 0xa5, SWAP_BYTES16((macAddrOUI >> 8) & 0xFFFF));
		virtual_eeprom0Write(devNum, 0xa6, SWAP_BYTES16(macAddrMid));
		virtual_eeprom0Write(devNum, 0xa7, SWAP_BYTES16(current & 0xFFFF)); 
*/
		uiPrintf("Wrote MAC address: %04X:%04X:%04X\n", art_eepromRead(devNum, 0x1f), 
			art_eepromRead(devNum, 0x1e), art_eepromRead(devNum, 0x1d));
	}
	
    current++;

    if(current >= (limit - 5)) {
        uiPrintf("WARNING: the next MAC ADDR to write is within 5 of the limit value\n");
    }

    if(append) {
        if( (fStream = fopen( CalSetup.macidFile, "a")) == NULL ) {
            uiPrintf("Failed to open %s - the \"current\" value cannot be written\n", CalSetup.macidFile);
            return;
        }
        fprintf(fStream, "\ncurrent=%06x;\n", current);
    }
    else {
        if( (fStream = fopen( CalSetup.macidFile, "r+")) == NULL ) {
            uiPrintf("Failed to open %s - the \"current\" value cannot be updated\n", CalSetup.macidFile);
            return;
        }
        fgetpos(fStream, &currentLine);
        while(fgets(lineBuf, 80, fStream) != NULL) {
            pLine = lineBuf;
            while(isspace(*pLine)) pLine++;
            if(*pLine == '#') {
                continue;
            }
            else if(strnicmp("current", pLine, strlen("current")) == 0) {
                fsetpos(fStream, &currentLine);
                fprintf(fStream, "current=%06x;\n", current);
                break;  // Leave while loop
            }
            fgetpos(fStream, &currentLine);
        }
    }
 
	fclose(fStream);
	if(isDragon(devNum)) {
		return;
	}

	if (configSetup.remote)
	{
		// Append current
		if(IPcurrent == -1) {
			IPcurrent = IPstart;
			IPappend = TRUE;
		}

		if(IPcurrent >= IPlimit) {
			uiPrintf("The current ethernet mac addres to write has reached the limit\n");
			uiPrintf(" - no new Ethernet MACID will be written\n");
			return;
		}

		if (!(isPredator(swDeviceID)) && CalSetup.numEthernetPorts < 1)
		{
			uiPrintf("Bad value for NUM_ETHERNET_PORTS given in calsetup.txt: %d\n", CalSetup.numEthernetPorts);
			uiPrintf("overriding it with 1. Please fix the file.\n");
			CalSetup.numEthernetPorts = 1;
		}

		for (jj = 0; jj < CalSetup.numEthernetPorts; jj++)
		{
			ii = (A_UINT16)(jj + CalSetup.startEthernetPort);		
			EthernetMACID[0+6*ii] = (A_UCHAR)(macAddrOUI >> 16) & 0xFF;
			EthernetMACID[1+6*ii] = (A_UCHAR)(macAddrOUI >>  8) & 0xFF;
			EthernetMACID[2+6*ii] = (A_UCHAR)(macAddrOUI  & 0xFF);
			EthernetMACID[3+6*ii] = (A_UCHAR)(IPcurrent >> 16) & 0xFF;
			EthernetMACID[4+6*ii] = (A_UCHAR)(IPcurrent >>  8) & 0xFF;
			EthernetMACID[5+6*ii] = (A_UCHAR)(IPcurrent  & 0xFF);
			uiPrintf("Setting Ethernet MAC ID %d to : %04X%04X:%04X%04X:%04X%04X\n", jj, 
				EthernetMACID[0+6*ii], EthernetMACID[1+6*ii], EthernetMACID[2+6*ii],
				EthernetMACID[3+6*ii], EthernetMACID[4+6*ii], EthernetMACID[5+6*ii]);
			
			IPcurrent++;
		}
		
		if (CalSetup.numEthernetPorts < 2)
		{
			if (CalSetup.startEthernetPort == 0) {
				for(ii=6; ii<12; ii++)	EthernetMACID[ii] = 0xFF;			
			} else {
				for(ii=0; ii<6; ii++)	EthernetMACID[ii] = 0xFF;			
			}
		}

		// Write the Ethernet MAC Addr
		art_writeProdData(devNum, macID_arr, macID_arr2, EthernetMACID, &(EthernetMACID[6]));

		// starting with viper, write radio_mask in board config info
		if ( ((swDeviceID & 0xFF) >= 0x16) && (configSetup.remote) && !(isPredator(swDeviceID))) { 
			argList[0] = CalSetup.modeMaskForRadio[0];
			argList[1] = CalSetup.modeMaskForRadio[1];
			numArgs = 2;			
			art_writeNewProdData(devNum, &(argList[0]), numArgs);
		}

	    if(IPcurrent >= (IPlimit - 5)) {
			uiPrintf("WARNING: the next Ethernet MAC ADDR to write is within 5 of the IPlimit value\n");
		}

		if(IPappend) {
			if( (fStream = fopen( CalSetup.macidFile, "a")) == NULL ) {
				uiPrintf("Failed to open %s - the \"IPcurrent\" value cannot be written\n", CalSetup.macidFile);
				return;
			}
			fprintf(fStream, "\nIPcurrent=%06x;\n", IPcurrent);
		}
		else {
			if( (fStream = fopen( CalSetup.macidFile, "r+")) == NULL ) {
				uiPrintf("Failed to open %s - the \"current\" value cannot be updated\n", CalSetup.macidFile);
				return;
			}
			fgetpos(fStream, &currentLine);
			while(fgets(lineBuf, 80, fStream) != NULL) {
				pLine = lineBuf;
				while(isspace(*pLine)) pLine++;
				if(*pLine == '#') {
					continue;
				}
				else if(strnicmp("IPcurrent", pLine, strlen("IPcurrent")) == 0) {
					fsetpos(fStream, &currentLine);
					fprintf(fStream, "IPcurrent=%06x;\n", IPcurrent);
					break;  // Leave while loop
				}
				fgetpos(fStream, &currentLine);
			}
		}

	}

	fclose(fStream);
}

void reset_on_off(A_UINT32 devNum) {
    art_regWrite(devNum, 0x4000,(art_regRead(devNum,0x4000)|0xf));
    art_regWrite(devNum, 0x4000,0);
}


char *qq( char *format, ...) {

    static char buf[512];
    va_list vl;

    va_start( vl, format );
    vsprintf( buf, format, vl );
    va_end( vl );
    return buf;

}

void read_dataset_from_file(dPCDACS_EEPROM *pDataSet, char *filename)
{
	A_UINT16	i, j;
	FILE *fStream;
	char lineBuf[1023], *pLine;
	char seps[] = " \t\n";
	char *token;
	A_UINT16 gotChannels = 0;
	A_UINT16 channel;
	double power;
	A_UINT16 pcdac;

    uiPrintf("\nReading from file %s\n", filename);
    if( (fStream = fopen( filename, "r")) == NULL ) {
        uiPrintf("Failed to open %s - using Defaults\n", filename);
        return;
    }

	j=0; //represents pcdac index

    while(fgets(lineBuf, 1020, fStream) != NULL) {
        pLine = lineBuf;
        while(isspace(*pLine)) pLine++;
        if(*pLine == '#') {
            continue;
        }

		if(gotChannels < 1) // read only the first line this way
		{
			token = strtok(pLine, seps);
			i = 0;
			while (token != NULL)
			{
				sscanf(token, "%d", &channel);
				token = strtok(NULL, seps);
				pDataSet->pChannels[i] = channel;
				pDataSet->pDataPerChannel[i].channelValue = channel;
				i++;
			}
			if (i != pDataSet->numChannels)
			{
				uiPrintf("Expected # of Channels: %d, Actual # of channels in file %s : %d\nExiting...\n", 
									pDataSet->numChannels,
									filename, i);
				exit(0);
			}
			gotChannels = 1;
			continue;
		}

		token = strtok(pLine, seps);
		sscanf(token, "%d", &pcdac);
		if (CalSetup.customerDebug)
			uiPrintf("reading for pcdac =%d\n", pcdac);

		token = strtok(NULL, seps);
		i = 0;
		while (token != NULL)
		{
			sscanf(token, "%lf", &power);
			token = strtok(NULL, seps);
			pDataSet->pDataPerChannel[i].pPwrValues[j] = power;
			pDataSet->pDataPerChannel[i].pPcdacValues[j] = pcdac;
			i++;
		}
		
		j++;
	}

	if (j != pDataSet->pDataPerChannel[0].numPcdacValues)
	{
		uiPrintf("Expected # of Pcdacs: %d, Actual # of pcdacs in file %s : %d\nExiting...\n", 
							pDataSet->pDataPerChannel[0].numPcdacValues,
							filename, j);
		exit(0);
	}

	if (fStream) fclose(fStream);
}

void goldenTest()
{
	A_UINT32 devNum  = devNumArr[MODE_11a];
	A_UINT32 jumbo_frame_mode_save;

	if (configSetup.validInstance == 2) {
		devNum = devNumArr[MODE_11g];
	}


	if(!configSetup.enableLabelScheme)
	{
		getDutTargetPowerFilename();// kill this when sync over ethernet.

		if(!parseTargets()) 
		{
			uiPrintf("An error occured while parsing the file %s. Pl. check for format errors.\n", CalSetup.tgtPwrFilename);
		}
	}

	uiPrintf("\nGolden Unit Program is running. Hit any key to quit...\n");
	while(!kbhit())
	{
			do_11g_CCK = FALSE;
			REWIND_TEST = FALSE;
			configSetup.eepromLoad = 1;
			art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
							(A_BOOL)1, (A_UCHAR)MODE_11A, configSetup.use_init);		

//golden does not need to reread prom every time round loop
//			printf("After art_setResetParams()\n");
//			art_rereadProm(devNum);
//			printf("After art_rereadProm()\n");
			
		//	printf("NEED_GAIN_OPT_FOR_MODE[MODE_11a] = %c\n",NEED_GAIN_OPT_FOR_MODE[MODE_11a]);
			if (NEED_GAIN_OPT_FOR_MODE[MODE_11a])
			{
				setGainLadderForMaxGain(pCurrGainLadder);
				programNewGain(pCurrGainLadder, devNum, 0);
			}

			art_resetDevice(devNum, txStation, bssID, 5220, 0);


			if ( CalSetup.useFastCal || CalSetup.testTXPER || CalSetup.testRXSEN ||
				 CalSetup.testTXPER_2p4[MODE_11b] || CalSetup.testRXSEN_2p4[MODE_11b] ||
				 CalSetup.testTXPER_2p4[MODE_11g] || CalSetup.testRXSEN_2p4[MODE_11g] ||
				 CalSetup.testDataIntegrity[MODE_11a] || CalSetup.testDataIntegrity[MODE_11g] ||
				 CalSetup.testDataIntegrity[MODE_11b] || (CalSetup.calPower && CalSetup.do_iq_cal) ||
				 CalSetup.testThroughput[MODE_11a] || CalSetup.testThroughput[MODE_11g] ||
			     CalSetup.testThroughput[MODE_11b])			
			{
				waitForSync(CalSetup.customerDebug);		
				sendAck(devNum, "Comm Established", 5220, 0, 0, CalSetup.customerDebug);
				//if we are using the label scheme, we need to wait for a sync from the DUT to set the ssid
				if(configSetup.enableLabelScheme) 
				{
					waitForAck(CalSetup.customerDebug);
					if (!verifyAckStr("Sending DUT SSID"))
					{
						uiPrintf("acks out of sync. expected -->Sending DUT SSID<--\n");
						exit(0);
					}
					
					configSetup.dutSSID = ackRecvPar1;

					getDutTargetPowerFilename(); // kill this when sync over ethernet.

					if(!parseTargets()) {
						uiPrintf("An error occured while parsing the file %s. Pl. check for format errors.\n", CalSetup.tgtPwrFilename);
					}
				}
				if(configSetup.computeCalsetupName) {
					waitForAck(CalSetup.customerDebug);
					memcpy(calsetupFileName, ackRecvStr, sizeof(A_UCHAR)*strlen(ackRecvStr));
					//FJC, need to add null terminator
					calsetupFileName[sizeof(A_UCHAR)*strlen(ackRecvStr)] = '\0';
					uiPrintf("Parsing Calsetup File : %s\n", calsetupFileName);
					parseSetup(devNum);
				}				
			}

         if(CalSetup.doCCKin11g) {
			   if(DUT_SUPPORTS_MODE[MODE_11g]) DUT_SUPPORTS_MODE[MODE_11b] = TRUE;
         }
//			if (1) {
			if (CalSetup.do_iq_cal) {
				waitForAck(CalSetup.customerDebug);
				while (!verifyAckStr("Done with iq_cal")) 
				{
					uiPrintf("Beginning iq_cal for mode %s\n", modeName[calModeFor[ackRecvPar1]]);
					golden_iq_cal(devNumArr[calModeFor[ackRecvPar1]], ackRecvPar1, ackRecvPar2); // mode and channel
					if (REWIND_TEST) {
						break;
					}
					waitForAck(CalSetup.customerDebug);
				}
			}

			if (REWIND_TEST) {
				closeComms();
				continue;
			}


			if (CalSetup.useFastCal && CalSetup.calPower)
			{

				configSetup.eepromLoad = 1;
				art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
									(A_BOOL)1, MODE_11A, configSetup.use_init);		
				waitForAck(CalSetup.customerDebug);
				if (!verifyAckStr("DUT relinquished instrument control. Go ahead GU."))
				{
					uiPrintf("acks out of sync. expected -->DUT relinquished instrument control. Go ahead GU.<--\n");
					exit(0);
				}
				gpibRSC(0,1); // GU request system controller (1)

				uiPrintf("\nSetting up Power Meter");
 				guDevPM = pmInit(CalSetup.pmGPIBaddr, CalSetup.pmModel);

				uiPrintf("\nSetting up Attenuator");
				guDevAtt = attInit(CalSetup.attGPIBaddr, CalSetup.attModel);
				attSet(guDevAtt, MAX_ATTENUATOR_RANGE);
				
				setupGoldenForCal(devNum, CalSetup.customerDebug);
				
				waitForAck(CalSetup.customerDebug);
				gpibONL(guDevAtt, 0);	// take att offline. relinquish for dut.
				gpibONL(guDevPM, 0);	// take att offline. relinquish for dut.
				sendAck(devNum, "Golden Unit Relinquished Instrument Control", 0, 0, 0, CalSetup.customerDebug);
			}

			if(DUT_SUPPORTS_MODE[MODE_11a] && ((CalSetup.testTXPER) || (CalSetup.testRXSEN)))
			{
				configSetup.eepromLoad = 1;
				art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
									(A_BOOL)1, MODE_11A, configSetup.use_init);		

				waitForAck(CalSetup.customerDebug);
				if (REWIND_TEST) {
					closeComms();
					continue;
				}
				sendAck(devNumArr[MODE_11a], "start TXRX test", 5220, 0, 0, CalSetup.customerDebug);
			
				goldenTestTXRX(devNumArr[MODE_11a], MODE_11a);

				if (CalSetup.testTXPER_margin) {

					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						closeComms();
						continue;
					}
					sendAck(devNumArr[MODE_11a], "start TXRX +1dB margin test", 5220, 0, 0, CalSetup.customerDebug);
					TX_PER_STRESS_MARGIN = 1;				
					goldenTestTXRX(devNumArr[MODE_11a], MODE_11a);
					TX_PER_STRESS_MARGIN = 0;

					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						closeComms();
						continue;
					}
					sendAck(devNumArr[MODE_11a], "start TXRX +2dB margin test", 5220, 0, 0, CalSetup.customerDebug);
					TX_PER_STRESS_MARGIN = 2;				
					goldenTestTXRX(devNumArr[MODE_11a], MODE_11a);
					TX_PER_STRESS_MARGIN = 0;
				}
			}

			if(DUT_SUPPORTS_MODE[MODE_11a] && CalSetup.testDataIntegrity[MODE_11a])
			{
				configSetup.eepromLoad = 1;
				art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
									(A_BOOL)1, MODE_11A, configSetup.use_init);		
				art_resetDevice(devNumArr[MODE_11a], rxStation, bssID, 5200, 0);

				waitForAck(CalSetup.customerDebug);
				if (REWIND_TEST) {
					closeComms();
					continue;
				}
				sendAck(devNumArr[MODE_11a], "start 11a verifyDataTXRX test", 5220, 0, 0, CalSetup.customerDebug);
			
				goldenVerifyDataTXRX(devNumArr[MODE_11a], MODE_11a, 0, 6, VERIFY_DATA_PACKET_LEN);
			}
		
			
			if(DUT_SUPPORTS_MODE[MODE_11a] && CalSetup.testThroughput[MODE_11a])
			{
				configSetup.eepromLoad = 1;
				art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
									(A_BOOL)1, MODE_11A, configSetup.use_init);		
				art_resetDevice(devNumArr[MODE_11a], rxStation, bssID, 5200, 0);
				waitForAck(CalSetup.customerDebug);
				if (REWIND_TEST) {
					closeComms();
					continue;
			}
				sendAck(devNumArr[MODE_11a], "start 11a throughput test", 5220, 0, 0, CalSetup.customerDebug);
		
				goldenThroughputTest(devNumArr[MODE_11a], MODE_11a, 0, 54, THROUGHPUT_PACKET_SIZE, NUM_THROUGHPUT_PACKETS);

				
				if(CalSetup.testTURBO) {
					art_resetDevice(devNumArr[MODE_11a], rxStation, bssID, 5200, 0);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						closeComms();
						continue;
					}
					sendAck(devNumArr[MODE_11a], "start 11a throughput turbo test", 5220, 0, 0, CalSetup.customerDebug);
			
   					art_writeField(devNumArr[MODE_11a], "bb_max_rx_length", 0xFFF);
					art_writeField(devNumArr[MODE_11a], "bb_en_err_length_illegal", 0);
					jumbo_frame_mode_save = art_getFieldForMode(devNumArr[MODE_11a], "mc_jumbo_frame_mode", MODE_11a, 1);
					art_writeField(devNumArr[MODE_11a], "mc_jumbo_frame_mode", 0);
					goldenThroughputTest(devNumArr[MODE_11a], MODE_11a, 1, 54, THROUGHPUT_TURBO_PACKET_SIZE, NUM_THROUGHPUT_PACKETS);
					art_writeField(devNumArr[MODE_11a], "mc_jumbo_frame_mode", jumbo_frame_mode_save);
				}

			}
			
	        if(kbhit())
		        break;
			if (REWIND_TEST) {
				closeComms();
				continue;
			}

			if(DUT_SUPPORTS_MODE[MODE_11g]) {
				goldenTest_2p4(devNumArr[MODE_11g], MODE_11g);
			}
	        if(kbhit())
		        break;
			if (REWIND_TEST) {
				closeComms();
				continue;
			}

			if(DUT_SUPPORTS_MODE[MODE_11b]) {
				goldenTest_2p4(devNumArr[MODE_11b], MODE_11b);
			}
	        if(kbhit())
		        break;

			closeComms();
	}
    while (kbhit())	{	// clean up the buffer
        getch();
    }
}

#ifdef _IQV
void free_TargetsSet()
{
	if (pTargetsSet->pTargetChannel != NULL) {
		free(pTargetsSet->pTargetChannel);
		pTargetsSet->pTargetChannel = NULL;
	}
	if (pTargetsSet_2p4[MODE_11g]->pTargetChannel != NULL) {
		free(pTargetsSet_2p4[MODE_11g]->pTargetChannel);
		pTargetsSet_2p4[MODE_11g]->pTargetChannel = NULL;
	}
	if (pTargetsSet_2p4[MODE_11b]->pTargetChannel != NULL) {
		free(pTargetsSet_2p4[MODE_11b]->pTargetChannel);
		pTargetsSet_2p4[MODE_11b]->pTargetChannel = NULL;
	}
	if (pTestGroupSet->pTestGroup != NULL) {
		free(pTestGroupSet->pTestGroup);
		pTestGroupSet->pTestGroup = NULL;
	}
}
#endif // _IQV

A_UINT16 parseTargets(void)
{
    FILE *fStream;
    char lineBuf[122], *pLine;
	A_UINT16	ii, jj ;
	A_BOOL		parsingTP = FALSE ;
	A_BOOL		parsingTP_11b = FALSE ;
	A_BOOL		parsingTP_11g = FALSE ;
	A_BOOL		parsingTG = FALSE ;
	A_UINT16	parsingLine1 = 1;
	A_UINT16	parsingLine2 = 0;
	A_UINT16	parsingLine3 = 0;
	char delimiters[] = " \t\n\r" ;
	double   tmpRead;

#ifdef _IQV
	if (pTestGroupSet->pTestGroup != NULL)
		return TRUE;
#endif

	pTargetsSet->pTargetChannel = (TARGET_CHANNEL *)malloc(sizeof(TARGET_CHANNEL) * NUM_TARGET_POWER_CHANNELS) ;
	if (pTargetsSet->pTargetChannel == NULL) {
		uiPrintf("Unable to allocate TargetsSet\n");
		return(0);
	}

	pTargetsSet_2p4[MODE_11g]->pTargetChannel = (TARGET_CHANNEL *)malloc(sizeof(TARGET_CHANNEL) * NUM_TARGET_POWER_CHANNELS_11g) ;
	if (pTargetsSet_2p4[MODE_11g]->pTargetChannel == NULL) {
		uiPrintf("Unable to allocate TargetsSet_2p4[MODE_11g]\n");
		return(0);
	}

	pTargetsSet_2p4[MODE_11b]->pTargetChannel = (TARGET_CHANNEL *)malloc(sizeof(TARGET_CHANNEL) * NUM_TARGET_POWER_CHANNELS_11b) ;
	if (pTargetsSet_2p4[MODE_11b]->pTargetChannel == NULL) {
		uiPrintf("Unable to allocate TargetsSet_2p4[MODE_11b]\n");
		return(0);
	}


	pTestGroupSet->pTestGroup = (TEST_GROUP_DATA *)malloc(sizeof(TEST_GROUP_DATA) * MAX_NUM_CTL) ;
	if (pTestGroupSet->pTestGroup == NULL) {
		uiPrintf("Unable to allocate TestGroups Set\n");
		return(0);
	}

	memset(pTestGroupSet->pTestGroup, 0, sizeof(TEST_GROUP_DATA) * MAX_NUM_CTL);
	ii=0;
	jj=0;


    uiPrintf("\nReading in Target Powers and CTL Info from %s\n", CalSetup.tgtPwrFilename);
    if( (fStream = fopen( CalSetup.tgtPwrFilename, "r")) == NULL ) {
        uiPrintf("Failed to open %s - using Defaults\n", CalSetup.tgtPwrFilename);
        return(0);
    }

    while(fgets(lineBuf, 120, fStream) != NULL) {
        pLine = lineBuf;
        while(isspace(*pLine)) pLine++;

		if(strnicmp("#BEGIN_11a_TARGET_POWER_TABLE", pLine, strlen("#BEGIN_11a_TARGET_POWER_TABLE")) == 0)  {
            parsingTP = TRUE;
			ii = 0;
			continue;
        }

		if(strnicmp("#BEGIN_11b_TARGET_POWER_TABLE", pLine, strlen("#BEGIN_11b_TARGET_POWER_TABLE")) == 0)  {
            parsingTP_11b = TRUE;
			ii = 0;
			continue;
        }

		if(strnicmp("#BEGIN_11g_TARGET_POWER_TABLE", pLine, strlen("#BEGIN_11g_TARGET_POWER_TABLE")) == 0)  {
            parsingTP_11g = TRUE;
			ii = 0;
			continue;
        }

		if(strnicmp("#BEGIN_TEST_GROUPS", pLine, strlen("#BEGIN_TEST_GROUPS")) == 0)  {
            parsingTG = TRUE;
			continue;
        }

        if((*pLine == '#') && !(parsingTP || parsingTG)) {
            continue;
        }

        while(parsingTP && (fgets(lineBuf, 120, fStream) != NULL)) {
			pLine = lineBuf;
			pLine = strtok(pLine, delimiters);
		    // while(isspace(*pLine)) pLine++;
			if(pLine == NULL) continue;

			if(strnicmp("#END_11a_TARGET_POWER_TABLE", pLine, strlen("#END_11a_TARGET_POWER_TABLE")) == 0)  {
		        parsingTP = FALSE;
				pTargetsSet->numTargetChannelsDefined = ii ;
			}

			if (pLine[0] == '#') {
				continue ;
			}
			if (!sscanf(pLine, "%d", &(pTargetsSet->pTargetChannel[ii].channelValue))) {
				uiPrintf("Could not read the test frequency %d from token %s\n", ii, pLine);
			}

			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(tmpRead))) {
				uiPrintf("Could not read the 6-24 Mbps target for test frequency %d from token %s\n", ii, pLine);
			} else
			{
                            pTargetsSet->pTargetChannel[ii].Target24 = tmpRead - getPowerRef();
			}
			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(pTargetsSet->pTargetChannel[ii].Target36))) {
				uiPrintf("Could not read the 36 Mbps target for test frequency %d from token %s\n", ii, pLine);
			}
			pLine = strtok(NULL, delimiters);
                        (pTargetsSet->pTargetChannel[ii].Target36) -= getPowerRef();
			if (!sscanf(pLine, "%lf", &(pTargetsSet->pTargetChannel[ii].Target48))) {
				uiPrintf("Could not read the 48 Mbps target for test frequency %d from token %s\n", ii, pLine);
			}
                        pTargetsSet->pTargetChannel[ii].Target48 -= getPowerRef();
			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(tmpRead))) {
				uiPrintf("Could not read the 54 Mbps target for test frequency %d from token %s\n", ii, pLine);
			} else
			{
			    pTargetsSet->pTargetChannel[ii].Target54 = tmpRead - getPowerRef() ;
			}

			ii++ ;

			if(ii > NUM_TARGET_POWER_CHANNELS) {
				uiPrintf("ERROR: Attempt to define more than %d target channels. Fix the %s file\n", NUM_TARGET_POWER_CHANNELS, CalSetup.tgtPwrFilename);
				return(0);
			}
		} // done parsing target power for 11a
			
        while(parsingTP_11g && (fgets(lineBuf, 120, fStream) != NULL)) {
			pLine = lineBuf;
			pLine = strtok(pLine, delimiters);
		    // while(isspace(*pLine)) pLine++;
			if(pLine == NULL) continue;

			if(strnicmp("#END_11g_TARGET_POWER_TABLE", pLine, strlen("#END_11g_TARGET_POWER_TABLE")) == 0)  {
		        parsingTP_11g = FALSE;
				pTargetsSet_2p4[MODE_11g]->numTargetChannelsDefined = ii ;
			}

			if (pLine[0] == '#') {
				continue ;
			}
			if (!sscanf(pLine, "%d", &(pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].channelValue))) {
				uiPrintf("Could not read the test frequency %d from token %s\n", ii, pLine);
			}

			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(tmpRead))) {
				uiPrintf("Could not read the 6-24 Mbps target for test frequency %d from token %s\n", ii, pLine);
			} else
			{
		            pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target24 = tmpRead - getPowerRef();
			}
			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target36))) {
				uiPrintf("Could not read the 36 Mbps target for test frequency %d from token %s\n", ii, pLine);
			}
			pLine = strtok(NULL, delimiters);
			pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target36 -= getPowerRef();
			if (!sscanf(pLine, "%lf", &(pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target48))) {
				uiPrintf("Could not read the 48 Mbps target for test frequency %d from token %s\n", ii, pLine);
			}
			pLine = strtok(NULL, delimiters);
			pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target48 -= getPowerRef();
			if (!sscanf(pLine, "%lf", &(tmpRead))) {
				uiPrintf("Could not read the 54 Mbps target for test frequency %d from token %s\n", ii, pLine);
			} else
			{
			    pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target54 = tmpRead- getPowerRef();
			}

			ii++ ;

			if(ii > NUM_TARGET_POWER_CHANNELS_11g) {
				uiPrintf("ERROR: Attempt to define more than %d test channels for ofdm@2p4 mode. Fix the %s file\n", NUM_TARGET_POWER_CHANNELS_11g, CalSetup.tgtPwrFilename);
				return(0);
			}
		} // done parsing target power for ofdm@2p4

		while(parsingTP_11b && (fgets(lineBuf, 120, fStream) != NULL)) {
			pLine = lineBuf;
			pLine = strtok(pLine, delimiters);
		    // while(isspace(*pLine)) pLine++;
			if(pLine == NULL) continue;

			if(strnicmp("#END_11b_TARGET_POWER_TABLE", pLine, strlen("#END_11b_TARGET_POWER_TABLE")) == 0)  {
		        parsingTP_11b = FALSE;
				pTargetsSet_2p4[MODE_11b]->numTargetChannelsDefined = ii ;
			}

			if (pLine[0] == '#') {
				continue ;
			}
			if (!sscanf(pLine, "%d", &(pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].channelValue))) {
				uiPrintf("Could not read the test frequency %d from token %s\n", ii, pLine);
			}

			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(tmpRead))) {
				uiPrintf("Could not read the 1 Mbps target for test frequency %d from token %s\n", ii, pLine);
			} else
			{
			    pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target24 = tmpRead - getPowerRef();
			}
			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target36))) {
				uiPrintf("Could not read the 2 Mbps target for test frequency %d from token %s\n", ii, pLine);
			}
                        pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target36 -= getPowerRef();
			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target48))) {
				uiPrintf("Could not read the 5 Mbps target for test frequency %d from token %s\n", ii, pLine);
			}
                        pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target48 -= getPowerRef();
			pLine = strtok(NULL, delimiters);
			if (!sscanf(pLine, "%lf", &(tmpRead))) {
				uiPrintf("Could not read the 11 Mbps target for test frequency %d from token %s\n", ii, pLine);
			} else
			{
                            pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target54 = tmpRead - getPowerRef();
			}

			ii++ ;

			if(ii > NUM_TARGET_POWER_CHANNELS_11b) {
				uiPrintf("ERROR: Attempt to define more than %d test channels for 11b mode. Fix the %s file\n", NUM_TARGET_POWER_CHANNELS_11b, CalSetup.tgtPwrFilename);
				return(0);
			}


		} // done parsing target power for 11b
			
        while( parsingTG && (fgets(lineBuf, 120, fStream) != NULL)) {
			pLine = lineBuf;
			pLine = strtok(pLine, delimiters);
		    // while(isspace(*pLine)) pLine++;
			if(pLine == NULL) continue;

			if(strnicmp("#END_TEST_GROUPS", pLine, strlen("#END_TEST_GROUPS")) == 0)  {
		        parsingTG = FALSE;
				pTestGroupSet->numTestGroupsDefined = jj ;
				continue ;
			}

			if (pLine[0] == '#') continue ;


			if (parsingLine1 > 0) {

				pTestGroupSet->pTestGroup[jj].TG_Code = (A_UINT16) strtoul(pLine, NULL, 0);
				pLine = strtok(NULL, delimiters);					

				ii=0;
				while ( (pLine != NULL) && (ii < NUM_BAND_EDGES) && (pLine[0] != '#'))
				{
					if (!sscanf(pLine, "%d", &(pTestGroupSet->pTestGroup[jj].BandEdge[ii++]))) {
						uiPrintf("Could not read the band edge %d for CTL %d from token %s\n", ii-1, jj, pLine);
					}
					pLine = strtok(NULL, delimiters);					
				}	

				pTestGroupSet->pTestGroup[jj].numBandEdgesDefined = ii ;					
	
				parsingLine1 = 0 ;
				parsingLine2 = 1 ;
				continue;
			} 

			if (parsingLine2 > 0)
			{
				ii = 0;
				while ( (pLine != NULL) && (ii < pTestGroupSet->pTestGroup[jj].numBandEdgesDefined))
				{
					if (!sscanf(pLine, "%lf", &(pTestGroupSet->pTestGroup[jj].maxEdgePower[ii++]))) {
						uiPrintf("Could not read the power for band edge %d for CTL %d from token %s\n", ii-1, jj, pLine);
					}
					pLine = strtok(NULL, delimiters);
				}	

				parsingLine2 = 0 ;
				parsingLine3 = 1 ;
				continue;
			}

			if (parsingLine3 > 0)
			{

				ii = 0;
				while ( (pLine != NULL) && (ii < pTestGroupSet->pTestGroup[jj].numBandEdgesDefined))
				{
					if (!sscanf(pLine, "%d", &(pTestGroupSet->pTestGroup[jj].inBandFlag[ii++]))) {
						uiPrintf("Could not read the inband flag %d for CTL %d from token %s\n", ii-1, jj, pLine);
					}
					pLine = strtok(NULL, delimiters);
				}	

				if (!( (pLine == NULL) || (pLine[0] == '#') ) && sscanf(pLine, "%d", &(parsingLine3)) )
				{
					uiPrintf("Incorrect specification of in-band flags for CTL %d. [%d]\n", jj, parsingLine3);
					uiPrintf("pline = %s, read %d back.\n", pLine, parsingLine3);
					exit(0);
				}

				jj++;

				parsingLine3 = 0 ;
				parsingLine1 = 1 ;
				continue;
			}


			if(jj > MAX_NUM_CTL) {
				uiPrintf("ERROR: Attempt to define more than %d CTLs. Fix the %s file\n", MAX_NUM_CTL, CalSetup.tgtPwrFilename);
				return(0);
			}
		} // done parsing Test Groups

	} // end of file
	if (fStream)
		fclose(fStream);

	return(1);  // success
}

void showTargets (void)
{
	A_UINT16 ii, jj;

	uiPrintf("Target Channels: %d\nChannels  ", pTargetsSet->numTargetChannelsDefined);
	for(ii=0; ii<pTargetsSet->numTargetChannelsDefined; ii++)
	{
		uiPrintf("%6d", pTargetsSet->pTargetChannel[ii].channelValue);
	}
	uiPrintf("\n6-24 Mbps ");
	for(ii=0; ii<pTargetsSet->numTargetChannelsDefined; ii++)
	{
		uiPrintf("  %4.1f", pTargetsSet->pTargetChannel[ii].Target24);
	}
	uiPrintf("\n  36 Mbps ");
	for(ii=0; ii<pTargetsSet->numTargetChannelsDefined; ii++)
	{
		uiPrintf("  %4.1f", pTargetsSet->pTargetChannel[ii].Target36);
	}
	uiPrintf("\n  48 Mbps ");
	for(ii=0; ii<pTargetsSet->numTargetChannelsDefined; ii++)
	{
		uiPrintf("  %4.1f", pTargetsSet->pTargetChannel[ii].Target48);
	}
	uiPrintf("\n  54 Mbps ");
	for(ii=0; ii<pTargetsSet->numTargetChannelsDefined; ii++)
	{
		uiPrintf("  %4.1f", pTargetsSet->pTargetChannel[ii].Target54);
	}	

	for(jj=0; jj<pTestGroupSet->numTestGroupsDefined; jj++)
	{
		uiPrintf("\nTG%d code: %x\nBand Edges  ", jj, pTestGroupSet->pTestGroup[jj].TG_Code);
		for(ii=0; ii<pTestGroupSet->pTestGroup[jj].numBandEdgesDefined; ii++)
		{
			uiPrintf("%6d", pTestGroupSet->pTestGroup[jj].BandEdge[ii]);
		}
		uiPrintf("\nMax Power   ");
		for(ii=0; ii<pTestGroupSet->pTestGroup[jj].numBandEdgesDefined; ii++)
		{
			uiPrintf("  %4.1f", pTestGroupSet->pTestGroup[jj].maxEdgePower[ii]);
		}
	}
}

A_UINT16 fbin2freq(A_UINT16 fbin)
{
	return( (A_UINT16)(4800 + 5*fbin)) ;
}

A_UINT16 fbin2freq_2p4(A_UINT16 fbin)
{
	return( 2300 + fbin ) ;
}

A_UINT16 freq2fbin(A_UINT16 freq)
{
	if (freq == 0)
	{
		return 0;
	}

	if (freq > 3000)
	{
		return((A_UINT16)((freq-4800)/5) ) ;
	} else
	{
		if ((freq - 2300) > 256)
		{
			uiPrintf("Illegal channel %d. Only channels in 2.3-2.555G range supported.\n", freq);
			exit(0);
		}
		return( freq - 2300 );
	}
}

void program_eeprom(A_UINT32 devNum)
{

	//A_UINT32 word[1024];
	A_UINT16 fbin[20];
	A_UINT32 ii;
	A_UINT16 numPiers;
	A_UINT16 startOffset = 0x150;
	A_UINT16 dbmmask  = 0x3f;
	A_UINT16 pcdmask  = 0x3f;
	A_UINT16 freqmask = 0xff;
	A_UINT32 numWords = 367; // for 16k eeprom (0x2BE - 0x150) this need to be caluculated
	A_UINT32 startAddr,length;
	A_UINT16 eepromUpper, eepromLower,i;
	A_BOOL   atCal = TRUE;
	A_UINT32 calStart_11g;
	A_UINT32 *word;

    //if(isDragon(devNum)) {
    if(isDragon_sd(swDeviceID)) {
#if defined(LINUX) || defined(_WINDOWS)	
		programCompactEeprom(devNum);
#endif
		return;
	}

	length = CalSetup.EARStartAddr + CalSetup.EARLen + 2 + CalSetup.UartPciCfgLen;
	
	
	word = ( A_UINT32 *)calloc(eepromSize,sizeof(A_UINT32));
	if(word == NULL)
	{
			uiPrintf(" Memory not allocated in program_eeprom() \n");
			exit(1);
	}
	for(i=0;i<eepromSize;i++)
		word[i]=0xbeef;
	
    // Write Common Data to EEPROM : locations 0x00-0x14F
	Write_eeprom_Common_Data(devNum, 1, 0);

	//calculate the correct number of words based on EAR start location
	numWords = (A_UINT16) (CalSetup.EARStartAddr - 0x150);

	switch (CalSetup.eeprom_map) {
	case CAL_FORMAT_GEN5 : // griffin based
		dbmmask  = 0xff;
		numPiers = pCalDataset_gen5[MODE_11a]->numChannels;
		for (ii=0; ii<numPiers; ii++)
		{
			fbin[ii] = freq2fbin(pCalDataset_gen5[MODE_11a]->pChannels[ii]) ;
		}
		if (numPiers < NUM_PIERS) {
			for (ii=numPiers; ii<NUM_PIERS; ii++) {
				fbin[ii] = 0;
			}
		}
		fill_words_for_eeprom_gen5(word, (A_UINT16)numWords, fbin, dbmmask, pcdmask, freqmask);
		break;
	case CAL_FORMAT_GEN3 :  // derby based
		dbmmask  = 0xff;
		numPiers = pCalDataset_gen3[MODE_11a]->numChannels;
		for (ii=0; ii<numPiers; ii++)
		{
			fbin[ii] = freq2fbin(pCalDataset_gen3[MODE_11a]->pChannels[ii]) ;
		}
		if (numPiers < NUM_PIERS) {
			for (ii=numPiers; ii<NUM_PIERS; ii++) {
				fbin[ii] = 0;
			}
		}
		fill_words_for_eeprom_gen3(word, (A_UINT16)numWords, fbin, dbmmask, pcdmask, freqmask);
		break;
	default :              // sombrero based
		numPiers = pCalDataset->numChannels;
		for (ii=0; ii<numPiers; ii++)
		{
			fbin[ii] = freq2fbin(pCalDataset->pChannels[ii]) ;
		}
		fill_words_for_eeprom(word, (A_UINT16)numWords, fbin, dbmmask, pcdmask, freqmask);
		break;
	}
	
	//	art_eepromWriteBlock(devNum, startOffset, numWords, word) ;

	virtual_eeprom0WriteBlock(devNum, startOffset, numWords, word) ;

	
	if (configSetup.enableLabelScheme) {
//printf("SNOOP::calling write eeprom label\n");
    	write_eeprom_label(devNum);
//printf("SNOOP::calling program MACID from label\n");
#ifndef _IQV
// don't overwrite MACID during power cal EEPROM write
	    programMACIDFromLabel(devNum);
#endif 
//printf("SNOOP::exit program MACID from label\n");
	}

		
	startAddr = CalSetup.EARStartAddr + CalSetup.EARLen ;//+ 1; 
		length = startAddr;
	//check to see if this board needs an EAR
	if(configSetup.cfgTable.pCurrentElement->earFilename[0] != '\0') {
		//an ear file should be written.
		// writeEarToEeprom(devNum, configSetup.cfgTable.pCurrentElement->earFilename);
		virtual_writeEarToEeprom(devNum, configSetup.cfgTable.pCurrentElement->earFilename);
		startAddr = CalSetup.EARStartAddr + CalSetup.EARLen ;//+ 1; 

		//	uiPrintf("SNOOP: IN program_eeprom startAddr = 0x%x\n", startAddr );

		length = startAddr;
	//		uiPrintf("SNOOP: IN program_eeprom length = 0x%x\n", length );


		// Program Dynamic EAR for Griffin if enabled
		if (isGriffin(swDeviceID) && CalSetup.enableDynamicEAR) {
		//	uiPrintf("SNOOP: calsetup_earlen = 0x%x\n", CalSetup.EARLen );			
			atCal = TRUE;		
			calStart_11g = 0x150 + NUM_DUMMY_EEP_MAP1_LOCATIONS + 2 + 6*pCalDataset_gen5[MODE_11b]->numChannels; // for 2 pd_gains
			for(ii=calStart_11g; ii<(calStart_11g + 2 + 6*pCalDataset_gen5[MODE_11g]->numChannels); ii++) {
				word[ii-calStart_11g] = EEPROM_DATA_BUFFER[EEPBLK0][ii];
		//		printf("SNOOP: word[%d]=0x%x\n", ii, word[ii-calStart_11g]);
			}

			startAddr -= 1; //writeEarToEeprom automatically writes the 0 to end ear,
							//need to rewind back over this.
			for (ii = 0; ii < CalSetup.numDynamicEARChannels; ii++ ) {
				numWords = art_getEARCalAtChannel(devNum, atCal, (A_UINT16)(CalSetup.dynamicEARChannels[ii]), &(word[0]), (A_UINT16)(CalSetup.cal_mult_xpd_gain_mask[MODE_11g]), CalSetup.dynamicEARVersion);
				if (CalSetup.customerDebug) {
			//		uiPrintf("Dynamic Ear start = %x\n", startAddr);				
				}
				virtual_eeprom0WriteBlock(devNum, startAddr, numWords, word) ;
				startAddr += numWords;
				CalSetup.EARLen += numWords;
				length = startAddr;
			}
		}

		// write a 0x0000 to mark the end of EAR
		virtual_eeprom0Write(devNum, startAddr, 0x0000);
		length+=1;
			//uiPrintf("SNOOP: IN program_eeprom startAddr = 0x%x\n", startAddr );
			//uiPrintf("SNOOP: IN program_eeprom length = 0x%x\n", length );
	}
	else {
		virtual_eeprom0Write(devNum, startAddr, 0x0000);
		length+=1;
	      	CalSetup.EARLen+=CalSetup.EARLen;
		}
		//	uiPrintf("SNOOP: IN program_eeprom startAddr = 0x%x\n", startAddr );
	if ((needsUartPciCfg(swDeviceID)) && (CalSetup.uartEnable)) {
		// skip 2 locations to read a 0 to terminate EAR parsing (fixed w/ v5.0b5)
		startAddr = CalSetup.EARStartAddr + CalSetup.EARLen + 2; 
		//uiPrintf("SNOOP: IN program_eeprom startAddr = 0x%x\n", startAddr );
		
		length+=2;
		//	uiPrintf("SNOOP: IN program_eeprom length = 0x%x\n", length );
		CalSetup.UartPciCfgLen = programUartPciCfgFile(devNum, UART_PCI_CFG_FILE, startAddr , TRUE);
		length+=CalSetup.UartPciCfgLen;	
		//uiPrintf("SNOOP: IN program_eeprom length = 0x%x\n", length );
	}

	if(length > eepromSize)	{
		uiPrintf("Ear Locations exceded the eeprom size\n");
		free(word);
		return ;
	}

	//write the 2 eeprom locations to specify the size
		if((length > 0x400) && (CalSetup.eepromLength >0)) {
		//write the values to 0x1c and 0x1b
			checkSumLength = length;
		//	uiPrintf("CheckSumLength  in IF condition = %x\n",checkSumLength);
			i = (A_UINT16)set_eeprom_size(devNum,length);
			eepromLower = (A_UINT16)(length & 0xffff);		
			eepromUpper = (A_UINT16)((length & 0xffff0000) >> 11);
			eepromUpper = eepromUpper | i;  //fix the size to 32k for now, but need this to be variable		
		virtual_eeprom0Write(devNum, 0x1c, eepromUpper);
		virtual_eeprom0Write(devNum, 0x1b, eepromLower);
	}
	else {
		
		length = 0x400;	
		checkSumLength =0x400;
		virtual_eeprom0Write(devNum, 0x1b, 0);
		virtual_eeprom0Write(devNum, 0x1c, 0);
	}

		//uiPrintf("SNOOP: IN program_eeprom Before writing to virtual eeprom_write = 0x%x\n", length );
		virtual_eeprom0Write(devNum, (A_UINT32)0xc0, virtual_eeprom_get_checksum(devNum, 0xc1, (A_UINT16)(length-0xc1), EEPBLK0));

	if (CalSetup.customerDebug) {
		dump_virtual_eeprom(EEPBLK0, "eepblk0_dump.txt");
	}

	
		// the final REAL write to eeprom
	if (usb_client) {
	   	A_UINT32 iIndex, lAddr, dBLen;
	   	A_UINT16 *eepData;

		eepData = (A_UINT16 *) malloc(length * sizeof(A_UINT16));
		if (eepData != NULL) {
			for(iIndex=0;iIndex<length;iIndex++) {
			  eepData[iIndex] = (A_UINT16)(EEPROM_DATA_BUFFER[EEPBLK0][iIndex] & 0xffff);
			}
			dBLen = length * sizeof(A_UINT16);
			lAddr = art_memAlloc(dBLen, 0, devNum);
			art_load_and_program_code(devNum, lAddr, dBLen, (A_UCHAR *)eepData, 1);
			art_memFree(lAddr, devNum);
		}
		return;
	}
	else {
	art_eepromWriteBlock(devNum, 0x0, length, &(EEPROM_DATA_BUFFER[EEPBLK0][0]));
	}
	free(word);
}

void fill_words_for_eeprom(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask)
{

	A_UINT16 idx = 0;
	dDATA_PER_CHANNEL *currChannel;
	double *pwrList;
	A_UINT16 ii, jj;
	A_UINT16 numRDedges = 6;


	// Group 1. 11a Frequency pier locations
	if(CalSetup.calPower && CalSetup.Amode)
	{
		word[idx++] = ( (fbin[0] & freqmask) << 8) | (fbin[1] & freqmask)  ;
		word[idx++] = ( (fbin[2] & freqmask) << 8) | (fbin[3] & freqmask)  ;
		word[idx++] = ( (fbin[4] & freqmask) << 8) | (fbin[5] & freqmask)  ;
		word[idx++] = ( (fbin[6] & freqmask) << 8) | (fbin[7] & freqmask)  ;
		word[idx++] = ( (fbin[8] & freqmask) << 8) | (fbin[9] & freqmask)  ;
	} else {
		for (jj=0; jj<5; jj++)
		{
			word[idx++] = 0x0000;
		}
	}

	//Group 2. 11a calibration data for all frequency piers
	if(CalSetup.calPower && CalSetup.Amode)
	{
		if(NUM_PIERS != pCalDataset->numChannels)
		{
			uiPrintf("Number of channels in the Calibration dataset (%d) ", pCalDataset->numChannels);
			uiPrintf("is different \nfrom number of piers (%d).\n", NUM_PIERS);
			exit(0);
		}
		for (ii=0; ii<pCalDataset->numChannels; ii++)
		{
			currChannel = &(pCalDataset->pDataPerChannel[ii]);
			pwrList = currChannel->pPwrValues;

			word[idx++] = ( ( (currChannel->pcdacMax & pcdmask) << 10) |
							( (currChannel->pcdacMin & pcdmask) <<  4) |
							( (((int)(2*pwrList[0])) & dbmmask ) >> 2 ) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[0]) &     0x3 ) << 14) |
							( ((A_UINT16) (2*pwrList[1]) & dbmmask ) <<  8) |
							( ((A_UINT16) (2*pwrList[2]) & dbmmask ) <<  2) |
							( ((A_UINT16) (2*pwrList[3]) & dbmmask ) >>  4) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[3]) &     0xf ) << 12) |
							( ((A_UINT16) (2*pwrList[4]) & dbmmask ) <<  6) |
							( ((A_UINT16) (2*pwrList[5]) & dbmmask ) <<  0) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[6]) & dbmmask ) << 10) |
							( ((A_UINT16) (2*pwrList[7]) & dbmmask ) <<  4) |
							( ((A_UINT16) (2*pwrList[8]) & dbmmask ) >> 2 ) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[8]) &     0x3 ) << 14) |
							( ((A_UINT16) (2*pwrList[9]) & dbmmask ) <<  8) |
							( ((A_UINT16) (2*pwrList[10]) & dbmmask ) <<  2) );
		}
	} else
	{
		for (ii=0; ii < NUM_PIERS; ii++)
		{
			for (jj=0; jj<5; jj++)
			{
				word[idx++] = 0x0000;
			}
		}
	}

	//Group 3. 11b Calibration Information at piers 2.412, 2.447 and 2.484 GHz
	if(CalSetup.calPower && CalSetup.Bmode)
	{
		for (ii=0; ii<pCalDataset_2p4[MODE_11b]->numChannels; ii++)
		{
			currChannel = &(pCalDataset_2p4[MODE_11b]->pDataPerChannel[ii]);
			pwrList = currChannel->pPwrValues;

			word[idx++] = ( ( (currChannel->pcdacMax & pcdmask) << 10) |
							( (currChannel->pcdacMin & pcdmask) <<  4) |
							( (((int)(2*pwrList[0])) & dbmmask ) >> 2 ) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[0]) &     0x3 ) << 14) |
							( ((A_UINT16) (2*pwrList[1]) & dbmmask ) <<  8) |
							( ((A_UINT16) (2*pwrList[2]) & dbmmask ) <<  2) |
							( ((A_UINT16) (2*pwrList[3]) & dbmmask ) >>  4) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[3]) &     0xf ) << 12) |
							( ((A_UINT16) (2*pwrList[4]) & dbmmask ) <<  6) |
							( ((A_UINT16) (2*pwrList[5]) & dbmmask ) <<  0) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[6]) & dbmmask ) << 10) |
							( ((A_UINT16) (2*pwrList[7]) & dbmmask ) <<  4) |
							( ((A_UINT16) (2*pwrList[8]) & dbmmask ) >> 2 ) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[8]) &     0x3 ) << 14) |
							( ((A_UINT16) (2*pwrList[9]) & dbmmask ) <<  8) |
							( ((A_UINT16) (2*pwrList[10]) & dbmmask ) <<  2) );
		}
	} else
	{
		for (ii=0; ii<3; ii++)
		{
			for (jj=0; jj<5; jj++)
			{
				word[idx++] = 0x0000;
			}
		}
	}

	//Group 4. OFDM at 2.4GHz Calibration Information at piers 2.412, 2.447 and 2.484 GHz
	if(CalSetup.calPower && CalSetup.Gmode)
	{
		for (ii=0; ii<pCalDataset_2p4[MODE_11g]->numChannels; ii++)
		{
			currChannel = &(pCalDataset_2p4[MODE_11g]->pDataPerChannel[ii]);
			pwrList = currChannel->pPwrValues;

			word[idx++] = ( ( (currChannel->pcdacMax & pcdmask) << 10) |
							( (currChannel->pcdacMin & pcdmask) <<  4) |
							( (((int)(2*pwrList[0])) & dbmmask ) >> 2 ) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[0]) &     0x3 ) << 14) |
							( ((A_UINT16) (2*pwrList[1]) & dbmmask ) <<  8) |
							( ((A_UINT16) (2*pwrList[2]) & dbmmask ) <<  2) |
							( ((A_UINT16) (2*pwrList[3]) & dbmmask ) >>  4) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[3]) &     0xf ) << 12) |
							( ((A_UINT16) (2*pwrList[4]) & dbmmask ) <<  6) |
							( ((A_UINT16) (2*pwrList[5]) & dbmmask ) <<  0) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[6]) & dbmmask ) << 10) |
							( ((A_UINT16) (2*pwrList[7]) & dbmmask ) <<  4) |
							( ((A_UINT16) (2*pwrList[8]) & dbmmask ) >> 2 ) );

			word[idx++] = ( ( ((A_UINT16) (2*pwrList[8]) &     0x3 ) << 14) |
							( ((A_UINT16) (2*pwrList[9]) & dbmmask ) <<  8) |
							( ((A_UINT16) (2*pwrList[10]) & dbmmask ) <<  2) );
		}
	} else
	{
		for (ii=0; ii<3; ii++)
		{
			for (jj=0; jj<5; jj++)
			{
				word[idx++] = 0x0000;
			}
		}
	}

	fill_Target_Power_and_Test_Groups(&(word[idx]), (A_UINT16)(numWords - idx + 1), dbmmask, pcdmask, freqmask);
}


void fill_Target_Power_and_Test_Groups(A_UINT32 *word, A_UINT16 numWords,  
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask)
{
	A_UINT16 idx = 0;
	A_UINT16 ii, jj;
	A_UINT16 numRDedges = 6;	
	//Group 5. 11a Target power for rates 6-24, 36, 48, 54 at 8 target channels
	if (pTargetsSet->numTargetChannelsDefined > 0)
	{
		for (ii=0; ii<pTargetsSet->numTargetChannelsDefined; ii++)
		{
			word[idx++] = ( ((freq2fbin(pTargetsSet->pTargetChannel[ii].channelValue) & freqmask) << 8 ) |
							(((A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target24) & dbmmask) << 2) |
							(((A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target36) & dbmmask) >> 4) ) ;

			word[idx++] = ( (((A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target36) & 0xf) << 12) |
							(((A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target48) & dbmmask) << 6) |
							(((A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target54) & dbmmask) << 0) ) ;
		}

		// fill the unspecified test channels with 0s
		for (ii=pTargetsSet->numTargetChannelsDefined; ii<NUM_TARGET_POWER_CHANNELS; ii++)
		{
			word[idx++] = 0x0000 ;
			word[idx++] = 0x0000 ;
		}
	} else
	{
		for (ii=0; ii<2*NUM_TARGET_POWER_CHANNELS; ii++)
		{
			word[idx++] = 0x0000;
		}
	}

	// Group 6. 11b Target power. reserved 4 words for now.
	if (pTargetsSet_2p4[MODE_11b]->numTargetChannelsDefined > 0)
	{
		for (ii=0; ii<pTargetsSet_2p4[MODE_11b]->numTargetChannelsDefined; ii++)
		{
			word[idx++] = ( ((freq2fbin(pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].channelValue) & freqmask) << 8 ) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target24) & dbmmask) << 2) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target36) & dbmmask) >> 4) ) ;

			word[idx++] = ( (((A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target36) & 0xf) << 12) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target48) & dbmmask) << 6) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target54) & dbmmask) << 0) ) ;
		}

		// fill the unspecified test channels with 0s. None yet but may have in future revs.
		for (ii=pTargetsSet_2p4[MODE_11b]->numTargetChannelsDefined; ii<NUM_TARGET_POWER_CHANNELS_11b; ii++)
		{
			word[idx++] = 0x0000 ;
			word[idx++] = 0x0000 ;
		}

	} else
	{
		for (ii=0; ii<2*NUM_TARGET_POWER_CHANNELS_11b; ii++)
		{
			word[idx++] = 0x0000;
		}
	}

	// Group 7. OFDM at 2.4 GHz Target power. reserved 6 words for now.
	if (pTargetsSet_2p4[MODE_11g]->numTargetChannelsDefined > 0)
	{
		for (ii=0; ii<pTargetsSet_2p4[MODE_11g]->numTargetChannelsDefined; ii++)
		{
			word[idx++] = ( ((freq2fbin(pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].channelValue) & freqmask) << 8 ) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target24) & dbmmask) << 2) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target36) & dbmmask) >> 4) ) ;

			word[idx++] = ( (((A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target36) & 0xf) << 12) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target48) & dbmmask) << 6) |
							(((A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target54) & dbmmask) << 0) ) ;
		}

		// fill the unspecified test channels with 0s. 
		for (ii=pTargetsSet_2p4[MODE_11g]->numTargetChannelsDefined; ii<NUM_TARGET_POWER_CHANNELS_11g; ii++)
		{
			word[idx++] = 0x0000 ;
			word[idx++] = 0x0000 ;
		}

	} else
	{
		for (ii=0; ii<2*NUM_TARGET_POWER_CHANNELS_11g; ii++)
		{
			word[idx++] = 0x0000;
		}
	}

	// Group 8. Test Group Information
	for (ii=0; ii<pTestGroupSet->numTestGroupsDefined; ii++)
	{
		word[idx++] = ( (freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[0]) & freqmask) << 8 ) |
					  ( freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[1]) & freqmask) ;

		word[idx++] = ( (freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[2]) & freqmask) << 8 ) |
					  ( freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[3]) & freqmask) ;
		
		word[idx++] = ( (freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[4]) & freqmask) << 8 ) |
					  ( freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[5]) & freqmask) ;
		
		word[idx++] = ( (freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[6]) & freqmask) << 8 ) |
					  ( freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[7]) & freqmask) ;
		
		word[idx++] = ( ((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[1]) & dbmmask) | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[1] & 0x1) <<  6 ) |
						(((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[0]) & dbmmask) <<  8 )  | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[0] & 0x1) <<  14 ) );
	
		word[idx++] = ( ((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[3]) & dbmmask) | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[3] & 0x1) <<  6 ) |
						(((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[2]) & dbmmask) <<  8 )  | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[2] & 0x1) <<  14 ) );

		word[idx++] = ( ((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[5]) & dbmmask) | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[5] & 0x1) <<  6 ) |
						(((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[4]) & dbmmask) <<  8 )  | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[4] & 0x1) <<  14 ) );

		word[idx++] = ( ((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[7]) & dbmmask) | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[7] & 0x1) <<  6 ) |
						(((A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[6]) & dbmmask) <<  8 )  | 
							((pTestGroupSet->pTestGroup[ii].inBandFlag[6] & 0x1) <<  14 ) );

	}

	// fill the unspecified test groups with 0s
	for (ii=pTestGroupSet->numTestGroupsDefined; ii< MAX_NUM_CTL; ii++)
	{
		for(jj=0; jj< 8; jj++)
		{
			word[idx++] = 0x0000;
		}
	}
}

/* NOT_USED: in Dragon/Mercury */
void program_Target_Power_and_Test_Groups(A_UINT32 devNum)
{
#if defined(AR6001) || defined(AR6002)
#else
//	A_UINT32 word[1024];
	A_UINT32 startOffset = 0x1A5;
	A_UINT16 dbmmask  = 0x3f;
	A_UINT16 pcdmask  = 0x3f;
	A_UINT16 freqmask = 0xff;
	A_UINT16 numWords = 282; // for 16k eeprom (0x2BE - 0x1A5 + 1)
	A_UINT16 ii,i;
	A_UINT32 eep_map, tmpVal, numCTL = 0, done = 0;
	A_UINT32 startAddr = 0x400;
	A_UINT16 eepromUpper, eepromLower;

	A_UINT32 length;
	A_BOOL   atCal = FALSE;


	A_UINT32 *word;
	
	checkSumLength = CalSetup.EARStartAddr + CalSetup.EARLen + 2 + CalSetup.UartPciCfgLen;
	length = checkSumLength;

	word = ( A_UINT32 *)calloc(eepromSize,sizeof(A_UINT32));
	if(word == NULL)
	{
			printf(" Memory not allocated in program_Target_Power_and_Test_Groups() \n");
			exit(1);
	}

	for(i=0;i<eepromSize;i++)
		word[i]=0xffff;


	art_eepromReadBlock(devNum, 0x0, 0x3FF, &(EEPROM_DATA_BUFFER[EEPBLK0][0]));
	if (CalSetup.customerDebug) {
		dump_virtual_eeprom(EEPBLK0, "eepblk0_reprogram_before_dump.txt");
	}


	//tmpVal = art_eepromRead(devNum, 0xC4);
	tmpVal = EEPROM_DATA_BUFFER[EEPBLK0][0xC4];
	eep_map = (tmpVal >> 14) & 0x3;
	if (eep_map > 0) {
		CalSetup.EARStartAddr = (tmpVal & 0xFFF); // read the EAR start addr
		ii = 0x128; // start of CTL codes
		while ((ii <= 0x137) && (done < 1)) {
			//tmpVal = art_eepromRead(devNum, ii);
			tmpVal = EEPROM_DATA_BUFFER[EEPBLK0][ii++];
			if ((tmpVal >> 8) == 0) {
				done = 1;				
			} else {
				numCTL++;
				if ((tmpVal & 0xFF) == 0) {
					done = 1;				
				} else {
					numCTL++;
				}
			}
		}

		startOffset = CalSetup.EARStartAddr - (16 + 4 + 6) - numCTL*8; // back up for target powers and CTLs

		// correct the EAR start address for the new number of CTLs
		CalSetup.EARStartAddr += (A_UINT16)((pTestGroupSet->numTestGroupsDefined - numCTL)*8) ;
		EEPROM_DATA_BUFFER[EEPBLK0][0xC4] = (EEPROM_DATA_BUFFER[EEPBLK0][0xC4] & 0xF000) | (CalSetup.EARStartAddr & 0xFFF);
	}

	// added to be able to update eeprom ssid etc.
	// '0' to update eeprom header 'cause that would invalidate the cal data.
	Write_eeprom_Common_Data(devNum, 0, 0); 

	// Update the country/reg domain
    EEPROM_DATA_BUFFER[EEPBLK0][0xBF] = ((CalSetup.countryOrDomain & 0x1) << 15) | ((CalSetup.worldWideRoaming & 0x1) << 14) | 
							     (CalSetup.countryOrDomainCode & 0xfff) ;

	fill_Target_Power_and_Test_Groups(word, numWords, dbmmask, pcdmask, freqmask);

	//	art_eepromWriteBlock(devNum, startOffset, numWords, word) ;
	virtual_eeprom0WriteBlock(devNum, startOffset, numWords, word) ;

	startOffset = 0x128;
	for(ii=0; ii<=(A_UINT16) (pTestGroupSet->numTestGroupsDefined / 2); ii++)
	{
	  //		art_eepromWrite(devNum, startOffset + ii, 
		virtual_eeprom0Write(devNum, startOffset + ii, 
			(((pTestGroupSet->pTestGroup[2*ii].TG_Code & 0xff) << 8) | 
			  (pTestGroupSet->pTestGroup[2*ii+1].TG_Code & 0xff) ) );
	}

	while (ii < 16)
	{
	  //		art_eepromWrite(devNum, startOffset + ii, 0x0000);
		virtual_eeprom0Write(devNum, startOffset + ii, 0x0000);
		ii++;
	}

	//check to see if this board needs an EAR
	if(configSetup.cfgTable.pCurrentElement->earFilename[0] != '\0') {
		//an ear file should be written.
		//writeEarToEeprom(devNum, configSetup.cfgTable.pCurrentElement->earFilename);
		virtual_writeEarToEeprom(devNum, configSetup.cfgTable.pCurrentElement->earFilename);

		startAddr = CalSetup.EARStartAddr + CalSetup.EARLen + 1; 
		length = startAddr;
		
		// Program Dynamic EAR for Griffin if enabled
		if (isGriffin(swDeviceID) && CalSetup.enableDynamicEAR) {
			atCal = FALSE;		

//	uiPrintf("SNOOP: dynaEarStart = 0x%x\n", startAddr);
			startAddr -= 1;
			for (ii = 0; ii < CalSetup.numDynamicEARChannels; ii++ ) {
				numWords = art_getEARCalAtChannel(devNum, atCal, (A_UINT16)(CalSetup.dynamicEARChannels[ii]), &(word[0]), (A_UINT16)(CalSetup.cal_mult_xpd_gain_mask[MODE_11g]), CalSetup.dynamicEARVersion);
				virtual_eeprom0WriteBlock(devNum, startAddr, numWords, word) ;
				startAddr += numWords;
				CalSetup.EARLen += numWords;
				length = startAddr;
//	uiPrintf("SNOOP: num_dynaEar_words  = %d \n", numWords);
			}
		}

		// write a 0x0000 to mark the end of EAR
		virtual_eeprom0Write(devNum, startAddr, 0x0000);
		length+=1;
	} 

	if ((needsUartPciCfg(swDeviceID)) && (CalSetup.uartEnable)) {
		startAddr = CalSetup.EARStartAddr + CalSetup.EARLen + 2;
		length+=2;
		CalSetup.UartPciCfgLen = programUartPciCfgFile(devNum, UART_PCI_CFG_FILE, startAddr ,TRUE);
		length+=CalSetup.UartPciCfgLen;	
	}

	if((length > 0x400) && (CalSetup.eepromLength > 0)) {
		//write the values to 0x1c and 0x1b
			checkSumLength = length;
			//uiPrintf("CheckSumLength  in IF condition = %x\n",checkSumLength);
			i = (A_UINT16)set_eeprom_size(devNum,length);
			eepromLower = (A_UINT16)(length & 0xffff);		
			eepromUpper = (A_UINT16)((length & 0xffff0000) >> 11);
			eepromUpper = eepromUpper | i;  //fix the size to 32k for now, but need this to be variable		
		virtual_eeprom0Write(devNum, 0x1c, eepromUpper);
		virtual_eeprom0Write(devNum, 0x1b, eepromLower);
	}
	else {
		
				length = 0x400;	
				checkSumLength =0x400;
				virtual_eeprom0Write(devNum, 0x1b, 0);
		virtual_eeprom0Write(devNum, 0x1c, 0);
	}
	/*else
	{
		uiPrintf("EEpromSize is Lesser than the CheckSumLength\n"):
		exit(1);
	}*/

		//uiPrintf("SNOOP: IN program_eeprom Before writing to virtual eeprom_write = 0x%x\n", length );
		virtual_eeprom0Write(devNum, (A_UINT32)0xc0, virtual_eeprom_get_checksum(devNum, 0xc1, (A_UINT16)(length-0xc1), EEPBLK0));

	if (CalSetup.customerDebug) {
		dump_virtual_eeprom(EEPBLK0, "eepblk0_reprogram_after_dump.txt");
	}
	
	// the final REAL write to eeprom
	//art_eepromWriteBlock(devNum, 0x0, 0x400, &(EEPROM_DATA_BUFFER[EEPBLK0][0]));
	art_eepromWriteBlock(devNum, 0x0, length, &(EEPROM_DATA_BUFFER[EEPBLK0][0]));
	free(word);
#endif
}

void set_appropriate_OBDB(A_UINT32 devNum, A_UINT16 channel)
{
	A_UINT32 tempOB, tempDB;
	
	if((channel > 4000) && (channel < 5260)) {
		tempOB = CalSetup.ob_1 ;
		tempDB = CalSetup.db_1;
	}
	else if ((channel >= 5260) && (channel < 5500)) {
		tempOB = CalSetup.ob_2;
		tempDB = CalSetup.db_2;
	}
	else if ((channel >= 5500) && (channel < 5725)) {
		tempOB = CalSetup.ob_3;
		tempDB = CalSetup.db_3;
	}
	else if (channel >= 5725) {
		tempOB = CalSetup.ob_4;
		tempDB = CalSetup.db_4;
	}

	art_changeField(devNum, "rf_ob", tempOB);
	art_changeField(devNum, "rf_db", tempDB);
}

A_UINT16 dutCalibration_2p4(A_UINT32 devNum, A_UINT32 mode)
{

	if (CalSetup.eeprom_map == CAL_FORMAT_GEN3) {
		dutCalibration_gen3(devNum, mode);
		return TRUE;
	}

//++JC++
	if (CalSetup.eeprom_map >= CAL_FORMAT_GEN5) {
		if (!dutCalibration_gen5(devNum, mode))
			return FALSE;
		return TRUE;
	}
//++JC++


	// power and gainf datasets setup and measurement
	if (!setup_raw_datasets_2p4(mode)) { 
		uiPrintf("Could not setup raw datasets for mode %d. Exiting...\n", mode);
        closeEnvironment();
        return FALSE;
	}

	if (CalSetup.readFromFile_2p4[mode])
	{
		read_dataset_from_file(pRawDataset_2p4[mode], CalSetup.rawDataFilename_2p4[mode]);
	} else
	{
		if ((mode == MODE_11b) && (CalSetup.useOneCal)){
			uiPrintf("Using 11g calibration data for 11b mode\n");
			copy_11g_cal_to_11b(pRawDataset_2p4[MODE_11g], pRawDataset_2p4[MODE_11b]);
		} else {
			if (!getCalData(devNum, mode, CalSetup.customerDebug))
				return FALSE;
		}
		dump_rectangular_grid_to_file(pRawDataset_2p4[mode], calPowerLogFile_2p4[mode]) ;
		if (CalSetup.customerDebug)
			dump_rectangular_grid_to_file(pRawGainDataset_2p4[mode], calGainfLogFile_2p4[mode]) ;
	}

	if(!d_allocateEepromStruct( pCalDataset_2p4[mode], numRAWChannels_2p4, goldenParams.numIntercepts  )) {
		uiPrintf("unable to allocate eeprom struct pCalDataset_2p4[%d]. Exiting...\n", mode);
		return FALSE;
	}
	pCalDataset_2p4[mode]->hadIdenticalPcdacs = FALSE ;
	build_cal_dataset_skeleton(pRawDataset_2p4[mode], pCalDataset_2p4[mode], goldenParams.pInterceptPercentages, 
								goldenParams.numIntercepts,  RAW_CHAN_LIST_2p4[mode], numRAWChannels_2p4);
	dMapGrid(pRawDataset_2p4[mode], pCalDataset_2p4[mode]);
	quantize_hash(pCalDataset_2p4[mode]);

	if(CalSetup.customerDebug) {
		switch (mode) {
		case MODE_11g :
			dump_nonrectangular_grid_to_file(pCalDataset_2p4[mode], "junkToee_11g.log");
			break;
		case MODE_11b :
			dump_nonrectangular_grid_to_file(pCalDataset_2p4[mode], "junkToee_11b.log");
			break;
		default :
			uiPrintf("Unknown Mode in dutCalibration_2p4: %d. Exiting ...\n", mode);
			closeEnvironment();        
			return FALSE;
			break;
		}
	}
	return TRUE;
}

A_BOOL setup_raw_datasets_2p4(A_UINT32 mode) 
{
	A_UINT16  numPcdacs;
	A_UINT16 i, j, channelValue;
	dPCDACS_EEPROM *pEepromStruct;
	A_UINT16  myNumRawChannels; 
	A_UINT16 *pMyRawChanList ;

	if(CalSetup.forcePiers_2p4[mode] && !CalSetup.readFromFile_2p4[mode]) // 'cause read from file supercedes
	{
		myNumRawChannels = (A_UINT16) CalSetup.numForcedPiers_2p4[mode];
		pMyRawChanList = &(CalSetup.piersList_2p4[mode][0]) ;
	} else
	{
		myNumRawChannels = numRAWChannels_2p4;
		pMyRawChanList = &(RAW_CHAN_LIST_2p4[mode][0]) ;
	}

	numPcdacs = sizeOfRawPcdacs;

	if(!d_allocateEepromStruct( pRawDataset_2p4[mode], myNumRawChannels, numPcdacs )) {
		uiPrintf("unable to allocate eeprom struct RawDataset_2p4 for mode %s for full struct\n", modeName[mode]);
		return(0);
	}

	if(!d_allocateEepromStruct( pRawGainDataset_2p4[mode], myNumRawChannels, numPcdacs )) {
		uiPrintf("unable to allocate eeprom struct RawGainDataset_2p4 for mode %s for full struct\n", modeName[mode]);
		return(0);
	}

	//now fill in the channel list and pcdac lists
	for (j=0; j<2; j++) 
	{
		pEepromStruct = pRawDataset_2p4[mode];
		if (j > 0) 
			pEepromStruct = pRawGainDataset_2p4[mode];

		for (i = 0; i < myNumRawChannels; i++) {
			channelValue = pMyRawChanList[i];
	
			pEepromStruct->hadIdenticalPcdacs = TRUE;
			pEepromStruct->pChannels[i] = channelValue;
			
			pEepromStruct->pDataPerChannel[i].channelValue = channelValue;
			pEepromStruct->pDataPerChannel[i].numPcdacValues = numPcdacs ;
			memcpy(pEepromStruct->pDataPerChannel[i].pPcdacValues, RAW_PCDACS, numPcdacs*sizeof(A_UINT16));

			pEepromStruct->pDataPerChannel[i].pcdacMin = pEepromStruct->pDataPerChannel[i].pPcdacValues[0];
			pEepromStruct->pDataPerChannel[i].pcdacMax = pEepromStruct->pDataPerChannel[i].pPcdacValues[numPcdacs - 1];
	
		}
	}
	return(1);
};

A_UINT16 measure_all_channels_2p4(A_UINT32 devNum, A_UINT16 debug, A_UINT32 mode) 
{
	A_UINT16 fill_zero_pcdac = 40 ;
	A_BOOL fillZeros ;
	A_BOOL fillMaxPwr;
	A_UINT16 numPcdacs ;
	A_INT16 i;
	A_UINT16 *reordered_pcdacs_index ;
	A_UINT16 rr = 0; //reordered_pcdacs_index index
	A_UINT16 channel;
	A_UINT16  pcdac ;
	A_UINT16 reset = 0;
	double power;
	A_UCHAR  devlibMode ; // use setResetParams to communicate appropriate mode to devlib
	

	dDATA_PER_CHANNEL	*pRawChannelData;
	double				*pRawPwrValue;

	dDATA_PER_CHANNEL	*pGainfChannelData;
	double				*pGainfValue;

	switch (mode) {
	case MODE_11g :
		devlibMode = MODE_11G;
		break;
	case MODE_11b :
		devlibMode = MODE_11B;
		break;
	default:
		uiPrintf("Unknown mode : %d \n", mode);
		break;
	}

	configSetup.eepromLoad = 0;
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)devlibMode, configSetup.use_init);		
						
	uiPrintf("\nCollecting raw data for the adapter for mode %s\n", modeName[mode]);
	numPcdacs = (A_UINT16) sizeOfRawPcdacs ;
	reordered_pcdacs_index = (A_UINT16 *)malloc(sizeof(A_UINT16) * numPcdacs) ;
	if (NULL == reordered_pcdacs_index) 
		uiPrintf("Could not allocate memory for the reordered_pcdacs_index array.\n");

	for (i=numPcdacs-1; i>=0; i--)
	{
		if (RAW_PCDACS[i] <= fill_zero_pcdac)
		{
			reordered_pcdacs_index[rr] = i;
			rr++;
		}
	}

	for (i=0; i<numPcdacs; i++)
	{
		if (RAW_PCDACS[i] > fill_zero_pcdac)
		{
			reordered_pcdacs_index[rr] = i;
			rr++;
		}
	}

	if (rr != numPcdacs)
	{
		uiPrintf("Couldn't reorder pcdacs.\n");
		return FALSE;
	}


	if(debug)
	{
		uiPrintf("Reordered pcdacs are: ");
		for (i=0; i<numPcdacs; i++)
		{
			uiPrintf("%d, ", RAW_PCDACS[reordered_pcdacs_index[i]]);
		}
		uiPrintf("\n");
	}

	for (i=0; i<pRawDataset_2p4[mode]->numChannels; i++)
	{
		channel = pRawDataset_2p4[mode]->pDataPerChannel[i].channelValue;
		pRawChannelData = &(pRawDataset_2p4[mode]->pDataPerChannel[i]) ;
		pGainfChannelData = &(pRawGainDataset_2p4[mode]->pDataPerChannel[i]) ;
				
		if (i == 0)
		{
			if (!art_resetDevice(devNum, txStation, NullID, channel, 0))
				return FALSE;
		} else
		{
//			art_resetDevice(devNum, txStation, NullID, channel, 0); 
			art_changeChannel(devNum, channel); //SNOOP: get this in eventually for efficiency
		}

		art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
					    rates[0], DESC_ANT_A | USE_DESC_ANT);

		Sleep(50); // sleep 20 ms
//		Sleep(250) ;

		reset = 0; //1;
		if(CalSetup.useInstruments) 
		{	
			power = pmMeasAvgPower(devPM, reset) ;
		} else
		{
			power = 0;
		}
		reset = 0;

		fillZeros = FALSE ;
		fillMaxPwr = FALSE;

		for(rr=0; rr<numPcdacs; rr++)
		{
			pcdac = RAW_PCDACS[reordered_pcdacs_index[rr]] ;
			pRawPwrValue = &(pRawChannelData->pPwrValues[reordered_pcdacs_index[rr]]) ;
			pGainfValue  = &(pGainfChannelData->pPwrValues[reordered_pcdacs_index[rr]]) ;
			
			if (CalSetup.xpd_2p4[mode] < 1)
			{
				art_txContEnd(devNum);
				if (!art_resetDevice(devNum, txStation, NullID, channel, 0))
					return FALSE;
				art_ForceSinglePCDACTable(devNum, (A_UCHAR) pcdac);
				art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
							    rates[0], DESC_ANT_A | USE_DESC_ANT);
			} else
			{
				art_ForceSinglePCDACTable(devNum, pcdac);
			}

			if (pcdac > fill_zero_pcdac) 
				fillZeros = FALSE ;

			if (pcdac < fill_zero_pcdac) 
				fillMaxPwr = FALSE ;

			if (fillZeros)
			{
				*pRawPwrValue = 0;
				*pGainfValue  = 0;
				continue ;
			}

			if (fillMaxPwr)
			{
				*pRawPwrValue = CalSetup.maxPowerCap[mode];
				*pGainfValue  = 110;
				continue ;
			}

			(mode == MODE_11g) ? Sleep(50) : Sleep(150) ;

			if(CalSetup.useInstruments) 
			{	
				power = pmMeasAvgPower(devPM, reset) ;
			} else
			{
				power = 0;
			}

			*pRawPwrValue = (power + CalSetup.attenDutPM_2p4[mode]) ;
			if (CalSetup.customerDebug)
			{
				art_txContEnd(devNum);
				//Sleep(100);
				*pGainfValue  = (double) dump_a2_pc_out(devNum) ;
				art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
							    rates[0], DESC_ANT_A | USE_DESC_ANT);

			} else
			{
				*pGainfValue = (double) 0 ;
			}

			if ( *pRawPwrValue <= 0 )
				fillZeros = TRUE ;

			if ( *pRawPwrValue >= CalSetup.maxPowerCap[mode] )
				fillMaxPwr = TRUE ;

			if (debug)
			{
				uiPrintf("  power at pcdac=%d is %2.1f + %2.1f = %2.1f\n", pcdac, power, CalSetup.attenDutPM_2p4[mode], *pRawPwrValue);
			}
		}

		uiPrintf("ch: %d  --> max pwr is %2.1f dBm\n", channel, *pRawPwrValue) ;

		art_txContEnd(devNum) ;
	}

	free(reordered_pcdacs_index);
	return TRUE;
};

A_UINT16 dutTest_2p4(A_UINT32 devNum,  A_UINT32 mode)
{
	A_UCHAR  devlibMode ; // use setResetParams to communicate appropriate mode to devlib
	A_UINT32 jumbo_frame_mode_save;
   A_UINT16 throughputRate = 54;
#ifndef _IQV		
	if (!(CalSetup.testTXPER_2p4[mode] | CalSetup.testRXSEN_2p4[mode] | 
		  CalSetup.testSpecMask_2p4[mode] | CalSetup.testTargetPowerControl[mode] | 
		  CalSetup.testDataIntegrity[mode] | CalSetup.testThroughput[mode]))
		return TRUE;
#endif // _IQV
	switch (mode) {
	case MODE_11g :
		devlibMode = MODE_11G;
		do_11g_CCK = FALSE;
		break;
	case MODE_11b :
		if(CalSetup.doCCKin11g) {
			devlibMode = MODE_11G;
			do_11g_CCK = TRUE;
			throughputRate = 0xdb;
		} else {
		   devlibMode = MODE_11B;
			do_11g_CCK = FALSE;
		}
		break;
	default:
		uiPrintf("Unknown mode : %d \n", mode);
		break;
	}
#ifndef _IQV
	if(CalSetup.useInstruments) {
	    attSet(devATT,81);
	}
#endif // _IQV

#ifdef _IQV
  if (testType == ART_CAL || testType == ART_MANUF) {
#endif // _IQV
	if ( ( (mode == MODE_11g) && (CalSetup.Gmode)) ||
		( (mode == MODE_11b) && (CalSetup.Bmode)) ||
		(do_11g_CCK && CalSetup.Gmode)) {
		configSetup.eepromLoad = 1;
		art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
						(A_BOOL)1, (A_UCHAR)devlibMode, configSetup.use_init);		
			
		art_rereadProm(devNum);
	}

	if (NEED_GAIN_OPT_FOR_MODE[mode])
	{
		//initializeGainLadder(pCurrGainLadder);
		//setGainLadderForMaxGain(pCurrGainLadder);
		setGainLadderForIndex(pCurrGainLadder, optGainLadderIndex[mode]);
		programNewGain(pCurrGainLadder, devNum, 0);
	}

	if (!art_resetDevice(devNum, txStation, bssID, 2412, 0))
		return FALSE;
	if(!processEepFile(devNum, configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion))
		return FALSE;
	if(!art_resetDevice(devNum, txStation, bssID, 2412, 0))
		return FALSE;
#ifdef _IQV
	if (testType != ART_MANUF)	// do target measurement
		return TRUE;	// from testType == ART_CAL
  }	
  if (testType == ART_MANUF) {	// do target measurement
#endif // _IQV
	// get 11g values
	if( configSetup.force_antenna && isDragon_sd(swDeviceID))
	{
		updateForcedAntennaVals (devNum);
		updateAntenna(devNum);	
	}
	
	if (CalSetup.testTargetPowerControl[mode])
	{
		timestart = milliTime();
		strcpy(testname[testnum],"    target pwr ctl");
		strncpy(testname[testnum],modeName[mode], 3);
		setYieldParamsForTest(devNum, modeName[mode], "tgt_pwr", "pwr", "dBm", "max_lin_pwr", "dBm"); 
		dutTestTargetPower(devNum, mode);
		testtime[testnum++] = milliTime() - timestart;
	}
#ifdef _IQV
  }	// testType == ART_MANUF
#endif	// _IQV

#ifndef _IQV
    if(CalSetup.testSpecMask_2p4[mode])
	{
		timestart = milliTime();
		strcpy(testname[testnum],"    spectral mask");
		strncpy(testname[testnum],modeName[mode], 3);
		setYieldParamsForTest(devNum, modeName[mode], "spectral_mask", "fail_pts", "num", "delta_tgt_pwr", "dBm"); 
		dutTestSpecMask_2p4(devNum, mode);
		testtime[testnum++] = milliTime() - timestart;
	}

    if((CalSetup.testTXPER_2p4[mode]) || (CalSetup.testRXSEN_2p4[mode]))
	{
		sendAck(devNum, "Start 2.5G PER and SEN test", 0, 0, 0, CalSetup.customerDebug);
		waitForAck(CalSetup.customerDebug);
		if (REWIND_TEST) {
			return TRUE;
		}

		timestart = milliTime();
		strcpy(testname[testnum],"    PER and RXSEN");
		strncpy(testname[testnum],modeName[mode], 3);
		setYieldParamsForTest(devNum, modeName[mode], "tx_per", "good_pkts", "percent", "rssi", "dB"); 
		dutTestTXRX(devNum, mode);
		testtime[testnum++] = milliTime() - timestart;

		// test names to be used in "descriptive error reports"
		strcpy(testname[testnum],"    PER");
		strncpy(testname[testnum],modeName[mode], 3);
		testtime[testnum++] = -1;
		strcpy(testname[testnum],"    RXSEN");
		strncpy(testname[testnum],modeName[mode], 3);
		testtime[testnum++] = -1;
		strcpy(testname[testnum],"    PPM");
		strncpy(testname[testnum],modeName[mode], 3);
		testtime[testnum++] = -1;

		if (CalSetup.testTXPER_margin) {
			sendAck(devNum, "Start 2.5G PER +1dB Margin test", 0, 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return TRUE;
			}
			timestart = milliTime();
			strcpy(testname[testnum],"    +1dB PER MARGIN");
			strncpy(testname[testnum],modeName[mode], 3);
			setYieldParamsForTest(devNum, modeName[mode], "1dB_tx_per", "good_pkts", "percent", "rssi", "dB"); 
			TX_PER_STRESS_MARGIN = 1; // do 1dB tx_per margin test 
			dutTestTXRX(devNum, mode);
			TX_PER_STRESS_MARGIN = 0; // set back to default
			if (REWIND_TEST) {
				return TRUE;
			}
			testtime[testnum++] = milliTime() - timestart;

			// test names to be used in "descriptive error reports"
			strcpy(testname[testnum],"    +1dB PER");
			strncpy(testname[testnum],modeName[mode], 3);
			testtime[testnum++] = -1;

			sendAck(devNum, "Start 2.5G PER +2dB Margin test", 0, 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return TRUE;
			}
			timestart = milliTime();
			strcpy(testname[testnum],"    +2dB PER MARGIN");
			strncpy(testname[testnum],modeName[mode], 3);
			setYieldParamsForTest(devNum, modeName[mode], "2dB_tx_per", "good_pkts", "percent", "rssi", "dB"); 
			TX_PER_STRESS_MARGIN = 2; // do 2dB tx_per margin test 
			dutTestTXRX(devNum, mode);
			TX_PER_STRESS_MARGIN = 0; // set back to default				
			testtime[testnum++] = milliTime() - timestart;

			// test names to be used in "descriptive error reports"
			strcpy(testname[testnum],"    +2dB PER");
			strncpy(testname[testnum],modeName[mode], 3);
			testtime[testnum++] = -1;

			if (REWIND_TEST) {
				return TRUE;
			}				
		}
	}

	if(CalSetup.testDataIntegrity[mode])
	{
		sendAck(devNumArr[MODE_11a], "Start 2.5G verifyDataIntegrity test", 0, 0, 0, CalSetup.customerDebug);
		waitForAck(CalSetup.customerDebug);
		if (REWIND_TEST) {
			return TRUE;
		}

		timestart = milliTime();
		strcpy(testname[testnum],"    Data Integrity Check");
		strncpy(testname[testnum],modeName[mode], 3);
		setYieldParamsForTest(devNum, modeName[mode], "data_integrity", "result", "string", "iter", "num"); 
		dutVerifyDataTXRX(devNum, mode, 0, 6, VERIFY_DATA_PACKET_LEN);
		if (REWIND_TEST) {
			return TRUE;
		}
		testtime[testnum++] = milliTime() - timestart;
	}

	if(CalSetup.testThroughput[mode])
	{
		sendAck(devNumArr[MODE_11a], "Start 2.5G throughput test", 0, 0, 0, CalSetup.customerDebug);
		waitForAck(CalSetup.customerDebug);
		if (REWIND_TEST) {
			return TRUE;
		}

		timestart = milliTime();
		strcpy(testname[testnum],"    Throughput Check");
		strncpy(testname[testnum],modeName[mode], 3);
		setYieldParamsForTest(devNum, modeName[mode], "tx_thruput", "thruput", "mbps", "rssi", "dB"); 
		if(isDragon_sd(swDeviceID)) {
		    dutThroughputTest(devNum, mode, 0, throughputRate, THROUGHPUT_PACKET_SIZE, NUM_THROUGHPUT_PACKETS_11B);
		}
	    else {
		    dutThroughputTest(devNum, mode, 0, throughputRate, THROUGHPUT_PACKET_SIZE, 
			   (mode == MODE_11g) ? NUM_THROUGHPUT_PACKETS : NUM_THROUGHPUT_PACKETS_11B);
		}
		if (REWIND_TEST) {
			return TRUE;
		}
		testtime[testnum++] = milliTime() - timestart;

		if((CalSetup.testTURBO_2p4[mode]) && (mode == MODE_11g)) {
			sendAck(devNumArr[MODE_11a], "Start 2.5G throughput turbo test", 0, 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return TRUE;
			}

			timestart = milliTime();
			strcpy(testname[testnum],"    Throughput Check");
			strncpy(testname[testnum],modeName[mode], 3);
			setYieldParamsForTest(devNum, modeName[mode], "tx_thruput", "thruput", "mbps", "rssi", "dB"); 
			jumbo_frame_mode_save = art_getFieldForMode(devNum, "mc_jumbo_frame_mode", mode, 1);
			art_writeField(devNum, "mc_jumbo_frame_mode", 0);
			dutThroughputTest(devNum, mode, 1, throughputRate, THROUGHPUT_TURBO_PACKET_SIZE, 
				(mode == MODE_11g) ? NUM_THROUGHPUT_PACKETS : NUM_THROUGHPUT_PACKETS_11B);
			art_writeField(devNum, "mc_jumbo_frame_mode", jumbo_frame_mode_save);
			if (REWIND_TEST) {
				return TRUE;
			}
			testtime[testnum++] = milliTime() - timestart;
		}
	
	}
	initializeGainLadder(pCurrGainLadder);
	programNewGain(pCurrGainLadder, devNum, 0);
#endif // _IQV

#ifdef _IQV
  if (testType == ART_EXIT)
  {
	initializeGainLadder(pCurrGainLadder);
	programNewGain(pCurrGainLadder, devNum, 0);
  }	// testType == ART_EXIT
#endif // _IQV
  return TRUE;
};


void dutTestSpecMask_2p4(A_UINT32 devNum, A_UINT32 mode){

    A_UCHAR  antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 cnt1;
	A_UINT16 numChannels;
	A_UINT32 *channelListTimes10;
	A_UINT16 ii;
	A_UINT32 usePreSet=1;
	A_UINT32 fail_num, iter = 0;

	double  center;
	double  span;

//#ifndef CUSTOMER_REL
	double	pwr;
//#endif

	double  yldLogRate;

	A_UINT32 reset, verbose, plot_output; 

    span  = 60e6;	// span = 60 MHz 
    reset = 1; 
    verbose = 0; 
    plot_output = 0; 

   if(do_11g_CCK) {
		uiPrintf("\n\nStart Spectral Mask Test for 11g CCK mode \n");
	} else {
      uiPrintf("\n\nStart Spectral Mask Test for mode %s ", modeName[mode]);
   }

	numChannels = pTestSet[mode]->numTestChannelsDefined;
	channelListTimes10 = (A_UINT32 *)malloc(numChannels*sizeof(A_UINT32)) ;
	if(NULL == channelListTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in spec test\n");
		return;
	}
	for (ii=0; ii<numChannels; ii++)
	{
		channelListTimes10[ii] = pTestSet[mode]->pTestChannel[ii].channelValueTimes10;
	}
	art_rereadProm(devNum); // re-read the calibration data from eeprom
						// No need to change domain.


	for(cnt1=0; cnt1<numChannels; cnt1++) {
		if (!pTestSet[mode]->pTestChannel[cnt1].TEST_MASK)
		{
			continue; //skip spectral mask test at this channel
		}

		// resetDevice loads the calibrated 64-entry pcdac table for
		// the channel. the power limits are min(targetPwr, bandEdgeMax) and hence
		// are domain independent. 
        art_resetDevice(devNum, txStation, bssID, channelListTimes10[cnt1], 0);	// turbo=0
        if (mode == MODE_11b)
		{
			if(do_11g_CCK) {
				art_txContBegin(devNum, CONT_DATA, PN9_PATTERN, 0xb5, antenna); // test with CCK modulation
			} else {
			   art_txContBegin(devNum, CONT_DATA, PN9_PATTERN, 24, antenna); // test with CCK modulation
                        }
			yldLogRate = 5.51;
		} else
		{
			art_txContBegin(devNum, CONT_DATA, PN9_PATTERN, 6, antenna); 
			yldLogRate = 6.0;
		}

		Sleep(20);		//wait 100 ms?


 
        if(CalSetup.useInstruments) {
		    //reset = 1;
		    center = channelListTimes10[cnt1] * 1e5;
			if (mode == MODE_11g)
			{
				fail_num = spaMeasSpectralMask(devSA,center,span,reset,verbose,plot_output);
			} else
			{
				fail_num = spaMeasSpectralMask11b(devSA,center,span,reset,verbose,plot_output);
			}
            reset = 0;
        }
        else {
            fail_num = 0;
            Sleep(2);
        }

        uiPrintf("\nspec mask - chan:%5.1lf", (double)(channelListTimes10[cnt1])/10);
		if (CalSetup.atherosLoggingScheme) {
			pwr = pmMeasAvgPower(devPM, 0) + CalSetup.attenDutPM;
		}
//        if (fail_num > 20)	{
        if (fail_num > CalSetup.maskFailLimit)	{
			TestFail = TRUE;
			failTest[testnum] = 1;
			uiPrintf(", points failed out of 400: %4d", fail_num);
			uiPrintf("  [power=%3.1f]", pwr);
			logMeasToYield((double)(channelListTimes10[cnt1])/10, yldLogRate, (double)iter, (double)(CalSetup.maskFailLimit), (double)(fail_num), (double)(pwr), "FAIL");
//#ifndef CUSTOMER_REL
//			pwr = pmMeasAvgPower(devPM, 0) + CalSetup.attenDutPM_2p4[mode];
//			uiPrintf("  [power=%3.1f]", pwr);
//#endif
		} else {
			logMeasToYield((double)(channelListTimes10[cnt1])/10, yldLogRate, (double)iter, (double)(CalSetup.maskFailLimit), (double)(fail_num), (double)(pwr), "PASS");
		}
	    //art_txContEnd(devNum);
    }


//	ResetSA(devSA, 2.412e9, 200e6, 10, 10, 1e6, 1e6);
//	uiPrintf("\nReset Spectrum Analyzer & Exit Spectral Mask Test\n");

	
	if(channelListTimes10) {
		free(channelListTimes10);
	}
}

void goldenTest_2p4(A_UINT32 devNum, A_UINT32 mode)
{
	A_UCHAR  devlibMode ; // use setResetParams to communicate appropriate mode to devlib
	A_UINT32 jumbo_frame_mode_save;
   A_UINT16 throughputRate = 54;
	
	if (!(CalSetup.testTXPER_2p4[mode] | CalSetup.testRXSEN_2p4[mode] | CalSetup.testDataIntegrity[mode]
		| CalSetup.testThroughput[mode]))
		return;

	switch (mode) {
	case MODE_11g :
		devlibMode = MODE_11G;
		do_11g_CCK = FALSE;
		break;
	case MODE_11b :
		if(CalSetup.doCCKin11g) {
			devlibMode = MODE_11G;
			do_11g_CCK = TRUE;
			throughputRate = 0xdb;
		} else {
		   devlibMode = MODE_11B;
			do_11g_CCK = FALSE;
		}
		break;
	default:
		uiPrintf("Unknown mode : %d \n", mode);
		break;
	}

	configSetup.eepromLoad = 1;
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
					(A_BOOL)1, (A_UCHAR)devlibMode, configSetup.use_init);		

	art_rereadProm(devNum);

	if (NEED_GAIN_OPT_FOR_MODE[mode])
	{
		initializeGainLadder(pCurrGainLadder);
		//setGainLadderForMaxGain(pCurrGainLadder);
		programNewGain(pCurrGainLadder, devNum, 0);
	}

	art_resetDevice(devNum, txStation, bssID, 2412, 0);

	uiPrintf("\nGolden Unit Program for mode %s is running. Hit any key to quit...\n", modeName[mode]);

    if((CalSetup.testTXPER_2p4[mode]) || (CalSetup.testRXSEN_2p4[mode]))
	{
		waitForAck(CalSetup.customerDebug);
		if (REWIND_TEST) {
			return;
		}
		sendAck(devNum, "start TXRX test", 2412, 0, 0, CalSetup.customerDebug);
		goldenTestTXRX(devNum, mode);

		if (CalSetup.testTXPER_margin) {
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return;
			}
			sendAck(devNumArr[MODE_11a], "start TXRX +1dB margin test", 2412, 0, 0, CalSetup.customerDebug);
			TX_PER_STRESS_MARGIN = 1;				
			goldenTestTXRX(devNum, mode);
			TX_PER_STRESS_MARGIN = 0;

			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return;
			}
			sendAck(devNumArr[MODE_11a], "start TXRX +2dB margin test", 2412, 0, 0, CalSetup.customerDebug);
			TX_PER_STRESS_MARGIN = 2;				
			goldenTestTXRX(devNum, mode);
			TX_PER_STRESS_MARGIN = 0;
		}

	}

	if(CalSetup.testDataIntegrity[mode])
	{
		waitForAck(CalSetup.customerDebug);
		if (REWIND_TEST) {
			return;
		}
		sendAck(devNum, "start 2.5G verifyDataTXRX test", 2412, 0, 0, CalSetup.customerDebug);
		goldenVerifyDataTXRX(devNum, mode, 0, 6, VERIFY_DATA_PACKET_LEN);
	}

	if(CalSetup.testThroughput[mode])
	{
		waitForAck(CalSetup.customerDebug);
		if (REWIND_TEST) {
			return;
		}
		sendAck(devNum, "start 2.5G throughput test", 2412, 0, 0, CalSetup.customerDebug);
		goldenThroughputTest(devNum, mode, 0, throughputRate, THROUGHPUT_PACKET_SIZE, NUM_THROUGHPUT_PACKETS);

		if((CalSetup.testTURBO_2p4[mode]) && (mode == MODE_11g)) {
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return;
			}
			sendAck(devNum, "start 2.5G throughput turbo test", 2412, 0, 0, CalSetup.customerDebug);
   			art_writeField(devNum, "bb_max_rx_length", 0xFFF);
			art_writeField(devNum, "bb_en_err_length_illegal", 0);
			jumbo_frame_mode_save = art_getFieldForMode(devNum, "mc_jumbo_frame_mode", mode, 1);
			art_writeField(devNum, "mc_jumbo_frame_mode", 0);
			goldenThroughputTest(devNum, mode, 1, throughputRate, THROUGHPUT_TURBO_PACKET_SIZE, NUM_THROUGHPUT_PACKETS);
			art_writeField(devNum, "mc_jumbo_frame_mode", jumbo_frame_mode_save);
		}

	}

	if(kbhit())
		return;;

}

void dutTestTargetPower(A_UINT32 devNum, A_UINT32 mode)
{
	A_UINT32 *channelTimes10;
	A_UINT16 index, ChannelCount, rate, ii;

	// txDataSetup
	A_UINT32 rate_mask = RATE_GROUP | PER_RATEMASK;
//	A_UINT32 rate_mask = PER_RATEMASK;
	A_UINT32 turbo = 0;

	A_UINT32 num_tx_desc = PER_TEST_FRAME;
	A_UINT32 tx_length = PER_FRAME_LEN;
	A_UINT32 retry = 0;		// broadcast mode, disable retry
	A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 0;

	// rxDataSetup
	A_UINT32 num_rx_desc = 50;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 10000;
	A_UINT32 stats_mode =  ENABLE_STATS_RECEIVE; // ENABLE_STATS_SEND |
	A_UINT32 compare = 0;

	double	 power, target, attenVal;
	A_UINT32 reset = 1;
	A_BOOL	 done_first = FALSE;

	A_UINT16 iter;
	A_BOOL   failedThisTest;
	A_UINT32 sleep_interval = 50;
	double   maxLinPwr = 0.0;
        A_UINT32 *pRatesArray = PER_TEST_RATE;

	A_UINT32    mixvga_ovr, mixgain_ovr;

	if (CalSetup.useInstruments < 1)
	{
		uiPrintf("useInstruments set to 0 in calsetup. Skipping Target Power Control test.\n");
		return;
	} 

	if (mode == MODE_11a)
	{
		attenVal = CalSetup.attenDutPM;
	} else
	{
		attenVal = CalSetup.attenDutPM_2p4[mode];
	}

	if (mode == MODE_11b)
	{
		sleep_interval = 250; // sleep 350 ms
	} else
	{
		if (CalSetup.pmModel == PM_436A) {
			sleep_interval = 150;
		} else if (CalSetup.pmModel == NRP_Z11) {
			sleep_interval = 50;
		} else {
			sleep_interval = 50;  //sleep 250 ms
		}		
	}

   if(do_11g_CCK) {
      pRatesArray = PER_TEST_RATE_11G_CCK;
   }

	if (CalSetup.eeprom_map == 1) {
		// change made to accomodate fast corner chips. this will be taken care of by dynamic optimization
		mixvga_ovr = art_getFieldForMode(devNum, "rf_mixvga_ovr", mode, turbo);
		mixgain_ovr = art_getFieldForMode(devNum, "rf_mixgain_ovr", mode, turbo);
		art_writeField(devNum, "rf_mixvga_ovr", 0);
	}
	
	if(do_11g_CCK && (mode == MODE_11b)) {
		uiPrintf("\n\nStart Target Power Control Test in 11g CCK mode");
	} else {
	uiPrintf("\n\nStart Target Power Control Test in %s mode", modeName[mode]);
   }

	ChannelCount = pTestSet[mode]->numTestChannelsDefined;
	channelTimes10 = (A_UINT32 *)malloc(ChannelCount*sizeof(A_UINT32)) ;
	if(NULL == channelTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in target power test\n");
		return;
	}
	for (ii=0; ii<ChannelCount; ii++)
	{
		channelTimes10[ii] = pTestSet[mode]->pTestChannel[ii].channelValueTimes10;
	}


	if (mode == MODE_11b)
	{
		uiPrintf("\nFREQ      1Mbps       5.5Mbps(S)   11Mbps(L)    11Mbps(S) Limits:+%3.1f/-%3.1f dB", CalSetup.targetPowerToleranceUpper, CalSetup.targetPowerToleranceLower);
	} else
	{
		uiPrintf("\nFREQ      6Mbps       36Mbps       48Mbps       54Mbps    Limits:+%3.1f/-%3.1f dB", CalSetup.targetPowerToleranceUpper, CalSetup.targetPowerToleranceLower);
	}

	uiPrintf("\n                                                          measured/target");

	for(index=0; index<ChannelCount; index++) {
		if ( pTestSet[mode]->pTestChannel[index].TEST_TGT_PWR < 1) 
		{
			continue; // skip Target Power test at this channel unless requested
		}

		if (index < 1)
		{
			art_resetDevice(devNum, rxStation, bssID, channelTimes10[index], turbo);
		} else
		{
			art_resetDevice(devNum, rxStation, bssID, channelTimes10[index], turbo);
		//	art_changeChannel(devNum, *(channel+index)); // use when fixed.
		}
		
		done_first = FALSE;

		uiPrintf("\n%5.1lf    ", (double)(channelTimes10[index])/10);
		for(rate=0; rate<PER_RATE_COUNT; rate++) {
			if (((pTestSet[mode]->pTestChannel[index].TEST_TGT_PWR >> (PER_RATE_COUNT -1 - rate)) & 0x1) < 1)
			{
				uiPrintf("  ----      ");
				continue;
			}
	
			target = getTargetPower(mode, pTestSet[mode]->pTestChannel[index].channelValueTimes10/10, pRatesArray[rate]);
			art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
						    pRatesArray[rate], DESC_ANT_A | USE_DESC_ANT);

			Sleep(sleep_interval);
			reset = 0; //1;

			failedThisTest = TRUE;
			iter = 0;
			while (failedThisTest && (iter < CalSetup.maxRetestIters)) {
				iter++;				
				if (CalSetup.customerDebug) uiPrintf("SNOOP: trgt_pwr repeat iter = %d \n", iter);
			power = pmMeasAvgPower(devPM, reset) + attenVal;
				power = ((A_INT16)(power*10 + 0.5))/10.0 ;
				failedThisTest = FALSE;
				if (((power - target) > CalSetup.targetPowerToleranceUpper) ||
					((target - power) > CalSetup.targetPowerToleranceLower) ) {
					failedThisTest |= TRUE;
					//Sleep(50);
					Sleep(sleep_interval);
				}			
			} // while failedThisTest
		 
			art_txContEnd(devNum);

			if ((!done_first) && (power < (target - 2.0)))
			{
				if (CalSetup.customerDebug)
				    uiPrintf("retried: %3.1f/%3.1f\n", power + getPowerRef(), target + getPowerRef());
				art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
								pRatesArray[rate], DESC_ANT_A | USE_DESC_ANT);
				Sleep(sleep_interval);
				power = pmMeasAvgPower(devPM, reset) + attenVal;
				power = ((A_INT16)(power*10 + 0.5))/10.0 ;
				art_txContEnd(devNum);
				done_first = TRUE;
			}

			uiPrintf("%3.1f/%3.1f", power + getPowerRef(), target + getPowerRef());

			if (CalSetup.atherosLoggingScheme) {
				maxLinPwr = art_getMaxLinPower(devNum);
			}

			if (((power - target) > CalSetup.targetPowerToleranceUpper) ||
				((target - power) > CalSetup.targetPowerToleranceLower) )
			{
				TestFail = TRUE;
				failTest[testnum] = 1;
				uiPrintf("(X) ");
				logMeasToYield((double)(channelTimes10[index])/10, (double)pRatesArray[rate], (double)CalSetup.pmModel, (double)target, (double)power, (double)maxLinPwr, "FAIL");
			} else
			{
				uiPrintf("    ");
				logMeasToYield((double)(channelTimes10[index])/10, (double)pRatesArray[rate], (double)CalSetup.pmModel, (double)target, (double)power, (double)maxLinPwr, "PASS");
			}
			
			done_first = TRUE;
		}	
	}
	uiPrintf("\n");

	if (channelTimes10) {
		free(channelTimes10);
	}
	
	if (CalSetup.eeprom_map == 1) {
		art_writeField(devNum, "rf_mixvga_ovr", mixvga_ovr);
		art_writeField(devNum, "rf_mixgain_ovr", mixgain_ovr);
	}
}

A_UINT16 getSubsystemIDfromCardType(A_CHAR *pLine, A_BOOL quiet)
{
	A_UINT16 subsytemID;

	if (strstr(pLine, "CB21") != NULL) {
            subsytemID = ATHEROS_CB21;
        }
        else if (strstr(pLine, "CB22") != NULL) {
            subsytemID = ATHEROS_CB22;
        }
        else if (strstr(pLine, "MB22") != NULL) {
            subsytemID = ATHEROS_MB22;
        }
        else if (strstr(pLine, "MB23") != NULL) {
            subsytemID = ATHEROS_MB23;
        }
        else if (strstr(pLine, "AP21") != NULL) {
            subsytemID = ATHEROS_AP21;
        }
        else {
            if(!quiet) {
				uiPrintf("Unknown card type %s\n", pLine);
			}
			return(0);
        }
		return(subsytemID);
}

A_BOOL isUNI3OddChannel(A_UINT16 channel)
{
	if ((channel > 5720) && ((channel % 5) == 0) && ((channel % 10) == 5))
	{
		return TRUE;
	} else
	{
		return FALSE;
	}
}



				
   
double getTargetPower (A_UINT32 mode, A_UINT32 ch, A_UINT32 rate)
{

	A_UINT32 Lch, Rch;
	double Lpwr, Rpwr;

	A_UINT32  numTgts;
	double retVal;
	A_INT32 prod;

	TARGETS_SET *pTargetsSet_local;

	if (mode == MODE_11a)
	{
		pTargetsSet_local = pTargetsSet;
	} else
	{
		pTargetsSet_local = pTargetsSet_2p4[mode];
	}


	numTgts = pTargetsSet_local->numTargetChannelsDefined;
	Lch = 0;
	Rch = 0;

	if (ch >= pTargetsSet_local->pTargetChannel[numTgts - 1].channelValue)
	{
		Lch = numTgts - 1;
		Rch = Lch ;
	} else if ((ch < pTargetsSet_local->pTargetChannel[numTgts - 1].channelValue) &&
				(ch > pTargetsSet_local->pTargetChannel[0].channelValue) )
	{
		Lch =0;
		Rch = Lch + 1;

		prod = (ch - pTargetsSet_local->pTargetChannel[Lch].channelValue)*
			   (ch - pTargetsSet_local->pTargetChannel[Rch].channelValue);

		while ( ( prod > 0) &&
				(Lch < numTgts) )
		{
			Lch++;
			Rch = Lch + 1;
			prod = (ch - pTargetsSet_local->pTargetChannel[Lch].channelValue)*
				   (ch - pTargetsSet_local->pTargetChannel[Rch].channelValue);

		}
	};

	if((mode == MODE_11b) && do_11g_CCK) {
	switch (rate) {
		case 0xb1:
		case 0xb2:
		case 0xd2:
		case 0xb5:
		Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target24;
		Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target24;
		break;
		case 0xd5:
		Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target36;
		Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target36;
		break;
		case 0xbb:
		Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target48;
		Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target48;
		break;
		case 0xdb:
		Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target54;
		Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target54;
		break;
	default:
		uiPrintf("target saught for an illegal rate: %d\n", rate);
		exit(0);
		break;
	}
	} else {
	   switch (rate) {
	   case 6:
	   case 9:
	   case 12:
	   case 18:
	   case 24:
		   Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target24;
		   Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target24;
		   break;
	   case 36:
		   Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target36;
		   Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target36;
		   break;
	   case 48:
		   Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target48;
		   Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target48;
		   break;
	   case 54:
		   Lpwr = pTargetsSet_local->pTargetChannel[Lch].Target54;
		   Rpwr = pTargetsSet_local->pTargetChannel[Rch].Target54;
		   break;
	   default:
		   uiPrintf("target saught for an illegal rate: %d\n", rate);
		   exit(0);
		   break;
	   }
   }

	retVal = dGetInterpolatedValue(ch, pTargetsSet_local->pTargetChannel[Lch].channelValue,
								  pTargetsSet_local->pTargetChannel[Rch].channelValue,
								  Lpwr, Rpwr);

	return(retVal);

}

void build_cal_dataset_skeleton_old(dPCDACS_EEPROM *srcDataset, dPCDACS_EEPROM *destDataset, 
								A_UINT16 *pPercentages, A_UINT16 numIntercepts, 
								A_UINT16 *tpList, A_UINT16 numTPs)
{

	A_UINT16 ii, jj, kk, ss;
	double maxdB, mindB, intdB, pwrL, pwrR, frac;
	dDATA_PER_CHANNEL *currChannel;
	A_UINT16 intc, pcdMin, pcdMax, pcdL, pcdR;


	destDataset->numChannels = numTPs;
	for (ii=0; ii<numTPs; ii++)
	{
		destDataset->pChannels[ii] = tpList[ii];
		destDataset->pDataPerChannel[ii].channelValue = tpList[ii];
		destDataset->pDataPerChannel[ii].numPcdacValues = numIntercepts;
	}

	for (jj=0; jj<numTPs; jj++)
	{
		kk = 0;
		while ((destDataset->pDataPerChannel[jj].channelValue > srcDataset->pDataPerChannel[kk].channelValue) &&
			   (kk < (srcDataset->numChannels - 1)) )
		{
			kk++;
		}

		for (ii=0; ii < srcDataset->pDataPerChannel[0].numPcdacValues; ii++)
		{
			if (((srcDataset->pDataPerChannel[kk].pPwrValues[ii+1]-srcDataset->pDataPerChannel[kk].pPwrValues[ii]) > 0.5) &&
				(srcDataset->pDataPerChannel[kk].pPwrValues[ii] > 0.0))
			{
				mindB = srcDataset->pDataPerChannel[kk].pPwrValues[ii];
				destDataset->pDataPerChannel[jj].pcdacMin = srcDataset->pDataPerChannel[kk].pPcdacValues[ii];
				break;
			}
		}
	
		currChannel = &(srcDataset->pDataPerChannel[kk]);
		maxdB = currChannel->pPwrValues[currChannel->numPcdacValues-1];
		for (ii=srcDataset->pDataPerChannel[0].numPcdacValues-1; ii>0; ii--)
		{
			if (currChannel->pPwrValues[ii-1] < (maxdB-0.25))
			{				
				destDataset->pDataPerChannel[jj].pcdacMax = currChannel->pPcdacValues[ii];
				break;
			}
		}

		pcdMin = destDataset->pDataPerChannel[jj].pcdacMin;
		pcdMax = destDataset->pDataPerChannel[jj].pcdacMax;

		for (ii=0; ii < destDataset->pDataPerChannel[0].numPcdacValues; ii++)
		{
			intc = pPercentages[ii];
			intdB = (intc*maxdB + (100-intc)*mindB)/100 ;
			for (ss=0; ss<(srcDataset->pDataPerChannel[0].numPcdacValues-2); ss++)
			{
				if ((intdB - currChannel->pPwrValues[ss])*(intdB - currChannel->pPwrValues[ss+1]) <= 0)
				{				
					pcdL = currChannel->pPcdacValues[ss];
					pcdR = currChannel->pPcdacValues[ss+1];	
					pwrL = currChannel->pPwrValues[ss];
					pwrR = currChannel->pPwrValues[ss+1];
					break;
				}
			}

			frac = ((pwrR - pwrL) > 0) ? (intdB - pwrL)/(pwrR - pwrL) : 0;

			destDataset->pDataPerChannel[jj].pPcdacValues[ii] = (A_UINT16)(frac*pcdR + (1.0-frac)*pcdL);
		}
	}
}

void getDutTargetPowerFilename ()
{
	A_UINT32 ii, tempVal;
    FILE *fStream;
    char lineBuf[122], *pLine;
	char delimiters[] = " \t\n\r;=" ;
	A_BOOL done;


	if ( configSetup.dutSSID < 1 )
	{
		uiPrintf("please specify appropriate DUT_CARD_SSID in artsetup.txt\n");
		exit(0);
	} 

	for (ii=0; ii < configSetup.cfgTable.sizeCfgTable; ii++)
	{
		if (configSetup.dutSSID == configSetup.cfgTable.pCfgTableElements[ii].subsystemID)
		{
			break;
		}
	}

    if( (fStream = fopen( configSetup.cfgTable.pCfgTableElements[ii].eepFilename, "r")) == NULL ) {
        uiPrintf("Failed to open %s - using Defaults\n", configSetup.cfgTable.pCfgTableElements[ii].eepFilename);
        return;
    }

	uiPrintf("Modes supported in DUT eep file (%s) are : ", configSetup.cfgTable.pCfgTableElements[ii].eepFilename);

//	uiPrintf("target power filename for golden  : %s\n", CalSetup.tgtPwrFilename);

	done = FALSE;

    while(fgets(lineBuf, 120, fStream) != NULL)  {
        pLine = lineBuf;
        while(isspace(*pLine)) pLine++;
		// skip everything else

/*		if((strnicmp("TARGET_POWER_FILENAME", pLine, strlen("TARGET_POWER_FILENAME")) == 0) &&
					((pLine[strlen("TARGET_POWER_FILENAME")] == ' ') || 
					 (pLine[strlen("TARGET_POWER_FILENAME")] == '\t') ) ){
				pLine = strchr(pLine, '=');
				pLine = strtok(pLine, delimiters);
				if(!sscanf(pLine, "%s", CalSetup.tgtPwrFilename)) {
					uiPrintf("Unable to read the TARGET_POWER_FILENAME from %s\n", configSetup.cfgTable.pCfgTableElements[ii].eepFilename);
				} 
			}
*/		if((strnicmp("A_MODE", pLine, strlen("A_MODE")) == 0) &&
					((pLine[strlen("A_MODE")] == ' ') || 
					 (pLine[strlen("A_MODE")] == '\t') ) ){
				pLine = strchr(pLine, '=');
				pLine = strtok(pLine, delimiters);
				if(!sscanf(pLine, "%d", &tempVal)) {
					uiPrintf("Unable to read the A_MODE from %s\n", configSetup.cfgTable.pCfgTableElements[ii].eepFilename);
				} else {
					if (tempVal > 0) uiPrintf(" 11a ");
					DUT_SUPPORTS_MODE[MODE_11a] = (tempVal) ? TRUE : FALSE;
				}
			}
		if((strnicmp("B_MODE", pLine, strlen("B_MODE")) == 0) &&
					((pLine[strlen("B_MODE")] == ' ') || 
					 (pLine[strlen("B_MODE")] == '\t') ) ){
				pLine = strchr(pLine, '=');
				pLine = strtok(pLine, delimiters);
				if(!sscanf(pLine, "%d", &tempVal)) {
					uiPrintf("Unable to read the B_MODE from %s\n", configSetup.cfgTable.pCfgTableElements[ii].eepFilename);
				} else {
					if (tempVal > 0) uiPrintf(" 11b ");
					DUT_SUPPORTS_MODE[MODE_11b] = (tempVal) ? TRUE : FALSE;
				}
			}
		if((strnicmp("G_MODE", pLine, strlen("G_MODE")) == 0) &&
					((pLine[strlen("G_MODE")] == ' ') || 
					 (pLine[strlen("G_MODE")] == '\t') ) ){
				pLine = strchr(pLine, '=');
				pLine = strtok(pLine, delimiters);
				if(!sscanf(pLine, "%d", &tempVal)) {
					uiPrintf("Unable to read the G_MODE from %s\n", configSetup.cfgTable.pCfgTableElements[ii].eepFilename);
				} else {
					if (tempVal > 0) uiPrintf(" 11g ");
					DUT_SUPPORTS_MODE[MODE_11g] = (tempVal) ? TRUE : FALSE;
				}
			}
	}

	if (fStream) fclose(fStream);

	uiPrintf("\n");
/*
	if (!done)
	{
		uiPrintf("No Target Power specified in the eep file for DUT (ssid = %x)\n", configSetup.dutSSID);
		exit(0);
	}
*/

//	uiPrintf("target power filename for the dut : %s\n", CalSetup.tgtPwrFilename);
}

void dutTestTXRX(A_UINT32 devNum, A_UINT32 mode)
{
	A_UINT32 *channelTimes10;
		
	A_UINT16 index, ChannelCount, rate, ii;
    RX_STATS_STRUCT rStats;

	// txDataSetup
	A_UINT32 rate_mask = RATE_GROUP | PER_RATEMASK;
	A_UINT32 turbo = 0;

	A_UINT32 num_tx_desc = PER_TEST_FRAME;
	A_UINT32 tx_length = PER_FRAME_LEN;
	A_UINT32 per_rate_count_local = PER_RATE_COUNT;
	A_UINT32 *per_test_rates_local = &(PER_TEST_RATE[0]);
	A_UINT32 retry = 0;		// broadcast mode, disable retry
	A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 0;
	A_CHAR  testname_local[128];

	// rxDataSetup
	A_UINT32 num_rx_desc = 10;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode =  ENABLE_STATS_RECEIVE; // ENABLE_STATS_SEND |
	A_UINT32 compare = 0;

	A_INT32  attenVal;

	A_UINT32 trig_level;
	A_BOOL	 done_reset = FALSE;

	A_UINT32 numSenRates;
	A_UINT32 senRates[8]; 
	A_UINT32 kk;
	A_UINT32 testingMask = TEST_NORMAL_MODE;
   A_UINT32 lowestPowerRate = 54;
	
	double	 goldenTXPowerLocal, attenDutGoldenLocal ;
	A_BOOL	 testTurboLocal, testPERLocal, testSENLocal;

	A_UINT16 iter, channel_repeat;
	A_BOOL   failedThisTest, restore_local_lib_print;
	A_INT32 ppmMaxLimit = CalSetup.ppmMaxLimit;
	A_INT32 ppmMinLimit = CalSetup.ppmMinLimit;

	if( isGriffin_lite(macRev) && CalSetup.testTURBO_2p4[mode]  || 
		isEagle_lite(macRev) && (CalSetup.testTURBO || CalSetup.testTURBO_2p4[mode]) ){
		uiPrintf("\n\n###################################################################\n");
		uiPrintf("Turbo mode not supported by the ar5005g, 11g_TEST_TURBO_MODE\n");
		uiPrintf("is set in calsetup.txt.  Disable turbo mode test in calsetup.txt and restart the golden\n");
		uiPrintf("###################################################################\n\n");
		exit(0);
	}

	testTurboLocal = TEST_NORMAL_MODE;		
	if (mode == MODE_11a)
	{
		attenDutGoldenLocal = CalSetup.attenDutGolden;
		goldenTXPowerLocal = CalSetup.goldenTXPower;
		if (CalSetup.testTURBO) testTurboLocal |= TEST_TURBO_MODE;
		if (CalSetup.testHALFRATE) testTurboLocal |= TEST_HALF_MODE;
		if (CalSetup.testQUARTERRATE) testTurboLocal |= TEST_QUARTER_MODE;
		testPERLocal = CalSetup.testTXPER;
		testSENLocal = CalSetup.testRXSEN;		
	} else
	{
		attenDutGoldenLocal = CalSetup.attenDutGolden_2p4[mode];
		goldenTXPowerLocal = CalSetup.goldenTXPower_2p4[mode];
		testPERLocal = CalSetup.testTXPER_2p4[mode];
		testSENLocal = CalSetup.testRXSEN_2p4[mode];
		if ((mode == MODE_11g) && (CalSetup.testTURBO_2p4[mode])) testTurboLocal |= TEST_TURBO_MODE;
	}

    // setup attenuator here
    if(CalSetup.useInstruments) {		
    	attSet(devATT, (A_INT32)(goldenTXPowerLocal - attenDutGoldenLocal - pTestSet[mode]->pTestChannel[0].targetSensitivity ));
    }

	if (TX_PER_STRESS_MARGIN > 0) {
		testSENLocal = FALSE;
		rate_mask = RATE_GROUP | PER_MARGIN_RATEMASK;
		per_rate_count_local = PER_MARGIN_RATE_COUNT;
		per_test_rates_local = &(PER_MARGIN_TEST_RATE[0]);
		uiPrintf("\n\nStart PER +%1ddB Margin Test in %s mode", TX_PER_STRESS_MARGIN, modeName[mode]);
		if(mode != MODE_11b) {
			uiPrintf("\nFREQ          48Mbps      54Mbps            ():RSSI\n");
		} else {
			uiPrintf("\nFREQ          11Mbps(L)   11Mbps(S)      ():RSSI\n");
		}
	} else {
		if(do_11g_CCK) {
			uiPrintf("\n\nStart PER and SEN Test in 11g CCK mode");
		} else {
		uiPrintf("\n\nStart PER and SEN Test in %s mode", modeName[mode]);
      }
		if(mode != MODE_11b) {
			uiPrintf("\nFREQ          6Mbps      36Mbps      48Mbps      54Mbps     ppm    ():RSSI\n");
      } else {
			uiPrintf("\nFREQ          1Mbps      5.5Mbps(S)  11Mbps(L)   11Mbps(S)      ():RSSI\n");
		}
	}

   if(do_11g_CCK) {
       rate_mask = RATE_GROUP | PER_RATEMASK_11G_CCK;
       lowestPowerRate = 0xdb;
       per_test_rates_local = &PER_TEST_RATE_11G_CCK[0];
   }

	ChannelCount = pTestSet[mode]->numTestChannelsDefined;
	channelTimes10 = (A_UINT32 *)malloc(ChannelCount*sizeof(A_UINT32)) ;
	if(NULL == channelTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in TX RX test\n");
		return;
	}
	for (ii=0; ii<ChannelCount; ii++)
	{
		channelTimes10[ii] = pTestSet[mode]->pTestChannel[ii].channelValueTimes10;
	}

//	art_rereadProm(devNum); // re-read the calibration data from eeprom
	if(mode != MODE_11b) {
		trig_level = art_getFieldForMode(devNum, "mc_trig_level", 
			((mode == MODE_11g) ? MODE_11G : MODE_11A), 0);
	}

	while (testTurboLocal)
//	for(turbo_iter=0; turbo_iter <= testTurboLocal; turbo_iter++)
	{
		//turbo = turbo_iter ;	
/*			configSetup.printPciWrites = 1;
			art_configureLibParams(devNum);
*/
		if(!(testTurboLocal & 0x01)) {
			testingMask = testingMask << 1;
			testTurboLocal = testTurboLocal >> 1;
			continue;
		}

		if(testingMask & TEST_TURBO_MODE)
		{
			if (TX_PER_STRESS_MARGIN > 0) {
				uiPrintf("\n\nFREQ         96Mbps     108Mbps          TURBO MODE TESTING\n");
			} else {
				uiPrintf("\n\nFREQ         12Mbps      72Mbps      96Mbps     108Mbps     ppm  TURBO MODE TESTING\n");
			}
			turbo = TURBO_ENABLE;
		}
			
		if(testingMask & TEST_HALF_MODE)
		{
			if (TX_PER_STRESS_MARGIN > 0) {
				uiPrintf("\n\nFREQ         24Mbps     27Mbps          HALF RATE MODE TESTING\n");
			} else {
				uiPrintf("\n\nFREQ         3Mbps      18Mbps      24Mbps     27Mbps     ppm  HALF RATE MODE TESTING\n");
			}
			turbo = HALF_SPEED_MODE ;
		}

		if(testingMask & TEST_QUARTER_MODE)
		{
			if (TX_PER_STRESS_MARGIN > 0) {
				uiPrintf("\n\nFREQ         12Mbps     13.5Mbps          QUARTER RATE MODE TESTING\n");
			} else {
				uiPrintf("\n\nFREQ         1.5Mbps      9Mbps      12Mbps     13.5Mbps     ppm  QUARTER RATE MODE TESTING\n");
			}
			turbo = QUARTER_SPEED_MODE ;
		}

		if( (turbo == HALF_SPEED_MODE) || (turbo == QUARTER_SPEED_MODE) ) {
			ppmMaxLimit = CalSetup.ppmMaxQuarterLimit;
			ppmMinLimit = CalSetup.ppmMinQuarterLimit;
		}
		else {
			ppmMaxLimit = CalSetup.ppmMaxLimit;
			ppmMinLimit = CalSetup.ppmMinLimit;
		}
		for(index=0; index<ChannelCount; index++) {
			if (( pTestSet[mode]->pTestChannel[index].TEST_PER < 1) && (turbo == 0))
			{
				continue; // skip PER/SEN test at this channel in base mode unless requested
			}
			if (( pTestSet[mode]->pTestChannel[index].TEST_TURBO < 1) && (turbo == TURBO_ENABLE))
			{
				continue; // skip PER/SEN test at this channel in turbo mode unless requested
			}
			if (!(pTestSet[mode]->pTestChannel[index].TEST_HALF_QUART_RATE & CHANNEL_TEST_HALF_RATE) && (turbo == HALF_SPEED_MODE))
			{
				continue; // skip PER/SEN test at this channel in half rate mode unless requested
			}
			if (!(pTestSet[mode]->pTestChannel[index].TEST_HALF_QUART_RATE & CHANNEL_TEST_QUART_RATE) && (turbo == QUARTER_SPEED_MODE))
			{
				continue; // skip PER/SEN test at this channel in quarter rate mode unless requested
			}

			//cleanup descriptor queues, to free up mem
			art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);
		    if(testPERLocal) 
			{

	// need to reset the params changed by SEN test for PER for each iteration
				num_rx_desc = 10;
				rx_length = 100;
				start_timeout = 0;	// no wait time for transmit
				complete_timeout = ((mode == MODE_11b) ? 10000 : 5000);
				stats_mode =  ENABLE_STATS_RECEIVE; // ENABLE_STATS_SEND |

	// send message to setup for receive here
				// setup attenuator here
				if(CalSetup.useInstruments) {
					attenVal = (A_INT32)(getTargetPower(mode, pTestSet[mode]->pTestChannel[index].channelValueTimes10/10,lowestPowerRate) 
						- attenDutGoldenLocal - pTestSet[mode]->pTestChannel[index].targetSensitivity - CalSetup.txperBackoff) ;
                                        attenVal += getPowerRef();
					attenVal -= (turbo == 0) ? 0 : 4;
					attenVal += TX_PER_STRESS_MARGIN;
					attSet(devATT, attenVal);
				}

				
				if(mode != MODE_11b) {
					if(isDragon_sd(swDeviceID)) {
					    art_writeField(devNum, "mc_trig_level", 0x1);
					} else {
					    art_writeField(devNum, "mc_trig_level", 0x3f);
					}
				} 
				if(1)
				//if (!done_reset)
				{
					art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
					done_reset = TRUE;
					if(configSetup.force_antenna && isDragon_sd(swDeviceID))
					{
						updateAntenna(devNum);
					}
				} else
				{
					art_changeChannel(devNum, *(channelTimes10+index));
				}
				if (TX_PER_STRESS_MARGIN > 0) {
					stress_target_powers(devNum, (*(channelTimes10+index))/10, (double)TX_PER_STRESS_MARGIN);
				}
				iter = 0;
				failedThisTest = TRUE;

				restore_local_lib_print = printLocalInfo;
				printLocalInfo = 0;

				while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
					iter++;
					art_txDataSetup(devNum,rate_mask,rxStation,num_tx_desc,tx_length,pattern,2,retry,antenna, broadcast);
					art_rxDataSetup(devNum,num_rx_desc,rx_length,enable_ppm);
					
					sendAck(devNum, "PER: Prepare to receive", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					Sleep(30);
					art_txrxDataBegin(devNum,start_timeout,complete_timeout,stats_mode,compare,pattern,2);
					failedThisTest = FALSE;
					for(rate=0; rate<per_rate_count_local; rate++) {
/*						if ((mode == MODE_11a) && (PER_TEST_RATE[rate] == 54) && isUNI3OddChannel(*(channel+index))) {
							continue;
						}
*/						art_rxGetStats(devNum, per_test_rates_local[rate], 1, &rStats);
						if(rStats.goodPackets < (A_UINT16)(PER_TEST_FRAME*CalSetup.perPassLimit/100)) {
							failedThisTest |= TRUE;
						} 
					}
//					if ((mode != MODE_11b) && (turbo != TURBO_ENABLE) && (turbo != HALF_SPEED_MODE))
					
					if ((mode == MODE_11a) && (TX_PER_STRESS_MARGIN == 0))
					{
						if(((rStats.ppmAvg + CalSetup.goldenPPM) > ppmMaxLimit)||((rStats.ppmAvg + CalSetup.goldenPPM)< ppmMinLimit)) {
							failedThisTest |= TRUE;						
						}
					}
//					if(art_mdkErrNo!=0) failedThisTest = FALSE ; // if link is really bad - no point wasting time
					if (art_mdkErrNo != 0) {
						art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
						if(configSetup.force_antenna && isDragon_sd(swDeviceID))
						{
							updateAntenna(devNum);
						}
					}
					channel_repeat = (failedThisTest && (iter < CalSetup.maxRetestIters)) ? 1 : 0 ;
					sendAck(devNum, "PER: Repeat This Channel - see par2", *(channelTimes10+index), channel_repeat, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}

					if (CalSetup.customerDebug) uiPrintf("SNOOP: per repeat iter = %d \n", iter);
				} // while failedThisTest

				if(art_mdkErrNo!=0)
					uiPrintf("\n***** Failed sending packet to Golden Unit (code:%d)\n", art_mdkErrNo);

				printLocalInfo = restore_local_lib_print;

				if (TX_PER_STRESS_MARGIN > 0) {
					sprintf(testname_local, "txper%s_%1ddB_margin", ((turbo==0) ? "" : ( (turbo==1) ? "_turbo" : ( (turbo==HALF_SPEED_MODE) ? "_halfspeed" : ""))),
							                                        TX_PER_STRESS_MARGIN);
				} else {
					sprintf(testname_local, "txper%s", ((turbo==0) ? "" : ( (turbo==1) ? "_turbo" : ( (turbo==HALF_SPEED_MODE) ? "_halfspeed" : ""))));
				}
				setYieldParamsForTest(devNum, modeName[mode], testname_local, "good_pkts", "percent", "rssi", "dB"); 
				uiPrintf("%5.1lf PER     ", (double)(*(channelTimes10+index))/10);
				for(rate=0; rate<per_rate_count_local; rate++) {
/*					if ((mode == MODE_11a) && (PER_TEST_RATE[rate] ==54) && isUNI3OddChannel(*(channel+index)))
					{
						if (CalSetup.customerDebug)
						{
							uiPrintf("                 ");
						} else 
						{
							uiPrintf("            ");
						}
						continue;
					}
*/
					art_rxGetStats(devNum, per_test_rates_local[rate], 1, &rStats);
					if (CalSetup.customerDebug)
					{
						uiPrintf("%3d(%2d) [%3d]", rStats.goodPackets, rStats.DataSigStrengthAvg, rStats.crcPackets); 
					} else
					{
						uiPrintf("%3d(%2d) ", rStats.goodPackets, rStats.DataSigStrengthAvg); 
					}
					if(rStats.goodPackets < (A_UINT16)(PER_TEST_FRAME*CalSetup.perPassLimit/100)) {
						uiPrintf("(X) ");
						TestFail = TRUE;
						failTest[testnum+1] = 1;
						logMeasToYield((double)(channelTimes10[index])/10, (double)(per_test_rates_local[rate]), TX_PER_STRESS_MARGIN, (double)(PER_TEST_FRAME*CalSetup.perPassLimit/100), (double)(rStats.goodPackets), (double)(rStats.DataSigStrengthAvg), "FAIL");
					} else  {
						uiPrintf("    ");
						logMeasToYield((double)(channelTimes10[index])/10, (double)(per_test_rates_local[rate]), TX_PER_STRESS_MARGIN, (double)(PER_TEST_FRAME*CalSetup.perPassLimit/100), (double)(rStats.goodPackets), (double)(rStats.DataSigStrengthAvg), "PASS");
					}

				}

				if ((mode == MODE_11g) && (TX_PER_STRESS_MARGIN == 0)){ // do ppm test 
					iter = 0;
					failedThisTest = TRUE;

					restore_local_lib_print = printLocalInfo;
					printLocalInfo = 0;
						
					rate_mask  = RATE_GROUP|RATE_54;
					num_tx_desc = NUM_11G_PPM_PKTS;

					while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
						iter++;

						art_txDataSetup(devNum,rate_mask,rxStation,num_tx_desc,tx_length,pattern,2,retry,antenna, broadcast);
						art_rxDataSetup(devNum,num_rx_desc,rx_length,enable_ppm);
						
						sendAck(devNum, "PPM: Prepare to receive", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
						waitForAck(CalSetup.customerDebug);
						if (REWIND_TEST) {
							return;
						}
						Sleep(10);
						art_txrxDataBegin(devNum,start_timeout,complete_timeout,stats_mode,compare,pattern,2);

						failedThisTest = FALSE;
						art_rxGetStats(devNum, 54, 1, &rStats);
						if(((rStats.ppmAvg + CalSetup.goldenPPM) > ppmMaxLimit)||((rStats.ppmAvg + CalSetup.goldenPPM)< ppmMinLimit)) {
							failedThisTest |= TRUE;						
						}
						if (art_mdkErrNo != 0) {
							art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
							if(configSetup.force_antenna && isDragon_sd(swDeviceID))
							{
								updateAntenna(devNum);
							}
						}
						channel_repeat = (failedThisTest && (iter < CalSetup.maxRetestIters)) ? 1 : 0 ;
						sendAck(devNum, "PPM: Repeat This Channel - see par2", *(channelTimes10+index), channel_repeat, 0, CalSetup.customerDebug);
						waitForAck(CalSetup.customerDebug);
						if (REWIND_TEST) {
							return;
						}

						if (CalSetup.customerDebug) uiPrintf("SNOOP: 11g ppm repeat iter = %d \n", iter);
					} // while failedThisTest

					if(art_mdkErrNo!=0)
						uiPrintf("\n***** Failed sending packet to Golden Unit [11g ppm] (code:%d)\n", art_mdkErrNo);

					printLocalInfo = restore_local_lib_print;
   	         if(do_11g_CCK) {
                  rate_mask = RATE_GROUP | PER_RATEMASK_11G_CCK;
               } else {
					rate_mask = RATE_GROUP | PER_RATEMASK;					
               }
					num_tx_desc = PER_TEST_FRAME;
				} // end 11g ppm test


				if ((mode != MODE_11b) && (TX_PER_STRESS_MARGIN == 0))
				{
					setYieldParamsForTest(devNum, modeName[mode], "txper_ppm", "ppm", "ppm", "none", "none"); 					
					uiPrintf("%2d", rStats.ppmAvg + CalSetup.goldenPPM);
					if(((rStats.ppmAvg  + CalSetup.goldenPPM) > ppmMaxLimit)||((rStats.ppmAvg+ CalSetup.goldenPPM)< ppmMinLimit)) {
						uiPrintf("(X)");
						TestFail = TRUE;
						failTest[testnum+3] = 1; // ppm fail
						logMeasToYield((double)(channelTimes10[index])/10, (double)iter, 0.0, 0.0, (double)(rStats.ppmAvg+ CalSetup.goldenPPM), 0.0, "FAIL");
					} else {
						logMeasToYield((double)(channelTimes10[index])/10, (double)iter, 0.0, 0.0, (double)(rStats.ppmAvg+ CalSetup.goldenPPM), 0.0, "PASS");
					}

					art_writeField(devNum, "mc_trig_level", trig_level);
				}
				uiPrintf("\n");

			}  // if test_TXPER

// turn around for RX_SEN at 48 and 54

			if (testSENLocal)
			{
			    //art_memFreeAll(devNum);
				if (!testPERLocal)
//				if (1)
				{
					//if (!done_reset)
					if (1)
					{
						art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
						done_reset = TRUE;
						if(configSetup.force_antenna && isDragon_sd(swDeviceID))
						{
							updateAntenna(devNum);
						}
					} else
					{
						art_changeChannel(devNum, *(channelTimes10+index));
					}
				}

				numSenRates = SEN_RATE_COUNT;
				for (kk = 0; kk < numSenRates; kk++)
				{
               if(do_11g_CCK) {
                  senRates[kk] = SEN_TEST_RATE_11G_CCK[kk];
               } else {
					senRates[kk] = SEN_TEST_RATE[kk];
				}
				}

				num_rx_desc = CalSetup.numSensPackets[mode] * numSenRates; // + 40;
				rx_length = SEN_FRAME_LEN;
				start_timeout = (mode == MODE_11b) ? 30000 : 20000;		// 2000 will cause error
				complete_timeout = 15000 + num_rx_desc;
				stats_mode = NO_REMOTE_STATS ;

				// Set attenuator
				if(CalSetup.useInstruments) {
					attenVal = (A_INT32)(goldenTXPowerLocal - attenDutGoldenLocal - pTestSet[mode]->pTestChannel[index].targetSensitivity);
					attenVal -= (turbo == 0) ? 0 : 4; // give a 4 dB margin in turbo mode
					attenVal -= CalSetup.rxSensBackoff; 
    				attSet(devATT, attenVal);
				}

				iter = 0;
				failedThisTest = TRUE;
				restore_local_lib_print = printLocalInfo;
				printLocalInfo = 0;
				while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
					iter++;

					if (!configSetup.force_antenna)
					{
					art_setAntenna(devNum, (USE_DESC_ANT | DESC_ANT_A));
					}
					art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);
					
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					art_rxDataStart(devNum);
					sendAck(devNum, "RXSEN: Ready to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
					//art_rxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
					
					failedThisTest = FALSE;
					for(rate=0; rate<numSenRates; rate++) {
						art_rxGetStats(devNum, senRates[rate], 0, &rStats);
						if(rStats.goodPackets < (A_UINT16)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100)) {
							failedThisTest |= TRUE;
						}
					}
//					if(art_mdkErrNo!=0) failedThisTest = FALSE ; // if link is really bad - no point wasting time
					if (art_mdkErrNo != 0) {
						art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
						if(configSetup.force_antenna && isDragon_sd(swDeviceID))
						{
							updateAntenna(devNum);
						}
					}
					channel_repeat = (failedThisTest && (iter < CalSetup.maxRetestIters)) ? 1 : 0 ;

					sendAck(devNum, "SEN: Repeat This Channel - see par2", *(channelTimes10+index), channel_repeat, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}

					if (CalSetup.customerDebug) uiPrintf("SNOOP: sen repeat iter = %d \n", iter);
				} // while failedThisTest

				if(art_mdkErrNo!=0)
					uiPrintf("\nError Code (%d)", art_mdkErrNo);

				printLocalInfo = restore_local_lib_print;

				if (!testPERLocal)
				{
					uiPrintf("%5.1lf SEN     ", (double)(*(channelTimes10+index))/10);
				} else
				{
					uiPrintf("       SEN     ");
				}

				if (CalSetup.customerDebug)
				{
					uiPrintf("%s%s", "                 ", "                 ");
				} else 
				{
					uiPrintf("%s%s", "            ", "            ");
				}

				sprintf(testname_local, "sens%s", ((turbo==0) ? "" : ( (turbo==1) ? "_turbo" : ( (turbo==HALF_SPEED_MODE) ? "_halfspeed" : ""))));
				setYieldParamsForTest(devNum, modeName[mode], testname_local, "good_pkts", "percent", "rssi", "dB"); 

				for(rate=0; rate<numSenRates; rate++) {
					art_rxGetStats(devNum, senRates[rate], 0, &rStats);
/*					if ((mode == MODE_11a) && (senRates[rate] ==54) && isUNI3OddChannel(*(channel+index)))
					{
						uiPrintf("            ");
						continue;
					}
*/

					if (CalSetup.customerDebug)
					{
						uiPrintf("%3d(%2d) [%3d] [%d]", (A_UINT16)(rStats.goodPackets*100/CalSetup.numSensPackets[mode]), rStats.DataSigStrengthAvg, rStats.crcPackets, rStats.goodPackets); 
					} else
					{
						uiPrintf("%3d(%2d) ", (A_UINT16)(rStats.goodPackets*100/CalSetup.numSensPackets[mode]), rStats.DataSigStrengthAvg); 
					}
					if(rStats.goodPackets < (A_UINT16)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100)) {
						uiPrintf("(X) ");
						TestFail = TRUE;
						failTest[testnum+2] = 1; // sens fail
						logMeasToYield((double)(channelTimes10[index])/10, (double)(senRates[rate]), (double)iter, (double)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100), (double)(rStats.goodPackets), (double)(rStats.DataSigStrengthAvg), "FAIL");
					} else
					{
						uiPrintf("    ");
						logMeasToYield((double)(channelTimes10[index])/10, (double)(senRates[rate]), (double)iter, (double)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100), (double)(rStats.goodPackets), (double)(rStats.DataSigStrengthAvg), "PASS");
					}
				}
				uiPrintf( "\n");

// also do RX_SEN at 6

				if (pTestSet[mode]->pTestChannel[index].LO_RATE_SEN)
				{
//					if (!testPERLocal && !pTestSet[mode]->pTestChannel[index].TEST_PER)
					if (1)
					{
						//if (!done_reset)
						if (1)
						{
							art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
							done_reset = TRUE;
							if(configSetup.force_antenna && isDragon_sd(swDeviceID))
							{
								updateAntenna(devNum);
							}	
						} else
						{
							art_changeChannel(devNum, *(channelTimes10+index));
						}
					}

					numSenRates = 1;
					for (kk = 0; kk < numSenRates; kk++)
					{
                  if(do_11g_CCK) {
                      senRates[kk] = LO_SEN_TEST_RATE_11G_CCK[kk];    
                  } else {
						senRates[kk] = 6; // 6 mbps
					}
					}

					num_rx_desc = CalSetup.numSensPackets[mode] * numSenRates + 40;
					rx_length = SEN_FRAME_LEN;
					start_timeout = (mode == MODE_11b) ? 30000 : 20000;		// 2000 will cause error
					complete_timeout = 15000 + num_rx_desc;
					stats_mode = NO_REMOTE_STATS ;

					// Set attenuator
					if(CalSetup.useInstruments) {
						attenVal = (A_INT32)(goldenTXPowerLocal - attenDutGoldenLocal - pTestSet[mode]->pTestChannel[index].targetLoRateSensitivity);
						attenVal -= (turbo == 0) ? 0 : 4; // give a 4 dB margin in turbo mode
					        attenVal -= CalSetup.rxSensBackoff; 
    					attSet(devATT, attenVal);
					}

					iter = 0;
					failedThisTest = TRUE;
					restore_local_lib_print = printLocalInfo;
					printLocalInfo = 0;

					while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
						iter++;
						if (!configSetup.force_antenna)
						{
						art_setAntenna(devNum, (USE_DESC_ANT | DESC_ANT_A));
						}
						art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);
						
						waitForAck(CalSetup.customerDebug);
						if (REWIND_TEST) {
							return;
						}
						art_rxDataStart(devNum);
						sendAck(devNum, "RXSEN: Ready to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
						art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
						//art_rxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
						
						failedThisTest = FALSE;
						for(rate=0; rate<numSenRates; rate++) {
							art_rxGetStats(devNum, senRates[rate], 0, &rStats);
							if(rStats.goodPackets < (A_UINT16)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100)) {
								failedThisTest |= TRUE;
							}
						}
//						if(art_mdkErrNo!=0) failedThisTest = FALSE ; // if link is really bad - no point wasting time
						if (art_mdkErrNo != 0) {
							art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
							if(configSetup.force_antenna && isDragon_sd(swDeviceID))
							{
								updateAntenna(devNum);
							}
						}
						channel_repeat = (failedThisTest && (iter < CalSetup.maxRetestIters)) ? 1 : 0 ;
						sendAck(devNum, "SEN_LO: Repeat This Channel - see par2", *(channelTimes10+index), channel_repeat, 0, CalSetup.customerDebug);
						waitForAck(CalSetup.customerDebug);
						if (REWIND_TEST) {
							return;
						}

						if (CalSetup.customerDebug) uiPrintf("SNOOP: sen_lo repeat iter = %d \n", iter);
					}  // while failedThisTest

					if(art_mdkErrNo!=0)
							uiPrintf("\nError Code (%d)", art_mdkErrNo);
					printLocalInfo = restore_local_lib_print;
	
					if (!testPERLocal && !pTestSet[mode]->pTestChannel[index].TEST_PER)
					{
						uiPrintf("%5.1lf SEN_LO  ", (double)(*(channelTimes10+index))/10);
					} else
					{
						uiPrintf("       SEN_LO  ");
					}

					sprintf(testname_local, "sens%s", ((turbo==0) ? "" : ( (turbo==1) ? "_turbo" : ( (turbo==HALF_SPEED_MODE) ? "_halfspeed" : ""))));
					setYieldParamsForTest(devNum, modeName[mode], testname_local, "good_pkts", "percent", "rssi", "dB"); 
					for(rate=0; rate<numSenRates; rate++) {
						art_rxGetStats(devNum, senRates[rate], 0, &rStats);

						if (CalSetup.customerDebug)
						{
							uiPrintf("%3d(%2d) [%3d]", (A_UINT16)(rStats.goodPackets*100/CalSetup.numSensPackets[mode]), rStats.DataSigStrengthAvg, rStats.crcPackets); 
						} else
						{
							uiPrintf("%3d(%2d) ", (A_UINT16)(rStats.goodPackets*100/CalSetup.numSensPackets[mode]), rStats.DataSigStrengthAvg); 
						}
						if(rStats.goodPackets < (A_UINT16)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100)) {
							uiPrintf("(X) ");
							TestFail = TRUE;
							failTest[testnum+2] = 1; // lo rate sen
							logMeasToYield((double)(channelTimes10[index])/10, (double)(senRates[rate]), (double)iter, (double)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100), (double)(rStats.goodPackets), (double)(rStats.DataSigStrengthAvg), "FAIL");
						} else
						{
							uiPrintf("    ");
							logMeasToYield((double)(channelTimes10[index])/10, (double)(senRates[rate]), (double)iter, (double)(CalSetup.numSensPackets[mode]*CalSetup.senPassLimit/100), (double)(rStats.goodPackets), (double)(rStats.DataSigStrengthAvg), "PASS");
						}
					}
					uiPrintf( "\n");
				} // if test_LO_RATE_RXSEN
			} // if test_RXSEN 

		} // ch loop

/*
			configSetup.printPciWrites = 0;
			art_configureLibParams(devNum);
*/
		testingMask = testingMask << 1;
		testTurboLocal = testTurboLocal >> 1;
	} // turbo loop

	if (channelTimes10) {
		free(channelTimes10);
	}
}

void goldenTestTXRX(A_UINT32 devNum, A_UINT32 mode)
{
	A_UINT32 *channelTimes10;

	A_UINT16 index, ChannelCount, ii;

	A_UINT32 rate_mask = SEN_RATEMASK;
    A_UINT32 turbo = 0;
//	A_UINT32 num_desc_per_rate = SEN_TEST_FRAME;
	A_UINT32 num_desc_per_rate = CalSetup.numSensPackets[mode];
	A_UINT32 tx_length = SEN_FRAME_LEN;
	A_UINT32 retry = 0;		// broadcast mode, disable retry
    A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 1;	// disable broadcast mode

    // for rx
	A_UINT32 num_rx_desc = PER_TEST_FRAME * PER_RATE_COUNT + 40;
	A_UINT32 rx_length = PER_FRAME_LEN;
	A_UINT32 enable_ppm = 1; 


    // for rxDataBegin
	A_UINT32 start_timeout = 5000;
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode = ENABLE_STATS_SEND ; //| ENABLE_STATS_RECEIVE;
	A_UINT32 compare = 0;
	A_BOOL done_reset = FALSE;
	A_UINT32 testingMask = TEST_NORMAL_MODE;


//	A_UINT16 goldenPwrList[8], PERStatsPwrList[8] ;
	A_UINT32 trig_level;


	TARGETS_SET	*pTargetsSetLocal;
	double		goldenTXPowerLocal ;
	A_BOOL		testTurboLocal, testPERLocal, testSENLocal;

	A_BOOL   repeatThisTest;

	testTurboLocal = TEST_NORMAL_MODE;		
	if (mode == MODE_11a)
	{
		pTargetsSetLocal = pTargetsSet;
		goldenTXPowerLocal = CalSetup.goldenTXPower;
		if (CalSetup.testTURBO) testTurboLocal |= TEST_TURBO_MODE;
		if (CalSetup.testHALFRATE) testTurboLocal |= TEST_HALF_MODE;
		if (CalSetup.testQUARTERRATE) testTurboLocal |= TEST_QUARTER_MODE;
		testPERLocal = CalSetup.testTXPER;
		testSENLocal = CalSetup.testRXSEN;
	} else
	{
		pTargetsSetLocal = pTargetsSet_2p4[mode];
		goldenTXPowerLocal = CalSetup.goldenTXPower_2p4[mode];
		testPERLocal = CalSetup.testTXPER_2p4[mode];
		testSENLocal = CalSetup.testRXSEN_2p4[mode];
		if ((mode == MODE_11g) && (CalSetup.testTURBO_2p4[mode])) testTurboLocal |= TEST_TURBO_MODE;
	}

	if ((mode == MODE_11b) || (mode == MODE_11g))
		enable_ppm = 0;

	if (TX_PER_STRESS_MARGIN > 0) {
		testSENLocal = FALSE;
		uiPrintf("\n\nStart PER +%1ddB MARGIN Test for %s mode \n", TX_PER_STRESS_MARGIN, modeName[mode]);
	} else {
		uiPrintf("\n\nStart PER and SEN Test for %s mode \n", modeName[mode]);
	}

	ChannelCount = pTestSet[mode]->numTestChannelsDefined;
	channelTimes10 = (A_UINT32 *)malloc(ChannelCount*sizeof(A_UINT32)) ;
	if(NULL == channelTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in TX RX test\n");
		return;
	}
	for (ii=0; ii<ChannelCount; ii++)
	{
		channelTimes10[ii] = pTestSet[mode]->pTestChannel[ii].channelValueTimes10;
	}

	if(mode != MODE_11b) {
		// to avoid TX underruns on slow machines
		trig_level = art_getFieldForMode(devNum, "mc_trig_level", 
			((mode == MODE_11g) ? MODE_11G : MODE_11A), 0);
		//art_writeField(devNum, "mc_trig_level", 0x3f);
	}

   // Start receiving
	while (testTurboLocal)
//	for(turbo_iter=0; turbo_iter <= testTurboLocal; turbo_iter++)
	{
/*
			configSetup.printPciWrites = 1;
			art_configureLibParams(devNum);
*/
//		turbo = turbo_iter ;	
		if(!(testTurboLocal & 0x01)) {
			testingMask = testingMask << 1;
			testTurboLocal = testTurboLocal >> 1;
			continue;
		}

		if(testingMask & TEST_TURBO_MODE)
		{
			uiPrintf("\nTesting Turbo Mode\n");
			turbo = TURBO_ENABLE;
		}
			
		if(testingMask & TEST_HALF_MODE)
		{
			uiPrintf("\nTesting Half Rate Mode\n");
			turbo = HALF_SPEED_MODE ;
		}

		if(testingMask & TEST_QUARTER_MODE)
		{
			uiPrintf("\nTesting Quarter Rate Mode\n");
			turbo = QUARTER_SPEED_MODE ;
		}

		for(index=0; index<ChannelCount; index++) {
			if (( pTestSet[mode]->pTestChannel[index].TEST_PER < 1) && (turbo == 0))
			{
				continue; // skip PER test at this channel in base mode unless requested
			}
			if (( pTestSet[mode]->pTestChannel[index].TEST_TURBO < 1) && (turbo == TURBO_ENABLE))
			{
				continue; // skip PER/SEN test at this channel in turbo mode unless requested
			}
			if (!(pTestSet[mode]->pTestChannel[index].TEST_HALF_QUART_RATE & CHANNEL_TEST_HALF_RATE) && (turbo == HALF_SPEED_MODE))
			{
				continue; // skip PER/SEN test at this channel in half rate mode unless requested
			}
			if (!(pTestSet[mode]->pTestChannel[index].TEST_HALF_QUART_RATE & CHANNEL_TEST_QUART_RATE) && (turbo == QUARTER_SPEED_MODE))
			{
				continue; // skip PER/SEN test at this channel in quarter rate mode unless requested
			}

			//cleanup descriptor queues, to free up mem
			art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);

			if (testPERLocal)
			{
	// reset params changed by SEN test for PER for each iteration
				complete_timeout = 5000;
				num_rx_desc = PER_TEST_FRAME * PER_RATE_COUNT + 40;
				stats_mode = ENABLE_STATS_SEND ; //| ENABLE_STATS_RECEIVE;

				uiPrintf("\nReceiving Freq: %5.1lf GHz\n", (double)(*(channelTimes10+index))/10);
				//art_setSingleTransmitPower(devNum, 50); // send stats back at higher power
				//if (!done_reset)
				if(1)
				{
					art_resetDevice(devNum, rxStation, bssID, *(channelTimes10+index), turbo);
					done_reset = TRUE;
				} else
				{
					art_changeChannel(devNum, *(channelTimes10+index));
				}
				//art_forcePowerTxMax(devNum, PERStatsPwrList);
				art_forceSinglePowerTxMax(devNum, 2*20); // send stats at 20dBm
				
				repeatThisTest = TRUE;
				while (repeatThisTest) {
					art_setAntenna(devNum, (USE_DESC_ANT | DESC_ANT_A));
					art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);

					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					start_timeout = 50000;  // make it big as starting to rx before sending ack over ethernet.					
					art_rxDataStart(devNum);
					sendAck(devNum, "PER: Ready to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
					//art_rxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);

					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					if (!verifyAckStr("PER: Repeat This Channel - see par2")) {
						uiPrintf("ERROR: miscommunication in sync. expected ack string: PER: Repeat This Channel - see par2\n");
						exit(0);
					}
					repeatThisTest = (ackRecvPar2 > 0) ? TRUE : FALSE ;
					sendAck(devNum, "PER: Repeat This Channel - saw par2", *(channelTimes10+index), ackRecvPar2, 0, CalSetup.customerDebug);
				}

				if ((mode == MODE_11g) && (TX_PER_STRESS_MARGIN == 0)){ // do ppm measurement
					enable_ppm = 1;
					num_rx_desc = NUM_11G_PPM_PKTS + 20;
					repeatThisTest = TRUE;
					while (repeatThisTest) {
						art_setAntenna(devNum, (USE_DESC_ANT | DESC_ANT_A));
						art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);

						waitForAck(CalSetup.customerDebug);
						if (REWIND_TEST) {
							return;
						}
						start_timeout = 50000;  // make it big as starting to rx before sending ack over ethernet.					
						art_rxDataStart(devNum);
						sendAck(devNum, "PPM: Ready to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
						art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);


						waitForAck(CalSetup.customerDebug);

						if (REWIND_TEST) {
							return;
						}
						if (!verifyAckStr("PPM: Repeat This Channel - see par2")) {
							uiPrintf("ERROR: miscommunication in sync. expected ack string: PER: Repeat This Channel - see par2\n");
							exit(0);
						}
						repeatThisTest = (ackRecvPar2 > 0) ? TRUE : FALSE ;
						sendAck(devNum, "PPM: Repeat This Channel - saw par2", *(channelTimes10+index), ackRecvPar2, 0, CalSetup.customerDebug);
					}

					num_rx_desc = PER_TEST_FRAME * PER_RATE_COUNT + 40;
					enable_ppm = 0;
				} // end 11g ppm test

				if(kbhit())
					break;
			}

			if (testSENLocal)
			{
	// turn around for rxsen test
				complete_timeout = (mode == MODE_11b) ? 15000 : 10000;
				complete_timeout += num_desc_per_rate*2 ;
				stats_mode = 0;

				if (!testPERLocal)
				{
					//if (!done_reset)
					if(1)
					{
						art_resetDevice(devNum, rxStation, bssID, *(channelTimes10+index), turbo);
						done_reset = TRUE;
					} else
					{
						art_changeChannel(devNum, *(channelTimes10+index));
					}
				}

            if(do_11g_CCK) {
               rate_mask = SEN_RATEMASK_11G_CCK;
            } else {
				rate_mask = SEN_RATEMASK;
            }

				if(mode != MODE_11b) {
					art_writeField(devNum, "mc_trig_level", 0x3f);
				}

				//art_forcePowerTxMax(devNum, goldenPwrList);
				art_forceSinglePowerTxMax(devNum, (A_UINT16)(2*goldenTXPowerLocal));

				uiPrintf("Sending Freq: %5.1lf GHz\n", (double)(*(channelTimes10+index))/10);
				
				repeatThisTest = TRUE;
				while (repeatThisTest) {
					art_txDataSetup(devNum,rate_mask,txStation,num_desc_per_rate,tx_length,pattern,2,retry,antenna, broadcast);
					if (CalSetup.customerDebug > 0) {
						uiPrintf("Sending %d packets for sensitivity test.\n", num_desc_per_rate);
					}
					sendAck(devNum, "RXSEN: Prepare to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
		
					Sleep(30);
					art_txDataBegin(devNum, complete_timeout, stats_mode);

					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					if (!verifyAckStr("SEN: Repeat This Channel - see par2")) {
						uiPrintf("ERROR: miscommunication in sync. expected ack string: SEN: Repeat This Channel - see par2\n");
						exit(0);
					}
					repeatThisTest = (ackRecvPar2 > 0) ? TRUE : FALSE ;
					sendAck(devNum, "SEN: Repeat This Channel - saw par2", *(channelTimes10+index), ackRecvPar2, 0, CalSetup.customerDebug);
				}

				if(mode != MODE_11b) {
					art_writeField(devNum, "mc_trig_level", trig_level);
				}

// TEST rxsen FOR lo_rate
				if (pTestSet[mode]->pTestChannel[index].LO_RATE_SEN)
				{
					complete_timeout = (mode == MODE_11b) ? 15000 : 10000;
					complete_timeout += num_desc_per_rate ;
					stats_mode = 0;

					if (!testPERLocal && !pTestSet[mode]->pTestChannel[index].TEST_PER)
					{
						//if (!done_reset)
						if(1)
						{
							art_resetDevice(devNum, rxStation, bssID, *(channelTimes10+index), turbo);
							done_reset = TRUE;
						} else
						{
							art_changeChannel(devNum, *(channelTimes10+index));
						}
					}

               if(do_11g_CCK) {
                  rate_mask = RATE_1L;
               } else {
					rate_mask = RATE_6;
               }

					if(mode != MODE_11b) {
						art_writeField(devNum, "mc_trig_level", 0x3f);
					}

					//art_forcePowerTxMax(devNum, goldenPwrList);
					art_forceSinglePowerTxMax(devNum, (A_UINT16)(2*goldenTXPowerLocal));

					repeatThisTest = TRUE;
					while (repeatThisTest) {
						art_txDataSetup(devNum,rate_mask,txStation,num_desc_per_rate,tx_length,pattern,2,retry,antenna, broadcast);

						sendAck(devNum, "LO_RATE_RXSEN: Prepare to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
						waitForAck(CalSetup.customerDebug);
						if (REWIND_TEST) {
							return;
						}
			
						uiPrintf("Sending Freq: %5.1lf GHz (LO_RATE)\n", (double)(*(channelTimes10+index))/10);
						Sleep(30);
						art_txDataBegin(devNum, complete_timeout, stats_mode);

						waitForAck(CalSetup.customerDebug);
						if (REWIND_TEST) {
							return;
						}
						if (!verifyAckStr("SEN_LO: Repeat This Channel - see par2")) {
							uiPrintf("ERROR: miscommunication in sync. expected ack string: SEN_LO: Repeat This Channel - see par2\n");
							exit(0);
						}
						repeatThisTest = (ackRecvPar2 > 0) ? TRUE : FALSE ;
						sendAck(devNum, "SEN_LO: Repeat This Channel - saw par2", *(channelTimes10+index), ackRecvPar2, 0, CalSetup.customerDebug);

					} // while repeatThisTest

					if(mode != MODE_11b) {
						art_writeField(devNum, "mc_trig_level", trig_level);
					}
				} // if test_RXSEN

			
			} // if test_RXSEN
		} // ch loop
/*
			configSetup.printPciWrites = 0;
			art_configureLibParams(devNum);
*/
		testingMask = testingMask << 1;
		testTurboLocal = testTurboLocal >> 1;
	} // turbo loop

	
	if(channelTimes10) {
		free(channelTimes10);
	}
	if(art_mdkErrNo!=0)
		uiPrintf("\nReceiving Error, Errno: %d", art_mdkErrNo);
}


void report_timing_summary()
{
	A_UINT32 ii, total_time, end_time, test_time = 0;

	uiPrintf("\n\nTiming Report ...\n\n");

	end_time = milliTime();
	total_time = end_time - globalteststarttime;

	for (ii=0; ii<testnum; ii++)
	{
		if (testtime[ii] < 0) continue; // mechanism built to share testnames in error report summary.
		test_time += testtime[ii];
		uiPrintf("time taken for %s \t = %3.1f\n", testname[ii], testtime[ii]/1000.0);
	}

	uiPrintf("time taken for other things \t = %3.1f\n", (total_time - test_time)/1000.0);
	
	uiPrintf("\nTotal test time \t = %3.1f s  [%3.1f min]\n", total_time/1000.0, total_time/60000.0);
}

A_UINT32 sendSync (A_UINT32 devNum, char *machineName, A_UINT32 debug)
{
#ifndef _IQV
	if (!strcmp(machineName, "0.0.0.0"))
	{
		uiPrintf("Invalid IP address for golden unit. Please specify GOLDEN_IP_ADDR in calsetup.txt\n");
		exit(0);
	}

	if(!activateCommsInitHandshake(machineName)) {
		uiPrintf("Error making socket connect\n");
		exit(0);
	}
	
	if (debug)
		uiPrintf("Established connection to %s\n", machineName);
	selectSecondary();
	if(!art_sendGenericCmd(devNum, "Setting up communication socket", 1, 2, 3)) {
		uiPrintf("TRAPABLE\n");
	}
	if (debug)
		uiPrintf("Sent command, waiting for an ack\n");
	selectPrimary();
#endif // _IQV
	return 0;
}

A_UINT32 waitForSync (A_UINT32 debug)
{
#ifndef _IQV
	A_INT32 var1, var2, var3;
	A_CHAR testBuffer[256];

	if (debug)
		uiPrintf("Waiting for a connection\n");
	if(!waitCommsInitHandshake()) {
		uiPrintf("Error making socket connect\n");
		exit(0);
	}
	if (debug)
		uiPrintf("Received a connection, wait for command\n");
	selectSecondary();
	if(!art_waitForGenericCmd(artSockInfo, (A_UCHAR *)testBuffer, (A_UINT32 *)&var1, (A_UINT32 *)&var2,(A_UINT32 *)&var3)) {
//		uiPrintf("TRAPABLE\n");
	}

	uiPrintf("Established Socket Connection, %d, %d, %d\n", var1, var2, var3);
	selectPrimary();
#endif // _IQV
	return 0;
}

A_UINT32 sendAck (A_UINT32 devNum, char *message, A_INT32 par1, A_INT32 par2, A_INT32 par3, A_UINT32 debug)
{
#ifndef _IQV
	selectSecondary();
	if(!art_sendGenericCmd(devNum, message, par1, par2, par3)) {
		uiPrintf("TRAPPABLE\n");
	}
	if (debug)
		uiPrintf("\n\nSent ack: -->%s<-- , %d, %d, %d\n\n", message, par1, par2, par3);
	strcpy(ackSendStr, message);
	ackSendPar1 = par1;
	ackSendPar2 = par2;
	ackSendPar3 = par3;
	selectPrimary();
#endif // _IQV
	return 0;
}

A_UINT32 waitForAck (A_UINT32 debug)
{
#ifndef _IQV
	selectSecondary();
	if(!art_waitForGenericCmd(artSockInfo, (A_UCHAR *)ackRecvStr, (A_UINT32 *)&ackRecvPar1, (A_UINT32 *)&ackRecvPar2, (A_UINT32 *)&ackRecvPar3)) {
		uiPrintf("\n****************\nSocket Died. Rewinding Test.\n****************\n");
		REWIND_TEST = TRUE;		
	}
	if (debug)
		uiPrintf("\n\nReceived ack: -->%s<-- , %d, %d, %d\n\n", ackRecvStr, ackRecvPar1, ackRecvPar2, ackRecvPar3);

	selectPrimary();
#endif // _IQV
	return 0;
}

A_BOOL verifyAckStr (char *message)
{
#ifndef _IQV
	A_INT32 val;
					
	val = strncmp(message, ackRecvStr, strlen(message));

	return( (val == 0) ? TRUE : FALSE );
#else
	return TRUE;
#endif // _IQV
}

void setup_raw_pcdacs()
{
	A_UINT32 ii;

	RAW_PCDACS = (A_UINT16 *) malloc(sizeOfRawPcdacs*sizeof(A_UINT16));

	for (ii=0; ii<sizeOfRawPcdacs; ii++)
	{
		RAW_PCDACS[ii] = PCDACS_MASTER_LIST[ii];
	}
}

void test_for_temp_margin(A_UINT32 devNum, A_UINT32 mode)
{
	A_UINT32	ii;
	A_UINT32	gainUpLim=27; // for 6mbps  at temp = 25
	A_UINT32	gainLoLim=10; // for 54mbps at temp = 25
	double		tgtPwr_Hi, tgtPwr_Lo, pwr;
	double		attenVal;
	A_BOOL		done_first = FALSE;

	A_UINT16	iter;
	A_BOOL		failedThisTest;
	A_UINT32	mixvga_ovr, bb_val, rf_val;

	if(isGriffin(swDeviceID)) {
		uiPrintf("Temp margin test not needed for ar5005g chipset, not running\n");
		return;
	}

	if (((swDeviceID & 0xFF) == 0x14) || ((swDeviceID & 0xFF) >= 0x16)) {
		gainUpLim = 80;
		mixvga_ovr = art_getFieldForMode(devNum, "rf_mixvga_ovr", devlibModeFor[mode], 0);
		art_writeField(devNum, "rf_mixvga_ovr", 0);
	}

	if (!NEED_GAIN_OPT_FOR_MODE[mode])
	{
		uiPrintf("Temp Margin Test Not Needed For This Mode : %s\n\n", modeName[mode]);
		return;
	}

	attenVal = (mode == MODE_11a) ? CalSetup.attenDutPM : CalSetup.attenDutPM_2p4[mode];

	gainUpLim += (A_UINT32)((CalSetup.caseTemperature - 25)/3);
	gainLoLim += (A_UINT32)((CalSetup.caseTemperature - 25)/3);

	initializeGainLadder(pCurrGainLadder);
	programNewGain(pCurrGainLadder, devNum, 0);

	uiPrintf("\nCase temp during cal: %d C. \n", CalSetup.caseTemperature);
	uiPrintf("Temp Margin Test GainF Triggers :  %d (Hi)       %d (Lo)\n", gainUpLim, gainLoLim);

	uiPrintf("\nChannel     hi T (Meas > Tgt - .5)      lo T (Meas < Tgt + .5)   \n");

	for (ii=0; ii<pTestSet[mode]->numTestChannelsDefined; ii++)
	{
		if (!(pTestSet[mode]->pTestChannel[ii].TEST_TEMP_MARGIN) )
		{
			continue;
		}

		if (ii>0)
		{
			art_changeChannel(devNum, pTestSet[mode]->pTestChannel[ii].channelValueTimes10);
		} else {
			art_resetDevice(devNum, rxStation, bssID, pTestSet[mode]->pTestChannel[0].channelValueTimes10, 0);
		}
		setGainLadderForMaxGain(pCurrGainLadder);
		programNewGain(pCurrGainLadder, devNum, 0);
		art_writeField(devNum, "rf_ovr", 1); // transmit in the override mode
		if ((gainUpLim > 35) && (((swDeviceID & 0xFF) == 0x14) || ((swDeviceID & 0xFF) >= 0x16))) {
			rf_val = (A_UINT32) ((gainUpLim - 23)/12);
			bb_val = gainUpLim - 12*rf_val;
			art_writeField(devNum, "rf_gain_I", bb_val);
			art_writeField(devNum, "rf_rfgain_I", rf_val);
		} else {
			art_writeField(devNum, "rf_gain_I", gainUpLim);
		}

		art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
						rates[0], DESC_ANT_A | USE_DESC_ANT);
		if (mode == MODE_11b)
		{
			Sleep(150); // sleep 350 ms
		} else
		{
			(CalSetup.pmModel == PM_436A) ? Sleep(150) : Sleep(50);  //sleep 250 ms
		}

		tgtPwr_Hi = getTargetPower(mode, pTestSet[mode]->pTestChannel[ii].channelValueTimes10/10, 6);

		iter = 0;
		failedThisTest = TRUE;
		while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
			iter++;		
			if (CalSetup.customerDebug) uiPrintf("SNOOP: temp_mrgn_hi repeat iter = %d \n", iter);
			pwr = pmMeasAvgPower(devPM, 0) + attenVal;
			failedThisTest = FALSE;
			if (pwr < (tgtPwr_Hi - 0.5)) {
				failedThisTest = TRUE;
				Sleep(50);
			}
		} //while failedThisTest

		if ((CalSetup.pmModel == PM_436A) && (!done_first) && (pwr < (tgtPwr_Hi - 2.0)))
		{
			art_txContEnd(devNum);
			if (CalSetup.customerDebug)
				uiPrintf("retried: %3.1f/%3.1f\n", pwr, tgtPwr_Hi);
			art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
							rates[0], DESC_ANT_A | USE_DESC_ANT);
			if (mode == MODE_11b)
			{
				Sleep(150); // sleep 350 ms
			} else
			{
				(CalSetup.pmModel == PM_436A) ? Sleep(150) : Sleep(50);  //sleep 250 ms
			}
			pwr = pmMeasAvgPower(devPM, 0) + attenVal;
			done_first = TRUE;
		}
		
		uiPrintf("  %5.1lf          %4.1f > %3.1f - .5", (double)(pTestSet[mode]->pTestChannel[ii].channelValueTimes10)/10, pwr, tgtPwr_Hi);
		uiPrintf("%s", (pwr >= (tgtPwr_Hi - 0.5)) ? "        " : "[X]     ");

		art_txContEnd(devNum);
		
		setGainLadderForMinGain(pCurrGainLadder);
		programNewGain(pCurrGainLadder, devNum, 0);
		art_writeField(devNum, "rf_ovr", 1); // transmit in the override mode
		art_writeField(devNum, "rf_gain_I", gainLoLim);
		art_writeField(devNum, "rf_ovr", 1); // transmit in the override mode
		if  (((swDeviceID & 0xFF) == 0x14) || ((swDeviceID & 0xFF) >= 0x16)) {
			art_writeField(devNum, "rf_rfgain_I", 0);
		}

		art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
						rates[0], DESC_ANT_A | USE_DESC_ANT);
		if (mode == MODE_11b)
		{
			Sleep(150); // sleep 350 ms
		} else
		{
			(CalSetup.pmModel == PM_436A) ? Sleep(150) : Sleep(50);  //sleep 250 ms
		}
		
		tgtPwr_Lo = getTargetPower(mode, pTestSet[mode]->pTestChannel[ii].channelValueTimes10/10, 54);
		iter = 0;
		failedThisTest = TRUE;
		while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
			iter++;		
			if (CalSetup.customerDebug) uiPrintf("SNOOP: tmp_mrgn_lo repeat iter = %d \n", iter);
			pwr = pmMeasAvgPower(devPM, 0) + attenVal;
			failedThisTest = FALSE;
			if (pwr > (tgtPwr_Lo + 0.5)) {
				failedThisTest = TRUE;
				Sleep(50);
			}
		} //while failedThisTest

		uiPrintf("    %4.1f < %3.1f + .5", pwr, tgtPwr_Lo);
		uiPrintf("%s\n", (pwr <= (tgtPwr_Lo + 0.5)) ? "" : "[X]");

		art_txContEnd(devNum);				
	}

	if (((swDeviceID & 0xFF) == 0x14) || ((swDeviceID & 0xFF) >= 0x16)) {
		art_writeField(devNum, "rf_mixvga_ovr", mixvga_ovr);
	}

	initializeGainLadder(pCurrGainLadder);
	programNewGain(pCurrGainLadder, devNum, 0);
	art_writeField(devNum, "rf_ovr", 0); // transmit in the override mode
} // test_for_temp_margin

void free_all_eeprom_structs()
{
	A_UINT32 i;
	d_freeEepromStruct(pRawDataset);
	d_freeEepromStruct(pRawDataset_2p4[MODE_11b]);
	d_freeEepromStruct(pRawDataset_2p4[MODE_11g]);

	d_freeEepromStruct(pRawGainDataset);
	d_freeEepromStruct(pRawGainDataset_2p4[MODE_11b]);
	d_freeEepromStruct(pRawGainDataset_2p4[MODE_11g]);

	d_freeEepromStruct(pCalDataset);
	d_freeEepromStruct(pCalDataset_2p4[MODE_11b]);
	d_freeEepromStruct(pCalDataset_2p4[MODE_11g]);

	d_freeEepromStruct(pFullDataset);

	if(!eepromFree)
	{	
#ifndef _IQV
		for( i =0; i < NUMEEPBLK; i++){
			if(EEPROM_DATA_BUFFER[i])
				free(EEPROM_DATA_BUFFER[i]);
			}

		if(EEPROM_DATA_BUFFER)
			free(EEPROM_DATA_BUFFER);
#else
		if (EEPROM_DATA_BUFFER) {
			for( i =0; i < NUMEEPBLK; i++) {
				if(EEPROM_DATA_BUFFER[i])
					free(EEPROM_DATA_BUFFER[i]);
			}
			free(EEPROM_DATA_BUFFER);
		}
#endif
		eepromFree = TRUE;
	}
}


A_BOOL prepare_for_next_card (A_UINT32 *p_devNum)
{
	A_UINT32 devNumArr[3];
	// A_UINT32 devNum = *p_devNum;
	A_BOOL		exitLoop = FALSE;
	//A_UCHAR     resp[128];
	//A_UINT16    ii;
	A_UINT32 devNumInst[3],i,k;  //start from 1, ignore 0
	SUB_DEV_INFO devStruct;

	/*
	if(isDragon_sd(swDeviceID)) {
		uiPrintf("\n==============================================");
		uiPrintf("\nTest complete. To retest (Device Under Test)");
		uiPrintf("\nPress any key to start or <ESC> to quit");
		uiPrintf("\n==============================================\n");
		while (!kbhit())
			;
		if(getch() == 0x1b)
		{
			exitLoop = TRUE;
 		} 		
		return(exitLoop);	
	}
	*/
			    
	devNumInst[1] = INVALID_INSTANCE;
	devNumInst[2] = INVALID_INSTANCE;
	devNumArr[MODE_11a] = p_devNum[MODE_11a];
	devNumArr[MODE_11b] = p_devNum[MODE_11b];
	devNumArr[MODE_11g] = p_devNum[MODE_11g];
	

	if (CalSetup.atherosLoggingScheme) {
		closeAtherosCalLogging();
	}

	while(kbhit())		// Flush out previous key press.
	{	getch(); }
  
	if(!sdio_client)
	{
		art_teardownDevice(devNumArr[MODE_11a]);
		if(devNumArr[MODE_11a] != devNumArr[MODE_11g]) {
			art_teardownDevice(devNumArr[MODE_11g]);
		}
	}	
	
	free_all_eeprom_structs();

	
		uiPrintf("\n==============================================");
		uiPrintf("\nTest complete. Press any key to replace ");
		uiPrintf("\nDUT (Device Under Test) or <ESC> to quit");
		uiPrintf("\n==============================================\n");
        if (1 == StressTest) {
            exitLoop = TRUE;
            return(exitLoop);
        }
        else {
   		while (!kbhit());

		if(getch() == 0x1b)
		{			
			exitLoop = TRUE;
			return(exitLoop);
		} 
	}	
				
		if(sdio_client)
		{
			art_teardownDevice(devNumArr[MODE_11a]);
		if(devNumArr[MODE_11a] != devNumArr[MODE_11g]) {
			art_teardownDevice(devNumArr[MODE_11g]);
		}
			closeTarget();
		}
		
		uiPrintf("\n==============================================");
		uiPrintf("\nReplace DUT, then press any key to continue ");
		uiPrintf("\n==============================================\n");
		
		getch();
		
					
		if(sdio_client)
		{
			//add FLASE as an argumnet of closeEnvironment, no to unload...just this time.
			closeEnvironment();
			if(!initializeEnvironment(configSetup.remote)){
				uiPrintf("Failed to initialize the driver environment\n");
				return FALSE;
			} 
		}
		
		if(sdio_client)
		{
			if (!loadTarget())
				return FALSE;
			Sleep(SDIO_WAIT_INTERVAL_MS);
			if (!initTarget())
				return FALSE;
		}
               
		if(configSetup.enableLabelScheme) {
		    while (!promptForLabel(0));
		    if((strncmp(strupr(yldStruct.cardLabel), "AP", 2)==0) ||
		       (strncmp(strupr(yldStruct.cardLabel), "UB", 2)==0) ||
                       (strncmp(strupr(yldStruct.cardLabel), "SD", 2)==0) ||
                       (strncmp(strupr(yldStruct.cardLabel), "CU", 2)==0) ||
                       (strncmp(strupr(yldStruct.cardLabel), "MD", 2)==0) ) {
                        configSetup.remote = 1;
                        configSetup.primaryAP = 1;
                    }
                    else {
                        configSetup.remote = 0;
                        configSetup.primaryAP = 0;
                        uiPrintf("we do not support any other label except AP,UB,SD,CU,MD...\n"); //verify?
                        return FALSE;
                    }
		} //label scheme
	
	do_11g_CCK = FALSE;
	
	/*if ((exitLoop != TRUE) && (configSetup.remote) && (!sdio_client)) 
	{
		//uiPrintf("IP Address of the new AP: ");
		uiPrintf("Use the IP Address %s ? [y/ip_addr]  :  ", configSetup.machName);
 		//scanf("%s",configSetup.machName);
		scanf("%s", resp);
		if (strnicmp((const char *)resp, "USB", 3) == 0)
		{	
			usb_client = TRUE;
		}
		else if  ((strnicmp((const char *)resp, "SDIO", 4) == 0)   || 
					(  (strnicmp((const char *)resp, "y", 1) == 0) && 
						(strnicmp((const char *)configSetup.machName, "SDIO", 4) == 0)
					)
			) {
			if (strnicmp((const char *)resp, "y", 1) != 0)
				strcpy(configSetup.machName, (const char *)resp);
			sdio_client = TRUE;
		}
		else {
		  sdio_client = FALSE;  
		if (isdigit(resp[0])) {
			ii = 0;
			while (resp[ii] != '\0') {
				configSetup.machName[ii] = resp[ii];
				ii++;
			}
			configSetup.machName[ii] = '\0';

			//strcpy(configSetup.machName, resp);
			uiPrintf("Using New IP Address : %s\n", configSetup.machName);
		} else {
			uiPrintf("Using Same IP Address : %s\n", configSetup.machName);
		}
	}  
	    uiPrintf("Using Address : %s\n", configSetup.machName);
	}*/
        
	//Sleep(1000);
	
	devNumInst[0] =  art_setupDevice(configSetup.instance);
              if(devNumInst[0] < 0) {
                  uiPrintf("main: Error attaching to the device - ending test\n");
                  closeEnvironment();
                  return FALSE;
		}
		
	
	globalteststarttime = milliTime();
	testnum = 0;
	//if using the label scheme, could be any card, parse eep file again
	if(configSetup.enableLabelScheme) {
		//initialize some of the CalSetup variables that may not get 
		//reinitialized		
		CalSetup.modeMaskForRadio[0] = 0x3;
		CalSetup.modeMaskForRadio[1] = 0;
		CalSetup.instanceForMode[0] = 1;
		CalSetup.instanceForMode[1] = 1;
		CalSetup.instanceForMode[2] = 1;
		CalSetup.eepromLength =0;
		CalSetup.EARStartAddr =0x0;
		CalSetup.EARLen=0x0;
		//CalSetup.UartPciCfgLen=0x0;
		//findEepFile();
		findEepFileNew(devNumInst[0]);
	}
	
	Sleep(1000); //lengthen delay if problems with seg. fault

	if(!isDragon_sd(swDeviceID))
	{
		printf("modeMaskforRadio[0] = %d\n", CalSetup.modeMaskForRadio[0]);
		if(CalSetup.modeMaskForRadio[0] != 0) {	
			devNumInst[1] = art_setupDevice(1);
			if(devNumInst[1] < 0) {
				uiPrintf("main: Error attaching to the device - ending test\n");
        			closeEnvironment();
        			return FALSE;
    			}
			art_configureLibParams(devNumInst[1]);
			if (!art_resetDevice(devNumInst[1], txStation, bssID, configSetup.channel, configSetup.turbo))
				return FALSE;
			art_getDeviceInfo(devNumInst[1], &devStruct);
			macRev = devStruct.macRev;
			uiPrintf("Attached to the Device for instance = %d\n", 1);
		}

		if(CalSetup.modeMaskForRadio[1] != 0) {
			devNumInst[2] = art_setupDevice(2);
			if(devNumInst[2] < 0) {
				uiPrintf("main: Error attaching to the device - ending test\n");
				closeEnvironment();
				return FALSE;
			}
			art_configureLibParams(devNumInst[2]);
			if (!art_resetDevice(devNumInst[2], txStation, bssID, configSetup.channel, configSetup.turbo))
				return FALSE;
			art_getDeviceInfo(devNumInst[2], &devStruct);
			macRev = devStruct.macRev;
			uiPrintf("Attached to the Device for instance = %d\n", 2);
		}
	} // !isDragon


	if(isDragon_sd(swDeviceID) && sdio_client)
	{	
		devNumArr[MODE_11a] = devNumArr[MODE_11b] = devNumArr[MODE_11g] = 0;
	}
	else
	{
	//Setup the 2g and 5g instances
	devNumArr[MODE_11a] = devNumInst[CalSetup.instanceForMode[MODE_11a]];
	devNumArr[MODE_11b] = devNumInst[CalSetup.instanceForMode[MODE_11g]];
	devNumArr[MODE_11g] = devNumArr[MODE_11b];
	}

	//check that we have legal devNums for both.  If not, point to a valid
	//devNum, manufacturing expects 2 devNums to be valid
	//if all board text files are setup correctly, should not use it.
	if((devNumArr[MODE_11a] == INVALID_INSTANCE) && (devNumArr[MODE_11g] == INVALID_INSTANCE)) {
		uiPrintf("Unable to allocate a valid devNum, exiting\n");
		closeEnvironment();
		return FALSE;			
	}
	else if (devNumArr[MODE_11a] == INVALID_INSTANCE) {
		devNumArr[MODE_11a] = devNumArr[MODE_11g];
	}
	else if (devNumArr[MODE_11g] == INVALID_INSTANCE) {
		devNumArr[MODE_11g] = devNumArr[MODE_11a];
	}

	if(CalSetup.instanceForMode[MODE_11g] != CalSetup.instanceForMode[MODE_11a]) {
		configSetup.eepromLoad = 0;
		art_setResetParams(devNumArr[MODE_11b], configSetup.pCfgFile,(A_BOOL)configSetup.eepromLoad, 
							(A_BOOL)configSetup.eepromHeaderLoad, MODE_11B, configSetup.use_init); 	
	}   
	configSetup.eepromLoad = 0;
	art_setResetParams(devNumArr[MODE_11a], configSetup.pCfgFile,(A_BOOL)configSetup.eepromLoad, 
						(A_BOOL)configSetup.eepromHeaderLoad, MODE_11A, configSetup.use_init); 	


        //To avoid cal timeout in resetDevice, moved this section here (from a few lines below) ...
	// force loadEAR to 0 to capture values from new config file
	// loadEAR is restored just before the dutTest
	configSetup.loadEar = 0;
	art_configureLibParams(devNumArr[MODE_11a]);
	if (!art_resetDevice(devNumArr[MODE_11a], rxStation, bssID, 5260, 0))
		return FALSE;
	processEepFile(devNumArr[MODE_11a], configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion);

	if(!setEepFile(devNumArr[MODE_11a])) {
		uiPrintf("failed to setup EEP file\n");
		closeEnvironment();
		return FALSE;
	}

	//parse the target power files if needed.
	if(configSetup.enableLabelScheme) {
		if(!parseTargets()) {
			uiPrintf("An error occured while parsing the file %s. Pl. check for format errors.\n", CalSetup.tgtPwrFilename);
		}
	}

	// force loadEAR to 0 to capture values from new config file
	// loadEAR is restored just before the dutTest
	//configSetup.loadEar = 0;
	//art_configureLibParams(devNumArr[MODE_11a]);
	//art_resetDevice(devNumArr[MODE_11a], rxStation, bssID, 5260, 0);	
	//processEepFile(devNumArr[MODE_11a], configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion);

	art_configureLibParams(devNumArr[MODE_11g]);
	if (!art_resetDevice(devNumArr[MODE_11g], rxStation, bssID, 2412, 0))
		return FALSE;
	processEepFile(devNumArr[MODE_11g], configSetup.cfgTable.pCurrentElement->eepFilename, &configSetup.eepFileVersion);
    
//	art_resetDevice(devNumArr[MODE_11a], rxStation, bssID, 5220, 0);	
//    art_resetDevice(devNumArr[MODE_11b], rxStation, bssID, 2412, 0);	
    /*art_changeField(devNumArr[MODE_11a], "mc_eeprom_size_ovr", 3);
    art_changeField(devNumArr[MODE_11b], "mc_eeprom_size_ovr", 3); */

	p_devNum[MODE_11a] = devNumArr[MODE_11a];
	p_devNum[MODE_11b] = devNumArr[MODE_11b];
	p_devNum[MODE_11g] = devNumArr[MODE_11g];

	if(!art_testLib(devNumArr[MODE_11a], 1000)) {
		uiPrintf("PCI test fail.\n");
		exitLoop = TRUE;
	}

	if ((CalSetup.eeprom_map == CAL_FORMAT_GEN3) ||
		(CalSetup.eeprom_map == CAL_FORMAT_GEN5) ){
		CalSetup.TrgtPwrStartAddr = 0x150;
		if(CalSetup.eeprom_map == CAL_FORMAT_GEN5) {
			//add extra dummy bytes to calculation of ear start
			CalSetup.TrgtPwrStartAddr += NUM_DUMMY_EEP_MAP1_LOCATIONS; 
			CalSetup.calStartAddr = CalSetup.TrgtPwrStartAddr;

		}
	} else {
		CalSetup.TrgtPwrStartAddr = 0x1A5;
	}

// must've obtained card label etc.. by now
	if (CalSetup.atherosLoggingScheme) {
		setupAtherosCalLogging();
	}

	
	//more init needed by label scheme
	if(configSetup.enableLabelScheme) {
		if ((CalSetup.eeprom_map == CAL_FORMAT_GEN3)||
    		(CalSetup.eeprom_map == CAL_FORMAT_GEN5) ) {
			if (CalSetup.cal_mult_xpd_gain_mask[MODE_11a] == 0) {
				CalSetup.xgain = 1 << CalSetup.xgain ;
			} else {
				CalSetup.xgain = CalSetup.cal_mult_xpd_gain_mask[MODE_11a];
			}
			if (CalSetup.cal_mult_xpd_gain_mask[MODE_11b] == 0) {
				CalSetup.xgain_2p4[MODE_11b] = 1 << CalSetup.xgain_2p4[MODE_11b] ;
			} else {
				CalSetup.xgain_2p4[MODE_11b] = CalSetup.cal_mult_xpd_gain_mask[MODE_11b];			
			}
			if (CalSetup.cal_mult_xpd_gain_mask[MODE_11g] == 0) {
				CalSetup.xgain_2p4[MODE_11g] = 1 << CalSetup.xgain_2p4[MODE_11g] ;
			} else {
				CalSetup.xgain_2p4[MODE_11g] = CalSetup.cal_mult_xpd_gain_mask[MODE_11g];
			}
		}
	}
	
	EEPROM_DATA_BUFFER =(A_UINT32 **) malloc(sizeof(A_UINT32 *) * NUMEEPBLK);
	
	if(EEPROM_DATA_BUFFER != NULL){
		for (i = 0; i < 2; i++) {
			EEPROM_DATA_BUFFER[i]= (A_UINT32 *)malloc(sizeof(A_UINT32) * eepromSize);
				if(EEPROM_DATA_BUFFER[i] == NULL){
						uiPrintf(" Memory Not allocated in prepare for next card() eep size =%x\n",eepromSize);
						return FALSE;
				}
			}
		}
	else {
		uiPrintf(" Memory Not allocated in prepare for next card() \n");
		return FALSE;
	}

	for(i=0;i<NUMEEPBLK;i++)	{
		for(k =0;k< eepromSize;k++)		{
				EEPROM_DATA_BUFFER[i][k]=0xffff;			
		}
	}
	return(exitLoop);
}


void copy_11g_cal_to_11b(dPCDACS_EEPROM *pSrcStruct, dPCDACS_EEPROM *pDestStruct)
{
	dDATA_PER_CHANNEL	*pChannelData;
	A_UINT16			*pPcdacValue;
	double				*pPwrValue;
	A_UINT16			i, j;

	pDestStruct->hadIdenticalPcdacs = pSrcStruct->hadIdenticalPcdacs;
	pDestStruct->numChannels = pSrcStruct->numChannels;
	
	pChannelData = pDestStruct->pDataPerChannel; 

	for(i = 0; i < pSrcStruct->numChannels; i++ ) {
		pDestStruct->pChannels[i] = pSrcStruct->pChannels[i];
		pChannelData->numPcdacValues = pSrcStruct->pDataPerChannel[i].numPcdacValues;
		pChannelData->channelValue = pSrcStruct->pDataPerChannel[i].channelValue;
		pChannelData->pcdacMax = pSrcStruct->pDataPerChannel[i].pcdacMax;
		pChannelData->pcdacMin = pSrcStruct->pDataPerChannel[i].pcdacMin;

		pPcdacValue = pChannelData->pPcdacValues;
		pPwrValue = pChannelData->pPwrValues;
		for(j = 0; j < pChannelData->numPcdacValues; j++ ) {			
			*pPcdacValue = pSrcStruct->pDataPerChannel[i].pPcdacValues[j];
			*pPwrValue = pSrcStruct->pDataPerChannel[i].pPwrValues[j];
			pPcdacValue++;
			pPwrValue++;
	    }
		pChannelData++;			
	}
}


void report_fail_summary()
{
	A_UINT32 ii;

	for (ii = 0; ii<30; ii++) {
		if (failTest[ii] > 0) {
			uiPrintf("*****      %-24s    *****\n", testname[ii]);
		}
	}
}
		
A_UINT32 optimal_fixed_gain(A_UINT32 devNum, GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 mode) 
{
	A_UINT32	channels[5];
	A_UINT32	numCh;
	A_UINT32	ii;
	double		minPwr[5], maxPwr[5];
	double		minTarg[5], maxTarg[5];
	double		attenVal;
	A_UINT32	retIndex = ladder->defaultStepNum;

	if(!(CalSetup.useInstruments)) {		
		return(retIndex);
	}
	
	if (mode == MODE_11a) {
		numCh = 3;
		channels[0]=4920; channels[1]=5180; channels[2]=5500; 
		attenVal = CalSetup.attenDutPM;
	} else {
		numCh = 1;
		channels[0]=2412; //channels[1]=2484; 
		attenVal = CalSetup.attenDutPM_2p4[mode];
	}

	for(ii=0; ii<numCh; ii++) {
		minTarg[ii] = getTargetPower(mode, channels[ii], 54);
		maxTarg[ii] = getTargetPower(mode, channels[ii], 6);

		if (!art_resetDevice(devNum, rxStation, bssID, channels[ii], 0))
			return FALSE;
		initializeGainLadder(ladder);
		programNewGain(ladder, devNum, 0);
		art_txContBegin(devNum, CONT_FRAMED_DATA, RANDOM_PATTERN, 
						rates[0], DESC_ANT_A | USE_DESC_ANT);
		art_ForceSinglePCDACTable(devNum, 1);
		if (mode == MODE_11b)
		{
			Sleep(150); // sleep 350 ms
		} else
		{
			(CalSetup.pmModel == PM_436A) ? Sleep(150) : Sleep(50);  //sleep 250 ms
		}
		minPwr[ii] = pmMeasAvgPower(devPM, 0) + attenVal;
		if ((swDeviceID & 0xFF) >= 0x16) { // derby 2
			art_ForceSinglePCDACTable(devNum, (0x40 | 63));
		} else {
			art_ForceSinglePCDACTable(devNum, 63);
		}
		if (mode == MODE_11b)
		{
			Sleep(150); // sleep 350 ms
		} else
		{
			(CalSetup.pmModel == PM_436A) ? Sleep(150) : Sleep(50);  //sleep 250 ms
		}
		maxPwr[ii] = pmMeasAvgPower(devPM, 0) + attenVal;
		art_txContEnd(devNum);
		// uiPrintf("SNOOP: ch:%d, minTarg=%2.1f, maxTarg=%2.1f, minPwr=%2.1f, maxPwr=%2.1f\n", channels[ii],
		//	      minTarg[ii], maxTarg[ii], minPwr[ii], maxPwr[ii]);
	}

	for(ii=0; ii<numCh; ii++) {
		if (maxPwr[ii] < (maxTarg[ii] - 0.5)) {
			while ( (retIndex > 0) && 
					((maxPwr[ii] + ladder->optStep[retIndex].stepGain) < (maxTarg[ii] - 0.5))) {
				retIndex--;
			}
		}
	}

	for(ii=0; ii<numCh; ii++) {
		if ((minPwr[ii] + ladder->optStep[retIndex].stepGain) > (minTarg[ii] + 0.5)) {
			while ( (retIndex < ladder->numStepsInLadder) && 
					((minPwr[ii] + ladder->optStep[retIndex].stepGain) > (minTarg[ii] + 0.5))) {
				retIndex++;
			}
		}
	}

//	uiPrintf("SNOOP: returning index %d for mode %s\n", retIndex, modeName[mode]);
	uiPrintf(" (%s) ", ladder->optStep[retIndex].stepName);
	return(retIndex);
}
		
A_BOOL test_sleep_crystal(A_UINT32 devNum) 
{
    A_UINT32	lowTsf, lowTsf2;
    A_UINT32	time1, time2;
	A_BOOL		retVal = TRUE;
	A_UINT32    tmpVal;

    /* Test 32kHz presence */
    if (CalSetup.Enable_32khz) {

        art_regWrite(devNum, F2_BEACON, art_regRead(devNum, F2_BEACON) |
                         F2_BEACON_RESET_TSF); // Start TSF by writing x8020

        Sleep(1); // delay 1 ms

        /* Set USEC32 to 1 */
		//A_REG_RMW_FIELD(pDev, MAC_USEC, 32, 1);	
		if (((swDeviceID & 0xFF) == 0x14) || ((swDeviceID & 0xFF) >= 0x16)){
			art_writeField(devNum, "mc_usec_32", 1); // derby
		} else {
			art_writeField(devNum, "mc_usec_32", 1); // sombrero
		}
        
        /* Set TSF Increment for 32 KHz */
        //A_REG_WR(pDev, MAC_TSF_PARM, 61);
		art_writeField(devNum, "mc_tsf_increment", 61);

        /* Set sleep clock rate to 32 KHz. */
        //A_REG_RMW_FIELD(pDev, MAC_PCICFG, SLEEP_CLK_SEL, 1);
		tmpVal = art_regRead(devNum, F2_PCICFG);
		art_regWrite(devNum, F2_PCICFG, (tmpVal & (~F2_PCICFG_SLEEP_CLK_SEL) | 0x2));
		//art_writeField(devNum, "mc_sleep_clk_sel", 1);

        //A_REG_RMW_FIELD(pDev, MAC_PCICFG, SLEEP_CLK_RATE_IND, 0x3);
		tmpVal = art_regRead(devNum, F2_PCICFG);
		art_regWrite(devNum, F2_PCICFG, (tmpVal & (~F2_PCICFG_SLEEP_CLK_RATE_INDICATION) | (0x3 << 24)));
		//art_writeField(devNum, "mc_sleep_clk_rate_indication", 0x3);

        Sleep(1); // delay 1 ms

		time1 = milliTime();
        lowTsf = art_regRead(devNum, F2_TSF_L32);
        Sleep(100); // delay 100 ms
        lowTsf2 = art_regRead(devNum, F2_TSF_L32);
		time2 = milliTime();

		
        /* TSF should increment by 10000, provide 500 pad on either side for bad
test harness timing */
        if (((lowTsf2-lowTsf) < ( 1000*(time2-time1) - 5000)) || ((lowTsf2-lowTsf) > (1000*(time2-time1) + 5000))) {
//            uiPrintf("ERROR: TSF did not increment off 32kHz clock\n");
//            uiPrintf("TSF Info: lowTsf %d, lowTsf2 %d, diff %d [time=%d usec]\n", lowTsf, lowTsf2, 
//																   lowTsf2 - lowTsf, 1000*(time2 - time1));

			retVal = FALSE;
		}

		if (CalSetup.customerDebug) {
			uiPrintf("TSF Info: lowTsf %d, lowTsf2 %d, diff %d [time=%d usec]\n", lowTsf, lowTsf2, 
																   lowTsf2 - lowTsf, 1000*(time2 - time1));
		}

		logMeasToYield(0.0, 0.0, 0.0, 100*1000.0, (double)(lowTsf2-lowTsf), 1000.0*(time2-time1), (A_CHAR *)( (retVal) ? "PASS" : "FAIL"));

        /* Restore operation to ref Clock */
        //A_REG_RMW_FIELD(pDev, MAC_PCICFG, SLEEP_CLK_RATE_IND, 0);
		//art_writeField(devNum, "mc_sleep_clk_rate_indication", 0);
		tmpVal = art_regRead(devNum, F2_PCICFG);
		art_regWrite(devNum, F2_PCICFG, (tmpVal & (~F2_PCICFG_SLEEP_CLK_RATE_INDICATION) | (0x0 << 24)));
		

        //A_REG_RMW_FIELD(pDev, MAC_PCICFG, SLEEP_CLK_SEL, 0);
		tmpVal = art_regRead(devNum, F2_PCICFG);
		art_regWrite(devNum, F2_PCICFG, (tmpVal & (~F2_PCICFG_SLEEP_CLK_SEL) | 0x0));
		//art_writeField(devNum, "mc_sleep_clk_sel", 0);

        /* Set TSF Increment for 32 MHz */
        //A_REG_WR(pDev, MAC_TSF_PARM, 1);
		art_writeField(devNum, "mc_tsf_increment", 1);

         /* Set USEC32 to 1 */
        //A_REG_RMW_FIELD(pDev, MAC_USEC, 32, 39);
		// 31 for sombrero
		if (((swDeviceID & 0xFF) == 0x14) || ((swDeviceID & 0xFF) >= 0x16)){
			art_writeField(devNum, "mc_usec_32", 39); // derby
		} else {
			art_writeField(devNum, "mc_usec_32", 31); // sombrero
		}
    }

	return(retVal);
	
}

void dutVerifyDataTXRX(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT32 frameLen)
{
	A_UINT32 *channelTimes10;
	A_UINT16 testChannel, index, ii;
    RX_STATS_STRUCT rStats;

	// txDataSetup
	A_UINT32 rate_mask ;

	A_UINT32 num_tx_desc = NUM_DATA_CHECK_FRAME;
	A_UINT32 tx_length = frameLen;
	A_UINT32 retry = 0;		// broadcast mode, disable retry
	A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 0;

	// rxDataSetup
	A_UINT32 num_rx_desc = 50;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode =  ENABLE_STATS_RECEIVE; // ENABLE_STATS_SEND |
	A_UINT32 compare = 1;


	A_UINT32 trig_level, ChannelCount;
	A_BOOL	 done_reset = FALSE;
	
	A_UINT32 kk;
	A_UINT32 jumbo_frame_mode_save;
	
	double	 goldenTXPowerLocal, attenDutGoldenLocal ;

	A_UINT16 iter, channel_repeat;
	A_BOOL   failedThisTest, restore_local_lib_print;
	A_BOOL	 testDataTXLocal = TRUE;
	A_BOOL	 testDataRXLocal = FALSE;

	A_INT32  rxSignalLevel = -55;

	for (kk=0; kk<NUM_RATES; kk++) {
		if (rate == DataRate[kk]) {
			rate_mask = (1 << kk);
			break;
		}
	}
	rate_mask |= RATE_GROUP;

	if(!isGriffin(swDeviceID) && !isEagle(swDeviceID)) {
		art_writeField(devNum, "bb_max_rx_length", 0xFFF);
		art_writeField(devNum, "bb_en_err_length_illegal", 0);
	}
	jumbo_frame_mode_save = art_getFieldForMode(devNum, "mc_jumbo_frame_mode", mode, turbo);
	art_writeField(devNum, "mc_jumbo_frame_mode", 0);

	if (mode == MODE_11a)
	{
		attenDutGoldenLocal = CalSetup.attenDutGolden;
		goldenTXPowerLocal  = CalSetup.goldenTXPower;
		testChannel         = 5260;
	} else
	{
		attenDutGoldenLocal = CalSetup.attenDutGolden_2p4[mode];
		goldenTXPowerLocal = CalSetup.goldenTXPower_2p4[mode];
		testChannel         = 2462;		
	}

    // setup attenuator here
    if(CalSetup.useInstruments) {		
    	attSet(devATT, (A_INT32)(goldenTXPowerLocal - attenDutGoldenLocal - rxSignalLevel ));
    }

	uiPrintf("\n\nStart Data Integrity Test in %s mode\n\n", modeName[mode]);
	ChannelCount = 1;
	channelTimes10 = (A_UINT32 *)malloc(ChannelCount*sizeof(A_UINT32)) ;
	if(NULL == channelTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in Verify Data test\n");
		return;
	}
	for (ii=0; ii<ChannelCount; ii++)
	{
		channelTimes10[ii] = testChannel;
	}

	if(mode != MODE_11b) {
		trig_level = art_getFieldForMode(devNum, "mc_trig_level", 
			((mode == MODE_11g) ? MODE_11G : MODE_11A), 0);
	}

	uiPrintf("Pattern :   0x00   0xFF   0xAA   0x55   0x66   0x99  walk1  walk0\n");

	for(index=0; index<ChannelCount; index++) {

		//cleanup descriptor queues, to free up mem
		art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);

		if(testDataTXLocal) 
		{

			// stats receive params
			// need to reset the params changed by RX test for TX for each iteration
		    if (isDragon_sd(swDeviceID))  
                        num_rx_desc = 30; 
                    else
                        num_rx_desc = 50;

			rx_length = 100;
			start_timeout = 0;	// no wait time for transmit
			complete_timeout = ((mode == MODE_11b) ? 50000 : 5000);
			stats_mode =  ENABLE_STATS_RECEIVE; // ENABLE_STATS_SEND |
	
			if(mode != MODE_11b) {
					if(isDragon_sd(swDeviceID)) {
					    art_writeField(devNum, "mc_trig_level", 0x1);
					} else {
						art_writeField(devNum, "mc_trig_level", 0x3f);
					}
			} 

			if(1)
			//if (!done_reset)
			{
				if (!art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo))
					return;
				done_reset = TRUE;
				if ( configSetup.force_antenna && isDragon_sd(swDeviceID))
				{
					updateAntenna(devNum);
				}
			} else
			{
				art_changeChannel(devNum, *(channelTimes10+index));
			}

			uiPrintf("\n%5.1lf TX :   ", (double)(*(channelTimes10+index))/10);

			for (kk = 0; kk < NUM_VERIFY_DATA_PATTERNS; kk++) {

				iter = 0;
				failedThisTest = TRUE;

				restore_local_lib_print = printLocalInfo;
				printLocalInfo = 0;

				while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
					iter++;

					art_txDataSetup(devNum,rate_mask,rxStation,num_tx_desc,tx_length,verifyDataPattern[kk],LEN_VERIFY_DATA_PATTERNS,retry,antenna, broadcast);
					art_rxDataSetup(devNum,num_rx_desc,rx_length,enable_ppm);
					
					sendAck(devNum, "VerifyDataTX: Prepare to receive", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
//					Sleep(30);
					art_txrxDataBegin(devNum,start_timeout,complete_timeout,stats_mode,compare,pattern,2);

					failedThisTest = FALSE;

					art_rxGetStats(devNum, 0, 1, &rStats);

					// iterate only if failed to receive all packets
					if (rStats.goodPackets < NUM_DATA_CHECK_FRAME) {
						failedThisTest |= TRUE;
					} 
				
					if (art_mdkErrNo != 0) {
						if (!art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo))
							return;
					}
					channel_repeat = (failedThisTest && (iter < CalSetup.maxRetestIters)) ? 1 : 0 ;
					sendAck(devNum, "VerifyDataTX: Repeat This Channel - see par2", *(channelTimes10+index), channel_repeat, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}

					if (CalSetup.customerDebug) uiPrintf("SNOOP: VerifyDataTX repeat iter = %d \n", iter);
				} // while failedThisTest

				if(art_mdkErrNo!=0)
					uiPrintf("\n***** Failed sending packet to Golden Unit (code:%d)\n", art_mdkErrNo);

				printLocalInfo = restore_local_lib_print;
				if ((rStats.goodPackets < NUM_DATA_CHECK_FRAME) ||(rStats.bitErrorCompares > 0)) {					
					TestFail = TRUE;
					failTest[testnum] = 1;
					if (rStats.goodPackets < NUM_DATA_CHECK_FRAME) {
						uiPrintf(" [X]%d  ", rStats.goodPackets);
					} else {
						uiPrintf(" [X]   ");
					}
				} else {
					uiPrintf("PASS   ");
				}

			} //end kk loop over NUM_VERIFY_DATA_PATTERNS

		}  // if VerifyDataTX

// turn around for		

		if (testDataRXLocal)
		{
			if (!testDataTXLocal)
			{
				//if (!done_reset)
				if (1)
				{
					art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
					done_reset = TRUE;
					if ( configSetup.force_antenna && isDragon_sd(swDeviceID))
					{
						updateAntenna(devNum);
					}
				} else
				{
					art_changeChannel(devNum, *(channelTimes10+index));
				}
			}

			uiPrintf("\n%5.1lf RX :   ", (double)(*(channelTimes10+index))/10);

			num_rx_desc =  NUM_DATA_CHECK_FRAME * 1 + 10; //just 1 rate
			rx_length = frameLen;
			start_timeout = (mode == MODE_11b) ? 50000 : 5000;		// 2000 will cause error
			complete_timeout = (mode == MODE_11b) ? 50000 : 5000;
			complete_timeout += num_rx_desc;
			stats_mode = NO_REMOTE_STATS ;

			for (kk = 0; kk < NUM_VERIFY_DATA_PATTERNS; kk++) {

				iter = 0;
				failedThisTest = TRUE;
				restore_local_lib_print = printLocalInfo;
				printLocalInfo = 0;
				while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
					iter++;

					if (!configSetup.force_antenna)
					{
					art_setAntenna(devNum, (USE_DESC_ANT | DESC_ANT_A));
					}
					art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);
					
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					art_rxDataStart(devNum);
					sendAck(devNum, "VerifyDataRX: Ready to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, compare, verifyDataPattern[kk], LEN_VERIFY_DATA_PATTERNS);
					
					failedThisTest = FALSE;
					art_rxGetStats(devNum, 0, 0, &rStats);

					// iter only if failed to rx all packets
					if(rStats.goodPackets < NUM_DATA_CHECK_FRAME) {
						failedThisTest |= TRUE;
					}
					
					if (art_mdkErrNo != 0) {
						art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
					}
					channel_repeat = (failedThisTest && (iter < CalSetup.maxRetestIters)) ? 1 : 0 ;

					sendAck(devNum, "VerifyDataRX: Repeat This Channel - see par2", *(channelTimes10+index), channel_repeat, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}

					if (CalSetup.customerDebug) uiPrintf("SNOOP: VerifyDataRX repeat iter = %d \n", iter);
				} // while failedThisTest

				if(art_mdkErrNo!=0)
					uiPrintf("\nError Code (%d)", art_mdkErrNo);

				printLocalInfo = restore_local_lib_print;
				if ((rStats.goodPackets < NUM_DATA_CHECK_FRAME) ||(rStats.bitErrorCompares > 0)) {
					TestFail = TRUE;
					failTest[testnum] = 1;
					if (rStats.goodPackets < NUM_DATA_CHECK_FRAME) {
						uiPrintf(" [X]%d  ", rStats.goodPackets);
					} else {
						uiPrintf(" [X]   ");
					}
				} else {
					uiPrintf("PASS   ");
				}
			} //end kk loop over NUM_VERIFY_DATA_PATTERNS

		} // if VerifyDataRX

	} // ch loop

	uiPrintf("\n\n");

	art_writeField(devNum, "mc_jumbo_frame_mode", jumbo_frame_mode_save);

	if (channelTimes10) {
		free(channelTimes10);
	}
}


void goldenVerifyDataTXRX(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT32 frameLen)
{
	A_UINT32 *channelTimes10;
	A_UINT16 index, ChannelCount, ii, kk;

	A_UINT32 rate_mask ;
	A_UINT32 num_desc_per_rate = NUM_DATA_CHECK_FRAME;
	A_UINT32 tx_length = frameLen;
	A_UINT32 retry = 0;		// broadcast mode, disable retry
    A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 1;	// broadcast mode

    // for rx
	A_UINT32 num_rx_desc = NUM_DATA_CHECK_FRAME + 10;
	A_UINT32 rx_length = frameLen;
	A_UINT32 enable_ppm = 0; 

    // for rxDataBegin
	A_UINT32 start_timeout = 5000;
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode = ENABLE_STATS_SEND ; //| ENABLE_STATS_RECEIVE;
	A_UINT32 compare = 1;
	A_BOOL done_reset = FALSE;

	A_UINT32 trig_level;
	A_UINT16 testChannel;


	double	 goldenTXPowerLocal ;
	A_BOOL	 testDataTXLocal = TRUE;
	A_BOOL	 testDataRXLocal = FALSE;

	A_UINT32 jumbo_frame_mode_save;

	A_BOOL   repeatThisTest;

	for (kk=0; kk<NUM_RATES; kk++) {
		if (rate == DataRate[kk]) {
			rate_mask = (1 << kk);
			break;
		}
	}

	if (mode == MODE_11a)
	{
		goldenTXPowerLocal = CalSetup.goldenTXPower;
		testChannel         = 5260;		
	} else
	{
		goldenTXPowerLocal = CalSetup.goldenTXPower_2p4[mode];
		testChannel         = 2462;		
	}

	uiPrintf("\n\nStart VerifyDataTX and VerifyDataRX Test for %s mode \n", modeName[mode]);

	ChannelCount = 1;
	channelTimes10 = (A_UINT32 *)malloc(ChannelCount*sizeof(A_UINT32)) ;
	if(NULL == channelTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in Verify data test\n");
		return;
	}
	for (ii=0; ii<ChannelCount; ii++)
	{
		channelTimes10[ii] = testChannel;
	}

	if(mode != MODE_11b) {
		// to avoid TX underruns on slow machines
		trig_level = art_getFieldForMode(devNum, "mc_trig_level", 
			((mode == MODE_11g) ? MODE_11G : MODE_11A), 0);
		//art_writeField(devNum, "mc_trig_level", 0x3f);
	}

	art_writeField(devNum, "bb_max_rx_length", 0xFFF);
	art_writeField(devNum, "bb_en_err_length_illegal", 0);
	jumbo_frame_mode_save = art_getFieldForMode(devNum, "mc_jumbo_frame_mode", mode, turbo);
	art_writeField(devNum, "mc_jumbo_frame_mode", 0);

	// Start receiving
	for(index=0; index<ChannelCount; index++) {

		//cleanup descriptor queues, to free up mem
		art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);

		if (testDataTXLocal)
		{
			// reset params changed by testDataRXLocal test for testDataTXLocal for each iteration
			complete_timeout = (mode == MODE_11b) ? 50000 : 5000;
			num_rx_desc = NUM_DATA_CHECK_FRAME + 10;
			stats_mode = ENABLE_STATS_SEND ; //| ENABLE_STATS_RECEIVE;

			uiPrintf("\nReceiving Freq: %5.1lf GHz\n", (double)(*(channelTimes10+index))/10);
			//if (!done_reset)
			if(1)
			{
				art_resetDevice(devNum, rxStation, bssID, *(channelTimes10+index), turbo);
				done_reset = TRUE;
			} else
			{
				art_changeChannel(devNum, *(channelTimes10+index));
			}
			art_forceSinglePowerTxMax(devNum, 2*20); // send stats at 20dBm
			
			for (kk=0; kk<NUM_VERIFY_DATA_PATTERNS; kk++) {
				repeatThisTest = TRUE;
				while (repeatThisTest) {
					art_setAntenna(devNum, (USE_DESC_ANT | DESC_ANT_A));
					art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);

					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					start_timeout = 5000;  // make it big as starting to rx before sending ack over ethernet.					
					art_rxDataStart(devNum);
					sendAck(devNum, "VerifyDataTX: Ready to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, compare, verifyDataPattern[kk], LEN_VERIFY_DATA_PATTERNS);
					
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					if (!verifyAckStr("VerifyDataTX: Repeat This Channel - see par2")) {
						uiPrintf("ERROR: miscommunication in sync. expected ack string: VerifyDataTX: Repeat This Channel - see par2\n");
						exit(0);
					}
					repeatThisTest = (ackRecvPar2 > 0) ? TRUE : FALSE ;
					sendAck(devNum, "VerifyDataTX: Repeat This Channel - saw par2", *(channelTimes10+index), ackRecvPar2, 0, CalSetup.customerDebug);
				}


				if(kbhit())
					break;
			} //end kk loop over NUM_VERIFY_DATA_PATTERNS
		}

		if (testDataRXLocal)
		{

			// turn around for rxsen test
			complete_timeout = (mode == MODE_11b) ? 50000 : 5000;
			complete_timeout += num_desc_per_rate*2 ;
			stats_mode = 0;

			if (!testDataTXLocal)
			{
				//if (!done_reset)
				if(1)
				{
					art_resetDevice(devNum, rxStation, bssID, *(channelTimes10+index), turbo);
					done_reset = TRUE;
				} else
				{
					art_changeChannel(devNum, *(channelTimes10+index));
				}
			}

			if(mode != MODE_11b) {
				art_writeField(devNum, "mc_trig_level", 0x3f);
			}

			art_forceSinglePowerTxMax(devNum, (A_UINT16)(2*goldenTXPowerLocal));

			uiPrintf("goldenVerifyDataTXRX::Sending Freq: %5.1lf GHz\n", (double)(*(channelTimes10+index))/10);
			
			for (kk=0; kk<NUM_VERIFY_DATA_PATTERNS; kk++) {

				repeatThisTest = TRUE;
				while (repeatThisTest) {
					art_txDataSetup(devNum,rate_mask,txStation,num_desc_per_rate,tx_length,verifyDataPattern[kk],LEN_VERIFY_DATA_PATTERNS,retry,antenna, broadcast);
					if (CalSetup.customerDebug > 0) {
						uiPrintf("Sending %d packets for VerifyDataRX test.\n", num_desc_per_rate);
					}
					sendAck(devNum, "VerifyDataRX: Prepare to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
		
					//Sleep(30);
					art_txDataBegin(devNum, complete_timeout, stats_mode);

					waitForAck(CalSetup.customerDebug);
					if (REWIND_TEST) {
						return;
					}
					if (!verifyAckStr("VerifyDataRX: Repeat This Channel - see par2")) {
						uiPrintf("ERROR: miscommunication in sync. expected ack string: VerifyDataRX: Repeat This Channel - see par2\n");
						exit(0);
					}
					repeatThisTest = (ackRecvPar2 > 0) ? TRUE : FALSE ;
					sendAck(devNum, "VerifyDataRX: Repeat This Channel - saw par2", *(channelTimes10+index), ackRecvPar2, 0, CalSetup.customerDebug);
				}

				if(mode != MODE_11b) {
					art_writeField(devNum, "mc_trig_level", trig_level);
				}				
			} //end kk loop over NUM_VERIFY_DATA_PATTERNS

		} // if test_RXSEN
	} // ch loop

	art_writeField(devNum, "mc_jumbo_frame_mode", jumbo_frame_mode_save);

	
	if(channelTimes10) {
		free(channelTimes10);
	}
	if(art_mdkErrNo!=0)
		uiPrintf("\nReceiving Error, Errno: %d", art_mdkErrNo);
}

void printResultBanner(A_UINT32 devNum, A_BOOL fail) {

    time_t   current;
    struct   tm * bt;
    A_UINT32 offset;
    AR6K_EEPROM *eepromPtr = (AR6K_EEPROM *)dummyEepromWriteArea;
    offset = ((A_UINT32)&eepromPtr->baseEepHeader.macAddr - (A_UINT32)eepromPtr) /2;

    current = time(NULL);

    bt = localtime(&current);

	// assumption : macID is a valid array with at least 6 entries

	uiPrintf("\n\n\n###########################################################################\n");
	uiPrintf("Cal Date: %4d/%02d/%02d, Cal Time: %02d:%02d, Cal SSID: 0x%4x, Cal Result: %s\n", (1900+bt->tm_year), 
		                     (bt->tm_mon + 1), bt->tm_mday, bt->tm_hour, bt->tm_min, 
							 CalSetup.subsystemID,
							 (fail ? "FAIL" : "PASS") );

		
	if (!fail) {
		if(isDragon(devNum) ) {
		    uiPrintf("Cal wlan mac ID:  %04X:%04X:%04X  \n\n", art_eepromRead(devNum, offset+2), 
			art_eepromRead(devNum, offset+1), art_eepromRead(devNum, offset));
		} else {
			uiPrintf("Cal wlan mac ID:  %04X:%04X:%04X  \n\n", art_eepromRead(devNum, 0x1f), 
			art_eepromRead(devNum, 0x1e), art_eepromRead(devNum, 0x1d));
		}

	   uiPrintf("\n\n");
       uiPrintf("                    ########     ###     ######   ######              \n");
       uiPrintf("                    ##     ##   ## ##   ##    ## ##    ##             \n");
       uiPrintf("                    ##     ##  ##   ##  ##       ##                   \n");
       uiPrintf("                    ########  ##     ##  ######   ######              \n");
       uiPrintf("                    ##        #########       ##       ##             \n");
       uiPrintf("                    ##        ##     ## ##    ## ##    ##             \n");
       uiPrintf("                    ##        ##     ##  ######   ######              \n");       
       uiPrintf("\n\n");
	} else {
	   uiPrintf("\n\n");
	   uiPrintf("                    ########    ###    #### ##             \n");
	   uiPrintf("                    ##         ## ##    ##  ##             \n");
	   uiPrintf("                    ##        ##   ##   ##  ##             \n");
	   uiPrintf("                    ######   ##     ##  ##  ##             \n");
	   uiPrintf("                    ##       #########  ##  ##             \n");
	   uiPrintf("                    ##       ##     ##  ##  ##             \n");
	   uiPrintf("                    ##       ##     ## #### ########       \n");
       uiPrintf("\n\n");
	}
	
	uiPrintf("\n###########################################################################\n\n\n");
	
}

void printTestEndSummary() {

    A_UINT8  macAddr[6];
    A_BOOL	 result;
    A_UINT32 offset;
    AR6K_EEPROM *eepromPtr = (AR6K_EEPROM *)dummyEepromWriteArea;
    offset = ((A_UINT32)&eepromPtr->baseEepHeader.macAddr - (A_UINT32)eepromPtr) /2;

	if(TestFail) {
		printResultBanner(devNumArr[MODE_11a], TestFail);
		uiPrintf("\n********************************************\n");
		uiPrintf("*****    DUT FAILED FOLLOWING TESTS    *****\n");
		report_fail_summary();
		uiPrintf("********************************************\n");
		uiPrintf("\n\nProgram the MACID ? (y/n) ");
        if(toupper(getch()) == 'Y') {
			ProgramMACID(devNumArr[MODE_11a]);			
			if(isDragon(devNumArr[MODE_11a])) {
				uiPrintf("Cal wlan mac ID:  %04X:%04X:%04X  \n", art_eepromRead(devNumArr[MODE_11a], offset+2), 
				art_eepromRead(devNumArr[MODE_11a], offset+1), art_eepromRead(devNumArr[MODE_11a], offset));
			} else {
				uiPrintf("Cal wlan mac ID:  %04X:%04X:%04X  \n", art_eepromRead(devNumArr[MODE_11a], 0x1f), 
				art_eepromRead(devNumArr[MODE_11a], 0x1e), art_eepromRead(devNumArr[MODE_11a], 0x1d));
			}
			uiPrintf("\n");
		} else 
		{
			if((configSetup.remote) && (swDeviceID == 0x0011)) {
				art_GetMacAddr(devNumArr[MODE_11a], 1, 0, (A_UCHAR *)macAddr);
				uiPrintf("Cal wlan mac ID: 0x%02X%02X_%02X%02X_%02X%02X\n", macAddr[0], macAddr[1], macAddr[2],
					macAddr[3], macAddr[4], macAddr[5]); 
			}
			else {
				uiPrintf("Cal wlan mac ID:  %04X:%04X:%04X  \n", art_eepromRead(devNumArr[MODE_11a], 0x1f), 
				art_eepromRead(devNumArr[MODE_11a], 0x1e), art_eepromRead(devNumArr[MODE_11a], 0x1d));
			}
			uiPrintf(" - no new MACID will be assigned\n");
		}
	}
	else {
		ProgramMACID(devNumArr[MODE_11a]);
		printResultBanner(devNumArr[MODE_11a], TestFail);
	}

	if((configSetup.remote) && (CalSetup.ftpdownloadFileInfo.downloadRequired)) {
		uiPrintf("\n\nProceed with download of file %s? (y/n) ", CalSetup.ftpdownloadFileInfo.remotefile);
		if(toupper(getch()) == 'Y') {
			uiPrintf("\nDownloading...\n");
			result = art_ftpDownloadFile(devNumArr[MODE_11a], CalSetup.ftpdownloadFileInfo.hostname,
				CalSetup.ftpdownloadFileInfo.username, CalSetup.ftpdownloadFileInfo.password,
				CalSetup.ftpdownloadFileInfo.remotefile, CalSetup.ftpdownloadFileInfo.localfile);

			if(!result) {
				uiPrintf("\nFailed to download file %s\n", CalSetup.ftpdownloadFileInfo.remotefile);
			}
		}
		else {
			uiPrintf("\nFile %s will not be downloaded\n", CalSetup.ftpdownloadFileInfo.remotefile);
		}
	
	}

}
	
void dutThroughputTest(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT16 frameLen, 
					   A_UINT32 numPackets)
{
	A_UINT32 *channelTimes10;
	A_UINT16 index, ii, numChannels;
    RX_STATS_STRUCT rRateStats;
	TX_STATS_STRUCT tRateStats;

	// txDataSetup
	A_UINT32 rate_mask ;

	A_UINT32 num_tx_desc = numPackets;
	A_UINT32 tx_length = frameLen;
	A_UINT32 retry = 0;		// broadcast mode, disable retry
	A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 0;

	// rxDataSetup
	A_UINT32 num_rx_desc = 50;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode =  ENABLE_STATS_RECEIVE;
	A_UINT32 compare = 0;
	A_UCHAR  pattern[] = {0xaa, 0x55};


	A_UINT32 trig_level;
	A_BOOL	 done_reset = FALSE;
	
	A_UINT32 kk;
	
	double	 goldenTXPowerLocal, attenDutGoldenLocal ;

	A_UINT16 iter, channel_repeat;
	A_BOOL   failedThisTest, restore_local_lib_print;
	A_BOOL	 testDataTXLocal = TRUE;
	A_BOOL	 testDataRXLocal = FALSE;
	A_UINT32 iterationFailMask = 0;
	A_UINT32 dataRateIndex;
	A_UINT32 missedPackets;
	A_BOOL   testThisChannel = FALSE;
	double   passThreshold;

	A_INT32  rxSignalLevel = -55;
	A_CHAR  testname_local[256];

	for (kk=0; kk<NUM_RATES; kk++) {
		if (rate == DataRate[kk]) {
			rate_mask = (1 << kk);
			dataRateIndex = kk;
			break;
		}
        }
	if (mode == MODE_11a)
	{
		attenDutGoldenLocal = CalSetup.attenDutGolden;
		goldenTXPowerLocal  = CalSetup.goldenTXPower;
	} else
	{
		attenDutGoldenLocal = CalSetup.attenDutGolden_2p4[mode];
		goldenTXPowerLocal = CalSetup.goldenTXPower_2p4[mode];
	}

    // setup attenuator here
    if(CalSetup.useInstruments) {		
    	attSet(devATT, (A_INT32)(goldenTXPowerLocal - attenDutGoldenLocal - rxSignalLevel ));
    }

	if(do_11g_CCK) {
		uiPrintf("\n\nStart Throughput Test in 11g CCK mode\n");
	} else {
	uiPrintf("\n\nStart Throughput Test in %s mode\n\n", modeName[mode]);
   }

	numChannels = pTestSet[mode]->numTestChannelsDefined;
	channelTimes10 = (A_UINT32 *)malloc(numChannels*sizeof(A_UINT32)) ;
	if(NULL == channelTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in Throughput test\n");
		return;
	}
	for (ii=0; ii<numChannels; ii++)
	{
		channelTimes10[ii] = pTestSet[mode]->pTestChannel[ii].channelValueTimes10;
	}

	if(mode != MODE_11b) {
		trig_level = art_getFieldForMode(devNum, "mc_trig_level", 
			((mode == MODE_11g) ? MODE_11G : MODE_11A), 0);
	}

	uiPrintf("Channel Good  Throughput  Good  RSSI  CRCs  Missed  Result  Rate\n");
	uiPrintf("        (Tx)              (RX)  (RX)  (RX)\n");

	sprintf(testname_local, "thruput%s", ((turbo==0) ? "" : ( (turbo==1) ? "_turbo" : ( (turbo==HALF_SPEED_MODE) ? "_halfspeed" : ""))));
	setYieldParamsForTest(devNum, modeName[mode], testname_local, "thruput", "mbps", "per", "percent"); 

	for(index=0; index<numChannels; index++) {

		if(turbo) {
			testThisChannel = pTestSet[mode]->pTestChannel[index].TEST_TURBO_THROUGHPUT;
		} else {
			testThisChannel = pTestSet[mode]->pTestChannel[index].TEST_THROUGHPUT;
		}
		if (testThisChannel < 1)
		{
			continue; // skip Throughput test for this channel unless requested
		}

		//cleanup descriptor queues, to free up mem
		art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);

		// stats receive params
		// need to reset the params changed by RX test for TX for each iteration
		if (isDragon_sd(swDeviceID))
                    num_rx_desc = 30; 
                else 
                    num_rx_desc = 50;
		rx_length = 100;
		start_timeout = 0;	// no wait time for transmit
		complete_timeout = ((mode == MODE_11b) ? 50000 : 5000);
		stats_mode =  ENABLE_STATS_RECEIVE ;

		if(mode != MODE_11b) {
					if(isDragon_sd(swDeviceID)) {
					    art_writeField(devNum, "mc_trig_level", 0x1);
					} else {
					    art_writeField(devNum, "mc_trig_level", 0x3f);
					}
		} 

		if(1)
		//if (!done_reset)
		{
			art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
			done_reset = TRUE;
		} else
		{
			art_changeChannel(devNum, *(channelTimes10+index));
		}

		uiPrintf("%5.1lf : ", (double)(*(channelTimes10+index))/10);

		iter = 0;
		failedThisTest = TRUE;

		restore_local_lib_print = printLocalInfo;
		printLocalInfo = 0;

		while ( failedThisTest && (iter < CalSetup.maxRetestIters)) {
			iter++;

			art_txDataSetup(devNum, rate_mask, rxStation, 
				num_tx_desc, tx_length, pattern, 2, retry, antenna, broadcast);
			art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);		
						
			sendAck(devNum, "ThroughputTest: Prepare to receive", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return;
			}
//			Sleep(500);
			art_txrxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);			

			failedThisTest = FALSE;

			memset(&rRateStats, 0, sizeof(RX_STATS_STRUCT));
			memset(&tRateStats, 0, sizeof(TX_STATS_STRUCT));
		//	art_rxGetStats(devNum, 0, !remoteStats, &rStats);
		//	art_txGetStats(devNum, 0, remoteStats, &tStats);

			//#################Get the rate related stats and print also
			art_rxGetStats(devNum, 0, 1, &rRateStats);
			art_txGetStats(devNum, 0, 0, &tRateStats);
			missedPackets = numPackets - (rRateStats.goodPackets + rRateStats.crcPackets);
			if(turbo) {
				passThreshold = pTestSet[mode]->pTestChannel[index].throughputTurboThreshold;
			} else {
				passThreshold = pTestSet[mode]->pTestChannel[index].throughputThreshold;
			}
			if ((float)tRateStats.newThroughput/1000 < passThreshold) {
				failedThisTest |= TRUE;
			}
		
			if (art_mdkErrNo != 0) {
				art_resetDevice(devNum, txStation, bssID, *(channelTimes10+index), turbo);
			}
			channel_repeat = (failedThisTest && (iter < CalSetup.maxRetestIters)) ? 1 : 0 ;
			sendAck(devNum, "ThroughputTest: Repeat This Channel - see par2", *(channelTimes10+index), channel_repeat, 0, CalSetup.customerDebug);
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return;
			}

			if (CalSetup.customerDebug) uiPrintf("SNOOP: ThroughputTest repeat iter = %d \n", iter);
		} // while failedThisTest

		if(art_mdkErrNo!=0)
			uiPrintf("\n***** Failed sending packet to Golden Unit (code:%d)\n", art_mdkErrNo);

		printLocalInfo = restore_local_lib_print;

		uiPrintf("% 4d     %5.1f   % 4d  % 4d  % 4d  % 4d",
			tRateStats.goodPackets, 
			(float)tRateStats.newThroughput/1000,
			rRateStats.goodPackets, 
			rRateStats.DataSigStrengthAvg,
			rRateStats.crcPackets,
			missedPackets);

		if(failedThisTest) {
			TestFail = TRUE;
			failTest[testnum] = 1;
			uiPrintf("       X:");
			logMeasToYield((double)(channelTimes10[index])/10, (double)(rate), (double)(frameLen), (double)(pTestSet[mode]->pTestChannel[index].throughputThreshold), ((double)(tRateStats.newThroughput)/1000.0), ((double)(rRateStats.goodPackets)*100.0/numPackets), "FAIL");
		}
		else {
			uiPrintf("         ");
			logMeasToYield((double)(channelTimes10[index])/10, (double)(rate), (double)(frameLen), (double)(pTestSet[mode]->pTestChannel[index].throughputThreshold), ((double)(tRateStats.newThroughput)/1000.0), ((double)(rRateStats.goodPackets)*100.0/numPackets), "PASS");
		}
		if(mode != MODE_11b) {
			uiPrintf("   %s", DataRateStr[dataRateIndex]);
			if(turbo == TURBO_ENABLE) {
				uiPrintf(" turbo");
			}
			uiPrintf("\n");
		}
		else {
			uiPrintf("   %s\n", DataRate_11b[dataRateIndex]);
		}

	} // ch loop

	uiPrintf("\n\n");

	if (channelTimes10) {
		free(channelTimes10);
	}
}


void goldenThroughputTest(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT16 frameLen,
						  A_UINT32 numPackets)
{
	A_UINT32 *channelTimes10;
	A_UINT16 index, numChannels, ii, kk;

	A_UINT32 rate_mask ;
	A_UINT32 num_desc_per_rate = numPackets;
	A_UINT32 tx_length = frameLen;
	A_UINT32 retry = 0;		
    A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 0;
	A_UCHAR  pattern[] = {0xaa, 0x55};

    // for rx
	A_UINT32 num_rx_desc = numPackets + 10;
	A_UINT32 rx_length = frameLen;
	A_UINT32 enable_ppm = 0; 

    // for rxDataBegin
	A_UINT32 start_timeout = 5000;
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode = ENABLE_STATS_SEND ; //| ENABLE_STATS_RECEIVE;
	A_UINT32 compare = 0;
	A_BOOL done_reset = FALSE;
	A_BOOL testThisChannel = FALSE;
	A_UINT32 trig_level;


	double	 goldenTXPowerLocal ;
	A_BOOL	 testDataTXLocal = TRUE;
	A_BOOL	 testDataRXLocal = FALSE;


	A_BOOL   repeatThisTest;

	for (kk=0; kk<NUM_RATES; kk++) {
		if (rate == DataRate[kk]) {
			rate_mask = (1 << kk);
			break;
		}
	}

	if (mode == MODE_11a)
	{
		goldenTXPowerLocal = CalSetup.goldenTXPower;
	} else
	{
		goldenTXPowerLocal = CalSetup.goldenTXPower_2p4[mode];
	}

	uiPrintf("\n\nStart Throughput test for %s mode \n", modeName[mode]);

	numChannels = pTestSet[mode]->numTestChannelsDefined;
	channelTimes10 = (A_UINT32 *)malloc(numChannels*sizeof(A_UINT32)) ;
	if(NULL == channelTimes10) {
		uiPrintf("Error: unable to alloc mem for channel list in Throughput test\n");
		return;
	}
	for (ii=0; ii<numChannels; ii++)
	{
		channelTimes10[ii] = pTestSet[mode]->pTestChannel[ii].channelValueTimes10;
	}

	if(mode != MODE_11b) {
		// to avoid TX underruns on slow machines
		trig_level = art_getFieldForMode(devNum, "mc_trig_level", 
			((mode == MODE_11g) ? MODE_11G : MODE_11A), 0);
		//art_writeField(devNum, "mc_trig_level", 0x3f);
	}

	// Start receiving
	for(index=0; index<numChannels; index++) {
		if(turbo) {
			testThisChannel = pTestSet[mode]->pTestChannel[index].TEST_TURBO_THROUGHPUT;
		} else {
			testThisChannel = pTestSet[mode]->pTestChannel[index].TEST_THROUGHPUT;
		}
		if (testThisChannel < 1)
		{
			continue; // skip Throughput test for this channel unless requested
		}

		//cleanup descriptor queues, to free up mem
		art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);

		// reset params changed by testDataRXLocal test for testDataTXLocal for each iteration
		complete_timeout = (mode == MODE_11b) ? 50000 : 5000;
		num_rx_desc = numPackets + 10;
		stats_mode = ENABLE_STATS_SEND ; //| ENABLE_STATS_RECEIVE;

		uiPrintf("\nReceiving Freq: %5.1lf GHz\n", (double)(*(channelTimes10+index))/10);
		//if (!done_reset)
		if(1)
		{
			art_resetDevice(devNum, rxStation, bssID, *(channelTimes10+index), turbo);
			done_reset = TRUE;
		} else
		{
			art_changeChannel(devNum, *(channelTimes10+index));
		}
		art_forceSinglePowerTxMax(devNum, 2*20); // send stats at 20dBm
		
		repeatThisTest = TRUE;
		while (repeatThisTest) {
			art_setAntenna(devNum, (USE_DESC_ANT | DESC_ANT_A));
			art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);

			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return;
			}
			start_timeout = 5000;  // make it big as starting to rx before sending ack over ethernet.					
			art_rxDataStart(devNum);
			sendAck(devNum, "ThroughputTest: Ready to receive ", *(channelTimes10+index), 0, 0, CalSetup.customerDebug);
			art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
			
			waitForAck(CalSetup.customerDebug);
			if (REWIND_TEST) {
				return;
			}
			if (!verifyAckStr("ThroughputTest: Repeat This Channel - see par2")) {
				uiPrintf("ERROR: miscommunication in sync. expected ack string: ThroughputTest: Repeat This Channel - see par2\n");
				exit(0);
			}
			repeatThisTest = (ackRecvPar2 > 0) ? TRUE : FALSE ;
			sendAck(devNum, "ThroughputTest: Repeat This Channel - saw par2", *(channelTimes10+index), ackRecvPar2, 0, CalSetup.customerDebug);
		}


		if(kbhit())
			break;

	} // ch loop

	if(channelTimes10) {
		free(channelTimes10);
	}
	if(art_mdkErrNo!=0)
		uiPrintf("\nReceiving Error, Errno: %d", art_mdkErrNo);
}

void virtual_eepromWrite(A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue, A_UINT32 eeprom_block, A_BOOL WRITE_NOW)
{

	A_UINT32 lengthofeeprom;
	//lengthofeeprom = checkSumLength;
	lengthofeeprom = eepromSize;
	art_assert(eeprom_block < NUMEEPBLK);
	//art_assert(eepromOffset < 0x400);
	art_assert(eepromOffset < lengthofeeprom);
	if(WRITE_NOW) {
		art_eepromWrite(devNum, eepromOffset, eepromValue);
	}
	
	EEPROM_DATA_BUFFER[eeprom_block][eepromOffset] = eepromValue;
}

void virtual_eeprom0Write(A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue)
{
	A_BOOL   write_now = FALSE;
	virtual_eepromWrite(devNum, eepromOffset, eepromValue, EEPBLK0, write_now);
}

void  virtual_eeprom1Write(A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue)
{
	A_BOOL   write_now = FALSE;
	virtual_eepromWrite(devNum, eepromOffset, eepromValue, EEPBLK1, write_now);
}

void virtual_eepromWriteBlock(A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT32 *buf, A_UINT32 eeprom_block, A_BOOL WRITE_NOW)
{

	A_UINT32 lengthofeeprom;
	lengthofeeprom = eepromSize;
	//lengthofeeprom = checkSumLength;
	
	art_assert(eeprom_block < NUMEEPBLK);
	//art_assert(eepromOffset < 0x400);
	art_assert(eepromOffset < lengthofeeprom);

	//art_assert((startOffset + length) < 0x400);

	art_assert((startOffset + length) < lengthofeeprom);

	if (WRITE_NOW) {
		art_eepromWriteBlock(devNum, startOffset, length, buf);
	}

	//memcpy(&(EEPROM_DATA_BUFFER[eeprom_block][startOffset]), buf, length*sizeof(A_UINT32));
	memcpy(&(EEPROM_DATA_BUFFER[eeprom_block][startOffset]), buf, length*sizeof(A_UINT32));

}

void virtual_eeprom0WriteBlock(A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT32 *buf)
{
	A_BOOL   write_now = FALSE;
	virtual_eepromWriteBlock(devNum, startOffset, length, buf, EEPBLK0, write_now);
}

void virtual_eeprom1WriteBlock(A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT32 *buf)
{
	A_BOOL   write_now = FALSE;
	virtual_eepromWriteBlock(devNum, startOffset, length, buf, EEPBLK1, write_now);
}

// writes EAR to EEPROM_DATA_BUFFER[EEPBLK0] instead of real eeprom
A_BOOL virtual_writeEarToEeprom(A_UINT32 devNum, char *pEarFile)
{
	A_UINT32 numEarValues,length;
	A_UINT16 earLocation;	
	A_UINT32 ear[MAX_EAR_LOCATIONS];

	//length = checkSumLength;
	//length = eepromSize;

	if (parseLoadEar(pEarFile, ear, &numEarValues, 0) == -1) {
		uiPrintf("virtual_writeEarToEeprom : Unable to parse ear file\n");
		return FALSE;
	}	
	CalSetup.EARLen = numEarValues;
	earLocation = (A_UINT16)(0xFFF & EEPROM_DATA_BUFFER[EEPBLK0][0xc4]);	
	//art_assert((earLocation + numEarValues) < 0x400);

	art_assert((earLocation + numEarValues) < eepromSize);
	length = numEarValues+numEarValues;

	//if ((earLocation + numEarValues) >= 0x400) 
	if ((earLocation + numEarValues) >= eepromSize) 
	{
		uiPrintf("ERROR : virtual_writeEarToEeprom : EAR file exceeded the length (%d) limit \n", length);
	}
	virtual_eeprom0WriteBlock(devNum, earLocation, numEarValues, ear);  	
	//virtual_eeprom0Write(devNum, 0xc0, virtual_eeprom_get_checksum(devNum, 0xc1, 0x400-0xc1, EEPBLK0));
	//virtual_eeprom0Write(devNum, 0xc0, virtual_eeprom_get_checksum(devNum, 0xc1, length-0xc1, EEPBLK0));
	return TRUE;
}

A_BOOL dump_virtual_eeprom(A_UINT32 eep_blk, char *filename) 
{
	A_UINT16	i;
  FILE *fStream;
	A_UINT32 length;

	length = eepromSize;
 
	CalSetup.customerDebug =1;
	if (CalSetup.customerDebug)
	{
		uiPrintf("\nWriting virtual eeprom to file %s\n", filename);
		CalSetup.customerDebug =0;
	}
	
    if( (fStream = fopen( filename, "w")) == NULL ) {
        uiPrintf("Failed to open %s - using Defaults\n", filename);
        return FALSE;
    }

	//print the frequency values
	fprintf(fStream, "loc       val\n");
	for (i = 0; i < length; i++) {
		fprintf(fStream,"0x%4x        0x%x\n", i, EEPROM_DATA_BUFFER[eep_blk][i]);
	}
	fprintf(fStream,"\n");

	fclose(fStream);
	return TRUE;
}

void program_eeprom_block1(A_UINT32 devNum)
{

	//A_UINT32 word[0x400];
	A_UINT16 fbin[20];
	A_UINT16 ii,i;
	A_UINT16 numPiers;
	A_UINT16 startOffset = 0x150;
	A_UINT16 dbmmask  = 0x3f;
	A_UINT16 pcdmask  = 0x3f;
	A_UINT16 freqmask = 0xff;
	A_UINT16 numWords = 367; // for 16k eeprom (0x2BE - 0x150)

	A_UINT32 newChkSum,length;

	A_UINT32 *word;

	if(CalSetup.EARStartAddr < 0x400) {
		length = 0x400;
	}
	else 
		length = eepromSize;
	uiPrintf("SNOOP: IN program_eeprom_block1 length = 0x%x\n", length );
	word = (A_UINT32 *)calloc(eepromSize,sizeof(A_UINT32));
	
	if(word == NULL)
	{
		printf(" Memory Not Allocated in program_eeprom_block1()\n");
		exit(1);
	}
	for(i=0;i<eepromSize;i++);
		word[i]=0xffff;
	// duplicate the block0 baseline
	//for (ii=0; ii<0x400; ii++) 
	for (ii=0; ii<length; ii++) 
	{
		word[ii] = EEPROM_DATA_BUFFER[EEPBLK0][ii];
	}

	// verify that eeprom_block0 contained valid data
	//newChkSum = dataArr_get_checksum(devNum, 0xc1, (0x400-0xc1), word);

    uiPrintf("SNOOP: IN program_eeprom_block1 length = 0x%x\n", length );
	newChkSum = dataArr_get_checksum(devNum, 0xc1, (A_UINT16)(length-0xc1), word);
	uiPrintf("SNOOP: IN program_eeprom_block1 length = 0x%x\n", newChkSum );

	if (newChkSum != word[0xc0]) {
		uiPrintf("ERROR: program_eeprom_block1: eeprom block 0 did not contain valid data\n");
		exit(0);
	}


	//calculate the correct number of words based on EAR start location
	numPiers = pCalDataset_gen3[MODE_11a]->numChannels;
	numWords = 5 + 5*numPiers; // to specify upto 10 freq piers

	if (CalSetup.eeprom_map >= CAL_FORMAT_GEN3) {
		dbmmask  = 0xff;		
		for (ii=0; ii<numPiers; ii++)
		{
			fbin[ii] = freq2fbin(pCalDataset_gen3[MODE_11a]->pChannels[ii]) ;
		}
		if (numPiers < NUM_PIERS) {
			for (ii=numPiers; ii<NUM_PIERS; ii++) {
				fbin[ii] = 0;
			}
		}
		
		get_cal_info_for_mode_gen3(&(word[0x150]), numWords, fbin, dbmmask, pcdmask, freqmask, MODE_11a);

	} else {
		uiPrintf("ERROR: program_eeprom_block1: dual 11a mode cal not supported for eeprom_map = %d\n", CalSetup.eeprom_map);
		exit(0);
	}
		

	// write checksum at 0xc0
	//word[0xc0] = dataArr_get_checksum(devNum, 0xc1, 0x400-0xc1, word);
	word[0xc0] = dataArr_get_checksum(devNum, 0xc1, (A_UINT16)(length-0xc1), word);

	// fill EEPROM_DATA_BUFFER for EEPBLK1
	//virtual_eeprom1WriteBlock(devNum, 0, 0x400, word);
	virtual_eeprom1WriteBlock(devNum, 0, length, word);

	// the final REAL write to eeprom
	if (CalSetup.customerDebug) {
		dump_virtual_eeprom(EEPBLK1, "eepblk1_dump.txt");
	}

	//art_eepromWriteBlock(devNum, 0x400, 0x400, word);

	art_eepromWriteBlock(devNum, length, length, word); // This is yet to be modifed as per the definiton
														//FJC modified, start 2nd block where other one ends
	free(word);
}

void write_eeprom_label (A_UINT32 devNum) 
{
	A_UINT32  tmpWord;
	A_CHAR    tmpStr[10];
	A_UINT32  word[0xF];
	A_UINT32  ii, chksum;

	// Example Label : MB42_035_E_1234_a0

	virtual_eeprom0Write(devNum, 0xbd, yldStruct.labelFormatID);

	tmpWord = ( (((yldStruct.cardType[1]) & 0xFF) << 8) |  // "B"
		        ((yldStruct.cardType[0]) & 0xFF) );        // "M"
	virtual_eeprom0Write(devNum, 0xb0, tmpWord);

	tmpWord = ( (((yldStruct.cardType[3]) & 0xFF) << 8) |  // "2"
		        ((yldStruct.cardType[2]) & 0xFF) );        // "4"
	virtual_eeprom0Write(devNum, 0xb1, tmpWord);

	sprintf(tmpStr, "%2d", yldStruct.cardRev);	
	tmpWord = ( (tmpStr[1] << 8) |   // "5"
		         tmpStr[0] );                                // "3"
	virtual_eeprom0Write(devNum, 0xb2, tmpWord);

	tmpWord = ( (('_' & 0xFF) << 8) |    // "_" placeholder for spare s/n spill-over
		         ((yldStruct.mfgID[0]) & 0xFF)); // "E"
	virtual_eeprom0Write(devNum, 0xb3, tmpWord);

	sprintf(tmpStr, "%04d", yldStruct.cardNum);	
	
	tmpWord = ( (tmpStr[1] << 8) |          // "2"
				 tmpStr[0] );               // "1"
	virtual_eeprom0Write(devNum, 0xb4, tmpWord);	

	tmpWord = ( (tmpStr[3] << 8) |          // "4"
				 tmpStr[2] );               // "3"
	virtual_eeprom0Write(devNum, 0xb5, tmpWord);	

	tmpWord = ( (((yldStruct.reworkID[1]) & 0xFF) << 8) | 
		        ((yldStruct.reworkID[0]) & 0xFF) );
	virtual_eeprom0Write(devNum, 0xb6, tmpWord);	

	sprintf(tmpStr, "-%d", yldStruct.chipIdentifier);	
	
	tmpWord = ( (tmpStr[0] << 8) |   //"-" spare
		         tmpStr[1] );        //0                      
	virtual_eeprom0Write(devNum, 0xb7, tmpWord);	

	// duplicate the block0 baseline
	for (ii=0; ii<0xF; ii++) {
		word[ii] = EEPROM_DATA_BUFFER[EEPBLK0][0xb0+ii];
	}
	chksum = dataArr_get_checksum(devNum, 0, 0xE, word);
	virtual_eeprom0Write(devNum, 0xbe, chksum);
}

void stress_target_powers(A_UINT32 devNum, A_UINT32 chan, double margin) {
	A_UINT32 ii;
	A_UINT16 tgt_pwr[NUM_RATES];
	
	for (ii=0; ii<NUM_RATES; ii++) {
		tgt_pwr[ii] = art_getMaxPowerForRate(devNum, (A_UINT16)chan, DataRate[ii]);
		tgt_pwr[ii] += (A_UINT16)(2*margin);
		if (tgt_pwr[ii] > 63) {
			tgt_pwr[ii] = 63;
		}
	}
	art_forcePowerTxMax(devNum, tgt_pwr);
}

void setYieldParamsForTest(A_UINT32 devNum, A_CHAR *mode, A_CHAR *test_name,
						   A_CHAR *meas_name, A_CHAR *meas_unit, 
						   A_CHAR *meas2_name, A_CHAR *meas2_unit) {

	A_UINT32 ii;

	if(!CalSetup.atherosLoggingScheme) {
		return;
	}

	yldStruct.devNum = devNum;
	for(ii=0; ii<20; ii++) {yldStruct.mode[ii] = mode[ii]; if (mode[ii] == '\0') break;}
	for(ii=0; ii<50; ii++) {yldStruct.testName[ii] = test_name[ii]; if (test_name[ii] == '\0') break;}
	for(ii=0; ii<50; ii++) {yldStruct.measName[ii] = meas_name[ii]; if (meas_name[ii] == '\0') break;}
	for(ii=0; ii<50; ii++) {yldStruct.measUnit[ii] = meas_unit[ii]; if (meas_unit[ii] == '\0') break;}
	for(ii=0; ii<50; ii++) {yldStruct.meas2Name[ii] = meas2_name[ii]; if (meas2_name[ii] == '\0') break;}
	for(ii=0; ii<50; ii++) {yldStruct.meas2Unit[ii] = meas2_unit[ii]; if (meas2_unit[ii] == '\0') break;}	
}

void logMeasToYield(double par1, double par2, double par3, double tgt, 
					double meas, double meas2, A_CHAR *result) {

	A_UINT32 ii;

	if(!CalSetup.atherosLoggingScheme) {
		return;
	}
	yldStruct.param1 = par1;
	yldStruct.param2 = par2;
	yldStruct.param3 = par3;
	yldStruct.meas = meas;
	yldStruct.meas2 = meas2;
	yldStruct.target = tgt;
	for(ii=0; ii<20; ii++) {yldStruct.result[ii] = result[ii]; if (result[ii] == '\0') break;}
	logYieldData();
}

void logYieldData() {
	A_CHAR  str[512];
	A_UINT32 currloc = 0;

	// Make sure to prog the current devNum, mode and test_name for 
	// these results to be meaningful.

	// card_macID
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.macID1[0] & 0xFFFF, 
															 yldStruct.macID1[1] & 0xFFFF, 
															 yldStruct.macID1[2] & 0xFFFF);

	// cardType
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.cardType);

	// cardRev
	currloc += sprintf(&(str[currloc]), "%02d ", yldStruct.cardRev);

	// cardMfg
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.mfgID);

	// cardNum
	currloc += sprintf(&(str[currloc]), "%04d ", yldStruct.cardNum);

	// testName
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.testName);

	// devNum
	currloc += sprintf(&(str[currloc]), "%02d ", yldStruct.devNum);

	// mode
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.mode);

	// test_param1
	currloc += sprintf(&(str[currloc]), "%5.1f ", yldStruct.param1);

	// test_param2
	currloc += sprintf(&(str[currloc]), "%5.1f ", yldStruct.param2);

	// test_param3
	currloc += sprintf(&(str[currloc]), "%5.1f ", yldStruct.param3);

	// result
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.result);

	// meas_name
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.measName);

	// test_target
	currloc += sprintf(&(str[currloc]), "%5.1f ", yldStruct.target);

	// test_meas
	currloc += sprintf(&(str[currloc]), "%5.1f ", yldStruct.meas);

	// test_unit
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.measUnit);

	// test_meas2_name
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.meas2Name);

	// test_meas2
	currloc += sprintf(&(str[currloc]), "%5.1f ", yldStruct.meas2);

	// test_meas2_unit
	currloc += sprintf(&(str[currloc]), "%s ", yldStruct.meas2Unit);

	// additional_macID_1
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.macID2[0] & 0xFFFF, 
															 yldStruct.macID2[1] & 0xFFFF, 
															 yldStruct.macID2[2] & 0xFFFF);

	// additional_macID_2
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.macID3[0] & 0xFFFF, 
															 yldStruct.macID3[1] & 0xFFFF, 
															 yldStruct.macID3[2] & 0xFFFF);

	// additional_macID_3
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.macID4[0] & 0xFFFF, 
															 yldStruct.macID4[1] & 0xFFFF, 
															 yldStruct.macID4[2] & 0xFFFF);

	// enetID_1
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.enetID1[0] & 0xFFFF, 
															 yldStruct.enetID1[1] & 0xFFFF, 
															 yldStruct.enetID1[2] & 0xFFFF);

	// additional_enetID_2
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.enetID2[0] & 0xFFFF, 
															 yldStruct.enetID2[1] & 0xFFFF, 
															 yldStruct.enetID2[2] & 0xFFFF);

	// additional_enetID_3
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.enetID3[0] & 0xFFFF, 
															 yldStruct.enetID3[1] & 0xFFFF, 
															 yldStruct.enetID3[2] & 0xFFFF);

	// additional_enetID_4
	currloc += sprintf(&(str[currloc]), "0x%04x_%04x_%04x ", yldStruct.enetID4[0] & 0xFFFF, 
															 yldStruct.enetID4[1] & 0xFFFF, 
															 yldStruct.enetID4[2] & 0xFFFF);

	uiYieldLog(str);
	uiYieldLog("\n");
}

A_BOOL dutTestEthernetThroughput(A_UINT32 devNum) 
{

    A_UINT32	time1, time2;
	A_UCHAR     *membufferW;
	A_UCHAR     *membufferR;
	A_UINT32    startAddress = 0xa05f1780 + 0*1024 + 10;
	double      tput_dnlink, tput_uplink;

	membufferR = (A_UCHAR *)malloc(1024*1024*sizeof(A_UCHAR));
	memset(membufferR, 0, 1024*1024*sizeof(A_UCHAR));

	membufferW = (A_UCHAR *)malloc(1024*1024*sizeof(A_UCHAR));
	memset(membufferW, 0x5A5A, 1024*1024*sizeof(A_UCHAR));

	time1 = milliTime();
	art_memWrite(devNum, startAddress, membufferW, (1024*1024 - 100));
	time2 = milliTime();
	tput_dnlink = (1000.0/(time2 - time1));

	uiPrintf("Downlink Ethernet Throughput = %3.1f [time = 0x%d ms]\n", tput_dnlink, (time2 - time1));

	membufferR = (A_UCHAR *)malloc(1024*1024*sizeof(A_UCHAR));
	memset(membufferR, 0, 1024*1024*sizeof(A_UCHAR));

	time1 = milliTime();
	art_memRead(devNum, startAddress, membufferR, (1024*1024-100));
	time2 = milliTime();
	tput_uplink = (1000.0/(time2 - time1));

	uiPrintf("Uplink Ethernet Throughput = %3.1f [time = 0x%d ms]\n", tput_uplink, (time2 - time1));

	free(membufferR);
	free(membufferW);

	return(TRUE);
}


A_UINT32 set_eeprom_size(A_UINT32 devNum,A_UINT32 length)
{

	A_UINT32 i,	size = 0;
	//A_UINT16 eepromLower,eepromUpper,i =0;

	size = (CalSetup.eepromLength*1024)/8;
	if((size ==0) && (length < 0x400)){
		//art_eepromWrite(devNum,	0x1b,0x0);
		//art_eepromWrite(devNum,	0x1c,0x0);
		size = 0x400;
	}
	else
	{
		for( i = 2;i<32;i++)
		{
			if(( int)(size)==(2 << (i + 9)))
			{
				size = size/2;						
				break;
			}
		}
	/*	eepromLower = (A_UINT16)(length & 0xffff);
		eepromUpper = (A_UINT16)((length & 0xfffe0000) >> 11);
		eepromUpper = eepromUpper | i;  //fix the size to 32k for now, but need this to be variable
		art_eepromWrite(devNum,	0x1b,length);
		art_eepromWrite(devNum,	0x1c,i);*/	
	}
	eepromSize=size;
	return i;
}
