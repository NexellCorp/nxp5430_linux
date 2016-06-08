ifeq ($(TARGET_CPU_VARIANT2),s5p6818)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(call first-makefiles-under,$(LOCAL_PATH))
endif
