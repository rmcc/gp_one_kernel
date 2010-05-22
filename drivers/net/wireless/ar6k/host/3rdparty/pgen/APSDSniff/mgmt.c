/////////////////////////////////////////////////////////////////////
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
#include "wl.h"
#include "mgmt.h"
#include "parseMgmtIelems.h"
#include "parseMgmtFixed.h"
#include "prism.h"
#include "wlcommon.h"

int parse_management_frame(u_char **packetPtr, struct packet_info *pInfo, int *pLen)
{
    pInfo->mgmt.isAp =0;
    if (memcmp(&(pInfo->addr2[0]),&(pInfo->addr3[0]),6) == 0) { 
        // BSSID == SA
        pInfo->mgmt.isAp = 1;
    }

    switch (pInfo->fc.typeIndex) {
    case 0: // Association request
        parse_association_request(packetPtr,pInfo,pLen);
        break;
    case 4: // Association response
        parse_association_response(packetPtr,pInfo,pLen);
        break;
    case 8: // Reassociation request
        parse_reassociation_request(packetPtr,pInfo,pLen);
        break;
    case 16: // Probe Request
        parse_probe_request(packetPtr,pInfo,pLen);
        break;
    case 20: // Probe Response
        parse_probe_response(packetPtr,pInfo,pLen);
        break;
    case 32: // Beacon Frame
        parse_beacon_frame(packetPtr,pInfo,pLen);
		if ((!(pInfo->mgmt.error & SSID_ERR)) && g_filter_spec.ssid) {
			if ( strcmp(&(pInfo->mgmt.ssid.ssid[0]), g_filter_spec.filtSsid) == 0 ) {
                g_filter_spec.ssid = 0;
                g_filter_spec.bssid = 1;
                memcpy(&(g_filter_spec.filtBssid[0]), &(pInfo->addr3[0]), 6);
			}
            else {
                return 1;
            }
		}
        break;
    case 44:
        parse_authentication(packetPtr,pInfo,pLen);
        break;
    case 48:
        parse_deauthentication(packetPtr,pInfo,pLen);
        break;
    case 52: // Management Action
        parse_management_action(packetPtr,pInfo,pLen);
        break;
    default:
        printf ("ERROR - Unknown management subtype: (%d)\n",
                pInfo->fc.subtype);
        break;
    }

    if (g_filter_spec.ssid)
        return 1;

	// BSSID filter
	if (g_filter_spec.bssid && memcmp(&(pInfo->addr3[0]), &(g_filter_spec.filtBssid[0]), 6)) {
		return 1;
	}

    if (pInfo->fc.typeIndex == 32 && g_filter_spec.beaconLimit) {
        if (g_filter_spec.beaconCount >= g_filter_spec.beaconLimit)
            return 1;
        g_filter_spec.beaconCount++;
    }

	return 0;
}

void print_management_frame(struct packet_info *pInfo)
{
    int i;

    printf ("DA: (");
    print_hw_address(&(pInfo->addr1[0]));
    printf (") SA: (");
    print_hw_address(&(pInfo->addr2[0]));
    printf (") BSSID: (");
    print_hw_address(&(pInfo->addr3[0]));
    printf (")\n");

    switch (pInfo->fc.typeIndex) {
    case 0: // Association request
        print_association_request(pInfo);
        break;
    case 4: // Association response
        print_association_response(pInfo);
        break;
    case 8: // Reassociation request
        print_reassociation_request(pInfo);
        break;
    case 16: // Probe Request
        print_probe_request(pInfo);
        break;
    case 20: // Probe Response
        print_probe_response(pInfo);
        break;
    case 32: // Beacon Frame
        print_beacon_frame(pInfo);
        break;
    case 44: // Authentication frame
        print_authentication(pInfo);
        break;
    case 48: // Deauthentication frame
        print_deauthentication(pInfo);
        break;
    case 52: // Management Action
        print_management_action(pInfo);
        break;
    }
    print_management_error(pInfo);
}

void parse_beacon_frame(u_char **packetPtr, struct packet_info *pInfo,
			int *pLen)
{
    unsigned long long error = 0;
    u_char id;

    error = get_timestamp(packetPtr,pInfo,pLen);
    error |= get_beacon_interval(packetPtr,pInfo,pLen);
    error |= get_capability_info(packetPtr,pInfo,pLen);
    while (*pLen > 0 ) {
        id = **packetPtr;
        switch (id) {
        case SSID_IELEM:
            error |= get_ssid_elem(packetPtr,pInfo,pLen);
            break;
        case SUPP_RATE_IELEM:
            error |= get_supported_rates(packetPtr,pInfo,pLen);
            break;
        case FH_PARAM_SET :
            error |= get_fh_param_set(packetPtr,pInfo,pLen);
            break;
        case DS_PARAM_SET :
            error |= get_ds_param_set(packetPtr,pInfo,pLen);
            break;
        case CF_PARAM_SET :
            error |= get_cf_param_set(packetPtr,pInfo,pLen);
            break;
        case IBSS_PARAM_SET :
            error |= get_ibss_param_set(packetPtr,pInfo,pLen);
            break;
        case COUNTRY_IELEM:
            error |= get_country_elem(packetPtr, pInfo, pLen);
            break;
        case TIM :
            error |= get_tim(packetPtr,pInfo,pLen);
            break;
        case VENDOR_PRIVATE :
            error |= get_ath_advCapability(packetPtr,pInfo,pLen);
            if (pInfo->mgmt.ielemParsed )
                break;
            error |= get_wme_ielem(packetPtr,pInfo,pLen);
            if (pInfo->mgmt.ielemParsed )
                break;
            error |= get_wme_param_elem(packetPtr,pInfo,pLen);
            break;
//#if 0 /* XXX something wrong here... Cisco has length = 4 */
        case QBSS_LOAD:
            error |= get_qbss_load_ielem(packetPtr,pInfo,pLen);
            break;
//#endif
        case EDCA_PARAM_SET:
            error |= get_edca_param_set(packetPtr,pInfo,pLen);
            break;
        case QOS_CAPABILITY:
            error |= get_qos_capability(packetPtr,pInfo,pLen);
            break;
        case POWER_CONSTRAINT:
            error |= get_power_constraint(packetPtr,pInfo,pLen);
            break;
        case CHAN_SWITCH:
            error |= get_chan_switch_ielem(packetPtr, pInfo, pLen);
            break;
        default:
	  //            fprintf (stderr, "Skipping Unknown Ielem: (%d) Len: (%d) PacketLen: (%d)\n", id,*((*packetPtr)+1), *pLen);
            skip_ielem(packetPtr,pLen);
            pInfo->mgmt.ielemParsed = 1;
            break;
        }
        if (!(pInfo->mgmt.ielemParsed)) {
            skip_ielem(packetPtr,pLen);
        }
    }
    pInfo->mgmt.error = error;
}

void parse_association_request(u_char **packetPtr, 
			       struct packet_info *pInfo,
			       int *pLen)
{
    unsigned long long error = 0;
    u_char id=0;

    error = get_capability_info(packetPtr,pInfo,pLen);
    error |= get_listen_interval(packetPtr,pInfo,pLen);
    while (*pLen > 0 ) {
        id = **packetPtr;
        switch (id) {
        case SSID_IELEM :
            error |= get_ssid_elem(packetPtr,pInfo,pLen);
            break;
        case SUPP_RATE_IELEM :
            error |= get_supported_rates(packetPtr,pInfo,pLen);
            break;
        case VENDOR_PRIVATE:
            error |= get_ath_advCapability(packetPtr,pInfo,pLen);
            error |= get_wme_ielem(packetPtr,pInfo,pLen);
        case SUPPORTED_CHANS:
            error |= get_supported_chans(packetPtr,pInfo,pLen);
            break;
        case POWER_CAPABILITY:
            error |= get_power_capability(packetPtr,pInfo,pLen);
            break;
        default:
            skip_ielem(packetPtr,pLen);
            pInfo->mgmt.ielemParsed=1;
            break;
        }
        if (!(pInfo->mgmt.ielemParsed)) {
            skip_ielem(packetPtr,pLen);
        }
    }
    pInfo->mgmt.error = error;
}

void parse_management_action(u_char **packetPtr, struct packet_info *pInfo,
			     int *pLen)
{
    unsigned long long error=0;
    u_char id =0;
    error = get_mgmt_action_frame(packetPtr,pInfo,pLen);

    while (*pLen > 0) {
        id = **packetPtr;
        switch(id) {
        case VENDOR_PRIVATE:
            error |= get_wme_tspec_elem(packetPtr,pInfo,pLen);
            error |= get_wme_ielem(packetPtr,pInfo,pLen);
            error |= get_wme_param_elem(packetPtr,pInfo,pLen);
            break;
        case TCLAS_IELEM:
            error |= get_tclas_ielem(packetPtr,pInfo,pLen);
            break;
        case CHAN_SWITCH:
            error |= get_chan_switch_ielem(packetPtr, pInfo, pLen);
            break;
        default:
            skip_ielem(packetPtr,pLen);
            pInfo->mgmt.ielemParsed = 1;
            break;
        }
        if (!(pInfo->mgmt.ielemParsed)) {
            skip_ielem(packetPtr,pLen);
        }
    }
    pInfo->mgmt.error = error;
}

void parse_reassociation_request(u_char **packetPtr,
				 struct packet_info *pInfo,
				 int *pLen)
{
    unsigned long long error=0;
    u_char id;
    
    error = get_capability_info(packetPtr,pInfo,pLen);
    error |= get_listen_interval(packetPtr,pInfo,pLen);
    error |= get_curr_ap_address(packetPtr,pInfo,pLen);
    while (*pLen > 0 ) {
        id = **packetPtr;
        switch (id) {
        case SSID_IELEM :
            error |= get_ssid_elem(packetPtr,pInfo,pLen);
            break;
        case SUPP_RATE_IELEM :
            error |= get_supported_rates(packetPtr,pInfo,pLen);
            break;
        case VENDOR_PRIVATE:
            error |= get_ath_advCapability(packetPtr,pInfo,pLen);
            break;
        case SUPPORTED_CHANS:
            error |= get_supported_chans(packetPtr,pInfo,pLen);
            break;
        case POWER_CAPABILITY:
            error |= get_power_capability(packetPtr,pInfo,pLen);
            break;
        default:
            skip_ielem(packetPtr,pLen);
            pInfo->mgmt.ielemParsed = 1;
            break;
        }
        if (!(pInfo->mgmt.ielemParsed)) {
            skip_ielem(packetPtr,pLen);
        }

    }
    pInfo->mgmt.error = error;
}

void parse_association_response(u_char **packetPtr,
				struct packet_info *pInfo,
				int *pLen)
{
    unsigned long long error =0;
    u_char id;

    error = get_capability_info(packetPtr,pInfo,pLen);
    error |= get_status_code(packetPtr,pInfo,pLen);
    error |= get_aid(packetPtr,pInfo,pLen);
    while (*pLen > 0 ) {
        id = **packetPtr;
        switch (id) {
        case SUPP_RATE_IELEM :
            error |= get_supported_rates(packetPtr,pInfo,pLen);
            break;
        case VENDOR_PRIVATE:
            error |= get_wme_param_elem(packetPtr,pInfo,pLen);
            break;
        default:
            skip_ielem(packetPtr,pLen);
            pInfo->mgmt.ielemParsed = 1;
            break;
        }
        if (!(pInfo->mgmt.ielemParsed)) {
            skip_ielem(packetPtr,pLen);
        }

    }
    pInfo->mgmt.error = error;
}

void parse_probe_response(u_char **packetPtr,
			  struct packet_info *pInfo,
			  int *pLen)
{
    unsigned long long error =0;
    u_char id;

    error = get_timestamp(packetPtr,pInfo,pLen);
    error |= get_beacon_interval(packetPtr,pInfo,pLen);
    error |= get_capability_info(packetPtr,pInfo,pLen);
    while (*pLen > 0 ) {
        id = **packetPtr;
        switch (id) {
        case SSID_IELEM:
            error |= get_ssid_elem(packetPtr,pInfo,pLen);
            break;
        case SUPP_RATE_IELEM:
            error |= get_supported_rates(packetPtr,pInfo,pLen);
            break;
        case FH_PARAM_SET :
            error |= get_fh_param_set(packetPtr,pInfo,pLen);
            break;
        case DS_PARAM_SET :
            error |= get_ds_param_set(packetPtr,pInfo,pLen);
            break;
        case CF_PARAM_SET :
            error |= get_cf_param_set(packetPtr,pInfo,pLen);
            break;
        case IBSS_PARAM_SET :
            error |= get_ibss_param_set(packetPtr,pInfo,pLen);
            break;
        case QBSS_LOAD:
            error |= get_qbss_load_ielem(packetPtr,pInfo,pLen);
            break;
        case EDCA_PARAM_SET:
            error |= get_edca_param_set(packetPtr,pInfo,pLen);
            break;
        case VENDOR_PRIVATE:
            error |= get_wme_ielem(packetPtr,pInfo,pLen);
            error |= get_wme_param_elem(packetPtr,pInfo,pLen);
            error |= get_ath_advCapability(packetPtr,pInfo,pLen);
            break;
        case COUNTRY_IELEM:
            error |= get_country_elem(packetPtr, pInfo, pLen);
            break;
        case POWER_CONSTRAINT:
            error |= get_power_constraint(packetPtr,pInfo,pLen);
            break;
        default:
            skip_ielem(packetPtr,pLen);
            pInfo->mgmt.ielemParsed = 1;
            break;
        }
        if (!(pInfo->mgmt.ielemParsed)) {
            skip_ielem(packetPtr,pLen);
        }

    }
    pInfo->mgmt.error = error;
}

void parse_probe_request(u_char **packetPtr,
			 struct packet_info *pInfo,
			 int *pLen)
{
    unsigned long long error =0;
    u_char id;

    while (*pLen > 0 ) {
        id = **packetPtr;
        switch (id) {
        case SSID_IELEM:
            error |= get_ssid_elem(packetPtr,pInfo,pLen);
            break;
        case SUPP_RATE_IELEM:
            error |= get_supported_rates(packetPtr,pInfo,pLen);
            break;
        case VENDOR_PRIVATE:
            error |= get_ath_advCapability(packetPtr,pInfo,pLen);
            break;
        default:
            skip_ielem(packetPtr,pLen);
            pInfo->mgmt.ielemParsed = 1;
            break;
        }
        if (!(pInfo->mgmt.ielemParsed)) {
            skip_ielem(packetPtr,pLen);
        }

    }
    pInfo->mgmt.error = error;
}

void parse_authentication(u_char **packetPtr, 
			  struct packet_info *pInfo,
			  int *pLen)
{
    unsigned long long error = 0;

    error = get_auth_alg_num(packetPtr, pInfo, pLen);
    error |= get_auth_trans_seq_num(packetPtr, pInfo, pLen);
    error |= get_status_code(packetPtr, pInfo, pLen);
    error |= get_challenge_text(packetPtr, pInfo, pLen);
    pInfo->mgmt.error = error;
}

void parse_deauthentication(u_char **packetPtr,
			    struct packet_info *pInfo,
			    int *pLen)
{
    unsigned long long error =0;
    error = get_reason_code(packetPtr, pInfo, pLen);
    pInfo->mgmt.error = error;
}

void print_beacon_frame(struct packet_info *pInfo)
{
    print_timestamp(pInfo);
    print_beacon_interval(pInfo);
    print_ssid(pInfo);
    printf ("\n");
    print_capability_info(pInfo);
    print_supported_rates(pInfo);
    printf ("\n");
    print_fh_param_set(pInfo);
    print_ds_param_set(pInfo);
	print_country(pInfo);
	print_power_constraint(pInfo);
	print_chan_switch(pInfo);
    print_cf_param_set(pInfo);
    print_ibss_param_set(pInfo);
    print_tim(pInfo);
    print_wme_ielem(pInfo);
    print_wme_param(pInfo);
    print_qbss_load_ielem(pInfo);
    print_edca_param_set(pInfo);
    print_qos_capability(pInfo);
    print_ath_advcap(pInfo);
}

void print_association_request(struct packet_info *pInfo)
{
    print_capability_info(pInfo);
    print_listen_interval(pInfo);
    print_ssid(pInfo);
    print_supported_rates(pInfo);
	print_supported_chan(pInfo);
    print_ath_advcap(pInfo);
	print_power_capability(pInfo);
	printf("\n");
    print_wme_ielem(pInfo);
}

void print_reassociation_request(struct packet_info *pInfo)
{
    print_capability_info(pInfo);
    print_listen_interval(pInfo);
    print_current_ap_address(pInfo);
    print_ssid(pInfo);
    print_supported_rates(pInfo);
	print_supported_chan(pInfo);
    print_ath_advcap(pInfo);
	print_power_capability(pInfo);
	printf("\n");
    print_wme_ielem(pInfo);
}

void print_association_response(struct packet_info *pInfo)
{
    print_capability_info(pInfo);
    print_status_code(pInfo);
    print_aid(pInfo);
    print_supported_rates(pInfo);
    printf ("\n");
    print_wme_param(pInfo);
    print_ath_advcap(pInfo);
}

void print_management_action (struct packet_info *pInfo)
{
    print_mgmt_action(pInfo);
	print_chan_switch(pInfo);
    print_wme_ielem(pInfo);
    print_wme_param(pInfo);
    print_wme_tspec_elem(pInfo);
    print_tclas_ielem(pInfo);
}

void print_probe_response(struct packet_info *pInfo)
{
    print_timestamp(pInfo);
    print_beacon_interval(pInfo);
    print_ssid(pInfo);
    printf ("\n");
    print_capability_info(pInfo);
    print_supported_rates(pInfo);
    printf ("\n");
	print_country(pInfo);
	print_power_constraint(pInfo);
    print_fh_param_set(pInfo);
    print_ds_param_set(pInfo);
    print_cf_param_set(pInfo);
    print_ibss_param_set(pInfo);
    printf("\n");
    print_wme_ielem(pInfo);
    print_wme_param(pInfo);
    print_qbss_load_ielem(pInfo);
    print_edca_param_set(pInfo);
    print_ath_advcap(pInfo);
}

void print_probe_request(struct packet_info *pInfo)
{
    print_ssid(pInfo);
    print_supported_rates(pInfo);
    printf ("\n");
    print_ath_advcap(pInfo);
}

void print_authentication (struct packet_info *pInfo)
{
    print_auth_alg_num(pInfo);
    print_auth_trans_seq_num(pInfo);
    print_status_code(pInfo);
    printf("\n");
    print_challenge_text(pInfo);
    printf("\n");
}

void print_deauthentication (struct packet_info *pInfo)
{
    print_reason_code(pInfo);
    printf("\n");
}

void print_management_error (struct packet_info *pInfo)
{
    unsigned long long value;

    value = pInfo->mgmt.error;
    if (value & AUTH_ALG_ERR) {
        printf ("ERROR in Authenticaion Algorithm Number field\n");
    }
    if (value & CAP_INFO_ERR) {
        printf ("ERROR in Capability Information field\n");
    }
    if (value & REASON_CODE_ERR) {
        printf ("ERROR in Reason Code field\n");
    }
    if (value & STATUS_CODE_ERR) {
        printf ("ERROR in Status Code field\n");
    }
    if (value & SSID_ERR) {
        printf ("ERROR in SSID information element\n");
    }
    if (value & SUPP_RATE_ERR) {
        printf ("ERROR in Supported Rates information element\n");
    }
    if (value & FH_PARAM_SET_ERR) {
        printf ("ERROR in FH Parameter Set information element\n");
    }
    if (value & DS_PARAM_SET_ERR) {
        printf ("ERROR in DS Parameter Set information element\n");
    }
    if (value & CF_PARAM_SET_ERR) {
        printf ("ERROR in CF Parameter Set information element\n");
    }
    if (value & TIM_ERR) {
        printf ("ERROR in TIM information element\n");
    }
    if (value & IBSS_PARAM_ERR) {
        printf ("ERROR in IBSS Parameter Set information element\n");
    }
    if (value & CHALL_TEXT_ERR) {
        printf ("ERROR in the Challenge Text information element\n");
    }
    if (value & AID_ERR) {
        printf ("ERROR in AID field\n");
    }
    if (value & WME_IELEM_ERR) {
        printf ("ERROR in WME information element\n");
    }
    if (value & WME_PARAM_ERR) {
        printf ("ERROR in WME Parameter information element\n");
    }
    if (value & WME_TSPEC_ERR) {
        printf ("ERROR in WME TSPEC information element\n");
    }
    if (value & MGMT_ACTION_ERR) {
        printf ("ERROR in Management Action information element\n");
    }
    if (value & QBSS_LOAD_ERR) {
        printf ("ERROR in QBSS Load information element\n");
    }
    if (value & EDCA_PARAM_ERR) {
        printf ("ERROR in EDCA Parameter Set information element\n");
    }
    if (value & QOS_CAP_ERR) {
        printf ("ERROR in QoS Capability information element\n");
    }
    if (value & ATH_ADVCAP_ERR) {
        printf ("ERROR in Atheros Advanced Capability information element\n");
    }
}
