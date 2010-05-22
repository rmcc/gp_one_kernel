/* MLIBif.c - contians wrapper functions for MLIB */

/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */
#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/MLIBif.c#3 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/MLIBif.c#3 $"

#ifdef _WINDOWS 
#include <windows.h>
#endif

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "manlibInst.h"
#include "dk_ver.h"
#include "common_hw.h"
#ifdef JUNGO
#include "mld.h"
#endif
#include "MLIBif.h"
#include "art_if.h"
#include "test.h"
#include <stdlib.h>

#if defined(LINUX) || defined(__linux__)
#include <string.h>
#endif

// Dev to driverDev mapping table.  devNum must be in range 0 to LIB_MAX_DEV
A_UINT32   devNum2driverTable[LIB_MAX_DEV];
extern A_BOOL thin_client;
extern A_BOOL sdio_client;
static A_BOOL last_op_was_read = 0;
static A_BOOL last_op_offset_was_fc = 0;

static void printRxStats(RX_STATS_STRUCT *pRxStats);
static void printTxStats(TX_STATS_STRUCT *pTxStats);

A_UCHAR *t_bytesRead;
A_UCHAR *t_bytesWrite;

#ifdef _IQV
extern A_INT32 remoteMdkErrNo;
#endif

A_BOOL initializeEnvironment(A_BOOL remote) {
	A_BOOL	openDriver;
  
    // print version string
    uiPrintf("\n    --- Atheros Radio Test (ART) 6000 ---\n");
    uiPrintf(MAUIDK_VER2);
    uiPrintf(MAUIDK_VER3);
 
	openDriver = remote ?  0 : 1;

    // perform environment initialization
#ifndef _IQV
#ifdef _DEBUG
    if ( A_ERROR == envInit(TRUE, openDriver) ) {
#else
    if ( A_ERROR == envInit(FALSE, openDriver) ) {
#endif
#else // _IQV
    if ( A_ERROR == envInit(FALSE, openDriver) ) {
#endif	// _IQV
        uiPrintf("Error: Unable to initialize the local environment... Closing down!\n");
        return FALSE;
    }
	t_bytesRead = (A_UCHAR *) malloc(MAX_MEMREAD_BYTES * sizeof(A_UCHAR));
	t_bytesWrite = (A_UCHAR *) malloc(MAX_MEMREAD_BYTES * sizeof(A_UCHAR));
	if ((t_bytesRead == NULL) || (t_bytesWrite == NULL)) 	
		return FALSE;
    return TRUE;
}


void closeEnvironment(void) {
    // clean up anything that was allocated and close the local driver
#ifdef _IQV
	DWORD uExitCode;
//	HANDLE	devlibHandle;
	int	err;

//	devlibHandle = IQ_get_devlibHandle();
//	err = GetExitCodeProcess(devlibHandle, &uExitCode);
//	if (uExitCode != 259) {
	//	remoteMdkErrNo = 99;
	//	uiPrintf("devlibHandle exitCode: %u _ %u skip closeEnvironment()\n", uExitCode, err);
	//	envCleanup(TRUE);

	//	// close process to start devlib ???
	//	err = TerminateProcess(devlibHandle, uExitCode);
	//	// Close process and thread handles. 
	//	if (err!=0)// otherwise will crash.
	//		err = CloseHandle(devlibHandle);
	//	devlibHandle = 0;
	//	
	//	return;
	//} 
#endif       

    envCleanup(TRUE);
	if (t_bytesRead != NULL) 
			free(t_bytesRead);
	if (t_bytesWrite != NULL) 
			free(t_bytesWrite);
#if 0
	if(sdio_client)
	{
		printf("closing target\n");		
		closeTarget();  //unload SDIO driver
	}
#endif
	
	return;
}

A_BOOL devNumValid
(
 A_UINT32 devNum
)
{
    if(globDrvInfo.pDevInfoArray[dev2drv(devNum)] != NULL) {
        return TRUE;
    }
    else {
        return FALSE;
    }
}

void txPrintStats
(
 A_UINT32 devNum, 
 A_UINT32 rateInMb,
 A_UINT32 remote
)
{
    TX_STATS_STRUCT rStats;

    txGetStats(devNum, rateInMb, remote, &rStats);

	//check for errors
    if (!getMdkErrNo(devNum)) {
	    printTxStats(&rStats);
    }
}

void printTxStats
(
 TX_STATS_STRUCT *pTxStats
)
{
	uiPrintf("Good Packets           %10lu  ", pTxStats->goodPackets);
	uiPrintf("Ack sig strnth min     %10lu\n", pTxStats->ackSigStrengthMin);
	uiPrintf("Underruns              %10lu  ", pTxStats->underruns);
	uiPrintf("Ack sig strenth avg    %10lu\n", pTxStats->ackSigStrengthAvg);
	uiPrintf("Throughput             %10.2f  ",(float) pTxStats->throughput/1000);
	uiPrintf("Ack sig strenth max    %10lu\n", pTxStats->ackSigStrengthMax);
	uiPrintf("Excess retries         %10lu  ", pTxStats->excessiveRetries);
	uiPrintf("short retries 4        %10lu\n", pTxStats->shortRetry4); 
	uiPrintf("short retries 1        %10lu  ", pTxStats->shortRetry1); 
	uiPrintf("short retries 5        %10lu\n", pTxStats->shortRetry5); 
	uiPrintf("short retries 2        %10lu  ", pTxStats->shortRetry2); 
	uiPrintf("short retries 6-10     %10lu\n", pTxStats->shortRetry6to10); 
	uiPrintf("short retries 3        %10lu  ", pTxStats->shortRetry3); 
	uiPrintf("short retries 11-15    %10lu\n", pTxStats->shortRetry11to15); 
/*	uiPrintf("long  retries 1        %10lu  ", pTxStats->longRetry1); 
	uiPrintf("long  retries 2        %10lu\n", pTxStats->longRetry2); 
	uiPrintf("long  retries 3        %10lu  ", pTxStats->longRetry3); 
	uiPrintf("long  retries 4        %10lu\n", pTxStats->longRetry4); 
	uiPrintf("long  retries 5        %10lu  ", pTxStats->longRetry5); 
	uiPrintf("long  retries 6-10     %10lu\n", pTxStats->longRetry6to10); 
	uiPrintf("long  retries 11-15    %10lu\n", pTxStats->longRetry11to15);  */
}

void rxPrintStats
(
 A_UINT32 devNum,
 A_UINT32 rateInMb,
 A_UINT32 remote
)
{
    RX_STATS_STRUCT rStats;

    rxGetStats(devNum, rateInMb, remote, &rStats);

	//check for errors
    if (!getMdkErrNo(devNum)) {
	    printRxStats(&rStats);
    }

}

void printRxStats
(
 RX_STATS_STRUCT *pRxStats
)
{
	uiPrintf("Good Packets           %10lu  ", pRxStats->goodPackets);
	uiPrintf("Data sig strenth min   %10lu\n", pRxStats->DataSigStrengthMin);
	uiPrintf("CRC-Failure Packets    %10lu  ", pRxStats->crcPackets);
	uiPrintf("Data sig strenth avg   %10lu\n", pRxStats->DataSigStrengthAvg);
	uiPrintf("Bad CRC Miscomps       %10lu  ", pRxStats->bitMiscompares);
	uiPrintf("Data sig strenth max   %10lu\n", pRxStats->DataSigStrengthMax);
	uiPrintf("Good Packet Miscomps   %10lu  ", pRxStats->bitErrorCompares);
	uiPrintf("PPM min                %10ld\n", pRxStats->ppmMin);
	uiPrintf("Single dups            %10lu  ", pRxStats->singleDups);
	uiPrintf("PPM avg                %10ld\n", pRxStats->ppmAvg);
	uiPrintf("Multiple dups          %10lu  ", pRxStats->multipleDups);
	uiPrintf("PPM max                %10ld\n", pRxStats->ppmMax);
	return;
}

/**************************************************************************
 * Workaround for HW bug that causes read or write bursts that
 * cross a 256-boundary to access the incorrect register.
 */  
static void
reg_burst_war
(
	A_UINT16 devIndex,
	A_UINT32 offset, 
	A_BOOL op_is_read
)
{
    A_UINT32 offset_lsb;
	MDK_WLAN_DEV_INFO *pdevInfo;
	
    offset_lsb = offset & 0xff;
	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

    // Check if offset is aligned to a 256-byte boundary
    if (offset_lsb == 0) {
      if (last_op_offset_was_fc && (last_op_was_read == op_is_read)) {
         // Last access was to an offset with lsbs of 0xfc and was
         // of the same type (read or write) as the current access, so
         // need to break a possible burst across the 256-byte boundary.
         // Do this by reading the SREV reg, as it's always safe to do so.
         hwMemRead32(devIndex, 0x4020 + pdevInfo->pdkInfo->f2MapAddress);
      }
    }
  
    last_op_was_read = op_is_read;
    last_op_offset_was_fc = (offset_lsb == 0xfc);
}

/**************************************************************************
* OSregRead - MLIB command for reading a register
*
* RETURNS: value read
*/
A_UINT32 OSregRead
(
    A_UINT32 devNum,
    A_UINT32 regOffset
)
{
    A_UINT32         regReturn;
    MDK_WLAN_DEV_INFO    *pdevInfo;
	A_UINT16 devIndex;

	if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: OSregRead did not receive a valid devNum\n");
	    return(0xdeadbeef);
	}

	devIndex = (A_UINT16)dev2drv(devNum);
   	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

  if (thin_client) {
        regOffset -= pdevInfo->pdkInfo->aregPhyAddr[0];
        regReturn = art_regRead(devNum, regOffset);
  }
  else {
   	/* check that the register is within the range of registers */
	if( (regOffset < pdevInfo->pdkInfo->f2MapAddress) || 
		(regOffset > (MAX_REG_OFFSET + pdevInfo->pdkInfo->f2MapAddress)) ) {
		uiPrintf("Error:  OSregRead Not a valid register offset:%x\n", regOffset);
		return(0xdeadbeef);
    }

	/* read the register */
	reg_burst_war(devIndex, regOffset, 1);
	regReturn = hwMemRead32(devIndex, regOffset); 
	// uiPrintf("Register at offset %08lx: %08lx\n", regOffset, regReturn);
    //
  }

    return(regReturn);
}



/**************************************************************************
* OSmem32Write - MLIB command for writing a register
*
*/
void OSmem32Write
(
    A_UINT32 devNum,
    A_UINT32 regOffset,
    A_UINT32 regValue
)
{
    MDK_WLAN_DEV_INFO    *pdevInfo;
    A_UINT16 devIndex;

    if(devNumValid(devNum) == FALSE) {
        uiPrintf("Error: OSregWrite did not receive a valid devNum\n");
        return;
    }

    devIndex = (A_UINT16)dev2drv(devNum);
    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
    art_mem32Write(devNum, regOffset, regValue);
    //uiPrintf("...OSmem32Write: 0x%04x 0x%08x\n", regOffset, regValue);
}


/**************************************************************************
* OSregRead - MLIB command for reading a register
*
* RETURNS: value read
*/
A_UINT32 OSmem32Read
(
    A_UINT32 devNum,
    A_UINT32 regOffset
)
{
    A_UINT32         regReturn;
    MDK_WLAN_DEV_INFO    *pdevInfo;
    A_UINT16 devIndex;

    if(devNumValid(devNum) == FALSE) {
        uiPrintf("Error: OSmem32Read did not receive a valid devNum\n");
        return(0xdeadbeef);
    }

    devIndex = (A_UINT16)dev2drv(devNum);
    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

    regReturn = art_mem32Read(devNum, regOffset);
    //uiPrintf("...OSmem32Read: offset %08lx, %08lx\n", regOffset, regReturn);

    return(regReturn);
}


/**************************************************************************
* OSregWrite - MLIB command for writing a register
*
*/
void OSregWrite
(
	A_UINT32 devNum,
    A_UINT32 regOffset,
    A_UINT32 regValue
)
{
    MDK_WLAN_DEV_INFO    *pdevInfo;
	A_UINT16 devIndex;

	if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: OSregWrite did not receive a valid devNum\n");
		return;
	}

	devIndex = (A_UINT16)dev2drv(devNum);
   	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
	if (thin_client) {
       regOffset -= pdevInfo->pdkInfo->aregPhyAddr[0];
       art_regWrite(devNum, regOffset, regValue);
	   if(pdevInfo->pdkInfo->printPciWrites) 
	   {
		uiPrintf("0x%04x 0x%08x\n", regOffset & 0xffff, regValue);
	   } 
	}
	else {

//    regOffset += pdevInfo->pdkInfo->aregPhyAddr[0];

	  if( (regOffset < pdevInfo->pdkInfo->f2MapAddress) || 
		(regOffset > (MAX_REG_OFFSET + pdevInfo->pdkInfo->f2MapAddress)) ) {
		uiPrintf("Error:  OSregWrite Not a valid register offset\n");
		return;
      }

	  /* write the register */
	  reg_burst_war(devIndex, regOffset, 0);
	  hwMemWrite32(devIndex, regOffset, regValue); 
	  if(pdevInfo->pdkInfo->printPciWrites) 
	  {
		uiPrintf("0x%04x 0x%08x\n", regOffset & 0xffff, regValue);
	  } 
	}
}


/**************************************************************************
* OScfgRead - MLIB command for reading a pci configuration register
*
* RETURNS: value read
*/
A_UINT32 OScfgRead
(
	A_UINT32 devNum,
    A_UINT32 regOffset
)
{
    A_UINT32         regReturn;
	A_UINT16 devIndex;

	if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: OScfgRead did not receive a valid devNum\n");
	    return(0xdeadbeef);
	}

	if (thin_client) {
       regReturn = art_cfgRead(devNum, regOffset);
	}
	else {
	   devIndex = (A_UINT16)dev2drv(devNum);

	   if(regOffset > MAX_CFG_OFFSET) {
		   uiPrintf("Error:  OScfgRead: not a valid config offset\n");
		   return(0xdeadbeef);
       }
	   regReturn = hwCfgRead32(devIndex, regOffset); 

       /* display the value */
       //uiPrintf("%08lx: ", regOffset);
       //uiPrintf("%08lx\n", regReturn);
	}

    return(regReturn);
}

/**************************************************************************
* OScfgWrite - MLIB command for writing a pci config register
*
*/
void OScfgWrite
(
	A_UINT32 devNum,
    A_UINT32 regOffset,
    A_UINT32 regValue
)
{
	A_UINT16 devIndex;
   
	if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: OScfgWrite did not receive a valid devNum\n");
		return;
	}

	if (thin_client) {
       art_cfgWrite(devNum, regOffset, regValue);
	}
	else {
	   devIndex = (A_UINT16)dev2drv(devNum);
	
	   if(regOffset > MAX_CFG_OFFSET) {
		   uiPrintf("Error:  OScfgWrite: not a valid config offset\n");
		   return;
       }
	   hwCfgWrite32(devIndex, regOffset, regValue); 
	}
}

/**************************************************************************
* OSmemRead - MLIB command to read a block of memory
*
* read a block of memory
*
* RETURNS: An array containing the bytes read
*/
void OSmemRead
(
	A_UINT32 devNum,
    A_UINT32 physAddr, 
	A_UCHAR  *bytesRead,
    A_UINT32 length
)
{
	A_UINT32 t_length, i;
	A_UCHAR *t_bytesRead;
    A_UINT32 t_physAddr, t_sOffset;
	A_UINT16 devIndex;

	if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: OSmemRead did not receive a valid devNum\n");
		return;
	}

    /* check to see if the size will make us bigger than the send buffer */
    if (length > MAX_MEMREAD_BYTES) {
        uiPrintf("Error: OSmemRead length too large, can only read %x bytes\n", MAX_MEMREAD_BYTES);
		return;
    }
	if (bytesRead == NULL) {
        uiPrintf("Error: OSmemRead received a NULL ptr to the bytes to read - please preallocate\n");
		return;
    }

	if (thin_client) {
	   t_length = length;
	   t_physAddr = physAddr;
	   t_sOffset = 0;
	   if (t_physAddr % 4) {
	      t_length += (t_physAddr % 4);
	      t_sOffset = (t_physAddr % 4);
	      t_physAddr &= 0xfffffffc;
	   }
 	   if ( (t_length % 4)) { 
		   t_length += (4 -(t_length % 4));
	   }
	   t_bytesRead = (A_UCHAR *) malloc(t_length * sizeof(A_UCHAR));
//       uiPrintf("SNOOP::OSmemRead:physAddr=%x:t_physAddr=%x:t_sOffset=%d:length=%d:t_length=%d:\n", physAddr, t_physAddr, t_sOffset, length, t_length);
       art_memRead(devNum, t_physAddr, t_bytesRead, t_length);

	   for(i=0;i<length;i++) {
	     bytesRead[i] = t_bytesRead[i+t_sOffset];
	   }
	   free(t_bytesRead);
	}
    else {
	   devIndex = (A_UINT16)dev2drv(devNum);

	   if(hwMemReadBlock(devIndex, bytesRead, physAddr, length) == -1) {
		   uiPrintf("Error: OSmemRead failed call to hwMemReadBlock()\n");
		return;
	   }
	}

}

/**************************************************************************
* OSmemWrite - MLIB command to write a block of memory
*
*/
void OSmemWrite
(
	A_UINT32 devNum,
    A_UINT32 physAddr,
	A_UCHAR  *bytesWrite,
	A_UINT32 length
)
{
	A_UINT32 t_length, i;
    A_UINT32 t_physAddr, t_sOffset;
	A_UINT16 devIndex;//, iIndex; 

	if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: OSmemWrite did not receive a valid devNum\n");
		return;
	}
    /* check to see if the size will make us bigger than the send buffer */
    if (length > MAX_MEMREAD_BYTES) {
        uiPrintf("Error: OSmemWrite length too large, can only read %x bytes\n", MAX_MEMREAD_BYTES);
		return;
    }
	if (bytesWrite == NULL) {
        uiPrintf("Error: OSmemWrite received a NULL ptr to the bytes to write\n");
		return;	
	}
/*    uiPrintf("Writing the following values @addr=%x\n", physAddr);
    for(iIndex=0; iIndex<length; iIndex++) {
            uiPrintf("%x ", bytesWrite[iIndex]);
            if (!(iIndex+1 % 16)) uiPrintf("\n");
    }
    uiPrintf("\n");
    */
	if (thin_client) {
	   t_length = length;
	   t_physAddr = physAddr;
	   t_sOffset = 0;
	   if (t_physAddr % 4) {
	      t_length += (t_physAddr % 4);
	      t_sOffset = (t_physAddr % 4);
	      t_physAddr &= 0xfffffffc;
	   }
 	   if ( (t_length % 4)) { 
		   t_length += (4 -(t_length % 4));
	   }
	   memcpy(t_bytesWrite, bytesWrite, length * sizeof(A_UCHAR));
       art_memWrite(devNum, t_physAddr, t_bytesWrite, t_length);
	}
    else {
	  devIndex = (A_UINT16)dev2drv(devNum);

	  if(hwMemWriteBlock(devIndex,bytesWrite, length, &(physAddr)) == -1) {
			uiPrintf("Error:  OSmemWrite failed call to hwMemWriteBlock()\n");
			return;
      }
	}

}

/**************************************************************************
* getISREvent - MLIB command get latest ISR event
*
*/
ISR_EVENT getISREvent
(
 A_UINT32 devNum
)
{
	// Initialize event to invalid.
  ISR_EVENT event = {0, 0};
  EVENT_STRUCT ppEvent;
  MDK_WLAN_DEV_INFO    *pdevInfo;
  A_UINT16 devIndex;

  if (thin_client) {

	if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: getISREvent did not receive a valid devNum\n");
	    return event;
	}

    if (art_getISREvent(devNum, &ppEvent) == 0xdead) return event;
    if (ppEvent.type == ISR_INTERRUPT) {
		event.valid = 1;
		event.ISRValue = ppEvent.result[0];
     }
    //printf("SNOOP::event type=%d:valid=%d:isrvalue=%d:\n", ppEvent.type, event.valid,  event.ISRValue);
    
  }
  else {

     #if defined(ANWI) || defined(LINUX) ||defined(_WINDOWS) 
          EVENT_STRUCT pLocEventSpace;
     #else
          EVENT_STRUCT *pLocEventSpace;
     #endif // defined(ANWI/..
     devIndex = (A_UINT16)dev2drv(devNum);
     pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
        #ifdef JUNGO
         if(checkForEvents(globDrvInfo.triggeredQ)) {
                 //call into the kernel plugin to get the event
                 if((pLocEventSpace = popEvent(globDrvInfo.triggeredQ)) != NULL) {
                     if(pLocEventSpace->type == ISR_INTERRUPT) {
                         event.valid = 1;
                         event.ISRValue = pLocEventSpace->result;
                      }
                  else {
                      uiPrintf("Error: getISREvent - found a non-ISR event in a client - is this possible???\n");
                      uiPrintf("If this has become a possibility... then remove this error check\n");
                      }
                  }
                  else {
                     uiPrintf("Error: getISREvent Unable to get event\n");
                  }
          }
        #endif // JUNGO
     #if defined(ANWI) || defined (LINUX) || defined(_WINDOWS)  
          if (getNextEvent(devIndex, &pLocEventSpace)) {
              if(pLocEventSpace.type == ISR_INTERRUPT) {
                    event.valid = 1;
                    event.ISRValue = pLocEventSpace.result[0];
              }   
          }
     //    printf("SNOOP::event type=%d:valid=%d:isrvalue=%d:\n", pLocEventSpace.type, event.valid,  event.ISRValue);
     #endif
    }
        
	return(event);
}

/**************************************************************************
* setupDevice - initialization call to ManLIB
*
* RETURNS: devNum or -1 if fail
*/
A_INT32 setupDevice
(
 A_UINT32 whichDevice, 
 DK_DEV_INFO *pdkInfo,
 A_UINT16 remoteLib
)
{
	static DEVICE_MAP devMap;
    MDK_WLAN_DEV_INFO    *pdevInfo;
    A_UINT32 devNum;
    EVT_HANDLE  eventHdl;
	A_UINT16 devIndex;
    A_UINT16 device_fn = WMAC_FUNCTION;

        //uiPrintf("SNOOP::setupDevice called\n");
 	if ( whichDevice > WLAN_MAX_DEV ) {
		/* don't have a large enough array to accommodate this device */
		uiPrintf("Error: devInfo array not large enough, only support %d devices\n", WLAN_MAX_DEV);
	    return -1;
	}


    devIndex = (A_UINT16)(whichDevice - 1);

	if ( A_FAILED( deviceInit(devIndex, device_fn, pdkInfo) ) ) {
		uiPrintf("setupDevice Error: Failed call to local deviceInit()!\n");
#ifndef _IQV
        exit(EXIT_FAILURE);
#endif
	    return -1;
	}
	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

    // Finally, setup the library mapping
	devMap.DEV_CFG_ADDRESS = 0;
	devMap.DEV_CFG_RANGE = MAX_CFG_OFFSET;
#if defined(ANWI) || defined (LINUX) || defined(_WINDOWS)
	devMap.DEV_MEMORY_ADDRESS = (unsigned long) pdevInfo->pdkInfo->memPhyAddr;
#endif
#ifdef JUNGO
	devMap.DEV_MEMORY_ADDRESS = (unsigned long) pdevInfo->pdkInfo->dma.Page[0].pPhysicalAddr;
#endif
	devMap.DEV_MEMORY_RANGE = pdevInfo->pdkInfo->memSize; 
	devMap.DEV_REG_ADDRESS = pdevInfo->pdkInfo->f2MapAddress;

	devMap.DEV_REG_RANGE = 65536;

	devMap.OScfgRead = OScfgRead;
	devMap.OScfgWrite = OScfgWrite;
	devMap.OSmemRead = OSmemRead;
	devMap.OSmemWrite = OSmemWrite;
	devMap.OSregRead = OSregRead;
	devMap.OSregWrite = OSregWrite;
	devMap.OSmem32Read = OSmem32Read;
	devMap.OSmem32Write = OSmem32Write;
	devMap.getISREvent = getISREvent;
	devMap.devIndex = devIndex;

    devMap.remoteLib = remoteLib;
    devMap.r_eepromRead = art_eepromRead;
    devMap.r_eepromWrite = art_eepromWrite;
    devMap.r_eepromReadBlock = art_eepromReadBlock;
    devMap.r_eepromReadLocs = art_eepromReadLocs;
	if (thin_client) {
       devMap.r_hwReset = art_hwReset;
       devMap.r_pllProgram = art_pllProgram;
	   devMap.r_calCheck = art_calCheck;
	   devMap.r_pciWrite = art_pciWrite;
	   devMap.r_fillTxStats = art_fillTxStats;
	   devMap.r_createDescriptors = art_createDescriptors;
   }
   else {
       devMap.r_hwReset = NULL;
       devMap.r_pllProgram = NULL;
	   devMap.r_calCheck = NULL;
	   devMap.r_pciWrite = NULL;
	   devMap.r_fillTxStats = NULL;
	   devMap.r_createDescriptors = NULL;
	}
	devNum = initializeDevice(devMap);

    if(devNum > WLAN_MAX_DEV) {
        uiPrintf("setupDevice Error: Manlib Failed to initialize for this Device:devNum returned = %x\n", devNum);
        return -1;
    }
   	devNum2driverTable[devNum] = devIndex;

    // assign the handle (id, actually) that will be sent back to Perl to uniquely
    eventHdl.eventID = 0;
    eventHdl.f2Handle = (A_UINT16)devNum;

    if (!pdkInfo)
	if(!pdevInfo->pdkInfo->haveEvent) {
        if(hwCreateEvent(devIndex, ISR_INTERRUPT, 1, 0, 0, 0, eventHdl) == -1) {
            uiPrintf("setupDevice Error: Could not initalize driver ISR events\n");
            teardownDevice(devNum);
            return -1;
        }
		pdevInfo->pdkInfo->haveEvent = TRUE;
	}

    return devNum;
}

void teardownDevice
(
 A_UINT32 devNum
) 
{
        //uiPrintf("SNOOP::teardownDevice called\n");
    if(devNumValid(devNum) == FALSE) {
		uiPrintf("Error: teardownDevice did not receive a valid devNum\n");
	    return;
	}

    // Close the Manufacturing Lib
    closeDevice(devNum);
    // Close the driver Jungo driver entries
    deviceCleanup((A_UINT16)dev2drv(devNum));
	
        //uiPrintf("SNOOP::teardownDevice exit\n");
}

/**************************************************************************
* setPciWritesFlag - Change the value to flag for printing pci writes data
*
* RETURNS: N/A
*/
void changePciWritesFlag
(
	A_UINT32 devNum,
	A_UINT32 flag
)
{
    MDK_WLAN_DEV_INFO    *pdevInfo;
	A_UINT16 devIndex;

	devIndex = (A_UINT16)dev2drv(devNum);
	pdevInfo = globDrvInfo.pDevInfoArray[devIndex];

	pdevInfo->pdkInfo->printPciWrites = (A_BOOL)flag;
	return;
}

