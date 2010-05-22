ifeq ($(CT_OS_TYPE),linux)
obj-m += sdio_modem_fd.o
sdio_modem_fd-objs := modem.o linux/modem_os.o
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


  