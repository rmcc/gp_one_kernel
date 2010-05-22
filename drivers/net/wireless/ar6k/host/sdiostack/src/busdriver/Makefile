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

#+++FIH+++
EXTRA_CFLAGS += -D__linux__ -DDEBUG
#---FIH---

##LINUX
ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
obj-y += sdio_busdriver.o
sdio_busdriver-objs := sdio_bus.o sdio_function.o sdio_bus_misc.o sdio_bus_events.o linux/sdio_bus_os.o
endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
# reset CFLAGs, for some reason on 2.4.20, the EXTRA_FLAGS is not propogated from our top-level makefile
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
O_TARGET := sdio_busdriver.o 
# need export line for module export
export-objs := linux/sdio_bus_os.o
obj-y += sdio_bus.o sdio_function.o sdio_bus_misc.o sdio_bus_events.o sdio_bus_os.o
obj-m += sdio_bus.o sdio_function.o sdio_bus_misc.o sdio_bus_events.o linux/sdio_bus_os.o sdio_busdriver.o
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


  
