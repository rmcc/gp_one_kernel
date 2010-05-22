HOST_ROOT = ../../
OUTPUT_DIR = bin
ifneq ($(KERNELRELEASE),)
# This is the actual kbuild step...

obj-m:= dksdio.o
EXTRA_CFLAGS += -I$(obj)/$(HOST_ROOT)/include -I$(obj)/$(HOST_ROOT)/../include -I$(obj)/$(HOST_ROOT)/bmi/include\
                -I$(obj)/$(HOST_ROOT)/os/linux/include -I$(obj)/$(HOST_ROOT)/htc/AR6000/include \
		-I$(obj)/$(HOST_ROOT)/hif/sdio/ellenlinux/include  \
                -DSDIO -DLINUX -DDEBUG -DHTC_HEADER -DHTC_RAW_INTERFACE -DAR6000REV2
                #-DAR6000REV1  
               # -DLINUX -DCONFIGURE_CLOCK -DHTC_HEADER 

dksdio-objs := ../../htc/AR6000/src/htc.o ../../htc/AR6000/src/htc_send.o \
                ../../htc/AR6000/src/htc_recv.o ../../htc/AR6000/src/htc_utils.o \
                 ../../bmi/src/bmi.o dksdio_drv.o ../../hif/sdio/ellenlinux/src/hif.o ../../htc/AR6000/src/htc_events.o

else
# This sets up the kbuild and invokes make with the proper settings.

KERNEL_SRC := /usr/src/redhat/BUILD/kernel-2.6.9/linux-2.6.9
#KERNEL_SRC := /trees/etna/root/linux
PWD :=  $(shell pwd)
MAKE := make

all: modules

modules:
	$(MAKE) -C $(KERNEL_SRC) SUBDIRS=$(PWD) SDIO_SRC=$(PWD)/../../sdio/ellenlinux/src modules
	mkdir -p $(OUTPUT_DIR)
	cp -f dksdio.ko $(HOST_ROOT)/hif/sdio/ellenlinux/src/modules/sdio_* $(OUTPUT_DIR)

clean:
	rm -rf *.o *.ko ../../htc/AR6000/src/*.o  ../../bmi/src/*.o $(OUTPUT_DIR)

endif
