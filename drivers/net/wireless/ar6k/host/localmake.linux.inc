#
#  Local Makefile includes for tool and kernel source paths
#  ***** This is only a sample, modify the paths for your specific build environment *****
# 
#  Copyright 2004-2006 Atheros Communications, Inc.
#
# When creating a new build type, use the following template:
#  	ifeq ($(ATH_BUILD_TYPE),<platform name>) 
#       ATH_ARCH_CPU_TYPE :=  < cpu architecture >
#       ATH_CROSS_COMPILE_TYPE := < cross compiler path >
#       ATH_LINUXPATH := < kernel source path >
#	endif 
#
# For a complete list of supported platforms, please refer to the documentation
# packaged in the release
#


#ATH_BUILD_TYPE := <Platform (Eg: LOCAL_i686)>
#ATH_BUS_TYPE := <Interconnect type (Eg: SDIO, SPI)>
#ATH_LINUXPATH := <Pointer to kernel source (Eg: /<path>/fc3-i686-2.6.9)>
#ATH_ARCH_CPU_TYPE := <Type of architecture (Eg: arm)>
#ATH_CROSS_COMPILE_TYPE := <Path to the compiler toolchain - Eg: (/<path>/arm_v4t_le-)
#ATH_OS_SUB_TYPE := <Kernel version - (Eg: linux_2_4, linux_2_6)>

#+++FIH+++
ATH_BUILD_TYPE=QUALCOMM_ARM
ATH_BSP_TYPE=MSM7201A_BSP6320
ATH_OS_SUB_TYPE=linux_2_6
ATH_SDIO_STACK_BASE=sdiostack
#---FIH---

#ATHENV
ifeq ($(ATH_BUILD_TYPE), QUALCOMM_ARM)

  # Some common variable
  ATH_ARCH_CPU_TYPE := arm 
  #ATH_CROSS_COMPILE_TYPE := arm-none-linux-gnueabi-
  ATH_BUS_TYPE := SDIO

  # SDIO host driver
  ifeq ($(ATH_BUS_TYPE), SDIO)
    ATH_HC_DRIVERS := 'qualcomm/'
  endif

endif
#ATHENV
