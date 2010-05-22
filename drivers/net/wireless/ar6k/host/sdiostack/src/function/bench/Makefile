ifeq ($(CT_OS_TYPE),linux)
EXTRA_CFLAGS += -DUSE_64_BIT_DO_DIV

ifneq ($(CT_OS_SUB_TYPE),linux_2_4)
obj-m += sdio_bench_fd.o

sdio_bench_fd-objs := common/benchmark.o \
                      common/benchmem.o  \
                      common/benchsdio.o \
                      common/benchsandbox.o \
                      common/linux/benchmisc.o \
                      emsdio/benchmark_ct.o \
                      emsdio/linux/benchmark_os.o
endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)
EXTRA_CFLAGS += $(CT_PASS_CFLAGS)
O_TARGET := sdio_bench_fd.o
# need export line for module export
export-objs := emsdio/linux/benchmark_os.o
obj-y += common/benchmark.o \
         common/benchmem.o  \
         common/benchsdio.o \
         common/benchsandbox.o \
         common/linux/benchmisc.o \
         emsdio/benchmark_ct.o \
         benchmark_os.o
         
obj-m += benchmark.o \
         benchmem.o  \
         benchsdio.o \
         benchmisc.o \
         benchsandbox.o \
         benchmark_ct.o \
         emsdio/linux/benchmark_os.o \
         sdio_bench_fd.o
              
include $(CT_OS_TOP_LEVEL_RULE)
endif

endif


  