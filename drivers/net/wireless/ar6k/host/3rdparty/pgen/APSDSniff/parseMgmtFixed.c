//////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2005  Andy Patti, Kevin Yu, Greg Chesson,
//    Atheros Communications
//
// License is granted to Wi-Fi Alliance members and designated
// contractors for exclusive use in testing of Wi-Fi equipment.
// This license is not transferable and does not extend to non-Wi-Fi
// applications.
//
// Derivative works are authorized and are limited by the
// same restrictions.
//
// Derivatives in binary form must reproduce the
// above copyright notice, the name of the authors "Andy Patti", "Kevin Yu"
// and "Greg Chesson",
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// The name of the authors may not be used to endorse or promote
// products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include "wl.h"
#include "mgmt.h"
#include "parseMgmtFixed.h"
#include "prism.h"
#include "wlcommon.h"

unsigned long long get_auth_alg_num(u_char **packetPtr, struct packet_info *pInfo,
				      int *pLen)
{
    unsigned long long error=0;
    int err;

    pInfo->mgmt.authAlgNum = 0;

    err = get_u_short(packetPtr,&(pInfo->mgmt.authAlgNum),pLen);
    if ((!err) && (pInfo->mgmt.authAlgNum > 1)) {
        error = AUTH_ALG_ERR;
    }
    return(error);
}

unsigned long long get_auth_trans_seq_num(u_char **packetPtr, 
					  struct packet_info *pInfo,
					  int *pLen)
{
    unsigned long long error=0;

    pInfo->mgmt.authTransSeqNum = 0;
    get_u_short(packetPtr,&(pInfo->mgmt.authTransSeqNum),pLen);
    return(error);
}

unsigned long long get_beacon_interval(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen)
{
    unsigned long long error=0;

    pInfo->mgmt.beaconInt = 0;
    get_u_short(packetPtr,&(pInfo->mgmt.beaconInt),pLen);
    return(error);
}

unsigned long long get_capability_info(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen)
{
    unsigned long long error=0;
    int err;
    u_short val;

    memset(&(pInfo->mgmt.capInfo),0,sizeof(struct capability_info));
    err = get_u_short(packetPtr,&(pInfo->mgmt.capInfo.rawData),pLen);
    if (!err) {
        val = pInfo->mgmt.capInfo.rawData;
        pInfo->mgmt.capInfo.ess = val & 0x0001;
        pInfo->mgmt.capInfo.ibss = (val >> 1) & 0x0001;
        pInfo->mgmt.capInfo.cfPoll = (val >> 2) & 0x0001;
        pInfo->mgmt.capInfo.cfPollReq = (val >> 3) & 0x0001;
        pInfo->mgmt.capInfo.privacy = (val >> 4) & 0x0001;
        pInfo->mgmt.capInfo.shortPreamble = (val >> 5) & 0x0001;
        pInfo->mgmt.capInfo.pbcc = (val >> 6) & 0x0001;
        pInfo->mgmt.capInfo.chanAgil = (val >> 7) & 0x0001;
        pInfo->mgmt.capInfo.specMgmt = (val >> 8) & 0x0001;
        pInfo->mgmt.capInfo.qos = (val >> 9) & 0x0001;
        pInfo->mgmt.capInfo.shortSlotTime = (val >> 10) & 0x0001;
        pInfo->mgmt.capInfo.apsd = (val >> 11) & 0x0001;
        pInfo->mgmt.capInfo.dsssOfdm = (val >> 13) & 0x0001;
        pInfo->mgmt.capInfo.blockAck = (val >> 14) & 0x0001;
        if ((pInfo->fc.typeIndex == 32) || (pInfo->fc.typeIndex == 20)) {
            if (pInfo->mgmt.isAp == 1) {
                if ((pInfo->mgmt.capInfo.ess != 1) ||
                    (pInfo->mgmt.capInfo.ibss != 0)) {
                    error = CAP_INFO_ERR;
                }

            } else {
                if ((pInfo->mgmt.capInfo.ess != 0) ||
                    (pInfo->mgmt.capInfo.ibss != 1)) {
                    error = CAP_INFO_ERR;
                }
            }
        }
        val = pInfo->mgmt.capInfo.qos << 2;
        val |= pInfo->mgmt.capInfo.cfPoll << 1;
        val |= pInfo->mgmt.capInfo.cfPollReq;
        if (pInfo->mgmt.isAp) { 
            if ((val == 3) || (val == 7)) {
                error = CAP_INFO_ERR;
            }
        } else {
            if (val >= 5) {
                error = CAP_INFO_ERR;
            }
        }
    }
    return(error);
}

unsigned long long get_curr_ap_address(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen)
{
    u_char *ptr;
    int numToCopy=6;
    unsigned long long error=0;

    
    error = get_hw_address(packetPtr,&(pInfo->mgmt.currAPaddr[0]),pLen);
    return(error);
}

unsigned long long get_listen_interval(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen)
{
    unsigned long long error=0;

    pInfo->mgmt.listenInt = 0;
    get_u_short(packetPtr,&(pInfo->mgmt.listenInt),pLen);
    return(error);
}

unsigned long long get_reason_code(u_char **packetPtr, struct packet_info *pInfo,
				   int *pLen)
{
    unsigned long long error=0;
    int err;

    pInfo->mgmt.reasonCode = 0;
    err = get_u_short(packetPtr,&(pInfo->mgmt.reasonCode),pLen);
    if (!err) {
        if (((pInfo->mgmt.reasonCode > 11) && (pInfo->mgmt.reasonCode < 32)) ||
            (pInfo->mgmt.reasonCode > 35)) {
            error = REASON_CODE_ERR;
        }
    }
    return(error);
}

unsigned long long get_aid(u_char **packetPtr, struct packet_info *pInfo,
			   int *pLen)
{
    unsigned long long error=0;
    int err;

    pInfo->mgmt.aid = 0;
    err = get_u_short(packetPtr,&(pInfo->mgmt.aid),pLen);
    if (!err) {
        if ((pInfo->mgmt.aid & 0xC000) != 0xC000) {
            error = AID_ERR;
        } else {
            if (((pInfo->mgmt.aid & 0x3FFF) > 2007) ||
                ((pInfo->mgmt.aid & 0x3FFF) == 0)) {
                error = AID_ERR;
            }
        }
    }
    return(error);
}

unsigned long long get_status_code(u_char **packetPtr,
				   struct packet_info *pInfo,
				   int *pLen)
{
    unsigned long long error=0;
    int err;

    pInfo->mgmt.statusCode = 0;
    err = get_u_short(packetPtr,&(pInfo->mgmt.statusCode),pLen);
    if ((!err) && (pInfo->mgmt.statusCode >43)) {
        error = STATUS_CODE_ERR;
    }
    return(err);
}

unsigned long long get_timestamp(u_char **packetPtr, struct packet_info *pInfo,
				 int *pLen)
{
    unsigned long long error=0;
    u_char *ptr;
    int i,shift;

    pInfo->mgmt.timestamp = 0;
    ptr = *packetPtr;
    for (i=0,shift=0;(i<8) && (*pLen > 0); i++,shift+=8) {
        pInfo->mgmt.timestamp |= ((unsigned long long) (*ptr++)) << shift;
        *pLen -= 1;
    }
    *packetPtr = ptr;
    return(error);
}


//==============================================================

void print_auth_alg_num(struct packet_info *pInfo)
{
    printf ("Auth. Alg. #: (%d) ",pInfo->mgmt.authAlgNum);
}

void print_auth_trans_seq_num(struct packet_info *pInfo)
{
    printf ("Auth. Trans. Seq. #: (%d) ",pInfo->mgmt.authTransSeqNum);
}

void print_beacon_interval(struct packet_info *pInfo)
{
    printf ("BeaconInt: (%d) ",pInfo->mgmt.beaconInt);
}

void print_capability_info(struct packet_info *pInfo)
{
    printf("CAP:  ess: (%d) ibss:(%d) cfPoll: (%d) cfPollReq: (%d) priv: (%d) shortPreamble: (%d)\n",
           pInfo->mgmt.capInfo.ess, pInfo->mgmt.capInfo.ibss,
           pInfo->mgmt.capInfo.cfPoll, pInfo->mgmt.capInfo.cfPollReq,
           pInfo->mgmt.capInfo.privacy, pInfo->mgmt.capInfo.shortPreamble);
    printf ("     pbcc: (%d) channelAgil: (%d)  spectrumMgt: (%d) qos: (%d) shortSlotTime: (%d)\n",
            pInfo->mgmt.capInfo.pbcc, pInfo->mgmt.capInfo.chanAgil, 
            pInfo->mgmt.capInfo.specMgmt, pInfo->mgmt.capInfo.qos,
            pInfo->mgmt.capInfo.shortSlotTime);
    printf ("      apsd: (%d) dsss/ofdm: (%d) blockAck: (%d)\n",
            pInfo->mgmt.capInfo.apsd, pInfo->mgmt.capInfo.dsssOfdm,
            pInfo->mgmt.capInfo.blockAck);
}

void print_current_ap_address(struct packet_info *pInfo)
{
    int i;

    printf ("Curr AP Addr: (");
    print_hw_address(&(pInfo->mgmt.currAPaddr[0]));
    printf (") ");
}

void print_listen_interval(struct packet_info *pInfo)
{
    printf ("Listen Int: (%x) ",pInfo->mgmt.listenInt);
}

void print_reason_code(struct packet_info *pInfo)
{
    printf ("Reason Code: (%d) ",pInfo->mgmt.reasonCode);
}

void print_aid(struct packet_info *pInfo)
{
    printf ("AID: (%d) ",pInfo->mgmt.aid);
}

void print_status_code(struct packet_info *pInfo)
{
    printf ("Status Code: (%d) ",pInfo->mgmt.statusCode);
}

void print_timestamp(struct packet_info *pInfo)
{
    printf ("TS: (0x%llx) (%llu) ",pInfo->mgmt.timestamp,
            pInfo->mgmt.timestamp);
}
