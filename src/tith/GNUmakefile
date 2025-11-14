all: tith nodelist
-include $(OBJDIR)*.d

CFLAGS	+=	-std=c11 -MMD -MP -D_C11_SOURCE
ifdef DEBUG
 CFLAGS	+=	-g -O0 -Wall -pedantic -Wconversion -Wextra -Wno-format-truncation
else
 CFLAGS	+=	-Oz -flto
 LDFLAGS+=	-Oz -flto -fwhole-program -s
endif

TITH_OBJS := \
	base64.o \
	hydro/hydrogen.o \
	tith.o \
	tith-common.o \
	tith-config.o \
	tith-client.o \
	tith-file.o \
	tith-server.o \
	tith-stdio.o

NODELIST_OBJS := \
	nodelist.o \
	tith-file.o \
	tith-nodelist.o

$(OBJDIR)%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

tith: $(TITH_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

nodelist: $(NODELIST_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o *.d hydro/*.o hydro/*.d tith nodelist
