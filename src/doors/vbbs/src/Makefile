#
#  VS3 makefile
#
##################################

PLAT ?= dos

ifeq ($(PLAT), dos)
    include Makefile.dos
endif

ifeq ($(PLAT), win32)
    include Makefile.win32
endif

ifeq ($(PLAT), linux)
	include Makefile.linux
endif
