
CROSS_COMPILE=arm-linux-

default:
ifeq ($(LOCAL), 1)
	gcc testhcd.c -o testhcd
else 
	$(CROSS_COMPILE)gcc testhcd.c -o testhcd
endif
	