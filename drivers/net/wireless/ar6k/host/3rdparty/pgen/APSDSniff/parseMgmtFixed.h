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
#ifndef _PARSEMGMETFIXED_H_
#define _PARSEMGMTFIXED_H_

#include "wl.h"

unsigned long long get_auth_alg_num(u_char **packetPtr,
				    struct packet_info *pInfo,
				    int *pLen);
unsigned long long get_auth_trans_seq_num(u_char **packetPtr, 
					  struct packet_info *pInfo,
					  int *pLen);
unsigned long long get_beacon_interval(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen);
unsigned long long get_capability_info(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen);
unsigned long long get_curr_ap_address(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen);
unsigned long long get_listen_interval(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen);
unsigned long long get_reason_code(u_char **packetPtr,
				   struct packet_info *pInfo,
				   int *pLen);
unsigned long long get_aid(u_char **packetPtr, struct packet_info *pInfo,
			   int *pLen);
unsigned long long get_status_code(u_char **packetPtr,
				   struct packet_info *pInfo,
				   int *pLen);
unsigned long long get_timestamp(u_char **packetPtr,
				 struct packet_info *pInfo,
				 int *pLen);

//====================================================================

void print_auth_alg_num(struct packet_info *pInfo);
void print_auth_trans_seq_num(struct packet_info *pInfo);
void print_beacon_interval(struct packet_info *pInfo);
void print_capability_info(struct packet_info *pInfo);
void print_current_ap_address(struct packet_info *pInfo);
void print_listen_interval(struct packet_info *pInfo);
void print_reason_code(struct packet_info *pInfo);
void print_aid(struct packet_info *pInfo);
void print_status_code(struct packet_info *pInfo);
void print_timestamp(struct packet_info *pInfo);

#endif
