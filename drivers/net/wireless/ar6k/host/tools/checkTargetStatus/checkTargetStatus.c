#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <a_config.h>
#include <a_osapi.h>
#include <a_types.h>
#include <athdefs.h>
#include <athdrv.h>
#include <htc.h>
#include <htc_api.h>


#define FILE_TX_ATTEMPT   "/sys/module/ar6000/tx_attempt"
#define FILE_TX_POST      "/sys/module/ar6000/tx_post"
#define FILE_TX_COMPLETE  "/sys/module/ar6000/tx_complete"

enum {
    TX_ATTEMPT_CNT,
    TX_POST_CNT,
    TX_COMPLETE_CNT
    };

#define DEFAULT_DELAY   2

typedef struct tx_cnts_t {
    int tx_attempt;
    int tx_post;
    int tx_complete;
    int qStuckTimerTicks;
    }TX_CNTS;


int  get_counts(TX_CNTS *pCur, int member, char *fileName);
void checkStatus(TX_CNTS *pCur, TX_CNTS *pLast, char *intf);

int
main(int argc, char **argv)
{
    int delay=DEFAULT_DELAY;
    TX_CNTS cur[HTC_MAILBOX_NUM_MAX], last[HTC_MAILBOX_NUM_MAX];

    if(argc < 3)
    {
        printf("Error: Incorrect usage\n"
                "Usage: %s <intf> <delay>\n", argv[0]);
        exit(1);
    }

    memset(cur, 0, sizeof(cur));
    memset(last, 0, sizeof(last));

    delay = atoi(argv[2]);

    printf("Using a delay of %dsec\n", delay);

    while(1) 
    {
        sleep(delay);
        memcpy(last, cur, sizeof(cur));

        if( !(get_counts(cur, TX_ATTEMPT_CNT, FILE_TX_ATTEMPT) &&
              get_counts(cur, TX_POST_CNT, FILE_TX_POST) &&
              get_counts(cur, TX_COMPLETE_CNT, FILE_TX_COMPLETE)) )
        {

            continue;
        }

        checkStatus(cur, last, argv[1]);
    }
    
    return 0;
}

void
checkStatus(TX_CNTS *pCur, TX_CNTS *pLast, char *intf)
{
    char   cmdBuf[100];
    int i;

    for(i = 0; i < HTC_MAILBOX_NUM_MAX; i++)
    {
        /* If no packet transfer was confirmed, check if we 
         * did send something. As long as txComplete is
         * going up, the target is draining, albeit slowly.
         */
        if(pCur->tx_complete == pLast->tx_complete)
        {
            /* Did we drain out anything, otherwise its stuck */
            if(pCur->tx_post == pCur->tx_complete) /* Queue is empty */
                pCur->qStuckTimerTicks = 0;
            else
                pCur->qStuckTimerTicks++; 

            /* Wait for atleast two timer ticks, due std queuing issues,
             * bursty traffic..
             */
            if(pCur->qStuckTimerTicks > 1) 
            {
                printf("\nTX que %d is stuck. Host seems dead\n", i);
                sprintf(cmdBuf, "ifconfig %s down; "
                                "sleep 5; "
                                "ifconfig %s up\n",
                                intf, intf);
                system(cmdBuf);
                /* When we reset the target, we clear driver counters.
                 * Lets clear our counters.
                 */
                 memset(pCur - i, 0, sizeof(*pCur)*HTC_MAILBOX_NUM_MAX);
                 memset(pLast - i, 0, sizeof(*pLast)*HTC_MAILBOX_NUM_MAX);
                return;
            }
        }
        else
        {
            pCur->qStuckTimerTicks = 0;
        }

        printf("%c", pCur->qStuckTimerTicks ? '?': '#');

        pCur++;
        pLast++;
    }

    printf(" ");
    fflush(stdout);
}

int
get_counts(TX_CNTS *pCur, int member, char *fileName)
{
    char    buf[80];
    int     i, len, val[HTC_MAILBOX_NUM_MAX], offset;
    FILE *fp = fopen(fileName, "r");

    if( fp == NULL)
        return FALSE;
    
    fgets(buf, 80, fp);

    len = strlen(buf);
    for(i = 0; i < len; i++)
        if(buf[i] == ',') buf[i] = ' ';

    sscanf(buf, "%d%d%d%d", &val[0],&val[1], &val[2], &val[3]);

    offset = member*sizeof(int);

    for(i = 0; i < HTC_MAILBOX_NUM_MAX; i++)
    {
        *(int *)((char *)pCur + offset) = val[i];
        pCur++;
    }

    fclose(fp);

    return TRUE;
}

