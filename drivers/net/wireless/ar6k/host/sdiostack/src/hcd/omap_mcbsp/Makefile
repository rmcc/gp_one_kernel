ifeq ($(CT_OS_TYPE),linux)
obj-m += sdio_omapmcbsp_hcd.o

sdio_omapmcbsp_hcd-objs := sdio_hcd.o linux/sdio_hcd_os.o ../SpiLib/spilib.o
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
  