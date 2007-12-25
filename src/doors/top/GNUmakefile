CPP	:= g++
CC	?= gcc

OS	:=	$(shell uname)
CCFLAGS += -g -O2 -I../../odoors -L../../odoors/libs-$(OS) -I../../xpdev

ifeq ($(OS),NetBSD)
  CCFLAGS	+=	-D__unix__
endif

ifeq ($(shell if [ -f /usr/include/inttypes.h ] ; then echo YES ; fi),YES)
 CCFLAGS	+=	-DHAS_INTTYPES_H
endif

OBJDIR	:= obj.$(OS)/

vpath %.c ../../xpdev


all : topact.$(OS) top.$(OS) topmaint.$(OS)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)filewrap.o: $(OBJDIR) ../../xpdev/filewrap.c ../../xpdev/filewrap.h ../../xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c ../../xpdev/filewrap.c -o $(OBJDIR)filewrap.o
	
$(OBJDIR)dirwrap.o: $(OBJDIR) ../../xpdev/dirwrap.c ../../xpdev/dirwrap.h ../../xpdev/genwrap.h ../../xpdev/gen_defs.h ../../xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c ../../xpdev/dirwrap.c -o $(OBJDIR)dirwrap.o
	
$(OBJDIR)genwrap.o: $(OBJDIR) ../../xpdev/genwrap.c ../../xpdev/genwrap.h ../../xpdev/gen_defs.h ../../xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c ../../xpdev/genwrap.c -o $(OBJDIR)genwrap.o

$(OBJDIR)datewrap.o: $(OBJDIR) ../../xpdev/datewrap.c ../../xpdev/datewrap.h ../../xpdev/gen_defs.h ../../xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c ../../xpdev/datewrap.c -o $(OBJDIR)datewrap.o

$(OBJDIR)strwrap.o: $(OBJDIR) ../../xpdev/strwrap.c ../../xpdev/strwrap.h ../../xpdev/gen_defs.h ../../xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c ../../xpdev/strwrap.c -o $(OBJDIR)strwrap.o

$(OBJDIR)conwrap.o: $(OBJDIR) ../../xpdev/conwrap.c ../../xpdev/conwrap.h ../../xpdev/gen_defs.h ../../xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c ../../xpdev/conwrap.c -o $(OBJDIR)conwrap.o

topact.$(OS) : topact.c $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o
	$(CC) $(CCFLAGS) topact.c $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o -o topact.$(OS)

topmaint.$(OS) : topmaint.c $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o $(OBJDIR)datewrap.o $(OBJDIR)conwrap.o
	$(CC) $(CCFLAGS) topmaint.c $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o $(OBJDIR)datewrap.o $(OBJDIR)conwrap.o -o topmaint.$(OS)

# Implicit C Compile Rule
$(OBJDIR)%.o : %.c
	$(CC) $(CCFLAGS) -o $@ -c $<

clean:
	rm -rf  topact.$(OS) top.$(OS) topmaint.$(OS) $(OBJDIR)

OBJS := $(OBJDIR)actions.o \
        $(OBJDIR)bbs.o \
        $(OBJDIR)bbssbbs.o \
        $(OBJDIR)bio.o \
        $(OBJDIR)censor.o \
        $(OBJDIR)cfg.o \
        $(OBJDIR)change.o \
        $(OBJDIR)channels.o \
        $(OBJDIR)cmi.o \
        $(OBJDIR)help.o \
        $(OBJDIR)kernel.o \
        $(OBJDIR)lang.o \
        $(OBJDIR)main.o \
        $(OBJDIR)maint.o \
        $(OBJDIR)messages.o \
        $(OBJDIR)moderate.o \
        $(OBJDIR)nodecfg.o \
        $(OBJDIR)nodes.o \
        $(OBJDIR)privchat.o \
        $(OBJDIR)procinp.o \
        $(OBJDIR)profile.o \
        $(OBJDIR)screens.o \
        $(OBJDIR)strings.o \
        $(OBJDIR)sysop.o \
        $(OBJDIR)words.o \
        $(OBJDIR)bbsmax.o \
        $(OBJDIR)bbsra.o \
        $(OBJDIR)global.o \
        $(OBJDIR)init.o \
        $(OBJDIR)output.o \
        $(OBJDIR)procmsgs.o \
        $(OBJDIR)system.o \
        $(OBJDIR)user.o \
	$(OBJDIR)filewrap.o \
	$(OBJDIR)dirwrap.o \
	$(OBJDIR)genwrap.o \
	$(OBJDIR)datewrap.o \
	$(OBJDIR)strwrap.o \
	$(OBJDIR)matchgam.o \
	$(OBJDIR)slots.o \
#        $(OBJDIR)bbswc.o \	# Experimental
#        $(OBJDIR)charchat.o \  # Experimental
#        $(OBJDIR)spawn.o \	# *MAY* not be required



top.$(OS) : $(OBJS)
	$(CC) $(CCFLAGS) -o $@ $^ -lODoors
