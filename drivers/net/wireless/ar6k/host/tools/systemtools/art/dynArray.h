/* dynArray.h - Generic dynamic array function definitions */
/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ifndef	__INCdynArrayh
#define	__INCdynArrayh

typedef struct dynamicArray {
	A_UINT32 numElements;
	A_UINT32 sizeElement;
	void *pElements;
} DYNAMIC_ARRAY;

DYNAMIC_ARRAY *createArray
(
 A_UINT32 sizeElement
); 

A_BOOL addElement
(
 DYNAMIC_ARRAY *pArray,
 void *pElement
);

void *getElement
(
 DYNAMIC_ARRAY *pArray,
 A_UINT32		index
);

void
freeArray
(
 DYNAMIC_ARRAY *pArray
);


#endif //__INCdynArrayh

