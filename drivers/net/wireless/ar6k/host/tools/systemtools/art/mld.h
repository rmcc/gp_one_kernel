/* mld.h - macros and definitions for sim environment hardware access */

/* Copyright (c) 2001 Atheros Communications, Inc., All Rights Reserved */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/mld.h#1 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/mld.h#1 $"

#ifndef __INCmldh
#define __INCmldh
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* includes */
#include "wlantype.h"
#include "pci.h"
#ifndef __ATH_DJGPPDOS__
#include "windrvr.h"
#endif

/* defines */
#define INTERRUPT_F2    1
#define TIMEOUT         4
#define ISR_INTERRUPT   0x10
#define DEFAULT_TIMEOUT 0xff

#define A_MEM_ZERO(addr, len) memset(addr, 0, len)

#define F2_VENDOR_ID			0x168C		/* vendor ID for our device */
#define MAX_REG_OFFSET			0xfffc	    /* maximum platform register offset */
#define MAX_CFG_OFFSET          256         /* maximum locations for PCI config space per device */
#define MAX_MEMREAD_BYTES       2048        /* maximum of 2k location per OSmemRead action */

/* PCI Config space mapping */
#define F2_PCI_CMD				0x04		/* address of F2 PCI config command reg */
#define F2_PCI_CACHELINESIZE    0x0C        /* address of F2 PCI cache line size value */
#define F2_PCI_LATENCYTIMER     0x0D        /* address of F2 PCI Latency Timer value */
#define F2_PCI_BAR				0x10		/* address of F2 PCI config BAR register */
#define F2_PCI_INTLINE          0x3C        /* address of F2 PCI Interrupt Line reg */
/* PCI Config space bitmaps */
#define MEM_ACCESS_ENABLE		0x002       /* bit mask to enable mem access for PCI */
#define MASTER_ENABLE           0x004       /* bit mask to enable bus mastering for PCI */
#define MEM_WRITE_INVALIDATE    0x010       /* bit mask to enable write and invalidate combos for PCI */
#define SYSTEMERROR_ENABLE      0x100		/* bit mask to enable system error */

#define NUM_DESCS				512		    /* number of descriptors - must be multiple of 8 */
#define DESC_SIZE				48			/* #### may need to change this */
#define SIZE_DESC_MEM			(NUM_DESCS*DESC_SIZE)   /* how much memory to allocate */
#define NUM_MAP_BYTES			(NUM_DESCS/8)			  /* how many bytes to track desc allocation */
#define BUFF_BLOCK_SIZE			0x100  /* size of a buffer block */
#define NUM_BUFF_BLOCKS			0x1000 
#define NUM_BUFF_MAP_BYTES		(NUM_BUFF_BLOCKS/8)

#define FCS_FIELD				4		   /* size of FCS */				
#define WEP_IV_FIELD			4		   /* wep IV field size */
#define WEP_ICV_FIELD			4		   /* wep ICV field size */
#define WEP_FIELDS	(WEP_IV_FIELD + WEP_ICV_FIELD) /* total size of wep fields needed */

/* typedefs */

/* Mapping back to original definitions by Fiona. They
 * should be phased out when convenient.
 */
#define STATUS				A_STATUS
#define	OK					A_OK
#define DEVICE_NOT_FOUND	A_DEVICE_NOT_FOUND
#define	NO_MEMORY			A_NO_MEMORY
#define	MEMORY_NOT_AVAIL	A_MEMORY_NOT_AVAIL
#define NO_FREE_DESC		A_NO_FREE_DESC
#define	BAD_ADDRESS			A_BAD_ADDRESS
#define	WIN_DRIVER_ERROR	A_WIN_DRIVER_ERROR
#define REGS_NOT_MAPPED		A_REGS_NOT_MAPPED

typedef struct eHandle {
    A_UINT16 eventID;
    A_UINT16 f2Handle;
} EVT_HANDLE;

typedef struct eventStruct {
    struct eventStruct  *pNext;         // pointer to next event
    struct eventStruct  *pLast;         // backward pointer to pervious event
    EVT_HANDLE          eventHandle;
    A_UINT32            type;
    A_UINT32            persistent;
    A_UINT32            param1;
    A_UINT32            param2;
    A_UINT32            param3;
    A_UINT32            result;
	A_UINT32			additionalParams[5];    
} EVENT_STRUCT;

typedef struct eventQueue {
    EVENT_STRUCT    *pHead;     // pointer to first event in queue
    EVENT_STRUCT    *pTail;     // pointer to last event in queue
    HANDLE          queueMutex; // mutex to make queue access mutually exclusive
    A_UINT16        queueSize;  // count of how many items are in queue
    A_BOOL          queueScan;  // set to true if in middle of a queue scan
} EVENT_QUEUE;

#define WLAN_MAX_DEV	8		/* Number of maximum supported devices */

//structure to share info with the kernel plugin
typedef struct dkKernelInfo
{
	A_BOOL	volatile	anyEvents;		/* set to true by plugin if it has any events */
	A_UINT32	regMemoryTrns;
	A_UINT32	dmaMemoryTrns;
	A_UINT32	f2MapAddress;	    /* address of where f2 registers are mapped */
	A_UINT32	G_memoryRange;
	A_BOOL	volatile	rxOverrun	;	/* set by kernel plugin if it detects a receive overrun */
	A_UINT32		devMapAddress; // address of where f2 registers are mapped 
} DK_KERNEL_INFO;

/* holds all the dk specific information within DEV_INFO structure */
typedef struct dkDevInfo {
	A_UINT32	f2MapAddress;	    /* address of where f2 registers are mapped */
	A_UINT16	f2Mapped;		    /* true if the f2 registers are mapped */
	A_UINT16	devIndex;	/* used to track which F2 within system this is */
	A_UINT32	memSize;
	A_UINT32	dwBus;		/* hold bus slot and function info about device */
	A_UINT32	dwSlot;
	A_UINT32	dwFunction;	
	A_BOOL	    haveEvent;   // set to true when we have an event created
#ifndef __ATH_DJGPPDOS__
	WD_CARD_REGISTER G_cardReg; /* holds resource info about pci card */
	WD_CARD_REGISTER pluginMemReg; /* holds info about the memory allocated in plugin */
	A_UINT32 G_baseMemory;
	WD_DMA	dma;				//holds info  about memory for transfers
	A_UINT32 G_resMemory;
	A_UINT32 G_intIndex;
	WD_INTERRUPT gIntrp;
	A_UINT32 volatile intEnabled;

	WD_KERNEL_PLUGIN kernelPlugIn; //handle for kernel plugin
	WD_DMA	kerplugDma;			   //handle to shared mem between plugin and client
	WD_CARD_REGISTER kerplugCardReg;
#endif

/* shared info by kernel plugin */
	A_BOOL	volatile	anyEvents;		/* set to true by plugin if it has any events */
	A_UINT32	regMemoryTrns;
	A_UINT32	dmaMemoryTrns;
	A_UINT32	G_memoryRange;
	A_BOOL	volatile	rxOverrun;	/* set by kernel plugin if it detects a receive overrun */
   	EVENT_QUEUE *IsrEventQ;
	
} DK_DEV_INFO;

/* WLAN_DRIVER_INFO structure will hold the driver global information.
 *
 */
typedef struct mdk_wlanDrvInfo {
	A_UINT32           devCount;                     /* No. of currently connected devices */
	struct mdk_wlanDevInfo *pDevInfoArray[WLAN_MAX_DEV]; /* Array of devinfo pointers */
	struct eventQueue  *triggeredQ;	                 /* pointer to q of triggered events */
} MDK_WLAN_DRV_INFO;

/*
 * MDK_WLAN_DEV_INFO structure will hold all kinds of device related information.
 * It will hold OS specific information about the device and a device number.
 * Most of the code doesn't need to know what's inside that structure.
 * The fields are very likely to change.
 */

typedef	struct	mdk_wlanDevInfo
{
	DK_DEV_INFO *   	pdkInfo;            /* pointer to structure containing info for dk */
	PCI_CARD_INFO		pciInfo;            /* Struct to hold PCI related information */
	A_UINT32	        devno;			    /* Device number. */
	void	            *pDescMemory;       /* pointer to start of descriptor memory block */
	A_UINT32	        descPhysAddr;       /* physical address of descriptor memory block */
	A_UINT32 volatile	intEnabled;
} MDK_WLAN_DEV_INFO;

// SOCK_INFO contains the socket information for commuincating with AP

typedef struct artSockInfo {
    A_CHAR 	 hostname[128];
    A_INT32  port_num;
    A_UINT32 ip_addr;
    A_INT32  sockfd;
} ART_SOCK_INFO;

/////////////////////////////////////
// function declarations

A_UINT32 hwMemRead32(A_UINT16 devIndex, A_UINT32 memAddress);
void hwMemWrite32(A_UINT16 devIndex, A_UINT32 memAddress, A_UINT32 writeValue);
A_UINT32 hwCfgRead32(A_UINT16 devIndex, A_UINT32 address);
void hwCfgWrite32(A_UINT16 devIndex, A_UINT32 address, A_UINT32 writeValue);
A_INT16 hwMemWriteBlock(A_UINT16 devIndex,A_UCHAR *pBuffer, A_UINT32 length, A_UINT32 *pPhysAddr);
A_INT16 hwMemReadBlock(A_UINT16 devIndex,A_UCHAR *pBuffer, A_UINT32 physAddr, A_UINT32 length);
EVENT_STRUCT *popEvent(EVENT_QUEUE *pQueue);
A_UINT16 checkForEvents(EVENT_QUEUE *pQueue);
A_INT16 hwCreateEvent(A_UINT16 devIndex, A_UINT32 type, A_UINT32 persistent, A_UINT32 param1, A_UINT32 param2,
    A_UINT32 param3, EVT_HANDLE eventHandle);
A_STATUS envInit(A_BOOL debugMode,A_BOOL openDriver);
void envCleanup(A_BOOL closeDriver);
A_STATUS deviceInit(A_UINT16 devIndex);
void deviceCleanup(A_UINT16 devIndex);

extern	A_STATUS osThreadCreate(void threadFunc(void * param), A_UINT32 value, MDK_WLAN_DEV_INFO * pdevInfo);
extern void milliSleep(A_UINT32 millitime);

#define milliTime() (GetTickCount())

A_UINT16 uilog(char *, A_BOOL);
void uilogClose(void);
void dk_quiet(A_UINT16 Mode);
A_INT32 uiPrintf(const char * format, ...);
A_INT32 q_uiPrintf(const char * format, ...);
void configPrint(A_BOOL flag);

#ifndef A_ASSERT
  #include <assert.h>
  #define A_ASSERT assert
#endif
#define A_MALLOC(a)	(malloc(a))
#define A_FREE(a)	(free(a))

// Global Externs
extern MDK_WLAN_DRV_INFO	globDrvInfo;	            /* Global driver data structure */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __INCmldh */
