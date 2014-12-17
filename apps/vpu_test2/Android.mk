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
	main.cpp                \
	VpuEncTest.cpp		
#	VpuJpgTest.cpp		\
#	img_proc_main.cpp	\

LOCAL_C_INCLUDES += \
	frameworks/native/include		\
	system/core/include				\
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
	libui			\
	libion			\
	libion-nexell	\
	libnx_vpu

LOCAL_LDFLAGS += \
	-Llinux/platform/$(TARGET_CPU_VARIANT2)/library/lib	\
	-L$(LOCAL_PATH)/ffmpeg/libs	\
	-lavutil-2.1.4 		\
	-lavcodec-2.1.4 	\
	-lavformat-2.1.4	\
	-lavdevice-2.1.4	\
	-lavfilter-2.1.4	\
	-lswresample-2.1.4	\
	-ltheoraparser_and

LOCAL_MODULE:= codec_tests

include $(BUILD_EXECUTABLE)
