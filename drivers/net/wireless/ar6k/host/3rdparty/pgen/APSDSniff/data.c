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
#include "wl.h"
#include "prism.h"
#include "wlcommon.h"
#include "data.h"

extern unsigned long countQoS;
extern unsigned long qosCount;
int parse_data_frame(u_char **packetPtr, struct packet_info *pInfo,
                     int *pLen)
{
    int val;

    switch(pInfo->fc.typeIndex) {
    case 18:
    case 22:
    case 26:
    case 30:
    case 50:
    case 54:
    case 58:
    case 62:  // No data in these frames
        if (*pLen > 0) {
            pInfo->data.error = NULL_DATA_ERROR;
        }
    default:
        pInfo->data.payPtr = *packetPtr;
        pInfo->data.payLen = *pLen;
        *pLen = 0;  // Skip to the end of the data
        if (countQoS) {
            if ((pInfo->fc.typeIndex == 34) || 
                (pInfo->fc.typeIndex == 38) ||
                (pInfo->fc.typeIndex == 42) ||
                (pInfo->fc.typeIndex == 46)) {
                qosCount++;
            }
        }
        break;
    }

    // BSSID filter
    if (g_filter_spec.bssid) {
        u_char *bssidAddr = NULL;
        val = (pInfo->fc.toDS << 1) | pInfo->fc.fromDS;
        switch (val) {
        case 0: 
            bssidAddr = &(pInfo->addr3[0]);
            break;
        case 1:
            bssidAddr = &(pInfo->addr2[0]);
            break;
        case 2:
            bssidAddr = &(pInfo->addr1[0]);
            break;
        }
        if (bssidAddr && memcmp(bssidAddr, &(g_filter_spec.filtBssid[0]), 6))
            return 1;
    }

    // have we mapped the ssid to bssid yet?
    if (g_filter_spec.ssid) {
        return 1;
    }
    
    return 0;
}

void print_data_error(struct packet_info *pInfo)
{
    if (pInfo->data.error & NULL_DATA_ERROR) {
        printf ("ERROR in NULL data frame, non empty payload\n");
    }
}

void print_data_frame(struct packet_info *pInfo)
{
    int val=0,numToPrint = 2048;

    val = (pInfo->fc.toDS << 1) | pInfo->fc.fromDS;
    switch (val) {
    case 0: 
        printf ("DA: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") SA: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (") BSSID: (");
        print_hw_address(&(pInfo->addr3[0]));
        printf (")\n");
        break;
    case 1:
        printf ("DA: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") BSSID: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (") SA: (");
        print_hw_address(&(pInfo->addr3[0]));
        printf (")\n");
        break;
    case 2:
        printf ("BSSID: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") SA: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (") DA: (");
        print_hw_address(&(pInfo->addr3[0]));
        printf (")\n");
        break;
    case 3:
        printf ("RA: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") TA: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (")\nDA: (");
        print_hw_address(&(pInfo->addr3[0]));
        printf (") SA: (");
        print_hw_address(&(pInfo->addr3[0]));
        printf (")\n");
        break;
    }
    if (pInfo->data.payLen < numToPrint) {
		numToPrint = pInfo->data.payLen;
    }
    pr_ether_data(pInfo->data.payPtr, numToPrint);
    print_data_error(pInfo);
    fflush(stdout);
    fflush(stderr);
}


void print_control_frame(struct packet_info *pInfo)  // palm
{
    int val=0,numToPrint = 2048;

    val = (pInfo->fc.toDS << 1) | pInfo->fc.fromDS;
    switch (val) {
    case 0: 
        printf ("DA: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") SA: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (")\n");
        break;
    case 1:
        printf ("RA: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") BSSID: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (")\n");
        break;
    case 2:
        printf ("BSSID: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") TA: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (")\n");
        break;
    case 3:
        printf ("RA: (");
        print_hw_address(&(pInfo->addr1[0]));
        printf (") TA: (");
        print_hw_address(&(pInfo->addr2[0]));
        printf (")\nDA: (");
        print_hw_address(&(pInfo->addr3[0]));
        printf (") SA: (");
        print_hw_address(&(pInfo->addr3[0]));
        printf (")\n");
        break;
    }
    if (pInfo->data.payLen < numToPrint) {
		numToPrint = pInfo->data.payLen;
    }
    pr_ether_data(pInfo->data.payPtr, numToPrint);
    print_data_error(pInfo);
    fflush(stdout);
    fflush(stderr);
}
