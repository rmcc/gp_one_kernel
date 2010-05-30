###ifeq ($(CONFIG_ARCH_QSD),y)
###  zreladdr-y		:= 0x16008000
###params_phys-y		:= 0x16000100
###initrd_phys-y		:= 0x1A000000
###else
###  zreladdr-y		:= 0x10008000
###params_phys-y		:= 0x10000100
###initrd_phys-y		:= 0x10800000
###endif
###+++FIH_ADQ+++
##zreladdr-y		:= 0x00208000
##params_phys-y		:= 0x00200100
##initrd_phys-y		:= 0x00A00000
## PHYS_OFFSET == 0x1A000000 
  zreladdr-y		:= 0x1A008000
params_phys-y		:= 0x1A000100
initrd_phys-y		:= 0x1A800000
###---FIH_ADQ---
