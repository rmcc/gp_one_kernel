// Copyright (c) 2004, 2005 Atheros Communications Inc.
// 
//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sample_app.h

@abstract: SDIO Sample Function Driver - ioctl definitions

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#ifndef __SDIO_SAMPLE_APP_H___
#define __SDIO_SAMPLE_APP_H___

/* example structures for data transfer */
struct sdio_sample_args {
    unsigned char testindex;    /* test index 1-15, 0 is reserved as default */
    unsigned int  reg;          /* register to read/write */
    unsigned char argument;     /* data argument */
};

#define SD_SAMPLE_BUFFER_SIZE 5
struct sdio_sample_buffer {
    unsigned char testindex;    /* test index 1-15, 0 is reserved as default */
    unsigned int  reg;           /* register to read/write */
    unsigned char argument[SD_SAMPLE_BUFFER_SIZE]; /* data arguments */
};


#define SDIO_IOCTL_SAMPLE_BASE     's'
// PUT/GET CMD52
#define SDIO_IOCTL_SAMPLE_PUT_CMD _IOW(SDIO_IOCTL_SAMPLE_BASE,  1, struct sdio_sample_args)
#define SDIO_IOCTL_SAMPLE_GET_CMD _IOR(SDIO_IOCTL_SAMPLE_BASE,  2, struct sdio_sample_args)

// PUT/GET CMD53
#define SDIO_IOCTL_SAMPLE_PUT_BUFFER _IOW(SDIO_IOCTL_SAMPLE_BASE,  3, struct sdio_sample_buffer)
#define SDIO_IOCTL_SAMPLE_GET_BUFFER _IOR(SDIO_IOCTL_SAMPLE_BASE,  4, struct sdio_sample_buffer)

#endif  /* __SDIO_SAMPLE_APP_H___ */
