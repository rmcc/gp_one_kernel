
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "../../include/linux/sdio_hcd.h"

#define DEVICE_NAME "/dev/sdiobd0"

int main(int argc, char * argv[])
{
    int fd;
    int err;
    char buffer[10];
	char arg;
	
  	if( argc < 2 ) {
    	fprintf(stderr, "Missing argument, -i insert, -r remove, -b get buffer, \n");
		exit(-1);
	}
	arg = argv[1][1];
    
    fd = open(DEVICE_NAME,  O_RDWR | O_NOCTTY);

    if (fd < 0) {
        perror("Could not open device: " DEVICE_NAME);
        exit(-1);
    }
	
	switch(arg) {    
	  case 'b':	/* example */	
	    err = ioctl(fd, SDIO_IOCTL_GET_BUFFER, buffer);
	    if (err < 0) {
	        perror("SDIO_IOCTL_GET_BUFFER failed: ");
	        exit(-1);
	    }
	    printf("buffer[]: %d, %d, %d, %d, %d\n", 
	            buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
	    break;
	  case 'i':	{ /* force card insert */
    	  	int dummy;
    	    err = ioctl(fd, SDIO_IOCTL_FORCE_INSERT, &dummy);
    	    if (err < 0) {
    	        perror("SDIO_IOCTL_FORCE_INSERT: ");
    	        exit(-1);
    	    }
    	    break;
      }
	  case 'r':	{/* force card remove */
            int dummy;
    	    err = ioctl(fd, SDIO_IOCTL_FORCE_REMOVE, &dummy);
    	    if (err < 0) {
    	        perror("SDIO_IOCTL_FORCE_REMOVE: ");
    	        exit(-1);
    	    }
    	    break;
      }
      default:
        fprintf(stderr, "Missing argument, -i insert, -r remove, -b get buffer, \n");
        
	}
	printf(".\n");
    exit(0);
}
