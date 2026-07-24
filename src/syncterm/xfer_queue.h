#ifndef XFER_QUEUE_H
#define XFER_QUEUE_H

#include <stddef.h>
#include <stdint.h>

enum transfer_ready_sequence {
	TRANSFER_READY_ZRQINIT,
	TRANSFER_READY_ZRINIT,
	TRANSFER_READY_XYMODEM,
};

/*
 * Retain the suffix beginning with the newest applicable transfer-ready
 * sequence.  Returns the new length and optionally reports the number of
 * leading bytes removed.  A buffer without a recognized sequence is left
 * unchanged.
 */
size_t xfer_queue_compact(uint8_t *queued, size_t len,
    enum transfer_ready_sequence sequence, size_t *dropped);

#endif
