CFLAGS	+=	-I../ -I../../xpdev -I../../uifc -DUSE_CURSES -DUSE_SIGNALS
LFLAGS	+=	-lcurses
CC	?=	gcc
LD	?=	gcc

vpath %.c ../../xpdev ../../uifc

umonitor: filewrap.o uifcc.o uifcx.o  umonitor.o
	$(CC) $(LFLAGS) filewrap.o uifcc.o uifcx.o  umonitor.o -o umonitor

# Implicit C Compile Rule for utils
$(EXEODIR)/%.o : %.c $(BUILD_DEPENDS)
	$(CC) $(CFLAGS) -o $@ -c $<
