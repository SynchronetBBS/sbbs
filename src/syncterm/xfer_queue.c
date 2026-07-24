#include "xfer_queue.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define XFER_NAK   0x15
#define XFER_ZDLE  0x18
#define XFER_ZPAD  0x2a
#define XFER_ZHEX  0x42

static bool
is_xymodem_ready(uint8_t ch)
{
	return ch == XFER_NAK || ch == 'C' || ch == 'G';
}

size_t
xfer_queue_compact(uint8_t *queued, size_t len,
    enum transfer_ready_sequence sequence, size_t *dropped)
{
	size_t keep = SIZE_MAX;

	if (dropped != NULL)
		*dropped = 0;
	if (queued == NULL)
		return len;

	switch (sequence) {
		case TRANSFER_READY_ZRQINIT:
		case TRANSFER_READY_ZRINIT:
			for (size_t i = 0; i + 5 <= len; i++) {
				if (queued[i] == XFER_ZPAD
				    && queued[i + 1] == XFER_ZDLE
				    && queued[i + 2] == XFER_ZHEX
				    && queued[i + 3] == '0'
				    && queued[i + 4] ==
				        (sequence == TRANSFER_READY_ZRQINIT ? '0' : '1'))
					keep = i;
			}
			break;
		case TRANSFER_READY_XYMODEM:
			for (size_t i = 1; i < len; i++) {
				if (queued[i - 1] == queued[i]
				    && is_xymodem_ready(queued[i]))
					keep = i;
			}
			break;
	}

	if (keep == SIZE_MAX)
		return len;
	if (keep > 0)
		memmove(queued, queued + keep, len - keep);
	if (dropped != NULL)
		*dropped = keep;
	return len - keep;
}
