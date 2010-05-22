power_supply-objs := power_supply_core.o

ifeq ($(CONFIG_SYSFS),y)
power_supply-objs += power_supply_sysfs.o
endif

ifeq ($(CONFIG_LEDS_TRIGGERS),y)
power_supply-objs += power_supply_leds.o
endif
obj-$(CONFIG_USB_FUNCTION_MSM_HSUSB)	+= msm_hsusb.o
obj-$(CONFIG_USB_FUNCTION_NULL)		+= null.o
obj-$(CONFIG_USB_FUNCTION_NULL)		+= zero.o
obj-$(CONFIG_USB_FUNCTION_LOOPBACK)	+= loopback.o
obj-$(CONFIG_USB_FUNCTION_ADB)		+= adb.o
obj-$(CONFIG_USB_FUNCTION_UMS)		+= ums.o
obj-$(CONFIG_USB_FUNCTION_MASS_STORAGE)	+= mass_storage.o
obj-$(CONFIG_USB_FUNCTION_DIAG)		+= diag.o
obj-$(CONFIG_USB_FUNCTION_SERIAL)       += serial.o
obj-$(CONFIG_USB_FUNCTION_ETHER)	+= ether_cdc_ecm.o
