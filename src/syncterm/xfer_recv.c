#include "xfer_recv.h"

#include <stddef.h>
#include <stdint.h>

int
xfer_recv_byte_interruptible(void *cbdata, unsigned timeout,
    unsigned poll_interval, xfer_recv_byte_ms_fn recv_byte_ms,
    xfer_is_cancelled_fn is_cancelled)
{
	uint64_t remaining = (uint64_t)timeout * 1000;

	if (recv_byte_ms == NULL || is_cancelled == NULL || poll_interval == 0)
		return -1;

	do {
		unsigned wait;
		int      ch;

		if (is_cancelled(cbdata))
			return -1;
		wait = remaining > poll_interval
		    ? poll_interval : (unsigned)remaining;
		ch = recv_byte_ms(cbdata, wait);
		if (ch >= 0)
			return ch;
		remaining -= wait;
	} while (remaining > 0);

	return -1;
}
