// Copyright (c) 2006 Atheros Communications Inc.
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
@file: sdio_std_hcd_linux_lib.h

@abstract: include file for linux std host core APIs
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef SDIO_STD_HCD_LINUX_LIB_H_
#define SDIO_STD_HCD_LINUX_LIB_H_

typedef struct _SDHCD_CORE_CONTEXT {
    SDLIST       List;
    PVOID        pBusContext;        /* bus context this one belongs to */
    SDLIST       SlotList;         /* the list of current slots handled by this driver */
    spinlock_t   SlotListLock;     /* protection for the slot List */
    UINT         SlotCount;        /* number of slots currently installed */  
    /* everything below this line is reserved for the user of this library */
    UINT32       CoreReserved1; 
    UINT32       CoreReserved2; 
}SDHCD_CORE_CONTEXT, *PSDHCD_CORE_CONTEXT;

void  InitStdHostLib(void);
void  DeinitStdHostLib(void);
PSDHCD_CORE_CONTEXT CreateStdHostCore(PVOID pBusContext);
void  DeleteStdHostCore(PSDHCD_CORE_CONTEXT pStdCore);
PSDHCD_CORE_CONTEXT GetStdHostCore(PVOID pBusContext);

INT GetCurrentHcdInstanceCount(PSDHCD_CORE_CONTEXT pStdCore);
PSDHCD_INSTANCE CreateStdHcdInstance(POS_DEVICE pOSDevice, 
                                     UINT       SlotNumber, 
                                     PTEXT      pName);
void DeleteStdHcdInstance(PSDHCD_INSTANCE pHcInstance);
#define START_HCD_FLAGS_FORCE_NO_DMA  0x01  /* don't use DMA even though capabilities indicate it can */
#define START_HCD_FLAGS_FORCE_SDMA    0x02  /* force SDMA even though the capabilities show advance DMA support */

typedef SDIO_STATUS (*PPLAT_OVERRIDE_CALLBACK)(PSDHCD_INSTANCE);
SDIO_STATUS AddStdHcdInstance(PSDHCD_CORE_CONTEXT pStdCore, 
                              PSDHCD_INSTANCE pHcInstance, 
                              UINT  Flags, 
                              PPLAT_OVERRIDE_CALLBACK pCallBack,                                
                              SDDMA_DESCRIPTION       *pSDMADescrip,
                              SDDMA_DESCRIPTION       *pADMADescrip);                              
SDIO_STATUS StartStdHostCore(PSDHCD_CORE_CONTEXT pStdCore);  
PSDHCD_INSTANCE RemoveStdHcdInstance(PSDHCD_CORE_CONTEXT pStdCore);
BOOL HandleSharedStdHostInterrupt(PSDHCD_CORE_CONTEXT pStdCore);
#endif /*SDIO_STD_HCD_LINUX_LIB_H_*/
