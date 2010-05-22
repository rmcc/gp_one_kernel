#include <math.h>
#define NRANSI

#ifdef _WINDOWS
#include <windows.h>
#endif
#include <stdio.h>
#ifndef LINUX
#include <conio.h>
#endif
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include "wlantype.h"   /* typedefs for A_UINT16 etc.. */
#include "athreg.h"
#include "manlib.h"     /* The Manufacturing Library */
#include "MLIBif.h"     /* Manufacturing Library low level driver support functions */
#ifdef JUNGO
#include "mld.h"        /* Low level driver information */
#endif
#include "common_hw.h"        /* Low level driver information */
#include "manlibInst.h" /* The Manufacturing Library Instrument Library extension */
#include "mEeprom.h"    /* Definitions for the data structure */
#include "dynamic_optimizations.h"
#include "maui_cal.h"   /* Definitions for the Calibration Library */
#include "rssi_power.h" /* Definitions for the rssi to power cal */
#include "mathRoutines.h" 
//#include "test.h"
#include "parse.h"
#include "dk_cmds.h"
#include "art_if.h"

// #include "nr.h"
#include "nrutil.h"
#include "mathRoutines.h"


void curveFit(float x[], float y[], int nDataPoints, int nCoeffs, float coeffs[]) 
{
	int i;
	float chisq,*sig,*w,**cvm,**u,**v, *newX, *newY, *newCoeffs;

	newX=vector(1,nDataPoints);
	newY=vector(1,nDataPoints);
	newCoeffs=vector(1,nCoeffs);
	
	sig=vector(1,nDataPoints);
	w=vector(1,nCoeffs);
	cvm=matrix(1,nCoeffs,1,nCoeffs);
	u=matrix(1,nDataPoints,1,nCoeffs);
	v=matrix(1,nCoeffs,1,nCoeffs);
	for (i=1;i<=nDataPoints;i++) {
		newX[i] = x[i-1];
		newY[i] = y[i-1];
	//	uiPrintf("%d: x=%f, y=%f\n", i, newX[i], newY[i]);
		sig[i]=1.0;
	}

	svdfit(newX, newY, sig, nDataPoints, newCoeffs, nCoeffs, u, v, w, &(chisq), fpoly);
	svdvar(v,nCoeffs,w,cvm);
	uiPrintf("\npolynomial fit:\n\n");
	for (i=1;i<=nCoeffs;i++)
		uiPrintf("%12.6e %s %10.6f\n",newCoeffs[i],"  +-",sqrt(cvm[i][i]));
	uiPrintf("\nChi-squared %12.6f\n",chisq);
	for (i=1; i<=nCoeffs; i++)
		coeffs[i-1] = newCoeffs[i];
}


void svdvar(float **v, int ma, float w[], float **cvm)
{
	int k,j,i;
	float sum,*wti;

	wti=vector(1,ma);
	for (i=1;i<=ma;i++) {
		wti[i]=0.0;
		if (w[i]) wti[i]=(float)1.0/(w[i]*w[i]);
	}
	for (i=1;i<=ma;i++) {
		for (j=1;j<=i;j++) {
			for (sum=0.0,k=1;k<=ma;k++) sum += v[i][k]*v[j][k]*wti[k];
			cvm[j][i]=cvm[i][j]=sum;
		}
	}
	free_vector(wti,1,ma);
}

void svdfit(float x[], float y[], float sig[], int ndata, float a[], int ma,
	float **u, float **v, float w[], float *chisq,
	void (*funcs)(float, float [], int))
{
#ifndef LINUX
	void svbksb(float **u, float w[], float **v, int m, int n, float b[],
		float x[]);
	void svdcmp(float **a, int m, int n, float w[], float **v);
#endif
	int j,i;
	float wmax,tmp,thresh,sum,*b,*afunc;

	b=vector(1,ndata);
	afunc=vector(1,ma);
	for (i=1;i<=ndata;i++) {
		(*funcs)(x[i],afunc,ma);
		tmp=(float)1.0/sig[i];
		for (j=1;j<=ma;j++) u[i][j]=afunc[j]*tmp;
		b[i]=y[i]*tmp;
	}
	svdcmp(u,ndata,ma,w,v);
	wmax=0.0;
	for (j=1;j<=ma;j++)
		if (w[j] > wmax) wmax=w[j];
	thresh=(float)TOL*wmax;
	for (j=1;j<=ma;j++)
	{
//		uiPrintf("w[%d]=%f -> ", j, w[j]);
		if (w[j] < thresh) w[j]=0.0;
//		uiPrintf("w[%d]=%f\n", j, w[j]);
	}
	svbksb(u,w,v,ndata,ma,b,a);
	*chisq=0.0;
	for (i=1;i<=ndata;i++) {
		(*funcs)(x[i],afunc,ma);
		for (sum=0.0,j=1;j<=ma;j++) {
			sum += a[j]*afunc[j];
			//uiPrintf("a[%d]:%e, afunc[j]:%e, sum:%e\n", j, a[j],afunc[j], sum);
		}
		*chisq += (tmp=(y[i]-sum)/sig[i],tmp*tmp);
		//uiPrintf("tmp=%f, y[%d]:%f, sig[i]:%f, sum:%f, chisq:%f\n", tmp, i, y[i],sig[i],sum, *chisq);
	}
	free_vector(afunc,1,ma);
	free_vector(b,1,ndata);
}

void svdcmp(float **a, int m, int n, float w[], float **v)
{
//	float pythag(float a, float b);
	int flag,i,its,j,jj,k,l,nm;
	float anorm,c,f,g,h,s,scale,x,y,z,*rv1;

	rv1=vector(1,n);
	g=scale=anorm=0.0;
	for (i=1;i<=n;i++) {
		l=i+1;
		rv1[i]=scale*g;
		g=s=scale=0.0;
		if (i <= m) {
			for (k=i;k<=m;k++) scale += (float)fabs(a[k][i]);
			if (scale) {
				for (k=i;k<=m;k++) {
					a[k][i] /= scale;
					s += a[k][i]*a[k][i];
				}
				f=a[i][i];
				g = (float)(-SIGN(sqrt(s),f));
				h=f*g-s;
				a[i][i]=f-g;
				for (j=l;j<=n;j++) {
					for (s=0.0,k=i;k<=m;k++) s += a[k][i]*a[k][j];
					f=s/h;
					for (k=i;k<=m;k++) a[k][j] += f*a[k][i];
				}
				for (k=i;k<=m;k++) a[k][i] *= scale;
			}
		}
		w[i]=scale *g;
		g=s=scale=0.0;
		if (i <= m && i != n) {
			for (k=l;k<=n;k++) scale += (float)fabs(a[i][k]);
			if (scale) {
				for (k=l;k<=n;k++) {
					a[i][k] /= scale;
					s += a[i][k]*a[i][k];
				}
				f=a[i][l];
				g = (float)(-SIGN(sqrt(s),f));
				h=f*g-s;
				a[i][l]=f-g;
				for (k=l;k<=n;k++) rv1[k]=a[i][k]/h;
				for (j=l;j<=m;j++) {
					for (s=0.0,k=l;k<=n;k++) s += a[j][k]*a[i][k];
					for (k=l;k<=n;k++) a[j][k] += s*rv1[k];
				}
				for (k=l;k<=n;k++) a[i][k] *= scale;
			}
		}
		anorm=(float)FMAX(anorm,((float)fabs(w[i])+(float)fabs(rv1[i])));
	}
	for (i=n;i>=1;i--) {
		if (i < n) {
			if (g) {
				for (j=l;j<=n;j++)
					v[j][i]=(a[i][j]/a[i][l])/g;
				for (j=l;j<=n;j++) {
					for (s=0.0,k=l;k<=n;k++) s += a[i][k]*v[k][j];
					for (k=l;k<=n;k++) v[k][j] += s*v[k][i];
				}
			}
			for (j=l;j<=n;j++) v[i][j]=v[j][i]=0.0;
		}
		v[i][i]=1.0;
		g=rv1[i];
		l=i;
	}
	for (i=IMIN(m,n);i>=1;i--) {
		l=i+1;
		g=w[i];
		for (j=l;j<=n;j++) a[i][j]=0.0;
		if (g) {
			g=(float)1.0/g;
			for (j=l;j<=n;j++) {
				for (s=0.0,k=l;k<=m;k++) s += a[k][i]*a[k][j];
				f=(s/a[i][i])*g;
				for (k=i;k<=m;k++) a[k][j] += f*a[k][i];
			}
			for (j=i;j<=m;j++) a[j][i] *= g;
		} else for (j=i;j<=m;j++) a[j][i]=0.0;
		++a[i][i];
	}
	for (k=n;k>=1;k--) {
		for (its=1;its<=30;its++) {
			flag=1;
			for (l=k;l>=1;l--) {
				nm=l-1;
				if ((float)(fabs(rv1[l])+anorm) == anorm) {
					flag=0;
					break;
				}
				if ((float)(fabs(w[nm])+anorm) == anorm) break;
			}
			if (flag) {
				c=0.0;
				s=1.0;
				for (i=l;i<=k;i++) {
					f=s*rv1[i];
					rv1[i]=c*rv1[i];
					if ((float)(fabs(f)+anorm) == anorm) break;
					g=w[i];
					h=pythag(f,g);
					w[i]=h;
					h=(float)1.0/h;
					c=g*h;
					s = -f*h;
					for (j=1;j<=m;j++) {
						y=a[j][nm];
						z=a[j][i];
						a[j][nm]=y*c+z*s;
						a[j][i]=z*c-y*s;
					}
				}
			}
			z=w[k];
			if (l == k) {
				if (z < 0.0) {
					w[k] = -z;
					for (j=1;j<=n;j++) v[j][k] = -v[j][k];
				}
				break;
			}
			if (its == 30) nrerror("no convergence in 30 svdcmp iterations");
			x=w[l];
			nm=k-1;
			y=w[nm];
			g=rv1[nm];
			h=rv1[k];
			f=(float)((y-z)*(y+z)+(g-h)*(g+h))/((float)2.0*h*y);
			g=pythag(f,1.0);
			f=(float)((x-z)*(x+z)+h*((y/(f+SIGN(g,f)))-h))/x;
			c=s=1.0;
			for (j=l;j<=nm;j++) {
				i=j+1;
				g=rv1[i];
				y=w[i];
				h=s*g;
				g=c*g;
				z=pythag(f,h);
				rv1[j]=z;
				c=f/z;
				s=h/z;
				f=x*c+g*s;
				g = g*c-x*s;
				h=y*s;
				y *= c;
				for (jj=1;jj<=n;jj++) {
					x=v[jj][j];
					z=v[jj][i];
					v[jj][j]=x*c+z*s;
					v[jj][i]=z*c-x*s;
				}
				z=pythag(f,h);
				w[j]=z;
				if (z) {
					z=(float)1.0/z;
					c=f*z;
					s=h*z;
				}
				f=c*g+s*y;
				x=c*y-s*g;
				for (jj=1;jj<=m;jj++) {
					y=a[jj][j];
					z=a[jj][i];
					a[jj][j]=y*c+z*s;
					a[jj][i]=z*c-y*s;
				}
			}
			rv1[l]=0.0;
			rv1[k]=f;
			w[k]=x;
		}
	}
	free_vector(rv1,1,n);
}


void svbksb(float **u, float w[], float **v, int m, int n, float b[], float x[])
{
	int jj,j,i;
	float s,*tmp;

	tmp=vector(1,n);
	for (j=1;j<=n;j++) {
		s=0.0;
		if (w[j]) {
			for (i=1;i<=m;i++) s += u[i][j]*b[i];
			s /= w[j];
		}
		tmp[j]=s;
	}
	for (j=1;j<=n;j++) {
		s=0.0;
		for (jj=1;jj<=n;jj++) s += v[j][jj]*tmp[jj];
		x[j]=s;
	}
	free_vector(tmp,1,n);
}

void fpoly(float x, float p[], int np)
{
	int j;

	p[1]=1.0;
//	uiPrintf("np:%d, p[1] = %f, ", np, p[1]);
	for (j=2;j<=np;j++) {
		p[j]=p[j-1]*x;
//		uiPrintf("p[%d] = %f, ", j, p[j]);
	}
//	uiPrintf("\n");
}

float pythag(float a, float b)
{
	float absa,absb;
	absa=(float)fabs(a);
	absb=(float)fabs(b);
	if (absa > absb) return absa*(float)sqrt(1.0+SQR(absb/absa));
	else return (float)((absb == 0.) ? 0.0 : absb*sqrt(1.0+SQR(absa/absb)));
}

#undef NRANSI
