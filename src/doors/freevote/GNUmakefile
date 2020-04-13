CPP	?= g++
CC	?= gcc
XPDEV	?=	../../xpdev

OS	:=	$(shell uname)
OS	:=	$(shell echo $(OS) | tr "[ A-Z]" "[\-a-z]")
OS	:=	$(shell echo $(OS) | tr -d "/")
CCFLAGS += -g -O2 -I../../odoors -L../../odoors/libs-`uname` -I$(XPDEV)

ifeq ($(OS),netbsd)
  CCFLAGS	+=	-D__unix__
endif

ifeq ($(shell if [ -f /usr/include/inttypes.h ] ; then echo YES ; fi),YES)
 CCFLAGS	+=	-DHAS_INTTYPES_H
endif

OBJDIR	:= obj.$(OS)/

all : freevote.$(OS)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)filewrap.o: $(OBJDIR) $(XPDEV)/filewrap.c $(XPDEV)/filewrap.h $(XPDEV)/wrapdll.h
	$(CC) $(CCFLAGS) -c $(XPDEV)/filewrap.c -o $(OBJDIR)filewrap.o
	
$(OBJDIR)dirwrap.o: $(OBJDIR) $(XPDEV)/dirwrap.c $(XPDEV)/dirwrap.h $(XPDEV)/genwrap.h $(XPDEV)/gen_defs.h $(XPDEV)/wrapdll.h
	$(CC) $(CCFLAGS) -c $(XPDEV)/dirwrap.c -o $(OBJDIR)dirwrap.o
	
$(OBJDIR)genwrap.o: $(OBJDIR) $(XPDEV)/genwrap.c $(XPDEV)/genwrap.h $(XPDEV)/gen_defs.h $(XPDEV)/wrapdll.h
	$(CC) $(CCFLAGS) -c $(XPDEV)/genwrap.c -o $(OBJDIR)genwrap.o

freevote.$(OS) : freevote.cpp $(OBJDIR)dirwrap.o $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o
	$(CPP) $(CCFLAGS) freevote.cpp $(OBJDIR)dirwrap.o $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o -lODoors -o freevote.$(OS)

clean:
	rm -rf freevote.$(OS) $(OBJDIR)

