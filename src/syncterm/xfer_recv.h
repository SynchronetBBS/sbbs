#ifndef XFER_RECV_H
#define XFER_RECV_H

typedef int (*xfer_recv_byte_ms_fn)(void *, unsigned);
typedef int (*xfer_is_cancelled_fn)(void *);

/*
 * Apply a seconds-based protocol timeout in short millisecond intervals so
 * a locally requested cancellation can interrupt the wait.
 */
int xfer_recv_byte_interruptible(void *cbdata, unsigned timeout,
    unsigned poll_interval, xfer_recv_byte_ms_fn recv_byte_ms,
    xfer_is_cancelled_fn is_cancelled);

#endif
