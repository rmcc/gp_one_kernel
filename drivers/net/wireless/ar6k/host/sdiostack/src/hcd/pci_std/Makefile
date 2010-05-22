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
obj-m += sdio_pcistd_hcd.o

sdio_pcistd_hcd-objs := linux/sdio_hcd_os.o ../stdhost/sdio_std_hcd.o ../stdhost/linux/sdio_std_hcd_os.o
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
  
