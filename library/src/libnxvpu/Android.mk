ifeq ($(TARGET_ARCH),arm)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
# LOCAL_PRELINK_MODULE := false

SLSIAP_INCLUDE	:= $(TOP)/hardware/samsung_slsi/slsiap/include
LINUX_INCLUDE	:= $(TOP)/linux/platform/$(TARGET_CPU_VARIANT2)/library/include
LINUX_LIBS		:= $(TOP)/linux/platform/$(TARGET_CPU_VARIANT2)/library/lib

LOCAL_SHARED_LIBRARIES :=	\
	liblog \
	libcutils \
	libion \
	libion-nexell

LOCAL_STATIC_LIBRARIES := \
	libnxmalloc		\
	libhevcdec_and

LOCAL_C_INCLUDES := system/core/include/ion \
					$(SLSIAP_INCLUDE) \
					$(LINUX_INCLUDE) \
					$(LOCAL_PATH)/../libhevc/common \
					$(LOCAL_PATH)/../libhevc/decoder

LOCAL_CFLAGS += \
	-DHEVC_DEC

LOCAL_SRC_FILES := \
	parser_vld.c \
	nx_video_api.c

# We need this because the current asm generates the following link error:
# requires unsupported dynamic reloc R_ARM_REL32; recompile with -fPIC
# Bug: 16853291
LOCAL_LDFLAGS := -Wl,-Bsymbolic

LOCAL_LDFLAGS += \
	-L$(LINUX_LIBS)		\
	-lnxvidrc_android

LOCAL_MODULE := libnx_vpu

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
