// Copyright (c) 2004-2006 Atheros Communications Inc.
// 
//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: mmc_defs.h

@abstract: MMC definitions not already defined in _sdio_defs.h
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef ___MMC_DEFS_H___
#define ___MMC_DEFS_H___

#define MMC_MAX_BUS_CLOCK    20000000 /* max clock speed in hz */
#define MMC_HS_MAX_BUS_CLOCK 52000000 /* MMC PLUS (high speed) max clock rate in hz */

/* R2 (CSD) macros */
#define GET_MMC_CSD_TRANS_SPEED(pR) (pR)[12]
#define GET_MMC_SPEC_VERSION(pR)    (((pR)[15] >> 2) & 0x0F)
#define MMC_SPEC_1_0_TO_1_2         0x00
#define MMC_SPEC_1_4                0x01
#define MMC_SPEC_2_0_TO_2_2         0x02
#define MMC_SPEC_3_1                0x03
#define MMC_SPEC_4_0_TO_4_1         0x04

#define MMC_CMD_SWITCH    6
#define MMC_CMD8    8

#define MMC_SWITCH_CMD_SET    0
#define MMC_SWITCH_SET_BITS   1
#define MMC_SWITCH_CLEAR_BITS 2
#define MMC_SWITCH_WRITE_BYTE 3
#define MMC_SWITCH_CMD_SET0   0
#define MMC_SWITCH_BUILD_ARG(cmdset,access,index,value) \
     (((cmdset) & 0x07) | (((access) & 0x03) << 24) | (((index) & 0xFF) << 16) | (((value) & 0xFF) << 8)) 

#define MMC_EXT_CSD_SIZE                     512

#define MMC_EXT_S_CMD_SET_OFFSET             504
#define MMC_EXT_MIN_PERF_W_8_52_OFFSET       210  
#define MMC_EXT_MIN_PERF_R_8_52_OFFSET       209
#define MMC_EXT_MIN_PERF_W_8_26_4_52_OFFSET  208
#define MMC_EXT_MIN_PERF_R_8_26_4_52_OFFSET  207
#define MMC_EXT_MIN_PERF_W_4_26_OFFSET       206
#define MMC_EXT_MIN_PERF_R_4_56_OFFSET       205  
#define MMC_EXT_PWR_CL_26_360_OFFSET         203
#define MMC_EXT_PWR_CL_52_360_OFFSET         202
#define MMC_EXT_PWR_CL_26_195_OFFSET         201
#define MMC_EXT_PWR_CL_52_195_OFFSET         200
#define MMC_EXT_GET_PWR_CLASS(reg)    ((reg) & 0xF)
#define MMC_EXT_MAX_PWR_CLASSES       16
#define MMC_EXT_CARD_TYPE_OFFSET             196
#define MMC_EXT_CARD_TYPE_HS_52  (1 << 1)
#define MMC_EXT_CARD_TYPE_HS_26  (1 << 0)
#define MMC_EXT_CSD_VER_OFFSET               194
#define MMC_EXT_VER_OFFSET                   192
#define MMC_EXT_VER_1_0          0
#define MMC_EXT_VER_1_1          1
#define MMC_EXT_CMD_SET_OFFSET               191
#define MMC_EXT_CMD_SET_REV_OFFSET           189
#define MMC_EXT_PWR_CLASS_OFFSET             187
#define MMC_EXT_HS_TIMING_OFFSET             185
#define MMC_EXT_HS_TIMING_ENABLE   0x01
#define MMC_EXT_BUS_WIDTH_OFFSET             183
#define MMC_EXT_BUS_WIDTH_1_BIT    0x00
#define MMC_EXT_BUS_WIDTH_4_BIT    0x01
#define MMC_EXT_BUS_WIDTH_8_BIT    0x02

#endif
