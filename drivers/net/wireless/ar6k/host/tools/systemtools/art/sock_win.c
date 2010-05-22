/* sock_win.c - contains socket related functions */

/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "wlantype.h"
#ifdef ANWI
#include "mld_anwi.h"
#endif
#ifdef JUNGO
#include "mld.h"
#endif 

#include "mInst.h"
#define COM1_PORT_NUM       0
#define COM2_PORT_NUM       1
#define SOCK_PORT_NUM       33120
#define SEND_BUF_SIZE		 1024

A_UINT32 os_com_read(ART_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len);
A_UINT32 os_com_write(ART_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len);
A_UINT32 os_com_close(ART_SOCK_INFO *pOSSock);
A_UINT32 os_com_open(ART_SOCK_INFO *pOSSock);


static int	socketConnect(char *target_hostname, int target_port_num,
			       A_UINT32 *ip_addr);


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
    ART_SOCK_INFO    *pSockInfo,
    A_UINT8         *buf,
    A_INT32         len
)
{
    int dwRead, i = 0;
	A_INT32 cnt;
	A_UINT8* bufpos; 
	A_INT32 tmp_len;
	A_UINT32 err;

	
	q_uiPrintf("osSockRead\n");
	dwRead = 0;
	tmp_len = len;
	bufpos = buf;
    if (pSockInfo->port_num == COM1_PORT_NUM || pSockInfo->port_num == COM2_PORT_NUM) {

        if ((err=os_com_read(pSockInfo, buf, &tmp_len)) != 0) {
            uiPrintf("ERROR::osSockRead::Com port read failed with error = %x\n", err);
            return 0;
        }
        dwRead = tmp_len; // return number of bytes read
    } // end of if
    else {

	
	while (len) {
		cnt = recv(pSockInfo->sockfd, (char *)bufpos, len, 0);

		if ((cnt == SOCKET_ERROR) || (!cnt)) break;

		len = len - cnt;
		dwRead = dwRead + cnt;
		bufpos += cnt;
	}

	len = tmp_len;
	if((dwRead != len) || (dwRead == INVALID_SOCKET)) {
	    uiPrintf("osSockRead: recv Failed with error: %d\n", WSAGetLastError());
        dwRead = 0;
    }
	}// end of else
#ifdef _DEBUG
        for (i=0;(i<len);i++) {
          q_uiPrintf(" %02X",*((unsigned char *)buf+i));
          if (3==(i%4)) q_uiPrintf(" ");
		  if (15==(i%16)) q_uiPrintf("\n");
        }
#endif // _DEBUG


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
    ART_SOCK_INFO    *pSockInfo,
    A_UINT8         *buf,
    A_INT32         len
    )
{
		int	dwWritten, i = 0;
		A_INT32 bytes,cnt;
		A_UINT8* bufpos; 
		A_INT32 tmp_len;
		A_UINT32 err;

#ifdef _DEBUG 
    q_uiPrintf("osPipeWrite: buf=0x%08lx  len=%d\n", (unsigned long) buf, len);
    for (i=0;(i<len);i++) {
      q_uiPrintf(" %02X",*((unsigned char *)buf +i));
      if (3==(i%4)) q_uiPrintf(" ");
	  if (15==(i%16)) q_uiPrintf("\n");
    }
#endif // _DEBUG

    if (pSockInfo->port_num == COM1_PORT_NUM || pSockInfo->port_num == COM2_PORT_NUM) {
       if ((err=os_com_write(pSockInfo, buf, &len)) != 0) {
            uiPrintf("ERROR::osSockWrite::Com port write failed with error = %x\n", err);
            return 0;
       }
       dwWritten = len;
    } // end of if 
    else {

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
    
    if ((dwWritten != len) || (dwWritten == INVALID_SOCKET)) {
	    uiPrintf("fdWrite: send Failed with error: %d\n", WSAGetLastError());
        dwWritten = 0;
    }

   } // end of else

    return dwWritten;
}



ART_SOCK_INFO*
osSockConnect(char *mach_name)
{
    ART_SOCK_INFO *pOSSock;
    int			res;
	A_UINT32 err;

    q_uiPrintf("osSockConnect: starting mach_name = '%s'\n", mach_name);

    if (!strcmp(mach_name, ".")) {
	    /* A windows convention meaning "local machine" */
	    mach_name = "localhost";
    }

    q_uiPrintf("osSockConnect: revised mach_name = '%s'\n", mach_name);

    pOSSock = (ART_SOCK_INFO *) malloc(sizeof(ART_SOCK_INFO));
    if(!pOSSock) {
		uiPrintf("osSockConnect: malloc failed for pOSSock\n");
        return NULL;
	}

    //strncpy(pOSSock->hostname, mach_name, strlen(mach_name));
	strcpy(pOSSock->hostname, mach_name);
    //pOSSock->hostname[sizeof(pOSSock->hostname) - 1] = '\0';

    pOSSock->port_num = SOCK_PORT_NUM;

	if (!stricmp(pOSSock->hostname, "COM1")) {
       pOSSock->port_num = COM1_PORT_NUM;
	}
	if (!stricmp(pOSSock->hostname, "COM2")) {
       pOSSock->port_num = COM2_PORT_NUM;
	}

	if (pOSSock->port_num != SOCK_PORT_NUM) {
      if ((err=os_com_open(pOSSock)) != 0) {
            uiPrintf("ERROR::osSockConnect::Com port open failed with error = %x\n", err);
            exit(0);
      }
    } // end of if
	else {
    	res = socketConnect(pOSSock->hostname, pOSSock->port_num,
				   &pOSSock->ip_addr);
    	if (res < 0) {
   	   		 uiPrintf("osSockConnect: pipe connect failed\n");
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

/**************************************************************************
* osSockClose - close socket
*
* Close the handle to the pipe
*
* RETURNS: 0 if error, non 0 if no error
*/
A_BOOL
osSockClose(ART_SOCK_INFO* pOSSock)
{
    A_UINT32 err;

    if (pOSSock->port_num == COM1_PORT_NUM || pOSSock->port_num == COM2_PORT_NUM) {
       if ((err=os_com_close(pOSSock)) != 0) {
            uiPrintf("ERROR::osSockWrite::Com port close failed with error = %x\n", err);
            return 0;
       }
    } // end of if 
    else {
       closesocket(pOSSock->sockfd);
    }
    return 1;

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
    	uiPrintf("socketConnect: getprotobyname failed: %d\n", WSAGetLastError());
   		WSACleanup( );
	    return -1;
    }

    q_uiPrintf("socket start\n");
    sfd = WSASocket(PF_INET, SOCK_STREAM, proto->p_proto, NULL, 0,0);
    q_uiPrintf("socket end\n");
    if (sfd == INVALID_SOCKET) {
	    uiPrintf("socketConnect: socket failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }

    /* Allow immediate reuse of port */
    q_uiPrintf("setsockopt SO_REUSEADDR start\n");
    i = 1;
    res = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("socketConnect: setsockopt SO_REUSEADDR failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt SO_REUSEADDR end\n");

    /* Set TCP Nodelay */
    q_uiPrintf("setsockopt TCP_NODELAY start\n");
    i = 1;
    res = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (char *) &i, sizeof(i));
    if (res != 0) {
	    uiPrintf("socketCreateAccept: setsockopt TCP_NODELAY failed: %d\n", WSAGetLastError());
        WSACleanup( );
	    return -1;
    }	
    q_uiPrintf("setsockopt TCP_NODELAY end\n");

    q_uiPrintf("gethostbyname start\n");
    q_uiPrintf("socket_connect: target_hostname = '%s'\n", target_hostname);
    hostent = gethostbyname(target_hostname);
    q_uiPrintf("gethostbyname end\n");
    if (!hostent) {
	    uiPrintf("socketConnect: gethostbyname failed: %d\n", WSAGetLastError());
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
	    uiPrintf("connect failed completely\n");
        WSACleanup( );
	    return -1;
    }

    return sfd;
}


A_UINT32 os_com_open(ART_SOCK_INFO *pOSSock) {
    DCB          dcb;
    COMMTIMEOUTS timeouts;
    A_UINT32 nComErr=0;
    A_UINT32 err=0;

	// Close the port 
    (void)os_com_close(pOSSock);

    // device handle
    pOSSock->sockfd = (A_UINT32) CreateFile( pOSSock->hostname,                             // port name
                       GENERIC_READ | GENERIC_WRITE,     // allow r/w access
                       0,                                // always no sharing
                       0,                                // no security atributes for file
                       OPEN_EXISTING,                    // always open existing
                       FILE_ATTRIBUTE_NORMAL,            // nonoverlapped operation   
                       0);                               // always no file template

    if ((HANDLE)pOSSock->sockfd == INVALID_HANDLE_VALUE) {
       nComErr = nComErr | COM_ERROR_GETHANDLE;
       return 0;
    }

    // port configuration
    FillMemory (&dcb, sizeof(dcb),0);
    dcb.DCBlength = sizeof(dcb);
    if (!BuildCommDCB("9600,n,8,1", &dcb)) {
       nComErr = nComErr | COM_ERROR_BUILDDCB;
       return nComErr;
    }
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
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
	

    // set mask to notify thread if a character was received
    if (!SetCommMask((HANDLE) pOSSock->sockfd, EV_RXCHAR)) {
       // error setting communications event mask
       nComErr = nComErr | COM_ERROR_READ;
       return nComErr;
    }
    return 0;
}

A_UINT32 os_com_close(ART_SOCK_INFO *pOSSock) { 

    // reset error byte
    A_UINT32 nComErr = 0;

    // port configuration
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

A_UINT32 os_com_write(ART_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len) {
    A_UINT32 nComErr; A_INT32 iIndex, written_len, tmp_len;
	A_UINT32 status, jIndex=0, numblocks, remaining_bytes;

    // reset error byte
    nComErr = 0;
	written_len = *len;
	// split the write

	numblocks = (*len/WRITE_BUF_SIZE); 
	remaining_bytes = *len - (numblocks * WRITE_BUF_SIZE);
	q_uiPrintf("numblocks = %d:remainingbytes = %d\n", numblocks, remaining_bytes);
	for(jIndex=0; jIndex<numblocks; jIndex++) {
       status = WriteFile((HANDLE) pOSSock->sockfd, (const char *)&buf[jIndex * WRITE_BUF_SIZE], WRITE_BUF_SIZE, (A_UINT32 *)&tmp_len, NULL);
       if (!status || tmp_len != WRITE_BUF_SIZE) {
           nComErr = nComErr | COM_ERROR_WRITE;
           return nComErr;
       }
	   milliSleep(5);
	}
	if (remaining_bytes) {	
    // write the com port
       status = WriteFile((HANDLE) pOSSock->sockfd, (const char *)&buf[jIndex * WRITE_BUF_SIZE], remaining_bytes, (A_UINT32 *)&tmp_len, NULL);
       if (!status || tmp_len != remaining_bytes) {
           nComErr = nComErr | COM_ERROR_WRITE;
           return nComErr;
       }
	}

    return 0;

}

A_UINT32 os_com_read(ART_SOCK_INFO *pOSSock, A_UINT8 *buf, A_INT32 *len) {

    A_UINT32 dwCommEvent;
    A_UINT32 dwRead, pos = 0;
    char          chRead;
    A_UINT32 nComErr, iIndex=0;

    // reset error byte
    nComErr = 0;

    // wait for comm event
    if (WaitCommEvent((HANDLE) pOSSock->sockfd, &dwCommEvent, NULL)) {
       while(pos < (A_UINT32)*len) {
          // read receive buffer
          if (ReadFile((HANDLE) pOSSock->sockfd, &chRead, 1, &dwRead, NULL)) {
             buf[pos] = chRead;
             pos++;
          }
          else {
             nComErr = nComErr | COM_ERROR_READ;
             return nComErr;
          }
       } 
    }
    else {
       nComErr = nComErr | COM_ERROR_READ;
       return nComErr;
    }
    *len=pos;
    return 0;
}


