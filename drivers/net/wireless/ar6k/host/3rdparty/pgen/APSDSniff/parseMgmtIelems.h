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
#ifndef _PARSEMGMTIELEMS_H_
#define _PARSEMGMTIELEMS_H_

#define SSID_IELEM       0
#define SUPP_RATE_IELEM  1
#define FH_PARAM_SET     2
#define DS_PARAM_SET     3
#define CF_PARAM_SET     4
#define TIM              5
#define IBSS_PARAM_SET   6
#define COUNTRY_IELEM    7
#define QBSS_LOAD        11
#define EDCA_PARAM_SET   12
#define TRAFFIC_SPEC     13
#define TCLAS_IELEM      14
#define SCHEDULE         15
#define CHALL_TEXT_IELEM 16 /* AJP fixed. was 7. */
#define POWER_CONSTRAINT 32
#define POWER_CAPABILITY 33
#define SUPPORTED_CHANS  36
#define CHAN_SWITCH      37
#define TS_DELAY         43
#define TCLAS_PROCESSING 44
#define QOS_ACTION       45
#define QOS_CAPABILITY   46
#define VENDOR_PRIVATE   221


#define ATH_ADVCAP_TYPE 1
#define WME_OUI_TYPE 2

#define WME_IELEM_SUBTYPE 0
#define WME_PARAM_SUBTYPE 1
#define WME_TSPEC_SUBTYPE 2
#define ATH_ADVCAP_SUBTYPE 1

#define ATH_ADVCAP_VERSION 0

#define ATH_CAP_TURBOPRIME	0x0001
#define ATH_CAP_COMPRESSION	0x0002
#define ATH_CAP_FASTFRAMES	0x0004
#define ATH_CAP_XR		0x0008
#define ATH_CAP_AR		0x0010
#define ATH_CAP_BOOST		0x0080

#define MGMT_ACTION_CAT_CODE 17
#define IPv4 4
#define IPv6 61

unsigned long long get_ssid_elem(u_char **packetPtr,
				 struct packet_info *pInfo,
				 int *pLen);
unsigned long long get_supported_rates(u_char **packetPtr, 
				       struct packet_info *pInfo,
				       int *pLen);
unsigned long long get_fh_param_set(u_char **packetPtr,
				    struct packet_info *pInfo,
				    int *pLen);
unsigned long long get_ds_param_set(u_char **packetPtr,
				    struct packet_info *pInfo,
				    int *pLen);
unsigned long long get_cf_param_set(u_char **packetPtr,
				    struct packet_info *pInfo,
				    int *pLen);
unsigned long long get_tim(u_char **packetPtr,
			   struct packet_info *pInfo,
			   int *pLen);
unsigned long long get_ibss_param_set(u_char **packetPtr,
				      struct packet_info *pInfo,
				      int *pLen);
unsigned long long get_challenge_text(u_char **packetPtr,
				      struct packet_info *pInfo,
				      int *pLen);
unsigned long long get_wme_ielem(u_char **packetPtr, 
				 struct packet_info *pInfo,
				 int *pLen);
int get_ac_param_record(u_char **packetPtr,
			struct ac_param_record *record,
			int *pLen);
unsigned long long get_wme_param_elem(u_char **packetPtr,
				      struct packet_info *pInfo,
				      int *pLen);
unsigned long long get_mgmt_action_frame(u_char **packetPtr,
                                         struct packet_info *pInfo,
                                         int *pLen);
unsigned long long get_wme_tspec_elem(u_char **packetPtr,
				      struct packet_info *pInfo,
				      int *pLen);
unsigned long long get_qbss_load_ielem(u_char **packetPtr,
				       struct packet_info *pInfo,
				       int *pLen);
unsigned long long get_edca_param_set(u_char **packetPtr,
				      struct packet_info *pInfo,
				      int *pLen);
unsigned long long get_qos_capability(u_char **packetPtr,
				      struct packet_info *pInfo,
				      int *pLen);
unsigned long long get_ath_advCapability(u_char **packetPtr,
					 struct packet_info *pInfo,
					 int *pLen);
unsigned long long get_tclas_ielem(u_char **packetPtr,
				   struct packet_info *pInfo,
				   int *pLen);
unsigned long long get_type0_tclas_field(u_char **packetPtr,
					 struct packet_info *pInfo,
					 int *pLen);
unsigned long long get_type1_tclas_field(u_char **packetPtr,
					 struct packet_info *pInfo, 
					 int *pLen);
unsigned long long get_ipv4_type1_tclas_params(u_char **packetPtr,
					       struct packet_info *pInfo,
					       int *pLen);
unsigned long long get_ipv6_type1_tclas_params(u_char **packetPtr,
					       struct packet_info *pInfo,
					       int *pLen);
unsigned long long get_type2_tclas_field(u_char **packetPtr,
					 struct packet_info *pInfo,
					 int *pLen);
void skip_tclas_field (u_char **packetPtr,
		       struct packet_info *pInfo, int *pLen);
void skip_ielem(u_char **packetPtr, int *pLen);

//=============================================================

void print_ssid(struct packet_info *pInfo);
void print_country(struct packet_info *pInfo);
void print_supported_rates(struct packet_info *pInfo);
void print_fh_param_set(struct packet_info *pInfo);
void print_ds_param_set(struct packet_info *pInfo);
void print_cf_param_set(struct packet_info *pInfo);
void print_tim(struct packet_info *pInfo);
void print_ibss_param_set(struct packet_info *pInfo);
void print_challenge_text(struct packet_info *pInfo);
void print_wme_ielem(struct packet_info *pInfo);
void print_ac_param_record(struct ac_param_record *record);
void print_wme_param(struct packet_info *pInfo);
void print_mgmt_actioni(struct packet_info *pInfo);
void print_wme_tspec_elem(struct packet_info *pInfo);
void print_qbss_load_ielem(struct packet_info *pInfo);
void print_edca_param_set(struct packet_info *pInfo);
void print_qos_capability(struct packet_info *pInfo);
void print_tclas_ielem(struct packet_info *pInfo);
void print_type1_tclas_field(struct packet_info *pInfo, int i);
void print_ath_advCapability(struct packet_info *pInfo); 

#endif
