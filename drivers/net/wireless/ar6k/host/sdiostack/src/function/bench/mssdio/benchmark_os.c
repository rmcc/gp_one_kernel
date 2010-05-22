/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: benchmark_os.c

@abstract: Windows CE implementation for the SDIO Benchmark Function driver

#notes:
 
@notice: Copyright (c), 2006 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 7;
#include <ctsystem.h>
#include <SDCardDDK.h>
#include "../../common/benchmark.h"
#include "benchmark_ms.h"


#define SDIO_BENCH_REG_PATH  (SDIO_STACK_REG_BASE TEXT("\\SDIOBench"))

#ifdef DEBUG

// Need this for sdcard modules
DBGPARAM dpCurSettings = {
    _T("BENCHMARK"), 
    {
        _T(""), _T(""), _T(""), _T(""),
        _T(""), _T(""), _T(""), _T(""),
        _T(""), _T(""), _T(""), _T(""),
        _T(""), _T(""), _T(""), _T("") 
    },
    0x0 
};

#endif


SDIO_STATUS GetRegistryKeyDWORD(HKEY   hKey, 
                                WCHAR  *pKeyPath,
                                WCHAR  *pValueName, 
                                PDWORD pValue);

#define GetDefaultDWORDSetting(pKey,pDword)         \
    GetRegistryKeyDWORD(SDIO_STACK_BASE_HKEY,  \
                              SDIO_BENCH_REG_PATH, \
                              (pKey),                \
                              (pDword))


VOID GetBenchMarkParameters(PVOID pContext, PBM_TEST_PARAMS pParams)
{
    
    PBENCH_MARK_INSTANCE pInstance = (PBENCH_MARK_INSTANCE)pContext;
    
    pParams->pTestBuffer = pInstance->pTestBuffer;
    pParams->BufferSize = pInstance->BufferSize;
    
    GetDefaultDWORDSetting(TEXT("Cmd52FixedAddress"), (DWORD *)&pParams->Cmd52FixedAddress);
}


SD_API_STATUS IssueCMD52ASync(PBENCH_MARK_INSTANCE pInstance);


VOID AsyncCMD52Callback (SD_DEVICE_HANDLE hDev,
                         PSD_BUS_REQUEST  pRequest,
                         PVOID            pContext,
                         DWORD            RequestParam)
{
    PBENCH_MARK_INSTANCE pInstance = (PBENCH_MARK_INSTANCE)pContext;   
    SD_API_STATUS        apiStatus;
    BOOL                 done = FALSE;
    
    
    apiStatus = pRequest->Status;
    
    do {
        
        if (!SD_API_SUCCESS(apiStatus)) {
            pInstance->LastStatus = apiStatus;
            done = TRUE;
            break;    
        }
        
        if (!(pInstance->TestFlags & BM_FLAGS_WRITE)) {
            *pInstance->pCurrBuffer = 
            pRequest->CommandResponse.ResponseBuffer[SD_IO_R5_RESPONSE_DATA_BYTE_OFFSET];  
        }
            
        pInstance->CurrentCount--;
               
        if (0 == pInstance->CurrentCount) {
            done = TRUE;
            break;    
        }        
        
        pInstance->pCurrBuffer++;
        
        if (!(pInstance->TestFlags & BM_FLAGS_CMD52_FIXED_ADDR)) {
            pInstance->CurrentAddress++;    
        }
           
        apiStatus = IssueCMD52ASync(pInstance);
        
        if (!SD_API_SUCCESS(apiStatus)) {
            done = TRUE;    
            pInstance->LastStatus = apiStatus;
        }
        
    } while (FALSE); 
    
    SDFreeBusRequest(pRequest);
    
    if (done) {
        SetEvent(pInstance->hIOComplete);   
    }
}
                                          

SD_API_STATUS IssueCMD52ASync(PBENCH_MARK_INSTANCE pInstance)
{
    DWORD   argument;
    
    argument = BUILD_IO_RW_DIRECT_ARG(((pInstance->TestFlags & BM_FLAGS_WRITE) ?  SD_IO_OP_WRITE: SD_IO_OP_READ),      
                                      SD_IO_RW_NORMAL,
                                      pInstance->FuncNo,
                                      (pInstance->CurrentAddress),
                                      (pInstance->TestFlags & BM_FLAGS_WRITE) ? *pInstance->pCurrBuffer  : 0);
                                          
    return SDBusRequest(pInstance->hDevice,
                        SD_IO_RW_DIRECT,
                        argument,
                        SD_COMMAND,
                        ResponseR5,
                        0,
                        0,
                        NULL,
                        AsyncCMD52Callback,
                        0,
                        &pInstance->pCurrentRequest,
                        0);   
    
    
}
                                     
SDIO_STATUS Cmd52Transfer(PVOID      pContext,
                          UINT32     Address,
                          PUINT8     pBuffer,
                          UINT32     Count,
                          UINT32     Flags)
{
    SD_API_STATUS apiStatus;
    DWORD         argument;
    SD_COMMAND_RESPONSE response;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PBENCH_MARK_INSTANCE pInstance = (PBENCH_MARK_INSTANCE)pContext;
    
    
    do { 
               
        if (Flags & BM_FLAGS_ASYNC) {
            pInstance->LastStatus = SD_API_STATUS_SUCCESS;
            pInstance->pCurrBuffer = pBuffer;
            pInstance->TestFlags = Flags;
            pInstance->CurrentCount = Count;
            pInstance->CurrentAddress = Address;
            
            apiStatus = IssueCMD52ASync(pInstance);
            
            if (!SD_API_SUCCESS(apiStatus)) {
                status = SDIO_STATUS_ERROR;
                break;    
            }
                        
            WaitForSingleObject(pInstance->hIOComplete, INFINITE);
            
            if (!SD_API_SUCCESS(pInstance->LastStatus)) {
                status = SDIO_STATUS_ERROR;
            }
            
                /* done */
            break;
        }
            
        
        /* all sync stuff below this line */
            
        if (!(Flags & BM_FLAGS_CMD52_FIXED_ADDR)){         
        
            if (Flags & BM_FLAGS_ASYNC) {
                    /* not supported yet */
                status = SDIO_STATUS_ERROR;
                DBG_ASSERT(FALSE);
                break;    
            }
            
            apiStatus = SDReadWriteRegistersDirect(pInstance->hDevice,
                                                   (Flags & BM_FLAGS_WRITE) ? SD_IO_WRITE : SD_IO_READ,
                                                   pInstance->FuncNo,
                                                   Address,
                                                   FALSE,
                                                   pBuffer,
                                                   Count);
            
            if (!SD_API_SUCCESS(apiStatus)) {
                status = SDIO_STATUS_ERROR;
                break;    
            }
            
            break;    
        }  
     
            /* FIFO - mode synchronous */       
        while (Count) {
            argument = BUILD_IO_RW_DIRECT_ARG(((Flags & BM_FLAGS_WRITE) ?  SD_IO_OP_WRITE: SD_IO_OP_READ),      
                                              SD_IO_RW_NORMAL,
                                              pInstance->FuncNo,
                                              (Address),
                                              (Flags & BM_FLAGS_WRITE) ? *pBuffer : 0);
              
            apiStatus = SDSynchronousBusRequest(pInstance->hDevice, 
                                                SD_IO_RW_DIRECT,
                                                argument,
                                                SD_COMMAND,
                                                ResponseR5,
                                                &response,
                                                0,
                                                0,
                                                NULL,
                                                0);
            
            if (!SD_API_SUCCESS(apiStatus)) {
                status = SDIO_STATUS_ERROR;
                break;    
            }
            
            if (!(Flags & BM_FLAGS_WRITE)) {
                *pBuffer = response.ResponseBuffer[SD_IO_R5_RESPONSE_DATA_BYTE_OFFSET];  
            }
            
            pBuffer++;
            Count--;            
        }   
                     
    } while (FALSE);                            
      
    return status;        
}

SDIO_STATUS SetCardBlockLength(PVOID        pContext, 
                               BM_CARD_TYPE CardType, 
                               UINT16       Length)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PBENCH_MARK_INSTANCE pInstance = (PBENCH_MARK_INSTANCE)pContext;
    SD_API_STATUS apiStatus;
    SD_COMMAND_RESPONSE response;
    DWORD         blockSize = Length;
    
    switch (CardType) {
        case  BM_CARD_SDIO:
            apiStatus = SDSetCardFeature(pInstance->hDevice,
                                         SD_IO_FUNCTION_SET_BLOCK_SIZE,
                                         &blockSize,
                                         sizeof(blockSize));
            if (!SD_API_SUCCESS(apiStatus)) {
                status = SDIO_STATUS_ERROR;        
            }            
        break;
        case BM_CARD_MMC:
        case BM_CARD_SD:
            apiStatus = SDSynchronousBusRequest(pInstance->hDevice, 
                                                SD_CMD_SET_BLOCKLEN,
                                                blockSize,
                                                SD_COMMAND,
                                                ResponseR1,
                                                &response,
                                                0,
                                                0,
                                                NULL,
                                                0);
            if (!SD_API_SUCCESS(apiStatus)) {
                status = SDIO_STATUS_ERROR;        
            }                                  
                              
        break;
        default:
            status = SDIO_STATUS_INVALID_PARAMETER;
            break;
    }
   
    return status;
}

SDIO_STATUS MemCardBlockTransfer(PVOID          pContext,
                                 UINT32         BlockAddress,
                                 UINT16         BlockCount,
                                 UINT16         BytesPerBlock,
                                 PUINT8         pBuffer,
                                 UINT32         Flags)
{
    SDIO_STATUS status = SDIO_STATUS_UNSUPPORTED;
    PBENCH_MARK_INSTANCE pInstance = (PBENCH_MARK_INSTANCE)pContext;
    
    return status;   
}

SDIO_STATUS CMD53Transfer_BlockBasis(PVOID  pContext, 
                                     UINT32 Address,
                                     PUINT8 pBuffer,
                                     UINT32 Blocks,
                                     UINT32 BytesPerBlock,
                                     UINT32 Flags)
{
    SDIO_STATUS status = SDIO_STATUS_UNSUPPORTED;

    
    return status;
}


SDIO_STATUS CMD53Transfer_ByteBasis(PVOID  pContext, 
                                    UINT32 Address,
                                    PUINT8 pBuffer,
                                    UINT32 Bytes,
                                    UINT32 BlockSizeLimit,
                                    UINT32 Flags)
{ 
    SDIO_STATUS status = SDIO_STATUS_UNSUPPORTED;

    return status;
}

SDIO_STATUS SetCardBusMode(PVOID pContext,BM_BUS_MODE BusMode,PUINT32 pClock)
{
    SDIO_STATUS                  status = SDIO_STATUS_UNSUPPORTED;
    PBENCH_MARK_INSTANCE pInstance = (PBENCH_MARK_INSTANCE)pContext;
 
    do {
        
        
        if (BM_BUS_1_BIT == BusMode) {
             
        } else if (BM_BUS_4_BIT == BusMode) {
            
        } else {
            DBG_ASSERT(FALSE);
            status = SDIO_STATUS_ERROR;
            break;   
        }
            
        // busSettings.ClockRate = *pClock;
                                   
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        //*pClock = busSettings.ActualClockRate;
    
    } while (FALSE);
                                               
    return status;
}


void SetHcdDebugLevel(PVOID pContext,CT_DEBUG_LEVEL Level) 
{
   
}

void RestoreHcdDebugLevel(PVOID pContext) 
{

}


/***** Streams interface entry points: ***********************/

BOOL DllEntry(HINSTANCE  hInstance,
              ULONG      Reason,
              LPVOID     pReserved)
{
   
    if (Reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls((HMODULE)hInstance);
    }

    if (Reason == DLL_PROCESS_DETACH) {        
    }

    return TRUE;
} 

VOID CleanUpInstance(PBENCH_MARK_INSTANCE pInstance)
{
    
    if (pInstance->hTestThread != NULL) {
        WaitForSingleObject(pInstance->hTestThread, INFINITE);
        CloseHandle(pInstance->hTestThread);    
        pInstance->hTestThread = NULL;
    }
    
    if (pInstance->pRegPath != NULL) {
        SDFreeMemory(pInstance->pRegPath);   
        pInstance->pRegPath = NULL;
    }
    
    if (pInstance->hIOComplete != NULL) {
        CloseHandle(pInstance->hIOComplete);
        pInstance->hIOComplete = NULL;    
    }
    
    if (pInstance->pTestBuffer != NULL) {
        LocalFree(pInstance->pTestBuffer);  
        pInstance->pTestBuffer = NULL;
    }     
    
    LocalFree(pInstance);   
}

DWORD RunTest(PVOID pContext)
{
    PBENCH_MARK_INSTANCE  pInstance = (PBENCH_MARK_INSTANCE)pContext; 
    
    CeSetThreadPriority(GetCurrentThread(),TEST_THREAD_DEF_PRIORITY);
    
    Sleep(1000);
    REL_PRINT(SDDBG_TRACE, ("SDIO BenchMark : Thread Enter \n"));
    BenchMarkTests(pInstance, 
                   &pInstance->BusChars);   
    return 0;                                 
}

/* streams INIT */
DWORD SBM_Init(DWORD InitContext)
{
    SDCARD_CLIENT_REGISTRATION_INFO clientInfo;
    PBENCH_MARK_INSTANCE            pInstance = NULL;
    SD_IO_FUNCTION_ENABLE_INFO      functionEnable;
    SDIO_CARD_INFO                  sdioInfo;
    SD_CARD_INTERFACE               sdioInterface;
    SD_HOST_BLOCK_CAPABILITY        sdioBlockCapability;
    DWORD                           threadId;
    BOOL                            success = FALSE;
    
    
    DBG_PRINT(SDDBG_TRACE,("SBM_Init: called  Context:0x%X \n",InitContext));
    
    do {
        
        pInstance = LocalAlloc(LPTR,sizeof(BENCH_MARK_INSTANCE));
        
        if (NULL == pInstance) {
            break;    
        }
        
        pInstance->BufferSize = BLOCK_BUFFER_BYTES;
        
        pInstance->pTestBuffer = (PUINT8)LocalAlloc(LPTR,pInstance->BufferSize);
        
        if (NULL == pInstance->pTestBuffer) {
            break;    
        }
        
        pInstance->hIOComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
        
        if (NULL == pInstance->hIOComplete) {
            break;    
        }
        
        pInstance->hDevice = SDGetDeviceHandle(InitContext, &pInstance->pRegPath);
        
        if (NULL == pInstance->hDevice) {
            break;
        }
    
        ZERO_OBJECT(clientInfo);
        wcscpy(clientInfo.ClientName, TEXT("SDIO Benchmark"));
    
        if (!SD_API_SUCCESS(SDRegisterClient(pInstance->hDevice, pInstance, &clientInfo))) {
            break;
        }
        
        ZERO_OBJECT(functionEnable);
        functionEnable.Interval = 10;
        functionEnable.ReadyRetryCount = 100;
    
        if (!SD_API_SUCCESS(SDSetCardFeature(pInstance->hDevice, 
                                             SD_IO_FUNCTION_ENABLE, 
                                             &functionEnable, 
                                             sizeof(functionEnable)))) {
            break;
        }
    
        ZERO_OBJECT(sdioInfo);
        
        if (!SD_API_SUCCESS(SDCardInfoQuery(pInstance->hDevice, 
                                            SD_INFO_SDIO, 
                                            &sdioInfo, 
                                            sizeof(sdioInfo)))) {
            break;
        }    
        
        pInstance->FuncNo = sdioInfo.FunctionNumber;
        
        ZERO_OBJECT(sdioInterface);
        
        if (!SD_API_SUCCESS(SDCardInfoQuery(pInstance->hDevice, 
                                            SD_INFO_CARD_INTERFACE, 
                                            &sdioInterface, 
                                            sizeof(sdioInterface)))) {
            break;
        }   

        ZERO_OBJECT(sdioBlockCapability);
        
        sdioBlockCapability.ReadBlocks = 0xFFFF;
        sdioBlockCapability.WriteBlocks = 0xFFFF;
        sdioBlockCapability.ReadBlockSize = 0xFFFF;
        sdioBlockCapability.WriteBlockSize = 0xFFFF;
                                            
        if (!SD_API_SUCCESS(SDCardInfoQuery(pInstance->hDevice, 
                                            SD_INFO_HOST_BLOCK_CAPABILITY, 
                                            &sdioBlockCapability, 
                                            sizeof(sdioBlockCapability)))) {
            break;
        }   
        
        
        ZERO_OBJECT(pInstance->BusChars);
        
        pInstance->BusChars.ClockRate = sdioInterface.ClockRate;
        
        if (sdioInterface.InterfaceMode == SD_INTERFACE_SD_MMC_1BIT) {
            pInstance->BusChars.BusMode = BM_BUS_1_BIT;   
        } else if (sdioInterface.InterfaceMode == SD_INTERFACE_SD_4BIT) {
            pInstance->BusChars.BusMode = BM_BUS_4_BIT;    
        } else {
            DBG_ASSERT(FALSE);    
        }
            
        pInstance->BusChars.MaxBlocksPerTransfer = max(sdioBlockCapability.ReadBlocks,
                                            sdioBlockCapability.WriteBlocks);
        pInstance->BusChars.MaxBytesPerBlock =  max(sdioBlockCapability.ReadBlockSize,
                                            sdioBlockCapability.WriteBlockSize);
    
    
        pInstance->hTestThread = CreateThread(NULL,0,RunTest,pInstance,0,&threadId);
        
        
        if (NULL == pInstance->hTestThread) {
            break;    
        }        
       
        success = TRUE;    
        
        DBG_PRINT(SDDBG_TRACE,("SBM_Init: Benchmark instance ready! \n"));
        
        
    } while (FALSE);
    
    if (!success) {
        if (pInstance != NULL) {
            CleanUpInstance(pInstance);
            pInstance = NULL; 
        }
    }
    
    return (DWORD)pInstance;
}

/* streams Deinit */
BOOL SBM_Deinit(DWORD DeviceContext)
{
    PBENCH_MARK_INSTANCE  pInstance = (PBENCH_MARK_INSTANCE)DeviceContext;
    
    DBG_PRINT(SDDBG_TRACE,("SBM_Deinit: Benchmark instance cleaned up.\n"));

    CleanUpInstance(pInstance);
    
    return TRUE;
}    

DWORD SBM_Open(DWORD    DeviceContext,
               DWORD    AccessCode,
               DWORD    ShareMode)
{
    return 0;
}

BOOL SBM_Close(DWORD OpenContext)
{
    return FALSE;
}


BOOL SBM_IOControl(DWORD   OpenContext,
                   DWORD   dwCode,
                   PBYTE   pBufIn,
                   DWORD   dwLenIn,
                   PBYTE   pBufOut,
                   DWORD   dwLenOut,
                   PDWORD  pdwActualOut)
{
    return FALSE;
}

DWORD SBM_Write(DWORD   OpenContext,
                LPCVOID pBuffer,
                DWORD   Count)
{

   return -1;
}


DWORD SBM_Read(DWORD    OpenContext,
               LPVOID   pBuffer,
               DWORD    Count)
{

    return -1;   
}

DWORD SBM_Seek(DWORD    OpenContext,
               LONG     Amount,
               DWORD    Type)
{
    return -1;
}

VOID SBM_PowerDown(DWORD DeviceContext)
{
    
}

VOID SBM_PowerUp(DWORD DeviceContext)
{
    
}

SDIO_STATUS GetRegistryKeyDWORD(HKEY   hKey, 
                                WCHAR  *pKeyPath,
                                WCHAR  *pValueName, 
                                PDWORD pValue)
{
    LONG  status;       /* reg api status */
    HKEY  hOpenKey;     /* opened key handle */
    DWORD type;
    DWORD value;
    ULONG bufferSize;
    
    status = RegOpenKeyEx(hKey,
                          pKeyPath,
                          0,
                          0,
                          &hOpenKey);

    if (status != ERROR_SUCCESS) {
        return SDIO_STATUS_ERROR;
    }
    
    bufferSize = sizeof(DWORD);
    
    status = RegQueryValueEx(hOpenKey,
                             pValueName,
                             NULL,
                             &type,
                             (PUCHAR)&value,
                             &bufferSize);

    RegCloseKey(hOpenKey); 
    
    if (ERROR_SUCCESS == status) {        
        if (REG_DWORD == type) {
            *pValue = value;    
            return SDIO_STATUS_SUCCESS;
        }
    } 
    
    return SDIO_STATUS_ERROR;
}
