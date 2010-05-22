//------------------------------------------------------------------------------
// <copyright file="abtfilt_main.c" company="Atheros">
//    Copyright (c) 2008 Atheros Corporation.  All rights reserved.
// 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

/*
 * Bluetooth Filter Main Routine
 *
 */
static const char athId[] __attribute__ ((unused)) = "$Id: //depot/sw/releases/olca2.1-RC/host/tools/athbtfilter/bluez/abtfilt_main.c#3 $";

#include "abtfilt_int.h"

const char *progname;
static ATH_BT_FILTER_INSTANCE g_AthBtFilterInstance;
A_FILE_HANDLE gConfigFile;
A_COND_OBJECT   g_WaitTerminate;
A_MUTEX_OBJECT  g_WaitTerminateLock;

static void 
usage(void)
{
    fprintf(stderr, "usage:\n%s [options] \n", progname);
    fprintf(stderr, "  -n   : do not run as a daemon \n");
    fprintf(stderr, "  -d   : enable debug logging \n");
    fprintf(stderr, "  -c   : output debug logs to the console \n");  
    fprintf(stderr, "  -a   : issue AFH channel classification when WLAN connects \n");
    fprintf(stderr, "  -f <config file>  : do not run as a daemon \n");
}

void
Abf_ShutDown(void)
{
    A_INFO("Shutting Down\n");

    /* Clean up all the resources */
    Abf_BtStackNotificationDeInit(&g_AthBtFilterInstance);
    Abf_WlanStackNotificationDeInit(&g_AthBtFilterInstance);
    AthBtFilter_Detach(&g_AthBtFilterInstance);
    
    A_INFO("Shutting Down Complete\n");
}

static void
Abf_SigTerm(int sig)
{
    /* Initiate the shutdown sequence */
    Abf_ShutDown();
        /* unblock main thread */
    A_MUTEX_LOCK(&g_WaitTerminateLock);
    A_COND_SIGNAL(&g_WaitTerminate);
    A_MUTEX_UNLOCK(&g_WaitTerminateLock);
}

int 
main(int argc, char *argv[])
{
    int ret;
    char *config_file = NULL;
    int opt = 0, daemonize = 1, debug = 0, console_output=0;
    progname = argv[0];
    A_STATUS status;
    struct sigaction sa;
    ATHBT_FILTER_INFO *pInfo;
    A_UINT32 btfiltFlags = 0;

    A_COND_INIT(&g_WaitTerminate);
    A_MUTEX_INIT(&g_WaitTerminateLock);
    
    /* 
     * Keep an option to specify the wireless extension. By default, 
     * assume it to be equal to WIRELESS_EXT TODO 
     */

    /* Get user specified options */
    while ((opt = getopt(argc, argv, "andcf:")) != EOF) {
        switch (opt) {
        case 'n':
            daemonize = 0;
            break;

        case 'd':
            debug = 1;
            break;

        case 'f':
            if (optarg) {
                config_file = strdup(optarg);
            }
            break;
        case 'c':
            console_output = 1;
            break;
        case 'a':
            btfiltFlags |= ABF_ENABLE_AFH_CHANNEL_CLASSIFICATION;
            break;
        default:
            usage();
            exit(1);
        }
    }

    /* Launch the daemon if desired */
    if (daemonize && daemon(0, console_output ? 1 : 0)) {
        printf("Can't daemonize: %s\n", strerror(errno));
        exit(1);
    }

    /* Initialize the debug infrastructure */
    A_DBG_INIT("ATHBT", "Ath BT Filter Daemon");
    if (debug) {
        if (console_output) {
            A_DBG_SET_OUTPUT_TO_CONSOLE();
        }
        A_INFO("Enabling Debug Information\n");
        A_SET_DEBUG(1);
    }
    
    if (config_file) {
        A_DEBUG("Config file: %s\n", config_file);
        if (!(gConfigFile = fopen(config_file, "r")))
        {
            A_ERR("[%s] fopen failed\n", __FUNCTION__);
        }
    }

    A_MEMZERO(&g_AthBtFilterInstance, sizeof(ATH_BT_FILTER_INSTANCE));
    A_MEMZERO(&sa, sizeof(struct sigaction));
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = Abf_SigTerm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    /* Initialize the Filter core */
    do {
        ret = AthBtFilter_Attach(&g_AthBtFilterInstance);
        if (ret) {
            A_ERR("Filter initialization failed\n");
            break;
        }

        /* Initialize the WLAN notification mechanism */
        status = Abf_WlanStackNotificationInit(&g_AthBtFilterInstance);
        if (A_FAILED(status)) {
            AthBtFilter_Detach(&g_AthBtFilterInstance);
            A_ERR("WLAN stack notification init failed\n");
            break;
        }

        /* Initialize the BT notification mechanism */
        status = Abf_BtStackNotificationInit(&g_AthBtFilterInstance,btfiltFlags);
        if (A_FAILED(status)) {
            Abf_WlanStackNotificationDeInit(&g_AthBtFilterInstance);
            AthBtFilter_Detach(&g_AthBtFilterInstance);
            A_ERR("BT stack notification init failed\n");
            break;
        }

        /* Check for errors on the return value TODO */
        pInfo = g_AthBtFilterInstance.pContext;
        
        A_DEBUG("Service running, waiting for termination .... \n");
        
            /* wait for termination signal */          
        A_MUTEX_LOCK(&g_WaitTerminateLock);
        A_COND_WAIT(&g_WaitTerminate, &g_WaitTerminateLock,WAITFOREVER);
        A_MUTEX_UNLOCK(&g_WaitTerminateLock);                 
                         
    } while(FALSE);

    /* Shutdown */
    if (gConfigFile) {
        fclose(gConfigFile);
    }

    if (config_file) {
        A_FREE(config_file);
    }

    A_DEBUG("Service terminated \n");
    A_MEMZERO(&g_AthBtFilterInstance, sizeof(ATH_BT_FILTER_INSTANCE));
    A_DBG_DEINIT();
    
    return 0;
}
