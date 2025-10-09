CPP	?= g++
CC	?= gcc
XPDEV	?=	../../xpdev

OS	:=	$(shell uname)
OS	:=	$(shell echo $(OS) | tr "[ A-Z]" "[\-a-z]")
OS	:=	$(shell echo $(OS) | tr -d "/")
CFLAGS += -DNO_OS_VERSION
CXXFLAGS += -g -O2 -I../../odoors -I$(XPDEV)
LDFLAGS += -L../../odoors/libs-`uname`

ifeq ($(OS),netbsd)
  CXXFLAGS	+=	-D__unix__
endif

ifeq ($(shell if [ -f /usr/include/inttypes.h ] ; then echo YES ; fi),YES)
 CXXFLAGS	+=	-DHAS_INTTYPES_H
endif

OBJDIR	:= obj.$(OS)/

all : freevote.$(OS)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)filewrap.o: $(XPDEV)/filewrap.c $(XPDEV)/filewrap.h $(XPDEV)/wrapdll.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(XPDEV)/filewrap.c -o $(OBJDIR)filewrap.o
	
$(OBJDIR)dirwrap.o: $(XPDEV)/dirwrap.c $(XPDEV)/dirwrap.h $(XPDEV)/genwrap.h $(XPDEV)/gen_defs.h $(XPDEV)/wrapdll.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(XPDEV)/dirwrap.c -o $(OBJDIR)dirwrap.o
	
$(OBJDIR)genwrap.o: $(XPDEV)/genwrap.c $(XPDEV)/genwrap.h $(XPDEV)/gen_defs.h $(XPDEV)/wrapdll.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(XPDEV)/genwrap.c -o $(OBJDIR)genwrap.o

$(OBJDIR)freevote.o: freevote.cpp | $(OBJDIR)
	$(CC) $(CXXFLAGS) -c freevote.cpp -o $@

freevote.$(OS) : $(OBJDIR)freevote.o $(OBJDIR)dirwrap.o $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o
	$(CXX) $(LDFLAGS) $(OBJDIR)freevote.o $(OBJDIR)dirwrap.o $(OBJDIR)genwrap.o $(OBJDIR)filewrap.o -lODoors -o freevote.$(OS)

clean:
	rm -rf freevote.$(OS) $(OBJDIR)
