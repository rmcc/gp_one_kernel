/*============================================================================
//
// Copyright(c) 2006 Intel Corporation. All rights reserved.
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Intel Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//  File Name : InbWlan.cpp
//  Description: This file contains implementation of functions for the
//               Inband WLAN manager.
//
//===========================================================================*/

#include <stdio.h>
#ifdef __linux__
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#endif // __linux__
#ifdef WIN32
#include <windows.h>
#endif // WIN32

#include "tutrace.h"
#include "slist.h"
#include "WscCommon.h"
#include "WscError.h"
#include "Portability.h"
#include "UdpLib.h"
#include "InbWlan.h"

#ifdef __linux__
#define SLEEP(X) sleep(X)
#endif // __linux__
#ifdef WIN32
#define SLEEP(X) Sleep(X * 1000)
#endif // WIN32


CInbWlan::CInbWlan()
{
    TUTRACE((TUTRACE_INFO, "CInbWlan Construction\n"));
    m_initialized = false;
    m_recvEvent = 0;
    m_recvThreadHandle = 0;
    m_udpFd = -1;
    m_recvPort = 0;
    IeBufLen = 0;
	ProbRespBufLen = 0;
}

CInbWlan::~CInbWlan()
{
    TUTRACE((TUTRACE_INFO, "CInbWlan Destruction\n"));
}

uint32 CInbWlan::Init()
{
    uint32 retVal;
    int32 udpRet;

    retVal = WscCreateEvent(&m_recvEvent);
    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR, "CreateEvent failed.\n"));
        return retVal;
    }

    m_udpFd = udp_open();
    if (m_udpFd == -1)
    {
        TUTRACE((TUTRACE_ERR, "UDP Open failed.\n"));
        return WSC_ERR_SYSTEM;
    }

    udpRet = udp_bind(m_udpFd, WSC_WLAN_UDP_PORT);
    if (udpRet == -1)
    {
        TUTRACE((TUTRACE_ERR, "UDP Bind failed.\n"));
        return WSC_ERR_SYSTEM;
    }

    m_initialized = true;

    return WSC_SUCCESS;
}

uint32 CInbWlan::StartMonitor()
{
    uint32 retVal;

    if ( ! m_initialized)
    {
        return WSC_ERR_NOT_INITIALIZED;
    }

    retVal = WscCreateThread(&m_recvThreadHandle, StaticRecvThread, this);

    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "CreateThread failed.\n"));
        return retVal;
    }

    SLEEP(0);

    WscResetEvent(m_recvEvent);

    return WSC_SUCCESS;
}

uint32 CInbWlan::StopMonitor()
{
    if ( ! m_initialized)
    {
        return WSC_ERR_NOT_INITIALIZED;
    }

    if (m_recvThreadHandle)
    {
        struct sockaddr_in to;
        char data[] = "QUIT";
        to.sin_addr.s_addr = inet_addr("127.0.0.1");
        to.sin_family = AF_INET;
        to.sin_port = htons(WSC_WLAN_UDP_PORT);

        udp_write(m_udpFd, data, 5, &to);

        WscDestroyThread(m_recvThreadHandle);
        m_recvThreadHandle = 0;
    }

    SLEEP(0);

    return WSC_SUCCESS;
}

uint32 CInbWlan::WriteData(char * dataBuffer, uint32 dataLen)
{
    int sentBytes = 0;
    struct sockaddr_in to;

    if ( (! dataBuffer) || (! dataLen))
    {
        TUTRACE((TUTRACE_ERR, "Invalid Parameters\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if (m_recvPort == 0)
    {
        TUTRACE((TUTRACE_ERR, "No data has been received from lower layer\n"));
        return TRWLAN_ERR_SENDRECV;
    }

    to.sin_addr.s_addr = inet_addr(WSC_WLAN_UDP_ADDR);
    to.sin_family = AF_INET;
    to.sin_port = htons(m_recvPort);

    sentBytes = udp_write(m_udpFd, dataBuffer, dataLen, &to);

    if (sentBytes != (int32) dataLen)
    {
        TUTRACE((TUTRACE_ERR, "UDP send failed; sentBytes = %d\n", sentBytes));
        return TRWLAN_ERR_SENDRECV;
    }

    return WSC_SUCCESS;
}

uint32 CInbWlan::ReadData(char * dataBuffer, uint32 * dataLen)
{
    struct sockaddr_in from;
    int recvBytes = 0;
    int len;

    if (dataBuffer && (! dataLen))
    {
        return WSC_ERR_INVALID_PARAMETERS;
    }

    memset(&from, 0, sizeof(struct sockaddr_in));

    len = (int) *dataLen;
    recvBytes = udp_read(m_udpFd, dataBuffer, len, &from);

    if (recvBytes < 0)
    {
        TUTRACE((TUTRACE_ERR, "UDP recv failed; recvBytes = %d\n", recvBytes));
        return TRWLAN_ERR_SENDRECV;
    }

    *dataLen = recvBytes;

    m_recvPort = ntohs(from.sin_port);

    return WSC_SUCCESS;
}

uint32 CInbWlan::Deinit()
{
    TUTRACE((TUTRACE_INFO, "In CInbWlan::Deinit\n"));
    if ( ! m_initialized)
    {
        TUTRACE((TUTRACE_ERR, "Not initialized; Returning\n"));
        return WSC_ERR_NOT_INITIALIZED;
    }

    StopMonitor();

    if (m_udpFd != -1)
    {
        udp_close(m_udpFd);
        m_udpFd = -1;
        m_recvPort = 0;
    }

    if (m_recvEvent)
    {
        WscDestroyEvent(m_recvEvent);
        m_recvEvent = 0;
    }

    m_initialized = false;

    return WSC_SUCCESS;
}

void * CInbWlan::StaticRecvThread(IN void *p_data)
{
    TUTRACE((TUTRACE_INFO, "In CInbWlan::StaticRecvThread\n"));
    ((CInbWlan *)p_data)->ActualRecvThread();
    return 0;
} // StaticRecvThread


void * CInbWlan::ActualRecvThread()
{
    char buf[WSC_WLAN_DATA_MAX_LENGTH];
    bool runThread = true;
    char * buffer;
    uint32 len;
    S_CB_TRWLAN * wlanComm;
	WSC_IE_COMMAND_DATA * cmdData;

    TUTRACE((TUTRACE_INFO, "CInbWlan::ActualRecvThread Started\n"));

    while (runThread)
    {
        TUTRACE((TUTRACE_INFO, "Inside the thread\n"));
        len = WSC_WLAN_DATA_MAX_LENGTH;
        if (ReadData(buf, &len) == WSC_SUCCESS)
        {
            TUTRACE((TUTRACE_INFO, "WLAN Received Data; Length = %d\n", len));

            if (len == 5 && (strcmp(buf, "QUIT") == 0))
            {
                TUTRACE((TUTRACE_INFO, "QUIT received; quitting..\n"));
                runThread = false;
                continue;
            }

			if (len == 5 && (strcmp(buf, "PORT") == 0))
			{
                TUTRACE((TUTRACE_INFO, "PORT received; continueing..\n"));
				if(IeBufLen != 0)
					SendDataDown(IeBuf, IeBufLen);
				if(ProbRespBufLen != 0)
					SendDataDown(ProbRespBuf, ProbRespBufLen);

				continue;
			}

			cmdData = (WSC_IE_COMMAND_DATA *) buf;
			if (cmdData->type == WSC_IE_TYPE_BEACON_IE_DATA)
			{
                TUTRACE((TUTRACE_INFO, "BEACON_IE_DATA received; "
						"sending it up\n"));
			}
			else if (cmdData->type == WSC_IE_TYPE_PROBE_RESPONSE_IE_DATA)
			{
                TUTRACE((TUTRACE_INFO, "PROBE_RESPONSE_IE_DATA received; "
						"sending it up\n"));
			}
			else
			{
                TUTRACE((TUTRACE_INFO, "Type Unknown; continuing..\n"));
                continue; // do not process it if unknown.
			}

            // call callback
            buffer = new char[len - sizeof(WSC_IE_COMMAND_DATA)  +
						sizeof(S_CB_HEADER)];
            wlanComm = (S_CB_TRWLAN *) buffer;

			TUTRACE((TUTRACE_INFO, "Type = %d, Length = %d\n",
					cmdData->type, cmdData->length));

			if (cmdData->type == WSC_IE_TYPE_BEACON_IE_DATA)
			{
                wlanComm->cbHeader.eType = CB_TRWLAN_BEACON;
			}
			else if (cmdData->type == WSC_IE_TYPE_PROBE_REQUEST_IE_DATA)
			{
                wlanComm->cbHeader.eType = CB_TRWLAN_PR_REQ;
			}
			else if (cmdData->type == WSC_IE_TYPE_PROBE_RESPONSE_IE_DATA)
			{
                wlanComm->cbHeader.eType = CB_TRWLAN_PR_RESP;
			}

            wlanComm->cbHeader.dataLength = cmdData->length;

            memcpy(buffer + sizeof(S_CB_HEADER), cmdData->data, cmdData->length);
            if (m_trCallbackInfo.pf_callback)
            {
                TUTRACE((TUTRACE_INFO, "WLAN: Calling Transport Callback\n"));
                m_trCallbackInfo.pf_callback(buffer, m_trCallbackInfo.p_cookie);
                TUTRACE((TUTRACE_INFO, "Transport Callback Returned\n"));
            }
            else
            {
                TUTRACE((TUTRACE_ERR, "No Callback function set\n"));
            }
            // runThread = false;
        }
        else
        {
            TUTRACE((TUTRACE_INFO, "ReadData Failed for some reason; Continue\n"));
            SLEEP(1);
            continue;
        }
    } // while

    TUTRACE((TUTRACE_INFO, "WlanRecvThread Finished\n"));
    return 0;
}

uint32 CInbWlan::SendDataDown(char * dataBuffer, uint32 dataLen)
{
    int sentBytes = 0;
    struct sockaddr_in to;

    TUTRACE((TUTRACE_INFO, "In CInbWlan::SendDataDown buffer Length = %d\n",
            dataLen));
    if ( (! dataBuffer) || (! dataLen))
    {
        TUTRACE((TUTRACE_ERR, "Invalid Parameters\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if (m_recvPort == 0)
    {
        TUTRACE((TUTRACE_ERR, "No data has been received from lower layer\n"));
        return TRWLAN_ERR_SENDRECV;
    }

    to.sin_addr.s_addr = inet_addr(WSC_WLAN_UDP_ADDR);
    to.sin_family = AF_INET;
    to.sin_port = htons(m_recvPort);

/*
	for (int i = 0; i < dataLen; i ++)
	{
		printf("%02x ", (unsigned char) dataBuffer[i]);
	}
	printf("\n");
*/

    sentBytes = udp_write(m_udpFd, dataBuffer, dataLen, &to);

    if (sentBytes != (int32) dataLen)
    {
        TUTRACE((TUTRACE_ERR, "UDP send failed; sentBytes = %d\n", sentBytes));
        return TRWLAN_ERR_SENDRECV;
    }

    return WSC_SUCCESS;
}
uint32 CInbWlan::SetBeaconIE( IN uint8 *p_data, IN uint32 length )
{
    char sendBuf[WSC_WLAN_DATA_MAX_LENGTH];
	WSC_IE_COMMAND_DATA * cmdData;
	WSC_IE_HEADER ieHdr;
	u8 * tmpPtr;
    //test
    unsigned int i;
    unsigned char * pdata;

    TUTRACE((TUTRACE_INFO, "In CInbWlan::SetBeaconIE  Length = %d\n",
            length));

	ieHdr.elemId = 221;
	ieHdr.length = length + 4;
	ieHdr.oui[0] = 0x00; ieHdr.oui[1] = 0x50;
    ieHdr.oui[2] = 0xF2; ieHdr.oui[3] = 0x04;


	cmdData = (WSC_IE_COMMAND_DATA *) sendBuf;

	cmdData->type = WSC_IE_TYPE_SET_BEACON_IE;
	cmdData->length = sizeof(WSC_IE_HEADER) + length;
	tmpPtr = &(cmdData->data[0]);
#ifdef PROVISION_IE
	tmpPtr[0] = 0xDD;
	tmpPtr[1] = 0x05;
	tmpPtr[2] = 0x00;
	tmpPtr[3] = 0x50;
	tmpPtr[4] = 0xF2;
	tmpPtr[5] = 0x05;
	tmpPtr[6] = 0x0;
	tmpPtr = tmpPtr + 7;
	cmdData->length += 7;
#endif
	memcpy(tmpPtr, &ieHdr, sizeof(WSC_IE_HEADER));
	memcpy(tmpPtr + sizeof(WSC_IE_HEADER), p_data, length);

#ifdef PROVISION_IE
	length += 7;
#endif

    printf("prob resp ie:\n");
    pdata = tmpPtr;
#ifdef PROVISION_IE
    pdata = &cmdData->data[0];
#endif
    for(i=0; i< (sizeof(WSC_IE_COMMAND_DATA) + sizeof(WSC_IE_HEADER) + length);i++)
    {
        printf("%02x,", *pdata++);
        if(i % 16 ==0)
            printf("\n");
    }
    printf("\n");

	if(m_recvPort == 0) {
		IeBufLen = sizeof(WSC_IE_COMMAND_DATA) +
			sizeof(WSC_IE_HEADER) + length;
		for(i = 0; i < IeBufLen; i++)
			IeBuf[i] = sendBuf[i];
	}
	else
		IeBufLen = 0;

    return SendDataDown(sendBuf, sizeof(WSC_IE_COMMAND_DATA) +
			sizeof(WSC_IE_HEADER) + length);
}

uint32 CInbWlan::SetProbeReqIE( IN uint8 *p_data, IN uint32 length )
{
    char sendBuf[WSC_WLAN_DATA_MAX_LENGTH];
	WSC_IE_COMMAND_DATA * cmdData;
	WSC_IE_HEADER ieHdr;
	u8 * tmpPtr;

    TUTRACE((TUTRACE_INFO, "In CInbWlan::SetProbeReqIE  Length = %d\n",
            length));

	ieHdr.elemId = 221;
    ieHdr.length = length + 4;
	ieHdr.oui[0] = 0x00; ieHdr.oui[1] = 0x50;
    ieHdr.oui[2] = 0xF2; ieHdr.oui[3] = 0x04;

	cmdData = (WSC_IE_COMMAND_DATA *) sendBuf;

	cmdData->type = WSC_IE_TYPE_SET_PROBE_REQUEST_IE ;
	cmdData->length = sizeof(WSC_IE_HEADER) + length;
	tmpPtr = &(cmdData->data[0]);
	memcpy(tmpPtr, &ieHdr, sizeof(WSC_IE_HEADER));
	memcpy(tmpPtr + sizeof(WSC_IE_HEADER), p_data, length);

    return SendDataDown(sendBuf, sizeof(WSC_IE_COMMAND_DATA) +
			sizeof(WSC_IE_HEADER) + length);
}

uint32 CInbWlan::SetProbeRespIE( IN uint8 *p_data, IN uint32 length )
{
    char sendBuf[WSC_WLAN_DATA_MAX_LENGTH];
	WSC_IE_COMMAND_DATA * cmdData;
	WSC_IE_HEADER ieHdr;
	u8 * tmpPtr;

    //test
    unsigned int i;
    unsigned char * pdata;

    TUTRACE((TUTRACE_INFO, "In CInbWlan::SetProbeRespIE  Length = %d\n",
            length));

	ieHdr.elemId = 221;
	ieHdr.length = length + 4;
	ieHdr.oui[0] = 0x00; ieHdr.oui[1] = 0x50;
    ieHdr.oui[2] = 0xF2; ieHdr.oui[3] = 0x04;

	cmdData = (WSC_IE_COMMAND_DATA *) sendBuf;

	cmdData->type = WSC_IE_TYPE_SET_PROBE_RESPONSE_IE ;
	cmdData->length = sizeof(WSC_IE_HEADER) + length;
	tmpPtr = &(cmdData->data[0]);
#ifdef PROVISION_IE
	tmpPtr[0] = 0xDD;
	tmpPtr[1] = 0x05;
	tmpPtr[2] = 0x00;
	tmpPtr[3] = 0x50;
	tmpPtr[4] = 0xF2;
	tmpPtr[5] = 0x05;
	tmpPtr[6] = 0x0;
	tmpPtr = tmpPtr + 7;
	cmdData->length += 7;
#endif
	memcpy(tmpPtr, &ieHdr, sizeof(WSC_IE_HEADER));
	memcpy(tmpPtr + sizeof(WSC_IE_HEADER), p_data, length);
#ifdef PROVISION_IE
	length += 7;
#endif

    printf("prob_resp ie:\n");
    pdata = tmpPtr;
#ifdef PROVISION_IE
    pdata = &cmdData->data[0];
#endif
    for(i=0; i< (sizeof(WSC_IE_COMMAND_DATA) + sizeof(WSC_IE_HEADER) + length);i++)
    {
        printf("%02x,", *pdata++);
        if(i % 16 ==0)
            printf("\n");
    }
    printf("\n");

	if(m_recvPort == 0) {
		ProbRespBufLen = sizeof(WSC_IE_COMMAND_DATA) +
			sizeof(WSC_IE_HEADER) + length;
		for(i = 0; i < ProbRespBufLen; i++)
			ProbRespBuf[i] = sendBuf[i];
	}
	else
		ProbRespBufLen = 0;

    return SendDataDown(sendBuf, sizeof(WSC_IE_COMMAND_DATA) +
			sizeof(WSC_IE_HEADER) + length);
    return WSC_SUCCESS;
}

uint32 CInbWlan::SendProbeRequest( IN char * ssid)
{
    char sendBuf[WSC_WLAN_DATA_MAX_LENGTH];
	WSC_IE_COMMAND_DATA * cmdData;
	u8 * tmpPtr;
    int ssidLen = strlen(ssid) + 1;

    TUTRACE((TUTRACE_INFO, "In CInbWlan::SendProbeRequest %s\n", ssid));

	cmdData = (WSC_IE_COMMAND_DATA *) sendBuf;

	cmdData->type = WSC_IE_TYPE_SEND_PROBE_REQUEST;
	cmdData->length = sizeof(WSC_IE_HEADER) + ssidLen;
	tmpPtr = &(cmdData->data[0]);
    memcpy(tmpPtr, ssid, ssidLen);

    return SendDataDown(sendBuf, sizeof(WSC_IE_COMMAND_DATA) + ssidLen);
}

uint32 CInbWlan::SendBeaconsUp(IN bool activate)
{
    char sendBuf[WSC_WLAN_DATA_MAX_LENGTH];
	WSC_IE_COMMAND_DATA * cmdData;
	u8 * tmpPtr;

    TUTRACE((TUTRACE_INFO, "In CInbWlan::SendBeaconsUp Flag  = %d\n",
            (int) activate));

	cmdData = (WSC_IE_COMMAND_DATA *) sendBuf;

	cmdData->type = WSC_IE_TYPE_SEND_BEACONS_UP;
	cmdData->length = sizeof(WSC_IE_HEADER) + 1;
	tmpPtr = &(cmdData->data[0]);
	tmpPtr[0] = (uint8) activate;

    return SendDataDown(sendBuf, sizeof(WSC_IE_COMMAND_DATA) + 1);
}

uint32 CInbWlan::SendProbeResponsesUp(IN bool activate)
{
    char sendBuf[WSC_WLAN_DATA_MAX_LENGTH];
	WSC_IE_COMMAND_DATA * cmdData;
	u8 * tmpPtr;

    TUTRACE((TUTRACE_INFO, "In CInbWlan::SendProbeResponsesUp Flag  = %d\n",
            (int) activate));

	cmdData = (WSC_IE_COMMAND_DATA *) sendBuf;

	cmdData->type = WSC_IE_TYPE_SEND_PR_RESPS_UP;
	cmdData->length = sizeof(WSC_IE_HEADER) + 1;
	tmpPtr = &(cmdData->data[0]);
	tmpPtr[0] = (uint8) activate;

    return SendDataDown(sendBuf, sizeof(WSC_IE_COMMAND_DATA) + 1);
}

