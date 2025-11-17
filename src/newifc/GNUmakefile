CFLAGS += -I../conio -I../xpdev -I../cutest -pthread
CFLAGS += -DHAS_STDINT_H
CFLAGS += -DHAS_INTTYPES_H
CFLAGS += -g -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS += -L../conio/clang.freebsd.amd64.lib.portable -L../xpdev/clang.freebsd.amd64.lib.portable
LIBS += -pthread -lciolib_mt -lxpdev_mt -lcursesw -lm

VPATH = ../cutest:../xpdev

ifdef TESTS
CFLAGS += -DBUILD_TESTS
all: runtest

test: alltests.o CuTest.o libnewifc.a
	cc ${LDFLAGS} -L. ${CFLAGS} $^ -o $@ ${LIBS} -lnewifc

runtest: test
	./test
else
all: libnewifc.a
endif

newifc.c: genapi
	./genapi

newifc.h: newifc.c

newifc.o: newifc.c root_window.c label.c sizer.c sizer_cell.c newifc_nongen.c newifc_nongen_after.c

libnewifc.a: libnewifc.a(newifc.o)
	ranlib $@

clean:
	rm -f *.o genapi test newifc_internal.h new_ifc.h new_ifc.c libnewifc.a newifc.c newifc.h
