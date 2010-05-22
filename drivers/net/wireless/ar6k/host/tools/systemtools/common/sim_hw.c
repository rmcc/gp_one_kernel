
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <fcntl.h>

#include "dk_common.h"
#include "dk_client.h"
#include "dk_mem.h"
#include "ar5210reg.h"
#include "dw16550reg.h"
#include "common_hw.h"
#include "sim.h"

#include "linux_ansi.h"

#ifdef PERL_CORE
#include "dk_master.h"
#endif

extern void        envCleanup(A_BOOL closeDriver);
extern DRV_VERSION_INFO driverVer;

#define DRV_MINOR_VERSION 2

A_UINT32		sock_fd;
A_UINT32		shm_id;
A_UCHAR			*shm_addr=NULL;
A_UINT16 idSelectValues[WLAN_MAX_DEV];
A_UINT32 numDevices;
A_UINT32 devAddrValues[WLAN_MAX_DEV] = 
		{0xa0000000, 0xa4000000, 0xa8000000, 0xac000000, 
		 0xb0000000, 0xb4000000, 0xb8000000, 0xbc000000};
A_UINT32 devAddrValues1[WLAN_MAX_DEV] = 
		{0xc0000000, 0xc4000000, 0xc8000000, 0xcc000000, 
		 0xd0000000, 0xd4000000, 0xd8000000, 0xdc000000};
A_UINT32 devAddrValues2[WLAN_MAX_DEV] = 
		{0xe0000000, 0xe4000000, 0xe8000000, 0xec000000, 
		 0xf0000000, 0xf4000000, 0xf8000000, 0xfc000000};

void 		sig_handler(int arg);

A_STATUS connectSigHandler(void) {
	signal(SIGINT,sig_handler);
    return A_OK;
}

void sig_handler
(
 	int arg
) 
{
		uiPrintf("Received SIGINT !! cleanup and close down ! \n");
#ifdef PERL_CORE
		dkPerlCleanup();
#else
		envCleanup(TRUE);
#endif
		exit(0);
}

/**************************************************************************
* driverOpen - Only needed in NT_HW environment to open comms with driver
*
* Called within the NT_HW environment. Opens and gets a handle to the 
* driver for doing the hardware access. 
*
* RETURNS: 1 if OK and driver open, 0 if not
*/

A_UINT16 driverOpen
(
	void
)
{
	A_UINT32 idSelectMask;
	A_UINT32 n,idSelect;
	A_UINT32        NumBuffBlocks;
	A_UINT32        NumBuffMapBytes;


	if  (PCIsimInitCmd(sock_fd) == -1) {
			return 0;
	} 
	
	NumBuffBlocks	= PCI_SIM_MEM_SIZE / BUFF_BLOCK_SIZE;
	NumBuffMapBytes = NumBuffBlocks / 8;

	// Attached the shared DMA memory to the process
	shm_addr = (A_UCHAR *) shmat(shm_id, NULL, SHM_R | SHM_W);
	if (shm_addr == (void *) -1) {
		perror ("Could not attach shared DMA memory");
		shm_addr = NULL;
		return 0;
	}
	
	idSelectMask = 0;
	numDevices = 0;

	if (PCIsimGetIdsel(&numDevices, &idSelectMask) == -1) {
		shmdt(shm_addr);
        uiPrintf("Error: Unable to get device details \n");
        return 0;
	}

	if (numDevices > WLAN_MAX_DEV) {
		shmdt(shm_addr);
		uiPrintf("Error: too many devices for mdk to support, num devices = %d max supported = %d \n",numDevices,WLAN_MAX_DEV);
        return 0;
	}

	n=0;
	idSelect=0;
	while (idSelectMask) {
		if (idSelectMask & 0x1) {
			idSelectValues[n] = idSelect;
			n++;
		}
		idSelect++;
		idSelectMask = idSelectMask >> 1;
	}
	
    return 1;
}

void close_driver() {
		shmdt(shm_addr);
		
		PCIsimQuit();

//		sleep(10);
}


/**************************************************************************
* hwCfgRead8 - read an 8 bit configuration value
*
* This routine will call into the driver to activate a 8 bit PCI configuration 
* read cycle.
*
* RETURNS: the value read
*/
A_UINT8 hwCfgRead8
(
	A_UINT16 devIndex,
    A_UINT32 address                    /* the address to read from */
)
{
	A_UINT8 data;
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	address = address | (pdevInfo->pdkInfo->device_fn << 8);
	
	PCIsimConfigReadB(pdevInfo->idSelect, address, &data);

	return data;
}

/**************************************************************************
* hwCfgRead16 - read a 16 bit value
*
* This routine will call into the driver to activate a 16 bit PCI configuration 
* read cycle.
*
* RETURNS: the value read
*/
A_UINT16 hwCfgRead16
(
	A_UINT16 devIndex,
    A_UINT32 address                    /* the address to read from */
)
{
	A_UINT16 data;
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
	
	address = address | (pdevInfo->pdkInfo->device_fn << 8);

	PCIsimConfigReadW(pdevInfo->idSelect, address, &data);

	return data;
}

/**************************************************************************
* hwCfgRead32 - read a 32 bit value
*
* This routine will call into the driver to activate a 32 bit PCI configuration 
* read cycle.
*
* RETURNS: the value read
*/
A_UINT32 hwCfgRead32
(
	A_UINT16 devIndex,
    A_UINT32 address                    /* the address to read from */
)
{
	A_UINT32 data;
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	address = address | (pdevInfo->pdkInfo->device_fn << 8);

	PCIsimConfigReadDW(pdevInfo->idSelect, address, &data);

	return data;
}



/**************************************************************************
* hwCfgWrite8 - write an 8 bit value
*
* This routine will call into the simulation environment to activate an
* 8 bit PCI configuration write cycle
*
* RETURNS: N/A
*/
void hwCfgWrite8
(
	A_UINT16 devIndex,
    A_UINT32 address,                    /* the address to write */
    A_UINT8  value                        /* value to write */
)
{
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	address = address | (pdevInfo->pdkInfo->device_fn << 8);

	PCIsimConfigWriteB(pdevInfo->idSelect, address, value);
	return;
}

/**************************************************************************
* hwCfgWrite16 - write a 16 bit value
*
* This routine will call into the simulation environment to activate a
* 16 bit PCI configuration write cycle
*
* RETURNS: N/A
*/
void hwCfgWrite16
(
	A_UINT16 devIndex,
    A_UINT32 address,                    /* the address to write */
    A_UINT16  value                        /* value to write */
)
{
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	address = address | (pdevInfo->pdkInfo->device_fn << 8);

	PCIsimConfigWriteW(pdevInfo->idSelect, address, value);
	return;	
}

/**************************************************************************
* hwCfgWrite32 - write a 32 bit value
*
* This routine will call into the simulation environment to activate a
* 32 bit PCI configuration write cycle
*
* RETURNS: N/A
*/
void hwCfgWrite32
(
	A_UINT16 devIndex,
    A_UINT32 address,                    /* the address to write */
    A_UINT32 value                        /* value to write */
)
{
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	address = address | (pdevInfo->pdkInfo->device_fn << 8);

	PCIsimConfigWriteDW(pdevInfo->idSelect, address, value);
	return;
}

/*
 *  hwFindPciDevice - look for a device on the PCI bus
 * 
 * This routine will perform the necessary algorithm in each environment
 * to find the device on the PCI bus.  Within the sim environment, it
 * does not do very much, it should be there already.  Have a hard coded
 * IDselect value.  Will check that can see vendor ID and device ID.
 *
 *  RETURNS: OK if successful, ERROR otherwise
 */

A_STATUS  findPciDevice
(
	A_UINT16 devIndex,
    MDK_WLAN_DEV_INFO   *pdevInfo
)
{
    A_UINT32    cfgValue;
    A_UINT32      regValue;
	A_UINT16 device_fn;

	device_fn = pdevInfo->pdkInfo->device_fn;

	if ( (devIndex - (device_fn * MDK_MAX_NUM_DEVICES))>= numDevices) {
		return(A_DEVICE_NOT_FOUND);
	}

	pdevInfo->idSelect = idSelectValues[devIndex - (device_fn * MDK_MAX_NUM_DEVICES)];
	uiPrintf("::idselect for this device = %d:device_fn=%d \n",pdevInfo->idSelect, device_fn);

	/* see if this is our device, look for vendorID */
    cfgValue = hwCfgRead32(devIndex, 0);
	if ((cfgValue & 0xffff) != F2_VENDOR_ID) {
		return(A_DEVICE_NOT_FOUND);
	}

	uiPrintf("device fn = %d:devAddressvalues[%d]=%x\n", device_fn, devIndex, devAddrValues[devIndex]);
	/* program the base address */	
    hwCfgWrite32(devIndex, F2_PCI_BAR, devAddrValues[devIndex]); 
	
	hwCfgWrite32(devIndex, F2_PCI_BAR + 4 , 0xffffffff);
	regValue = hwCfgRead32(devIndex, F2_PCI_BAR + 4);
	if (regValue != 0x00000000) {
    	hwCfgWrite32(devIndex, F2_PCI_BAR + 4, devAddrValues1[devIndex]); 
	}
	hwCfgWrite32(devIndex, F2_PCI_BAR + 8 , 0xffffffff);
	regValue = hwCfgRead32(devIndex, F2_PCI_BAR + 8);
	if (regValue != 0x00000000) {
    	hwCfgWrite32(devIndex, F2_PCI_BAR + 8, devAddrValues2[devIndex]); 
	}

	return(A_OK);
}

A_STATUS get_device_client_info(MDK_WLAN_DEV_INFO *pdevInfo, PDRV_VERSION_INFO pDrvVer, PCLI_INFO cliInfo) {

    A_UINT32      mapAddress;
    A_UINT32      regValue;
	A_UINT16 devIndex;

    devIndex = pdevInfo->pdkInfo->devIndex;

	/* search for device on pci bus */
	uiPrintf("Looking for device on PCI bus\n");
    globDrvInfo.pDevInfoArray[devIndex] = pdevInfo;
	// Find the device, functions are part of the device
	if (findPciDevice(devIndex,pdevInfo) != A_OK) {
		globDrvInfo.pDevInfoArray[devIndex] = NULL;
		globDrvInfo.devCount--;
		freeDevInfo(pdevInfo);
		uiPrintf("Unable to find PCI device\n");
        return(A_ERROR);
	} else {
		uiPrintf("Found PCI device!!\n");
	}


	// program address to F2 pci register 
    mapAddress = hwCfgRead32(devIndex, F2_PCI_BAR);
	if ( !mapAddress ) {
		globDrvInfo.pDevInfoArray[devIndex] = NULL;
		globDrvInfo.devCount--;
		freeDevInfo(pdevInfo);
		uiPrintf("Error: Invalid (0) Memory Address in PCI Config Space !\n");
	    return A_ERROR;
	}

    driverVer.minorVersion = DRV_MINOR_VERSION; 

	// virtual address not used in simulation 
	// using the same address
	cliInfo->regVirAddr = mapAddress;

	cliInfo->numBars = 0;

	hwCfgWrite32(devIndex, F2_PCI_BAR , 0xffffffff);
	regValue = hwCfgRead32(devIndex, F2_PCI_BAR);
	hwCfgWrite32(devIndex, F2_PCI_BAR , mapAddress);
	cliInfo->regRange = (~regValue) + 1; 
	cliInfo->aregPhyAddr[cliInfo->numBars] = mapAddress;
	cliInfo->aregVirAddr[cliInfo->numBars] = mapAddress;
	cliInfo->aregRange[cliInfo->numBars] = (~regValue) + 1; 
	cliInfo->numBars++;

    mapAddress = hwCfgRead32(devIndex, F2_PCI_BAR + 4);
	if (mapAddress) {
		cliInfo->aregPhyAddr[cliInfo->numBars] = mapAddress;
		cliInfo->aregVirAddr[cliInfo->numBars] = mapAddress;
		hwCfgWrite32(devIndex, F2_PCI_BAR + 4 , 0xffffffff);
		regValue = hwCfgRead32(devIndex, F2_PCI_BAR+4);
		hwCfgWrite32(devIndex, F2_PCI_BAR + 4, mapAddress);
		cliInfo->aregRange[cliInfo->numBars] = (~regValue) + 1; 
		cliInfo->numBars++;
	}
	
    mapAddress = hwCfgRead32(devIndex, F2_PCI_BAR + 8);
	if (mapAddress) {
		cliInfo->aregPhyAddr[cliInfo->numBars] = mapAddress;
		cliInfo->aregVirAddr[cliInfo->numBars] = mapAddress;
		hwCfgWrite32(devIndex, F2_PCI_BAR + 8 , 0xffffffff);
		regValue = hwCfgRead32(devIndex, F2_PCI_BAR+8);
		hwCfgWrite32(devIndex, F2_PCI_BAR + 8, mapAddress);
		cliInfo->aregRange[cliInfo->numBars] = (~regValue) + 1; 
		cliInfo->numBars++;
	}
	
	cliInfo->memVirAddr = (A_UINT32)shm_addr;
	cliInfo->memSize = PCI_SIM_MEM_SIZE;
	cliInfo->memPhyAddr = PCI_SIM_MEM_PHY_ADDR;
   
}

void close_device(MDK_WLAN_DEV_INFO *pdevInfo) {

}

/**************************************************************************
* hwGetTime - Get the current time
*
* Get current  time
*
* RETURNS: 0 on success, -1 on error
*/
A_INT16 hwGetTime
(
    A_UINT32 *outTime        /* returns time in nanoseconds */
)
{
	A_UINT32 highTime;      
 
	// gets upper 32 bits of time, not sure if will give to user 
	return(PCIsimGetTime((A_UINT32 *)&highTime, (A_UINT32 *)outTime));
}

/**************************************************************************
* hwWaitTime - Delay 
*
* RETURNS: 0 on success, -1 on error
*/
A_INT16 hwWaitTime
(
    A_UINT32 timeToWait
)
{
	return(PCIsimTimeWait(timeToWait));
}


void milliSleep
(   
   A_UINT32 millitime
)       
{
   hwWaitTime(millitime * 1000);
}       

A_UINT32 milliTime
(
     void
)
{
   A_UINT32 nanoTime;

   hwGetTime(&nanoTime);

   return (nanoTime/1000000);
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
	PIPE_CMD *pCmd
)
{
	A_UINT32 evt_handle;
	A_UINT32 event_data[3];
	A_UINT32 type;
	A_UINT32 persistent;
	A_UINT32 idselect;
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	type = pCmd->CMD_U.CREATE_EVENT_CMD.type;
	persistent = pCmd->CMD_U.CREATE_EVENT_CMD.persistent;	
	idselect = pdevInfo->idSelect;

	evt_handle = (pCmd->CMD_U.CREATE_EVENT_CMD.eventHandle.f2Handle << 16) | pCmd->CMD_U.CREATE_EVENT_CMD.eventHandle.eventID;
	event_data[0] = pCmd->CMD_U.CREATE_EVENT_CMD.param1;
	event_data[1] = pCmd->CMD_U.CREATE_EVENT_CMD.param2;
	event_data[2] = pCmd->CMD_U.CREATE_EVENT_CMD.param3;

	return  PCIsimCreateEvent(type, persistent,idselect,evt_handle,event_data);
}

A_UINT16 getNextEvent
(
	MDK_WLAN_DEV_INFO *pdevInfo,
	EVENT_STRUCT *pEvent
)
{
	A_INT32 valid_event;
	A_UINT32 evt_handle;
	A_UINT32 event_data[10];
	A_UINT32 type;
	A_UINT32 persistent;
	A_UINT32 idselect;
	A_INT32 i;

	pEvent->type = 0;
	idselect = pdevInfo->idSelect;

	valid_event = PCIsimGetNextEvent(&type,&persistent,idselect,&evt_handle,event_data);

	if (valid_event == 1) {
		pEvent->type = type;
		pEvent->persistent = persistent;
		pEvent->param1 = event_data[0];
		pEvent->param2 = event_data[1];
		pEvent->param3 = event_data[2];
		pEvent->eventHandle.eventID = evt_handle & 0xffff;
		pEvent->eventHandle.f2Handle = (evt_handle >> 16 ) & 0xffff;
		pEvent->result = event_data[3];
#ifdef MAUI
		for (i=0;i<5;i++) { 
			pEvent->additionalParams[i] = event_data[4+i];
		}
#endif
	}

	return valid_event;
}

A_INT16 hwGetNextEvent
(
	A_UINT16 devIndex,
	void *pBuf
)
{
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
	return getNextEvent(pdevInfo, (EVENT_STRUCT *)pBuf);
}


