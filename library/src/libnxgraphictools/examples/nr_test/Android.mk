ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)

##############################################################################
##
##     Graphic Library Test Application
##

include $(CLEAR_VARS)
NX_HW_TOP        := $(TOP)/hardware/samsung_slsi/slsiap/
NX_HW_INCLUDE    := $(NX_HW_TOP)/include
NX_LINUX_INCLUDE := $(TOP)/linux/platform/s5p6818/library/include

LOCAL_SHARED_LIBRARIES :=	\
	liblog \
	libcutils \
	libnxgraphictools \
	libion-nexell \
	libion \
	libnx_vpu

LOCAL_STATIC_LIBRARIES :=	\
	libnxmalloc

LOCAL_C_INCLUDES :=	$(TOP)/system/core/include/ion \
					$(NX_HW_INCLUDE) \
					$(LOCAL_PATH)/src \
					$(NX_LINUX_INCLUDE) \
					$(LOCAL_PATH)/include/khronos

LOCAL_CFLAGS := 

LOCAL_SRC_FILES := \
	main.cpp

LOCAL_MODULE := gt_nr_test
LOCAL_MODULE_PATH := $(LOCAL_PATH)

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

endif	# arm
