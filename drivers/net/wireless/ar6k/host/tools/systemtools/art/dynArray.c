/* dynArray.c - Generic dynamic array functions */
/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ifdef __ATH_DJGPPDOS__
 #define __int64	long long
 #define HANDLE long
 typedef unsigned long DWORD;
 #define Sleep	delay
 #include <bios.h>
#endif	// #ifdef __ATH_DJGPPDOS__

#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "wlantype.h"
#include "dynArray.h"
#include <stdio.h>
#if defined(LINUX) || defined(__linux__)
#include "linuxdrv.h"
#else
#include "ntdrv.h"
#endif


DYNAMIC_ARRAY *createArray
(
 A_UINT32 sizeElement
) 
{
	DYNAMIC_ARRAY *pArray;
	
	pArray = (DYNAMIC_ARRAY *)malloc(sizeof(DYNAMIC_ARRAY));

	if (!pArray) {
		return NULL;
	}

	pArray->numElements = 0;
	pArray->sizeElement = sizeElement;
	pArray->pElements = NULL;

	return pArray;
}


A_BOOL addElement
(
 DYNAMIC_ARRAY *pArray,
 void *pElement
)
{
	void *pNewArray;
	char *pTemp;

	//allocate new array to
	pNewArray = malloc((pArray->numElements + 1 ) * pArray->sizeElement);

	if(!pNewArray) {
		return 0;
	}

	//copy existing array elements first
	memcpy(pNewArray, pArray->pElements, pArray->numElements * pArray->sizeElement);

	//point to place where to copy the new element
	pTemp = (char *)pNewArray + (pArray->numElements * pArray->sizeElement);
	memcpy(pTemp, pElement, pArray->sizeElement);

	//update the pointers and free the old array
	A_FREE(pArray->pElements);
	pArray->pElements = pNewArray;
	pArray->numElements++;
	return 1;
}

void *getElement
(
 DYNAMIC_ARRAY *pArray,
 A_UINT32		index
)
{
	if(index >= pArray->numElements){
		return NULL;
	}

	return((void *)((A_UCHAR *)pArray->pElements + pArray->sizeElement * index));
}

void
freeArray
(
 DYNAMIC_ARRAY *pArray
)
{
	A_FREE(pArray->pElements);
	A_FREE(pArray);
	return;
}

