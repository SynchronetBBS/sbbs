CPP	:= g++
CC	?= gcc

OS	:=	$(shell uname)
OS	:=	$(shell echo $(OS) | tr "[ A-Z]" "[\-a-z]")
OS	:=	$(shell echo $(OS) | tr -d "/")
CCFLAGS += -g -O2 -I/usr/bbs/sbbs/src/src/odoors -L/usr/bbs/sbbs/doors/libs.$(OS) -I./xpdev

ifeq ($(OS),netbsd)
  CCFLAGS	+=	-D__unix__
endif

OBJDIR	:= obj.$(OS)/

all : freevote.$(OS)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)filewrap.o: $(OBJDIR) xpdev/filewrap.c xpdev/filewrap.h xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c xpdev/filewrap.c -o $(OBJDIR)filewrap.o
	
$(OBJDIR)dirwrap.o: $(OBJDIR) xpdev/dirwrap.c xpdev/dirwrap.h xpdev/genwrap.h xpdev/gen_defs.h xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c xpdev/dirwrap.c -o $(OBJDIR)dirwrap.o
	
$(OBJDIR)genwrap.o: $(OBJDIR) xpdev/genwrap.c xpdev/genwrap.h xpdev/gen_defs.h xpdev/wrapdll.h
	$(CC) $(CCFLAGS) -c xpdev/genwrap.c -o $(OBJDIR)genwrap.o

freevote.$(OS) : freevote.cpp $(OBJDIR)dirwrap.o $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o
	$(CPP) $(CCFLAGS) freevote.cpp $(OBJDIR)dirwrap.o $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o -lODoors -o freevote.$(OS)

clean:
	rm -rf freevote.$(OS) $(OBJDIR)

