#include "xfer_recv.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "dirwrap.h"
#include "filewrap.h"
#include "sexyz.h"
#include "xmodem.h"

static unsigned failures;

#define CHECK(expression) do {                                      \
	if (!(expression)) {                                          \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, \
		    __LINE__, #expression);                               \
		failures++;                                               \
	}                                                             \
} while (0)

struct mock_io {
	unsigned recv_calls;
	unsigned cancel_after;
	unsigned elapsed;
	unsigned byte_after;
	int      byte;
	bool     cancelled;
};

static int
mock_send(void *cbdata, uchar ch, unsigned timeout)
{
	return 0;
}

static int
mock_recv_seconds(void *cbdata, unsigned timeout)
{
	struct mock_io *io = cbdata;

	io->recv_calls++;
	if (io->cancel_after != 0 && io->recv_calls >= io->cancel_after)
		io->cancelled = true;
	return -1;
}

static int
mock_recv_milliseconds(void *cbdata, unsigned timeout)
{
	struct mock_io *io = cbdata;

	io->recv_calls++;
	io->elapsed += timeout;
	if (io->cancel_after != 0 && io->recv_calls >= io->cancel_after)
		io->cancelled = true;
	if (io->byte_after != 0 && io->recv_calls >= io->byte_after)
		return io->byte;
	return -1;
}

static BOOL
mock_connected(void *cbdata)
{
	return TRUE;
}

static BOOL
mock_cancelled(void *cbdata)
{
	struct mock_io *io = cbdata;

	return io->cancelled;
}

static void
mock_flush(void *cbdata)
{
}

static void
init_xmodem(xmodem_t *xm, struct mock_io *io, long *mode)
{
	xmodem_init(xm, io, mode, NULL, NULL, mock_send,
	    mock_recv_seconds, mock_connected, mock_cancelled, mock_flush);
}

static void
test_mode_wait_stops_on_cancel(void)
{
	struct mock_io io = {.cancel_after = 1};
	xmodem_t       xm;
	long           mode = XMODEM | SEND;

	init_xmodem(&xm, &io, &mode);
	CHECK(!xmodem_get_mode(&xm));
	CHECK(io.recv_calls == 1);
	CHECK(xm.cancelled);
}

static void
test_ack_wait_stops_on_cancel(void)
{
	struct mock_io io = {.cancel_after = 1};
	xmodem_t       xm;
	long           mode = XMODEM | SEND;

	init_xmodem(&xm, &io, &mode);
	CHECK(xmodem_get_ack(&xm, 5, 1) == NOINP);
	CHECK(io.recv_calls == 1);
	CHECK(xm.cancelled);
}

static void
test_block_wait_honours_existing_cancel(void)
{
	struct mock_io io = {.cancelled = true};
	xmodem_t       xm;
	long           mode = XMODEM | RECV;
	uchar          block[XMODEM_MAX_BLOCK_SIZE];

	init_xmodem(&xm, &io, &mode);
	CHECK(xmodem_get_block(&xm, block, 1) == FAILURE);
	CHECK(io.recv_calls == 0);
	CHECK(xm.cancelled);
}

static void
test_interruptible_receive(void)
{
	struct mock_io io = {.cancel_after = 3};

	CHECK(xfer_recv_byte_interruptible(&io, 10, 100,
	    mock_recv_milliseconds, mock_cancelled) == -1);
	CHECK(io.recv_calls == 3);
	CHECK(io.elapsed == 300);
}

static void
test_interruptible_receive_preserves_timeout(void)
{
	struct mock_io io = {0};

	CHECK(xfer_recv_byte_interruptible(&io, 1, 100,
	    mock_recv_milliseconds, mock_cancelled) == -1);
	CHECK(io.recv_calls == 10);
	CHECK(io.elapsed == 1000);
}

static void
test_interruptible_receive_returns_data(void)
{
	struct mock_io io = {.byte_after = 2, .byte = 'C'};

	CHECK(xfer_recv_byte_interruptible(&io, 10, 100,
	    mock_recv_milliseconds, mock_cancelled) == 'C');
	CHECK(io.recv_calls == 2);
	CHECK(io.elapsed == 200);
}

static void
test_nonblocking_receive(void)
{
	struct mock_io io = {0};

	CHECK(xfer_recv_byte_interruptible(&io, 0, 100,
	    mock_recv_milliseconds, mock_cancelled) == -1);
	CHECK(io.recv_calls == 1);
	CHECK(io.elapsed == 0);
}

int
main(void)
{
	test_mode_wait_stops_on_cancel();
	test_ack_wait_stops_on_cancel();
	test_block_wait_honours_existing_cancel();
	test_interruptible_receive();
	test_interruptible_receive_preserves_timeout();
	test_interruptible_receive_returns_data();
	test_nonblocking_receive();

	if (failures != 0) {
		fprintf(stderr, "xmodem_cancel_test: %u failure%s\n", failures,
		    failures == 1 ? "" : "s");
		return EXIT_FAILURE;
	}
	puts("xmodem_cancel_test: all checks passed");
	return EXIT_SUCCESS;
}
