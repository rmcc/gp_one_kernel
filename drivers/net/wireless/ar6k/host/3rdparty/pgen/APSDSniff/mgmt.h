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
#ifndef _MGMT_H_
#define _MGMT_H_

#define AUTH_ALG_ERR     0x0000000000000001
#define CAP_INFO_ERR     0x0000000000000002
#define REASON_CODE_ERR  0x0000000000000004
#define STATUS_CODE_ERR  0x0000000000000008
#define SSID_ERR         0x0000000000000010
#define SUPP_RATE_ERR    0x0000000000000020
#define FH_PARAM_SET_ERR 0x0000000000000040
#define DS_PARAM_SET_ERR 0x0000000000000080
#define CF_PARAM_SET_ERR 0x0000000000000100
#define TIM_ERR          0x0000000000000200
#define IBSS_PARAM_ERR   0x0000000000000400
#define CHALL_TEXT_ERR   0x0000000000000800
#define AID_ERR          0x0000000000001000
#define WME_IELEM_ERR    0x0000000000002000
#define WME_PARAM_ERR    0x0000000000004000
#define WME_TSPEC_ERR    0x0000000000008000
#define MGMT_ACTION_ERR  0x0000000000010000
#define QBSS_LOAD_ERR    0x0000000000020000
#define EDCA_PARAM_ERR   0x0000000000040000
#define TRAF_SPEC_ERR    0x0000000000080000
#define TRAF_CLASS_ERR   0x0000000000100000
#define SCHEDULE_ERR     0x0000000000200000
#define TS_DELAY_ERR     0x0000000000400000
#define TCLAS_PROC_ERR   0x0000000000800000
#define QOS_ACTION_ERR   0x0000000001000000
#define QOS_CAP_ERR      0x0000000002000000
#define WME_ELEM_ERR     0x0000000004000000
#define TCLAS_IELEM_ERR  0x0000000008000000
#define TCLAS_TYPE_ERR   0x0000000010000000
#define ATH_ADVCAP_ERR	 0x0000000020000000

int parse_management_frame(u_char **packetPtr,
                           struct packet_info *pInfo, int *pLen);
void print_management_frame(struct packet_info *pInfo);
void parse_beacon_frame(u_char **packetPtr, struct packet_info *pInfo,
                        int *pLen);
void parse_association_request(u_char **packetPtr,
                               struct packet_info *pInfo,
                               int *pLen);
void parse_reassociation_request(u_char **packetPtr,
                                 struct packet_info *pInfo,
                                 int *pLen);
void parse_association_response(u_char **packetPtr,
                                struct packet_info *pInfo,
                                int *pLen);
void parse_management_action(u_char **packetPtr, struct packet_info *pInfo,
                             int *pLen);
void parse_probe_response(u_char **packetPtr,
                          struct packet_info *pInfo,
                          int *pLen);
void parse_probe_request(u_char **packetPtr,
                         struct packet_info *pInfo,
                         int *pLen);
void parse_authentication(u_char **packetPtr, 
                          struct packet_info *pInfo,
                          int *pLen);
void parse_deauthentication(u_char **packetPtr,
                            struct packet_info *pInfo,
                            int *pLen);

void print_beacon_frame(struct packet_info *pInfo);
void print_association_request(struct packet_info *pInfo);
void print_reassociation_request(struct packet_info *pInfo);
void print_association_response(struct packet_info *pInfo);
void print_management_action(struct packet_info *pInfo);
void print_probe_response(struct packet_info *pInfo);
void print_probe_request(struct packet_info *pInfo);
void print_authentication(struct packet_info *pInfo);
void print_deauthentication(struct packet_info *pInfo);
void print_management_error (struct packet_info *pInfo);

#endif
