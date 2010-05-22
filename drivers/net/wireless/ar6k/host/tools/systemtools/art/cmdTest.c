/* cmdTest.c - contains more tests for ART can be run from command line or within ART */

/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */
#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/cmdTest.c#2 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/cmdTest.c#2 $"

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
 #include <dir.h>
#endif	// #ifdef __ATH_DJGPPDOS__

#ifdef _WINDOWS
 #include <windows.h>
#endif
#include "common_hw.h"
#ifdef JUNGO
#include "mld.h"
#endif

#if defined(LINUX) || defined(__linux__)
#include "linux_ansi.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifndef LINUX
#include <conio.h>
#include <io.h>
#endif
#include <string.h>
#include <ctype.h>
#include "wlantype.h"
#include "wlanproto.h"
#if defined(LINUX) || defined(__linux__)
#include "linuxdrv.h"
#else
#include "ntdrv.h"
#endif
#include "athreg.h"
#include "manlib.h"     /* The Manufacturing Library */
#include "manlibInst.h" /* The Manufacturing Library Instrument Library extension */

#include "art_if.h"
#include "pci.h"        /* PCI Config Space definitions */
#include "test.h"
#include "dynamic_optimizations.h"
#include "maui_cal.h"
#include "parse.h"
#include "dk_cmds.h"
#include "cmdTest.h"

#ifndef __ATH_DJGPPDOS__
#include "MLIBif.h"     /* Manufacturing Library low level driver support functions */
#else
#include "mlibif_dos.h"
#endif

#if defined(LINUX) || defined(__linux__)
#include <unistd.h>
#endif

static A_UCHAR  bssID[6]     = {0x50, 0x55, 0x55, 0x55, 0x55, 0x05};
static A_UCHAR  goldenStation[6] = {0x10, 0x11, 0x11, 0x11, 0x11, 0x01};	// DUT
static A_UCHAR  dutStation[6] = {0x20, 0x22, 0x22, 0x22, 0x22, 0x02};	// Golden
static A_UINT32 globalTestMask;

static void goldenSetup(A_UINT32 devNum);
static A_UINT32 transmitTest(A_UINT32 devNum, A_UINT32 testChannel, A_UINT32 testMode, A_UINT32 turbo, A_UINT16 antenna, A_UINT16 goldAntenna);
static A_UINT32 receiveTest(A_UINT32 devNum, A_UINT32 testChannel, A_UINT32 testMode, A_UINT32 turbo, A_UINT16 antenna, A_UINT16 goldAntenna);
static A_UINT32 broadcastTest(A_UINT32 devNum, A_UINT32 testChannel, A_UINT32 testMode, A_UINT32 turbo, A_UINT16 antenna);
static A_UINT32 throughputUpTest(A_UINT32 devNum, A_UINT32 testChannel, A_UINT32 testMode, A_UINT32 turbo, A_UINT16 antenna, A_UINT16 goldAntenna);
static A_UINT32 throughputDownTest(A_UINT32 devNum, A_UINT32 testChannel, A_UINT32 testMode, A_UINT32 turbo, A_UINT16 antenna, A_UINT16 goldAntenna);
static void printTestMenu(void);
static void dutSendWakeupCall(A_UINT32 devNum, A_UINT32 testType, A_UINT32 testChannel, A_UINT32 testMode, A_UINT32 turbo, A_UINT16 goldAntenna, A_UINT32 miscParam);
static void goldenWait4WakeupCall(A_UINT32 devNum, TEST_INFO_STRUCT *pTestInfo);
static void goldenCompleteWakeupCall(A_UINT32 devNum);
static void printFailures(A_UINT32 reasonFailMask);
static void printFailLetters(A_UINT32 reasonFailMask);
static A_UINT32 analyzeLinkResults( A_UINT32 devNum, A_UINT32 rateMask, A_UINT32 testMode, A_UINT32 turbo, A_BOOL remoteStats, A_UINT16 antenna);
static A_UINT32 analyzeThroughputResults(A_UINT32 devNum, A_UINT32 rateIndex, A_UINT32 testMode, A_UINT32 turbo, A_BOOL remoteStats, 
										 A_UINT16 antenna, A_UINT32 testChannel, A_CHAR *testString);
static A_UINT32 printTestSummary(A_UINT32 reasonFailMask);
static A_BOOL parseTestFile(void); 
static A_BOOL printFooter(void);
static A_BOOL prepareRangeLogging(void);
static A_CHAR   goldAntString[20];

extern A_BOOL thin_client;
extern WLAN_MACADDR  macAddr;
extern const A_CHAR  *DataRateStr[];
extern const A_CHAR  *DataRate_11b[];
static FILE *rangeLogFileHandle;
const A_CHAR  *DataRateShortStr[] = {"   6", "   9", "  12", "  18", "  24", "  36", "  48", "  54", 
									 "  1L", "  2L", "  2S", "5.5L", "5.5S", " 11L", " 11S"};
const A_CHAR  *DataRateShortStr_11b[] = {"  1L", "  1L", "  2L", "  2S", "5.5L", "5.5S", " 11L", " 11S"};

TEST_CONFIG testSetup =
{
	5,							//num iterations
	100,						//num packets
	800,						//num packets throughput
	100,						//num packets throughput CCK
	1000,						//packet size
	1500,                       //throughput packet size
	7,							//throughput data rate index
	90,							//PER Pass threshold
	-9,							//PPM Min
	9,							//PPM Max
	40,							//RSSI threshold 11a ant A 
	40,							//RSSI threshold 11a ant B
	40,							//RSSI threshold 11b ant A
	40,							//RSSI threshold 11b ant B
	40,							//RSSI threshold 11g ant A
	40,							//RSSI threshold 11g ant B
	50,							//max allowed CRC
	10000,						//beacon timeout (milliseconds)
	20.0,						//throughput threshold 11a
	10.0,						//throughput threshold 11b
	10.0,						//throughput threshold 11g
	750,						//throughput PER (num packets)	
	90,						    //throughput 11b PER (num packets)	
	"",							//MACaddr min
	"",							//MACaddr mac
	5140,						//5G side channel
	2302,						//2G side channel
	'N',						//DUT Orientation
	'N',						//AP Orientation
};


A_UINT32 performCmdLineTests
(
 A_UINT32 devNum
)
{
	A_UINT16 i, j, k;
	A_UINT16 antennaMask = configSetup.antennaMask;
	A_UINT16 goldAntennaMask = configSetup.goldAntennaMask;
	A_UINT16 numAntenna = 1;
	A_UINT16 numGoldAntenna = 1;
	char  *modeStr[] = {"11a", "11g", "11b", "OFDM@2.4"}; 
	A_UINT16 antennaSelected = DESC_ANT_A;
	A_UINT16 goldAntenna;
	A_UINT32 lastErrorCode;
	A_UINT32 eepromValue;

	globalTestMask = 0;
	if(!parseTestFile()) {
		return ERR_TEST_SETUP;
	}
	
	//if the golden test mask is set, this will be the only test performed
	if(configSetup.cmdLineTestMask & GOLDEN_TEST_MASK) {
		uiPrintf("Setting up for golden, this will be the only test operation performed\n");
		goldenSetup(devNum);
		return ERR_TEST_SETUP;
	}

	if(configSetup.cmdLineTestMask & MAC_ADDR_MASK) {
		if(!testMacAddress(&macAddr)) {
			uiPrintf("MAC address is outside of comparison range.  Error code %d\n", ERR_MAC_ADDR_MISMATCH);
			return (ERR_MAC_ADDR_MISMATCH);
		}
		else {
			uiPrintf("MAC Address Test PASS, return code 0\n");
		}
	}

	//check to see if the user entered any channels on the command line, if not, then
	//set one up with the defaults.
	if(configSetup.numListChannels == 0) {
		//create one.
		configSetup.testChannelList = (TEST_CHANNEL_INFO *)malloc(sizeof(TEST_CHANNEL_INFO));
		if(!configSetup.testChannelList) {
			uiPrintf("Unable to allocate memory for testChannel list\n");
			return ERR_SYSTEM;
		}
		
		configSetup.testChannelList->channel = configSetup.channel;
		configSetup.testChannelList->mode = configSetup.mode;
		configSetup.testChannelList->turbo = configSetup.turbo;
		configSetup.numListChannels = 1;
	}

	//num antenna is either 1 (ie user selected one, or we use the default) or we want to test both
	if(antennaMask == (ANTENNA_A_MASK | ANTENNA_B_MASK)) {
		numAntenna = 2;
	}
	if(goldAntennaMask == (ANTENNA_A_MASK | ANTENNA_B_MASK)) {
		numGoldAntenna = 2;
	}
	if(configSetup.rangeLogging) {
		if(!prepareRangeLogging()) {
			uiPrintf("Unable to prepare for range logging \n");
			return ERR_TEST_SETUP;
		}
	}

	for (i = 0; i < configSetup.numListChannels; i++) {
		goldAntennaMask = configSetup.goldAntennaMask;
		for(k = 0; k < numGoldAntenna; k++) {
			if (goldAntennaMask & ANTENNA_A_MASK) {
				goldAntenna = USE_DESC_ANT|DESC_ANT_A;
				goldAntennaMask = goldAntennaMask & ~ANTENNA_A_MASK;
				strcpy(goldAntString, "A");
			}
			else if (goldAntennaMask & ANTENNA_B_MASK) {
				goldAntenna = USE_DESC_ANT|DESC_ANT_B;
				goldAntennaMask = goldAntennaMask & ~ANTENNA_B_MASK;
				strcpy(goldAntString, "B");
			}
			else {
				//have the golden unit use the default antenna
				goldAntenna = 0;
				strcpy(goldAntString, "default");
			}
    		
			//reinitialize the dut antennaMask
			antennaMask = configSetup.antennaMask;
			for(j = 0; j < numAntenna; j++) {
				if (antennaMask & ANTENNA_A_MASK) {
					configSetup.antenna = USE_DESC_ANT|DESC_ANT_A;
					antennaMask = antennaMask & ~ANTENNA_A_MASK;
					antennaSelected = DESC_ANT_A;
				}
				else if (antennaMask & ANTENNA_B_MASK) {
					configSetup.antenna = USE_DESC_ANT|DESC_ANT_B;
					antennaMask = antennaMask & ~ANTENNA_B_MASK;
					antennaSelected = DESC_ANT_B;
				}
				//perform the tests that have masks set
				if(configSetup.cmdLineTestMask & TX_TEST_MASK) {
					uiPrintf("\n\n===========================================================================\n");
					uiPrintf("Performing transmit test on DUT Antenna %c, golden Antenna %s at Channel %d in mode %s %s\n",
						(configSetup.antenna==(USE_DESC_ANT|DESC_ANT_A)) ? 'A' : 'B',
						goldAntString,
						configSetup.testChannelList[i].channel, modeStr[configSetup.testChannelList[i].mode],
						(configSetup.testChannelList[i].turbo == TURBO_ENABLE) ? "turbo" : "" );
					globalTestMask |= transmitTest(devNum, configSetup.testChannelList[i].channel, configSetup.
						testChannelList[i].mode, configSetup.testChannelList[i].turbo, antennaSelected, goldAntenna);
					Sleep(1000);
				}

				if(configSetup.cmdLineTestMask & RX_TEST_MASK) {
					uiPrintf("\n\n===========================================================================\n");
					uiPrintf("Performing receive test on DUT Antenna %c, golden Antenna %s at Channel %d in mode %s %s\n",
						(configSetup.antenna==(USE_DESC_ANT|DESC_ANT_A)) ? 'A' : 'B',
						goldAntString,
						configSetup.testChannelList[i].channel, modeStr[configSetup.testChannelList[i].mode],
						(configSetup.testChannelList[i].turbo == TURBO_ENABLE) ? "turbo" : "" );
					globalTestMask |= receiveTest(devNum, configSetup.testChannelList[i].channel, 
						configSetup.testChannelList[i].mode, configSetup.testChannelList[i].turbo, antennaSelected, goldAntenna);
					Sleep(1000);
				}

				if(configSetup.cmdLineTestMask & BEACON_TEST_MASK) {
					uiPrintf("\n\n===========================================================================\n");
					uiPrintf("Performing beacon receive test on DUT Antenna %c, golden Antenna %s at Channel %d in mode %s %s\n",
						(configSetup.antenna==(USE_DESC_ANT|DESC_ANT_A)) ? 'A' : 'B',
						goldAntString,
						configSetup.testChannelList[i].channel, modeStr[configSetup.testChannelList[i].mode],
						(configSetup.testChannelList[i].turbo == TURBO_ENABLE) ? "turbo" : "" );
					globalTestMask |= broadcastTest(devNum, configSetup.testChannelList[i].channel, 
						configSetup.testChannelList[i].mode, configSetup.testChannelList[i].turbo, antennaSelected);
					Sleep(1000);
				
				}
				if(configSetup.cmdLineTestMask & TP_TEST_UP_MASK) {
					uiPrintf("\n\n===========================================================================\n");
					uiPrintf("Performing throughput uplink test on DUT Antenna %c, golden Antenna %s at Channel %d in mode %s %s\n",
						(configSetup.antenna==(USE_DESC_ANT|DESC_ANT_A)) ? 'A' : 'B',
						goldAntString,
						configSetup.testChannelList[i].channel, modeStr[configSetup.testChannelList[i].mode],
						(configSetup.testChannelList[i].turbo == TURBO_ENABLE) ? "turbo" : "" );
					globalTestMask |= throughputUpTest(devNum, configSetup.testChannelList[i].channel, 
						configSetup.testChannelList[i].mode, configSetup.testChannelList[i].turbo, antennaSelected, goldAntenna);
//###					Sleep(1000);
				
				}
				if(configSetup.cmdLineTestMask & TP_TEST_DOWN_MASK) {
					uiPrintf("\n\n===========================================================================\n");
					uiPrintf("Performing throughput downlink test on DUT Antenna %c, golden Antenna %s at Channel %d in mode %s %s\n",
						(configSetup.antenna==(USE_DESC_ANT|DESC_ANT_A)) ? 'A' : 'B',
						goldAntString,
						configSetup.testChannelList[i].channel, modeStr[configSetup.testChannelList[i].mode],
						(configSetup.testChannelList[i].turbo == TURBO_ENABLE) ? "turbo" : "" );
					globalTestMask |= throughputDownTest(devNum, configSetup.testChannelList[i].channel, 
						configSetup.testChannelList[i].mode, configSetup.testChannelList[i].turbo, antennaSelected, goldAntenna);
//###					Sleep(1000);
				
				}
				if(configSetup.cmdLineTestMask & BACKUP_EEPROM_MASK) {
					if(!backupEeprom(devNum, configSetup.eepBackupFilename)) {
						uiPrintf("EEPROM Backup Failed\n");
						globalTestMask |= MASK_EEPROM_BACKUP_FAIL;
					}
				}
				if(configSetup.cmdLineTestMask & RESTORE_EEPROM_MASK) {
					if(!restoreEeprom(devNum, configSetup.eepRestoreFilename)) {
						uiPrintf("EEPROM Restore Failed\n");
						globalTestMask |= MASK_EEPROM_RESTORE_FAIL;
					}
				}
				if(configSetup.cmdLineTestMask & EEP_COMPARE_MASK) {
					eepromValue = art_eepromRead(devNum, testSetup.eepromCompareSingleLocation);
					if( eepromValue != configSetup.eepromCompareSingleValue) {
						uiPrintf("EEPROM location 0x%04x contains 0x%04x (expected 0x%04x)\n",
							testSetup.eepromCompareSingleLocation, eepromValue,
							configSetup.eepromCompareSingleValue);
						globalTestMask |= MASK_EEPROM_COMP_VALUE_FAIL;
					}
				}

			}
		}
	}
	lastErrorCode = printTestSummary(globalTestMask);
	return (lastErrorCode);
}



//Golden unit will sit in loop waiting for a wakeup call from dut then setup the test
void goldenSetup
(
 A_UINT32 devNum
)
{
	TEST_INFO_STRUCT testInfo;
	A_UINT32 waitTime = 10000;
	A_UINT32 timeOut  = 20000;
	A_UINT32 i;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 stats_mode = ENABLE_STATS_SEND ; //| ENABLE_STATS_RECEIVE;
	A_UINT32 broadcast = 1;
	A_BOOL   enablePPM = 1;
	A_UINT32 origAntenna = configSetup.antenna;
	A_UINT16 numRates;
	A_UINT32 rateMask;

	uiPrintf("\nGolden Unit Program is running. Hit any key to quit...\n");
	while(!kbhit()) {
		goldenWait4WakeupCall(devNum, &testInfo);
		if(kbhit())
			break;
		if((testInfo.mode == MODE_11B) || ((testInfo.mode == MODE_11G) && ((swDeviceID & 0xff) >= 0x0013))) {
			enablePPM = 0;
		}
		if(testInfo.testType > THROUGHPUT_DOWN_TEST) {
			uiPrintf("Illegal test type in sync packet, wait for another\n");
			continue;
		}
		//setup the test 
		if(!configSetup.enableXR) {
			goldenCompleteWakeupCall(devNum);
		}
#if 0
		uiPrintf("Num Iterations = %d\n", testInfo.numIterations);
		uiPrintf("Num Packets = %d\n", testInfo.numPackets);
		uiPrintf("Packet size = %d\n", testInfo.packetSize);
		uiPrintf("Channel = %d\n", testInfo.channel);
		uiPrintf("Test type = %d\n", testInfo.testType);
		uiPrintf("Rate mask = %x\n", testInfo.rateMask);
		uiPrintf("Mode = %d\n", testInfo.mode);
#endif //_DEBUG
		//setup the golden antenna if needed
		if(testInfo.goldAntenna) {
			configSetup.antenna = testInfo.goldAntenna;
		}

		//set the power control if needed
		if(testInfo.txPower != USE_TARGET_POWER) {
			configSetup.useTargetPower = 0;
			configSetup.powerOutput = (A_UINT16)testInfo.txPower;
		}

		art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)testInfo.mode, configSetup.use_init);		
		art_resetDevice(devNum, goldenStation, bssID, testInfo.channel, testInfo.turbo);
		switch(testInfo.testType) {
		case TRANSMIT_TEST:
			art_resetDevice(devNum, goldenStation, bssID, testInfo.channel, testInfo.turbo);
			art_setAntenna(devNum, configSetup.antenna);
			break;

		case RECEIVE_TEST:
			//doing all the receive stuff below
			break;

		case THROUGHPUT_UP_TEST:
			art_resetDevice(devNum, goldenStation, bssID, testInfo.channel, testInfo.turbo);
			art_setAntenna(devNum, configSetup.antenna);
			break;

		case THROUGHPUT_DOWN_TEST:		
			//do nothing here
			break;
	
		default:
			uiPrintf("Fatal Error: Illegal TestType, should be trapped before this\n");
			return;
		}

		for(i = 0; i < testInfo.numIterations; i++) {
			switch(testInfo.testType) {
			//perform/complete transmit test. complete receive test here
			case TRANSMIT_TEST:		
				if(testInfo.rateMask > 0xff) {
					numRates = 15;
				}
				else {
					numRates = 8;
				}

				art_rxDataSetup(devNum, (testInfo.numPackets * numRates) + 40, testInfo.packetSize, enablePPM);
				art_rxDataStart(devNum);
				art_rxDataComplete(devNum, waitTime, timeOut, stats_mode, 0, pattern, 2);
				if (art_mdkErrNo!=0) {
					uiPrintf("Error: unable to receive packets from DUT\n");
				}
				break;
			case RECEIVE_TEST:
				Sleep(2000);
				art_resetDevice(devNum, goldenStation, bssID, testInfo.channel, testInfo.turbo);
				
				//set power if needed
				forcePowerOrPcdac(devNum);

				art_txDataSetup(devNum, testInfo.rateMask, dutStation, 
					testInfo.numPackets, testInfo.packetSize, pattern, 2, 0, configSetup.antenna, 0);
				art_txDataBegin(devNum, timeOut, 0);			
				if (art_mdkErrNo!=0) {
					uiPrintf("Transmit Iteration Failed - error %d\n", art_mdkErrNo);
					continue;
				}
				break;
			case THROUGHPUT_UP_TEST:		
//				Sleep(1000);
				//calculate how many rates
				rateMask = 0x80000;
				numRates = 0;
				while(rateMask) {
					if(rateMask & testInfo.rateMask) {
						numRates++;
					}
					rateMask >>= 1;
				}
				art_rxDataSetup(devNum, (testInfo.numPackets * numRates) + 40, testInfo.packetSize, 0);
				art_rxDataStart(devNum);
				art_rxDataComplete(devNum, waitTime, timeOut, stats_mode, 0, pattern, 2);
				if (art_mdkErrNo!=0) {
					uiPrintf("Error: unable to receive packets from DUT\n");
				}
//				Sleep(200);
				break;
			case THROUGHPUT_DOWN_TEST:		
				Sleep(2000);	// delay between packet sends
				art_resetDevice(devNum, goldenStation, bssID, testInfo.channel, testInfo.turbo);

				//set power if needed
				forcePowerOrPcdac(devNum);

				art_txDataSetup(devNum, testInfo.rateMask | RATE_GROUP, dutStation, 
					testInfo.numPackets, testInfo.packetSize, pattern, 2, 0, configSetup.antenna, 0);
				art_txDataBegin(devNum, timeOut, ENABLE_STATS_SEND);				
//				Sleep(200);
				break;
			}
			//cleanup descriptor queues, to free up mem
			art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);

		} //end for numIterations
		//restore the mode and antenna back to default
		art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		
		configSetup.antenna = origAntenna;

		//revert back to target power, in case DUT turns it back off in next test.
		configSetup.useTargetPower = 1;

	} //end while !kbhit

    while (kbhit())	{	// clean up the buffer
        getch();
    }
}

A_UINT32 transmitTest
(
 A_UINT32 devNum,
 A_UINT32 testChannel,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_UINT16 antenna,
 A_UINT16 goldAntenna
)
{
	// rxDataSetup
	A_UINT32 num_rx_desc = 50;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;
	A_UINT32 stats_mode =  ENABLE_STATS_RECEIVE; 

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 10000;
	A_UINT32 compare = 0;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 broadcast = 1;
	
	A_UINT32 i;
	A_UINT32 reasonFailMask = 0;
	A_UINT32 iterationFailMask = 0;
	A_UINT32 rateMask = configSetup.rateMask;

	//send wakeup call to golden unit
	dutSendWakeupCall(devNum, TRANSMIT_TEST, testChannel, testMode, turbo, goldAntenna, 0);
//	Sleep(1000);  //need to give the golden unit time to setup the receive

	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)testMode, configSetup.use_init);		
	art_resetDevice(devNum, dutStation, bssID, testChannel, turbo);

	//start the transmit test, golden should already be in receive mode by now
	if(testMode == MODE_11B) {
		uiPrintf("\n         1Mb/s   2Mb/s   2Mb/s  5.5Mb/s 5.5Mb/s 11Mb/s  11Mb/s \n"); 
		uiPrintf("Preamble: long   long    short  long    short   long    short   CRCs  Result\n"); 
		rateMask = rateMask & 0xfd;
	} 
	else if ((testMode == MODE_11G) &&
		((swDeviceID & 0xff) >= 0x0013) && (turbo != TURBO_ENABLE) && (rateMask > 0xff)) {
		uiPrintf("\n  6   9   12  18  24  36  48  54  1L  2L  2S  5L  5S 11L 11S  CRCs Result\n"); 	
		uiPrintf("  --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---  ---- ------\n");
	}
	else {
		if(turbo == TURBO_ENABLE) {
			uiPrintf("\n12Mb/s  18Mb/s  24Mb/s  36Mb/s  48Mb/s  72Mb/s  96Mb/s  108Mb/s CRCs PPM Result\n"); 
		} 
		else if (turbo == HALF_SPEED_MODE) {
			uiPrintf("\n 3Mb/s 4.5Mb/s   6Mb/s   9Mb/s  12Mb/s  18Mb/s  24Mb/s  27Mb/s  CRCs PPM Result\n"); 	
		} else {
			uiPrintf("\n 6Mb/s   9Mb/s  12Mb/s  18Mb/s  24Mb/s  36Mb/s  48Mb/s  54Mb/s  CRCs PPM Result\n"); 	
		}
		rateMask = rateMask & 0xff;
	}

	for(i = 0; i < testSetup.numIterations; i++) {
//		uiPrintf("Starting transmit iteration number %d, out of %d\n", i, testSetup.numIterations);
		Sleep(1500);	// delay between packet sends
		art_resetDevice(devNum, dutStation, bssID, testChannel, turbo);
	
		//set the txPower if needed
		forcePowerOrPcdac(devNum);

		art_txDataSetup(devNum, rateMask, goldenStation, 
			testSetup.numPackets, testSetup.packetSize, pattern, 2, 0, configSetup.antenna, broadcast);
		art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);
		art_txrxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);			
		if (art_mdkErrNo!=0) {
			reasonFailMask |= MASK_COMMS_FAILURE;
			continue;
		}
		
		if(testMode == MODE_11B) {
			uiPrintf("        ");
		}

		iterationFailMask = analyzeLinkResults(devNum, rateMask, testMode, turbo, 1, antenna);

		if(iterationFailMask) {
			reasonFailMask |= iterationFailMask;
			uiPrintf(" X:");
			printFailLetters(iterationFailMask);
		}
		uiPrintf("\n");
	}
	
	if(reasonFailMask) {
		uiPrintf("\nTransmit Test FAIL, due to: \n");
		printFailures(reasonFailMask);
		uiPrintf("\n");
	}
	else {
		uiPrintf("\nTransmit Test Pass\n");
	}

	//restore the mode
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		
	return (reasonFailMask);
}



A_UINT32 receiveTest
(
 A_UINT32 devNum,
 A_UINT32 testChannel,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_UINT16 antenna,
 A_UINT16 goldAntenna
)
{
	A_UINT32 start_timeout = 5000;	
	A_UINT32 complete_timeout = 10000;
	A_UINT32 stats_mode = 0; 
	A_UINT32 compare = 0;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 i;
	A_UINT32 rateMask = configSetup.rateMask;
	A_UINT32 reasonFailMask = 0;
	A_UINT32 iterationFailMask = 0;
	A_BOOL   enablePPM = 1;
	A_UINT32 numRates = 8;

	if((testMode == MODE_11B) || ((testMode == MODE_11G) && ((swDeviceID & 0xff) >= 0x0013))) {
		enablePPM = 0;
	}

	//send wakeup call to golden unit, note this waits for reply from GU before exit
	dutSendWakeupCall(devNum, RECEIVE_TEST, testChannel, testMode, turbo, goldAntenna, 0);

	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)testMode, configSetup.use_init);		

	//start the receive test
	if(testMode == MODE_11B) {
		uiPrintf("\n         1Mb/s   2Mb/s   2Mb/s  5.5Mb/s 5.5Mb/s 11Mb/s  11Mb/s \n"); 
		uiPrintf("Preamble: long   long    short  long    short   long    short   CRCs Result\n"); 
		rateMask = rateMask & 0xfd;
	}
	else if ((testMode == MODE_11G) &&
		((swDeviceID & 0xff) >= 0x0013) && (turbo != TURBO_ENABLE) && (rateMask > 0xff)) {
		uiPrintf("\n  6   9   12  18  24  36  48  54  1L  2L  2S  5L  5S 11L 11S  CRCs Result\n"); 	
		uiPrintf("  --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---  ---- ------\n");
		numRates = 15;
	}
	else {
		if(turbo == TURBO_ENABLE) {
			uiPrintf("\n12Mb/s  18Mb/s  24Mb/s  36Mb/s  48Mb/s  72Mb/s  96Mb/s  108Mb/s CRCs PPM Result\n"); 
		} 
		else if (turbo == HALF_SPEED_MODE) {
			uiPrintf("\n 3Mb/s 4.5Mb/s   6Mb/s   9Mb/s  12Mb/s  18Mb/s  24Mb/s  27Mb/s  CRCs PPM Result\n"); 	
		} else {
			uiPrintf("\n 6Mb/s   9Mb/s  12Mb/s  18Mb/s  24Mb/s  36Mb/s  48Mb/s  54Mb/s  CRCs PPM Result\n"); 	
		}
		rateMask = rateMask & 0xff;
	}
	for(i = 0; i < testSetup.numIterations; i++) {
		art_resetDevice(devNum, dutStation, bssID, testChannel, turbo);
		art_setAntenna(devNum, configSetup.antenna);
		art_rxDataSetup(devNum, (testSetup.numPackets * numRates) + 40, testSetup.packetSize, enablePPM);
		art_rxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
		if (art_mdkErrNo!=0) {
			reasonFailMask |= MASK_COMMS_FAILURE;
			continue;
		}
		iterationFailMask = 0;
		if(testMode == MODE_11B) {
			uiPrintf("        ");
		}
		
		
		iterationFailMask = analyzeLinkResults(devNum, rateMask, testMode, turbo, 0, antenna);

		if(iterationFailMask) {
			reasonFailMask |= iterationFailMask;
			uiPrintf(" X:");
			printFailLetters(iterationFailMask);
		}
		uiPrintf("\n");
	}
	
	if(reasonFailMask) {
		uiPrintf("\nReceive Test FAIL, due to: \n");
		printFailures(reasonFailMask);
		uiPrintf("\n");
	}
	else {
		uiPrintf("\nReceive Test Pass, return code 0\n");
	}
	//restore the mode
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		
	return(reasonFailMask);
}


A_UINT32 analyzeLinkResults
(
 A_UINT32 devNum,
 A_UINT32 rateMask,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_BOOL remoteStats,
 A_UINT16 antenna
)
{
	A_UINT32 mask;
	A_UINT32 j;
    RX_STATS_STRUCT rStats;
	A_UINT32 reasonFailMask = 0;
	A_INT32 rssiThreshold;
	A_UINT16 antennaMaskOffset = 0;
	A_UINT32 numLoops = 8;
	A_UINT32 startLoop = 0;

	if(antenna == DESC_ANT_B) {
		//shift masks by 1 to get to the B antenna mask
		antennaMaskOffset = 1;
	}

	switch(testMode) {
	case MODE_11A:
		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11a_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11a_antB;
			break;
		} //end switch antenna
		break;

	case MODE_11B:
		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11b_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11b_antB;
			break;
		} //end switch antenna
		break;
	
	case MODE_11G:
		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11g_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11g_antB;
			break;
		} //end switch antenna
		break;
	} //end switch mode

	memset(&rStats, 0, sizeof(RX_STATS_STRUCT));
	mask = 0x01;
	if (((testMode == MODE_11G) || (testMode == MODE_11B))&&
		((swDeviceID & 0xff) >= 0x0013) && (turbo != TURBO_ENABLE) ) {
		numLoops = 15;
		if(testMode == MODE_11B) {
			startLoop = 8;
		}
		else {
			uiPrintf("\n ");
		}
	}
	for(j = startLoop; j < numLoops; j++, mask <<= 1) {
		if(!(( j == 1) && (testMode ==MODE_11B))) {
			art_rxGetStats(devNum, DataRate[j], remoteStats, &rStats);
			if ((testMode == MODE_11G) &&
				((swDeviceID & 0xff) >= 0x0013) && (turbo != TURBO_ENABLE) && (rateMask > 0xff)) {
					uiPrintf("%3d", rStats.goodPackets);
			}
			else {
				uiPrintf("%3d(%2d)", rStats.goodPackets, rStats.DataSigStrengthAvg);
			}
			if(rStats.goodPackets < testSetup.perPassThreshold) {
				if(remoteStats) {
					//means dut was doing transmit test
					reasonFailMask |= (MASK_LINKTEST_TX_PER_ANTA << antennaMaskOffset);
				}
				else {
					//dut was doing a receive test
					reasonFailMask |= (MASK_LINKTEST_RX_PER_ANTA << antennaMaskOffset);
				}
			}
			
			if (rStats.DataSigStrengthAvg < rssiThreshold) {
				if(remoteStats) {
					//means dut was doing transmit test
					reasonFailMask |= (MASK_LINKTEST_TX_RSSI_ANTA << antennaMaskOffset);
				}
				else {
					//dut was doing a receive test
					reasonFailMask |= (MASK_LINKTEST_RX_RSSI_ANTA << antennaMaskOffset);
				}
			}
			uiPrintf(" ");
		}

	}
	art_rxGetStats(devNum, 0, remoteStats, &rStats);
	uiPrintf("%3d", rStats.crcPackets); 
	if(rStats.crcPackets > testSetup.maxCRCAllowed) {
		if(remoteStats) {
			//means dut was doing transmit test
			reasonFailMask |= (MASK_LINKTEST_TX_CRC_ANTA << antennaMaskOffset);
		}
		else {
			//dut was doing a receive test
			reasonFailMask |= (MASK_LINKTEST_RX_CRC_ANTA << antennaMaskOffset);
		}
	}
	uiPrintf(" ");

	if(!((testMode == MODE_11B) || ((testMode == MODE_11G) && ((swDeviceID & 0xff) >= 0x0013)))) {
		uiPrintf("%3d", rStats.ppmAvg); 
		if((rStats.ppmAvg > testSetup.ppmMax) || (rStats.ppmAvg < testSetup.ppmMin)) {
			if(remoteStats) {
				//means dut was doing transmit test
				reasonFailMask |= (MASK_LINKTEST_TX_PPM_ANTA << antennaMaskOffset);
			}
			else {
				//dut was doing a receive test
				reasonFailMask |= (MASK_LINKTEST_RX_PPM_ANTA << antennaMaskOffset);
			}
		}
	}

	//print the RSSI for llg if needed
	if ((testMode == MODE_11G) &&
		((swDeviceID & 0xff) >= 0x0013) && (turbo != TURBO_ENABLE) && (rateMask > 0xff)) {
	    uiPrintf("\n ");
		for(j = 0; j < numLoops; j++) {
			art_rxGetStats(devNum, DataRate[j], remoteStats, &rStats);
			uiPrintf("(%2d)", rStats.DataSigStrengthAvg);
		}
		uiPrintf("       ");
	}
	return(reasonFailMask);
}



A_UINT32 broadcastTest
(
 A_UINT32 devNum,
 A_UINT32 testChannel,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_UINT16 antenna
)
{
	A_UCHAR  pattern[] = {0xaa, 0x55};
    RX_STATS_STRUCT rStats;
	A_UINT16 index;
	A_UINT32 i;
	A_BOOL	testFailed = 0;
	A_INT32 rssiThreshold;
	A_UINT32 reasonFailMask = 0;
	A_UINT16 antennaMaskOffset = 0;
	A_BOOL   enablePPM = 1;

	if(antenna == DESC_ANT_B) {
		//shift masks by 1 to get to the B antenna mask
		antennaMaskOffset = 1;
	}

	if((testMode == MODE_11B) || ((testMode == MODE_11G) && ((swDeviceID & 0xff) >= 0x0013))) {
		enablePPM = 0;
	}
	switch(testMode) {
	case MODE_11A:
		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11a_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11a_antB;
			break;
		} //end switch antenna
		break;

	case MODE_11B:
		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11b_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11b_antB;
			break;
		} //end switch antenna
		break;
	
	case MODE_11G:
		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11g_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11g_antB;
			break;
		} //end switch antenna
		break;
	} //end switch mode

	//start the receive test
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)testMode, configSetup.use_init);		
	for(i = 0; i < testSetup.numIterations; i++) {
		art_resetDevice(devNum, dutStation, configSetup.beaconBSSID.octets, testChannel, turbo);
		art_setAntenna(devNum, configSetup.antenna);
		rxDataSetupFixedNumber(devNum, testSetup.numPackets, testSetup.packetSize, enablePPM);
		rxDataBeginFixedNumber(devNum, testSetup.beaconTimeout, 0, 0, pattern, 2);
		if (art_mdkErrNo!=0) {
			reasonFailMask |= MASK_COMMS_FAILURE;
			continue;
		}

		art_rxGetStats(devNum, 0, 0, &rStats);

		uiPrintf("\nReceived %d good packets on BSSID ", rStats.goodPackets);
		for (index = 0; index < 6; index++) {
			uiPrintf("%02x ", configSetup.beaconBSSID.octets[index]);
		}
		uiPrintf("RSSI = %d ", rStats.DataSigStrengthAvg);

		if(rStats.DataSigStrengthAvg < rssiThreshold) {
			uiPrintf("FAILED\n");
			testFailed = 1;
			reasonFailMask = MASK_BEACON_RSSI_ANTA << antennaMaskOffset;
		}
		else {
			uiPrintf("PASSED\n");
		}
	}

	if (testFailed) {
		uiPrintf("\nBeacon Test Failed. Error code %d \n", ERR_BEACON_RSSI_ANTA + antennaMaskOffset);
	}
	else {
		uiPrintf("\nBeacon Test Passed, return code 0\n");
	}

	//restore the mode
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		
	return(reasonFailMask);
}


A_UINT32 throughputUpTest
(
 A_UINT32 devNum,
 A_UINT32 testChannel,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_UINT16 antenna,
 A_UINT16 goldAntenna
)
{
	// rxDataSetup
	A_UINT32 num_rx_desc = 50;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;
	A_UINT32 stats_mode =  ENABLE_STATS_RECEIVE; 

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 25000;
	A_UINT32 compare = 0;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 broadcast = 0;
	
	A_UINT32 reasonFailMask = 0;
	A_UINT32 iterationFailMask = 0;
	A_UINT32 rateMask;
	A_UINT32 currentRate;
	A_UINT32 currentRateIndex;
	A_UINT32 numPackets;
//	A_UINT32 startTime = milliTime();

	//start the transmit test
//	uiPrintf("Good Packets        Throughput        RSSI    Result  Rate\n");
	uiPrintf("Good  Throughput  Good  RSSI  CRCs  Missed  Result  Rate\n");
	uiPrintf("(Tx)              (RX)  (RX)  (RX)\n");

	rateMask = testSetup.dataRateMaskTP;
	if(configSetup.enableXR) {
		rateMask &= 0xf80ff;
	}
	else if(((swDeviceID & 0xff) < 0x0013) || (testMode != MODE_11G)) { 
		rateMask &= 0xff;
	}
	if(testMode == MODE_11B) {
		rateMask &= 0xfd;
	}

	if((turbo == TURBO_ENABLE) && !configSetup.enableXR) {
		rateMask &= 0xff;
	}

	//send wakeup call to golden unit
	Sleep(1500);	
	dutSendWakeupCall(devNum, THROUGHPUT_UP_TEST, testChannel, testMode, turbo, goldAntenna, rateMask);

	Sleep(2000);	// delay between packet sends
	if(testMode == MODE_11B){
		numPackets = testSetup.numPacketsTP_CCK;
	}
	else {
		numPackets = testSetup.numPacketsTP;
	}
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
		(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)testMode, configSetup.use_init);		
	art_resetDevice(devNum, dutStation, bssID, testChannel, turbo);

	//set the txPower if needed
	forcePowerOrPcdac(devNum);
	
	art_txDataSetup(devNum, rateMask | RATE_GROUP, goldenStation, 
		numPackets, testSetup.packetSizeTP, pattern, 2, 0, configSetup.antenna, broadcast);
	if(art_mdkErrNo) {
		uiPrintf("Error: Unable to setup packets for throughput test\n");
		return(ERR_TEST_SETUP);
	}
	art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);
	art_txrxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);			
//	if(art_mdkErrNo) {
//		uiPrintf("Error: Throughput test did not complete\n");
//		return(ERR_TEST_SETUP);
//	}
				
//	for(i = 0; i < testSetup.numIterations; i++) {
//		uiPrintf("Starting transmit iteration number %d, out of %d\n", i, testSetup.numIterations);
		rateMask = testSetup.dataRateMaskTP;
		if(configSetup.enableXR) {
			rateMask &= 0xf80ff;
		}
		else if(((swDeviceID & 0xff) < 0x0013) || (testMode != MODE_11G)) { 
			rateMask &= 0xff;
		}
		if(testMode == MODE_11B) {
			rateMask &= 0xfd;
		}
		if((turbo == TURBO_ENABLE) && !configSetup.enableXR) {
			rateMask &= 0xff;
		}
		currentRate = 0x01;
		currentRateIndex = 0;
		while(rateMask) {
			if(currentRate & rateMask) {
				iterationFailMask = analyzeThroughputResults(devNum, currentRateIndex, testMode, turbo, 0, antenna, testChannel, "up");

				if(iterationFailMask) {
					reasonFailMask |= iterationFailMask;
					uiPrintf("      X:");
					printFailLetters(iterationFailMask);
				}
				else {
					uiPrintf("         ");
				}
				if(testMode != MODE_11B) {
					uiPrintf("  %s", DataRateStr[currentRateIndex]);
					if(turbo == TURBO_ENABLE) {
						uiPrintf(" turbo");
					}
					uiPrintf("\n");
				}
				else {
					uiPrintf("  %s\n", DataRate_11b[currentRateIndex]);
				}
				//uiPrintf("\n");
				rateMask = rateMask & ~currentRate;
			}
			currentRate = currentRate << 1;
			currentRateIndex++;
		}
		uiPrintf("\n");
//	}
	
	if(reasonFailMask) {
		uiPrintf("\nThroughput Test FAIL, due to: \n");
		printFailures(reasonFailMask);
		uiPrintf("\n");
	}
	else {
		uiPrintf("\nThroughput Test Pass, return code 0\n");
	}

	//restore the mode
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		
	if(configSetup.rangeLogging) {
		fprintf(rangeLogFileHandle, "\n");
	}
	return(reasonFailMask);
}

A_UINT32 throughputDownTest
(
 A_UINT32 devNum,
 A_UINT32 testChannel,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_UINT16 antenna,
 A_UINT16 goldAntenna
)
{
	// rxDataSetup
	A_UINT32 num_rx_desc = 50;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;
	A_UINT32 stats_mode =  ENABLE_STATS_RECEIVE; 

	// txrxDataBegin
	A_UINT32 start_timeout = 5000;	
	A_UINT32 complete_timeout = 15000;
	A_UINT32 compare = 0;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 broadcast = 0;
	
	A_UINT32 reasonFailMask = 0;
	A_UINT32 iterationFailMask = 0;
	A_UINT32 rateMask;
	A_UINT32 currentRate;
	A_UINT32 currentRateIndex;
	A_UINT32 numPackets;
	A_UINT32 mask;
	A_UINT32 numRates;

	//start the transmit test
//	uiPrintf("Good Packets        Throughput        RSSI    Result  Rate\n");
	uiPrintf("Good  Throughput  Good  RSSI  CRCs  Missed  Result  Rate\n");
	uiPrintf("(Tx)              (RX)  (RX)  (RX)\n");
	Sleep(1500);

	rateMask = testSetup.dataRateMaskTP;
	if(configSetup.enableXR) {
		rateMask &= 0xf80ff;
	}
	else if(((swDeviceID & 0xff) < 0x0013) || (testMode != MODE_11G)) { 
		rateMask &= 0xff;
	}
	if(testMode == MODE_11B) {
		rateMask &= 0xfd;
	}

	if((turbo == TURBO_ENABLE) && !configSetup.enableXR) {
		rateMask &= 0xff;
	}

	//send wakeup call to golden unit
	dutSendWakeupCall(devNum, THROUGHPUT_DOWN_TEST, testChannel, testMode, turbo, goldAntenna, rateMask);
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
		(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)testMode, configSetup.use_init);		
	if(testMode == MODE_11B){
		numPackets = testSetup.numPacketsTP_CCK;
	}
	else {
     	numPackets = testSetup.numPacketsTP;
	}
	art_resetDevice(devNum, dutStation, bssID, testChannel, turbo);

	mask = 0x80000;
	numRates = 0;
	while(mask) {
		if(mask & rateMask) {
			numRates++;
		}
		mask >>= 1;
	}
	#ifdef _DEBUG 
		printf(" F2_DEF_ANT Antenna =%x F2_STA_ID1)=%x\n",REGR(devNum, 0x8058),REGR(devNum, F2_STA_ID1));
	#endif
	art_setAntenna(devNum,antenna);
	#ifdef _DEBUG 
		printf(" Antenna =%x REGR(devNum, F2_STA_ID1)=%x\n",REGR(devNum, 0x8058),REGR(devNum, F2_STA_ID1));
	#endif
	art_rxDataSetup(devNum, (numPackets * numRates) + 40, testSetup.packetSizeTP, enable_ppm);
	#ifdef _DEBUG 
    	printf("ELSE  Antenna =%x REGR(devNum, F2_STA_ID1)=%x\n",REGR(devNum, 0x8058),REGR(devNum, F2_STA_ID1));
	#endif
	if(art_mdkErrNo) {
		uiPrintf("Error: Unable to setup packets for throughput test\n");
		return(ERR_TEST_SETUP);
	}
	art_rxDataStart(devNum);
	art_rxDataComplete(devNum, start_timeout, complete_timeout, stats_mode, 0, pattern, 2);
	//	if(art_mdkErrNo) {
//		uiPrintf("Error: Throughput test did not complete\n");
//		return(ERR_TEST_SETUP);
//	}
				
//	for(i = 0; i < testSetup.numIterations; i++) {
//		uiPrintf("Starting transmit iteration number %d, out of %d\n", i, testSetup.numIterations);
		rateMask = testSetup.dataRateMaskTP;
		if(configSetup.enableXR) {
			rateMask &= 0xf80ff;
		}
		else if(((swDeviceID & 0xff) < 0x0013) || (testMode != MODE_11G)) { 
			rateMask &= 0xff;
		}
		if(testMode == MODE_11B) {
			rateMask &= 0xfd;
		}
		if((turbo == TURBO_ENABLE) && !configSetup.enableXR) {
			rateMask &= 0xff;
		}
		currentRate = 0x01;
		currentRateIndex = 0;
		while(rateMask) {
			if(currentRate & rateMask) {
				iterationFailMask = analyzeThroughputResults(devNum, currentRateIndex, testMode, turbo, 1, antenna, testChannel, "dn");

				if(iterationFailMask) {
					reasonFailMask |= iterationFailMask;
					uiPrintf("      X:");
					printFailLetters(iterationFailMask);
				}
				else {
					uiPrintf("         ");
				}
				if(testMode != MODE_11B) {
					uiPrintf("  %s", DataRateStr[currentRateIndex]);
					if(turbo == TURBO_ENABLE) {
						uiPrintf(" turbo");
					}
					uiPrintf("\n");
				}
				else {
					uiPrintf("  %s\n", DataRate_11b[currentRateIndex]);
				}
//				uiPrintf("\n");
				rateMask = rateMask & ~currentRate;
			}
			currentRate = currentRate << 1;
			currentRateIndex++;
		}
		uiPrintf("\n");
//	}
	
	if(reasonFailMask) {
		uiPrintf("\nThroughput Test FAIL, due to: \n");
		printFailures(reasonFailMask);
		uiPrintf("\n");
	}
	else {
		uiPrintf("\nThroughput Test Pass, return code 0\n");
	}

	//restore the mode
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		
	if(configSetup.rangeLogging) {
		fprintf(rangeLogFileHandle, "\n");
	}
	return(reasonFailMask);
}

A_UINT32 analyzeThroughputResults
(
 A_UINT32 devNum,
 A_UINT32 rateIndex,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_BOOL remoteStats,
 A_UINT16 antenna,
 A_UINT32 testChannel,
 A_CHAR   *testString
)
{
//    RX_STATS_STRUCT rStats;
    RX_STATS_STRUCT rRateStats;
//	TX_STATS_STRUCT tStats;
	TX_STATS_STRUCT tRateStats;
	A_UINT32 reasonFailMask = 0;
	float tpThreshold;
	A_INT32 rssiThreshold;
	A_UINT16 antennaMaskOffset = 0;
	A_UINT32 numPackets;
	A_UINT32 missedPackets;
	A_CHAR   antString[10];
	const A_CHAR   *rateString;
	char  *modeStr[] = {"11a", "11g", "11b", "11o", "11t", "11u"};
	A_UINT16 modeOffset = 0;
	A_UINT32 perThreshold; 

	if(turbo == TURBO_ENABLE) {
		modeOffset = 4;
	}

	if(antenna == DESC_ANT_B) {
		//shift masks by 1 to get to the B antenna mask
		antennaMaskOffset = 1;
		strcpy(antString, "B");
	}
	else {
		strcpy(antString, "A");
	}

	if(testMode == MODE_11B){
		numPackets = testSetup.numPacketsTP_CCK;
		perThreshold = testSetup.perTPThreshold11b;
	}
	else {
		numPackets = testSetup.numPacketsTP;
		perThreshold = testSetup.perTPThreshold;
	}

	rateString = DataRateShortStr[rateIndex];
	switch(testMode) {
	case MODE_11A:
		tpThreshold = (float)testSetup.throughputThreshold11a;

		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11a_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11a_antB;
			break;
		} //end switch antenna
		break;

	case MODE_11B:
		tpThreshold = (float)testSetup.throughputThreshold11b;
		rateString = DataRateShortStr_11b[rateIndex];

		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11b_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11b_antB;
			break;
		} //end switch antenna
		break;
	
	case MODE_11G:
		tpThreshold = (float)testSetup.throughputThreshold11g;

		switch(antenna) {
		case DESC_ANT_A:
			rssiThreshold = testSetup.rssiThreshold11g_antA;
			break;

		case DESC_ANT_B:
			rssiThreshold = testSetup.rssiThreshold11g_antB;
			break;
		} //end switch antenna
		break;
	} //end switch mode

	memset(&rRateStats, 0, sizeof(RX_STATS_STRUCT));
	memset(&tRateStats, 0, sizeof(TX_STATS_STRUCT));
//	art_rxGetStats(devNum, 0, !remoteStats, &rStats);
//	art_txGetStats(devNum, 0, remoteStats, &tStats);

	//#################Get the rate related stats and print also
	art_rxGetStats(devNum, DataRate[rateIndex], !remoteStats, &rRateStats);
	art_txGetStats(devNum, DataRate[rateIndex], remoteStats, &tRateStats);
	missedPackets = numPackets - (rRateStats.goodPackets + rRateStats.crcPackets);
	uiPrintf("% 4d   %6.2f  % 4d  % 4d  % 4d  % 4d",
		tRateStats.goodPackets, 
		(float)tRateStats.newThroughput/1000,
		rRateStats.goodPackets, 
		rRateStats.DataSigStrengthAvg,
		rRateStats.crcPackets,
		missedPackets);
	
	if(configSetup.rangeLogging) {
		fprintf(rangeLogFileHandle, "   %s %d tput  %s  %s   %s   %c   %c %s   *   %6.2f % 4d % 3d % 3d\n",
			modeStr[testMode + modeOffset],  
			testChannel,
			testString,
			antString,
			goldAntString,
			testSetup.dutOrientation,
			testSetup.apOrientation,
			rateString,
			(float)tRateStats.newThroughput/1000,
			rRateStats.DataSigStrengthAvg,
			rRateStats.crcPackets,
			missedPackets);
		printFooter();		
	}

	if(tRateStats.goodPackets < perThreshold) {
		reasonFailMask |= MASK_THROUGHPUT_PER_ANTA << antennaMaskOffset;
	}
	
	if (rRateStats.DataSigStrengthAvg < rssiThreshold) {
		reasonFailMask |= MASK_THROUGHPUT_RSSI_ANTA << antennaMaskOffset;
	}

	if ((float)tRateStats.newThroughput/1000 < tpThreshold) {
		reasonFailMask |= MASK_THROUGHPUT_THRESH_ANTA << antennaMaskOffset;
	}
	
	return(reasonFailMask);
}

A_BOOL prepareRangeLogging
(
	void
)
{
	if(configSetup.rangeLogFile == "") {
		uiPrintf("Error: No range Log file specified\n");
		return FALSE;
	}
	rangeLogFileHandle = fopen(configSetup.rangeLogFile, "r+");

	if (rangeLogFileHandle == NULL) {
		//file may not exist
		//uiPrintf("Trying to open a new file\n");
		rangeLogFileHandle = fopen(configSetup.rangeLogFile, "w+");
	}
	else
	{
		//position file pointer at end of file
		fseek(rangeLogFileHandle, 0, SEEK_END);
	}

    if (rangeLogFileHandle == NULL) {
        uiPrintf("Unable to open file %s for logging\n", configSetup.rangeLogFile);
		configSetup.rangeLogging = FALSE;
		return FALSE;
    }


	//write the header to the file
	fprintf(rangeLogFileHandle, "\n#==================================================\n");
	fprintf(rangeLogFileHandle, "DATASET_BEGIN\n");
	fprintf(rangeLogFileHandle, "#==================================================\n\n");
	fprintf(rangeLogFileHandle, "HEADER_BEGIN\n");
	fprintf(rangeLogFileHandle, "  ap_id: %s\n", testSetup.apType);
	fprintf(rangeLogFileHandle, "  dut_id: %s\n", testSetup.dutType);
	fprintf(rangeLogFileHandle, "HEADER_END\n\n");
	fprintf(rangeLogFileHandle, "# mode  ch  type dir dut ap  dut ap rate        tput  rssi crc miss\n");
	fprintf(rangeLogFileHandle, "#                    ant ant ori or \n");
	fprintf(rangeLogFileHandle, "#                            ent en \n");
	fprintf(rangeLogFileHandle, "\nDATA_BEGIN\n\n");

	if (rangeLogFileHandle) fclose(rangeLogFileHandle);

	return TRUE;
}

//print the footer then rewind file pointer to start of footer
A_BOOL printFooter 
(
    void
)
{
	fpos_t pos;    //mark the position of where the footer starts

	if(!rangeLogFileHandle) {
		uiPrintf("Error: range log file not opened\n");
		return FALSE;
	}

	fgetpos(rangeLogFileHandle, &pos);
	fprintf(rangeLogFileHandle, "\nDATA_END\n\n");
	fprintf(rangeLogFileHandle, "DATASET_END\n");
	fsetpos(rangeLogFileHandle, &pos);
	return TRUE;
}


void dutSendWakeupCall
(
 A_UINT32 devNum,
 A_UINT32 testType,
 A_UINT32 testChannel,
 A_UINT32 testMode,
 A_UINT32 turbo,
 A_UINT16 goldAntenna,
 A_UINT32 miscParam
)
{
	// txDataSetup
	A_UINT32 rate_mask = RATE_6; //0xff; // all rates	
	A_UINT32 num_tx_desc = 15; //2;
	A_UINT32 tx_length = sizeof(TEST_INFO_STRUCT);
	A_UINT32 retry = 0xf;	
	A_UINT32 broadcast = 0;

	// rxDataSetup
	A_UINT32 num_rx_desc = 100;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;
	A_UINT32 compare = 0;

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode = 0; 
	A_UINT16 ii=0;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 channel = testSetup.sideChannel2G;
	TEST_INFO_STRUCT  testInfo;
	A_UINT32   i;

#ifdef _DEBUG
	uiPrintf("\n\nDUT send wakeup call to wakeup Golden Receiver");
#endif
	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		

	if(configSetup.mode == MODE_11A){
		channel = testSetup.sideChannel5G;
	}

	testInfo.channel = testChannel;
	testInfo.numIterations = testSetup.numIterations;
	testInfo.testType = testType;
	testInfo.goldAntenna = goldAntenna;
	testInfo.turbo = turbo;
	if((testType == THROUGHPUT_UP_TEST)|| (testType == THROUGHPUT_DOWN_TEST)) {
		testInfo.packetSize = testSetup.packetSizeTP;
		testInfo.rateMask = miscParam;
		if(configSetup.enableXR) {
			testInfo.rateMask &= 0xf80ff;
		}
		else if(((swDeviceID & 0xff) < 0x0013) || (testMode != MODE_11G)) { 
			testInfo.rateMask &= 0xff;
		}
		if((testInfo.rateMask & 0xff00) || (testMode == MODE_11B)) {
			testInfo.numPackets = testSetup.numPacketsTP_CCK;
		}
		else {
			testInfo.numPackets = testSetup.numPacketsTP;
		}
		testInfo.numIterations = 1;
	}
	else {
		testInfo.numPackets = testSetup.numPackets;
		testInfo.rateMask = configSetup.rateMask;
    	testInfo.packetSize = testSetup.packetSize;
		if(testMode == MODE_11B) {
			testInfo.rateMask = testInfo.rateMask & 0xfd;
		}
		if(((swDeviceID & 0xff) < 0x0013) || (testMode != MODE_11G)) { 
			testInfo.rateMask &= 0xff;
		}
	}
	testInfo.mode = testMode;
	if(configSetup.useTargetPower) {
		testInfo.txPower = USE_TARGET_POWER;
	}
	else {
		testInfo.txPower = configSetup.powerOutput;
	}

#if 0
	uiPrintf("SNOOP: sending wakeup call \n");		
	uiPrintf("Num Iterations = %d\n", testInfo.numIterations);
	uiPrintf("Num Packets = %d\n", testInfo.numPackets);
	uiPrintf("Packet size = %d\n", testInfo.packetSize);
	uiPrintf("Channel = %d\n", testInfo.channel);
	uiPrintf("Test type = %d\n", testInfo.testType);
	uiPrintf("Rate mask = %x\n", testInfo.rateMask);
	uiPrintf("Mode = %d\n", testInfo.mode);
#endif //_DEBUG

	for(i = 0; i < MAX_WAKEUP_ATTEMPTS; i++) {		
		art_resetDevice(devNum, dutStation, bssID, channel, configSetup.turbo);
		art_txDataSetup(devNum, rate_mask, goldenStation, num_tx_desc, tx_length,
			(A_UCHAR *)&testInfo, sizeof(TEST_INFO_STRUCT), retry,configSetup.antenna, broadcast);
		art_setAntenna(devNum, configSetup.antenna);
		if(configSetup.enableXR) {
			art_txDataBegin(devNum, complete_timeout, 0);
		}
		else {
			art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);
			art_txrxDataBegin(devNum, start_timeout, complete_timeout, stats_mode, compare, pattern, 2);
		}
		if(art_mdkErrNo==0) {
#ifdef _DEBUG
			uiPrintf("\nGolden Unit is awake now!\n");
#endif
			break;
		}
//		Sleep(100);
	}

	while(kbhit())
		getch();	// clean up buffer
	
	if(i == MAX_WAKEUP_ATTEMPTS) {
		uiPrintf("Given up trying to wakeup golden unit.  Exiting...\n");
		exit(0);
	}
}

void goldenWait4WakeupCall
(
 A_UINT32 devNum,
 TEST_INFO_STRUCT *pTestInfo   //for return
)
{
	A_UINT32 rate_mask = RATE_6;
    A_UINT32 turbo = 0;
	A_UINT32 num_tx_desc = 2;
	A_UINT32 tx_length = 100;
	A_UINT32 retry = 5;		// broadcast mode, disable retry
    A_UINT32 antenna = USE_DESC_ANT | DESC_ANT_A;
	A_UINT32 broadcast = 0;	// disable broadcast mode

	// for rx
	A_UINT32 num_rx_desc = 1000;
	A_UINT32 rx_length = sizeof(TEST_INFO_STRUCT);
	A_UINT32 enable_ppm = 0;

	// for rxDataBegin
	A_UINT32 start_timeout = 30000; //100;
	A_UINT32 complete_timeout = 10000;
	A_UINT32 stats_mode = 0; //ENABLE_STATS_SEND | ENABLE_STATS_RECEIVE;
	A_UINT32 compare = 0;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 channel = testSetup.sideChannel2G;
	A_UCHAR  buffer[500];
	A_UINT16 *mdkPacketType;

//	uiPrintf("\nWaiting for transmitter to ring the bell ...\n");

	if(configSetup.mode == MODE_11A){
		channel = testSetup.sideChannel5G;
	}


	art_setResetParams(devNum, configSetup.pCfgFile, (A_BOOL)configSetup.eepromLoad,
			(A_BOOL)configSetup.eepromHeaderLoad, (A_UCHAR)configSetup.mode, configSetup.use_init);		
	while(!kbhit()) {
		art_resetDevice(devNum, goldenStation, bssID, channel, configSetup.turbo);
		art_mdkErrNo=0;
		art_setAntenna(devNum, configSetup.antenna);
		art_rxDataSetup(devNum, num_rx_desc, rx_length, enable_ppm);
		art_rxDataBegin(devNum, start_timeout, complete_timeout, stats_mode | LEAVE_DESC_STATUS, compare, pattern, 2);
//		Sleep(100);
		if (art_mdkErrNo==0) {
#ifdef _DEBUG
			uiPrintf("\nReceived wakeup by DUT, setting up for test\n");
#endif
			//extract the info from the packet
			art_rxGetData(devNum, 0, buffer, rx_length + MDK_PACKET_OVERHEAD);  
			memcpy(pTestInfo, buffer + MDK_PACKET_OVERHEAD, sizeof(TEST_INFO_STRUCT));
			//SNOOP
//			tempPtr = (A_UINT32 *)buffer;
//			for(i = 0; i < 16; i++, tempPtr++) {
//				uiPrintf("%x ", *tempPtr);
//			}
//			uiPrintf("\n");
			//end snoop
			mdkPacketType = (A_UINT16 *)(buffer + sizeof(WLAN_DATA_MAC_HEADER3));
			if(*mdkPacketType != MDK_NORMAL_PKT) {
				//this is not the correct packet, so go back to waiting
				uiPrintf("SNOOP: handshake is not normal packet\n");
				continue;
			}
			break;
		}
	}
	return;
}

void goldenCompleteWakeupCall
(
 A_UINT32 devNum
)
{
	// txDataSetup
	A_UINT32 rate_mask = RATE_6; //0xff; // all rates	
	A_UINT32 turbo = 0;
	A_UINT32 num_tx_desc = 15; //2;
	A_UINT32 tx_length = sizeof(TEST_INFO_STRUCT);
	A_UINT32 retry = 10;	
	A_UINT32 broadcast = 0;

	// rxDataSetup
	A_UINT32 num_rx_desc = 100;
	A_UINT32 rx_length = 100;
	A_UINT32 enable_ppm = 0;
	A_UINT32 compare = 0;

	// txrxDataBegin
	A_UINT32 start_timeout = 0;	// no wait time for transmit
	A_UINT32 complete_timeout = 5000;
	A_UINT32 stats_mode = 0; 
	A_UINT16 ii=0;
	A_UCHAR  pattern[] = {0xaa, 0x55};
	A_UINT32 channel = testSetup.sideChannel2G;

	if(configSetup.mode == MODE_11A){
		channel = testSetup.sideChannel5G;
	}

	art_resetDevice(devNum, goldenStation, bssID, channel, configSetup.turbo);

	while(!kbhit()) {
	    art_txDataSetup(devNum, rate_mask, dutStation, num_tx_desc, tx_length,
			pattern, 2, retry,configSetup.antenna, broadcast);
		art_txDataBegin(devNum, complete_timeout, 0);
		if(art_mdkErrNo==0) {
#ifdef _DEBUG
			uiPrintf("\nGolden Unit is ready to go!\n");
#endif
			break;
		}
	}
	while(kbhit())
		getch();	// clean up buffer

	//cleanup descriptor queues, to free up mem
	art_cleanupTxRxMemory(devNum, TX_CLEAN | RX_CLEAN);
	return;
}

void printFailures
(
 A_UINT32 reasonFailMask
)
{
	if(reasonFailMask & MASK_LINKTEST_TX_PER_ANTA) {
		uiPrintf("   High PER(P) on antenna A. Error code %d\n", ERR_LINKTEST_TX_PER_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_TX_PER_ANTB) {
		uiPrintf("   High PER(P) on antenna B. Error code %d\n", ERR_LINKTEST_TX_PER_ANTB);
	}
	if(reasonFailMask & MASK_LINKTEST_TX_PPM_ANTA) {
		uiPrintf("   PPM out of range(M) on antenna A. Error code %d\n", ERR_LINKTEST_TX_PPM_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_TX_PPM_ANTB) {
		uiPrintf("   PPM out of range(M) on antenna B. Error code %d\n", ERR_LINKTEST_TX_PPM_ANTB);
	}
	if(reasonFailMask & MASK_LINKTEST_TX_RSSI_ANTA) {
		uiPrintf("   Low RSSI(R) on antenna A. Error code %d\n", ERR_LINKTEST_TX_RSSI_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_TX_RSSI_ANTB) {
		uiPrintf("   Low RSSI(R) on antenna B. Error code %d\n", ERR_LINKTEST_TX_RSSI_ANTB);
	}
	if(reasonFailMask & MASK_LINKTEST_TX_CRC_ANTA) {
		uiPrintf("   Many CRC errors(C) on antenna A. Error code %d\n", ERR_LINKTEST_TX_CRC_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_TX_CRC_ANTB) {
		uiPrintf("   Many CRC errors(C) on antenna B. Error code %d\n", ERR_LINKTEST_TX_CRC_ANTB);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_PER_ANTA) {
		uiPrintf("   High PER(P) on antenna A. Error code %d\n", ERR_LINKTEST_RX_PER_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_PER_ANTB) {
		uiPrintf("   High PER(P) on antenna B. Error code %d\n", ERR_LINKTEST_RX_PER_ANTB);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_PPM_ANTA) {
		uiPrintf("   PPM out of range(M) on antenna A. Error code %d\n", ERR_LINKTEST_RX_PPM_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_PPM_ANTB) {
		uiPrintf("   PPM out of range(M) on antenna B. Error code %d\n", ERR_LINKTEST_RX_PPM_ANTB);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_RSSI_ANTA) {
		uiPrintf("   Low RSSI(R) on antenna A. Error code %d\n", ERR_LINKTEST_RX_RSSI_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_RSSI_ANTB) {
		uiPrintf("   Low RSSI(R) on antenna B. Error code %d\n", ERR_LINKTEST_RX_RSSI_ANTB);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_CRC_ANTA) {
		uiPrintf("   Many CRC errors(C) on antenna A. Error code %d\n", ERR_LINKTEST_RX_CRC_ANTA);
	}
	if(reasonFailMask & MASK_LINKTEST_RX_CRC_ANTB) {
		uiPrintf("   Many CRC errors(C) on antenna B. Error code %d\n", ERR_LINKTEST_RX_CRC_ANTB);
	}
	if(reasonFailMask & MASK_THROUGHPUT_PER_ANTA) {
		uiPrintf("   High PER(P) on antenna A. Error code %d\n", ERR_THROUGHPUT_PER_ANTA);
	}
	if(reasonFailMask & MASK_THROUGHPUT_PER_ANTB) {
		uiPrintf("   High PER(P) on antenna B. Error code %d\n", ERR_THROUGHPUT_PER_ANTB);
	}
	if(reasonFailMask & MASK_THROUGHPUT_RSSI_ANTA) {
		uiPrintf("   Low RSSI(R) on antenna A. Error code %d\n", ERR_THROUGHPUT_RSSI_ANTA);
	}
	if(reasonFailMask & MASK_THROUGHPUT_RSSI_ANTB) {
		uiPrintf("   Low RSSI(R) on antenna B. Error code %d\n", ERR_THROUGHPUT_RSSI_ANTB);
	}
	if(reasonFailMask & MASK_THROUGHPUT_THRESH_ANTA) {
		uiPrintf("   Throughput too low(t) on antenna A. Error code %d\n", ERR_THROUGHPUT_THRESH_ANTA);
	}
	if(reasonFailMask & MASK_THROUGHPUT_THRESH_ANTB) {
		uiPrintf("   Throughput too low(t) on antenna B. Error code %d\n", ERR_THROUGHPUT_THRESH_ANTB);
	}
	if(reasonFailMask & MASK_COMMS_FAILURE) {
		uiPrintf("   Poor communications with golden unit. Error code %d\n", ERR_COMMS_FAIL);
	}
	if(reasonFailMask & MASK_EEPROM_BACKUP_FAIL) {
		uiPrintf("   Backup EEPROM failure. Error code %d\n", ERR_EEPROM_BACKUP);
	}
	if(reasonFailMask & MASK_EEPROM_RESTORE_FAIL) {
		uiPrintf("   Restore EEPROM failure. Error code %d\n", ERR_EEPROM_RESTORE);
	}
	if(reasonFailMask & MASK_EEPROM_COMP_VALUE_FAIL) {
		uiPrintf("   EEPROM single value compare failure. Error code %d\n", ERR_COMP_SINGLE_EEPROM_VAL);
	}
	return;
}

void printFailLetters
(
 A_UINT32 reasonFailMask
)
{
	
	if((reasonFailMask & MASK_LINKTEST_TX_RSSI_ANTA)||(reasonFailMask & MASK_LINKTEST_TX_RSSI_ANTB) ||
	   (reasonFailMask & MASK_LINKTEST_RX_RSSI_ANTA)||(reasonFailMask & MASK_LINKTEST_RX_RSSI_ANTB) ||
	   (reasonFailMask & MASK_THROUGHPUT_RSSI_ANTA)||(reasonFailMask & MASK_THROUGHPUT_RSSI_ANTB)) {
		uiPrintf("R");
	}
	if((reasonFailMask & MASK_LINKTEST_TX_CRC_ANTA)||(reasonFailMask & MASK_LINKTEST_TX_CRC_ANTB)||
		(reasonFailMask & MASK_LINKTEST_RX_CRC_ANTA)||(reasonFailMask & MASK_LINKTEST_RX_CRC_ANTB)) {
		uiPrintf("C");
	}
	if((reasonFailMask & MASK_LINKTEST_TX_PER_ANTA)||(reasonFailMask & MASK_LINKTEST_TX_PER_ANTB)||
		(reasonFailMask & MASK_LINKTEST_RX_PER_ANTA)||(reasonFailMask & MASK_LINKTEST_RX_PER_ANTB)||
		(reasonFailMask & MASK_THROUGHPUT_PER_ANTA)||(reasonFailMask & MASK_THROUGHPUT_PER_ANTB)) {
		uiPrintf("P");
	}
	if((reasonFailMask & MASK_LINKTEST_TX_PPM_ANTA)||(reasonFailMask & MASK_LINKTEST_TX_PPM_ANTB)||
		(reasonFailMask & MASK_LINKTEST_RX_PPM_ANTA)||(reasonFailMask & MASK_LINKTEST_RX_PPM_ANTB)) {
		uiPrintf("M");
	}
	if((reasonFailMask & MASK_THROUGHPUT_THRESH_ANTA)||(reasonFailMask & MASK_THROUGHPUT_THRESH_ANTA)) {
		uiPrintf("T");
	}
	return;
}

A_UINT32 printTestSummary
(
 A_UINT32 reasonFailMask
)
{
	A_UINT32 mask = 0x01;
	A_UINT32 nextErrorCode = ERR_LINKTEST_TX_PER_ANTA;
	A_UINT32 lastErrorCode = 0;

	uiPrintf("\n\n*************************************************\n");
	if(!reasonFailMask) {
		uiPrintf("\nDEVICE PASSED, return code 0");
	}
	else {
		uiPrintf("\nDEVICE FAILED with error code(s): ");
		while(reasonFailMask) {
			if(mask & reasonFailMask) {
				uiPrintf("%d ", nextErrorCode);
				lastErrorCode = nextErrorCode;
			}
			reasonFailMask = reasonFailMask & ~mask;
			mask = mask << 1;
			nextErrorCode++;
		}
	}
	uiPrintf("\n\n*************************************************\n");
	return(lastErrorCode);
}

A_BOOL testMacAddress
(
	WLAN_MACADDR *addressIn
)
{
	

	if((A_MACADDR_COMP(addressIn, &testSetup.minMacAddress) >= 0) &&
		(A_MACADDR_COMP(addressIn, &testSetup.maxMacAddress) <= 0)) {
		return TRUE;
	}
	return FALSE;
}

A_BOOL parseTestFile(void) 
{
    FILE *fStream;
    char lineBuf[222], *pLine;
	char delimiters[]   = " \t";

    uiPrintf("\nReading in Test Setup\n");
    if( (fStream = fopen( TESTSETUP_FILE, "r")) == NULL ) {
        uiPrintf("Failed to open %s - using Defaults\n", TESTSETUP_FILE);
        return 0;
    }

    while(fgets(lineBuf, 120, fStream) != NULL) {
        pLine = lineBuf;
        while(isspace(*pLine)) pLine++;
        if(*pLine == '#') {
            continue;
        }
		if(*pLine == '\0') {
			continue;
		}
		else if(strnicmp("NUM_ITERATIONS", pLine, strlen("NUM_ITERATIONS")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.numIterations)) {
                uiPrintf("Unable to read the NUM_ITERATIONS from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check to see if command line arg was specified, if so this overrides file
			if(configSetup.iterations) {
				testSetup.numIterations = configSetup.iterations;
			}
        }
		else if(strnicmp("NUM_PACKETS", pLine, strlen("NUM_PACKETS")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.numPackets)) {
                uiPrintf("Unable to read the NUM_PACKETS from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check for exeeding max
			if(testSetup.numPackets > MAX_NUM_PACKETS) {
                uiPrintf("NUM_PACKETS should be less than %d\n", MAX_NUM_PACKETS);
				return 0;
			}
			if ((thin_client) && (configSetup.cmdLineTestMask & RX_TEST_MASK) &&  (testSetup.numPackets > MAX_NUM_RX_PACKETS)) {
				uiPrintf("WARNING:Setting NUM_PACKETS to %d, for thin client cards\n", MAX_NUM_RX_PACKETS);
				testSetup.numPackets = MAX_NUM_RX_PACKETS;
			}
        }
		else if(strnicmp("PACKET_SIZE", pLine, strlen("PACKET_SIZE")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.packetSize)) {
                uiPrintf("Unable to read the PACKET_SIZE from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check for exeeding max
			if(testSetup.packetSize > MAX_PACKET_SIZE) {
                uiPrintf("PACKET_SIZE should be less than %d\n", MAX_PACKET_SIZE);
				return 0;
			}
        }
		else if(strnicmp("TP_PACKET_SIZE", pLine, strlen("TP_PACKET_SIZE")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.packetSizeTP)) {
                uiPrintf("Unable to read the TP_PACKET_SIZE from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check for exeeding max
//			if(testSetup.packetSizeTP > MAX_PACKET_SIZE) {
//                uiPrintf("TP_PACKET_SIZE should be less than %d\n", MAX_PACKET_SIZE);
//				return 0;
//			}
        }
		else if(strnicmp("TP_NUM_PACKETS", pLine, strlen("TP_NUM_PACKETS")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.numPacketsTP)) {
                uiPrintf("Unable to read the TP_NUM_PACKETS from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check for exeeding max
//			if(testSetup.numPacketsTP > MAX_NUM_PACKETS_TP) {
//                uiPrintf("TP_NUM_PACKETS should be less than %d\n", MAX_NUM_PACKETS_TP);
//				return 0;
//			}
        }
		else if(strnicmp("TP_CCK_NUM_PACKETS", pLine, strlen("TP_CCK_NUM_PACKETS")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.numPacketsTP_CCK)) {
                uiPrintf("Unable to read the TP_CCK_NUM_PACKETS from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check for exeeding max
//			if(testSetup.numPacketsTP > MAX_NUM_PACKETS_TP) {
//                uiPrintf("TP_NUM_PACKETS should be less than %d\n", MAX_NUM_PACKETS_TP);
//				return 0;
//			}
        }
		else if(strnicmp("TP_RATE_MASK", pLine, strlen("TP_RATE_MASK")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%x", &testSetup.dataRateMaskTP)) {
                uiPrintf("Unable to read the TP_RATE_MASK from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check for exeeding max
//			if(testSetup.dataRateIndexTP > 7) {
//                uiPrintf("TP_RATE_CODE should be less than 8\n");
//				return 0;
//			}
        }
		else if(strnicmp("5G_SIDE_CHANNEL", pLine, strlen("5G_SIDE_CHANNEL")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.sideChannel5G)) {
                uiPrintf("Unable to read the 5G_SIDE_CHANNEL from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("2G_SIDE_CHANNEL", pLine, strlen("2G_SIDE_CHANNEL")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.sideChannel2G)) {
                uiPrintf("Unable to read the 2G_SIDE_CHANNEL from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("EEP_SINGLE_LOCATION", pLine, strlen("EEP_SINGLE_LOCATION")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%lx", &testSetup.eepromCompareSingleLocation)) {
                uiPrintf("Unable to read the EEP_SINGLE_LOCATION from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//check for greater than 1 location
			if(testSetup.eepromCompareSingleLocation > 0x3ff) {
				uiPrintf("EEP_SINGLE_VALUE must be less than 16K (ie <= 0x3ff)\n");
				return 0;
			}
        }
		else if(strnicmp("DUT_ORIENTATION", pLine, strlen("DUT_ORIENTATION")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%c", &testSetup.dutOrientation)) {
                uiPrintf("Unable to read the DUT_ORIENTATION from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			testSetup.dutOrientation = toupper(testSetup.dutOrientation);
			if((testSetup.dutOrientation != 'N') && (testSetup.dutOrientation != 'S') &&
				(testSetup.dutOrientation != 'E') && (testSetup.dutOrientation != 'W')) {
				uiPrintf("Illegal DUT_ORIENTATION must be N|S|E|W\n");
				return 0;
			}
        }
		else if(strnicmp("AP_ORIENTATION", pLine, strlen("AP_ORIENTATION")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%c", &testSetup.apOrientation)) {
                uiPrintf("Unable to read the AP_ORIENTATION from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			testSetup.apOrientation = toupper(testSetup.apOrientation);
			if((testSetup.apOrientation != 'N') && (testSetup.apOrientation != 'S') &&
				(testSetup.apOrientation != 'E') && (testSetup.apOrientation != 'W')) {
				uiPrintf("Illegal AP_ORIENTATION must be N|S|E|W\n");
				return 0;
			}
        }
		else if(strnicmp("DUT_ID", pLine, strlen("DUT_ID")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

			strncpy(testSetup.dutType, pLine, MAX_AP_DUT_TYPE_LENGTH);			
			//no limit checking
        }
		else if(strnicmp("AP_ID", pLine, strlen("AP_ID")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

			strncpy(testSetup.apType, pLine, MAX_AP_DUT_TYPE_LENGTH);			
			//no limit checking
        }
		else if(strnicmp("PER_THRESHOLD", pLine, strlen("PER_THRESHOLD")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.perPassThreshold)) {
                uiPrintf("Unable to read the PER_THRESHOLD from %s\n", TESTSETUP_FILE);
				return 0;
            }
			testSetup.perPassThreshold = (testSetup.perPassThreshold * testSetup.numPackets)/100;
			printf("Threshold set to %d\n", testSetup.perPassThreshold);
			//no limit checking
        }
		else if(strnicmp("TP_PER_THRESH_11B", pLine, strlen("TP_PER_THRESH_11B")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.perTPThreshold11b)) {
                uiPrintf("Unable to read the TP_PER_THRESH_11B from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("TP_PER_THRESH", pLine, strlen("TP_PER_THRESH")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.perTPThreshold)) {
                uiPrintf("Unable to read the TP_PER_THRESH from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("PPM_MIN", pLine, strlen("PPM_MIN")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.ppmMin)) {
                uiPrintf("Unable to read the PPM_MIN from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("PPM_MAX", pLine, strlen("PPM_MAX")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.ppmMax)) {
                uiPrintf("Unable to read the PPM_MAX from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("RSSI_THRESHOLD_11A_ANT_A", pLine, strlen("RSSI_THRESHOLD_11A_ANT_A")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.rssiThreshold11a_antA)) {
                uiPrintf("Unable to read the RSSI_THRESHOLD_11A_ANT_A from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("RSSI_THRESHOLD_11B_ANT_A", pLine, strlen("RSSI_THRESHOLD_11B_ANT_A")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.rssiThreshold11b_antA)) {
                uiPrintf("Unable to read the RSSI_THRESHOLD_11B_ANT_A from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("RSSI_THRESHOLD_11A_ANT_B", pLine, strlen("RSSI_THRESHOLD_11A_ANT_B")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.rssiThreshold11a_antB)) {
                uiPrintf("Unable to read the RSSI_THRESHOLD_11A_ANT_B from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("RSSI_THRESHOLD_11B_ANT_B", pLine, strlen("RSSI_THRESHOLD_11B_ANT_B")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.rssiThreshold11b_antB)) {
                uiPrintf("Unable to read the RSSI_THRESHOLD_11B_ANT_B from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("RSSI_THRESHOLD_11G_ANT_A", pLine, strlen("RSSI_THRESHOLD_11G_ANT_A")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.rssiThreshold11g_antA)) {
                uiPrintf("Unable to read the RSSI_THRESHOLD_11G_ANT_A from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("RSSI_THRESHOLD_11G_ANT_B", pLine, strlen("RSSI_THRESHOLD_11G_ANT_B")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.rssiThreshold11g_antB)) {
                uiPrintf("Unable to read the RSSI_THRESHOLD_11G_ANT_B from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("SET_TX_POWER", pLine, strlen("SET_TX_POWER")) == 0) {
			if(!(configSetup.cmdLineTestMask & GOLDEN_TEST_MASK)) {
				pLine = strchr(pLine, '=');
				pLine++;
				pLine = strtok( pLine, delimiters ); //get past any white space etc

				if(!sscanf(pLine, "%ld", &configSetup.powerOutput)) {
					uiPrintf("Unable to read the PER_THRESHOLD from %s\n", TESTSETUP_FILE);
					return 0;
				}
				
				//need to double the power value, and clear the useTargetPower
				configSetup.powerOutput *= 2;
				configSetup.useTargetPower = 0;
			}
        }
		else if(strnicmp("BEACON_TIMEOUT", pLine, strlen("BEACON_TIMEOUT")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.beaconTimeout)) {
                uiPrintf("Unable to read the BEACON_TIMEOUT from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("CRC_THRESHOLD", pLine, strlen("CRC_THRESHOLD")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%ld", &testSetup.maxCRCAllowed)) {
                uiPrintf("Unable to read the CRC_THRESHOLD from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("TP_THRESHOLD_11A", pLine, strlen("TP_THRESHOLD_11A")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%f", &testSetup.throughputThreshold11a)) {
                uiPrintf("Unable to read the TP_THRESHOLD_11A from %s\n", TESTSETUP_FILE);
				return 0;
			}
			
			//no limit checking
        }
		else if(strnicmp("TP_THRESHOLD_11B", pLine, strlen("TP_THRESHOLD_11B")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%f", &testSetup.throughputThreshold11b)) {
                uiPrintf("Unable to read the TP_THRESHOLD_11B from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if(strnicmp("TP_THRESHOLD_11G", pLine, strlen("TP_THRESHOLD_11G")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc

            if(!sscanf(pLine, "%f", &testSetup.throughputThreshold11g)) {
                uiPrintf("Unable to read the TP_THRESHOLD_11G from %s\n", TESTSETUP_FILE);
				return 0;
            }
			
			//no limit checking
        }
		else if (strnicmp("MAC_ADDRESS_MIN", pLine, strlen("MAC_ADDRESS_MIN")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc
			if(!getBssidFromString(testSetup.minMacAddress.octets, pLine)) {
                uiPrintf("Unable to read the MAC_ADDRESS_MIN from %s\n", TESTSETUP_FILE);
				return 0;
			}
		}
		else if (strnicmp("MAC_ADDRESS_MAX", pLine, strlen("MAC_ADDRESS_MAX")) == 0) {
            pLine = strchr(pLine, '=');
            pLine++;
	        pLine = strtok( pLine, delimiters ); //get past any white space etc
			if(!getBssidFromString(testSetup.maxMacAddress.octets, pLine)) {
                uiPrintf("Unable to read the MAC_ADDRESS_MAX from %s\n", TESTSETUP_FILE);
				return 0;
			}
		}
		else {
			uiPrintf("Illegal option in %s:\n   %s\n", TESTSETUP_FILE, pLine);
			return 0;
		}
	}  // End while (get line)
    fclose(fStream);
	return 1;
} 

//expect the bssid to be specified in the format NN-NN-NN-NN-NN-NN
A_BOOL getBssidFromString
(
 A_UCHAR *bssid,
 A_CHAR *string
)
{
	A_CHAR *token;
	A_UINT16 index = 0;
	A_UINT32 value;

	token = strtok(string, "-");
	while(token) {
		if(!sscanf(token, "%x", &value)) {
			uiPrintf("Unable to read address, use format NN-NN-NN-NN-NN-NN\n");
			return FALSE;
		}

		if(value > 0xff) {
			uiPrintf("Unable to read address, use format NN-NN-NN-NN-NN-NN\n");
			return FALSE;			
		}

		bssid[index] = (A_UCHAR)value;	

		token = strtok(NULL, "-");
		index++;
		if(token && (index == 6)) {
			//too many chars specified for bssid
			uiPrintf("address string too long, use format NN-NN-NN-NN-NN-NN\n");
			return FALSE;			
		}
		else if(!token && (index != 6)) {
			//not enough chars specified for bssid
			uiPrintf("address string too short, use format NN-NN-NN-NN-NN-NN\n");
			return FALSE;			
		}
	}	

	return TRUE;
}

