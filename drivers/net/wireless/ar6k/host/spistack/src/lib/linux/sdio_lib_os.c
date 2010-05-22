/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_function_os.c

@abstract: Linux implementation module for SDIO library

#notes: includes module load and unload functions
 
@notice: Copyright (c), 2004 Atheros Communications, Inc.


// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 4;
#include "../../include/ctsystem.h"
 
#include <linux/module.h>
#include <linux/init.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
#include <linux/kthread.h>
#endif

#include "../../include/sdio_busdriver.h"
#include "../../include/sdio_lib.h"

#define DESCRIPTION "SDIO Kernel Library"
#define AUTHOR "Atheros Communications, Inc."

/* debug print parameter */

CT_DECLARE_MODULE_PARAM_INTEGER(debuglevel);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");

/* proxies */
SDIO_STATUS SDLIB_IssueConfig(PSDDEVICE        pDevice,
                              SDCONFIG_COMMAND Command,
                              PVOID            pData,
                              INT              Length)
{
    return _SDLIB_IssueConfig(pDevice,Command,pData,Length);
}   
  
void SDLIB_PrintBuffer(PUCHAR pBuffer,INT Length,PTEXT pDescription)
{
    _SDLIB_PrintBuffer(pBuffer,Length,pDescription);   
} 


/* helper function launcher */
INT HelperLaunch(PVOID pContext)
{
    INT exit;
        /* call function */
    exit = ((POSKERNEL_HELPER)pContext)->pHelperFunc((POSKERNEL_HELPER)pContext);
    complete_and_exit(&((POSKERNEL_HELPER)pContext)->Completion, exit);    
    return exit;
}

/*
 * OSCreateHelper - create a worker kernel thread
*/
SDIO_STATUS SDLIB_OSCreateHelper(POSKERNEL_HELPER pHelper,
                           PHELPER_FUNCTION pFunction, 
                           PVOID            pContext)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    memset(pHelper,0,sizeof(OSKERNEL_HELPER));  
    
    do {
        pHelper->pContext = pContext;
        pHelper->pHelperFunc = pFunction;
        status = SignalInitialize(&pHelper->WakeSignal);
        if (!SDIO_SUCCESS(status)) {
            break; 
        }    
        init_completion(&pHelper->Completion);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        pHelper->pTask = kthread_create(HelperLaunch,
                                       (PVOID)pHelper,
                                       "SDIO Helper");
        if (NULL == pHelper->pTask) {
            status = SDIO_STATUS_NO_RESOURCES;
            break;  
        }
        wake_up_process(pHelper->pTask);
#else 
    /* 2.4 */       
        pHelper->pTask = kernel_thread(HelperLaunch,
                                       (PVOID)pHelper,
                                       (CLONE_FS | CLONE_FILES | SIGCHLD));
        if (pHelper->pTask < 0) {
            DBG_PRINT(SDDBG_TRACE, 
                ("SDIO BusDriver - OSCreateHelper, failed to create thread\n"));
        }        
#endif

    } while (FALSE);
    
    if (!SDIO_SUCCESS(status)) {
        SDLIB_OSDeleteHelper(pHelper);   
    }
    return status;
}
                           
/*
 * OSDeleteHelper - delete thread created with OSCreateHelper
*/
void SDLIB_OSDeleteHelper(POSKERNEL_HELPER pHelper)
{
 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (pHelper->pTask != NULL) {
#else 
    /* 2.4 */       
    if (pHelper->pTask >= 0) {
#endif        
        pHelper->ShutDown = TRUE;       
        SignalSet(&pHelper->WakeSignal); 
            /* wait for thread to exit */
        wait_for_completion(&pHelper->Completion);  
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        pHelper->pTask = NULL;
#else 
    /* 2.4 */       
        pHelper->pTask = 0;
#endif        
    }  
    
    SignalDelete(&pHelper->WakeSignal);
}
                          
/*
 * module init
*/
static int __init sdio_lib_init(void) {
    REL_PRINT(SDDBG_TRACE, ("SDIO Library load\n"));
    return 0;
}

/*
 * module cleanup
*/
static void __exit sdio_lib_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("SDIO Library unload\n"));
}

PSDMESSAGE_QUEUE SDLIB_CreateMessageQueue(INT MaxMessages, INT MaxMessageLength)
{
    return _CreateMessageQueue(MaxMessages,MaxMessageLength);
  
}
void SDLIB_DeleteMessageQueue(PSDMESSAGE_QUEUE pQueue)
{
    _DeleteMessageQueue(pQueue);
}

SDIO_STATUS SDLIB_PostMessage(PSDMESSAGE_QUEUE pQueue, PVOID pMessage, INT MessageLength)
{
    return _PostMessage(pQueue,pMessage,MessageLength);
}

SDIO_STATUS SDLIB_GetMessage(PSDMESSAGE_QUEUE pQueue, PVOID pData, INT *pBufferLength)
{
    return _GetMessage(pQueue,pData,pBufferLength);
}

MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_lib_init);
module_exit(sdio_lib_cleanup);
EXPORT_SYMBOL(SDLIB_IssueConfig);
EXPORT_SYMBOL(SDLIB_PrintBuffer);
EXPORT_SYMBOL(SDLIB_OSCreateHelper);
EXPORT_SYMBOL(SDLIB_OSDeleteHelper);
EXPORT_SYMBOL(SDLIB_CreateMessageQueue);
EXPORT_SYMBOL(SDLIB_DeleteMessageQueue);
EXPORT_SYMBOL(SDLIB_PostMessage);
EXPORT_SYMBOL(SDLIB_GetMessage);
