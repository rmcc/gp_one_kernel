/*
 *  Copyright © 2002 Atheros Communications, Inc.,  All Rights Reserved.
 *
 *  mcfg(5)210.h - Type definitions needed for device specific functions 
 */

#ifndef	_MEEP211_H
#define	_MEEP211_H

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar5211/mEEP211.h#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/devlib/ar5211/mEEP211.h#1 $"




A_BOOL readCalData_gen2
(
 A_UINT32 devNum,
 MDK_PCDACS_ALL_MODES	*pEepromData
);


#endif //_MEEP211_H

