/* linux/include/asm-arm/arch-msm/msm_smd.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MSM_SMD_H
#define __ASM_ARCH_MSM_SMD_H

typedef struct smd_channel smd_channel_t;

/* warning: notify() may be called before open returns */
int smd_open(const char *name, smd_channel_t **ch, void *priv,
	     void (*notify)(void *priv, unsigned event));

#define SMD_EVENT_DATA 1
#define SMD_EVENT_OPEN 2
#define SMD_EVENT_CLOSE 3

int smd_close(smd_channel_t *ch);

/* passing a null pointer for data reads and discards */
int smd_read(smd_channel_t *ch, void *data, int len);
int smd_read_from_cb(smd_channel_t *ch, void *data, int len);

/* Write to stream channels may do a partial write and return
** the length actually written.
** Write to packet channels will never do a partial write --
** it will return the requested length written or an error.
*/
int smd_write(smd_channel_t *ch, const void *data, int len);

int smd_write_avail(smd_channel_t *ch);
int smd_read_avail(smd_channel_t *ch);

/* Returns the total size of the current packet being read.
** Returns 0 if no packets available or a stream channel.
*/
int smd_cur_packet_size(smd_channel_t *ch);


#if 0
/* these are interruptable waits which will block you until the specified
** number of bytes are readable or writable.
*/
int smd_wait_until_readable(smd_channel_t *ch, int bytes);
int smd_wait_until_writable(smd_channel_t *ch, int bytes);
#endif

/* these are used to get and set the IF sigs of a channel.
 * DTR and RTS can be set; DSR, CTS, CD and RI can be read.
 */
int smd_tiocmget(smd_channel_t *ch);
int smd_tiocmset(smd_channel_t *ch, unsigned int set, unsigned int clear);

enum {
	SMD_APPS_MODEM = 0,
	SMD_APPS_QDSP,
	SMD_MODEM_QDSP,
	SMD_APPS_DSPS,
	SMD_MODEM_DSPS,
	SMD_QDSP_DSPS,
	SMD_LOOPBACK_TYPE = 100,

};

int smd_named_open_on_edge(const char *name, uint32_t edge, smd_channel_t **_ch,
			   void *priv, void (*notify)(void *, unsigned));

// +++ FIH_ADQ +++
typedef enum 
{
    CMCS_HW_VER_EVB1=0,  
    CMCS_HW_VER_EVB2,    //20k resister
    CMCS_HW_VER_EVB3,
    CMCS_HW_VER_EVB4,
    CMCS_HW_VER_EVB5,
    CMCS_HW_VER_EVB6,
    CMCS_HW_VER_EVB7,
    CMCS_HW_VER_EVB8,
    CMCS_HW_VER_EVB9,
    CMCS_HW_VER_IVT,     //10k resister
    CMCS_HW_VER_EVT1,    //30k resister    
    CMCS_HW_VER_EVT2,    //40k resister
    CMCS_HW_VER_EVT3,
    CMCS_HW_VER_EVT4,
    CMCS_HW_VER_EVT5,
    CMCS_HW_VER_DVT1,    //68.2k resister, ZEUS_CR_1199
    CMCS_HW_VER_DVT2,
    CMCS_HW_VER_DVT3,
    CMCS_HW_VER_DVT4,
    CMCS_HW_VER_DVT5,
    CMCS_HW_VER_PVT,
    CMCS_HW_VER_MP,
    CMCS_HW_VER_MAX    
}cmcs_hw_version_type;

/* Temporary. ADQ/Zeus is DVT1, keep this here until all calls have been cleaned up */
#ifdef CONFIG_MACH_ADQ
#define FIH_READ_HWID_FROM_SMEM()	15 
#endif

// --- FIH_ADQ ---


#endif
