/*
 *  Copyright © 2002 Atheros Communications, Inc.,  All Rights Reserved.
 *
 *  mcfg(5)210.h - Type definitions needed for device specific functions 
 */

#ifndef	_MEEP210_H
#define	_MEEP210_H

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar5210/mEEP210.h#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar5210/mEEP210.h#1 $"


A_BOOL read5210eepData
(
 A_UINT32	devNum
);

A_UINT32 check5210Prom
(
 A_UINT32 devNum,
 A_UINT32 enablePrint
);



#endif //_MEEP210_H

