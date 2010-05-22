#ifdef __ATH_DJGPPDOS__
#include <unistd.h>
#ifndef EILSEQ  
    #define EILSEQ EIO
#endif	// EILSEQ

 #define __int64	long long
 #define HANDLE long
 typedef unsigned long DWORD;
 #define Sleep	delay
 #include <bios.h>
 #include <dir.h>
#endif	// #ifdef __ATH_DJGPPDOS__

#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "rate_constants.h"

//add rate extensions for venice
//                            6   9  12  18 24  36 48  54  
const A_UCHAR rateValues[] = {11, 15, 10, 14, 9, 13, 8, 12,    
//                       1L   2L   2S   5.5L 5.5S 11L  11S
                         0x1b,0x1a,0x1e,0x19,0x1d,0x18,0x1c,
//						 0.25 0.5 1  2  3
						 3,   7,  2, 6, 1
};

const A_UCHAR  rateCodes[] =  {6, 9, 12, 18, 24, 36, 48, 54, 0xb1, 0xb2, 0xd2, 0xb5, 0xd5, 0xbb, 0xdb, 
//                        XR0.25  XR0.5  XR1   XR2   XR3
							0xea, 0xeb,  0xe1, 0xe2, 0xe3};

const A_UINT16 numRateCodes = sizeof(rateCodes)/sizeof(A_UCHAR);


A_UINT32 rate2bin(A_UINT32 rateCode) {
	A_UINT32 rateBin = 0, i;
	//A_UCHAR rateCodes[]={6, 10};
	for(i = 0; i < NUM_RATES; i++) {
		if(rateCode == rateCodes[i]) {
			rateBin = i+1;
			break;
		}
	}
	return rateBin;
}

A_UINT32 descRate2bin(A_UINT32 descRateCode) {
	A_UINT32 rateBin = 0, i;
	//A_UCHAR rateValues[]={6, 10};
	for(i = 0; i < NUM_RATES; i++) {
		if(descRateCode == rateValues[i]) {
			rateBin = i+1;
			break;
		}
	}
	return rateBin;
}


