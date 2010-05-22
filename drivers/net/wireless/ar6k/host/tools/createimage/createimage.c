/*
 * Copyright (c) 2005 Atheros Communications Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<time.h>
#include<sys/param.h>
#include <netinet/in.h>
#include "fwimg.h"

const char  explain[] ="commands: createimage imagename OR  createimage -f pathname imagename \n\
 imagename corresponds to name of the image to create \n\
 If no parameter are specified, the configuration file is read from $WORKAREA/host/support/createimage.txt \n\
 				OR \n\
2. Specify \"-f\" followed by the filename for different location of configuration file. \n\
 Refer $WORKAREA/host/support/createimage.txt for more details on the format";


void
usage(void)
{
	printf(" %s\n", explain);
	exit(-1);
}
/* Return the length of an open file. */
unsigned int
file_length(int fd)
{
    struct stat filestat;
    int length;

    memset(&filestat, '\0', sizeof(struct stat));
    fstat(fd, &filestat);
    length = filestat.st_size;

    return length;
}

int
create_dset_file(unsigned int dset_addr, char* outfile)
{
    char mkimg_loc[MAXPATHLEN];
    char dsetfile[MAXPATHLEN];
    char cmdbuf[3*MAXPATHLEN];
    char *workarea = getenv("WORKAREA");
    char *platform= NULL;
    
    if ((platform = getenv("ATH_PLATFORM")) == NULL) {
        fprintf(stderr, "The environment variable \"ATH_PLATFORM\" \
                 needs to be specified.\n");
        return -1;
    }
    
    sprintf(mkimg_loc, "%s/host/.output/%s/image/mkdsetimg", workarea, platform);             
    sprintf(dsetfile, "%s/host/support/dsets.txt", workarea);
    sprintf(cmdbuf, "%s --desc=%s --out=%s --start=0x%x\n", mkimg_loc, dsetfile,
            outfile, dset_addr);
    
    printf("Executing mkdsetimg file as :\n %s\n", cmdbuf);
    if (system(cmdbuf) == -1) {
        perror("failed to execute mkdsetimg.\n");
        return -1;
    }
    return 0;
}

int 
copy_image(int fd,  unsigned int flags, unsigned int address,
	unsigned int data, char * filename)
{
	a_imghdr_t head1;
	int fd_img;
	char * mem = NULL;
    int len, ret = -1;
    char outfile[MAXPATHLEN];
    
    outfile[0] = '\0';           
	head1.flags = htonl(flags);
	head1.address = htonl(address);
	head1.data = htonl(data);

	printf("Filename = \"%s\".\n", filename);
	printf("Flags 0x%08x Address 0x%08x Data 0x%08x.\n", flags, address, data);

	if(strcmp(filename, "-") == 0) {
        sprintf(outfile, "/tmp/dataset.flash.bin.%d", rand());
        
        if (create_dset_file(address, outfile) != 0)
        {
            remove(outfile);
            return -1;
        }
        filename = outfile;
	}

	if ((fd_img =open(filename, O_RDONLY,0)) < 0 ) {
		fprintf(stderr, "Unable to open \"%s\".\n", filename);
		goto error;
	}
	
    len = file_length(fd_img);
    printf("Size %d (%d).\n", len, sizeof(head1));
	head1.length = htonl(len);
	
    mem =(char *)malloc(len);
	if (mem==NULL) {
		fprintf(stderr, "malloc failed\n");
		goto error;
	}
	if ((read(fd_img, mem, len)) < 0) {
		fprintf(stderr, "Unable to read the file \"%s\". \n", filename);
	    goto error;
	}
	if ((write(fd, &head1, sizeof(head1))) < 0) {
		fprintf(stderr, "Unable write the header fields to the file. \n");
		goto error;
	}
	if ((write(fd, mem, len)) < 0) {
		fprintf(stderr, "Failed to write \"%s\" to the file. \n", 
				filename);
		goto error;
	}

    ret = 0;
error:
    if (mem) {
	   free(mem);
    }
    if (outfile[0] != '\0')
    {
        close(fd_img);
        remove(outfile);
    }
    return ret;
}

/* Indicate the end of image file */
int 
write_end(int fd)
{
	a_imghdr_t head1;

	head1.flags = htonl(FW_FLAG_END);
	head1.address = htonl(0);
	head1.data = htonl(0);
	head1.length = htonl(0);
	if ((write(fd, &head1, sizeof(head1))) < 0) {
		fprintf(stderr, "Unable write the header fields to the file. \n");
		return -1;
	}	
	return 0 ;
}

int main(int argc, char ** argv)
{
	FILE * filep = NULL;
	int fd = 0,skipparam = 0;
	unsigned int flags;
	unsigned int data;
	unsigned int address;
	char input[LINE_MAX];
	char filename[MAXPATHLEN];
    char component_file[MAXPATHLEN];
	char *workarea= NULL;
	char paramfile[MAXPATHLEN];
	char *p, *s, *d, *image_name = NULL;

	if (!(argc == 4 || argc == 2)) {
		printf(" Incorrect number of arguments \n");
		usage();
	}
	if(argc == 4) {
		if (strcmp(argv[1], "-f") == 0) {
			strcpy(paramfile, argv[2]);
			skipparam = 1;
		} else {
			printf("Invalid argument\n");
			usage();
		}
        image_name = argv[3];
	}
    else {
        image_name = argv[1];
    }

	printf("Creating \"%s\".\n", image_name);
	
	if(skipparam ==0) {
		if ((workarea = getenv("WORKAREA")) == NULL) {
		 		fprintf(stderr, "The environment variable \"WORKAREA\" \
                         needs to be specified.\n");
		        goto error; 
		}
		printf("Reading the configuration file from \"%s/host/support/createimage.txt\". \n", workarea);
	sprintf(paramfile, "%s/host/support/createimage.txt", workarea);
	}
	if ((filep = fopen(paramfile, "r")) == NULL) {
	 	fprintf(stderr, "Unable to open \"%s\".\n", paramfile);
		goto error;
	}
	/* Create/open image file */
	if ((fd = open(image_name, O_CREAT|O_WRONLY|O_TRUNC, 00644)) < 0) {
		fprintf(stderr, " Unable to open \n");
		goto error;
	}
	/* Read each line in configuration file and populate the image file 
       with specified header values and binary file */		
  	while (fgets(input,LINE_MAX,filep) != NULL) {	
		if ((input[0] =='#')||(input[0] =='\n')) {
			continue; /* ignore any commented or newlines in the file  */
		}
		sscanf(input,"%x %x %d %s", &flags, &address, &data, filename);

		/* The filename can referenceded a be gainst the WORKAREA variable
           or complete path name  */
		if ((p = strstr(filename, "$WORKAREA")) != NULL) {
			if (workarea == NULL) {
				if ((workarea = getenv("WORKAREA")) == NULL) {
					fprintf(stderr, "The environment variable \"WORKAREA\"  \
                            needs to be specified.\n");
		    	     goto error ; 
				}else {
					printf(" WORKAREA is set to \"%s\" \n", workarea);
				}
			}
			s = &filename[0];
			d = &component_file[0];
			*p = '\0';
			strcpy(d, s);
			d += strlen(s);
			strcpy(d, workarea);
			d += strlen(workarea);
			p += 9; /* strlen("$WORKAREA") */
			strcpy(d, p);
		}	 
		else {
			strcpy(component_file, filename);
		}
		if (copy_image(fd, flags, address, data, component_file) != 0) {
				goto error;
		}
    }
	if (write_end(fd) != 0 ) {
		goto error;
	}
	close(fd);
    fclose(filep);
	exit(0);

error:
	remove(image_name);
	exit(1);
}
