/*
 * Copyright (c) 2004-2005 Atheros Communications Inc.
 * All rights reserved.
 *
 * $Id: //depot/sw/releases/olca2.1-RC/host/tools/wlan-dset/wlan_dset_gen.c#1 $
 * $DateTime: 2008/03/03 10:26:54 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <err.h>
#include <errno.h>

#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#include "wlan_dset.h"

const char *progname;
const char commands[] =
"commands:\n\
--pin=<gpio pin #>\n\
--gpio <enable/disable>\n\
--fname=<filename for bin file>\n\
The options can be given in any order";

void generate_bin_file(char *pFileName, A_BOOL enable, A_UINT16 pin);

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}


int main(int argc, char **argv)
{
    int c, s, fd;
    char filename[128];
    A_BOOL gpio_enable = FALSE;
    A_UINT16 pin = 0;
    progname = argv[0];
    int flag;
    fd = 0;

    if (argc == 1) usage();

    flag = 0;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) err(1, "socket");

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"pin", 1, NULL, 'p'},
            {"gpio", 1, NULL, 'g'},
            {"fname", 1, NULL, 'f'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "p:g:f:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch(c) {
            case 'p': pin = atoi(optarg);
                      break;
            case 'f': memset(filename, '\0', 128);
                      strcpy(filename, optarg);
                      break;
            case 'g': if (!strcmp(optarg,"enable")) {
                            gpio_enable = TRUE;
                      } else if (!strcmp(optarg, "disable")) {
                            gpio_enable = FALSE;
                      }
                      break;
            default: break;
        }
    }

    printf("Generating : %s\n", filename);
    generate_bin_file(filename, gpio_enable, pin);

    return 0;
}

void print_dset(WOW_CONFIG_DSET *dset)
{
    printf("DSET\n");
    printf("valid = %1d\n", dset->valid_dset);
    printf("gpio_enable = %1d\n", dset->gpio_enable);
    printf("pin = %2d\n", dset->gpio_pin);
}

void
generate_bin_file(char *pFileName, A_BOOL enable, A_UINT16 pin)
{
    WOW_CONFIG_DSET dset, read_dset;    

    dset.valid_dset = TRUE;
    dset.gpio_enable = enable;
    dset.gpio_pin = pin;

    printf("This is the dataset I read\n");
    print_dset(&dset);

    printf("pin=%2d gpio=%s fname=%s\n", pin, 
            ((enable==TRUE)?"enable":"disable"), pFileName);
    
     FILE *fptr = fopen(pFileName, "w+b");
     if (!fptr) {
		printf("ERROR: creating file...\n");
		assert(0);
     }

     printf("Wrote %2d items\n", (fwrite(&dset,sizeof(dset), 1, fptr)));
     fclose(fptr);

     fptr = fopen(pFileName, "r");
     if (!fptr) {
		printf("ERROR: opening file...\n");
                assert(0);
    }
    do {
        
        fread(&read_dset, sizeof(read_dset), 1, fptr); 
        printf("This is the dataset I wrote\n");
        print_dset(&read_dset);  
        if (feof(fptr)) 
            break;       
    } while(!feof(fptr));

    fclose(fptr);
}
