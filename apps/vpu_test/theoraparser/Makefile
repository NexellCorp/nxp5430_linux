########################################################################
# Get Linux Build Enviornment:
include ../../../build.env

######################################################################
# Build options
INCLUDE += -I./ -I./include

######################################################################
# Target
COBJS  	:= src/bitpack.o
COBJS  	+= src/decode.o
COBJS  	+= src/info.o
COBJS  	+= src/internal.o
COBJS  	+= src/state.o
COBJS  	+= src/theora_parser.o

CPPOBJS	:=  

CFLAGS	+= -g

LIBNAME	:= libtheoraparser
TARGET  := libtheoraparser.so

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET):	depend $(OBJS)
	$(AR) $(ARFLAGS) $(LIBNAME).a $(OBJS)

clean:
	rm -f src/*.o *.so *.a .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
