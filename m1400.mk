$(call inherit-product,$(SRC_TARGET_DIR)/product/generic_x86.mk)

PRODUCT_NAME := motion_m1400
PRODUCT_DEVICE := m1400
PRODUCT_MANUFACTURER := motion
PRODUCT_MODEL := Motion M1400
PRODUCT_BRAND := Motion
PRODUCT_PACKAGE_OVERLAYS := device/motion/m1400/overlays
LOCAL_KEYMAPS_DIR := $(LOCAL_PATH)/keylayout

TARGET_STRIP := 1 
USE_SQUASHFS := 0

#PRODUCT_COPY_FILES += \
#	$(LOCAL_PATH)/init.x41t.sh:system/etc/init.x41t.sh
#	$(LOCAL_PATH)/asound.conf:system/etc/asound.conf

PRODUCT_COPY_FILES += \
 	$(LOCAL_PATH)/keylayout/m1400_keyboard.kl:system/usr/keylayout/m1400_keyboard.kl \
	$(LOCAL_PATH)/excluded-input-devices.xml:system/etc/excluded-input-devices.xml

PRODUCT_PROPERTY_OVERRIDES += \
	dalvik.vm.heapsize=32m \
	ro.sf.lcd_density=240 \
	wifi.interface=eth1 \
	wifi.supplicant_scan_interval=120

PRODUCT_PACKAGES_EXCLUDE := 
	

#include $(call all-subdir-makefiles)
