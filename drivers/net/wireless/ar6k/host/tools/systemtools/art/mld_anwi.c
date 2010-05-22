/* mld_anwi.c - access hardware for manlib */

/* Copyright (c) 2001 Atheros Communications, Inc., All Rights Reserved */

/*
DESCRIPTION
This module contains the functions to access the hardware for the dk_client
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include <time.h>
#include <process.h>
#include <winioctl.h>
#ifndef LEGACY
#include <initguid.h>
#include <setupapi.h>
#endif

#include "wlantype.h"
#include "mld_anwi.h"
#include "anwiioctl.h"
#include "ar5210reg.h"
#include "athreg.h"
#include "manlib.h"

#ifndef LEGACY
#include "anwi_guid.h"
#endif

anwiVersionInfo anwiVer;
static FILE     *logFile;  /* file handle of logfile */    
static FILE     *yieldLogFile;  /* file handle of yieldLogfile */    
static A_BOOL   logging;   /* set to 1 if a log file open */
static A_BOOL   enablePrint = 1;
static A_BOOL   yieldLogging;   /* set to 1 if a log file open */
static A_BOOL   yieldEnablePrint = 1;
static A_UINT16 quietMode; /* set to 1 for quiet mode, 0 for not */
static A_UINT16 yieldQuietMode; /* set to 1 for quiet mode, 0 for not */
static A_BOOL	driverOpened;


/////////////////////////////////////////////////////////////////////////////
//GLOBAL VARIABLES
#ifdef LEGACY
HANDLE hDriver;
#endif
MDK_WLAN_DRV_INFO	globDrvInfo;				/* Global driver info */

/////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS
A_UINT16    driverOpen();
void        envCleanup(A_BOOL closeDriver);
void        deviceCleanup(A_UINT16 devIndex);
BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType);

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

    // open a handle to the driver 
	// need not open the driver if art is controlling a remote client
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

    if ( !SetConsoleCtrlHandler(consoleCtrlHandler, TRUE) ) {
        uiPrintf("Error: Unable to register CTRL-C handler, got %d from GetLastError()\n", GetLastError());
		return A_ERROR;
    }

    
    return A_OK;
}


BOOL WINAPI consoleCtrlHandler
    (
    DWORD dwCtrlType
    )
{
    char* sType;

    switch ( dwCtrlType ) {
    case CTRL_C_EVENT:
        sType = "CTRL_C_EVENT";
        break;
    case CTRL_BREAK_EVENT:
        sType = "CTRL_BREAK_EVENT";
        break;
    case CTRL_CLOSE_EVENT:
        sType = "CTRL_CLOSE_EVENT";
        break;
    case CTRL_LOGOFF_EVENT:
        sType = "CTRL_LOGOFF_EVENT";
        break;
    default:
        sType = "UNKNOWN event";
    }

    uiPrintf("!!! Received %s.... Cleaning up and closing down !!!\n", sType);
    // MDS: should possibly worry about reentrancy into envCleanup(), but
    // there's only a very small probability of that happening, so we won't
    // worry about it right now....
    envCleanup(TRUE);
    ExitProcess(1);

    return TRUE; // we handled the situation, so we return TRUE
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

    // close the handle to the driver
    if ((closeDriver) && (driverOpened)) {
#ifdef LEGACY
		CloseHandle (hDriver); 
#endif
		driverOpened = 0;
    }
}

#ifdef LEGACY
// This function tries to register ourself with the wdm.
BOOL RegisterClient(int index,pAnwiOutClientInfo pRetCliInfo) 
{ 
	A_BOOL status; 
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	pAnwiVersionInfo pAnwiVer;
	anwiInClientInfo cliInInfo;
	pAnwiOutClientInfo pCliOutInfo;
	ULONG extraLen;

		status = DeviceIoControl  (hDriver,  
						IOCTL_ANWI_GET_VERSION,  
						NULL,  
						0,
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by get_version DeviceIoControl call \n");
			return FALSE;
		}

		if (returnContext.contextLen != sizeof(anwiVersionInfo)) {
			printf("Return size (%d) from get version DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiVersionInfo));
			return FALSE;
		}

		pAnwiVer = (pAnwiVersionInfo) returnContext.context;

		if (pAnwiVer->majorVersion != ANWI_MAJOR_VERSION) {
			printf("Driver Version doesnt match with the client version \n");
			return FALSE;
		}

		printf("ANWI %d.%d \n",pAnwiVer->majorVersion,pAnwiVer->minorVersion);
		memcpy(&anwiVer, pAnwiVer, sizeof(anwiVersionInfo));
		

		cliInInfo.index = index;
		cliInInfo.baseAddress = 0;
		cliInInfo.irqLevel = 0;

		status = DeviceIoControl  (hDriver,  
						IOCTL_ANWI_DEVICE_OPEN,
						&cliInInfo,  
						sizeof(anwiInClientInfo),
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by device open DeviceIoControl call \n");
			return FALSE;
		}

		extraLen = 0;
		
		if (anwiVer.minorVersion == 0) {
			extraLen = sizeof(pCliOutInfo->regRange) + sizeof(pCliOutInfo->memSize) + sizeof(aregPhyAddr) + sizeof(aregVirAddr) + sizeof(aregRange);
		} 
		if (anwiVer.minorVersion == 1) {
			extraLen = sizeof(aregPhyAddr) + sizeof(aregVirAddr) + sizeof(aregRange);
		} 

		if (returnContext.contextLen != (sizeof(anwiOutClientInfo) - extraLen)) {
			printf("Return size (%d) from device open DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiOutClientInfo));
			return -1;
		}

		pCliOutInfo = (pAnwiOutClientInfo) returnContext.context;
		memcpy(pRetCliInfo, pCliOutInfo, (sizeof(anwiOutClientInfo) - extraLen));

		if (anwiVer.minorVersion == 0) {
			pRetCliInfo->regRange = 65536; 
			pRetCliInfo->memSize = 1024 * 1024; // 1 MB 
		}

		return TRUE;
}

A_BOOL unRegisterClient(int cliId) 
{ 
	A_BOOL status; 
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	anwiUnRegisterClientInfo unRegInfo;

		unRegInfo.cliId = cliId;

		status = DeviceIoControl  (hDriver,  
						IOCTL_ANWI_DEVICE_CLOSE,  
						&unRegInfo,  
						sizeof(anwiUnRegisterClientInfo),
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by device close DeviceIoControl call \n");
			return FALSE;
		}

	return TRUE;
}
#else
HANDLE getDeviceViaInterface(GUID *pGuid, DWORD instance)
{
	HDEVINFO info;
	SP_INTERFACE_DEVICE_DATA ifdata;
	DWORD ReqLen;
	PSP_INTERFACE_DEVICE_DETAIL_DATA ifDetail;
	HANDLE handle;


	info = SetupDiGetClassDevs(pGuid,NULL,NULL,DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

	if (info == INVALID_HANDLE_VALUE) {
		printf("No HDEVINFO available for this GUID \n");
		return  INVALID_HANDLE_VALUE;
		
	}

	ifdata.cbSize = sizeof(ifdata);

	if (!SetupDiEnumDeviceInterfaces(info,NULL,pGuid, instance, &ifdata)) {
		printf("No SP_INTERFACE_DEVICE_DATA availabe for this GUID instance \n");
		SetupDiDestroyDeviceInfoList(info);
		return  INVALID_HANDLE_VALUE;
	}

	SetupDiGetDeviceInterfaceDetail(info,&ifdata,NULL,0,&ReqLen,NULL);

	ifDetail = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(ReqLen*sizeof(char));
	if (ifDetail == NULL) {
		printf("cannot allocate memory\n");
		SetupDiDestroyDeviceInfoList(info);
		return  INVALID_HANDLE_VALUE;
	}

	// Get symbolic link name 
	ifDetail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
	if (!SetupDiGetDeviceInterfaceDetail(info,&ifdata,ifDetail,ReqLen,NULL,NULL)) {
			printf("No PSP_INTERFACE_DEVICE_DETAIL_DATA availabe for this GUID instance \n");
			A_FREE(ifDetail);
			SetupDiDestroyDeviceInfoList(info);
			return  INVALID_HANDLE_VALUE;
	}

//	printf("Symbolic name is %s \n",ifDetail->DevicePath);

	handle = CreateFile(ifDetail->DevicePath, 
						GENERIC_READ | GENERIC_WRITE,
						0,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL);
						
	A_FREE(ifDetail);
	SetupDiDestroyDeviceInfoList(info);

	return  handle;
}

int getClientInfo(HANDLE hDevice,pAnwiOutClientInfo pRetCliInfo) 
{ 
	BOOL status; 
	ULONG bR = 0; 
	anwiReturnContext returnContext; 
	pAnwiVersionInfo pAnwiVer;
	anwiInClientInfo cliInInfo;
	pAnwiOutClientInfo pCliOutInfo;
	ULONG extraLen;
    ULONG iIndex;

		status = DeviceIoControl  (hDevice,  
						IOCTL_ANWI_GET_VERSION,  
						NULL,  
						0,
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by get_version DeviceIoControl call \n");
			return -1;
		}

		if (returnContext.contextLen != sizeof(anwiVersionInfo)) {
			printf("Return size (%d) from get version DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiVersionInfo));
			return -1;
		}

		pAnwiVer = (pAnwiVersionInfo) returnContext.context;

		if (pAnwiVer->majorVersion != ANWI_MAJOR_VERSION) {
			printf("Driver Version doesnt match with the client version \n");
			return -1;
		}
		
		printf("::ANWI %d.%d \n",pAnwiVer->majorVersion,pAnwiVer->minorVersion);
		memcpy(&anwiVer, pAnwiVer, sizeof(anwiVersionInfo));

		cliInInfo.baseAddress = 0;
		cliInInfo.irqLevel = 0;

		status = DeviceIoControl  (hDevice,  
						IOCTL_ANWI_GET_CLIENT_INFO,
						&cliInInfo,  
						sizeof(anwiInClientInfo),
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by device open DeviceIoControl call \n");
			return -1;
		}

		// backward compatible with 1.0 driver
		extraLen = 0;
		
		if (anwiVer.minorVersion == 0) {
			extraLen = sizeof(pCliOutInfo->regRange) + sizeof(pCliOutInfo->memSize) + sizeof(pCliOutInfo->aregPhyAddr) + sizeof(pCliOutInfo->aregVirAddr) + sizeof(pCliOutInfo->aregRange) + sizeof(pCliOutInfo->numBars);
		} 
printf("SNOOP: extraLen = %d\n",extraLen);

		if (anwiVer.minorVersion == 1) {
			extraLen = sizeof(pCliOutInfo->aregPhyAddr) + sizeof(pCliOutInfo->aregVirAddr) + sizeof(pCliOutInfo->aregRange)+ sizeof(pCliOutInfo->numBars);
		} 


		if (returnContext.contextLen != (sizeof(anwiOutClientInfo) - extraLen)) {
			printf("Return size (%d) from device open DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiOutClientInfo));
			return -1;
		}

		pCliOutInfo = (pAnwiOutClientInfo) returnContext.context;
		memcpy(pRetCliInfo, pCliOutInfo, (sizeof(anwiOutClientInfo) - extraLen));

		if (anwiVer.minorVersion == 0) {
			pRetCliInfo->regRange = 65536; 
			pRetCliInfo->memSize = 1024 * 1024; // 1 MB 
		}

#ifndef CUSTOMER_REL
		printf("Mem phy addr = %x vir addr = %x \n",pCliOutInfo->memPhyAddr,pCliOutInfo->memVirAddr);
        if (anwiVer.minorVersion >= 2) {
            for(iIndex=0; iIndex<pCliOutInfo->numBars; iIndex++) {
               q_uiPrintf("Bar Reg %d phy addr = %x vir addr = %x \n", iIndex, pCliOutInfo->aregPhyAddr[iIndex],pCliOutInfo->aregVirAddr[iIndex]);
            }
         }
         else {
		    printf("Reg phy addr = %x vir addr = %x \n",pCliOutInfo->regPhyAddr,pCliOutInfo->regVirAddr);
         }
#endif

		return 0;
}

#endif // LEGACY

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
	anwiOutClientInfo cliInfo;
    A_UINT32 iIndex;
#ifndef LEGACY
	HANDLE hDevice;
#endif

    /* check to see if we already have a devInfo structure created for this device */
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

#ifdef LEGACY
	if (!RegisterClient(devIndex,&cliInfo)) {
		uiPrintf("Error: Unable to register client with the driver \n");
		A_FREE(pdevInfo->pdkInfo);
		A_FREE(pdevInfo);
		return A_ERROR;
	}
	pdevInfo->cliId = cliInfo.cliId;
#else
	hDevice = getDeviceViaInterface((LPGUID)&ANWI_GUID,devIndex);
	if (hDevice == INVALID_HANDLE_VALUE) { 
		uiPrintf("Error: Failed to obtain file handle to device \n"); 
		return A_ERROR; 
	} 

	if (getClientInfo(hDevice,&cliInfo) < 0) {
		uiPrintf("Unable to get client info \n"); 
		CloseHandle(hDevice); 
		return A_ERROR; 
	} 
	pdevInfo->hDevice = hDevice;
#endif
	pdevInfo->pdkInfo->f2Mapped = 1;
	pdevInfo->pdkInfo->devIndex = devIndex;
    if (anwiVer.minorVersion >= 2) {
       pdevInfo->pdkInfo->f2MapAddress = cliInfo.aregPhyAddr[0];
       pdevInfo->pdkInfo->regMapRange = cliInfo.aregRange[0];
       pdevInfo->pdkInfo->regVirAddr = cliInfo.aregVirAddr[0];
       for(iIndex=0; iIndex<cliInfo.numBars; iIndex++) {
          pdevInfo->pdkInfo->aregPhyAddr[iIndex] = cliInfo.aregPhyAddr[iIndex];
          pdevInfo->pdkInfo->aregVirAddr[iIndex] = cliInfo.aregVirAddr[iIndex];
          pdevInfo->pdkInfo->aregRange[iIndex] = cliInfo.aregRange[iIndex];
       }
    }
    else {
         pdevInfo->pdkInfo->aregPhyAddr[0] = cliInfo.regPhyAddr;
         pdevInfo->pdkInfo->aregVirAddr[0] = cliInfo.regVirAddr;
         pdevInfo->pdkInfo->aregRange[0] = cliInfo.regRange;
         pdevInfo->pdkInfo->f2MapAddress = cliInfo.regPhyAddr;
         pdevInfo->pdkInfo->regMapRange = cliInfo.regRange;
         pdevInfo->pdkInfo->regVirAddr = cliInfo.regVirAddr;
    }
    pdevInfo->pdkInfo->numBars = cliInfo.numBars;

	pdevInfo->pdkInfo->memPhyAddr = cliInfo.memPhyAddr;
	pdevInfo->pdkInfo->memVirAddr = cliInfo.memVirAddr;
	pdevInfo->pdkInfo->memSize = cliInfo.memSize;
	pdevInfo->pdkInfo->bar_select = 0;
    
    q_uiPrintf("+ Allocated memory in the driver.\n");
    q_uiPrintf("+ VirtAddress = %08x\n", (A_UINT32)(pdevInfo->pdkInfo->memPhyAddr));
    q_uiPrintf("+ PhysAddress = %08x\n", (A_UINT32)(pdevInfo->pdkInfo->memVirAddr));

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
    A_UINT16 tmp_bar_select;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
	
    tmp_bar_select = pdevInfo->pdkInfo->bar_select;
	// disable the interrupts from the hardware
    pdevInfo->pdkInfo->bar_select = 0;
    hwMemWrite32(devIndex, pdevInfo->pdkInfo->aregPhyAddr[0] + F2_IER, F2_IER_DISABLE);

    // place the hardware into reset to quiesce all transfers
    hwMemWrite32(devIndex, pdevInfo->pdkInfo->aregPhyAddr[0] + F2_RC, (F2_RC_PCI | F2_RC_F2 | F2_RC_D2 | 
        F2_RC_DMA | F2_RC_PCU) );
    pdevInfo->pdkInfo->bar_select = tmp_bar_select;

#ifdef LEGACY
	unRegisterClient(pdevInfo->cliId); 
#else
	CloseHandle(pdevInfo->hDevice);
#endif
	
	/* free the DK_DEV_INFO struct */
    A_FREE(pdevInfo->pdkInfo);
    A_FREE(pdevInfo);

	globDrvInfo.pDevInfoArray[devIndex] = NULL;
    globDrvInfo.devCount--;

}

/**************************************************************************
* driverOpen - Only needed in NT_HW environment to open comms with driver
*
* Called within the NT_HW environment. Opens and gets a handle to the 
* driver for doing the hardware access. 
*
* RETURNS: 1 if OK and driver open, 0 if not
*/
A_UINT16 driverOpen()
{
	// Opening a handle to the device. 
#ifdef LEGACY
	hDriver = CreateFile ("\\\\.\\Anwiwdm", 
	                      GENERIC_READ | GENERIC_WRITE, 
	                      0, 
	                      NULL, 
	                      OPEN_EXISTING, 
	                      FILE_ATTRIBUTE_NORMAL, 
	                      NULL ); 
 
	if (hDriver == INVALID_HANDLE_VALUE) { 
		printf("Failed to obtain file handle to device: %d", GetLastError() ); 
		return 0;
	} 
#endif

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
        A_UINT32 iIndex;

        if (anwiVer.minorVersion >= 2) {
            for(iIndex=0; iIndex<pdevInfo->pdkInfo->numBars; iIndex++) {
               if((address >= pdevInfo->pdkInfo->aregPhyAddr[iIndex]) && 
                   (address < pdevInfo->pdkInfo->aregPhyAddr[iIndex] + pdevInfo->pdkInfo->aregRange[iIndex])) 
                   return A_OK;
             }
         }
         else {
            if((address >= pdevInfo->pdkInfo->aregPhyAddr[0]) && (address < pdevInfo->pdkInfo->aregPhyAddr[0] + pdevInfo->pdkInfo->regMapRange)) 
            return A_OK;
         }
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
	A_BOOL status; 
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	anwiDevOpStruct inStruct;
	pAnwiDevOpStruct pOutStruct;
	A_UINT32 data;
	HANDLE hnd;
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
#ifdef LEGACY
		inStruct.cliId = pdevInfo->cliId;
#endif
		inStruct.opType = ANWI_CFG_READ;
		inStruct.param1 = offset;
#ifdef LEGACY
		inStruct.param2 = 0;
#else
		inStruct.param2 = 4;
		inStruct.param3 = 0;
#endif

#ifdef LEGACY
		hnd = hDriver;
#else
		hnd = pdevInfo->hDevice;
#endif

		status = DeviceIoControl  (hnd,  
						IOCTL_ANWI_DEV_OP,  
						&inStruct,  
						sizeof(anwiDevOpStruct),
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by DEV_OP DeviceIoControl call \n");
			return 0xdeadbeef;
		}

		if (returnContext.contextLen != sizeof(anwiDevOpStruct)) {
			printf("Return size (%d) from DEV_OP DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiDevOpStruct));
			return 0xdeadbeef;
		}

		pOutStruct = (pAnwiDevOpStruct) returnContext.context;
		data = pOutStruct->param1;
//		printf("Cfg read at %x : %x \n",offset,data);
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
	A_BOOL status; 
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	anwiDevOpStruct inStruct;
	HANDLE hnd;
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

#ifdef LEGACY
		inStruct.cliId = pdevInfo->cliId;
#endif
		inStruct.opType = ANWI_CFG_WRITE;
		inStruct.param1 = offset;
#ifdef LEGACY
		inStruct.param2 = value;
#else
		inStruct.param2 = 4;
		inStruct.param3 = value;
#endif

#ifdef LEGACY
		hnd = hDriver;
#else
		hnd = pdevInfo->hDevice;
#endif

		status = DeviceIoControl  (hnd,  
						IOCTL_ANWI_DEV_OP,  
						&inStruct,  
						sizeof(anwiDevOpStruct),
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by DEV_OP DeviceIoControl call \n");
			return;
		}

		if (returnContext.contextLen != sizeof(anwiDevOpStruct)) {
			printf("Return size (%d) from DEV_OP DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiDevOpStruct));
			return;
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
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
    
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
	MDK_WLAN_DEV_INFO    *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
    
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
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	anwiEventOpStruct event;
	HANDLE hnd;

	    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

#ifdef LEGACY
		event.cliId = pdevInfo->cliId;
#endif
		event.opType = ANWI_CREATE_EVENT;
		event.valid = 1;
		event.param[0] = type;
		event.param[1] = persistent;
		event.param[2] = param1;
		event.param[3] = param2;
		event.param[4] = param3;
		event.param[5] = (eventHandle.f2Handle << 16) | eventHandle.eventID;

#ifdef LEGACY
		hnd = hDriver;
#else
		hnd = pdevInfo->hDevice;
#endif

		status = DeviceIoControl  (hnd,  
						IOCTL_ANWI_EVENT_OP,  
						&event,  
						sizeof(event),
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by event op DeviceIoControl call \n");
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
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	anwiEventOpStruct inEvent;
	pAnwiEventOpStruct outEvent;
	A_UINT32 i;
	HANDLE hnd;

	    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

#ifdef LEGACY
		inEvent.cliId = pdevInfo->cliId;
#endif
		inEvent.opType = ANWI_GET_NEXT_EVENT;
		inEvent.valid = 0;

#ifdef LEGACY
		hnd = hDriver;
#else
		hnd = pdevInfo->hDevice;
#endif

		status = DeviceIoControl  (hnd,
						IOCTL_ANWI_EVENT_OP,  
						&inEvent,  
						sizeof(anwiEventOpStruct),
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by event op DeviceIoControl call \n");
			return 0;
		}

	   	if (returnContext.contextLen != sizeof(anwiEventOpStruct)) {
			printf("Return size (%d) from event op DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiEventOpStruct));
			return 0;
		}

		outEvent = (pAnwiEventOpStruct) returnContext.context;
		
		if (!outEvent->valid) {
			return 0;
		}

		pEvent->type = outEvent->param[0];
		pEvent->persistent = outEvent->param[1];
		pEvent->param1 = outEvent->param[2];
		pEvent->param2 = outEvent->param[3];
		pEvent->param3 = outEvent->param[4];
		pEvent->eventHandle.eventID = outEvent->param[5] & 0xffff;
		pEvent->eventHandle.f2Handle = (outEvent->param[5] >> 16 ) & 0xffff;
		for (i=0;i<6;i++) {
			pEvent->result[i] = outEvent->param[6+i];
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
    
			//check start and end addresswithin memory allocation
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
    MDK_WLAN_DEV_INFO    *pdevInfo;
    A_UINT32 startPhysAddr;        /* physical address of start of device memory block,
                                   for easier readability */
	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

    /* first need to check that the phys address is within the allocated memory block.
       Need to make sure that the begin size and endsize match.  Will check all the 
       devices.  Only checking the memory block, will not allow registers to be accessed
       this way
     */
			//check start and end addresswithin memory allocation
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
    A_UCHAR    buffer[1024];

    /*if have logging turned on then can also write to a file if needed */

    /* get the arguement list */
    va_start(argList, format);

    /* using vprintf to perform the printing it is the same is printf, only
     * it takes a va_list or arguments
     */
    retval = vprintf(format, argList);
    fflush(stdout);

    if (logging) {
        vsprintf(buffer, format, argList);
        fputs(buffer, logFile);
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
            vsprintf(buffer, format, argList);
            fputs(buffer, logFile);
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
	Sleep((DWORD) millitime);
}

A_UINT16 hwGetBarSelect(A_UINT16 devIndex) {
    return globDrvInfo.pDevInfoArray[devIndex]->pdkInfo->bar_select;
}

A_UINT16 hwSetBarSelect(A_UINT16 devIndex, A_UINT16 bs) {
    if (anwiVer.minorVersion >= 2) { 
            hwCfgWrite32(devIndex, F2_PCI_BAR0_REG + (bs*4), 0xffffffff);
            if (hwCfgRead32(devIndex, F2_PCI_BAR0_REG + (bs*4)) != 0) {
                 globDrvInfo.pDevInfoArray[devIndex]->pdkInfo->bar_select = bs;
                 return 1;
             }
     }
     globDrvInfo.pDevInfoArray[devIndex]->pdkInfo->bar_select = 0;
     return 0;
}


/**************************************************************************
* uiOpenYieldLog - open the yield log file.
*
* A user interface command which turns on logging to the yield log file
*
* RETURNS: 1 if file opened, 0 if not
*/
A_UINT16 uiOpenYieldLog
(
    char *filename,        /* name of file to log to */
	A_BOOL append
)
{
    /* open file for writing */
    if (append) {
		yieldLogFile = fopen(filename, "a+");
	}
	else {
		yieldLogFile = fopen(filename, "w");
	}
    if (yieldLogFile == NULL) {
        uiPrintf("Unable to open file %s\n", filename);
        return(0);
    } else {
		uiPrintf("Opened file %s for yieldLog\n", filename);
	}

    /* set flag to say yield logging enabled */
    yieldLogging = 1;

    return(1);
}

/**************************************************************************
* uiYieldLog - write a string to the yield log file
*
* A user interface command which writes a string to the log file
*
* RETURNS: 1 if sucessful, 0 if not
*/
A_UINT16 uiYieldLog
(
	char *string
)
{
	if (yieldLogging > 0) {
		if(yieldLogFile == NULL) {
			uiPrintf("Error, logfile not valid, unable to write to file\n");
			return 0;
		}
	}
	/* write string to file */
	fprintf(yieldLogFile, string);

	fflush(yieldLogFile);
	return 1;
}

/**************************************************************************
* uiCloseYieldLog - close the yield logging file
*
* A user interface command which closes an already open log file
*
* RETURNS: void
*/
void uiCloseYieldLog(void)
{
    if ( yieldLogging) {
        fclose(yieldLogFile);
        yieldLogging = 0;
    }

    return;
}
