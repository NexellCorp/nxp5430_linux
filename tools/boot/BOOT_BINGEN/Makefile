CC		= gcc
TARGET 	= BOOT_BINGEN
OBJS 	= BOOT_BINGEN.o

SRCS 	= $(OBJS:.o=.c)

all : $(TARGET)
	$(CC) $(SRCS) -o $(TARGET)
	
.c.o:
	$(CC) $<
dep:
	gccmakedep $(SRCS)
clean:
	rm -f *.o
