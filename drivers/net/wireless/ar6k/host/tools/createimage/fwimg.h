/*
 * Copyright (c) 2004-2006 Atheros Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _FWIMG_H_
#define _FWIMG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PREPACK
#ifdef __GNUC__
#define PREPACK
#else
#define PREPACK
#endif
#endif

#ifndef POSTPACK
#ifdef __GNUC__
#define POSTPACK __attribute__ ((packed))
#else
#define POSTPACK
#endif
#endif

typedef PREPACK  struct 
{
	/* All values are in network byte order (big endian) */
	unsigned long flags;	
#define FW_FLAG_BMI_OS			0x00000001	/* BMI OS                  */
#define FW_FLAG_BMI_FLASHER		0x00000002	/* BMI Flash application   */
#define FW_FLAG_BMI_FLASHLIB	0x00000004	/* BMI Flash part library  */
#define FW_FLAG_OS				0x00000008	/* OS                      */
#define FW_FLAG_APP				0x00000010	/* WLAN application        */
#define FW_FLAG_DSET			0x00000020	/* Data Set                */
#define FW_FLAG_DSETINDEX		0x00000040	/* Data Set Index          */
#define FW_FLAG_END				0x80000000	/* End                     */
    unsigned long  address;
    unsigned long  data;
    unsigned long  length;
} POSTPACK a_imghdr_t;

#ifdef __cplusplus
}
#endif

#endif
