/* rssi_power.h - rssi to power calibration routines header definitions */

/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ifndef __INCrssipowerh
#define __INCrssipowerh
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NUM_COEFFS  4
#define NUM_POWER_AVERAGES 4

#define FAST_CAL_FILES_VERSION 1.0

#define NUM_POWER_PACKETS 32
#define LEN_POWER_PACKETS 32
#define IFS_IN_SLOTS	  1
#define NUM_INITIAL_PKTS_TO_SKIP 8

#define RSSI_OUT_SELECT 1  // 0, 1, 2 -> 1, .5, .25

#define GET_GU_PWR   0
#define CAL_PM_PWR   1
#define CAL_GU_PWR	 2

#define COEFF_GU_FILENAME "fastCal.tbl"

#define CAL_PM_FILENAME  "fastCal.ref"
#define CAL_GU_FILENAME  "fastCal.raw"
#define LOG_GU_FILENAME  "fastCal.log"

#define COEFF_FILETYPE  0  // golden coeficients from correlating the raw and ref data
#define REF_FILETYPE	1  // reference data from measurements with power meter
#define RAW_FILETYPE	2  // raw rssi measurement data
#define LOG_FILETYPE	3

// values in dBm
#define MAX_RECEIVE_POWER -35.0
#define MAX_TRANSMIT_POWER 20.0
#define MIN_TRANSMIT_POWER 0.0
#define MAX_RECEIVE_ROLLOFF 10.0

#define MAX_PEAK_TO_AVERAGE 10.0

#define MAX_ATTENUATOR_RANGE 81
#define E4416_SLEEP_INTERVAL 50


typedef struct _coeffDataPerChannel {
	double			*pCoeff;		
	A_UINT16		channelValue;	
} COEFF_DATA_PER_CHANNEL;

typedef struct _coeffDataStruct {
	A_UINT16			*pChannels;
	A_UINT16			numChannels;
	A_UINT16			numCoeffs;
	COEFF_DATA_PER_CHANNEL	*pDataPerChannel;		//ptr to array of info held per channel
} COEFF_DATA_STRUCT;

A_UINT16 getCalData(A_UINT32 devNum, A_UINT32 mode, A_UINT32 debug) ;
void getCalDataUsingGU(A_UINT32 devNum, A_UINT32 mode, A_UINT32 opcode, A_UINT32 debug) ;
void setupDutForPwrMeas(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 mode, A_UINT32 debug);
void retrievePwrData(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 channel, A_UINT32 mode, 
						A_UINT16 *pcdacs, A_UINT32 numPcd, double *retVals, 
						A_UINT32 debug);
void extractDoublesFromStr(char *srcStr, double *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug);
void extractDoublesFromScientificStr(char *srcStr, double *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug);
void extractFloatsFromStr(char *srcStr, float *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug);
void extractIntsFromStr(char *srcStr, A_UINT32 *retVals, A_UINT32 size, A_UINT32 *par1, A_UINT32 debug);

void setupGoldenForCal(A_UINT32 devNum, A_UINT32 debug);
void sendCalDataFromGU(A_UINT32 devNum, A_UINT32 mode, A_UINT32 opcode, A_UINT32 debug) ;
void setupGUForPwrMeas(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 mode, A_UINT32 debug);
void sendPwrData(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 channel, A_UINT32 mode, 
						A_UINT16 *pcdacs, A_UINT32 numPcd, double *retVals, 
						A_UINT32 debug);
void abandoned_sendPwrData(A_UINT32 devNum, A_UINT32 opcode, A_UINT32 channel, A_UINT32 mode, 
						A_UINT16 *pcdacs, A_UINT32 numPcd, double *retVals, 
						A_UINT32 debug);

void parseCoeffTable(char *filename, A_UINT32 debug);
void writeIntListToStr(char *deststr, A_UINT32 par1, A_UINT16 *list, A_UINT32 listsize);
void writeDoubleListToStr(char *deststr, A_UINT32 par1, double *list, A_UINT32 listsize);

double getCorrVal(A_UINT32 channel, double rssi, A_UINT32 debug);
void extractGoldenCalCoeffs( A_UINT32 mode, A_UINT32 debug );
void calibrateGoldenForFastCal_GU(A_UINT32 devNum, A_UINT32 mode, A_UINT32 debug);
void calibrateGoldenForFastCal_DUT(A_UINT32 devNum, A_UINT32 mode, A_UINT32 debug);
void fastCalMenu_GU(A_UINT32 devNum) ;
void fastCalMenu_DUT(A_UINT32 devNum) ;
void openRawRefLogFile(char *filename, A_UINT32 expectedType, A_UINT32 *numCh, A_UINT32 *numDataPoints, A_UINT32 debug);


#ifdef __cplusplus
}
#endif

#endif //__INCrssipowerh




