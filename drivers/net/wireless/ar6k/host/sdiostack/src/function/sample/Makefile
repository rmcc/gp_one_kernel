ifeq ($(CT_OS_TYPE),linux)
ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
obj-m += sdio_sample_fd.o
sdio_sample_fd-objs := sample.o linux/sample_os.o
endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
# reset CFLAGs, for some reason on 2.4.20, the EXTRA_FLAGS is not propogated from our top-level makefile
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
O_TARGET := sdio_sample_fd.o 
# need export line for module export
export-objs := linux/sample_os.o
obj-y += sample.o sample_os.o
obj-m += sample.o linux/sample_os.o sdio_sample_fd.o
include $(CT_OS_TOP_LEVEL_RULE)
endif

endif

ifeq ($(CT_OS_TYPE),qnx)
##QNX
LIST=CPU
ifndef QRECURSE
QRECURSE=recurse.mk
ifdef QCONFIG
QRDIR=$(dir $(QCONFIG))
endif
endif
include $(QRDIR)$(QRECURSE)
endif

  