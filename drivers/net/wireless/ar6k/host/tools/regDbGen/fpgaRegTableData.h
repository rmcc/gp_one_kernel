/*
 * Copyright (c) 2004-2005 Atheros Communications Inc.
 * All rights reserved.
 *
 *
 * This file contains the actual Regulatory domain data. Use the 
 * DB schema from etna/include/regulatory/reg_dbschema.h and 
 * values from etna/include/regulatory/reg_dbvalues.h
 * These Data definations are used by both generator and parser!
 *
 */

#ifndef __AH_REG_TABLE_DATA_H__
#define __AH_REG_TABLE_DATA_H__

static REG_DMN_PAIR_MAPPING regDomainPairs[] = {
    {FCC1_FCCA,     FCC1,           FCCA,           NO_REQ, NO_REQ, PSCAN_DEFER},
};


static COUNTRY_CODE_TO_ENUM_RD allCountries[] = {
    {CTRY_DEBUG,       NO_ENUMRD,     "DB",  REGDB_YES}, 
    {CTRY_DEFAULT,     DEF_REGDMN,    "NA",  REGDB_YES},
    {CTRY_UNITED_STATES, FCC1_FCCA,   "US",  REGDB_YES},
};


static REG_DMN_FREQ_BAND regDmn5GhzFreq[] = {
    { 4920, 4980, 23, 20, NO_DFS, MODE_11A, PSCAN_MKK2},                /* F1_4920_4980 */
    { 5040, 5080, 23, 20, NO_DFS, MODE_11A, PSCAN_MKK2},                /* F1_5040_5080 */

    { 5120, 5240, 5,  20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F1_5120_5240 */

    { 5170, 5230, 23, 20, NO_DFS, MODE_11A, PSCAN_MKK1 | PSCAN_MKKA1 | PSCAN_MKK2 | PSCAN_MKKA2},           /* F1_5170_5230 */

    { 5180, 5240, 15, 20, NO_DFS, MODE_11A, PSCAN_FCC | PSCAN_ETSI},     /* F1_5180_5240 */
    { 5180, 5240, 17, 20, NO_DFS, MODE_11A, PSCAN_FCC},                  /* F2_5180_5240 */
    { 5180, 5240, 18, 20, NO_DFS, MODE_11A, PSCAN_FCC | PSCAN_ETSI},     /* F3_5180_5240 */
    { 5180, 5240, 20, 20, NO_DFS, MODE_11A, PSCAN_FCC | PSCAN_ETSI},     /* F4_5180_5240 */
    { 5180, 5240, 23, 20, NO_DFS, MODE_11A, PSCAN_FCC | PSCAN_ETSI | PSCAN_MKKA | PSCAN_MKK2},     /* F5_5180_5240 */
    { 5180, 5240, 23, 20, NO_DFS, MODE_11A, PSCAN_FCC},                  /* F6_5180_5240 */

    { 5240, 5280, 23, 20, DFS_FCC3, MODE_11A, PSCAN_FCC | PSCAN_ETSI},   /* F1_5240_5280 */

    { 5260, 5280, 23, 20, DFS_FCC3 | DFS_ETSI, MODE_11A, PSCAN_FCC | PSCAN_ETSI},   /* F1_5260_5280 */

    { 5260, 5320, 18, 20, DFS_FCC3 | DFS_ETSI, MODE_11A, PSCAN_FCC | PSCAN_ETSI},   /* F1_5260_5320 */

    { 5260, 5320, 20, 20, DFS_FCC3 | DFS_ETSI | DFS_MKK3, MODE_11A, PSCAN_FCC | PSCAN_ETSI | PSCAN_MKK3},
                                            /* F2_5260_5320 */

    { 5260, 5320, 20, 20, DFS_FCC3 | DFS_ETSI, MODE_11A, PSCAN_FCC},      /* F3_5260_5320 */
    { 5260, 5320, 23, 20, DFS_FCC3 | DFS_ETSI | DFS_MKK3, MODE_11A,
                          PSCAN_FCC | PSCAN_MKK1 | PSCAN_MKKA | PSCAN_MKK2 | PSCAN_MKKA1 | PSCAN_MKKA2},  /* F4_5260_5320 */

    { 5260, 5700, 5,  20, DFS_FCC3 | DFS_ETSI, MODE_11A, NO_PSCAN},        /* F1_5260_5700 */

    { 5280, 5320, 17, 20, DFS_FCC3 | DFS_ETSI, MODE_11A, PSCAN_FCC},       /* F1_5280_5320 */

    { 5500, 5700, 20, 20, DFS_FCC3 | DFS_ETSI, MODE_11A, PSCAN_FCC},        /* F1_5500_5700 */
    { 5500, 5700, 27, 20, DFS_FCC3 | DFS_ETSI, MODE_11A, PSCAN_FCC | PSCAN_ETSI},   /* F2_5500_5700 */
    { 5500, 5700, 30, 20, DFS_FCC3 | DFS_ETSI, MODE_11A, PSCAN_FCC | PSCAN_ETSI},   /* F3_5500_5700 */
    { 5500, 5700, 23, 20, DFS_FCC3 | DFS_ETSI | DFS_MKK3, MODE_11A,  PSCAN_MKK3 | PSCAN_FCC},
                                            /* F4_5500_5700 */

    { 5745, 5805, 23, 20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F1_5745_5805 */
    { 5745, 5805, 30, 20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F2_5745_5805 */

    { 5745, 5825, 5,  20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F1_5745_5825 */
    { 5745, 5825, 17, 20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F2_5745_5825 */
    { 5745, 5825, 20, 20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F3_5745_5825 */  
    { 5745, 5825, 30, 20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F4_5745_5825 */
    { 5745, 5825, 30, 20, NO_DFS, MODE_11A, NO_PSCAN},                  /* F5_5745_5825 */

    /* Below are the world roaming channels */

    { 4920, 4980, 17, 20, NO_DFS, MODE_11A, PSCAN_WWR},                 /* W1_4920_4980 */
    { 5040, 5080, 17, 20, NO_DFS, MODE_11A,  PSCAN_WWR},                /* W1_5040_5080 */
    { 5170, 5230, 15, 20, DFS_FCC3 | DFS_ETSI,  MODE_11A, PSCAN_WWR},   /* W1_5170_5230 */
    { 5180, 5240, 15, 20, DFS_FCC3 | DFS_ETSI,  MODE_11A, PSCAN_WWR},   /* W1_5180_5240 */
    { 5260, 5320, 18, 20, DFS_FCC3 | DFS_ETSI,  MODE_11A, PSCAN_WWR},   /* W1_5260_5320 */
    { 5745, 5825, 20, 20, NO_DFS, MODE_11A, PSCAN_WWR},                 /* W1_5745_5825 */
    { 5500, 5700, 20, 20, DFS_FCC3 | DFS_ETSI,  MODE_11A, PSCAN_WWR},   /* W1_5500_5700 */
};

static REG_DMN_FREQ_BAND regDmn2Ghz_BG_Freq[] = {
    { 2312, 2372, 5,  5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG1_2312_2372 */
    { 2312, 2372, 20, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG2_2312_2372 */

    { 2412, 2472, 5,  5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG1_2412_2472 */
    { 2412, 2472, 20, 5, NO_DFS, MODE_11G,  PSCAN_MKKA},     /* BG2_2412_2472 */
    { 2412, 2472, 30, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG3_2412_2472 */

    { 2412, 2462, 27, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG1_2412_2462 */
    { 2412, 2462, 20, 5, NO_DFS, MODE_11G,  PSCAN_MKKA},     /* BG2_2412_2462 */
    
    { 2432, 2442, 20, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG1_2432_2442 */

    { 2457, 2472, 20, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG1_2457_2472 */

    { 2467, 2472, 20, 5, NO_DFS, MODE_11G,  PSCAN_MKKA2 | PSCAN_MKKA}, /* BG1_2467_2472 */

    { 2484, 2484, 5,  5, NO_DFS, MODE_11B,  NO_PSCAN},       /* BG1_2484_2484 */
    { 2484, 2484, 20, 5, NO_DFS, MODE_11B,  PSCAN_MKKA | PSCAN_MKKA1 | PSCAN_MKKA2}, /* BG2_2484_2484 */

    { 2512, 2732, 5,  5, NO_DFS, MODE_11G,  NO_PSCAN},       /* BG1_2512_2732 */

    { 2312, 2372, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG1_2312_2372 */
    { 2412, 2412, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG1_2412_2412 */
    { 2417, 2432, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG1_2417_2432 */
    { 2437, 2442, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG1_2437_2442 */
    { 2447, 2457, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG1_2447_2457 */
    { 2462, 2462, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG1_2462_2462 */
    { 2467, 2467, 18, 5, NO_DFS, MODE_11G,  PSCAN_WWR},      /* WBG1_2467_2467 */
    { 2467, 2467, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG2_2467_2467 */
    { 2472, 2472, 18, 5, NO_DFS, MODE_11G,  PSCAN_WWR},      /* WBG1_2472_2472 */
    { 2472, 2472, 18, 5, NO_DFS, MODE_11G,  NO_PSCAN},       /* WBG2_2472_2472 */
    { 2484, 2484, 18, 5, NO_DFS, MODE_11B,  PSCAN_WWR},      /* WBG1_2484_2484 */
    { 2484, 2484, 18, 5, NO_DFS, MODE_11B,  NO_PSCAN},       /* WBG2_2484_2484 */
};



static REG_DOMAIN regDomains[] = {
    {FCC1, FCC, 6, NO_DFS, NO_REQ, NO_PSCAN, 
     BM(F2_5180_5240,F4_5260_5320,F5_5745_5825, -1, -1, -1, -1, -1),
     BMZERO},

    {FCCA, FCC, 6, NO_DFS, NO_REQ, NO_PSCAN, 
     BMZERO,
     BM(BG1_2412_2462,-1,-1,-1,-1,-1,-1,-1)},
};

#endif /* __AH_REG_TABLE_DATA_H__ */
