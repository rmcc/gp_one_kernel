# Copyright 2004-2006 Atheros Communications Inc.
# 
#
#The software source and binaries included in this development package are
#licensed, not sold. You, or your company, received the package under one
#or more license agreements. The rights granted to you are specifically
#listed in these license agreement(s). All other rights remain with Atheros
#Communications, Inc., its subsidiaries, or the respective owner including
#those listed on the included copyright notices.  Distribution of any
#portion of this package must be in strict compliance with the license
#agreement(s) terms.
#
#
ifeq ($(CT_OS_TYPE),linux)

ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
# For Linux 2.6 and higher

#+++FIH+++
#obj-m += $(CT_HC_DRIVERS) 
obj-y += qualcomm/
#---FIH---

endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
# reset CFLAGs, for some reason on 2.4.20, the EXTRA_FLAGS is not propogated from our top-level makefile
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
subdir-m += $(CT_HC_DRIVERS)  
include $(CT_OS_TOP_LEVEL_RULE)
endif

endif  

ifeq ($(CT_OS_TYPE),qnx)
##QNX
LIST=VARIANT
ifndef QRECURSE
QRECURSE=recurse.mk
ifdef QCONFIG
QRDIR=$(dir $(QCONFIG))
endif
endif
include $(QRDIR)$(QRECURSE)
endif
