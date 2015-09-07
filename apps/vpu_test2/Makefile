########################################################################
# Get Linux Build Enviornment:
include ../../build.env

######################################################################
# Build options
INCLUDE += -I./
INCLUDE += -I../../library/include -I../../library/include/theoraparser
INCLUDE += -I./ffmpeg/include/

LIBRARY	+= -L../../library/lib -L../../library/lib/ratecontrol
LIBRARY += -lnxvpu -lnxdsp -lnxvip -lnxv4l2 -lnxvmem -lnxvidrc -lnxdeinterlace -lnxgraphictools
#LIBRARY += -lnxvpu -lnxdsp -lnxvip -lnxv4l2 -lnxvmem -lnxvidrc -lhevcdec -lnxdeinterlace -lnxgraphictools
LIBRARY += -ltheoraparser
LIBRARY	+= -lstdc++ -lm

LIBRARY	+= -L./ffmpeg/libs -lavcodec -lavformat -lavutil
LIBRARY += -I../../library/src/libnxgraphictools/libs
LIBRARY += -lEGL -lGLESv1_CM -lGLESv2 -lVR

######################################################################
# Target
CPPOBJS	:= \
	MediaExtractor.o	\
	CodecInfo.o			\
	NX_Queue.o			\
	NX_Semaphore.o		\
	Util.o				\
	VpuDecTest.o		\
	VpuEncTest.o		\
	VpuJpgTest.o		\
	main.o

CFLAGS	+= -g -pthread 

TARGET:= codec_tests

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all:	\
	$(THEORAPARSER) \
	$(TARGET)

$(TARGET):	depend $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJS) -o $@ $(LIBRARY)

clean:
	rm -f $(OBJS) $(TARGET) .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
