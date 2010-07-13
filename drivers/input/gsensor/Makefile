#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

#ChihChia 2009.04.01 FIH_ADQ
obj-$(CONFIG_SENSORS_BMA020)	+= BMA020.o


#ChihChia 2009.04.01 FIH_ADQ
ifeq ($(CONFIG_GSENSORS_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
