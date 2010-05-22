-include ../../localmake.linux.inc
ARCH :=$(ATH_ARCH_CPU_TYPE)
KERNEL_SRC :=$(ATH_LINUXPATH)
MAKE := make
CC := $(ATH_CROSS_COMPILE_TYPE)gcc

ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -D# "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

ifeq ($(ATH_BUILD_TYPE),OMAP5912)
CFLAGS += $(DEBFLAGS) -I$(KERNEL_SRC)/include \
	  -Wall -DMODULE -D__KERNEL__ -DEXPORT_SYMTAB -DOMAP5912_SPI
all: ForceROM.o
endif
	
ifneq ($(ATH_BUILD_TYPE),OMAP5912)
CFLAGS += $(DEBFLAGS) -I$(KERNEL_SRC)/include \
	  -Wall -DMODULE -D__KERNEL__ -DEXPORT_SYMTAB -DOMAP2420_SPI
all: ForceROM.o
endif

clean:
	rm -f *.o *~ core .depend
