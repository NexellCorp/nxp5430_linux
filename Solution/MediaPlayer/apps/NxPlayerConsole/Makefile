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
TARGET	:= NxPlayerConsole

COBJS	:= 
CPPOBJS	:= main.o NX_CSemaphore.o NX_CQueue.o NX_CCmdQueue.o

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET) 

clean:
	rm -f $(TARGET) *.o .depend

$(TARGET): depend $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) -o $(TARGET) $(LIBRARY) -g
	@echo "done"

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend
