ifeq ($(CT_OS_TYPE),linux)
ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
obj-m += sdio_mx21_hcd.o
sdio_mx21_hcd-objs := sdio_mx21.o linux/sdio_mx21_os.o linux/sdio_mx21_linux.o
endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
O_TARGET := sdio_mx21_hcd.o 
# need export line for module export
export-objs := linux/sdio_mx21_os.o 
obj-y += sdio_mx21.o sdio_mx21_os.o linux/sdio_mx21_linux2_4.o 
obj-m += sdio_mx21.o linux/sdio_mx21_os.o linux/sdio_mx21_linux2_4.o sdio_mx21_hcd.o
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
  