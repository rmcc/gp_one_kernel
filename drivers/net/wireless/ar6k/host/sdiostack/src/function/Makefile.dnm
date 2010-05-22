ifeq ($(CT_OS_TYPE),linux)
##LINUX
obj-m += sdio_busdriver.o

sdio_busdriver-objs := sdio_bus.o sdio_function.o sdio_bus_misc.o sdio_bus_events.o linux/sdio_bus_os.o
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


  