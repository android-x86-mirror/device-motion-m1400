PRODUCT_PACKAGES := $(THIRD_PARTY_APPS)

$(call inherit-product,$(SRC_TARGET_DIR)/product/generic_x86.mk)

PRODUCT_NAME := motion_m1400
PRODUCT_DEVICE := m1400
PRODUCT_MANUFACTURER := Motion
#PRODUCT_PACKAGE_OVERLAYS := device/viewsonic/viewpad10/overlays

TARGET_STRIP := 1 
USE_SQUASHFS := 0

#PRODUCT_COPY_FILES += \
#	$(LOCAL_PATH)/init.x41t.sh:system/etc/init.x41t.sh
#	$(LOCAL_PATH)/asound.conf:system/etc/asound.conf

include $(call all-subdir-makefiles)
