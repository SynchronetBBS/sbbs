#include "xfer_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

#define CHECK(expression) do {                                      \
	if (!(expression)) {                                          \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, \
		    __LINE__, #expression);                               \
		failures++;                                               \
	}                                                             \
} while (0)

static void
check_compact(const uint8_t *input, size_t input_len,
    enum transfer_ready_sequence sequence,
    const uint8_t *expected, size_t expected_len, size_t expected_dropped)
{
	uint8_t buffer[256];
	size_t  dropped = SIZE_MAX;

	CHECK(input_len <= sizeof(buffer));
	if (input_len > sizeof(buffer))
		return;
	memcpy(buffer, input, input_len);

	size_t len = xfer_queue_compact(buffer, input_len, sequence, &dropped);
	CHECK(len == expected_len);
	CHECK(dropped == expected_dropped);
	if (len == expected_len)
		CHECK(len == 0 || memcmp(buffer, expected, len) == 0);
}

static void
test_ticket_271(void)
{
	static const uint8_t queued[] = "CCCCCCCCCCC";
	static const uint8_t expected[] = "C";
	uint8_t              buffer[sizeof(queued) - 1];
	size_t               input_pos = 0;

	memcpy(buffer, queued, sizeof(buffer));
	size_t len = xfer_queue_compact(buffer, sizeof(buffer),
	    TRANSFER_READY_XYMODEM, NULL);

	/*
	 * xmodem_get_mode() consumes the retained C.  Its first block ACK
	 * wait must then see an empty queue rather than the ten stale Cs
	 * that caused the transfer to exhaust its retry count.
	 */
	CHECK(len == sizeof(expected) - 1);
	CHECK(buffer[input_pos++] == 'C');
	CHECK(input_pos == len);
}

static void
test_xymodem_requests(void)
{
	static const uint8_t c_with_suffix[] = "menuCCCCafter";
	static const uint8_t c_expected[] = "Cafter";
	static const uint8_t g_queued[] = {'x', 'G', 'G', 'G'};
	static const uint8_t g_expected[] = {'G'};
	static const uint8_t nak_queued[] = {'x', 0x15, 0x15, 0x15};
	static const uint8_t nak_expected[] = {0x15};
	static const uint8_t single_c[] = "ordinary C text";

	check_compact(c_with_suffix, sizeof(c_with_suffix) - 1,
	    TRANSFER_READY_XYMODEM,
	    c_expected, sizeof(c_expected) - 1, 7);
	check_compact(g_queued, sizeof(g_queued),
	    TRANSFER_READY_XYMODEM,
	    g_expected, sizeof(g_expected), 3);
	check_compact(nak_queued, sizeof(nak_queued),
	    TRANSFER_READY_XYMODEM,
	    nak_expected, sizeof(nak_expected), 3);
	check_compact(single_c, sizeof(single_c) - 1,
	    TRANSFER_READY_XYMODEM,
	    single_c, sizeof(single_c) - 1, 0);
}

static void
test_zmodem_requests(void)
{
	static const uint8_t zrinit_queued[] = {
		'x', '*', '*', 0x18, 'B', '0', '1', 'a',
		'*', '*', 0x18, 'B', '0', '1', 'b'
	};
	static const uint8_t zrinit_expected[] = {
		'*', 0x18, 'B', '0', '1', 'b'
	};
	static const uint8_t zrqinit_queued[] = {
		'x', '*', '*', 0x18, 'B', '0', '0', 'a',
		'*', '*', 0x18, 'B', '0', '0', 'b'
	};
	static const uint8_t zrqinit_expected[] = {
		'*', 0x18, 'B', '0', '0', 'b'
	};

	check_compact(zrinit_queued, sizeof(zrinit_queued),
	    TRANSFER_READY_ZRINIT,
	    zrinit_expected, sizeof(zrinit_expected), 9);
	check_compact(zrqinit_queued, sizeof(zrqinit_queued),
	    TRANSFER_READY_ZRQINIT,
	    zrqinit_expected, sizeof(zrqinit_expected), 9);
	check_compact(zrqinit_queued, sizeof(zrqinit_queued),
	    TRANSFER_READY_ZRINIT,
	    zrqinit_queued, sizeof(zrqinit_queued), 0);
}

int
main(void)
{
	test_ticket_271();
	test_xymodem_requests();
	test_zmodem_requests();

	if (failures != 0) {
		fprintf(stderr, "xfer_queue_test: %u failure%s\n", failures,
		    failures == 1 ? "" : "s");
		return EXIT_FAILURE;
	}
	puts("xfer_queue_test: all checks passed");
	return EXIT_SUCCESS;
}
