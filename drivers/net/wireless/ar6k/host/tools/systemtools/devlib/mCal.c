/* mCal.c - contains functions related to radio calibration */

/* Copyright (c) 2001 Atheros Communications, Inc., All Rights Reserved */

/* 
Revsision history
--------------------
1.0       Created.
*/

#ifdef __ATH_DJGPPDOS__
#define __int64	long long
typedef unsigned long DWORD;
#endif	// #ifdef __ATH_DJGPPDOS__

#include <errno.h>
#include "wlantype.h"
#include "mConfig.h"
#include "manlib.h"

#if defined(SPIRIT_AP) || defined(FREEDOM_AP)
#include "misclib.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//++JC++
static AR2413_TXGAIN_TBL griffin_tx_gain_tbl[] =
{
#include  "AR2413_tx_gain.tbl"
} ;

static AR2413_TXGAIN_TBL spider_tx_gain_tbl[] =
{
#include  "spider.tbl"
} ;

static AR2413_TXGAIN_TBL eagle_tx_gain_tbl_2[] =
{
#include  "ar5413_tx_gain_2.tbl"
} ;
static AR2413_TXGAIN_TBL eagle_tx_gain_tbl_5[] =
{
#include  "ar5413_tx_gain_5.tbl"
} ;

#define  AR2413_TX_GAIN_TBL_SIZE  26
//#define  AR6000_TX_GAIN_TBL_SIZE  22
//#define  TXGAIN_TABLE_STRING      "bb_tx_gain_table_"
#define  TXGAIN_TABLE_FILENAME    "ar6002_tx_gain_2.tbl"
#define  TXGAIN_TABLE_FILENAME_A   "ar6002_tx_gain_2A.tbl"

#define  TXGAIN_TABLE_TX1DBLOQGAIN_LSB           0
#define  TXGAIN_TABLE_TX1DBLOQGAIN_MASK          0x7
#define  TXGAIN_TABLE_TXV2IGAIN_LSB              3
#define  TXGAIN_TABLE_TXV2IGAIN_MASK             0x3
#define  TXGAIN_TABLE_PABUF5GN_LSB               5
#define  TXGAIN_TABLE_PABUF5GN_MASK              0x1
#define  TXGAIN_TABLE_PADRVGN_LSB                6
#define  TXGAIN_TABLE_PADRVGN_MASK               0x7
#define  TXGAIN_TABLE_PAOUT2GN_LSB               9
#define  TXGAIN_TABLE_PAOUT2GN_MASK              0x7
//#define  TXGAIN_TABLE_GAININHALFDB_LSB           12
//#define  TXGAIN_TABLE_GAININHALFDB_MASK          0x7F

//#define  CREATE_TXGAIN_TABLE_FILE   1

static A_BOOL generateTxGainTblFromCfg_mode(A_UINT32 devNum, A_UINT32 mode, AR6002_TXGAIN_TBL *txGainTbl, char *fileName);
static AR6002_TXGAIN_TBL mercury_tx_gain_table_2[AR6000_TX_GAIN_TBL_SIZE];
static AR6002_TXGAIN_TBL mercury_tx_gain_table_2A[AR6000_TX_GAIN_TBL_SIZE];

MANLIB_API A_BOOL generateTxGainTblFromCfg(A_UINT32 devNum)
{
    generateTxGainTblFromCfg_mode(devNum, MODE_11A, mercury_tx_gain_table_2A, TXGAIN_TABLE_FILENAME_A);
    generateTxGainTblFromCfg_mode(devNum, MODE_11G, mercury_tx_gain_table_2,  TXGAIN_TABLE_FILENAME);
    return(TRUE);
}

static A_BOOL generateTxGainTblFromCfg_mode(A_UINT32 devNum, A_UINT32 mode, AR6002_TXGAIN_TBL *txGainTbl, char *fileName)
{
#if defined(AR6002)
    A_UINT32 i, val;
    A_CHAR fieldName[50];
    LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
    if (pLibDev != NULL) {
      if (isMercury_sd(pLibDev->swDevID)) {
#if defined(CREATE_TXGAIN_TABLE_FILE)
    FILE *fd;

    if ( NULL == (fd = fopen(fileName, "w")) ) {
        printf("....Failed to open %s\n", fileName);
        return(FALSE);
    } 
#endif

    for (i=0; i<AR6000_TX_GAIN_TBL_SIZE; i++) {
        sprintf(fieldName, "%s%d", TXGAIN_TABLE_STRING, i);      
        val = getFieldForMode(devNum, fieldName, mode, pLibDev->turbo);
        txGainTbl[i].txldBloqgain = (val >> TXGAIN_TABLE_TX1DBLOQGAIN_LSB) & TXGAIN_TABLE_TX1DBLOQGAIN_MASK; 
        txGainTbl[i].txV2Igain = (val >> TXGAIN_TABLE_TXV2IGAIN_LSB) & TXGAIN_TABLE_TXV2IGAIN_MASK; 
        txGainTbl[i].pabuf5gn = (val >> TXGAIN_TABLE_PABUF5GN_LSB) & TXGAIN_TABLE_PABUF5GN_MASK; 
        txGainTbl[i].padrvgn = (val >> TXGAIN_TABLE_PADRVGN_LSB) & TXGAIN_TABLE_PADRVGN_MASK; 
        txGainTbl[i].paout2gn = (val >> TXGAIN_TABLE_PAOUT2GN_LSB) & TXGAIN_TABLE_PAOUT2GN_MASK; 
        txGainTbl[i].desired_gain = (val >> TXGAIN_TABLE_GAININHALFDB_LSB) & TXGAIN_TABLE_GAININHALFDB_MASK; 

#if defined(CREATE_TXGAIN_TABLE_FILE)
        fprintf(fd, "%d   %d   %d   %d   %d   %d\n", txGainTbl[i].desired_gain, txGainTbl[i].paout2gn, 
            txGainTbl[i].padrvgn, txGainTbl[i].pabuf5gn, txGainTbl[i].txV2Igain,
            txGainTbl[i].txldBloqgain); 
#endif
    }
    
#if defined(CREATE_TXGAIN_TABLE_FILE)
    fclose(fd);
#endif
      }
    }
#endif
    return(TRUE);
}

MANLIB_API void ForceSinglePCDACTableMercury(A_UINT32 devNum, A_UINT16 pcdac, A_UINT16 offset)
{
	A_UINT16 i;
	A_UINT32 dac_gain = 0;
	AR6002_TXGAIN_TBL *pGainTbl; 
   	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

        //printf(".....ForceSinglePCDACTableMercury pcdac:%d offset_1:%d\n ", pcdac, offset); 
        if(pLibDev->mode == MODE_11A) {
            if (offset > 20) {
	        offset -= 20;
	    }

	    pGainTbl = mercury_tx_gain_table_2A;
        }		
        else
	    pGainTbl = mercury_tx_gain_table_2;

	if(pGainTbl == NULL) {
            mError(devNum, EIO, "Error: unable to initialize gainTable in ForceSinglePCDACTableMercury\n");
	}
	i = 0;
	if(offset != GAIN_OVERRIDE) {
	    pcdac = (A_UINT16)(pcdac + offset);		// Offset pcdac to get in a reasonable range
	}
	while ((pcdac > pGainTbl[i].desired_gain) && (i < (AR6000_TX_GAIN_TBL_SIZE -1)) ) {
            //printf(".....pcdac:%d i %d des_gain %d\n ", pcdac, i, pGainTbl[i].desired_gain);
            i++;
        }  // Find entry closest
	if (pGainTbl[i].desired_gain > pcdac) {
            dac_gain = pGainTbl[i].desired_gain - pcdac;
	}
	writeField(devNum, "bb_force_dac_gain", 1);
	writeField(devNum, "bb_forced_dac_gain", dac_gain);
	writeField(devNum, "bb_force_tx_gain", 1);
	writeField(devNum, "bb_forced_paout2gn", pGainTbl[i].paout2gn);
	writeField(devNum, "bb_forced_padrvgn", pGainTbl[i].padrvgn);
	writeField(devNum, "bb_forced_txV2Igain", pGainTbl[i].txV2Igain);
	writeField(devNum, "bb_forced_txldBloqgain", pGainTbl[i].txldBloqgain);
        if (pLibDev->mode == MODE_11A) {
	    writeField(devNum, "bb_forced_pabuf5gn", pGainTbl[i].pabuf5gn);
        }
        //printf("...i:%d pcdac:%d offset:%d desiredGain:%d dac_gain:%d paout2gn:0x%x padrvgn:0x%x txV2Igain:0x%x txldBloqgain:0x%x \n",i, pcdac, offset, pGainTbl[i].desired_gain, dac_gain, pGainTbl[i].paout2gn, pGainTbl[i].padrvgn, pGainTbl[i].txV2Igain, pGainTbl[i].txldBloqgain);
        //printf("0x%x\n", pLibDev->devMap.OSmem32Read(devNum, 0x2a274)); 
        //getchar(); 
	return;
}

MANLIB_API void ForceSinglePCDACTableGriffin(A_UINT32 devNum, A_UINT16 pcdac, A_UINT16 offset)
{
#if defined(AR6001) || defined(MERCURY_EMULATION)
	A_UINT16 i;
	A_UINT32 dac_gain = 0;
	AR2413_TXGAIN_TBL *pGainTbl = NULL;
	
   	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];

	if(isSpider(pLibDev->swDevID)) {
		pGainTbl = spider_tx_gain_tbl;
            printf("SNOOP: using spider gain table\n");
	}
	else if(isGriffin(pLibDev->swDevID)) {
		pGainTbl = griffin_tx_gain_tbl;
	}
	else if(isEagle(pLibDev->swDevID)) {
		if(pLibDev->mode == MODE_11A) {
			pGainTbl = eagle_tx_gain_tbl_5;
			offset += 10;
		} else {
			pGainTbl = eagle_tx_gain_tbl_2;
		}
	}
	if(isDragon(devNum)) {
		if(pLibDev->mode == MODE_11A) {
			if (offset > 20) {
				offset -= 20;
			}
		} else {
			//offset += 20;
			offset +=10;
		}		
	}

	if(pGainTbl == NULL) {
		mError(devNum, EIO, "Error: unable to initialize gainTable in ForceSinglePCDACTableGriffin\n");
	}
	i = 0;
	if(offset != GAIN_OVERRIDE) {
		if (pLibDev->mode == MODE_11G) {
			offset = (A_UINT16)(offset + 10);	// Up the offset for 11b mode
		}
		pcdac = (A_UINT16)(pcdac + offset);		// Offset pcdac to get in a reasonable range
	}
//	printf("SNOOP: pcdac = %d\n", pcdac);
	while ((pcdac > pGainTbl[i].desired_gain) && (i < AR2413_TX_GAIN_TBL_SIZE) ) {i++;}  // Find entry closest
	if (pGainTbl[i].desired_gain > pcdac) {
            dac_gain = pGainTbl[i].desired_gain - pcdac;
	}
//printf("dg = %d, bb1 = %d, bb2 = %d, if = %d, rf = %d\n",
//		dac_gain, pGainTbl[i].bb1_gain, pGainTbl[i].bb2_gain, pGainTbl[i].if_gain, pGainTbl[i].rf_gain);
	writeField(devNum, "bb_force_dac_gain", 1);
	writeField(devNum, "bb_forced_dac_gain", dac_gain);
	writeField(devNum, "bb_force_tx_gain", 1);
	writeField(devNum, "bb_forced_txgainbb1", pGainTbl[i].bb1_gain);
	writeField(devNum, "bb_forced_txgainbb2", pGainTbl[i].bb2_gain);
	writeField(devNum, "bb_forced_txgainif", pGainTbl[i].if_gain);
	writeField(devNum, "bb_forced_txgainrf", pGainTbl[i].rf_gain);
#elif defined(AR6002)
        ForceSinglePCDACTableMercury(devNum, pcdac, offset);
#else
//#error "Unknown arch"
#endif
	return;
}
//++JC++

MANLIB_API void ForceSinglePCDACTable(A_UINT32 devNum, A_UINT16 pcdac, A_INT32 offset)
{
	A_UINT16 temp16, i;
	A_UINT32 temp32;
	A_UINT32 regoffset ;

//++JC++
	LIB_DEV_INFO *pLibDev = gLibInfo.pLibDevArray[devNum];
        if (isMercury_sd(pLibDev->swDevID)) {
            ForceSinglePCDACTableMercury(devNum, pcdac, offset);
            return;
        }
	else if(isGriffin(pLibDev->swDevID) || isEagle(pLibDev->swDevID))  {    // For Griffin
		ForceSinglePCDACTableGriffin(devNum, pcdac, offset);  // By default, offset of 30
		return;
	}
//++JC++
	temp16 = (A_UINT16) (0x0000 | (pcdac << 8) | 0x00ff);
	temp32 = (temp16 << 16) | temp16 ;

	regoffset = 0x9800 + (608 << 2) ;
	for (i=0; i<32; i++)
	{
		REGW(devNum, regoffset, temp32);
		regoffset += 4;
	}

	return;
}

/* Return the mac address of the mac specified 
   wmac = 0 (for ethernet) wmac = 1 (for wireless)
   instance number (used only for ethernet) to get the mac address, if more than one 
   ethernet mac is present.
   macAddr - buffer to return the mac address
 */
MANLIB_API void getMacAddr(A_UINT32 devNum, A_UINT16 wmac, A_UINT16 instNo, A_UINT8 *macAddr)
{
	A_UINT32 readVal;

	//verify some of the arguments
	if (checkDevNum(devNum) == FALSE) {
		mError(devNum, EINVAL, "Device Number %d:getMacAddr \n", devNum);
		return;
	}

	if (wmac > 1) {
		mError(devNum, EINVAL, "Device Number %d:getMacAddr: Invalid wmac argument \n", devNum);
		return;
	}

	if (macAddr == NULL) {
		mError(devNum, EINVAL, "Device Number %d:getMacAddr: Invalid buffer address for returning mac address \n", devNum);
		return;
	}

#ifndef MDK_AP
	// Client card can have only wmac 
/*
#ifndef PREDATOR_BUILD
	if (wmac == 0) {
		mError(devNum, EINVAL, "Device Number %d:getMacAddr: Client card can read only WMAC address \n", devNum);
		return;
	}
#endif
*/
			// Read the mac address from the eeprom
	readVal = eepromRead(devNum,0x1d);
	macAddr[0] = (A_UINT8)(readVal & 0xff);
	macAddr[1] = (A_UINT8)((readVal >> 8) & 0xff);
	readVal = eepromRead(devNum,0x1e);
	macAddr[2] = (A_UINT8)(readVal & 0xff);
	macAddr[3] = (A_UINT8)((readVal >> 8) & 0xff);
	readVal = eepromRead(devNum,0x1f);
	macAddr[4] = (A_UINT8)(readVal & 0xff);
	macAddr[5] = (A_UINT8)((readVal >> 8) & 0xff);

	instNo = 0; // referencing to remove warning
#endif

#ifdef SPIRIT_AP
	if (spiritGetMacAddr(devNum,wmac,instNo,macAddr) < 0) {
		mError(devNum, EIO, "Get mac address failed \n");
		return;
	}	
#endif // SPIRIT

#ifdef AP22_AP
	mError(devNum, EIO,"Get Mac Address not implemented for AP22 \n");
#endif // AP22

#ifdef FREEDOM_AP
	if (freedomGetMacAddr(devNum,wmac,instNo,macAddr) < 0) {
		mError(devNum, EIO, "Get mac address failed \n");
		return;
	}	
#endif // FREEDOM

#ifdef SENAO_AP
	mError(devNum, EIO,"Get Mac Address not implemented for senao \n");
#endif // SENAO


	return;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
