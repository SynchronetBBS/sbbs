# headers.mk

# Make 'include file' for building Synchronet DLLs 
# Used with GNU and Borland compilers

# $Id$

HEADERS = \
	ars_defs.h \
	client.h \
	cmdshell.h \
	nodedefs.h \
	post.h \
	ringbuf.h \
	riodefs.h \
	sbbs.h \
	sbbsdefs.h \
	scfgdefs.h \
	scfglib.h \
	smbdefs.h \
	smblib.h \
	text.h \
	userdat.h \
	$(XPDEV)gen_defs.h \
	$(XPDEV)genwrap.h \
	$(XPDEV)wrapdll.h \
	$(XPDEV)dirwrap.h \
	$(XPDEV)filewrap.h \
	$(XPDEV)sockwrap.h \
	$(XPDEV)threadwrap.h
