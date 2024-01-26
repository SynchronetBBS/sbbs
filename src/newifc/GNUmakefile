CFLAGS += -I../xpdev -I../cutest -pthread
CFLAGS += -DHAS_STDINT_H
CFLAGS += -DHAS_INTTYPES_H
LIBS += -lpthread

VPATH = ../cutest:../xpdev

ifdef TESTS
CFLAGS += -DBUILD_TESTS
all: runtest

test: alltests.o CuTest.o root_window.o threadwrap.o
	cc ${LDFLAGS} ${CFLAGS} $^ -o $@ ${LIBS}

runtest: test
	./test
else
all: libnewifc.a
endif

newifc.c: genapi
	./genapi

newifc.h: newifc.c

root_window.o: newifc.h

newifc.o: newifc.c internal_macros.h root_window.c

libnewifc.a: libnewifc.a(newifc.o)
	ranlib $@
	strip --strip-unneeded $@

clean:
	rm -f *.o genapi test newifc_internal.h new_ifc.h new_ifc.c libnewifc.a newifc.c newifc.h
