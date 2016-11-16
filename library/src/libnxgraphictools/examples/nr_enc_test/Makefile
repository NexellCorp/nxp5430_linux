########################################################################
# Get Linux Build Enviornment:
include ../../../../../build.env

######################################################################
# Build options

# Add Include Path
INCLUDE		+= -I./ -I../../include -I./include/khronos -I../../include

# Add Library Path
LIBRARY     += -L./
LIBRARY     += -L../../../../lib
LIBRARY     += -lnxvmem
LIBRARY 	+= -lnxgraphictools
# Add Library
LIBRARY     += -L./libs/
LIBRARY     += -lEGL -lGLESv1_CM -lGLESv2 -lVR

######################################################################
# Target
COBJS		:=
CPPOBJS		:= main.o
TARGET		:= gt_nr_test 

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET): depend $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) -o $@ $(LIBRARY)

install:


clean:
	rm -f *.o *.a $(TARGET) .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CPPFLAGS) $(INCS) $(SRCS) > .depend

