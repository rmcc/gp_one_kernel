/* osWrap_vxworks.c - functions to hide os dependent calls */
/* Copyright (c) 2000 Atheros Communications, Inc., All Rights Reserved */

/*
modification history
--------------------
12/20/2001 	Inital implementation	 	sharmat
*/
#include <stdio.h>
#include <stdlib.h>

#include <vxworks.h>
#include <iolib.h>
#include <socklib.h>
#include <inetlib.h>
#include <time.h>
#include <netinet/tcp.h>
#include <taskLib.h>
#include <semlib.h>
#include "wlantype.h"
#include <fcntl.h>
#include "dk_common.h"

#define COM1 "/tyCo/0"
#define COM2 "/tyCo/1"
extern int     consoleFd;

// forward declarations

static A_UINT32 os_com_open(OS_SOCK_INFO *pOSSock);
static A_UINT32 os_com_close(OS_SOCK_INFO *pOSSock);
static A_INT32 socket_listen(OS_SOCK_INFO *pOSSock);
static A_INT32 fd_read(A_INT32 fd, A_UINT16 port_num, A_UINT8 *buf, A_INT32 bytes);
static A_INT32 fd_write(A_INT32 fd, A_UINT16 port_num, A_UINT8 *buf, A_INT32 bytes);

A_INT32 osSockRead
	(
	OS_SOCK_INFO    *pSockInfo,
	A_UINT8         *buf,
	A_INT32         len
	)
{
	A_INT32		res, iIndex;

	res = fd_read(pSockInfo->sockfd, pSockInfo->port_num,(A_UINT8 *) buf, len);
	q_uiPrintf("Readbytes = ");
	for(iIndex=0; iIndex<res; iIndex++) {
	   q_uiPrintf("%x ", buf[iIndex]);
	}
	q_uiPrintf("\n");

	if (res != len) {
		return 0;
	}
	else {
		return len;
	}
}


A_INT32 osSockWrite
	(
	OS_SOCK_INFO           *pSockInfo,
	A_UINT8                *buf,
	A_INT32                len
	)
{
	A_INT32		res, iIndex;

	res = fd_write(pSockInfo->sockfd, pSockInfo->port_num, (A_UINT8 *) buf, len);
	if (res != -1) {
	  q_uiPrintf("Sent bytes = ");
	  for(iIndex=0; iIndex<res; iIndex++) {
	     q_uiPrintf("%x ", buf[iIndex]);
	  }
	  q_uiPrintf("\n");
	}


	if (res != len) {
		return 0;
	}
	else {
		return len;
	}
}

/**************************************************************************
* osSockClose - close socket
*
* Close the handle to the socket
*
*/
void osSockClose
	(
	OS_SOCK_INFO *pOSSock
	)
{
	A_UINT32		res;

	res = close(pOSSock->sockfd);
	A_FREE(pOSSock);

	return;
}


/* Only Master will call connect calls */

OS_SOCK_INFO *osSockConnect
	(
	char *pname
	)
{
	return NULL;
}



/**************************************************************************
* osSockAccept - Wait for a connection
*
*/
OS_SOCK_INFO *osSockAccept
	(
	OS_SOCK_INFO *pOSSock
	)
{
	OS_SOCK_INFO *pOSNewSock;
	A_INT32		i;
	A_INT32		sfd;
	struct sockaddr_in	sin;

	pOSNewSock = (OS_SOCK_INFO *) A_MALLOC(sizeof(*pOSNewSock));
	if (!pOSNewSock) {
		uiPrintf("osSockAccept: malloc failed for pOSNewSock \n");
		return NULL;
	}
   
	
   	i = sizeof(sin);
    	sfd = accept(pOSSock->sockfd, (struct sockaddr *) &sin, &i);
    	if (sfd == -1) {
		A_FREE(pOSNewSock);
		uiPrintf( "accept failed: %s\n", strerror(errno));
		return NULL;
	}

  	strcpy(pOSNewSock->hostname, inet_ntoa(sin.sin_addr));
	pOSNewSock->port_num = sin.sin_port;
	pOSNewSock->sockDisconnect = 0;
	pOSNewSock->sockClose = 0;
	pOSNewSock->sockfd = sfd;

	return pOSNewSock;
}

OS_SOCK_INFO *osSockListen
(
	A_UINT32 acceptFlag, A_UINT16 port
)
{
	OS_SOCK_INFO *pOSSock;
	OS_SOCK_INFO *pOSNewSock;
	A_UINT32 err;
	char buf[90];
	

	pOSSock = (OS_SOCK_INFO *) A_MALLOC(sizeof(*pOSSock));
	if (!pOSSock) {
		uiPrintf("osSockCreate: malloc failed for pOSSock \n");
		return NULL;
	}
   
	strcpy(pOSSock->hostname, "localhost");
	pOSSock->port_num = SOCK_PORT_NUM;
	pOSSock->port_num = port;
	pOSSock->sockDisconnect = 0;
	pOSSock->sockClose = 0;

    switch(pOSSock->port_num) {
    case COM1_PORT_NUM:
        strcpy(pOSSock->hostname, COM1);
        break;
    case COM2_PORT_NUM:
        strcpy(pOSSock->hostname, COM2);
        break;
    case SOCK_PORT_NUM:
        strcpy(pOSSock->hostname, "localhost");
        break;
    }

    pOSSock->sockDisconnect = 0;
    pOSSock->sockClose = 0;


    if (pOSSock->port_num == SOCK_PORT_NUM) {

	   pOSSock->sockfd = socket_listen(pOSSock);
	   if (pOSSock->sockfd == -1) {
		   uiPrintf("Socket create failed \n");
		   A_FREE(pOSSock);
		   return NULL;
	   }

	   if (acceptFlag) {
		   pOSNewSock = osSockAccept(pOSSock);
		   if (!pOSNewSock) {
			   uiPrintf("osSockCreate: malloc failed for pOSSock \n");
			   osSockClose(pOSSock);
			   return NULL;
		   }
		   osSockClose(pOSSock);
		   pOSSock = pOSNewSock;
	   }
    }
    else { 
        if ((err=os_com_open(pOSSock)) != 0) {
            uiPrintf("ERROR::osSockListen::Com port open failed\n");
        }
    }


	return pOSSock;
}



/* The VxWorks function taskSpawn creates a new task. In VxWorks all the 
   tasks uses the same address space. So a task in VxWorks is equivalent 
   to a thread. */

A_STATUS osThreadCreate
	(
		void            threadFunc(void * param), 
		void 		*param,
		A_CHAR*		threadName,
		A_INT32 	threadPrio,
		A_UINT32 	*threadId
	)
{
	int taskId;
	
	taskId = taskSpawn (threadName, threadPrio, 0, 64*1024, (FUNCPTR) threadFunc, (int) param, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (taskId == ERROR) return A_ERROR;
	q_uiPrintf("Task id of thread %s : %x \n",threadName,taskId);

	if (threadId != NULL) {
		*threadId = (A_UINT32)taskId;
	}

    	return A_OK;
}

void osThreadKill
(
	A_UINT32 threadId
)  
{
	int taskId;
	
	taskId = (int)threadId;
	taskDelete(taskId);	
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
	struct timespec t;

		t.tv_sec = millitime / 1000;
		t.tv_nsec = (millitime % 1000) * 1000000;

		nanosleep(&t,NULL);

		return;
}

/**************************************************************************
* milliTime - return time in milliSeconds 
*
* This routine calls a OS specific routine for gettting the time
* 
* RETURNS: time in milliSeconds
*/

A_UINT32 milliTime
(
	void
)
{
	struct timespec ts;
	A_UINT32 timeMilliSeconds;
		
	if (clock_gettime(CLOCK_REALTIME,&ts) == 0) {
		// convert time into milliseconds
		timeMilliSeconds=ts.tv_sec*1000+ts.tv_nsec/1000000;
	} else {
		timeMilliSeconds=0;
	}
	
	return timeMilliSeconds;

}

static A_INT32 socket_listen
(
	OS_SOCK_INFO *pOSSock
)
{
	A_INT32		sockfd;
	A_INT32		res;
	struct sockaddr_in	sin;
	A_INT32		i;
	A_INT32		j;

	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1) {
		uiPrintf( "socket failed: %s\n", strerror(errno));
		return -1;
   	}

	// Allow immediate reuse of port 
	i = 1;
	j = sizeof(i);
	res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (A_INT8 *)&i, j);
	if (res == -1) {
		uiPrintf( "setsockopt failed: %s\n", strerror(errno));
		return -1;
    	}


	i = 1;
    	j = sizeof(i);
    	res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (A_INT8 *)&i, j);
   	if (res == -1) {
		uiPrintf( "setsockopt failed: %s\n", strerror(errno));
		return -1;
    	}	

    	sin.sin_family = AF_INET;
    	sin.sin_addr.s_addr =  INADDR_ANY;
    	sin.sin_port = htons(pOSSock->port_num);

    	res = bind(sockfd, (struct sockaddr *) &sin, sizeof(sin));
    	if (res == -1) {
		uiPrintf( "bind failed: %s\n", strerror(errno));
		return -1;
    	}

    	res = listen(sockfd, 4);
    	if (res == -1) {
		uiPrintf( "listen failed: %s\n", strerror(errno));
		return -1;
    	}

    	return sockfd;
}

/* Returns number of bytes read, -1 if error */
static A_INT32 fd_read
	(
	A_INT32 fd,
    A_UINT16 port_num,
 	A_UINT8 *buf,
 	A_INT32 bytes
)
{
    	A_INT32		cnt;
    	A_UINT8*		bufpos;

    	bufpos = buf;
	q_uiPrintf("Reading %d bytes\n", bytes);
    	while (bytes) {
		    cnt = read(fd, (A_INT8 *)bufpos, bytes);	
		  if (!cnt) break;
	
		  if (cnt == -1) {
	    		if (errno == EINTR) {
				continue;
	     		}
	     		else {
				return -1;
	     		}
   		}
		bytes -= cnt;
		bufpos += cnt;
    	}

    	return (bufpos - buf);
}

/* Returns number of bytes written, -1 if error */
static A_INT32 fd_write
	(
	A_INT32 fd,
    A_UINT16 port_num,
	A_UINT8 *buf,
	A_INT32 bytes
	)
{
    	A_INT32	cnt, tmp_nBytes=-1;
    	A_UINT8*	bufpos;

    	bufpos = buf;

    	while (bytes) {
		    cnt = write(fd, bufpos, bytes);
            (void) ioctl(fd, FIONWRITE, &tmp_nBytes);
            q_uiPrintf("bytes queued=%d\n", tmp_nBytes);

		if (!cnt) {
			break;
		}
		if (cnt == -1) {
	    		if (errno == EINTR) {
				continue;
	    		}
	    		else {
				return -1;
	    		}
		}

		bytes -= cnt;
		bufpos += cnt;
    	}

    	return (bufpos - buf);
}


A_UINT32 semInit
(
	void
) 
{
	SEM_ID semId;

	semId = semBCreate(SEM_Q_FIFO, SEM_FULL); 
	if (semId == NULL) {
		return 0;
	}
	return (A_UINT32)semId;
}

A_INT32 semLock
(
	A_UINT32 sem
)
{
	SEM_ID semId;

	semId = (SEM_ID)sem;
	if (semTake(semId, -1) == ERROR) {
		return -1;
	}
	
	return 0;
}

A_INT32 semUnLock
(
	A_UINT32 sem
)
{
	SEM_ID semId;

	semId = (SEM_ID)sem;
	if (semGive(semId) == ERROR) {
		return -1;
	}

	return 0;
}

A_INT32 semClose
(
	A_UINT32 sem
)
{
	SEM_ID semId;


	semId = (SEM_ID)sem;
        if (semDelete(semId) == ERROR) {
		return -1;
	} 

	return 0;
}

A_UINT32 os_com_open(OS_SOCK_INFO *pOSSock)
{
    A_INT32 fd, stdfd, null_consoleFd;
    A_UINT32 nComErr=0;
	char buf[90];
	int bytes=4;
    
    fd = -1;
uiPrintf("hostname=%s:consolefd=%d\n", pOSSock->hostname, consoleFd);
    fd = open("/dev/null", O_RDWR, 0);
    if (fd < 0) {
        uiPrintf("os_com_open::open to /dev/null failed\n");
    }
    close(consoleFd);
    ioGlobalStdSet (STD_IN,  null_consoleFd);
    ioGlobalStdSet (STD_OUT, null_consoleFd);
    ioGlobalStdSet (STD_ERR, null_consoleFd);

    uiPrintf("Opening host name\n");
    fd = open(pOSSock->hostname, O_RDWR, 0777);
   
    if (fd < 0) {
        nComErr = nComErr | 1;
        return nComErr;
    }

    pOSSock->sockfd = fd;
    (void) ioctl (pOSSock->sockfd, FIOBAUDRATE, 19200);
    (void) ioctl (pOSSock->sockfd, FIOSETOPTIONS, OPT_RAW);
    (void) ioctl (pOSSock->sockfd, FIOFLUSH, 0);

    return 0;
}

A_UINT32 os_com_close(OS_SOCK_INFO *pOSSock)
{
    A_UINT32 nComErr;
    // reset error byte
    nComErr = 0;
    if (pOSSock->sockfd < 0) {
            nComErr = nComErr | 800;
            return nComErr;
    }

    uiPrintf("close returned = %d\n", close(pOSSock->sockfd));
    pOSSock->sockfd = 0;

    return 0;
}

