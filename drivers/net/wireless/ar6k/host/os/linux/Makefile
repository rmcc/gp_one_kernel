#------------------------------------------------------------------------------
# <copyright file="makefile" company="Atheros">
#    Copyright (c) 2005-2008 Atheros Corporation.  All rights reserved.
# 
# The software source and binaries included in this development package are
# licensed, not sold. You, or your company, received the package under one
# or more license agreements. The rights granted to you are specifically
# listed in these license agreement(s). All other rights remain with Atheros
# Communications, Inc., its subsidiaries, or the respective owner including
# those listed on the included copyright notices.  Distribution of any
# portion of this package must be in strict compliance with the license
# agreement(s) terms.
# </copyright>
# 
# <summary>
# 	Wifi driver for AR6002
# </summary>
#
#------------------------------------------------------------------------------
#==============================================================================
# Author(s): ="Atheros"
#==============================================================================
#######################################################################################
#  AR6K Kernel Module makefile.
#
#    This makefile is invoked by the master makefile in the linux kernel, the linux kernel
#    source must be initially compiled.  The top level make will invoke the main linux kernel
#    makefile which will subsequently pick up this makefile.
#
#######################################################################################

#+++FIH+++
#ATH_SRC_BASE :=$(CUR_PATH)../..
ATH_SRC_BASE :=drivers/net/wireless/ar6k/host
#---FIH---

REV ?= 2

EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/../include
# TBDXXX....handle include/AR6002
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/../include/AR6001
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/wlan/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/os/linux/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/os/
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/bmi/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/hif/sdio/linux_athsdio/include
EXTRA_CFLAGS += -I$(ATH_SRC_BASE)/sdiostack/src/include


EXTRA_CFLAGS += -DLINUX -D__KERNEL__ -DHTC_RAW_INTERFACE\
                -DTCMD -DSEND_EVENT_TO_APP -DUSER_KEYS \
                -DNO_SYNC_FLUSH #\
                -DMULTIPLE_FRAMES_PER_INTERRUPT -DAR6000REV$(REV) \
                -DBLOCK_TX_PATH_FLAG \

#EXTRA_CFLAGS += -DDEBUG

EXTRA_CFLAGS += -D__linux__
ifneq ($(ATH_OS_SUB_TYPE),linux_2_4)
# linux 2.6 and higher kernel module build settings
EXTRA_CFLAGS += -DKERNEL_2_6
#+++FIH+++
obj-y += ../../sdiostack/src/ 
#---FIH---
obj-y += ar6000.o
# -include $(ATH_SRC_BASE)/hif/$(ATH_HIF_TYPE)/Makefile
ar6000-objs := ../../hif/sdio/linux_athsdio/src/hif.o
# ATHENV
ar6000-objs += ../../htc2/AR6000/ar6k.o        \
			   ../../htc2/AR6000/ar6k_events.o \
               ../../htc2/htc_send.o       \
               ../../htc2/htc_recv.o       \
               ../../htc2/htc_services.o   \
               ../../htc2/htc.o     		  \
               ../../bmi/src/bmi.o            \
               ar6000_drv.o           			\
               ar6000_raw_if.o					\
               netbuf.o							\
               wireless_ext.o					\
               ioctl.o							\
               engine.o							\
               eeprom.o							\
               ../../miscdrv/common_drv.o		\
               ../../miscdrv/credit_dist.o					\
               ../../wmi/wmi.o                       \
               ../../wlan/src/wlan_node.o            \
               ../../wlan/src/wlan_recv_beacon.o     \
               ../../wlan/src/wlan_utils.o			 
# ATHENV

endif

ifeq ($(ATH_OS_SUB_TYPE),linux_2_4)
# linux 2.4 kernel module build settings
EXTRA_CFLAGS += -DKERNEL_2_4

O_TARGET := ar6000.o
# need export line for module export
export-objs := ar6000_drv.o
-include $(ATH_SRC_BASE)/hif/$(ATH_HIF_TYPE)/Makefile
obj-y += $(ATH_SRC_BASE)/htc2/htc.o \
         $(ATH_SRC_BASE)/htc2/htc_send.o \
         $(ATH_SRC_BASE)/htc2/htc_recv.o  \
         $(ATH_SRC_BASE)/htc2/htc_services.o \
         $(ATH_SRC_BASE)/htc2/AR6000/ar6k.o  \
         $(ATH_SRC_BASE)/htc2/AR6000/ar6k_events.o  \
         $(ATH_SRC_BASE)/bmi/src/bmi.o \
         $(ATH_SRC_BASE)/wmi/wmi.o \
         $(ATH_SRC_BASE)/wlan/src/wlan_node.o \
         $(ATH_SRC_BASE)/wlan/src/wlan_recv_beacon.o \
         $(ATH_SRC_BASE)/wlan/src/wlan_utils.o \
         $(ATH_SRC_BASE)/os/linux/ar6000_drv.o \
         $(ATH_SRC_BASE)/os/linux/ar6000_raw_if.o \
         $(ATH_SRC_BASE)/os/linux/wireless_ext.o \
         $(ATH_SRC_BASE)/os/linux/ioctl.o 		\
         $(ATH_SRC_BASE)/os/linux/netbuf.o     \
         $(ATH_SRC_BASE)/miscdrv/common_drv.o   \
         $(ATH_SRC_BASE)/miscdrv/credit_dist.o

obj-m += htc.o \
         htc_send.o \
         htc_recv.o \
         htc_services.o  \
         ar6k_events.o \
         ar6k.o \
         hif.o  \
         bmi.o  \
         wmi.o  \
         wlan_node.o \
         wlan_recv_beacon.o \
         wlan_utils.o \
         ar6000_drv.o \
         ar6000_raw_if.o \
         common_drv.o \
         credit_dist.o \
         netbuf.o     \
         wireless_ext.o \
         ioctl.o        \
         ar6000.o
              
include $(ATH_LINUXPATH)/Rules.make
endif
