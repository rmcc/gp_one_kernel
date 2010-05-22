/*
 *  Copyright © 2003 Atheros Communications, Inc.,  All Rights Reserved.
 *
 *  mcfg(5)212.h - Type definitions needed for device specific functions 
 */

#ifndef	_MCFG212D_H
#define	_MCFG212D_H

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar5212/mCfg212d.h#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar5212/mCfg212d.h#1 $"

A_BOOL setChannelAr5212
(
 A_UINT32 devNum,
 A_UINT32 freq		// New channel
);

void initPowerAr5212
(
	A_UINT32 devNum,
	A_UINT32 freq,
	A_UINT32  override,
	A_UCHAR  *pwrSettings
);

#endif //_MCFG212D_H
