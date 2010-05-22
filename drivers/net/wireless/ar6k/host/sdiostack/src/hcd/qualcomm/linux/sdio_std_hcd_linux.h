/******************************************************************************
 *
 * @File:       sdio_std_hcd_linux.c
 *
 * @Abstract:   Standard SDIO Host Controller Driver Header File
 *
 * @Notice:     Copyright(c), 2008 Atheros Communications, Inc.
 *
 * $ATH_LICENSE_SDIOSTACK0$
 *
 *****************************************************************************/

#ifndef __SDIO_STD_HCD_LINUX_H__
#define __SDIO_STD_HCD_LINUX_H__

/* debug level */
///+++FIH+++
///#define DBG_DECLARE                              4
///---FIH---

#include <linux/kernel.h>
#include "../sdio_qc_hcd.h"

/*
 * defaults for all std hosts, various attributes will be
 * cleared based on values from the capabilities register.
 */
#define DEFAULT_ATTRIBUTES (SDHCD_ATTRIB_BUS_1BIT      | \
                            SDHCD_ATTRIB_BUS_4BIT      | \
                            SDHCD_ATTRIB_MULTI_BLK_IRQ | \
                            SDHCD_ATTRIB_AUTO_CMD12    | \
                            SDHCD_ATTRIB_SLOT_POLLING  | \
                            SDHCD_ATTRIB_POWER_SWITCH  | \
                            SDHCD_ATTRIB_BUS_MMC8BIT   | \
                            SDHCD_ATTRIB_SD_HIGH_SPEED | \
                            SDHCD_ATTRIB_MMC_HIGH_SPEED)

/* flags */
/* don't use DMA even though capabilities indicate it can */
#define START_HCD_FLAGS_FORCE_NO_DMA             0x01
/* force SDMA even though the capabilities show advance DMA support */
#define START_HCD_FLAGS_FORCE_SDMA               0x02

/* for work item */
#define WORK_ITEM_IO_COMPLETE                    0
#define WORK_ITEM_CARD_DETECT                    1
#define WORK_ITEM_SDIO_IRQ                       2
 
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define ATH_INIT_WORK(_t, _f, _c)                INIT_WORK((_t), (void (*)(void *))(_f), (_c));
#else
#define ATH_INIT_WORK(_t, _f, _c)                INIT_DELAYED_WORK((_t), (_f));
#endif

/* for DMA */
#define HOST_REG_CAPABILITIES_ADMA               (1 << 20)
#define HOST_REG_CAPABILITIES_DMA                (1 << 22)
#define IS_HCD_ADMA(pHc)                         \
        ((pHc)->Hcd.pDmaDescription != NULL) &&  \
        ((pHc)->Hcd.pDmaDescription->Flags & SDDMA_DESCRIPTION_FLAG_SGDMA)

#define IS_HCD_SDMA(pHc)                         \
	(((pHc)->Hcd.pDmaDescription != NULL) && \
        (((pHc)->Hcd.pDmaDescription->Flags &    \
        (SDDMA_DESCRIPTION_FLAG_SGDMA | SDDMA_DESCRIPTION_FLAG_DMA)) == \
        SDDMA_DESCRIPTION_FLAG_DMA))

#define SDDMA_VALID                              0x1
#define SDDMA_END                                0x2
#define SDDMA_INT                                0x4
#define SDDMA_LENGTH                             0x10
#define SDDMA_TRANSFER                           0x20
#define SDDMA_DESCRIP_LINK                       0x30

#define SET_DMA_LENGTH(d, l)                     \
        ((d)->Length = ((l) << 12) | SDDMA_LENGTH | SDDMA_VALID)
#define SET_DMA_ADDRESS(d, l)                    \
        ((d)->Address = ((l) & 0xFFFFF000) | SDDMA_TRANSFER | SDDMA_VALID)
#define SET_DMA_END_OF_TRANSFER(d)               \
        ((d)->Address |= SDDMA_END);
 
/* structure define */
typedef struct _SDHCD_CORE_CONTEXT {
    SDLIST       List;
    PVOID        pBusContext;                    /* bus context this one belongs to */
    SDLIST       SlotList;                       /* the list of current slots handled by this driver */
    spinlock_t   SlotListLock;                   /* protection for the slot List */
    UINT         SlotCount;                      /* number of slots currently installed */  
    /* everything below this line is reserved for the user of this library */
    UINT32       CoreReserved1; 
    UINT32       CoreReserved2; 
} SDHCD_CORE_CONTEXT, *PSDHCD_CORE_CONTEXT;

typedef struct _STDHCD_DEV {
    SDLIST       CoreList;                       /* the list of core contexts */
    spinlock_t   CoreListLock;                   /* protection for the list */  
} STDHCD_DEV, *PSTDHCD_DEV;

/* scatter-gather tables, as we use it in 32-bit mode */
struct _SDHCD_SGDMA_DESCRIPTOR {
    UINT32      Length;
    UINT32      Address;
} CT_PACK_STRUCT; 

typedef struct _SDHCD_SGDMA_DESCRIPTOR           SDHCD_SGDMA_DESCRIPTOR;
typedef struct _SDHCD_SGDMA_DESCRIPTOR           *PSDHCD_SGDMA_DESCRIPTOR; 

/* prototype */
void  InitStdHostLib(void);
void  DeinitStdHostLib(void);
PSDHCD_CORE_CONTEXT CreateStdHostCore(PVOID pBusContext);
void  DeleteStdHostCore(PSDHCD_CORE_CONTEXT pStdCore);
PSDHCD_CORE_CONTEXT GetStdHostCore(PVOID pBusContext);
INT GetCurrentHcdInstanceCount(PSDHCD_CORE_CONTEXT pStdCore);
SDIO_STATUS StartStdHostCore(PSDHCD_CORE_CONTEXT pStdCore);
PSDHCD_INSTANCE RemoveStdHcdInstance(PSDHCD_CORE_CONTEXT pStdCore);
BOOL HandleSharedStdHostInterrupt(PSDHCD_CORE_CONTEXT pStdCore);
void DeleteStdHcdInstance(PSDHCD_INSTANCE pHcInstance);
typedef SDIO_STATUS (*PPLAT_OVERRIDE_CALLBACK)(PSDHCD_INSTANCE);
static SDIO_STATUS SetupDmaBuffers(PSDHCD_INSTANCE pHcInstance);
static void DeinitializeStdHcdInstance(PSDHCD_INSTANCE pHcInstance);
PSDHCD_INSTANCE CreateStdHcdInstance(
                        POS_DEVICE pOSDevice,
                        UINT       SlotNumber,
                        PTEXT      pName);
SDIO_STATUS AddStdHcdInstance(
                        PSDHCD_CORE_CONTEXT     pStdCore,
                        PSDHCD_INSTANCE         pHcInstance,
                        UINT                    Flags,
                        PPLAT_OVERRIDE_CALLBACK pCallBack);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void hcd_iocomplete_wqueue_handler(void *context); 
static void hcd_sdioirq_wqueue_handler(void *context); 
#else
static void hcd_iocomplete_wqueue_handler(struct work_struct *work);
static void hcd_sdioirq_wqueue_handler(struct work_struct *work);   
#endif

#endif /* __SDIO_STD_HCD_LINUX_H__ */
