#ifndef __ASM_ARCH_MSM_RPC_PMIC_H
#define __ASM_ARCH_MSM_RPC_PMIC_H

#include <mach/msm_rpcrouter.h>

#define PMIC_RPC_API_VERS_COMP			0x00010001

typedef enum {
	PM_LCD_LED,
	PM_KBD_LED,
	PM_INVALID,
} pm_led_intensity_type;

#endif
