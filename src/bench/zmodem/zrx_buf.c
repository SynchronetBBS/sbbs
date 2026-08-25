/* Minimal ZMODEM *receiver* linking the REAL sbbs3 zmodem.c protocol engine
 * behind a plain buffered recv_byte -- single-threaded, no ring buffer, no
 * input thread, no telnet-IAC state machine.
 *
 * The receive-side counterpart of ztx_buf.c, and it exists for the same
 * reason: to split "how much of the receive cost is zmodem.c" from "how much
 * is the transport wrapper around it".  sexyz.c's recv_byte accounts for 30%
 * of the instructions in a sexyz receive, and SyncTERM does NOT share that
 * code -- its own recv_byte (src/syncterm/term.c) is a buffer-index fetch
 * much like the one here.  So a sexyz receive number cannot be quoted as
 * zmodem.c's, and this tool is what measures the shared part.
 *
 * By default this passes a real is_cancelled callback, matching SYNCTERM
 * (term.c passes xfer_zmodem_check_abort).  sexyz passes NULL there, so it
 * pays one fewer indirect call per byte -- zmodem_rx's loop condition is
 * "while (is_connected(zm) && !is_cancelled(zm))", evaluated per byte, and
 * both helpers dispatch through a function pointer when one is supplied.
 * Set ZRX_NO_CANCEL=1 to pass NULL instead and measure that difference.
 *
 * Same caveat as ztx_buf: this is NOT a SyncTERM throughput model.  SyncTERM's
 * real-world speed is a deliberate BDP/socket-buffer choice, and its input path
 * additionally runs wren/RIP filters.  This isolates the ENGINE's per-byte
 * receive cost, nothing more.
 *
 * Reads from fd0, writes the back-channel to fd1.  Run under the socket
 * harness against a real lsz sender:
 *   zbench_sock.py --file F --outdir D --sender "lsz -8 F" --receiver "zrx_buf"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include "gen_defs.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "zmodem.h"

#define NOINP (-1)

static uchar    obuf[16384];
static unsigned olen = 0;
static uchar    ibuf[65536];
static unsigned ipos = 0, ilen = 0;
static BOOL     connected = TRUE;

static void obuf_write(void)
{
	unsigned off = 0;
	while (off < olen) {
		int w = write(STDOUT_FILENO, obuf + off, olen - off);
		if (w <= 0) { connected = FALSE; break; }
		off += w;
	}
	olen = 0;
}

static int my_send_byte(void* u, uchar ch, unsigned timeout)
{
	if (olen >= sizeof(obuf))
		obuf_write();
	obuf[olen++] = ch;
	return 0;   /* SEND_SUCCESS */
}

static void my_flush(void* u)
{
	if (olen)
		obuf_write();
}

static int fill_ibuf(unsigned timeout_sec)
{
	struct pollfd p = { .fd = STDIN_FILENO, .events = POLLIN };
	int n;
	if (ipos < ilen)
		return ilen - ipos;
	/* The engine asks for the back-channel with the send buffer still
	 * unsent; push it first or the peer never sees our ZRPOS/ZACK. */
	my_flush(NULL);
	n = poll(&p, 1, (int)timeout_sec * 1000);
	if (n <= 0)
		return 0;
	n = read(STDIN_FILENO, ibuf, sizeof(ibuf));
	if (n <= 0) { if (n == 0) connected = FALSE; return 0; }
	ipos = 0; ilen = n;
	return ilen;
}

static int my_recv_byte(void* u, unsigned timeout)
{
	if (ipos >= ilen) {
		if (fill_ibuf(timeout) <= 0)
			return NOINP;
	}
	return ibuf[ipos++];
}

/* Bulk receive: copy while the engine's table says the byte needs no
   zmodem_rx() handling, stopping before the first one that does.  Never
   refills -- my_recv_byte() owns the blocking. */
static uint64_t span_calls, span_bytes, span_empty;
static size_t my_recv_span(void* u, uint8_t* buf, size_t maxlen, const uint8_t* plain_tab)
{
	size_t n = 0;

	while (n < maxlen && ipos < ilen && plain_tab[ibuf[ipos]])
		buf[n++] = ibuf[ipos++];
	span_calls++;
	span_bytes += n;
	if (n == 0)
		span_empty++;
	return n;
}

static BOOL my_data_waiting(void* u, unsigned timeout)
{
	if (ipos < ilen)
		return TRUE;
	return fill_ibuf(timeout) > 0;
}

static BOOL my_is_connected(void* u)  { return connected || ipos < ilen; }
static BOOL my_is_cancelled(void* u)  { return FALSE; }
static BOOL no_cancel_cb(void) { const char* e = getenv("ZRX_NO_CANCEL"); return e && *e == '1'; }
static int  my_lputs(void* u, int level, const char* str)
{
	fprintf(stderr, "%s\n", str);
	return 0;
}
static void my_progress(void* u, int64_t pos) { }

int main(int argc, char** argv)
{
	zmodem_t zm;
	uint64_t received = 0;
	const char* dir = (argc >= 2) ? argv[1] : ".";

	memset(&zm, 0, sizeof(zm));
	zmodem_init(&zm, NULL, my_lputs, my_progress, my_send_byte, my_recv_byte,
	            my_is_connected, no_cancel_cb() ? NULL : my_is_cancelled,
	            my_data_waiting, my_flush);
	if (getenv("ZRX_NO_SPAN") == NULL)
		zm.recv_span = my_recv_span;
	zm.want_fcs_16 = FALSE;      /* CRC-32 */
	zm.can_full_duplex = TRUE;
	zm.duplicate_filename = NULL;

	zmodem_recv_files(&zm, dir, &received);
	my_flush(NULL);
	fprintf(stderr, "zrx_buf: received %llu bytes\n", (unsigned long long)received);
	fprintf(stderr, "zrx_buf: span calls=%llu (empty=%llu) bytes=%llu avg_run=%.1f cover=%.1f%%\n",
	    (unsigned long long)span_calls, (unsigned long long)span_empty,
	    (unsigned long long)span_bytes,
	    span_calls > span_empty ? (double)span_bytes / (span_calls - span_empty) : 0.0,
	    received ? 100.0 * span_bytes / received : 0.0);
	return 0;
}
