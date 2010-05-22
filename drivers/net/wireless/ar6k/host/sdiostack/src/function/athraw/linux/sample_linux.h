/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sample_linux.h

@abstract: OS dependent include for SDIO Sample function driver

#notes: 
 
@notice: Copyright (c), 2005-2006 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_SAMPLE_LINUX_H___
#define __SDIO_SAMPLE_LINUX_H___

typedef struct _SAMPLE_CONFIG {
    OS_SEMAPHORE   RemoveSem;  /* remove lock semaphore */
    BOOL           Removing;   /* removing flag */
}SAMPLE_CONFIG, *PSAMPLE_CONFIG;

#endif /*__SDIO_SAMPLE_LINUX_H___*/

