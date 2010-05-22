/*
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mEepStruct6000.h#5 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mEepStruct6000.h#5 $"

#ifndef _AR6000_EEPROM_STRUCT_H_
#define _AR6000_EEPROM_STRUCT_H_

#define AR6000_EEP_VER               0xF
#define AR6000_EEP_VER_MINOR         0x1
#define AR6000_NUM_11A_CAL_PIERS     8
#define AR6000_NUM_11G_CAL_PIERS     4
#define AR6000_NUM_11A_TARGET_POWERS 4
#define AR6000_NUM_11B_TARGET_POWERS 2
#define AR6000_NUM_11G_TARGET_POWERS 3
#define AR6000_NUM_CTLS              8
#define AR6000_NUM_BAND_EDGES        8
#define AR6000_NUM_PD_GAINS          2 // Allow support for 1 or 2
#define AR6000_PD_GAINS_IN_MASK      4
#define AR6000_EEPROM_MODAL_SPURS    5
#define AR6000_MAX_RATE_POWER        63
#define AR6000_NUM_PDADC_VALUES      128
#define AR6000_NUM_RATES             16
#define AR6000_BCHAN_UNUSED          0xFF
#define AR6000_MAX_PWR_RANGE_IN_HALF_DB 64
#define AR6000_OPFLAGS_11A           1
#define AR6000_OPFLAGS_11G           2
#define AR6000_OPFLAGS_LNA2_FRONTEND 0x100
#define AR6000_OPFLAGS_0dBm          0x200
#define AR6000_OPFLAGS_LNA2_FE_NOXPA 0x400
#define AR6000_OPFLAGS_TXGAIN_TBL    0x800
#define AR6000_OPFLAGS_ANT_DIVERSITY 0x1000
#define FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))

#define NEG_PWR_OFFSET        (-20)

#if defined(AR6001) || defined(MERCURY_EMULATION)
#define ANTCTRL_MASK          0x3F
#define ANTCTRL_SHIFT         6
#elif defined(AR6002)
#define ANTCTRL_MASK          0x1F
#define ANTCTRL_SHIFT         5
#else
#error "Unknown arch"
#endif

typedef enum ConformanceTestLimits {
    FCC        = 0x10,
    MKK        = 0x40,
    ETSI       = 0x30,
    SD_NO_CTL  = 0xE0,
    NO_CTL     = 0xFF,
    CTL_MODE_M = 7,
    CTL_11A    = 0,
    CTL_11B    = 1,
    CTL_11G    = 2,
    CTL_TURBO  = 3,
    CTL_108G   = 4
} ATH_CTLS;

#if defined(AR6001)
typedef struct BaseEepHeader {
    A_UINT16  checksum;
    A_UINT16  version;
    A_UINT16  regDmn;
    A_UINT8   macAddr[6];
    A_UINT16  opFlags;
    A_UINT8   custData[30];
} __ATTRIB_PACK BASE_EEP_HEADER;
#elif defined(AR6002)
typedef struct BaseEepHeader {
    A_UINT32  length;               // The total length of EEPROM data (768)
    A_UINT16  checksum;
    A_UINT16  version;
    A_UINT16  regDmn;
    A_UINT8   macAddr[6];
    A_UINT16  opFlags;              // May need an OpFlag or parameter to differentiate 2GHz LNA path (which changes Rx Gain tables)                        // May also need an OpFlag for whether the device is operating in old or new NF mode
    A_UINT16  subSystemId;
    A_UINT16  blueToothOptions[2];  // Future use for fixed BT setup based on necessary BT coexistence
    A_UINT32  binBuildNumber;       // A calibration binary tracking #
    A_INT8    negPwrOffset;
    A_INT8    cckOfdmDelta;         // cck_ofdm_delta
    A_UINT8   futureBase[30];       // Reserved space for future parameters
    A_UINT8   custData[112];
} __ATTRIB_PACK BASE_EEP_HEADER;
#else
#error "Unknown arch"
#endif

#if defined(AR6001)
typedef struct ModalEepHeader {
    A_UINT32  antCtrl[2];            // 64 bits
    A_UINT8   antCtrl0;              // 8 bits
    A_INT8    antennaGain;           // 8 bits
    A_UINT8   switchSettling;        // 7 bits
    A_UINT8   txRxAtten;             // 6 bits
    A_UINT8   rxTxMargin;            // 6 bits
    A_INT8    adcDesiredSize;        // 8 bits
    A_INT8    pgaDesiredSize;        // 8 bits
    A_UINT8   txEndToXlnaOn;         // 8 bits
    A_UINT8   xlnaGain;              // 8 bits
    A_UINT8   txEndToXpaOff;         // 8 bits
    A_UINT8   txFrameToXpaOn;        // 8 bits
    A_UINT8   thresh62;              // 8 bits
    A_INT8    noiseFloorThresh;      // 8 bits
    A_UINT8   xpdGain:4,             // 4 bits
              xpd    :4;             // 4 bits
    A_INT8    iqCalI;                // 6 bits
    A_INT8    iqCalQ;                // 5 bits
} __ATTRIB_PACK MODAL_EEP_HEADER;
#elif defined(AR6002)
typedef struct ModalEepHeader {
    A_UINT32  antCtrl[2];            // 64 bits
    A_UINT8   antCtrl0;              // 8 bits
    A_INT8    antennaGain;           // 8 bits
    A_UINT8   switchSettling;        // 7 bits
    A_UINT8   xAtten1Hyst;           // Hyst and Margin replace the (previously txRxAtten) value
    A_UINT8   xAtten1Margin;         // One is for moving into and one out of the external attenuation level
    A_UINT8   xAtten1Db;             // The external atten value (previously rxTxMargin)
    A_UINT8   xAtten2Hyst;           // A secondary stage of attenuation which was not encoded into Dragon EEPROM
    A_UINT8   xAtten2Margin;         //
    A_UINT8   xAtten2Db;             //
    A_INT8    adcDesiredSize;        // 8 bits
    A_UINT8   txEndToXlnaOn;         // 8 bits
    A_UINT8   xlnaGain;              // 8 bits
    A_UINT8   txEndToXpaOff;         // 8 bits
    A_UINT8   txFrameToXpaOn;        // 8 bits
    A_INT8    thresh62;              // Note thresh62 is now signed
    A_INT8    noiseFloorThresh;      // 8 bits
    A_UINT8   xpdGain;               // Param has been split to remove bit operation
    A_UINT8   xpd;                   // Param has been split to remove bit operation
    A_INT8    iqCalI;                // 6 bits
    A_INT8    iqCalQ;                // 5 bits
    A_UINT8   pdGainOverlap;         // per solution power detector overlap instead of fixed in the code
    A_UINT8   ob;                     // analog output stage bias (values 1-7)
    A_UINT8   db;                    // analog driver stage bias (values 1-7)
    A_UINT8   xpaBiasLvl;            // bias value for external PA
    A_INT8    txPowerOffset;         // An offset in dBm for all txPower values from 0 dBm in half dB
                                     // e.g., usually a negative offset to allow a device to work below 0 dBm txPower
    A_UINT8   sellna;                // an_sellna, the following 7 bytes allow flexible configuration to simplify s/w 
    A_UINT8   selintpd;              // an_selintpd
    A_UINT8   enablePca;             // an_enable_pca
    A_UINT8   enablePcb;             // an_enable_pcb
    A_UINT8   enableXpaa;            // bb_enable_xpaa
    A_UINT8   enableXpab;            // bb_enable_xpab
    A_UINT8   useTxPdInXpa;          // bb_use_tx_pd_in_xpa
    A_UINT8   xpaBiasLvl2;           // bias value for external PA, additional bits for Mercury 2.0
    A_UINT8   initTxGain;            // bb_init_tx_gain_setting
    A_UINT8   txGainTbl_0;           // bb_tx_gain_table_0
    A_UINT8   txGainTblDelta[11];    // bb_tx_gain_table_x (_1 to _21, 4 bits per delta)
    A_UINT8   futureModal[10];       // Reserved space for future modal values
} __ATTRIB_PACK MODAL_EEP_HEADER;
#else
#error "Unknown arch"
#endif

typedef struct calDataPerFreq {
    A_UINT8 pwrPdg0[4];
    A_UINT8 vpdPdg0[4];
    A_UINT8 pwrPdg1[5];
    A_UINT8 vpdPdg1[5];
} __ATTRIB_PACK CAL_DATA_PER_FREQ;

typedef struct CalTargetPower {
    A_UINT32 bChannel    : 8,
             tPow6to24   : 6,
             tPow36      : 6,
             tPow48      : 6,
             tPow54      : 6;
} __ATTRIB_PACK CAL_TARGET_POWER;

typedef struct CalCtlEdges {
    A_UINT8  bChannel;
    A_UINT8  tPower :6,
             flag   :2;
} __ATTRIB_PACK CAL_CTL_EDGES;

typedef struct CalCtlData {
    CAL_CTL_EDGES  ctlEdges[AR6000_NUM_BAND_EDGES];
} __ATTRIB_PACK CAL_CTL_DATA;

#ifndef AR6001
typedef struct {
    A_UINT8   bChannel;        // Channel encoding similiar to target power channel
    A_INT8    calNFOffset;     // The offset to be added to a dB recorded NF to adjust to dBm (at 25C)
    A_INT8    calNFSlope;      // A dB per degree additional slope adjustment to NF for temp compensation
    A_UINT8   futureNF;        // Currently reserved and for alignment
} __ATTRIB_PACK CAL_NF_PER_FREQ;
#endif
    
#if defined(AR6001)
typedef struct ar6kEeprom {
    BASE_EEP_HEADER    baseEepHeader;                                   // 44 bytes
    MODAL_EEP_HEADER   modalHeader[2];                                  // 48 bytes
    A_UINT16           spurChans[2][AR6000_EEPROM_MODAL_SPURS];         // 20 bytes
    A_UINT8            calFreqPier11A[AR6000_NUM_11A_CAL_PIERS];        // 8 bytes
    A_UINT8            calFreqPier11G[AR6000_NUM_11G_CAL_PIERS];        // 4 bytes
    CAL_DATA_PER_FREQ  calPierData11A[AR6000_NUM_11A_CAL_PIERS];        // 144 bytes
    CAL_DATA_PER_FREQ  calPierData11G[AR6000_NUM_11G_CAL_PIERS];        // 72 bytes
    CAL_TARGET_POWER   calTargetPower11A[AR6000_NUM_11A_TARGET_POWERS]; // 16 bytes
    CAL_TARGET_POWER   calTargetPower11B[AR6000_NUM_11B_TARGET_POWERS]; // 8 bytes
    CAL_TARGET_POWER   calTargetPower11G[AR6000_NUM_11G_TARGET_POWERS]; // 12 bytes
    A_UINT8            ctlIndex[AR6000_NUM_CTLS];                       // 8 bytes
    CAL_CTL_DATA       ctlData[AR6000_NUM_CTLS];                        // 128 bytes
} __ATTRIB_PACK AR6K_EEPROM;                                            // Total 512 bytes

#define AR6K_EEPROM_SIZE   512
#elif defined(AR6002)
typedef struct ar6kEeprom {
    BASE_EEP_HEADER    baseEepHeader;                            // 172 bytes
    MODAL_EEP_HEADER   modalHeader[2];                           // 128 bytes
    A_UINT16           spurChans[2][AR6000_EEPROM_MODAL_SPURS];  // 20 bytes
    A_UINT8            calFreqPier11A[AR6000_NUM_11A_CAL_PIERS]; // 8 bytes
    A_UINT8            calFreqPier11G[AR6000_NUM_11G_CAL_PIERS]; // 4 bytes
    CAL_DATA_PER_FREQ  calPierData11A[AR6000_NUM_11A_CAL_PIERS]; // 144 bytes
    CAL_DATA_PER_FREQ  calPierData11G[AR6000_NUM_11G_CAL_PIERS]; // 72 bytes
    CAL_TARGET_POWER   calTargetPower11A[AR6000_NUM_11A_TARGET_POWERS]; // 16 bytes
    CAL_TARGET_POWER   calTargetPower11B[AR6000_NUM_11B_TARGET_POWERS]; // 8 bytes
    CAL_TARGET_POWER   calTargetPower11G[AR6000_NUM_11G_TARGET_POWERS]; // 12 bytes
    A_UINT8            ctlIndex[AR6000_NUM_CTLS];                       // 8 bytes
    CAL_CTL_DATA       ctlData[AR6000_NUM_CTLS];                        // 128 bytes
    CAL_NF_PER_FREQ    calNFData11A[AR6000_NUM_11A_CAL_PIERS];          // 32 bytes
    CAL_NF_PER_FREQ    calNFData11G[AR6000_NUM_11G_CAL_PIERS];          // 16 bytes
} __ATTRIB_PACK AR6K_EEPROM;                                     // Total 768 bytes

#define AR6K_EEPROM_SIZE   768
#else
#error "Unknown arch"
#endif

#endif //_AR6000_EEPROM_STRUCT_H_
