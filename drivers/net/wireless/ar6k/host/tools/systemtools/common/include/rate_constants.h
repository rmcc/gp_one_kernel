
#ifndef __INCrateconstantsh
#define __INCrateconstantsh

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


extern const A_UCHAR rateValues[];
extern const A_UCHAR rateCodes[];
extern const A_UINT16 numRateCodes;


extern A_UINT32 descRate2bin(A_UINT32 descRateCode);
extern A_UINT32 rate2bin(A_UINT32 rateCode);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

