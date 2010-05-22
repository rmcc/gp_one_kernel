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
//  File Name: WscCmd.cpp
//  Description: Implements a basic command-line UI for the WSC stack.
//      Instantiates the Master Control module, and then provides a basic
//      interface between the user and the Master Control.
//
****************************************************************************/

#ifdef WIN32
#include <windows.h>
#endif // WIN32

#include <stdio.h>
#include <memory>        // for auto_ptr
#include <signal.h>

#include <openssl/bn.h>
#include <openssl/dh.h>

#include "WscHeaders.h"
#include "WscCommon.h"
#include "StateMachineInfo.h"
#include "WscError.h"
#include "Portability.h"
#include "WscQueue.h"
#include "Transport.h"
#include "StateMachine.h"
#include "RegProtocol.h"
#include "tutrace.h"
#include "slist.h"
#include "Info.h"
#include "MasterControl.h"
#include "WscCmd.h"

#define WSC_VERSION_STRING	"Build 1.0.5, November 17 2006"

char inf[16]="eth1";

/*
 * Name        : main
 * Description : Main entry point for the WSC stack
 * Arguments   : int argc, char *argv[] - command line parameters
 * Return type : int
 */
int
main(int argc, char* argv[])
{
    uint32 i_ret = 0;
    char choice[2];

    system("clear");

    printf("\E[1;37;44m");
    printf( "                                                \n" );
    printf( "  Wi-Fi Simple Config - Atheros Communications  \n" );
    printf( "                                                \n" );
    printf("\033[0m");
    //printf( "*************************************************************\n" );
    //printf( "  Wi-Fi Simple Config Application - Atheros Communications.  \n" );
    //printf( "  Version: %s\n", WSC_VERSION_STRING );
    //printf( "*************************************************************\n" );
    //TUTRACE((TUTRACE_INFO, "Version: %s\n", WSC_VERSION_STRING));

    if(argv[1] && (strcmp(argv[1],"-i")==0))
    {
         if(argv[2])
	 	strcpy(inf, argv[2]);
    }
    TUTRACE((TUTRACE_INFO, "Using Interface: %s\n",inf));

    //printf("Options: \n");
    //printf("1. Join network       -Client \n");
    //printf("2. Configure network  -Registrar\n");
    //printf("3. Add another device -Registrar\n");
    //printf( "Enter selection: " );
    //fgets( choice, 3, stdin );
    //fflush( stdin );

    choice[0] = '1'; // we now support only Enrollee
    switch(choice[0])
    {
    case '1' :
	system("cp -f wsc_config.txt.sta wsc_config.txt");
	break;
    case '2' :
    case '3' :
	system("cp -f wsc_config.txt.reg wsc_config.txt");
	break;
    default :
	printf("Error in input\n");
	return 0;
    }

    TUTRACE((TUTRACE_INFO, "Initializing stack..." ));
    i_ret = Init();
    if ( WSC_SUCCESS == i_ret )
    {
        TUTRACE((TUTRACE_INFO, " OK\n"));
        TUTRACE((TUTRACE_INFO, "Now starting stack\n"));
        TUTRACE((TUTRACE_INFO, "WscCmd::Init ok, starting stack...\n"));
        strcpy(gp_mc->mp_info->inf,inf);
        i_ret = gp_mc->StartStack();
        if ( WSC_SUCCESS == i_ret )
        {
            TUTRACE((TUTRACE_INFO, "WscCmd:: Stack started ok\n"));
            // Wait until the UI thread dies
            // Note: this thread will die when the user
            // selects the "quit" option
#ifdef __linux__
            WscDestroyThread( g_uiThreadHandle );
#else
            // Call WaitForSingleObject rather than WscDestroyThread as
            // the latter only waits for 2000ms
            WaitForSingleObject( (HANDLE)g_uiThreadHandle, INFINITE );
#endif
            // Now stop the stack
            gp_mc->StopStack();
        } // StartStack ok
        else
        {

            printf( "Unable to start the stack\n" );
            printf( "Hit enter to quit application\n" );
            char inp[2];
            fgets( inp, 2, stdin );
            fflush( stdin );
            KillUIThread();
            WscDestroyThread( g_uiThreadHandle );
        }

        // Stop wpa_supplicant/hostapd if running
        if ( gb_apRunning )
        {
            APRestartNetwork();
            gb_apRunning = false;
        }
        if ( gb_suppRunning )
        {
            SuppManage( "TERMINATE" );
            gb_suppRunning = false;
        }

        DeInit();
    }
    else
    {
        printf( " FAILED\n" );
        TUTRACE((TUTRACE_ERR, "WscCmd::Init failed\n"));
        printf( "Hit enter to quit application\n" );
        char inp[2];
        fgets( inp, 2, stdin );
        fflush( stdin );
    }

    printf( "\nAll shut down, ready to exit\n" );
    return 0;
} // main

/*
 * Name        : Init
 * Description : Initialize member variables
 * Arguments   : none
 * Return type : uint32 - result of the initialize operation
 */
uint32
Init()
{
    int i_ret;

    // Basic initialization
    gp_mc				= NULL;
    gb_apRunning        = false;
    gb_suppRunning      = false;
    gb_regDone          = false;
	gb_useUsbKey		= false;
	gb_useUpnp			= false;
    gp_cbQ              = NULL;
    gp_uiQ              = NULL;
    g_cbThreadHandle    = 0;
    g_uiThreadHandle    = 0;

    try
    {
        // create callback queue
        gp_cbQ = new CWscQueue();
        if ( !gp_cbQ )
            throw "WscCmd::Init: cbQ not created";
        gp_cbQ->Init();

        // create callback thread
        i_ret = WscCreateThread(
                        &g_cbThreadHandle,
                        ActualCBThreadProc,
                        NULL );
        if ( WSC_SUCCESS != i_ret )
            throw "WscCmd::Init: cbThread not created";
        WscSleep( 1 );

        // create UI queue
        gp_uiQ = new CWscQueue();
        if ( !gp_uiQ )
            throw "WscCmd::Init: uiQ not created";
        gp_uiQ->Init();

        // create UI thread
        i_ret = WscCreateThread(
                        &g_uiThreadHandle,
                        ActualUIThreadProc,
                        NULL );
        if ( WSC_SUCCESS != i_ret )
            throw "WscCmd::Init: uiThread not created";
        WscSleep( 1 );

        // initialize MasterControl
        gp_mc = new CMasterControl();
        if ( gp_mc )
        {
            TUTRACE((TUTRACE_INFO, "WscCmd::MC instantiated ok\n"));
            if ( WSC_SUCCESS == gp_mc->Init( CallbackProc, NULL ) )
                TUTRACE((TUTRACE_INFO, "WscCmd::MC intialized ok\n"));
            else
                throw "WscCmd::Init: MC initialization failed\n";
        }
        else
        {
            throw "WscCmd::Init: MC instantiation failed\n";
        }

        // Everything's initialized ok
        // Transfer control to member variables
    }
    catch ( char *err )
    {
        TUTRACE((TUTRACE_ERR, "WscCmd::Init failed\n %s\n", err));
        if ( gp_mc )
        {
            gp_mc->DeInit();
            delete gp_mc;
        }
        if ( g_uiThreadHandle )
        {
            KillUIThread();
            WscDestroyThread( g_uiThreadHandle );
        }
        if ( gp_uiQ )
        {
            gp_uiQ->DeInit();
            delete gp_uiQ;
        }
        if ( g_cbThreadHandle )
        {
            KillCallbackThread();
            WscDestroyThread( g_cbThreadHandle );
        }
        if ( gp_cbQ )
        {
            gp_cbQ->DeInit();
            delete gp_cbQ;
        }

        return WSC_ERR_SYSTEM;
    }

    return WSC_SUCCESS;
} // Init

/*
 * Name        : DeInit
 * Description : Deinitialize member variables
 * Arguments   : none
 * Return type : uint32
 */
uint32
DeInit()
{
    if ( gp_mc )
	{
        gp_mc->DeInit();
	}
    TUTRACE((TUTRACE_INFO, "WscCmd::MC deinitialized\n"));

    // kill the callback thread
    // gp_cbQ deleted in the callback thread
    KillCallbackThread();
    WscDestroyThread( g_cbThreadHandle );

    return WSC_SUCCESS;
} // DeInit


uint32
StartSupplicant( IN char *ssid, IN char *keyMgmt, IN char *nwKey,
                            IN uint32 nwKeyLen, IN char *identity,
							IN bool b_startWsc, IN bool b_regDone )
{
	TUTRACE((TUTRACE_INFO, "WscCmd:StartSupplicant:\n"));

	if ( gb_suppRunning )
	{
		TUTRACE((TUTRACE_ERR, "WscCmd:ActualCBThreadProc: "
					"STARTSUPP: supp already running\n"));
		return WSC_SUCCESS;
	}

	// write the config file
	if ( WSC_SUCCESS != SuppWriteConfFile(
				ssid, keyMgmt, nwKey, nwKeyLen, identity, b_startWsc, b_regDone ))
	{
		TUTRACE((TUTRACE_ERR, "WscCmd:ActualCBThreadProc: "
					"STARTSUPP: Could not write config\n"));
		return WSC_ERR_FILE_WRITE;
	}

#ifdef __linux__
	// Do a fork() to run wpa_supplicant
	pid_t childPid;
	childPid = fork();
	if ( childPid >= 0 )
	{
		// fork succeeded
		if ( 0 == childPid )
		{
			char buf[96];
			// child process
			// Start with Atheros drive
			sprintf(buf, "./wpa_supplicant -i %s -c config.conf > supplicant.log", inf );
			system( buf );
			exit( 0 );
		}
		else
		{
			// parent process
			// do nothing further
			sleep( 3 );
			TUTRACE((TUTRACE_INFO, "WscCmd:ActualCBThreadProc: "
						"STARTSUPP: After fork\n"));
		}
	}
	else
	{
		// fork failed
		TUTRACE((TUTRACE_ERR, "WscCmd:ActualCBThreadProc: "
					"STARTSUPP: Could not start supp\n"));
		return WSC_ERR_SYSTEM;
	}

	gb_suppRunning = true;
	TUTRACE((TUTRACE_INFO,
				"WscCmd:ActualCBThreadProc: STARTSUPP: Started\n"));
#else
	// Do CreateProcess() to run wpa_supplicant
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory( &si, sizeof(STARTUPINFO) );
	si.cb = sizeof( STARTUPINFO );
	ZeroMemory( &pi, sizeof(pi) );

	// Example:
	// CreateProcess(NULL,
	//            "\"C:\\Program Files\\MyApp.exe\" -L -S", ...)
	if( !CreateProcess(
				NULL,            // No module name (use command line).
				"wpa_supplicant.exe -i Intel -c config.conf",
				// Command line.
				NULL,           // Process handle not inheritable.
				NULL,           // Thread handle not inheritable.
				FALSE,          // Set handle inheritance to FALSE.
				CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
				// Creation flags.
				NULL,           // Use parent's environment block.
				NULL,           // Use parent's starting directory.
				&si,            // Pointer to STARTUPINFO structure.
				&pi )           // Pointer to
			// PROCESS_INFORMATION structure.
	  )
	{
		gb_suppRunning = false;
		TUTRACE((TUTRACE_INFO,
					"WscCmd:ActualCBThreadProc: STARTSUPP: Failed\n"));
	}
	else
	{
		gb_suppRunning = true;
		TUTRACE((TUTRACE_INFO,
					"WscCmd:ActualCBThreadProc: STARTSUPP: Started\n"));
	}
#endif
	return WSC_SUCCESS;
}

/*
 * Name        : ActualCBThreadProc
 * Description : This is the thread procedure for the callback thread.
 *                 Monitor the callbackQueue, and process all callbacks that
 *                 are received.
 * Arguments   : void *p_data = NULL
 * Return type : void *
 */
void *
ActualCBThreadProc( IN void *p_data )
{
    bool    b_done = false;
    uint32  h_status;
    void    *p_cbData;
    S_CB_HEADER *p_header;

    TUTRACE((TUTRACE_INFO, "WscCmd:ActualCBThreadProc: Started\n"));
    // keep doing this until the thread is killed
    while ( !b_done )
    {
        // block on the callbackQueue
        h_status = gp_cbQ->Dequeue(
                            NULL,        // size of dequeued msg
                            1,            // sequence number
                            0,            // infinite timeout
                            (void **) &p_cbData);
                                        // pointer to the dequeued msg

        if ( WSC_SUCCESS != h_status )
        {
            // something went wrong
            TUTRACE((TUTRACE_ERR, "WscCmd:ActualCBThreadProc: "
                        "Error in Dequeue!\n"));
            b_done = true;
            break; // from while loop
        }

        p_header = (S_CB_HEADER *)p_cbData;

        // once we get something, parse it,
        // do whats necessary, and then block
        switch( p_header->eType )
        {
            case CB_QUIT:
            {
                // no params
                // destroy the queue
                if ( gp_cbQ )
                {
                    gp_cbQ->DeInit();
                    delete gp_cbQ;
                }
                // kill the thread
                b_done = true;
                break;
            }

            case CB_MAIN_PUSH_MSG:
            {
                TUTRACE((TUTRACE_INFO,
                    "WscCmd:ActualCBThreadProc: CB_MAIN_PUSH_MSG recd\n"));

				// Display the message
				S_CB_MAIN_PUSH_MSG *p = (S_CB_MAIN_PUSH_MSG *)p_cbData;
				printf( "%s\n", p->c_msg );

                break;
            }

			case CB_MAIN_PUSH_REG_RESULT:
			{
				TUTRACE((TUTRACE_INFO,
                    "WscCmd:ActualCBThreadProc: "
					"CB_MAIN_PUSH_REG_RESULT recd\n"));

				S_CB_MAIN_PUSH_REG_RESULT *p =
						(S_CB_MAIN_PUSH_REG_RESULT *)p_cbData;

                // Check whether the SM returned TRUE or FALSE from
                // the registration process
				if ( p->b_result )
				{
					printf( "WSC registration protocol successfully"
							" completed\n" );
					TUTRACE((TUTRACE_INFO, "WSC registration protocol successfully"
							" completed\n"));

				}
				else
				{
					printf( "WSC registration protocol did not"
							" successfully complete\n" );
					TUTRACE((TUTRACE_INFO, "WSC registration protocol did not"
							" successfully complete\n"));
				}

                gb_regDone = true;


                // Post a message to the UI thread
                switch( ge_mode )
                {
                    case EModeUnconfAp:
                    {
                        // Display menu options for AP
                        S_LCB_MENU_UNCONF_AP *p = new S_LCB_MENU_UNCONF_AP;
                        p->eType = LCB_MENU_UNCONF_AP;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue( sizeof(S_LCB_MENU_UNCONF_AP),
                                            2, p );
                        break;
                    }

                    case EModeClient:
                    {
                        // Display menu options for Client
                        S_LCB_MENU_CLIENT *p = new S_LCB_MENU_CLIENT;
                        p->eType = LCB_MENU_CLIENT;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue( sizeof(S_LCB_MENU_CLIENT),
                                            2, p );
                        break;
                    }

                    case EModeRegistrar:
                    {
                        // Display menu options for Registrar
                        S_LCB_MENU_REGISTRAR *p =
                                        new S_LCB_MENU_REGISTRAR;
                        p->eType = LCB_MENU_REGISTRAR;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue( sizeof(S_LCB_MENU_REGISTRAR),
                                            2, p );
                        break;
                    }

                    case EModeApProxyRegistrar:
                    {
                        // Display menu options for Registrar
                        S_LCB_MENU_AP_PROXY_REGISTRAR *p =
                                        new S_LCB_MENU_AP_PROXY_REGISTRAR;
                        p->eType = LCB_MENU_AP_PROXY_REGISTRAR;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue(
								sizeof(S_LCB_MENU_AP_PROXY_REGISTRAR),
                                2, p );
                        break;
                    }

					case EModeApProxy:
					{
						// TODO
						break;
					}

					default:
					{
						break;
					}
                } // switch
				break;
			}

			case CB_MAIN_REQUEST_PWD:
			{
				TUTRACE((TUTRACE_INFO,
                    "WscCmd:ActualCBThreadProc: "
					"CB_MAIN_REQUEST_PWD recd\n"));

				S_CB_MAIN_REQUEST_PWD *p =
						(S_CB_MAIN_REQUEST_PWD *)p_cbData;

				// Post message to UI thread to display peer info and
				// request password
				S_LCB_MENU_REQUEST_PWD *p_cb =
									new S_LCB_MENU_REQUEST_PWD;
				memcpy( p_cb, p, sizeof(S_LCB_MENU_REQUEST_PWD) );
				p_cb->cbHeader.eType = LCB_MENU_REQUEST_PWD;
				gp_uiQ->Enqueue( sizeof(S_LCB_MENU_REQUEST_PWD),
                                2, p_cb );
				break;
			}

            case CB_MAIN_START_AP:
            {
                // NOTE: AP code is intended to be run only on Linux
#ifdef __linux__
                uint32 ret;
                TUTRACE((TUTRACE_INFO,
                    "WscCmd:ActualCBThreadProc: CB_MAIN_START_AP recd\n"));
                S_CB_MAIN_START_AP *p = (S_CB_MAIN_START_AP *)p_cbData;

                if ( gb_apRunning && !p->b_restart )
                {
                    TUTRACE((TUTRACE_ERR,
                        "WscCmd:ActualCBThreadProc: STARTAP: "
                        "AP already running\n"));
                    break;
                }

                // Re-write the config file
                if ( WSC_SUCCESS != APCopyConfFile() )
                {
                    TUTRACE((TUTRACE_INFO,
                        "WscCmd:ActualCBThreadProc: STARTAP: "
                        "Cannot copy file\n"));
                    break;
                }

                if ( p->b_configured )
                {
                    ret = APAddParams( p->ssid, p->keyMgmt,
											p->nwKey, p->nwKeyLen );
                }
                else
                {
                    ret = APAddParams( p->ssid, p->keyMgmt, NULL, 0 );
                }
                if ( WSC_SUCCESS != ret )
                {
                    TUTRACE((TUTRACE_INFO,
                        "WscCmd:ActualCBThreadProc: STARTAP: "
                        "Cannot write params\n"));
                    break;
                }

                if ( p->b_restart )
                {
                    APRestartNetwork();
                }

                // Now start hostapd
                // Do a fork() to run hostapd
                pid_t childPid;
                childPid = fork();
                if ( childPid >= 0 )
                {
                    // fork succeeded
                    if ( 0 == childPid )
                    {
                        // child process
                        system( "xterm -e \"./hostapd hostapd.conf\"" );
                        exit( 0 );
                    }
                    else
                    {
                        // parent process
                        // do nothing further
                        TUTRACE((TUTRACE_INFO,
                            "WscCmd:ActualCBThreadProc: STARTAP: "
                            "After fork\n"));
                    }
                }
                else
                {
                    // fork failed
                    TUTRACE((TUTRACE_ERR,
                        "WscCmd:ActualCBThreadProc: STARTAP: "
                        "Could not start hostap\n"));
                    break;
                }

                gb_apRunning = true;
                TUTRACE((TUTRACE_INFO, "WscCmd: AP started\n"));
#else
                TUTRACE((TUTRACE_ERR, "WscCmd: AP code not implemented "
                            "for WinXP\n"));
#endif
                break;
            }

            case CB_MAIN_STOP_AP:
            {
                // NOTE: AP code is intended to be run only on Linux
#ifdef __linux__
                if ( gb_apRunning )
                {
                    APRestartNetwork();
                    gb_apRunning = false;
                }
#else
                TUTRACE((TUTRACE_ERR, "WscCmd: AP code not implemented "
                            "for WinXP\n"));
#endif
                break;
            }

            case CB_MAIN_START_WPASUPP:
            {
		TUTRACE((TUTRACE_INFO, "WscCmd:ActualCBThreadProc: "
				"CB_MAIN_START_WPASUPP recd\n"));
		StartSupplicant("JunkAP", "WPA-PSK", "abcdefgh", 10, NULL, false, false);
                break;
            }

            case CB_MAIN_RESET_WPASUPP:
            {
                TUTRACE((TUTRACE_INFO, "WscCmd:ActualCBThreadProc: "
                            "CB_MAIN_RESET_WPASUPP recd\n"));

                S_CB_MAIN_RESET_WPASUPP *p =
                        (S_CB_MAIN_RESET_WPASUPP *)p_cbData;

                // re-write the config file
                if ( p->b_startWsc )
                    SuppWriteConfFile( p->ssid, p->keyMgmt, p->nwKey,
							p->nwKeyLen, p->identity, p->b_startWsc, p->b_regDone );
                else
                    SuppWriteConfFile( p->ssid, p->keyMgmt, p->nwKey,
							p->nwKeyLen, NULL, p->b_startWsc, p->b_regDone );

                if ( !gb_suppRunning )
                {
                    TUTRACE((TUTRACE_ERR, "WscCmd:ActualCBThreadProc: "
                                "RESETSUPP: supp not running\n"));
					StartSupplicant(p->ssid, p->keyMgmt, p->nwKey,
							p->nwKeyLen, NULL, p->b_startWsc, p->b_regDone);
                    TUTRACE((TUTRACE_INFO,
                         "WscCmd:ActualCBThreadProc: RESETSUPP: Started\n"));
                }
				else {
					// connect to the supp and pass message
					SuppManage( "RECONFIGURE" );
					TUTRACE((TUTRACE_INFO, "WscCmd::ActualCBThreadProc: "
								"RESETSUPP: reconfigured\n"));
				}
                break;
            }

            case CB_MAIN_STOP_WPASUPP:
            {
                TUTRACE((TUTRACE_INFO, "WscCmd:ActualCBThreadProc: "
                            "CB_MAIN_RESET_WPASUPP recd\n"));

                if ( !gb_suppRunning )
                {
                    TUTRACE((TUTRACE_ERR, "WscCmd:ActualCBThreadProc: "
                                "STOPSUPP: supp not running\n"));
                    break;
                }

                // connect to the supp and pass message
                SuppManage( "TERMINATE" );
                gb_suppRunning = false;
                TUTRACE((TUTRACE_INFO, "WscCmd::ActualCBThreadProc: "
                            "RESETSUPP: Supp stopped\n"));
                break;
            }

            case CB_MAIN_PUSH_MODE:
            {
                TUTRACE((TUTRACE_INFO,
                    "WscCmd:ActualCBThreadProc: CB_MAIN_PUSH_MODE recd\n"));

                S_CB_MAIN_PUSH_MODE *p = (S_CB_MAIN_PUSH_MODE *)p_cbData;

				// Save the UsbKey and Upnp flags
				gb_useUsbKey = p->b_useUsbKey;
				gb_useUpnp = p->b_useUpnp;

				// Process the mode
                switch( p->e_mode )
                {
                    case EModeUnconfAp:
                    {
                        ge_mode = EModeUnconfAp;
    			printf("\n\E[1;37;41m MODE: Access Point \033[0m\n" );

                        // Display menu options for AP
                        S_LCB_MENU_UNCONF_AP *p = new S_LCB_MENU_UNCONF_AP;
                        p->eType = LCB_MENU_UNCONF_AP;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue( sizeof(S_LCB_MENU_UNCONF_AP), 2, p );
                        break;
                    }

                    case EModeClient:
                    {
                        ge_mode = EModeClient;
    			printf("\n\E[1;37;41m MODE: Enrollee/Client \033[0m\n" );

                        // Display menu options for Client
                        S_LCB_MENU_CLIENT *p = new S_LCB_MENU_CLIENT;
                        p->eType = LCB_MENU_CLIENT;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue( sizeof(S_LCB_MENU_CLIENT), 2, p );
                        break;
                    }

                    case EModeRegistrar:
                    {
                        ge_mode = EModeRegistrar;
    			printf("\n\E[1;37;41m MODE: Registrar \033[0m\n" );

                        // Display menu options for Registrar
                        S_LCB_MENU_REGISTRAR *p = new S_LCB_MENU_REGISTRAR;
                        p->eType = LCB_MENU_REGISTRAR;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue( sizeof(S_LCB_MENU_REGISTRAR), 2, p );
                        break;
                    }

                    case EModeApProxyRegistrar:
                    {
                        ge_mode = EModeApProxyRegistrar;
                        printf( "\n******* MODE: AP with built-in "
                                "Registrar and UPnP Proxy *******\n" );

                        // Display menu options for AP+Proxy+Registrar
                        S_LCB_MENU_AP_PROXY_REGISTRAR *p =
                                    new S_LCB_MENU_AP_PROXY_REGISTRAR;
                        p->eType = LCB_MENU_AP_PROXY_REGISTRAR;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue(
								sizeof(S_LCB_MENU_AP_PROXY_REGISTRAR),
                                2, p );
                        break;
                    }

					case EModeApProxy:
					{
						ge_mode = EModeApProxy;
						printf( "\n******* MODE: AP with UPnP Proxy "
                                "*******\n" );

                        // Display menu options for AP+Proxy
                        S_LCB_MENU_AP_PROXY *p =
                                    new S_LCB_MENU_AP_PROXY;
                        p->eType = LCB_MENU_AP_PROXY;
                        p->dataLength = 0;
                        gp_uiQ->Enqueue(
								sizeof(S_LCB_MENU_AP_PROXY),
                                2, p );
						break;
					}

                    default:
                    {
                        printf( "******* MODE: Unknown [Error!] *******\n" );
                        break;
                    }
                } // switch
                break;
            }

            default:
                // not understood, do nothing
                break;
        } // switch

        // free the data
        delete (uint8 *)p_cbData;
    } // while

    TUTRACE((TUTRACE_INFO, "WscCmd:ActualCBThreadProc: Exiting.\n"));
#ifdef __linux__
    pthread_exit(NULL);
#endif

    return NULL;
}

/*
 * Name        : KillCallbackThread
 * Description : Attempt to terminate the callback thread. Enqueue a
 *                 CB_QUIT in the callbackQueue
 * Arguments   : none
 * Return type : void
 */
void
KillCallbackThread()
{
    // enqueue a CB_QUIT
    S_CB_HEADER *p = new S_CB_HEADER;
    p->eType = CB_QUIT;
    p->dataLength = 0;

    gp_cbQ->Enqueue( sizeof(S_CB_HEADER), 1, p );
    return;
} // KillCallbackThread


/*
 * Name        : ActualUIThreadProc
 * Description : This is the thread procedure for the UI thread.
 *                 Monitor the uiQueue, and process all callbacks that
 *                 are received.
 * Arguments   : void *p_data = NULL
 * Return type : void *
 */
void *
ActualUIThreadProc( IN void *p_data )
{
    bool    b_done = false;
    uint32    h_status;
    void    *p_cbData;
    S_LCB_HEADER *p_header;

    TUTRACE((TUTRACE_INFO, "WscCmd:ActualUIThreadProc: Started.\n"));
    // keep doing this until the thread is killed
    while ( !b_done )
    {
        // block on the uiQueue
        h_status = gp_uiQ->Dequeue(
                            NULL,        // size of dequeued msg
                            2,            // sequence number
                            0,            // infinite timeout
                            (void **) &p_cbData);
                                        // pointer to the dequeued msg

        if ( WSC_SUCCESS != h_status )
        {
            // something went wrong
            b_done = true;
            break; // from while loop
        }

        p_header = (S_LCB_HEADER *)p_cbData;

        // once we get something, parse it,
        // do whats necessary, and then block
        switch( p_header->eType )
        {
            case LCB_QUIT:
            {
                // no params
                // destroy the queue
                if ( gp_uiQ )
                {
                    gp_uiQ->DeInit();
                    delete gp_uiQ;
                }
                // kill the thread
                b_done = true;
                break;
            }

            case LCB_MENU_UNCONF_AP:
            {
                char inp[3];
                bool b_tryAgain = true;

                while ( b_tryAgain )
                {
                    printf( "Options:\n" );
                    printf( "0. QUIT\n" );
                    if ( !gb_regDone )
                    {
                        printf( "1. PIN Method\n" );
						printf( "2. PUSH BUTTON Method\n" );
						// printf( "3. Get configured using IEs\n" );
						if ( gb_useUsbKey )
						{
							printf( "4. Write PIN to USB Key\n" );
						}
                    }
                    printf( "Enter selection: " );
                    fgets( inp, 3, stdin );
                    fflush( stdin );
                    // Check if this is valid input
					// Subtract 1 from strlen to remove the '\n'
                    if ( 0 == strlen(inp)-1 )
                    {
                        // We got no input
                        printf( "Error: Invalid input.\n" );
                        continue;
                    }

                    switch( inp[0] )
                    {
                        case '0':
                        {
                            printf( "\nShutting down...\n" );
                            // Do same functionality as LCB_QUIT
                            // destroy the queue
                            if ( gp_uiQ )
                            {
                                gp_uiQ->DeInit();
                                delete gp_uiQ;
                            }
                            // kill the thread
                            b_done = true;
                            b_tryAgain = false;
                            break;
                        }

                        case '1': // Get Configured
						case '2': // Get Configured via push-button
						case '3': // Get configured using IEs
                        {
                            // Invalid input if registration is done
                            if ( gb_regDone )
                            {
                                printf( "ERROR: Invalid input.\n" );
                                break;
                            }

							// e_targetMode not used in InitiateRegistration
							// call below
							if ( '1' == inp[0] )
							{
								// Not doing PBC
								gp_mc->InitiateRegistration(
                                            EModeUnconfAp, EModeRegistrar,
                                            NULL, NULL );
							}
							else if ( '2' == inp[0] )
							{
								// Doing PBC
								char devPwd[9];
								strcpy( devPwd, "00000000\0" );
								gp_mc->InitiateRegistration(
                                            EModeUnconfAp, EModeRegistrar,
                                            devPwd, NULL,
											false,
											WSC_DEVICEPWDID_PUSH_BTN );
							}
							else
							{
								// Need to use IEs
								gp_mc->InitiateRegistration(
											EModeUnconfAp,EModeRegistrar,
											NULL, NULL, true );
							}

							printf( "Waiting for Registrar to connect...\n" );
                            TUTRACE((TUTRACE_INFO,
                                "WscCmd::Registration initiated\n"));
                            b_tryAgain = false;
                            break;
                        }

						case '4': // Write PIN to USB key
						{
							if ( gb_useUsbKey )
							{
								gp_mc->CanWritePin();
								printf( "=====>Please insert USB Key now...\n" );
							}
							else
							{
								printf( "ERROR: Invalid input.\n" );
							}
							break;
						}

                        default:
                        {
                            printf( "ERROR: Invalid input.\n" );
                            break;
                        }
                    } // switch
                } // while
                break;
            }

            case LCB_MENU_CLIENT:
            {
                char inp[3], inp1[3];
                bool b_tryAgain = true;

                while ( b_tryAgain )
                {
                    printf( "Options:\n" );
                    if ( !gb_regDone )
                    {
                        printf( "1. PIN Method\n" );
						printf( "2. PUSH BUTTON Method\n" );
						if ( gb_useUsbKey )
						{
							printf( "3. Write PIN to USB Key\n" );
							printf( "4. Get configured thru USB Key\n" );
						}
                    }
		    		else
                    	printf( "0. QUIT\n" );

                    printf( "Enter selection: " );
                    fgets( inp, 3, stdin );
                    fflush( stdin );
                    // Check if this is valid input
					// Subtract 1 from strlen to remove the '\n'
                    if ( 0 == strlen(inp)-1 )
                    {
                        // We got no input
                        printf( "Error: Invalid input.\n" );
                        continue;
                    }
					if(inp[0] != '0')
					{
		    			printf("\nNetwork selection: \n");
		    			printf("1. Automatic \n");
		    			printf("2. Manual \n");
		    			printf( "Enter selection: " );
		    			fgets( inp1, 3, stdin );
		    			fflush( stdin );
		    			gp_mc->automatic = ((inp1[0]=='1')?1:2);
					}

		    		switch( inp[0] )
                    {
                        case '0':
                        {
                            printf( "\nShutting down...\n" );
                            // Do same functionality as LCB_QUIT
                            // destroy the queue
                            if ( gp_uiQ )
                            {
                                gp_uiQ->DeInit();
                                delete gp_uiQ;
                            }
                            // kill the thread
                            b_done = true;
                            b_tryAgain = false;
                            break;
                        }

                        case '1': // Get configured
						case '2': // Get configured via push-button
                        {
                            if ( gb_regDone )
                            {
                                printf( "ERROR: Invalid input.\n" );
                                break;
                            }
                           Start2minTimer();
							// Set the value of the PBC flag
							bool b_pbc;
							b_pbc = ( '1' == inp[0] )?false:true;

                            //printf( "Initiating registration...\n" );
                            printf( "\nSearching for WPS AP..." );
				fflush(stdout);
							// e_targetMode not used in InitiateRegistration
							// call below
							if ( '1' == inp[0] )
							{
								// Not doing PBC
								gp_mc->InitiateRegistration(
                                            EModeClient, EModeRegistrar,
                                            NULL, NULL );
							}
							else
							{
								// Doing PBC
								char devPwd[9];
								strcpy( devPwd, "00000000\0" );
								gp_mc->InitiateRegistration(
                                            EModeClient, EModeRegistrar,
                                            devPwd, NULL, false,
											WSC_DEVICEPWDID_PUSH_BTN );
							}

                            TUTRACE((TUTRACE_INFO,
                                "WscCmd::Registration initiated\n"));

                            b_tryAgain = false;
                            break;
                        }

						case '3': // Write PIN to USB key
						{
							if ( gb_useUsbKey )
							{
								gp_mc->CanWritePin();
								printf( "=====>Please insert USB Key now...\n" );
							}
							else
							{
								printf( "ERROR: Invalid input.\n" );
							}
							break;
						}

						case '4': // Get configured thru USB key
						{
							if ( gb_useUsbKey )
							{
								gp_mc->CanReadPin();
								printf( "=====>Please insert USB Key now...\n" );
							}
							else
							{
								printf( "ERROR: Invalid input.\n" );
							}
							break;
						}
                        default:
                        {
                            printf( "ERROR: Invalid input.\n" );
                            break;
                        }

                    } // switch
                } // while
                break;
            }

            case LCB_MENU_REGISTRAR:
            {
                char inp[3];
                bool b_tryAgain = true;

                while ( b_tryAgain )
                {
                    printf( "Options:\n" );
                    printf( "0. Quit\n" );
                    printf( "1. Configure AP\n" );
					printf( "2. Configure AP with push-button\n" );
					// printf( "3. Configure AP using IEs\n" );
					if ( gb_useUpnp )
					{
						// Client can be configured only over UPnP
						printf( "4. Configure Client\n" );
						printf( "5. Configure Client with push-button\n" );
					}
					if ( gb_useUsbKey )
					{
						printf( "6. Read PIN from USB Key\n" );
						printf( "7. Write unencrypted settings to USB\n");
						printf( "8. Write Registrar-specified PIN to USB\n");
					}
                    printf( "Enter selection: " );
                    fgets( inp, 3, stdin );
                    fflush( stdin );
                    // Check if this is valid input
					// Subtract 1 from strlen to remove the '\n'
                    if ( 0 == strlen(inp)-1 )
                    {
                        // We got no input
                        printf( "Error: Invalid input.\n" );
                        continue;
                    }

                    switch( inp[0] )
                    {
                        case '0':
                        {
                            printf( "\nShutting down...\n" );
                            // Do same functionality as LCB_QUIT
                            // destroy the queue
                            if ( gp_uiQ )
                            {
                                gp_uiQ->DeInit();
                                delete gp_uiQ;
                            }
                            // kill the thread
                            b_done = true;
                            b_tryAgain = false;
                            break;
                        }

                        case '1': // Configure AP
						case '3': // Configure AP using IEs
                        {
                            char devPwd[64], ssid[32], nwkey[64];
							uint32 len;

                            // Get settings from the user
							// Device password
							//if ( !gb_useUsbKey )
							{
								bool b_devPwdOk = false;
								char check[3];

								while ( !b_devPwdOk )
								{
									if (gp_mc->IsPinEntered())
									{
										strcpy(devPwd, gp_mc->GetDevPwd(len));
									}
									else
									{
										printf( "Enter enrollee's PIN: ");
										fgets( devPwd, 64, stdin );
										// Subtract 1 from strlen to remove the '\n'
										len = (int)strlen(devPwd) - 1;
									}
									/*
									// Remove this check since we're
									// ok with no input
									// Check if this is valid input
									if ( 0 == len )
									{
										// We got no input
										printf( "ERROR: Invalid input.\n" );
										break;
									}
									*/
									devPwd[len] = '\0';

									// Verify the checksum if needed
									if ( 8 == len )
									{
										uint32 val = strtoul(
														devPwd, NULL, 10 );
										if ( !(gp_mc->ValidateChecksum(val)) )
										{
											TUTRACE((TUTRACE_ERR,
												"ValidateChecksum failed\n"));
											printf( "Checksum failed. "
												"Use PIN anyway? (y/n)" );
											fgets( check, 3, stdin );
											if ( 'y' == check[0] )
											{
												b_devPwdOk = true;
											}
											else
											{
												// Ask for the pwd again
												memset( devPwd, 0, 64 );
												b_devPwdOk = false;
											}
										}
										else
										{
											TUTRACE((TUTRACE_ERR,
											  "ValidateChecksum succeeded\n"));
											b_devPwdOk = true;
										}
									}
									else
									{
										b_devPwdOk = true;
									}
								} // while

								if ( !b_devPwdOk )
								{
									// Ask for input again
									break;
								}
							}

							fflush( stdin );

							// SSID
                            printf( "Enter current SSID  : " );
                            fgets( ssid, 32, stdin );
							// Subtract 1 from strlen to remove the '\n'
							len = (int)strlen(ssid) - 1;
                            // Check if this is valid input
                            if ( 0 == len )
                            {
                                // We got no input
                                printf( "ERROR: Invalid input.\n" );
                                break;
                            }
                            ssid[len] = '\0';
				strcpy(gp_mc->ap_ssid_new, ssid);

                            printf( "Enter new SSID      : " );
                            fgets( ssid, 32, stdin );
                            printf( "Enter new passphrase: " );
                            fgets( nwkey, 64, stdin );
                            //nwkey[(int)strlen(nwkey)-1] = '\0';
			    gp_mc->mp_info->SetNwKey(nwkey, strlen(nwkey));

                            printf( "Initiating registration...\n" );

							if ( 0 != devPwd[0] )
							{
								// Check if option '3' has been selected
								bool b_useIe = ('3' == inp[0])?true:false;
                            	gp_mc->InitiateRegistration(
                                            EModeRegistrar, EModeUnconfAp,
                                            devPwd, ssid, b_useIe );
							}
							else
							{

                            	gp_mc->InitiateRegistration(
                                            EModeRegistrar, EModeUnconfAp,
                                            NULL, ssid );
							}

                            b_tryAgain = false;
                            break;
                        }

						case '2': // Configure AP with push-button
						{
							printf( "Using SSID <PBCSecureAP>\n" );
							char ssid[16];
							char devPwd[9];
							strcpy( ssid, "PBCSecureAP\0" );
							strcpy( devPwd, "00000000\0" );
							printf( "Initiating registration...\n" );
							gp_mc->InitiateRegistration(
                                            EModeRegistrar, EModeUnconfAp,
                                            devPwd, ssid,
											false, WSC_DEVICEPWDID_PUSH_BTN );
							b_tryAgain = false;
							break;
						}

						case '4': // Configure Client
						{
							if ( !gb_useUpnp )
							{
								printf( "ERROR: Invalid input\n" );
								break;
							}

							char devPwd[64];
							uint32 len;

							// Get settings from the user
							// Device password
							//if ( !gb_useUsbKey )
							{
								bool b_devPwdOk = false;
								char check[3];

								while ( !b_devPwdOk )
								{
									if (gp_mc->IsPinEntered())
									{
										strcpy(devPwd, gp_mc->GetDevPwd(len));
									}
									else
									{
										printf( "Enter enrollee's PIN. Hit "
												"<Enter> if you don't know it: ");
										fgets( devPwd, 64, stdin );
										// Subtract 1 from strlen to remove the '\n'
										len = (int)strlen(devPwd) - 1;
									}
									/*
									// Remove this check since we're
									// ok with no input
									// Check if this is valid input
									if ( 0 == len )
									{
										// We got no input
										printf( "ERROR: Invalid input.\n" );
										break;
									}
									*/
									devPwd[len] = '\0';

									// Verify the checksum if needed
									if ( 8 == len )
									{
										uint32 val = strtoul(
														devPwd, NULL, 10 );
										if ( !(gp_mc->ValidateChecksum(val)) )
										{
											TUTRACE((TUTRACE_ERR,
												"ValidateChecksum failed\n"));
											printf( "Checksum failed. "
												"Use PIN anyway? (y/n)" );
											fgets( check, 3, stdin );
											if ( 'y' == check[0] )
											{
												b_devPwdOk = true;
											}
											else
											{
												// Ask for the pwd again
												memset( devPwd, 0, 64 );
												b_devPwdOk = false;
											}
										}
										else
										{
											TUTRACE((TUTRACE_ERR,
											  "ValidateChecksum succeeded\n"));
											b_devPwdOk = true;
										}
									}
									else
									{
										b_devPwdOk = true;
									}
								} // while

								if ( !b_devPwdOk )
								{
									// Ask for input again
									break;
								}
							}

							if ( 0 != devPwd[0] )
							{
                            	gp_mc->InitiateRegistration(
                                            EModeRegistrar, EModeClient,
                                            devPwd, NULL );
							}
							else
							{
                            	gp_mc->InitiateRegistration(
                                            EModeRegistrar, EModeClient,
                                            NULL, NULL );
							}

                            TUTRACE((TUTRACE_INFO,
                                "WscCmd::Registration initiated\n"));
                            printf( "Initiating registration...\n" );
                            b_tryAgain = false;
							break;
						}

						case '5': // Configure client via push-button
						{
							if ( !gb_useUpnp )
							{
								printf( "ERROR: Invalid input\n" );
								break;
							}

							char devPwd[9];
							strcpy( devPwd, "00000000\0" );
							printf( "Initiating registration...\n" );
							gp_mc->InitiateRegistration(
                                            EModeRegistrar, EModeClient,
                                            devPwd, NULL,
											false, WSC_DEVICEPWDID_PUSH_BTN );
							b_tryAgain = false;
							break;
						}


						case '6': // Read PIN from USB key
						{
							if ( gb_useUsbKey )
							{
								gp_mc->CanReadPin();
								printf( "=====>Insert USB key...\n" );
							}
							else
							{
								printf( "ERROR: Invalid input.\n" );
							}
							break;
						}

						case '7':
						{
							gp_mc->CanWritePin();
							printf( "=====>Insert USB key now...\n" );
							break;
						}

						case '8':
						{
							gp_mc->CanWriteRegistrarPin();
							printf( "=====>Insert USB key now...\n" );
							break;
						}

                        default:
                        {
                            printf( "ERROR: Invalid input.\n" );
                            break;
                        }
                    } // switch
                } // while
                break;
            }

            case LCB_MENU_AP_PROXY_REGISTRAR:
            {
                char inp[3];
                bool b_tryAgain = true;

                while ( b_tryAgain )
                {
                    printf( "Options:\n" );
                    printf( "0. Quit\n" );
                    printf( "1. Configure Client\n" );
					printf( "2. Configure Client via push-button\n" );
					if ( gb_useUsbKey )
					{
						printf( "3. Read PIN from USB Key\n" );
						printf( "4. Write unencrypted settings to USB\n");
						printf( "5. Write Registrar-specified PIN to USB\n");
					}
                    printf( "Enter selection: " );
                    fgets( inp, 3, stdin );
                    fflush( stdin );
                    // Check if this is valid input
					// Subtract 1 from strlen to remove the '\n'
                    if ( 0 == strlen(inp)-1 )
                    {
                        // We got no input
                        printf( "Error: Invalid input.\n" );
                        continue;
                    }

                    switch( inp[0] )
                    {
                        case '0':
                        {
                            printf( "\nShutting down...\n" );
                            // Do same functionality as LCB_QUIT
                            // destroy the queue
                            if ( gp_uiQ )
                            {
                                gp_uiQ->DeInit();
                                delete gp_uiQ;
                            }
                            // kill the thread
                            b_done = true;
                            b_tryAgain = false;
                            break;
                        }

                        case '1': // Configure Client
                        {
							char devPwd[65];
							uint32 len;

							// Get settings from the user
							// Device Password
							//if ( !gb_useUsbKey )
							{
								bool b_devPwdOk = false;
								char check[3];

								while ( !b_devPwdOk )
								{
									if (gp_mc->IsPinEntered())
									{
										strcpy(devPwd, gp_mc->GetDevPwd(len));
									}
									else
									{
										printf( "Enter enrollee's PIN. Hit "
												"<Enter> if you don't know it: ");
										fgets( devPwd, 64, stdin );
										// Subtract 1 from strlen to remove the '\n'
										len = (int)strlen(devPwd) - 1;
									}
									/*
									// Removed this check since we're
									// ok with no input
									// Check if this is valid input
									if ( 0 == len )
									{
										// We got no input
										printf( "ERROR: Invalid input.\n" );
										break;
									}
									*/
									devPwd[len] = '\0';

									// Verify the checksum if needed
									if ( 8 == len )
									{
										uint32 val = strtoul(
														devPwd, NULL, 10 );
										if ( !(gp_mc->ValidateChecksum(val)) )
										{
											TUTRACE((TUTRACE_ERR,
												"ValidateChecksum failed\n"));
											printf( "Checksum failed. "
												"Use PIN anyway? (y/n)" );
											fgets( check, 3, stdin );
											if ( 'y' == check[0] )
											{
												b_devPwdOk = true;
											}
											else
											{
												// Ask for the pwd again
												memset( devPwd, 0, 64 );
												b_devPwdOk = false;
											}
										}
										else
										{
											TUTRACE((TUTRACE_ERR,
											  "ValidateChecksum succeeded\n"));
											b_devPwdOk = true;
										}
									}
									else
									{
										b_devPwdOk = true;
									}
								} // while

								if ( !b_devPwdOk )
								{
									// Ask for input again
									break;
								}
							}

                            printf( "Waiting for Client to connect...\n" );
							if ( 0 != devPwd[0] )
							{
								gp_mc->InitiateRegistration(
                                            EModeApProxyRegistrar,
											EModeClient, devPwd, NULL );
							}
							else
							{
								gp_mc->InitiateRegistration(
                                            EModeApProxyRegistrar,
											EModeClient, NULL, NULL );
							}

                            TUTRACE((TUTRACE_INFO,
                                "WscCmd::Registration initiated\n"));
                            b_tryAgain = false;
                            break;
                        }

						case '2': // Configure client via push-button
						{
							char devPwd[9];
							strcpy( devPwd, "00000000\0" );
							printf( "Waiting for Client to connect...\n" );
							gp_mc->InitiateRegistration(
                                            EModeApProxyRegistrar,
											EModeClient, devPwd, NULL,
											false, WSC_DEVICEPWDID_PUSH_BTN );
							TUTRACE((TUTRACE_INFO,
                                "WscCmd::Registration initiated\n"));
                            b_tryAgain = false;
							break;
						}

						case '3': // Read PIN from USB key
						{
							if ( gb_useUsbKey )
							{
								gp_mc->CanReadPin();
								printf( "=====>Insert USB key...\n" );
							}
							else
							{
								printf( "ERROR: Invalid input.\n" );
							}
							break;
						}

						case '4':
						{
							gp_mc->CanWritePin();
							printf( "=====>Insert USB key now...\n" );
							break;
						}

						case '5':
						{
							gp_mc->CanWriteRegistrarPin();
							printf( "=====>Insert USB key now...\n" );
							break;
						}

                        default:
                        {
                            printf( "ERROR: Invalid input.\n" );
                            break;
                        }
                    } // switch
                } // while
                break;
            }

			case LCB_MENU_AP_PROXY:
            {
				char inp[3];
                bool b_tryAgain = true;

                while ( b_tryAgain )
                {
                    printf( "Options:\n" );
                    printf( "0. Quit\n" );
					printf( "Enter selection: " );
                    fgets( inp, 3, stdin );
                    fflush( stdin );
                    // Check if this is valid input
					// Subtract 1 from strlen to remove the '\n'
                    if ( 0 == strlen(inp)-1 )
                    {
                        // We got no input
                        printf( "Error: Invalid input.\n" );
                        continue;
                    }

					switch( inp[0] )
                    {
                        case '0':
                        {
                            printf( "\nShutting down...\n" );
                            // Do same functionality as LCB_QUIT
                            // destroy the queue
                            if ( gp_uiQ )
                            {
                                gp_uiQ->DeInit();
                                delete gp_uiQ;
                            }
                            // kill the thread
                            b_done = true;
                            b_tryAgain = false;
                            break;
                        }

						default:
						{
                            printf( "ERROR: Invalid input.\n" );
                            break;
                        }
					} // switch
				} // while
				break;
			}

			case LCB_MENU_REQUEST_PWD:
			{
				S_LCB_MENU_REQUEST_PWD *p =
						(S_LCB_MENU_REQUEST_PWD *)p_cbData;
				printf( "An Enrollee is requesting permission to connect.\n" );
				printf( "Device name: %s\n", p->deviceName );
				printf( "Model number: %s\n", p->modelNumber );
				printf( "Serial number: %s\n", p->serialNumber );

				// Get device password
				// TODO: Need to figure out how to use USB key here

				bool b_devPwdOk = false, b_proceed = true;
				char devPwd[64], check[3];
				int len;

				while ( !b_devPwdOk )
				{
					printf( "Enter PIN for this device (0 to not "
							"set this device up): ");
					fgets( devPwd, 64, stdin );
					// Subtract 1 from strlen to remove the '\n'
					len = (int)strlen(devPwd) - 1;
					// Check if this is valid input
					if ( 0 == len )
					{
						// We got no input
						printf( "ERROR: Invalid input.\n" );
						continue;
					}
					devPwd[len] = '\0';

					if ( (1 == len) && ('0' == devPwd[0]) )
					{
						// User does not want to use this device as the
						// registrar
						b_devPwdOk = true;
						b_proceed = false;
						break;
					}

					// Verify the checksum if needed
					if ( 8 == len )
					{
						uint32 val = strtoul(
										devPwd, NULL, 10 );
						if ( !(gp_mc->ValidateChecksum(val)) )
						{
							TUTRACE((TUTRACE_ERR,
								"ValidateChecksum failed\n"));
							printf( "Checksum failed. "
								"Use PIN anyway? (y/n)" );
							fgets( check, 3, stdin );
							if ( 'y' == check[0] )
							{
								b_devPwdOk = true;
							}
							else
							{
								// Ask for the pwd again
								memset( devPwd, 0, 64 );
								b_devPwdOk = false;
							}
						}
						else
						{
							TUTRACE((TUTRACE_ERR,
								"ValidateChecksum succeeded\n"));
							b_devPwdOk = true;
						}
					}
					else
					{
						b_devPwdOk = true;
					}
				} // while

				if ( b_proceed )
				{
					// Send this device password to the Registrar SM
					gp_mc->SetDevicePassword( devPwd, p->uuid );
					printf( "Using this PIN to connect to the "
							"Enrollee...\n" );
				}
				else
				{
					// Go back to the main menu
					// TODO: does the SM need to be reset?

					// Post a message to the UI thread
					switch( ge_mode )
					{
						case EModeUnconfAp:
						{
							// Display menu options for AP
							S_LCB_MENU_UNCONF_AP *p =
											new S_LCB_MENU_UNCONF_AP;
							p->eType = LCB_MENU_UNCONF_AP;
							p->dataLength = 0;
							gp_uiQ->Enqueue( sizeof(S_LCB_MENU_UNCONF_AP),
												2, p );
							break;
						}

						case EModeClient:
						{
							// Display menu options for Client
							S_LCB_MENU_CLIENT *p = new S_LCB_MENU_CLIENT;
							p->eType = LCB_MENU_CLIENT;
							p->dataLength = 0;
							gp_uiQ->Enqueue( sizeof(S_LCB_MENU_CLIENT),
												2, p );
							break;
						}

						case EModeRegistrar:
						{
							// Display menu options for Registrar
							S_LCB_MENU_REGISTRAR *p =
											new S_LCB_MENU_REGISTRAR;
							p->eType = LCB_MENU_REGISTRAR;
							p->dataLength = 0;
							gp_uiQ->Enqueue( sizeof(S_LCB_MENU_REGISTRAR),
												2, p );
							break;
						}

						case EModeApProxyRegistrar:
						{
							// Display menu options for Registrar
							S_LCB_MENU_AP_PROXY_REGISTRAR *p =
											new S_LCB_MENU_AP_PROXY_REGISTRAR;
							p->eType = LCB_MENU_AP_PROXY_REGISTRAR;
							p->dataLength = 0;
							gp_uiQ->Enqueue(
									sizeof(S_LCB_MENU_AP_PROXY_REGISTRAR),
									2, p );
							break;
						}

						case EModeApProxy:
						{
							// TODO
							break;
						}

						default:
						{
							break;
						}
					} // switch
				}

				break;
			}

            default:
            {
                // not understood, do nothing
                TUTRACE((TUTRACE_INFO,
                    "WscCmd:: Unknown callback type received\n"));
                break;
            }
        } // switch

        // free the data
        delete (uint8 *)p_cbData;
    } // while

    TUTRACE((TUTRACE_INFO, "WscCmd:ActualUIThreadProc: Exiting.\n"));
#ifdef __linux__
    pthread_exit(NULL);
#endif

    return NULL;
}

/*
 * Name        : KillUIThread
 * Description : Attempt to terminate the UI thread. Enqueue a CB_QUIT in the
 *               UI message queue.
 * Arguments   : none
 * Return type : void
 */
void
KillUIThread()
{
    // enqueue a CB_QUIT
    S_LCB_HEADER *p = new S_LCB_HEADER;
    p->eType = LCB_QUIT;
    p->dataLength = 0;

    gp_uiQ->Enqueue( sizeof(S_LCB_HEADER), 2, p );
    return;
} // KillUIThread

/*
 * Name        : CallbackProc
 * Description : Callback method that MasterControl uses to pass
 *                    info back to main()
 * Arguments   : IN void *p_callBackMsg - pointer to the data being
 *                    passed in
 *                 IN void *p_thisObj - NULL
 * Return type : none
 */
void
CallbackProc(IN void *p_callbackMsg, IN void *p_thisObj)
{
    S_CB_HEADER *p_header = (S_CB_HEADER *)p_callbackMsg;

    uint32 dw_length = sizeof(p_header->dataLength) +
                        sizeof(S_CB_HEADER);

    uint32 h_status = gp_cbQ->Enqueue( dw_length,    // size of the data
                                    1,                // sequence number
                                    p_callbackMsg );// pointer to the data
    if ( WSC_SUCCESS != h_status )
	{
    	TUTRACE((TUTRACE_ERR, "WscCmd::CallbackProc Enqueue failed\n"));
	}
	else
	{
	    TUTRACE((TUTRACE_INFO, "WscCmd::CallbackProc Enqueue done\n"));
	}
    return;
} // CallbackProc


/*
 * Name        : APCopyConfFile
 * Description : Copy the hostapd template config file
 * Arguments   : none
 * Return type : uint32 - result of the operation
 */
uint32 APCopyConfFile()
{
    FILE *fp_templ, *fp_host;
    int len;
    char buf[1024];

    // Open files
    fp_templ = fopen( AP_CONF_TEMPLATE, "r" );
    if ( !fp_templ )
        return WSC_ERR_FILE_OPEN;

    fp_host = fopen( AP_CONF_FILENAME, "w" );
    if ( !fp_host )
    {
        fclose( fp_templ );
        return WSC_ERR_FILE_OPEN;
    }

    // Copy contents of the template into the hostapd.conf file
    while( !feof(fp_templ) )
    {
        len = fread( buf, 1, 1024, fp_templ );
        if( len != 0 )
        {
            fwrite( buf, 1, len, fp_host );
        }
    }

    fclose( fp_templ );
    fclose( fp_host );
    TUTRACE((TUTRACE_INFO, "WscCmd::AP Config file copied\n"));
    return WSC_SUCCESS;
} // APCopyConfFile

/*
 * Name        : APAddParams
 * Description : Add specific params to the hostapd config file
 * Arguments   : char *ssid - ssid
 *               char *keyMgmt - value of keyMgmt
 *               char *nwKey - the psk to be used
 *				 uint32 nwKeyLen - length of the key
 * Return type : uint32 - result of the operation
 */
uint32 APAddParams( IN char *ssid, IN char *keyMgmt,
					IN char *nwKey, IN uint32 nwKeyLen )
{
    if ( !ssid && !keyMgmt && !nwKey )
        return WSC_SUCCESS;

    FILE *fp = fopen( AP_CONF_FILENAME, "a" );
    if( !fp )
        return WSC_ERR_FILE_OPEN;

    if ( ssid )
    {
        fprintf( fp, "ssid=%s\n", ssid );
    }

    if ( keyMgmt )
    {
        fprintf( fp, "wpa_key_mgmt=%s\n", keyMgmt );
    }

    if ( nwKeyLen > 0 )
    {
		// If nwKeyLen == 64 bytes, then it is the PSK
		// If psk < 64 bytes , it should be interpreted as the passphrase
		char line[100];
		if ( nwKeyLen < 64 )
		{
			// Interpret as passphrase
			// Should be < 63 characters
			sprintf( line, "wpa_passphrase=" );
		}
		else
		{
			// Interpret as psk
			// Should be 64 characters
			sprintf( line, "wpa_psk=" );
		}
		strncat( line, nwKey, nwKeyLen );

		fprintf( fp, "%s\n", line );
    }
    fclose(fp);

    TUTRACE((TUTRACE_INFO, "WscCmd::Params added to AP config file\n"));
    return WSC_SUCCESS;
} // APAddParams

/*
 * Name        : APRestartNetwork
 * Description : Restart network config if necessary
 * Arguments   : none
 * Return type : uint32 - result of the operation
 */
uint32 APRestartNetwork()
{
#ifdef __linux__
    system( "killall hostapd" );
    // system( "killall dhcpd" );

    // system( "/etc/init.d/network restart" );
    // system( "dhcpd ath0" );

    WscSleep( 2 );
    TUTRACE((TUTRACE_INFO, "WscCmd::Network Restarted\n"));
#else
    TUTRACE((TUTRACE_INFO, "WscCmd::APRestartNetwork not implemented "
                "for WinXP\n"));
#endif
    return WSC_SUCCESS;
} // APRestart

/*
 * Name        : SuppWriteConfFile
 * Description : Write out a specific version of the config file for
 *               wpa_supplicant. Dependant on the mode that we are
 *               currently in
 * Arguments   : char *ssid - ssid
 *               char *keyMgmt - value of keyMgmt to be used
 *               char *nwKey - psk to be used
 *				 uint32 nwKeyLen - length of the key
 *               char *identity - identity to be used for the EAP-WSC method
 *               bool b_startWsc - flag that indicates whether the EAP-WSC
 *               method needs to be run
 * Return type : uint32 - result of the operation
 */
uint32 SuppWriteConfFile( IN char *ssid, IN char *keyMgmt, IN char *nwKey,
                            IN uint32 nwKeyLen, IN char *identity,
							IN bool b_startWsc, IN bool b_regDone)
{
    FILE *fp = fopen( SUPP_CONF_FILENAME, "w" );
    if ( !fp )
        return WSC_ERR_FILE_OPEN;

    fprintf( fp, "ctrl_interface=/var/run/wpa_supplicant\n" );
    fprintf( fp, "eapol_version=1\n" );
    if( b_regDone )
    {
	    fprintf( fp, "wsc_done=1\n" );
    }
    else
    {
	    fprintf( fp, "wsc_done=0\n" );

    }
    if(strcmp(ssid, "JunkAP")==0)
    {
    	fprintf( fp, "ap_scan=1\n" );
    	fprintf( fp, "network={\n" );
    	fprintf( fp, "\tssid=\"%s\"\n", ssid );
    	fprintf( fp, "\tkey_mgmt=NONE\n", keyMgmt );
		goto end;
    }
    else
    	    fprintf( fp, "ap_scan=2\n" );
    fprintf( fp, "network={\n" );
    fprintf( fp, "\tssid=\"%s\"\n", ssid );
    fprintf( fp, "\tkey_mgmt=%s\n", keyMgmt );

    if(strcmp(keyMgmt, "NONE")==0)
	goto end;

    if ( b_startWsc )
    {
        // set the profile so that we use EAP-WSC
        fprintf( fp, "\teap=WSC\n" );
        fprintf( fp, "\tidentity=\"%s\"\n", identity );
    }
    else
    {
		// Assuming non-zero nwKeyLen
		if ( nwKeyLen > 0 )
		{
			fprintf( fp, "\tproto=WPA\n" );
			fprintf( fp, "\tpairwise=TKIP\n" );
			fprintf( fp, "\tgroup=TKIP\n" );
			char line[100];
			sprintf( line, "\tpsk=" );
			if ( nwKeyLen < 64 )
			{
				// Interpret as passphrase
				// Must be 8-63 characters
				strcat( line, "\"" );
				strcat( line, nwKey );
				strcat( line, "\"" );
			}
			else
			{
				// Interpret as PSK
				// Must be exactly 64 characters
				strncat( line, nwKey, 64 );
			}
			fprintf( fp, "%s\n", line );
		}
    }
    fprintf( fp, "\tscan_ssid=1\n" );
end:
    fprintf( fp, "}\n" );

    fclose( fp );
	WscSleep( 1 );
    TUTRACE((TUTRACE_INFO, "WscCmd::Supp config file written\n"));
    return WSC_SUCCESS;
} // SuppWriteConfFile

/*
 * Name        : SuppManage
 * Description : Manage the supplicant via the command-line app wpa_cli
 * Arguments   : char *cmd - the command to pass to wpa_cli
 * Return type : uint32 - result of the operation
 */
uint32 SuppManage( IN char *cmd )
{
    // Send a message to wpa_supplicant via wpa_cli
    char buf[64];
    sprintf( buf, "./wpa_cli -i %s %s > /dev/null", inf, cmd );
#ifdef WIN32
    sprintf( buf, "wpa_cli -i %s %s ", inf, cmd );
#endif
    TUTRACE((TUTRACE_INFO, "%s\n", buf));
    system( buf );
    WscSleep( 1 );
    return WSC_SUCCESS;
} // SuppManage

static void RegTimeout(int dummy)
{
	if(!gb_regDone)
	{
		printf("\nTIMEOUT: Registration Protocol Failed. Exiting.\n");
		TUTRACE((TUTRACE_INFO, "WscCmd:TIMEOUT: Exiting.\n"));
		kill(0,SIGTERM);
	}
}

void Start2minTimer(void)
{
        signal(SIGALRM, RegTimeout);
        alarm(120);
}

