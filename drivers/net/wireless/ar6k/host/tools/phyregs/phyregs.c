#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/param.h>
#include<fcntl.h>
#include<endian.h>
#include<linux/init.h>
#include<linux/config.h>
#include<linux/module.h>
#include<linux/kernel.h>


unsigned int x[] = { 0x8170, 0x32163b00, 1, 0x8174, 0xaa88ff55, 1, 0x817c, 
                     0x1000d, 1  };

int
main (int argc, char **argv) {

	int fd,  i;
	char phyregfile[MAXPATHLEN];
	char *workarea = NULL;
	if ((workarea = getenv("WORKAREA")) == NULL) {
         fprintf(stderr, "The environment variable \"WORKAREA\" \
                          needs to be specified.\n");
         exit(1);
    }
	sprintf(phyregfile, "%s/host/support/phyregs.bin", workarea);
	
	printf("Creating \"%s \". \n", phyregfile);
	if ((fd = open(phyregfile, O_CREAT|O_WRONLY|O_TRUNC, 00644)) < 0) {
        fprintf(stderr, " Unable to open \n");
        exit(1);
    }
	for(i = 0; i < sizeof(x)/sizeof(x[0]) ; i++)	{
		if (write(fd, &x[i], sizeof(x[0]))< 0 ) { 
			fprintf(stderr, "Failed to write \n");
			close(fd);
			remove(phyregfile);
		}
	}
close(fd);
return 0;
}
