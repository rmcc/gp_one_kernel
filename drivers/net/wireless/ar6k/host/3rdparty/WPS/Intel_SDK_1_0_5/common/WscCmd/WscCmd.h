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
//  File Name: WscCmd.h
//  Description: Prototypes for methods implemented in WscCmd.cpp
//
****************************************************************************/

#ifndef _WSC_CMD_
#define _WSC_CMD_

#pragma pack(push, 1)

#define AP_CONF_TEMPLATE    "template.conf"
#define AP_CONF_FILENAME    "hostapd.conf"
#define SUPP_CONF_FILENAME    "config.conf"

typedef enum 
{
    LCB_QUIT = 0,
    LCB_MENU_UNCONF_AP,
    LCB_MENU_CLIENT,
    LCB_MENU_REGISTRAR,
	LCB_MENU_AP_PROXY,
    LCB_MENU_AP_PROXY_REGISTRAR,
	LCB_MENU_REQUEST_PWD
} ELCBType;

typedef struct 
{
    ELCBType	eType;
    uint32      dataLength;
} S_LCB_HEADER, S_LCB_MENU_UNCONF_AP,
    S_LCB_MENU_CLIENT, S_LCB_MENU_REGISTRAR,
    S_LCB_MENU_AP_PROXY, S_LCB_MENU_AP_PROXY_REGISTRAR;

typedef struct
{
	S_LCB_HEADER	cbHeader;
	char			deviceName[SIZE_32_BYTES];
    char			modelNumber[SIZE_32_BYTES];
    char			serialNumber[SIZE_32_BYTES];	
	uint8			uuid[SIZE_16_BYTES];
} S_LCB_MENU_REQUEST_PWD;


// ******** data ********
CMasterControl  *gp_mc;
bool            gb_apRunning, gb_suppRunning, 
				gb_regDone, gb_useUsbKey, gb_useUpnp,
				gb_writeToUsb;
EMode           ge_mode;


// callback data structures
CWscQueue    *gp_cbQ, *gp_uiQ;
uint32        g_cbThreadHandle, g_uiThreadHandle;


// ******** functions ********
// local functions
uint32 Init();
uint32 DeInit();

// callback thread functions
void * ActualCBThreadProc( IN void *p_data = NULL );
void KillCallbackThread();
void * ActualUIThreadProc( IN void *p_data = NULL );
void KillUIThread();

// callback function for CMasterControl to call in to
void CallbackProc(IN void *p_callbackMsg, IN void *p_thisObj);

// helper functions for hostapd
uint32 APCopyConfFile();
uint32 APAddParams( IN char *ssid, IN char *keyMgmt, 
					IN char *nwKey, IN uint32 nwKeyLen );
uint32 APRestartNetwork();

// helper functions for wpa_supp
uint32 SuppWriteConfFile( IN char *ssid, IN char *keyMgmt, IN char *nwKey,
							IN uint32 nwKeyLen, IN char *identity,  
							IN bool b_startWsc, IN bool b_regDone );
uint32 SuppManage( IN char *cmd );

void Start2minTimer(void);

#pragma pack(pop)
#endif
