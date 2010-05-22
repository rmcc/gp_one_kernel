// Copyright (c) 2004 Atheros Communications Inc.
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
@file: function.h

@abstract: OS dependent include generic function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_FUNCTION_LINUX_H___
#define __SDIO_FUNCTION_LINUX_H___


#define SDIO_FUNCTION_BASE "sdiofn"
#define SDIO_FUNCTION_MAX_DEVICES 1
#define SDIO_FUNCTION_MAJOR 0

typedef struct _GENERIC_CONFIG {
    BOOL CharRegistered;    /* char device was registered */
    UINT Major;             /* device major number */        
}GENERIC_CONFIG, *PGENERIC_CONFIG;

#endif /*__SDIO_FUNCTION_LINUX_H___*/

