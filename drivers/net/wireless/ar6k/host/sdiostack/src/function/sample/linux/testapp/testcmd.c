/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: testcmd.c

@abstract: SDIO Sample test application

#notes: 
 * simple test application to pass commands to SDIO Sample function driver sdio_sample_fd.ko
 * 
 * Requires a dev entry of /dev/sdiosam0
 *  after the device is found, run: cat /proc/devices
 *  look for major number for sdiosam
 *  mknod /dev/sdiosam0 c majornumber 0
 *      (alternately udev can be setup to handle this automatically)
 * 
@notice: Copyright (c), 2004,2005 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "../sample_app.h"

static void argHelp(void);

#define DEVICE_NAME "/dev/sdiosam0"
 
int GetTestIndex(char * argv[], int argc) 
{
    int i;
    int index = 0;
    
    for (i = 0; i < argc; i++) {
        if ((argv[i][0] == '-') && (argv[i][1] == 'i')) {
            if (argv[i][2] != 0) {
                sscanf(&argv[i][2], "%d", &index);  
            }     
        }
    }
    
    return index;
}


int main(int argc, char * argv[])
{
    int fd;
    int err;
	char arg;
	
  	if( argc < 3 ) {
        argHelp();
		exit(-1);
	}
	arg = argv[1][1];
    
    fd = open(DEVICE_NAME,  O_RDWR | O_NOCTTY);

    if (fd < 0) {
        perror("Could not open device: " DEVICE_NAME);
        exit(-1);
    }
	
	switch(arg) {    
	  case 'r': {	/* read byte */	
        struct sdio_sample_args data;
        memset (&data, 0, sizeof(data));
        sscanf(argv[2], "%d", &data.reg);
        data.testindex = GetTestIndex(argv, argc);        
	    err = ioctl(fd, SDIO_IOCTL_SAMPLE_GET_CMD, &data);
	    if (err < 0) {
	        perror("SDIO_IOCTL_SAMPLE_GET_CMD failed: ");
	        exit(-1);
	    }
	    printf("register %d = %d\n", data.reg, (unsigned int)data.argument); 
	    break;
      }
      case 'w': {   /* write byte */ 
        struct sdio_sample_args data;
        memset (&data, 0, sizeof(data));
        if( argc < 4 ) {
            argHelp();
            exit(-1);
        }
        sscanf(argv[2], "%d", &data.reg);
        sscanf(argv[3], "%d", &data.argument);
        data.testindex = GetTestIndex(argv, argc);
        err = ioctl(fd, SDIO_IOCTL_SAMPLE_PUT_CMD, &data);
        if (err < 0) {
            perror("SDIO_IOCTL_SAMPLE_PUT_CMD failed: ");
            exit(-1);
        }
        printf("register %d, %d written\n", data.reg, (unsigned int)data.argument); 
        break;
      }
      case 'a': {   /* read bytes */ 
        struct sdio_sample_buffer data;
        memset (&data, 0, sizeof(data));
        sscanf(argv[2], "%d", &data.reg);
        data.testindex = GetTestIndex(argv, argc);
        err = ioctl(fd, SDIO_IOCTL_SAMPLE_GET_BUFFER, &data);
        if (err < 0) {
            perror("SDIO_IOCTL_SAMPLE_GET_BUFFER failed: ");
            exit(-1);
        }
        printf("register %d = %d, %d, %d, %d, %d\n", data.reg, 
            (unsigned int)data.argument[0],(unsigned int)data.argument[1],
            (unsigned int)data.argument[2],(unsigned int)data.argument[3],
            (unsigned int)data.argument[4]); 
        break;
      }
      case 'b': {   /* read bytes */ 
        struct sdio_sample_buffer data;
        memset (&data, 0, sizeof(data));
        int ii;
        if( argc < 4 ) {
            argHelp();
            exit(-1);
        }
        sscanf(argv[2], "%d", &data.reg);
        for (ii = 3; ii < argc; ii++) {
            sscanf(argv[ii], "%d", &data.argument[ii]);
        }
        data.testindex = GetTestIndex(argv, argc);
        err = ioctl(fd, SDIO_IOCTL_SAMPLE_PUT_BUFFER, &data);
        if (err < 0) {
            perror("SDIO_IOCTL_SAMPLE_PUT_BUFFER failed: ");
            exit(-1);
        }
        printf("register %d written\n", data.reg); 
        break;
      }

      default:
        argHelp();
        
	}
	printf(".\n");
    exit(0);
}

static void argHelp(void) 
{
    fprintf(stderr, "Arguments, -r register [-iX] - read byte from register\n"
                    "           -w register data [-iX] - write byte to register\n"
                    "           -a register  [-iX]     - read bytes from register\n"
                    "           -b register data ... data [-iX] - write bytes to register (max. 5 bytes)\n"
                    "       All input data is decimal.  \n"
                    "       The optional -i switch followed by a decimel number directs the \n"
                    "       driver to use a special test case indexed by X. \n");
}
