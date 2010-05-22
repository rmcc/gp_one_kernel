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
#include "common_hw.h"
#include "anwiioctl.h"
#include "athreg.h"
#include "manlib.h"

#ifndef LEGACY
#include "anwi_guid.h"
//#include "athusr.h"
//no need for this header when compile win_sdio app
#endif

#ifdef PERL_CORE
#include "dk_master.h"
#endif

#ifdef ART_BUILD
#include "art_if.h"
extern A_UINT32 glbl_devNum;
#endif


#ifdef LEGACY
HANDLE hDriver;
#endif
#define ANWI_MAJOR_VERSION  1
#define ANWI_MINOR_VERSION  2


extern DRV_VERSION_INFO driverVer;
volatile A_BOOL inSignalHandler = FALSE;
BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType);

#ifdef LEGACY
// This function tries to register ourself with the wdm.
BOOL RegisterClient(int index,pAnwiOutClientInfo pRetCliInfo) 
{ 
	A_BOOL status; 
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	PDRV_VERSION_INFO pDrvVer;
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

		if (returnContext.contextLen != sizeof(DRV_VERSION_INFO)) {
			printf("Return size (%d) from get version DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(DRV_VERSION_INFO));
			return FALSE;
		}

		pDrvVer = (PDRV_VERSION_INFO) returnContext.context;

		if (pDrvVer->majorVersion != ANWI_MAJOR_VERSION) {
			printf("Driver Version doesnt match with the client version \n");
			return FALSE;
		}

		printf("ANWI %d.%d \n",pDrvVer->majorVersion,pDrvVer->minorVersion);
		memcpy(&driverVer, pDrvVer, sizeof(DRV_VERSION_INFO));
		

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
		
		if (driverVer.minorVersion == 0) {
			extraLen = sizeof(pCliOutInfo->regRange) + sizeof(pCliOutInfo->memSize) + sizeof(aregPhyAddr) + sizeof(aregVirAddr) + sizeof(aregRange) + sizeof(res_type);
		} 
		if (driverVer.minorVersion == 1) {
			extraLen = sizeof(aregPhyAddr) + sizeof(aregVirAddr) + sizeof(aregRange) + sizeof(res_type);
		} 

		if (returnContext.contextLen != (sizeof(anwiOutClientInfo) - extraLen)) {
			printf("Return size (%d) from device open DeviceIoControl call doesnt match the expected size (%d) ...\n",returnContext.contextLen,sizeof(anwiOutClientInfo));
			return -1;
		}

		pCliOutInfo = (pAnwiOutClientInfo) returnContext.context;
		memcpy(pRetCliInfo, pCliOutInfo, (sizeof(anwiOutClientInfo) - extraLen));

		if (driverVer.minorVersion == 0) {
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


HANDLE getDeviceViaInterface(GUID *pGuid, DWORD instance, char *pipeName)
{
	HDEVINFO info;
	SP_INTERFACE_DEVICE_DATA ifdata;
	DWORD ReqLen;
	PSP_INTERFACE_DEVICE_DETAIL_DATA ifDetail=NULL;
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
			free(ifDetail);
			SetupDiDestroyDeviceInfoList(info);
			return  INVALID_HANDLE_VALUE;
	}


	if (pipeName != NULL) {
		strcat(ifDetail->DevicePath, pipeName);
	}

	handle = CreateFile(ifDetail->DevicePath, 
						GENERIC_READ | GENERIC_WRITE,
						0,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL);
						
// Comment out for now, this free causes abort in debug version
//	free(ifDetail);
	SetupDiDestroyDeviceInfoList(info);

	return  handle;
}


A_STATUS get_version_info(HANDLE hDevice, PDRV_VERSION_INFO pDrvVer) {
        A_STATUS status;
	    anwiReturnContext returnContext; 
        PDRV_VERSION_INFO tmpDrvVer;
	    ULONG bR = 0; 

		status = (A_STATUS) DeviceIoControl  (hDevice,  
						IOCTL_ANWI_GET_VERSION,  
						NULL,  
						0,
						&returnContext, 
						sizeof(anwiReturnContext),  
						&bR,  
						NULL); 

	
		if ((!status) || (returnContext.returnCode != ANWI_OK)) {
			printf("Error returned by get_version DeviceIoControl call \n");
			return (A_STATUS) -1;
		}

		if (returnContext.contextLen != sizeof(DRV_VERSION_INFO)) {
			printf("Return size (%d) from get version DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(DRV_VERSION_INFO));
			return (A_STATUS) -1;
		}

		tmpDrvVer = (PDRV_VERSION_INFO) returnContext.context;
		pDrvVer->majorVersion = tmpDrvVer->majorVersion;
		pDrvVer->minorVersion = tmpDrvVer->minorVersion;

		if (pDrvVer->majorVersion != ANWI_MAJOR_VERSION) {
			printf("Driver Version doesnt match with the client version \n");
			return (A_STATUS) -1;
		}
		
		printf("::ANWI %d.%d \n",pDrvVer->majorVersion,pDrvVer->minorVersion);
    return A_OK;
}

HANDLE open_device(A_UINT32 device_fn, A_UINT32 devIndex, char *pipeName) {
   HANDLE       hDevice = INVALID_HANDLE_VALUE;

   switch(device_fn) {
	case UART_FUNCTION: {
		uiPrintf(".......... UART function \n");
	    hDevice = getDeviceViaInterface((LPGUID)&ANWI_UART_GUID, (devIndex - (device_fn * MDK_MAX_NUM_DEVICES)), pipeName);
        break;
	}
    case WMAC_FUNCTION: {
		uiPrintf(".......... WMAC function \n");
	    hDevice = getDeviceViaInterface((LPGUID)&ANWI_GUID,devIndex, pipeName);
        break;
	}
    case USB_FUNCTION: {
		uiPrintf(".......... USB function \n");
	    //hDevice = getDeviceViaInterface((LPGUID)&GUID_CLASS_ATH_USB,devIndex, pipeName);
            //no need for this func when win_sdio app   
        break;
	}
   }
   if (hDevice == INVALID_HANDLE_VALUE) { 
printf("SNOOP: invalid handle, trying for wmac\n");
	    //may be condor, try this.
	    hDevice = getDeviceViaInterface((LPGUID)&ANWI_GUID,devIndex, pipeName);
	    if (hDevice == INVALID_HANDLE_VALUE) { 
	    	uiPrintf("Error: Failed to obtain file handle to device again\n"); 
		}
   } 
   return hDevice;
}

int getClientInfo(HANDLE hDevice,pAnwiOutClientInfo pRetCliInfo) 
{ 
	BOOL status; 
	ULONG bR = 0; 
	anwiReturnContext returnContext; 
	DRV_VERSION_INFO DrvVer;
	anwiInClientInfo cliInInfo;
	pAnwiOutClientInfo pCliOutInfo;
	ULONG extraLen;
    ULONG iIndex;

        if ((status=get_version_info(hDevice, &DrvVer)) != A_OK) {
            return status;
        }
		memcpy(&driverVer, &DrvVer, sizeof(DRV_VERSION_INFO));

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
		
		if (driverVer.minorVersion == 0) {
			extraLen = sizeof(pCliOutInfo->regRange) + sizeof(pCliOutInfo->memSize) + sizeof(pCliOutInfo->aregPhyAddr) + sizeof(pCliOutInfo->aregVirAddr) + sizeof(pCliOutInfo->aregRange) + sizeof(pCliOutInfo->numBars) + sizeof(pCliOutInfo->res_type);
		} 

		if (driverVer.minorVersion == 1) {
			extraLen = sizeof(pCliOutInfo->aregPhyAddr) + sizeof(pCliOutInfo->aregVirAddr) + sizeof(pCliOutInfo->aregRange)+ sizeof(pCliOutInfo->numBars) + sizeof(pCliOutInfo->res_type);
		} 
		if (driverVer.minorVersion == 2) {
			printf("Ver 2\n");
			extraLen = sizeof(pCliOutInfo->res_type);
		}
//		printf("extraLen = %d\n", extraLen);


		if (returnContext.contextLen != (sizeof(anwiOutClientInfo) - extraLen)) {
			printf("Return size (%d) from device open DeviceIoControl call doesnt match the expected size (%d) ....\n",returnContext.contextLen,sizeof(anwiOutClientInfo));
			return -1;
		}

		pCliOutInfo = (pAnwiOutClientInfo) returnContext.context;
		memcpy(pRetCliInfo, pCliOutInfo, (sizeof(anwiOutClientInfo) - extraLen));

		if (driverVer.minorVersion == 0) {
			pRetCliInfo->regRange = 65536; 
			pRetCliInfo->memSize = 1024 * 1024; // 1 MB 
		}

#ifndef CUSTOMER_REL
		printf("Mem phy addr = %x vir addr = %x size = %x\n",pCliOutInfo->memPhyAddr,pCliOutInfo->memVirAddr, pCliOutInfo->memSize);
        if (driverVer.minorVersion >= 2) {
            for(iIndex=0; iIndex<pCliOutInfo->numBars; iIndex++) {
               uiPrintf("Bar Reg %d phy addr = %x vir addr = %x \n", iIndex, pCliOutInfo->aregPhyAddr[iIndex],pCliOutInfo->aregVirAddr[iIndex]);
            }
         }
         else {
		    printf("Reg phy addr = %x vir addr = %x \n",pCliOutInfo->regPhyAddr,pCliOutInfo->regVirAddr);
         }
#endif
		iIndex = 0; // quiet the warnings
		return 0;
}

#endif // LEGACY

A_STATUS connectSigHandler(void) {
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

    inSignalHandler = TRUE;

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

#ifdef PERL_CORE
       dkPerlCleanup();
#else
#ifdef WIN_CLIENT_BUILD
       	envCleanup(TRUE);
#else
       art_teardownDevice(glbl_devNum);
#endif //WIN_CLIENT_BUILD
       envCleanup(TRUE);
#endif

#ifdef _IQV
	uiWriteToLog("!!!!! consoleCtrlHandler\n");		
#endif	// _IQV

#ifdef ART_BUILD
    ExitProcess(1);
#else
    exit(1);
#endif

    return TRUE; // we handled the situation, so we return TRUE
}


void close_driver() {
#ifdef LEGACY
		CloseHandle (hDriver); 
#endif
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

A_UINT32 hwIORead
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
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];


#ifdef LEGACY
		inStruct.cliId = pdevInfo->cliId;
#endif
		inStruct.opType = ANWI_IO_READ;
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
		return data;
}

void hwIOWrite
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
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];


#ifdef LEGACY
		inStruct.cliId = pdevInfo->cliId;
#endif
		inStruct.opType = ANWI_IO_WRITE;
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
* hwCfgRead - called by hwCfgRead*
*
* This routine will call into the driver
* 32 bit PCI configuration read.
*
* RETURNS: the value read
*/
A_UINT32 hwCfgRead
(
 	struct mdk_wlanDevInfo *pdevInfo,        /* device info pointer */
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
		return data;
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
	A_UINT32 data;
	A_UINT8 out;
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	data = hwCfgRead(pdevInfo,address & 0xfffffffc);
	out = (A_UINT8)((data >> (address & 0x00000003)) & 0x000000ff);

	return out;
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
	A_UINT32 data;
	A_UINT16 out;
	MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	data = hwCfgRead(pdevInfo, address & 0xfffffffc);
	out = (A_UINT16)((data >> (address & 0x00000002)) & 0x0000ffff);

	return out;
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
	data = hwCfgRead(pdevInfo, address & 0xfffffffc);

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
	MDK_WLAN_DEV_INFO *pdevInfo;

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
	printf("hwCfgWrite8 not implemented for ANWI \n");
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
	printf("hwCfgWrite16 not implemented for ANWI \n");
}

#ifdef ART_BUILD
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
#else
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
	A_BOOL status; 
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	anwiEventOpStruct event;
	HANDLE hnd;
    MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

#ifdef LEGACY
		event.cliId = pdevInfo->cliId;
#endif
		event.opType = ANWI_CREATE_EVENT;
		event.valid = 1;
		event.param[0] = pCmd->CMD_U.CREATE_EVENT_CMD.type;
		event.param[1] = pCmd->CMD_U.CREATE_EVENT_CMD.persistent;
		event.param[2] = pCmd->CMD_U.CREATE_EVENT_CMD.param1;
		event.param[3] = pCmd->CMD_U.CREATE_EVENT_CMD.param2;
		event.param[4] = pCmd->CMD_U.CREATE_EVENT_CMD.param3;
		event.param[5] = (pCmd->CMD_U.CREATE_EVENT_CMD.eventHandle.f2Handle << 16) | pCmd->CMD_U.CREATE_EVENT_CMD.eventHandle.eventID;

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
	MDK_WLAN_DEV_INFO *pdevInfo,
	EVENT_STRUCT *pEvent
)
{
	
	A_BOOL status; 
	A_UINT32 bR = 0; 
	anwiReturnContext returnContext; 
	anwiEventOpStruct inEvent;
	pAnwiEventOpStruct outEvent;
	HANDLE hnd;
	int i;

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
			return -1;
		}

	   	if (returnContext.contextLen != sizeof(anwiEventOpStruct)) {
			printf("Return size (%d) from event op DeviceIoControl call doesnt match the expected size (%d) \n",returnContext.contextLen,sizeof(anwiEventOpStruct));
			return -1;
		}

		pEvent->type = 0;

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
		pEvent->result = outEvent->param[6];
#ifdef MAUI
		for (i=0;i<5;i++) {
			pEvent->additionalParams[i] = outEvent->param[7+i];
		}
#endif

    return(1);
}

A_INT16 hwGetNextEvent
(
	A_UINT16 devIndex,
	void *pBuf
)
{
    MDK_WLAN_DEV_INFO *pdevInfo;

	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	return getNextEvent(pdevInfo,(EVENT_STRUCT *)pBuf);
}

#endif // else ART_BUILD

A_STATUS get_device_client_info(MDK_WLAN_DEV_INFO *pdevInfo, PDRV_VERSION_INFO pDrvVer, PCLI_INFO cliInfo) {
	HANDLE hDevice;
    anwiOutClientInfo anwi_cliInfo;

#ifdef LEGACY
	if (!RegisterClient(pdevInfo->pdkInfo->devIndex,&anwi_cliInfo)) {
		uiPrintf("Error: Unable to register client with the driver \n");
		A_FREE(pdevInfo->pdkInfo);
		A_FREE(pdevInfo);
		return A_ERROR;
	}
	pdevInfo->cliId = cliInfo->cliId;
#else
	hDevice = open_device(pdevInfo->pdkInfo->device_fn, pdevInfo->pdkInfo->devIndex, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        uiPrintf("Error: Unable to open the device !\n");
		return A_ERROR; 
    }
	if (getClientInfo(hDevice,&anwi_cliInfo) < 0) {
		uiPrintf("Unable to get client info \n"); 
		CloseHandle(hDevice); 
		return A_ERROR; 
	} 
	pdevInfo->hDevice = hDevice;
#endif

    pDrvVer->majorVersion = driverVer.majorVersion;
    pDrvVer->minorVersion = driverVer.minorVersion;

    // Copy from driver client info structure 
	memcpy(cliInfo, &anwi_cliInfo, sizeof(CLI_INFO));
    return A_OK;
}

void close_device(MDK_WLAN_DEV_INFO *pdevInfo) {

#ifdef LEGACY
	unRegisterClient(pdevInfo->cliId); 
#else
    if (pdevInfo->hDevice > 0) 
	CloseHandle(pdevInfo->hDevice);
#endif
	
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

