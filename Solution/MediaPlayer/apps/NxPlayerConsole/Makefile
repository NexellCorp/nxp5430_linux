########################################################################
# Get Linux Build Enviornment:
include ../../path.env
include $(BUILD_ENV)

######################################################################
# Build options
INCLUDE += -I./ -I../../include

LIBRARY	+= -L../../lib -L../../lib/ffmpeg/linux
LIBRARY	+= -L$(LIBSDIR)/lib -L$(LIBSDIR)/lib/ratecontrol
LIBRARY += -lnxfilter -lnxfilterhelper -lnxmpmanager
LIBRARY	+= -lnxvpu -lnxvidrc -lnxdsp -lnxv4l2 -lnxvmem -lnxscaler -ltheoraparser -lnxuevent
LIBRARY	+= -lavcodec -lavdevice -lavfilter -lavformat
LIBRARY	+= -lavutil -lswresample -lswscale
LIBRARY	+= -lstdc++ -lpthread -lasound

######################################################################
# Target
TARGET		:= NxPlayerConsole
TARGET2		:= NxPlayerConsole_Simple
TARGET3		:= NxPlayerDualDisplay

COBJS		:= 
CPPOBJS		:= NX_CSemaphore.o NX_CQueue.o NX_CCmdQueue.o main.o

COBJS2		:=
CPPOBJS2	:= NX_CSemaphore.o NX_CQueue.o NX_CCmdQueue.o main_simple.o

COBJS3		:=
CPPOBJS3	:= NX_CSemaphore.o NX_CQueue.o NX_CCmdQueue.o main_dualdisplay.o

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)
OBJS2	:= $(COBJS2) $(CPPOBJS2)
OBJS3	:= $(COBJS3) $(CPPOBJS3)

all: $(TARGET) $(TARGET2) $(TARGET3)

$(TARGET): depend $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) -o $(TARGET) $(LIBRARY) -g
	@echo "done"

$(TARGET2): depend $(OBJS2)
	$(CC) $(CPPFLAGS) $(OBJS2) -o $(TARGET2) $(LIBRARY) -g
	@echo "done"

$(TARGET3): depend $(OBJS3)
	$(CC) $(CPPFLAGS) $(OBJS3) -o $(TARGET3) $(LIBRARY) -g
	@echo "done"

clean:
	rm -f $(TARGET) $(TARGET2) $(TARGET3) *.o .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
SRCS += $(COBJS2:.o=.c) $(CPPOBJS2:.o=.cpp)
SRCS += $(COBJS3:.o=.c) $(CPPOBJS3:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend
