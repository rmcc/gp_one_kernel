/* mathRoutines.h - PM to rssi curve-fit routines header definitions */

/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ifndef __INCmathRoutinesh
#define __INCmathRoutinesh
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


//#define NPT 100
#define SPREAD 0.02
//#define NPOL 5

#define TOL 1.0e-7       // max ill-conditioned w-Matrix. max permitted wMax/wMin ratio.



// int fit_example(void);
void curveFit(float x[], float y[], int nDataPoints, int nCoeffs, float coeffs[]) ;
void svdvar(float **v, int ma, float w[], float **cvm);
void svdfit(float x[], float y[], float sig[], int ndata, float a[], int ma,
	float **u, float **v, float w[], float *chisq,
	void (*funcs)(float, float [], int));
void svdcmp(float **a, int m, int n, float w[], float **v);
void svbksb(float **u, float w[], float **v, int m, int n, float b[], float x[]);
void fpoly(float x, float p[], int np);
float pythag(float a, float b);




#ifdef __cplusplus
}
#endif

#endif //__INCmathRoutinesh



