/*
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mEep6000.c#6 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mEep6000.c#6 $"

#ifdef VXWORKS
#include "vxworks.h"
#endif

#ifdef __ATH_DJGPPDOS__
#define __int64 long long
typedef unsigned long DWORD;
#define Sleep   delay
#endif  // #ifdef __ATH_DJGPPDOS__


#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifndef VXWORKS
#include <malloc.h>
#include <assert.h>
#endif
#include "wlantype.h"

#if !defined(ZEROCAL_TOOL)
#include "mCfg6000.h"
#include "mEep6000.h"

#include "ar6000reg.h"
#include "mConfig.h"

//#include "ar5211reg.h"
#endif // #if !defined(ZEROCAL_TOOL)

#include "mEepStruct6000.h"

#if !defined(ZEROCAL_TOOL)
MANLIB_API A_UINT8 dummyEepromWriteArea[AR6K_EEPROM_SIZE]; 

//tmp YM
//#define DEBUG1
//#define DEBUG2
//#define DEBUG3

#else  // ZEROCAL_TOOL
#define FALSE 0
#define TRUE  1
#define NUM_RATES 16
#define MODE_11A 0
#define MODE_11G 1
#define MODE_11B 2
#define MODE_11O 3   
#define uiPrintf printf

extern AR6K_EEPROM eepStruct;

static A_BOOL isZerodBm(void)
{
    AR6K_EEPROM *ar6kEep= &eepStruct;
    return(ar6kEep->baseEepHeader.opFlags & AR6000_OPFLAGS_0dBm);
}

#endif // ZEROCAL_TOOL

static A_UINT16
fbin2freq(A_UINT8 fbin, A_BOOL is2GHz);

static A_INT16
interpolate(A_UINT16 target, A_UINT16 srcLeft, A_UINT16 srcRight,
            A_INT16 targetLeft, A_INT16 targetRight);

static A_BOOL
getLowerUpperIndex(A_UINT8 target, A_UINT8 *pList, A_UINT16 listSize,
                   A_UINT16 *indexL, A_UINT16 *indexR);

/* Configuring the Transmit Power Per Rate Baseband Table */
static void
ar6000SetPowerPerRateTable(A_UINT32 devNum, A_UINT32 freq, A_UINT16 *ratesArray,
                           A_UINT16 cfgCtl, A_UINT16 AntennaReduction, A_UINT16 powerLimit);

static A_UINT16
ar6000GetMaxEdgePower(A_UINT32 freq, CAL_CTL_EDGES *pRdEdgesPower, A_BOOL is2GHz);

static void
ar6000GetTargetPowers(A_UINT32 freq, CAL_TARGET_POWER *powInfo,
                      A_UINT16 numChannels, CAL_TARGET_POWER *pNewPower, A_BOOL is2GHz);

/* Interpolating to get the txPower calibration data based on EEPROM values */
static void
ar6000SetPowerCalTable(A_UINT32 devNum, A_UINT32 freq, A_INT16 *pTxPowerIndexOffset);

static void
ar6000GetGainBoundariesAndPdadcs(A_UINT32 devNum, A_UINT32 freq, CAL_DATA_PER_FREQ * pRawDataSet, 
                                 A_UINT8 * bChans,  A_UINT16 availPiers,
                                 A_UINT16 tPdGainOverlap, A_INT16 *pMinCalPower, A_UINT16 * pPdGainBoundaries,
                                 A_UINT8 * pPDADCValues, A_UINT16 * xpdGainValues, A_UINT16 numXpdGains);

static A_BOOL
ar6000FillVpdTable(A_UINT8 pMin, A_UINT8 pMax, A_UINT8 *pwrList,
                   A_UINT8 *vpdList, A_UINT16 numIntercepts, A_UINT8 *pRetVpdList);

#if !defined(ZEROCAL_TOOL)
static void readEepromOldStyle (
 A_UINT32 devNum
);

static void readEepromLocs (
 A_UINT32 devNum
);

/**************************************************************
 * ar6000EepromAttach
 *
 * Attach either the provided data stream or EEPROM to the EEPROM data structure
 */
A_BOOL
ar6000EepromAttach(A_UINT32 devNum)
{
    LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
    A_UINT16 sum = 0;
    A_UINT16 *pHalf;
    A_UINT32 i;

    //printf("ar6000EepromAttach\n");
    //readEepromOldStyle(devNum);    
    readEepromLocs(devNum);    
    pLibDev->ar6kEep = (AR6K_EEPROM *)dummyEepromWriteArea;

    pHalf = (A_UINT16 *)pLibDev->ar6kEep;

    for (i = 0; i < sizeof(AR6K_EEPROM)/2; i++) {
        sum ^= *pHalf++;
    }

    /* Check CRC - Attach should fail on a bad checksum */
    if (sum != 0xffff) {
        //mError(devNum, EIO, "%s: bad EEPROM checksum 0x%x\n", __func__, sum);
        return FALSE;
    }

    /* Setup some values checked by common code */
    pLibDev->eepData.version = pLibDev->ar6kEep->baseEepHeader.version;
    pLibDev->eepData.protect = 0;
    pLibDev->eepData.turboDisabled = TRUE;
    pLibDev->eepData.currentRD = pLibDev->ar6kEep->baseEepHeader.regDmn;
    /* Do we need to fill in the tpcMap? */

    return TRUE;
}

static A_UINT32 newEepromArea[AR6K_EEPROM_SIZE];

void readEepromOldStyle (
 A_UINT32 devNum
)
{
    A_UINT32 jj;
    A_UINT16 *tempPtr = (A_UINT16 *)dummyEepromWriteArea;

    eepromReadBlock(devNum, 0, (sizeof(AR6K_EEPROM) / 2), newEepromArea);
        
    for(jj=0; jj < (sizeof(AR6K_EEPROM) / 2); jj++) {
        *(tempPtr+jj) = *(newEepromArea + jj);  
    }
}

void readEepromLocs (
 A_UINT32 devNum
)
{
    LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
    A_UINT32 jj;
    A_UINT8 *tempPtr = (A_UINT8 *)dummyEepromWriteArea;

    //eepromReadBlock(devNum, 0, 256, newEepromArea);
    if (pLibDev->devMap.remoteLib && (pLibDev->devMap.r_eepromReadLocs != NULL)) {
	   pLibDev->devMap.r_eepromReadLocs(devNum, 0, (sizeof(AR6K_EEPROM) / 2), tempPtr);
    }
        
/*
    for(jj=0; jj < 256; jj++) {
        *(tempPtr+jj) = *(newEepromArea + jj);  
    }
*/
}

/**************************************************************
 * ar6000EepromSetBoardValues
 *
 * Read EEPROM header info and program the device for correct operation
 * given the channel value.
 */
void
ar6000EepromSetBoardValues(A_UINT32 devNum, A_UINT32 freq)
{
    LIB_DEV_INFO  *pLibDev = gLibInfo.pLibDevArray[devNum];
    int is2GHz = ((pLibDev->mode == MODE_11G) || (pLibDev->mode == MODE_11B));
    MODAL_EEP_HEADER *pModal;
    A_UINT16 antWrites[11];
    A_UINT32 regVal;
    A_UINT8  deltaVal, curVal;
    A_CHAR fieldName[50];
    int i;

    assert(((pLibDev->eepData.version >> 12) & 0xF) == AR6000_EEP_VER);
    pModal = &(pLibDev->ar6kEep->modalHeader[is2GHz]);

    /* Set the antenna register(s) correctly for the chip revision */
    antWrites[0] = pModal->antCtrl0;
    antWrites[1] = pModal->antCtrl[0] & ANTCTRL_MASK;
    antWrites[2] = (pModal->antCtrl[0] >> ANTCTRL_SHIFT) & ANTCTRL_MASK;
    antWrites[3] = (pModal->antCtrl[0] >> (ANTCTRL_SHIFT *2)) & ANTCTRL_MASK;
    antWrites[4] = (pModal->antCtrl[0] >> (ANTCTRL_SHIFT *3)) & ANTCTRL_MASK;
    antWrites[5] = (pModal->antCtrl[0] >> (ANTCTRL_SHIFT *4)) & ANTCTRL_MASK;
    antWrites[6] = pModal->antCtrl[1] & ANTCTRL_MASK;
    antWrites[7] = (pModal->antCtrl[1] >> ANTCTRL_SHIFT) & ANTCTRL_MASK;
    antWrites[8] = (pModal->antCtrl[1] >> (ANTCTRL_SHIFT *2)) & ANTCTRL_MASK;
    antWrites[9] = (pModal->antCtrl[1] >> (ANTCTRL_SHIFT *3)) & ANTCTRL_MASK;
    antWrites[10] = (pModal->antCtrl[1] >> (ANTCTRL_SHIFT *4)) & ANTCTRL_MASK;
    /*printf(".....antWrites %x %x %x %x %x %x %x %x %x %x\n", antWrites[0], antWrites[1], antWrites[2], antWrites[3], 
        antWrites[4], antWrites[5], antWrites[6], antWrites[7], antWrites[8], antWrites[9], antWrites[10]); */ 
    forceAntennaTbl5211(devNum, antWrites);

    writeField(devNum, "bb_switch_settling", pModal->switchSettling);
    writeField(devNum, "bb_adc_desired_size", pModal->adcDesiredSize);
#if defined(AR6001)
    writeField(devNum, "bb_pga_desired_size", pModal->pgaDesiredSize);
    writeField(devNum, "bb_txrxatten", pModal->txRxAtten);
    writeField(devNum, "bb_rxtx_margin_2ghz", pModal->rxTxMargin);
#elif defined(AR6002)
#if !defined(MERCURY_EMULATION)
    writeField(devNum, "bb_xatten1_margin", pModal->xAtten1Margin);
    writeField(devNum, "bb_xatten1_db", pModal->xAtten1Db);
    writeField(devNum, "bb_xatten1_hyst_margin", pModal->xAtten1Hyst);
    writeField(devNum, "bb_xatten2_margin", pModal->xAtten2Margin);
    writeField(devNum, "bb_xatten2_db", pModal->xAtten2Db);
    writeField(devNum, "bb_xatten2_hyst_margin", pModal->xAtten2Hyst);
    writeField(devNum, "an_level", pModal->xpaBiasLvl);
    if (isMercury2_0(devNum))
        writeField(devNum, "an_level2", pModal->xpaBiasLvl2);

    if (pLibDev->mode == MODE_11G) {
        writeField(devNum, "an_ob", pModal->ob);
        writeField(devNum, "an_db", pModal->db);
    }
    else {
        writeField(devNum, "an_ob5", pModal->ob);
        writeField(devNum, "an_db5", pModal->db);
    }
	
#endif
    writeField(devNum, "bb_pd_gain_overlap", pModal->pdGainOverlap);
#endif
    writeField(devNum, "bb_tx_end_to_xpaa_off", pModal->txEndToXpaOff);
    writeField(devNum, "bb_tx_end_to_xpab_off", pModal->txEndToXpaOff);
    writeField(devNum, "bb_tx_frame_to_xpaa_on", pModal->txFrameToXpaOn);
    writeField(devNum, "bb_tx_frame_to_xpab_on", pModal->txFrameToXpaOn);

    writeField(devNum, "bb_tx_end_to_xlna_on",pModal->txEndToXlnaOn);
    writeField(devNum, "bb_thresh62", pModal->thresh62);
    if (pModal->initTxGain)
        writeField(devNum, "bb_init_tx_gain_setting", pModal->initTxGain);

    /* write previous IQ results */
    writeField(devNum, "bb_iqcorr_q_i_coff", pModal->iqCalI);
    writeField(devNum, "bb_iqcorr_q_q_coff", pModal->iqCalQ);
    // writeField(devNum, "bb_do_iqcal", 1);

    /* Set an_sellna and an_selintpd according to the opFlag bit */
    /* TBD: We could set them based on configBits now, given selLna, selIntPd are explicitly stored */
    if (pLibDev->ar6kEep->baseEepHeader.opFlags & AR6000_OPFLAGS_LNA2_FE_NOXPA ) {
        writeField(devNum, "an_sellna", 1);
        writeField(devNum, "an_selintpd", 1);
    }
    else if (pLibDev->ar6kEep->baseEepHeader.opFlags & AR6000_OPFLAGS_LNA2_FRONTEND ) {
        writeField(devNum, "an_sellna", 1);
        writeField(devNum, "an_selintpd", 0);
    }
    else { /* default to 0rBOM */
        writeField(devNum, "an_sellna", 0);
        writeField(devNum, "an_selintpd", 1);
    }

    /* apply tx gain table if enabled */
    if (pLibDev->ar6kEep->baseEepHeader.opFlags & AR6000_OPFLAGS_TXGAIN_TBL) {
        //writeField(devNum, "bb_tx_gain_table_0", pModal->txGainTbl_0);
        curVal = pModal->txGainTbl_0;

        for (i=0; i<AR6000_TX_GAIN_TBL_SIZE; i++) {
            sprintf(fieldName, "%s%d", TXGAIN_TABLE_STRING, i);
            regVal = (A_UINT32)getFieldForMode(devNum, fieldName, pLibDev->mode, pLibDev->turbo);
            if (i %2)   
                deltaVal = (pModal->txGainTblDelta[i/2]>>4) &0xf;
            else // 0 and even start from lower nible
                deltaVal = pModal->txGainTblDelta[i/2] &0xf;
            curVal += deltaVal;
            regVal &= ~TXGAIN_TABLE_GAININHALFDB_MASKFULL;
            regVal |= (((A_UINT32)curVal << TXGAIN_TABLE_GAININHALFDB_LSB) & TXGAIN_TABLE_GAININHALFDB_MASKFULL); 
            writeField(devNum, fieldName, regVal);
        }

    }

    pLibDev->eepromHeaderApplied[pLibDev->mode] = TRUE;
    return;
}
#endif //#if !defined(ZEROCAL_TOOL)

/**************************************************************
 * ar6000EepromSetTransmitPower
 *
 * Set the transmit power in the baseband for the given
 * operating channel and mode.
 */
void
ar6000EepromSetTransmitPower(A_UINT32 devNum, A_UINT32 freq, A_UINT16 *pRates)
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))
#define N(a)            (sizeof (a) / sizeof (a[0]))
#if !defined(ZEROCAL_TOOL)
    LIB_DEV_INFO  *pLibDev = gLibInfo.pLibDevArray[devNum];
    AR6K_EEPROM *ar6kEep = pLibDev->ar6kEep;
    A_BOOL eepromLoad = pLibDev->eePromLoad;
    A_UCHAR mode = pLibDev->mode;
#endif //ZEROCAL_TOOL
    A_UINT16 ratesArray[NUM_RATES];
    A_UINT16 powerLimit = AR6000_MAX_RATE_POWER;
    A_INT16  txPowerIndexOffset = 0;
    A_UINT16 cfgCtl;
    A_UINT16 AntennaReduction;
    A_INT8   cckOfdmDelta=0;
    int i;
#if defined(ZEROCAL_TOOL)
    char fileName[50];
    FILE *fStream;
    AR6K_EEPROM *ar6kEep = &eepStruct;
    A_BOOL eepromLoad = TRUE;
    A_UCHAR mode = MODE_11G;
    if ((freq >= 4900) && (freq < 6000))
        mode = MODE_11A;
#endif //ZEROCAL_TOOL

    memset(ratesArray, 0, sizeof(ratesArray));

    if (pRates) {
        memcpy(ratesArray, pRates, sizeof(A_UINT16) * NUM_RATES);
    }

#if !defined(ZEROCAL_TOOL)
    if(pLibDev->eePromLoad) {
        assert(((pLibDev->eepData.version >> 12) & 0xF) == AR6000_EEP_VER);
#endif //ZEROCAL_TOOL
        cfgCtl = ar6kEep->baseEepHeader.regDmn & 0xFF;
        AntennaReduction = (cfgCtl == 0x10) ? 6 : 0;
        cckOfdmDelta = ar6kEep->baseEepHeader.cckOfdmDelta;
        switch (mode) {
        case MODE_11A:
            cfgCtl |= CTL_11A;
            break;
        case MODE_11G:
        case MODE_11O:
            cfgCtl |= CTL_11G;
            break;
        case MODE_11B:
            cfgCtl |= CTL_11B;
            break;
        default:
            assert(0);
        } 
        if (!pRates) {
            ar6000SetPowerPerRateTable(devNum, freq, ratesArray, cfgCtl, AntennaReduction, powerLimit);
        }
        ar6000SetPowerCalTable(devNum, freq, &txPowerIndexOffset);
#if !defined(ZEROCAL_TOOL)
    }
#endif //ZEROCAL_TOOL

    /*
     * txPowerIndexOffset is set by the SetPowerTable() call -
     *  adjust the rate table (0 offset if rates EEPROM not loaded)
     */
    for (i = 0; i < N(ratesArray); i++) {
        ratesArray[i] += txPowerIndexOffset;
        if (ratesArray[i] > AR6000_MAX_RATE_POWER)
            ratesArray[i] = AR6000_MAX_RATE_POWER;
	if(i > 7) { // for CCK, may need to increase power depending on board configuration (PD)
            ratesArray[i] += cckOfdmDelta;
        }
    }
#if defined(ZEROCAL_TOOL)
    sprintf(fileName, "csvfiles/rateArray_%d.csv", freq);
    if( (fStream = fopen(fileName, "a")) == NULL) {
        uiPrintf("Could not open rateArray file %s\n", fileName);
        return;
    }

    fprintf(fStream, "%d, %d, %d, %d, %d, %d, %d, %d,\n", ratesArray[0], ratesArray[1], ratesArray[2], ratesArray[3], ratesArray[4], ratesArray[5], ratesArray[6], ratesArray[7]);

    fclose(fStream);
#else
    pLibDev->pwrIndexOffset = txPowerIndexOffset;
	
//printf("txPowerIndexOffest = %d\n", txPowerIndexOffset);
    /* Write the OFDM power per rate set */
    REGW(devNum, 0x9934,
        POW_SM(ratesArray[3], 24)
          | POW_SM(ratesArray[2], 16)
          | POW_SM(ratesArray[1],  8)
          | POW_SM(ratesArray[0],  0)
    );
    
#ifdef DEBUG1
	printf("\n0x9934 = 0x%x [0x%x]\n",
 	(POW_SM(ratesArray[3], 24)
          | POW_SM(ratesArray[2], 16)
          | POW_SM(ratesArray[1],  8)
             | POW_SM(ratesArray[0],  0)), REGR(devNum, 0x9934));
#endif

    REGW(devNum, 0x9938,
        POW_SM(ratesArray[7], 24)
          | POW_SM(ratesArray[6], 16)
          | POW_SM(ratesArray[5],  8)
          | POW_SM(ratesArray[4],  0)
    );
#ifdef DEBUG1	
	printf("0x9938 = 0x%x [0x%x]\n", (POW_SM(ratesArray[7], 24)
          | POW_SM(ratesArray[6], 16)
          | POW_SM(ratesArray[5],  8)
                                        | POW_SM(ratesArray[4],  0)), REGR(devNum, 0x9938)
    );
#endif

    /* Write the CCK power per rate set */
    REGW(devNum, 0xa234,
        POW_SM(ratesArray[10], 24)
          | POW_SM(ratesArray[9],  16)
          | POW_SM(ratesArray[15],  8) /* XR target power */
          | POW_SM(ratesArray[8],   0)
    );
    REGW(devNum, 0xa238,
        POW_SM(ratesArray[14], 24)
          | POW_SM(ratesArray[13], 16)
          | POW_SM(ratesArray[12],  8)
          | POW_SM(ratesArray[11],  0)
    );

    /*
     * Set max power to 31.5 dBm and, optionally,
     * enable TPC in tx descriptors.
     */
    REGW(devNum, 0x993c, AR6000_MAX_RATE_POWER);
#endif //ZEROCAL_TOOL
    return;
#undef N
#undef POW_SM
}

/**************************************************************
 * ar6000SetPowerPerRateTable
 *
 * Sets the transmit power in the baseband for the given
 * operating channel and mode.
 */
static void
ar6000SetPowerPerRateTable(A_UINT32 devNum, A_UINT32 freq, A_UINT16 *ratesArray,
                           A_UINT16 cfgCtl, A_UINT16 AntennaReduction, A_UINT16 powerLimit)
{
#if !defined(ZEROCAL_TOOL)
    LIB_DEV_INFO  *pLibDev = gLibInfo.pLibDevArray[devNum];
    A_BOOL   is2GHz = ((pLibDev->mode == MODE_11G) || (pLibDev->mode == MODE_11B));
    A_UCHAR  mode = pLibDev->mode;
    AR6K_EEPROM *ar6kEep = pLibDev->ar6kEep;
#endif
    A_UINT16 twiceMaxEdgePower = AR6000_MAX_RATE_POWER;
    A_UINT16 twiceMaxEdgePowerCck = AR6000_MAX_RATE_POWER;
    A_UINT16 twiceMaxRDPower = AR6000_MAX_RATE_POWER;
    int i;
    A_INT16  twiceAntennaReduction;
    CAL_CTL_DATA *rep;
    CAL_TARGET_POWER  targetPower;
    A_INT16 scaledPower;
#if defined(ZEROCAL_TOOL)
    AR6K_EEPROM *ar6kEep = &eepStruct;
    A_BOOL is2GHz=TRUE;
    A_UCHAR mode = MODE_11G;
    if ((freq >= 4900) && (freq < 6000)) {
        is2GHz = FALSE;
        mode = MODE_11A;
    }
#endif

    twiceMaxRDPower = AR6000_MAX_RATE_POWER;

    /* Compute TxPower reduction due to Antenna Gain */
    twiceAntennaReduction = A_MAX((AntennaReduction * 2) - ar6kEep->modalHeader[is2GHz].antennaGain, 0);
    for (i = 0; (i < AR6000_NUM_CTLS) && ar6kEep->ctlIndex[i]; i++) {
        A_UINT16 twiceMinEdgePower;

        if ((cfgCtl == ar6kEep->ctlIndex[i]) ||
            (cfgCtl == ((ar6kEep->ctlIndex[i] & CTL_MODE_M) | SD_NO_CTL)))
        {
            rep = &(ar6kEep->ctlData[i]);
            twiceMinEdgePower = ar6000GetMaxEdgePower(freq, rep->ctlEdges, is2GHz);
            //for cus99
#if !defined(ZEROCAL_TOOL)
            twiceMinEdgePower -= 2 * (A_INT16)getPowerRef();                     
#endif //#if !defined(ZEROCAL_TOOL)
            if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL) {
                /* Find the minimum of all CTL edge powers that apply to this channel */
                twiceMaxEdgePower = A_MIN(twiceMaxEdgePower, twiceMinEdgePower);
            } else {
                twiceMaxEdgePower = twiceMinEdgePower;
                break;
            }
        }
    }

    if (mode == MODE_11G) {
        /* Check for a CCK CTL for 11G CCK powers */
        cfgCtl = (cfgCtl &~ ~CTL_MODE_M) | CTL_11B;
        for (i = 0; (i < AR6000_NUM_CTLS) && ar6kEep->ctlIndex[i]; i++) {
            A_UINT16 twiceMinEdgePowerCck;
            if ((cfgCtl == ar6kEep->ctlIndex[i]) ||
                (cfgCtl == ((ar6kEep->ctlIndex[i] & CTL_MODE_M) | SD_NO_CTL)))
            {
                rep = &(ar6kEep->ctlData[i]);
                twiceMinEdgePowerCck = ar6000GetMaxEdgePower(freq, rep->ctlEdges, is2GHz);
                if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL) {
                    /* Find the minimum of all CTL edge powers that apply to this channel */
                    twiceMaxEdgePowerCck = A_MIN(twiceMaxEdgePowerCck, twiceMinEdgePowerCck);
                } else {
                    twiceMaxEdgePowerCck = twiceMinEdgePowerCck;
                    break;
                }
            }
        }
    } else {
        /* Set the 11B cck edge power to the one found before */
        twiceMaxEdgePowerCck = twiceMaxEdgePower;
    }

    /* Get OFDM target powers */
    if (is2GHz) {
        ar6000GetTargetPowers(freq, ar6kEep->calTargetPower11G,
                              AR6000_NUM_11G_TARGET_POWERS, &targetPower, is2GHz);
    } else {
        ar6000GetTargetPowers(freq, ar6kEep->calTargetPower11A,
                              AR6000_NUM_11A_TARGET_POWERS, &targetPower, is2GHz);
    }

    /* Reduce by CTL edge power and regulatory allowable power */
    scaledPower = A_MIN(twiceMaxEdgePower, twiceMaxRDPower + twiceAntennaReduction);

    /* Apply PER target power restriction */
    scaledPower = A_MIN(scaledPower, targetPower.tPow6to24);

    /* Reduce power by user set power limit */
    scaledPower = A_MIN(scaledPower, powerLimit);

    /* Set OFDM rates 9, 12, 18, 24 */
    ratesArray[0] = ratesArray[1] = ratesArray[2] = ratesArray[3] = ratesArray[4] = scaledPower;

    /* Set OFDM rates 36, 48, 54, XR */
    ratesArray[5] = A_MIN(scaledPower, targetPower.tPow36);
    ratesArray[6] = A_MIN(scaledPower, targetPower.tPow48);
    ratesArray[7] = A_MIN(scaledPower, targetPower.tPow54);
    /* XR uses 6mb power */
    ratesArray[15] = scaledPower;

    if (is2GHz) {
        /* Get final CCK target powers */
        ar6000GetTargetPowers(freq, ar6kEep->calTargetPower11B,
                              AR6000_NUM_11B_TARGET_POWERS, &targetPower, is2GHz);

        /* Reduce by CTL edge power and regulatory allowable power */
        scaledPower = A_MIN(twiceMaxEdgePowerCck, twiceMaxRDPower + twiceAntennaReduction);

        /* Apply PER target power restriction */
        scaledPower = A_MIN(scaledPower, targetPower.tPow6to24);

        /* Reduce power by user set power limit */
        scaledPower = A_MIN(scaledPower, powerLimit);

        /* Set CCK rates 2L, 2S, 5.5L, 5.5S, 11L, 11S */
        ratesArray[8]  = scaledPower;
        ratesArray[9]  = A_MIN(scaledPower, targetPower.tPow36);
        ratesArray[10] = ratesArray[9];
        ratesArray[11] = A_MIN(scaledPower, targetPower.tPow48);
        ratesArray[12] = ratesArray[11];
        ratesArray[13] = A_MIN(scaledPower, targetPower.tPow54);
        ratesArray[14] = ratesArray[13];
    }
    return;
}


/**************************************************************
 * ar6000GetMaxEdgePower
 *
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static A_UINT16
ar6000GetMaxEdgePower(A_UINT32 freq, CAL_CTL_EDGES *pRdEdgesPower, A_BOOL is2GHz)
{
    A_UINT16 twiceMaxEdgePower = AR6000_MAX_RATE_POWER;
    int      i;

    /* Get the edge power */
    for (i = 0; (i < AR6000_NUM_BAND_EDGES) && (pRdEdgesPower[i].bChannel != AR6000_BCHAN_UNUSED) ; i++) {
        /*
         * If there's an exact channel match or an inband flag set
         * on the lower channel use the given rdEdgePower
         */
        if (freq == fbin2freq(pRdEdgesPower[i].bChannel, is2GHz)) {
            twiceMaxEdgePower = pRdEdgesPower[i].tPower;
            break;
        } else if ((i > 0) && (freq < fbin2freq(pRdEdgesPower[i].bChannel, is2GHz))) {
            if (fbin2freq(pRdEdgesPower[i - 1].bChannel, is2GHz) < freq && pRdEdgesPower[i - 1].flag) {
                twiceMaxEdgePower = pRdEdgesPower[i - 1].tPower;
            }
            /* Leave loop - no more affecting edges possible in this monotonic increasing list */
            break;
        }
    }
    assert(twiceMaxEdgePower > 0);
    return twiceMaxEdgePower;
}


/**************************************************************
 * ar6000GetTargetPowers
 *
 * Return the four rates of target power for the given target power table
 * channel, and number of channels
 */
static void
ar6000GetTargetPowers(A_UINT32 freq, CAL_TARGET_POWER *powInfo,
                      A_UINT16 numChannels, CAL_TARGET_POWER *pNewPower, A_BOOL is2GHz)
{
    int clo, chi;
    int i;
    int matchIndex = -1, lowIndex = -1;

    /* Copy the target powers into the temp channel list */
    if (freq <= fbin2freq(powInfo[0].bChannel, is2GHz)) {
        matchIndex = 0;
    } else {
        for (i = 0; (i < numChannels) && (powInfo[i].bChannel != AR6000_BCHAN_UNUSED); i++) {
            if (freq == fbin2freq(powInfo[i].bChannel, is2GHz)) {
                matchIndex = i;
                break;
            } else if ((freq < fbin2freq(powInfo[i].bChannel, is2GHz)) &&
                       (freq > fbin2freq(powInfo[i - 1].bChannel, is2GHz)))
            {
                lowIndex = i - 1;
                break;
            }
        }
        if ((matchIndex == -1) && (lowIndex == -1)) {
            assert(freq > fbin2freq(powInfo[i - 1].bChannel, is2GHz));
            matchIndex = i - 1;
        }
    }

    if (matchIndex != -1) {
        *pNewPower = powInfo[matchIndex];
    } else {
        assert(lowIndex != -1);
        /*
         * Get the lower and upper channels, target powers,
         * and interpolate between them.
         */
        clo = fbin2freq(powInfo[lowIndex].bChannel, is2GHz);
        chi = fbin2freq(powInfo[lowIndex + 1].bChannel, is2GHz);
        pNewPower->tPow6to24 = interpolate(freq, clo, chi,
                                          powInfo[lowIndex].tPow6to24, powInfo[lowIndex + 1].tPow6to24);
        pNewPower->tPow36 = interpolate(freq, clo, chi,
                                       powInfo[lowIndex].tPow36, powInfo[lowIndex + 1].tPow36);
        pNewPower->tPow48 = interpolate(freq, clo, chi,
                                       powInfo[lowIndex].tPow48, powInfo[lowIndex + 1].tPow48);
        pNewPower->tPow54 = interpolate(freq, clo, chi,
                                       powInfo[lowIndex].tPow54, powInfo[lowIndex + 1].tPow54);
    }
}


/**************************************************************
 * ar6000SetPowerCalTable
 *
 * Pull the PDADC piers from cal data and interpolate them across the given
 * points as well as from the nearest pier(s) to get a power detector
 * linear voltage to power level table.
 */
static void
ar6000SetPowerCalTable(A_UINT32 devNum, A_UINT32 freq, A_INT16 *pTxPowerIndexOffset)
{
#if !defined(ZEROCAL_TOOL)
    LIB_DEV_INFO  *pLibDev = gLibInfo.pLibDevArray[devNum];
    A_BOOL   is2GHz = ((pLibDev->mode == MODE_11G) || (pLibDev->mode == MODE_11B));
    AR6K_EEPROM *ar6kEep = pLibDev->ar6kEep;
#endif // ZEROCAL_TOOL
    CAL_DATA_PER_FREQ *pRawDataset = NULL;
    A_UINT8  *pCalBChans = NULL;
    A_UINT16 pdGainOverlap_t2;
    static A_UINT8  pdadcValues[AR6000_NUM_PDADC_VALUES];
    A_UINT16 gainBoundaries[AR6000_PD_GAINS_IN_MASK];
    A_UINT16 numPiers;
    A_INT16  tMinCalPower;
    A_UINT16 numXpdGain, xpdMask;
    A_UINT16 xpdGainValues[AR6000_NUM_PD_GAINS];
    A_UINT32 i, reg32, regOffset;
#if defined(ZEROCAL_TOOL)
    char fileName[50];
    FILE *fStream, *fp;
    A_BOOL is2GHz=TRUE;
    AR6K_EEPROM *ar6kEep = &eepStruct;
    if ((freq >= 4900) && (freq < 6000))
        is2GHz = FALSE;
#endif

    if (is2GHz) {
        pRawDataset = ar6kEep->calPierData11G;
        pCalBChans = ar6kEep->calFreqPier11G;
        numPiers = AR6000_NUM_11G_CAL_PIERS;
        xpdMask = ar6kEep->modalHeader[1].xpdGain;
    } else {
        pRawDataset = ar6kEep->calPierData11A;
        pCalBChans = ar6kEep->calFreqPier11A;
        numPiers = AR6000_NUM_11A_CAL_PIERS;
        xpdMask = ar6kEep->modalHeader[0].xpdGain;
    }

#if defined(ZEROCAL_TOOL)
    pdGainOverlap_t2 = 5;   // TBD: get the real value in zero CAL tool?
#else
    pdGainOverlap_t2 = REGR(devNum, TPCRG5_REG) & BB_PD_GAIN_OVERLAP_MASK;
#endif //ZEROCAL_TOOL

    numXpdGain = 0;
    /* Calculate the value of xpdgains from the xpdGain Mask */
    for (i = 1; i <= AR6000_PD_GAINS_IN_MASK; i++) {
        if ((xpdMask >> (AR6000_PD_GAINS_IN_MASK - i)) & 1) {
            if (numXpdGain >= AR6000_NUM_PD_GAINS) {
                assert(0);
                break;
            }
            xpdGainValues[numXpdGain] = AR6000_PD_GAINS_IN_MASK - i;
#if !defined(ZEROCAL_TOOL)
            pLibDev->eepData.xpdGainValues[numXpdGain] = xpdGainValues[numXpdGain];
#endif //#if !defined(ZEROCAL_TOOL)
            numXpdGain++;
        }
    }
#if !defined(ZEROCAL_TOOL)
    pLibDev->eepData.numPdGain = numXpdGain;
#endif //#if !defined(ZEROCAL_TOOL)

    ar6000GetGainBoundariesAndPdadcs(devNum, freq, pRawDataset, pCalBChans, numPiers, pdGainOverlap_t2,
                                     &tMinCalPower, gainBoundaries, pdadcValues, xpdGainValues, numXpdGain);

#if defined(ZEROCAL_TOOL)
    sprintf(fileName, "csvfiles/pdadc_%d%d_%d.csv", xpdGainValues[0], xpdGainValues[1], freq);
    if( (fStream = fopen(fileName, "a")) == NULL) {
        uiPrintf("Could not open pdadcVal file %s\n", fileName);
        return;
    }

    for (i = 0; i < 32; i++) {
	fprintf(fStream, "%3d, %3d, %3d, %3d, ", pdadcValues[4*i], pdadcValues[4*i + 1], 
            pdadcValues[4*i + 2], pdadcValues[4*i + 3]);
    }
    fprintf(fStream, "\n");

    fclose(fStream);

    *pTxPowerIndexOffset = ar6kEep->modalHeader[1].txPowerOffset;
    if (tMinCalPower != 0)
        *pTxPowerIndexOffset -= tMinCalPower;

    if( (fp = fopen("csvfiles/xpdGain.txt", "a")) == NULL) {
        uiPrintf("Could not open xpdGain.txt\n");
        return;
    }
    fprintf(fp, "freq %d, Num_PdGains %d, xpdGain1 %d, xpdGain2 %d, overlap_t2 %d, gainBoundary0 %d, gainBoundary1 %d, tMinCalPwr %d, txPowerIndexOffset %d\n",
        freq, numXpdGain, xpdGainValues[0], xpdGainValues[1], pdGainOverlap_t2, gainBoundaries[0], gainBoundaries[1], tMinCalPower, *pTxPowerIndexOffset );
    fclose(fp);
#else // ZEROCAL_TOOL

    pLibDev->eepData.midPower = gainBoundaries[0];

#ifdef DEBUG3
    printf("\ngainB0 %d, 1 %d, minCalPower %d, midPower %d\n", gainBoundaries[0], gainBoundaries[1], 
        tMinCalPower, pLibDev->eepData.midPower);
#endif

    REGW(devNum, TPCRG1_REG, (REGR(devNum, TPCRG1_REG) & 0xFFFF3FFF) | 
         (((numXpdGain - 1) & 0x3) << 14));

    writeField(devNum, "bb_pd_gain_setting1", pLibDev->eepData.xpdGainValues[0]);
    if (numXpdGain > 1)
        writeField(devNum, "bb_pd_gain_setting2", pLibDev->eepData.xpdGainValues[1]);
    if (numXpdGain > 2)
        writeField(devNum, "bb_pd_gain_setting3", pLibDev->eepData.xpdGainValues[2]);
    /*printf("...ar6000SetPowerCalTable numPdGain %d, xpdGain0 %d, xpdGain1 %d\n", numXpdGain, 
        pLibDev->eepData.xpdGainValues[0], pLibDev->eepData.xpdGainValues[1] ); */

    /*
     * Note the pdadc table may not start at 0 dBm power, could be
     * negative or greater than 0.  Need to offset the power
     * values by the amount of minPower for griffin
     */
#if defined(AR6001)
    if (tMinCalPower != 0) {
        *pTxPowerIndexOffset = 0 - tMinCalPower;
    } else {
        *pTxPowerIndexOffset = 0;
    }
#elif defined(AR6002)
    if (is2GHz) 
        *pTxPowerIndexOffset = pLibDev->ar6kEep->modalHeader[1].txPowerOffset;
    else 
        *pTxPowerIndexOffset = pLibDev->ar6kEep->modalHeader[0].txPowerOffset;

    if (tMinCalPower != 0) { 
        *pTxPowerIndexOffset -= tMinCalPower;
        // Take into account the minimum calibrated power non-zero dBm case
        for (i=0;i<numXpdGain;i++) {
            gainBoundaries[i] -= tMinCalPower;
        }
    }
#endif
    
#ifdef DEBUG3
    printf("\nnumPdGains = %d, xpdGain1 = %d, xpdGain2 = %d, overlap = %d, gainB0 %d, 1 %d, 2 %d, tMinCalPower %d, txPwrIdxOffset %d\n",
    numXpdGain, xpdGainValues[0], xpdGainValues[1], pdGainOverlap_t2, gainBoundaries[0], gainBoundaries[1], gainBoundaries[2], tMinCalPower, *pTxPowerIndexOffset);
#endif    
    /* Finally, write the power values into the baseband power table */
    regOffset = 0x9800 + (672 << 2); /* beginning of pdadc table in griffin */
    for (i = 0; i < 32; i++) {
        reg32 = ((pdadcValues[4*i + 0] & 0xFF) << 0)  |
            ((pdadcValues[4*i + 1] & 0xFF) << 8)  |
            ((pdadcValues[4*i + 2] & 0xFF) << 16) |
            ((pdadcValues[4*i + 3] & 0xFF) << 24) ;
        REGW(devNum, regOffset, reg32);
//	printf("Snoop: regOffset = %x, regValue = %x\n", regOffset, reg32);

	regOffset += 4;
	
	
#ifdef DEBUG2
	printf("PDADC %3d Value %3d|PDADC %3d Value %3d|PDADC %3d Value %3d|PDADC %3d Value %3d|\n",
    	4*i, pdadcValues[4*i],
	4*i + 1, pdadcValues[4*i + 1],
	4*i + 2, pdadcValues[4*i + 2],
	4*i + 3, pdadcValues[4*i + 3]);
#endif	
    }

    REGW(devNum, 0xa26c,
         (pdGainOverlap_t2 & 0xf) | 
         ((gainBoundaries[0] & 0x3f) << 4)  |
         ((gainBoundaries[1] & 0x3f) << 10) |
         ((gainBoundaries[2] & 0x3f) << 16) |
         ((gainBoundaries[3] & 0x3f) << 22));

	 
/*    printf("SNOOP: overlap register 0xa26c = %x\n",
         (pdGainOverlap_t2 & 0xf) | 
         ((gainBoundaries[0] & 0x3f) << 4)  |
         ((gainBoundaries[1] & 0x3f) << 10) |
         ((gainBoundaries[2] & 0x3f) << 16) |
         ((gainBoundaries[3] & 0x3f) << 22));
  */  
          

#endif //#if !defined(ZEROCAL_TOOL)
    return;
}


/**************************************************************
 * ar6000GetGainBoundariesAndPdadcs
 *
 * Uses the data points read from EEPROM to reconstruct the pdadc power table
 * Called by ar6000SetPowerCalTable only.
 */
static void
ar6000GetGainBoundariesAndPdadcs(A_UINT32 devNum, A_UINT32 freq, CAL_DATA_PER_FREQ * pRawDataSet, 
                                 A_UINT8 * bChans,  A_UINT16 availPiers,
                                 A_UINT16 tPdGainOverlap, A_INT16 *pMinCalPower, A_UINT16 * pPdGainBoundaries,
                                 A_UINT8 * pPDADCValues, A_UINT16 * xpdGainValues, A_UINT16 numXpdGains)
{
#if !defined(ZEROCAL_TOOL)
    LIB_DEV_INFO  *pLibDev = gLibInfo.pLibDevArray[devNum];
    A_BOOL    is2GHz = ((pLibDev->mode == MODE_11G) || (pLibDev->mode == MODE_11B));
#endif //ZEROCAL_TOOL
    int       i, j, k;
    A_INT16   ss;         /* potentially -ve index for taking care of pdGainOverlap */
    A_UINT16  idxL, idxR, numPiers; /* Pier indexes */

    /* filled out Vpd table for all pdGains (chanL) */
    static A_UINT8   vpdTableL[AR6000_NUM_PD_GAINS][AR6000_MAX_PWR_RANGE_IN_HALF_DB];

    /* filled out Vpd table for all pdGains (chanR) */
    static A_UINT8   vpdTableR[AR6000_NUM_PD_GAINS][AR6000_MAX_PWR_RANGE_IN_HALF_DB];

    /* filled out Vpd table for all pdGains (interpolated) */
    static A_UINT8   vpdTableI[AR6000_NUM_PD_GAINS][AR6000_MAX_PWR_RANGE_IN_HALF_DB];

    A_UINT8   *pVpdL, *pVpdR, *pPwrL, *pPwrR;
    A_UINT8   minPwrT4[AR6000_NUM_PD_GAINS];
    A_UINT8   maxPwrT4[AR6000_NUM_PD_GAINS];
    A_UINT16  numVpd = 0;
    A_INT16   vpdStep;
    A_INT16   tmpVal;
    A_UINT16  sizeCurrVpdTable, maxIndex, tgtIndex;
    A_BOOL    match;
#if defined(ZEROCAL_TOOL)
    A_BOOL    is2GHz=TRUE;
    if ((freq >= 4900) && (freq < 6000))
        is2GHz = FALSE;
#endif

    /* Trim numPiers for the number of populated channel Piers */
    for (numPiers = 0; numPiers < availPiers; numPiers++) {
        if (bChans[numPiers] == AR6000_BCHAN_UNUSED) {
            break;
        }
    }

    /* Find pier indexes around the current channel */
    match = getLowerUpperIndex(FREQ2FBIN(freq, is2GHz), bChans,
                       numPiers, &idxL, &idxR);

    if (match) {
        /* Directly fill both vpd tables from the matching index */
        minPwrT4[0] = pRawDataSet[idxL].pwrPdg0[0];
        minPwrT4[1] = pRawDataSet[idxL].pwrPdg1[0];
        maxPwrT4[0] = pRawDataSet[idxL].pwrPdg0[3];
        maxPwrT4[1] = pRawDataSet[idxL].pwrPdg1[4];
        ar6000FillVpdTable(minPwrT4[0], maxPwrT4[0], pRawDataSet[idxL].pwrPdg0,
                           pRawDataSet[idxL].vpdPdg0, 4, vpdTableI[0]);
        ar6000FillVpdTable(minPwrT4[1], maxPwrT4[1], pRawDataSet[idxL].pwrPdg1,
                           pRawDataSet[idxL].vpdPdg1, 5, vpdTableI[1]);
    } else {
        for (i = 0; i < numXpdGains; i++) {
            if (i == 0) {
                numVpd = 4;
                pVpdL = pRawDataSet[idxL].vpdPdg0;
                pPwrL = pRawDataSet[idxL].pwrPdg0;
                pVpdR = pRawDataSet[idxR].vpdPdg0;
                pPwrR = pRawDataSet[idxR].pwrPdg0;
            } else {
                numVpd = 5;
                pVpdL = pRawDataSet[idxL].vpdPdg1;
                pPwrL = pRawDataSet[idxL].pwrPdg1;
                pVpdR = pRawDataSet[idxR].vpdPdg1;
                pPwrR = pRawDataSet[idxR].pwrPdg1;
            }

            /* Start Vpd interpolation from the max of the minimum powers */
            minPwrT4[i] = A_MAX(pPwrL[0], pPwrR[0]);

            /* End Vpd interpolation from the min of the max powers */
            maxPwrT4[i] = A_MIN(pPwrL[numVpd - 1], pPwrR[numVpd - 1]);
            assert(maxPwrT4[i] > minPwrT4[i]);

            /* Fill pier Vpds */
            ar6000FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrL, pVpdL, numVpd, vpdTableL[i]);
            ar6000FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrR, pVpdR, numVpd, vpdTableR[i]);

            /* Interpolate the final vpd */
            for (j = 0; j <= (maxPwrT4[i] - minPwrT4[i]) / 2; j++) {
                vpdTableI[i][j] = interpolate(FREQ2FBIN(freq, is2GHz), bChans[idxL],
                                               bChans[idxR], vpdTableL[i][j], vpdTableR[i][j]);
            }
        }
    }

    if (isZerodBm()) {
        *pMinCalPower = 0;
    } else {
    	*pMinCalPower = minPwrT4[0] / 2;
    }


    k = 0; /* index for the final table */
    for (i = 0; i < numXpdGains; i++) {
        if (i == (numXpdGains - 1)) {
            pPdGainBoundaries[i] = maxPwrT4[i] / 2;
        } else {
            pPdGainBoundaries[i] = ((maxPwrT4[i] + minPwrT4[i+1]) / 4 );
        }
        pPdGainBoundaries[i] = A_MIN(63, pPdGainBoundaries[i]);

        /* Find starting index for this pdGain */
        if (i == 0) {
            if (isZerodBm()){
            	ss = -(minPwrT4[0]/2);
            } else {
                ss = 0;
            } 
        } else {
            ss = (pPdGainBoundaries[i-1] - (minPwrT4[i] / 2)) - tPdGainOverlap + 1;
        }
        vpdStep = (vpdTableI[i][1] - vpdTableI[i][0]);
        vpdStep = ((vpdStep < 1) ? 1 : vpdStep);
        /*
         *-ve ss indicates need to extrapolate data below for this pdGain
         */
        while ((ss < 0) && (k < (AR6000_NUM_PDADC_VALUES - 1))) {
            tmpVal = (vpdTableI[i][0] + ss * vpdStep);
            pPDADCValues[k++] = ((tmpVal < 0) ? 0 : tmpVal);
            ss++;
        }

        sizeCurrVpdTable = (maxPwrT4[i] - minPwrT4[i]) / 2;
        tgtIndex = pPdGainBoundaries[i] + tPdGainOverlap - (minPwrT4[i] / 2);
        maxIndex = (tgtIndex < sizeCurrVpdTable) ? tgtIndex : sizeCurrVpdTable;

        while ((ss < maxIndex) && (k < (AR6000_NUM_PDADC_VALUES - 1))) {
            pPDADCValues[k++] = vpdTableI[i][ss++];
        }

        vpdStep = (vpdTableI[i][sizeCurrVpdTable - 1] - vpdTableI[i][sizeCurrVpdTable - 2]);
        vpdStep = ((vpdStep < 1) ? 1 : vpdStep);
        /*
         * for last gain, pdGainBoundary == Pmax_t2, so will
         * have to extrapolate
         */
        if (tgtIndex > maxIndex) {  /* need to extrapolate above */
            while ((ss <= tgtIndex) && (k < (AR6000_NUM_PDADC_VALUES - 1))) {
                tmpVal = (vpdTableI[i][sizeCurrVpdTable - 1] +
                          (ss - maxIndex) * vpdStep);
                pPDADCValues[k++] = (tmpVal > 255) ? 255 : tmpVal;
                ss++;
            }
        }               /* extrapolated above */
    }                   /* for all pdGainUsed */

    /* Fill out pdGainBoundaries - only up to 2 allowed here, but hardware allows up to 4 */
    while (i < AR6000_PD_GAINS_IN_MASK) {
        pPdGainBoundaries[i] = pPdGainBoundaries[i-1];
        i++;
    }

    while (k < AR6000_NUM_PDADC_VALUES) {
        pPDADCValues[k] = pPDADCValues[k-1];
        k++;
    }
    return;
}


/**************************************************************
 * getLowerUppderIndex
 *
 * Return indices surrounding the value in sorted integer lists.
 * Requirement: the input list must be monotonically increasing
 *     and populated up to the list size
 * Returns: match is set if an index in the array matches exactly
 *     or a the target is before or after the range of the array.
 */
static A_BOOL
getLowerUpperIndex(A_UINT8 target, A_UINT8 *pList, A_UINT16 listSize,
                   A_UINT16 *indexL, A_UINT16 *indexR)
{
    A_UINT16 i;

    /*
     * Check first and last elements for beyond ordered array cases.
     */
    if (target <= pList[0]) {
        *indexL = *indexR = 0;
        return TRUE;
    }
    if (target >= pList[listSize-1]) {
        *indexL = *indexR = listSize - 1;
        return TRUE;
    }

    /* look for value being near or between 2 values in list */
    for (i = 0; i < listSize - 1; i++) {
        /*
         * If value is close to the current value of the list
         * then target is not between values, it is one of the values
         */
        if (pList[i] == target) {
            *indexL = *indexR = i;
            return TRUE;
        }
        /*
         * Look for value being between current value and next value
         * if so return these 2 values
         */
        if (target < pList[i + 1]) {
            *indexL = i;
            *indexR = i + 1;
            return FALSE;
        }
    }
    assert(0);
    return FALSE;
}


/**************************************************************
 * ar6000FillVpdTable
 *
 * Fill the Vpdlist for indices Pmax-Pmin
 * Note: pwrMin, pwrMax and Vpdlist are all in dBm * 4
 */
static A_BOOL
ar6000FillVpdTable(A_UINT8 pwrMin, A_UINT8 pwrMax, A_UINT8 *pPwrList,
                   A_UINT8 *pVpdList, A_UINT16 numIntercepts, A_UINT8 *pRetVpdList)
{
    A_UINT16  i, k;
    A_UINT8   currPwr = pwrMin;
    A_UINT16  idxL, idxR;

    assert(pwrMax > pwrMin);
    for (i = 0; i <= (pwrMax - pwrMin) / 2; i++) {
        getLowerUpperIndex(currPwr, pPwrList, numIntercepts,
                           &(idxL), &(idxR));
        if (idxR < 1)
            idxR = 1;           /* extrapolate below */
        if (idxL == numIntercepts - 1)
            idxL = numIntercepts - 2;   /* extrapolate above */
        if (pPwrList[idxL] == pPwrList[idxR])
            k = pVpdList[idxL];
        else
            k = ( ((currPwr - pPwrList[idxL]) * pVpdList[idxR] + (pPwrList[idxR] - currPwr) * pVpdList[idxL]) /
                  (pPwrList[idxR] - pPwrList[idxL]) );
        assert(k < 256);
        pRetVpdList[i] = (A_UINT8)k;
        currPwr += 2;               /* half dB steps */
    }

    return TRUE;
}


/**************************************************************************
 * interpolate
 *
 * Returns signed interpolated or the scaled up interpolated value
 */
static A_INT16
interpolate(A_UINT16 target, A_UINT16 srcLeft, A_UINT16 srcRight,
            A_INT16 targetLeft, A_INT16 targetRight)
{
    A_INT16 rv;

    if (srcRight == srcLeft) {
        rv = targetLeft;
    } else {
        rv = ((target - srcLeft) * targetRight +
              (srcRight - target) * targetLeft) / (srcRight - srcLeft);
    }
    return rv;
}

/**************************************************************************
 * fbin2freq
 *
 * Get channel value from binary representation held in eeprom
 * RETURNS: the frequency in MHz
 */
static A_UINT16
fbin2freq(A_UINT8 fbin, A_BOOL is2GHz)
{
    /*
     * Reserved value 0xFF provides an empty definition both as
     * an fbin and as a frequency - do not convert
     */
    if (fbin == AR6000_BCHAN_UNUSED) {
        return fbin;
    }

    return ((is2GHz) ? (2300 + fbin) : (4800 + 5 * fbin));
}

#if !defined(ZEROCAL_TOOL)
void getAR6000Struct(
 A_UINT32 devNum,
 void **ppReturnStruct,     //return ptr to struct asked for
 A_UINT32 *pNumBytes        //return the size of the structure
)
{
    LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
    
    *ppReturnStruct = pLibDev->ar6kEep;
    *pNumBytes = sizeof(AR6K_EEPROM);
    return;
}

void
ar6000GetPowerPerRateTable(A_UINT32 devNum, A_UINT16 freq, A_INT16 *pRatesPower)
{
    LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
    A_UINT16 cfgCtl = pLibDev->ar6kEep->baseEepHeader.regDmn & 0xFF;
    A_UINT16 AntennaReduction = (cfgCtl == 0x10) ? 6 : 0;
    A_UINT16 powerLimit = AR6000_MAX_RATE_POWER;

    switch (pLibDev->mode) {
    case MODE_11A:
        cfgCtl |= CTL_11A;
        break;
    case MODE_11G:
    case MODE_11O:
        cfgCtl |= CTL_11G;
        break;
    case MODE_11B:
        cfgCtl |= CTL_11B;
        break;
    default:
        assert(0);
    }

    assert(pRatesPower);
    ar6000SetPowerPerRateTable(devNum, freq, (A_UINT16 *)pRatesPower, cfgCtl, AntennaReduction, powerLimit);
}
#endif //ZEROCAL_TOOL

