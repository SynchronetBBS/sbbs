SRC_ROOT        = ../..
include ${SRC_ROOT}/build/Common.gmake

# OpenDoors
LDFLAGS	+=	-L../../odoors
CFLAGS	+=	-I../../odoors

ifdef STATIC
 LDFLAGS	+=	-static
endif

# Set up LD_RUN_PATH for run-time locating of the .so files
PWD :=  $(shell pwd)
LD_RUN_PATH :=  .
export LD_RUN_PATH

$(DGNLNCE): $(OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) -o $@ $(OBJS) -lODoors

$(ODOORS_LIB):
	@echo Building $@
	$(MAKE) -C $(SRC_ROOT)$(DIRSEP)odoors
