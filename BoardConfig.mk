TARGET_HAS_THIRD_PARTY_APPS := true

BOARD_WPA_SUPPLICANT_DRIVER:=AWEXT

BOARD_USES_TSLIB := true

BOARD_USES_I915C := true
#BOARD_USES_DRM := true
#BOARD_USES_HWOPENGL := true
#TARGET_HARDWARE_3D := true
BOARD_USES_WACOMINPUT := true

#BOARD_USES_GENERIC_AUDIO := false
#BOARD_USES_ALSA_AUDIO := true
#BUILD_WITH_ALSA_UTILS := true

#USE_CAMERA_STUB := false
#BOARD_CAMERA_LIBRARIES := libcamera
#BOARD_USES_OLD_CAMERA_HACK := true

#BUILD_WITH_MPLAYER := true

include $(GENERIC_X86_CONFIG_MK)

#BOARD_KERNEL_CMDLINE += SDCARD=sdc
