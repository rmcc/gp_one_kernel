ifeq ($(CT_OS_TYPE),linux)
ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
obj-m += sdio_memory_fd.o
sdio_memory_fd-objs := sdio_memory.o linux/sdio_memory_os.o
endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
# reset CFLAGs, for some reason on 2.4.20, the EXTRA_FLAGS is not propogated from our top-level makefile
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
O_TARGET := sdio_memory_fd.o 
# need export line for module export
export-objs := linux/sdio_memory_os.o
obj-y += sdio_memory.o sdio_memory_os.o
obj-m += sdio_memory.o linux/sdio_memory_os.o sdio_memory_fd.o
include $(CT_OS_TOP_LEVEL_RULE)
endif

endif

  