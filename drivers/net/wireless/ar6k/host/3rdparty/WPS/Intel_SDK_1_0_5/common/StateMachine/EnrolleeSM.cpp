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
//  File Name: EnrolleeSM.cpp
//  Description: Implements Enrollee State Machine Class.
//
****************************************************************************/

#ifdef WIN32
#include <windows.h>
#endif

//OpenSSL includes
#include <openssl/bn.h>
#include <openssl/dh.h>

#include "WscHeaders.h"
#include "WscCommon.h"
#include "WscError.h"
#include "Portability.h"
#include "WscQueue.h"
#include "tutrace.h"
#include "Transport.h"
#include "StateMachineInfo.h"
#include "RegProtoMsgs.h"
#include "RegProtocol.h"
#include "StateMachine.h"

#define M2D_SLEEP_TIME 15 // 15 seconds

CEnrolleeSM::CEnrolleeSM(IN CRegProtocol *pc_regProt, IN CTransport *pc_trans)
        :CStateMachine(pc_regProt, pc_trans, MODE_ENROLLEE)
{
    WscSyncCreate(&mp_m2dLock);
}

CEnrolleeSM::~CEnrolleeSM()
{
    WscSyncDestroy(mp_m2dLock);
}

uint32 
CEnrolleeSM::InitializeSM(IN S_DEVICE_INFO *p_registrarInfo, 
                          IN void * p_StaEncrSettings,
                          IN void * p_ApEncrSettings,
                          IN char *p_devicePasswd, 
                          IN uint32 passwdLength)
{
    // if(!p_registrarInfo)
    //    return WSC_ERR_INVALID_PARAMETERS;

    uint32 err = CStateMachine::InitializeSM();
    if(WSC_SUCCESS != err)
        return err;

    mps_regData->p_enrolleeInfo = mps_localInfo;
    mps_regData->p_registrarInfo = p_registrarInfo;

    SetPassword(p_devicePasswd, passwdLength);
    SetEncryptedSettings(p_StaEncrSettings, p_ApEncrSettings);

    return WSC_SUCCESS;
}

uint32 
CEnrolleeSM::Step(IN uint32 msgLen, IN uint8 *p_msg)
{
    BufferObj *inMsg=NULL;
    uint32 err;

    TUTRACE((TUTRACE_INFO, "ENRSM: Entering Step.\n"));

    if(false == m_initialized)
    {
        TUTRACE((TUTRACE_ERR, "ENRSM: Not yet initialized.\n"));
        return WSC_ERR_NOT_INITIALIZED;
    }

    if(START == mps_regData->e_smState)
    {
        //No special processing here
        HandleMessage(*inMsg);
    }
    else
    {
        //do the regular processing
        if(!p_msg || !msgLen)
        {
            //Preferential treatment for UPnP
            if(mps_regData->e_lastMsgSent == M1)
            {
                //If we have already sent M1 and we get here, assume that it is
                //another request for M1 rather than an error.
                //Send the bufferred M1 message

                TUTRACE((TUTRACE_INFO, "ENRSM: Got another request for M1. "
                                       "Resending the earlier M1\n"));
                err = mpc_trans->TrWrite(m_transportType, 
                                        (char *)mps_regData->outMsg.GetBuf(), 
                                        mps_regData->outMsg.Length());
                if(WSC_SUCCESS != err)
                {
                    mps_regData->e_smState = FAILURE;
                    TUTRACE((TUTRACE_ERR, "ENRSM: TrWrite generated an "
                                        "error: %d\n", err));
                    return err;
                }
                return WSC_SUCCESS;
            }
            else
            {
                TUTRACE((TUTRACE_ERR, "ENRSM: Wrong input parameters.\n"));
                //Notify the MasterControl
                NotifyMasterControl(SM_FAILURE, NULL, NULL);
                m_initialized = false;
                return WSC_ERR_INVALID_PARAMETERS;
            }
        }

        BufferObj regProtoMsg(p_msg, msgLen);
        inMsg = &regProtoMsg;
        HandleMessage(*inMsg);
    }

    //now check the state so we can act accordingly
    switch(mps_regData->e_smState)
    {
    case START:
    case CONTINUE:
        //do nothing.
        break;
    case SUCCESS:
        {
            m_initialized = false;
            //Notify the MasterControl
            NotifyMasterControl(SM_SUCCESS, 
                                mps_regData->p_registrarInfo, 
                                mp_peerEncrSettings);

            //reset the transport connection
            mpc_trans->TrWrite(m_transportType, NULL, 0);

            //reset the SM
            RestartSM();
        }
           break;
    case FAILURE:
        {
            TUTRACE((TUTRACE_ERR, "ENRSM: Notifying MC of failure.\n"));
            m_initialized = false;
            //Notify the MasterControl
            NotifyMasterControl(SM_FAILURE, NULL, NULL);

            //reset the transport connection
            mpc_trans->TrWrite(m_transportType, NULL, 0);

            //reset the SM
            RestartSM();
        }
         break;
    default:
        break;
    }
    return WSC_SUCCESS;
}

void CEnrolleeSM::HandleMessage(BufferObj &msg)
{
    uint32 err;
    char errMsg[256];
    BufferObj outBuf, tempBuf;
    void *encrSettings = NULL;
    uint32 msgType = 0;
    
    try
    {
        //Append the header before doing any processing
        //S_WSC_HEADER hdr;
        //hdr.opCode = WSC_MSG;
        //hdr.flags = 0;
        //outBuf.Append(sizeof(hdr), (uint8 *)&hdr);

        //If we get a valid message, extract the message type received. 
        if(MNONE != mps_regData->e_lastMsgSent)
        {
            err = mpc_regProt->GetMsgType(msgType, msg);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, 
                            "ENRSM: GetMsgType returned error: %d\n", err);
                throw errMsg;
            }

            //If this is a late-arriving M2D, ACK it.  Otherwise, ignore it.
			// Should probably also NACK an M2 at this point.  
			//
            if((SM_RECVD_M2 == m_m2dStatus) &&
               (msgType <= WSC_ID_MESSAGE_M2D))
            {
				if (WSC_ID_MESSAGE_M2D == msgType) {
					SendAck();
				}

                TUTRACE((TUTRACE_INFO, "ENRSM: Possible late M2D received.  Sending ACK.\n"));
                return;
            }
        }

        switch(mps_regData->e_lastMsgSent)
        {
        case MNONE:
            err = mpc_regProt->BuildMessageM1(mps_regData, tempBuf);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "BuildMessageM1: %d", err);
                throw errMsg;
            }
            mps_regData->e_lastMsgSent = M1;

            outBuf.Append(tempBuf.Length(), tempBuf.GetBuf());
            
            //Now send the message to the transport
            err = mpc_trans->TrWrite(m_transportType, 
                               (char *)outBuf.GetBuf(), 
                               outBuf.Length());
            if(WSC_SUCCESS != err)
            {
                mps_regData->e_smState = FAILURE;
                TUTRACE((TUTRACE_ERR, "ENRSM: TrWrite generated an "
                                      "error: %d\n", err));
                return;
            }

            //Set the m2dstatus.
            m_m2dStatus = SM_AWAIT_M2;

            //set the message state to CONTINUE
            mps_regData->e_smState = CONTINUE;
            break;
        case M1:
            //Check whether this is M2D
            if(WSC_ID_MESSAGE_M2D == msgType)
            {
                err = mpc_regProt->ProcessMessageM2D(mps_regData, msg);
                if(WSC_SUCCESS != err)
                {
                    stringPrintf(errMsg, 256, "ProcessMessageM2D: %d", err);
                    throw errMsg;
                }
                
                //Send an ACK to the registrar
                err = SendAck();
                if(WSC_SUCCESS != err)
                {
                    stringPrintf(errMsg, 256, "SendAck: %d", err);
                    throw errMsg;
                }

                //Now, schedule a thread to sleep for some time to allow other 
                //registrars to send M2 or M2D messages.
                WscLock(mp_m2dLock);
                if(SM_AWAIT_M2 == m_m2dStatus)
                {
                    //if the M2D status is 'await', set the timer. For all 
                    //other cases, don't do anything, because we've either 
                    //already received an M2 or M2D, and possibly, the SM reset
                    //process has already been initiated
                    TUTRACE((TUTRACE_INFO, "ENRSM: Starting M2DThread\n"));

                    m_m2dStatus = SM_RECVD_M2D;

                    err = WscCreateThread(
                                &m_timerThrdId,     // thread ID
                                M2DTimerThread,     // thread proc
                                (void *)this);      // data to pass to thread
                    if (WSC_SUCCESS != err)
                    {
                        throw "RegSM: m_cbThread not created";
                    }
                    TUTRACE((TUTRACE_INFO, "ENRSM: Started M2DThread\n"));

                    WscSleep(1);

                    //set the message state to CONTINUE
                    mps_regData->e_smState = CONTINUE;
                    WscUnlock(mp_m2dLock);
                    return;
                }
                else
                {
                    TUTRACE((TUTRACE_INFO, "ENRSM: Did not start M2DThread. "
                             "status = %d\n", m_m2dStatus));
                }
                WscUnlock(mp_m2dLock);

                break; //done processing for M2D, return
            }//if(M2D == msgType)

            //If the message wasn't M2D, do processing for M2
            WscLock(mp_m2dLock);
            if(SM_M2D_RESET == m_m2dStatus)
            {
                WscUnlock(mp_m2dLock);
                return; //a SM reset has been initiated. Don't process any M2s
            }
            else
            {
                m_m2dStatus = SM_RECVD_M2;
            }
            WscUnlock(mp_m2dLock);

            err =mpc_regProt->ProcessMessageM2(mps_regData, msg, &encrSettings);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "ProcessMessageM2: %d", err);
                //Send a NACK to the registrar
                SendNack(WSC_MESSAGE_PROCESSING_ERROR);
                throw errMsg;
            }
            mps_regData->e_lastMsgRecd = M2;

            err = mpc_regProt->BuildMessageM3(mps_regData, tempBuf);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "BuildMessageM3: %d", err);
                //Send a NACK to the registrar
                SendNack(WSC_MESSAGE_PROCESSING_ERROR);
                throw errMsg;
            }
            mps_regData->e_lastMsgSent = M3;
            
            outBuf.Append(tempBuf.Length(), tempBuf.GetBuf());
            
            //Now send the message to the transport
            err = mpc_trans->TrWrite(m_transportType, 
                               (char *)outBuf.GetBuf(), 
                               outBuf.Length());
            if(WSC_SUCCESS != err)
            {
                mps_regData->e_smState = FAILURE;
                TUTRACE((TUTRACE_ERR, "ENRSM: TrWrite generated an "
                                      "error: %d\n", err));
                return;
            }

            //set the message state to CONTINUE
            mps_regData->e_smState = CONTINUE;
            break;
        case M3:
            err = mpc_regProt->ProcessMessageM4(mps_regData, msg);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "ProcessMessageM4: %d", err);
                //Send a NACK to the registrar
				if (err == RPROT_ERR_CRYPTO) {
					SendNack(WSC_PASSWORD_AUTH_ERROR);
				} else {
					SendNack(WSC_MESSAGE_PROCESSING_ERROR);
				}
                throw errMsg;
            }
            mps_regData->e_lastMsgRecd = M4;

            err = mpc_regProt->BuildMessageM5(mps_regData, tempBuf);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "BuildMessageM5: %d", err);
                //Send a NACK to the registrar
                SendNack(WSC_MESSAGE_PROCESSING_ERROR);
                throw errMsg;
            }
            mps_regData->e_lastMsgSent = M5;            
            
            outBuf.Append(tempBuf.Length(), tempBuf.GetBuf());
            
            //Now send the message to the transport
            err = mpc_trans->TrWrite(m_transportType, 
                               (char *)outBuf.GetBuf(), 
                               outBuf.Length());
            if(WSC_SUCCESS != err)
            {
                mps_regData->e_smState = FAILURE;
                TUTRACE((TUTRACE_ERR, "ENRSM: TrWrite generated an "
                                      "error: %d\n", err));
                return;
            }

            //set the message state to CONTINUE
            mps_regData->e_smState = CONTINUE;
            break;
        case M5:
            err = mpc_regProt->ProcessMessageM6(mps_regData, msg);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "ProcessMessageM6: %d", err);
                //Send a NACK to the registrar
				if (err == RPROT_ERR_CRYPTO) {
					SendNack(WSC_PASSWORD_AUTH_ERROR);
				} else {
					SendNack(WSC_MESSAGE_PROCESSING_ERROR);
				}
                throw errMsg;
            }
            mps_regData->e_lastMsgRecd = M6;

            //Build message 7 with the appropriate encrypted settings
            if(mps_regData->p_enrolleeInfo->b_ap)
            {
                err = mpc_regProt->BuildMessageM7(mps_regData, 
                                                  tempBuf,
                                                  mps_regData->apEncrSettings);
            }
            else
            {
                err = mpc_regProt->BuildMessageM7(mps_regData, 
                                                  tempBuf,
                                                  mps_regData->staEncrSettings);
            }

            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "BuildMessageM7: %d", err);
                //Send a NACK to the registrar
                SendNack(WSC_MESSAGE_PROCESSING_ERROR);
                throw errMsg;
            }
            mps_regData->e_lastMsgSent = M7;

            outBuf.Append(tempBuf.Length(), tempBuf.GetBuf());
            
            //Now send the message to the transport
            err = mpc_trans->TrWrite(m_transportType, 
                               (char *)outBuf.GetBuf(), 
                               outBuf.Length());
            if(WSC_SUCCESS != err)
            {
                mps_regData->e_smState = FAILURE;
                TUTRACE((TUTRACE_ERR, "ENRSM: TrWrite generated an "
                                      "error: %d\n", err));
                return;
            }
            //set the message state to CONTINUE
            mps_regData->e_smState = CONTINUE;

            break;
        case M7:
            err = mpc_regProt->ProcessMessageM8(mps_regData, msg,&encrSettings);
            if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "ProcessMessageM8: %d", err);
				// Send NACK to the Registrar
				SendNack(WSC_MESSAGE_PROCESSING_ERROR);
                throw errMsg;
            }
            mps_regData->e_lastMsgRecd = M8;
            mp_peerEncrSettings = encrSettings;

            //Send a Done message
            SendDone();
            mps_regData->e_lastMsgSent = DONE;

            //Decide if we need to wait for an ACK
            //Wait only if we're an AP AND we're running EAP
            if((!mps_regData->p_enrolleeInfo->b_ap) ||
               (m_transportType != TRANSPORT_TYPE_EAP))
            {
                //set the message state to success
                mps_regData->e_smState = SUCCESS;
            }
            else
            {
                //Wait for ACK. set the message state to continue
                mps_regData->e_smState = CONTINUE;
            }
            break;
        case DONE:
            err = mpc_regProt->ProcessMessageAck(mps_regData, msg);
			if (RPROT_ERR_NONCE_MISMATCH == err) { 
                mps_regData->e_smState = CONTINUE; // ignore nonce mismatches
			} else if(WSC_SUCCESS != err)
            {
                stringPrintf(errMsg, 256, "ProcessMessageAck: %d", err);
                throw errMsg;
            }

            //set the message state to success
            mps_regData->e_smState = SUCCESS;
            break;
        default:
            throw "Unexpected message received";
        }
    }
    catch(uint32 err)
    {
        TUTRACE((TUTRACE_ERR, "ENRSM: HandleMessage threw an exception: %d\n",
                              err));
        //send an empty message to the transport
        mpc_trans->TrWrite(m_transportType, NULL, 0);

        //set the message state to failure
        mps_regData->e_smState = FAILURE;

    }
    catch(char *str)
    {
        TUTRACE((TUTRACE_ERR, "ENRSM: HandleMessage threw an exception: %s\n",
                              str));
        //send an empty message to the transport
        mpc_trans->TrWrite(m_transportType, NULL, 0);

        //set the message state to failure
        mps_regData->e_smState = FAILURE;
    }
    catch(...)
    {
        TUTRACE((TUTRACE_ERR, "ENRSM: HandleMessage threw an unknown "
                              "exception\n"));
        //send an empty message to the transport
        mpc_trans->TrWrite(m_transportType, NULL, 0);

        //set the message state to failure
        mps_regData->e_smState = FAILURE;
    }
}

void *
CEnrolleeSM::M2DTimerThread(IN void *p_data)
{
    TUTRACE((TUTRACE_INFO, "ENRSM: In M2DTimerThread\n"));
    CEnrolleeSM *enrollee = (CEnrolleeSM *)p_data;
    //We need to wait for some time to allow additional M2/M2D messages
    //to arrive
#ifndef __linux__
    Sleep(M2D_SLEEP_TIME*1000);
#else
    sleep(M2D_SLEEP_TIME);
#endif

    WscLock(enrollee->mp_m2dLock);
    if(SM_RECVD_M2 == enrollee->m_m2dStatus)
    {
        //Nothing to be done. 
        WscUnlock(enrollee->mp_m2dLock);
        return NULL;
    }
    WscUnlock(enrollee->mp_m2dLock);

    //Push a message into the queue to restart the SM. 
    //This has better thread safety
    S_CB_COMMON *p_NotifyBuf = new S_CB_COMMON;
    p_NotifyBuf->cbHeader.eType = CB_SM_RESET;
    p_NotifyBuf->cbHeader.dataLength = 0;

    TUTRACE((TUTRACE_INFO, "ENRSM: Sending RESET callback to SM\n"));

    StaticCallbackProc(p_NotifyBuf, p_data);

    //Now make the transport send an EAP fail
    TUTRACE((TUTRACE_INFO, "ENRSM: Sending EAP FAIL\n"));

    enrollee->mpc_trans->TrWrite(TRANSPORT_TYPE_EAP, NULL, 0);
    return NULL;
}

