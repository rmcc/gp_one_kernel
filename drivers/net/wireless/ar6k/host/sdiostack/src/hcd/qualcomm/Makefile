ifeq ($(CT_OS_TYPE),linux)

#+++FIH+++
EXTRA_CFLAGS += -D__linux__
#EXTRA_CFLAGS += -DDEBUG
EXTRA_CFLAGS += -DMSM7201A_BSP6320 -DSLOT_2 
#---FIH---

  obj-y += sdio_qualcomm_hcd.o
  sdio_qualcomm_hcd-objs := sdio_qc_hcd.o linux/sdio_qc_hcd_os.o linux/sdio_std_hcd_os.o
  sdio_qualcomm_hcd-objs += sdio_qc_hcd_ath.o


endif
