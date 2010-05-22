/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008 QUALCOMM USA, INC.
 * Author: Brian Swetland <swetland@google.com>
 */

#ifndef __ARCH_ARM_MACH_MSM_GPIO_HW_H
#define __ARCH_ARM_MACH_MSM_GPIO_HW_H

#include <mach/msm_iomap.h>

/* see 80-VA736-2 Rev C pp 695-751
**
** These are actually the *shadow* gpio registers, since the
** real ones (which allow full access) are only available to the
** ARM9 side of the world.
**
** Since the _BASE need to be page-aligned when we're mapping them
** to virtual addresses, adjust for the additional offset in these
** macros.
*/

#define GPIO1_REG(off) (MSM_GPIO1_BASE + 0x800 + (off))
#define GPIO2_REG(off) (MSM_GPIO2_BASE + 0xC00 + (off))

#if defined(CONFIG_ARCH_QSD)
#include "gpio_hw-8xxx.h"
#else
#include "gpio_hw-7xxx.h"
#endif

#endif
