#ifdef __ATH_DJGPPDOS__
#define __int64	long long
typedef unsigned long DWORD;
#define Sleep	delay
#endif	// #ifdef __ATH_DJGPPDOS__

#include <errno.h>
#include <stdio.h>
#ifndef VXWORKS
#include <malloc.h>
#endif
#include "wlantype.h"

#include "mCfg900d.h"

#ifdef Linux
#include "../athreg.h"
#include "../manlib.h"
#include "../mEeprom.h"
#include "../mConfig.h"
#else
#include "..\athreg.h"
#include "..\manlib.h"
#include "..\mEeprom.h"
#include "..\mConfig.h"
#endif

#include <string.h>

/**************************************************************************
 * setChannelAr1900 - Perform the algorithm to change the channel
 *					  for AR5002 adapters
 *
 */
A_BOOL setChannelAr1900
(
 A_UINT32 devNum,
 A_UINT32 freq		// New channel
)
{

	return(1);
}

/**************************************************************************
* initPowerAr1900 - Set the power for the AR5002 chipsets
*
*/
void initPowerAr1900
(
	A_UINT32 devNum,
	A_UINT32 freq,
	A_UINT32  override,
	A_UCHAR  *pwrSettings
)
{

}

void setSinglePowerAr1900
(
 A_UINT32 devNum, 
 A_UCHAR pcdac
)
{

}


