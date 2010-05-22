ifeq ($(CT_OS_TYPE),linux)
obj-m += sdio_gps_fd.o
sdio_gps_fd-objs := gps.o linux/gps_os.o
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


  