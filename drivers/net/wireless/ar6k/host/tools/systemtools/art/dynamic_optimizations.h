/* dynamic_optimizations.h - the dynamic optimizations header definitions */

/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ifndef __INCdynOpth
#define __INCdynOpth

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_NUM_PARAMS_IN_GAIN_OPTIMIZATION_STEP 15
#define NUM_PARAMS_IN_GAIN_OPTIMIZATION_STEP 7
#define NUM_PARAMS_IN_DERBY2_GAIN_OPTIMIZATION_STEP 7
//#define NUM_PARAMS_IN_DERBY2_GAIN_OPTIMIZATION_STEP 9
#define DYN_ADJ_UP_MARGIN 15
#define DYN_ADJ_LO_MARGIN 20
#define NUM_CORNER_FIX_BITS 4

typedef struct _gainOptStep {

    A_INT32 paramVal[MAX_NUM_PARAMS_IN_GAIN_OPTIMIZATION_STEP];
    A_INT32 stepGain;
	A_CHAR	stepName[16];
} GAIN_OPTIMIZATION_STEP;

typedef struct _gainOptLadder {

    A_UINT32				numStepsInLadder;
	A_UINT32				numParamsInStep;
	A_UINT32				defaultStepNum;
	A_UINT32				currStepNum;
	A_UINT32				currGain;
	A_UINT32				gainFCorrection;
	A_UINT32				targetGain;
	A_UINT32				loTrig;
	A_UINT32				hiTrig;
	A_BOOL					active;

    A_CHAR					paramName[MAX_NUM_PARAMS_IN_GAIN_OPTIMIZATION_STEP][122];

    GAIN_OPTIMIZATION_STEP	optStep[15];
	GAIN_OPTIMIZATION_STEP	*currStep;

} GAIN_OPTIMIZATION_LADDER;



void  initializeGainLadder(GAIN_OPTIMIZATION_LADDER *ladder);
A_BOOL  invalidGainReadback(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum);
A_INT32 setupGainFRead(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 power);
A_INT32 readGainF(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 power);
A_INT32 readGainFInContTx(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 power);
A_BOOL  needGainAdjustment(GAIN_OPTIMIZATION_LADDER *ladder);
A_INT32 adjustGain(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 debug);
void programNewGain(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 debug);
void tweakGainF (GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum, A_UINT32 debug);

void  setGainLadderForMaxGain(GAIN_OPTIMIZATION_LADDER *ladder);
void  setGainLadderForMinGain(GAIN_OPTIMIZATION_LADDER *ladder);
void  loopIncrementFixedGain(GAIN_OPTIMIZATION_LADDER *ladder);
void  setGainLadderForIndex(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 index);

A_INT32 computeGainFCorrection(GAIN_OPTIMIZATION_LADDER *ladder, A_UINT32 devNum);

#ifdef __cplusplus
}
#endif

#endif


