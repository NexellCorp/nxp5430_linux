ifndef	JOBS
JOBJS	:= 8
endif

DIR		:= 
######################################################################
# Build options

DIR += v4l2_test
DIR += vpu_test2


######################################################################
# Build
all:
	@for dir in $(DIR); do			\
	make -C $$dir || exit $?;		\
	done

clean:
	@for dir in $(DIR); do			\
	make -C $$dir clean || exit $?;	\
	done

