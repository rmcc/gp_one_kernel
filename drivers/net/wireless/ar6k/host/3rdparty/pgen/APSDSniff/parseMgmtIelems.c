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
#include "parseMgmtIelems.h"
#include "prism.h"
#include "wlcommon.h"

u_char OUIVAL[] = {0x00, 0x50,0xf2};
u_char ATH_OUIVAL[] = {0x00, 0x03, 0x7f};

char *actionCodes[4] = {
    "ADDTS Request",
    "ADDTS Response",
    "DELTS",
    "Reserved"
};

char *actionStatus[5] = {
    "Admission accepted",
    "Invalid parameters",
    "Reserved",
    "Refused",
    "Reserved"
};

char *tspecDirStr[4] = {
    "Uplink",
    "Downlink",
    "Reserved",
    "Bi-directional"
};

char *tspecPsbStr[2] = {
    "Legacy",
    "Triggered"
};

unsigned long long get_country_elem(u_char **packetPtr, struct packet_info *pInfo,
                                    int *pLen)
{
    unsigned long long error=0;
	u_char *ptr = *packetPtr, id;

	/*
	 * XXX: add error bit.
	 */
	
	pInfo->mgmt.ielemParsed = 0;
    if (*pLen > 0) {
		id = *ptr;
		if (id == COUNTRY_IELEM) {
			ptr++;
			pInfo->mgmt.ielemParsed = 1;
			pInfo->mgmt.country.length = *(ptr++);
			pInfo->mgmt.country.country_str[0] = *(ptr++);
			pInfo->mgmt.country.country_str[1] = *(ptr++);
			pInfo->mgmt.country.usage = *(ptr++);
			memcpy(pInfo->mgmt.country.triplets, ptr, pInfo->mgmt.country.length - 3);
			ptr += pInfo->mgmt.country.length - 3;
			*pLen -= pInfo->mgmt.country.length + 2;
		}
	}
	*packetPtr = ptr;
	return error;
}

unsigned long long get_power_constraint(u_char **packetPtr, struct packet_info *pInfo,
                                        int *pLen)
{
    unsigned long long error=0;
	u_char *ptr = *packetPtr, id, len;
	
	/*
	 * XXX: add error bit.
	 */

	pInfo->mgmt.ielemParsed = 0;
    if (*pLen >= 3) {
		id = *ptr;
		if (id == POWER_CONSTRAINT) {
			ptr++;
			pInfo->mgmt.ielemParsed = 1;
			len = *(ptr++);
			if (len != 1) {
				/* XXX: handle error case */
			}
			pInfo->mgmt.powerConstraint.length = len;
			pInfo->mgmt.powerConstraint.constraint = *(ptr++);
			*pLen -= 3;
		}
	}
	*packetPtr = ptr;
	return error;
}

unsigned long long get_power_capability(u_char **packetPtr, struct packet_info *pInfo,
                                        int *pLen)
{
    unsigned long long error=0;
	u_char *ptr = *packetPtr, id, len;
	
	/*
	 * XXX: add error bit.
	 */

	pInfo->mgmt.ielemParsed = 0;
    if (*pLen >= 4) {
		id = *ptr;
		if (id == POWER_CAPABILITY) {
			ptr++;
			pInfo->mgmt.ielemParsed = 1;
			len = *(ptr++);
			if (len != 2) {
				/* XXX: handle error case */
			}
			pInfo->mgmt.powerCapability.length = len;
			pInfo->mgmt.powerCapability.minTx = *(ptr++);
			pInfo->mgmt.powerCapability.maxTx = *(ptr++);
			*pLen -= 4;
		}
	}
	*packetPtr = ptr;
	return error;
}

unsigned long long get_chan_switch_ielem(u_char **packetPtr, struct packet_info *pInfo,
                                         int *pLen)
{
    unsigned long long error=0;
	u_char *ptr = *packetPtr, id, len;
	
	/*
	 * XXX: add error bit.
	 */

	pInfo->mgmt.ielemParsed = 0;
    if (*pLen >= 5) {
		id = *ptr;

		if (id == CHAN_SWITCH) {
			ptr++;
			pInfo->mgmt.ielemParsed = 1;
			len = *(ptr++);
			if (len != 3) {
				/* XXX: handle error case */
			}
			pInfo->mgmt.chanSwitch.length = len;

			pInfo->mgmt.chanSwitch.mode = *ptr++;
			pInfo->mgmt.chanSwitch.newChan = *ptr++;
			pInfo->mgmt.chanSwitch.count = *ptr++;
			*pLen -= 5;
		}
	}
	*packetPtr = ptr;
	return error;
}

unsigned long long get_supported_chans(u_char **packetPtr, struct packet_info *pInfo,
                                       int *pLen)
{
    unsigned long long error=0;
	u_char *ptr = *packetPtr, id, len;
	
	/*
	 * XXX: add error bit.
	 */

	pInfo->mgmt.ielemParsed = 0;
    if (*pLen >= 28) {
		id = *ptr;
		if (id == SUPPORTED_CHANS) {
			ptr++;
			pInfo->mgmt.ielemParsed = 1;
			len = *(ptr++);
			if (len != 26) {
				/* XXX: handle error case */
			}
			pInfo->mgmt.supportedChan.length = len;

			memcpy(&pInfo->mgmt.supportedChan.channelBitmap, ptr, 26);
			*pLen -= 28;
			ptr += 26;
		}
	}
	*packetPtr = ptr;
	return error;
}


unsigned long long get_ssid_elem(u_char **packetPtr, struct packet_info *pInfo,
                                 int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;
    int numToCopy=0,i;
    
    ptr = *packetPtr;
    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.ssid),0,sizeof(struct ssid_ielem));
    if (*pLen > 0) {
        id = *ptr;
        if (id == SSID_IELEM) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
            }
            pInfo->mgmt.ssid.length = len;
            if (len > 32) {
                error = SSID_ERR;
                len = 32;
                pInfo->mgmt.ssid.ssid[31]='\0';
            }
            if (len == 0) {
                strcpy(&(pInfo->mgmt.ssid.ssid[0]),"BROADCAST SSID");
                len = strlen(pInfo->mgmt.ssid.ssid);
            } else {
                numToCopy = len;
                if (numToCopy > *pLen) {
                    numToCopy = *pLen;
                }
                memcpy(&(pInfo->mgmt.ssid.ssid[0]),ptr,numToCopy);
                pInfo->mgmt.ssid.ssid[numToCopy] = '\0';
                ptr += numToCopy;
                *pLen -= numToCopy;
            }
            for (i=0; i<32; i++) {
                pInfo->mgmt.ssid.ssid[i] &= 0x7f;
                if ((pInfo->mgmt.ssid.ssid[i] < 32) &&
                    (pInfo->mgmt.ssid.ssid[i] != '\0'))
                    pInfo->mgmt.ssid.ssid[i] = 32;
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_supported_rates(u_char **packetPtr, 
                                       struct packet_info *pInfo, int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;
    int i,shift;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.suppRates),0,sizeof(struct supp_rates_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == SUPP_RATE_IELEM) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.suppRates.length = len;
                if (len > 8) {
                    error = SUPP_RATE_ERR;
                    len = 8;
                }
            }
            if (*pLen > 0) {
                for (i=0,shift=0;(i<len)&&(*pLen > 0);i++,shift+=8) {
                    pInfo->mgmt.suppRates.rates[i] = *ptr++;
                    *pLen -= 1;
                }
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_fh_param_set(u_char **packetPtr,
                                    struct packet_info *pInfo,
                                    int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.fhParamSet),0,sizeof(struct fh_param_set_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == FH_PARAM_SET) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.fhParamSet.length = len;
                if (len != 5) {
                    error = FH_PARAM_SET_ERR;
                }
            }
            get_u_short(&ptr,&(pInfo->mgmt.fhParamSet.dwellTime),pLen);
            if (*pLen > 0) {
                pInfo->mgmt.fhParamSet.hopSet = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.fhParamSet.hopPattern = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.fhParamSet.hopIndex = *ptr++;
                *pLen -= 1;
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_ds_param_set(u_char **packetPtr,
                                    struct packet_info *pInfo,
                                    int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.dsParamSet),0,sizeof(struct ds_param_set_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == DS_PARAM_SET) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.dsParamSet.length = len;
                if (len != 1) {
                    error = DS_PARAM_SET_ERR;
                }
            }
            if (*pLen > 0) {
                pInfo->mgmt.dsParamSet.currChannel = *ptr++;
                *pLen -= 1;
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_cf_param_set(u_char **packetPtr,
                                    struct packet_info *pInfo,
                                    int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.cfParamSet),0,sizeof(struct cf_param_set_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == CF_PARAM_SET) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.cfParamSet.length = len;
                if (len != 6) {
                    error = CF_PARAM_SET_ERR;
                }
            }
            if (*pLen > 0) {
                pInfo->mgmt.cfParamSet.cfpCount = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.cfParamSet.cfpPeriod = *ptr++;
                *pLen -= 1;
            }
            get_u_short(&ptr,&(pInfo->mgmt.cfParamSet.cfpMaxDur),pLen);
            get_u_short(&ptr,&(pInfo->mgmt.cfParamSet.cfpDurRemain),pLen);
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_tim(u_char **packetPtr,
                           struct packet_info *pInfo,
                           int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error =0;
    int i;
    
    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.tim),0,sizeof(struct tim_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == TIM) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.tim.length = len;
            }
            if (*pLen > 0) {
                pInfo->mgmt.tim.dtimCount = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.tim.dtimPeriod = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.tim.bitmapControl = *ptr++;
                *pLen -= 1;
            }
            len -= 3;
            for (i=0;(i<len)&&(*pLen > 0);i++) {
                pInfo->mgmt.tim.virtualBitmap[i] = *ptr++;
                *pLen -= 1;
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_ibss_param_set(u_char **packetPtr,
                                      struct packet_info *pInfo,
                                      int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.ibssParamSet),0,sizeof(struct ibss_param_set_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == IBSS_PARAM_SET) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.ibssParamSet.length = len;
                if (len != 2) {
                    error = IBSS_PARAM_ERR;
                }
            }
            get_u_short(&ptr,&(pInfo->mgmt.ibssParamSet.atimWindow),pLen);
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_challenge_text(u_char **packetPtr,
                                      struct packet_info *pInfo,
                                      int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;
    int numToCopy;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.challText),0,sizeof(struct chall_text_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == CHALL_TEXT_IELEM) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.challText.length = len;
                if ((len < 1) && (len > 253)) {
                    error = CHALL_TEXT_ERR;
                }
                if (len > 253) {
                    len = 253;
                }
            }
            numToCopy = len;
            if (numToCopy > *pLen) {
                numToCopy = *pLen;
            }
            memcpy(&(pInfo->mgmt.challText.text[0]),ptr,numToCopy);
            pInfo->mgmt.challText.text[numToCopy] = '\0';
            ptr += numToCopy;
            *pLen -= numToCopy;
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_wme_ielem(u_char **packetPtr, 
                                 struct packet_info *pInfo,
                                 int *pLen)
{
    u_char *ptr,id,type,subtype,len,oui[3];
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.wme),0,sizeof(struct wme_ielem));
    ptr = *packetPtr;
    if (*pLen >= 7) {
        id = *ptr;
        len = *(ptr+1);
        memcpy(&(oui[0]),ptr+2,3);
        type = *(ptr+5);
        subtype = *(ptr+6);
        if ((id == VENDOR_PRIVATE) && (type == WME_OUI_TYPE) &&
            (subtype == WME_IELEM_SUBTYPE)) {
            pInfo->mgmt.ielemParsed = 1;
            pInfo->mgmt.wme.length = len;
            memcpy(&(pInfo->mgmt.wme.oui[0]),&(oui[0]),3);
            pInfo->mgmt.wme.ouiType = type;
            pInfo->mgmt.wme.ouiSubtype = subtype;
            if (len != 7) {
                error = WME_IELEM_ERR;
            }
            if (memcmp(&(pInfo->mgmt.wme.oui[0]),&(OUIVAL[0]),3) != 0) {
                error = WME_IELEM_ERR;
            }
            ptr += 7;
            *pLen -= 7;
            if (*pLen > 0) {
                pInfo->mgmt.wme.version = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.wme.qosInfo = *ptr++;
                *pLen -= 1;
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

int get_ac_param_record(u_char **packetPtr,
                        struct ac_param_record *record,
                        int *pLen)
{
    u_char *ptr;
    int error = 0;

    ptr = *packetPtr;
    if (*pLen > 0) {
        record->aciByte = *ptr++;
        *pLen -= 1;
        record->aifsn = (record->aciByte) & (0x0F);
        if (record->aifsn == 0) {
            error = 1;
        }
        printf( "record->aifsn = %x\n", record->aifsn ); // palm
        record->acm = (record->aciByte >> 4) & (0x01);
        record->aci = (record->aciByte >> 5) & (0x03);
    }
    if (*pLen > 0) {
        record->ecwByte = *ptr++;
        *pLen -= 1;
        record->ecwMin = (record->ecwByte) & (0x0F);
        record->ecwMax = (record->ecwByte >> 4) & (0x0F);
    }
    get_u_short(&ptr,&(record->txop),pLen);
    *packetPtr = ptr;
    return(error);
}
    
unsigned long long get_wme_param_elem(u_char **packetPtr,
                                      struct packet_info *pInfo,
                                      int *pLen)
{
    u_char *ptr,id,type,subtype,len,oui[3];
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    ptr = *packetPtr;
    if (*pLen >= 7) {
        id = *ptr;
        len = *(ptr+1);
        memcpy(&(oui[0]),ptr+2,3);
        type = *(ptr+5);
        subtype = *(ptr+6);
        if ((id == VENDOR_PRIVATE) && (type == WME_OUI_TYPE) &&
            (subtype == WME_PARAM_SUBTYPE)) {
            pInfo->mgmt.ielemParsed = 1;
            pInfo->mgmt.wmeParam.length = len;
            memcpy(&(pInfo->mgmt.wmeParam.oui[0]),&(oui[0]),3);
            pInfo->mgmt.wmeParam.ouiType = type;
            pInfo->mgmt.wmeParam.ouiSubtype = subtype;
            if (len != 24) {
                error = WME_PARAM_ERR;
                printf( "WME_PARAM_ERROR length=%d\n", len ); // palm
            }
            if (memcmp(&(pInfo->mgmt.wmeParam.oui[0]),&(OUIVAL[0]),3) != 0) {
                error = WME_PARAM_ERR;
                printf( "WME_PARAM_ERROR memcmp %x  %x\n", (pInfo->mgmt.wmeParam.oui[0]), (OUIVAL[0]) ); // palm
            }
            ptr += 7;
            *pLen -= 7;
            if (*pLen > 0) {
                pInfo->mgmt.wmeParam.version = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.wmeParam.qosInfo = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.wmeParam.reserved = *ptr++;
                *pLen -= 1;
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.wmeParam.AC_BE),pLen)) {
                error = WME_PARAM_ERR;
                printf( "WME_PARAM_ERROR BE\n" ); // palm
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.wmeParam.AC_BK),pLen)) {
                error = WME_PARAM_ERR;
                printf( "WME_PARAM_ERROR BK\n" ); //palm
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.wmeParam.AC_VI),pLen)) {
                error = WME_PARAM_ERR;
                printf( "WME_PARAM_ERROR VI\n" ); // palm
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.wmeParam.AC_VO),pLen)) {
                error = WME_PARAM_ERR;
                printf( "WME_PARAM_ERROR VO\n" ); // palm
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_mgmt_action_frame(u_char **packetPtr,
                                         struct packet_info *pInfo,
                                         int *pLen)
{
    u_char *ptr;
    unsigned long long error=0;

    memset(&(pInfo->mgmt.mgmtAction),0,sizeof(struct mgmt_action_elem));

    ptr = *packetPtr;

    if (*pLen >0) {
        pInfo->mgmt.mgmtAction.catCode = *ptr++;
        *pLen -= 1;
        if (pInfo->mgmt.mgmtAction.catCode != MGMT_ACTION_CAT_CODE) {
            error = MGMT_ACTION_ERR;
            printf ("Mgmt ACTION ERR\n");
        }
    }
    if (*pLen > 0) {
        pInfo->mgmt.mgmtAction.actionCode = *ptr++;
        *pLen -= 1;
        if (pInfo->mgmt.mgmtAction.actionCode > 2) {
            error = MGMT_ACTION_ERR;
        }
    }
    if (*pLen > 0) {
        pInfo->mgmt.mgmtAction.dialogToken = *ptr++;
        *pLen -= 1;
        if ((!(pInfo->mgmt.isAp)) &&  // is a STA
            (pInfo->mgmt.mgmtAction.actionCode == 0) && // and is setup req
            (pInfo->mgmt.mgmtAction.dialogToken == 0)) {
            error = MGMT_ACTION_ERR;
        }
        if ((pInfo->mgmt.mgmtAction.actionCode == 2) &&
            (pInfo->mgmt.mgmtAction.dialogToken != 0)) {
            error = MGMT_ACTION_ERR;
        }
    }
    if (*pLen > 0) {
        pInfo->mgmt.mgmtAction.statusCode = *ptr++;
        *pLen -= 1;
        if ((pInfo->mgmt.mgmtAction.actionCode == 1) &&
            ((pInfo->mgmt.mgmtAction.statusCode > 3) || 
             (pInfo->mgmt.mgmtAction.statusCode == 2)))
            error = MGMT_ACTION_ERR;
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_wme_tspec_elem(u_char **packetPtr,
                                      struct packet_info *pInfo,
                                      int *pLen)
{
    u_char *ptr,id,type,subtype,len,oui[3];
    int numToCopy;
    unsigned long long error=0;
        
    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.wmeTspec),0,sizeof(struct wme_tspec_ielem));
    ptr = *packetPtr;
    if (*pLen >= 7) {
        id = *ptr;
        len = *(ptr+1);
        memcpy(&(oui[0]),ptr+2,3);
        type = *(ptr+5);
        subtype = *(ptr+6);
        if ((id == VENDOR_PRIVATE) && (type == WME_OUI_TYPE) &&
            (subtype == WME_TSPEC_SUBTYPE)) {
            pInfo->mgmt.ielemParsed = 1;
            pInfo->mgmt.wmeTspec.length = len;
            memcpy(&(pInfo->mgmt.wmeTspec.oui[0]),&(oui[0]),3);
            pInfo->mgmt.wmeTspec.ouiType = type;
            pInfo->mgmt.wmeTspec.ouiSubtype = subtype;
            if (len != 61) {
                error = WME_TSPEC_ERR;
            }
            if (memcmp(&(pInfo->mgmt.wmeTspec.oui[0]),&(OUIVAL[0]),3) != 0) {
                error = WME_TSPEC_ERR;
            }
            ptr += 7;
            *pLen -= 7;
            if (*pLen > 0) {
                pInfo->mgmt.wmeTspec.version = *ptr++;
                *pLen -= 1;
            }
            numToCopy = 3;
            if (*pLen < numToCopy) {
                numToCopy = *pLen;
            }
            memcpy(&(pInfo->mgmt.wmeTspec.tsInfo[0]),ptr,numToCopy);
            ptr += numToCopy;
            *pLen -= numToCopy;
            get_u_short(&ptr,&(pInfo->mgmt.wmeTspec.nomMSDUsize),
                        pLen);
            get_u_short(&ptr,&(pInfo->mgmt.wmeTspec.maxMSDUsize),
                        pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.minServInt),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.maxServInt),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.inactivInt),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.suspensionInt),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.servStartTime),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.minDataRate),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.meanDataRate),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.peakDataRate),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.maxBurstSize),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.delayBound),
                       pLen);
            get_u_long(&ptr,&(pInfo->mgmt.wmeTspec.minPhyRate),
                       pLen);
            get_u_short(&ptr,&pInfo->mgmt.wmeTspec.surplusBWallow,
                        pLen);
            get_u_short(&ptr,&pInfo->mgmt.wmeTspec.mediumTime,
                        pLen);
            pInfo->mgmt.wmeTspec.tid = 
                (pInfo->mgmt.wmeTspec.tsInfo[0] & 0x1E) >> 1;
            pInfo->mgmt.wmeTspec.direction = 
                (pInfo->mgmt.wmeTspec.tsInfo[0] & 0x60) >> 5;
            if (!(pInfo->mgmt.wmeTspec.tsInfo[0] & 0x80)) {
                error = WME_TSPEC_ERR;
            }
            pInfo->mgmt.wmeTspec.psb = 
                (pInfo->mgmt.wmeTspec.tsInfo[1] & 0x04) >> 2;
            pInfo->mgmt.wmeTspec.up =
                (pInfo->mgmt.wmeTspec.tsInfo[1] & 0x38) >> 3;
        } else {
            if ((id == VENDOR_PRIVATE) && 
                (memcmp(&(pInfo->mgmt.wmeTspec.oui[0]),&(OUIVAL[0]),3))) {
                printf ("Unknonwn OUI Type (%d) and Subtype: (%d) - Skipping WME Elem\n",
                        type, subtype);
                pInfo->mgmt.ielemParsed = 1;
                numToCopy = len+1;
                if (numToCopy > *pLen) {
                    numToCopy = *pLen;
                }
                ptr += numToCopy;  // Skip the rest of the Ielem
                *pLen -= numToCopy;
            }
        }
                
                
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_qbss_load_ielem(u_char **packetPtr,
                                       struct packet_info *pInfo,
                                       int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.qbssLoad),0,sizeof(struct qbss_load_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == QBSS_LOAD) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.qbssLoad.length = len;
                if (len != 5) {
                    error = QBSS_LOAD_ERR;
                }
            }
            get_u_short(&ptr,&(pInfo->mgmt.qbssLoad.stationCount),pLen);
            if (*pLen > 0) {
                pInfo->mgmt.qbssLoad.chanUtil = *ptr++;
                *pLen -= 1;
            }
            get_u_short(&ptr,&(pInfo->mgmt.qbssLoad.availAdmCap),pLen);
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_edca_param_set(u_char **packetPtr,
                                      struct packet_info *pInfo,
                                      int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.edcaParamSet),0,sizeof(struct edca_param_set_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == EDCA_PARAM_SET) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.edcaParamSet.length = len;
                if (len != 18) {
                    error = EDCA_PARAM_ERR;
                }
            }
            if (*pLen > 0) {
                pInfo->mgmt.edcaParamSet.qosInfo = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.edcaParamSet.qAck = 
                    pInfo->mgmt.edcaParamSet.qosInfo & 0x01;
                pInfo->mgmt.edcaParamSet.qReq = 
                    (pInfo->mgmt.edcaParamSet.qosInfo >> 1) & 0x01;
                pInfo->mgmt.edcaParamSet.txopReq = 
                    (pInfo->mgmt.edcaParamSet.qosInfo >> 2)& 0x01;
                pInfo->mgmt.edcaParamSet.moreDataAck = 
                    (pInfo->mgmt.edcaParamSet.qosInfo >> 3) & 0x01;
                pInfo->mgmt.edcaParamSet.edcaParamUpdateCnt = 
                    (pInfo->mgmt.edcaParamSet.qosInfo >> 4)& 0x0F;
            }
            if (*pLen > 0) {
                pInfo->mgmt.edcaParamSet.reserved = *ptr++;
                *pLen -= 1;
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.edcaParamSet.AC_BE),pLen)) {
                error = EDCA_PARAM_ERR;
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.edcaParamSet.AC_BK),pLen)) {
                error = EDCA_PARAM_ERR;
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.edcaParamSet.AC_VI),pLen)) {
                error = EDCA_PARAM_ERR;
            }
            if (get_ac_param_record(&ptr,&(pInfo->mgmt.edcaParamSet.AC_VO),pLen)) {
                error = EDCA_PARAM_ERR;
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_qos_capability(u_char **packetPtr,
                                      struct packet_info *pInfo,
                                      int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;

    pInfo->mgmt.ielemParsed = 0;
    memset(&(pInfo->mgmt.qosCap),0,sizeof(struct qos_capability_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == QOS_CAPABILITY) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.qosCap.length = len;
                if (len != 1) {
                    error = QOS_CAP_ERR;
                }
            }
            if (*pLen > 0) {
                pInfo->mgmt.qosCap.qosInfo = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.qosCap.qAck = 
                    pInfo->mgmt.qosCap.qosInfo & 0x01;
                pInfo->mgmt.qosCap.qReq = 
                    (pInfo->mgmt.qosCap.qosInfo >> 1) & 0x01;
                pInfo->mgmt.qosCap.txopReq = 
                    (pInfo->mgmt.qosCap.qosInfo >> 2)& 0x01;
                pInfo->mgmt.qosCap.moreDataAck = 
                    (pInfo->mgmt.qosCap.qosInfo >> 3) & 0x01;
                pInfo->mgmt.qosCap.edcaParamUpdateCnt = 
                    (pInfo->mgmt.qosCap.qosInfo >> 4)& 0x0F;
            }
        }
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_ath_advCapability(u_char **packetPtr,
                                         struct packet_info *pInfo,
                                         int *pLen)
{
    u_char *ptr,len=0,id,oui[3],type,subtype,version;
    unsigned long long error=0;
    
    pInfo->mgmt.ielemParsed = 0;
    ptr = *packetPtr;
    if (*pLen >= 8) {
        id = *ptr;
        len = *(ptr+1);
        memcpy(&(oui[0]),ptr+2,3);
        type = *(ptr+5);
        subtype = *(ptr+6);
        version = *(ptr+7);
        if ((id == VENDOR_PRIVATE) && (type == ATH_ADVCAP_TYPE) &&
            (subtype == ATH_ADVCAP_SUBTYPE) &&
            (version == ATH_ADVCAP_VERSION) &&
            (memcmp(&(oui[0]),&(ATH_OUIVAL[0]),3) == 0)) {
            ptr +=8;
            *pLen -= 8;
            pInfo->mgmt.ielemParsed = 1;
            pInfo->mgmt.athAdvCap.length = len;
            memcpy(&(pInfo->mgmt.athAdvCap.oui[0]),&(oui[0]),3);
            pInfo->mgmt.athAdvCap.ouiType = type;
            pInfo->mgmt.athAdvCap.ouiSubtype = subtype;
            pInfo->mgmt.athAdvCap.version = version;
            if (*pLen > 0) {
                pInfo->mgmt.athAdvCap.capability = *ptr++;
                *pLen--;
            } else {
                error = ATH_ADVCAP_ERR;
            }
            if (*pLen < 2) {
                error = ATH_ADVCAP_ERR;
            }
            get_u_short(&ptr,&pInfo->mgmt.athAdvCap.defKeyIndex,
                        pLen);
        }
    }
    *packetPtr = ptr;
    return (error);
}

unsigned long long get_tclas_ielem(u_char **packetPtr,
                                   struct packet_info *pInfo, int *pLen)
{
    u_char *ptr,id,len=0;
    unsigned long long error=0;
    int count=0;

    pInfo->mgmt.ielemParsed = 0;
    count = pInfo->mgmt.tclasCount;
    memset(&(pInfo->mgmt.tclas[count]),0,sizeof(struct tclas_ielem));
    ptr = *packetPtr;
    if (*pLen > 0) {
        id = *ptr;
        if (id == TCLAS_IELEM) {
            ptr++;
            *pLen -= 1;
            pInfo->mgmt.ielemParsed = 1;
            if (*pLen > 0) {
                len = *ptr++;
                *pLen -= 1;
                pInfo->mgmt.tclas[count].length = len;
                if (len < 1) {
                    error = TCLAS_IELEM_ERR;
                }
            }
            if (*pLen > 0) {
                pInfo->mgmt.tclas[count].userPriority = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.tclas[count].clasType = *ptr++;
                *pLen -= 1;
            }
            if (*pLen > 0) {
                pInfo->mgmt.tclas[count].clasMask = *ptr++;
                *pLen -= 1;
            }
            switch (pInfo->mgmt.tclas[count].clasType) {
            case 0:
                error |= get_type0_tclas_field(&ptr,pInfo,pLen);
                break;
            case 1:
                error |= get_type1_tclas_field(&ptr,pInfo,pLen);
                break;
            case 2:
                error |= get_type2_tclas_field(&ptr,pInfo,pLen);
                break;
            default:
                error = TCLAS_TYPE_ERR;
                skip_tclas_field(&ptr,pInfo,pLen);
                break;
            }
            pInfo->mgmt.tclasCount = count+1;
        }
    }
    *packetPtr = ptr;
    return(error);
}

void skip_tclas_field (u_char **packetPtr,
                       struct packet_info *pInfo, int *pLen)
{
    u_char *ptr;
    int numToSkip=0;

    
    ptr = *packetPtr;
    if (*pLen > 0) {
        numToSkip = pInfo->mgmt.tclas[pInfo->mgmt.tclasCount].length - 3;
    }
    if (numToSkip > 0) {
        ptr += numToSkip;
        *pLen -= numToSkip;
    }
}
	
unsigned long long get_type0_tclas_field(u_char **packetPtr, 
                                         struct packet_info *pInfo, int *pLen)
{
    u_char *ptr;
    int numToCopy=6,count;
    unsigned long long error=0;
    
    count = pInfo->mgmt.tclasCount;
    get_hw_address(packetPtr,&(pInfo->mgmt.tclas[count].type0Field.sourceAddr[0]),pLen);
    get_hw_address(packetPtr,&(pInfo->mgmt.tclas[count].type0Field.destAddr[0]),pLen);
    get_u_short(packetPtr,&(pInfo->mgmt.tclas[count].type0Field.type),pLen);
    if (pInfo->mgmt.tclas[count].length != 17) {
        error = TCLAS_TYPE_ERR;
    }
    return(error);
}

unsigned long long get_type1_tclas_field(u_char **packetPtr,
                                         struct packet_info *pInfo, int *pLen)
{
    u_char *ptr;
    int numToIncr,count;
    unsigned long long error=0;

    count = pInfo->mgmt.tclasCount;
    ptr = *packetPtr;
    if (*pLen > 0) {
        pInfo->mgmt.tclas[count].type1Field.version = *ptr++;
    }
    switch (pInfo->mgmt.tclas[count].type1Field.version) {
    case IPv4:
        error = get_ipv4_type1_tclas_params(&ptr, pInfo, pLen);
        break;
    case IPv6:
        error = get_ipv6_type1_tclas_params(&ptr, pInfo, pLen);
        break;
    default:
        error = TCLAS_TYPE_ERR;
        numToIncr = pInfo->mgmt.tclas[count].length -4;
        if (numToIncr > *pLen) {
            numToIncr = *pLen;
        }
        *pLen += numToIncr;
        ptr += numToIncr;
        break;
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_ipv4_type1_tclas_params(u_char **packetPtr,
                                               struct packet_info *pInfo,
                                               int *pLen)
{
    u_char *ptr;
    unsigned long long error=0;
    int numToCopy=4,count;

    count = pInfo->mgmt.tclasCount;
    ptr = *packetPtr;
    if (numToCopy > *pLen) {
        numToCopy = *pLen;
    }
    memcpy(&(pInfo->mgmt.tclas[count].type1Field.ipv4.sourceIPAddr[0]), ptr, 
           numToCopy);
    *pLen -= numToCopy;
    ptr += numToCopy;
    if (numToCopy > *pLen) {
        numToCopy = *pLen;
    }
    memcpy(&(pInfo->mgmt.tclas[count].type1Field.ipv4.destIPAddr[0]), ptr,
           numToCopy);
    *pLen -= numToCopy;
    ptr += numToCopy;
    get_u_short(&ptr,&(pInfo->mgmt.tclas[count].type1Field.ipv4.sourcePort),pLen);
    get_u_short(&ptr,&(pInfo->mgmt.tclas[count].type1Field.ipv4.destPort),pLen);
    if (*pLen > 0) {
        pInfo->mgmt.tclas[count].type1Field.ipv4.dscp = *ptr++;
        *pLen -= 1;
    }
    if (*pLen > 0) {
        pInfo->mgmt.tclas[count].type1Field.ipv4.protocol = *ptr++;
        *pLen -= 1;
    }
    if (*pLen > 0) {
        pInfo->mgmt.tclas[count].type1Field.ipv4.reserved = *ptr++;
        *pLen -= 1;
    }
    if (pInfo->mgmt.tclas[count].length != 19) {
        error = TCLAS_TYPE_ERR;
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_ipv6_type1_tclas_params(u_char **packetPtr,
                                               struct packet_info *pInfo,
                                               int *pLen)
{
    u_char *ptr;
    unsigned long long error=0;
    int numToCopy = 16,count;
   
    count = pInfo->mgmt.tclasCount;
    ptr = *packetPtr;
    if (numToCopy > *pLen) {
        numToCopy = *pLen;
    }
    memcpy(&(pInfo->mgmt.tclas[count].type1Field.ipv6.sourceIPAddr[0]),ptr,
           numToCopy);
    ptr += numToCopy;
    *pLen -= numToCopy;
    if (numToCopy > *pLen) {
        numToCopy = *pLen;
    }
    memcpy(&(pInfo->mgmt.tclas[count].type1Field.ipv6.destIPAddr[0]),ptr,
           numToCopy);
    ptr += numToCopy;
    *pLen -= numToCopy;
    get_u_short(&ptr,&(pInfo->mgmt.tclas[count].type1Field.ipv6.sourcePort),
                pLen);
    get_u_short(&ptr,&(pInfo->mgmt.tclas[count].type1Field.ipv6.destPort),
                pLen);
    numToCopy = 3;
    if (numToCopy > *pLen) {
        numToCopy = *pLen;
    }
    memcpy(&(pInfo->mgmt.tclas[count].type1Field.ipv6.flowLabel[0]), ptr,
           numToCopy);
    ptr += numToCopy;
    *pLen -= numToCopy;
    if (pInfo->mgmt.tclas[count].length != 43) {
        error = TCLAS_TYPE_ERR;
    }
    *packetPtr = ptr;
    return(error);
}

unsigned long long get_type2_tclas_field(u_char **packetPtr,
                                         struct packet_info *pInfo,
                                         int *pLen)
{
    get_u_short(packetPtr,&(pInfo->mgmt.tclas[pInfo->mgmt.tclasCount].qTagType8021),
                pLen);
    return(0);
}

	    
void skip_ielem(u_char **packetPtr, int *pLen)
{
    u_char *ptr,val=0;

    ptr = *packetPtr;
    if (*pLen > 0) {
        val = *ptr++;
        *pLen -= 1;  // Discard id
    }
    if (*pLen > 0) {
        val = *ptr++;
        *pLen -= 1; // get Length
        if (val > *pLen) { //lenght of ielem > total bytes left
            val = *pLen;
        }
        ptr += val;
        *pLen -= val;
    }
    *packetPtr = ptr;
}

//=================================================================

void print_country(struct packet_info *pInfo)
{
	struct country_ielem *country = &pInfo->mgmt.country;
	int i;
	if (country->length > 0) {
		char usage[16];
		switch (country->usage) {
		case 'i':
		case 'I':
			strcpy(usage, "indoor");
			break;
		case 'o':
		case 'O':
			strcpy(usage, "outdoor");
			break;
		case ' ':
			strcpy(usage, "indoor+outdoor");
			break;
		default:
			strcpy(usage, "bogus");
		}
		printf("Country IE:\n");
		printf("   Country: %c%c, Usage: %s\n", country->country_str[0],
			   country->country_str[1], usage);
		for (i = 0; i < (country->length - 4); i += 3) { /* -4 for the 2B align */
			printf("     Channel: %3d, RunLength: %2d, Power (dBm): %d\n",
				   country->triplets[i], 
				   country->triplets[i+1],
				   country->triplets[i+2]);
		}
	}
}

void print_power_constraint(struct packet_info *pInfo)
{
	struct power_constraint_ielem *pc = &pInfo->mgmt.powerConstraint;
	if (pc->length > 0) {
		printf ("Power Constraint: (%d dBm)\n", pc->constraint);
	}
}

void print_chan_switch(struct packet_info *pInfo)
{
	struct chan_switch_ielem *cs = &pInfo->mgmt.chanSwitch;
	if (cs->length > 0) {
		printf ("Channel Switch: (mode %d, newchan %d, count %d)\n", 
				cs->mode, cs->newChan, cs->count);
	}
}

void print_power_capability(struct packet_info *pInfo)
{
	struct power_capability_ielem *pc = &pInfo->mgmt.powerCapability;
	if (pc->length > 0) {
		printf ("Power Capbility: (Min %d dBm, Max %d dBm)\n", pc->minTx, pc->maxTx);
	}
}

void print_supported_chan(struct packet_info *pInfo)
{
	int i,j;
	u_char val;
	struct supported_chan_ielem *supChan = &pInfo->mgmt.supportedChan;
	if (supChan->length > 0) {
		printf("\nSupp. Channels: (");
		for (i=0; i<26; i++) {
			val = supChan->channelBitmap[i];
			for (j=0; j<8; j++) {
				if (val & (1<<(j))) {
					printf("%d ", i*8+j);
				}
			}
		}
		printf(")\n");
	}
	
}

void print_ssid(struct packet_info *pInfo)
{
    if (pInfo->mgmt.ssid.length > 0) {
        printf ("SSID: (%s) ",pInfo->mgmt.ssid.ssid);
    }
}

void print_supported_rates(struct packet_info *pInfo)
{
    int i;

    if (pInfo->mgmt.suppRates.length >0) {
        printf ("Supp. Rates= (");
        for (i=0;i<(pInfo->mgmt.suppRates.length-1); i++) {
            printf("%d ",pInfo->mgmt.suppRates.rates[i]);
        }
        printf ("%d) ",pInfo->mgmt.suppRates.rates[i]);
    }
}

void print_fh_param_set(struct packet_info *pInfo)
{
    if (pInfo->mgmt.fhParamSet.length > 0) {
        printf ("FH: dwellTime: (%d)  hopSet: (%d)  hopPatter: (%d)  hopIndex: (%d)\n",
                pInfo->mgmt.fhParamSet.dwellTime,
                pInfo->mgmt.fhParamSet.hopSet,
                pInfo->mgmt.fhParamSet.hopPattern,
                pInfo->mgmt.fhParamSet.hopIndex);
    }
}

void print_ds_param_set(struct packet_info *pInfo)
{
    if (pInfo->mgmt.dsParamSet.length > 0) {
		printf ("DS: currChannel: (%d) \n",pInfo->mgmt.dsParamSet.currChannel);
    }
}

void print_cf_param_set(struct packet_info *pInfo)
{
    if (pInfo->mgmt.cfParamSet.length > 0) {
        printf ("CF: cfpCount: (%d)  cfpPeriod:(%d)  cfpMaxDur: (%d)  cfpDurRemaining: (%d)\n",
                pInfo->mgmt.cfParamSet.cfpCount, pInfo->mgmt.cfParamSet.cfpPeriod,
                pInfo->mgmt.cfParamSet.cfpMaxDur,
                pInfo->mgmt.cfParamSet.cfpDurRemain);
    }
}

void print_tim(struct packet_info *pInfo)
{
    int i,len;

    if (pInfo->mgmt.tim.length > 0) {
        printf ("TIM: dtimCount: (%d)  dtimPeriod: (%d)  bitmapControl: (0x%02x) bcast(%d)\n",
                pInfo->mgmt.tim.dtimCount, pInfo->mgmt.tim.dtimPeriod,
                pInfo->mgmt.tim.bitmapControl,
                pInfo->mgmt.tim.bitmapControl&0x01);
        len = pInfo->mgmt.tim.length - 3;
        printf ("Partial Virtual Bitmap: (0x");
        for (i=0;i<len;i++) {
            printf ("%02x",pInfo->mgmt.tim.virtualBitmap[i]);
        }
        printf(")\n");
    }
}

void print_ibss_param_set(struct packet_info *pInfo)
{
    if (pInfo->mgmt.ibssParamSet.length > 0) {
        printf ("IBSS: ATIM Window: (%d) ",
                pInfo->mgmt.ibssParamSet.atimWindow);
    }
}

void print_challenge_text(struct packet_info *pInfo)
{
    if (pInfo->mgmt.challText.length > 0) {
        printf ("CHALL. TXT: (%s)",pInfo->mgmt.challText.text);
    }
}

void print_wme_ielem(struct packet_info *pInfo)
{
	char maxSpLimit[8];
	u_char qosInfo;

    if (pInfo->mgmt.wme.length <= 0) 
		return;

	qosInfo = pInfo->mgmt.wme.qosInfo;

	if (pInfo->mgmt.isAp) {
		printf ("WME: Ver: (%d) OUI: (%02x:%02x:%02x) Param Set Count: (%d) UAPSD: (%s)\n",
				pInfo->mgmt.wme.version,
				pInfo->mgmt.wme.oui[0], pInfo->mgmt.wme.oui[1],
				pInfo->mgmt.wme.oui[2], 
				GET_BIT_FIELD(qosInfo, WME_QOSINFO_PARAM_SET_COUNT), 
				GET_BIT_FIELD(qosInfo, WME_QOSINFO_UAPSD_EN) ? "Yes" : "No");
	}
    else {
		printf ("WME: Ver: (%d) OUI: (%02x:%02x:%02x) ",
				pInfo->mgmt.wme.version,
				pInfo->mgmt.wme.oui[0], pInfo->mgmt.wme.oui[1],
				pInfo->mgmt.wme.oui[2]);
		if (qosInfo) {
			switch (GET_BIT_FIELD(qosInfo, WME_QOSINFO_UAPSD_MAXSP)) {
			case 0:
				strcpy(maxSpLimit, "None");
				break;
			case 1:
				strcpy(maxSpLimit, "2");
				break;
			case 2:
				strcpy(maxSpLimit, "4");
				break;
			default:
				strcpy(maxSpLimit, "6");
			}
			printf ("UAPSD-ACs: ( %s%s%s%s) MaxSP Limit: (%s)\n",
					GET_BIT_FIELD(qosInfo, WME_QOSINFO_UAPSD_VO) ? "VO " : "",
					GET_BIT_FIELD(qosInfo, WME_QOSINFO_UAPSD_VI) ? "VI " : "",
					GET_BIT_FIELD(qosInfo, WME_QOSINFO_UAPSD_BK) ? "BK " : "",
					GET_BIT_FIELD(qosInfo, WME_QOSINFO_UAPSD_BE) ? "BE " : "",
					maxSpLimit);
		}
		else {
			printf ("\n");
		}
	}
}


void print_ac_param_record(struct ac_param_record *record)
{
    printf ("ACI: (%d) ACM: (%d) AIFSN: (%d) ECW_Max: (%d) ECW_Min: (%d) TxOp Lim: (%d)\n",
            record->aci, record->acm, record->aifsn, record->ecwMax,
            record->ecwMin, record->txop);
}

void print_wme_param(struct packet_info *pInfo)
{
    if (pInfo->mgmt.wmeParam.length > 0) {
		printf ("WME Param: Ver: (%d) OUI: (%02x:%02x:%02x) Param Set Count: (%d) UAPSD: (%s)\n",
				pInfo->mgmt.wmeParam.version, 
				pInfo->mgmt.wmeParam.oui[0],
				pInfo->mgmt.wmeParam.oui[1],
				pInfo->mgmt.wmeParam.oui[2],
				GET_BIT_FIELD(pInfo->mgmt.wmeParam.qosInfo, WME_QOSINFO_PARAM_SET_COUNT),
				GET_BIT_FIELD(pInfo->mgmt.wmeParam.qosInfo, WME_QOSINFO_UAPSD_EN) ? "Yes" : "No"
			);
		printf ("AC_BE: ");
		print_ac_param_record(&(pInfo->mgmt.wmeParam.AC_BE));
		printf ("AC_BK: ");
		print_ac_param_record(&(pInfo->mgmt.wmeParam.AC_BK));
		printf ("AC_VI: ");
		print_ac_param_record(&(pInfo->mgmt.wmeParam.AC_VI));
		printf ("AC_VO: ");
		print_ac_param_record(&(pInfo->mgmt.wmeParam.AC_VO));
    }
}

void print_mgmt_action(struct packet_info *pInfo)
{
    int index;
    
    index = pInfo->mgmt.mgmtAction.actionCode;
    if (index > 2) {
        index = 3;
    }
    printf ("Mgmt Action: Action Code: (%d) (%s) Dialog Token: (%d) Status Code: (%d)",
            pInfo->mgmt.mgmtAction.actionCode,
            actionCodes[index], pInfo->mgmt.mgmtAction.dialogToken,
            pInfo->mgmt.mgmtAction.statusCode);
    if (index == 1) {
        index = pInfo->mgmt.mgmtAction.statusCode;
        if (index > 3)
            index = 4;
        printf (" (%s)\n",actionStatus[index]);
    } else {
        printf ("\n");
    }
}

void print_wme_tspec_elem(struct packet_info *pInfo)
{
    if (pInfo->mgmt.wmeTspec.length > 0) {
        printf ("TSPEC: TS Info: (0x%x%x%x) nomMSDUsize: (%d) maxMSDUsize: (%d) minServInt: (%ld)\n",
                pInfo->mgmt.wmeTspec.tsInfo[0], 
                pInfo->mgmt.wmeTspec.tsInfo[1],
                pInfo->mgmt.wmeTspec.tsInfo[2],
                pInfo->mgmt.wmeTspec.nomMSDUsize,
                pInfo->mgmt.wmeTspec.maxMSDUsize, 
                pInfo->mgmt.wmeTspec.minServInt);
        printf ("       maxServInt: (%ld) SuspensionInt: (%ld) inactivInt: (%ld) servStartTime: (%ld) minDataRate: (%ld)\n",
                pInfo->mgmt.wmeTspec.maxServInt,
                pInfo->mgmt.wmeTspec.suspensionInt,
                pInfo->mgmt.wmeTspec.inactivInt,
                pInfo->mgmt.wmeTspec.servStartTime, 
                pInfo->mgmt.wmeTspec.minDataRate);
        printf ("       meanDataRate: (%ld) maxBurstSize: (%ld) minPhyRate: (%ld)\n",
                pInfo->mgmt.wmeTspec.meanDataRate, 
                pInfo->mgmt.wmeTspec.maxBurstSize,
                pInfo->mgmt.wmeTspec.minPhyRate);
        printf ("       peakDataRate: (%lu) delayBound: (%lu) surplusBWallow: (%u) mediumTime: (%u)\n",
                pInfo->mgmt.wmeTspec.peakDataRate,
                pInfo->mgmt.wmeTspec.delayBound,
                pInfo->mgmt.wmeTspec.surplusBWallow,
                pInfo->mgmt.wmeTspec.mediumTime);
        printf ("tsInfo: tid: (%u) direction: (%s:%u) psb: (%s:%u) up: (%u)\n",
                pInfo->mgmt.wmeTspec.tid,
                tspecDirStr[pInfo->mgmt.wmeTspec.direction],
                pInfo->mgmt.wmeTspec.direction,
                tspecPsbStr[pInfo->mgmt.wmeTspec.psb],
                pInfo->mgmt.wmeTspec.psb,
                pInfo->mgmt.wmeTspec.up);
    }
}

void print_qbss_load_ielem(struct packet_info *pInfo)
{
    if (pInfo->mgmt.qbssLoad.length > 0) {
        printf ("QBSS Load: Station Cnt: (%d) Chan. Util.: (%d) Avail. Adm. Cap: (%d)\n",
                pInfo->mgmt.qbssLoad.stationCount,
                pInfo->mgmt.qbssLoad.chanUtil,
                pInfo->mgmt.qbssLoad.availAdmCap);
    }
}

void print_ath_advcap(struct packet_info *pInfo)
{
    if (pInfo->mgmt.athAdvCap.length > 0) {
        printf ("Atheros Adv Cap: (0x%04x) TurboPrime: (%d) Compression: (%d) FF: (%d) XR: (%d)\n", 
                pInfo->mgmt.athAdvCap.capability,
                pInfo->mgmt.athAdvCap.capability & ATH_CAP_TURBOPRIME ? 1 : 0,
                pInfo->mgmt.athAdvCap.capability & ATH_CAP_COMPRESSION ? 1 : 0,
                pInfo->mgmt.athAdvCap.capability & ATH_CAP_FASTFRAMES ? 1 : 0,
                pInfo->mgmt.athAdvCap.capability & ATH_CAP_XR ? 1 : 0 );
        printf ("                 AR: (%d) Boost: (%d) DefKeyIndex: (0x%04x)\n",
                pInfo->mgmt.athAdvCap.capability & ATH_CAP_AR ? 1 : 0,
                pInfo->mgmt.athAdvCap.capability & ATH_CAP_BOOST ? 1 : 0,
                pInfo->mgmt.athAdvCap.defKeyIndex);
    }
}

void print_edca_param_set(struct packet_info *pInfo)
{
    if (pInfo->mgmt.edcaParamSet.length > 0) {
        printf ("EDCA Param: Q-Ack: (%d) QueueReq: (%d) TXOPreq: (%d)\n",
                pInfo->mgmt.edcaParamSet.qAck,
                pInfo->mgmt.edcaParamSet.qReq,
                pInfo->mgmt.edcaParamSet.txopReq);
        printf ("            MoreDataAck: (%d) EDCA Param Set Update Counter: (%d)\n",
                pInfo->mgmt.edcaParamSet.moreDataAck,
                pInfo->mgmt.edcaParamSet.edcaParamUpdateCnt);
        printf ("AC_BE: ");
        print_ac_param_record(&(pInfo->mgmt.edcaParamSet.AC_BE));
        printf ("AC_BK: ");
        print_ac_param_record(&(pInfo->mgmt.edcaParamSet.AC_BK));
        printf ("AC_VI: ");
        print_ac_param_record(&(pInfo->mgmt.edcaParamSet.AC_VI));
        printf ("AC_VO: ");
        print_ac_param_record(&(pInfo->mgmt.edcaParamSet.AC_VO));
    }
}

void print_qos_capability(struct packet_info *pInfo)
{
    if (pInfo->mgmt.qosCap.length > 0) {
        printf ("QOS Cap: Q-Ack: (%d) QueueReq: (%d) TXOPreq: (%d ) MoreDataAck: (%d)\n",
                pInfo->mgmt.qosCap.qAck, pInfo->mgmt.qosCap.qReq,
                pInfo->mgmt.qosCap.txopReq, pInfo->mgmt.qosCap.moreDataAck);
        printf ("            EDCA Param Set Update Counter: (%d)\n",
                pInfo->mgmt.qosCap.edcaParamUpdateCnt);
    }
}	

void print_tclas_ielem(struct packet_info *pInfo)
{
    int i;

    for (i=0;i<=pInfo->mgmt.tclasCount; i++) {
        if (pInfo->mgmt.tclas[i].length > 0) {
            printf ("TCLAS: Len: (%d) User Priority: (%d) Mask: (0x%x)\n",
                    pInfo->mgmt.tclas[i].length, pInfo->mgmt.tclas[i].userPriority,
                    pInfo->mgmt.tclas[i].clasMask);
            printf ("       Classifier Type: (%d) ",pInfo->mgmt.tclas[i].clasType);
            switch (pInfo->mgmt.tclas[i].clasType) {
            case 0:
                printf ("(Ethernet Params)\n");
                printf ("Type 0: SrcAddr: (");
                print_hw_address(&(pInfo->mgmt.tclas[i].type0Field.sourceAddr[0]));
                printf (") DestAddr: (");
                printf (") Type: (%d)\n", pInfo->mgmt.tclas[i].type0Field.type);
                print_hw_address(&(pInfo->mgmt.tclas[i].type0Field.destAddr[0]));
                break;
            case 1:
                printf ("(TCP.UDP IP Params)\n");
                print_type1_tclas_field(pInfo,i);
                break;
            case 2:
                printf ("(IEEE 802.1D/Q Params)\n");
                printf ("       802.1Q Tag Type: (0x%x)\n",
                        pInfo->mgmt.tclas[i].qTagType8021);
                break;
            default: 
                printf ("\nUnknown TCLAS Classifier Type\n");
                break;
            } 
        }
    }
}

void print_type1_tclas_field(struct packet_info *pInfo, int i)
{
    printf ("IPv%d:",pInfo->mgmt.tclas[i].type1Field.version);
    switch (pInfo->mgmt.tclas[i].type1Field.version) {
    case IPv4:
        printf (" Src IP: (");
        print_IPv4_address(&(pInfo->mgmt.tclas[i].type1Field.ipv4.sourceIPAddr[0]));
        printf (":%d) Dest IP: (",
                pInfo->mgmt.tclas[i].type1Field.ipv4.sourcePort);
        print_IPv4_address(&(pInfo->mgmt.tclas[i].type1Field.ipv4.destIPAddr[0]));
        printf (":%d) DSCP: (0x%02x) Protocol:(%d) Resv: (0x%02x)\n",
                pInfo->mgmt.tclas[i].type1Field.ipv4.destPort,
                pInfo->mgmt.tclas[i].type1Field.ipv4.dscp,
                pInfo->mgmt.tclas[i].type1Field.ipv4.protocol,
                pInfo->mgmt.tclas[i].type1Field.ipv4.reserved);
        break;
    case IPv6:
        printf (" Src IP(");
        print_IPv6_address(&(pInfo->mgmt.tclas[i].type1Field.ipv6.sourceIPAddr[0]));
        printf (") Dest IP: (");
        print_IPv6_address(&(pInfo->mgmt.tclas[i].type1Field.ipv6.destIPAddr[0]));
        printf (")\n");
        printf ("Src Port: (%d) Dest Port: (%d) Flow Label: (%02x %02x %02x)\n",
                pInfo->mgmt.tclas[i].type1Field.ipv6.sourcePort,
                pInfo->mgmt.tclas[i].type1Field.ipv6.destPort,
                pInfo->mgmt.tclas[i].type1Field.ipv6.flowLabel[0],
                pInfo->mgmt.tclas[i].type1Field.ipv6.flowLabel[1],
                pInfo->mgmt.tclas[i].type1Field.ipv6.flowLabel[2]);
        break;
    default:
        printf ("Unknown IP type (%d)\n",
                pInfo->mgmt.tclas[i].type1Field.version);
    }
}
