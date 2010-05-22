/* test.h - the mld test header definitions */
 
/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ifndef __INCtesth
#define __INCtesth

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "perlarry.h"

#define CONFIGSETUP_FILE "artsetup.txt"
#define TESTSETUP_FILE   "arttest.txt"
#if defined(LINUX) || defined(__linux__)
#define BASE_LOG_DIR     "/lab/labenv/CAL_LOG"
#else
#define BASE_LOG_DIR     "K:\\labenv\\CAL_LOG"
#endif

#define CAL_FORMAT_VERSION    1.0
#define YIELD_FORMAT_VERSION  1.0

#define MIN_CHANNEL		 4900
#define MIN_CHANNEL_DERBY 4800
#define MAX_SOM_CHANNEL	 5900
#define MAX_FEZ_CHANNEL  5430		 
#define MIN_2G_CHANNEL	 2252
#define MAX_2G_CHANNEL   2732
#define MIN_2G_CHANNEL_DERBY   2192
#define MAX_2G_CHANNEL_DERBY   2507

//continuous tx modes
#define CONT_TX100				0
#define CONT_TX99				1
#define CONT_CARRIER			2
#define CONT_FRAME				3
#define USE_REG_FILE			0xff
#define EXTERNAL_PWR_UNKNOWN	0xff
#define INVALID_INSTANCE		0xffff0000
#define SDIO_WAIT_INTERVAL_MS   1750      //miliseconds
//max and min params
#define PCDAC_MIN				1
#define PCDAC_MAX				63
//#define PCDAC_MAX_DERBY2		127
#define PCDAC_MAX_DERBY2		63
#define INITIAL_PCDAC			30
#define POWER_OUT_MIN			0
#define POWER_OUT_MAX			63
#define INITIAL_POWER_OUT		30


#define OB_MAX					7
#define DB_MAX					7
#define B_OB_MAX				7
#define B_DB_MAX				7

#ifdef AR6002

#define OB_MIN					0
#define DB_MIN					0
#define B_OB_MIN				0
#define B_DB_MIN				0

#else

#define OB_MIN					1
#define DB_MIN					1
#define B_OB_MIN				1
#define B_DB_MIN				1

#endif // AR6002

#define GAINI_MIN				1
#define GAINI_MAX				50
#define DERBY_GAINI_MAX			35
#define GRIFFIN_GAIN_MAX        106
#define RX_GAIN_MIN				4
#define RX_GAIN_MAX				83
#define RX_GAIN_MIN_11bg		0
#define RX_GAIN_MAX_11bg		100
#define RX_GAIN_MIN_AR5112		0
#define RX_GAIN_MAX_AR5112		100
#define RX_GAIN_MIN_AR6000		-11
#define RX_GAIN_MAX_AR6000		87
#define RX_GAIN_MIN_AR6000_11g		-19
#define RX_GAIN_MAX_AR6000_11g		91
#define INITIAL_RXGAIN			60
#define MAX_XPD_GAIN_INDEX		4
//#define MAX_TP_PKT_SIZE			2000
//#define MIN_TP_PKT_SIZE			50
#define MAX_TP_PKT_SIZE			4000
#define MIN_TP_PKT_SIZE			1500
#define MIN_NUM_RETRIES			0
#define MAX_NUM_RETRIES			15
//#define MIN_NUM_PKTS			1000
#define MIN_NUM_PKTS			100
#define MAX_NUM_PKTS				10000
//#define MAX_NUM_PKTS_THIN_CLIENT	3000
#define MAX_NUM_PKTS_THIN_CLIENT	650
#define MAX_NUM_SLOTS			255
#define MIN_NUM_SLOTS			0

#define NO_PWR_CTL    0
#define DB_PWR_CTL    1
#define PCDAC_PWR_CTL 2

#define USE_TARGET_POWER 0xffffffff

//test masks
#define TX_TEST_MASK	 0x00000001
#define RX_TEST_MASK	 0x00000002
#define BEACON_TEST_MASK 0x00000004
#define TP_TEST_UP_MASK	 0x00000008
#define TP_TEST_DOWN_MASK 0x00000010
#define MAC_ADDR_MASK	 0x00000020
#define GOLDEN_TEST_MASK 0x80000000
#define BACKUP_EEPROM_MASK 0x00000040   
#define RESTORE_EEPROM_MASK 0x00000080  
#define EEP_COMPARE_MASK 0x00000100
#define ANTENNA_A_MASK   0x00000001
#define ANTENNA_B_MASK	 0x00000002

//test type definitions
#define IDLE_MODE	  0
#define TRANSMIT_TEST 1
#define RECEIVE_TEST  2
#define GOLDEN_TEST	  3
#define BEACON_TEST	  4
#define THROUGHPUT_UP_TEST 5
#define THROUGHPUT_DOWN_TEST 6

#define SIDE_CHANNEL_5G			5140
#define SIDE_CHANNEL_2G			2302
#define MDK_PACKET_OVERHEAD		30


//Failure masks
//#define RSSI_FAIL_M		0x00000001
//#define CRC_FAIL_M		0x00000002
//#define PER_FAIL_M		0x00000004
//#define PPM_FAIL_M		0x00000008
//#define TP_FAIL_M       0x00000010

//limits on arttest params
#define MAX_NUM_PACKETS		100
#define MAX_NUM_RX_PACKETS	60
#define MAX_NUM_PACKETS_TP	1000
#define MAX_PACKET_SIZE		1000


#define MAX_SIZE_COMMENT_BUFFER	256
#define MAX_AP_DUT_TYPE_LENGTH  128

#define N_A						0

#define MAX_FILE_LENGTH		265

#define DUPLICATE_NUM_ART_ANI_TYPES 3
#define FORCE_ANTA 0
#define FORCE_ANTB 1


#ifdef _WINDOWS
#define WINXP_ENE
// used to support the platform of WinXP+ENE for CUS88/CUS99
#endif

/*
typedef struct cfgTableElement {
	A_UINT16	subsystemID;
	A_CHAR		eepFilename[MAX_FILE_LENGTH];
	A_CHAR      earFilename[MAX_FILE_LENGTH];
} CFG_TABLE_ELEMENT;

typedef struct cfgTable {
	A_UINT32	sizeCfgTable;
	CFG_TABLE_ELEMENT *pCurrentElement;
	CFG_TABLE_ELEMENT *pCfgTableElements;
} CFG_TABLE;
*/

typedef struct testChannelInfo {
	A_UINT32 channel;
	A_UINT32 mode;
	A_UINT32 turbo;
} TEST_CHANNEL_INFO;

typedef struct testInfoStruct {
	A_UINT32 testType;
	A_UINT32 channel;
	A_UINT32 numIterations;
	A_UINT32 packetSize;
	A_UINT32 numPackets;
	A_UINT32 rateMask;
	A_UINT32 mode;
	A_UINT32 goldAntenna;
	A_UINT32 turbo;
	A_UINT32 txPower;
} TEST_INFO_STRUCT;

typedef struct mldConfig {
	A_UINT32	channel;
	A_UINT32    eepromLoad;
	A_UINT32	eepromLoadOverride;
	A_UINT32	eepromHeaderLoad;
	A_CHAR		creteFezCfg[MAX_FILE_LENGTH];
	A_CHAR		qmacSombreroCfg[MAX_FILE_LENGTH];
	A_CHAR		qmacSombreroBeanieCfg[MAX_FILE_LENGTH];
	A_CHAR		oahuCfg[MAX_FILE_LENGTH];
	A_CHAR		veniceCfg[MAX_FILE_LENGTH];
	A_CHAR		*pCfgFile;
	A_UINT32	maxChannel5G;
	A_UINT32    minChannel5G;
	A_UINT32    maxChannel2G;
	A_UINT32    minChannel2G;
	A_UINT32	powerOvr;
	A_UCHAR		externalPower;
	A_UCHAR		xpdGainIndex;
	A_UCHAR		xpdGainIndex2;
	A_BOOL		applyXpdGain;
	A_UINT32	dataRateIndex;
	A_UINT32	dataRateIndexTP;
	A_INT32		pcDac;
	A_UINT16	powerControlMethod;
	A_UINT32	ob;
	A_UINT32	db;
	A_UINT32	b_ob;
	A_UINT32	b_db;
	A_INT32		gainI;
	A_UINT32	contMode;
	A_UINT32	antenna;
	A_BOOL	 	force_antenna; //override the mac, force antenna to the value of the next parameter
	A_UINT8		antenna_to_force; /*determines which configuration will get written to BOTH parts of the antenna switch table
						the preceding mldConfig.antenna field is retained for compatibility*/
	A_UINT32	ANTA_Value;
	A_UINT32	ANTB_Value;
	A_UINT32	dataPattern;
	A_UINT32	turbo;
	A_INT32		rxGain;
	A_BOOL		rfGainBoost;
	A_BOOL		overwriteRxGain;
	A_BOOL		remote;
	A_BOOL		remote_exec;
	A_CHAR	    machName[256];
	A_UINT32	instance;
	A_BOOL		userInstanceOverride;
	A_UINT32    validInstance;
	A_UINT32	mode;	
	A_UINT16	use_init;	
	A_UINT32	channel5;
	A_UINT32	channel2_4;
	A_UINT16    powerOutput;
	A_INT32		rxGain5;
	A_INT32		rxGain2_4;
	A_BOOL		pktInterleave;
	A_UINT32	logging;
	A_CHAR		logFile[MAX_FILE_LENGTH];
	A_UINT32	dutSSID;
//	A_CHAR		dutCardType[112];
//	A_CHAR		cmdLineDutCardType[112];
	A_UINT32	all2GChan;
	A_UINT32	rateMask;
	A_CHAR		eepFileDir[MAX_FILE_LENGTH];
	CFG_TABLE	cfgTable;
	A_UINT32	blankEepID;
	A_UINT32	cmdLineID;
	A_UINT32	cmdLineSubsystemID;
	A_UINT32	pktSizeTP;
	A_UINT16	numRetriesTP;
	A_UINT32	numPacketsTP;
	A_UINT32	broadcastTP;
	A_BOOL		primaryAP;
	A_UINT32    enablePrint;
	A_INT32		numSlots;
	A_BOOL		validCalData;
	A_BOOL		useTargetPower;
	A_BOOL		cmdLineTest;
	A_UINT32    cmdLineTestMask;
	TEST_CHANNEL_INFO   *testChannelList;
	A_UINT16    numListChannels; 
	A_UINT16    antennaMask;
	A_UINT16    goldAntennaMask;
	A_UINT32    iterations;
	WLAN_MACADDR beaconBSSID;
	A_BOOL		rangeLogging;
	A_CHAR		rangeLogFile[MAX_FILE_LENGTH];
	A_UINT32    linkPacketSize;
	A_UINT32    linkNumPackets;
	
	// two fields to accomodate signal generators
	A_UINT32    sgNumPackets;
	A_UINT32    sgNumLoop;

	A_UINT32    refClk;
	A_UINT32    beanie2928Mode;
	A_UINT32    channelStep5g;
	A_UINT32    enableXR;
	A_UINT32    loadEar;
	A_UINT32    eepFileVersion;
	A_UINT32    earFileVersion;
	A_UINT32    earFileIdentifier;
	A_UINT32    enableCal;
	A_UINT32    artAniEnable;
	A_UINT32    artAniReuse;
	A_UINT32	artAniLevel[DUPLICATE_NUM_ART_ANI_TYPES];
	A_INT32		maxRxGain; // max gain in cont RX
	A_INT32		minRxGain; // min gain in cont RX
	A_UINT32    userDutIdOverride;
	A_UINT32    eeprom2StartLocation; // added for dual 11a and falcon support
	                                  // set to 0x400 for the 2nd eeprom_block 
	A_UINT32    computeCalsetupName;  // whether to use standard "calsetup.txt" or figure out
	                                  // appropriate filename from the computer name. default=0
	A_CHAR		eepBackupFilename[MAX_FILE_LENGTH];
	A_CHAR		eepRestoreFilename[MAX_FILE_LENGTH];
	A_UINT32    eepromCompareSingleValue;
	A_BOOL		applyCtlLimit;
	A_UINT16    ctlToApply;
	A_BOOL		debugInfo;
	A_CHAR		manufName[MAX_FILE_LENGTH]; // manufacturer name
	A_CHAR      yieldLogFile[MAX_FILE_LENGTH];  // filename for yield format logfile
	A_UINT32    enableLabelScheme;
	A_BOOL      scrambleModeOff;            // dataScramble mode
	A_UINT32    printPciWrites;
	A_BOOL		quarterChannel;
	A_CHAR		cal_eepFileName[MAX_FILE_LENGTH];
	A_CHAR		cal_usb_eepFileName[MAX_FILE_LENGTH];
	A_CHAR		cal_express_eepFileName[MAX_FILE_LENGTH];
	A_BOOL		applyPLLOverride;
	A_UINT32	pllValue;
	A_BOOL		noEepromUnlock;
        A_UINT16        eepromPresent;
        A_BOOL          antenna_BT;
        A_UINT8         switchTable_t1;
        A_UINT8         switchTable_r1;
        A_UINT8         switchTable_r1x1;
        A_UINT8         switchTable_r1x2;
        A_UINT8         switchTable_t2;
        A_UINT8         switchTable_r2;
        A_UINT8         switchTable_r2x1;
        A_UINT8         switchTable_r2x2;
        A_UINT32        rateMaskMcs20;
        A_UINT32        rateMaskMcs40;
        A_BOOL          evmDisplay;
        A_UINT32        tx_chain_mask;
	A_CHAR		eepromFile[MAX_FILE_LENGTH];
} MLD_CONFIG;

typedef struct testConfig {
	A_UINT32    numIterations;  
	A_UINT32    numPackets;		
	A_UINT32	numPacketsTP;
	A_UINT32    numPacketsTP_CCK;
	A_UINT32    packetSize;		
	A_UINT32    packetSizeTP;		
	A_UINT32	dataRateMaskTP;
	A_UINT32    perPassThreshold;	
	A_INT32     ppmMin;			
	A_INT32     ppmMax;			
	A_INT32    rssiThreshold11a_antA;  
	A_INT32    rssiThreshold11a_antB;  
	A_INT32    rssiThreshold11b_antA;  
	A_INT32    rssiThreshold11b_antB;  
	A_UINT32    rssiThreshold11g_antA;  
	A_UINT32    rssiThreshold11g_antB;  
	A_UINT32    maxCRCAllowed;  
	A_UINT32	beaconTimeout;
	float   	throughputThreshold11a;	
	float		throughputThreshold11b;	
	float		throughputThreshold11g;	
	A_UINT32    perTPThreshold;  
	A_UINT32    perTPThreshold11b;  
	WLAN_MACADDR     minMacAddress;
	WLAN_MACADDR     maxMacAddress;
	A_UINT32	sideChannel5G;
	A_UINT32	sideChannel2G;
	A_CHAR		dutOrientation;
	A_CHAR		apOrientation;
	A_CHAR		dutType[MAX_AP_DUT_TYPE_LENGTH];
	A_CHAR		apType[MAX_AP_DUT_TYPE_LENGTH];
	A_UINT32    eepromCompareSingleLocation;
} TEST_CONFIG;

typedef struct rxGainRegs {
	A_UCHAR		desiredGain;
	A_UCHAR		rfGain;
	A_UCHAR		rfAtten0;
	A_UCHAR		ifGain;
	A_UCHAR		bbGain1;
	A_UCHAR		bbGain2;
	A_UCHAR		bbGain3;
	A_UCHAR		pgaGain;
} RX_GAIN_REGS;

         
typedef struct rxGainRegsAR5112 {
	A_CHAR	   desiredGain;
	A_UCHAR    rxtxFlag;
	A_UCHAR    lnaGain;
	A_UCHAR    rfGain;
	A_UCHAR    ifGain;
	A_UCHAR    bbGainCoarse;
	A_UCHAR    bbGainFine;
} RX_GAIN_REGS_AR5112;

typedef struct rxGainRegs_11bg {
  A_UCHAR desiredGain ;
  A_UCHAR rf_b_lnagain ;
  A_UCHAR rf_b_rfgain2 ;
  A_UCHAR rf_b_rfgain1 ;
  A_UCHAR rf_b_rfgain0 ;
  A_UCHAR rf_rf_gain ;
  A_UCHAR rf_rf_atten0 ;
  A_UCHAR rf_if_gain ;
  A_UCHAR rf_bb_gain1 ;
  A_UCHAR rf_bb_gain2 ;
  A_UCHAR rf_bb_gain3 ;
  A_UCHAR rf_b_ifgain1 ;
  A_UCHAR rf_b_ifgain0 ;
  A_UCHAR rf_pga_gain ;
  A_UCHAR beanie_switch ;
  A_UCHAR bridge_switch ;
 } RX_GAIN_REGS_11bg; 

typedef struct xpdGainInfo {
	A_UCHAR			gain;
	A_UINT32		regValue;
} XPD_GAIN_INFO;

typedef struct supportedModes {
	A_BOOL	aMode;
	A_BOOL  bMode;
	A_BOOL  gMode;
} SUPPORTED_MODES;

extern MLD_CONFIG configSetup;

void printEepromStruct_16K
(
 A_UINT32	devNum,
 A_UINT32	mode
);

void changeRxGainFields
(
 A_UINT32 devNum,
 RX_GAIN_REGS *pGainValues
); 

A_BOOL setEepFile
(
 A_UINT32 devNum
);

void changeRxGainFields_11bg
(
 A_UINT32 devNum,
 RX_GAIN_REGS_11bg *pGainValues_llbg
);

void changeRxGainFields_derby
(
 A_UINT32 devNum,
 RX_GAIN_REGS_AR5112 *pGainValues
);

A_UINT32
getEarPerforceVersion
(
 char *earFile
);

A_INT32
getEarFileIdentifier
(
 char *filename
);

#if defined(_MLD) || defined(__ATH_DJGPPDOS__)
extern A_UINT32 swDeviceID;
#endif
extern A_UINT32 macRev;
extern A_UINT32 analogProdRev;
extern A_UCHAR  DataRate[];

extern A_UINT32 userEepromSize;

extern A_BOOL sizeWarning;
extern A_UINT32 checkSumLength;
extern A_UINT32 eepromSize;



void	updateChannel(A_INT16 inputKey);
void updateGainI(A_INT16 inputKey);
void setInitialPowerControlMode(A_UINT32 devNum, A_UINT32 rateIndex);
void invalidEepromMessage (A_UINT32 airtime);
void printContMenu(A_UINT32 devNum);
A_BOOL printMode (void);
void testMenu(A_UINT32 devNum);
void clearScreen(void);
A_BOOL getBssidFromString(A_UCHAR *bssid, A_CHAR *string);
A_BOOL testMacAddress(WLAN_MACADDR *addressIn);
A_UINT32 performCmdLineTests
(
 A_UINT32 devNum
);
void switchTableLoop(A_UINT32 devNum);
A_BOOL backupEeprom(A_UINT32 devNum, A_CHAR *filename);
A_BOOL restoreEeprom(A_UINT32 devNum, A_CHAR *filename);
A_BOOL updateBootRom(A_UINT32 devNum, A_CHAR *filename);

A_BOOL updateEepromBootData(A_UINT32 devNum, A_UINT32 *data, A_UINT32 size);

A_UINT16 getNFChList(A_UINT16 ch, A_UINT16 *channels);
void getNoiseFloor(A_UINT32 devNum, A_UINT16 numCh, A_UINT16 *channels, A_INT16 *nfHist);
void plotNFHist(A_UINT16 numCh, A_UINT16 *chList, A_INT16 *nfHist);

#ifdef _IQV
void setRegistersFromConfig(A_UINT32 devNum);
#endif // _IQV	
A_BOOL writeEarToEeprom(A_UINT32 devNum, char *pEarFile);
void forcePowerOrPcdac(A_UINT32 devNum);
A_BOOL show_eep_label (A_UINT32 devNum, A_BOOL print);
void setupAtherosCalLogging();
void incrementLogFile(A_CHAR *filename);
void closeAtherosCalLogging();
A_BOOL promptForLabel(A_BOOL);
void noiseImmunityMenu(A_UINT32 devNum);
void displayNoiseImmunityMenu(void);

#ifndef _ATH_DJGPPDOS
void findEepFileNew(A_UINT32 devNum);
void findEepFile();
#endif

A_BOOL initTarget(void);
A_BOOL loadTarget(void);
A_BOOL closeTarget(void);
void updateAntenna(A_UINT32 devNum);
void updateForcedAntennaVals(A_UINT32 devNum);

	   
#ifdef __cplusplus
}
#endif

#endif
