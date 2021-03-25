/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: conn_telnet.c,v 1.18 2020/05/03 20:12:42 deuce Exp $ */

#include <stdlib.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "threadwrap.h"

#include "bbslist.h"
#include "conn.h"
#include "term.h"
#include "uifcinit.h"

#include "telnet_io.h"
#include "rlogin.h"

extern int	telnet_log_level;

void *telnet_rx_parse_cb(const void *buf, size_t inlen, size_t *olen)
{
	// telnet_interpret() can add up to one byte to inbuf ('\r')
	void *ret = malloc(inlen + 1);

	if (ret == NULL)
		return ret;
	if (telnet_interpret((BYTE *)buf, inlen, ret, olen) != ret)
		memcpy(ret, buf, *olen);
	return ret;
}

void *telnet_tx_parse_cb(const void *buf, size_t len, size_t *olen)
{
	void *ret = malloc(len * 2);
	void *parsed;

	*olen = telnet_expand(buf, len, ret, len * 2
		,telnet_local_option[TELNET_BINARY_TX]!=TELNET_DO, (BYTE **)&parsed);

	if (parsed != ret)
		memcpy(ret, parsed, *olen);

	return ret;
}

int telnet_connect(struct bbslist *bbs)
{
	if (!bbs->hidepopups)
		init_uifc(TRUE, TRUE);

	telnet_log_level = bbs->telnet_loglevel;

	rlogin_sock=conn_socket_connect(bbs);
	if(rlogin_sock==INVALID_SOCKET)
		return(-1);

	if(!create_conn_buf(&conn_inbuf, BUFFER_SIZE))
		return(-1);
	if(!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		return(-1);
	}
	conn_api.rd_buf=(unsigned char *)malloc(BUFFER_SIZE);
	if(!conn_api.rd_buf) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return(-1);
	}
	conn_api.rd_buf_size=BUFFER_SIZE;
	conn_api.wr_buf=(unsigned char *)malloc(BUFFER_SIZE);
	if(!conn_api.wr_buf) {
		FREE_AND_NULL(conn_api.rd_buf);
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return(-1);
	}
	conn_api.wr_buf_size=BUFFER_SIZE;

	memset(telnet_local_option,0,sizeof(telnet_local_option));
	memset(telnet_remote_option,0,sizeof(telnet_remote_option));
	conn_api.rx_parse_cb = telnet_rx_parse_cb;
	conn_api.tx_parse_cb = telnet_tx_parse_cb;

	_beginthread(rlogin_output_thread, 0, NULL);
	_beginthread(rlogin_input_thread, 0, bbs);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	return(0);
}

void telnet_binary_mode_on(void)
{
	request_telnet_opt(TELNET_DO,TELNET_BINARY_TX);
	request_telnet_opt(TELNET_WILL,TELNET_BINARY_TX);
}

void telnet_binary_mode_off(void)
{
	request_telnet_opt(TELNET_DONT,TELNET_BINARY_TX);
	request_telnet_opt(TELNET_WONT,TELNET_BINARY_TX);
}
