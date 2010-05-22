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
ifeq ($(ALL_BUILD),1)
obj-m += sdioplatformdriver/
else 
ifeq ($(FUNCTION_BUILD),1)
# use this to test PDK headers
obj-m += sdioplatformdriver/
else 
ifeq ($(PDK_BUILD),1)
obj-m += sdioplatformdriver/
else
ifeq ($(HDK_BUILD),1)
obj-m += sdioplatformdriver/
endif
endif
endif
endif

endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
endif

include $(CT_OS_TOP_LEVEL_RULE)
endif
endif


