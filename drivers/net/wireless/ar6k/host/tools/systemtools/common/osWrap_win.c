/* osWrap_win.c - DK 2.0 functions to hide os dependent calls */


#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/osWrap_win.c#3 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/osWrap_win.c#3 $"
static  char *rcsid =  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/osWrap_win.c#3 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/common/osWrap_win.c#3 $"; 

/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */
#ifndef PERL_CORE
 #include <winsock2.h>
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#include <Tlhelp32.h>
#include <io.h>
#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "mInst.h" 
#include "dk_common.h"

#define SPI_USB
// use this def to support TWL_ART SPI_USB com

#if defined(PREDATOR_BUILD_____)
#undef WRITE_BUF_SIZE
#define WRITE_BUF_SIZE (1 * 4092)  // 3 pages for faster transfer of large data, like pci writes in  resetDevice
#endif

#define NAME_PREFIX "\\\\.\\pipe\\"
#define SEND_BUF_SIZE		1024
#define inPipe "\\PIPE00"
#define outPipe "\\PIPE01"

#define IN_QUEUE	0
#define OUT_QUEUE	1

extern volatile A_BOOL inSignalHandler;

// Local functions
static int	socketListen(OS_SOCK_INFO    *pOSSock);
static int	socketConnect(char *target_hostname, int target_port_num,
			       A_UINT32 *ip_addr);
A_UINT32 os_com_read(OS_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len);
A_UINT32 write_device(OS_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len);
A_UINT32 read_device(OS_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len);
A_UINT32 os_com_close(OS_SOCK_INFO *pOSSock);
A_UINT32 os_com_open(OS_SOCK_INFO *pOSSock);
extern HANDLE open_device(A_UINT32 device_fn, A_UINT32 devIndex, char*);

static A_UINT8 recvBuffer[MIN_TRANSFER_SIZE];
char lpBuffer[6000];
static A_UINT8 rBuffer[MAX_TRANSFER_SIZE];
static A_UINT32 rBufferIndex;


DWORD getBytesBuffered(HANDLE handle, DWORD queueType);

/*
 * A_STATUS onlyOneRunning( char *progname)
 *
 *  DESCRIPTION: Check to make sure that there are no other programs of either of these
 *      names currently running.
 *
 *  RETURNS: A_ERROR if there is another client, else A_OK.
 *
 */

#ifdef _IQV
extern A_INT32 remoteMdkErrNo;
/*
extern HANDLE get_ene_handle(A_UINT32 device_fn);
extern A_BOOL close_ene_handle(void);
extern DWORD ene_DRG_Write(HANDLE COM_Write,  PUCHAR buf, ULONG length);
extern DWORD ene_DRG_Read(HANDLE pContext,  PUCHAR buf, ULONG length,  PULONG pBytesRead);
*/
#endif

A_STATUS onlyOneRunning(char *prog1, char *prog2)
{
	HANDLE toolHandle;
	PROCESSENTRY32 lppe;
	int progCnt = 0;

	// Snapshot the process table
	if( (HANDLE)-1 == (toolHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) )
		return A_ERROR;

	// Seach through the process table for the # of occurences of progName
	lppe.dwSize = sizeof(PROCESSENTRY32);
	if(	FALSE == Process32First(toolHandle, &lppe) ) {
		CloseHandle( toolHandle );
		return A_ERROR;
	}
    if((strstr(prog1, lppe.szExeFile) != NULL)  || 
       (strstr(prog2, lppe.szExeFile) != NULL)) {
		progCnt++;
    }
	while( FALSE != Process32Next(toolHandle, &lppe) ) {
        if((strstr(prog1, lppe.szExeFile) != NULL)  || 
           (strstr(prog2, lppe.szExeFile) != NULL)) {
			progCnt++;
        }
	}
	CloseHandle( toolHandle );
	if( progCnt > 2) 
		return A_ERROR;
	return A_OK;
}

A_STATUS osThreadCreate
    (
    void            threadFunc(void * param), 
    void		*param,
	A_CHAR*		threadName,
	A_INT32 	threadPrio,
	A_UINT32		*threadId
    )
{
	HANDLE hThread;	
	DWORD thId;
	
//	_beginthread(threadFunc, 0, param);
	hThread = CreateThread(NULL, 0, (unsigned long(__stdcall *)(void *))(threadFunc), param, 0, &thId);

    if(hThread == NULL) {
		 return A_ERROR;
	}

	if (threadId != NULL) {
		*threadId = (A_UINT32)hThread;
	}

    return A_OK;
}  

void osThreadKill
(
	A_UINT32 threadId
)
{
	HANDLE hThread;	

	hThread = (HANDLE)threadId;
	TerminateThread(hThread,0);
	
	return;
}


/**************************************************************************
* osSockRead - read len bytes into *buf from the socket in pSockInfo
*
* This routine calls recv for socket reading
* 
* RETURNS: length read
*/

A_INT32
osSockRead
(
    OS_SOCK_INFO    *pSockInfo,
    A_UINT8         *buf,
    A_INT32         len
)
{
    int dwRead, i = 0;
	A_INT32 cnt;
	A_UINT8* bufpos; 
	A_INT32 tmp_len;
    	A_UINT32 err;
	A_INT32 total_len;
	A_INT32		bytesRemaining; 
	static A_INT32  bytesRead=0; 
	static A_INT32    cmdlen_read=0, cmdlen_size=0;


#ifdef _DEBUG_XXX
    q_uiPrintf("osSockRead: buf=0x%08lx  len=%d\n", (unsigned long) buf, len);
#endif
	if (pSockInfo->port_num != USB_PORT_NUM && inSignalHandler == TRUE) return -1;

	tmp_len = len;
   	bufpos = buf;
	dwRead = 0;
	
    switch(pSockInfo->port_num) {
            case MBOX_PORT_NUM:
//#ifndef _IQV
                if(DRG_Read(pSockInfo->sockfd,buf,len,0) < 0)
//#else
//				if (ene_DRG_Read(pSockInfo->sockfd,buf,len,0) < 0)
//#endif	// _IQV
                {
                    uiPrintf("ERROR::DRG_Read::SDIO  read failed with error\n");
                    return 0;
                }
	            dwRead = len;
                break;
            case COM_PORT_NUM:{
                    if ((err=os_com_read(pSockInfo, buf, &tmp_len)) != 0) {
                        uiPrintf("ERROR::osSockRead::Com port read failed with error = %x\n", err);
                        return 0;
                    }
                    dwRead = tmp_len; // return number of bytes read
                break;
	            } // end of case
            case SOCK_PORT_NUM: {
                while (len) {
#ifdef _IQV
	ex_timeStart();	
#endif	// _IQV
                    cnt = recv(pSockInfo->sockfd, (char *)bufpos, len, 0);
#ifdef _IQV
 	ex_timeDiff("SOCK_PORT_NUM recv");
#endif	// _IQV
                    if ((cnt == SOCKET_ERROR) || (!cnt)) break;

                    dwRead += cnt;
                    len  -= cnt;
                    bufpos += cnt;
                }

                len = tmp_len;

                if (dwRead != len) {
                    dwRead = 0;
                }
#ifdef _DEBUG_XXX
                else {
                    for (i=0;(i<len)&&(i<16);i++) {
                        q_uiPrintf(" %02X",*((unsigned char *)buf+i));
                        if (3==(i%4)) q_uiPrintf(" ");
                    }
                    q_uiPrintf("\n");
                }
#endif // _DEBUG
                break;
                }
            case USB_PORT_NUM: {
	            if (!cmdlen_read) {
					total_len = MIN_TRANSFER_SIZE;
						pSockInfo->sockfd = pSockInfo->inHandle;
                    	if ((err=read_device(pSockInfo, recvBuffer, &total_len)) != 0) {
                      		uiPrintf("ERROR::osSockRead::USB  read failed with error = %x\n", err);
                      		return 0;
                    	}
#ifdef _DEBUG_XXX
		        q_uiPrintf("osSockRead::cmdlen_read=%d:recv_buf_sem set\n", cmdlen_read);
#endif
			memcpy(rBuffer, recvBuffer, MIN_TRANSFER_SIZE);
			memcpy(buf, rBuffer, len);
			rBufferIndex = MIN_TRANSFER_SIZE;
			cmdlen_size = len;
			bytesRead = MIN_TRANSFER_SIZE - len;
		    }
		    else {
	  		  bytesRemaining = len - bytesRead;
#ifdef _DEBUG_XXX
			  q_uiPrintf("bytesRemaining=%d\n", bytesRemaining);
#endif
	  		  while(bytesRemaining>0) {
				total_len = MIN_TRANSFER_SIZE;
							pSockInfo->sockfd = pSockInfo->inHandle;
                    		if ((err=read_device(pSockInfo, recvBuffer, &total_len)) != 0) {
                      		   uiPrintf("ERROR::osSockRead::USB  read failed with error = %x\n", err);
                      		   return 0;
                    		}
#ifdef _DEBUG_XXX
				q_uiPrintf("osSockRead::cmdlen_read=%d:recv_buf_sem set\n", cmdlen_read);
#endif
				memcpy(rBuffer+rBufferIndex, recvBuffer, MIN_TRANSFER_SIZE);
	    			bytesRead+= MIN_TRANSFER_SIZE;
				rBufferIndex+=MIN_TRANSFER_SIZE;
	    			bytesRemaining = len - bytesRead;
	  		 }
	  		 memcpy(buf, rBuffer+cmdlen_size, len);
		    }
		    cmdlen_read = !cmdlen_read;
		    dwRead = bytesRead;
#ifdef _DEBUG_XXX
                    for (i=0;i<len;i++) {
                        q_uiPrintf(" %02X",*((unsigned char *)buf+i));
                        if (3==(i%4)) q_uiPrintf(" ");
                    }
                    q_uiPrintf("\n");
#endif
              		break;
            		}// end of case
    }
#ifdef _DEBUG_XXX
    q_uiPrintf(":osSockRead:%d read\n", dwRead);
#endif

    return dwRead;
}

/**************************************************************************
* osSockWrite - write len bytes into the socket, pSockInfo, from *buf
*
* This routine calls a OS specific routine for socket writing
* 
* RETURNS: length read
*/
A_INT32
osSockWrite
    (
    OS_SOCK_INFO    *pSockInfo,
    A_UINT8         *buf,
    A_INT32         len
    )
{
		int	dwWritten, i = 0;
		A_INT32 bytes,cnt;
		A_UINT8* bufpos; 
		A_INT32 tmp_len;
        	A_UINT32 err;
		A_UINT8  *pad_buf;
		A_INT32 numblocks, pad_len, total_len;

#ifdef _IQV
	DWORD uExitCode;
//	HANDLE	devlibHandle;
	char wMsg[1000];
#endif

//		if (inSignalHandler == TRUE) return 0;
	if (pSockInfo->port_num != USB_PORT_NUM && inSignalHandler == TRUE) return -1;

#ifdef _DEBUG_XXX 
    q_uiPrintf("osSockWrite: buf=0x%08lx  len=%d\n", (unsigned long) buf, len);
    for (i=0;(i<len)&&(i<16);i++) {
      q_uiPrintf(" %02X",*((unsigned char *)buf +i));
      if (3==(i%4)) q_uiPrintf(" ");
    }
    q_uiPrintf("\n");
#endif // _DEBUG

    switch(pSockInfo->port_num) {
            case MBOX_PORT_NUM:
//#ifndef _IQV
                if(DRG_Write(pSockInfo->sockfd,buf,len) < 0)
//#else
//                if(ene_DRG_Write(pSockInfo->sockfd,buf,len) < 0)
//#endif	// _IQV
                {
                    uiPrintf("ERROR::DRG_Write::SDIO  write failed with error\n");
                    return 0;
                }
	            dwWritten = len;
                break;
            case COM_PORT_NUM: {
                if ((err=write_device(pSockInfo, buf, &len)) != 0) {
                        uiPrintf("ERROR::osSockWrite::Com port write failed with error = %x\n", err);
                        return 0;
                }
	            dwWritten = len;
                break;
	            } // end of case
            case SOCK_PORT_NUM: {
		        tmp_len = len;
	   	        bufpos = buf;
		        dwWritten = 0;

			
		        while (len) {
			        if (len < SEND_BUF_SIZE) bytes = len;
			        else bytes = SEND_BUF_SIZE;
	    	
			        cnt = send(pSockInfo->sockfd, (char *)bufpos, bytes, 0);
    
			        if (cnt == SOCKET_ERROR) break;
    
			        dwWritten += cnt;
		    	
			        len  -= cnt;
			        bufpos += cnt;
    	        }

		        len = tmp_len;
    
		        if (dwWritten != len) {
			        dwWritten = 0;
		        }

            break;
	        } // end of case
            case USB_PORT_NUM: {
		numblocks = (len/MIN_TRANSFER_SIZE) + ((len%MIN_TRANSFER_SIZE)?1:0);
		total_len = numblocks * MIN_TRANSFER_SIZE;
		pad_buf = (A_UINT8 *)A_MALLOC(total_len * sizeof(A_UINT8));
		pad_len = (numblocks*MIN_TRANSFER_SIZE) - len;
		memcpy(pad_buf, buf, len);
#ifdef _DEBUG_XXX
		q_uiPrintf("osSockWrite::numblocks=%d:total_len=%d:pad_len=%d:actual len=%d\n", numblocks, total_len, pad_len, len);
#endif
				pSockInfo->sockfd = pSockInfo->outHandle;
                if ((err=write_device(pSockInfo, pad_buf, &total_len)) != 0) {
                        uiPrintf("ERROR::osSockWrite::USB port write failed with error = %x\n", err);
			A_FREE(pad_buf);
                        return 0;
                }
#ifdef _DEBUG_XXX
		q_uiPrintf("osSockWrite::total bytes written = %d:Padded len = %d:Actual len = %d\n", total_len, (numblocks * MIN_TRANSFER_SIZE), len);
#endif
		A_FREE(pad_buf);
		if (total_len == (numblocks * MIN_TRANSFER_SIZE))
	        	dwWritten = len;
		else 
			dwWritten = total_len;
            break;
            } 
    }// end of switch

    return dwWritten;
}

/**************************************************************************
* osSockClose - close socket
*
* Close the handle to the pipe
*
* RETURNS: 0 if error, non 0 if no error
*/
void
osSockClose(OS_SOCK_INFO* pOSSock)
{
	A_UINT32 err;

#ifdef _DEBUG_XXX
    q_uiPrintf("osSockClose::hostname=%s\n", pOSSock->hostname);
#endif

    switch(pOSSock->port_num) {
            case COM_PORT_NUM: {
                if ((err=os_com_close(pOSSock)) != 0) {
                        uiPrintf("ERROR::osSockClose::Com port close failed with error = %x\n", err);
                        return;
                }
                break;
	            } // end of case
            case SOCK_PORT_NUM: {
	            if (inSignalHandler == TRUE) return;
                closesocket(pOSSock->sockfd);
	            A_FREE(pOSSock);
                break;
	        }
            case USB_PORT_NUM:{
             //   CloseHandle((HANDLE) pOSSock->sockfd);
                CloseHandle((HANDLE)pOSSock->inHandle);
                CloseHandle((HANDLE)pOSSock->outHandle);
                break;
            }
/*
#ifdef _IQV
            case MBOX_PORT_NUM:{
				close_ene_handle();
				if (pOSSock!=NULL) {
					free(pOSSock);
					pOSSock = NULL;
				}
                break;
            }
#endif
*/
    }
    return;
}





OS_SOCK_INFO*
osSockConnect(char *pname)
{
    char		pname_lcl[256];
    char *		mach_name;
    char *		cp;
    OS_SOCK_INFO *pOSSock;
    int			res;
    A_UINT32 err;
    HANDLE handle;

    strncpy(pname_lcl, pname, sizeof(pname_lcl));
    pname_lcl[sizeof(pname_lcl) - 1] = '\0';
#ifdef _DEBUG_XXX
    q_uiPrintf("osSockConnect: pname_lcl = '%s'\n", pname_lcl);
#endif
    mach_name = pname_lcl;
    while (*mach_name == '\\') {
	    mach_name++;
    }
    for (cp = mach_name; (*cp != '\0') && (*cp != '\\'); cp++) {
    }
    *cp = '\0';
    
#ifdef _DEBUG_XXX
    q_uiPrintf("osSockConnect: starting mach_name = '%s'\n", mach_name);
#endif

    if (!strcmp(mach_name, ".")) {
	    /* A windows convention meaning "local machine" */
	    mach_name = "localhost";
    }

    q_uiPrintf("osSockConnect: revised mach_name = '%s'\n", mach_name);

    pOSSock = (OS_SOCK_INFO *) A_MALLOC(sizeof(OS_SOCK_INFO));
    if(!pOSSock) {
		uiPrintf("ERROR::osSockConnect: malloc failed for pOSSock\n");
        return NULL;
	}

    strncpy(pOSSock->hostname, mach_name, sizeof(pOSSock->hostname));
    pOSSock->hostname[sizeof(pOSSock->hostname) - 1] = '\0';

    pOSSock->port_num = SOCK_PORT_NUM;

    if (strnicmp("COM",pOSSock->hostname, strlen("COM")) == 0) {
       pOSSock->port_num = COM_PORT_NUM;
	}
    if (!stricmp(pOSSock->hostname, "USB")) {
       pOSSock->port_num = USB_PORT_NUM;
	}

    if (!stricmp(mach_name, "SDIO")) {
        pOSSock->port_num = MBOX_PORT_NUM;
    }

    switch(pOSSock->port_num) {
       case MBOX_PORT_NUM: 
//#ifndef _IQV
            handle = open_device_ene(SDIO_FUNCTION, 0, NULL);
//#else
//			handle = get_ene_handle(SDIO_FUNCTION);
//#endif	// _IQV
            pOSSock->sockfd =(A_INT32) handle;
            break;
       case USB_PORT_NUM: {
             pOSSock->sockDisconnect = 0;
             pOSSock->sockClose = 0;
             handle = open_device(USB_FUNCTION, 0, inPipe);
             pOSSock->inHandle  = (A_INT32) handle;
             if (handle == INVALID_HANDLE_VALUE) {
                printf("Error:osSockConnect::Invalid Handle to inPipe:%s\n", inPipe);
                A_FREE(pOSSock);
	            return NULL;
             }
             handle = open_device(USB_FUNCTION, 0, outPipe);
             pOSSock->outHandle  = (A_INT32) handle;
             if (handle == INVALID_HANDLE_VALUE) {
                printf("Error:osSockConnect::Invalid Handle outPipe:%s\n", outPipe);
                A_FREE(pOSSock);
	            return NULL;
             }
             break;
             }
       case COM_PORT_NUM:  {
          pOSSock->sockDisconnect = 0;
          pOSSock->sockClose = 0;
          if ((err=os_com_open(pOSSock)) != 0) {
              uiPrintf("ERROR::osSockConnect::Com port open failed with error = %x\n", err);
              exit(0);
          }
       break;
       } // end of case
       case SOCK_PORT_NUM: {
         res = socketConnect(pOSSock->hostname, pOSSock->port_num,
				      &pOSSock->ip_addr);;
         if (res < 0) {
   	      uiPrintf("ERROR::osSockConnect: pipe connect failed\n");
          A_FREE(pOSSock);
	      return NULL;
         }
	     q_uiPrintf("osSockConnect: Connected to pipe\n");

	     q_uiPrintf("ip_addr = %d.%d.%d.%d\n",
	        (pOSSock->ip_addr >> 24) & 0xff,
	        (pOSSock->ip_addr >> 16) & 0xff,
	        (pOSSock->ip_addr >> 8) & 0xff,
                  (pOSSock->ip_addr >> 0) & 0xff);
               pOSSock->sockfd = res;
               break;
	   } // end of else
    }

	return pOSSock;
}



/* osSockAccept - Wait for a connection
*
*/
OS_SOCK_INFO *osSockAccept
	(
	OS_SOCK_INFO *pOSSock
	)
{
	OS_SOCK_INFO *pOSNewSock;
	int		i;
	int		sfd;
	struct sockaddr_in	sin;

	pOSNewSock = (OS_SOCK_INFO *) A_MALLOC(sizeof(*pOSNewSock));
	if (!pOSNewSock) {
		uiPrintf("ERROR::osSockAccept: malloc failed for pOSNewSock \n");
		return NULL;
	}

	i = sizeof(sin);
	sfd = accept(pOSSock->sockfd, (struct sockaddr *) &sin, (int *)&i);
	if (sfd == INVALID_SOCKET) {
		A_FREE(pOSNewSock);
		uiPrintf("ERROR::osSockAccept: accept failed: %d\n", WSAGetLastError());
		WSACleanup( );
		return NULL;
	}
   	
  	strcpy(pOSNewSock->hostname, inet_ntoa(sin.sin_addr));
	pOSNewSock->port_num = sin.sin_port;
	
	pOSNewSock->sockClose = 0;
	pOSNewSock->sockDisconnect = 0;
	pOSNewSock->sockfd = sfd;

	return pOSNewSock;
}

OS_SOCK_INFO* osSockListen(A_UINT32 acceptFlag, A_UINT16 port)
{
    OS_SOCK_INFO *pOSSock;
	OS_SOCK_INFO *pOSNewSock;
    A_UINT32 err;


    pOSSock = (OS_SOCK_INFO *) A_MALLOC(sizeof(OS_SOCK_INFO));
    if(!pOSSock) {
		uiPrintf("ERROR::osSockListen: malloc failed for pOSSock\n");
        return NULL;
	}

	pOSSock->port_num = port;
	switch(pOSSock->port_num) {
			case COM1_PORT_NUM:
    			strcpy(pOSSock->hostname, "COM1");
				break;
			case COM2_PORT_NUM:
    			strcpy(pOSSock->hostname, "COM2");
				break;
			case COM3_PORT_NUM:
    			strcpy(pOSSock->hostname, "COM3");
				break;
			case COM4_PORT_NUM:
    			strcpy(pOSSock->hostname, "COM4");
				break;

			case SOCK_PORT_NUM:
    			strcpy(pOSSock->hostname, "localhost");
				break;
	}
	pOSSock->sockDisconnect = 0;
	pOSSock->sockClose = 0;

	if (port == SOCK_PORT_NUM) {	
       pOSSock->sockfd = socketListen(pOSSock);
	   if(pOSSock->sockfd < 0) {
	   	   uiPrintf("ERROR::osPipeCreate: socket creation failed\n");
           A_FREE(pOSSock);
		   return NULL;
	   }

	   if (acceptFlag) {
		   pOSNewSock = osSockAccept(pOSSock);
		   if (!pOSNewSock) {
			   uiPrintf("ERROR::osSockListen: malloc failed for pOSSock \n");
			   osSockClose(pOSSock);
			   return NULL;
		   }
		   osSockClose(pOSSock);
		   pOSSock = pOSNewSock;
	   }
	}
	else {
		if ((err=os_com_open(pOSSock)) != 0) {
			uiPrintf("osSockListen::Com port open failed\n");
		}
	}

	return pOSSock;
}

static int	
socketCreateAccept(int port_num)
{
    int	   sockfd;
    struct protoent *	proto;
    int	   res;
    struct sockaddr_in sin;
    int	   i;
    int	   sfd;
	WORD   wVersionRequested;
	WSADATA wsaData;
 
	wVersionRequested = MAKEWORD( 2, 2 );
 
	res = WSAStartup( wVersionRequested, &wsaData );
	if ( res != 0 ) {
		uiPrintf("socketCreateAccept: Could not find windows socket library\n");
		return -1;
	}
 
	if ( LOBYTE( wsaData.wVersion ) != 2 ||
        HIBYTE( wsaData.wVersion ) != 2 ) {
		uiPrintf("socketCreateAccept: Could not find windows socket library\n");
		WSACleanup( );
		return -1; 
	}

    if((proto = getprotobyname("tcp")) == NULL) {
    	uiPrintf("socketCreateAccept: getprotobyname failed: %d\n", WSAGetLastError());
   		WSACleanup( );
	    return -1;
    }

    q_uiPrintf("socket start\n");
    sockfd = WSASocket(PF_INET, SOCK_STREAM, proto->p_proto, NULL, (GROUP)NULL, 0);
    q_uiPrintf("socket end\n");
    if (sockfd == INVALID_SOCKET) {
	    uiPrintf("socketCreateAccept: socket failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    /* Allow immediate reuse of port */
    q_uiPrintf("setsockopt SO_REUSEADDR start\n");
    i = 1;
    res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("socketCreateAccept: setsockopt failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt SO_REUSEADDR end\n");

    /* Set TCP Nodelay */
    q_uiPrintf("setsockopt TCP_NODELAY start\n");
    i = 1;
    res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("socketCreateAccept: setsockopt failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt TCP_NODELAY end\n");

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr =  INADDR_ANY;
    sin.sin_port = htons((A_UINT16) port_num);

    q_uiPrintf("bind start\n");
    res = bind(sockfd, (struct sockaddr *) &sin, sizeof(sin));
    q_uiPrintf("bind end\n");
    if (res != 0) {
	    uiPrintf("socketCreateAccept: bind failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    q_uiPrintf("listen start\n");
    res = listen(sockfd, 4);
    q_uiPrintf("listen end\n");
    if (res != 0) {
	    uiPrintf("socketCreateAccept: listen failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    /* Call accept */
    q_uiPrintf("accept start\n");
    i = sizeof(sin);
    uiPrintf("* socket created, waiting for connection...\n");
    sfd = accept(sockfd, (struct sockaddr *) &sin, &i);
    q_uiPrintf("accept end\n");
    if (sfd == INVALID_SOCKET) {
	    uiPrintf("socketCreateAccept: accept failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    q_uiPrintf("accept: sin.sin_family=0x%x\n", (int) sin.sin_family);
    q_uiPrintf("accept: sin.sin_port=0x%x (%d)\n", (int) sin.sin_port,
	    (int) ntohs(sin.sin_port));
    q_uiPrintf("accept: sin.sin_addr=0x%08x\n", (int) ntohl(sin.sin_addr.s_addr));

    res = closesocket(sockfd);
    if (res != 0) {
	    uiPrintf("socketCreateAccept: closesocket failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    return sfd;
}


ART_SOCK_INFO*
osSockCreate(char *pname)
{
    ART_SOCK_INFO *pOSSock;

    pOSSock = (ART_SOCK_INFO *) A_MALLOC(sizeof(ART_SOCK_INFO));
    if(!pOSSock) {
		uiPrintf("osSockCreate: malloc failed for pOSSock\n");
        return NULL;
	}

    strcpy(pOSSock->hostname, "localhost");
    pOSSock->port_num = SOCK_PORT_NUM;

    pOSSock->sockfd = socketCreateAccept(pOSSock->port_num);
	if(pOSSock->sockfd < 0) {
		uiPrintf("osPipeCreate: socket creation failed\n");
        A_FREE(pOSSock);
		return NULL;
	}

    return pOSSock;
}

static int	socketListen(OS_SOCK_INFO* pOSSock)
{
    int	   sockfd;
    struct protoent *	proto;
    int	   res;
    struct sockaddr_in sin;
    int	   i;
	WORD   wVersionRequested;
	WSADATA wsaData;
 
	wVersionRequested = MAKEWORD( 2, 2 );
 
	res = WSAStartup( wVersionRequested, &wsaData );
	if ( res != 0 ) {
		uiPrintf("socketCreateAccept: Could not find windows socket library\n");
		return -1;
	}
 
	if ( LOBYTE( wsaData.wVersion ) != 2 ||
        HIBYTE( wsaData.wVersion ) != 2 ) {
		uiPrintf("socketCreateAccept: Could not find windows socket library\n");
		WSACleanup( );
		return -1; 
	}

    if((proto = getprotobyname("tcp")) == NULL) {
    	uiPrintf("ERROR::socketCreateAccept: getprotobyname failed: %d\n", WSAGetLastError());
   		WSACleanup( );
	    return -1;
    }

    q_uiPrintf("socket start\n");
    sockfd = WSASocket(PF_INET, SOCK_STREAM, proto->p_proto, NULL, (GROUP)NULL, 0);
    q_uiPrintf("socket end\n");
    if (sockfd == INVALID_SOCKET) {
	    uiPrintf("ERROR::socketCreateAccept: socket failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    /* Allow immediate reuse of port */
    q_uiPrintf("setsockopt SO_REUSEADDR start\n");
    i = 1;
    res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("ERROR::socketCreateAccept: setsockopt failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt SO_REUSEADDR end\n");

    /* Set TCP Nodelay */
    q_uiPrintf("setsockopt TCP_NODELAY start\n");
    i = 1;
    res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("ERROR::socketCreateAccept: setsockopt failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt TCP_NODELAY end\n");

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr =  INADDR_ANY;
    sin.sin_port = htons(pOSSock->port_num);

    q_uiPrintf("bind start\n");
    res = bind(sockfd, (struct sockaddr *) &sin, sizeof(sin));
    q_uiPrintf("bind end\n");
    if (res != 0) {
	    uiPrintf("ERROR::socketCreateAccept: bind failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    q_uiPrintf("listen start\n");
    res = listen(sockfd, 4);
    q_uiPrintf("listen end\n");
    if (res != 0) {
	    uiPrintf("ERROR::socketCreateAccept: listen failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    return sockfd;
}

static int
socketConnect(char *target_hostname, int target_port_num, A_UINT32 *ip_addr)
{
    int	   sfd;
    struct protoent *	proto;
    int	   res;
    struct sockaddr_in	sin;
    int	   i;
    int	   ffd;
    struct hostent *hostent;
	WORD   wVersionRequested;
	WSADATA wsaData;

    ffd = fileno(stdout);
	wVersionRequested = MAKEWORD( 2, 2 );
 
	res = WSAStartup( wVersionRequested, &wsaData );
	if ( res != 0 ) {
		uiPrintf("socketConnect: Could not find windows socket library\n");
		return -1;
	}
 
	if ( LOBYTE( wsaData.wVersion ) != 2 ||
        HIBYTE( wsaData.wVersion ) != 2 ) {
		uiPrintf("socketConnect: Could not find windows socket library\n");
		WSACleanup( );
		return -1; 
	}

    if((proto = getprotobyname("tcp")) == NULL) {
    	uiPrintf("ERROR::socketConnect: getprotobyname failed: %d\n", WSAGetLastError());
   		WSACleanup( );
	    return -1;
    }

    q_uiPrintf("socket start\n");
    sfd = WSASocket(PF_INET, SOCK_STREAM, proto->p_proto, NULL, (GROUP)NULL,0);
    q_uiPrintf("socket end\n");
    if (sfd == INVALID_SOCKET) {
	    uiPrintf("ERROR::socketConnect: socket failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    /* Allow immediate reuse of port */
    q_uiPrintf("setsockopt SO_REUSEADDR start\n");
    i = 1;
    res = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("ERROR::socketConnect: setsockopt SO_REUSEADDR failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt SO_REUSEADDR end\n");

    /* Set TCP Nodelay */
    q_uiPrintf("setsockopt TCP_NODELAY start\n");
    i = 1;
    res = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("ERROR::socketCreateAccept: setsockopt TCP_NODELAY failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt TCP_NODELAY end\n");

    q_uiPrintf("gethostbyname start\n");
    q_uiPrintf("socket_connect: target_hostname = '%s'\n", target_hostname);
    hostent = gethostbyname(target_hostname);
    q_uiPrintf("gethostbyname end\n");
    if (!hostent) {
	    uiPrintf("ERROR::socketConnect: gethostbyname failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	

    memcpy(ip_addr, hostent->h_addr_list[0], hostent->h_length);
    *ip_addr = ntohl(*ip_addr);

    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr.s_addr, hostent->h_addr_list[0], hostent->h_length);
    sin.sin_port = htons((short)target_port_num);

    for (i = 0; i < 20; i++) {
        q_uiPrintf("connect start %d\n", i);
	    res = connect(sfd, (struct sockaddr *) &sin, sizeof(sin));
        q_uiPrintf("connect end %d\n", i);
	    if (res == 0) {
	        break;
	    }
	    milliSleep(1);
    }
    if (i == 20) {
	    uiPrintf("ERROR::connect failed completely\n");
        WSACleanup( );
	    return -1;
    }

    return sfd;
}

A_UINT32 semInit
(
	void
) 
{
	HANDLE hSemaphore;

	hSemaphore = CreateSemaphore(NULL, 1, 1, NULL);

	if (hSemaphore == NULL) {
		return 0;
	}

	return (A_UINT32)hSemaphore;
}

A_INT32 semLock
(
	A_UINT32 sem
)
{
	HANDLE hSemaphore;

	hSemaphore = (HANDLE)sem;
	if (WaitForSingleObject(hSemaphore, INFINITE) == WAIT_FAILED) {
		return -1;
	}

	return 0;
}

A_INT32 semUnLock
(
	A_UINT32 sem
)
{
	HANDLE hSemaphore;

	hSemaphore = (HANDLE)sem;
	if (ReleaseSemaphore(hSemaphore, 1, NULL) == 0) {
		return -1;
	}

	return 0;
}

A_INT32 semClose
(
	A_UINT32 sem
)
{
	HANDLE hSemaphore;

	hSemaphore = (HANDLE)sem;
	if (CloseHandle(hSemaphore) == 0) {
		return -1;
	}

	return 0;
}

extern A_BOOL com_client;
A_UINT32 os_com_open(OS_SOCK_INFO *pOSSock) {
    DCB          dcb;
    A_UINT32 nComErr=0;
    A_UINT32 err=0;
    A_UINT32 len;
    A_UINT32 i=0;
    A_CHAR tmphostname[128];

	// Close the port 
    (void)os_com_close(pOSSock);

    //make the below change to support the serial port num > 9
    if(com_client)
    {
        len = strlen(pOSSock->hostname);
        memcpy(tmphostname,pOSSock->hostname,128);      		
        pOSSock->hostname[0] = '\\';
        pOSSock->hostname[1] = '\\';
        pOSSock->hostname[2] = '.';
        pOSSock->hostname[3] = '\\';
        for(i=4; i<len+4; i++)
        {
             pOSSock->hostname[i] = tmphostname[i-4];
        }
        uiPrintf("revised pOSSock->hostname=%s\n",pOSSock->hostname);
    }
        
    // device handle
    pOSSock->sockfd = (A_UINT32) CreateFile( pOSSock->hostname,                             // port name
                       GENERIC_READ | GENERIC_WRITE,     // allow r/w access
                       0,                                // always no sharing
                       0,                                // no security atributes for file
                       OPEN_EXISTING,                    // always open existing
                       //FILE_FLAG_OVERLAPPED,            // overlapped operation   
                       FILE_FLAG_NO_BUFFERING,            // non-overlapped operation   
                       0);                               // always no file template

    if ((HANDLE)pOSSock->sockfd == INVALID_HANDLE_VALUE) {
       nComErr = nComErr | COM_ERROR_GETHANDLE;
       return 0;
    }

    // port configuration
    FillMemory (&dcb, sizeof(dcb),0);
    dcb.DCBlength = sizeof(dcb);
//    if (!BuildCommDCB("19200,n,8,1", &dcb)) {
#ifdef SPI_USB
    if (!BuildCommDCB("115200,n,8,2", &dcb)) {
#else
    if (!BuildCommDCB("38400,n,8,1", &dcb)) {
#endif
    //if (!BuildCommDCB("115200,n,8,1", &dcb)) {
       nComErr = nComErr | COM_ERROR_BUILDDCB;
       return nComErr;
    }
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
#ifdef SPI_USB
    dcb.fDtrControl = DTR_CONTROL_ENABLE; //DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE; //RTS_CONTROL_DISABLE;
#else
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
#endif
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    if (!SetCommState((HANDLE) pOSSock->sockfd, &dcb)) {
       nComErr = nComErr | COM_ERROR_CONFIGDEVICE;
       return nComErr;
    }
    if (!SetupComm((HANDLE) pOSSock->sockfd, READ_BUF_SIZE, WRITE_BUF_SIZE)) {
       nComErr = nComErr | COM_ERROR_CONFIGBUFFERS;
       return nComErr;
    }
    if (!EscapeCommFunction((HANDLE) pOSSock->sockfd, SETDTR)) {
       nComErr = nComErr | COM_ERROR_SETDTR;
       return nComErr;
    }
    if (!PurgeComm((HANDLE) pOSSock->sockfd, PURGE_RXABORT | PURGE_RXCLEAR |
                       PURGE_TXABORT | PURGE_TXCLEAR)) {
       nComErr = nComErr | COM_ERROR_PURGEBUFFERS;
       return nComErr;
    }
	

    // set mask to notify thread if a character was received
    if (!SetCommMask((HANDLE) pOSSock->sockfd, EV_RXCHAR|EV_BREAK|EV_RXFLAG)) {
       // error setting communications event mask
       nComErr = nComErr | COM_ERROR_CONFIGDEVICE;
       return nComErr;
    }


    return 0;
}

A_UINT32 os_com_close(OS_SOCK_INFO *pOSSock) { 

    // reset error byte
    A_UINT32 nComErr = 0;

    if (inSignalHandler == TRUE) return -1;

    if (!EscapeCommFunction((HANDLE) pOSSock->sockfd, CLRDTR)) {
       nComErr = nComErr | COM_ERROR_CLEARDTR;
       return nComErr;
    }
    if (!PurgeComm((HANDLE) pOSSock->sockfd, PURGE_RXABORT | PURGE_RXCLEAR |
                       PURGE_TXABORT | PURGE_TXCLEAR)) {
       nComErr = nComErr | COM_ERROR_PURGEBUFFERS;
       return nComErr;
    }

    // device handle
    CloseHandle((HANDLE) pOSSock->sockfd);
	

    return 0;

}

A_UINT32 write_device(OS_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len) {
    A_UINT32 nComErr; A_INT32 written_len, tmp_len;
    A_UINT32 status, jIndex=0, numblocks, remaining_bytes;

    	// reset error byte
    	nComErr = 0;
	written_len = *len;
	// split the write

	numblocks = (*len/WRITE_BUF_SIZE); 
	remaining_bytes = *len - (numblocks * WRITE_BUF_SIZE);
#ifdef _DEBUG_XXX
	q_uiPrintf("write_device::sockfd=%x:", pOSSock->sockfd);
	q_uiPrintf("numblocks = %d:remainingbytes = %x\n", numblocks, remaining_bytes);
#endif
	for(jIndex=0; jIndex<numblocks; jIndex++) {
       status = WriteFile((HANDLE) pOSSock->sockfd, (const char *)&buf[jIndex * WRITE_BUF_SIZE], WRITE_BUF_SIZE, (A_UINT32 *)&tmp_len, NULL);
       if (!status || tmp_len != WRITE_BUF_SIZE) {
           //nComErr = nComErr | COM_ERROR_WRITE;
           nComErr = GetLastError();
			uiPrintf("write_device::Error=%x\n", nComErr);
           return nComErr;
       }
	  // milliSleep(5);
	}
	if (remaining_bytes) {	
    // write the com port
       status = WriteFile((HANDLE) pOSSock->sockfd, (const char *)&buf[jIndex * WRITE_BUF_SIZE], remaining_bytes, (A_UINT32 *)&tmp_len, NULL);
       if (!status || tmp_len != (A_INT32) remaining_bytes) {
           nComErr = nComErr | COM_ERROR_WRITE;
           nComErr = GetLastError();
		   uiPrintf("write_device::Error=%x\n", nComErr);
           return nComErr;
       }
	}

    return 0;

}

A_UINT32 os_com_read(OS_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len) {

    A_UINT32 dwCommEvent;
    DWORD dwRead, pos = 0, numCharsToRead, numCharsInBuffer, iIndex;
	A_UINT8 *chReadBuf;
    A_UINT32 nComErr;

    // reset error byte
    nComErr = 0;
#ifdef _DEBUG_XXX
	uiPrintf("os_com_read::");
	uiPrintf("enter while loop::with *len = %d\n", *len);
#endif

	chReadBuf=buf;


    while(pos < (A_UINT32)*len) {


	  numCharsInBuffer = getBytesBuffered((HANDLE)pOSSock->sockfd, IN_QUEUE);

	  if (numCharsInBuffer == 0) { 

		  // Wait for the event

#ifdef _DEBUG_XXX
	      uiPrintf("Waiting for Event:numCharsInBuffer = %d\n", numCharsInBuffer);
#endif

          if ( WaitCommEvent((HANDLE) pOSSock->sockfd, &dwCommEvent, NULL)) {
#ifdef _DEBUG_XXX
			  uiPrintf("Event obtained = %x:pos=%d:*len=%d\n", dwCommEvent, pos, *len);
#endif
	         if (!(dwCommEvent&EV_RXCHAR) ) continue;
	      }
	      else continue;

	  }

       // read receive buffer
	  dwRead=0;

	  numCharsInBuffer = getBytesBuffered((HANDLE)pOSSock->sockfd, IN_QUEUE);
	
	  if (numCharsInBuffer == 0)  continue;

	  if (numCharsInBuffer > (A_UINT32)(*len-pos))
		numCharsToRead = (*len-pos);
	  else
		numCharsToRead = numCharsInBuffer;
		  
#ifdef _DEBUG_XXX
	  		printf("pos=%d:len=%d\n", pos, *len);
			printf("Number of bytes in buffer=%d:to request=%d:pos=%d:total to read=%d\n", numCharsInBuffer, numCharsToRead, pos, *len);
#endif

      if (ReadFile((HANDLE) pOSSock->sockfd, chReadBuf, numCharsToRead, &dwRead, NULL)) {
		 if (dwRead != numCharsToRead) {
			printf("WARNING:: Number of bytes in buffer=%d:requested=%d:read=%d\n", numCharsInBuffer, numCharsToRead, dwRead);
		 }
		 for(iIndex=0; iIndex<dwRead; iIndex++) {
#ifdef _DEBUG_XXX
	        uiPrintf("%x ", buf[pos]);
#endif
			pos++;
		    chReadBuf++;
		 }
      }
      else {
        nComErr = nComErr | COM_ERROR_READ;
        return nComErr;
      }
	} 
    *len=pos;
    return 0;

}

A_UINT32 read_device(OS_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len) {
    A_UINT32 pos = 0, dwRead;
    A_UINT32 nComErr=0;

#ifdef _DEBUG_XXX
       q_uiPrintf("Read %d bytes from device::\n", *len);
#endif
          // read receive buffer
          if (ReadFile((HANDLE) pOSSock->sockfd, lpBuffer, *len, &dwRead, NULL)) {
#ifdef _DEBUG_XXX
                  q_uiPrintf("read_device::number of bytes read=%d\n", dwRead);
#endif
				  for(pos=0;pos<dwRead;pos++) {
					buf[pos]=lpBuffer[pos];
#ifdef _DEBUG_XXX
					q_uiPrintf("%x ", buf[pos]);
#endif
				  }

          }
          else {
             nComErr =  GetLastError();
             uiPrintf("ERROR:read_device::Error=%x", nComErr);
          }
       return nComErr;

}



DWORD getBytesBuffered(HANDLE handle, DWORD queueType) {

    COMSTAT comStat;
	DWORD dwErrors;

	if (!ClearCommError(handle, &dwErrors, &comStat)) {
		printf("ERROR:: ClearCommError :%x\n", GetLastError());
		return 0;
	}

	switch(queueType) {
	  case IN_QUEUE:
			return comStat.cbInQue;
			break;
	  case OUT_QUEUE:
			return comStat.cbOutQue;
			break;
	}
    return 0;
}


