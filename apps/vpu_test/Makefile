########################################################################
# Get Linux Build Enviornment:
include ../../build.env

######################################################################
# Build options
INCLUDE += -I./
INCLUDE += -I../../library/include
LIBRARY	+= -L../../library/lib -L../../library/lib/ratecontrol 
LIBRARY += -lnxvpu -lnxdsp -lnxvip -lnxv4l2 -lnxvmem -lnxvidrc
LIBRARY += -L./theoraparser -ltheoraparser
LIBRARY	+= -lavcodec -lavformat -lstdc++ -lm

######################################################################
# Target
DEC_OBJS	:= 
DEC_OBJS	+= codec_info.o
DEC_OBJS	+= dec_main.o

ENC_OBJS	:=
ENC_OBJS	+= queue.o
ENC_OBJS	+= enc_main.o

ENC_PERF_OBJS	:=
ENC_PERF_OBJS	+= enc_perf_main.o

JPG_OBJS	:=
JPG_OBJS	+= queue.o
JPG_OBJS	+= jpg_main.o

TRNAS_OBJS	:= codec_info.o
TRNAS_OBJS	+= trans_main.o

COBJS	:= dec_main.o enc_main.o queue.o codec_info.o jpg_main.o trans_main.o enc_perf_main.o

CPPOBJS	:=  

CFLAGS	+= -g

ENCAPP := enc_test
ENCPERFAPP := enc_perf_test
DECAPP := dec_test
JPGAPP := jpg_test
TRANSAPP := trans_test
THEORAPARSER := libtheoraparser

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all:	\
	$(THEORAPARSER) \
	$(DECAPP)	\
	$(JPGAPP)	\
	$(TRANSAPP) \
	$(JPGAPP)	\
	$(ENCAPP)	\
	$(ENCPERFAPP)	\

$(THEORAPARSER):
	make -C theoraparser

$(ENCAPP):	depend $(OBJS)
	$(CC) $(CFLAGS) $(ENC_OBJS) -o $@ $(LIBRARY)

$(ENCPERFAPP):	depend $(OBJS)
	$(CC) $(CFLAGS) $(ENC_PERF_OBJS) -o $@ $(LIBRARY)

$(JPGAPP):	depend $(OBJS)
	$(CC) $(CFLAGS) $(JPG_OBJS) -o $@ $(LIBRARY)

$(DECAPP):	depend $(OBJS)
	make -C theoraparser
	$(CC) $(CFLAGS) $(DEC_OBJS) -o $@ $(LIBRARY)

$(TRANSAPP):	depend $(OBJS)
	$(CC) $(CFLAGS) $(TRNAS_OBJS) -o $@ $(LIBRARY)

clean:
	make -C theoraparser clean
	rm -f $(ENCAPP) $(ENCPERFAPP) $(JPGAPP) $(DECAPP) $(TRANSAPP) *.o .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
