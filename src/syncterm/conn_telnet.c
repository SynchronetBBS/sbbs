/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: conn_telnet.c,v 1.18 2020/05/03 20:12:42 deuce Exp $ */

#include <stdbool.h>
#include <stdlib.h>

#include "bbslist.h"
#include "conn.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "rlogin.h"
#include "sockwrap.h"
#include "telnet_io.h"
#include "term.h"
#include "threadwrap.h"
#include "uifcinit.h"

extern int telnet_log_level;

/*****************************************************************************/

// Escapes Telnet IACs in 'inbuf' by doubling the IAC char

// 'result' may point to either inbuf (if there were no IACs) or outbuf

// Returns the final byte count of the result

/*****************************************************************************/
static size_t
st_telnet_expand(const uchar *inbuf, size_t inlen, uchar *outbuf, size_t outlen, bool expand_cr, uchar **result)
{
	static bool last_was_lf = false;
	BYTE       *first_iac = (BYTE *)memchr(inbuf, TELNET_IAC, inlen);
	BYTE       *first_cr = NULL;

	if (inlen == 0) {
		if (result != NULL)
			*result = (uchar *)inbuf;
		return 0;
	}
	if (last_was_lf && (inbuf[0] == '\n')) {
		inbuf++;
		inlen--;
	}
	last_was_lf = false;
	if (expand_cr)
		first_cr = (BYTE *)memchr(inbuf, '\r', inlen);
	else
		last_was_lf = false;

	if ((first_iac == NULL) && (first_cr == NULL)) { /* Nothing to expand */
		if (result != NULL)
			*result = (uchar *)inbuf;
		return inlen;
	}

	size_t o;

	if ((first_iac != NULL) && ((first_cr == NULL) || (first_iac < first_cr)))
		o = first_iac - inbuf;
	else
		o = first_cr - inbuf;
	memcpy(outbuf, inbuf, o);

	for (size_t i = o; i < inlen && o < outlen; i++) {
		if ((inbuf[i] == '\n') && last_was_lf)
			continue;
		last_was_lf = false;
		if (inbuf[i] == TELNET_IAC)
			outbuf[o++] = TELNET_IAC;
		if (o >= outlen)
			break;
		outbuf[o++] = inbuf[i];
		if (expand_cr && (inbuf[i] == '\r') && (o < outlen)) {
			last_was_lf = true;
			outbuf[o++] = '\n'; // See RFC5198
		}
	}
	if (result != NULL)
		*result = outbuf;
	return o;
}

void *
telnet_rx_parse_cb(const void *buf, size_t inlen, size_t *olen)
{
        // telnet_interpret() can add up to one byte to inbuf ('\r')
	void *ret = malloc(inlen + 1);

	if (ret == NULL)
		return ret;
	if (telnet_interpret((BYTE *)buf, inlen, ret, olen) != ret)
		memcpy(ret, buf, *olen);
	return ret;
}

void *
telnet_tx_parse_cb(const void *buf, size_t len, size_t *olen)
{
	void *ret = malloc(len * 2);
	void *parsed;

	*olen = st_telnet_expand(buf, len, ret, len * 2,
	        telnet_local_option[TELNET_BINARY_TX] != TELNET_DO, (BYTE **)&parsed);

	if (parsed != ret)
		memcpy(ret, parsed, *olen);

	return ret;
}

int
telnet_connect(struct bbslist *bbs)
{
	if (!bbs->hidepopups)
		init_uifc(true, true);

	telnet_log_level = bbs->telnet_loglevel;

	rlogin_sock = conn_socket_connect(bbs);
	if (rlogin_sock == INVALID_SOCKET)
		return -1;

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE))
		return -1;
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		return -1;
	}
	conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE);
	if (!conn_api.rd_buf) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE);
	if (!conn_api.wr_buf) {
		FREE_AND_NULL(conn_api.rd_buf);
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;

	memset(telnet_local_option, 0, sizeof(telnet_local_option));
	memset(telnet_remote_option, 0, sizeof(telnet_remote_option));
	conn_api.rx_parse_cb = telnet_rx_parse_cb;
	conn_api.tx_parse_cb = telnet_tx_parse_cb;

	_beginthread(rlogin_output_thread, 0, NULL);
	_beginthread(rlogin_input_thread, 0, bbs);
	// Suppress Go Aheads (both directions)
	request_telnet_opt(TELNET_WILL, TELNET_SUP_GA);
	request_telnet_opt(TELNET_DO, TELNET_SUP_GA);
	if (!bbs->telnet_no_binary) {
		// Enable binary mode (both directions)
		request_telnet_opt(TELNET_WILL, TELNET_BINARY_TX);
		request_telnet_opt(TELNET_DO, TELNET_BINARY_TX);
	}
	// Request that the server echos
	request_telnet_opt(TELNET_DO, TELNET_ECHO);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	return 0;
}

void
telnet_binary_mode_on(void)
{
	request_telnet_opt(TELNET_DO, TELNET_BINARY_TX);
	request_telnet_opt(TELNET_WILL, TELNET_BINARY_TX);
}

void
telnet_binary_mode_off(void)
{
	request_telnet_opt(TELNET_DONT, TELNET_BINARY_TX);
	request_telnet_opt(TELNET_WONT, TELNET_BINARY_TX);
}
