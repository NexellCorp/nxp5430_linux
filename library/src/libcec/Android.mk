LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := liblog \
	libcutils

LOCAL_SRC_FILES := libcec.c

LOCAL_MODULE := libcec

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
