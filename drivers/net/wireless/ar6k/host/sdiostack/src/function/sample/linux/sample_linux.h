// Copyright (c) 2004, 2005 Atheros Communications Inc.
// 
//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sample_linux.h

@abstract: OS dependent include for SDIO Sample function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_SAMPLE_LINUX_H___
#define __SDIO_SAMPLE_LINUX_H___

#define SDIO_SAMPLE_FUNCTION_MAJOR 0

#define BLOCK_BUFFER_BYTES   512
#define BLOCK_BASIS_SIZE      64
#define BLOCK_BUFFERS          8
#define BYTE_BASIS_SIZE      128

#define BLOCK_BASIS_COUNT BLOCK_BUFFER_BYTES / BLOCK_BASIS_SIZE

typedef struct _SAMPLE_CONFIG {
    BOOL    CharRegistered;    /* char device was registered */
    UINT    Major;             /* device major number */    
    TEXT    DeviceName[20];    /* char device name */
    PUINT8  pBlockBuffer;      /* I/O output buffer */
    DMA_ADDRESS hBlockBuffer;  /* handle for data buffer */
    PUINT8  pVerifyBuffer;     /* buffer to verify I/O */
    UINT    BufferSize;        /* size of pBlockBuffer and pVerifyBuffer */
    SDDMA_DESCRIPTOR SGList;   /* scatter-gather list with one entry for pBlockBuffer */
    BOOL    Removing;          /* removing flag */
    
}SAMPLE_CONFIG, *PSAMPLE_CONFIG;

#endif /*__SDIO_SAMPLE_LINUX_H___*/

