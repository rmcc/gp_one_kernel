/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008 QUALCOMM USA, INC.
 * Author: Brian Swetland <swetland@google.com>
 */

#ifndef __ASM_ARCH_MSM_IRQS_H
#define __ASM_ARCH_MSM_IRQS_H

#define MSM_IRQ_BIT(irq)     (1 << ((irq) & 31))

#define NR_MSM_IRQS 64
#define NR_BOARD_IRQS 64

#if defined(CONFIG_ARCH_QSD)
#include "irqs-8xxx.h"
#include "sirc.h"
#else
#include "irqs-7xxx.h"
#endif

#define NR_IRQS (NR_MSM_IRQS + NR_GPIO_IRQS + NR_BOARD_IRQS)

#define MSM_GPIO_TO_INT(n) (NR_MSM_IRQS + (n))

#endif
