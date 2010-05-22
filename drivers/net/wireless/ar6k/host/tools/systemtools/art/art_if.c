/* art_if.c -  contains the ART wrapper functions */
 
/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */

#ifdef __ATH_DJGPPDOS__
#include <unistd.h>
#ifndef EILSEQ  
    #define EILSEQ EIO
#endif	// EILSEQ

 #define __int64	long long
 #define HANDLE long
 typedef unsigned long DWORD;
 #define Sleep	delay
 #include <bios.h>
 #include "dk_structures.h"
 #include "dk_common.h"
 #include "common_hwext.h"
#endif	// #ifdef __ATH_DJGPPDOS__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "wlantype.h"
#include "wlanproto.h"
#include "athreg.h"
#include "manlib.h"
#include "test.h"
#include "dk_cmds.h"
#include "art_if.h"
#include "art_ani.h"
#ifndef __ATH_DJGPPDOS__
#include "MLIBif.h"    
#if defined(LINUX) || defined(__linux__)
#include "sock_linux.h"
#else
#include "sock_win.h"
#endif
#else
#include "mlibif_dos.h"
#endif
#if defined(LINUX)
#include "mEepStruct6000.h"
#elif defined(_WINDOWS)
#include "ar6000/mEepStruct6000.h"
#endif
#include "dynamic_optimizations.h"
#include "maui_cal.h"

#include "misc_routines.h"
extern A_BOOL thin_client;
extern A_BOOL usb_client;
extern A_BOOL sdio_client;
extern A_UINT32 sent_bytes;
extern A_UINT32 received_bytes;

extern MLD_CONFIG configSetup;
extern A_BOOL printLocalInfo;
	   
// extern declarations for dut-golden sync
extern ART_SOCK_INFO *artSockInfo;
extern ART_SOCK_INFO *pArtPrimarySock;
extern ART_SOCK_INFO *pArtSecondarySock;

extern MANLIB_API A_UINT8 dummyEepromWriteArea[AR6K_EEPROM_SIZE];

static A_UINT32 globalNumInstances = 0;

extern void *artConnect(void);
extern A_BOOL prepare2WayComms
(
 void
);

extern void cleanupSockMem
(
	void*			pOSSock,
	A_BOOL			closeSocket
);



// Error number and error string 
A_INT32 art_mdkErrNo = 0;
A_CHAR art_mdkErrStr[SIZE_ERROR_BUFFER];
//  Remote error number and error string
A_INT32 remoteMdkErrNo = 0;
A_CHAR remoteMdkErrStr[SIZE_ERROR_BUFFER];
//static A_BOOL  ownSocketClose = 0;

// holds the cmd replies sent over channel 
PIPE_CMD GlobalCmd;
CMD_REPLY cmdReply;

// forward function declaration
ART_SOCK_INFO *openCommsChannel(A_CHAR *machineName);

A_BOOL artSendCmd(PIPE_CMD *pCmdStruct,A_UINT32 cmdSize,void **returnCmdStruct);
A_BOOL receiveCmdReturn(A_UINT32 *pReturnLength);
A_UINT32 art_createEvent(A_UINT32 devNum,A_UINT32 type, A_UINT32 persistent,
						 A_UINT32 param1,A_UINT32 param2,A_UINT32 param3);

#if defined(LINUX)
A_UINT32 BmiOpeation(HANDLE nitro_handle, A_UINT32 nTargetID) {return 1;}
#endif

#ifdef _IQV
	char	sProductId[20];
#endif	// _IQV

A_INT32  art_setupDevice(A_UINT32 whichDevice)
{
	A_UINT32 devNum;
	A_UINT32 *pRegValue;
    DK_DEV_INFO  *pdkInfo=NULL;
	A_UINT32 bootRomVer, bootRomMajVer, bootRomMinVer;
        HANDLE nitro_handle;

    	//uiPrintf("art_setupDevice %d\n", whichDevice);
	//at setup time, remote flag being set, applies to primary ART session being AP
	if (configSetup.remote)
	{
		if(pArtPrimarySock == NULL) {	
			artSockInfo = openCommsChannel(configSetup.machName);
			pArtPrimarySock = artSockInfo;
			if (artSockInfo == NULL) {
				uiPrintf("Error: Unable to open communications channel to target!\n");
				return -1;
			}
		} 
            if (strncmp(configSetup.machName,"com",3) == 0)
            {
                // we sould add the nTargetID judegement for TWL platform
                // since we had not running initAR6000_ene for TWL

                GlobalCmd.cmdID = ART_BMI_READ_SOC_TARGET_ID;
                GlobalCmd.devNum = 0;
                if(!artSendCmd(&GlobalCmd,0,(void **)&pRegValue))
                {
                    uiPrintf("Error: Unable to successfully send  GlobalCmd.cmdID->%d\n",GlobalCmd.cmdID);
#ifdef _IQV
					return -1;
#endif	// _IQV
                }
                else
                {
                    uiPrintf("*pRegValue: 0x%x\n",*pRegValue);
                }
                if (!BmiOpeation(nitro_handle, *pRegValue))
					return -1;
            }
	    if (strnicmp(configSetup.machName, "USB", 3) == 0) {
			whichDevice += USB_FN_DEV_START_NUM;
	    }
		
	    //if (thin_client) 
        GlobalCmd.devNum = (A_UINT8)getNextDevNum();

		globalNumInstances++;	
		GlobalCmd.cmdID = INIT_F2_CMD_ID;
		GlobalCmd.CMD_U.INIT_F2_CMD.whichF2 = whichDevice;
		if (!artSendCmd(&GlobalCmd,
			 sizeof(GlobalCmd.CMD_U.INIT_F2_CMD) + sizeof(GlobalCmd.cmdID), 
			(void **)&pRegValue) ) 
		{
			uiPrintf("Error: Unable to send command to client! Handle not created.\n");
			return -1;
		}
		devNum = *pRegValue & 0x0fffffff;
		thin_client = (A_BOOL)((*pRegValue) >> 28);
		configSetup.remote_exec = !thin_client;

        if (!configSetup.remote_exec) {
           pdkInfo = (DK_DEV_INFO *)(pRegValue+1);

#ifndef __ATH_DJGPPDOS__
		   devNum= setupDevice(whichDevice, pdkInfo, configSetup.remote);
#else
			devNum= setupDevice(whichDevice);
#endif
#ifndef _IQV
		   if (devNum == -1) {
#else
		   if (devNum > WLAN_MAX_DEV) {
#endif	// _IQV
               uiPrintf("art_setupDevice::Error: invalid devNum from setupDevice\n");
			   return -1;
		   }
#if 0//!defined (_DEBUG)  /* ??? JSC */
		   bootRomVer = (art_ap_reg_read(devNum, 0xbfc00878) & 0xffff);
#else
		   bootRomVer = 0x1234;
#endif
		   bootRomMajVer = bootRomVer >> 8;
		   bootRomMinVer = bootRomVer & 0xff;
    	   uiPrintf("Boot rom version %x.%x\n", bootRomMajVer, bootRomMinVer);
#ifndef _IQV
		   art_createEvent(devNum, ISR_INTERRUPT, 1, 0, 0, 0);
#else
		   if(art_createEvent(devNum, ISR_INTERRUPT, 1, 0, 0, 0)!=0)
				return -1;
#endif //  _IQV
	    }
	}
	if (!configSetup.remote)
	{
#ifndef __ATH_DJGPPDOS__
		devNum= setupDevice(whichDevice, pdkInfo, configSetup.remote);
#else
		devNum= setupDevice(whichDevice);
#endif

#ifndef _IQV
		if (devNum == -1) {
#else
		if (devNum > WLAN_MAX_DEV) {
#endif //  _IQV
			return -1;
		}
	}

	if(checkLibError(devNum, 1)) {
		return -1;
	}
	return devNum;
}



//#ifndef __ATH_DJGPPDOS__
/**************************************************************************
* create_event - Create an event
*
* RETURNS: The event handle for the event.
*/
A_UINT32 art_createEvent
(
	A_UINT32 devNum,
    A_UINT32 type,
    A_UINT32 persistent,
    A_UINT32 param1,
    A_UINT32 param2,
    A_UINT32 param3
)
{
    EVT_HANDLE		eventHdl;
	A_UINT16 devIndex;

        // check to see if we have a valid type
    if (ISR_INTERRUPT != type) 
	{
		uiPrintf("Error: Illegal event type\n");
		return 0xffffffff;
    }

	eventHdl.eventID = 0;
	eventHdl.f2Handle = (A_UINT16)devNum;

	if (!configSetup.remote)
	{
			devIndex = (A_UINT16)dev2drv(devNum);
#ifndef __ATH_DJGPPDOS__
			if (!hwCreateEvent(devIndex, ISR_INTERRUPT, 1, 0, 0, 0, eventHdl)) return 0xffffffff;
#endif
	}
    else
	{

		GlobalCmd.devNum = (A_INT8) devNum;
 		// create cmd to send 
		GlobalCmd.cmdID		= CREATE_EVENT_CMD_ID;
		GlobalCmd.CMD_U.CREATE_EVENT_CMD.type	       = type;
		GlobalCmd.CMD_U.CREATE_EVENT_CMD.persistent    = persistent;
		GlobalCmd.CMD_U.CREATE_EVENT_CMD.param1	       = param1;
		GlobalCmd.CMD_U.CREATE_EVENT_CMD.param2	       = param2;
		GlobalCmd.CMD_U.CREATE_EVENT_CMD.param3	       = param3;
		GlobalCmd.CMD_U.CREATE_EVENT_CMD.eventHandle   = eventHdl;
			
		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.CREATE_EVENT_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
				uiPrintf("Error: Unable to successfully send CREATE_EVENT command to client!\n");
				return 0xffffffff;
		}
	}

	return 0;
}
//#endif

/**************************************************************************
* cfg_read - User interface command for reading a pci configuration register
*
*
* RETURNS: value read
*/
A_UINT32 art_cfgRead
(
	A_UINT32 devNum,
    A_UINT32 regOffset
)
{
     A_UINT32	     *pRegValue, regReturn;
    A_UINT16 devIndex;
    devIndex = (A_UINT16)dev2drv(devNum);

    //uiPrintf("SNOOP::art_cfgRead\n");
	
	if(!configSetup.remote) 
	{
	    if (thin_client) {
			#ifndef __ATH_DJGPPDOS__
			regReturn = hwCfgRead32(devIndex, regOffset);
			#endif
		}
		else {
            regReturn = OScfgRead(devNum, regOffset);
		}
		pRegValue = &regReturn;
	} 
	else 
	{
	    /* create cmd structure and send command */
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.cmdID = CFG_READ_CMD_ID;
		GlobalCmd.CMD_U.CFG_READ_CMD.cfgReadAddr = regOffset;
		GlobalCmd.CMD_U.CFG_READ_CMD.readSize = 32;

		if(!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.CFG_READ_CMD)+sizeof(GlobalCmd.cmdID),
				       (void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send CFG_READ command\n");
			return 0xdeadbeef;
		}
	}

    return(*pRegValue);
}

/**************************************************************************
* cfg_write - User interface command for writing a configuration register
*
*
* RETURNS: 1 if OK, 0 if error
*/
A_UINT32 art_cfgWrite
(
	A_UINT32 devNum,
    A_UINT32 regOffset,
    A_UINT32 regValue
)
{
    
    A_UINT16 devIndex = (A_UINT16)dev2drv(devNum);
    
  //  uiPrintf("SNOOP::art_cfgWrite\n");
	if (!configSetup.remote)
	{
			#ifndef __ATH_DJGPPDOS__
			hwCfgWrite32(devIndex, regOffset, regValue);
			#else
				OScfgWrite(devNum,regOffset,regValue );
			#endif

	} 
	else 
	{
		GlobalCmd.devNum = (A_INT8) devNum;
        /* create cmd structure and send command */
        GlobalCmd.cmdID = CFG_WRITE_CMD_ID;
        GlobalCmd.CMD_U.CFG_WRITE_CMD.cfgWriteAddr = regOffset;
        GlobalCmd.CMD_U.CFG_WRITE_CMD.cfgValue = regValue;
        GlobalCmd.CMD_U.CFG_WRITE_CMD.writeSize = 32;


		if(!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.CFG_WRITE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send CFG_WRITE command\n");
			return 0;
		}
	}
    return 1;
}

A_UINT32 mem_read_block_2048
(
	A_UINT32 devNum,
     A_UINT32 physAddr,
     A_UINT32 length,
     A_UCHAR  *buf
)
{
     A_UINT32        *pReadValues;
    A_UINT16 devIndex = (A_UINT16)dev2drv(devNum);

     /* check to see if the size will make us bigger than the send buffer */
     if (length > MAX_BLOCK_BYTES) {
         uiPrintf("Error: block size too large, can only write %x bytes\n", MAX_BLOCK_BYTES);
         return(0);
     }

	if (!configSetup.remote) {
         /* read the memory */
#ifndef __ATH_DJGPPDOS__
         if(hwMemReadBlock(devIndex, buf,  physAddr, length) == -1) {
            uiPrintf("Failed call to hwMemWriteBlock()\n");
            return(0);
         }
#else
		 uiPrintf(" Not Implemeted in DOS Version \n");
#endif
      } else {

		GlobalCmd.devNum = (A_INT8) devNum;
        /* create cmd structure and send command */
        GlobalCmd.cmdID = MEM_READ_BLOCK_CMD_ID;
        GlobalCmd.CMD_U.MEM_READ_BLOCK_CMD.physAddr = physAddr;
        GlobalCmd.CMD_U.MEM_READ_BLOCK_CMD.length = length;


		if(!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.MEM_READ_BLOCK_CMD)+sizeof(GlobalCmd.cmdID),
				       (void **)&pReadValues)) 
		{
			uiPrintf("Error: Unable to successfully send MEM_READ_BLOCK command to addr %x for % bytes\n", physAddr, length);
			return 0;
		}
        memcpy(buf, pReadValues, length);
       }
   return(1);
}


/**************************************************************************
* mem_read - Command for read a block of memory
*
* RETURNS: value write
*/
A_UINT32 art_memRead 
(
	A_UINT32 devNum,
    A_UINT32 physAddr, 
	A_UCHAR  *bytesRead,
    A_UINT32 length
)
{
    A_UINT16 devIndex;
    A_UINT32 ii, startAddr_ii, len_ii, arrayStart_ii;
    A_UINT32 retAddr=0;

    //uiPrintf("SNOOP::art_memRead:physAddr=%x:length=%d\n", physAddr, length);

    devIndex = (A_UINT16)dev2drv(devNum);
    // Split the writes into blocks of 2048 bytes only if the memory is already allocated
    ii = length;
    startAddr_ii= physAddr;
    arrayStart_ii=0;

    while( ii > 0)
    {
         if(ii > MAX_MEM_CMD_BLOCK_SIZE) {
                  len_ii = MAX_MEM_CMD_BLOCK_SIZE;
              } else {
                  len_ii = ii;
              }
              if (!mem_read_block_2048(devNum, startAddr_ii, len_ii, ((A_UCHAR *)bytesRead+arrayStart_ii))) {
                      bytesRead = NULL;

              }
              startAddr_ii += len_ii;
              ii -= len_ii;
              arrayStart_ii += len_ii;
    }

    return 1;
}

A_UINT32 art_memAlloc
(   
    A_UINT32 allocSize,       /* how many bytes to allocate */
    A_UINT32 physAddr,        /* requested physical address */
    A_UINT32 devNum  /* optionally change select new F2 */
)
{   
    A_UINT32       *pAllocatedAddr, realAddr;
    A_UINT16 devIndex;
    devIndex = (A_UINT16)dev2drv(devNum);

      pAllocatedAddr = &realAddr;

	  if (thin_client) {
			realAddr = memAlloc(devNum, allocSize);
		}
		else {
            if(!configSetup.remote || !configSetup.remote_exec) {
                  /* do the allocation */
#ifndef __ATH_DJGPPDOS__
			      realAddr = memAlloc(devNum, allocSize);
/*                  if(hwGetPhysMem(devIndex, allocSize, &realAddr) == NULL) {
                     return(0);
                  }
				  */
#else
				  uiPrintf("Not Implemented in DOS Version\n");
#endif
             } else {
		            GlobalCmd.devNum = (A_INT8) devNum;
                    /* create the command structure for alloc */
                    GlobalCmd.cmdID = MEM_ALLOC_CMD_ID;
                    GlobalCmd.CMD_U.MEM_ALLOC_CMD.allocSize = allocSize;
                    GlobalCmd.CMD_U.MEM_ALLOC_CMD.physAddr = physAddr;
		            if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.MEM_ALLOC_CMD)+sizeof(GlobalCmd.cmdID),
				             (void **)&pAllocatedAddr))  {
                        uiPrintf("Error: Unable to successfully send MEM_ALLOC command\n");
                        return(0);
                    }
              }
		}

        q_uiPrintf("Memory allocated at physical address %08lx\n", *pAllocatedAddr);

        /* no longer need to pass virtual address back to user, or keep it */
        return(*pAllocatedAddr);
}

void art_memFree (   A_UINT32 fAddr, A_UINT32 devNum  ) {

	  if (thin_client) {
			memFree(devNum, fAddr);
	  }

}

void art_memFreeAll (   A_UINT32 devNum  ) {

	  if (thin_client) {
			memFreeAll(devNum);
	  }

}

A_UINT32 mem_write_block_2048
(
	A_UINT32 devNum,
     A_UINT32 physAddr,
     A_UINT32 length,
     A_UCHAR  *buf
)
{
     A_UINT32        *pAddr;
    A_UINT16 devIndex;
    devIndex = (A_UINT16)dev2drv(devNum);

     /* check to see if the size will make us bigger than the send buffer */
     if (length > MAX_BLOCK_BYTES) {
         uiPrintf("Error: block size too large, can only write %x bytes\n", MAX_BLOCK_BYTES);
         return(0);
     }

	if (!configSetup.remote) {
#ifndef __ATH_DJGPPDOS__
         /* write the memory */
         if(hwMemWriteBlock(devIndex, buf, length,  &(physAddr)) == -1) {
            uiPrintf("Failed call to hwMemWriteBlock()\n");
            return(0);
         }
#else
		 uiPrintf(" NOt Implemented in DOS Version \n");
#endif
         pAddr = &physAddr;
      } else {

		GlobalCmd.devNum = (A_INT8) devNum;
        /* create cmd structure and send command */
        GlobalCmd.cmdID = MEM_WRITE_BLOCK_CMD_ID;
        GlobalCmd.CMD_U.MEM_WRITE_BLOCK_CMD.physAddr = physAddr;
        GlobalCmd.CMD_U.MEM_WRITE_BLOCK_CMD.length = length;
        memcpy(GlobalCmd.CMD_U.MEM_WRITE_BLOCK_CMD.bytes, buf, length);


		if(!artSendCmd(&GlobalCmd,
						(sizeof(GlobalCmd.CMD_U.MEM_WRITE_BLOCK_CMD)+sizeof(GlobalCmd.cmdID) - MAX_BLOCK_BYTES + length),
				       (void **)&pAddr)) 
		{
			uiPrintf("Error: Unable to successfully send MEM_WRITE_BLOCK command\n");
			return 0;
		}
       }
   return(*pAddr);
}


/**************************************************************************
* mem_write - Command for write a block of memory
*
* RETURNS: value write
*/
A_UINT32 art_memWrite
(
	A_UINT32 devNum,
    A_UINT32 physAddr, 
	A_UCHAR  *bytesWrite,
    A_UINT32 length
)
{
    A_UINT32 ii, startAddr_ii, len_ii, arrayStart_ii;
    A_UINT32 retAddr=0;
    A_UINT32 ret;
    A_UINT16 devIndex = (A_UINT16)dev2drv(devNum);

   //uiPrintf("SNOOP::art_memWrite:devNum=%d:physAddr=%x:length=%d\n", devNum, physAddr, length);

    // Split the writes into blocks of 2048 bytes only if the memory is already allocated
    if (!physAddr) {
       physAddr = art_memAlloc(length, 0, devNum);
    }
        ii = length;
        startAddr_ii= physAddr;
        arrayStart_ii=0;

        while( ii > 0)
        {
              if(ii > MAX_MEM_CMD_BLOCK_SIZE) {
                  len_ii = MAX_MEM_CMD_BLOCK_SIZE;
              } else {
                  len_ii = ii;
              }
              ret = mem_write_block_2048(devNum, startAddr_ii, len_ii, ((A_UCHAR *)bytesWrite+arrayStart_ii));
              if (ret == 0) return 0;
              if (retAddr == 0)  retAddr = ret;
              startAddr_ii += len_ii;
              ii -= len_ii;
              arrayStart_ii += len_ii;
        }

    return retAddr;
}



/**************************************************************************
* reg_read - Command for reading a register
*
* RETURNS: value read
*/
A_UINT32 art_regRead
(
	A_UINT32 devNum,
    A_UINT32 regOffset
)
{
    A_UINT32	     *pRegValue, regReturn;
    A_UINT16 devIndex;
    devIndex = (A_UINT16)dev2drv(devNum);

    //sent_bytes=received_bytes=0;
    //uiPrintf("SNOOP::art_regRead:offset=%x:sent_bytes=%d:recd_bytes=%d\n", regOffset, sent_bytes, received_bytes);

	if (!configSetup.remote)
	{
		/* read the register */
	  if (thin_client) {
		#ifndef __ATH_DJGPPDOS__
                    regReturn = hwMemRead32(devIndex, (globDrvInfo.pDevInfoArray[devIndex]->pdkInfo->aregPhyAddr[globDrvInfo.pDevInfoArray[devIndex]->pdkInfo->bar_select]) + regOffset);
		#endif
        //regReturn = hwMemRead32(devIndex, regOffset);
	  }
      else {
        regReturn = REGR(devNum, regOffset);
	  }
	  pRegValue = &regReturn;
	} 
	else 
	{
		GlobalCmd.devNum = (A_INT8) devNum;
		/* create cmd structure and send command */
		GlobalCmd.cmdID = REG_READ_CMD_ID;
		GlobalCmd.CMD_U.REG_READ_CMD.readAddr = regOffset;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.REG_READ_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send REG_READ command\n");
			return 0xdeadbeef;
		}
	}
//	q_uiPrintf("Register at offset %08lx: %08lx\n", regOffset, *pRegValue);
    //uiPrintf("SNOOP::AFTER art_regRead:offset=%x:sent_bytes=%d:recd_bytes=%d\n", regOffset, sent_bytes, received_bytes);

	//if(checkLibError(devNum, 1)) {
	//	return 0xdeadbeef;
	//}

    return(*pRegValue);
}

A_UINT32 art_hwReset(A_UINT32 devNum, A_UINT32 rMask) {

    A_UINT32	     *pRevIdValue;
    A_UINT16 devIndex;
    devIndex = (A_UINT16)dev2drv(devNum);

    //uiPrintf("SNOOP::art_hwReset:rMask=%x\n", rMask);

		GlobalCmd.devNum = (A_INT8) devNum;
		/* create cmd structure and send command */
		GlobalCmd.cmdID = M_HW_RESET_CMD_ID;
		GlobalCmd.CMD_U.HW_RESET_CMD.resetMask = (A_UINT8) rMask;
		if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.HW_RESET_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRevIdValue)) 
		{
			uiPrintf("Error: Unable to successfully send HW_RESET command\n");
			return 0xdeadbeef;
		}
	if(checkLibError(devNum, 1)) {
		return 0xdeadbeef;
    }

    return(*pRevIdValue);

}

void art_pllProgram(A_UINT32 devNum, A_UINT32 turbo, A_UINT32 mode) {

    A_UINT32	     *pRevIdValue;
    A_UINT16 devIndex;
    devIndex = (A_UINT16)dev2drv(devNum);

		GlobalCmd.devNum = (A_INT8) devNum;
		/* create cmd structure and send command */
		GlobalCmd.cmdID = M_PLL_PROGRAM_CMD_ID;
		GlobalCmd.CMD_U.PLL_PROGRAM_CMD.turbo = (A_UINT8) turbo;
		GlobalCmd.CMD_U.PLL_PROGRAM_CMD.mode = (A_UINT8) mode;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.PLL_PROGRAM_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRevIdValue)) 
		{
			uiPrintf("Error: Unable to successfully send PLL_PROGRAM command\n");
		}
}

#ifndef _IQV
void art_pciWrite(A_UINT32 devNum, PCI_VALUES *pPciValues, A_UINT32 length) {
#else
A_UINT32 art_pciWrite(A_UINT32 devNum, PCI_VALUES *pPciValues, A_UINT32 length) {
#endif
	A_UINT32 dataSize;

    GlobalCmd.devNum = (A_INT8) devNum;
    GlobalCmd.cmdID = M_PCI_WRITE_CMD_ID;
    GlobalCmd.CMD_U.PCI_WRITE_CMD.length = length;
	memcpy(GlobalCmd.CMD_U.PCI_WRITE_CMD.pPciValues, pPciValues, length * sizeof(PCI_VALUES));

	dataSize = sizeof(GlobalCmd.CMD_U.PCI_WRITE_CMD) - ((MAX_PCI_ENTRIES-length) * sizeof(PCI_VALUES));

    if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID)+ dataSize, NULL))
    {
            uiPrintf("Error: Unable to successfully send PCI_WRITE command\n");
#ifdef _IQV
			return FALSE;
#endif
    }
#ifdef _IQV
	return TRUE;
#endif
}

A_UINT32 art_calCheck (A_UINT32 devNum, A_UINT32 enableCal, A_UINT32 timeout ) {
    A_UINT32	    *pRegValue;

    GlobalCmd.devNum = (A_INT8) devNum;
    GlobalCmd.cmdID = M_CAL_CHECK_CMD_ID;
    GlobalCmd.CMD_U.CAL_CHECK_CMD.enableCal = enableCal;
    GlobalCmd.CMD_U.CAL_CHECK_CMD.timeout = timeout;

    if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.CAL_CHECK_CMD)+sizeof(GlobalCmd.cmdID), (void **)&pRegValue))
    {
            uiPrintf("Error: Unable to successfully send CAL_CHECK command\n");
			return(0xdeadbeef);
    }
		return(*pRegValue);


}

/**************************************************************************
* reg_write - User interface command for writing a register
*
*
* RETURNS: 1 if OK, 0 if error
*/
A_UINT32 art_regWrite
(
	A_UINT32 devNum,
    A_UINT32 regOffset,
    A_UINT32 regValue
)
{
    
    A_UINT16 devIndex = (A_UINT16)dev2drv(devNum);

    //sent_bytes=received_bytes=0;
    //uiPrintf("SNOOP::art_regWrite:offset=%x\n", regOffset);
    //uiPrintf("SNOOP::art_regWrite:offset=%x:sent_bytes=%d:recd_bytes=%d\n", regOffset, sent_bytes, received_bytes);

	if (!configSetup.remote)
	{
	if (thin_client) {
#ifndef __ATH_DJGPPDOS__
       hwMemWrite32(devIndex, (globDrvInfo.pDevInfoArray[devIndex]->pdkInfo->aregPhyAddr[globDrvInfo.pDevInfoArray[devIndex]->pdkInfo->bar_select]) + regOffset, regValue); 
#endif
	 }
     else {
       REGW(devNum,regOffset,regValue);
	 }
	} 
	else 
	{
		GlobalCmd.devNum = (A_INT8) devNum;
		/* create cmd structure and send command */
		GlobalCmd.cmdID = REG_WRITE_CMD_ID;
		GlobalCmd.CMD_U.REG_WRITE_CMD.writeAddr = regOffset;
		GlobalCmd.CMD_U.REG_WRITE_CMD.regValue = regValue;

		if(!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.REG_WRITE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send REG_WRITE command\n");
			return 0;
		}
	}
    //uiPrintf("SNOOP::AFTER art_regWrite:offset=%x:sent_bytes=%d:recd_bytes=%d\n", regOffset, sent_bytes, received_bytes);
    return 1;
}



/**************************************************************************
* reg_mem32Write - 
*
* RETURNS: 1 if OK, 0 if error
*/
A_UINT32 art_mem32Write
(
    A_UINT32 devNum,
    A_UINT32 regAddr,
    A_UINT32 regValue
)
{
    A_UINT16 devIndex = (A_UINT16)dev2drv(devNum);

    GlobalCmd.devNum = (A_INT8) devNum;
    /* create cmd structure and send command */
    GlobalCmd.cmdID = MEM_WRITE_CMD_ID;
    GlobalCmd.CMD_U.MEM_WRITE_CMD.writeAddr = regAddr;
    GlobalCmd.CMD_U.MEM_WRITE_CMD.writeValue = regValue;
    GlobalCmd.CMD_U.MEM_WRITE_CMD.writeSize = 32;

    if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.MEM_WRITE_CMD)+sizeof(GlobalCmd.cmdID), NULL)) 
    {
        uiPrintf("Error: Unable to successfully send MEM_WRITE command\n");
        return 0;
    }
    //uiPrintf("..art_mem32Write:addr=0x%08lx\n", regAddr);
    return 1;
}




/**************************************************************************
* art_mem32Read - 
*
* RETURNS: value read
*/
A_UINT32 art_mem32Read
(
    A_UINT32 devNum,
    A_UINT32 regAddr
)
{
    A_UINT32 *pRegValue, regReturn;
    A_UINT16 devIndex;
    devIndex = (A_UINT16)dev2drv(devNum);

    GlobalCmd.devNum = (A_INT8) devNum;
    /* create cmd structure and send command */
    GlobalCmd.cmdID = MEM_READ_CMD_ID;
    GlobalCmd.CMD_U.MEM_READ_CMD.readAddr = regAddr;
    GlobalCmd.CMD_U.MEM_READ_CMD.readSize = 32;
    if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.MEM_READ_CMD)+sizeof(GlobalCmd.cmdID), (void **)&pRegValue)) 
    {
        uiPrintf("Error: Unable to successfully send MEM_READ command\n");
        return 0xdeadbeef;
    }
    //uiPrintf("..art_mem32Read: addr=0x%08lx, val=0x%08lx\n", regAddr, *pRegValue);
    return(*pRegValue);
}


void art_setResetParams
(
 A_UINT32   devNum,
 A_CHAR     *pFilename,
 A_BOOL		eePromLoad,
 A_BOOL		forceCfgLoad,
 A_UCHAR	mode,
 A_UINT16     initCodeFlag 
)
{
	A_UINT32 *pRegValue;

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		setResetParams(devNum,pFilename,eePromLoad, forceCfgLoad, mode, initCodeFlag);
	}
	else
	{
			// create cmd to send to client
		GlobalCmd.cmdID = M_SET_RESET_PARAMS_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_RESET_PARAMS_CMD.eePromLoad = (A_UINT32)eePromLoad;
		GlobalCmd.CMD_U.SET_RESET_PARAMS_CMD.forceCfgLoad = (A_UINT32)forceCfgLoad;
		GlobalCmd.CMD_U.SET_RESET_PARAMS_CMD.mode = (A_UINT32)mode;
		GlobalCmd.CMD_U.SET_RESET_PARAMS_CMD.use_init = (A_UINT16)initCodeFlag;
		if (pFilename) strcpy(GlobalCmd.CMD_U.SET_RESET_PARAMS_CMD.fileName, pFilename);
		else GlobalCmd.CMD_U.SET_RESET_PARAMS_CMD.fileName[0] = '\0';

		if ( !artSendCmd(&GlobalCmd,
						 sizeof(GlobalCmd.CMD_U.SET_RESET_PARAMS_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) {
			uiPrintf("Error: Unable to successfully send SET_RESET_PARAMS command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

A_UINT32 art_resetDevice
(
	A_UINT32 devNum,
	A_UCHAR *mac, 
	A_UCHAR *bss, 
	A_UINT32 freq,
	A_UINT32 turbo
)
{
#ifdef _IQV
	ex_timeStart();	
#endif	// _IQV
    //if a quarter channel has been requested, then add the flag onto turbo
	//do it here, so can contain the changes to one place
        //uiPrintf("qc %d, rem %d remExe %d\n", configSetup.quarterChannel, configSetup.remote, configSetup.remote_exec);
	if(configSetup.quarterChannel) {
		freq = freq * 10 + 25;
	}

	if(!configSetup.remote || !configSetup.remote_exec)
	{
			resetDevice(devNum, mac, bss, freq, turbo);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RESET_DEVICE_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		memcpy(GlobalCmd.CMD_U.RESET_DEVICE_CMD.mac, mac, 6);
		memcpy(GlobalCmd.CMD_U.RESET_DEVICE_CMD.bss, bss, 6);
		GlobalCmd.CMD_U.RESET_DEVICE_CMD.freq = freq;
		GlobalCmd.CMD_U.RESET_DEVICE_CMD.turbo = turbo;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.RESET_DEVICE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send RESET_DEVICE_CMD command to client, exiting!!\n");
			closeEnvironment();	
#ifndef _IQV
			exit(0);
#endif	// _IQV
		}
	}

	if(checkLibError(devNum, 1)) {
		uiPrintf("Error: resetDevice command was unsuccessful, exiting!!\n");
//		exit(0);
#ifdef _IQV
 		ex_timeDiff("art_resetDevice");
		return 0;
#endif	// _IQV
	}
	//printf("SNOOP: writing pll\n");
	//Sleep(5);
	//art_ap_reg_write(devNum, 0x0c000014, 0x502c);
	//printf("SNOOP: pll written\n");
	//Sleep(10);
#ifdef _IQV
 	ex_timeDiff("art_resetDevice");
#endif	// _IQV
	return 1;
}

void art_getDeviceInfo
(
 A_UINT32 devNum,
 SUB_DEV_INFO *devStruct
)
{
	A_UCHAR  *pReadValues;		  /* pointer to the values that were read */

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		getDeviceInfo(devNum, devStruct);
	}
	else {
				// create cmd to send to client
		GlobalCmd.cmdID = M_GET_DEVICE_INFO_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		
		if ( !artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID),
						(void **)&pReadValues)) 
		{
				uiPrintf("Error: Unable to successfully send GET_DEVICE_INFO_CMD command to client!\n");
				return;
		}
		if(checkLibError(devNum, 1)) return;
		memcpy((void *)devStruct,(void *)pReadValues,sizeof(SUB_DEV_INFO));
	}
	checkLibError(devNum, 1);
}

A_UINT32 art_eepromRead
(
	A_UINT32  devNum,
	A_UINT32 eepromOffset
)
{
    A_UINT32 	*pRegValue;
    A_UINT32	eepromValue;
    if (!configSetup.eepromPresent && isMercury_sd(swDeviceID)) {
        //uiPrintf("...eepromPresent %d\n", configSetup.eepromPresent);
        eepromValue= *((A_UINT32 *) &dummyEepromWriteArea[eepromOffset *2]);
    }
    else {
	if (!configSetup.remote)
	{
		eepromValue = eepromRead(devNum, eepromOffset);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_EEPROM_READ_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.EEPROM_READ_CMD.offset	= eepromOffset;

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.EEPROM_READ_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
				uiPrintf("Error: Unable to successfully send EEPROM_READ_CMD command to client!\n");
				return 0xdeadbeef;
		}
		eepromValue = *pRegValue;
        //printf("SNOOP::art_eepromRead::eepromValue=%x:*pRegValue=%x\n", eepromValue, *pRegValue);
	}
	if(checkLibError(devNum, 1)) {
		return 0xdeadbeef;
	}
    }
    return eepromValue;
}

void art_eepromWrite
(
	A_UINT32 devNum,
	A_UINT32 eepromOffset,
	A_UINT32 eepromValue
)
{
    if (!configSetup.eepromPresent && isMercury_sd(swDeviceID)) {
        uiPrintf("..No art_eepromWrite, eepromPresent %d\n", configSetup.eepromPresent);
    }
    else {
    if (!configSetup.remote)
	{
			eepromWrite(devNum, eepromOffset, eepromValue);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_EEPROM_WRITE_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.EEPROM_WRITE_CMD.offset = eepromOffset;
		GlobalCmd.CMD_U.EEPROM_WRITE_CMD.value = eepromValue;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.EEPROM_WRITE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send EEPROM_WRITE_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
    }
}

static A_INT32 art_eepromWriteBlock_256
(
	A_UINT32 devNum,
	A_UINT32 startOffset,
	A_UINT32 length,
	A_UINT32 *buf
)
{
    if (!configSetup.eepromPresent && isMercury_sd(swDeviceID)) {
        uiPrintf("..No art_eepromWriteBlock_256, eepromPresent %d\n", configSetup.eepromPresent);
    }
    else {
	if (!configSetup.remote) {
		eepromWriteBlock(devNum,startOffset,length,buf);
	} else {
		/* setup the command struct to send */
		GlobalCmd.cmdID = M_EEPROM_WRITE_BLOCK_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.EEPROM_WRITE_BLOCK_CMD.length = length;
		GlobalCmd.CMD_U.EEPROM_WRITE_BLOCK_CMD.startOffset = startOffset;
		memcpy(GlobalCmd.CMD_U.EEPROM_WRITE_BLOCK_CMD.eepromValue, buf, length * 4);
	
		/* send the command.  Note that the size to send is only for num bytes want to write */
		if(!artSendCmd(&GlobalCmd,
					   (sizeof(GlobalCmd.CMD_U.EEPROM_WRITE_BLOCK_CMD) + sizeof(GlobalCmd.cmdID) - ((MAX_BLOCK_DWORDS - length)*4)),
					   NULL)) {
				uiPrintf("Error: Unable to successfully send EEPROM_WRITE_BLOCK command\n");
				return 0;
		}
	}

	if (checkLibError(devNum, 1)) return 0;
    }
    return 1;
}

void art_eepromWriteBlock
(
	A_UINT32 devNum,
	A_UINT32 startOffset,
	A_UINT32 length,
	A_UINT32 *buf
)
{
	A_UINT32 ii = length, startAddr_ii=startOffset, len_ii, arrayStart_ii=0;
    A_UINT32 max_locs_per_write; 

	if (sdio_client) 
    	max_locs_per_write = 256;
	else 
    	max_locs_per_write = 256;

	while( ii > 0)
	{
		if(ii > (max_locs_per_write-1))
		{
			len_ii = max_locs_per_write;
		} else
		{
			len_ii = ii;
		}
		if (!art_eepromWriteBlock_256(devNum, startAddr_ii, len_ii, &(buf[arrayStart_ii]))) break;
		startAddr_ii += len_ii;
		ii -= len_ii;
		arrayStart_ii += len_ii;
	}

	return;
}

static A_INT32 art_eepromReadBlock_256
(
	A_UINT32 devNum,
	A_UINT32 startOffset,
	A_UINT32 length,
	A_UINT32 *buf
)
{	
    if (!configSetup.eepromPresent && isMercury_sd(swDeviceID)) {
        uiPrintf("..No art_eepromReadBlock_256, eepromPresent %d\n", configSetup.eepromPresent);
    }
    else {
        A_UCHAR  *pReadValues;		  /* pointer to the values that were read */
	

	//printf("SNOOP: setting eeprom mode\n");
	//art_ap_reg_write(devNum, 0x0c004004, 0x90020044);
	if(!configSetup.remote) {
		eepromReadBlock(devNum,startOffset,length,buf);
	} else {
		/* setup the command struct to send */
		GlobalCmd.cmdID = M_EEPROM_READ_BLOCK_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.EEPROM_READ_BLOCK_CMD.length = length;
		GlobalCmd.CMD_U.EEPROM_READ_BLOCK_CMD.startOffset = startOffset;
			
			    /* send the command.  Note that the size to send is only for num bytes want to write */
		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.EEPROM_READ_BLOCK_CMD) + sizeof(GlobalCmd.cmdID)),
					(void **)(&pReadValues))) {
				uiPrintf("Error: Unable to successfully send EEPROM_READ_BLOCK command\n");
				return 0;
		}
		if(checkLibError(devNum, 1)) return 0;
		memcpy((void *)buf,(void *)pReadValues, length * 4);
        }
        if (checkLibError(devNum, 1)) return 0;
    }
    return 1;
}

void art_eepromReadBlock
(
	A_UINT32 devNum,
	A_UINT32 startOffset,
	A_UINT32 length,
	A_UINT32 *buf
)
{
	A_UINT32 ii = length, startAddr_ii=startOffset, len_ii, arrayStart_ii=0;
    A_UINT32 max_locs_per_read;

	if (sdio_client) 
    	max_locs_per_read = 128;
	else 
    	max_locs_per_read = 128;  //for mercury, still need 128 bytes


	while( ii > 0)
	{
		if(ii > (max_locs_per_read-1))
		{
			len_ii = max_locs_per_read;
		} else
		{
			len_ii = ii;
		}
		if (!art_eepromReadBlock_256(devNum, startAddr_ii, len_ii, &(buf[arrayStart_ii]))) return;
		startAddr_ii += len_ii;
		ii -= len_ii;
		arrayStart_ii += len_ii;
	}

	return;
}

void art_eepromReadLocs
(
	A_UINT32 devNum,
	A_UINT32 startOffset,
	A_UINT32 length,
	A_UINT8 *buf
)
{
    if (!configSetup.eepromPresent && isMercury_sd(swDeviceID)) {
        memcpy((void*)buf, (void*)&dummyEepromWriteArea[startOffset *2], (length *2));
        //uiPrintf("..art_eepromReadLocs, eepromPresent %d\n", configSetup.eepromPresent);
    }
    else {
         A_UCHAR  *pReadValues;		  /* pointer to the values that were read */
	

		/* setup the command struct to send */
		GlobalCmd.cmdID = M_EEPROM_READ_LOCS_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.EEPROM_READ_LOCS_CMD.length = length;
		GlobalCmd.CMD_U.EEPROM_READ_LOCS_CMD.startOffset = startOffset;
			
			    /* send the command.  Note that the size to send is only for num bytes want to write */
		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.EEPROM_READ_LOCS_CMD) + sizeof(GlobalCmd.cmdID)),
					(void **)(&pReadValues))) {
				uiPrintf("Error: Unable to successfully send EEPROM_READ_LOCS command\n");
				return ;
		}
		if(checkLibError(devNum, 1)) { uiPrintf("remote command error\n"); return ;}
		memcpy((void *)buf,(void *)pReadValues, length * NUM_BYTES_PER_EEPROM_LOC);
    }
}

#if 0
void art_eepromWriteLocs
(
	A_UINT32 devNum,
	A_UINT32 startOffset,
	A_UINT32 length,
	A_UINT8 *buf
)
{
    A_UCHAR  *pReadValues;		  /* pointer to the values that were read */
	A_UINT32 *pBuffer;
	
	//allocate a 32 bit value buffer for sending regular eeprom write command
	pBuffer = malloc(sizeof(A_UINT32) * length);
	if(pBuffer == NULL) {
		uiPrintf("Error: unable to allocate buffer for sending eeprom write, eepromWriteLocs\n");
		return;
	}
	
	for (i = 0; i < length; i++) {
		pBuffer[i] = buf[i];
	}
	
	art_eepromWriteBlock
}
#endif

A_UINT32 art_checkRegs
(
	A_UINT32 devNum
)
{
	A_UINT32	    *pRegValue;
	A_UINT32		regValue;

	if(!configSetup.remote || !configSetup.remote_exec)
	{
			regValue = checkRegs(devNum);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_CHECK_REGS_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;

		if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send M_CHECK_REGS_CMD_ID command to client!\n");
			return 0xdeadbeef;
		}
		regValue = *pRegValue;
	}
	if(checkLibError(devNum, 1)) {
		return 0xdeadbeef;
	}
	return regValue;
}


A_UINT32 art_checkProm
(
	A_UINT32 devNum,
	A_UINT32 enablePrint
)
{
	A_UINT32	    *pRegValue;
	A_UINT32		regValue;

	if(!configSetup.remote || !configSetup.remote_exec)
	{
		regValue = checkProm(devNum, enablePrint);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_CHECK_PROM_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = enablePrint;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send CHECK_PROM_CMD command to client!\n");
			return(1);
		}
		regValue = *pRegValue;
	}
	if(checkLibError(devNum, 1)) {
		return 1;
	}
	return regValue;
}

void art_enableHwCal
(
 A_UINT32 devNum,	
 A_UINT32 calFlag
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{
		enableCal = calFlag;
	}
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = ENABLE_HW_CAL_CMD;
		GlobalCmd.devNum = (A_INT8) calFlag;

		if (!artSendCmd(&GlobalCmd,sizeof(GlobalCmd.cmdID), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send ENABLE_HW_CAL_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);

}

void art_rereadProm
(
	A_UINT32 devNum
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{
		    rereadProm(devNum);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_REREAD_PROM_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;

		if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send REREAD_PROM_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_changeChannel
(
	A_UINT32 devNum,
	A_UINT32 freq
)
{
    
	if(!configSetup.remote || !configSetup.remote_exec) {
		changeChannel(devNum, freq);
	} else {
		// create cmd to send to client
		GlobalCmd.cmdID = M_CHANGE_CHANNEL_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = freq;

		if ( !artSendCmd(&GlobalCmd,
			 sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID), NULL)) {
			uiPrintf("Error: Unable to successfully send CHANGE_CHANNEL_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}


void art_txDataSetup
(
	A_UINT32 devNum,
	A_UINT32 rateMask,
	A_UCHAR *dest, 
	A_UINT32 numDescPerRate,
	A_UINT32 dataBodyLength,
	A_UCHAR *dataPattern, 
	A_UINT32 dataPatternLength, 
	A_UINT32 retries,
	A_UINT32 antenna,
	A_UINT32 broadcast
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{	
			txDataSetup(devNum, rateMask, dest, numDescPerRate, dataBodyLength,
				dataPattern, dataPatternLength, retries, antenna, broadcast);
	} else {
		// create cmd to send to client
		GlobalCmd.cmdID = M_TX_DATA_SETUP_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.rateMask = rateMask;
		memcpy(GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.dest, dest, 6);
		GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.numDescPerRate = numDescPerRate;
		GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.dataBodyLength = dataBodyLength;
		memcpy(GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.dataPattern, dataPattern, dataPatternLength);
		GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.dataPatternLength = dataPatternLength;
		GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.retries = retries;
		GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.antenna = antenna;
		GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.broadcast = broadcast;


		if (!artSendCmd(&GlobalCmd,
						(sizeof(GlobalCmd.CMD_U.TX_DATA_SETUP_CMD)+
						sizeof(GlobalCmd.cmdID)-
						MAX_BLOCK_BYTES+
						GlobalCmd.CMD_U.TX_DATA_SETUP_CMD.dataPatternLength), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send TX_DATA_SETUP_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_txDataSetupNoEndPacket
(
	A_UINT32 devNum,
	A_UINT32 rateMask,
	A_UCHAR *dest, 
	A_UINT32 numDescPerRate,
	A_UINT32 dataBodyLength,
	A_UCHAR *dataPattern, 
	A_UINT32 dataPatternLength, 
	A_UINT32 retries,
	A_UINT32 antenna,
	A_UINT32 broadcast
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{	
			txDataSetupNoEndPacket(devNum, rateMask, dest, numDescPerRate, dataBodyLength,
				dataPattern, dataPatternLength, retries, antenna, broadcast);
	} else {
#if 0
		// create cmd to send to client
		GlobalCmd.cmdID = M_TX_DATA_SETUP_NO_END_PACKET_CMD_ID;
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.devNum = devNum;
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.rateMask = rateMask;
		memcpy(GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.dest, dest, 6);
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.numDescPerRate = numDescPerRate;
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.dataBodyLength = dataBodyLength;
		memcpy(GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.dataPattern, dataPattern, dataPatternLength);
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.dataPatternLength = dataPatternLength;
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.retries = retries;
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.antenna = antenna;
		GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.broadcast = broadcast;


		if (!artSendCmd(&GlobalCmd,
						(sizeof(GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD)+
						sizeof(GlobalCmd.cmdID)-
						MAX_BLOCK_BYTES+
						GlobalCmd.CMD_U.TX_DATA_SETUP_NO_END_PACKET_CMD.dataPatternLength), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send TX_DATA_SETUP_NO_END_PACKET_CMD command to client!\n");
		}
#endif
	}
	checkLibError(devNum, 0);
}

void art_txDataBegin
(
	A_UINT32 devNum,
	A_UINT32 timeout,
	A_UINT32 remoteStats
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
    {
			txDataBegin(devNum, timeout, remoteStats);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_TX_DATA_BEGIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.TX_DATA_BEGIN_CMD.timeout = timeout;
		GlobalCmd.CMD_U.TX_DATA_BEGIN_CMD.remoteStats = remoteStats;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.TX_DATA_BEGIN_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send TX_DATA_BEGIN_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_rxDataSetup
(
	A_UINT32 devNum,
	A_UINT32 numDesc,
	A_UINT32 dataBodyLength,
	A_UINT32 enablePPM
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{		
		rxDataSetup(devNum, numDesc, dataBodyLength, enablePPM);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RX_DATA_SETUP_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.RX_DATA_SETUP_CMD.numDesc = numDesc;
		GlobalCmd.CMD_U.RX_DATA_SETUP_CMD.dataBodyLength = dataBodyLength;
		GlobalCmd.CMD_U.RX_DATA_SETUP_CMD.enablePPM = enablePPM;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.RX_DATA_SETUP_CMD)+sizeof(GlobalCmd.cmdID), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send RX_DATA_SETUP_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_rxDataBegin
(
	A_UINT32 devNum,
	A_UINT32 waitTime,
	A_UINT32 timeout,
	A_UINT32 remoteStats,
	A_UINT32 enableCompare,
	A_UCHAR *dataPattern, 
	A_UINT32 dataPatternLength
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{
			rxDataBegin(devNum, waitTime, timeout, remoteStats, enableCompare, dataPattern, dataPatternLength);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RX_DATA_BEGIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.waitTime = waitTime;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.timeout = timeout;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.remoteStats = remoteStats;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.enableCompare = enableCompare;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPatternLength = dataPatternLength;
		memcpy(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPattern, dataPattern, dataPatternLength);

		if (!artSendCmd(&GlobalCmd,
						(sizeof(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD) +
						sizeof(GlobalCmd.cmdID) -
						MAX_BLOCK_BYTES +
						GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPatternLength), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send RX_DATA_BEGIN_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

//version of art_rxDataBegin that works with Signal Generators
void art_rxDataBeginSG
(
	A_UINT32 devNum,
	A_UINT32 waitTime,
	A_UINT32 timeout,
	A_UINT32 remoteStats,
	A_UINT32 enableCompare,
	A_UCHAR *dataPattern, 
	A_UINT32 dataPatternLength,
	A_UINT32 sgpacketnumber
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{
			rxDataBeginSG(devNum, waitTime, timeout, remoteStats, enableCompare, dataPattern, dataPatternLength, sgpacketnumber);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RX_DATA_BEGIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.waitTime = waitTime;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.timeout = timeout;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.remoteStats = remoteStats;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.enableCompare = enableCompare;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPatternLength = dataPatternLength;
		memcpy(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPattern, dataPattern, dataPatternLength);

		if (!artSendCmd(&GlobalCmd,
						(sizeof(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD) +
						sizeof(GlobalCmd.cmdID) -
						MAX_BLOCK_BYTES +
						GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPatternLength), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send RX_DATA_BEGIN_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_rxDataComplete
(
	A_UINT32 devNum,
	A_UINT32 waitTime,
	A_UINT32 timeout,
	A_UINT32 remoteStats,
	A_UINT32 enableCompare,
	A_UCHAR *dataPattern, 
	A_UINT32 dataPatternLength
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{
		rxDataComplete(devNum, waitTime, timeout, remoteStats, enableCompare, dataPattern, dataPatternLength);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RX_DATA_COMPLETE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.waitTime = waitTime;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.timeout = timeout;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.remoteStats = remoteStats;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.enableCompare = enableCompare;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPatternLength = dataPatternLength;
		memcpy(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPattern, dataPattern, dataPatternLength);

		if (!artSendCmd(&GlobalCmd,
						(sizeof(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD) +
						sizeof(GlobalCmd.cmdID) -
						MAX_BLOCK_BYTES +
						GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPatternLength), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send RX_DATA_COMPLETE_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_rxGetData
(
 A_UINT32 devNum, 
 A_UINT32 bufferNum, 
 A_UCHAR *pReturnBuffer, 
 A_UINT32 sizeBuffer
)
{
    A_UCHAR  *pReadValues;		  /* pointer to the values that were read */

	if(!configSetup.remote || !configSetup.remote_exec)
	{		
		rxGetData(devNum, bufferNum, pReturnBuffer, sizeBuffer);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RX_GET_DATA_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.RX_GET_DATA_CMD.bufferNum = bufferNum;
		GlobalCmd.CMD_U.RX_GET_DATA_CMD.sizeBuffer = sizeBuffer;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.RX_GET_DATA_CMD)+sizeof(GlobalCmd.cmdID), 
						(void **)&pReadValues)) 
		{
			uiPrintf("Error: Unable to successfully send RX_GET_DATA_CMD command to client!\n");
		}
		if(checkLibError(devNum, 1)) return;
		memcpy(pReturnBuffer, pReadValues, sizeBuffer);
	}
	checkLibError(devNum, 1);
}


A_BOOL art_rxLastDescStatsSnapshot
(
 A_UINT32 devNum, 
 RX_STATS_SNAPSHOT *pRxStats
)
{
	A_BOOL returnFlag = TRUE;
	A_UCHAR *pReadValues;

	if(!configSetup.remote || !configSetup.remote_exec)
	{
		returnFlag = rxLastDescStatsSnapshot(devNum, pRxStats);
	}
	else
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RX_STATS_SNAPSHOT_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID),
						(void **)&pReadValues)) 
		{
			uiPrintf("Error: Unable to successfully send M_RX_STATS_SNAPSHOT_CMD command to client!\n");
			return 0;
			
		}
		if(checkLibError(devNum, 1)) return 0;
		//get the returnFlag from the start of the return buffer
		returnFlag = *pReadValues;
		
		pReadValues+=4;
		/* copy the data from command receive buffer into buffer */
		memcpy(pRxStats, pReadValues, sizeof(RX_STATS_SNAPSHOT));
	}
	if(checkLibError(devNum, 1)) {
		return 0;
	}
	return(returnFlag);
}

void art_cleanupTxRxMemory
(
 A_UINT32 devNum,
 A_UINT32 flags
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{
		cleanupTxRxMemory(devNum, flags);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_CLEANUP_TXRX_MEMORY_CMD_ID;
		GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = flags;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send CLEANUP_TXRX_MEMORY command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_txrxDataBegin
(
	A_UINT32 devNum,
	A_UINT32 waitTime,
	A_UINT32 timeout,
	A_UINT32 remoteStats,
	A_UINT32 enableCompare,
	A_UCHAR *dataPattern, 
	A_UINT32 dataPatternLength
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		txrxDataBegin(devNum, waitTime, timeout, remoteStats, enableCompare, dataPattern, dataPatternLength);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_TXRX_DATA_BEGIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.waitTime = waitTime;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.timeout = timeout;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.remoteStats = remoteStats;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.enableCompare = enableCompare;
		GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPatternLength = dataPatternLength;
		memcpy(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD.dataPattern, dataPattern, dataPatternLength);

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.RX_DATA_BEGIN_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send TXRX_DATA_BEGIN_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_setAntenna
(
	A_UINT32 devNum,
	A_UINT32 antenna
)
{
	if(!configSetup.remote || !configSetup.remote_exec)
	{
	    setAntenna(devNum, antenna);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_SET_ANTENNA_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = antenna;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send SET_ANTENNA_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}


// Continuous Transmit Functions
void art_txContBegin
(
	A_UINT32 devNum,
	A_UINT32 type,
	A_UINT32 typeOption1,
	A_UINT32 typeOption2,
	A_UINT32 antenna
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
			txContBegin(devNum, type, typeOption1, typeOption2, antenna);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_TX_CONT_BEGIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.TX_CONT_BEGIN_CMD.type = type;
		GlobalCmd.CMD_U.TX_CONT_BEGIN_CMD.typeOption1 = typeOption1;
		GlobalCmd.CMD_U.TX_CONT_BEGIN_CMD.typeOption2 = typeOption2;
		GlobalCmd.CMD_U.TX_CONT_BEGIN_CMD.antenna = antenna;

		if (!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.TX_CONT_BEGIN_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send TX_CONT_BEGIN_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

// agehari
void art_txContFrameBegin
(
	A_UINT32 devNum,
	A_UINT32 length,
	A_UINT32 ifswait,
	A_UINT32 typeOption1,
	A_UINT32 typeOption2,
	A_UINT32 antenna,
	A_BOOL   performStabilizePower,
	A_UINT32 numDescriptors,
	A_UCHAR *dest
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		txContFrameBegin(devNum, length, ifswait, typeOption1, typeOption2, antenna, performStabilizePower, numDescriptors, dest);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_TX_CONT_FRAME_BEGIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.length = length;
		GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.ifswait = ifswait;
		GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.typeOption1 = typeOption1;
		GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.typeOption2 = typeOption2;
		GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.antenna = antenna;
		GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.performStabilizePower = performStabilizePower;
		GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.numDescriptors = numDescriptors;
		if(dest) {
			memcpy(GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.dest, dest, 6);
		}
		else {
			memset(GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD.dest, 0, 6);
		}

		if (!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.TX_CONT_FRAME_BEGIN_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send TX_CONT_FRAME_BEGIN_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_txContEnd
(
	A_UINT32 devNum
)
{
	
	if (!configSetup.remote || !configSetup.remote_exec)
	{
			txContEnd(devNum);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_TX_CONT_END_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send TX_CONT_END_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_txGetStats
(
 A_UINT32 devNum, 
 A_UINT32 rateInMb,
 A_UINT32 remote,
 TX_STATS_STRUCT *pReturnStats 
)
{
    A_UCHAR  *pReadValues;		  /* pointer to the values that were read */
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		txGetStats(devNum, rateInMb, remote, pReturnStats);
	}
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_TX_GET_STATS_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_STATS_CMD.rateInMb = rateInMb;
		GlobalCmd.CMD_U.GET_STATS_CMD.remote = remote;

		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.GET_STATS_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pReadValues)) 
		{
			uiPrintf("Error: Unable to successfully send TX_GET_STATS_CMD command to client!\n");
			
		}
		if(checkLibError(devNum, 1)) return;
		/* copy the data from command receive buffer into buffer for perl */
		memcpy(pReturnStats, pReadValues, sizeof(TX_STATS_STRUCT));
	}
	checkLibError(devNum, 1);
	return;
}

void art_rxGetStats
(
	A_UINT32 devNum,
	A_UINT32 rateInMb,
	A_UINT32 remote,
	RX_STATS_STRUCT *pReturnStats 
)
{
	    A_UCHAR  *pReadValues;		  /* pointer to the values that were read */

		if (!configSetup.remote || !configSetup.remote_exec)
		{
				rxGetStats(devNum, rateInMb, remote, pReturnStats);
		}
		else 
		{
			// create cmd to send to client
			GlobalCmd.cmdID = M_RX_GET_STATS_CMD_ID;
            GlobalCmd.devNum = (A_INT8) devNum;
			GlobalCmd.CMD_U.GET_STATS_CMD.rateInMb = rateInMb;
			GlobalCmd.CMD_U.GET_STATS_CMD.remote = remote;

			if (!artSendCmd(&GlobalCmd,
							sizeof(GlobalCmd.CMD_U.GET_STATS_CMD)+sizeof(GlobalCmd.cmdID),
							(void **)&pReadValues)) 
			{
				uiPrintf("Error: Unable to successfully send RX_GET_STATS_CMD command to client!\n");
				
			}
		if(checkLibError(devNum, 1)) return;
			/* copy the data from command receive buffer into buffer for perl */
			memcpy(pReturnStats, pReadValues, sizeof(RX_STATS_STRUCT));
		}
	checkLibError(devNum, 1);
}

void art_rxDataStart
(
 A_UINT32 devNum
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		rxDataStart(devNum);
	}
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_RX_DATA_START_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;

		if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID), NULL)) 
		{
			uiPrintf("Error: Unable to successfully send RX_DATA_START command to client!\n");
		}
	}
	checkLibError(devNum, 1);

	return;
}

void art_setSingleTransmitPower
(
 A_UINT32 devNum,
 A_UCHAR pcdac
)
{
	if	(!configSetup.remote || !configSetup.remote_exec)
	{
			setSingleTransmitPower(devNum, pcdac);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_SET_SINGLE_TRANSMIT_POWER_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = pcdac;

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send SET_SINGLE_TRANSMIT_POWER_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

void art_specifySubSystemID
(
 A_UINT32 devNum,
 A_INT16  subsystemID
)
{
	if	(!configSetup.remote || !configSetup.remote_exec)
	{
		specifySubSystemID(devNum, subsystemID);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_SPECIFY_SUBSYSTEM_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = subsystemID;

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send M_SPECIFY_SUBSYSTEM_CMD_ID command to client!\n");
		}
	}
	checkLibError(devNum, 1);
}

A_UINT16 art_getMaxPowerForRate
(
 A_UINT32 devNum,
 A_UINT16 freq,
 A_UINT16 rate
)
{
	A_UINT16 returnValue;
	A_UINT16 *pRegValue;

	if	(!configSetup.remote || !configSetup.remote_exec )
	{
		returnValue = getMaxPowerForRate(devNum, (A_UINT32)freq, (A_UINT32)rate);
	} 
	else {
		GlobalCmd.cmdID = M_GET_MAX_POWER_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_MAX_POWER_CMD.freq = freq;
		GlobalCmd.CMD_U.GET_MAX_POWER_CMD.rate = rate;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.GET_MAX_POWER_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_MAX_POWER_CMD command\n");
			return 0xdead;
		}
		returnValue = *pRegValue;

	}


	return(returnValue);
}

A_UINT16 art_getPcdacForPower
(
 A_UINT32 devNum,
 A_UINT16 freq,
 A_INT16 power
)
{
	A_UINT16 returnValue;
	A_UINT16 *pRegValue;

	if	(!configSetup.remote || !configSetup.remote_exec )
	{
		returnValue = getPcdacForPower(devNum, (A_UINT32)freq, power);
	} 
	else {
		GlobalCmd.cmdID = M_GET_MAX_POWER_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_PCDAC_FOR_POWER_CMD.freq = freq;
		GlobalCmd.CMD_U.GET_PCDAC_FOR_POWER_CMD.power = power;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.GET_PCDAC_FOR_POWER_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_PCDAC_FOR_POWER_CMD command\n");
			return 0xdead;
		}
		returnValue = *pRegValue;

	}


	return(returnValue);
}

A_INT32 art_getFieldForMode
(
 A_UINT32 devNum,
 A_CHAR   *fieldName,
 A_UINT32  mode,			//desired mode 
 A_UINT32  turbo		//Flag for base or turbo value
)
{

	A_INT32 returnValue; 
	A_INT32	     *pRegValue;

	if	(!configSetup.remote || !configSetup.remote_exec) 
	{
		returnValue = getFieldForMode(devNum, fieldName, mode, turbo);
	} 
	else 
	{
	    /* create cmd structure and send command */
		GlobalCmd.cmdID = M_GET_FIELD_FOR_MODE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD.mode = mode;
		GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD.turbo = turbo;
		strcpy(GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD.fieldName, fieldName);
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_FIELD_FOR_MODE command\n");
			return 0xdeadbeef;
		}
		returnValue = *pRegValue;
	}
	if (checkLibError(devNum, 1)) {
		return(0xdeadbeef);
	}
	return (returnValue);
}

// art_getFieldForMode returns 0xdeadbeef even if the field doesn't exist
// and it DOES NOT WARN/ERROR in this case. don't know what the legacy reason
// is for suppressing warning on something so gross. adding a checked routine to 
// visibly warn if returned value is 0xdeadbeef. Use this wherever you care about value 
// being valid.  -- P.D. 2/04
A_INT32 art_getFieldForModeChecked
(
 A_UINT32 devNum,
 A_CHAR   *fieldName,
 A_UINT32  mode,			//desired mode 
 A_UINT32  turbo		//Flag for base or turbo value
)
{

	A_INT32 returnValue; 
	A_INT32	     *pRegValue;

	if	(!configSetup.remote || !configSetup.remote_exec) 
	{
		returnValue = getFieldForMode(devNum, fieldName, mode, turbo);
	} 
	else 
	{
	    /* create cmd structure and send command */
		GlobalCmd.cmdID = M_GET_FIELD_FOR_MODE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD.mode = mode;
		GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD.turbo = turbo;
		strcpy(GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD.fieldName, fieldName);
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.GET_FIELD_FOR_MODE_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_FIELD_FOR_MODE command\n");
			return 0xdeadbeef;
		}
		returnValue = *pRegValue;
	}
	if ((checkLibError(devNum, 1)) || (returnValue == 0xdeadbeef)) {
		printf("***************************************************************\n");
		printf("ERROR : art_getFieldForModeChecked : fieldname %s does not exist or other major error happened\n", fieldName);
		printf("***************************************************************\n");
		return(0xdeadbeef);
	}
	return (returnValue);
}

void art_readField
(
    A_UINT32   devNum,
    A_CHAR     *fieldName,
    A_UINT32   *pUnsignedValue,                        //returns unsigned value
    A_INT32    *pSignedValue,                          //returns signed value
    A_BOOL     *pSignedFlag                            //return true if this is a signed value
)
{
    readField(devNum, fieldName, pUnsignedValue, pSignedValue, pSignedFlag);                           
}

void art_getField
(
	A_UINT32 devNum,
	A_CHAR   *fieldName,
	A_UINT32 *baseValue,
	A_UINT32 *turboValue
)
{
	A_UINT32 *pRegValue; 

	if	(!configSetup.remote || !configSetup.remote_exec) 
	{
		getField(devNum, fieldName, baseValue, turboValue);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_GET_FIELD_VALUE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		strcpy(GlobalCmd.CMD_U.GET_FIELD_VALUE_CMD.fieldName, fieldName);
		GlobalCmd.CMD_U.GET_FIELD_VALUE_CMD.turbo = 0;
			
		if (!artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.GET_FIELD_VALUE_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_FIELD_VALUE_CMD command to client!\n");
		}
		*baseValue = *pRegValue;
		*turboValue = *(pRegValue + 1);
	}
	checkLibError(devNum, 1);
}

void art_changeField
(
	A_UINT32 devNum,
	A_CHAR *fieldName, 
	A_UINT32 newValue
)
{
	
	if	(!configSetup.remote || !configSetup.remote_exec) 
	{
			changeField(devNum, fieldName, newValue);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_CHANGE_FIELD_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		strcpy(GlobalCmd.CMD_U.CHANGE_FIELD_CMD.fieldName, fieldName);
		GlobalCmd.CMD_U.CHANGE_FIELD_CMD.newValue = newValue;
	
		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.CHANGE_FIELD_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send CHANGE_FIELD_CMD command to client!\n");
		}
	}
    
	checkLibError(devNum, 1);
    return;
}

void art_writeField
(
	A_UINT32 devNum,
	A_CHAR *fieldName, 
	A_UINT32 newValue
)
{
	
	if	(!configSetup.remote || !configSetup.remote_exec) 
	{
		writeField(devNum, fieldName, newValue);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_WRITE_FIELD_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		strcpy(GlobalCmd.CMD_U.WRITE_FIELD_CMD.fieldName, fieldName);
		GlobalCmd.CMD_U.WRITE_FIELD_CMD.newValue = newValue;
	
		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.WRITE_FIELD_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send WRITE_FIELD_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
    return;
}


void art_changeMultipleFieldsAllModes
(
 A_UINT32		  devNum,
 PARSE_MODE_INFO *pFieldsToChange,
 A_UINT32		  numFields
)
{
	if	(!configSetup.remote || !configSetup.remote_exec) 
	{
		changeMultipleFieldsAllModes(devNum, pFieldsToChange, numFields);
	} 
	else 
	{
		GlobalCmd.cmdID = M_CHANGE_MULTIPLE_FIELDS_ALL_MODES_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.CHANGE_MULTI_FIELDS_ALL_MODES_CMD.numFields = numFields;
		memcpy(GlobalCmd.CMD_U.CHANGE_MULTI_FIELDS_ALL_MODES_CMD.FieldsToChange, pFieldsToChange, numFields * sizeof(PARSE_MODE_INFO));
	
		/* send the command.  Note that the size to send is only for num bytes want to write */
		if(!artSendCmd(&GlobalCmd,
					   (sizeof(GlobalCmd.CMD_U.CHANGE_MULTI_FIELDS_ALL_MODES_CMD) + sizeof(GlobalCmd.cmdID) - ((MAX_NUM_FIELDS - numFields)*sizeof(PARSE_MODE_INFO))),
					   NULL)) {
			uiPrintf("Error: Unable to successfully send CHANGE_MULTIPLE_FIELDS_ALL_MODES command\n");
			return;
		}
	}
	checkLibError(devNum, 1);
	return;
}

void art_changeMultipleFields
(
 A_UINT32		  devNum,
 PARSE_FIELD_INFO *pFieldsToChange,
 A_UINT32		  numFields
)
{
	if	(!configSetup.remote || !configSetup.remote_exec) 
	{
		changeMultipleFields(devNum, pFieldsToChange, numFields);
	} 
	else 
	{
		GlobalCmd.cmdID = M_CHANGE_MULTIPLE_FIELDS_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.CHANGE_MULTI_FIELDS_CMD.numFields = numFields;
		memcpy(GlobalCmd.CMD_U.CHANGE_MULTI_FIELDS_CMD.FieldsToChange, pFieldsToChange, numFields * sizeof(PARSE_FIELD_INFO));
		
		/* send the command.  Note that the size to send is only for num bytes want to write */
		if(!artSendCmd(&GlobalCmd,
					   (sizeof(GlobalCmd.CMD_U.CHANGE_MULTI_FIELDS_CMD) + sizeof(GlobalCmd.cmdID) - ((MAX_NUM_FIELDS - numFields)*sizeof(PARSE_FIELD_INFO))),
					   NULL)) {
			uiPrintf("Error: Unable to successfully send CHANGE_MULTIPLE_FIELDS command\n");
			return;
		}
	}
	checkLibError(devNum, 1);
	return;
}


void art_teardownDevice
(
	A_UINT32 devNum
)
{
	if(configSetup.remote)
	{
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.cmdID = M_CLOSE_DEVICE_CMD_ID;
		if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID), 
						NULL)) 
		{
			uiPrintf("Error: problem sending CLOSE_DEVICE cmd to client!\n");
		}
#ifndef __ATH_DJGPPDOS__
		globalNumInstances--;
		if (globalNumInstances == 0) {
            GlobalCmd.devNum = (A_INT8) devNum;
			GlobalCmd.cmdID = DISCONNECT_PIPE_CMD_ID;
			if (!artSendCmd(&GlobalCmd, 
							sizeof(GlobalCmd.cmdID), 
							NULL)) 
			{
				uiPrintf("Error: problem sending DISCONNECT cmd to client in teardownDevice()!\n");
			}
			osSockClose(artSockInfo);
			pArtPrimarySock = NULL;
		}
        if (!configSetup.remote_exec) {
		    teardownDevice(devNum);
        }
#endif //__ATH_DJGPPDOS__
	}
	if (!configSetup.remote)
	{
		teardownDevice(devNum);
	}
}

A_BOOL art_testLib
(
 A_UINT32 devNum,
 A_UINT32 timeout
)
{
	A_BOOL ret;
	A_UINT32 *pRegValue;

	ret = FALSE;

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		ret = testLib(devNum, timeout);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_TEST_LIB_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = timeout;

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID), 
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send TEST_LIB_CMD command to client!\n");
			return FALSE;

		}
		ret = (A_BOOL) *pRegValue;
	}

	if(checkLibError(devNum, 1)) {
		return FALSE;
	}
	return ret;
	
}

void art_ForceSinglePCDACTable
(
	A_UINT32 devNum, 
	A_UINT16 pcdac
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		ForceSinglePCDACTable(devNum,pcdac, CalSetup.pcdacOffset);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_FORCE_SINGLE_PCDAC_TABLE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.FORCE_SINGLE_PCDAC_TABLE_CMD.pcdac = pcdac;

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.FORCE_SINGLE_PCDAC_TABLE_CMD)+sizeof(GlobalCmd.cmdID), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send FORCE_SINGLE_PCDAC_TABLE_CMD command to client!\n");
			return;
		}
	}
	checkLibError(devNum, 1);
	return;
}

void art_ForceSinglePCDACTableGriffin
(
	A_UINT32 devNum, 
	A_UINT16 pcdac,
	A_UINT16 offset
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		ForceSinglePCDACTableGriffin(devNum,pcdac,offset);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_FORCE_SINGLE_PCDAC_TABLE_GRIFFIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.FORCE_SINGLE_PCDAC_TABLE_GRIFFIN_CMD.pcdac = pcdac;
        GlobalCmd.CMD_U.FORCE_SINGLE_PCDAC_TABLE_GRIFFIN_CMD.offset = offset;

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.FORCE_SINGLE_PCDAC_TABLE_GRIFFIN_CMD)+sizeof(GlobalCmd.cmdID), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send FORCE_SINGLE_PCDAC_TABLE_GRIFFIN_CMD command to client!\n");
			return;
		}
	}
	checkLibError(devNum, 1);
	return;
}
 
void art_ForcePCDACTable
(
	A_UINT32 devNum, 
	A_UINT16 *pcdac
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		forcePCDACTable(devNum, pcdac);
	} 
	else 
	{
		GlobalCmd.cmdID = M_FORCE_PCDAC_TABLE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		memcpy(GlobalCmd.CMD_U.FORCE_PCDAC_TABLE_CMD.pcdac, pcdac, MAX_PCDACS * 2);

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.FORCE_PCDAC_TABLE_CMD)+sizeof(GlobalCmd.cmdID), 
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send FORCE_PCDAC_TABLE_CMD command to client!\n");
			return;
		}
	}
	checkLibError(devNum, 1);
}

void art_forcePowerTxMax 
(
	A_UINT32		devNum,
	A_UINT16		*pRatesPower
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		forcePowerTxMax(devNum, (A_INT16 *)pRatesPower);
	} 
	else 
	{
		/* setup the command struct to send */
		GlobalCmd.cmdID = M_FORCE_POWER_TX_MAX_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.FORCE_POWER_TX_MAX_CMD.length = NUM_RATES;
		memcpy(GlobalCmd.CMD_U.FORCE_POWER_TX_MAX_CMD.ratesPower, pRatesPower, NUM_RATES * 2);
	
		/* send the command.  Note that the size to send is only for num bytes want to write */
		if(!artSendCmd(&GlobalCmd,
					   (sizeof(GlobalCmd.CMD_U.FORCE_POWER_TX_MAX_CMD) + sizeof(GlobalCmd.cmdID) - ((MAX_BLOCK_SWORDS - NUM_RATES)*2)),
					   NULL)) {
			uiPrintf("Error: Unable to successfully send FORCE_POWER_TX_MAX command\n");
			return;
		}
	}
	checkLibError(devNum, 1);
	return;
}

void art_forceSinglePowerTxMax 
(
	A_UINT32		devNum,
	A_UINT16		powerValue
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		forceSinglePowerTxMax(devNum, powerValue);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_FORCE_SINGLE_POWER_TX_MAX_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ONE_CMD.param = (A_UINT32)powerValue;

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.SET_ONE_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send FORCE_SINGLE_POWER_TX_MAX_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
	return;
}

A_INT16 art_GetMacAddr
(
	A_UINT32 devNum,
	A_UINT16 wmac,
	A_UINT16 instNo,
	A_UINT8	*macAddr
)
{
	A_UCHAR  *pReadValues;		  
	A_UCHAR	 buf[6];
	
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		getMacAddr(devNum, wmac, instNo, buf);
		pReadValues = buf;
	}
	else 
	{
		// setup the command struct to send 
		GlobalCmd.cmdID = M_GET_MAC_ADDR_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_MAC_ADDR_CMD.instNo = instNo;
		GlobalCmd.CMD_U.GET_MAC_ADDR_CMD.wmac = wmac;

		/* send the command.  Note that the size to send is only for num bytes want to write */

		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.GET_MAC_ADDR_CMD) + sizeof(GlobalCmd.cmdID)),
					(void **)(&pReadValues))) {
				uiPrintf("Error: Unable to successfully send GET_MAC_ADDR command\n");
				return -1;
		}
	}

	if(checkLibError(devNum, 1)) {
		return -1;
	}

	memcpy((void *)macAddr, (void *)pReadValues, 6); // copy the mac address
	return(0);
}

A_INT16 art_configureLibParams
(
	A_UINT32 devNum
)
{

	LIB_PARAMS  LibParams;

	LibParams.beanie2928Mode = configSetup.beanie2928Mode;
	LibParams.refClock  = configSetup.refClk;
	LibParams.enableXR  = configSetup.enableXR;
	LibParams.loadEar = configSetup.loadEar;
	LibParams.eepStartLocation = configSetup.eeprom2StartLocation;
	LibParams.applyCtlLimit = configSetup.applyCtlLimit;
	LibParams.ctlToApply = configSetup.ctlToApply;
	LibParams.applyPLLOverride = configSetup.applyPLLOverride;
	LibParams.pllValue = configSetup.pllValue;
	LibParams.noUnlockEeprom = configSetup.noEepromUnlock;
#ifndef __ATH_DJGPPDOS__
	LibParams.artAniEnable = configSetup.artAniEnable;
	LibParams.artAniReuse  = configSetup.artAniReuse;
	LibParams.artAniNILevel = configSetup.artAniLevel[ART_ANI_TYPE_NI];
	LibParams.artAniBILevel = configSetup.artAniLevel[ART_ANI_TYPE_BI];
	LibParams.artAniSILevel = configSetup.artAniLevel[ART_ANI_TYPE_SI];
	LibParams.printPciWrites = configSetup.printPciWrites;
#else
	LibParams.artAniEnable = ART_ANI_DISABLED;
#endif

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		configureLibParams(devNum, &LibParams);
#ifndef __ATH_DJGPPDOS__
		changePciWritesFlag(devNum, LibParams.printPciWrites);
#else
//		uiPrintf(" Not Implemented in DOS Version\n");
#endif
	}
	else 
	{
		// setup the command struct to send 
		GlobalCmd.cmdID = M_SET_LIB_CONFIG_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		memcpy(&(GlobalCmd.CMD_U.SET_LIB_CONFIG_CMD.libParams), &LibParams, sizeof(LIB_PARAMS));

		/* send the command.  Note that the size to send is only for num bytes want to write */
		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.SET_LIB_CONFIG_CMD) + sizeof(GlobalCmd.cmdID)),
					NULL)) {
				uiPrintf("Error: Unable to successfully send SET_LIB_CONFIG command\n");
				return -1;
		}
	}

	if(checkLibError(devNum, 1)) {
		return -1;
	}
	return(0);
}

A_BOOL	 art_getCtlPowerInfo
(
 A_UINT32 devNum,
 CTL_POWER_INFO *pReturnStruct //pointer to structure to fill
)
{
	A_UCHAR *pReadValues;

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		getCtlPowerInfo(devNum, pReturnStruct);
	}
	else {
		// create cmd to send to client
		GlobalCmd.cmdID = M_GET_CTL_INFO_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		if (!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID),
						(void **)&pReadValues)) 
		{
			uiPrintf("Error: Unable to successfully send M_GET_CTL_INFO_CMD_ID command to client!\n");
			return 0;
			
		}
		if(checkLibError(devNum, 1)) return 0;

		/* copy the data from command receive buffer into buffer */
		memcpy(pReturnStruct, pReadValues, sizeof(CTL_POWER_INFO));
	}
	if(pReturnStruct->structureFilled) {
		return(TRUE);
	}
	return (FALSE);
}

A_UINT16 art_GetEepromStruct
(
 A_UINT32 devNum,
 A_UINT16 eepStructFlag,	//which eeprom strcut
 void **ppReturnStruct		//return ptr to struct asked for
)
{
	A_UINT32	sizeStruct; //don't need to pass back the size of the struct,
							//this is needed for remove command on the client side.
	A_UCHAR  *pReadValues;		  
	
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		getEepromStruct(devNum, eepStructFlag, ppReturnStruct, &sizeStruct);
	}
	else 
	{
		// setup the command struct to send 
		GlobalCmd.cmdID = M_GET_EEPROM_STRUCT_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_EEPROM_STRUCT_CMD.eepStructFlag = eepStructFlag;

			    /* send the command.  Note that the size to send is only for num bytes want to write */

		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.GET_EEPROM_STRUCT_CMD) + sizeof(GlobalCmd.cmdID)),
					(void **)(&pReadValues))) {
				uiPrintf("Error: Unable to successfully send GET_EEPROM_STRUCT command\n");
				return 0xffff;
		}
		sizeStruct = *((A_UINT32 *)pReadValues);
		*ppReturnStruct = (void *)(pReadValues+4);
	}
	if(checkLibError(devNum, 1)) {
		return 0xffff;
	}
	return(0);
}

A_UINT16 art_getXpdgainForPower
(
	A_UINT32 devNum,
	A_INT16  power
)
{
	A_UINT16 returnValue;
	A_UINT16 *pRegValue;

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		returnValue = getXpdgainForPower(devNum, power);
	}
	else {
		GlobalCmd.cmdID = M_GET_XPDGAIN_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_XPDGAIN_CMD.power = power;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.GET_XPDGAIN_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_XPDGAIN_CMD command\n");
			return 0xdead;
		}
		if (checkLibError(devNum, 1)) {
			return(0xdead);
		} 
		returnValue = *pRegValue;

	}
	return returnValue;
}

void art_writeNewProdData
(
 A_UINT32 devNum,
 A_INT32  *argList,
 A_UINT32 numArgs
)
{
	if (numArgs > 16) {
		uiPrintf("Error: A maximum of 16 arguments supported in art_writeNewProdData\n");
		return;
	}

	if (configSetup.remote)
	{
		// setup the command struct to send 
		GlobalCmd.cmdID = M_WRITE_NEW_PROD_DATA_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;

		GlobalCmd.CMD_U.WRITE_NEW_PROD_DATA_CMD.numArgs = numArgs;
		memcpy((void *)GlobalCmd.CMD_U.WRITE_NEW_PROD_DATA_CMD.argList,(void *)argList,4*numArgs);				
		
		 /* send the command.  Note that the size to send is only for num bytes want to write */

		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.WRITE_NEW_PROD_DATA_CMD) + sizeof(GlobalCmd.cmdID)),
					NULL)) {
				uiPrintf("Error: Unable to successfully send WRITE_NEW_PROD_DATA_CMD command\n");
				return;
		}
	}

	if(checkLibError(devNum, 1)) {
		return;
	}

	return;
}

void art_writeProdData
(
 A_UINT32 devNum,
 A_UCHAR wlan0Mac[6],
 A_UCHAR wlan1Mac[6],
 A_UCHAR enet0Mac[6],
 A_UCHAR enet1Mac[6]
)
{
	if (configSetup.remote)
	{
		// setup the command struct to send 
		GlobalCmd.cmdID = M_WRITE_PROD_DATA_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;

		memcpy((void *)GlobalCmd.CMD_U.WRITE_PROD_DATA_CMD.wlan0Mac,(void *)wlan0Mac,6);
		memcpy((void *)GlobalCmd.CMD_U.WRITE_PROD_DATA_CMD.wlan1Mac,(void *)wlan1Mac,6);
		memcpy((void *)GlobalCmd.CMD_U.WRITE_PROD_DATA_CMD.enet0Mac,(void *)enet0Mac,6);
		memcpy((void *)GlobalCmd.CMD_U.WRITE_PROD_DATA_CMD.enet1Mac,(void *)enet1Mac,6);
		
		 /* send the command.  Note that the size to send is only for num bytes want to write */

		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.WRITE_PROD_DATA_CMD) + sizeof(GlobalCmd.cmdID)),
					NULL)) {
				uiPrintf("Error: Unable to successfully send WRITE_PROD_DATA_CMD command\n");
				return;
		}
	}
	if(checkLibError(devNum, 1)) {
		return;
	}
	return;
}

A_BOOL art_ftpDownloadFile
(
 A_UINT32 devNum,
 A_CHAR *hostname,
 A_CHAR *user,
 A_CHAR *password,
 A_CHAR *remoteFile,
 A_CHAR *localFile
)
{
	if (configSetup.remote || !configSetup.remote_exec)
	{
		// setup the command struct to send 
		GlobalCmd.cmdID = M_FTP_DOWNLOAD_FILE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;

		//add one to the length of string so we copy the null terminator
		memcpy((void *)GlobalCmd.CMD_U.FTP_DOWNLOAD_FILE_CMD.hostname,(void *)hostname,strlen(hostname)+1);
		memcpy((void *)GlobalCmd.CMD_U.FTP_DOWNLOAD_FILE_CMD.user,(void *)user,strlen(user)+1);
		memcpy((void *)GlobalCmd.CMD_U.FTP_DOWNLOAD_FILE_CMD.passwd,(void *)password,strlen(password)+1);
		memcpy((void *)GlobalCmd.CMD_U.FTP_DOWNLOAD_FILE_CMD.remotefile,(void *)remoteFile,strlen(remoteFile)+1);
		memcpy((void *)GlobalCmd.CMD_U.FTP_DOWNLOAD_FILE_CMD.localfile,(void *)localFile,strlen(localFile)+1);
		
		 /* send the command.*/

		if(!artSendCmd(&GlobalCmd,
					(sizeof(GlobalCmd.CMD_U.FTP_DOWNLOAD_FILE_CMD) + sizeof(GlobalCmd.cmdID)),
					NULL)) {
				uiPrintf("Error: Unable to successfully send FTP_DOWNLOAD_FILE_CMD command\n");
				return FALSE;
		}
	}
	if(checkLibError(devNum, 1)) {
		return FALSE;
	}
	return TRUE;
}

A_BOOL art_waitForGenericCmd
(
 void *pSock,
 A_UCHAR   *pStringVar,
 A_UINT32  *pIntVar1,
 A_UINT32  *pIntVar2,
 A_UINT32  *pIntVar3
)
{

#ifndef __ATH_DJGPPDOS__
	if(waitForGenericCmd(pSock, pStringVar, pIntVar1, pIntVar2, pIntVar3) != A_OK) {
		return 0;
	}
#else
	return 0;
#endif
	return 1;
}

A_BOOL art_sendGenericCmd
(
 A_UINT32 devNum,
 A_CHAR *stringVar,
 A_INT32 intVar1,
 A_INT32 intVar2,
 A_INT32 intVar3
)
{
	A_UINT16 stringVarLength;

	if (!configSetup.remote)
	{
		uiPrintf("Error: art_sendGenericCmd() remote flag must be set\n");  
		return 0;
	}

	// setup the command struct to send 
	GlobalCmd.cmdID = M_GENERIC_CMD_ID;
    GlobalCmd.devNum = (A_INT8) devNum;

	stringVarLength = strlen(stringVar);
	if(stringVarLength >= MAX_GENERIC_CMD_LEN) {
		uiPrintf("Error: art_sendGenericCmd() stringVar length must be less than %d\n", MAX_GENERIC_CMD_LEN);
		return 0;
	}
	memcpy((void *)GlobalCmd.CMD_U.GENERIC_CMD.stringVar, (void *)stringVar, stringVarLength);
	GlobalCmd.CMD_U.GENERIC_CMD.stringVar[stringVarLength] = '\0';
	stringVarLength ++;
	GlobalCmd.CMD_U.GENERIC_CMD.intVar1 = intVar1;
	GlobalCmd.CMD_U.GENERIC_CMD.intVar2 = intVar2;
	GlobalCmd.CMD_U.GENERIC_CMD.intVar3 = intVar3;


	if(!artSendCmd(&GlobalCmd,
			(sizeof(GlobalCmd.CMD_U.GENERIC_CMD) + sizeof(GlobalCmd.cmdID) - (MAX_GENERIC_CMD_LEN - stringVarLength)),
			NULL)) {
		uiPrintf("Error: Unable to successfully send GENERIC_CMD command\n");
		return 0;
	}

	if(checkLibError(devNum, 1)) {
		return 0;
	}
	return 1;
}

void art_sleepCmd
(
 A_UINT32 devNum,
 A_UINT32 sleep_enable
)
{
	// setup the command struct to send 
	GlobalCmd.cmdID = SLEEP_CMD_ID;
    GlobalCmd.devNum = (A_INT8) devNum;
    GlobalCmd.CMD_U.SLEEP_CMD.id =  sleep_enable;

	if(!artSendCmd(&GlobalCmd,
			(sizeof(GlobalCmd.CMD_U.SLEEP_CMD) + sizeof(GlobalCmd.cmdID) ),
			NULL)) {
		uiPrintf("Error: Unable to successfully send SLEEP_CMD command\n");
		return ;
	}
}

void art_supplyFalseDetectbackoff
(
	A_UINT32 devNum,
	A_UINT32 *pBackoffValues
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		supplyFalseDetectbackoff(devNum, pBackoffValues);
	} 
	else 
	{
		// create cmd to send to client
		GlobalCmd.cmdID = M_FALSE_DETECT_BACKOFF_VALS_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		memcpy((void *)GlobalCmd.CMD_U.FALSE_DETECT_BACKOFF_VALS_CMD.backoffValues,(void *)pBackoffValues,sizeof(A_UINT32)*3);

		if ( !artSendCmd(&GlobalCmd,
						sizeof(GlobalCmd.CMD_U.FALSE_DETECT_BACKOFF_VALS_CMD)+sizeof(GlobalCmd.cmdID),
						NULL)) 
		{
			uiPrintf("Error: Unable to successfully send FALSE_DETECT_BACKOFF_VALS_CMD command to client!\n");
		}
	}
	checkLibError(devNum, 1);
	return;
}

#ifndef __ATH_DJGPPDOS__
/**************************************************************************
* openCommsChannel - Open a comms channel to the specified system
*
* Look to see if a pipe is already open to that F2, if so return the os
* structure for that existing pipe.  If pipe is not already open then try to
* open one and create an os structure for that pipe
*
* RETURNS: NULL on ERROR
*/
ART_SOCK_INFO *openCommsChannel
    (
    A_CHAR	    *machineName
    )
{
    ART_SOCK_INFO   *sockInfo;

    if (machineName == NULL) {
		uiPrintf("Error: invalid machine name (NULL) in openCommsChannel\n");
		return NULL;
    }
   
    sockInfo = osSockConnect(machineName);
    if ( !sockInfo ) {
		uiPrintf("Error:  Unable to connect to %s in openCommsChannel()\n",machineName);
		A_FREE(artSockInfo);
		return NULL;
    }

    return sockInfo;
}


/**************************************************************************
* artSendCmd - Send a command to a dk client
*
* Construct a command to send to the mdk client.  
* Wait for a return struct to verify that the command was successfully 
* excuted.
*
* RETURNS: TRUE if command was successful, FALSE if it was not
*/
A_BOOL artSendCmd
    (
    PIPE_CMD	    *pCmdStruct,	// In:	pointer to structure containing cmd info
    A_UINT32	    cmdSize,		// In:	size of the command structure
    void			**returnCmdStruct	// Out: pointer to structure returned by cmd
    )					// NOTE: if command does not expect a return struct
{
 	CMD_REPLY		*pCmdReply;
	A_BOOL			bGoodWrite = FALSE;
	A_UINT32		retLen;
	A_UINT16		errorNo;

#ifdef _IQV
	char	msg[30];
	sprintf(msg, "artSendCmd:%d", pCmdStruct->cmdID);
//	uiWriteToLog(msg);
//	ex_timeStart();	
// Too many cmd to print out for debug, hard to read, so only print out those cmdID > 11 (not those reg access)
	if (pCmdStruct->cmdID > 11)
		ex_timeDiff(msg);

	if(remoteMdkErrNo == 99){
		uiPrintf("devlib failed, skip osSockWrite\n");
		return FALSE;
	}
	if (art_mdkErrNo == 0xdeadbeef)
	{
//		uiPrintf("devlib failed, skip osSockWrite (2)\n");
		return FALSE;
	}
#endif

	// send	the	command
#if	defined(_DEBUG_XXX)
	q_uiPrintf("artSendCmd %d:cmdSize %d !!	  ", pCmdStruct->cmdID,	cmdSize);
	//uiPrintf("Sending	Pipe command %d:with cmdSize=%d:for	devNum=%d\n", pCmdStruct->cmdID, cmdSize, pCmdStruct->devNum);
#endif

//	  if ( osSockWrite(artSockInfo,	(A_UINT8*)&cmdSize,	sizeof(A_UINT32)) )	{
	pCmdStruct->cmdLen = (A_UINT16)	(cmdSize + sizeof(pCmdStruct->devNum));

	if ( osSockWrite(artSockInfo, (A_UINT8*)pCmdStruct,	cmdSize+sizeof(pCmdStruct->devNum)+sizeof(pCmdStruct->cmdLen)) ) {
		bGoodWrite = TRUE;
	}
	if ( !bGoodWrite ) {
	uiPrintf("Error: Unable	to write command to	pipe in	artSendCmd()!\n");
#ifdef _IQV
	  remoteMdkErrNo = 99;
	  art_mdkErrNo == 0xdeadbeef;
      os_com_close(artSockInfo);
	  closeEnvironment();
		return 0xdeadbeef;
#endif
	}

	// disconnect and close	don't expect ANY return	(not even the ID ack)
	// so we check for that	here
#if defined(AR6001)
    if ( (DISCONNECT_PIPE_CMD_ID == pCmdStruct->cmdID) || (CLOSE_PIPE_CMD_ID == pCmdStruct->cmdID) || (SLEEP_CMD_ID == pCmdStruct->cmdID))
#else
    if ( (DISCONNECT_PIPE_CMD_ID == pCmdStruct->cmdID) || (CLOSE_PIPE_CMD_ID == pCmdStruct->cmdID) )
#endif
	//milliSleep(1000);
	return TRUE;

//#ifdef _IQV
//	if (pCmdStruct->cmdID == M_EEPROM_WRITE_BLOCK_CMD_ID)
//		return TRUE;
//#endif

	// wait	on reply to	command
	if ( !receiveCmdReturn(&retLen)	) {
		uiPrintf("Error: Bad or	missing	reply from client in response to a command!\n");
		return FALSE;
	}

	pCmdReply =	&cmdReply;

	// check to	see	if the command ID's	match.	if they	don't, error
	if ( pCmdStruct->cmdID != pCmdReply->replyCmdId	) {
			uiPrintf("Error: client	reply to command has mismatched	ID!\n");
			uiPrintf("	   : sent cmdID: %d, returned: %d, size	%d\n",
			pCmdStruct->cmdID, pCmdReply->replyCmdId, retLen);
#ifdef _IQV
		art_mdkErrNo = 0xdeadbeef;
		return 0xdeadbeef;
#endif 
	}

	remoteMdkErrNo = 0;
#ifdef _IQV
	art_mdkErrNo = 0;
	if (pCmdStruct->cmdID != M_RX_DATA_COMPLETE_CMD_ID)	{
	// Don't wait on reply for all 
#endif 
	errorNo	= (A_UINT16) (pCmdReply->status	& COMMS_ERR_MASK) >> COMMS_ERR_SHIFT;
	if (errorNo	== COMMS_ERR_MDK_ERROR)
	{
		remoteMdkErrNo = (pCmdReply->status	& COMMS_ERR_INFO_MASK) >> COMMS_ERR_INFO_SHIFT;
		strncpy(remoteMdkErrStr,(const char	*)pCmdReply->cmdBytes,SIZE_ERROR_BUFFER);
		uiPrintf("Error: COMMS error MDK error for command %d\n", pCmdStruct->cmdID);
		return TRUE;
	}
	
	// check for a bad status in the command reply
	if (errorNo	!= CMD_OK) {
		uiPrintf("Error: Bad return	status in client command %d	response!\n", pCmdStruct->cmdID);
		return FALSE;
	}

	if ( !returnCmdStruct )	{
		// see if the length of	the	reply makes	sense
		if ( retLen	!= (sizeof(pCmdReply->replyCmdId) +	sizeof(pCmdReply->status)) ) {
			uiPrintf("Error: Invalid # of bytes	in client command %d response!\n", pCmdStruct->cmdID);
			return FALSE;
		}
		return TRUE;
	}

	// reply must be OK, return	pointer	to additional reply	info
	*returnCmdStruct = pCmdReply->cmdBytes;
 #ifdef	_IQV
	} else
		errorNo	= 0;
#endif 
	return TRUE;
}

/**************************************************************************
* receiveCmdReturn - Get the return info from a command sent
*
* Read from the pipe and get the info returned from a command.	######Need to
* make this timeout if don't receive reply, but don't know how to do that
* yet
*
*
* RETURNS: TRUE if reply was received, FALSE if it was not
*/
A_BOOL receiveCmdReturn
    (
    A_UINT32 *pReturnLength
    )
{
    A_BOOL	    bGoodRead = FALSE;
    if ( osSockRead(artSockInfo, (A_UINT8*)pReturnLength, sizeof(A_UINT32)) ) {
	if ( osSockRead(artSockInfo, (((A_UINT8 *)(&cmdReply))+sizeof(cmdReply.replyCmdLen)), *pReturnLength) )
	    bGoodRead= TRUE;
    }
    if ( !bGoodRead )
	return FALSE;

    cmdReply.replyCmdLen = *pReturnLength;
#if defined(_DEBUG_XXX)
    uiPrintf("recvCmdRet %d, len %d\n", cmdReply.replyCmdId, cmdReply.replyCmdLen);  
#endif

    return TRUE;
}

/**************************************************************************
* activateCommsInitHandshake - Activate comms pipe with secondary ART session
*
*
* RETURNS: TRUE if activated, FALSE if it was not
*/
A_BOOL activateCommsInitHandshake
(
 A_CHAR *machName
)
{
	pArtSecondarySock = openCommsChannel(machName);

	if (pArtSecondarySock == NULL) {
		uiPrintf("Error: Unable to open communications channel to secondary ART system!\n");
		return FALSE;
	}
	//###########TODO - need to close socket and cleanup mallocs 
	prepare2WayComms();
	return(TRUE);
}

A_BOOL waitCommsInitHandshake
(
 void
)
{
	pArtSecondarySock = (ART_SOCK_INFO *)artConnect();

	if (pArtSecondarySock == NULL) {
		uiPrintf("Error: Unable to open communications channel to secondary ART system!\n");
		return FALSE;
	}
	prepare2WayComms();
	return(TRUE);
	
}

void closeComms
(
 void
)
{
	//do this just in case 
	selectPrimary();
	cleanupSockMem(pArtSecondarySock, 1);
	
}

void activateArtSlave(void)
{
	//Change this to be a bit more sophisticated - more like MDK_Main
//	connectThread(pArtSecondarySock);

}

/**************************************************************************
* selectPrimary - Set flags and pointers back to primary ART session
*
*
* RETURNS: TRUE if selected, FALSE if it was not
*/
A_BOOL selectPrimary
(
 void
)
{
	if(configSetup.primaryAP) {
		if(!pArtPrimarySock) {
			uiPrintf("Fatal Error: Primary ART is AP and no pointer to socket initialized\n");
			return FALSE;
		}
		artSockInfo = pArtPrimarySock;
	}
	else {
		configSetup.remote = 0;
		artSockInfo = NULL;
	}
	return TRUE;
}

/**************************************************************************
* selectSecondary - Set flags and pointers for secondary ART session
*
*
* RETURNS: TRUE if selected, FALSE if it was not
*/
A_BOOL selectSecondary
(
 void
)
{
	if(!pArtSecondarySock) {
		uiPrintf("Fatal Error: Secondary socket is not initialized\n");
		return FALSE;
	}
	artSockInfo = pArtSecondarySock;
	configSetup.remote = 1;
	return TRUE;
}

#else
ART_SOCK_INFO *openCommsChannel
    (
    A_CHAR	    *machineName
    )
{
	//dummy function
	uiPrintf("Error: openCommsChannel() not supported in DOS version\n");
	return NULL;
}

A_BOOL artSendCmd
    (
    PIPE_CMD	    *pCmdStruct,	// In:	pointer to structure containing cmd info
    A_UINT32	    cmdSize,		// In:	size of the command structure
    void			**returnCmdStruct	// Out: pointer to structure returned by cmd
    )					// NOTE: if command does not expect a return struct
{
	//dummy function
	uiPrintf("Error: artSendCmd() not supported in DOS version\n");
	return(FALSE);

}

A_BOOL activateCommsInitHandshake
(
 A_CHAR *machName
)
{
	uiPrintf("Error: activateCommsInitHandshake() not supported in DOS version\n");
	return(FALSE);
}

A_BOOL selectPrimary
(
 void
)
{
	uiPrintf("Error: selectPrimary() not supported in DOS version\n");
	return(FALSE);
}

A_BOOL selectSecondary
(
 void
)
{
	uiPrintf("Error: selectSecondary() not supported in DOS version\n");
	return(FALSE);
}

#endif


/**************************************************************************
* art_getMdkErrNo - Get the last error number from the library
*
*/
A_INT32 art_getMdkErrNo
(
 A_UINT32 devNum
)
{
	if (!configSetup.remote || !configSetup.remote_exec) {
		return getMdkErrNo(devNum);
	}
	else {
		return remoteMdkErrNo;
	}
	return 0;
}


/**************************************************************************
* art_getMdkErrStr - Get the last error string from the library
*
*/
void art_getMdkErrStr
(
 A_UINT32 devNum,
 A_CHAR *pErrStr
)
{
	if (!configSetup.remote || !configSetup.remote_exec)
	{
		getMdkErrStr(devNum, pErrStr);
	}
	else {
		strncpy(pErrStr,remoteMdkErrStr,SIZE_ERROR_BUFFER);
	}
	return;
}

/**************************************************************************
* checkLibError - Check for errors in the last library call
*
* Check mdkErrno for errors.  If error, get and print error message 
* if requested
*
* RETURNS: TRUE if there was an error, FALSE if not
*/
A_BOOL checkLibError
(
 A_UINT32 devNum,	
 A_BOOL	printError 
)
{
	art_mdkErrNo = art_getMdkErrNo(devNum);
	
	if (art_mdkErrNo) {
		if (printError && printLocalInfo) {
			art_getMdkErrStr(devNum, art_mdkErrStr);
			uiPrintf("\nError: %s\n", art_mdkErrStr);
		}
		return TRUE;
	}
	return FALSE;
}


/**************************************************************************
* art_getPowerIndex - Get the power index pointing to the desired power
* level. In gen3, the pcdac table is made maxPower referenced - hence a 
* need for this for dynamicOptimization.
*
* RETURNS: index pointing to the desired power
*/
A_UINT16 art_getPowerIndex
(
	A_UINT32 devNum,
	A_INT32  power   // 2 x power in dB
)
{
	A_UINT16 returnValue;
	A_UINT16 *pRegValue;

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		returnValue = getPowerIndex(devNum, power);
	}
	else {
		GlobalCmd.cmdID = M_GET_POWER_INDEX_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_POWER_INDEX_CMD.power = power;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.GET_POWER_INDEX_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_POWER_INDEX_CMD command\n");
			return 0xdead;
		}
		if (checkLibError(devNum, 1)) {
			return(0xdead);
		} 
		returnValue = *pRegValue;

	}
	return returnValue;
}

A_UINT32 art_getISREvent(A_UINT32 devNum, EVENT_STRUCT *ppEvent) {
    A_UINT16 devIndex;
	EVENT_STRUCT pLocEventSpace;
	EVENT_STRUCT *ppEvent_tmp;
    MDK_WLAN_DEV_INFO    *pdevInfo;

    if (!configSetup.remote) {
	    devIndex = (A_UINT16)dev2drv(devNum);
	    pdevInfo = globDrvInfo.pDevInfoArray[devIndex];
#if defined(ART_BUILD) || (!defined(__ATH_DJGPPDOS__))
	    if (getNextEvent(devIndex, &pLocEventSpace)) {
             memcpy(ppEvent, &pLocEventSpace, sizeof(EVENT_STRUCT));
	    }
#endif
    }
    else {
        GlobalCmd.cmdID = GET_EVENT_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
        if ( !artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID), (void **)&ppEvent_tmp) ) {
              uiPrintf("Error: Problem sending GET_EVENT command to client!\n");
              return 0xdead;
        }
        memcpy(ppEvent, ppEvent_tmp, sizeof(EVENT_STRUCT));
    }
  return 1;
}


//#ifndef __ATH_DJGPPDOS__
/**************************************************************************
* art_getArtAniLevel - Get current level of noise immunity (NI/BI/SI)
*
* RETURNS: level of NI/BI/SI (0..max)
*
*/
A_UINT32 art_getArtAniLevel
(
	A_UINT32 devNum,
	A_UINT32 artAniType   // NI/BI/SI
)
{
	A_UINT32 returnValue = 0;
	A_UINT32 *pRegValue;

	if (!configSetup.artAniEnable) {
		return returnValue;
	}


	if (!configSetup.remote || !configSetup.remote_exec)
	{
		returnValue = getArtAniLevel(devNum, artAniType);
	}
	else {
		GlobalCmd.cmdID = M_GET_ART_ANI_LEVEL_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.GET_ART_ANI_LEVEL_CMD.artAniType = artAniType;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.GET_ART_ANI_LEVEL_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_ART_ANI_LEVEL_CMD command\n");
			return 0xdead;
		}
		if (checkLibError(devNum, 1)) {
			return(0xdead);
		} 
		returnValue = *pRegValue;

	}
	return returnValue;
}


/**************************************************************************
* art_setArtAniLevel - Set current level of noise immunity (NI/BI/SI)
*
* RETURNS: void
*
*/
void art_setArtAniLevel
(
	A_UINT32 devNum,
	A_UINT32 artAniType,   // NI/BI/SI
	A_UINT32 artAniLevel
)
{
	A_UINT32 *pRegValue;

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		setArtAniLevel(devNum, artAniType, artAniLevel);
	}
	else {
		GlobalCmd.cmdID = M_SET_ART_ANI_LEVEL_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SET_ART_ANI_LEVEL_CMD.artAniType = artAniType;
		GlobalCmd.CMD_U.SET_ART_ANI_LEVEL_CMD.artAniLevel = artAniLevel;
		if(!artSendCmd(&GlobalCmd,
			sizeof(GlobalCmd.CMD_U.SET_ART_ANI_LEVEL_CMD)+sizeof(GlobalCmd.cmdID),
			(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send SET_ART_ANI_LEVEL_CMD command\n");	
		}
		checkLibError(devNum, 1);
	}
}

/**************************************************************************
* art_getMaxLinPower - get maximum Linear power from eep_map = 1 format cal
*
* RETURNS: max linear power (in dBm)
*
*/
double art_getMaxLinPower
(
	A_UINT32 devNum
)
{
	double retVal = 0.0;
	A_UINT32 *pRegValue;

	if (!configSetup.remote || !configSetup.remote_exec)
	{
		retVal = ((double)getMaxLinPowerx4(devNum)/4.0);
	}
	else {
		GlobalCmd.cmdID = M_GET_MAX_LIN_PWR_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.cmdID),
			(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_MAX_LIN_PWR_CMD command\n");	
		}
		checkLibError(devNum, 1);
		retVal = ((double) *pRegValue / 4.0);
	}

	return(retVal);
}

/**************************************************************************
* art_getEARCalAtChannel - get the list of entries to program in EAR for 
*                          cal data at "channel" for eep_map = 2 format.
*
* RETURNS: number of EAR entries in the array returned in *word
*
* NOTE : the array *word is also used as input when atCal = 1
*
*/

A_UINT16 art_getEARCalAtChannel
(
 A_UINT32 devNum, 
 A_BOOL   atCal, 
 A_UINT16 channel, 
 A_UINT32 *word, 
 A_UINT16 xpd_mask,
 A_UINT32 version_mask   
 )
{
	A_UINT16 retVal;

	if(!configSetup.remote || !configSetup.remote_exec)
	{
			retVal = getEARCalAtChannel(devNum, atCal, channel, word, xpd_mask, version_mask);
	} 
	else 
	{
		// this routine only needed for griffin, thus local.
		return(0);
	}

	if(checkLibError(devNum, 1)) {
		return 0;
	}

	return retVal;
}
A_UINT32 art_ap_reg_read
(
    A_UINT32 devNum,
    A_UINT32 regAddr
)
{
    A_UINT32	     *pRegValue;

		GlobalCmd.cmdID = AP_REG_READ_CMD_ID;
		GlobalCmd.CMD_U.AP_REG_READ_CMD.readAddr = regAddr;
		GlobalCmd.CMD_U.AP_REG_READ_CMD.readSize = 32;
        GlobalCmd.devNum = (A_INT8) devNum;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.AP_REG_READ_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send AP_REG_READ command\n");
			return 0xdeadbeef;
		}

	return(*pRegValue);
}

A_UINT32 art_ap_reg_write ( A_UINT32 devNum, A_UINT32 regAddr, A_UINT32 regValue) {

		GlobalCmd.cmdID = AP_REG_WRITE_CMD_ID;
		GlobalCmd.CMD_U.AP_REG_WRITE_CMD.writeAddr = regAddr;
		GlobalCmd.CMD_U.AP_REG_WRITE_CMD.regValue = regValue;
		GlobalCmd.CMD_U.AP_REG_WRITE_CMD.writeSize = 32;
        GlobalCmd.devNum = (A_INT8) devNum;
		if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.AP_REG_WRITE_CMD)+sizeof(GlobalCmd.cmdID), NULL)) 
		{
			uiPrintf("Error: Unable to successfully send AP_REG_WRITE command\n");
			return 0xdeadbeef;
		}

  return 1;
}

A_UINT32 load_and_run_code( A_UINT32 devNum, A_UINT32 loadAddress, A_UINT32 totalBytes, A_UCHAR *loadBytes)
{

   A_UINT32 byteIndex = 0;
   A_UINT32 codeAddress = loadAddress;
   A_UINT32 nBytes;
   A_UINT32 *pRetValue, bytesRemaining;

   while(byteIndex < totalBytes) {

		nBytes = (totalBytes - byteIndex);
		if (nBytes > 256) 
			nBytes = 256;
		bytesRemaining = totalBytes - byteIndex - nBytes;
		GlobalCmd.cmdID = LOAD_AND_RUN_CODE_CMD_ID;
		if ( bytesRemaining ) {
		  GlobalCmd.CMD_U.LOAD_AND_RUN_CODE_CMD.loadFlag = 1; // load the code 
		}
		else {
		  GlobalCmd.CMD_U.LOAD_AND_RUN_CODE_CMD.loadFlag = 0; // Execute the code
		}
		GlobalCmd.CMD_U.LOAD_AND_RUN_CODE_CMD.pPhyAddr = codeAddress;
		GlobalCmd.CMD_U.LOAD_AND_RUN_CODE_CMD.length = nBytes;
		memcpy((void *)GlobalCmd.CMD_U.LOAD_AND_RUN_CODE_CMD.pBuffer, (void *)(loadBytes+byteIndex), nBytes);
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.LOAD_AND_RUN_CODE_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRetValue)) 
		{
			uiPrintf("Error: Unable to successfully send LOAD_AND_RUN_CODE_CMD command, with loadFlag=%d\n", GlobalCmd.CMD_U.LOAD_AND_RUN_CODE_CMD.loadFlag);
			return 0xdeadbeef;
		}
		byteIndex += 256;
		codeAddress += 256;
  }
	return(*pRetValue);

}

void art_load_and_program_code( A_UINT32 devNum, A_UINT32 loadAddress, A_UINT32 totalBytes, A_UCHAR *loadBytes, A_BOOL calData)
{

   A_UINT32 byteIndex = 0;
   A_UINT32 codeAddress = loadAddress;
   A_UINT32 nBytes;
   A_UINT32 *pRetValue, bytesRemaining;

   while(byteIndex < totalBytes) {

		nBytes = (totalBytes - byteIndex);
		if (nBytes > MAX_BUFFER_SIZE) 
			nBytes = MAX_BUFFER_SIZE;
		bytesRemaining = totalBytes - byteIndex - nBytes;
		GlobalCmd.cmdID = LOAD_AND_PROGRAM_CODE_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		if ( bytesRemaining ) {
		  GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD.loadFlag = 1; // load the code 
		}
		else {
		  GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD.loadFlag =  ( (calData << 16) | 0 ); // Execute the code
		}
		GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD.csAddr = loadAddress;
		GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD.pPhyAddr = codeAddress;
		GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD.length = nBytes;
		memcpy((void *)GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD.pBuffer, (void *)(loadBytes+byteIndex), nBytes);
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD)+sizeof(GlobalCmd.cmdID),
						(void **)&pRetValue)) 
		{
			uiPrintf("Error: Unable to successfully send LOAD_AND_PROGRAM_CODE_CMD command, with loadFlag=%d\n", GlobalCmd.CMD_U.LOAD_AND_PROGRAM_CODE_CMD.loadFlag);
			return;
		}
		byteIndex += MAX_BUFFER_SIZE;
		codeAddress += MAX_BUFFER_SIZE;
		if ((*pRetValue) != A_OK) {
			printf("Unable to send LOAD_AND_PROGRAM_CODE command successfully, check addresses\n");
			break;
		}

  	}
	if ((*pRetValue) != A_OK) {
	   printf("Programming failure:Code %d:", (*pRetValue));
	   switch (*pRetValue) {
		 case A_MEMORY_NOT_AVAIL:
	   			printf("Memory Not available");
		        break;
		 case A_BAD_ADDRESS:
	   			printf("Invalid Physcal Address");
		        break;
		 case A_EPROTO:
	   			printf("Verification");
		        break;
	   }
	}
	else {
		printf("Programming SUCCESS\n");
	}
   return;

}

void art_fillTxStats ( A_UINT32 devNum, A_UINT32 descAddress, A_UINT32 numDesc, A_UINT32 dataBodyLen, A_UINT32 txTime, TX_STATS_STRUCT *txStats) {
	    A_UINT32 *pRet;

		GlobalCmd.cmdID = M_FILL_TX_STATS_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.FILL_TX_STATS_CMD.descAddress = descAddress;
		GlobalCmd.CMD_U.FILL_TX_STATS_CMD.numDesc = numDesc;
		GlobalCmd.CMD_U.FILL_TX_STATS_CMD.dataBodyLen = dataBodyLen;
		GlobalCmd.CMD_U.FILL_TX_STATS_CMD.txTime = txTime;
		//memcpy(GlobalCmd.CMD_U.FILL_TX_STATS_CMD.txStats, txStats, sizeof(GlobalCmd.CMD_U.FILL_TX_STATS_CMD.txStats));
		if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.FILL_TX_STATS_CMD)+sizeof(GlobalCmd.cmdID), (void **)&pRet)) 
		{
			uiPrintf("Error: Unable to successfully send FILL_TX_STATS_CMD command\n");
			return ;
		}
		memcpy(txStats, pRet, sizeof(TX_STATS_STRUCT)*STATS_BINS);

}

void art_createDescriptors(A_UINT32 devNumIndex, A_UINT32 descBaseAddress,  A_UINT32 descInfo, A_UINT32 bufAddrIncrement, A_UINT32 descOp, A_UINT32 descWords[MAX_DESC_WORDS]) {
		A_UINT32 numDescWords;

		GlobalCmd.cmdID = M_CREATE_DESC_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNumIndex;
		GlobalCmd.CMD_U.CREATE_DESC_CMD.descBaseAddress = descBaseAddress;
		GlobalCmd.CMD_U.CREATE_DESC_CMD.descInfo = descInfo;
		GlobalCmd.CMD_U.CREATE_DESC_CMD.bufAddrIncrement = bufAddrIncrement;
		GlobalCmd.CMD_U.CREATE_DESC_CMD.descOp = descOp;
		numDescWords = (descInfo & DESC_INFO_NUM_DESC_WORDS_MASK) >> DESC_INFO_NUM_DESC_WORDS_BIT_START;
		memcpy(GlobalCmd.CMD_U.CREATE_DESC_CMD.descWords,descWords, sizeof(A_UINT32)*numDescWords);
		if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.CREATE_DESC_CMD)+sizeof(GlobalCmd.cmdID), NULL)) 
		{
			uiPrintf("Error: Unable to successfully send CREATE_DESC_CMD command\n");
			return ;
		}

}

A_INT32 art_getRefClkSpeed(A_UINT32 devNum) {
    A_UINT32	     *pRegValue, regReturn;

		GlobalCmd.cmdID = GET_REF_CLK_SPEED_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		if(!artSendCmd(&GlobalCmd,
					    sizeof(GlobalCmd.cmdID),
						(void **)&pRegValue)) 
		{
			uiPrintf("Error: Unable to successfully send GET_REF_CLK_SPEED command\n");
			return -1;
		}

    return(*pRegValue);
}


A_UINT32 get_eeprom_size(A_UINT32 devNum,A_UINT32 *eepromSize, A_UINT32 *checkSumLength)
{
    #if defined(AR6002)
    A_UINT32 offset;
    A_INT32 len;
    AR6K_EEPROM *eepromPtr = (AR6K_EEPROM *)dummyEepromWriteArea;
    offset = (A_UINT32)&eepromPtr->baseEepHeader.length - (A_UINT32)eepromPtr;
    len = art_eepromRead(devNum, offset /2);
    *eepromSize = *checkSumLength = len/2;
    return 0;

    #else
	A_UINT32 eepromLower,eepromUpper;
	A_UINT32	size = 0 , length =0,checkSumLengthError=0;

	
	eepromLower = art_eepromRead(devNum,0x1B);
	eepromUpper = art_eepromRead(devNum,0x1C);
	

//	printf("eepromLower = %x\n",eepromLower);
//	printf("eepromUpper = %x\n",eepromUpper);

	
	if((eepromUpper == 0x0000)|| (eepromLower == 0x0000)){
			length = 0x400;
			size = 0x400;
	}
	if((eepromUpper == 0xffff) || (eepromLower == 0xffff))
	{
		length = 0x400;
		size = 0x400;
	}
	
	if(length!=0x400)
	{
		size = eepromUpper & 0x1f;
		size = 2 << (size + 9);
		size = size/2;
		eepromUpper = (eepromUpper & 0xffe0) >> 5 ;
		length = eepromUpper | eepromLower;				
	}

	if(length > size) 
	{
		printf("|||| WARNING CHECKSUM LENGTH IS GREATER THEAN EEPROM SIZE\n");
		checkSumLengthError =1;
		length = 0x400;
	}
		
	if(!checkSumLengthError) 
	{
		*checkSumLength = length;
//		printf("CheckSumLength in get_eeprom_size() = %x\n",*checkSumLength);
		*eepromSize = size;
//		printf("CheckSumLength in get_eeprom_size() = %x\n",*checkSumLength);
		return 0;
	}
	else
	{
		*checkSumLength = length;
//		printf(" IN ELSE CheckSumLength in get_eeprom_size() = %x\n",*checkSumLength);
		*eepromSize = size;
//		printf("IN ELSE CheckSumLength in get_eeprom_size() = %x\n",*checkSumLength);
		return 1;
	}
    #endif
}


A_BOOL eeprom_verify_checksum (A_UINT32 devNum)
{
	A_UINT32	readChecksum, computedChecksum;
	A_UINT32	length;
        A_UINT8         tmpEeprom[AR6K_EEPROM_SIZE];
        A_UINT16        *eeptr, sum=0;
        int i;
        if (isMercury_sd(swDeviceID)) {
            art_eepromReadLocs(devNum, 0, (sizeof(AR6K_EEPROM) / 2), tmpEeprom);
            eeptr = (A_UINT16 *)tmpEeprom;
            for (i = 0; i < sizeof(AR6K_EEPROM)/2; i++) {
                sum ^= *eeptr++;
            }
            if (0xffff != sum) {
                return FALSE;
            }
            return TRUE;
        }
	else if(isDragon_sd(swDeviceID)) {
	    return TRUE;
	}

	length = art_eepromRead(devNum, 0x1B);
	if((length == 0x0) ||(length == 0xffff))
		length = 0x400;

//	uiPrintf(" length in eeprom_verify_checkSum = %x\n",length);
	
	readChecksum = art_eepromRead(devNum, 0xC0);

//	uiPrintf(" Read CheckSum in eeprom_verify_checkSum = %x\n",readChecksum);

	length = length - 0xC1;

	if(length == 0xffff)
	{
		uiPrintf(" Invalid Length need calibration \n");
		return FALSE;
	}
	computedChecksum = eeprom_get_checksum(devNum, 0xC1, length);
	return (readChecksum == computedChecksum);
}

A_UINT32 eeprom_get_checksum(A_UINT32 devNum, A_UINT16 startAddr, A_UINT32 numWords ) 
{
	A_UINT32 i,xor_data=0;
	A_UINT16 addr;
//	A_UINT32 block_rd[2048] ;
	A_UINT32 *block_rd;
	
	
	block_rd = (A_UINT32 *)calloc(numWords,sizeof(A_UINT32));
	if(block_rd == NULL)
	{
			printf(" Not able to allocate memory (%d words) in eeprom_get_checksum\n", numWords);
#ifdef _IQV
			uiWriteToLog("!!!!! exit - eeprom_get_checksum\n");		
			return FALSE;
#endif	// _IQV
			exit(1);
	}
	for(i=0;i<numWords;i++)
		block_rd[i]=0xffff;

	art_eepromReadBlock(devNum, startAddr, numWords, block_rd);
	for (addr=0;addr<numWords;addr++) {
		xor_data ^= block_rd[addr];
	}
    xor_data = ~xor_data & 0xffff;
		free(block_rd);
	return(xor_data);
}

#ifndef CUSTOMER_REL		  
#ifndef __ATH_DJGPPDOS__ 
A_UINT32 art_send_frame_and_recv(
   A_UINT32 devNum, 
   A_UINT8 *pBuffer, 
   A_UINT32 tx_desc_ptr, 
   A_UINT32 tx_buf_ptr, 
   A_UINT32 rx_desc_ptr, 
   A_UINT32 rx_buf_ptr,
   A_UINT32 rate_code
) {

	A_UINT32 *pRet, retVal;

	if (thin_client) {
		GlobalCmd.cmdID = SEND_FRAME_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.SEND_FRAME_CMD.tx_desc_ptr = tx_desc_ptr;
		GlobalCmd.CMD_U.SEND_FRAME_CMD.tx_buf_ptr = tx_buf_ptr;
		GlobalCmd.CMD_U.SEND_FRAME_CMD.rx_desc_ptr = rx_desc_ptr;
		GlobalCmd.CMD_U.SEND_FRAME_CMD.rx_buf_ptr = rx_buf_ptr;
		GlobalCmd.CMD_U.SEND_FRAME_CMD.rate_code = rate_code;
		memcpy(GlobalCmd.CMD_U.SEND_FRAME_CMD.pBuffer, pBuffer, sizeof(GlobalCmd.CMD_U.SEND_FRAME_CMD.pBuffer));
		if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.SEND_FRAME_CMD)+sizeof(GlobalCmd.cmdID), (void **)&pRet)) 
		{
			uiPrintf("Error: Unable to successfully send SEND_FRAME_CMD command\n");
			return 0;
		}
		retVal = *pRet;
	}
    else {
		retVal = m_send_frame_and_recv( devNum, pBuffer, tx_desc_ptr, tx_buf_ptr, rx_desc_ptr, rx_buf_ptr, rate_code);
    }
    return retVal;
}

A_UINT32 art_recv_frame_and_xmit(
   A_UINT32 devNum, 
   A_UINT32 tx_desc_ptr, 
   A_UINT32 tx_buf_ptr, 
   A_UINT32 rx_desc_ptr, 
   A_UINT32 rx_buf_ptr,
   A_UINT32 rate_code
)  {

	A_UINT32 *pRet, retVal;

	if (thin_client) {

		GlobalCmd.cmdID = RECV_FRAME_CMD_ID;
        GlobalCmd.devNum = (A_INT8) devNum;
		GlobalCmd.CMD_U.RECV_FRAME_CMD.tx_desc_ptr = tx_desc_ptr;
		GlobalCmd.CMD_U.RECV_FRAME_CMD.tx_buf_ptr = tx_buf_ptr;
		GlobalCmd.CMD_U.RECV_FRAME_CMD.rx_desc_ptr = rx_desc_ptr;
		GlobalCmd.CMD_U.RECV_FRAME_CMD.rx_buf_ptr = rx_buf_ptr;
		GlobalCmd.CMD_U.RECV_FRAME_CMD.rate_code = rate_code;
		if(!artSendCmd(&GlobalCmd, sizeof(GlobalCmd.CMD_U.RECV_FRAME_CMD)+sizeof(GlobalCmd.cmdID), (void **)&pRet)) 
		{
			uiPrintf("Error: Unable to successfully send RECV_FRAME_CMD command\n");
			return 0;
		}
		retVal = *pRet;
	}
    else {
		retVal = m_recv_frame_and_xmit( devNum, tx_desc_ptr, tx_buf_ptr, rx_desc_ptr, rx_buf_ptr, rate_code);
    }
    return retVal;

}
#endif //__ATH_DJGPPDOS__
#endif //! CUSTOMER_REL

