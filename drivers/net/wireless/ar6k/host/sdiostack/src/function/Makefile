ifeq ($(CT_OS_TYPE),linux)

ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
ifeq ($(ALL_BUILD),1)
obj-m += gps/ bluetooth/ memory/ sample/ bench/
else 
ifeq ($(FUNCTION_BUILD),1)
# use this to test PDK headers
obj-m += gps/ bluetooth/ memory/ sample/ bench/
else 
ifeq ($(PDK_BUILD),1)
obj-m += sample/
else
ifeq ($(HDK_BUILD),1)
obj-m += sample/
endif
endif
endif
endif


endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
# reset CFLAGs, for some reason on 2.4.20, the EXTRA_FLAGS is not propogated from our top-level makefile
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
ifeq ($(ALL_BUILD),1)
##subdir-m +=  gps/ bluetooth/ memory/ sample/
subdir-m +=  sample/ memory/ bench/
else 
ifeq ($(PDK_BUILD),1)
subdir-m += sample/
else
ifeq ($(HDK_BUILD),1)
subdir-m += sample/
endif
endif
endif


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

