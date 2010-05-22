/* mDevtbl.c - contians configuration table for all the devices supported by devlib */
/* Copyright (c) 2001 Atheros Communications, Inc., All Rights Reserved */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/mDevtbl.c#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/mDevtbl.c#1 $"

/* 
Revsision history
--------------------
1.0       Created.
*/
#ifdef VXWORKS
#include "vxworks.h"
#endif

#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "mDevtbl.h"
#include "mData210.h"
#include "mCfg210.h"
#include "mData211.h"
#include "mCfg211.h"
#include "mData212.h"
#if defined(LINUX)
#include "mData513.h"
#elif defined(_WINDOWS)
#include "..\ar5513\mData513.h"
#endif
#include "mCfg212.h"
#include "mCfg212d.h"
#include "mAni212.h"
#include "mCfg413.h"
#if defined(LINUX)
#include "mCfg6000.h"
#elif defined(_WINDOWS)
#include "..\ar6000\mCfg6000.h"
#endif
#include "mIds.h"

#ifndef MDK_AP
#ifndef CUSTOMER_REL
//static ATHEROS_REG_FILE ar5k0007_init[] = {
//#include "dk_crete_fez.ini"
//};
#endif //CUSTOMER_REL


static ATHEROS_REG_FILE hainan_derby2_1[] = {	//new version 2 ini file
#include "dk_0017_2_1.ini"
};
static MODE_INFO hainan_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_0017_2_1.mod"
};
static ATHEROS_REG_FILE griffin2[] = {	//new version 2 ini file
#include "dk_0018_2.ini"
};
static MODE_INFO griffin2_mode[] = {	//new version 2 mode ini file
#include "dk_0018_2.mod"
};
static ATHEROS_REG_FILE eagle2[] = {	//new version 2 ini file
#include "dk_0019_2.ini"
};
static MODE_INFO eagle2_mode[] = {	//new version 2 mode ini file
#include "dk_0019_2.mod"
};
//#ifdef PREDATOR_BUILD
static ATHEROS_REG_FILE predator_derby2_1[] = {	//new version 2 ini file
#include "dk_00b0_2_1.ini"
};
static MODE_INFO predator_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_00b0_2_1.mod"
};
//#endif   // PREDATOR_BUILD

static ATHEROS_REG_FILE condor[] = {	//new version 2 ini file
#include "dk_0020.ini"
};
static MODE_INFO condor_mode[] = {	//new version 2 mode ini file
#include "dk_0020.mod"
};

#if defined(LINUX) || defined(_WINDOWS)
static ATHEROS_REG_FILE dragon[] = {	//new version 2 ini file
#include "dk_0022.ini"
};
static MODE_INFO dragon_mode[] = {	//new version 2 mode ini file
#include "dk_0022.mod"
};

static ATHEROS_REG_FILE mercury[] = {	//Mercury 1.0/1.1 ini file
#include "dk_0027.ini"
};
static MODE_INFO mercury_mode[] = {	//Mercury 1.0/1.1 mode file
#include "dk_0027.mod"
};
static ATHEROS_REG_FILE mercurylna2[] = {	//Mercury 1.0/1.1 ini file
#include "dk_0027lna2.ini"
};
static MODE_INFO mercurylna2_mode[] = {	//Mercury 1.0/1.1 mode file
#include "dk_0027lna2.mod"
};

static ATHEROS_REG_FILE mercury2_0[] = { //Mercury 2.0 ini file
#include "dk_0027_2_0.ini"
};
static MODE_INFO mercury_mode2_0[] = {	//Mercury 2.0 mode file
#include "dk_0027_2_0.mod"
};

static ATHEROS_REG_FILE mercurylna22_0[] = { //Mercury 2.0 ini file
#include "dk_0027lna2_2_0.ini"
};
static MODE_INFO mercurylna2_mode2_0[] = {	//Mercury 2.0 mode file
#include "dk_0027lna2_2_0.mod"
};

#endif

#endif //MDK_AP

#ifdef AP11_AP
static ATHEROS_REG_FILE ar5k0007_init[] = {
#include "dk_crete_fez.ini"
};
#endif //AP11_AP

#ifdef SPIRIT_AP 
static ATHEROS_REG_FILE ar5k0011_spirit1_som2_e4[] = {
#include "dk_spirit1_som2_e4.ini"
};
#endif // SPIRIT_AP

#ifdef AP22_AP
static ATHEROS_REG_FILE boss_0012[] = {	//new version 2 ini file
#include "dk_boss_0012.ini"
};
static MODE_INFO boss_0012_mode[] = {	//new version 2 mode ini file
#include "dk_boss_0012.mod"
};
static ATHEROS_REG_FILE venice[] = {	//new version 2 ini file
#include "dk_boss_0013.ini"
};
static MODE_INFO venice_mode[] = {	//new version 2 mode ini file
#include "dk_boss_0013.mod"
};
static ATHEROS_REG_FILE venice_derby[] = {	//new version 2 ini file
#include "dk_0014.ini"
};
static MODE_INFO venice_derby_mode[] = {	//new version 2 mode ini file
#include "dk_0014.mod"
};
static ATHEROS_REG_FILE venice_derby2_1[] = {	//new version 2 ini file
#include "dk_0016_2_1.ini"
};
static MODE_INFO venice_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_0016_2_1.mod"
};
static ATHEROS_REG_FILE venice_derby2_1_ear[] = {	//new version 2 ini file
#include "dk_0016_2_1_ear.ini"
};
static MODE_INFO venice_derby2_1_mode_ear[] = {	//new version 2 mode ini file
#include "dk_0016_2_1_ear.mod"
};
static ATHEROS_REG_FILE hainan_derby2_1[] = {	//new version 2 ini file
#include "dk_0017_2_1.ini"
};
static MODE_INFO hainan_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_0017_2_1.mod"
};
static ATHEROS_REG_FILE hainan_derby2_1_ear[] = {	//new version 2 ini file
#include "dk_0017_2_1_ear.ini"
};
static MODE_INFO hainan_derby2_1_mode_ear[] = {	//new version 2 mode ini file
#include "dk_0017_2_1_ear.mod"
};
#endif //AP22_AP

#if (defined(FREEDOM_AP)||defined(THIN_CLIENT_BUILD))&&!defined(COBRA_AP)
static ATHEROS_REG_FILE freedom2_derby2_1[] = {	//new version 2 ini file
#include "dk_freedom2_derby2_1.ini"
};
static MODE_INFO freedom2_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_freedom2_derby2_1.mod"
};
#ifndef SOC_LINUX
static ATHEROS_REG_FILE freedom2_derby2[] = {	//new version 2 ini file
#include "dk_freedom2_derby2.ini"
};
static MODE_INFO freedom2_derby2_mode[] = {	//new version 2 mode ini file
#include "dk_freedom2_derby2.mod"
};
static ATHEROS_REG_FILE viper_derby2_1[] = {	//new version 2 ini file
#include "dk_viper_derby2_1.ini"
};
static MODE_INFO viper_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_viper_derby2_1.mod"
};

#endif
#endif

#ifdef SENAO_AP
static ATHEROS_REG_FILE venice_derby2_1_ear[] = {	//new version 2 ini file
#include "dk_0016_2_1_ear.ini"
};
static MODE_INFO venice_derby2_1_mode_ear[] = {	//new version 2 mode ini file
#include "dk_0016_2_1_ear.mod"
};
static ATHEROS_REG_FILE venice_derby2_1[] = {	//new version 2 ini file
#include "dk_0016_2_1.ini"
};
static MODE_INFO venice_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_0016_2_1.mod"
};
static ATHEROS_REG_FILE hainan_derby2_1[] = {	//new version 2 ini file
#include "dk_0017_2_1.ini"
};
static MODE_INFO hainan_derby2_1_mode[] = {	//new version 2 mode ini file
#include "dk_0017_2_1.mod"
};
static ATHEROS_REG_FILE hainan_derby2_1_ear[] = {	//new version 2 ini file
#include "dk_0017_2_1_ear.ini"
};
static MODE_INFO hainan_derby2_1_mode_ear[] = {	//new version 2 mode ini file
#include "dk_0017_2_1_ear.mod"
};
static ATHEROS_REG_FILE griffin2[] = {	//new version 2 ini file
#include "dk_0018_2.ini"
};
static MODE_INFO griffin2_mode[] = {	//new version 2 mode ini file
#include "dk_0018_2.mod"
};
static ATHEROS_REG_FILE eagle2[] = {	//new version 2 ini file
#include "dk_0019_2.ini"
};
static MODE_INFO eagle2_mode[] = {	//new version 2 mode ini file
#include "dk_0019_2.mod"
};


#endif //SENAO_AP


#ifdef COBRA_AP

static ATHEROS_REG_FILE cobra[] = {	//new version 2 ini file
#include "dk_cobra1_0.ini"
};
static MODE_INFO cobra_mode[] = {	//new version 2 mode ini file
#include "dk_cobra1_0.mod"
};
static ATHEROS_REG_FILE spider[] = {	//new version 2 ini file
#include "dk_spider1_0.ini"
};
static MODE_INFO spider_mode[] = {	//new version 2 mode ini file
#include "dk_spider1_0.mod"
};

#ifdef PCI_INTERFACE
static ATHEROS_REG_FILE eagle2[] = {	//new version 2 ini file
#include "dk_0019_2.ini"
};
static MODE_INFO eagle2_mode[] = {	//new version 2 mode ini file
#include "dk_0019_2.mod"
};
#endif

#endif //COBRA_AP


static MAC_API_TABLE creteAPI = {
	macAPIInitAr5210,
	eepromReadAr5210,
	eepromWriteAr5210,

	hwResetAr5210,
	pllProgramAr5210,

	setRetryLimitAr5210,
	setupAntennaAr5210,
	sendTxEndPacketAr5210,
	setDescriptorAr5210,
	setStatsPktDescAr5210,
	setContDescriptorAr5210,
	txBeginConfigAr5210,
	txBeginContDataAr5210,
	txBeginContFramedDataAr5210,
	txEndContFramedDataAr5210,
	beginSendStatsPktAr5210,
	writeRxDescriptorAr5210,
	rxBeginConfigAr5210,
	rxCleanupConfigAr5210,
	txCleanupConfigAr5210,
	txGetDescRateAr5210,
	setPPM5210,
	isTxdescEvent5210,
	isRxdescEvent5210,
	isTxComplete5210,
	enableRx5210,
	disableRx5210,
	setQueueAr5210,
	mapQueueAr5210,
	clearKeyCacheAr5210,
	AGCDeafAr5210,
	AGCUnDeafAr5210
};

static MAC_API_TABLE maui1API = {
	macAPIInitAr5210,
	eepromReadAr5210,
	eepromWriteAr5210,

	hwResetAr5210,
	pllProgramAr5210,

	setRetryLimitAr5210,
	setupAntennaAr5210,
	sendTxEndPacketAr5210,
	setDescriptorAr5210,
	setStatsPktDescAr5210,
	setContDescriptorAr5210,
	txBeginConfigAr5210,
	txBeginContDataAr5210,
	txBeginContFramedDataAr5210,
	txEndContFramedDataAr5210,
	beginSendStatsPktAr5210,
	writeRxDescriptorAr5210,
	rxBeginConfigAr5210,
	rxCleanupConfigAr5210,
	txCleanupConfigAr5210,
	txGetDescRateAr5210,
	setPPM5210,
	isTxdescEvent5210,
	isRxdescEvent5210,
	isTxComplete5210,
	enableRx5210,
	disableRx5210,
	setQueueAr5210,
	mapQueueAr5210,
	clearKeyCacheAr5210,
	AGCDeafAr5210,
	AGCUnDeafAr5210
};

static MAC_API_TABLE maui2API = {
	macAPIInitAr5211,
	eepromReadAr5211,		
	eepromWriteAr5211,

	hwResetAr5211,
	pllProgramAr5211,
	
	setRetryLimitAllAr5211,
	setupAntennaAr5211,
	sendTxEndPacketAr5211,
	setDescriptorAr5211,
	setStatsPktDescAr5211,
	setContDescriptorAr5211,
	txBeginConfigAr5211,
	txBeginContDataAr5211,
	txBeginContFramedDataAr5211,
	txEndContFramedDataAr5211,
	beginSendStatsPktAr5211,
	writeRxDescriptorAr5211,
	rxBeginConfigAr5211,
	rxCleanupConfigAr5211,
	txCleanupConfigAr5211,
	txGetDescRateAr5211,
	setPPM5211,
	isTxdescEvent5211,
	isRxdescEvent5211,
	isTxComplete5211,
	enableRx5211,
	disableRx5211,
	setQueueAr5211,
	mapQueueAr5211,
	clearKeyCacheAr5211,
	AGCDeafAr5211,
	AGCUnDeafAr5211
};

static MAC_API_TABLE veniceAPI = {
	macAPIInitAr5212,
	eepromReadAr5211,		
	eepromWriteAr5211,

	hwResetAr5211,
	pllProgramAr5212,
	
	setRetryLimitAllAr5211,
	setupAntennaAr5211,
	sendTxEndPacketAr5211,
	setDescriptorAr5212,
	setStatsPktDescAr5212,
	setContDescriptorAr5212,
	txBeginConfigAr5211,
	txBeginContDataAr5211,
	txBeginContFramedDataAr5211,
	txEndContFramedDataAr5211,
	beginSendStatsPktAr5211,
	writeRxDescriptorAr5211,
	rxBeginConfigAr5212,
	rxCleanupConfigAr5211,
	txCleanupConfigAr5211,
	txGetDescRateAr5212,
	setPPM5211,
	isTxdescEvent5211,
	isRxdescEvent5211,
	isTxComplete5211,
	enableRx5211,
	disableRx5211,
	setQueueAr5211,
	mapQueueAr5211,
	clearKeyCacheAr5211,
	AGCDeafAr5211,
	AGCUnDeafAr5211
};

//changing pll programming
static MAC_API_TABLE eagleAPI = {
	macAPIInitAr5212,
	eepromReadAr5211,		
	eepromWriteAr5211,

	hwResetAr5211,
	pllProgramAr5413,
	
	setRetryLimitAllAr5211,
	setupAntennaAr5211,
	sendTxEndPacketAr5211,
	setDescriptorAr5212,
	setStatsPktDescAr5212,
	setContDescriptorAr5212,
	txBeginConfigAr5211,
	txBeginContDataAr5211,
	txBeginContFramedDataAr5211,
	txEndContFramedDataAr5211,
	beginSendStatsPktAr5211,
	writeRxDescriptorAr5211,
	rxBeginConfigAr5212,
	rxCleanupConfigAr5211,
	txCleanupConfigAr5211,
	txGetDescRateAr5212,
	setPPM5211,
	isTxdescEvent5211,
	isRxdescEvent5211,
	isTxComplete5211,
	enableRx5211,
	disableRx5211,
	setQueueAr5211,
	mapQueueAr5211,
	clearKeyCacheAr5211,
	AGCDeafAr5211,
	AGCUnDeafAr5211
};

// Different PLL, synth, eep, descs...
#if defined(LINUX) || defined(_WINDOWS)
static MAC_API_TABLE dragonAPI = {
	macAPIInitAr5513,
	eepromReadAr5211,		
	eepromWriteAr5211,

	hwResetAr5211,
	pllProgramAr5212, // Call down to thin-client ala predator
	
	setRetryLimitAllAr5211,
	setupAntennaAr5513,
	sendTxEndPacketAr5513,
	setDescriptorAr5513,
	setStatsPktDescAr5513,
	setContDescriptorAr5513,
	txBeginConfigAr5513,
	txBeginContDataAr5513,
	txBeginContFramedDataAr5513,
	txEndContFramedDataAr5211,
	beginSendStatsPktAr5513,
	writeRxDescriptorAr5513,
	rxBeginConfigAr5513,
	rxCleanupConfigAr5513,
	txCleanupConfigAr5211,
	txGetDescRateAr5212,
	setPPM5513,
	isTxdescEvent5211,
	isRxdescEvent5211,
	isTxComplete5211,
	enableRx5211,
	disableRx5211,
	setQueueAr5211,
	mapQueueAr5211,
	clearKeyCacheAr5211,
	AGCDeafAr5211,
	AGCUnDeafAr5211
};
#endif
static RF_API_TABLE fezAPI = {
	initPowerAr5210,
	setSinglePowerAr5210,
	setChannelAr5210
};

static RF_API_TABLE sombreroAPI = {
	initPowerAr5211,
	setSinglePowerAr5211,
	setChannelAr5211
};

static RF_API_TABLE sombrero_beanieAPI = {
	initPowerAr5211,
	setSinglePowerAr5211,
	setChannelAr5211_beanie
};

static RF_API_TABLE derbyAPI = {
	initPowerAr5212,
	setSinglePowerAr5211,
	setChannelAr5212
};


static RF_API_TABLE griffinRfAPI = {
	initPowerAr2413,
	setSinglePowerAr5211,
	setChannelAr2413
};
#if defined(LINUX) || defined(_WINDOWS)
static RF_API_TABLE dragonRfAPI = {
	initPowerAr6000,
	setSinglePowerAr5211,
	setChannelAr6000
};
#endif
static ART_ANI_API_TABLE veniceArtAniAPI = {
	configArtAniLadderAr5212,
	enableArtAniAr5212,
	disableArtAniAr5212,
	setArtAniLevelAr5212,
	setArtAniLevelMaxAr5212,
	setArtAniLevelMinAr5212,
	incrementArtAniLevelAr5212,
	decrementArtAniLevelAr5212,
	getArtAniLevelAr5212,
	measArtAniFalseDetectsAr5212,
	isArtAniOptimizedAr5212,
	getArtAniFalseDetectsAr5212,
	setArtAniFalseDetectIntervalAr5212,
	programCurrArtAniLevelAr5212
};

ANALOG_REV fezRevs[] = { 
	{0, 0x8},
    {0, 0x9}
};
const A_UINT16 numFezRevs = sizeof(fezRevs)/sizeof(ANALOG_REV);

ANALOG_REV sombreroRevs[] = {
	{1, 0x5},
	{1, 0x6},
	{1, 0x7}
};
const A_UINT16 numSombreroRevs = sizeof(sombreroRevs)/sizeof(ANALOG_REV);

ANALOG_REV derby1Revs[] = {
	{3, 0x1},
	{3, 0x2}
};
const A_UINT16 numDerby1Revs = sizeof(derby1Revs)/sizeof(ANALOG_REV);

ANALOG_REV derby1_2Revs[] = {
	{3, 0x3},
	{3, 0x4},
//	{0, 0} 
};
const A_UINT16 numderby1_2Revs = sizeof(derby1_2Revs)/sizeof(ANALOG_REV);

ANALOG_REV derby2Revs[] = {
	{3, 0x5},
	{4, 0x5},
//	{3, 0x6},
//	{4, 0x6},
	{0, 0}   // dummy to handle AP31 with no 5G derby
};
const A_UINT16 numderby2Revs = sizeof(derby2Revs)/sizeof(ANALOG_REV);

ANALOG_REV derby2_1Revs[] = {
	{3, 0x6},
	{4, 0x6},
	{0, 0}   // dummy to handle AP31 with no 5G derby
};
const A_UINT16 numderby2_1Revs = sizeof(derby2_1Revs)/sizeof(ANALOG_REV);

A_UINT16 veniceRevs[] = {
	0x50, 0x51, 0x53, 0x56
};

const A_UINT16 numVeniceRevs = sizeof(veniceRevs)/sizeof(A_UINT16);

A_UINT16 predatorRevs[] = {
	0x00,  // For emulation
	0x80,  // Predator 1.0
	0x81,  // Predator 1.1
};

const A_UINT16 numPredatorRevs = sizeof(predatorRevs)/sizeof(A_UINT16);

A_UINT16 hainanRevs[] = {
	0x55,
	0x59
};
const A_UINT16 numHainanRevs = sizeof(hainanRevs)/sizeof(A_UINT16);

A_UINT16 griffinRevs[] = {
	0x74,  // griffin 1.0
    0x75,  // griffin 1.1
    0x76,  // griffin 2.0
	0x78,  //griffin lite
	0x79,   //griffin 2.1
//	0xa0,   //special build additions
	0x00
};
const A_UINT16 numGriffinRevs = sizeof(griffinRevs)/sizeof(A_UINT16);

A_UINT16 eagleRevs[] = {
	0xa0,   //eagle 1.0
	0xa1,  // eagle 1.0
	0x00,
};

const A_UINT16 numEagleRevs = sizeof(eagleRevs)/sizeof(A_UINT16);

A_UINT16 eagle2Revs[] = {
	0xa2,  // eagle 2.0
	0xa3,  // eagle 2.0
	0xa4,  // eagle 2.1 lite
	0xa5,  // eagle 2.1 super
};

const A_UINT16 numEagle2Revs = sizeof(eagle2Revs)/sizeof(A_UINT16);

A_UINT16 cobraRevs[] = {
	0xb0,  // cobra 1.0
};

A_UINT16 dragonRevs[] = {
	0x00,  // dragon 1.0
};
const A_UINT16 numDragonRevs = sizeof(dragonRevs)/sizeof(A_UINT16);

A_UINT16 mercuryRevs[] = {
	0x00,  // Mercury 1.0/1.1
};
const A_UINT16 numMercuryRevs = sizeof(mercuryRevs)/sizeof(A_UINT16);

ANALOG_REV griffinAnalogRevs[] = {
	{5, 0x1},
	{5, 0x2},
};
const A_UINT16 numGriffinAnalogRevs = sizeof(griffinAnalogRevs)/sizeof(ANALOG_REV);

ANALOG_REV griffin1_1_AnalogRevs[] = {
	{5, 0x3},
	{5, 0x4},
};
const A_UINT16 numGriffin1_1_AnalogRevs = sizeof(griffin1_1_AnalogRevs)/sizeof(ANALOG_REV);

ANALOG_REV griffin2_AnalogRevs[] = {
	{5, 0x5},
	{5, 0x6},
};
const A_UINT16 numGriffin2_AnalogRevs = sizeof(griffin2_AnalogRevs)/sizeof(ANALOG_REV);

ANALOG_REV eagleAnalogRevs[] = {
	{6, 0x0},
	{6, 0x1},
};
const A_UINT16 numEagleAnalogRevs = sizeof(eagleAnalogRevs)/sizeof(ANALOG_REV);

ANALOG_REV eagle2AnalogRevs[] = {
	{6, 0x2},
	{6, 0x3},
};
const A_UINT16 numEagle2AnalogRevs = sizeof(eagle2AnalogRevs)/sizeof(ANALOG_REV);

ANALOG_REV cobraAnalogRevs[] = {
	{7, 0x0},
};

ANALOG_REV spiderAnalogRevs[] = {
	{8, 0x3},
};

ANALOG_REV dragonAnalogRevs[] = {
	{9, 0x0},
};
const A_UINT16 numDragonAnalogRevs = sizeof(dragonAnalogRevs)/sizeof(ANALOG_REV);

ANALOG_REV mercuryAnalogRevs[] = {
	{PRODUCT_ID_MERCURY, ANALOG_REVID_MERCURY},
};
const A_UINT16 numMercuryAnalogRevs = sizeof(mercuryAnalogRevs)/sizeof(ANALOG_REV);

ANALOG_REV mercury2_0AnalogRevs[] = {
	{PRODUCT_ID_MERCURY, ANALOG_REVID_MERCURY2_0},
	{PRODUCT_ID_MERCURY, ANALOG_REVID_MERCURY2_0_QUICKSILVER},
};
const A_UINT16 numMercury2_0AnalogRevs = sizeof(mercury2_0AnalogRevs)/sizeof(ANALOG_REV);

A_UINT16 condorRevs[] = {
	0x92,  //condor 1.0 lite
	0x93,  //condor 1.0 full
	0x9a,  //condor 2.0 lite
	0x9b,  //condor 2.0 full
};

const A_UINT16 numCondorRevs = sizeof(condorRevs)/sizeof(A_UINT16);

ANALOG_REV condorAnalogRevs[] = {
	{6, 0x3},
	{7, 0x1},
	{0xa, 0x2}, //condor 2.0
};
const A_UINT16 numCondorAnalogRevs = sizeof(condorAnalogRevs)/sizeof(ANALOG_REV);


DEVICE_INIT_DATA ar5kInitData[] = {
#ifndef MDK_AP


	{DONT_MATCH, derby2_1Revs, numderby2_1Revs, hainanRevs, numHainanRevs, SW_DEVICE_ID_HAINAN_DERBY, //Identifiers
	 hainan_derby2_1,  sizeof(hainan_derby2_1)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,								//APIs
	 3, hainan_derby2_1_mode, sizeof(hainan_derby2_1_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d017 },												//configuration string


	{DONT_MATCH, condorAnalogRevs, numCondorAnalogRevs, condorRevs, numCondorRevs, SW_DEVICE_ID_CONDOR, //Identifiers
	 condor,  sizeof(condor)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &eagleAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, condor_mode, sizeof(condor_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_0020 },												//configuration string


	{DONT_MATCH, griffin2_AnalogRevs, numGriffin2_AnalogRevs, griffinRevs, numGriffinRevs, SW_DEVICE_ID_GRIFFIN, //Identifiers
	 griffin2,  sizeof(griffin2)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, griffin2_mode, sizeof(griffin2_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_c018 },												//configuration string

	{DONT_MATCH, eagle2AnalogRevs, numEagle2AnalogRevs, eagle2Revs, numEagle2Revs, SW_DEVICE_ID_EAGLE, //Identifiers
	 eagle2,  sizeof(eagle2)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &eagleAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, eagle2_mode, sizeof(eagle2_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d019 },												//configuration string

#if defined(LINUX) || defined(_WINDOWS)
	 {DONT_MATCH, dragonAnalogRevs, numDragonAnalogRevs, dragonRevs, numDragonRevs, SW_DEVICE_ID_DRAGON, //Identifiers
	 dragon,  sizeof(dragon)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &dragonAPI, &dragonRfAPI,	&veniceArtAniAPI,								//APIs
	 3, dragon_mode, sizeof(dragon_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d022 },

         /* IMPORTANT: Mercury1.0 LNA2 cfg must follow Mercury1.0 LNA1 cfg. */
	 {DONT_MATCH, mercuryAnalogRevs, numMercuryAnalogRevs, mercuryRevs, numMercuryRevs, SW_DEVICE_ID_MERCURY, //Identifiers
	 mercury,  sizeof(mercury)/sizeof(ATHEROS_REG_FILE),	
	 &dragonAPI, &dragonRfAPI,	&veniceArtAniAPI,
	 3, mercury_mode, sizeof(mercury_mode)/sizeof(MODE_INFO),
	 CFG_VERSION_STRING_0027 },

	 {DONT_MATCH, mercuryAnalogRevs, numMercuryAnalogRevs, mercuryRevs, numMercuryRevs, SW_DEVICE_ID_MERCURY, //Identifiers
	 mercurylna2,  sizeof(mercurylna2)/sizeof(ATHEROS_REG_FILE),	
	 &dragonAPI, &dragonRfAPI,	&veniceArtAniAPI,
	 3, mercurylna2_mode, sizeof(mercurylna2_mode)/sizeof(MODE_INFO),
	 CFG_VERSION_STRING_0029 },

         /* IMPORTANT: Mercury2.0 LNA2 cfg must follow Mercury2.0 LNA1 cfg. */
	 {DONT_MATCH, mercury2_0AnalogRevs, numMercury2_0AnalogRevs, mercuryRevs, numMercuryRevs, SW_DEVICE_ID_MERCURY, //Identifiers
	 mercury2_0,  sizeof(mercury2_0)/sizeof(ATHEROS_REG_FILE),
	 &dragonAPI, &dragonRfAPI,	&veniceArtAniAPI,
	 3, mercury_mode2_0, sizeof(mercury_mode2_0)/sizeof(MODE_INFO), 
	 CFG_VERSION_STRING_0028 },

	 {DONT_MATCH, mercury2_0AnalogRevs, numMercury2_0AnalogRevs, mercuryRevs, numMercuryRevs, SW_DEVICE_ID_MERCURY, //Identifiers
	 mercurylna22_0,  sizeof(mercurylna22_0)/sizeof(ATHEROS_REG_FILE),
	 &dragonAPI, &dragonRfAPI,	&veniceArtAniAPI,
	 3, mercurylna2_mode2_0, sizeof(mercurylna2_mode2_0)/sizeof(MODE_INFO), 
	 CFG_VERSION_STRING_0030 },
#endif
// Predator 
	{DONT_MATCH, derby2_1Revs, numderby2_1Revs, predatorRevs, numPredatorRevs, SW_DEVICE_ID_PREDATOR, //Identifiers
	 predator_derby2_1,  sizeof(predator_derby2_1)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,								//APIs
	 3, predator_derby2_1_mode, sizeof(predator_derby2_1_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_00b0 },												//configuration string
//#endif   // PREDATOR_BUILD

#endif //MDK_AP


#ifdef SPIRIT_AP
   	{0x0011, NULL, 0, NULL, 0, 0x0011,
	 ar5k0011_spirit1_som2_e4,  sizeof(ar5k0011_spirit1_som2_e4)/sizeof(ATHEROS_REG_FILE), 
	 &maui2API, &sombreroAPI,&veniceArtAniAPI,0,NULL,0,NULL },
#endif //SPIRIT_AP

#ifdef AP22_AP
	{0x0012, NULL, 0, NULL, 0, SW_DEVICE_ID_BOSS_0012,									//Identifiers
	 boss_0012, sizeof(boss_0012)/sizeof(ATHEROS_REG_FILE),				//register file
	 &maui2API, &sombrero_beanieAPI, &veniceArtAniAPI,					//APIs
	 2, boss_0012_mode, sizeof(boss_0012_mode)/sizeof(MODE_INFO),		//Mode file
	 CFG_VERSION_STRING_0012 },											//configuraton string

	{0xff12, NULL, 0, NULL, 0,	SW_DEVICE_ID_BOSS_0012,						    		//Identifiers
	 boss_0012, sizeof(boss_0012)/sizeof(ATHEROS_REG_FILE),				//Register file
	 &maui2API, &sombrero_beanieAPI, &veniceArtAniAPI,					//APIs
	 2, boss_0012_mode, sizeof(boss_0012_mode)/sizeof(MODE_INFO),		//Mode file
	 CFG_VERSION_STRING_0012 },											//configuration string

	{DONT_MATCH, derby2_1Revs, sizeof(derby2_1Revs)/sizeof(ANALOG_REV), veniceRevs, sizeof(veniceRevs)/sizeof(A_UINT16), SW_DEVICE_ID_VENICE_DERBY2, //Identifiers
	 venice_derby2_1,  sizeof(venice_derby2_1)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,							//APIs
	 3, venice_derby2_1_mode, sizeof(venice_derby2_1_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d016 },											//configuration string

	//will only point to this structure if want to load EAR from EEPROM
	{DONT_MATCH, derby2Revs, sizeof(derby2Revs)/sizeof(ANALOG_REV), veniceRevs, sizeof(veniceRevs)/sizeof(A_UINT16), SW_DEVICE_ID_VENICE_DERBY2, //Identifiers
	 venice_derby2_1_ear,  sizeof(venice_derby2_1_ear)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,							//APIs
	 3, venice_derby2_1_mode_ear, sizeof(venice_derby2_1_mode_ear)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d016_EAR },											//configuration string

	{DONT_MATCH, derby2_1Revs, sizeof(derby2_1Revs)/sizeof(ANALOG_REV), hainanRevs, sizeof(hainanRevs)/sizeof(A_UINT16), SW_DEVICE_ID_HAINAN_DERBY, //Identifiers
	 hainan_derby2_1,  sizeof(hainan_derby2_1)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,								//APIs
	 3, hainan_derby2_1_mode, sizeof(hainan_derby2_1_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d017 },												//configuration string

	//will only point to this structure if want to load EAR from EEPROM
	//This is to support hainan ear, which must use same frozen venice_derby config file
	{DONT_MATCH, derby2Revs, sizeof(derby2Revs)/sizeof(ANALOG_REV), veniceRevs, sizeof(veniceRevs)/sizeof(A_UINT16), SW_DEVICE_ID_VENICE_DERBY2, //Identifiers
	 hainan_derby2_1_ear,  sizeof(hainan_derby2_1_ear)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,							//APIs
	 3, hainan_derby2_1_mode_ear, sizeof(hainan_derby2_1_mode_ear)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d017_EAR },											//configuration string

	{DONT_MATCH, sombreroRevs, sizeof(sombreroRevs)/sizeof(ANALOG_REV), veniceRevs, sizeof(veniceRevs)/sizeof(A_UINT16), SW_DEVICE_ID_VENICE_SOMBRERO,	//Identifiers
	 venice,  sizeof(venice)/sizeof(ATHEROS_REG_FILE),						//Register file
	 &veniceAPI, &sombrero_beanieAPI, &veniceArtAniAPI,						//APIs
	 3, venice_mode, sizeof(venice_mode)/sizeof(MODE_INFO),					//Mode file
	 CFG_VERSION_STRING_0013 },												//configuration string
#endif // AP22_AP

#if (defined(FREEDOM_AP)||defined(THIN_CLIENT_BUILD))&&!defined(COBRA_AP)
	{0xa014,  derby2Revs, sizeof(derby2Revs)/sizeof(ANALOG_REV), NULL, 0,	0xa016,										//Identifiers
	 freedom2_derby2,  sizeof(freedom2_derby2)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,								//APIs
	 3, freedom2_derby2_mode, sizeof(freedom2_derby2_mode)/sizeof(MODE_INFO), //Mode file 
	 CFG_VERSION_STRING_a016 },																	//configuration string

#endif

//	{0xa014,  derby2_1Revs, sizeof(derby2_1Revs)/sizeof(ANALOG_REV), NULL, 0,	0xa016,										//Identifiers
//	 freedom2_derby2_1,  sizeof(freedom2_derby2_1)/sizeof(ATHEROS_REG_FILE),		//Register file
//	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,								//APIs
//	 3, freedom2_derby2_1_mode, sizeof(freedom2_derby2_1_mode)/sizeof(MODE_INFO), //Mode file 
//	 CFG_VERSION_STRING_ad16 },																	//configuration string

//#endif

#ifdef SENAO_AP
	{DONT_MATCH, derby2_1Revs, sizeof(derby2_1Revs)/sizeof(ANALOG_REV), veniceRevs, sizeof(veniceRevs)/sizeof(A_UINT16), SW_DEVICE_ID_VENICE_DERBY2, //Identifiers
	 venice_derby2_1,  sizeof(venice_derby2_1)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,							//APIs
	 3, venice_derby2_1_mode, sizeof(venice_derby2_1_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d016 },											//configuration string

	//will only point to this structure if want to load EAR from EEPROM
	{DONT_MATCH, derby2Revs, sizeof(derby2Revs)/sizeof(ANALOG_REV), veniceRevs, sizeof(veniceRevs)/sizeof(A_UINT16), SW_DEVICE_ID_VENICE_DERBY2, //Identifiers
	 venice_derby2_1_ear,  sizeof(venice_derby2_1_ear)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,							//APIs
	 3, venice_derby2_1_mode_ear, sizeof(venice_derby2_1_mode_ear)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d016_EAR },											//configuration string

	{DONT_MATCH, derby2_1Revs, sizeof(derby2Revs)/sizeof(ANALOG_REV), hainanRevs, sizeof(hainanRevs)/sizeof(A_UINT16), SW_DEVICE_ID_HAINAN_DERBY, //Identifiers
	 hainan_derby2_1,  sizeof(hainan_derby2_1)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,								//APIs
	 3, hainan_derby2_1_mode, sizeof(hainan_derby2_1_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d017 },												//configuration string

	//will only point to this structure if want to load EAR from EEPROM
	//This is to support hainan ear, which must use same frozen venice_derby config file
	{DONT_MATCH, derby2Revs, sizeof(derby2Revs)/sizeof(ANALOG_REV), veniceRevs, sizeof(veniceRevs)/sizeof(A_UINT16), SW_DEVICE_ID_VENICE_DERBY2, //Identifiers
	 hainan_derby2_1_ear,  sizeof(hainan_derby2_1_ear)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &derbyAPI,	&veniceArtAniAPI,							//APIs
	 3, hainan_derby2_1_mode_ear, sizeof(hainan_derby2_1_mode_ear)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d017_EAR },											//configuration string

	{DONT_MATCH, griffin2_AnalogRevs,  sizeof(griffin2_AnalogRevs)/sizeof(ANALOG_REV), griffinRevs,sizeof(griffinRevs)/sizeof(A_UINT16), SW_DEVICE_ID_GRIFFIN, //Identifiers
	 griffin2,  sizeof(griffin2)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, griffin2_mode, sizeof(griffin2_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_c018 },												//configuration string 

//will only point to this structure if want to load EAR from EEPROM
	//This is to support hainan ear, which must use same frozen venice_derby config file
	{DONT_MATCH, griffinAnalogRevs,sizeof(griffinAnalogRevs)/sizeof(ANALOG_REV),griffinRevs,sizeof(griffinRevs)/sizeof(A_UINT16),SW_DEVICE_ID_GRIFFIN, //Identifiers
	 hainan_derby2_1_ear,  sizeof(hainan_derby2_1_ear)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &veniceAPI, &griffinRfAPI,	&veniceArtAniAPI,							//APIs
	 3, hainan_derby2_1_mode_ear, sizeof(hainan_derby2_1_mode_ear)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d017_EAR },	


	{DONT_MATCH, eagle2AnalogRevs, sizeof(eagle2AnalogRevs)/sizeof(ANALOG_REV), eagle2Revs, sizeof(eagle2Revs)/sizeof(A_UINT16), SW_DEVICE_ID_EAGLE, //Identifiers
	 eagle2,  sizeof(eagle2)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &eagleAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, eagle2_mode, sizeof(eagle2_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d019 },												//configuration string

	//will only point to this structure if want to load EAR from EEPROM
	//This is to support hainan ear, which must use same frozen venice_derby config file
	{DONT_MATCH, eagle2AnalogRevs, sizeof(eagle2AnalogRevs)/sizeof(ANALOG_REV), eagle2Revs, sizeof(eagle2Revs)/sizeof(A_UINT16), SW_DEVICE_ID_GRIFFIN, //Identifiers
	 hainan_derby2_1_ear,  sizeof(hainan_derby2_1_ear)/sizeof(ATHEROS_REG_FILE),		//Register file
	 &eagleAPI, &griffinRfAPI,	&veniceArtAniAPI,							//APIs
	 3, hainan_derby2_1_mode_ear, sizeof(hainan_derby2_1_mode_ear)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d017_EAR },											//configuration string
#endif // SENAO_AP

#ifdef COBRA_AP
	{DEVICE_ID_COBRA, cobraAnalogRevs,  sizeof(cobraAnalogRevs)/sizeof(ANALOG_REV), cobraRevs, sizeof(cobraRevs)/sizeof(A_UINT16), SW_DEVICE_ID_AP51, //Identifiers
	 cobra,  sizeof(cobra)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, cobra_mode, sizeof(cobra_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_0020 },												//configuration string

	{DEVICE_ID_COBRA, spiderAnalogRevs,  sizeof(spiderAnalogRevs)/sizeof(ANALOG_REV), cobraRevs, sizeof(cobraRevs)/sizeof(A_UINT16), DEVICE_ID_SPIDER1_0, //Identifiers
	 spider,  sizeof(spider)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &veniceAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, spider_mode, sizeof(spider_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_a055 },												//configuration string

#ifdef PCI_INTERFACE
	{DONT_MATCH, eagle2AnalogRevs, sizeof(eagle2AnalogRevs)/sizeof(ANALOG_REV), eagle2Revs, sizeof(eagle2Revs)/sizeof(A_UINT16), SW_DEVICE_ID_EAGLE, //Identifiers
	 eagle2,  sizeof(eagle2)/sizeof(ATHEROS_REG_FILE),	//Register file
	 &eagleAPI, &griffinRfAPI,	&veniceArtAniAPI,								//APIs
	 3, eagle2_mode, sizeof(eagle2_mode)/sizeof(MODE_INFO), //Mode file
	 CFG_VERSION_STRING_d019 },												//configuration string
#endif

#endif // Cobra AP


};

A_UINT32 numDeviceIDs = (sizeof(ar5kInitData)/sizeof(DEVICE_INIT_DATA));



