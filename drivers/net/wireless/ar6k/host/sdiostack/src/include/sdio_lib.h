// Copyright (c) 2004-2006 Atheros Communications Inc.
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
@file: sdio_lib.h

@abstract: SDIO Library include

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_LIB_H___
#define __SDIO_LIB_H___

#ifdef UNDER_CE
#include "wince\sdio_lib_wince.h"
#endif /* WINCE */

#define CMD52_DO_READ  FALSE
#define CMD52_DO_WRITE TRUE

    /* read/write macros to any function */
#define Cmd52WriteByteFunc(pDev,Func,Address,pValue) \
                SDLIB_IssueCMD52((pDev),(Func),(Address),(pValue),1,CMD52_DO_WRITE)
#define Cmd52ReadByteFunc(pDev,Func,Address,pValue) \
                SDLIB_IssueCMD52((pDev),(Func),(Address),pValue,1,CMD52_DO_READ)
#define Cmd52ReadMultipleFunc(pDev,Func, Address, pBuf,length) \
                SDLIB_IssueCMD52((pDev),(Func),(Address),(pBuf),(length),CMD52_DO_READ)
                                  
   /* macros to access common registers */              
#define Cmd52WriteByteCommon(pDev, Address, pValue) \
                Cmd52WriteByteFunc((pDev),0,(Address),(pValue))
#define Cmd52ReadByteCommon(pDev, Address, pValue) \
                Cmd52ReadByteFunc((pDev),0,(Address),(pValue))
#define Cmd52ReadMultipleCommon(pDev, Address, pBuf,length) \
                Cmd52ReadMultipleFunc((pDev),0,(Address),(pBuf),(length)) 

#define SDLIB_SetupCMD52RequestAsync(f,a,w,wd,pR)   \
{                                                   \
    SDLIB_SetupCMD52Request((f),(a),(w),(wd),(pR)); \
    (pR)->Flags |= SDREQ_FLAGS_TRANS_ASYNC;         \
}
        
    /* a message block */
typedef struct _SDMESSAGE_BLOCK {
    SDLIST  SDList;                   /* list entry */
    INT     MessageLength;            /* number of bytes in this message */
    UINT8   MessageStart[1];          /* message start */
}SDMESSAGE_BLOCK, *PSDMESSAGE_BLOCK;

    /* message queue */
typedef struct _SDMESSAGE_QUEUE {
    SDLIST          MessageList;        /* message list */
    OS_CRITICALSECTION MessageCritSection; /* message semaphore */
    SDLIST          FreeMessageList;    /* free message list */
    INT             MaxMessageLength;   /* max message block length */
}SDMESSAGE_QUEUE, *PSDMESSAGE_QUEUE;
          
/* internal library prototypes that can be proxied */
SDIO_STATUS _SDLIB_IssueCMD52(PSDDEVICE     pDevice,
                        UINT8         FuncNo,
                        UINT32        Address,
                        PUINT8        pData,
                        INT           ByteCount,
                        BOOL          Write);                        
SDIO_STATUS _SDLIB_FindTuple(PSDDEVICE  pDevice,
                             UINT8      Tuple,
                             UINT32     *pTupleScanAddress,
                             PUINT8     pBuffer,
                             UINT8      *pLength);                               
SDIO_STATUS _SDLIB_IssueConfig(PSDDEVICE        pDevice,
                               SDCONFIG_COMMAND Command,
                               PVOID            pData,
                               INT              Length);                                  
void _SDLIB_PrintBuffer(PUCHAR pBuffer, INT Length,PTEXT pDescription);  
void _SDLIB_SetupCMD52Request(UINT8         FuncNo,
                              UINT32        Address,
                              BOOL          Write,
                              UINT8         WriteData,                                    
                              PSDREQUEST    pRequest);                             
SDIO_STATUS _SDLIB_SetFunctionBlockSize(PSDDEVICE        pDevice,
                                        UINT16           BlockSize);
                                        
SDIO_STATUS _SDLIB_GetDefaultOpCurrent(PSDDEVICE  pDevice, 
                                       SD_SLOT_CURRENT *pOpCurrent); 
PSDMESSAGE_QUEUE _CreateMessageQueue(INT MaxMessages, INT MaxMessageLength);
void _DeleteMessageQueue(PSDMESSAGE_QUEUE pQueue);
SDIO_STATUS _PostMessage(PSDMESSAGE_QUEUE pQueue, PVOID pMessage, INT MessageLength);
SDIO_STATUS _GetMessage(PSDMESSAGE_QUEUE pQueue, PVOID pData, INT *pBufferLength);

#ifdef CTSYSTEM_NO_FUNCTION_PROXIES
    /* OS port requires no proxy functions, use methods directly from the library */
#define SDLIB_IssueCMD52        _SDLIB_IssueCMD52
#define SDLIB_SetupCMD52Request _SDLIB_SetupCMD52Request
#define SDLIB_FindTuple         _SDLIB_FindTuple
#define SDLIB_IssueConfig       _SDLIB_IssueConfig
#define SDLIB_SetFunctionBlockSize  _SDLIB_SetFunctionBlockSize
#define SDLIB_GetDefaultOpCurrent   _SDLIB_GetDefaultOpCurrent
#define SDLIB_CreateMessageQueue    _CreateMessageQueue
#define SDLIB_DeleteMessageQueue    _DeleteMessageQueue
#define SDLIB_PostMessage           _PostMessage
#define SDLIB_GetMessage            _GetMessage
#define SDLIB_PrintBuffer           _SDLIB_PrintBuffer
#else

/* proxied versions */
SDIO_STATUS SDLIB_IssueCMD52(PSDDEVICE     pDevice,
                             UINT8         FuncNo,
                             UINT32        Address,
                             PUINT8        pData,
                             INT           ByteCount,
                             BOOL          Write); 
                                               
void SDLIB_SetupCMD52Request(UINT8         FuncNo,
                             UINT32        Address,
                             BOOL          Write,
                             UINT8         WriteData,                                    
                             PSDREQUEST    pRequest);
                             
SDIO_STATUS SDLIB_FindTuple(PSDDEVICE  pDevice,
                        UINT8      Tuple,
                        UINT32     *pTupleScanAddress,
                        PUINT8     pBuffer,
                        UINT8      *pLength);   
                        
SDIO_STATUS SDLIB_IssueConfig(PSDDEVICE        pDevice,
                              SDCONFIG_COMMAND Command,
                              PVOID            pData,
                              INT              Length); 
                                   
SDIO_STATUS SDLIB_SetFunctionBlockSize(PSDDEVICE        pDevice,
                                       UINT16           BlockSize);
                                       
void SDLIB_PrintBuffer(PUCHAR pBuffer, INT Length,PTEXT pDescription);                  

SDIO_STATUS SDLIB_GetDefaultOpCurrent(PSDDEVICE  pDevice, SD_SLOT_CURRENT *pOpCurrent);

PSDMESSAGE_QUEUE SDLIB_CreateMessageQueue(INT MaxMessages, INT MaxMessageLength);

void SDLIB_DeleteMessageQueue(PSDMESSAGE_QUEUE pQueue);

SDIO_STATUS SDLIB_PostMessage(PSDMESSAGE_QUEUE pQueue, PVOID pMessage, INT MessageLength);

SDIO_STATUS SDLIB_GetMessage(PSDMESSAGE_QUEUE pQueue, PVOID pData, INT *pBufferLength);
#endif /* CTSYSTEM_NO_FUNCTION_PROXIES */


SDIO_STATUS SDLIB_OSCreateHelper(POSKERNEL_HELPER pHelper,
                           PHELPER_FUNCTION pFunction, 
                           PVOID            pContext);                            

void SDLIB_OSDeleteHelper(POSKERNEL_HELPER pHelper);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: Check message queue is empty
  
  @function name: SDLIB_IsQueueEmpty
  @prototype: BOOL SDLIB_IsQueueEmpty(PSDMESSAGE_QUEUE pQueue)
  @category: Support_Reference
  
  @input: pQueue - message queue to check

  @return: TRUE if empty else false
            
  @see also: SDLIB_CreateMessageQueue 
         
  @example: Check message queue :
              if (SDLIB_IsQueueEmpty(pInstance->pQueue)) {
                   .. message queue is empty
              }
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static INLINE BOOL SDLIB_IsQueueEmpty(PSDMESSAGE_QUEUE pQueue) {
    return SDLIST_IS_EMPTY(&pQueue->MessageList);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: Issue an I/O abort request
  
  @function name: SDLIB_IssueIOAbort
  @prototype: SDIO_STATUS SDLIB_IssueIOAbort(PSDDEVICE pDevice)
  @category: PD_Reference
  
  @input: pDevice - the device that is the target of this request

  @return: SDIO_STATUS
            
  @notes: This procedure can be called to issue an I/O abort request to an I/O function.
          This procedure cannot be used to abort a data (block) transfer already in progress.
          It is intended to be used when a data (block) transfer completes with an error and only if 
          the I/O function requires an abort action.  Some I/O functions may automatically
          recover from such failures and not require this action. This function issues
          the abort command synchronously and can potentially block.
          If an async request is required, you must allocate a request and use 
          SDLIB_SetupIOAbortAsync() to prepare the request.
          
  @example: Issuing I/O Abort synchronously :
              .. check status from last block operation:
              if (status == SDIO_STATUS_BUS_READ_TIMEOUT) {
                   .. on failure, issue I/O abort
                   status2 = SDLIB_IssueIOAbort(pDevice);
              }
            Issuing I/O Abort asynchronously:
                ... allocate a request
                ... setup the request:
                 SDLIB_SetupIOAbortAsync(pDevice,pReq);
                 pReq->pCompletion = myIOAbortCompletion;
                 pReq->pCompleteContext = pDevice; 
                 status = SDDEVICE_CALL_REQUEST_FUNC(pDevice,pReq);
   
   @see also: SDLIB_SetupIOAbortAsync              
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static INLINE SDIO_STATUS SDLIB_IssueIOAbort(PSDDEVICE pDevice) {
    UINT8 value = SDDEVICE_GET_SDIO_FUNCNO(pDevice);
    return Cmd52WriteByteCommon(pDevice,0x06,&value);
}   

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @function: Setup an I/O abort request for async operation
  
  @function name: SDLIB_SetupIOAbortAsync
  @prototype: SDLIB_SetupIOAbortAsync(PSDDEVICE pDevice, PSDREQUEST pRequest)
  @category: PD_Reference
  
  @input: pDevice - the device that is the target of this request
          pRequest - the request to set up
            
  @see also: SDLIB_IssueIOAbort   
                
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define SDLIB_SetupIOAbortAsync(pDevice, pReq) \
        SDLIB_SetupCMD52RequestAsync(0,0x06,TRUE,SDDEVICE_GET_SDIO_FUNCNO(pDevice),(pReq))
               
               
#endif /* __SDIO_LIB_H___*/
