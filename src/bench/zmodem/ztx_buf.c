/* Minimal ZMODEM sender linking the REAL sbbs3 zmodem.c protocol engine,
 * but with a SyncTERM-style BUFFERED send_byte (accumulate + bulk write,
 * single-threaded, no ring buffer / no output thread).
 *
 * Purpose: prove that the sexyz sender's ~30x slowdown is the sexyz.c
 * transmit architecture, not zmodem.c. Same protocol code + buffered send
 * should run at lrzsz/Forsberg speeds -> confirms SyncTERM (which uses this
 * same buffered pattern in term.c) is immune.
 *
 * stdio mode: reads back-channel from fd0, writes to fd1. Run via the socket
 * harness against a real lrz receiver.
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
	n = poll(&p, 1, (int)timeout_sec * 1000);
	if (n <= 0)
		return 0;
	if (p.revents & (POLLHUP | POLLERR)) {
		/* still try one read to drain */
	}
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

static BOOL my_data_waiting(void* u, unsigned timeout)
{
	if (ipos < ilen)
		return TRUE;
	return fill_ibuf(timeout) > 0;
}

static BOOL my_is_connected(void* u)  { return connected; }
static BOOL my_is_cancelled(void* u)  { return FALSE; }
static int  my_lputs(void* u, int level, const char* str)
{
	fprintf(stderr, "%s\n", str);
	return 0;
}
static void my_progress(void* u, int64_t pos) { }

int main(int argc, char** argv)
{
	zmodem_t zm;
	FILE*    fp;
	time_t   start;
	uint64_t sent = 0;
	unsigned maxblk = 8192;   /* -8 default */

	if (argc < 2) { fprintf(stderr, "usage: ztx_buf file [maxblk]\n"); return 1; }
	if (argc >= 3) maxblk = (unsigned)atoi(argv[2]);

	if ((fp = fopen(argv[1], "rb")) == NULL) {
		fprintf(stderr, "cannot open %s\n", argv[1]); return 1;
	}

	memset(&zm, 0, sizeof(zm));
	zmodem_init(&zm, NULL, my_lputs, my_progress, my_send_byte, my_recv_byte,
	            my_is_connected, my_is_cancelled, my_data_waiting, my_flush);
	zm.block_size = 1024;
	zm.max_block_size = maxblk;
	zm.want_fcs_16 = FALSE;      /* CRC-32 */
	zm.can_full_duplex = TRUE;

	zmodem_send_file(&zm, argv[1], fp, /*request_init*/ TRUE, &start, &sent);
	zmodem_get_zfin(&zm);
	my_flush(NULL);
	fclose(fp);
	fprintf(stderr, "ztx_buf: sent %llu bytes\n", (unsigned long long)sent);
	return 0;
}
