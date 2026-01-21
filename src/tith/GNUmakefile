all: tith nodelist tith-bundle
-include $(OBJDIR)*.d

CFLAGS	+=	-std=c11 -MMD -MP -pthread
LDFLAGS	+=	-pthread
ifdef DEBUG
 CFLAGS	+=	-g -O0 -Wall -pedantic -Wconversion -Wextra -Wno-format-truncation
else
 CFLAGS	+=	-Os -flto
 LDFLAGS+=	-Os -flto -fwhole-program -s
endif

XSI_CFLAGS := $(CFLAGS) -D_XOPEN_SOURCE=500
CFLAGS	+=  -D_C11_SOURCE

TITH_BUNDLE_OBJS := \
	base64.o \
	hydro/hydrogen.o \
	tith-bundle.o \
	tith-common.o \
	tith-config.o \
	tith-file.o \
	tith-nodelist.o \
	tith-stdio.o \
	tith-strings.o \
	tith-xsi.o \

TITH_OBJS := \
	base64.o \
	hydro/hydrogen.o \
	tith.o \
	tith-common.o \
	tith-config.o \
	tith-client.o \
	tith-file.o \
	tith-nodelist.o \
	tith-server.o \
	tith-stdio.o \
	tith-strings.o

NODELIST_OBJS := \
	base64.o \
	nodelist.o \
	tith-file.o \
	tith-nodelist.o \
	tith-strings.o

$(OBJDIR)%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

tith-xsi.o: tith-xsi.c | $(OBJDIR)
	$(CC) $(XSI_CFLAGS) -c $< -o $@

tith: $(TITH_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

tith-bundle: $(TITH_BUNDLE_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

nodelist: $(NODELIST_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o *.d hydro/*.o hydro/*.d tith nodelist tith-bundle
