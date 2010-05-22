#
#  Local Makefile includes for tool and kernel source paths
#  Copyright 2004-2006 Atheros Communications, Inc.
#
# When creating a new build type, use the following template:
#  	ifeq ($(CT_BUILD_TYPE),<platform name>) 
#       CT_ARCH_CPU_TYPE :=  < cpu architecture >
#       CT_CROSS_COMPILE_TYPE := < cross compiler path >
#       CT_LINUXPATH := < kernel source path >
#       < Add HCD flags to compile specific HCDs for this platform >
#	endif 
#
#

DEV_BASE :=/usr/dev

CT_BUILD_TYPE := MV_OMAP2420_ARM

ifeq ($(CT_BUILD_TYPE),MV_OMAP2420_ARM) 
# cross compiling to OMAP
CT_ARCH_CPU_TYPE :=arm  
CT_CROSS_COMPILE_TYPE :=$(DEV_BASE)/linux/tools/gcc/arm/mv_3.3.1_v5t/bin/arm_v5t_le-
CT_LINUXPATH :=$(DEV_BASE)/linux/kernels/mv-omap-2420-linux-2.4.20
CT_OUTPUT_FLAGS :=
CT_OS_SUB_TYPE :=linux_2_4
CT_HC_DRIVERS :=omap2420_spi2/
endif
