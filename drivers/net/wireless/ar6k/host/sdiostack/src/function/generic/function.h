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

@abstract: OS independent include generic function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_FUNCTION_H___
#define __SDIO_FUNCTION_H___

#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/function_vxworks.h"
#endif /* VXWORKS */

#if defined(LINUX) || defined(__linux__)
#include "linux/function_linux.h"
#endif /* LINUX */

typedef struct _GENERIC_FUNCTION_INSTANCE {
    SDLIST         SDList;      /* link in the instance list */
    PSDDEVICE      pDevice;     /* bus driver's device we are supporting */
    GENERIC_CONFIG Config;      /* OS specific config  */
}GENERIC_FUNCTION_INSTANCE, *PGENERIC_FUNCTION_INSTANCE;

typedef struct _GENERIC_FUNCTION_CONTEXT {
    SDFUNCTION      Function;       /* function description for bus driver */ 
    OS_SEMAPHORE    InstanceSem;    /* instance lock */
    SDLIST          InstanceList;   /* list of instances */
}GENERIC_FUNCTION_CONTEXT, *PGENERIC_FUNCTION_CONTEXT;

SDIO_STATUS InitFunctionContext(PGENERIC_FUNCTION_CONTEXT pFuncContext);
void CleanupFunctionContext(PGENERIC_FUNCTION_CONTEXT pFuncContext);

PGENERIC_FUNCTION_INSTANCE CreateGenericInstance(PGENERIC_FUNCTION_CONTEXT pFuncContext,
                                                 PSDDEVICE                 pDevice);
void DeleteGenericInstance(PGENERIC_FUNCTION_CONTEXT pFuncContext,
                           PGENERIC_FUNCTION_INSTANCE pInstance); 
                           
SDIO_STATUS AddGenericInstance(PGENERIC_FUNCTION_CONTEXT  pFuncContext,
                               PGENERIC_FUNCTION_INSTANCE pInstance);       

PGENERIC_FUNCTION_INSTANCE FindGenericInstance(PGENERIC_FUNCTION_CONTEXT pFuncContext,
                                               PSDDEVICE                 pDevice);

BOOL TestAccept(PGENERIC_FUNCTION_CONTEXT pFuncContext, PSDDEVICE pDevice);

#endif /* __SDIO_FUNCTION_H___*/               
