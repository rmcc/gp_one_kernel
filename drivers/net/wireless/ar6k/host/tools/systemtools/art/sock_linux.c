/* sock_linux.c - functions to hide os dependent calls */
/* Copyright (c) 2001 Atheros Communications, Inc., All Rights Reserved */

/*
modification history
--------------------
12/20/2001 	Inital implementation	 	sharmat
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "wlantype.h"
#include "athreg.h"
#include "manlib.h"
#include "termios.h"
#include <fcntl.h>
#include "mInst.h"

#include "mld_linux.h"

struct termios oldtio;

#define COM1 "/dev/ttyS0"
#define COM2 "/dev/ttyS1"

#define COM1_PORT_NUM       0
#define COM2_PORT_NUM       1
#define SOCK_PORT_NUM		33120
#define SEND_BUF_SIZE        1024

// forward declarations
static A_UINT32 os_com_open(ART_SOCK_INFO *pOSSock);
static A_UINT32 os_com_close(ART_SOCK_INFO *pOSSock);
static A_INT32 fd_write(A_INT32 fd, A_UINT8 *buf, A_INT32 bytes);
static A_INT32 fd_read(A_INT32 fd, A_UINT8 *buf, A_INT32 bytes);
static A_INT32 socketConnect
(
 	char *target_hostname, 
	int target_port_num, 
	A_UINT32 *ip_addr
);

A_INT32 osSockRead
	(
	ART_SOCK_INFO    *pSockInfo,
	A_UINT8         *buf,
	A_INT32         len
	)
{
	A_INT32		res;

	res = fd_read(pSockInfo->sockfd, (A_UINT8 *) buf, len);

	if (res != len) {
		return 0;
	}
	else {
		return len;
	}
}


A_INT32 osSockWrite
(
	ART_SOCK_INFO           *pSockInfo,
	A_UINT8                *buf,
	A_INT32                len
	)
{
	A_UINT32		res;

	res = fd_write(pSockInfo->sockfd, (A_UINT8 *) buf, len);

	if (res != len) {
		return 0;
	}
	else {
		return len;
	}
}



ART_SOCK_INFO *osSockConnect
(
	char *mach_name
)
{
	ART_SOCK_INFO *pOSSock;
	int res;
	A_UINT32 err;

	q_uiPrintf("osSockConnect: starting mach_name = '%s'\n", mach_name);

	if (!strcmp(mach_name, ".")) {
			mach_name = "localhost";
     }

	q_uiPrintf("osSockConnect: revised mach_name = '%s'\n", mach_name);

	pOSSock = (ART_SOCK_INFO *) malloc(sizeof(ART_SOCK_INFO));
	if (!pOSSock) {
		uiPrintf("osSockCreate: malloc failed for pOSSock \n");
		return NULL;
	}
   
	strcpy(pOSSock->hostname, mach_name);
	pOSSock->port_num = SOCK_PORT_NUM;

    if (!strcasecmp(mach_name, "COM1")) {
	    q_uiPrintf("osSockConnect: Using serial communication port 1\n");
	    strcpy(pOSSock->hostname, COM1);
		pOSSock->port_num = COM1_PORT_NUM;
    }
    if (!strcasecmp(mach_name, "COM2")) {
	    q_uiPrintf("osSockConnect: Using serial communication port 2\n");
	    strcpy(pOSSock->hostname, COM2);
		pOSSock->port_num = COM2_PORT_NUM;
    }

	if (pOSSock->port_num != SOCK_PORT_NUM) {
        if ((err=os_com_open(pOSSock)) != 0) {
            uiPrintf("ERROR::osSockConnect::Com port open failed with error = %x\n", err);
            exit(0);
        }
	}
	else {
		res = socketConnect(pOSSock->hostname, pOSSock->port_num, &pOSSock->ip_addr);
		if (res < 0) {
	          uiPrintf("osSockConnect: pipe connect failed\n"); A_FREE(pOSSock);
	          return NULL;
		}
		q_uiPrintf("osSockConnect: Connected to pipe\n");

		q_uiPrintf("ip_addr = %d.%d.%d.%d\n",(pOSSock->ip_addr >> 24) & 0xff,
											(pOSSock->ip_addr >> 16) & 0xff,
											(pOSSock->ip_addr >> 8) & 0xff,
											(pOSSock->ip_addr >> 0) & 0xff);
		pOSSock->sockfd = res;
	}

	return pOSSock;
}

/**************************************************************************
* osSockClose - close socket
*
* Close the handle to the socket
*
* RETURNS: 0 if error, non 0 if no error
*/
A_BOOL osSockClose
(
	ART_SOCK_INFO *pOSSock
)
{
	close(pOSSock->sockfd);
	return 1;
}

static A_INT32 socketConnect
(
 	char *target_hostname, 
	int target_port_num, 
	A_UINT32 *ip_addr
)
{
	A_INT32		sockfd;
	A_INT32		res;
	struct sockaddr_in	sin;
	struct hostent *hostent;
	A_INT32		i;
	A_INT32		j;

	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1) {
		q_uiPrintf("socket failed: %s\n", strerror(errno));
		return -1;
   	}

	/* Allow immediate reuse of port */
    q_uiPrintf("setsockopt SO_REUSEADDR start\n");
    i = 1;
    res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &i, sizeof(i));
    if (res != 0) {
        uiPrintf("socketConnect: setsockopt SO_REUSEADDR failed: %d\n",strerror(errno));
		close(sockfd);
	     return -1;
 	}
    q_uiPrintf("setsockopt SO_REUSEADDR end\n");


	/* Set TCP Nodelay */
    q_uiPrintf("setsockopt TCP_NODELAY start\n");
	i = 1;
    j = sizeof(i);
    res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (A_INT8 *)&i, j);
   	if (res == -1) {
		uiPrintf("setsockopt failed: %s\n", strerror(errno));
		close(sockfd);
		return -1;
   	}	
	q_uiPrintf("setsockopt TCP_NODELAY end\n");


	q_uiPrintf("gethostbyname start\n");
    q_uiPrintf("socket_connect: target_hostname = '%s'\n", target_hostname);
    hostent = gethostbyname(target_hostname);
    q_uiPrintf("gethostbyname end\n");
    if (!hostent) {
        uiPrintf("socketConnect: gethostbyname failed: %d\n",strerror(errno));
		close(sockfd);
	    return -1;
    }
					
   	sin.sin_family = AF_INET;
	memcpy(&sin.sin_addr.s_addr, hostent->h_addr_list[0], hostent->h_length);
	sin.sin_port = htons((short)target_port_num);


	for (i = 0; i < 20; i++) {
		q_uiPrintf("connect start %d\n", i);
	   	res = connect(sockfd, (struct sockaddr *) &sin, sizeof(sin));
		q_uiPrintf("connect end %d\n", i);
        if (res == 0) {
            break;
        }
        milliSleep(1);
    }
	
    if (i == 20) {
        uiPrintf("connect failed completely\n");
		close(sockfd);
        return -1;
    }
		
   	return sockfd;
}

/* Returns number of bytes read, -1 if error */
static A_INT32 fd_read
(
	A_INT32 fd,
 	A_UINT8 *buf,
 	A_INT32 bytes
)
{
    	A_INT32		cnt;
    	A_UINT8*		bufpos;

    	bufpos = buf;

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
	A_UINT8 *buf,
	A_INT32 bytes
	)
{
    	A_INT32	cnt;
    	A_UINT8*	bufpos;

    	bufpos = buf;

    	while (bytes) {
		cnt = write(fd, bufpos, bytes);

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

static A_INT32 socket_create_and_accept
(
	A_INT32 port_num
)
{
	A_INT32     sockfd;
	A_INT32     res;
	struct sockaddr_in  sin;
	A_INT32     i;
	A_INT32     j;
	A_INT32     sfd;

	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1) {
          fprintf(stderr, "socket failed: %s\n", strerror(errno));
          return -1;
	}

	// Allow immediate reuse of port
	i = 1;
	j = sizeof(i);
	res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (A_INT8 *)&i, j);
	if (res == -1) {
		fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
		return -1;
	}

	i = 1;
	j = sizeof(i);
	res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (A_INT8 *)&i, j);
	if (res == -1) {
		fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr =  INADDR_ANY;
	sin.sin_port = htons(port_num);
	res = bind(sockfd, (struct sockaddr *) &sin, sizeof(sin)); 
	if (res == -1) { 
		fprintf(stderr, "bind failed: %s\n", strerror(errno)); 
		return -1; 
	}
	res = listen(sockfd, 4);
	if (res == -1) { 
		fprintf(stderr, "listen failed: %s\n", strerror(errno)); 
		return -1; 
	} 
						
	i = sizeof(sin);
	sfd = accept(sockfd, (struct sockaddr *) &sin, (socklen_t *)&i);
	if (sfd == -1) { 
		fprintf(stderr, "accept failed: %s\n", strerror(errno)); 
		return -1; 
	} 
	
	res = close(sockfd); 
	if (res == -1) { 
		fprintf(stderr, "sockfd close failed: %s\n", strerror(errno)); 
		return 1; 
	}
								
	return sfd;
}

ART_SOCK_INFO *osSockCreate
(
	char *pname
)
{
	ART_SOCK_INFO *pOSSock;

    pOSSock = (ART_SOCK_INFO *) A_MALLOC(sizeof(*pOSSock));
    if (!pOSSock) {
	        uiPrintf("osSockCreate: malloc failed for pOSSock \n");
	        return NULL;
    }

    strcpy(pOSSock->hostname, "localhost");
    pOSSock->port_num = SOCK_PORT_NUM;

    pOSSock->sockfd = socket_create_and_accept(pOSSock->port_num);

    if (pOSSock->sockfd == -1) {
	        uiPrintf("Socket create failed \n");
	        return NULL;
    }

    return pOSSock;
}

extern "C" {

A_UINT map_file(A_STATUS *status, A_UCHAR **memPtr, A_UCHAR *filename) {

  FILE *fp;
  int fd;
  A_UINT length;
  fp = (FILE *) fopen((const char*)filename, "r");
  if (fp == NULL) {
    return 0;
  }
  uiPrintf("map_file:uiPrintf\n");
  fseek(fp, 0, SEEK_END);
  uiPrintf("map_file:uiPrintf\n");
  length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  fclose(fp);
 

  fd = open( (const char*) filename, O_RDONLY);
  *memPtr = (A_UCHAR *)mmap(0, length, PROT_READ, MAP_SHARED, fd, 0);
  if ( -1 == (A_CHAR)**memPtr) {
     uiPrintf("Error:map_file:mmap returned with error %d\n", **memPtr);
     return 0;
  }
  close(fd);
  return length;

}
}

A_UINT32 os_com_open(ART_SOCK_INFO *pOSSock)
{
    A_INT32 fd; 
    A_UINT32 nComErr=0;
    struct termios my_termios;

    fd = -1;
    fd = open(pOSSock->hostname, O_RDWR | O_NOCTTY  | O_SYNC);

    if (fd < 0) { 
        nComErr = nComErr | COM_ERROR_GETHANDLE;
        return nComErr;
    }
	

	tcgetattr(fd, &oldtio);
	// NOTE: you may want to save the port attributes
	// here so that you can restore them later
	q_uiPrintf("%s Terminal attributes\n", pOSSock->hostname);
    q_uiPrintf("old cflag=%08x\n", my_termios.c_cflag);
	q_uiPrintf("old oflag=%08x\n", my_termios.c_oflag);
	q_uiPrintf("old iflag=%08x\n", my_termios.c_iflag);
	q_uiPrintf("old lflag=%08x\n", my_termios.c_lflag);
	q_uiPrintf("old line=%02x\n", my_termios.c_line);
	tcflush(fd, TCIFLUSH);
	memset(&my_termios,0,sizeof(my_termios));
	my_termios.c_cflag = B9600 | CS8 |CREAD | CLOCAL ;
	my_termios.c_iflag = IGNPAR;
	my_termios.c_oflag = 0;
	my_termios.c_lflag = 0; //FLUSHO; //|ICANON;
	my_termios.c_cc[VMIN] = 1;
	my_termios.c_cc[VTIME] = 0;
//	cfsetospeed(&my_termios, B9600);
	tcsetattr(fd, TCSANOW, &my_termios);
	q_uiPrintf("new cflag=%08x\n", my_termios.c_cflag);
	q_uiPrintf("new oflag=%08x\n", my_termios.c_oflag);
	q_uiPrintf("new iflag=%08x\n", my_termios.c_iflag);
	q_uiPrintf("new lflag=%08x\n", my_termios.c_lflag);
	q_uiPrintf("new line=%02x\n", my_termios.c_line);
	

    pOSSock->sockfd = fd;
    return 0;
}

A_UINT32 os_com_close(ART_SOCK_INFO *pOSSock)
{   
    A_UINT32 nComErr;
    // reset error byte
    nComErr = 0;
    if (pOSSock->sockfd < 0) {
            nComErr = nComErr | COM_ERROR_INVALID_HANDLE;
            return nComErr;
    }

    tcsetattr(pOSSock->sockfd,TCSANOW,&oldtio);

    close(pOSSock->sockfd);
    pOSSock->sockfd = 0;

    return 0;
}

