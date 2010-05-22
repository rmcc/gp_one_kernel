/*
 * Copyright (c) 2006 Atheros Communications Inc.
 * All rights reserved.
 * 
 *
 * 
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
 * 
 */

/* This tool parses the recevent logs stored in the binary format 
   by the wince athsrc */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <a_config.h>
#include <a_osapi.h>
#include <a_types.h>
#include <athdefs.h>
#include <ieee80211.h>
#include <wmi.h>
#include <athdrv_linux.h>
#include <dbglog_api.h>

#undef DEBUG
#undef DBGLOG_DEBUG

#define ID_LEN                         2
#define FILENAME_LENGTH_MAX            128
#define DBGLOG_FILE                    "dbglog.h"
#define DBGLOGID_FILE                  "dbglog_id.h"
#define DBGLOG_OUTPUT_FILE             "dbglog.out"

const char *progname;
char dbglogfile[FILENAME_LENGTH_MAX];
char dbglogidfile[FILENAME_LENGTH_MAX];
char dbglogoutfile[FILENAME_LENGTH_MAX];
char dbgloginfile[FILENAME_LENGTH_MAX];
FILE *fpout;
FILE *fpin;

char dbglog_id_tag[DBGLOG_MODULEID_NUM_MAX][DBGLOG_DBGID_NUM_MAX][DBGLOG_DBGID_DEFINITION_LEN_MAX];

#define AR6K_DBG_BUFFER_SIZE 1500

struct dbg_record {
    unsigned int ts;
    unsigned int length;
    unsigned char log[AR6K_DBG_BUFFER_SIZE];
};

#ifdef DEBUG
int debugRecEvent = 0;
#define RECEVENT_DEBUG_PRINTF(args...)        if (debugRecEvent) printf(args);
#else
#define RECEVENT_DEBUG_PRINTF(args...)
#endif

int
string_search(FILE *fp, char *string)
{
    char str[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    rewind(fp);
    memset(str, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
    while (!feof(fp)) {
        fscanf(fp, "%s", str);
        if (strstr(str, string)) return 1;
    }

    return 0;
}

void
get_module_name(char *string, char *dest)
{
    char *str1, *str2;
    char str[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    memset(str, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
    strcpy(str, string);
    str1 = strtok(str, "_");
    while ((str2 = strtok(NULL, "_"))) {
        str1 = str2;
    }

    strcpy(dest, str1);
}

#ifdef DBGLOG_DEBUG
void
dbglog_print_id_tags(void)
{
    int i, j;

    for (i = 0; i < DBGLOG_MODULEID_NUM_MAX; i++) {
        for (j = 0; j < DBGLOG_DBGID_NUM_MAX; j++) {
            printf("[%d][%d]: %s\n", i, j, dbglog_id_tag[i][j]);
        }
    }
}
#endif /* DBGLOG_DEBUG */

int
dbglog_generate_id_tags(void)
{
    int id1, id2;
    FILE *fp1, *fp2;
    char str1[DBGLOG_DBGID_DEFINITION_LEN_MAX];
    char str2[DBGLOG_DBGID_DEFINITION_LEN_MAX];
    char str3[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    if (!(fp1 = fopen(dbglogfile, "r"))) {
        perror(dbglogfile);
        return -1;
    }

    if (!(fp2 = fopen(dbglogidfile, "r"))) {
        perror(dbglogidfile);
        return -1;
    }

    memset(dbglog_id_tag, 0, sizeof(dbglog_id_tag));
    if (string_search(fp1, "DBGLOG_MODULEID_START")) {
        fscanf(fp1, "%s %s %d", str1, str2, &id1);
        do {
            memset(str3, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
            get_module_name(str2, str3);
            strcat(str3, "_DBGID_DEFINITION_START");
            if (string_search(fp2, str3)) {
                memset(str3, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
                get_module_name(str2, str3);
                strcat(str3, "_DBGID_DEFINITION_END");
                fscanf(fp2, "%s %s %d", str1, str2, &id2);
                while (!(strstr(str2, str3))) {
                    strcpy((char *)&dbglog_id_tag[id1][id2], str2);
                    fscanf(fp2, "%s %s %d", str1, str2, &id2);
                }
            }
            fscanf(fp1, "%s %s %d", str1, str2, &id1);
        } while (!(strstr(str2, "DBGLOG_MODULEID_END")));
    }

    fclose(fp2);
    fclose(fp1);

    return 0;
}

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s <input log file> <output file> \n", progname);
    exit(-1);
}

static int 
decode_debug_rec(struct dbg_record *dbg_rec)
{
#define BUF_SIZE    120
    A_UINT32 count;
    A_UINT32 timestamp;
    A_UINT32 debugid;
    A_UINT32 numargs;
    A_UINT32 moduleid;
    A_INT32 *buffer;
    A_UINT32 length;
    char buf[BUF_SIZE];
    long curpos;
    static int numOfRec = 0;
    int len;

#ifdef DBGLOG_DEBUG
    RECEVENT_DEBUG_PRINTF("Application received target debug event: %d\n", len);
#endif /* DBGLOG_DEBUG */
    count = 0;
    len = dbg_rec->length;
    length = (len >> 2);
    buffer = (A_INT32 *)dbg_rec->log;

    while (count < length) {
        debugid = DBGLOG_GET_DBGID(buffer[count]);
        moduleid = DBGLOG_GET_MODULEID(buffer[count]);
        numargs = DBGLOG_GET_NUMARGS(buffer[count]);
        timestamp = DBGLOG_GET_TIMESTAMP(buffer[count]);
        switch (numargs) {
            case 0:
            fprintf(fpout, "%8d: %s (%d)\n", dbg_rec->ts,
                    dbglog_id_tag[moduleid][debugid],
                    timestamp);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d)\n",
                                  dbglog_id_tag[moduleid][debugid], 
                                  timestamp);
#endif /* DBGLOG_DEBUG */
            break;

            case 1:
            fprintf(fpout, "%8d: %s (%d): 0x%x\n", dbg_rec->ts,
                    dbglog_id_tag[moduleid][debugid], 
                    timestamp, buffer[count+1]);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d): 0x%x\n", 
                                  dbglog_id_tag[moduleid][debugid], 
                                  timestamp, buffer[count+1]);
#endif /* DBGLOG_DEBUG */
            break;

            case 2:
            fprintf(fpout, "%8d: %s (%d): 0x%x, 0x%x\n", dbg_rec->ts,
                    dbglog_id_tag[moduleid][debugid], 
                    timestamp, buffer[count+1],
                    buffer[count+2]);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d): 0x%x, 0x%x\n",
                                  dbglog_id_tag[moduleid][debugid], 
                                  timestamp, buffer[count+1],
                                  buffer[count+2]);
#endif /* DBGLOG_DEBUG */
            break;

            default:
            RECEVENT_DEBUG_PRINTF("Invalid args: %d\n", numargs);
        }
        count += (numargs + 1);

        numOfRec++;
    }

    /* Update the last rec at the top of file */
    curpos = ftell(fpout);
    if( fgets(buf, BUF_SIZE, fpout) ) {
        buf[BUF_SIZE - 1] = 0;  /* In case string is longer from logs */
        length = strlen(buf);
        memset(buf, ' ', length-1);
        buf[length] = 0;
        fseek(fpout, curpos, SEEK_SET);
        fprintf(fpout, "%s", buf);
    }

    rewind(fpout);
    /* Update last record */
    fprintf(fpout, "%08d\n", numOfRec);
    fseek(fpout, curpos, SEEK_SET);
    fflush(fpout);

#undef BUF_SIZE
    return 0;
}

int main(int argc, char** argv)
{
    int s;
    char *workarea;
    char *platform;
    struct dbg_record dbg_rec;
    unsigned int min_ts;
    int min_rec_num;
    unsigned int rec_num;
    int i;

    progname = argv[0];
    if (argc != 3) {
        usage();
    }

    if ((workarea = getenv("WORKAREA")) == NULL) {
        printf("export WORKAREA\n");
        return -1;
    }

    if ((platform = getenv("ATH_PLATFORM")) == NULL) {
        printf("export ATH_PLATFORM\n");
        return -1;
    }

    /* Get the file name for dbglog header file */
    memset(dbglogfile, 0, FILENAME_LENGTH_MAX);
    strcpy(dbglogfile, workarea);
    strcat(dbglogfile, "/include/");
    strcat(dbglogfile, DBGLOG_FILE);

    /* Get the file name for dbglog id header file */
    memset(dbglogidfile, 0, FILENAME_LENGTH_MAX);
    strcpy(dbglogidfile, workarea);
    strcat(dbglogidfile, "/include/");
    strcat(dbglogidfile, DBGLOGID_FILE);

    /* Get the file name for dbglog input file */
    memset(dbgloginfile, 0, FILENAME_LENGTH_MAX);
    strcpy(dbgloginfile, argv[1]);
    if (!(fpin = fopen(dbgloginfile, "rb"))) {
        perror(dbgloginfile);
        return -1;
    }

    /* Get the file name for dbglog output file */
    memset(dbglogoutfile, 0, FILENAME_LENGTH_MAX);
    strcpy(dbglogoutfile, argv[2]);
    if (!(fpout = fopen(dbglogoutfile, "w+"))) {
        perror(dbglogoutfile);
        return -1;
    }


    /* first 8 bytes are to indicate the last record */
    fseek(fpout, 8, SEEK_SET);
    fprintf(fpout, "\n");

    dbglog_generate_id_tags();
#ifdef DBGLOG_DEBUG
    dbglog_print_id_tags();
#endif /* DBGLOG_DEBUG */

    min_ts = 0xFFFFFFFF;
    rec_num = 0;
    min_rec_num = 0;
    while (!feof(fpin)) {
        if (fread (&dbg_rec, sizeof(dbg_rec), 1, fpin)) {
            if (dbg_rec.ts < min_ts) {
                min_ts = dbg_rec.ts;
                min_rec_num = rec_num;
            }
            rec_num++;
        }
    }

    rewind(fpin);
        
    // Goto the first min record
    fseek(fpin, min_rec_num * sizeof(dbg_rec), SEEK_SET);
    while (!feof(fpin)) {
        if (fread (&dbg_rec, sizeof(dbg_rec), 1, fpin)) {
            decode_debug_rec(&dbg_rec);
        }
    }

    // decode the skipped records
    rewind(fpin);
    for (i=0;i<min_rec_num;i++) {
        if (fread (&dbg_rec, sizeof(dbg_rec), 1, fpin)) {
            decode_debug_rec(&dbg_rec);
        } else {
            break;
        }
    }

    fclose(fpin);
    fclose(fpout);
    close(s);
    return 0;
}

