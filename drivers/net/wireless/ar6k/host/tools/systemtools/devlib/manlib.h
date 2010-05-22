/* 
 *  Copyright © 2001 Atheros Communications, Inc.,  All Rights Reserved.
 */
/* manlib.h - Exported functions and defines for the manufacturing lib */

//#ifndef _IQV
//#define	_IQV
//#define	_IQVNODEBUG
//#endif	// _IQV
#ifdef _IQV
// make sure this is defined to substitute secure versions of deprecated library calls
#ifdef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#undef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 
#endif
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
// turn off deprecated warning for remaining ones
#pragma warning(disable : 4996)
#endif	// _IQV

#ifndef	__INCmanlibh
#define	__INCmanlibh
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef VXWORKS
#include <vxworks.h>
#endif

#ifndef MDK_AP 
#include <stdio.h>
#endif
#ifdef SOC_LINUX
#include <stdio.h>
#endif

#include "common_defs.h"

#if defined(AR6002)
#ifdef _WINDOWS
    #include "mEepStruct6000.h"
#endif
#endif

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/manlib.h#4 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/manlib.h#4 $"

#define LIB_MAX_DEV	8		/* Number of maximum supported devices */

#define MAX_LB_FRAME_LEN		320
#define BLOCK_PKT_COUNT			1000

#define RATE_6		0x00000001
#define RATE_9		0x00000002
#define RATE_12		0x00000004
#define RATE_18		0x00000008
#define RATE_24		0x00000010
#define RATE_36		0x00000020
#define RATE_48		0x00000040
#define RATE_54		0x00000080
#define RATE_1L     0x00000100
#define RATE_2L     0x00000200
#define RATE_2S     0x00000400
#define RATE_5L     0x00000800
#define RATE_5S     0x00001000
#define RATE_11L    0x00002000
#define RATE_11S    0x00004000
#define RATE_QUART  0x00008000
#define RATE_HALF   0x00010000
#define RATE_1		0x00020000
#define RATE_2      0x00040000
#define RATE_3      0x00080000
#define RATE_GROUP	0x10000000
#define NUM_RATES			20

#define DESC_ANT_A   0x00000000
#define DESC_ANT_B   0x00000001
#define USE_DESC_ANT 0x00000002

#define DESC_CHAIN_2_ANT_A   0x00000000
#define DESC_CHAIN_2_ANT_B   0x00010000

#define CONT_DATA           1  // Continuous tx 100 Data
#define CONT_SINE           2  // Continuous Sine Transmission
#define CONT_SINGLE_CARRIER 3  // Continuous Transmission on one subcarrier
#define CONT_FRAMED_DATA    4  // Continuous tx 99 Data - multi frame

#define DAC_9BIT_MAX_VALUE  511
#define DAC_11BIT_MAX_VALUE  2047

#define ZEROES_PATTERN 	 0x00000000
#define ONES_PATTERN     0x00000001
#define REPEATING_5A     0x00000002
#define COUNTING_PATTERN 0x00000003
#define PN7_PATTERN      0x00000004
#define PN9_PATTERN	     0x00000005
#define REPEATING_10 	 0x00000006
#define RANDOM_PATTERN 	 0x00000007

#define NO_REMOTE_STATS       0x00000000
#define ENABLE_STATS_SEND     0x00000001
#define ENABLE_STATS_RECEIVE  0x00000002
#define SKIP_STATS_COLLECTION     0x00000004 

#define SKIP_SOME_STATS		  0x00000010
#define LEAVE_DESC_STATUS  	  0x00000100
#define NUM_TO_SKIP_S		  16
#define NUM_TO_SKIP_M		  0xffff0000

//packet types
#define MDK_NORMAL_PKT		0x4d64         
#define MDK_LAST_PKT		0x4d65
#define MDK_TX_STATS_PKT	0x4d66
#define MDK_RX_STATS_PKT	0x4d67
#define MDK_TXRX_STATS_PKT	0x4d68
#define MDK_PROBE_PKT		0x4d69
#define MDK_PROBE_LAST_PKT	0x4d6a
#define MDK_SKIP_STATS_PKT	0x4d6b

// Last desc types
#define LAST_DESC_NEXT		0x0
#define LAST_DESC_NULL		0x1
#define LAST_DESC_LOOP		0x2
#define LAST_DESC_FIRST		0x3

// Desc info 
#define DESC_INFO_NUM_DESC_MASK				0xffff
#define DESC_INFO_NUM_DESC_BIT_START		0
#define DESC_INFO_NUM_DESC_WORDS_BIT_START	16
#define DESC_INFO_NUM_DESC_WORDS_MASK		(0xff << DESC_INFO_NUM_DESC_WORDS_BIT_START)
#define DESC_INFO_LAST_DESC_BIT_START		28
#define DESC_INFO_LAST_DESC_MASK			(0xf << DESC_INFO_LAST_DESC_BIT_START)

#define DESC_OP_INTR_BIT_START				0
#define DESC_OP_INTR_BIT_MASK				(0x1f << DESC_OP_INTR_BIT_START)
#define DESC_OP_WORD_OFFSET_BIT_START		8
#define DESC_OP_WORD_OFFSET_BIT_MASK		(0xff << DESC_OP_WORD_OFFSET_BIT_START)
#define DESC_OP_NDESC_OFFSET_BIT_START		16
#define DESC_OP_NDESC_OFFSET_BIT_MASK		(0xffff << DESC_OP_NDESC_OFFSET_BIT_START)

#define BUF_ADDR_INC_CLEAR_BUF_BIT_START		28
#define BUF_ADDR_INC_CLEAR_BUF_BIT_MASK		(0xf << BUF_ADDR_INC_CLEAR_BUF_BIT_START)


#define PROBE_PKT			0x10000		

// Defines for tpScale levels
#define TP_SCALE_LOWEST 0
#define TP_SCALE_MAX 0 
#define TP_SCALE_50  1
#define TP_SCALE_25  2
#define TP_SCALE_12  3  
#define TP_SCALE_MIN 4
#define TP_SCALE_HIGHEST 4

#define MAX_MODE				3		
#define QUARTER_CHANNEL_MASK	0x10000  //use the upper half word of turbo flag to specify the quarter channels 
	                                     //(ie cause 2.5 to be added to current freq
#define CLEAR_QUARTER_CHANNEL_MASK 0xfffeffff

#define NUM_TURBO_MASK_PTS	     512
#define NUM_BASE_MASK_PTS		 256     // must be .5 * NUM_TURBO_MASK_PTS

#ifndef SWIG

//flags for tx and rx descriptor cleanup
#define TX_CLEAN				0x01
#define RX_CLEAN				0x02

#define SIZE_ERROR_BUFFER		256

// Library Globals
#define DO_OFSET_CAL 0x00000001
#define DO_NF_CAL	 0x00000002

//setting for derby refclk param
#define REF_CLK_DYNAMIC		0xffff
#define REF_CLK_2_5			0
#define REF_CLK_5			1
#define REF_CLK_10			2
#define REF_CLK_20			3

//CTL mode Flags
#define CTL_MODE_11A		0
#define CTL_MODE_11A_TURBO  0x3
#define CTL_MODE_11G		0x2
#define CTL_MODE_11G_TURBO  0x4
#define CTL_MODE_11B		0x1

#define NO_CTL_IN_EEPROM	0xff
#define NOT_CTL_LIMITED		0xfe

#define UNINITIALIZED_PHASE_DELTA   0xFFFF
#define UNINITIALIZED_CHANNEL       0xFFFF

#define GAIN_OVERRIDE       0xffff

#define  AR6000_TX_GAIN_TBL_SIZE  22 
#define  TXGAIN_TABLE_STRING      "bb_tx_gain_table_"
#define  TXGAIN_TABLE_GAININHALFDB_LSB           12
#define  TXGAIN_TABLE_GAININHALFDB_MASK          0x7F
#define  TXGAIN_TABLE_GAININHALFDB_MASKFULL      0x7F000

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the MLIB_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// MLIB_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef WIN32
#ifdef MAUILIB_EXPORTS
#define MANLIB_API __declspec( dllexport )
#else
#define MANLIB_API __declspec( dllimport )
#endif
#else	// #ifdef WIN32
#define MANLIB_API  
#endif	// #ifdef WIN32

#ifdef __ATH_DJGPPDOS__
#undef  MANLIB_API
#define MANLIB_API
#endif //__ATH_DJGPPDOS__

// Non-ANSI definitions
#if !defined(VXWORKS) && !defined (WIN32) 
#ifndef HANDLE
typedef	long HANDLE;
#endif
typedef	unsigned long DWORD;
#endif

#ifndef NULL
#define NULL	0
#endif
#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

#define MAX_DESC_WORDS		8
#define MAX_TX_DESC_WORDS		8

typedef struct ISREvent {
	A_UINT32 valid;
	A_UINT32 ISRValue;
	A_UINT32 additionalInfo[5];
} ISR_EVENT; 


typedef struct txStats {
	A_UINT32 goodPackets;
	A_UINT32 underruns;
	A_UINT32 ackSigStrengthMin;
	A_UINT32 ackSigStrengthMax;
	A_UINT32 ackSigStrengthAvg;
	A_UINT32 throughput;
	A_UINT32 excessiveRetries;
	A_UINT32 shortRetry1;
	A_UINT32 shortRetry2;
	A_UINT32 shortRetry3;
	A_UINT32 shortRetry4;
	A_UINT32 shortRetry5;
	A_UINT32 shortRetry6to10;
	A_UINT32 shortRetry11to15;
	A_UINT32 longRetry1;
	A_UINT32 longRetry2;
	A_UINT32 longRetry3;
	A_UINT32 longRetry4;
	A_UINT32 longRetry5;
	A_UINT32 longRetry6to10;
	A_UINT32 longRetry11to15;
	A_UINT32 newThroughput;
	A_UINT32 startTime;
	A_UINT32 endTime;
	A_UINT32 firstPktGood;
	A_UINT32 AckRSSIPerAntMin[4];
	A_UINT32 AckRSSIPerAntMax[4];
	A_UINT32 AckRSSIPerAntAvg[4];
	A_UINT32 AckChain0AntSel[2];
	A_UINT32 AckChain1AntSel[2];
	A_UINT32 AckChain0AntReq[2];
	A_UINT32 AckChain1AntReq[2];
	A_UINT32 TXAnt[2];
	A_UINT32 BFCount; // for falcon. total number of bemformed packets
} TX_STATS_STRUCT;

typedef struct rxStats {
	A_UINT32 goodPackets;
	A_INT32 DataSigStrengthMin;
	A_INT32 DataSigStrengthMax;
	A_INT32 DataSigStrengthAvg;
	A_UINT32 crcPackets;
	A_UINT32 singleDups;
	A_UINT32 multipleDups;
	A_UINT32 bitMiscompares;
    A_UINT32 bitErrorCompares;
	A_INT32 ppmMin;
	A_INT32 ppmMax;
	A_INT32 ppmAvg;
    A_UINT32 decrypErrors;
	A_INT32 RSSIPerAntMin[4];
	A_INT32 RSSIPerAntMax[4];
	A_INT32 RSSIPerAntAvg[4];
	A_UINT32 maxRSSIAnt;
	A_UINT32 Chain0AntSel[2];
	A_UINT32 Chain1AntSel[2];
	A_UINT32 Chain0AntReq[2];
	A_UINT32 Chain1AntReq[2];
	A_UINT32 ChainStrong[2];
} RX_STATS_STRUCT;

typedef struct rxStatsSnapshot {
	A_UINT32 goodPackets;
	A_INT32 DataSigStrength;
	A_UINT32 dataRate;
	A_UINT32  bodySize;
	A_UINT32 crcPackets;
    A_UINT32 decrypErrors;
	A_INT32   DataSigStrengthPerAnt[4];
	A_UINT32  ch0Sel;
	A_UINT32  ch1Sel;
	A_UINT32  ch0Req;
	A_UINT32  ch1Req;
	A_UINT32  chStr;
} RX_STATS_SNAPSHOT;

//subset of device information that gets passed between layers
typedef struct subDevInfo {
    A_CHAR           regFilename[128];  //register file name
    A_CHAR	     libRevStr[128];
    A_UINT32         aRevID;    //analog revID
    A_UINT32         hwDevID;    //pci devID read from hardware
	A_UINT32		 swDevID;    //software based ID 
								 //more unique than pci devID to identify chipsets
    A_UINT32		 bbRevID;  //baseband devision
    A_UINT32	     macRev;   // The Mac revision number
	A_UINT32		 subSystemID;
	A_UINT32		defaultConfig;
} SUB_DEV_INFO;
// API extended commands from this Library

//Structure to hold library params that can be tweeked from outside
//setup a structure, so number of params can be increased without changing
//library calls. 
//IMPORTANT NOTE: only add 32bit values to this or fix the endian swapping in 
//dk_client.c.  It is assuming this will grow 32 bit params only
typedef struct _libParams
{
	A_UINT32 refClock;				//used only by derby
	A_UINT32 beanie2928Mode;		//set derby to 2928 rather than 3.168
	A_UINT32 enableXR;
	A_UINT32 loadEar;
	A_UINT32 artAniEnable;          // enable ART Automatic Noise Immunity 
	A_UINT32 artAniReuse;           // reuse ANI levels for that channel 
	A_UINT32 eepStartLocation;       // start location of 2nd eeprom_block if exists
	A_UINT32 chainSelect;            // chainSelect for falcon
	A_UINT32 flashCalSectorErased;   // whether to erase cal sector on flash for falcon
	A_UINT32 useShadowEeprom;        // whether to use shadow EEPROM instead of real flash
	A_UINT32 phaseDelta;             // phase delta bet'n chain0 and chain1 for falcon
	A_UINT32 applyCtlLimit;			//apply ctl limit flag
	A_UINT32 ctlToApply;			//which ctl should be applied if above flag is true
	A_UINT32 printPciWrites;		//debug mode to enable dumping of pci writes
	A_UINT32 artAniNILevel;         // manual noise immunity NI level
	A_UINT32 artAniBILevel;         // manual noise immunity BI level
	A_UINT32 artAniSILevel;         // manual noise immunity SI level
	A_UINT32 applyPLLOverride;		// set if want to override new pll value
	A_UINT32 pllValue;				// New pll value to apply
	A_UINT32 noUnlockEeprom;    	// Set to keep eeprom write protection for griffin
	A_UINT32 antenna;               // antenna selection from configSetup
	A_UINT32 useEepromNotFlash;     // whether to use EEPROM instead of flash
	A_UINT32 tx_chain_mask;         // for falcon. value from eeprom gets precedence
	A_UINT32 rx_chain_mask;         // for falcon. value from eeprom gets precedence
	A_BOOL   chain_mask_updated_from_eeprom;
	A_UINT32 synthesizerOffset;       // offset between 2.5G synthesizer phases for 2 derbies
} LIB_PARAMS;

#define NUM_16K_EDGES	8

//NOTE: in order for endian swapping to work (and be easy),
//leave the next 2 structures containing all 32 bit values.
typedef struct {
	A_UINT32 lowChannel;
	A_UINT32 highChannel;
	A_INT32 twicePower;
} CTL_PWR_RANGE;

typedef struct {
	CTL_PWR_RANGE      ctlPowerChannelRangeOfdm[NUM_16K_EDGES];
	CTL_PWR_RANGE      ctlPowerChannelRangeCck[NUM_16K_EDGES];
	A_INT32			   powerOfdm;
	A_INT32			   powerCck;
	A_UINT32		   channelApplied;
	A_UINT32		   structureFilled;
} CTL_POWER_INFO;

typedef struct struct_GenericFnCall{
	A_UINT32 fnId;
	A_UINT32 arg1;
	A_UINT32 arg2;
	A_UINT32 arg3;
	A_UINT32 arg4;
	A_UINT32 arg5;
	A_UINT32 arg6;
	A_UINT32 arg7;
	A_UINT32 arg8;
}ST_GENERIC_FN_CALL;

typedef struct DeviceMap {
	A_UINT32 DEV_MEMORY_ADDRESS; // Base location of memory access functions
	A_UINT32 DEV_MEMORY_RANGE;   // Range in bytes for memory access 
	A_UINT32 DEV_REG_ADDRESS;    // Base location of register access functions
	A_UINT32 DEV_REG_RANGE;      // Range in bytes for memory access 
	A_UINT32 DEV_CFG_ADDRESS;    // Base location of PCIconfig access functions
	A_UINT32 DEV_CFG_RANGE;      // Range in bytes for PCIconfig access 

	A_UINT16  devIndex;	// device index of the low level structure

	void (* OSmemRead)(A_UINT32 devNum, A_UINT32 address, A_UCHAR *memBytes, A_UINT32 length);
	void (* OSmemWrite)(A_UINT32 devNum, A_UINT32 address, A_UCHAR *memBytes, A_UINT32 length);
	A_UINT32 (* OSregRead)(A_UINT32 devNum, A_UINT32 address);
	void (* OSregWrite)(A_UINT32 devNum, A_UINT32 address, A_UINT32 regValue);   
	A_UINT32 (* OScfgRead)(A_UINT32 devNum, A_UINT32 address);
	void (* OScfgWrite)(A_UINT32 devNum, A_UINT32 address, A_UINT32 cfgValue);
	ISR_EVENT (* getISREvent)(A_UINT32 devNum); // Get a pointer to the latest ISREvent struct

    A_UINT16 remoteLib;
	void (* r_eepromReadBlock) ( A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT32 *buf);
	void (* r_eepromReadLocs) ( A_UINT32 devNum, A_UINT32 startOffset, A_UINT32 length, A_UINT8 *buf);
    A_UINT32 (* r_eepromRead) (A_UINT32 devNum, A_UINT32 eepromOffset);
    void (* r_eepromWrite) (A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue);
    A_UINT32 (* r_hwReset) (A_UINT32 devNum, A_UINT32 resetMask );
    void (* r_pllProgram) (A_UINT32 devNum, A_UINT32 turbo, A_UINT32 mode );
#ifndef _IQV
    void (* r_pciWrite) (A_UINT32 devNum, PCI_VALUES *pPciValues, A_UINT32 size);
#else
    A_UINT32 (* r_pciWrite) (A_UINT32 devNum, PCI_VALUES *pPciValues, A_UINT32 size);
#endif // _IQV
    A_UINT32 (* r_calCheck) (A_UINT32 devNum, A_UINT32 enableCal, A_UINT32 timeout);
	void (* r_fillTxStats) ( A_UINT32 devNum, A_UINT32 descAddress, A_UINT32 numDesc, A_UINT32 dataBodyLen, A_UINT32 txTime, TX_STATS_STRUCT *txStats);
	void (* r_createDescriptors)(A_UINT32 devNumIndex, A_UINT32 descBaseAddress, A_UINT32 descInfo, A_UINT32 bufAddrIncrement, A_UINT32 descOp, A_UINT32 *descWords);
	A_UINT32 (*send_generic_fn_call_cmd) (A_UINT32 devNum, void *stGenericFnCall);
	A_UINT32 (* OSapRegRead32)(A_UINT32 devNum, A_UINT32 address);
	void (* OSapRegWrite32)(A_UINT32 devNum, A_UINT32 address, A_UINT32 regValue);   
	A_UINT32 (* OSmem32Read)(A_UINT32 devNum, A_UINT32 address);
	void (* OSmem32Write)(A_UINT32 devNum, A_UINT32 address, A_UINT32 regValue);   
} DEVICE_MAP;   

// Setup/Configuration Functions
MANLIB_API A_INT32 getNextDevNum();
MANLIB_API A_UINT32 initializeDevice(struct DeviceMap map);
MANLIB_API A_UINT32 eepromRead(A_UINT32 devNum, A_UINT32 eepromOffset);
MANLIB_API void eepromWrite(A_UINT32 devNum, A_UINT32 eepromOffset, A_UINT32 eepromValue);
MANLIB_API void eepromReadBlock(A_UINT32 devNum,A_UINT32 startOffset,A_UINT32 length,A_UINT32 *eepromValue);
MANLIB_API void eepromWriteBlock(A_UINT32 devNum,A_UINT32 startOffset,A_UINT32 length,A_UINT32 *eepromValue);
MANLIB_API void resetDevice(A_UINT32 devNum, A_UCHAR *mac, A_UCHAR *bss, A_UINT32 freq, A_UINT32 turbo);
MANLIB_API A_UINT32 checkRegs(A_UINT32 devNum);
MANLIB_API void changeChannel(A_UINT32 devNum, A_UINT32 freq);
MANLIB_API A_BOOL setAllChannelAr5211(A_UINT32 devNum,A_UINT32 somFreq, A_UINT32 beanieFreq);
MANLIB_API void setAntenna(A_UINT32 devNum, A_UINT32 antenna);
MANLIB_API void setTransmitPower(A_UINT32 devNum, A_UCHAR txPowerArray[17]);
MANLIB_API void setSingleTransmitPower(A_UINT32 devNum, A_UCHAR pcdac);
MANLIB_API void setPowerScale(A_UINT32 devNum, A_UINT32 powerScale);
MANLIB_API void devSleep(A_UINT32 devNum);
MANLIB_API void closeDevice(A_UINT32 devNum);
MANLIB_API A_UINT32 checkProm(A_UINT32 devNum, A_UINT32 enablePrint);
MANLIB_API void rereadProm(A_UINT32 devNum);
MANLIB_API void setResetParams(A_UINT32 devNum, A_CHAR *pFilename, A_BOOL eePromLoad, A_BOOL eePromHeaderLoad, A_UCHAR mode, A_UINT16 initCodeFlag);
MANLIB_API void changeField(A_UINT32 devNum, A_CHAR *fieldName, A_UINT32 newValue);
MANLIB_API void dumpPciRegValues(A_UINT32 devNum); 
MANLIB_API void displayPciRegWrites(A_UINT32 devNum); 
MANLIB_API void getField(A_UINT32 devNum, A_CHAR   *fieldName, A_UINT32 *baseValue,	
 A_UINT32 *turboValue);
MANLIB_API void readField(A_UINT32	devNum, A_CHAR *fieldName, A_UINT32	*pUnsignedValue, 
						  A_INT32 *pSignedValue, A_BOOL	*pSignedFlag);
MANLIB_API void writeField(A_UINT32 devNum, A_CHAR *fieldName, A_UINT32 newValue);
MANLIB_API void forceAntennaTbl5211(A_UINT32 devNum, A_UINT16 *pAntennaTbl);
MANLIB_API void forceAntennaTbl5513(A_UINT32 devNum, A_UINT16 *pAntennaTbl);
MANLIB_API void setAntennaTbl5211(A_UINT32 devNum, A_UINT16 *pAntennaTbl);
MANLIB_API void readAntennaTbl5211(A_UINT32 devNum, A_UINT16 *pAntennaTbl);
MANLIB_API void specifySubSystemID(A_UINT32 devNum, A_UINT32	subsystemID);
// MDK library-use specific call
MANLIB_API void useMDKMemory(A_UINT32 devNum, A_UCHAR *pExtAllocMap, A_UINT16 *pExtIndexBlocks);

// Data Frame Functions
MANLIB_API void txDataSetup(A_UINT32 devNum, A_UINT32 rateMask, A_UCHAR *dest, 
							A_UINT32 numDescPerRate, A_UINT32 dataBodyLength, 
							A_UCHAR *dataPattern, A_UINT32 dataPatternLength, 
							A_UINT32 retries, A_UINT32 antenna, A_UINT32 broadcast);
MANLIB_API void txDataSetupNoEndPacket(A_UINT32 devNum, A_UINT32 rateMask, A_UCHAR *dest, 
							A_UINT32 numDescPerRate, A_UINT32 dataBodyLength, 
							A_UCHAR *dataPattern, A_UINT32 dataPatternLength, 
							A_UINT32 retries, A_UINT32 antenna, A_UINT32 broadcast);
MANLIB_API void txDataBegin(A_UINT32 devNum, A_UINT32 timeout, A_UINT32 remoteStats);
MANLIB_API void txDataStart(A_UINT32 devNum);
MANLIB_API void txDataComplete(A_UINT32 devNum, A_UINT32 timeout, A_UINT32 remoteStats);

MANLIB_API void rxDataSetup(A_UINT32 devNum, A_UINT32 numDesc, A_UINT32 dataBodyLength, A_UINT32 enablePPM);
MANLIB_API void rxDataSetupFixedNumber(A_UINT32 devNum, A_UINT32 numDesc, A_UINT32 dataBodyLength, A_UINT32 enablePPM);
MANLIB_API void rxDataBegin(A_UINT32 devNum, A_UINT32 waitTime, A_UINT32 timeout, A_UINT32 remoteStats,
				A_UINT32 enableCompare, A_UCHAR *dataPattern, A_UINT32 dataPatternLength);
MANLIB_API void rxDataBeginSG(A_UINT32 devNum, A_UINT32 waitTime, A_UINT32 timeout, A_UINT32 remoteStats,
				A_UINT32 enableCompare, A_UCHAR *dataPattern, A_UINT32 dataPatternLength, A_UINT32 sgpacketnumber);
MANLIB_API void cleanupTxRxMemory(A_UINT32 devNum, A_UINT32 flags);
MANLIB_API void rxDataStart(A_UINT32 devNum);
MANLIB_API void rxDataComplete(A_UINT32 devNum, A_UINT32 waitTime, A_UINT32 timeout, A_UINT32 remoteStats,
				A_UINT32 enableCompare, A_UCHAR *dataPattern, A_UINT32 dataPatternLength);
MANLIB_API void rxDataCompleteSG(A_UINT32 devNum, A_UINT32 waitTime, A_UINT32 timeout, A_UINT32 remoteStats,
				A_UINT32 enableCompare, A_UCHAR *dataPattern, A_UINT32 dataPatternLength, A_UINT32 sgpacketnumber);
MANLIB_API void rxDataBeginFixedNumber(A_UINT32 devNum, A_UINT32 timeout, A_UINT32 remoteStats,
				A_UINT32 enableCompare, A_UCHAR *dataPattern, A_UINT32 dataPatternLength);
MANLIB_API void rxDataCompleteFixedNumber(A_UINT32 devNum, A_UINT32 timeout, A_UINT32 remoteStats,
				A_UINT32 enableCompare, A_UCHAR *dataPattern, A_UINT32 dataPatternLength);
MANLIB_API void txrxDataBegin(A_UINT32 devNum, A_UINT32 waitTime, A_UINT32 timeout, A_UINT32 remoteStats,
				A_UINT32 enableCompare, A_UCHAR *dataPattern, A_UINT32 dataPatternLength);
MANLIB_API void txGetStats(A_UINT32 devNum, A_UINT32 rateInMb, A_UINT32 remote, TX_STATS_STRUCT *pReturnStats);
MANLIB_API void rxGetStats(A_UINT32 devNum, A_UINT32 rateInMb, A_UINT32 remote, RX_STATS_STRUCT *pReturnStats);
MANLIB_API void rxGetData(A_UINT32 devNum, A_UINT32 bufferNum, A_UCHAR *pReturnBuffer, A_UINT32 sizeBuffer);
MANLIB_API void enableWep(A_UINT32 devNum, A_UCHAR key);
MANLIB_API A_BOOL testLib(A_UINT32 devNum, A_UINT32 timeout);
MANLIB_API void enablePAPreDist(A_UINT32 devNum, A_UINT16 rate, A_UINT32 power);
MANLIB_API void forcePowerTxMax (A_UINT32 devNum, A_INT16 *pRatesPower);
MANLIB_API void forceChainPowerTxMax (A_UINT32 devNum, A_INT16 *pRatesPower, A_UINT32 chainNum);
MANLIB_API void forcePowerTx_Venice_Hack(A_UINT32 devNum, A_BOOL maxRange);
MANLIB_API A_INT32 getMaxLinPowerx4(A_UINT32 devNum);

MANLIB_API void forceSinglePowerTxMax(A_UINT32 devNum, A_INT16 powerValue);
MANLIB_API void ForceSinglePCDACTable(A_UINT32 devNum, A_UINT16 pcdac, A_INT32 offset);
MANLIB_API void ForceSinglePCDACTableGriffin(A_UINT32 devNum, A_UINT16 pcdac, A_UINT16 offset);
MANLIB_API void ForceSinglePCDACTableMercury(A_UINT32 devNum, A_UINT16 pcdac, A_UINT16 offset);
MANLIB_API A_BOOL generateTxGainTblFromCfg(A_UINT32 devNum);
MANLIB_API void forcePCDACTable (A_UINT32 devNum, A_UINT16 *pPcdacs);
MANLIB_API void forceChainPCDACTable (A_UINT32	devNum, A_UINT16 *pPcdacs, A_UINT32 chainNum);
MANLIB_API void getMacAddr(A_UINT32 devNum, A_UINT16 wmac, A_UINT16 instNo, A_UINT8 *macAddr);
MANLIB_API void getEepromStruct(A_UINT32 devNum,A_UINT16 eepStructFlag,	void **ppReturnStruct,A_UINT32 *pNumBytes);
MANLIB_API void getDeviceInfo(A_UINT32 devNum, SUB_DEV_INFO *pInfoStruct);
MANLIB_API void getMdkErrStr(A_UINT32 devNum, A_CHAR *pBuffer);
MANLIB_API A_INT32 getMdkErrNo(A_UINT32 devNum);
MANLIB_API A_BOOL rxLastDescStatsSnapshot(A_UINT32 devNum, RX_STATS_SNAPSHOT *pRxStats);
#ifndef MDK_AP
MANLIB_API void enableLogging(A_CHAR *);
MANLIB_API void disableLogging(void);
#endif

// Continuous Transmit Functions
MANLIB_API void txContBegin(A_UINT32 devNum, A_UINT32 type, A_UINT32 typeOption1,
							A_UINT32 typeOption2, A_UINT32 antenna);
MANLIB_API void txContFrameBegin(A_UINT32 devNum, A_UINT32 length, A_UINT32 ifswait,
                                  A_UINT32 typeOption1, A_UINT32 typeOption2, A_UINT32 antenna,
								   A_BOOL   performStabilizePower, A_UINT32 numDescriptors, A_UCHAR *dest);
MANLIB_API void txContEnd(A_UINT32 devNum);
MANLIB_API void devlibCleanup();
MANLIB_API A_UINT16 getDevIndex(A_UINT32 devNum);

MANLIB_API void setQueue
(
	A_UINT32 devNum, 
	A_UINT32 qcuNumber
);

MANLIB_API void mapQueue
(
	A_UINT32 devNum, 
	A_UINT32 qcuNumber,
	A_UINT32 dcuNumber
);

MANLIB_API void clearKeyCache
(
	A_UINT32 devNum
);

MANLIB_API A_INT32 getFieldForMode
(
 A_UINT32 devNum,
 A_CHAR   *fieldName,
 A_UINT32  mode,			//desired mode 
 A_UINT32  turbo		//Flag for base or turbo value
);

MANLIB_API void changeMultipleFieldsAllModes
(
 A_UINT32		  devNum,
 PARSE_MODE_INFO *pFieldsToChange,
 A_UINT32		  numFields
);

MANLIB_API void changeRegValueField
(
 A_UINT32 devNum,
 A_CHAR *fieldName, 
 A_UINT32 newValue
);

MANLIB_API void changeMultipleFields
(
 A_UINT32		  devNum,
 PARSE_FIELD_INFO *pFieldsToChange,
 A_UINT32		  numFields
);

MANLIB_API A_INT16 getMaxPowerForRate
(
 A_UINT32 devNum,
 A_UINT32 freq,
 A_UINT32 rate
);

MANLIB_API A_UINT16 getXpdgainForPower
(
 A_UINT32	devNum,
 A_INT32    powerIn
);

MANLIB_API A_UINT16 getPcdacForPower
(
	A_UINT32 devNum,
	A_UINT32 freq,
	A_INT32 twicePower
);

MANLIB_API A_UINT16 getPowerIndex
(
	A_UINT32 devNum,
	A_INT32 twicePower
);

MANLIB_API A_UINT16 getInterpolatedValue 
(
 A_UINT16	target,
 A_UINT16	srcLeft,
 A_UINT16	srcRight,
 A_UINT16	targetLeft,
 A_UINT16	targetRight,
 A_BOOL		scaleUp
);

MANLIB_API A_UINT32 getArtAniLevel
(
	A_UINT32 devNum,
	A_UINT32 artAniType
);

MANLIB_API void setArtAniLevel
(
	A_UINT32 devNum,
	A_UINT32 artAniType,
	A_UINT32 artAniLevel
);

MANLIB_API void setChain
(
	A_UINT32 devNum,
	A_UINT32 chain,
	A_UINT32 phase
);

MANLIB_API A_BOOL updateMacAddrAR5513 
(
	A_UINT32 devNum,
	A_UINT8  wmacAddr[6] // wmac addr [0]-->lsb, [5]-->msb
);

MANLIB_API A_BOOL updateSectorChunkAR5513 
(
	A_UINT32 devNum,
	A_UINT32 *pData, // intended for pci config data
	A_UINT32 sizeData,
	A_UINT32 sectorNum,
	A_UINT32 dataOffset
);

MANLIB_API A_BOOL updateSingleEepromValueAR5513 
(
	A_UINT32 devNum,
	A_UINT32 address,
	A_UINT32 value
);

MANLIB_API A_BOOL flashWriteSectorAr5513
(
 A_UINT32 devNum, 
 A_UINT32 sectorNum,
 A_UINT32 *retList,
 A_UINT32 retListSize
);

MANLIB_API A_BOOL flashReadSectorAr5513
(
 A_UINT32 devNum, 
 A_UINT32 sectorNum,
 A_UINT32 *retList,
 A_UINT32 retListSize
);


MANLIB_API A_UINT32 getPhaseCal
(
 A_UINT32 devNum,
 A_UINT32 freq
);

MANLIB_API void supplyFalseDetectbackoff
(
	A_UINT32 devNum,
	A_UINT32 *pBackoffValues
);

MANLIB_API void configureLibParams
(
 A_UINT32 devNum,
 LIB_PARAMS *pLibParamsInfo
);

MANLIB_API void set_dev_nums(A_UINT32 gold_dev, A_UINT32 dut_dev) ;
MANLIB_API void force_minccapwr (A_UINT32 maxccapwr) ;
MANLIB_API double detect_signal (A_UINT32 desc_cnt, A_UINT32 adc_des_size, A_UINT32 mode, A_UINT32 *gain) ;
MANLIB_API void config_capture (A_UINT32 dut_dev, A_UCHAR *RX_ID, A_UCHAR *BSS_ID, A_UINT32 channel, A_UINT32 turbo, A_UINT32 *gain, A_UINT32 mode) ;
MANLIB_API A_UINT32 trigger_sweep (A_UINT32 dut_dev, A_UINT32 channel, A_UINT32 mode, A_UINT32 averages, A_UINT32 path_loss, A_BOOL return_spectrum, double *psd) ;
MANLIB_API void
getCtlPowerInfo
(
 A_UINT32 devNum,
 CTL_POWER_INFO *pCtlStruct	
);

MANLIB_API A_BOOL isFalconEmul(A_UINT32 devNum);
MANLIB_API A_BOOL isFalcon(A_UINT32 devNum);
MANLIB_API A_BOOL large_pci_addresses(A_UINT32 devNum);
MANLIB_API A_BOOL isFalconDeviceID(A_UINT32 swDevID) ;

MANLIB_API A_BOOL needsUartPciCfg(A_UINT32 swDeviceID);
MANLIB_API A_BOOL isPredator(A_UINT32 swDeviceID);
MANLIB_API A_BOOL isGriffin(A_UINT32 swDeviceID);
MANLIB_API A_BOOL isGriffin_1_0(A_UINT32 macRev);
MANLIB_API A_BOOL isGriffin_1_1(A_UINT32 macRev);
MANLIB_API A_BOOL isGriffin_2_0(A_UINT32 macRev);
MANLIB_API A_BOOL isGriffin_lite(A_UINT32 macRev);
MANLIB_API A_BOOL isGriffin_2_1(A_UINT32 macRev);
MANLIB_API A_BOOL isEagle(A_UINT32 swDeviceID);
MANLIB_API A_BOOL isEagle_1_0(A_UINT32 macRev);
MANLIB_API A_BOOL isEagle_lite(A_UINT32 macRev);
MANLIB_API A_BOOL isCondor(A_UINT32 swDeviceID);
MANLIB_API A_BOOL isSpider(A_UINT32 swDeviceID);
MANLIB_API A_BOOL isCobra(A_UINT32 swDeviceID);
MANLIB_API A_BOOL isDragon(A_UINT32 devNum);
MANLIB_API A_BOOL isDragon_sd(A_UINT32 swDevID);
MANLIB_API A_BOOL isMercury(A_UINT32 devNum);
MANLIB_API A_BOOL isMercury2_0(A_UINT32 devNum);
MANLIB_API A_BOOL isMercury_sd(A_UINT32 swDevID);
MANLIB_API A_BOOL isMercuryLNA2(A_UINT32 subsystemID);
MANLIB_API void setKeepAGCDisable(void);
MANLIB_API void dumpRegs(A_UINT32 devNum, char *fileName);

MANLIB_API void clearKeepAGCDisable(void);

MANLIB_API A_UINT16 getEARCalAtChannel(A_UINT32 devNum, A_BOOL atCal, A_UINT16 channel, A_UINT32 *word, A_UINT16 xpd_mask, A_UINT32 version_mask);
MANLIB_API void memDisplay(A_UINT32 devNum, A_UINT32 address, A_UINT32 nWords);
MANLIB_API A_UINT32 memAlloc(A_UINT32 devNum, A_UINT32 numBytes);
MANLIB_API void memFree(A_UINT32 devNum, A_UINT32 physAddress);
MANLIB_API void memFreeAll(A_UINT32 devNum);

//for cus99
MANLIB_API void setPowerRef(A_INT32 __PowerOffset);
MANLIB_API A_INT32 getPowerRef(void);
MANLIB_API void setZerodBm(A_UINT32 flag0dBm);
MANLIB_API A_UINT32 isZerodBm(void);

extern MANLIB_API A_UINT32  enableCal;
#if defined(AR6002)
#ifdef _WINDOWS
extern MANLIB_API A_UINT8 dummyEepromWriteArea[AR6K_EEPROM_SIZE]; 
#endif
#endif

//extern MANLIB_API A_UINT32 checkSumLength;
//extern MANLIB_API A_UINT32 eepromSize;


#endif // #ifndef SWIG


#ifdef __cplusplus
}
#endif

#endif // #define __INCmanlibh

