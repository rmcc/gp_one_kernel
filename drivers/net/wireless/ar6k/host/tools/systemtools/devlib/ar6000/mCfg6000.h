/*
 *  Copyright © 2003 Atheros Communications, Inc.,  All Rights Reserved.
 *
 *  mcfg(5)212.h - Type definitions needed for device specific functions 
 */

#ifndef _MCFG6000_H
#define _MCFG6000_H

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mCfg6000.h#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar6000/mCfg6000.h#1 $"

A_BOOL setChannelAr6000
(
     A_UINT32 devNum,
     A_UINT32 freq      // New channel
);

void initPowerAr6000
(
    A_UINT32 devNum,
    A_UINT32 freq,
    A_UINT32  override,
    A_UCHAR  *pwrSettings
);

#endif //_MCFG6000_H
