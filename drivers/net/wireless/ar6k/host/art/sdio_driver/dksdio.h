//------------------------------------------------------------------------------
// <copyright file="dksdio.h" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
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
#ifndef __DKSDIO_H
#define __DKSDIO_H
/*
#define AR6000_IOCTL_BMI_DONE               0
#define AR6000_IOCTL_BMI_READ_MEMORY        1
#define AR6000_IOCTL_BMI_WRITE_MEMORY       2
#define AR6000_IOCTL_BMI_EXECUTE            3
#define AR6000_IOCTL_BMI_SET_APP_START      4
#define AR6000_IOCTL_BMI_READ_SOC_REGISTER  5
#define AR6000_IOCTL_BMI_WRITE_SOC_REGISTER 6
#define AR6000_IOCTL_BMI_TEST               7
*/

#define DK_SDIO_MAJOR_NUMBER 65
#define THIS_DEV_NAME  "dksdio"

// undef, (defined in if.h, which will not be used by us)
#undef ifr_data
#undef ifr_name

struct dk_idata {
	char ifr_name[100];
	char *ifr_data;
};

typedef struct dk_idata DK_IDATA;

/*
#ifdef DEBUG
#define A_ASSERT(x)           {if (!(x)) {panic("ASSERT: x\n");}}
#else
#define A_ASSERT(x)
#endif / * DEBUG */

#endif

