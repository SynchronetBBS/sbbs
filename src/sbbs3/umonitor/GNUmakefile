CFLAGS	+=	-I../ -I../../xpdev -I../../uifc -DUSE_CURSES -DUSE_SIGNALS -g
LFLAGS	+=	-lcurses
CC	?=	gcc
LD	?=	gcc

vpath %.c ../../xpdev ../../uifc

OBJS	:=	filewrap.o uifcc.o uifcx.o umonitor.o sockwrap.o dirwrap.o genwrap.o

umonitor: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o umonitor

# Implicit C Compile Rule for utils
$(EXEODIR)/%.o : %.c $(BUILD_DEPENDS)
	$(CC) $(CFLAGS) -o $@ -c $<
