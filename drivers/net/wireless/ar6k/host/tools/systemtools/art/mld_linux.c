/* mld_linux.c - access hardware for devmld */

/* Copyright (c) 2001 Atheros Communications, Inc., All Rights Reserved */

/*
DESCRIPTION
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>

#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "mld_linux.h"
#include "ar5210reg.h"
#include "dk_ioctl.h"
#include "linux_ansi.h"


static FILE     *logFile;  /* file handle of logfile */    
static A_BOOL   logging;   /* set to 1 if a log file open */
static A_BOOL   enablePrint = 1;
static A_BOOL 	driverOpened;
static A_UINT16 quietMode; /* set to 1 for quiet mode, 0 for not */

/////////////////////////////////////////////////////////////////////////////
//GLOBAL VARIABLES
MDK_WLAN_DRV_INFO	globDrvInfo;				/* Global driver info */
A_UINT16 minorVersion;
A_UINT16 majorVersion;


/////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS
A_UINT16    driverOpen();
void        envCleanup(A_BOOL closeDriver);
void        deviceCleanup(A_UINT16 devIndex);
void 		sig_handler(int arg);

/**************************************************************************
* envInit - performs any intialization needed by the environment
*
* For the windows NT hardware environment, need to open the driver and
* perform any other initialization required by it
*
* RETURNS: A_OK if it works, A_ERROR if not
*/
A_STATUS envInit
    (
    A_BOOL debugMode,
	A_BOOL openDriver
    )
{
    A_UINT16 i;

    // quiet our optional uiPrintfs...
    dk_quiet((A_UINT16)(debugMode ? 0 : 1));

	ansi_init();

    // open a handle to the driver
	driverOpened = 0;
	if (openDriver) {
		if (!driverOpen()) {
			return A_ERROR;
		}
		driverOpened = 1;
	}	

    globDrvInfo.devCount = 0;

    // set all the devInfo pointers to null
    for(i = 0; i < WLAN_MAX_DEV; i++)
        globDrvInfo.pDevInfoArray[i] = NULL;

	signal(SIGINT,sig_handler);

    return A_OK;
}

void envCleanup
(
	A_BOOL closeDriver
)
{
    A_UINT16 i;

    // cleanup all the devInfo structures
    for ( i = 0; i < WLAN_MAX_DEV; i++ ) {
        if ( globDrvInfo.pDevInfoArray[i] ) {
            deviceCleanup(i);
		}
    }
 
    globDrvInfo.devCount = 0;

    if ((closeDriver) && (driverOpened)) {
		driverOpened = 0;
    }
}

void sig_handler
(
 	int arg
) 
{
		uiPrintf("Received SIGINT !! cleanup and close down ! \n");
		envCleanup(TRUE);
		exit(0);
}


/**************************************************************************
* deviceInit - performs any initialization needed for a device
*
* Perform the initialization needed for a device.  This includes creating a 
* devInfo structure and initializing its contents
*
* RETURNS: A_OK if successful, A_ERROR if not
*/
A_STATUS deviceInit
    (
    A_UINT16 devIndex /* index of globalDrvInfo which to add device to */
                              
    )
{
    MDK_WLAN_DEV_INFO *pdevInfo;
    A_UINT32      regValue;
	A_UINT32 	  version, iIndex;
	A_INT32 	  handle;
	A_UINT32 	  *vir_addr;
	struct client_info cliInfo;
	char 		dev_name[16];

    if (globDrvInfo.pDevInfoArray[devIndex]) {
        uiPrintf("Error : Device already in use \n");
        return A_ERROR;
    }

    pdevInfo = (MDK_WLAN_DEV_INFO *) A_MALLOC(sizeof(MDK_WLAN_DEV_INFO));
    if(!pdevInfo) {                                            
        uiPrintf("Error: Unable to allocate MDK_WLAN_DEV_INFO struct!\n");
        return(A_ERROR);
    }

    pdevInfo->pdkInfo = (DK_DEV_INFO *) A_MALLOC(sizeof(DK_DEV_INFO));
    if(!pdevInfo->pdkInfo) {
        A_FREE(pdevInfo);
        uiPrintf("Error: Unable to allocate DK_DEV_INFO struct!\n");
        return A_ERROR;
    }
	/* zero out the dkInfo struct */
	A_MEM_ZERO(pdevInfo->pdkInfo, sizeof(DK_DEV_INFO));
	
	/* open the device based on the device index */
	if (devIndex > 3) {
	    A_FREE(pdevInfo->pdkInfo);
        A_FREE(pdevInfo);
        uiPrintf("Error: Only 4 devices supported !\n");
        return(A_ERROR);
	}
	
	strcpy(dev_name,"/dev/dk");
	dev_name[7]='0'+devIndex;
	dev_name[8]='\0';
	handle = open(dev_name,O_RDWR);
	if (handle <  0) {
        uiPrintf("Error: Unable to open the device !\n");
	    A_FREE(pdevInfo->pdkInfo);
        A_FREE(pdevInfo);
        return(A_ERROR);
	}

	if (ioctl(handle,DK_IOCTL_GET_VERSION,&version) < 0) {
		close(handle);
	    A_FREE(pdevInfo->pdkInfo);
        A_FREE(pdevInfo);
        uiPrintf("Error: get version ioctl failed !\n");
        return(A_ERROR);
	}

    minorVersion = version & 0xffff;
    majorVersion = (version>>16) & 0xffff;
    if (majorVersion != DRV_MAJOR_VERSION) {
		close(handle);
	    A_FREE(pdevInfo->pdkInfo);
        A_FREE(pdevInfo);
		uiPrintf("Error: Driver (%d) and application (%d) version mismatch \n");
        return(A_ERROR);
	}

	if (ioctl(handle,DK_IOCTL_GET_CLIENT_INFO,&cliInfo) < 0) {
		close(handle);
        uiPrintf("Error: get version ioctl failed !\n");
        return(A_ERROR);
    }

	pdevInfo->handle = handle;

	pdevInfo->pdkInfo->f2Mapped = 1;
	pdevInfo->pdkInfo->devMapped = 1;
	pdevInfo->pdkInfo->devIndex = devIndex;
	pdevInfo->pdkInfo->f2MapAddress = cliInfo.reg_phy_addr;
	pdevInfo->pdkInfo->regMapRange = cliInfo.reg_range;
	pdevInfo->pdkInfo->memPhyAddr = cliInfo.mem_phy_addr;
    pdevInfo->pdkInfo->numBars  = cliInfo.numBars;

    if (minorVersion >= 2) {
      for(iIndex=0;iIndex<cliInfo.numBars;iIndex++) {
        pdevInfo->pdkInfo->aregPhyAddr[iIndex] = (A_UINT32)cliInfo.areg_phy_addr[iIndex];
	     vir_addr = (A_UINT32 *)mmap((char *)0,cliInfo.reg_range,PROT_READ|PROT_WRITE,MAP_SHARED,handle,cliInfo.reg_phy_addr);
	     if (vir_addr == NULL) {
		   uiPrintf("Error: Cannot map the device registers in user address space \n");
		   close(handle);
	       A_FREE(pdevInfo->pdkInfo);
           A_FREE(pdevInfo);
		   return A_ERROR;
	   }
         pdevInfo->pdkInfo->aregRange[iIndex] = cliInfo.areg_range[iIndex];
         pdevInfo->pdkInfo->aregVirAddr[iIndex] = (A_UINT32)vir_addr;
      }
	pdevInfo->pdkInfo->regVirAddr = pdevInfo->pdkInfo->aregVirAddr[0];;
    }
    else {
        pdevInfo->pdkInfo->aregPhyAddr[0] = cliInfo.reg_phy_addr;
        vir_addr = (A_UINT32 *)mmap((char *)0,cliInfo.reg_range,PROT_READ|PROT_WRITE,MAP_SHARED,handle,cliInfo.reg_phy_addr);
        if (vir_addr == NULL) {
            close(handle);
            uiPrintf("Error: Cannot map the device registers in user address space \n");
            return A_ERROR;
         }
         pdevInfo->pdkInfo->aregVirAddr[0] = (A_UINT32)  vir_addr;
         pdevInfo->pdkInfo->aregRange[0] = cliInfo.reg_range;
         pdevInfo->pdkInfo->regVirAddr = pdevInfo->pdkInfo->aregVirAddr[0];
    }

	vir_addr = (A_UINT32 *)mmap((char *)0,cliInfo.mem_size,PROT_READ|PROT_WRITE,MAP_SHARED,handle,cliInfo.mem_phy_addr);
	if (vir_addr == NULL) {
		uiPrintf("Error: Cannot map memory in user address space \n");
		munmap((void *)pdevInfo->pdkInfo->regVirAddr, pdevInfo->pdkInfo->regMapRange);
		close(handle);
	    A_FREE(pdevInfo->pdkInfo);
        A_FREE(pdevInfo);
		return A_ERROR;
	}
	pdevInfo->pdkInfo->memVirAddr = (A_UINT32)vir_addr;
	pdevInfo->pdkInfo->memSize = cliInfo.mem_size;
    
    q_uiPrintf("+ Allocated memory\n");
    q_uiPrintf("+ VirtAddress = %08x\n", (A_UINT32)(pdevInfo->pdkInfo->memVirAddr));
    q_uiPrintf("+ PhysAddress = %08x\n", (A_UINT32)(pdevInfo->pdkInfo->memPhyAddr));

    globDrvInfo.pDevInfoArray[devIndex] = pdevInfo;
    globDrvInfo.devCount++;

  	// Setup memory window, bus mastering, & SERR
    regValue = hwCfgRead32(devIndex, F2_PCI_CMD);
    regValue |= (MEM_ACCESS_ENABLE | MASTER_ENABLE | SYSTEMERROR_ENABLE); 
	regValue &= ~MEM_WRITE_INVALIDATE; // Disable write & invalidate for our device
    hwCfgWrite32(devIndex, F2_PCI_CMD, regValue);

    regValue = hwCfgRead32(devIndex, F2_PCI_CACHELINESIZE);
    regValue = (regValue & 0xffff) | (0x40 << 8) | 0x08;
	hwCfgWrite32(devIndex, F2_PCI_CACHELINESIZE, regValue);
            

    return A_OK;
}

/**************************************************************************
* deviceCleanup - performs any memory cleanup needed for a device
*
* Perform any cleanup needed for a device.  This includes deleting any 
* memory allocated by a device, and unregistering the card with the driver
*
* RETURNS: 1 if successful, 0 if not
*/
void deviceCleanup
    (
    A_UINT16 devIndex
    )
{
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo =globDrvInfo.pDevInfoArray[devIndex] ;
	
	// disable the interrupts from the hardware
    hwMemWrite32(devIndex, pdevInfo->pdkInfo->aregPhyAddr[0] + F2_IER, F2_IER_DISABLE);

    // place the hardware into reset to quiesce all transfers
    hwMemWrite32(devIndex, pdevInfo->pdkInfo->aregPhyAddr[0] + F2_RC, (F2_RC_PCI | F2_RC_F2 | F2_RC_D2 | F2_RC_DMA | F2_RC_PCU) );
	
	munmap((void *)pdevInfo->pdkInfo->regVirAddr, pdevInfo->pdkInfo->regMapRange);
	munmap((void *)pdevInfo->pdkInfo->memVirAddr, pdevInfo->pdkInfo->memSize);

	close(pdevInfo->handle);

	/* free the DK_DEV_INFO struct */
    A_FREE(pdevInfo->pdkInfo);
    A_FREE(pdevInfo);

	globDrvInfo.pDevInfoArray[devIndex] = NULL;
	globDrvInfo.devCount--;
}

/**************************************************************************
* driverOpen - device opened in device_init
* 
* RETURNS: 1 
*/
A_UINT16 driverOpen()
{
    return 1;
}

/**************************************************************************
* checkRegSpace - Check to see if an address sits in the setup register space
* 
* This internal routine checks to see if an address lies in the register space
*
* RETURNS: A_OK to signify a valid address or A_ENOENT
*/
A_STATUS checkRegSpace
	(
	MDK_WLAN_DEV_INFO *pdevInfo,
	A_UINT32      address
	)
{
	
	if((address >= pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select]) && 
		(address < pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select] + pdevInfo->pdkInfo->aregRange[pdevInfo->pdkInfo->bar_select])) 
		return A_OK;
	else 
		return A_ENOENT;
}
 
/**************************************************************************
* checkMemSpace - Check to see if an address sits in the setup physical memory space
* 
* This internal routine checks to see if an address lies in the physical memory space
*
* RETURNS: A_OK to signify a valid address or A_ENOENT
*/
A_STATUS checkMemSpace
	(
	MDK_WLAN_DEV_INFO *pdevInfo,
	A_UINT32      address
	)
{
	if((address >= (A_UINT32)pdevInfo->pdkInfo->memPhyAddr) &&
            (address < (A_UINT32)((A_UCHAR *)(pdevInfo->pdkInfo->memPhyAddr) + pdevInfo->pdkInfo->memSize)))
		return A_OK;
	else 
		return A_ENOENT;
}

/**************************************************************************
* hwCfgRead32 - read an 32 bit value
*
* This routine will call into the driver
* 32 bit PCI configuration read.
*
* RETURNS: the value read
*/
A_UINT32 hwCfgRead32
(
	A_UINT16 devIndex,
	A_UINT32 offset                    /* the address to read from */
)
{
	struct cfg_op co;
	A_UINT32 data;
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo =globDrvInfo.pDevInfoArray[devIndex] ;

	co.offset=offset;
	co.size = 4;
	co.value = 0;

	if (ioctl(pdevInfo->handle,DK_IOCTL_CFG_READ,&co) < 0) {
		uiPrintf("Error: PCI Config read failed \n");
		data = 0xdeadbeef;
	} else { 
		data = co.value;
	} 
    return data;

}

/**************************************************************************
* hwCfgWrite32 - write a 32 bit value
*
* This routine will call into the driver to activate a
* 32 bit PCI configuration write.
*
* RETURNS: N/A
*/
void hwCfgWrite32
(
	A_UINT16 devIndex,
	A_UINT32  offset,                    /* the address to write */
	A_UINT32  value                        /* value to write */
)
{
	struct cfg_op co;
	A_UINT32 data;
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo =globDrvInfo.pDevInfoArray[devIndex] ;

	co.offset=offset;
	co.size = 4;
	co.value = value;

	if (ioctl(pdevInfo->handle,DK_IOCTL_CFG_WRITE,&co) < 0) {
		uiPrintf("Error: PCI Config write failed \n");
	} 
	
	return;
}

/**************************************************************************
* hwMemRead32 - read an 32 bit value
*
* This routine will call into the simulation environment to activate a
* 32 bit PCI memory read cycle, value read is returned to caller
*
* RETURNS: the value read
*/
A_UINT32 hwMemRead32
(
	A_UINT16 devIndex,
    A_UINT32 address                    /* the address to read from */
)
{
    A_UINT32 *pMem;
    A_UINT16 i;
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo =globDrvInfo.pDevInfoArray[devIndex] ;

    
	// check within the register regions 
	if (A_OK == checkRegSpace(pdevInfo, address))
	{
		pMem = (A_UINT32 *) (pdevInfo->pdkInfo->aregVirAddr[pdevInfo->pdkInfo->bar_select] + (address - pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select]));
		return(*pMem);
	}

	//check within memory allocation
	if(A_OK == checkMemSpace(pdevInfo, address))
	{
		pMem = (A_UINT32 *) (pdevInfo->pdkInfo->memVirAddr + 
						  (address - pdevInfo->pdkInfo->memPhyAddr));
		return(*pMem);
	}
 	uiPrintf("ERROR: hwMemRead32 could not access hardware address: %08lx\n", address);
    return (0xdeadbeef);
    }

/**************************************************************************
* hwMemWrite32 - write a 32 bit value
*
* This routine will call into the simulation environment to activate a
* 32 bit PCI memory write cycle
*
* RETURNS: N/A
*/
void hwMemWrite32
(
	A_UINT16 devIndex,
    A_UINT32 address,                    /* the address to write */
    A_UINT32  value                        /* value to write */
)
    {
    A_UINT32 *pMem;
    A_UINT16 i;
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo =globDrvInfo.pDevInfoArray[devIndex] ;

	// check within the register regions 
	if (A_OK == checkRegSpace(pdevInfo, address))
	{
		pMem = (A_UINT32 *) (pdevInfo->pdkInfo->aregVirAddr[pdevInfo->pdkInfo->bar_select] + (address - pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select]));
		*pMem = value;
		return;
	}

	// check within our malloc area
	if(A_OK == checkMemSpace(pdevInfo, address))
	{
		pMem = (A_UINT32 *) (pdevInfo->pdkInfo->memVirAddr + 
					  (address - pdevInfo->pdkInfo->memPhyAddr));
		*pMem = value;
		return;
	}
   	uiPrintf("ERROR: hwMemWrite32 could not access hardware address: %08lx\n", address);
}

/**************************************************************************
* hwCreateEvent - Handle event creation within windows environment
*
* Create an event within windows environment
*
*
* RETURNS: 0 on success, -1 on error
*/
A_INT16 hwCreateEvent
(
	A_UINT16 devIndex,
    A_UINT32 type, 
    A_UINT32 persistent, 
    A_UINT32 param1,
    A_UINT32 param2,
    A_UINT32 param3,
    EVT_HANDLE eventHandle
)
{
    MDK_WLAN_DEV_INFO *pdevInfo;
	A_BOOL status; 
	struct event_op event;

	    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

		event.valid = 1;
		event.param[0] = type;
		event.param[1] = persistent;
		event.param[2] = param1;
		event.param[3] = param2;
		event.param[4] = param3;
		event.param[5] = (eventHandle.f2Handle << 16) | eventHandle.eventID;

		if (ioctl(pdevInfo->handle,DK_IOCTL_CREATE_EVENT,&event) < 0) {
			uiPrintf("Error:Create Event failed \n");
			return -1;
		}

	   return(0);
}

A_UINT16 getNextEvent
(
	A_UINT16 devIndex,
	EVENT_STRUCT *pEvent
)
{
	MDK_WLAN_DEV_INFO *pdevInfo;
	A_BOOL status; 
	struct event_op event;
	A_INT32 i;

	    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

		event.valid = 0;
		pEvent->type = 0;

		if (ioctl(pdevInfo->handle,DK_IOCTL_GET_NEXT_EVENT,&event) < 0) {
			uiPrintf("Error:Get next Event failed \n");
			return (A_UINT16)-1;
		}

		if (!event.valid) {
			return 0;
		}

		pEvent->type = event.param[0];
		pEvent->persistent = event.param[1];
		pEvent->param1 = event.param[2];
		pEvent->param2 = event.param[3];
		pEvent->param3 = event.param[4];
		pEvent->eventHandle.eventID = event.param[5] & 0xffff;
		pEvent->eventHandle.f2Handle = (event.param[5] >> 16 ) & 0xffff;
		for (i=0;i<6;i++) {
			pEvent->result[i] = event.param[6+i];
		}
		
   		 return(1);
}

/**************************************************************************
* hwMemWriteBlock -  Write a block of memory within the simulation environment
*
* Write a block of memory within the simulation environment
*
*
* RETURNS: 0 on success, -1 on error
*/
A_INT16 hwMemWriteBlock
(
	A_UINT16 devIndex,
    A_UCHAR    *pBuffer,
    A_UINT32 length,
    A_UINT32 *pPhysAddr
)
{
    A_UCHAR *pMem;                /* virtual pointer to area to be written */
    MDK_WLAN_DEV_INFO    *pdevInfo;
    A_UINT16 i;
    A_UINT32 startPhysAddr;        /* physical address of start of device memory block,
                                   for easier readability */

    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
    if(*pPhysAddr == 0)
    {
        return(-1);
    }

    /* first need to check that the phys address is within the allocated memory block.
       Need to make sure that the begin size and endsize match.  Will check all the 
       devices.  Only checking the memory block, will not allow registers to be accessed
       this way
     */
    
			startPhysAddr = pdevInfo->pdkInfo->memPhyAddr;
			if((*pPhysAddr >= startPhysAddr) &&
				(*pPhysAddr <= (startPhysAddr + pdevInfo->pdkInfo->memSize)) &&
				((*pPhysAddr + length) >= startPhysAddr) &&
				((*pPhysAddr + length) <= (startPhysAddr + pdevInfo->pdkInfo->memSize))
				) { 
				/* address is within range, so can do the write */
            
				/* get the virtual pointer to start and read */
				pMem = (A_UINT8 *) (pdevInfo->pdkInfo->memVirAddr + (*pPhysAddr - pdevInfo->pdkInfo->memPhyAddr));
                memcpy(pMem, pBuffer, length);
				return(0);
            }
			// check within the register regions
            startPhysAddr = pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select];
			if ((*pPhysAddr >= startPhysAddr) &&
                (*pPhysAddr < startPhysAddr + pdevInfo->pdkInfo->aregRange[pdevInfo->pdkInfo->bar_select]) &&
                ((*pPhysAddr + length) >= startPhysAddr) &&
                ((*pPhysAddr + length) <= (startPhysAddr + pdevInfo->pdkInfo->aregRange[pdevInfo->pdkInfo->bar_select]))) {
				pMem = (A_UINT8 *) (pdevInfo->pdkInfo->aregVirAddr[pdevInfo->pdkInfo->bar_select] + (*pPhysAddr - pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select]));
                memcpy(pMem, pBuffer, length);
                return(0);
			}
    /* if got to here, then address is bad */
    uiPrintf("Warning: Address is not within legal memory range, nothing written\n");
    return(-1);
    }

/**************************************************************************
* hwMemReadBlock - Read a block of memory within the simulation environment
*
* Read a block of memory within the simulation environment
*
*
* RETURNS: 0 on success, -1 on error
*/
A_INT16 hwMemReadBlock
(
	A_UINT16 devIndex,
    A_UCHAR    *pBuffer,
    A_UINT32 physAddr,
    A_UINT32 length
)
    {
    A_UCHAR *pMem;                /* virtual pointer to area to be written */
    A_UINT16 i;
    A_UINT32 startPhysAddr;        /* physical address of start of device memory block,
                                   for easier readability */
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo =globDrvInfo.pDevInfoArray[devIndex] ;


    /* first need to check that the phys address is within the allocated memory block.
       Need to make sure that the begin size and endsize match.  Will check all the 
       devices.  Only checking the memory block, will not allow registers to be accessed
       this way
     */
			startPhysAddr = pdevInfo->pdkInfo->memPhyAddr;
			if((physAddr >= startPhysAddr) &&
				(physAddr <= (startPhysAddr + pdevInfo->pdkInfo->memSize)) &&
				((physAddr + length) >= startPhysAddr) &&
				((physAddr + length) <= (startPhysAddr + pdevInfo->pdkInfo->memSize))
				) { 
				/* address is within range, so can do the read */
				/* get the virtual pointer to start and read */
				pMem = (A_UINT8 *) (pdevInfo->pdkInfo->memVirAddr + (physAddr - pdevInfo->pdkInfo->memPhyAddr));
				memcpy(pBuffer, pMem, length);
				return(0);
			}
			startPhysAddr = pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select];
			if ((physAddr >= startPhysAddr) &&
                (physAddr < startPhysAddr + pdevInfo->pdkInfo->aregRange[pdevInfo->pdkInfo->bar_select]) &&
                ((physAddr + length) >= startPhysAddr) &&
                ((physAddr + length) <= (startPhysAddr + pdevInfo->pdkInfo->aregRange[pdevInfo->pdkInfo->bar_select]))) {
				pMem = (A_UINT8 *) (pdevInfo->pdkInfo->aregVirAddr[pdevInfo->pdkInfo->bar_select] + (physAddr - pdevInfo->pdkInfo->aregPhyAddr[pdevInfo->pdkInfo->bar_select]));
     		// check within the register regions
                memcpy(pBuffer, pMem, length);
                return(0);
            }
    /* if got to here, then address is bad */
    uiPrintf("Warning: Address is not within legal memory range, nothing read\n");
    return(-1);
}


/**************************************************************************
* uiPrintf - print to the perl console
*
* This routine is the equivalent of printf.  It is used such that logging
* capabilities can be added.
*
* RETURNS: same as printf.  Number of characters printed
*/
A_INT32 uiPrintf
    (
    const char * format,
    ...
    )
{
    va_list argList;
    A_INT32     retval = 0;
    A_UCHAR    buffer[256];

    /*if have logging turned on then can also write to a file if needed */

    /* get the arguement list */
    va_start(argList, format);

    /* using vprintf to perform the printing it is the same is printf, only
     * it takes a va_list or arguments
     */
    retval = vprintf(format, argList);
    fflush(stdout);

    if (logging) {
        vsprintf((A_CHAR *)buffer, format, argList);
        fputs((const A_CHAR *)buffer, logFile);
		fflush(logFile);
    }

    va_end(argList);    /* cleanup arg list */

    return(retval);
}

A_INT32 q_uiPrintf
    (
    const char * format,
    ...
    )
{
    va_list argList;
    A_INT32    retval = 0;
    A_UCHAR    buffer[256];

    if ( !quietMode ) {
        va_start(argList, format);

        retval = vprintf(format, argList);
        fflush(stdout);

        if ( logging ) {
            vsprintf((A_CHAR *)buffer, format, argList);
            fputs((const A_CHAR *)buffer, logFile);
        }

        va_end(argList);    // clean up arg list
    }

    return(retval);
}

/**************************************************************************
* uilog - turn on logging
*
* A user interface command which turns on logging to a fill, all of the
* information printed on screen
*
* RETURNS: 1 if file opened, 0 if not
*/
A_UINT16 uilog
    (
    char *filename,        /* name of file to log to */
	A_BOOL append
    )
{
    /* open file for writing */
    if (append) {
		logFile = fopen(filename, "a+");
	}
	else {
		logFile = fopen(filename, "w");
	}
    if (logFile == NULL) {
        uiPrintf("Unable to open file %s\n", filename);
        return(0);
    }

    /* set flag to say logging enabled */
    logging = 1;

	//turn on logging in the library
	enableLogging(filename);
    return(1);
}

/**************************************************************************
* uiWriteToLog - write a string to the log file
*
* A user interface command which writes a string to the log file
*
* RETURNS: 1 if sucessful, 0 if not
*/
A_UINT16 uiWriteToLog
(
	char *string
)
{
	if(logFile == NULL) {
		uiPrintf("Error, logfile not valid, unable to write to file\n");
		return 0;
	}

	/* write string to file */
	fprintf(logFile, string);
	return 1;
}

void configPrint
(
      A_BOOL flag
)
{
	    enablePrint = flag;
}



/**************************************************************************
* uilogClose - close the logging file
*
* A user interface command which closes an already open log file
*
* RETURNS: 1 if file opened, 0 if not
*/
void uilogClose(void)
{
    if ( logging ) {
        fclose(logFile);
        logging = 0;
    }

    return;
}

/**************************************************************************
* quiet - set quiet mode on or off
*
* A user interface command which turns quiet mode on or off
*
* RETURNS: N/A
*/
void dk_quiet
    (
    A_UINT16 Mode        // 0 for off, 1 for on
    )
{
    quietMode = Mode;
    return;
}

/**************************************************************************
* milliSleep - sleep for the specified number of milliseconds
*
* This routine calls a OS specific routine for sleeping
* 
* RETURNS: N/A
*/

void milliSleep
	(
	A_UINT32 millitime
	)
{
	usleep(millitime*1000);
}

A_UINT32 milliTime
(
 	void
)
{
	struct timeval tv;
	A_UINT32 millisec;

	if (gettimeofday(&tv,NULL) < 0) {
			millisec = 0;
	} else {
		millisec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	}

	return millisec;
}



A_UINT16 hwGetBarSelect(A_UINT16 index) {
            return globDrvInfo.pDevInfoArray[index]->pdkInfo->bar_select;
}

A_UINT16 hwSetBarSelect(A_UINT16 index, A_UINT16 bs) {
   if (minorVersion >= 2) {
       hwCfgWrite32(index, F2_PCI_BAR0_REG + (bs*4), 0xffffffff);
       if (hwCfgRead32(index, F2_PCI_BAR0_REG + (bs*4)) != 0) {
         hwCfgWrite32(index, F2_PCI_BAR0_REG + (bs*4), globDrvInfo.pDevInfoArray[index]->pdkInfo->aregPhyAddr[bs]);
         globDrvInfo.pDevInfoArray[index]->pdkInfo->bar_select = bs;
         return bs;
       }
    }
    globDrvInfo.pDevInfoArray[index]->pdkInfo->bar_select = 0;
    return 0;
}

