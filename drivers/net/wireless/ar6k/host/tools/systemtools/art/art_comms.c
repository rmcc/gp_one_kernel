/* art_comms.c - contians functions related to inter-art comms */

/* Copyright (c) 2002 Atheros Communications, Inc., All Rights Reserved */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/art_comms.c#2 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/art/art_comms.c#2 $"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wlantype.h"
#include "dk_cmds.h"
#if defined(LINUX) || defined(__linux__)
#include "sock_linux.h"
#endif
#ifdef WIN32
#include "sock_win.h"
#endif

#include "athreg.h"
#include "manlib.h"

#ifdef ENDIAN_SWAP
#include "endian_func.h"
#endif

// VxWorks 'C' library doesnt have stricmp or strcasecmp function.
#ifdef VXWORKS
#define stricmp (strcmp)
#endif


// GLOBAL VARIABLES
static A_UINT32 sockReconnect;
static A_UINT32 sockClose;
static PIPE_CMD *PipeBuffer_p = NULL;
static CMD_REPLY *ReplyCmd_p = NULL;


/* equivalent to mdk_main, simplier function to initiate socket communication between
   2 art nodes */
void *artConnect
(
	void 
)
{
	ART_SOCK_INFO *pOSSock;  	/* Pointer to socket info data */
 

	//don't know if need these
//	sockReconnect = 0;
//	sockClose     = 0;

    /* Create the socket for dk_master to connect to.  Note that this function will not
       return until something has connected to it. */
   	pOSSock = osSockCreate("art_pipe");

	if (!pOSSock) {
        uiPrintf("Error: Something went wrong with socket creation...  Closing down!\n");
     	return NULL;
    }

    uiPrintf("Socket connection to master established.  Waiting for commands....\n");

	//return the pointer to the socket.  Needs to be passed to all other 
	return((void *) pOSSock);
}

A_BOOL prepare2WayComms
(
 void
)
{
	if(!PipeBuffer_p) {
		PipeBuffer_p = (PIPE_CMD *)A_MALLOC(sizeof(PIPE_CMD));
		if ( NULL == PipeBuffer_p ) {
 			uiPrintf("Error: Unable to malloc Receive Pipe Buffer struct\n");
			return FALSE;
   		}
	}

	if(!ReplyCmd_p) {
		ReplyCmd_p = (CMD_REPLY *)A_MALLOC(sizeof(CMD_REPLY));
 		if ( NULL == ReplyCmd_p ) {
  	   		uiPrintf("Error: Unable to malloc Reply Pipe Buffer struct\n");
	   		A_FREE(PipeBuffer_p);
	   		return FALSE;
   		}
	}
	return TRUE;
}

void cleanupSockMem
(
 void* pSock,
 A_BOOL			closeSocket
)
{
	ART_SOCK_INFO *pOSSock = (ART_SOCK_INFO *)pSock;

	if(pOSSock) {
		if(closeSocket) {
	            osSockClose(pOSSock);
//		    A_FREE(pOSSock);
		}
	}

	if(PipeBuffer_p) {
		A_FREE(PipeBuffer_p);
		PipeBuffer_p = NULL;
	}

	if(ReplyCmd_p) {
		A_FREE(ReplyCmd_p);
		ReplyCmd_p = NULL;
	}
	return;
}

A_STATUS waitForGenericCmd
(
 void *pSock,
 A_UCHAR   *pStringVar,
 A_UINT32  *pIntVar1,
 A_UINT32  *pIntVar2,
 A_UINT32  *pIntVar3
)
{
  	A_UINT16	    connectlen;
   	ART_SOCK_INFO   *pOSSock;
	A_STATUS		status = A_OK;		
	A_UINT32		sizeReplyBuf;

	pOSSock = (ART_SOCK_INFO *)pSock;
    if ( osSockRead(pOSSock, (A_UINT8 *)(&connectlen), sizeof(connectlen)) ) {
#ifdef ENDIAN_SWAP
		connectlen = ltob_s(connectlen);
#endif
		if(connectlen > sizeof(PIPE_CMD)) {
	   		uiPrintf("Error: Pipe write size too large\n");
			status = (A_STATUS)COMMS_ERR_BAD_LENGTH;
		}
		PipeBuffer_p->cmdLen = (A_UINT16) connectlen;
	}
	
	if(status == A_OK) {
		// Received the length field already. 
		// Read the command structure of the specified length.
		if(!osSockRead(pOSSock, (((A_UINT8 *)PipeBuffer_p)+sizeof(PipeBuffer_p->cmdLen)), connectlen)) {
	 		uiPrintf("Error: Problem reading command from socket\n");
			status = (A_STATUS)COMMS_ERR_BAD_LENGTH;
   		}
		
		if(PipeBuffer_p->cmdID != M_GENERIC_CMD_ID) {
			uiPrintf("Error: waitForGenericCmd: received unexpected commandID\n");
			status = (A_STATUS)COMMS_ERR_BAD_LENGTH;
		}
	}

	//extract the command args
	if(status == A_OK) {
		memcpy(pStringVar, PipeBuffer_p->CMD_U.GENERIC_CMD.stringVar, strlen(PipeBuffer_p->CMD_U.GENERIC_CMD.stringVar));
		pStringVar[strlen(PipeBuffer_p->CMD_U.GENERIC_CMD.stringVar)] = '\0';

#ifdef ENDIAN_SWAP
		PipeBuffer_p->CMD_U.GENERIC_CMD.intVar1 = ltob_l(PipeBuffer_p->CMD_U.GENERIC_CMD.intVar1);
		PipeBuffer_p->CMD_U.GENERIC_CMD.intVar2 = ltob_l(PipeBuffer_p->CMD_U.GENERIC_CMD.intVar2);
		PipeBuffer_p->CMD_U.GENERIC_CMD.intVar3 = ltob_l(PipeBuffer_p->CMD_U.GENERIC_CMD.intVar3);
#endif

		*pIntVar1 = PipeBuffer_p->CMD_U.GENERIC_CMD.intVar1;
		*pIntVar2 = PipeBuffer_p->CMD_U.GENERIC_CMD.intVar2;
		*pIntVar3 = PipeBuffer_p->CMD_U.GENERIC_CMD.intVar3;
	}

	//send back a reply
	ReplyCmd_p->status = status << COMMS_ERR_SHIFT;
    ReplyCmd_p->replyCmdId = PipeBuffer_p->cmdID;
	sizeReplyBuf = ReplyCmd_p->replyCmdLen = sizeof(ReplyCmd_p->status) + sizeof(ReplyCmd_p->replyCmdId);

#ifdef ENDIAN_SWAP
	ReplyCmd_p->replyCmdLen = btol_l(ReplyCmd_p->replyCmdLen);
	ReplyCmd_p->replyCmdId = btol_l(ReplyCmd_p->replyCmdId);
	ReplyCmd_p->status = btol_l(ReplyCmd_p->status);
#endif

	// send the reply
	if (!osSockWrite(pOSSock, (A_UINT8*)ReplyCmd_p, (sizeReplyBuf+sizeof(ReplyCmd_p->replyCmdLen)))) {
		return(A_ERROR);
    }

	return(status);
}
