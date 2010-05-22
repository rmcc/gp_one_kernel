/*
 *  Copyright © 2003 Atheros Communications, Inc.,  All Rights Reserved.
 *
 *  mcfg(1)900.h - Type definitions needed for device specific functions 
 */

#ifndef	_MCFG212D_H
#define	_MCFG212D_H


A_BOOL setChannelAr1900
(
 A_UINT32 devNum,
 A_UINT32 freq		// New channel
);

void initPowerAr1900
(
	A_UINT32 devNum,
	A_UINT32 freq,
	A_UINT32  override,
	A_UCHAR  *pwrSettings
);

void setSinglePowerAr1900
(
 A_UINT32 devNum, 
 A_UCHAR pcdac
);

#endif //_MCFG212D_H
