all: tith-server tith-client nodelist
-include $(OBJDIR)*.d

CFLAGS	+=	-std=c11 -MMD -MP -D_C11_SOURCE
ifdef DEBUG
 CFLAGS	+=	-g -O0 -Wall -pedantic -Wconversion -Wextra -Wno-format-truncation
else
 CFLAGS	+=	-Oz -flto
 LDFLAGS+=	-Oz -flto -fwhole-program -s
endif

SERVER_OBJS := \
	base64.o \
	hydro/hydrogen.o \
	tith-common.o \
	tith-config.o \
	tith-server.o

CLIENT_OBJS := \
	base64.o \
	hydro/hydrogen.o \
	tith-common.o \
	tith-config.o \
	tith-client.o

NODELIST_OBJS := \
	nodelist.o \
	tith-file.o \
	tith-nodelist.o

$(OBJDIR)%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

tith-server: $(SERVER_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

tith-client: $(CLIENT_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

nodelist: $(NODELIST_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o *.d hydro/*.o hydro/*.d tith-server tith-client nodelist
