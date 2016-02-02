LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
	MediaExtractor.cpp	\
	CodecInfo.cpp		\
	NX_Queue.cpp		\
	NX_Semaphore.cpp	\
	NX_AndroidRenderer.cpp \
	Util.cpp			\
	VpuDecTest.cpp		\
	VpuEncTest.cpp		\
	main.cpp            \

#	VpuJpgTest.cpp		\
#	img_proc_main.cpp	\

LOCAL_C_INCLUDES += \
	frameworks/native/include		\
	system/core/include				\
	system/core/include/ion		\
	hardware/libhardware/include	\
	hardware/samsung_slsi/slsiap/include	\
	linux/platform/$(TARGET_CPU_VARIANT2)/library/include	\
	$(LOCAL_PATH)/ffmpeg/include	\
	$(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES := \
	libcutils		\
	libbinder		\
	libutils		\
	libgui			\
	libui			\
	libion			\
	libion-nexell	\
	libnx_vpu	\
	libv4l2-nexell

#LOCAL_SHARED_LIBRARIES += \
#	libnx_deinterlace	\
#	libnxgraphictools

LOCAL_STATIC_LIBRARIES := \
	libnxmalloc

#LOCAL_STATIC_LIBRARIES += \
#	libnx_dsp \

LOCAL_LDFLAGS += \
	-Llinux/platform/$(TARGET_CPU_VARIANT2)/library/lib	\
	-L$(LOCAL_PATH)/ffmpeg/libs	\
	-Lhardware/samsung_slsi/slsiap/omx/codec/ffmpeg/libs \
	-lavutil-2.1.4 		\
	-lavcodec-2.1.4 	\
	-lavformat-2.1.4	\
	-lavdevice-2.1.4	\
	-lavfilter-2.1.4	\
	-lswresample-2.1.4	\
	-ltheoraparser_and

LOCAL_MODULE:= codec_tests
LOCAL_MODULE_PATH := $(LOCAL_PATH)

include $(BUILD_EXECUTABLE)
