ifeq ($(CT_OS_TYPE),linux)
ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
obj-m += sdio_omap_hcd.o
sdio_omap_hcd-objs := sdio_hcd.o linux/sdio_hcd_os.o linux/sdio_hcd_os_2_6.o
endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
# reset CFLAGs, for some reason on 2.4.20, the EXTRA_FLAGS is not propogated from our top-level makefile
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
O_TARGET := sdio_omap_hcd.o 
# need export line for module export
export-objs := linux/sdio_hcd_os.o
obj-y += sdio_hcd.o sdio_hcd_os.o linux/sdio_hcd_os_2_4.o linux/slot_transceiver.o
obj-m += sdio_hcd.o linux/sdio_hcd_os.o linux/sdio_hcd_os_2_4.o linux/slot_transceiver.o sdio_omap_hcd.o
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
  