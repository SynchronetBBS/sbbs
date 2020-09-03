#
# This file is only to be used until the CMake system
# is used.  It outputs the hacked up output directories
# for the old build system.
#

# $Id: GNUmakefile,v 1.1 2014/02/12 04:00:03 deuce Exp $

all: output-dirs

PWD	:=	$(shell pwd)
SRC_ROOT	?=	${PWD}/..
include $(SRC_ROOT)/build/Common.gmake

.PHONY: output-dirs
output-dirs:
	@echo ${LIBODIR}
	@echo ${OBJODIR}
	@echo ${MTOBJODIR}
	@echo ${EXEODIR}
	@echo $(CCPRE).$(machine).$(BUILDPATH)
