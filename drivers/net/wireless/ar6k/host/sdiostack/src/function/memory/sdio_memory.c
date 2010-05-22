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
@file: sdio_memory.c

@abstract: SDIO Memeory Function driver

#notes: includes OS independent portions - 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MODULE_NAME  SDMEMORYFD
#include "../../include/ctsystem.h"

#include "../../include/sdio_busdriver.h"
#include "../../include/sdio_lib.h"
#include "../../include/_sdio_defs.h" 
#include "../../include/mmc_defs.h"
#include "sdio_memory.h"


#define MMC_GET_START_BIT(pCsd) (pCsd[0] & 0x1)
#define MMC_GET_FILE_FORMAT(pCsd) (pCsd[1] & 0xC)
#define MMC_GET_PERM_WRITE_PROTECT(pCsd) (pCsd[1] & 0x20)
#define MMC_GET_TEMP_WRITE_PROTECT(pCsd) (pCsd[1] & 0x10)
#define MMC_GET_FILE_FORMAT_GROUP(pCsd) (pCsd[1] & 0x80)
#define MMC_GET_PARTIAL_WRITE_DATA(pCsd) (pCsd[2] & 0x20)
#define MMC_GET_MAX_WRITE_DATA_BLOCK(pCsd) (((pCsd[2] & 0xC0)>>6) | ((pCsd[3] & 0x3) << 2))
#define MMC_GET_WRITE_SPEED_FACTOR(pCsd) (pCsd[3] & 0x1C)
#define MMC_GET_MAX_READ_DATA_BLOCK(pCsd) (pCsd[10] & 0x0F)
#define MMC_GET_PARTIAL_READ_DATA(pCsd) (pCsd[9] & 0x80)
#define MMC_GET_C_SIZE_MULT(pCsd) (((pCsd[5] & 0x80)>>7) | ((pCsd[6] & 0x03)<<1))
#define MMC_GET_C_SIZE(pCsd) (((pCsd[7] & 0xC0)>>6) | (pCsd[8]<<2) |((pCsd[9] & 0x03)<<10))
#define MMC_GET_SPEC_VERSION(pCsd) ((pCsd[15] & 0x3C) >> 2) /* MMC Only */
#define MMC_GET_CSD_VERSION(pCsd) ((pCsd[15] & 0xC0) >> 6)
static const INT MMC_POWER_TABLE[12] = {
    1,2,4,8,16,32,64,128,256,512,1024,2048
};

#define MMC_GET_WRITE_CURR_MAX(pCsd) (((pCsd)[6] >> 2) & 0x07)
#define MMC_GET_READ_CURR_MAX(pCsd) ((pCsd)[7] & 0x07)

    /* VDD current table, mA */
static const UINT8 VDDCurrentTable[8] = {
    1,5,10,25,35,45,80,200  
};

#define MMC_CMD_SET_BLOCK_LENGTH    CMD16
#define MMC_CMD_READ_SINGLE_BLOCK   CMD17
#define MMC_CMD_READ_MULTIPLE_BLOCK CMD18
#define MMC_CMD_WRITE_SINGLE_BLOCK  CMD24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK CMD25


const SD_SLOT_CURRENT MMC_PowerClass_3_6V_To_Current[MMC_EXT_MAX_PWR_CLASSES] =
    /* in milliamps */
{  
   200, /* 0 */  
   220, /* 1 */
   250, /* 2 */  
   280, /* 3 */  
   300, /* 4 */  
   320, /* 5 */  
   350, /* 6 */  
   400, /* 7 */  
   450, /* 8 */  
   500, /* 9 */  
   550, /* 10 */  
   0,0,0,0,0 /* 11 - 15 are reserved */  
};
 
const SD_SLOT_CURRENT MMC_PowerClass_1_95V_To_Current[MMC_EXT_MAX_PWR_CLASSES] =
    /* in milliamps */
{  
   130, /* 0 */  
   140, /* 1 */
   160, /* 2 */  
   180, /* 3 */  
   200, /* 4 */  
   220, /* 5 */  
   240, /* 6 */  
   260, /* 7 */  
   280, /* 8 */  
   300, /* 9 */  
   350, /* 10 */  
   0,0,0,0,0 /* 11 - 15 are reserved */  
};



static SDIO_STATUS IssueDeviceRequest(PSDDEVICE        pDevice,
                                      UINT8            Cmd,
                                      UINT32           Argument,
                                      SDREQUEST_FLAGS  Flags,
                                      PSDREQUEST       pReqToUse,
                                      PVOID            pData,
                                      INT              Length);
static SDIO_STATUS ReadBlocks(PSDDEVICE        pDevice,
                              PSDIO_MEMORY_INSTANCE pInstance,
                              UINT32           Address,
                              PVOID            pData,
                              UINT32           Length);
static SDIO_STATUS WriteBlocks(PSDDEVICE        pDevice,
                              PSDIO_MEMORY_INSTANCE pInstance,
                              UINT32           Address,
                              PVOID            pData,
                              UINT32           Length);
    
                                         
/* delete an instance  */
void DeleteInstance(PSDIO_MEMORY_CONTEXT   pFuncContext,
                    PSDIO_MEMORY_INSTANCE  pInstance)
{
 
    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pFuncContext->InstanceSem))) {
        return; 
    }
        /* pull it out of the list */
    SDListRemove(&pInstance->SDList);
    SemaphorePost(&pFuncContext->InstanceSem);
    
        
        /* free slot current */
    SDLIB_IssueConfig(pInstance->pDevice,
                      SDCONFIG_FUNC_FREE_SLOT_CURRENT,
                      NULL,
                      0);    
                      
    KernelFree(pInstance);
}

static void ReorderBuffer(UINT8 *pBuffer, INT Bytes)
{
    UINT8 *pEnd; 
    UINT8 temp;
    
    DBG_ASSERT(!(Bytes & 1));  
        /* point to the end */
    pEnd = &pBuffer[Bytes - 1];
        /* divide in half */
    Bytes = Bytes >> 1;
    
    while (Bytes) {
        temp = *pBuffer;
            /* swap bytes */
        *pBuffer = *pEnd;
        *pEnd = temp;
        pBuffer++;
        pEnd--;
        Bytes--;    
    }
}

SD_SLOT_CURRENT LookupCurrent(PSDDEVICE pDevice, PUINT8 pExtendedCSD) 
{
    const SD_SLOT_CURRENT *pTable = NULL;
    UINT8 powerClass = 0; 
     
    switch(SDDEVICE_GET_SLOT_VOLTAGE_MASK(pDevice)) {                
        case SLOT_POWER_3_3V:   
        case SLOT_POWER_3_0V:  
        case SLOT_POWER_2_8V: 
            pTable = MMC_PowerClass_3_6V_To_Current;
            if (SDDEVICE_GET_OPER_CLOCK(pDevice) <= 26000000) {
                DBG_PRINT(SDDBG_TRACE, ("    Using MMC 3.6V and 26Mhz power table \n")); 
                powerClass = pExtendedCSD[MMC_EXT_PWR_CL_26_360_OFFSET];    
            } else {
                DBG_PRINT(SDDBG_TRACE, ("    Using MMC 3.6V and 52Mhz power table \n")); 
                powerClass = pExtendedCSD[MMC_EXT_PWR_CL_52_360_OFFSET];   
            }      
            break;
        case SLOT_POWER_2_0V:  
        case SLOT_POWER_1_8V:  
        case SLOT_POWER_1_6V:
            pTable = MMC_PowerClass_1_95V_To_Current;
            if (SDDEVICE_GET_OPER_CLOCK(pDevice) <= 26000000) {
                DBG_PRINT(SDDBG_TRACE, ("    Using MMC 1.95V and 26Mhz power table \n")); 
                powerClass = pExtendedCSD[MMC_EXT_PWR_CL_26_195_OFFSET];    
            } else {
                DBG_PRINT(SDDBG_TRACE, ("    Using MMC 1.95V and 52Mhz power table \n")); 
                powerClass = pExtendedCSD[MMC_EXT_PWR_CL_52_195_OFFSET];   
            }      
            break;
        default:
            DBG_ASSERT(FALSE);
            break;        
    }
                                   
    if (pTable != NULL) {
        if (SDDEVICE_GET_BUSWIDTH(pDevice) == SDCONFIG_BUS_WIDTH_MMC8_BIT) {
                /* use upper nibble for power class */
            powerClass >>= 4;   
        }
        powerClass &= 0xF;
        DBG_PRINT(SDDBG_TRACE, ("    MMC Power Class %d: \n",powerClass));
        return pTable[powerClass];      
    }
    
    return 0;
}
/* create a memory instance */
PSDIO_MEMORY_INSTANCE CreateDeviceInstance(PSDIO_MEMORY_CONTEXT pFuncContext,
                                           PSDDEVICE            pDevice)
{
    PSDIO_MEMORY_INSTANCE pInstance = NULL;
    SDCONFIG_FUNC_SLOT_CURRENT_DATA   slotCurrent;
    PUINT8 pCSD = SDDEVICE_GET_CARDCSD(pDevice);
    SD_SLOT_CURRENT  maxReadCurrent = 0;
    SD_SLOT_CURRENT  maxWriteCurrent = 0;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDREQUEST  pReq = NULL;
    
    ZERO_OBJECT(slotCurrent);
    
    do {
        pInstance = (PSDIO_MEMORY_INSTANCE)KernelAlloc(sizeof(SDIO_MEMORY_INSTANCE));
        if (NULL == pInstance) {
            status = SDIO_STATUS_NO_RESOURCES;
            break;   
        }
        ZERO_POBJECT(pInstance);
        SDLIST_INIT(&pInstance->SDList);
        pInstance->pDevice = pDevice;

        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function Instance: 0x%X \n",(INT)pInstance));
        DBG_PRINT(SDDBG_TRACE, (" Card Flags:   0x%X \n",SDDEVICE_GET_CARD_FLAGS(pDevice)));     
        DBG_PRINT(SDDBG_TRACE, (" Card RCA:     0x%X \n",SDDEVICE_GET_CARD_RCA(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper Clock:   %d Hz \n",SDDEVICE_GET_OPER_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max Clock:    %d Hz \n",SDDEVICE_GET_MAX_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper BlklenLim:  %d bytes \n",SDDEVICE_GET_OPER_BLOCK_LEN(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max  BlkLen:     %d bytes\n",SDDEVICE_GET_MAX_BLOCK_LEN(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper BlksLim:    %d blocks per trans \n",SDDEVICE_GET_OPER_BLOCKS(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max  Blks:       %d blocks per trans \n",SDDEVICE_GET_MAX_BLOCKS(pDevice)));
        
       
        pReq = SDDeviceAllocRequest(pDevice);
        
        if (NULL == pReq) {
            status = SDIO_STATUS_NO_RESOURCES;
            break;   
        }  
        
            /* for SD cards in high speed mode, the power consumption is reported in the switch
             * status block */
        if ((SDDEVICE_GET_CARD_FLAGS(pDevice) & CARD_SD) &&
             (SDDEVICE_GET_BUSMODE_FLAGS(pDevice) & SDCONFIG_BUS_MODE_SD_HS)) {
                /* fetch the switch status block */
            pReq->Command = CMD6;
            pReq->Argument = SD_SWITCH_FUNC_ARG_GROUP_CHECK(SD_SWITCH_HIGH_SPEED_GROUP,
                                                     SD_SWITCH_HIGH_SPEED_FUNC_NO);
            pReq->Flags =  SDREQ_FLAGS_RESP_R1 | SDREQ_FLAGS_DATA_TRANS;
            pReq->BlockCount = 1;
            pReq->BlockLen = SD_SWITCH_FUNC_STATUS_BLOCK_BYTES;
            pReq->pDataBuffer = pInstance->ExtendedData;
            
            status = SDDEVICE_CALL_REQUEST_FUNC(pDevice,pReq);
                                            
            if (SDIO_SUCCESS(status)) { 
                    /* need to reorder this since cards send this MSB first */
                ReorderBuffer(pInstance->ExtendedData,SD_SWITCH_FUNC_STATUS_BLOCK_BYTES);
                maxWriteCurrent = SD_SWITCH_FUNC_STATUS_GET_MAX_CURRENT(pInstance->ExtendedData);  
                if (maxWriteCurrent == 0) {
                    DBG_PRINT(SDDBG_WARN, ("SDIO Memory: SD Card Switch Status indicates 0 current!, using CSD instead\n"));       
                } else {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory: SD High Speed card requires %d mA of current\n",
                        maxWriteCurrent));        
                }
                maxReadCurrent = maxWriteCurrent; 
            } else {
                DBG_PRINT(SDDBG_WARN, ("SDIO Memory: Failed to get SD Card Switch Status \n"));
                status = SDIO_STATUS_SUCCESS;
                    /* since we can't figure this out, use the largest value the SD spec says */
                maxWriteCurrent = 200;
                maxReadCurrent = 200;
            }
        }
            /* for MMC cards in high speed mode the current consumption is in the extended CSD */
        if ((SDDEVICE_GET_CARD_FLAGS(pDevice) & CARD_MMC) &&
             (SDDEVICE_GET_BUSMODE_FLAGS(pDevice) & SDCONFIG_BUS_MODE_MMC_HS)) {
             
            pReq->Command = MMC_CMD8;
            pReq->Argument = 0;
            pReq->Flags =  SDREQ_FLAGS_RESP_R1 | SDREQ_FLAGS_DATA_TRANS;
            pReq->BlockCount = 1;
            pReq->BlockLen = MMC_EXT_CSD_SIZE;
            pReq->pDataBuffer = pInstance->ExtendedData;
            
            status = SDDEVICE_CALL_REQUEST_FUNC(pDevice,pReq);
  
            if (SDIO_SUCCESS(status)) { 
                maxWriteCurrent = LookupCurrent(pDevice,pInstance->ExtendedData);
                if (maxWriteCurrent == 0) {
                    DBG_PRINT(SDDBG_WARN, ("SDIO Memory: MMC card Extended CSD indicates 0 current!, using old CSD instead\n"));       
                } else {
                    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory: MMC High Speed card requires %d mA of current\n",
                        maxWriteCurrent));          
                }
                maxReadCurrent = maxWriteCurrent; 
            } else {
                DBG_PRINT(SDDBG_WARN, ("SDIO Memory: Failed to get MMC Extended CSD \n"));
                status = SDIO_STATUS_SUCCESS;
                    /* since we can't figure this out, pick a reasonable value */
                maxWriteCurrent = 500;
                maxReadCurrent = 500;
            }
        }
                     
        if (0 == maxWriteCurrent) {          
                /* get max currents for write from CSD */
            maxWriteCurrent =  VDDCurrentTable[MMC_GET_WRITE_CURR_MAX(pCSD)];
        }
        
        if (0 == maxReadCurrent) {
                /* get max currents for read from CSD */
            maxReadCurrent =  VDDCurrentTable[MMC_GET_READ_CURR_MAX(pCSD)];
        }
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory: Write Max Curr: %d mA, Read Max Curr: %d mA\n", maxWriteCurrent,
            maxReadCurrent));        
        
            /* pick the highest of the read or write */
        slotCurrent.SlotCurrent = max(maxWriteCurrent, maxReadCurrent);
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory: Allocating Slot current: %d mA\n", slotCurrent.SlotCurrent));         
        status = SDLIB_IssueConfig(pDevice,
                                   SDCONFIG_FUNC_ALLOC_SLOT_CURRENT,
                                   &slotCurrent,
                                   sizeof(slotCurrent));
                                   
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Memory: failed to allocate slot current %d\n",
                                    status));
            if (status == SDIO_STATUS_NO_RESOURCES) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Memory: Remaining Slot Current: %d mA\n",
                                    slotCurrent.SlotCurrent));  
            }
            break;
        }
        
    } while (FALSE);

    if (!SDIO_SUCCESS(status) && (pInstance != NULL)) {
        KernelFree(pInstance);
        pInstance = NULL;
    }
    
    if (pReq != NULL) {
        SDDeviceFreeRequest(pDevice,pReq);    
    }
    return pInstance;
}

PSDIO_MEMORY_INSTANCE GetFirstInstance(PSDIO_MEMORY_CONTEXT pFuncContext)
{
    SDIO_STATUS status;
    PSDIO_MEMORY_INSTANCE pInstance;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    } 
    
    if (SDLIST_IS_EMPTY(&pFuncContext->InstanceList)) {
            /* no more left */
        pInstance = NULL; 
    } else {
            /* get the item at the head */
        pInstance = CONTAINING_STRUCT(SDLIST_GET_ITEM_AT_HEAD(&pFuncContext->InstanceList),
                                      SDIO_MEMORY_INSTANCE, 
                                      SDList);   
    }
    
    SemaphorePost(&pFuncContext->InstanceSem);
    return pInstance;
}

/* find an instance associated with the SD device */
PSDIO_MEMORY_INSTANCE FindInstance(PSDIO_MEMORY_CONTEXT pFuncContext,
                                          PSDDEVICE                 pDevice)
{
    SDIO_STATUS status;
    PSDLIST     pItem;
    PSDIO_MEMORY_INSTANCE pInstance = NULL;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    }
        /* walk the list and find our instance */
    SDITERATE_OVER_LIST(&pFuncContext->InstanceList, pItem) {
        pInstance = CONTAINING_STRUCT(pItem, SDIO_MEMORY_INSTANCE, SDList);
        if (pInstance->pDevice == pDevice) {
                /* found it */
            break;   
        }
        pInstance = NULL;  
    }    
    
    SemaphorePost(&pFuncContext->InstanceSem);
    return pInstance;
}

/* add and instance to our list */
SDIO_STATUS AddDeviceInstance(PSDIO_MEMORY_CONTEXT  pFuncContext,
                              PSDIO_MEMORY_INSTANCE pInstance)
{
    SDIO_STATUS status;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return status; 
    }
  
    SDListAdd(&pFuncContext->InstanceList,&pInstance->SDList);  
    SemaphorePost(&pFuncContext->InstanceSem);
    
    return SDIO_STATUS_SUCCESS;
}

/* cleanup the function context */
void CleanupFunctionContext(PSDIO_MEMORY_CONTEXT pFuncContext)
{
    SemaphoreDelete(&pFuncContext->InstanceSem);  
}

/* initialize the function context */
SDIO_STATUS InitFunctionContext(PSDIO_MEMORY_CONTEXT pFuncContext)
{
    SDIO_STATUS status;
    SDLIST_INIT(&pFuncContext->InstanceList); 
   
    status = SemaphoreInitialize(&pFuncContext->InstanceSem, 1);    
    if (!SDIO_SUCCESS(status)) {
        return status;
    }
    
    return SDIO_STATUS_SUCCESS;
}

/* 
 * GetCardCSD - get the interesting part of Card CSD register
*/
SDIO_STATUS GetCardCSD(PSDDEVICE pDevice, PSDIO_MEMORY_INSTANCE pInstance, BOOL IsMmcCardType)
{
    PUINT8 pCSD = SDDEVICE_GET_CARDCSD(pDevice);
    UINT32 size;
    SDIO_STATUS status;    
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: ViewCardCSD\n"));
    if (!MMC_GET_START_BIT(pCSD)) {
        /* this bit is not really available to be read DBG_PRINT(SDDBG_ERROR, ("**ERROR, start bit not 1\n"));*/
    }
    if (MMC_GET_CSD_VERSION(pCSD) == 0) {
        DBG_PRINT(SDDBG_TRACE, ("  CSD version: 1.0\n")); /* all SD cards should be here */
    } else if (MMC_GET_CSD_VERSION(pCSD) == 1) {
        DBG_PRINT(SDDBG_TRACE, ("  CSD version: 1.1\n"));
    } else if (MMC_GET_CSD_VERSION(pCSD) == 2) {
        DBG_PRINT(SDDBG_TRACE, ("  CSD version: 1.2\n"));
    } else {
        DBG_PRINT(SDDBG_TRACE, ("  CSD version: reserved %d\n", MMC_GET_SPEC_VERSION(pCSD)));
    }
    if (IsMmcCardType) {
        if (MMC_GET_SPEC_VERSION(pCSD) == 0) {
            DBG_PRINT(SDDBG_TRACE, ("  spec version: 1.0-1.2\n"));
        } else if (MMC_GET_SPEC_VERSION(pCSD) == 1) {
            DBG_PRINT(SDDBG_TRACE, ("  spec version: 1.4\n"));
        } else if (MMC_GET_SPEC_VERSION(pCSD) == 2) {
            DBG_PRINT(SDDBG_TRACE, ("  spec version: 2.0-2.2\n"));
        } else if (MMC_GET_SPEC_VERSION(pCSD) == 3) {
            DBG_PRINT(SDDBG_TRACE, ("  spec version: 3.1-3.31\n"));
        } else if (MMC_GET_SPEC_VERSION(pCSD) == 4) {
            DBG_PRINT(SDDBG_TRACE, ("  spec version: 4.0-4.1\n"));
        } else {
            DBG_PRINT(SDDBG_TRACE, ("  spec version: reserved %d\n", MMC_GET_SPEC_VERSION(pCSD)));
        }
    }
    if (MMC_GET_FILE_FORMAT_GROUP(pCSD)) {
        DBG_PRINT(SDDBG_TRACE, ("  File format reserved: %d\n", MMC_GET_FILE_FORMAT(pCSD)));
    } else {
        if (MMC_GET_FILE_FORMAT(pCSD) == 0) {
            DBG_PRINT(SDDBG_TRACE, ("  File format: Hard disk-like file system with partition table\n"));
        } else if (MMC_GET_FILE_FORMAT(pCSD) == 1) {
            DBG_PRINT(SDDBG_TRACE, ("  File format: DOS FAT (floppy-like) with boot sector only (no partition table)\n"));
        } else if (MMC_GET_FILE_FORMAT(pCSD) == 2) {
            DBG_PRINT(SDDBG_TRACE, ("  File format: Universal File Format\n"));
        } else if (MMC_GET_FILE_FORMAT(pCSD) == 3) {
            DBG_PRINT(SDDBG_TRACE, ("  File format: Others - Unknown\n"));
        }
    }
    if (MMC_GET_MAX_WRITE_DATA_BLOCK(pCSD) > 11) {
        DBG_PRINT(SDDBG_WARN, ("  Max write block *reserved* value: %d\n", 
                               MMC_GET_MAX_WRITE_DATA_BLOCK(pCSD)));
        /* use the max, as this size is in error */
        pInstance->WriteBlockLength = MMC_POWER_TABLE[11];
    } else {
        DBG_PRINT(SDDBG_TRACE, ("  Max write block: %d\n", 
                                MMC_POWER_TABLE[MMC_GET_MAX_WRITE_DATA_BLOCK(pCSD)]));
        pInstance->WriteBlockLength = MMC_POWER_TABLE[MMC_GET_MAX_WRITE_DATA_BLOCK(pCSD)];
    }
    pInstance->PartialWritesAllowed = MMC_GET_PARTIAL_WRITE_DATA(pCSD);
    if (pInstance->PartialWritesAllowed) {
        DBG_PRINT(SDDBG_TRACE, ("  partial write allowed:\n"));
    } else {
        DBG_PRINT(SDDBG_TRACE, ("  partial write not allowed:\n"));
    }

    DBG_PRINT(SDDBG_TRACE, ("  write speed R2W_FACTOR: %d\n", 
                            MMC_POWER_TABLE[MMC_GET_WRITE_SPEED_FACTOR(pCSD)]));

    if (MMC_GET_MAX_READ_DATA_BLOCK(pCSD) > 11) {
        DBG_PRINT(SDDBG_WARN, ("  Max read block reserved value: %d\n", 
                               MMC_GET_MAX_READ_DATA_BLOCK(pCSD)));
        /* use the max, as this size is in error */
        pInstance->ReadBlockLength = MMC_POWER_TABLE[11];
    } else {
        DBG_PRINT(SDDBG_TRACE, ("  Max read block: %d\n", 
                                MMC_POWER_TABLE[MMC_GET_MAX_READ_DATA_BLOCK(pCSD)]));
        pInstance->ReadBlockLength = MMC_POWER_TABLE[MMC_GET_MAX_READ_DATA_BLOCK(pCSD)];
    }
    pInstance->PartialReadsAllowed = MMC_GET_PARTIAL_READ_DATA(pCSD);
    if (pInstance->PartialReadsAllowed) {
        DBG_PRINT(SDDBG_TRACE, ("  partial read allowed:\n"));
    } else {
        DBG_PRINT(SDDBG_TRACE, ("  partial read not allowed:\n"));
    }

    DBG_PRINT(SDDBG_TRACE, ("  C_SIZE/C_SIZE_MULT: %d/%d\n", 
                            MMC_GET_C_SIZE(pCSD),MMC_GET_C_SIZE_MULT(pCSD) ));
    /* calulate the size in 1k increments, handle 4GB as a special case to avoid overflow */
    if ((MMC_POWER_TABLE[MMC_GET_C_SIZE_MULT(pCSD)+2] == 512) &&
        ((MMC_GET_C_SIZE(pCSD) + 1) == 4096) &&
        (MMC_POWER_TABLE[MMC_GET_MAX_READ_DATA_BLOCK(pCSD)] == 2048)) {
           /* special case of 4GB card */
        size = 0x400000;
    } else {
        size = (MMC_POWER_TABLE[MMC_GET_C_SIZE_MULT(pCSD)+2] * (MMC_GET_C_SIZE(pCSD) + 1) * 
               MMC_POWER_TABLE[MMC_GET_MAX_READ_DATA_BLOCK(pCSD)]) / 1024;
    }
    DBG_PRINT(SDDBG_TRACE, ("  capacity: %dK\n", (UINT)size));
    pInstance->Size = size;
    /* set the block size to the smaller of the read/write block sizes */
    /* use the smaller of two block sizes */
//??    pInstance->BlockSize = (pInstance->ReadBlockLength < pInstance->WriteBlockLength)?
//??                            pInstance->ReadBlockLength : pInstance->WriteBlockLength;
    /* MMC cards seem to want us to ignore the write block size and use the read block size,
       SD cards have these equal to each other */
    pInstance->BlockSize = pInstance->ReadBlockLength;
    pInstance->MaxBlocksPerTransfer = pDevice->pHcd->CardProperties.OperBlockCountLimit;
    /* tell the device this is the block size we'll use for reads and writes */
    status =  IssueDeviceRequest(pDevice, MMC_CMD_SET_BLOCK_LENGTH, pInstance->BlockSize,
                                 SDREQ_FLAGS_RESP_R1, NULL, NULL, 0);
    
    pInstance->WriteProtected = 
            (MMC_GET_PERM_WRITE_PROTECT(pCSD) || MMC_GET_TEMP_WRITE_PROTECT(pCSD)) ||
            SDDEVICE_IS_CARD_WP_ON(pDevice);
    DBG_PRINT(SDDBG_TRACE, ("  %s media- perm:%d temp:%d\n",
                            (pInstance->WriteProtected)? "write protected" : "writable", 
                            (UINT)MMC_GET_PERM_WRITE_PROTECT(pCSD), (UINT)MMC_GET_TEMP_WRITE_PROTECT(pCSD)));
    
    if (DBG_GET_DEBUG_LEVEL() >= SDDBG_DUMP) {
        SDLIB_PrintBuffer((PUCHAR)pCSD, (INT)MAX_CARD_RESPONSE_BYTES, "SDIO Memory Function: CSD");    
    }
    
    return status;
}



/*
 * MemoryTransfer - read/write to device
*/
SDIO_STATUS MemoryTransfer(PSDIO_MEMORY_INSTANCE pInstance, SDSECTOR_SIZE sectorNumber, 
                           ULONG sectorCount, PUCHAR pBuffer, BOOL WriteDirection)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    ULONG startOffset = sectorNumber * pInstance->FileSysBlockSize;
    ULONG length = sectorCount *  pInstance->FileSysBlockSize;
    
    if (((SDSECTOR_SIZE)startOffset+(SDSECTOR_SIZE)length) > 
            ((SDSECTOR_SIZE)pInstance->Size * (SDSECTOR_SIZE)1024)) {
        /* past end of disk */
        DBG_PRINT(SDDBG_WARN, ("SDIO Memory Function: MemoryTransfer sector past end of disk, %d/%d size/blockSize %d/%d \n",
                               (INT)sectorNumber, (INT)sectorCount, 
                               (INT)pInstance->Size, (INT)pInstance->FileSysBlockSize));
        return SDIO_STATUS_INVALID_PARAMETER;    
    }
    if (WriteDirection) {
        /* write to device */
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: MemoryTransfer writing, %d/%d \n",
                                (UINT)startOffset, (UINT)length));
        if ((length/pInstance->BlockSize) <= pInstance->MaxBlocksPerTransfer) { 
            status = WriteBlocks(pInstance->pDevice, pInstance, startOffset, pBuffer, length);
        } else {
            /* break into smaller chunks to transfer */
            int tlen;
            int offset = 0;
            for(tlen = length; tlen > 0; tlen = length) {
                tlen = (tlen/pInstance->MaxBlocksPerTransfer) * pInstance->BlockSize;
                tlen = ((tlen/pInstance->MaxBlocksPerTransfer) > pInstance->MaxBlocksPerTransfer) ?
                            pInstance->MaxBlocksPerTransfer * pInstance->BlockSize: tlen;
                length -= tlen;
                status = WriteBlocks(pInstance->pDevice, pInstance, startOffset, &pBuffer[offset], tlen);
                offset += tlen;
                startOffset += tlen/pInstance->MaxBlocksPerTransfer;
                if (!SDIO_SUCCESS(status)) {
                  break;
                }
            }
        }
        if (SDIO_SUCCESS(status)) {
            if (DBG_GET_DEBUG_LEVEL() >= SDDBG_DUMP) {
                SDLIB_PrintBuffer(pBuffer, length, "SDIO Memory Function: Write buffer"); 
            }   
        } else {
            DBG_PRINT(SDDBG_WARN, ("SDIO Memory Function: MemoryTransfer error on WriteBlock, %d\n",
                                   status ));
            if (DBG_GET_DEBUG_LEVEL() >= SDDBG_TRACE) {   
                SDLIB_PrintBuffer(pBuffer, (length > 16) ? 16 : length, "SDIO Memory Function: Write buffer, with error");    
            }
        }
    } else {
        /* read from device */
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: MemoryTransfer reading, Offset:%d Length:%d \n",
                                (UINT)startOffset, (UINT)length));
        if ((length/pInstance->BlockSize) <= pInstance->MaxBlocksPerTransfer) { 
            status = ReadBlocks(pInstance->pDevice, pInstance, startOffset, pBuffer, length);
        } else {
            /* break into smaller chunks to transfer */
            int tlen;
            int offset = 0;
            for(tlen = length; tlen > 0; tlen = length) {
                tlen = (tlen/pInstance->MaxBlocksPerTransfer) * pInstance->BlockSize;
                tlen = ((tlen/pInstance->MaxBlocksPerTransfer) > pInstance->MaxBlocksPerTransfer) ?
                            pInstance->MaxBlocksPerTransfer * pInstance->BlockSize: tlen;
                length -= tlen;
                status = ReadBlocks(pInstance->pDevice, pInstance, startOffset, &pBuffer[offset], tlen);
                offset += tlen;
                startOffset += tlen/pInstance->MaxBlocksPerTransfer;
                if (!SDIO_SUCCESS(status)) {
                  break;
                }
            }
        }
            
        if (SDIO_SUCCESS(status)) {
            if (DBG_GET_DEBUG_LEVEL() >= SDDBG_DUMP) {
                SDLIB_PrintBuffer(pBuffer, length, "SDIO Memory Function: Read buffer");
            }
        } else {
            DBG_PRINT(SDDBG_WARN, ("SDIO Memory Function: MemoryTransfer error on ReadBlock, %d\n",
                                   status ));            
            if (DBG_GET_DEBUG_LEVEL() >= SDDBG_TRACE) {   
                SDLIB_PrintBuffer(pBuffer, (length > 16) ? 16 : length, "SDIO Memory Function: Read buffer, with error");    
            }
        }
    }
    return status;
}

/*
 *  ReadBlocks
*/
static SDIO_STATUS ReadBlocks(PSDDEVICE        pDevice,
                              PSDIO_MEMORY_INSTANCE pInstance,
                              UINT32           Address,
                              PVOID            pData,
                              UINT32           Length)
{ 
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDREQUEST  pReq;
    
    pReq = SDDeviceAllocRequest(pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    pReq->Argument = Address;
    pReq->Flags = SDREQ_FLAGS_DATA_TRANS | SDREQ_FLAGS_RESP_R1;  
    pReq->Command = (Length > pInstance->BlockSize)? 
                        MMC_CMD_READ_MULTIPLE_BLOCK : MMC_CMD_READ_SINGLE_BLOCK;                 
    if (pReq->Command == MMC_CMD_READ_MULTIPLE_BLOCK) {   
            /* bus driver issues auto stop */ 
        pReq->Flags |= SDREQ_FLAGS_AUTO_CMD12; 
    }       
    pReq->pDataBuffer  = pData;
    pReq->BlockCount = Length / pInstance->BlockSize;
    pReq->BlockLen = pInstance->BlockSize;
    if (SDDEVICE_IS_BUSMODE_SPI(pDevice)) {
        pReq->RetryCount = 3;
    }
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: ReadBlocks reading, count/size length %d/%d %d cmd: %d \n",
                            (UINT)pReq->BlockCount, (UINT)pReq->BlockLen, 
                            (UINT)(pReq->BlockLen*pReq->BlockCount), (UINT)pReq->Command));
    status = SDDEVICE_CALL_REQUEST_FUNC(pDevice, pReq);
        
    SDDeviceFreeRequest(pDevice, pReq);   
 
    return status;
}

/*
 *  WriteBlocks
*/
static SDIO_STATUS WriteBlocks(PSDDEVICE        pDevice,
                              PSDIO_MEMORY_INSTANCE pInstance,
                              UINT32           Address,
                              PVOID            pData,
                              UINT32           Length)
{ 
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDREQUEST  pReq;
    
    pReq = SDDeviceAllocRequest(pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    pReq->Argument = Address;
    pReq->Flags = SDREQ_FLAGS_DATA_TRANS | SDREQ_FLAGS_RESP_R1 | SDREQ_FLAGS_DATA_WRITE | 
                  SDREQ_FLAGS_AUTO_TRANSFER_STATUS;              
    pReq->Command = (Length > pInstance->BlockSize)? 
                        MMC_CMD_WRITE_MULTIPLE_BLOCK : MMC_CMD_WRITE_SINGLE_BLOCK; 
    
    if (pReq->Command == MMC_CMD_WRITE_MULTIPLE_BLOCK){                   
        pReq->Flags |= SDREQ_FLAGS_AUTO_CMD12;
    }
    if (SDDEVICE_IS_BUSMODE_SPI(pDevice)) {
        pReq->RetryCount = 3;
    }
                            
    pReq->pDataBuffer  = pData;
    pReq->BlockCount = Length / pInstance->BlockSize;
    pReq->BlockLen = pInstance->BlockSize;
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: WriteBlocks reading, count/size length %d/%d %d cmd: %d \n",
                            (UINT)pReq->BlockCount, (UINT)pReq->BlockLen, 
                            (UINT)(pReq->BlockLen*pReq->BlockCount), (UINT)pReq->Command));
        
    status = SDDEVICE_CALL_REQUEST_FUNC(pDevice, pReq); 
     
    SDDeviceFreeRequest(pDevice, pReq);   
    return status;
}

/*
 *  IssueAsyncTransfer
*/
SDIO_STATUS IssueAsyncTransfer(PSDDEVICE        pDevice,
                            PSDIO_MEMORY_INSTANCE pInstance,
                            UINT32           Address,
                            UINT32           Length,
                            BOOL             Write,
                            PVOID            pBufferOrSGList,
                            UINT             SGcount,
                            void (*pCompletion)(struct _SDREQUEST *pRequest),
                            PVOID            pContext)
{ 
    PSDREQUEST  pReq;
    
    pReq = SDDeviceAllocRequest(pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
        
    pReq->Argument = Address;
   
        /* set up default flags */
    pReq->Flags = SDREQ_FLAGS_DATA_TRANS | SDREQ_FLAGS_RESP_R1 | 
                  SDREQ_FLAGS_TRANS_ASYNC;
                  
    if (SGcount > 0) {
             /* using DMA , pBufferOrSGList is a scatter gather list*/
        pReq->Flags |= SDREQ_FLAGS_DATA_DMA;
    }
    
    if (Write) {        
        pReq->Flags |= SDREQ_FLAGS_DATA_WRITE | SDREQ_FLAGS_AUTO_TRANSFER_STATUS;
        pReq->Command = (Length > pInstance->BlockSize)? 
                        MMC_CMD_WRITE_MULTIPLE_BLOCK : MMC_CMD_WRITE_SINGLE_BLOCK;                 
    } else {
        pReq->Command = (Length > pInstance->BlockSize)? 
                        MMC_CMD_READ_MULTIPLE_BLOCK : MMC_CMD_READ_SINGLE_BLOCK;       
    }
    
    if (Length > pInstance->BlockSize) {
            /* multi-block transfer requires CMD12 to stop */
        pReq->Flags |= SDREQ_FLAGS_AUTO_CMD12; 
    }
    
    pReq->pDataBuffer  = pBufferOrSGList;
    pReq->DescriptorCount = SGcount;
    pReq->BlockCount = Length / pInstance->BlockSize;
    pReq->BlockLen = pInstance->BlockSize;
    pReq->pCompletion = pCompletion; 
    pReq->pCompleteContext = pContext;
        
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: IssueAsyncTransfer (%s), (count/blksize:%d/%d) (length:%d) cmd: %d \n",
                            Write ? "TX": "RX",
                            (UINT)pReq->BlockCount, (UINT)pReq->BlockLen, 
                            (UINT)(pReq->BlockLen*pReq->BlockCount), (UINT)pReq->Command));
    return SDDEVICE_CALL_REQUEST_FUNC(pDevice, pReq);
}

/*
 *  WriteBlocksAsync
*/
SDIO_STATUS WriteBlocksAsync(PSDDEVICE        pDevice,
                             PSDIO_MEMORY_INSTANCE pInstance,
                             UINT32           Address,
                             UINT32           Length,
                             PSDDMA_DESCRIPTOR    pDmaList,
                             UINT             SGcount,
                             void (*pCompletion)(struct _SDREQUEST *pRequest),
                             PVOID pContext)
{ 
    PSDREQUEST  pReq;
    
    pReq = SDDeviceAllocRequest(pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    pReq->Argument = Address;
    pReq->Flags = SDREQ_FLAGS_DATA_TRANS | SDREQ_FLAGS_RESP_R1 | SDREQ_FLAGS_DATA_WRITE |
                  SDREQ_FLAGS_TRANS_ASYNC | SDREQ_FLAGS_DATA_DMA | SDREQ_FLAGS_AUTO_TRANSFER_STATUS;
    
    pReq->Command = (Length > pInstance->BlockSize)? 
                        MMC_CMD_WRITE_MULTIPLE_BLOCK : MMC_CMD_WRITE_SINGLE_BLOCK;                 
    if (pReq->Command == MMC_CMD_WRITE_MULTIPLE_BLOCK) {   
            /* bus driver issues auto stop */ 
        pReq->Flags |= SDREQ_FLAGS_AUTO_CMD12;  
    }       
    pReq->pDataBuffer  = (PVOID)pDmaList;
    pReq->DescriptorCount = SGcount;
    pReq->BlockCount = Length / pInstance->BlockSize;
    pReq->BlockLen = pInstance->BlockSize;
    pReq->pCompletion = pCompletion; 
    pReq->pCompleteContext = pContext;
        
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: WriteBlocksAsync reading, count/size length %d/%d %d cmd: %d \n",
                            (UINT)pReq->BlockCount, (UINT)pReq->BlockLen, 
                            (UINT)(pReq->BlockLen*pReq->BlockCount), (UINT)pReq->Command));
    return SDDEVICE_CALL_REQUEST_FUNC(pDevice, pReq);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  IssueDeviceRequest - issue a bus request
  Input:  pDevice - device to send to
          Cmd - command to issue
          Argument - command argument
          Flags - request flags
        
  Output: pReqToUse - request to use (if caller wants response data)
  Return: SDIO Status
  Notes:  This function only issues 1 block data transfers
          This function issues the request synchronously
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static SDIO_STATUS IssueDeviceRequest(PSDDEVICE        pDevice,
                                      UINT8            Cmd,
                                      UINT32           Argument,
                                      SDREQUEST_FLAGS  Flags,
                                      PSDREQUEST       pReqToUse,
                                      PVOID            pData,
                                      INT              Length)
{ 
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDREQUEST  pReq;
    
    if (NULL == pReqToUse) {
            /* caller doesn't care about the response data, allocate locally */
        pReq = SDDeviceAllocRequest(pDevice);
        if (NULL == pReq) {
            return SDIO_STATUS_NO_RESOURCES;    
        }
    } else {
            /* use the caller's request buffer */
        pReq = pReqToUse;  
    }
    
    pReq->Argument = Argument;          
    pReq->Flags = Flags;              
    pReq->Command = Cmd; 
    if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) {
        pReq->pDataBuffer  = pData;
        pReq->BlockCount = 1;
        pReq->BlockLen = Length;
    }
        
    status = SDDEVICE_CALL_REQUEST_FUNC(pDevice, pReq);

    if (NULL == pReqToUse) {
        DBG_ASSERT(pReq != NULL);
        SDDeviceFreeRequest(pDevice, pReq);   
    }
    return status;
}
