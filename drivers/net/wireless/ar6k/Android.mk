LOCAL_PATH := $(call my-dir)
ath6k_firmware_files := athwlan.bin.z77 data.patch.hw2_0.bin eeprom.bin eeprom.data

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/host/os/linux/ar6000.ko:system/lib/hw/wlan_ar6002/ar6000.ko
copy_file_list := \
        $(foreach f, $(ath6k_firmware_files),\
            $(LOCAL_PATH)/target/$(f):system/lib/hw/wlan_ar6002/$(f))
PRODUCT_COPY_FILES += $(copy_file_list)


# make it default driver
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/host/os/linux/ar6000.ko:system/lib/hw/wlan/ar6000.ko
copy_file_list := \
        $(foreach f,$(ath6k_firmware_files),\
            $(LOCAL_PATH)/target/$(f):system/lib/hw/wlan/$(f))
PRODUCT_COPY_FILES += $(copy_file_list)
