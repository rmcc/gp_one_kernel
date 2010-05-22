#ifdef _WINDOWS
#include <windows.h>
#endif 
#include <stdio.h>
#include <time.h>
#if !defined(LINUX) && !defined(__linux__)
#include <conio.h>
#endif
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include "wlantype.h"   /* typedefs for A_UINT16 etc.. */
#if !defined(ZEROCAL_TOOL)
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

#include "ar2413/mEEPROM_g.h"
#include "ar5212/mEEPROM_d.h"
#include "cal_gen5.h"
#else
#define  AR6000_TX_GAIN_TBL_SIZE  22
#endif // #if !defined(ZEROCAL_TOOL)

//#if defined(LINUX) || defined(__linux__)
#include "mEepStruct6000.h"
//#elif defined(_WINDOWS)
//#include "ar6000/mEepStruct6000.h"
//#endif

#if !defined(ZEROCAL_TOOL)

extern  RAW_DATA_STRUCT_GEN5 *pRawDataset_gen5[] ; 
extern TARGETS_SET      *pTargetsSet;
extern TARGETS_SET      *pTargetsSet_2p4[];
extern YIELD_LOG_STRUCT yldStruct;
extern CAL_SETUP        CalSetup;
extern MANLIB_API A_UINT8 dummyEepromWriteArea[AR6K_EEPROM_SIZE];

static void ar6000EepromDump(A_UINT32 devNum, AR6K_EEPROM *);

static void fillTxGainTbl(A_UINT32 devNum, MODAL_EEP_HEADER *pModal, A_UINT32 mode);
static void fillHeader(A_UINT32 devNum, AR6K_EEPROM *pEepStruct);
static void fillCalData (AR6K_EEPROM *pEepStruct);
static void fillPwrVpdData (CAL_DATA_PER_FREQ *pCalData, RAW_DATA_PER_CHANNEL_GEN5 *pCalCh, A_UINT32 pierIndex);
static void fillTargetPowerData (AR6K_EEPROM *pEepStruct);
static void fillCtlData (AR6K_EEPROM *pEepStruct);
static void fillNFData (AR6K_EEPROM *pEepStruct);
void computeChecksum (AR6K_EEPROM *pEepStruct);
void writeEepromStruct (A_UINT32 devNum, AR6K_EEPROM *pEepStruct);
static void fillAR6000EepromLabel (A_UINT32 devNum,  AR6K_EEPROM *pEepStruct) ;
//static A_BOOL eepStructValid = FALSE;
A_BOOL writeCalDataToFile(void * pEepStruct);

#if defined(_WINDOWS)
#define A_MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

void programCompactEeprom(
 A_UINT32 devNum
)
{
    AR6K_EEPROM eepStruct;
    void *pTempPtr;
	
    //zero out the structure
    memset((void *)&eepStruct, 0, sizeof(AR6K_EEPROM));

    //base structure (some of it)
    eepStruct.baseEepHeader.version = (AR6000_EEP_VER << 12);
    // zero checksum
    eepStruct.baseEepHeader.checksum = 0x0000;
    eepStruct.baseEepHeader.length = AR6K_EEPROM_SIZE;
    eepStruct.baseEepHeader.regDmn = (A_UINT16)(((CalSetup.countryOrDomain & 0x1) << 15) | ((CalSetup.worldWideRoaming & 0x1) << 14) | 
        (CalSetup.countryOrDomainCode & 0xfff)) ;
    if(CalSetup.countryOrDomain && CalSetup.worldWideRoaming && (CalSetup.countryOrDomainCode & 0xfff)) {
        //set the other 2 bits of regDmn to 1
        eepStruct.baseEepHeader.regDmn = (A_UINT16)(eepStruct.baseEepHeader.regDmn | (0x3 << 12));
    }
    else {
        //clear the two bits
        eepStruct.baseEepHeader.regDmn = (eepStruct.baseEepHeader.regDmn & 0xCFFF);
    }

    //mac address - assuming label scheme for now - need to make some more changes for customer release    
    if(configSetup.enableLabelScheme) {
        eepStruct.baseEepHeader.macAddr[0] = (A_UCHAR)((yldStruct.macID1[0] >> 8) & 0xFF);	
	eepStruct.baseEepHeader.macAddr[1] = (A_UCHAR)((yldStruct.macID1[0]) & 0xFF);	
	eepStruct.baseEepHeader.macAddr[2] = (A_UCHAR)((yldStruct.macID1[1] >> 8) & 0xFF);
	eepStruct.baseEepHeader.macAddr[3] = (A_UCHAR)((yldStruct.macID1[1]) & 0xFF);
	eepStruct.baseEepHeader.macAddr[4] = (A_UCHAR)((yldStruct.macID1[2] >> 8) & 0xFF);
	eepStruct.baseEepHeader.macAddr[5] = (A_UCHAR)((yldStruct.macID1[2]) & 0xFF);
    }
    else //write 1's, so WLAN driver will load even before real address is written.
    {
        eepStruct.baseEepHeader.macAddr[0] = (A_UCHAR)(0xFF);	
	eepStruct.baseEepHeader.macAddr[1] = (A_UCHAR)(0xFF);	
	eepStruct.baseEepHeader.macAddr[2] = (A_UCHAR)(0xFF);
	eepStruct.baseEepHeader.macAddr[3] = (A_UCHAR)(0xFF);
	eepStruct.baseEepHeader.macAddr[4] = (A_UCHAR)(0xFF);
	eepStruct.baseEepHeader.macAddr[5] = (A_UCHAR)(0xFF);
    }    

    eepStruct.baseEepHeader.opFlags = ((CalSetup.Amode) ? AR6000_OPFLAGS_11A : 0) | 
        ((CalSetup.Gmode) ? AR6000_OPFLAGS_11G : 0);
#if defined(AR6002)
    if (1 == CalSetup.opFlagLna2FrontEnd) 
        eepStruct.baseEepHeader.opFlags |= AR6000_OPFLAGS_LNA2_FRONTEND; 
    else if (2 == CalSetup.opFlagLna2FrontEnd) { 
        eepStruct.baseEepHeader.opFlags |= AR6000_OPFLAGS_LNA2_FE_NOXPA; 
        eepStruct.baseEepHeader.opFlags |= AR6000_OPFLAGS_LNA2_FRONTEND; 
    }
    eepStruct.baseEepHeader.opFlags |= ((CalSetup.opFlag0dBm) ? AR6000_OPFLAGS_0dBm : 0); 

    eepStruct.baseEepHeader.opFlags |= ((CalSetup.opFlagTxGainTbl) ? AR6000_OPFLAGS_TXGAIN_TBL : 0); 
    eepStruct.baseEepHeader.opFlags |= ((CalSetup.opFlagAntDiversity) ? AR6000_OPFLAGS_ANT_DIVERSITY : 0); 

#endif
    //uiPrintf("........opFlag 0x%x\n", eepStruct.baseEepHeader.opFlags);
    
    {
        A_UINT16 i, startIdx, endIdx;
#if defined(AR6001)
        A_UINT16  *pSubSystemID = (A_UINT16 *)(&(eepStruct.baseEepHeader.custData[0]));
        *pSubSystemID = CalSetup.subsystemID;
        startIdx = 2;
#elif defined(AR6002)
        eepStruct.baseEepHeader.subSystemId = (A_UINT16)CalSetup.subsystemID;
        /* Utilize the binBuildNumber, set it as  (AR6000_EEP_VER already in the version)
               ART_VERSION_MAJOR    31:24
               ART_VERSION_MINOR    23:16
               ART_BUILD_NUM        15:8
               AR6000_EEP_VER_MINOR  8:0 */
        eepStruct.baseEepHeader.binBuildNumber = (ART_VERSION_MAJOR << 24) | (ART_VERSION_MINOR << 16) |
            (ART_BUILD_NUM << 8) | AR6000_EEP_VER_MINOR;
        eepStruct.baseEepHeader.negPwrOffset = (A_INT8)CalSetup.negPwrOffset;
        eepStruct.baseEepHeader.cckOfdmDelta = (A_INT8)(2*CalSetup.cck_ofdm_delta);
        startIdx = 0;
#else
#error "Unknown arch"
#endif
	   
        endIdx = sizeof(eepStruct.baseEepHeader.custData);
        //uiPrintf("---------sizeOfCustData %d\n", endIdx);
	for(i = startIdx; i < endIdx; i++) {
	    eepStruct.baseEepHeader.custData[i] = 0xff;
	}   
    }


    //fill in the header information
    fillHeader(devNum, &eepStruct);

    //Would fill in the spur information here, leave these for now - default to no spurs
    eepStruct.spurChans[0][0] = 0x8000;
    eepStruct.spurChans[1][0] = 0x8000;


    //fill in the calibration information
    fillCalData(&eepStruct);


    //fill the target power information
    fillTargetPowerData(&eepStruct);

    //fill the CTL information
    fillCtlData(&eepStruct);

#if defined(AR6002)
    fillNFData(&eepStruct);
#endif

    if(configSetup.enableLabelScheme) {
        //fill the label - again assuming label scheme for now, change for customer release
	fillAR6000EepromLabel(devNum, &eepStruct);
    }

    //Compute checksum once all other areas are filled.
    computeChecksum(&eepStruct);

    if(CalSetup.writeToFile) {
        writeCalDataToFile(&eepStruct);
    }

    //write struct to eeprom 
    writeEepromStruct(devNum, &eepStruct);
    
#if defined(_WINDOWS)
    if( configSetup.eepromLoad) {

		art_GetEepromStruct(devNum, EEP_AR6000, &pTempPtr);
	}
    else {
        /* use dummyEepromWriteArea as the storage buffer for programMacID */
        pTempPtr = dummyEepromWriteArea; 
    }
    memcpy(pTempPtr, &eepStruct, sizeof(eepStruct));
#endif
    if (!configSetup.eepromPresent) {
        uiPrintf(".update dummyEepromWritArea\n");
        pTempPtr = dummyEepromWriteArea; 
        memcpy(pTempPtr, &eepStruct, sizeof(eepStruct));
    }

    //ar6000EepromDump(devNum, &eepStruct);    
	
}


static void fillNFData( AR6K_EEPROM *pEepStruct)
{
    /* place holders for now */
    memset((void*)&(pEepStruct->calNFData11A[0]), 0, (AR6000_NUM_11A_CAL_PIERS * sizeof(CAL_NF_PER_FREQ)));
    memset((void*)&(pEepStruct->calNFData11G[0]), 0, (AR6000_NUM_11G_CAL_PIERS * sizeof(CAL_NF_PER_FREQ)));
    
}
 
static void fillTxGainTbl(A_UINT32 devNum, MODAL_EEP_HEADER *pModal, A_UINT32 mode)
{
    A_UINT32 i, val, curVal;
    A_CHAR fieldName[50];
    A_UINT8 deltaVal;

    curVal = art_getFieldForMode(devNum,"bb_tx_gain_table_0", mode, BASE);
    curVal = (curVal >> TXGAIN_TABLE_GAININHALFDB_LSB) & TXGAIN_TABLE_GAININHALFDB_MASK;
    pModal->txGainTbl_0 = (A_UINT8)curVal;

    for (i=0; i<AR6000_TX_GAIN_TBL_SIZE; i++) {
        sprintf(fieldName, "%s%d", TXGAIN_TABLE_STRING, i);
        val = art_getFieldForMode(devNum, fieldName, mode, BASE);
        val = (val >> TXGAIN_TABLE_GAININHALFDB_LSB) & TXGAIN_TABLE_GAININHALFDB_MASK;
        deltaVal = (A_UINT8) (val - curVal);
        curVal = val;
        if (i %2) {  
            //pModal->txGainTblDelta[idx] &= (A_UINT8)~0xf0;
            pModal->txGainTblDelta[i/2] |= (A_UINT8)((deltaVal<<4) &0xf0);
        }
        else { // 0 & even starts from lower nible
            //pModal->txGainTblDelta[idx] &= (A_UINT8)~0xf;
            pModal->txGainTblDelta[i/2] |= (A_UINT8)(deltaVal &0xf);
        }

        // life would be much simpler if we had more eeprom ...
        //pModal->txGainTbl[i] = (A_UINT8) ((val >> TXGAIN_TABLE_GAININHALFDB_LSB) & TXGAIN_TABLE_GAININHALFDB_MASK);
    }

}

static void fillHeader(A_UINT32 devNum, AR6K_EEPROM *pEepStruct)
{
    MODAL_EEP_HEADER *pModal;
    

    /*---------------- 11a header -------------------*/
    pModal = &(pEepStruct->modalHeader[0]);
    
    //antennaControl
    pModal->antCtrl0 = (A_UINT8)(CalSetup.antennaControl[0] & 0xff);
    pModal->antCtrl[0] = CalSetup.antennaControl[1] & ANTCTRL_MASK;
    pModal->antCtrl[0] |= ((CalSetup.antennaControl[2] & ANTCTRL_MASK) << ANTCTRL_SHIFT);
    pModal->antCtrl[0] |= ((CalSetup.antennaControl[3] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *2));
    pModal->antCtrl[0] |= ((CalSetup.antennaControl[4] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *3));
    pModal->antCtrl[0] |= ((CalSetup.antennaControl[5] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *4));
    pModal->antCtrl[1] = CalSetup.antennaControl[6] & ANTCTRL_MASK;
    pModal->antCtrl[1] |= ((CalSetup.antennaControl[7] & ANTCTRL_MASK) << ANTCTRL_SHIFT);
    pModal->antCtrl[1] |= ((CalSetup.antennaControl[8] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *2));
    pModal->antCtrl[1] |= ((CalSetup.antennaControl[9] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *3));
    pModal->antCtrl[1] |= ((CalSetup.antennaControl[10] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *4));
    /*uiPrintf("......A CalSetup.antCtrl %x %x %x %x %x %x %x %x %x %x\n", CalSetup.antennaControl[1], 
        CalSetup.antennaControl[2], CalSetup.antennaControl[3], CalSetup.antennaControl[4], CalSetup.antennaControl[5], 
        CalSetup.antennaControl[6], CalSetup.antennaControl[7], CalSetup.antennaControl[8], CalSetup.antennaControl[9], 
        CalSetup.antennaControl[10]);
    uiPrintf("......A antCtrl0 0x%x antCtrl1 0x%x\n", pModal->antCtrl[0], pModal->antCtrl[1]); */
    pModal->antennaGain = CalSetup.antennaGain5G & 0xff;
    pModal->switchSettling = (A_UINT8)(CalSetup.switchSettling & 0x7f);

#if defined(AR6001)
    pModal->txRxAtten = (A_UINT8)(CalSetup.txrxAtten & 0x3f);
    pModal->rxTxMargin = (A_UINT8)(CalSetup.rxtx_margin[MODE_11a] & 0x3F);
#elif defined(AR6002)
    pModal->xAtten1Hyst = (A_UINT8)(CalSetup.xAtten1Hyst_11a & 0xff);
    pModal->xAtten1Margin  = (A_UINT8)(CalSetup.xAtten1Margin_11a & 0xff);
    pModal->xAtten1Db = (A_UINT8)(CalSetup.xAtten1Db_11a & 0xff);
    pModal->xAtten2Hyst = (A_UINT8)(CalSetup.xAtten2Hyst_11a & 0xff);
    pModal->xAtten2Margin  = (A_UINT8)(CalSetup.xAtten2Margin_11a & 0xff);
    pModal->xAtten2Db = (A_UINT8)(CalSetup.xAtten2Db_11a & 0xff); 
#endif 

    pModal->adcDesiredSize = CalSetup.adcDesiredSize & 0xff;

#if defined(AR6001)
    pModal->pgaDesiredSize = CalSetup.pgaDesiredSize & 0xff;
#endif
    pModal->txEndToXlnaOn = (A_UINT8)(CalSetup.txEndToXLNAOn & 0xff);
    pModal->xlnaGain = (A_UINT8)(CalSetup.xlnaGain & 0xff);
    pModal->txEndToXpaOff = (A_UINT8)(CalSetup.txEndToXPAOff & 0xff);
    pModal->txFrameToXpaOn = (A_UINT8)(CalSetup.txFrameToXPAOn & 0xff);
#if defined(AR6001)
    pModal->thresh62 = (A_UINT8)(CalSetup.thresh62 & 0xff);
#elif defined(AR6002)
    pModal->thresh62 = (A_INT8)(CalSetup.thresh62 & 0xff);
#endif
    pModal->noiseFloorThresh = CalSetup.noisefloor_thresh & 0xff;
#if defined(AR6001)
    pModal->xpdGain = (A_UINT8)(CalSetup.xgain & 0xf);
    pModal->xpd = (A_UINT8)(CalSetup.xpd & 0x1);
#elif defined(AR6002)
    pModal->xpdGain = (A_UINT8)(CalSetup.xgain & 0xff);
    pModal->xpd = (A_UINT8)(CalSetup.xpd & 0xff);
#endif
    
    //Set IQ coeffs to 0 right now, at some point we may decide we need it
    //at that point verify that the data is being correctly held for 
    //signed use.
    pModal->iqCalI = (A_INT8)CalSetup.iqcal_i_corr[MODE_11a] & 0x3f;
    pModal->iqCalQ = (A_INT8)CalSetup.iqcal_q_corr[MODE_11a] & 0x1f;

#if defined(AR6002)
    pModal->pdGainOverlap = CalSetup.pdGainOverlap & 0xff;
    pModal->ob = (A_UINT8)CalSetup.ob_1 & 0xff;
    pModal->db = (A_UINT8)CalSetup.db_1 & 0xff;
    pModal->xpaBiasLvl = (A_UINT8)CalSetup.xpaBiasLvl_11a & 0xff; 
    pModal->txPowerOffset = CalSetup.txPowerOffset_11a & 0xff; 

    pModal->sellna = (A_UINT8) CalSetup.selLna; 
    pModal->selintpd = (A_UINT8) CalSetup.selIntPd; 
    pModal->enablePca = (A_UINT8) CalSetup.enablePCA_11a; 
    pModal->enablePcb = (A_UINT8) CalSetup.enablePCB_11a; 
    pModal->enableXpaa = (A_UINT8) CalSetup.enableXpaA_11a; 
    pModal->enableXpab = (A_UINT8) CalSetup.enableXpaB_11a; 
    pModal->useTxPdInXpa = (A_UINT8) CalSetup.useTxPDinXpa; 

    pModal->xpaBiasLvl2 = (A_UINT8)CalSetup.xpaBiasLvl2_11a & 0xff; 
    pModal->initTxGain = (A_UINT8) CalSetup.initTxGain_11a & 0x2f; 

    fillTxGainTbl(devNum, pModal, MODE_11A);

    memset((void *)&(pModal->futureModal), 0, sizeof(pModal->futureModal));
#endif

    /*---------------- 11g header info -------------------*/
    pModal = &(pEepStruct->modalHeader[1]);

    //antennaControl   
    pModal->antCtrl0 = (A_UINT8)(CalSetup.antennaControl_2p4[MODE_11g][0]);
    pModal->antCtrl[0] = CalSetup.antennaControl_2p4[MODE_11g][1] & ANTCTRL_MASK;
    pModal->antCtrl[0] |= ((CalSetup.antennaControl_2p4[MODE_11g][2] & ANTCTRL_MASK) << ANTCTRL_SHIFT);
    pModal->antCtrl[0] |= ((CalSetup.antennaControl_2p4[MODE_11g][3] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *2));
    pModal->antCtrl[0] |= ((CalSetup.antennaControl_2p4[MODE_11g][4] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *3));
    pModal->antCtrl[0] |= ((CalSetup.antennaControl_2p4[MODE_11g][5] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *4));
    pModal->antCtrl[1] = CalSetup.antennaControl_2p4[MODE_11g][6] & ANTCTRL_MASK;
    pModal->antCtrl[1] |= ((CalSetup.antennaControl_2p4[MODE_11g][7] & ANTCTRL_MASK) << ANTCTRL_SHIFT);
    pModal->antCtrl[1] |= ((CalSetup.antennaControl_2p4[MODE_11g][8] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *2));
    pModal->antCtrl[1] |= ((CalSetup.antennaControl_2p4[MODE_11g][9] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *3));
    pModal->antCtrl[1] |= ((CalSetup.antennaControl_2p4[MODE_11g][10] & ANTCTRL_MASK) << (ANTCTRL_SHIFT *4));
    /*uiPrintf("......G CalSetup.antCtrl %x %x %x %x %x %x %x %x %x %x\n", CalSetup.antennaControl_2p4[MODE_11g][1]& ANTCTRL_MASK, 
        CalSetup.antennaControl_2p4[MODE_11g][2]& ANTCTRL_MASK, CalSetup.antennaControl_2p4[MODE_11g][3]& ANTCTRL_MASK, 
        CalSetup.antennaControl_2p4[MODE_11g][4]& ANTCTRL_MASK, CalSetup.antennaControl_2p4[MODE_11g][5] & ANTCTRL_MASK, 
        CalSetup.antennaControl_2p4[MODE_11g][6]& ANTCTRL_MASK, CalSetup.antennaControl_2p4[MODE_11g][7]& ANTCTRL_MASK, 
        CalSetup.antennaControl_2p4[MODE_11g][8]& ANTCTRL_MASK, 
        CalSetup.antennaControl_2p4[MODE_11g][9]& ANTCTRL_MASK, CalSetup.antennaControl_2p4[MODE_11g][10]& ANTCTRL_MASK);
    uiPrintf("......G antCtrl0 0x%x antCtrl1 0x%x\n", pModal->antCtrl[0], pModal->antCtrl[1]); */

    pModal->antennaGain = CalSetup.antennaGain2p5G & 0xff;
    pModal->switchSettling = (A_UINT8)(CalSetup.switchSettling_2p4[MODE_11g] & 0x7f);
#if defined(AR6001)
    pModal->txRxAtten = (A_UINT8)(CalSetup.txrxAtten_2p4[MODE_11g] & 0x3f);
    pModal->rxTxMargin = (A_UINT8)(CalSetup.rxtx_margin[MODE_11g] & 0x3F);
#elif defined(AR6002)
    pModal->xAtten1Hyst = (A_UINT8)(CalSetup.xAtten1Hyst_11g & 0xff);
    pModal->xAtten1Margin  = (A_UINT8)(CalSetup.xAtten1Margin_11g & 0xff);
    pModal->xAtten1Db = (A_UINT8)(CalSetup.xAtten1Db_11g & 0xff);
    pModal->xAtten2Hyst = (A_UINT8)(CalSetup.xAtten2Hyst_11g & 0xff);
    pModal->xAtten2Margin  = (A_UINT8)(CalSetup.xAtten2Margin_11g & 0xff);
    pModal->xAtten2Db = (A_UINT8)(CalSetup.xAtten2Db_11g & 0xff); 
#endif

    pModal->adcDesiredSize = CalSetup.adcDesiredSize_2p4[MODE_11g] & 0xff;

#if defined(AR6001)
    pModal->pgaDesiredSize = CalSetup.pgaDesiredSize_2p4[MODE_11g] & 0xff;
#endif

    pModal->txEndToXlnaOn = (A_UINT8)(CalSetup.txEndToXLNAOn_2p4[MODE_11g] & 0xff);
    pModal->xlnaGain = (A_UINT8)(CalSetup.xlnaGain_2p4[MODE_11g] & 0xff);
    pModal->txEndToXpaOff = (A_UINT8)(CalSetup.txEndToXPAOff_2p4[MODE_11g] & 0xff);
    pModal->txFrameToXpaOn = (A_UINT8)(CalSetup.txFrameToXPAOn_2p4[MODE_11g] & 0xff);
#if defined(AR6001)
    pModal->thresh62 = (A_UINT8)(CalSetup.thresh62_2p4[MODE_11g] & 0xff);
#elif defined(AR6002)
    pModal->thresh62 = (A_INT8)(CalSetup.thresh62_2p4[MODE_11g] & 0xff);
#endif
    pModal->noiseFloorThresh = CalSetup.noisefloor_thresh_2p4[MODE_11g] & 0xff;
#if defined(AR6001)
    pModal->xpdGain = (A_UINT8)(CalSetup.xgain_2p4[MODE_11g] & 0xf);
    pModal->xpd = (A_UINT8)(CalSetup.xpd_2p4[MODE_11g] & 0x1);
#elif defined(AR6002)
    pModal->xpdGain = (A_UINT8)(CalSetup.xgain_2p4[MODE_11g] & 0xff);
    pModal->xpd = (A_UINT8)(CalSetup.xpd_2p4[MODE_11g] & 0xff);
#endif

    //Set IQ coeffs to 0 right now, at some point we may decide we need it
    //at that point verify that the data is being correctly held for 
    //signed use.
    pModal->iqCalI = (A_INT8)CalSetup.iqcal_i_corr[MODE_11g] & 0x3f;
    pModal->iqCalQ = (A_INT8)CalSetup.iqcal_q_corr[MODE_11g] & 0x1f;

#if defined(AR6002)
    pModal->pdGainOverlap = (A_UINT8)CalSetup.pdGainOverlap_11g & 0xff;
    pModal->ob = (A_UINT8)CalSetup.ob_2p4[MODE_11g] & 0xff;
    pModal->db = (A_UINT8)CalSetup.db_2p4[MODE_11g] & 0xff;
    pModal->xpaBiasLvl = (A_UINT8)CalSetup.xpaBiasLvl_11g & 0xff; 
    pModal->txPowerOffset = CalSetup.txPowerOffset_11g & 0xff; 

    pModal->sellna = (A_UINT8) CalSetup.selLna; 
    pModal->selintpd = (A_UINT8) CalSetup.selIntPd; 
    pModal->enablePca = (A_UINT8) CalSetup.enablePCA_11g; 
    pModal->enablePcb = (A_UINT8) CalSetup.enablePCB_11g; 
    pModal->enableXpaa = (A_UINT8) CalSetup.enableXpaA_11g; 
    pModal->enableXpab = (A_UINT8) CalSetup.enableXpaB_11g; 
    pModal->useTxPdInXpa = (A_UINT8) CalSetup.useTxPDinXpa; 

    pModal->xpaBiasLvl2 = (A_UINT8)CalSetup.xpaBiasLvl2_11g & 0xff; 
    pModal->initTxGain = (A_UINT8) CalSetup.initTxGain_11g & 0x2f; 

    fillTxGainTbl(devNum, pModal, MODE_11G);

    memset((void *)&(pModal->futureModal), 0, sizeof(pModal->futureModal));
#endif
}

void fillCalData (
 AR6K_EEPROM *pEepStruct
)
{
    A_UINT32 ii, jj;
    A_UINT32 numPiers;
    A_UINT16 freqmask = 0xff;
    RAW_DATA_PER_CHANNEL_GEN5 *pCalCh;

    //fill 11a frequency piers and cal data
    numPiers = pRawDataset_gen5[MODE_11a]->numChannels;

    if (numPiers > AR6000_NUM_11A_CAL_PIERS) {
        numPiers = AR6000_NUM_11A_CAL_PIERS;
    }

    for (ii=0; ii<numPiers; ii++)
    {
        //frequency pier
        pEepStruct->calFreqPier11A[ii] = (A_UINT8)(freq2fbin(pRawDataset_gen5[MODE_11a]->pChannels[ii]));

        pCalCh = &(pRawDataset_gen5[MODE_11a]->pDataPerChannel[ii]);

        if((pCalCh->numPdGains > AR6000_NUM_PD_GAINS) || (pCalCh->numPdGains == 0)) {
            uiPrintf("Number of pdgains must be 1 or 2 for ar6000 chipset.  Exiting...\n");
            exit(0);
        }
        
        //fill the 11a calibration values
        fillPwrVpdData(pEepStruct->calPierData11A, pCalCh, ii);
    }
    
    if(numPiers < AR6000_NUM_11A_CAL_PIERS) {
        for(jj = ii ; jj < AR6000_NUM_11A_CAL_PIERS; jj++) {
            pEepStruct->calFreqPier11A[jj] = 0xff;

        }
    }


    //11g frequency piers
    numPiers = CalSetup.numForcedPiers_2p4[MODE_11g];
    if (numPiers > AR6000_NUM_11G_CAL_PIERS) {
        numPiers = AR6000_NUM_11G_CAL_PIERS;
    }

    for(ii = 0; ii < numPiers; ii++) {
        pEepStruct->calFreqPier11G[ii] = (A_UINT8)(freq2fbin(CalSetup.piersList_2p4[MODE_11g][ii]));

        pCalCh = &(pRawDataset_gen5[MODE_11g]->pDataPerChannel[ii]);

        if((pCalCh->numPdGains > AR6000_NUM_PD_GAINS) || (pCalCh->numPdGains == 0)) {
            uiPrintf("Number of pdgains must be 1 or 2 for ar6000 chipset.  Exiting...\n");
            exit(0);
        }
        
        //fill the 11g calibration values
        fillPwrVpdData(pEepStruct->calPierData11G, pCalCh, ii);
    }
    if(numPiers < AR6000_NUM_11G_CAL_PIERS) {
        for(jj = ii ; jj < AR6000_NUM_11G_CAL_PIERS; jj++) {
            //pEepStruct->calFreqPier11G[jj] = (A_UINT8)(freq2fbin(CalSetup.piersList_2p4[MODE_11g][ii - 1]));
            pEepStruct->calFreqPier11G[jj] = 0xff;

            //pCalCh = &(pRawDataset_gen5[MODE_11g]->pDataPerChannel[ii - 1]);

            //fill the 11g calibration values
            //fillPwrVpdData(pEepStruct->calPierData11G, pCalCh, jj);
            
        }
    }
}


void fillPwrVpdData (
 CAL_DATA_PER_FREQ *pCalData,
 RAW_DATA_PER_CHANNEL_GEN5 *pCalCh,
 A_UINT32 pierIndex
)
{
    A_UINT16 IndexPdg1, ii;
    
    if(pCalCh->numPdGains == AR6000_NUM_PD_GAINS) {
        //only fill pdg0 if there are 2 pdgains
        pCalData[pierIndex].pwrPdg0[0] = (A_UINT8)(pCalCh->pDataPerPDGain[0].pwr_t4[0]);
        pCalData[pierIndex].vpdPdg0[0] = (A_UINT8)(pCalCh->pDataPerPDGain[0].Vpd[0]);

        pCalData[pierIndex].pwrPdg0[1] = (A_UINT8)(pCalCh->pDataPerPDGain[0].pwr_t4[1]);
        pCalData[pierIndex].vpdPdg0[1] = (A_UINT8)(pCalCh->pDataPerPDGain[0].Vpd[1]);

        pCalData[pierIndex].pwrPdg0[2] = (A_UINT8)(pCalCh->pDataPerPDGain[0].pwr_t4[2]);
        pCalData[pierIndex].vpdPdg0[2] = (A_UINT8)(pCalCh->pDataPerPDGain[0].Vpd[2]);
        
        pCalData[pierIndex].pwrPdg0[3] = (A_UINT8)(pCalCh->pDataPerPDGain[0].pwr_t4[3]);
        pCalData[pierIndex].vpdPdg0[3] = (A_UINT8)(pCalCh->pDataPerPDGain[0].Vpd[3]);
        
/*        //note that the cal structures hold the delta pwr and Vpd, but ar6000 eeprom
        //struct holds the absolute value, so need to do the addition of the delta here
        pEepStruct->calPierData11A[pierIndex].pwrPdg0[1] = 
            pEepStruct->calPierData11A[pierIndex].pwrPdg0[0] + pCalCh->pwr_delta_t2[0][0];
        pEepStruct->calPierData11A[pierIndex].vpdPdg0[1] = 
            pEepStruct->calPierData11A[pierIndex].vpdPdg0[0] + pCalCh->Vpd_delta[0][0];

        pEepStruct->calPierData11A[pierIndex].pwrPdg0[2] = 
            pEepStruct->calPierData11A[pierIndex].pwrPdg0[1] + pCalCh->pwr_delta_t2[1][0];
        pEepStruct->calPierData11A[pierIndex].vpdPdg0[2] = 
            pEepStruct->calPierData11A[pierIndex].vpdPdg0[1] + pCalCh->Vpd_delta[1][0];

        pEepStruct->calPierData11A[pierIndex].pwrPdg0[3] = 
            pEepStruct->calPierData11A[pierIndex].pwrPdg0[2] + pCalCh->pwr_delta_t2[2][0];
        pEepStruct->calPierData11A[pierIndex].vpdPdg0[3] = 
            pEepStruct->calPierData11A[pierIndex].vpdPdg0[2] + pCalCh->Vpd_delta[2][0];
*/
        IndexPdg1 = 1;
    }
    else {
        //only fill the second pdg
        IndexPdg1 = 0;
    }

    //fill in the 5 point cal values
    for(ii = 0; ii< 5; ii++) {
        pCalData[pierIndex].pwrPdg1[ii] = (A_UINT8)(pCalCh->pDataPerPDGain[IndexPdg1].pwr_t4[ii]);
        pCalData[pierIndex].vpdPdg1[ii] = (A_UINT8)(pCalCh->pDataPerPDGain[IndexPdg1].Vpd[ii]);
    }

/*    pEepStruct->calPierData11A[pierIndex].pwrPdg1[1] = 
        pEepStruct->calPierData11A[pierIndex].pwrPdg0[0] + pCalCh->pwr_delta_t2[0][IndexPdg1];
    pEepStruct->calPierData11A[pierIndex].vpdPdg1[1] = 
        pEepStruct->calPierData11A[pierIndex].vpdPdg0[0] + pCalCh->Vpd_delta[0][IndexPdg1];

    pEepStruct->calPierData11A[pierIndex].pwrPdg1[2] = 
        pEepStruct->calPierData11A[pierIndex].pwrPdg0[1] + pCalCh->pwr_delta_t2[1][IndexPdg1];
    pEepStruct->calPierData11A[pierIndex].vpdPdg1[2] = 
        pEepStruct->calPierData11A[pierIndex].vpdPdg0[1] + pCalCh->Vpd_delta[1][IndexPdg1];

    pEepStruct->calPierData11A[pierIndex].pwrPdg1[3] = 
        pEepStruct->calPierData11A[pierIndex].pwrPdg0[2] + pCalCh->pwr_delta_t2[2][IndexPdg1];
    pEepStruct->calPierData11A[pierIndex].vpdPdg1[3] = 
        pEepStruct->calPierData11A[pierIndex].vpdPdg0[2] + pCalCh->Vpd_delta[2][IndexPdg1];

    pEepStruct->calPierData11A[pierIndex].pwrPdg1[4] = 
        pEepStruct->calPierData11A[pierIndex].pwrPdg0[3] + pCalCh->pwr_delta_t2[3][IndexPdg1];
    pEepStruct->calPierData11A[pierIndex].vpdPdg1[4] = 
        pEepStruct->calPierData11A[pierIndex].vpdPdg0[3] + pCalCh->Vpd_delta[3][IndexPdg1];
*/
}

/* NOT_USED: in Dragon/Mercury */
void writeAr6000Label(A_UINT32 devNum) {
    if (isMercury_sd(swDeviceID)) {
        A_UINT8         tmpEeprom[AR6K_EEPROM_SIZE];
        AR6K_EEPROM     *eeptr;

        art_eepromReadLocs(devNum, 0, (sizeof(AR6K_EEPROM) / 2), tmpEeprom);
        eeptr = (AR6K_EEPROM *)tmpEeprom;
        eeptr->baseEepHeader.checksum = 0x0;

        fillAR6000EepromLabel(devNum, eeptr);

        computeChecksum(eeptr);
        writeEepromStruct(devNum, eeptr);
        return;
    }
    else {
        AR6K_EEPROM tempEepStruct;
        A_UINT32 labelData[20];
        A_UINT16  *pLabelArea;
        A_UINT16  i;

        fillAR6000EepromLabel(devNum, &tempEepStruct);
        pLabelArea = (A_UINT16 *)(&tempEepStruct.baseEepHeader.custData[2]);
        //write block expects the data in array of 32 bits
        for(i = 0; i < 10; i++) {
            labelData[i] = *(pLabelArea + i);
        }

        art_eepromWriteBlock(devNum, 0x08, 10, labelData);
    }
}

void writeAr6000MacAddress(A_UINT32 devNum, A_UCHAR *pMacAddr_arr) {

    A_UINT16 i;
    void *pTempPtr;
    AR6K_EEPROM eepStruct;
	
#ifdef _IQV
	if (art_GetEepromStruct(devNum, EEP_AR6000, &pTempPtr) == 0xffff) {	
		if(checkLibError(devNum, 1))
			return;
		return;
	}
#else
    art_GetEepromStruct(devNum, EEP_AR6000, &pTempPtr);
#endif
    memcpy(&eepStruct, pTempPtr, sizeof(eepStruct));
    eepStruct.baseEepHeader.checksum = 0x0000;
	
    for(i = 0; i < 6; i++){
        eepStruct.baseEepHeader.macAddr[i] = pMacAddr_arr[i];
    }
		
    //Compute checksum once all other areas are filled.
    computeChecksum(&eepStruct);

    //write struct to eeprom 
    writeEepromStruct(devNum, &eepStruct);
} 

#ifdef _IQV
/*
 * \b Name:     readAr6000MacAddress(A_UINT32 devNum, A_UCHAR *pMacAddr_arr);
 *
 * \b Parameter: 
 *				unsigned long devNum     Device handle
 *				unsigned char* pMacAddr_arr  MAC addr list read from the EEPROM
 * \b Return: 
 *				N/A
 * \b Usage:    Called by IQ_art_ReadMACID in the IQ_test.c
 * 
 *  \sa        IQ_art_ReadMACID
 *
 * \b Author:   PS created on 06/21/2007
 *
 */
void readAr6000MacAddress(A_UINT32 devNum, A_UCHAR *pMacAddr_arr)
{
//	void *pTempPtr;
	int i;
	void *pTempPtr;
	AR6K_EEPROM eepStruct;
		
    art_GetEepromStruct(devNum, EEP_AR6000, &pTempPtr);

	 /* read eeprom */
    art_eepromReadLocs(	devNum, 0, 512, (A_UCHAR *)(pTempPtr) );

	memcpy(&eepStruct, pTempPtr, sizeof(eepStruct));

    /* copy the MAC address from eeprSturct to pMacAddr_arr */
	for(i=0;i<6;i++)
	  pMacAddr_arr[i] = eepStruct.baseEepHeader.macAddr[i];


}
#endif //_IQV

void fillAR6000EepromLabel (A_UINT32 devNum,  AR6K_EEPROM *pEepStruct) 
{
	A_UINT16  tmpWord;
	A_CHAR    tmpStr[10];
	A_UINT32 labelData[20];
	A_UINT32  ii, chksum;
#if defined(AR6001)
	A_UINT16  *pLabelArea = (A_UINT16 *)(&pEepStruct->baseEepHeader.custData[2]);
#elif defined(AR6002)
	A_UINT16  *pLabelArea = (A_UINT16 *)(&pEepStruct->baseEepHeader.custData[0]);
#endif

	// Example Label : MB42_035_E_1234_a0

	*(pLabelArea + 8) = (A_UINT16)yldStruct.labelFormatID;


	tmpWord = ( (((yldStruct.cardType[1]) & 0xFF) << 8) |  // "B"
		        ((yldStruct.cardType[0]) & 0xFF) );        // "M"
	*pLabelArea = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb0, tmpWord);

	tmpWord = ( (((yldStruct.cardType[3]) & 0xFF) << 8) |  // "2"
		        ((yldStruct.cardType[2]) & 0xFF) );        // "4"
	*(pLabelArea + 1) = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb1, tmpWord);

	sprintf(tmpStr, "%2d", yldStruct.cardRev);	
	tmpWord = ( (tmpStr[1] << 8) |   // "5"
		         tmpStr[0] );                                // "3"
	*(pLabelArea + 2) = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb2, tmpWord);

	tmpWord = ( (('_' & 0xFF) << 8) |    // "_" placeholder for spare s/n spill-over
		         ((yldStruct.mfgID[0]) & 0xFF)); // "E"
	*(pLabelArea + 3) = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb3, tmpWord);

	sprintf(tmpStr, "%04d", yldStruct.cardNum);	
	
	tmpWord = ( (tmpStr[1] << 8) |          // "2"
				 tmpStr[0] );               // "1"
	*(pLabelArea + 4) = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb4, tmpWord);	

	tmpWord = ( (tmpStr[3] << 8) |          // "4"
				 tmpStr[2] );               // "3"
	*(pLabelArea + 5) = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb5, tmpWord);	

	tmpWord = ( (((yldStruct.reworkID[1]) & 0xFF) << 8) | 
		        ((yldStruct.reworkID[0]) & 0xFF) );
	*(pLabelArea + 6) = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb6, tmpWord);	

	sprintf(tmpStr, "-%d", yldStruct.chipIdentifier);	
	
	tmpWord = ( (tmpStr[0] << 8) |   //"-" spare
		         tmpStr[1] );        //0                      
	*(pLabelArea + 7) = tmpWord;
//	virtual_eeprom0Write(devNum, 0xb7, tmpWord);	

	for(ii = 0; ii < 10; ii++) {
		labelData[ii] = *(pLabelArea + ii);
	}
	chksum = dataArr_get_checksum(devNum, 0, 9, labelData);
	*(pLabelArea + 9) = (A_UINT16)chksum;
//	virtual_eeprom0Write(devNum, 0xbe, chksum);
}

void fillTargetPowerData (
 AR6K_EEPROM *pEepStruct
)
{
    A_UINT16 ii;

    for (ii=0; ii<AR6000_NUM_11A_TARGET_POWERS; ii++)
    {
        pEepStruct->calTargetPower11A[ii].bChannel = freq2fbin(pTargetsSet->pTargetChannel[ii].channelValue) & 0xff;
        pEepStruct->calTargetPower11A[ii].tPow6to24 = (A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target24) & 0x3f;
        pEepStruct->calTargetPower11A[ii].tPow36 = (A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target36) & 0x3f;
        pEepStruct->calTargetPower11A[ii].tPow48 = (A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target48) & 0x3f;
        pEepStruct->calTargetPower11A[ii].tPow54 = (A_UINT16)(2*pTargetsSet->pTargetChannel[ii].Target54) & 0x3f;
    }

    for (ii=0; ii<AR6000_NUM_11B_TARGET_POWERS; ii++)
    {
        pEepStruct->calTargetPower11B[ii].bChannel = freq2fbin(pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].channelValue) & 0xff;
        pEepStruct->calTargetPower11B[ii].tPow6to24 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target24) & 0x3f;
        pEepStruct->calTargetPower11B[ii].tPow36 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target36) & 0x3f;
        pEepStruct->calTargetPower11B[ii].tPow48 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target48) & 0x3f;
        pEepStruct->calTargetPower11B[ii].tPow54 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11b]->pTargetChannel[ii].Target54) & 0x3f;
    }

    for (ii=0; ii<AR6000_NUM_11G_TARGET_POWERS; ii++)
    {
        pEepStruct->calTargetPower11G[ii].bChannel = freq2fbin(pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].channelValue) & 0xff;
        pEepStruct->calTargetPower11G[ii].tPow6to24 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target24) & 0x3f;
        pEepStruct->calTargetPower11G[ii].tPow36 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target36) & 0x3f;
        pEepStruct->calTargetPower11G[ii].tPow48 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target48) & 0x3f;
        pEepStruct->calTargetPower11G[ii].tPow54 = (A_UINT16)(2*pTargetsSet_2p4[MODE_11g]->pTargetChannel[ii].Target54) & 0x3f;
    }
}

void fillCtlData (
 AR6K_EEPROM *pEepStruct
)
{
    A_UINT16 ii, jj; 

    if(pTestGroupSet->numTestGroupsDefined > AR6000_NUM_CTLS) {
        uiPrintf("Number of CTLs must be less than or equal to %d for AR6000.  Exiting", AR6000_NUM_CTLS);
        exit(0);
    }

    for (ii=0; ii<pTestGroupSet->numTestGroupsDefined; ii++)
    {
        //the index value
        pEepStruct->ctlIndex[ii] = pTestGroupSet->pTestGroup[ii].TG_Code & 0xff;

        //band edge information
        for(jj = 0; jj < AR6000_NUM_BAND_EDGES; jj++) {
            pEepStruct->ctlData[ii].ctlEdges[jj].bChannel = 
                freq2fbin(pTestGroupSet->pTestGroup[ii].BandEdge[jj]) & 0xff;

            pEepStruct->ctlData[ii].ctlEdges[jj].tPower = 
                (A_UINT16)(2*pTestGroupSet->pTestGroup[ii].maxEdgePower[jj]) & 0x3f;

            pEepStruct->ctlData[ii].ctlEdges[jj].flag = 
                pTestGroupSet->pTestGroup[ii].inBandFlag[jj] & 0x01;
        }
    }
}
#endif // #if !defined(ZEROCAL_TOOL)

void computeChecksum (
 AR6K_EEPROM *pEepStruct
)
{
    A_UINT16 sum = 0, *pHalf, i;
    
    pHalf = (A_UINT16 *)pEepStruct;

    for (i = 0; i < sizeof(AR6K_EEPROM)/2; i++) {
        sum ^= *pHalf++;
    }

    pEepStruct->baseEepHeader.checksum = 0xFFFF ^ sum;
    //uiPrintf("--computeChecksum 0x%x sum 0x%x\n", pEepStruct->baseEepHeader.checksum, sum );
}

#if !defined(ZEROCAL_TOOL)
static A_UINT32 newEepromArea[AR6K_EEPROM_SIZE];

void writeEepromStruct (
 A_UINT32 devNum,
 AR6K_EEPROM *pEepStruct
)
{
    A_UINT32 jj;
    A_UINT16 *pTempPtr = (A_UINT16 *)pEepStruct;
    
    if (configSetup.eepromPresent) {
        //uiPrintf("++writeEepromStruct len %d\n", (sizeof(AR6K_EEPROM) / 2));
        for(jj = 0; jj < (sizeof(AR6K_EEPROM) / 2); jj++) {
            newEepromArea[jj] = *(pTempPtr + jj);    
        }
        art_eepromWriteBlock(devNum, 0, (sizeof(AR6K_EEPROM) / 2), newEepromArea);
        uiPrintf("--writeEepromStruct len %d\n", (sizeof(AR6K_EEPROM) / 2));
    }
    else { // eepromPresent = 0
        uiPrintf("--no writeEepromStruct len %d\n", (sizeof(AR6K_EEPROM) / 2));
    }
}

void printAR6000Eeprom(A_UINT32 devNum) 
{
    void *pTempPtr;
    
    art_GetEepromStruct(devNum, EEP_AR6000, &pTempPtr);
    ar6000EepromDump(devNum, (AR6K_EEPROM *)pTempPtr);
    return;
}
#endif // #if !defined(ZEROCAL_TOOL)

#if defined(ZEROCAL_TOOL)
#define uiPrintf printf

A_UINT16 fbin2freq(A_UINT16 fbin)
{
        return( (A_UINT16)(4800 + 5*fbin)) ;
}

A_UINT16 fbin2freq_2p4(A_UINT16 fbin)
{
        return( (A_UINT16)(2300 + fbin)) ;
}

#endif // #if defined(ZEROCAL_TOOL)

/**************************************************************
 * ar6000EepromDump
 *
 * Produce a formatted dump of the EEPROM structure
 */
void ar6000EepromDump(A_UINT32 devNum, AR6K_EEPROM *ar6kEep)
{
    A_UINT16          i, j, k,l;
    BASE_EEP_HEADER   *pBase;
    MODAL_EEP_HEADER  *pModal;
    CAL_TARGET_POWER  *pPowerInfo;
    CAL_DATA_PER_FREQ *pDataPerChannel;
    //A_UINT8           noMoreSpurs;
    A_UINT8           *pChannel;
    //A_UINT16          *pSpurData;
    A_UINT16          channelCount, channelRowCnt, vpdCount, rowEndsAtChan;
    A_UINT16          xpdGainMapping[] = {0, 1, 2, 4};
    A_UINT16          xpdGainValues[AR6000_NUM_PD_GAINS], numXpdGain = 0, xpdMask;
    A_UINT16          numPowers, ratePrint, numChannels, tempFreq;
	int tmpInt;
    const static char *sRatePrint[2][4] = {
      {"     6-24     ", "      36      ", "      48      ", "      54      "},
      {"       1      ", "       2      ", "     5.5      ", "      11      "}
    };
    const static char *sTargetPowerMode[3] = {
      "11A OFDM", "11G CCK ", "11G OFDM"
    };
    const static char *sCtlType[5] = {
        "[ 11A base mode ]",
        "[ 11B base mode ]",
        "[ 11G base mode ]",
        "[ UNKNOWN       ]"
    };

    //assert(pEeprom);

    /* Print Header info */
    pBase = &(ar6kEep->baseEepHeader);
    uiPrintf("\n");
    uiPrintf(" =======================Header Information======================\n");
#if defined(AR6002)
    uiPrintf(" |  Length                  %8d                           |\n", pBase->length);
#endif
    uiPrintf(" |  Major Version           %2d  |  Minor Version           %2d  |\n",
             pBase->version >> 12, pBase->version & 0xFFF);
    uiPrintf(" |-------------------------------------------------------------|\n");
    uiPrintf(" |  Checksum           0x%04X   |  Regulatory Domain  0x%04X   |\n",
             pBase->checksum, pBase->regDmn);
    uiPrintf(" |  MacAddress: 0x%02X:%02X:%02X:%02X:%02X:%02X                            |\n",
             pBase->macAddr[0], pBase->macAddr[1], pBase->macAddr[2],
             pBase->macAddr[3], pBase->macAddr[4], pBase->macAddr[5]);
#if defined(AR6001)
    uiPrintf(" |  OpFlags: 11A %d, 11G %d                                      |\n",
             (pBase->opFlags & AR6000_OPFLAGS_11A) || 0, (pBase->opFlags & AR6000_OPFLAGS_11G) || 0);
#elif defined(AR6002)
    uiPrintf(" |  OpFlags:A%d G%d FE%d TxGn%d aD%d |  SubSystemId 0x%04X          |\n",
             (pBase->opFlags & AR6000_OPFLAGS_11A) || 0, (pBase->opFlags & AR6000_OPFLAGS_11G) || 0, (pBase->opFlags & AR6000_OPFLAGS_LNA2_FRONTEND) || 0, (pBase->opFlags & AR6000_OPFLAGS_TXGAIN_TBL) || 0, (pBase->opFlags & AR6000_OPFLAGS_ANT_DIVERSITY) || 0, pBase->subSystemId);
    uiPrintf(" |  BlueToothOptions: 0x%04X 0x%04X                            | \n", pBase->blueToothOptions[0], pBase->blueToothOptions[1]);
    uiPrintf(" |  BinBuildNumber: 0x%08X  |  OpFlags:0x%04X              | \n", pBase->binBuildNumber, pBase->opFlags);
    uiPrintf(" |  negPwrOffset: %3d           |  cckOfdmDelta: %3d           | \n", pBase->negPwrOffset, pBase->cckOfdmDelta);
#endif

    uiPrintf(" |  Customer Data in hex                                       |\n");
    uiPrintf(" |= ");
    for (i = 0; i < sizeof(pBase->custData); i++) {
        if ((i%16) == 0) {
            uiPrintf("            |\n");
            uiPrintf(" | ");
        }
        uiPrintf("%02X ", pBase->custData[i]);
    }
    uiPrintf("\n");

    /* Print Modal Header info */
    for (i = 0; i < 2; i++) {
        pModal = &(ar6kEep->modalHeader[i]);
        if (i == 0) {
            uiPrintf(" |======================11A Modal Header=======================|\n");
        } else {
            uiPrintf(" |======================11G Modal Header=======================|\n");
        }
#if defined(AR6001)
        uiPrintf(" |  Ant Control 0   0x%08lX  |  Ant Control 1   0x%08lX  |\n",
                 pModal->antCtrl[0], pModal->antCtrl[1]);
        uiPrintf(" |  Antenna Control Idle   %3d  |  Antenna Gain           %3d  |\n",
                 pModal->antCtrl0, pModal->antennaGain);
        uiPrintf(" |  Switch Settling        %3d  |  TxRxAttenuation        %3d  |\n",
                 pModal->switchSettling, pModal->txrxAtten);
        uiPrintf(" |  RxTxMargin             %3d  |  adc desired size       %3d  |\n",
                 pModal->rxTxMargin , pModal->adcDesiredSize);
        uiPrintf(" |  pga desired size       %3d  |  tx end to xlna on      %3d  |\n",
                 pModal->pgaDesiredSize , pModal->txEndToXlnaOn);
        uiPrintf(" |  xlna gain              %3d  |  tx end to xpa off      %3d  |\n",
                 pModal->xlnaGain, pModal->txEndToXpaOff);
        uiPrintf(" |  tx frame to xpa on     %3d  |  thresh62               %3d  |\n",
                 pModal->txFrameToXpaOn, pModal->thresh62);
        uiPrintf(" |  noise floor threshold  %3d  |  Xpd Gain Mask 0x%X | Xpd %2d  |\n",
                 pModal->noiseFloorThresh, pModal->xpdGain, pModal->xpd);
        uiPrintf(" |  IQ Cal I               %3d  |  IQ Cal Q               %3d  |\n",
                 pModal->iqCalI, pModal->iqCalQ);
#elif defined(AR6002)
        uiPrintf(" |  Ant Control 0   0x%08lX  |  Ant Control 1   0x%08lX  |\n",
                 pModal->antCtrl[0], pModal->antCtrl[1]);
        uiPrintf(" |  Antenna Control Idle   %3d  |  Antenna Gain           %3d  |\n",
                 pModal->antCtrl0, pModal->antennaGain);
        uiPrintf(" |  Switch Settling        %3d  |  Atten1 Hyst            %3d  |\n",
                 pModal->switchSettling, pModal->xAtten1Hyst);
        uiPrintf(" |  Atten1 Margin          %3d  |  Atten1 Db              %3d  |\n",
                 pModal->xAtten1Margin, pModal->xAtten1Db);
        uiPrintf(" |  Atten2 Hyst            %3d  |  Atten2 Margin          %3d  |\n",
                 pModal->xAtten2Hyst, pModal->xAtten2Margin);
        uiPrintf(" |  Atten2 DB              %3d                                 |\n",
                 pModal->xAtten2Db);
        uiPrintf(" |  adc desired size       %3d  |  tx end to xlna on      %3d  |\n",
                 pModal->adcDesiredSize, pModal->txEndToXlnaOn);
        uiPrintf(" |  xlna gain              %3d  |  tx end to xpa off      %3d  |\n",
                 pModal->xlnaGain, pModal->txEndToXpaOff);
        uiPrintf(" |  tx frame to xpa on     %3d  |  thresh62               %3d  |\n",
                 pModal->txFrameToXpaOn, pModal->thresh62);
        uiPrintf(" |  noise floor threshold  %3d  |  Xpd Gain Mask 0x%X | Xpd %2d  |\n",
                 pModal->noiseFloorThresh, pModal->xpdGain, pModal->xpd);
        uiPrintf(" |  IQ Cal I               %3d  |  IQ Cal Q               %3d  |\n",
                 pModal->iqCalI, pModal->iqCalQ);
        uiPrintf(" |  pd Gain Overlap        %3d                                 |\n",
                 pModal->pdGainOverlap);
        uiPrintf(" |  OB                     %3d  |  DB                     %3d  |\n",
                 pModal->ob, pModal->db);
        uiPrintf(" |  xpa Bias Lvl           %3d  |  xpa Bias Lvl2          %3d  |\n",
                 pModal->xpaBiasLvl, pModal->xpaBiasLvl2);
        uiPrintf(" |  txPowerOffset          %3d  |\n",
                 pModal->txPowerOffset);
        uiPrintf(" |  sellna                 %3d  |  selintpd               %3d  |\n",
                 pModal->sellna, pModal->selintpd);
        uiPrintf(" |  enablePca              %3d  |  enablePcb              %3d  |\n",
                 pModal->enablePca, pModal->enablePcb); 
        uiPrintf(" |  enableXpaa             %3d  |  enableXpab             %3d  |\n",
                 pModal->enableXpaa, pModal->enableXpab);
        uiPrintf(" |  useTxPdInXpa           %3d  |  initTxGain             %3d  |\n",
                 pModal->useTxPdInXpa, pModal->initTxGain);
        uiPrintf(" |  txGainTbl_0              %02x |\n", pModal->txGainTbl_0);
        for (l=0; l<AR6000_TX_GAIN_TBL_SIZE/2; l++) {
            uiPrintf(" |  txGainTblDelta_%d         %02x |\n", l, pModal->txGainTblDelta[l]);
        }

#endif
    }
    uiPrintf(" |=============================================================|\n");

    /* Print spur data 
    uiPrintf("=======================Spur Information======================\n");
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            uiPrintf("| 11A Spurs in MHz                                          |\n");
        } else {
            uiPrintf("| 11G Spurs in MHz                                          |\n");
        }
        pSpurData = ar6kEep->spurChans[i];
        noMoreSpurs = 0;
        for (j = 0; j < AR6000_EEPROM_MODAL_SPURS; j++) {
            if ((pSpurData[j] == NO_SPUR) || noMoreSpurs) {
                noMoreSpurs = 1;
                uiPrintf("|  NO SPUR  ");
            } else {
                uiPrintf("|   %4d.%1d  ", SPUR_TO_KHZ(i, pSpurData[j])/1000,
                         (SPUR_TO_KHZ(i, pSpurData[j])/100) % 10);
            }
        }
        uiPrintf("|\n");
    } */
    uiPrintf("|===========================================================|\n");

    /* Print calibration info */
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            pDataPerChannel = ar6kEep->calPierData11A;
            numChannels = AR6000_NUM_11A_CAL_PIERS;
            pChannel = ar6kEep->calFreqPier11A;
        } else {
            pDataPerChannel = ar6kEep->calPierData11G;
            numChannels = AR6000_NUM_11G_CAL_PIERS;
            pChannel = ar6kEep->calFreqPier11G;
        }
        xpdMask = ar6kEep->modalHeader[i].xpdGain;

        numXpdGain = 0;
        /* Calculate the value of xpdgains from the xpdGain Mask */
        for (j = 1; j <= AR6000_PD_GAINS_IN_MASK; j++) {
            if ((xpdMask >> (AR6000_PD_GAINS_IN_MASK - j)) & 1) {
                if (numXpdGain >= AR6000_NUM_PD_GAINS) {
                    assert(0);
                    break;
                }
                xpdGainValues[numXpdGain++] = AR6000_PD_GAINS_IN_MASK - j;
            }
        }

        uiPrintf("====================Power Calibration Information===========================\n");
		for (channelRowCnt = 0; channelRowCnt < numChannels; channelRowCnt += 5) {
            tmpInt = A_MIN(numChannels, (channelRowCnt + 5));
            rowEndsAtChan = tmpInt; 
            for (channelCount = channelRowCnt; channelCount < rowEndsAtChan; channelCount++) {
                uiPrintf("|     %04d     ",  (i == 0) ?
                    fbin2freq(pChannel[channelCount]) : fbin2freq_2p4(pChannel[channelCount]));
            }
            uiPrintf("|\n|==============|==============|==============|==============|==============|\n");
            for (channelCount = channelRowCnt; channelCount < rowEndsAtChan; channelCount++) {
                uiPrintf("|pdadc pwr(dBm)");
            }
            uiPrintf("|\n");

            uiPrintf("|              |              |              |              |              |\n");
            uiPrintf("| PD_Gain %2d   |              |              |              |              |\n",
                     xpdGainMapping[xpdGainValues[0]]);

            for (vpdCount = 0; vpdCount < 4; vpdCount++) {
                for (channelCount = channelRowCnt; channelCount < rowEndsAtChan; channelCount++) {
                    uiPrintf("|  %02d   %2d.%02d  ", pDataPerChannel[channelCount].vpdPdg0[vpdCount],
                             pDataPerChannel[channelCount].pwrPdg0[vpdCount] / 4,
                             (pDataPerChannel[channelCount].pwrPdg0[vpdCount] % 4) * 25);
                }
                uiPrintf("|\n");
            }
            uiPrintf("|              |              |              |              |              |\n");
            uiPrintf("|              |              |              |              |              |\n");
            uiPrintf("| PD_Gain %2d   |              |              |              |              |\n",
                     xpdGainMapping[xpdGainValues[1]]);

            for (vpdCount = 0; vpdCount < 5; vpdCount++) {
                for (channelCount = channelRowCnt; channelCount < rowEndsAtChan; channelCount++) {
                    uiPrintf("|  %02d   %2d.%02d  ", pDataPerChannel[channelCount].vpdPdg1[vpdCount],
                             pDataPerChannel[channelCount].pwrPdg1[vpdCount] / 4,
                             (pDataPerChannel[channelCount].pwrPdg1[vpdCount] % 4) * 25);
                }
                uiPrintf("|\n");
            }
            uiPrintf("|              |              |              |              |              |\n");
            uiPrintf("|==============|==============|==============|==============|==============|\n");
        }
        uiPrintf("|\n");
    }

    /* Print Target Powers */
    for (i = 0; i < 3; i++) {
        if (i == 0) {
            pPowerInfo = ar6kEep->calTargetPower11A;
            numPowers = AR6000_NUM_11A_TARGET_POWERS;
            ratePrint = 0;
        } else if (i == 1) {
            pPowerInfo = ar6kEep->calTargetPower11B;
            numPowers = AR6000_NUM_11B_TARGET_POWERS;
            ratePrint = 1;
        } else {
            pPowerInfo = ar6kEep->calTargetPower11G;
            numPowers = AR6000_NUM_11G_TARGET_POWERS;
            ratePrint = 0;
        }
        uiPrintf("============================Target Power Info===============================\n");
        for (j = 0; j < numPowers; j+=4) {
            uiPrintf("|   %s   ", sTargetPowerMode[i]);
            for (k = j; k < A_MIN(j + 4, numPowers); k++) {
                uiPrintf("|     %04d     ", (i == 0) ?
                    fbin2freq((A_UINT16)pPowerInfo[k].bChannel) : fbin2freq_2p4((A_UINT16)pPowerInfo[k].bChannel));
            }
            uiPrintf("|\n");
            uiPrintf("|==============|==============|==============|==============|==============|\n");

            uiPrintf("|%s", sRatePrint[ratePrint][0]);
            for (k = j; k < A_MIN(j + 4, numPowers); k++) {
                uiPrintf("|     %2d.%d     ", pPowerInfo[k].tPow6to24 / 2,
                         (pPowerInfo[k].tPow6to24 % 2) * 5);
            }
            uiPrintf("|\n");
            uiPrintf("|%s", sRatePrint[ratePrint][1]);
            for (k = j; k < A_MIN(j + 4, numPowers); k++) {
                uiPrintf("|     %2d.%d     ", pPowerInfo[k].tPow36 / 2,
                         (pPowerInfo[k].tPow36 % 2) * 5);
            }
            uiPrintf("|\n");

            uiPrintf("|%s", sRatePrint[ratePrint][2]);
            for (k = j; k < A_MIN(j + 4, numPowers); k++) {
                uiPrintf("|     %2d.%d     ", pPowerInfo[k].tPow48 / 2,
                         (pPowerInfo[k].tPow48 % 2) * 5);
            }
            uiPrintf("|\n");

            uiPrintf("|%s", sRatePrint[ratePrint][3]);
            for (k = j; k < A_MIN(j + 4, numPowers); k++) {
                uiPrintf("|     %2d.%d     ", pPowerInfo[k].tPow54 / 2,
                         (pPowerInfo[k].tPow54 % 2) * 5);
            }
            uiPrintf("|\n");
            uiPrintf("|==============|==============|==============|==============|==============|\n");
        }
    }
    uiPrintf("\n");

    /* Print Band Edge Powers */
    uiPrintf("=======================Test Group Band Edge Power========================\n");
    for (i = 0; (ar6kEep->ctlIndex[i] != 0) && (i < AR6000_NUM_CTLS); i++) {
        uiPrintf("|                                                                       |\n");
        uiPrintf("| CTL: 0x%02x %s                                           |\n",
                 ar6kEep->ctlIndex[i], sCtlType[ar6kEep->ctlIndex[i] & 0x3]);
        uiPrintf("|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

        uiPrintf("| edge  ");
        for (j = 0; j < AR6000_NUM_BAND_EDGES; j++) {
            if (ar6kEep->ctlData[i].ctlEdges[j].bChannel == AR6000_BCHAN_UNUSED) {
                uiPrintf("|  --   ");
            } else {
                if(((ar6kEep->ctlIndex[i] & 0x7) == 0) ||
                    ((ar6kEep->ctlIndex[i] & 0x7) == 0x3)  ){ //turbo mode
                    tempFreq = fbin2freq(ar6kEep->ctlData[i].ctlEdges[j].bChannel);
                }
                else {
                    tempFreq = fbin2freq_2p4(ar6kEep->ctlData[i].ctlEdges[j].bChannel);
               }

               uiPrintf("| %04d  ", tempFreq);
            }
        }
        uiPrintf("|\n");
        uiPrintf("|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

        uiPrintf("| power ");
        for (j = 0; j < AR6000_NUM_BAND_EDGES; j++) {
            if (ar6kEep->ctlData[i].ctlEdges[j].bChannel == AR6000_BCHAN_UNUSED) {
                uiPrintf("|  --   ");
            } else {
                uiPrintf("| %2d.%d  ", ar6kEep->ctlData[i].ctlEdges[j].tPower / 2,
                    (ar6kEep->ctlData[i].ctlEdges[j].tPower % 2) * 5);
            }
        }
        uiPrintf("|\n");
        uiPrintf("|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

        uiPrintf("| flag  ");
        for (j = 0; j < AR6000_NUM_BAND_EDGES; j++) {
            if (ar6kEep->ctlData[i].ctlEdges[j].bChannel == AR6000_BCHAN_UNUSED) {
                uiPrintf("|  --   ");
            } else {
                uiPrintf("|   %1d   ", ar6kEep->ctlData[i].ctlEdges[j].flag);
            }
        }
        uiPrintf("|\n");
        uiPrintf("=========================================================================\n");
    }


    /* Print Noise Floor Data */
    uiPrintf("=======================Noise Floor Data========================\n");
    uiPrintf("bChannel  calNFOffset   calNFSlope   reserved \n");
    uiPrintf("11A\n");
    for (i=0; i< AR6000_NUM_11A_CAL_PIERS; i++) {
        uiPrintf("%d   %d   %d   %d\n", ar6kEep->calNFData11A[i].bChannel, ar6kEep->calNFData11A[i].calNFOffset, 
            ar6kEep->calNFData11A[i].calNFSlope, ar6kEep->calNFData11A[i].futureNF);
    }
    uiPrintf("11G\n");
    for (i=0; i< AR6000_NUM_11G_CAL_PIERS; i++) {
        uiPrintf("%d   %d   %d   %d\n", ar6kEep->calNFData11G[i].bChannel, ar6kEep->calNFData11G[i].calNFOffset, 
            ar6kEep->calNFData11G[i].calNFSlope, ar6kEep->calNFData11G[i].futureNF);
    }

}

#if !defined(ZEROCAL_TOOL)
#if defined(MERCURY_EMULATION)
static A_UINT8 fakeEepromData_tst[AR6K_EEPROM_SIZE] = {
    /* Base Eep Header */
    0x00, 0x03, 0x00, 0x00, // Length
    /*0xFC, 0x22, 0x00, 0xF0,*/ // Checksum, version
    0x00, 0x00, 0x00, 0xF0, // Checksum, version
    0x10, 0x00, 0x00, 0x03, // RegDmn, MacAddr 0,1
    0x7F, 0x00, 0x00, 0x00, // MacAddr 2-5
    0x03, 0x00, 0x22, 0x60, // OpFlags, SubsystemId
    0x00, 0x00, 0x00, 0x00, // blueToothOptions
    0x00, 0x00, 0x00, 0x00, // binBuildNumber
    0x00, 0x00, 0x00, 0x00, // futureBase 00-03
    0x00, 0x00, 0x00, 0x00, // futureBase 04-07
    0x00, 0x00, 0x00, 0x00, // futureBase 08-11
    0x00, 0x00, 0x00, 0x00, // futureBase 12-15
    0x00, 0x00, 0x00, 0x00, // futureBase 16-19
    0x00, 0x00, 0x00, 0x00, // futureBase 20-23
    0x00, 0x00, 0x00, 0x00, // futureBase 24-27
    0x00, 0x00, 0x00, 0x00, // futureBase 28-31
    0x01, 0x02, 0x03, 0x04, // CustData   00-03
    0x05, 0x06, 0x07, 0x08, // CustData   04-07
    0x09, 0x0A, 0x0B, 0x0C, // CustData   08-11
    0x0D, 0x0E, 0x0F, 0x10, // CustData   12-15
    0x11, 0x12, 0x13, 0x14, // CustData   16-19
    0x15, 0x16, 0x17, 0x18, // CustData   20-23
    0x19, 0x1A, 0x1B, 0x1C, // CustData   24-27
    0x1D, 0x1E, 0x1F, 0x20, // CustData   28-31
    0x21, 0x22, 0x23, 0x24, // CustData   32-35
    0x25, 0x26, 0x27, 0x28, // CustData   36-39
    0x29, 0x2A, 0x2B, 0x2C, // CustData   40-43
    0x2D, 0x2E, 0x2F, 0x30, // CustData   44-47
    0x31, 0x32, 0x33, 0x34, // CustData   48-51
    0x35, 0x36, 0x37, 0x38, // CustData   52-55
    0x39, 0x3A, 0x3B, 0x3C, // CustData   56-59
    0x3D, 0x3E, 0x3F, 0x40, // CustData   60-63
    0x41, 0x42, 0x43, 0x44, // CustData   64-67
    0x45, 0x46, 0x47, 0x48, // CustData   68-71
    0x49, 0x4A, 0x4B, 0x4C, // CustData   72-75
    0x4D, 0x4E, 0x4F, 0x50, // CustData   76-79
    0x51, 0x52, 0x53, 0x54, // CustData   80-83
    0x55, 0x56, 0x57, 0x58, // CustData   84-87
    0x59, 0x5A, 0x5B, 0x5C, // CustData   88-91
    0x5D, 0x5E, 0x5F, 0x60, // CustData   92-95
    0x61, 0x62, 0x63, 0x64, // CustData   96-99
    0x65, 0x66, 0x67, 0x68, // CustData   100-103
    0x69, 0x6A, 0x6B, 0x6C, // CustData   104-107
    0x6D, 0x6E, 0x6F, 0x70, // CustData   108-111
    /* Modal eep Header 11A */
    0x81, 0x69, 0x96, 0x25, // Ant 0 ctrl
    0x42, 0x59, 0x9A, 0x26, // Ant 1 ctrl
    0x00, 0x02, 0x2D, 0x0F, // Ant Idle, Ant Gain, SwitchSettle, xAtten1Hyst
    0x00, 0x0B, 0x00, 0x00, // xAtteen1Margin, xAtten1db, xAtten2Hyst, xAtten2Margin
    0x00, 0xE0, 0x02, 0x0D, // xAtteen2db, adcDesired, txEndtoXlnaOn, xlnaGain
    0x00, 0x0E, 0x0F, 0xCA, // txEndToXpaOff, txFrameToXpaOn, thresh62, noiseFloorThresh
    0x0A, 0x01, 0x08, 0x08, // xpdGain, xpd, iqCalI, iqCalQ
    0x00, 0x00, 0x00, 0x00, // pdGainOverlap, ob, db, xpaBiasLvl
    0x00, 0x00, 0x00, 0x00, // txPowerOffset, futureModal 00-02
    0x00, 0x00, 0x00, 0x00, // futureModal 03-06
    0x00, 0x00, 0x00, 0x00, // futureModal 07-10
    0x00, 0x00, 0x00, 0x00, // futureModal 11-14
    0x00, 0x00, 0x00, 0x00, // futureModal 15-18
    0x00, 0x00, 0x00, 0x00, // futureModal 19-22
    0x00, 0x00, 0x00, 0x00, // futureModal 23-26
    0x00, 0x00, 0x00, 0x00, // futureModal 27-30
    /* Modal eep Header 11G */
    0x81, 0x69, 0x96, 0x25, // Ant 0 ctrl
    0x42, 0x59, 0x9A, 0x26, // Ant 1 ctrl
    0x00, 0x02, 0x28, 0x21, // Ant Idle, Ant Gain, SwitchSettle, xAtten1Hyst
    0x00, 0x0A, 0x00, 0x00, // xAtteen1Margin, xAtten1db, xAtten2Hyst, xAtten2Margin
    0x00, 0xDA, 0x02, 0x0D, // xAtten2db, adcDesired, txEndtoXlnaOn, xlnaGain
    0x00, 0x0E, 0x1C, 0xFF, // txEndToXpaOff, txFrameToXpaOn, thresh62, noiseFloorThresh
    0x0A, 0x01, 0x08, 0x08, // xpdGain, xpd, iqCalI, iqCalQ
    0x00, 0x00, 0x00, 0x00, // pdGainOverlap, ob, db, xpaBiasLvl
    0x00, 0x00, 0x00, 0x00, // txPowerOffset, futureModal 00-02
    0x00, 0x00, 0x00, 0x00, // futureModal 03-06
    0x00, 0x00, 0x00, 0x00, // futureModal 07-10
    0x00, 0x00, 0x00, 0x00, // futureModal 11-14
    0x00, 0x00, 0x00, 0x00, // futureModal 15-18
    0x00, 0x00, 0x00, 0x00, // futureModal 19-22
    0x00, 0x00, 0x00, 0x00, // futureModal 23-26
    0x00, 0x00, 0x00, 0x00, // futureModal 27-30
    /* Spur Data */
    0x00, 0x80, 0x00, 0x00, // Spur 11A 1,2
    0x00, 0x00, 0x00, 0x00, // Spur 11A 3,4
    0x00, 0x00, 0x68, 0x06, // Spur 11A 5, 11G 1
    0xB0, 0x04, 0x00, 0x80, // Spur 11G 2,3
    0x00, 0x80, 0x00, 0x80, // Spur 11G 4,5
    /* Power calibration PDADCs */
    0x32, 0x4A, 0x5C, 0x68, // 11A Piers - 0
    0x78, 0xA0, 0xB9, 0xCD, // 11A Piers - 1
    0x70, 0xA2, 0xB8, 0xFF, // 11G Piers
    0x00, 0x0C, 0x18, 0x24, // 11A Cal Piers - 1 - 1
    0x00, 0x00, 0x07, 0x14, // 11A Cal Piers - 1 - 2
    0x1C, 0x2C, 0x3C, 0x4A, // 11A Cal Piers - 1 - 3
    0x56, 0x06, 0x0B, 0x15, // 11A Cal Piers - 1 - 4
    0x26, 0x3C, 0x00, 0x0C, // 11A Cal Piers - 1 - 5 / 2 - 1
    0x18, 0x24, 0x00, 0x00, // 11A Cal Piers - 2 - 2
    0x08, 0x16, 0x1C, 0x2C, // 11A Cal Piers - 2 - 3
    0x3A, 0x48, 0x54, 0x07, // 11A Cal Piers - 2 - 4
    0x0B, 0x16, 0x23, 0x39, // 11A Cal Piers - 2 - 5
    0x00, 0x0C, 0x18, 0x24, // 11A Cal Piers - 3 - 1
    0x00, 0x01, 0x0B, 0x1B, // 11A Cal Piers - 3 - 2
    0x1C, 0x2C, 0x3A, 0x48, // 11A Cal Piers - 3 - 3
    0x54, 0x06, 0x0D, 0x17, // 11A Cal Piers - 3 - 4
    0x26, 0x3D, 0x00, 0x0C, // 11A Cal Piers - 3 - 5 / 4 - 1
    0x18, 0x24, 0x00, 0x00, // 11A Cal Piers - 4 - 2
    0x0C, 0x1D, 0x1C, 0x2C, // 11A Cal Piers - 4 - 3
    0x38, 0x48, 0x52, 0x08, // 11A Cal Piers - 4 - 4
    0x0E, 0x17, 0x27, 0x3B, // 11A Cal Piers - 4 - 5
    0x00, 0x0C, 0x18, 0x24, // 11A Cal Piers - 5 - 1
    0x00, 0x01, 0x0E, 0x1F, // 11A Cal Piers - 5 - 2
    0x1C, 0x2C, 0x38, 0x48, // 11A Cal Piers - 5 - 3
    0x50, 0x08, 0x10, 0x16, // 11A Cal Piers - 5 - 4
    0x2C, 0x3C, 0x00, 0x0C, // 11A Cal Piers - 5 - 5 / 6 - 1
    0x18, 0x24, 0x01, 0x0A, // 11A Cal Piers - 6 - 2
    0x18, 0x2A, 0x1C, 0x2C, // 11A Cal Piers - 6 - 3
    0x3C, 0x4A, 0x52, 0x0A, // 11A Cal Piers - 6 - 4
    0x12, 0x20, 0x38, 0x4E, // 11A Cal Piers - 6 - 5
    0x00, 0x0C, 0x18, 0x24, // 11A Cal Piers - 7 - 1
    0x00, 0x08, 0x16, 0x25, // 11A Cal Piers - 7 - 2
    0x1C, 0x2C, 0x3A, 0x48, // 11A Cal Piers - 7 - 3
    0x50, 0x09, 0x12, 0x1E, // 11A Cal Piers - 7 - 4
    0x35, 0x49, 0x00, 0x0C, // 11A Cal Piers - 7 - 5 / 8 - 1
    0x18, 0x24, 0x03, 0x0F, // 11A Cal Piers - 8 - 2
    0x1D, 0x34, 0x1C, 0x2C, // 11A Cal Piers - 8 - 3
    0x3A, 0x4A, 0x50, 0x0B, // 11A Cal Piers - 8 - 4
    0x16, 0x25, 0x42, 0x56, // 11A Cal Piers - 8 - 5
    0x00, 0x0C, 0x18, 0x24, // 11G Cal Piers - 1 - 1
    0x00, 0x0A, 0x15, 0x2B, // 11G Cal Piers - 1 - 2
    0x1C, 0x2C, 0x3C, 0x48, // 11G Cal Piers - 1 - 3
    0x54, 0x09, 0x12, 0x20, // 11G Cal Piers - 1 - 4
    0x32, 0x4A, 0x00, 0x0C, // 11G Cal Piers - 1 - 5 / 2 - 1
    0x18, 0x24, 0x01, 0x09, // 11G Cal Piers - 2 - 2
    0x17, 0x2B, 0x1C, 0x2C, // 11G Cal Piers - 2 - 3
    0x3A, 0x4A, 0x56, 0x0A, // 11G Cal Piers - 2 - 4
    0x11, 0x1F, 0x35, 0x54, // 11G Cal Piers - 2 - 5
    0x00, 0x0C, 0x18, 0x24, // 11G Cal Piers - 3 - 1
    0x02, 0x0F, 0x15, 0x30, // 11G Cal Piers - 3 - 2
    0x1C, 0x2C, 0x3C, 0x4A, // 11G Cal Piers - 3 - 3
    0x56, 0x0A, 0x13, 0x21, // 11G Cal Piers - 3 - 4
    0x35, 0x55, 0x00, 0x00, // 11G Cal Piers - 3 - 5 / 4 - 1
    0x00, 0x00, 0x00, 0x00, // 11G Cal Piers - 4 - 2
    0x00, 0x00, 0x00, 0x00, // 11G Cal Piers - 4 - 3
    0x00, 0x00, 0x00, 0x00, // 11G Cal Piers - 4 - 4
    0x00, 0x00, 0x00, 0x00, // 11G Cal Piers - 4 - 5
    /* Target Power Data */
    0x4C, 0x22, 0xC8, 0x69, // 11A Target Powers - 0
    0x68, 0x22, 0xC8, 0x69, // 11A Target Powers - 1
    0xB4, 0x22, 0xC8, 0x69, // 11A Target Powers - 2
    0xBD, 0x22, 0xC8, 0x69, // 11A Target Powers - 3
    0x70, 0xA6, 0x69, 0x9A, // 11B Target Powers - 0
    0xA2, 0xA6, 0x69, 0x9A, // 11B Target Powers - 1
    0x70, 0xA4, 0x08, 0x7A, // 11G Target Powers - 0
    0x89, 0xA4, 0x08, 0x7A, // 11G Target Powers - 1
    0xFF, 0x00, 0x00, 0x00, // 11G Target Powers - 2
    /* CTL Data */
    0x10, 0x40, 0x30, 0x11, // CTL Index 0
    0x31, 0x12, 0x32, 0x00, // CTL Index 1
    0x4C, 0x1F, 0x50, 0x60, // CTL BE 0 - 0
    0x5C, 0x61, 0x68, 0x22, // CTL BE 0 - 1
    0x8C, 0x22, 0x90, 0x62, // CTL BE 0 - 2
    0xB4, 0x22, 0xBD, 0x62, // CTL BE 0 - 3
    0x4A, 0x22, 0x56, 0x22, // CTL BE 1 - 0
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 1 - 1
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 1 - 2
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 1 - 3
    0x4C, 0x24, 0x68, 0x24, // CTL BE 2 - 0
    0x8C, 0x24, 0xB4, 0x24, // CTL BE 2 - 1
    0xBD, 0x3C, 0xC1, 0x7C, // CTL BE 2 - 2
    0xCD, 0x3C, 0xFF, 0x00, // CTL BE 2 - 3
    0x70, 0x28, 0x75, 0x68, // CTL BE 3 - 0
    0xA2, 0x25, 0xFF, 0x00, // CTL BE 3 - 1
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 3 - 2
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 3 - 3
    0x70, 0x25, 0x75, 0x65, // CTL BE 4 - 0
    0xAC, 0x25, 0xFF, 0x00, // CTL BE 4 - 1
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 4 - 2
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 4 - 3
    0x70, 0x25, 0x75, 0x66, // CTL BE 5 - 0
    0x9D, 0x66, 0xA2, 0x25, // CTL BE 5 - 1
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 5 - 2
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 5 - 3
    0x70, 0x22, 0x75, 0x62, // CTL BE 6 - 0
    0xAC, 0x22, 0xFF, 0x00, // CTL BE 6 - 1
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 6 - 2
    0xFF, 0x00, 0xFF, 0x00, // CTL BE 6 - 3
    0x00, 0x00, 0x00, 0x00, // CTL BE 7 - 0
    0x00, 0x00, 0x00, 0x00, // CTL BE 7 - 1
    0x00, 0x00, 0x00, 0x00, // CTL BE 7 - 2
    0x00, 0x00, 0x00, 0x00, // CTL BE 7 - 3
    /* CAL_NF Data */
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 0
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 1
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 2
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 3
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 4
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 5
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 6
    0x00, 0x00, 0x00, 0x00, // 11 A Cal 7
    0x00, 0x00, 0x00, 0x00, // 11 G Cal 0
    0x00, 0x00, 0x00, 0x00, // 11 G Cal 1
    0x00, 0x00, 0x00, 0x00, // 11 G Cal 2
    0x00, 0x00, 0x00, 0x00, // 11 G Cal 3
};

void progFakeEepromData(A_UINT32 devNum) 
{
    //Compute checksum once all other areas are filled.
    computeChecksum((AR6K_EEPROM *)&fakeEepromData_tst);

    //write struct to eeprom 
    writeEepromStruct(devNum, (AR6K_EEPROM *)&fakeEepromData_tst);
}

#else
void progFakeEepromData(A_UINT32 devNum) 
{
    programCompactEeprom(devNum);
#if 0
    void *eepromPtr; 
    if( configSetup.eepromLoad) {
        art_GetEepromStruct(devNum, EEP_AR6000, &eepromPtr);
        writeCalDataToFile(eepromPtr);
    }
#endif
}
#endif //#if defined(MERCURY_EMULATION)


A_BOOL writeCalDataToFile(void *pEepStruct)
{
        A_UINT16 *pData;
        A_UINT32 address, length;
        A_CHAR fileName[128]; 
        FILE *fStream;

        if(configSetup.enableLabelScheme) {
            sprintf(fileName, "calData_%s.txt", yldStruct.cardLabel);
            if( (fStream = fopen(fileName, "w")) == NULL) {
                uiPrintf("Could not open calDataFile %s\n", fileName);
                return FALSE;
            }
        }
        else {
            strcpy(fileName, CalSetup.calDataFile);
            if(fileName == '\0') {
                uiPrintf("No name given for calDataFile.  Please add a name in calsetup.txt\n");
                return FALSE;
            }
            if( (fStream = fopen(fileName, "a")) == NULL) {
                uiPrintf("Could not open calDataFile %s\n", fileName);
                return FALSE;
            }
        }

        pData = (A_UINT16 *)pEepStruct;
        length = eepromSize;
        for (address = 0; address < length;  address++)
        {
                fprintf(fStream, "%04x    ;%04x\n", pData[address], address);
        }
        fclose(fStream);
        //uiPrintf("\nBacking up cal data to %s complete\n", fileName);

        // write a binary eeprom 
        if(configSetup.enableLabelScheme) {
            sprintf(fileName, "calData_%s.bin", yldStruct.cardLabel);
            if( (fStream = fopen(fileName, "wb")) == NULL) {
                uiPrintf("Could not open calDataFile %s to write\n", fileName);
                return FALSE;
            }
            if (AR6K_EEPROM_SIZE != fwrite((A_UCHAR *)pEepStruct, 1, AR6K_EEPROM_SIZE, fStream)) {
                uiPrintf("Error writing to %s\n", fileName);
            }
            if (fStream) fclose(fStream);

            uiPrintf(".written to %s\n", fileName);
        }

        return TRUE;
}

#endif // #if !defined(ZEROCAL_TOOL)

