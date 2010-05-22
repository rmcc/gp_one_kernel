/* maui_cal.h - Maui calibration header definitions */

/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */

#ifndef __INCmauicalh
#define __INCmauicalh
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* === Macro Definitions === */
#define WARNINGS_ON 0

// defines

#define MAX_RATES		8

#define INVALID_INDEX	9999

// unique EEPROM header maps defines
#define EEPROM_SIZE_16K  1

#define EEPROM_MAJOR_VERSION ART_VERSION_MAJOR
#define EEPROM_MINOR_VERSION ART_VERSION_MINOR

#define SENS11A_48 (-69)
#define SENS11A_54 (-68)

#define EEPBLK0      0
#define EEPBLK1      1
#define NUMEEPBLK    2

// Cardtypes as denoted by subsystem id value
#define ATHEROS_CB10 0x1010 // NOT SUPPORTED
#define ATHEROS_CB11 0x1011 // NOT SUPPORTED
#define ATHEROS_MB10 0x2010 // NOT SUPPORTED

#define ATHEROS_CB20 0x1020	 
#define ATHEROS_CB21 0x1021
#define ATHEROS_CB22 0x1022
#define ATHEROS_MB20 0x2020  // NOT SUPPORTED
#define ATHEROS_MB21 0x2021  // NOT SUPPORTED
#define ATHEROS_MB22 0x2022
#define ATHEROS_MB23 0x2023

#define ATHEROS_CB21G 0x1025
#define ATHEROS_CB22G 0x1026
#define ATHEROS_MB21G 0x2025
#define ATHEROS_MB22G 0x2026

#define ATHEROS_MB31G_DESTUFF     0x2030     
#define ATHEROS_MB32AG            0x2031     
#define ATHEROS_MB22G_DESTUFF     0x2027     
#define ATHEROS_MB22AG_SINGLE     0x2029     
#define ATHEROS_MB23_JAPAN        0x2024     
#define ATHEROS_CB31G_DESTUFF     0x1030     
#define ATHEROS_CB32AG            0x1031     
#define ATHEROS_CB22G_DESTUFF     0x1027     
#define ATHEROS_CB22AG_SINGLE     0x1029     
#define ATHEROS_AP30              0xa032     
#define ATHEROS_AP30_040          0xa034
#define ATHEROS_AP48              0xa048
#define ATHEROS_AP41              0xa041
#define ATHEROS_AP43              0xa043
#define ATHEROS_MB42              0x2042
#define ATHEROS_MB41              0x2041
#define ATHEROS_MB43              0x2043
#define ATHEROS_MB44              0x2044
#define ATHEROS_CB41              0x1041
#define ATHEROS_CB42              0x1042
#define ATHEROS_CB43              0x1043
#define ATHEROS_CB51              0x1051
#define ATHEROS_CB51_LITE         0x1052
#define ATHEROS_MB51              0x2051
#define ATHEROS_MB51_LITE         0x2052
#define ATHEROS_USB_UB51          0xb051
#define ATHEROS_USB_UB52          0xb052
#define ATHEROS_MB62              0x2062
#define ATHEROS_MB62_LITE         0x2063
#define ATHEROS_CB62              0x1062
#define ATHEROS_CB62_LITE         0x1063
#define ATHEROS_AP51_FULL         0xa051
#define ATHEROS_AP51_LITE         0xa052

#define ATHEROS_AP21 0xa021


#define devID_CB20   0x0011 
#define devID_CB21   0x0012
#define devID_CB22   0x0012
#define devID_AP21   0x0011
#define devID_MB22   0x0012
#define devID_MB23   0x0012

#define SPEC_MASK_20MHZ          0x0001
#define SPEC_MASK_DSRC_20MHZ     0x0002
#define SPEC_MASK_DSRC_10MHZ     0x0004
#define SPEC_MASK_DSRC_5MHZ      0x0008


#define MODE_11g	   0
#define MODE_11b	   1
#define MODE_11a	   2

#define BASE			0
#define TURBO			1

#define TEST_NORMAL_MODE 0x0001
#define TEST_TURBO_MODE 0x0002
#define TEST_HALF_MODE  0x0004
#define TEST_QUARTER_MODE 0x0008
#define CHANNEL_TEST_HALF_RATE   0x01
#define CHANNEL_TEST_QUART_RATE  0x02

// Setup files
//#define CALSETUP_FILE "calsetup.txt"
#define MACID_FILE    "macid.txt"

//#ifdef PREDATOR_BUILD
//#define EEP_FILE      "atheros-usb-eep.txt"
//#else
//#define EEP_FILE      "atheros-eep.txt"
//#define EEP_FILE      "atheros-usb-eep.txt"
//#endif

#define UART_PCI_CFG_FILE      "atheros-uart-pcicfg-eep.txt"

//EEPROM related parameters
#define NUM_BAND_EDGES					8
#define	NUM_TARGET_POWER_CHANNELS		8
#define	NUM_TARGET_POWER_CHANNELS_11b	2
#define	NUM_TARGET_POWER_CHANNELS_11g	3
#ifndef _IQV
#define	NUM_PIERS						10
#define	NUM_PIERS_2p4					3
#define NUM_TEST_CHANNELS				32  // max # of test channels
#else // _IQV
#define	NUM_PIERS						8
#define	NUM_PIERS_2p4					4
#define NUM_TEST_CHANNELS				32  // max # of test channels
#endif	// _IQV

#define NUM_CORNER_FIX_CHANNELS 4

#define CAL_FORMAT_GEN2			0
#define CAL_FORMAT_GEN3			1
#define CAL_FORMAT_GEN5			2  //++JC++

#define SWAP_BYTES16(x) ((((x) >> 8) & 0xFF) | (((x) << 8) & 0xFF00))

#define INVALID_FG   10

#define NUM_VERIFY_DATA_PATTERNS  8
#define LEN_VERIFY_DATA_PATTERNS  8
//#define VERIFY_DATA_PACKET_LEN    3000

#define NO_CHIP_IDENTIFIER 0xffffffff

#define PCI_EXPRESS_CONFIG_PTR  0x80
#define READ_CONFIG             0xffffffff

typedef struct {
	A_UINT32   pciConfigLocation;
	A_UINT32   eepromLocation_MSB;
	A_UINT32   eepromLocation_LSB;
} EEPROM_PCICONFIG_MAP;


typedef struct golenParams_t {
	A_UINT16 channelStart;
	A_UINT16 channelStop;
	A_UINT16 channelStep;

	A_UINT16 measurementStep;

	A_UINT16 pcdacStart;
	A_UINT16 pcdacStop;
	A_UINT16 pcdacStep;

	A_UINT16 numIntercepts;
#if defined(LINUX) || defined(__linux__)
	A_UINT16 pInterceptPercentages[11];
#else
	A_UINT16 pInterceptPercentages[11];
#endif

} GOLDEN_PARAMS ;
	
typedef struct ftpDownloadInfo {
	A_BOOL		downloadRequired;
	A_CHAR		hostname[256];
	A_CHAR		username[256];
	A_CHAR		password[256];
	A_CHAR		remotefile[256];
	A_CHAR		localfile[256];
} FTP_DOWNLOAD_INFO;

typedef struct calSetup_t {

	// keep all values 32-bit as needed by sscanf
	A_UINT32 benchId;
	A_BOOL   useInstruments;
    A_UINT32 subsystemID;
    A_UINT32 productID;
	A_UINT32 dutPromSize;
    A_UINT32 subVendorID;
	A_UINT32 countryOrDomain;
	A_UINT32 worldWideRoaming;
	A_UINT32 countryOrDomainCode;
    A_BOOL   calPower;
	A_BOOL	 useFastCal;   // power meter or golden unit
	A_BOOL   reprogramTargetPwr;
	A_BOOL   testOBW;
    A_BOOL   testSpecMask;
    A_BOOL   testTXPER;
    A_BOOL   testRXSEN;
	A_BOOL	 testTURBO;
	A_BOOL	 testHALFRATE;  // 10 MHz bandwidth
	A_BOOL	 testQUARTERRATE;  // 5 MHz bandwidth
	A_BOOL	 testTargetPowerControl[3]; // a/b/g mode
	double   targetPowerToleranceUpper;
	double   targetPowerToleranceLower;

    A_INT32  goldenPPM;
	A_UINT32 goldenPCDAC;
    double   goldenTXPower;
	A_UINT32 goldenPCDAC_2p4[2];
    double   goldenTXPower_2p4[2];
	A_UINT32 pmModel;
	A_UINT32 pmGPIBaddr;
	A_UINT32 saModel;
	A_UINT32 saGPIBaddr;
	A_UINT32 attModel;
	A_UINT32 attGPIBaddr;
    double   attenDutPM;
    double   attenDutSA;
    double   attenDutGolden;

    double   attenDutPM_2p4[2];
    double   attenDutSA_2p4[2];
    double   attenDutGolden_2p4[2];

	A_UINT32 numEthernetPorts;   // Number of Ethernet ports. Valid Only for APs
	A_UINT32 startEthernetPort;  // Begining ethernet port: AP38 has enet1 enabled but enet0 disabled

// Common parameters
	A_UINT32 turboDisable;
	A_UINT32 turboDisable_11g;
    A_UINT32 RFSilent;

	A_UINT32 deviceType;

	double TurboMaxPower_5G; 
	double TurboMaxPower_11g;
	A_UINT32 Amode;
	A_UINT32 Bmode;
	A_UINT32 Gmode;

	A_INT32 antennaGain5G;    // 8-bit signed value. 0.5 dB steps
	A_INT32 antennaGain2p5G;  // 8-bit signed value. 0.5 dB steps

// 11a parameters (5GHz)
    A_UINT32 switchSettling;
    A_UINT32 txrxAtten;
    A_UINT32 ob_1;
    A_UINT32 db_1;
    A_UINT32 ob_2;
    A_UINT32 db_2;
    A_UINT32 ob_3;
    A_UINT32 db_3;
    A_UINT32 ob_4;
    A_UINT32 db_4;
    A_UINT32 txEndToXLNAOn;
#if defined(AR6001)
    A_UINT32 thresh62;           
#elif defined(AR6002)
    A_INT32  thresh62;           
#endif
    A_UINT32 txEndToXPAOff;  
    A_UINT32 txFrameToXPAOn; 
	A_UINT32 xpd; 
	A_UINT32 xgain; 
	A_UINT32 xlnaGain;
	A_INT32	 noisefloor_thresh;
	A_INT32  adcDesiredSize; // 8-bit signed value. 0.5 dB steps
	A_INT32  pgaDesiredSize; // 8-bit signed value. 0.5 dB steps
	A_UINT32 antennaControl[11];
	A_UINT32 fixed_bias[3]; 
	A_BOOL   do_iq_cal;
	A_UINT32 iqcal_i_corr[3];
	A_UINT32 iqcal_q_corr[3];

// 2.4 GHz parameters 
    A_UINT32 switchSettling_2p4[2];
    A_UINT32 txrxAtten_2p4[2];
    A_UINT32 ob_2p4[2];
    A_UINT32 db_2p4[2];
    A_UINT32 b_ob_2p4[2];
    A_UINT32 b_db_2p4[2];
    A_UINT32 txEndToXLNAOn_2p4[2];
#if defined(AR6001)
    A_UINT32 thresh62_2p4[2];           
#elif defined(AR6002)
    A_INT32  thresh62_2p4[2];           
#endif
    A_UINT32 txEndToXPAOff_2p4[2];  
    A_UINT32 txFrameToXPAOn_2p4[2]; 
	A_UINT32 xpd_2p4[2]; 
	A_UINT32 xgain_2p4[2]; 
	A_UINT32 xlnaGain_2p4[2];
	A_INT32 noisefloor_thresh_2p4[2];
	A_INT32 adcDesiredSize_2p4[2]; // 8-bit signed value
	A_INT32 pgaDesiredSize_2p4[2]; // 8-bit signed value
	A_UINT32 antennaControl_2p4[2][11];

	A_BOOL   readFromFile; //flag to decide whether to read from file or make fresh measurements
	char     rawDataFilename[122]; // filename to read data from
	A_BOOL   customerDebug;
	A_BOOL   showTimingReport;

	A_BOOL   endTestOnFail;

	char     macidFile[122]; // filename to read the macIDs from
	char     logFilePath[122]; // path to logging directory

	A_BOOL   forcePiers ;
	A_UINT16 piersList[15];
	A_UINT32 numForcedPiers;

	A_BOOL   forcePiers_2p4[2] ;
	A_UINT16 piersList_2p4[2][15];
	A_UINT32 numForcedPiers_2p4[2];

	A_BOOL   useOneCal; // use 11g cal for 11b as well

	char     tgtPwrFilename[122];

	A_BOOL   readFromFile_2p4[2]; // 0 -> OFDM @ 2.4, 1 -> 11b
	char	 rawDataFilename_2p4[2][122];
    A_BOOL   testSpecMask_2p4[2];
    A_BOOL   testTXPER_2p4[2];
    A_BOOL   testRXSEN_2p4[2];
	A_BOOL   testTURBO_2p4[2];
	A_BOOL	 testTempMargin[3];
	A_BOOL	 test32KHzSleepCrystal;

	A_BOOL	 testDataIntegrity[3];
	A_BOOL	 testThroughput[3];

	A_BOOL   testTXPER_margin;

	A_INT32  targetSensitivity[3][NUM_TEST_CHANNELS];

	char		goldenIPAddr[132];
	A_UINT32	caseTemperature;
	A_UINT32	falseDetectBackoff[3];

	A_UINT32  perPassLimit;
	A_UINT32  senPassLimit;
	A_INT32   ppmMaxLimit;
	A_INT32   ppmMinLimit;
	A_INT32   ppmMaxQuarterLimit;
	A_INT32   ppmMinQuarterLimit;
	A_UINT32  maskFailLimit;

	double	 maxPowerCap[3];
	double   cck_ofdm_delta;
	double   ch14_filter_cck_delta;

	A_UINT32 maxRetestIters;
	A_UINT32 cal_fixed_gain[3];

	A_UINT32 eeprom_map;  // 0 for legacy 2nd gen cal format, 1 for 3rd gen cal format
//	A_UINT32 cal_single_xpd[3];
	A_UINT32 cal_mult_xpd_gain_mask[3]; 

	A_UINT32 EARStartAddr;
	A_UINT32 EARLen;
	A_UINT32 UartPciCfgLen;
	A_UINT32 TrgtPwrStartAddr;
	A_UINT32 calStartAddr;

	A_UINT32 numSensPackets[3];
	A_UINT32 txperBackoff;
	A_UINT32 Enable_32khz;  // indicates 32khz sleep crystal stuffed.
	A_UINT32 Enable_WOW;    // indicates Wake_On_WLAN support on the board.
	A_UINT32 rxtx_margin[3];
	double   ofdm_cck_gain_delta;

	A_UINT32 instanceForMode[3];  // ART cmd line parameter. 1 based
	A_UINT32 modeMaskForRadio[2]; // mask for supported modes for all radios

	A_INT32  i_coeff_5G; // statistical avg iq_cal_coeffs
	A_INT32  q_coeff_5G; // statistical avg iq_cal_coeffs

	A_INT32  i_coeff_2G; // statistical avg iq_cal_coeffs
	A_INT32  q_coeff_2G; // statistical avg iq_cal_coeffs

	A_BOOL   atherosLoggingScheme;
	FTP_DOWNLOAD_INFO ftpdownloadFileInfo;

        A_UINT32 switchSettling_Turbo[3];
        A_UINT32 txrxAtten_Turbo[3];
	A_INT32 adcDesiredSize_Turbo[3]; // 8-bit signed value
	A_INT32 pgaDesiredSize_Turbo[3]; // 8-bit signed value
	A_UINT32 rxtx_margin_Turbo[3];

	//new capabilities
	A_BOOL   uartEnable;
	A_BOOL   compressionDisable;
	A_BOOL   fastFrameDisable;
	A_BOOL   burstingDisable;
	A_BOOL   aesDisable;
	A_UINT32 maxNumQCU;
	A_UINT32 keyCacheSize;
	A_BOOL   enableHeavyClip;
	A_BOOL   xrDisable;
	A_BOOL   enableFCCMid;
	A_BOOL   enableJapanEvenU1;
	A_BOOL   enableJapenU2;
	A_BOOL   enableJapnMid;
	A_BOOL   disableJapanOddU1;
	A_BOOL   enableJapanMode11aNew;

	A_BOOL   enableDynamicEAR;   // for griffin, enable dynamic EAR write
	A_UINT32 numDynamicEARChannels;
	A_UINT32 dynamicEARChannels[8]; 
        A_UINT32 dynamicEARVersion;
	A_UINT32 eepromLength;
	A_UINT16    max_pcdac_11a;
	A_UINT16    max_pcdac_11b;
	A_UINT16    max_pcdac_11g;
	A_UINT16 numPdGains;
	A_UINT16 pdGainBoundary[4];
	A_UINT16 pdGainOverlap;
	A_UINT16    attempt_pcdac_11a;
	A_UINT16    attempt_pcdac_11b;
	A_UINT16    attempt_pcdac_11g;
	A_BOOL      staCardOnAP;
	A_BOOL      doCCKin11g;
#if defined(AR6002)
        A_INT32     txPowerOffset_11a;
        A_INT32     txPowerOffset_11g;
        A_UINT32    xAtten1Hyst_11g;           // Hyst and Margin replace the (previously txRxAtten) value
        A_UINT32    xAtten1Margin_11g;         // One is for moving into and one out of the external attenuation level
        A_UINT32    xAtten1Db_11g;             // The external atten value (previously rxTxMargin)
        A_UINT32    xAtten2Hyst_11g;           // A secondary stage of attenuation which was not encoded into Dragon EEPROM
        A_UINT32    xAtten2Margin_11g;         //
        A_UINT32    xAtten2Db_11g;             //
        A_UINT32    xAtten1Hyst_11a;           // Hyst and Margin replace the (previously txRxAtten) value
        A_UINT32    xAtten1Margin_11a;         // One is for moving into and one out of the external attenuation level
        A_UINT32    xAtten1Db_11a;             // The external atten value (previously rxTxMargin)
        A_UINT32    xAtten2Hyst_11a;           // A secondary stage of attenuation which was not encoded into Dragon EEPROM
        A_UINT32    xAtten2Margin_11a;         //
        A_UINT32    xAtten2Db_11a;             //
	A_UINT32    pdGainOverlap_11g;
	A_UINT32    xpaBiasLvl_11a;
	A_UINT32    xpaBiasLvl_11g;
	A_UINT32    xpaBiasLvl2_11a;
	A_UINT32    xpaBiasLvl2_11g;
	A_UINT32    opFlagLna2FrontEnd;
	A_INT32     rxGainOffSet5G;
	A_INT32     rxGainOffSet2G;
	A_INT32     pcdacOffset;
	A_UINT32    opFlag0dBm;
        A_INT32     initialPwr0;
        A_INT32     initialPwr1;
        A_INT32     pwrDelta0;
        A_INT32     pwrDelta1;
        A_INT32     initialPwr0_a;
        A_INT32     initialPwr1_a;
        A_INT32     pwrDelta0_a;
        A_INT32     pwrDelta1_a;
        A_INT32     rxSensBackoff;
        A_UINT32    selLna;
        A_UINT32    selIntPd;
        A_UINT32    enablePCA_11a;
        A_UINT32    enablePCA_11g;
        A_UINT32    enablePCB_11a;
        A_UINT32    enablePCB_11g;
        A_UINT32    enableXpaA_11a;
        A_UINT32    enableXpaA_11g;
        A_UINT32    enableXpaB_11a;
        A_UINT32    enableXpaB_11g;
        A_UINT32    useTxPDinXpa;
        A_INT32     negPwrOffset;
        A_UINT32    initTxGain_11a;
        A_UINT32    initTxGain_11g;
	A_UINT32    opFlagTxGainTbl;
	A_UINT32    opFlagAntDiversity;
#endif
	A_CHAR      calDataFile[122];
	A_BOOL      writeToFile;
} CAL_SETUP; 

typedef struct yieldLog_t {
	A_UINT32 macID1[3];  // 3 x 16bit macID : [0]-->MSB16bit, [2]-->LSB16bit
	A_UINT32 macID2[3];
	A_UINT32 macID3[3];
	A_UINT32 macID4[3];
	A_UINT32 enetID1[3];
	A_UINT32 enetID2[3];
	A_UINT32 enetID3[3];
	A_UINT32 enetID4[3];
	A_CHAR   cardType[10];
	A_UINT32 cardRev;
	A_UINT32 cardNum;
	A_CHAR   testName[50];
	double   param1;
	double   param2;
	double   param3;
	A_CHAR   result[20];
	A_CHAR   measName[50];
	double   target;
	double   meas;
	A_CHAR   measUnit[50];
	A_CHAR   meas2Name[50];
	double   meas2;
	A_CHAR   meas2Unit[50];		
	A_CHAR   mode[20];
	A_UINT32 devNum;

	A_CHAR   mfgID[20];  // 1 char. 0xb2[15:8]. "_" placeholder in 0xb2[7:0] as spare for extra s/n
	A_CHAR   reworkID[20];
	A_UINT32 labelFormatID;
	A_CHAR   cardLabel[256];
	A_UINT32 chipIdentifier;
} YIELD_LOG_STRUCT;

typedef struct d_dataPerChannel {
	A_UINT16		channelValue;
	A_UINT16			pcdacMin;
	A_UINT16			pcdacMax;
	A_UINT16		numPcdacValues;
	A_UINT16		*pPcdacValues;
	double			*pPwrValues;		//power is a float
} dDATA_PER_CHANNEL;

typedef struct d_pcdacsEeprom {
	A_UINT16			*pChannels;
	A_UINT16			numChannels;
	dDATA_PER_CHANNEL	*pDataPerChannel;		//ptr to array of info held per channel
	A_BOOL				hadIdenticalPcdacs;
} dPCDACS_EEPROM;

typedef struct testChannelTargets {
	A_UINT16		channelValue;
	double			Target24 ;
	double			Target36 ;
	double			Target48 ;
	double			Target54 ;
} TARGET_CHANNEL ;

typedef struct targetsSet {
	A_UINT16	numTargetChannelsDefined ;
	TARGET_CHANNEL	*pTargetChannel ;
} TARGETS_SET ;

typedef struct testChannelMasks {
	A_UINT32		channelValueTimes10;
	A_BOOL			TEST_PER ;
	A_INT32			targetSensitivity;
	A_BOOL			LO_RATE_SEN;
	A_INT32			targetLoRateSensitivity;
	A_BOOL			TEST_TURBO;
	A_UINT32		TEST_HALF_QUART_RATE;
	A_UINT32		TEST_MASK;
	A_BOOL			TEST_OBW;
	A_UINT32		TEST_TGT_PWR;
	A_UINT32		TEST_TEMP_MARGIN;
	A_BOOL			TEST_THROUGHPUT;
	double			throughputThreshold;
	A_BOOL			TEST_TURBO_THROUGHPUT;
	double			throughputTurboThreshold;
} TEST_CHANNEL_MASKS ;

typedef struct testSet {
	A_UINT16	numTestChannelsDefined ;
	A_UINT16	maxTestChannels;
	TEST_CHANNEL_MASKS	*pTestChannel ;
} TEST_SET ;

typedef struct testGroupData {
	A_UINT16	TG_Code ;
	A_UINT16	numBandEdgesDefined ;
	A_UINT16	BandEdge[NUM_BAND_EDGES] ;
	double		maxEdgePower[NUM_BAND_EDGES];
	A_UINT16	inBandFlag[NUM_BAND_EDGES];
} TEST_GROUP_DATA ;

typedef struct testGroupSet {
	A_UINT16	numTestGroupsDefined ;
	TEST_GROUP_DATA	*pTestGroup ;
} TEST_GROUP_SET ;

typedef struct _tpVal 
{
	A_UINT16 channel_idx;
	double   val;
} TP_VAL ;

A_UINT16 topCalibrationEntry(A_UINT32 *pdevNum_inst1, A_UINT32 *pdevNum_inst2) ;
A_UINT16 calibrationMenu() ;
A_UINT16 dutBegin() ;
A_UINT16 dutCalibration( A_UINT32 devNum ) ;

void setupChannelLists() ;
void setupMaxPower() ;
void mapValues(double max, double mask[], A_UINT16 maskSize, double values[]) ;
void set_backoff_list_for_RD(A_UINT16 domain, double values[], A_UINT16 numvals) ;
void set_maxpwr_list_for_RD(A_UINT16 domain, double values[], A_UINT16 numvals) ;
void set_maxpwr_list_for_rate(A_UINT16 rate, double values[], A_UINT16 numvals);
void set_maxpwr_list_for_rate_for_domain(A_UINT16 rate, A_UINT16 domain, double values[], A_UINT16 numvals);
A_UINT16 get_domain_idx( A_UINT16	domain ) ;
A_UINT16 get_rate_idx( A_UINT16	rate ) ;
A_UINT16 get_rd_edge_idx( A_UINT16	rd_edge ) ;
void parseSetup(A_UINT32 devNum) ;
void Write_eeprom_Common_Data(A_UINT32 devNum, A_BOOL writeHeader, A_BOOL immediateWrite) ;
void Write_eeprom_header_4K(A_UINT32 devNum) ;
void Write_eeprom_header(A_UINT32 devNum) ;

A_BOOL setup_raw_datasets() ;
A_BOOL	d_allocateEepromStruct(dPCDACS_EEPROM  *pEepromStruct, A_UINT16	numChannels, A_UINT16 numPcdacs ) ;
A_UINT16 measure_all_channels(A_UINT32 devNum, A_UINT16 debug) ;
A_UINT32 dump_a2_pc_out(A_UINT32 devNum) ;
A_UINT32 read_gainf_twice(A_UINT32 devNum) ;
A_UINT32 read_gainf_with_probe_packet(A_UINT32 devNum, A_UINT32 power);

void dump_rectangular_grid_to_file( dPCDACS_EEPROM  *pEepromData, char *filename );
void dump_nonrectangular_grid_to_file( dPCDACS_EEPROM  *pEepromData, char *filename );
void make_cal_dataset_from_raw_dataset();
void find_optimal_pier_locations(A_UINT16 *turning_points, A_UINT16 numTPsToPick, double filter_size, A_UINT16 data_ceiling, A_UINT16 debug);
void filter_hash(dPCDACS_EEPROM *pDataset, double filter_size, double data_ceiling);
void truncate_hash_subzero(dPCDACS_EEPROM *pDataset);
void differentiate_hash(dPCDACS_EEPROM *srcStruct, dPCDACS_EEPROM *destStruct);
void build_turning_points(dPCDACS_EEPROM *pDataset, dPCDACS_EEPROM *pDerivDataset, A_UINT16 *TPlist, A_UINT16 *totalTPs, A_UINT16 debug);
void addUniqueElementToList(A_UINT16 *list, A_UINT16 element, A_UINT16 *listSize);
int numerical_compare( const void *arg1, const void *arg2 );
void pick_local_extrema(dPCDACS_EEPROM *pDataset, A_UINT16 pcd_idx, A_UINT16 *retList, A_UINT16 *listSize);
void consolidate_turning_points(A_UINT16 **tpList, A_UINT16 *sizeList, A_UINT16 debug);
void build_turningpoint_totempole(dPCDACS_EEPROM *pDataset, TP_VAL **totempole, A_UINT16 sizeTotempole, A_UINT16 *tpList, A_UINT16 debug);
int totempole_compare( const void *arg1, const void *arg2 );


void dMapGrid(dPCDACS_EEPROM *pSrcStruct, dPCDACS_EEPROM *pDestStruct);
double getPowerAt( A_UINT16 channel, A_UINT16	pcdacValue, dPCDACS_EEPROM *pSrcStruct);
A_BOOL dFindValueInList( A_UINT16 channel, A_UINT16 pcdacValue, dPCDACS_EEPROM *pSrcStruct, double *powerValue);
void iGetLowerUpperValues (A_UINT16	value, A_UINT16	*pList,	A_UINT16 listSize, 
						   A_UINT16	*pLowerValue, A_UINT16	*pUpperValue );
void dGetLeftRightChannels( A_UINT16 channel, dPCDACS_EEPROM *pSrcStruct, A_UINT16 *pLowerChannel,
						    A_UINT16 *pUpperChannel);
void dGetLowerUpperPcdacs( A_UINT16 pcdac, A_UINT16 channel, dPCDACS_EEPROM *pSrcStruct,
						   A_UINT16 *pLowerPcdac, A_UINT16 *pUpperPcdac);
double dGetInterpolatedValue(A_UINT32 target, A_UINT32 srcLeft, A_UINT32 srcRight, 
							 double targetLeft, double targetRight);
void build_cal_dataset_skeleton(dPCDACS_EEPROM *srcDataset, dPCDACS_EEPROM *destDataset, 
								A_UINT16 *pPercentages, A_UINT16 numIntercepts, 
								A_UINT16 *tpList, A_UINT16 numTPs);
void quantize_hash(dPCDACS_EEPROM *pDataSet);



A_UINT16 dutTest(A_UINT32 *devNum);
void dutTestSpecMask(A_UINT32 devNum);
void ResetSA(const int ud, const double center, const double span, 
			  const double ref_level, const double scale, const double rbw, const double vbw) ;
void dutTestOBW(A_UINT32 devNum) ;
void goldenTestRXSEN(A_UINT32 devNum);
void dutTestRXSEN(A_UINT32 devNum);
void goldenTestTXPER(A_UINT32 devNum);
void dutTestTXPER(A_UINT32 devNum);
//void goldenWait4WakeupCall(A_UINT32 devNum);
//void dutSendWakeupCall(A_UINT32 devNum);
void ProgramMACID(A_UINT32 devNum);
void reset_on_off(A_UINT32 devNum);
char *qq( char *format, ...);

void read_dataset_from_file(dPCDACS_EEPROM *pDataSet, char *filename);
void goldenTest();

void copyDomainName(A_UINT32 country, char **domainName);
char *getDomainName(A_UINT32 country);

void new_goldenWait4WakeupCall(A_UINT32 devNum);
void new_dutSendWakeupCall(A_UINT32 devNum);

A_UINT16 parseTargets(void);
void showTargets (void);
A_UINT16 fbin2freq(A_UINT16 fbin);
A_UINT16 freq2fbin(A_UINT16 freq);
void program_eeprom(A_UINT32 devNum);
void fill_words_for_eeprom(A_UINT32 *word, A_UINT16 numWords, A_UINT16 *fbin, 
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask);

void fill_Target_Power_and_Test_Groups(A_UINT32 *word, A_UINT16 numWords,  
							 A_UINT16 dbmmask, A_UINT16 pcdmask, A_UINT16 freqmask);
void program_Target_Power_and_Test_Groups(A_UINT32 devNum);
void set_appropriate_OBDB(A_UINT32 devNum, A_UINT16 channel);

A_UINT16 dutCalibration_2p4(A_UINT32 devNum, A_UINT32 mode);
A_UINT16 measure_all_channels_2p4(A_UINT32 devNum, A_UINT16 debug, A_UINT32 mode) ;
A_BOOL setup_raw_datasets_2p4(A_UINT32 mode) ;

A_UINT16 fbin2freq_2p4(A_UINT16 fbin);

A_UINT16 dutTest_2p4(A_UINT32 devNum,  A_UINT32 mode);
void dutTestSpecMask_2p4(A_UINT32 devNum, A_UINT32 mode);
void dutSendWakeupCall_2p4(A_UINT32 devNum, A_UINT32 mode);
void goldenWait4WakeupCall_2p4(A_UINT32 devNum);
void dutTestTXPER_2p4(A_UINT32 devNum, A_UINT32 mode);
void goldenTestTXPER_2p4(A_UINT32 devNum, A_UINT32 mode);
void goldenTestRXSEN_2p4(A_UINT32 devNum, A_UINT32 mode);
void dutTestRXSEN_2p4(A_UINT32 devNum, A_UINT32 mode);
void goldenTestRXSEN_2p4(A_UINT32 devNum, A_UINT32 mode);
void goldenTest_2p4(A_UINT32 devNum, A_UINT32 mode);

A_UINT16 getSubsystemIDfromCardType(A_CHAR *pLine, A_BOOL quiet);

extern CAL_SETUP CalSetup;
extern A_UINT16 eepromType;
extern TEST_GROUP_SET TG_Set ;
extern TEST_GROUP_SET *pTestGroupSet;
extern EEPROM_PCICONFIG_MAP fullMapping[];
extern A_UINT32 numMappings;

void dutTestTargetPower(A_UINT32 devNum, A_UINT32 mode);
A_BOOL isUNI3OddChannel(A_UINT16 channel);

void load_calsetup_vals(void); 
void load_eep_vals(A_UINT32 devNum) ;
void load_cal_section(void);

//void cornerCal(A_UINT32 devNum);
double getTargetPower (A_UINT32 mode, A_UINT32 ch, A_UINT32 rate);
//void setCornerFixBits(A_UINT32 devnum, A_UINT16 channel);

void build_cal_dataset_skeleton_old(dPCDACS_EEPROM *srcDataset, dPCDACS_EEPROM *destDataset, 
								A_UINT16 *pPercentages, A_UINT16 numIntercepts, 
								A_UINT16 *tpList, A_UINT16 numTPs);

void getDutTargetPowerFilename ();

void dutTestTXRX(A_UINT32 devNum, A_UINT32 mode);
void goldenTestTXRX(A_UINT32 devNum, A_UINT32 mode);
void report_timing_summary();


A_UINT32 sendSync (A_UINT32 devNum, char *machineName, A_UINT32 debug);
A_UINT32 waitForSync (A_UINT32 debug);
A_UINT32 sendAck (A_UINT32 devNum, char *message, A_INT32 par1, A_INT32 par2, A_INT32 par3, A_UINT32 debug);
A_UINT32 waitForAck (A_UINT32 debug);
A_BOOL verifyAckStr (char *message);

void setup_raw_pcdacs();


A_UINT32 set_eeprom_size(A_UINT32 devNum,A_UINT32 length);
/*A_UINT32 get_eeprom_size(A_UINT32 devNum,A_UINT32 *eepromSize, A_UINT32 *checkSumLength);
A_BOOL eeprom_verify_checksum (A_UINT32 devNum);*/
void checkUserSize(A_UINT32 devNum);

A_UINT32 parseTestChannels(FILE *fStream, char *pLine, A_UINT32 mode);
void test_for_temp_margin(A_UINT32 devNum, A_UINT32 mode);

A_BOOL	d_freeEepromStruct(dPCDACS_EEPROM  *pEepromStruct);
void free_all_eeprom_structs();
A_BOOL prepare_for_next_card (A_UINT32 *p_devNum);

void copy_11g_cal_to_11b(dPCDACS_EEPROM *pSrcStruct, dPCDACS_EEPROM *pDestStruct);
void report_fail_summary();
A_UINT32 optimal_fixed_gain(A_UINT32 devNum, GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 mode) ;
A_BOOL test_sleep_crystal(A_UINT32 devNum) ;

void goldenVerifyDataTXRX(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT32 frameLen);
void dutVerifyDataTXRX(A_UINT32 devNum, A_UINT32 mode, A_UINT32 turbo, A_UINT16 rate, A_UINT32 frameLen);
void printResultBanner(A_UINT32 devNum, A_BOOL fail);
void printTestEndSummary() ;

void virtual_eepromWriteBlock(A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT32 *buf, A_UINT32 eeprom_block, A_BOOL WRITE_NOW);
void virtual_eeprom0WriteBlock(A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT32 *buf);
void virtual_eeprom1WriteBlock(A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT32 *buf);

void virtual_eepromWrite(A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue, A_UINT32 eeprom_block, A_BOOL WRITE_NOW);
void virtual_eeprom0Write(A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue);
void virtual_eeprom1Write(A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue);

A_UINT32 dataArr_get_checksum(A_UINT32 devNum, A_UINT16 startAddr, A_UINT16 numWords, A_UINT32 *data);
A_UINT32 virtual_eeprom_get_checksum(A_UINT32 devNum, A_UINT16 startAddr, A_UINT16 numWords, A_UINT32 eep_blk);


A_BOOL virtual_writeEarToEeprom(A_UINT32 devNum, char *pEarFile);
A_BOOL dump_virtual_eeprom(A_UINT32 eep_blk, char *filename) ;

void program_eeprom_block1(A_UINT32 devNum);
void write_eeprom_label(A_UINT32 devNum);
void stress_target_powers(A_UINT32 devNum, A_UINT32 chan, double margin);
void setYieldParamsForTest(A_UINT32 devNum, A_CHAR *mode, A_CHAR *test_name,
						   A_CHAR *meas_name, A_CHAR *meas_unit, 
						   A_CHAR *meas2_name, A_CHAR *meas2_unit);
void logMeasToYield(double par1, double par2, double par3, double tgt, 
					double meas, double meas2, A_CHAR *result);
void logYieldData();
//A_UINT32 eeprom_get_checksum(A_UINT32 devNum, A_UINT16 startAddr, A_UINT32 numWords) ;
void programMACIDFromLabel(A_UINT32 devNum);
A_BOOL dutTestEthernetThroughput(A_UINT32 devNum);

A_UINT32 programUartPciCfgFile(A_UINT32 devNum, const char *filename, A_UINT32 startAddr,A_BOOL atCal);

A_BOOL getPCIConfigMapping
(
	A_UINT32 devNum,
	A_UINT32 eepromLocation,
	A_UINT32 *remapLocation
);

#ifdef __cplusplus
}
#endif

#endif //__INCmauicalh








