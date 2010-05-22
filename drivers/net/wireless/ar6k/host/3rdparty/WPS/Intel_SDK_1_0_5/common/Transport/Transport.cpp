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
//  File Name : Transport.cpp
//  Description: This file implements the Transport Manager
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
#else // __linux__
#include <winsock2.h>
#include <windows.h>
#endif // __linux__

#include "tutrace.h"
#include "slist.h"
#include "WscCommon.h"
#include "WscError.h"
#include "Portability.h"
#include "WscQueue.h"
#include "OobUfd.h"
#include "OobNfc.h"
#include "InbEap.h"
#include "InbWlan.h"
#include "InbIp.h"
#include "InbUPnPCp.h"
#include "InbUPnPDev.h"
#include "Transport.h"  

#ifdef __linux__
#define SLEEP(X) sleep(X)
#else 
#define SLEEP(X) Sleep(X * 1000)
#endif

// uncomment out this line if you want to dump messages
//#define  _MSG_DUMP_

CTransport::CTransport()
{
    TUTRACE((TUTRACE_INFO, "CTransport Construction\n"));

    for (int i = 0; i < TRANSPORT_TYPE_MAX - 1; i ++) {
        m_TransportObjects[i] = NULL;
    }

    m_cbDead = true;
}

CTransport::~CTransport()
{
    TUTRACE((TUTRACE_INFO, "CTransport Destruction\n"));
    Deinit();
}

/*
 * Name        : StaticCallbackProc
 * Description : Static callback method that other objects use to pass
 *                    info back to the MasterControl
 * Arguments   : IN void *p_callBackMsg - pointer to the data being 
 *                    passed in
 *                 IN void *p_thisObj - pointer to MC
 * Return type : none
 */
void 
CTransport::StaticCallbackProc(
                        IN void *data, 
                        IN void *cookie) 
{
    S_CB_HEADER *p_header = (S_CB_HEADER *)data;

    TUTRACE((TUTRACE_INFO, "In CTransport::StaticCallbackProc\n"));

    uint32 dw_length = p_header->dataLength + 
                        sizeof(S_CB_HEADER);

    (((CTransport *)cookie)->mp_cbQ)->Enqueue(  
                                    dw_length,        // size of the data
                                    5,                // sequence number
                                    data );           // pointer to the data

    // TODO: check for return of Enqueue() operation

    return; 
} // StaticCallbackProc

/*
 * Name        : StaticCBThreadProc
 * Description : This is a static thread procedure for the callback thread.
 *                 Call the actual thread proc.
 * Arguments   : IN void *p_data
 * Return type : none
 */
void * 
CTransport::StaticCBThreadProc(IN void *p_data)
{
    return ((CTransport *)p_data)->ActualCBThreadProc();
} // StaticCBThreadProc

/*
 * Name        : ActualCBThreadProc
 * Description : This is the thread procedure for the callback thread.
 *                 Monitor the callbackQueue, and process all callbacks that 
 *                 are received.
 * Arguments   : none
 * Return type : none
 */
void *
CTransport::ActualCBThreadProc()
{
    bool    b_done = false;
    uint32  h_status;
    void    *p_message;
    S_CB_HEADER *p_header;

    TUTRACE((TUTRACE_INFO, "In CTransport::ActualCBThreadProc\n"));
    // keep doing this until the thread is killed
    while (!b_done) 
    {
        TUTRACE((TUTRACE_INFO, "Calling Dequeue\n"));
        // block on the callbackQueue
        h_status = mp_cbQ->Dequeue(  
                            NULL,        // size of dequeued msg
                            5,            // sequence number
                            0,            // infinite timeout
                            (void **) &p_message); 
                                        // pointer to the dequeued msg

        TUTRACE((TUTRACE_INFO, "Came out of Dequeue\n"));
        if (WSC_SUCCESS != h_status)
        {
            // something went wrong
            b_done = true;
            TUTRACE((TUTRACE_ERR, "Dequeue returned error %x\n", h_status));
            break; // from while loop
        }
        
        p_header = (S_CB_HEADER *)p_message;

        // once we get something, parse it, 
        // do whats necessary, and then block
        switch(p_header->eType) 
        {
            case CB_QUIT:
            {
                // no params
                // destroy the queue
                if (mp_cbQ) 
                {
                    mp_cbQ->DeInit();
                    delete mp_cbQ;
                }
                // kill the thread
                b_done = true;
                break;
            }
            case CB_TRUFD:
            case CB_TRUFD_INSERTED:
            case CB_TRNFC:
            case CB_TREAP:
            case CB_TRWLAN_BEACON:
            case CB_TRWLAN_PR_REQ:
            case CB_TRWLAN_PR_RESP:
            case CB_TRIP:
            case CB_TRUPNP_CP:
            case CB_TRUPNP_DEV:
            case CB_TRUPNP_DEV_SSR:
            {
                ProcessMessage(p_message);
                break;
            }
            default:
                // not understood, do nothing
                break;
        } // switch

        // free the data
        delete ((uint8 *)p_message);
    } // while

#ifdef __linux__
    pthread_exit(NULL);
#endif // __linux__
    return 0;
} // ActualCBThreadProc

/*
 * Name        : KillCallbackThread
 * Description : Attempt to terminate the callback thread. Enqueue a 
 *                 CB_QUIT in the callbackQueue
 * Arguments   : none
 * Return type : void
 */
void
CTransport::KillCallbackThread() const 
{
    // enqueue a CB_QUIT
    S_CB_HEADER *p = new S_CB_HEADER;
    p->eType = CB_QUIT;
    p->dataLength = 0;

    mp_cbQ->Enqueue(sizeof(S_CB_HEADER), 5, p);
    return;
} // KillCallbackThread


uint32 CTransport::SetMCCallback(IN CALLBACK_FN p_mcCallbackFn, IN void* cookie)
{
    m_mcCallbackInfo.pf_callback = p_mcCallbackFn;
    m_mcCallbackInfo.p_cookie = cookie;
    return WSC_SUCCESS;
}


uint32 CTransport::SetSMCallback(IN CALLBACK_FN p_smCallbackFn, IN void* cookie)
{
    m_smCallbackInfo.pf_callback = p_smCallbackFn;
    m_smCallbackInfo.p_cookie = cookie;
    return WSC_SUCCESS;
}


uint32 CTransport::Init()
{
    // create callback queue
    mp_cbQ = new CWscQueue();
    if (!mp_cbQ)
    {
        TUTRACE((TUTRACE_ERR, "Allocation of Queue failed\n"));
        return WSC_ERR_OUTOFMEMORY;
        // throw runtime_error("MC: mp_cbQ not created");
    }
    mp_cbQ->Init();

    // create callback thread
    uint32 retVal = WscCreateThread( &m_cbThreadHandle, StaticCBThreadProc, this);

    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "CreateThread failed.\n"));
        return WSC_ERR_SYSTEM;
    }

    m_cbDead = false;

    SLEEP(0);

    return WSC_SUCCESS;
}

uint32 CTransport::StartMonitor(TRANSPORT_TYPE trType)
{
    uint32 retVal = WSC_SUCCESS;

    if (trType < 1 || trType >= TRANSPORT_TYPE_MAX )
    {
        TUTRACE((TUTRACE_ERR, "Transport Type is not within the accepted range\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if (m_TransportObjects[trType - 1] == NULL)
    {
        switch (trType)
        {
#ifndef UNDER_CE
        case TRANSPORT_TYPE_UFD:
            {
                COobUfd * oobUfd = new COobUfd;
                if (oobUfd == NULL)
                {
                    TUTRACE((TUTRACE_ERR,  "Creating OobUfd failed.\n"));
                    return WSC_ERR_OUTOFMEMORY;
                }

                m_TransportObjects[trType - 1] = oobUfd;
            }
            break;

        case TRANSPORT_TYPE_EAP:
            {
                CInbEap * inbEap = new CInbEap;
                if (inbEap == NULL)
                {
                    TUTRACE((TUTRACE_ERR,  "Creating CInbEap failed.\n"));
                    return WSC_ERR_OUTOFMEMORY;
                }

                m_TransportObjects[trType - 1] = inbEap;
            }
            break;


        case TRANSPORT_TYPE_WLAN:
            {
                CInbWlan * inbWlan = new CInbWlan;
                if (inbWlan == NULL)
                {
                    TUTRACE((TUTRACE_ERR,  "Creating CInbWlan failed.\n"));
                    return WSC_ERR_OUTOFMEMORY;
                }

                m_TransportObjects[trType - 1] = inbWlan;
            }
            break;
        case TRANSPORT_TYPE_NFC:
#ifndef __linux__
            {
                // Not distributing NFC code... COobNfc * oobNfc = new COobNfc;
                //if (oobNfc == NULL)
                //{
                //    TUTRACE((TUTRACE_ERR,  "Creating OobNfc failed.\n"));
                //    return WSC_ERR_OUTOFMEMORY;
                //}

                m_TransportObjects[trType - 1] = NULL;
                // m_TransportObjects[trType - 1] = oobNfc;
            }
#endif // __linux__
            break;

#endif // UNDER_CE
        case TRANSPORT_TYPE_IP:
            {
                CInbIp * inbIp = new CInbIp;
                if (inbIp == NULL)
                {
                    TUTRACE((TUTRACE_ERR,  "Creating CInbIp failed.\n"));
                    return WSC_ERR_OUTOFMEMORY;
                }

                m_TransportObjects[trType - 1] = inbIp;
            }
            break;

       case TRANSPORT_TYPE_UPNP_CP:
            {
                CInbUPnPCp * UPnPCp = new CInbUPnPCp;
                if (UPnPCp == NULL)
                {
                    TUTRACE((TUTRACE_ERR,  "Creating CInbUPnPCp failed.\n"));
                    return WSC_ERR_OUTOFMEMORY;
                }

                m_TransportObjects[trType - 1] = UPnPCp;
            }
            break;

       case TRANSPORT_TYPE_UPNP_DEV:
            {
                CInbUPnPDev * UPnPDev = new CInbUPnPDev;
                if (UPnPDev == NULL)
                {
                    TUTRACE((TUTRACE_ERR,  "Creating CInbUPnPDev failed.\n"));
                    return WSC_ERR_OUTOFMEMORY;
                }

                m_TransportObjects[trType - 1] = UPnPDev;
            }
            break;

        case TRANSPORT_TYPE_BT:
            break;
        default:
            // No way it can come here
            break;
        } // switch

        retVal = GetTransportObject(trType)->SetTrCallback(
                this->StaticCallbackProc, this);
        if (retVal != WSC_SUCCESS)
        {
            TUTRACE((TUTRACE_ERR,  "Setting Callback for "
                    "trType %d failed.\n", trType));
            return retVal;
        }
        retVal = m_TransportObjects[trType - 1]->Init();
        if (retVal != WSC_SUCCESS)
        {
            TUTRACE((TUTRACE_ERR,  "Init for trType %d failed.\n", trType));
            return retVal;
        }
    } // if

    retVal = m_TransportObjects[trType - 1]->StartMonitor();
    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "StartMonitor for "
                "trType %d failed.\n", trType));
        return retVal;
    }

    return WSC_SUCCESS;
}

uint32 CTransport::StartMonitor(TRANSPORT_TYPE trType, bool serverFlag)
{
    uint32 retVal = WSC_SUCCESS;

    if (trType != TRANSPORT_TYPE_IP )
    {
        TUTRACE((TUTRACE_ERR, "TrType not supported by this function\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if (m_TransportObjects[trType - 1] == NULL)
    {
        CInbIp * inbIp = new CInbIp;
        if (inbIp == NULL)
        {
            TUTRACE((TUTRACE_ERR,  "Creating CInbIp failed.\n"));
            return WSC_ERR_OUTOFMEMORY;
        }

        m_TransportObjects[trType - 1] = inbIp;
        retVal = GetTransportObject(trType)->SetTrCallback(
                this->StaticCallbackProc, this);
        if (retVal != WSC_SUCCESS)
        {
            TUTRACE((TUTRACE_ERR,  "Setting Callback for "
                    "trType %d failed.\n", trType));
            return retVal;
        }
        retVal = inbIp->Init(serverFlag);
        if (retVal != WSC_SUCCESS)
        {
            TUTRACE((TUTRACE_ERR,  "Init for trType %d failed.\n", trType));
            return retVal;
        }
    } // if

    retVal = m_TransportObjects[trType - 1]->StartMonitor();
    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "StartMonitor for "
                "trType %d failed.\n", trType));
        return retVal;
    }

    return WSC_SUCCESS;
}

uint32 CTransport::StopMonitor(TRANSPORT_TYPE trType)
{
    uint32 retVal = WSC_SUCCESS;

    if (trType < 1 || trType >= TRANSPORT_TYPE_MAX )
    {
        TUTRACE((TUTRACE_ERR, "Transport Type is not within the "
                "accepted range\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if ( ! m_TransportObjects[trType - 1])
    {
        TUTRACE((TUTRACE_ERR,  "Transport Type %d is not "
                "initialized\n", trType));
        return WSC_ERR_NOT_INITIALIZED;
    }

    retVal = m_TransportObjects[trType - 1]->StopMonitor();
    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "StopMonitor for "
                "trType %d failed.\n", trType));
        return retVal;
    }

    return WSC_SUCCESS;
}


uint32 CTransport::TrRead(TRANSPORT_TYPE trType, char * dataBuffer, uint32 * dataLen)
{
    uint32 retVal = WSC_SUCCESS;

    TUTRACE((TUTRACE_INFO, "In CTransport::TrRead\n"));

    if (trType < 1 || trType >= TRANSPORT_TYPE_MAX )
    {
        TUTRACE((TUTRACE_ERR, "Transport Type is not within the "
                "accepted range\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if ( ! m_TransportObjects[trType - 1])
    {
        TUTRACE((TUTRACE_ERR,  "Transport Type %d is not "
                "initialized\n", trType));
        return WSC_ERR_NOT_INITIALIZED;
    }

    retVal = m_TransportObjects[trType - 1]->ReadData(dataBuffer, dataLen);
    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "ReadData for "
                "trType %d failed.\n", trType));
        return retVal;
    }

    return WSC_SUCCESS;
}

uint32 CTransport::TrWriteOobData(TRANSPORT_TYPE trType, 
		EOobDataType oobdType, uint8 *p_mac, BufferObj &buff)
{
    uint32 retVal = WSC_SUCCESS;

    TUTRACE((TUTRACE_INFO, "In CTransport::TrWriteOobData\n"));

    if (trType < 1 || trType >= TRANSPORT_TYPE_MAX )
    {
        TUTRACE((TUTRACE_ERR, "Transport Type is not within the "
                "accepted range\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if ( ! m_TransportObjects[trType - 1])
    {
        TUTRACE((TUTRACE_ERR,  "Transport Type %d is not "
                "initialized\n", trType));
        return WSC_ERR_NOT_INITIALIZED;
    }
#ifdef _MSG_DUMP_
	uint32 bufSize = buff.Length();
	fprintf(stderr, "Dumping Message to be written (%d bytes):\n", bufSize);
	uint8 *p = buff.GetBuf();
	for (uint32 i = 0; i < bufSize; i ++)
	{
		fprintf(stderr, "%02x ", (unsigned char) p[i]);
		if (((i+1) % 16) == 0)
		{
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
#endif // _MSG_DUMP_

    retVal = m_TransportObjects[trType - 1]->WriteOobData(oobdType, p_mac, buff);

    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "WriteOobData for "
                "trType %d failed.\n", trType));
        return retVal;
    }

    return WSC_SUCCESS;
}

uint32 CTransport::TrReadOobData(TRANSPORT_TYPE trType, 
		EOobDataType oobdType, uint8 *p_mac, BufferObj &buff)
{
    uint32 retVal = WSC_SUCCESS;

    TUTRACE((TUTRACE_INFO, "In CTransport::TrRead\n"));

    if (trType < 1 || trType >= TRANSPORT_TYPE_MAX )
    {
        TUTRACE((TUTRACE_ERR, "Transport Type is not within the "
                "accepted range\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if ( ! m_TransportObjects[trType - 1])
    {
        TUTRACE((TUTRACE_ERR,  "Transport Type %d is not "
                "initialized\n", trType));
        return WSC_ERR_NOT_INITIALIZED;
    }

    retVal = m_TransportObjects[trType - 1]->ReadOobData(oobdType, p_mac, buff);
    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "ReadOobData for "
                "trType %d failed.\n", trType));
        return retVal;
    }
#ifdef _MSG_DUMP_
	uint32 bufSize = buff.Length();
	fprintf(stderr, "Dumping Message Read (%d bytes):\n", bufSize);
	uint8 *p = buff.GetBuf();
	for (uint32 i = 0; i < bufSize; i ++)
	{
		fprintf(stderr, "%02x ", (unsigned char) p[i]);
		if (((i+1) % 16) == 0)
		{
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
#endif // _MSG_DUMP_

    return WSC_SUCCESS;
}

uint32 CTransport::TrWrite(TRANSPORT_TYPE trType, char * dataBuffer, uint32 dataLen)
{
    uint32 retVal = WSC_SUCCESS;

    TUTRACE((TUTRACE_INFO, "In CTransport::TrWrite\n"));

#ifdef _MSG_DUMP_
	fprintf(stderr, "Dumping Message being Sent Out:\n");
	for (uint32 i = 0; i < dataLen; i ++)
	{
		fprintf(stderr, "%02x ", (unsigned char) dataBuffer[i]);
		if (((i+1) % 16) == 0)
		{
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
#endif // _MSG_DUMP_

    if (trType < 1 || trType >= TRANSPORT_TYPE_MAX )
    {
        TUTRACE((TUTRACE_ERR, "Transport Type is not within the "
                "accepted range\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

    if ( ! m_TransportObjects[trType - 1])
    {
        TUTRACE((TUTRACE_ERR,  "Transport Type %d is not "
                "initialized\n", trType));
        return WSC_ERR_NOT_INITIALIZED;
    }

    retVal = m_TransportObjects[trType - 1]->WriteData(dataBuffer, dataLen);
    if (retVal != WSC_SUCCESS)
    {
        TUTRACE((TUTRACE_ERR,  "WriteData for "
                "trType %d failed.\n", trType));
        return retVal;
    }

    return WSC_SUCCESS;
}


uint32 CTransport::Deinit()
{
    // kill the callback thread
    // mp_cbQ deleted in the callback thread
    if ( ! m_cbDead)
    {
        KillCallbackThread();
        // TODO: add destroy thread later
        m_cbDead = true;
        SLEEP(0);
    }

    // deinitialize all transports
    for (int i = 0; i < TRANSPORT_TYPE_MAX - 1; i++)
    {
        if (m_TransportObjects[i])
        {
            m_TransportObjects[i]->Deinit();
            delete m_TransportObjects[i];
            m_TransportObjects[i] = NULL;
        }
    }

    return WSC_SUCCESS;
}


uint32 CTransport::ProcessMessage(void * p_message)
{
    S_CB_HEADER *recvHeader;
    S_CB_HEADER *sendHeader;
    char * sendBuf;
#ifdef _MSG_DUMP_
    char * tmpPtr;
#endif

    TUTRACE((TUTRACE_INFO, "In CTransport::ProcessMessage\n"));

    recvHeader = (S_CB_HEADER *) p_message;

//    sendBuf = (char *) malloc(recvHeader->dataLength + sizeof(S_CB_HEADER));
	if (recvHeader->dataLength > 10000) { // probable bad data
		return WSC_ERR_INVALID_PARAMETERS;
	}

    sendBuf = new char[recvHeader->dataLength + sizeof(S_CB_HEADER)];
    if (sendBuf == NULL)
    {
        TUTRACE((TUTRACE_ERR,  "Memory Allocation Failed\n"));
        return WSC_ERR_OUTOFMEMORY;
    }
    sendHeader = (S_CB_HEADER *) sendBuf;

    // sendHeader->eType = CB_TRANS;
    sendHeader->eType = recvHeader->eType;
    sendHeader->dataLength = recvHeader->dataLength;
    if (recvHeader->dataLength)
    {
        memcpy(sendBuf + sizeof(S_CB_HEADER), 
            ((char *)p_message) + sizeof(S_CB_HEADER), 
            recvHeader->dataLength);
    }

#ifdef _MSG_DUMP_
	fprintf(stderr, "Dumping Message Received:\n");
	tmpPtr = sendBuf + sizeof(S_CB_HEADER);
	for (uint32 i = 0; i < sendHeader->dataLength; i ++)
	{
		fprintf(stderr, "%02x ", (unsigned char) tmpPtr[i]);
		if (((i+1) % 16) == 0)
		{
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
#endif // _MSG_DUMP_

    TUTRACE((TUTRACE_INFO, "In CTransport::Received Message Length = %d\n", 
		recvHeader->dataLength));
    // call back
    switch (recvHeader->eType)
    {
    case CB_TREAP:
    case CB_TRIP:
    case CB_TRUPNP_CP:
    case CB_TRUPNP_DEV:
        // Sending to State Machine
        if (m_smCallbackInfo.pf_callback)
            m_smCallbackInfo.pf_callback(sendBuf, m_smCallbackInfo.p_cookie);
        break;
    case CB_TRWLAN_BEACON:
    case CB_TRWLAN_PR_REQ:
    case CB_TRWLAN_PR_RESP:
    case CB_TRNFC:
    case CB_TRUFD:
    case CB_TRUFD_INSERTED:
    case CB_TRUFD_REMOVED:
    case CB_TRUPNP_DEV_SSR:
        // Sending to MasterControl
        if (m_mcCallbackInfo.pf_callback)
            m_mcCallbackInfo.pf_callback(sendBuf, m_mcCallbackInfo.p_cookie);
        break;
    default:
        TUTRACE((TUTRACE_ERR, "Invalid CB_TYPE\n"));
        break;
    }

    return WSC_SUCCESS;
}

uint32 CTransport::SetBeaconIE( IN uint8 *p_data, IN uint32 length )
{
	// printf("returning\n");
	// return WSC_SUCCESS;
	if ( ! GetTransportObject(TRANSPORT_TYPE_WLAN))
	{
        TUTRACE((TUTRACE_ERR, "TRANSPORT_TYPE_WLAN not inited?\n"));
		return WSC_ERR_NOT_INITIALIZED;
	}
	
    CInbWlan * inbWlan;
	inbWlan = (CInbWlan *) GetTransportObject(TRANSPORT_TYPE_WLAN);
	return (inbWlan->SetBeaconIE( p_data, length));
	
}

uint32 CTransport::SetProbeReqIE( IN uint8 *p_data, IN uint32 length )
{
	if ( ! GetTransportObject(TRANSPORT_TYPE_WLAN))
	{
        TUTRACE((TUTRACE_ERR, "TRANSPORT_TYPE_WLAN not inited?\n"));
		return WSC_ERR_NOT_INITIALIZED;
	}
	
    CInbWlan * inbWlan;
	inbWlan = (CInbWlan *) GetTransportObject(TRANSPORT_TYPE_WLAN);
	return (inbWlan->SetProbeReqIE( p_data, length));
}

uint32 CTransport::SetProbeRespIE( IN uint8 *p_data, IN uint32 length )
{
	if ( ! GetTransportObject(TRANSPORT_TYPE_WLAN))
	{
        TUTRACE((TUTRACE_ERR, "TRANSPORT_TYPE_WLAN not inited?\n"));
		return WSC_ERR_NOT_INITIALIZED;
	}
	
    CInbWlan * inbWlan;
	inbWlan = (CInbWlan *) GetTransportObject(TRANSPORT_TYPE_WLAN);
	return (inbWlan->SetProbeRespIE( p_data, length));
}

uint32 CTransport::SendProbeRequest( IN char * ssid)
{
	if ( ! GetTransportObject(TRANSPORT_TYPE_WLAN))
	{
		TUTRACE((TUTRACE_ERR, "TRANSPORT_TYPE_WLAN not inited?\n"));
		return WSC_ERR_NOT_INITIALIZED;
	}
	
	CInbWlan * inbWlan;
	inbWlan = (CInbWlan *) GetTransportObject(TRANSPORT_TYPE_WLAN);
	return (inbWlan->SendProbeRequest(ssid));
	
    return WSC_SUCCESS;
}

uint32 CTransport::SendBeaconsUp( IN bool activate)
{
	if ( ! GetTransportObject(TRANSPORT_TYPE_WLAN))
	{
		TUTRACE((TUTRACE_ERR, "TRANSPORT_TYPE_WLAN not inited?\n"));
		return WSC_ERR_NOT_INITIALIZED;
	}
	
	CInbWlan * inbWlan;
	inbWlan = (CInbWlan *) GetTransportObject(TRANSPORT_TYPE_WLAN);
	return (inbWlan->SendBeaconsUp(activate));
	
}


uint32 CTransport::SendProbeResponsesUp( IN bool activate)
{
	if ( ! GetTransportObject(TRANSPORT_TYPE_WLAN))
	{
		TUTRACE((TUTRACE_ERR, "TRANSPORT_TYPE_WLAN not inited?\n"));
		return WSC_ERR_NOT_INITIALIZED;
	}
	
	CInbWlan * inbWlan;
	inbWlan = (CInbWlan *) GetTransportObject(TRANSPORT_TYPE_WLAN);
	return (inbWlan->SendProbeResponsesUp(activate));
	
}


uint32 CTransport::SendStartMessage(TRANSPORT_TYPE trType)
{
	if ( ! GetTransportObject(trType))
	{
        TUTRACE((TUTRACE_ERR, "TRANSPORT_TYPE %d not inited?\n", trType));
		return WSC_ERR_NOT_INITIALIZED;
	}
	
	if (trType == TRANSPORT_TYPE_EAP)
	{
		CInbEap * inbEap;
		inbEap = (CInbEap *) GetTransportObject(TRANSPORT_TYPE_EAP);
		if (inbEap)
			return (inbEap->SendStartMessage());
	}
	else if (trType == TRANSPORT_TYPE_UPNP_CP)
	{
		CInbUPnPCp * inbUPnPCp;
		inbUPnPCp = (CInbUPnPCp *) GetTransportObject(TRANSPORT_TYPE_UPNP_CP);
		if (inbUPnPCp)
			return (inbUPnPCp->SendStartMessage());
	}
	else
	{
        TUTRACE((TUTRACE_ERR, "Transport Type invalid\n"));
        return WSC_ERR_INVALID_PARAMETERS;
	}

	return WSC_SUCCESS;
}

uint32 CTransport::SendSetSelectedRegistrar(char * dataBuffer, uint32 dataLen)
{
	if ( ! GetTransportObject(TRANSPORT_TYPE_UPNP_CP))
    {
        TUTRACE((TUTRACE_ERR, "Transport Type UPNP CP not initialized\n"));
        return WSC_ERR_INVALID_PARAMETERS;
    }

	CInbUPnPCp * inbUPnPCp;
	inbUPnPCp = (CInbUPnPCp *) GetTransportObject(TRANSPORT_TYPE_UPNP_CP);
    
    return (inbUPnPCp->SendSetSelectedRegistrar(dataBuffer, dataLen));
}
