/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: modem.c,v 1.32 2020/06/27 08:27:39 deuce Exp $ */

#include <stdbool.h>
#include <stdlib.h>

#include "bbslist.h"
#include "ciolib.h"
#include "comio.h"
#include "conn.h"
#include "modem.h"
#include "sockwrap.h"
#include "syncterm.h"
#include "uifcinit.h"

static COM_HANDLE com = COM_HANDLE_INVALID;
static bool seven_bits = false;

void
modem_input_thread(void *args)
{
	int    rd;
	int    buffered;
	size_t bufsz = 0;
	bool   monitor_dsr = true;

	SetThreadName("Modem Input");
	conn_api.input_thread_running = 1;
	if (args != NULL) {
		if ((comGetModemStatus(com) & COM_DSR) == 0)
			monitor_dsr = false;
	}
	while (com != COM_HANDLE_INVALID && !conn_api.terminate) {
		if (bufsz < BUFFER_SIZE) {
			rd = comReadBuf(com, (char *)conn_api.rd_buf + bufsz, conn_api.rd_buf_size - bufsz, NULL, bufsz ? 0 : 100);
			// Strip high bits... we *should* check the parity
			if (seven_bits) {
				for (int i = 0; i < rd; i++)
					conn_api.rd_buf[bufsz + i] &= 0x7f;
			}
			bufsz += rd;
		}
		if (bufsz) {
			assert_pthread_mutex_lock(&(conn_inbuf.mutex));
			conn_buf_wait_free(&conn_inbuf, 1, 1000);
			buffered = conn_buf_put(&conn_inbuf, conn_api.rd_buf, bufsz);
			memmove(conn_api.rd_buf, &conn_api.rd_buf[buffered], bufsz - buffered);
			bufsz -= buffered;
			assert_pthread_mutex_unlock(&(conn_inbuf.mutex));
		}
		if (args == NULL) {
			if ((comGetModemStatus(com) & COM_DCD) == 0)
				break;
		}
		else if (monitor_dsr) {
			if ((comGetModemStatus(com) & COM_DSR) == 0)
				break;
		}
	}
	conn_api.terminate = true;
	if (args != NULL)
		comLowerDTR(com);
	conn_api.input_thread_running = 2;
}

void
modem_output_thread(void *args)
{
	size_t wr;
	size_t sent;
	int  i;
	int  ret;
	bool monitor_dsr = true;

	SetThreadName("Modem Output");
	conn_api.output_thread_running = 1;
	if (args != NULL) {
		if ((comGetModemStatus(com) & COM_DSR) == 0)
			monitor_dsr = false;
	}
	while (com != COM_HANDLE_INVALID && !conn_api.terminate) {
		assert_pthread_mutex_lock(&(conn_outbuf.mutex));
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
			if (seven_bits) {
				for (i = 0; i < wr; i++)
					conn_api.wr_buf[i] &= 0x7f;
			}
			sent = 0;
			while (com != COM_HANDLE_INVALID && sent < wr && !conn_api.terminate) {
				// coverity[overflow:SUPPRESS]
				ret = comWriteBuf(com, conn_api.wr_buf + sent, wr - sent);
				if (ret > 0)
					sent += ret;
				if (ret < 0)
					break;
			}
		}
		else {
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
		if (args == NULL) {
			if ((comGetModemStatus(com) & COM_DCD) == 0)
				break;
		}
		else if (monitor_dsr) {
			if ((comGetModemStatus(com) & COM_DSR) == 0)
				break;
		}
	}
	conn_api.terminate = true;
	conn_api.output_thread_running = 2;
}

int
modem_response(char *str, size_t maxlen, int timeout)
{
	char   ch;
	size_t len = 0;
	uint64_t start;

	start = xp_fast_timer64();
	while (1) {
                /* Abort with keystroke */
		if (kbhit()) {
			switch (getch()) {
				case 0:
				case 0xe0:
					getch();
			}
			return 1;
		}

		if (xp_fast_timer64() - start >= timeout)
			return -1;
		if (len >= maxlen)
			return -1;
		if (!comReadByte(com, (unsigned char *)&ch)) {
			YIELD();
			continue;
		}
		if ((ch < ' ') && (len == 0)) /* ignore prepended control chars */
			continue;

		if (ch == '\r') {
// while(comReadByte(com,&ch));    /* eat trailing ctrl chars (e.g. 'LF') */
			break;
		}
		str[len++] = ch;
	}
	str[len] = 0;

	return 0;
}

int
modem_connect(struct bbslist *bbs)
{
	int  ret;
	char respbuf[1024];

	seven_bits = (bbs->data_bits == 7);

	if (!bbs->hidepopups)
		init_uifc(true, true);

	if ((bbs->conn_type == CONN_TYPE_SERIAL) || (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)) {
		if ((com = comOpen(bbs->addr)) == COM_HANDLE_INVALID) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Open Port", "`Cannot Open Port`\n\n"
				    "Cannot open the specified serial device.\n");
			conn_api.terminate = true;
			return -1;
		}
		if (bbs->bpsrate) {
			if (!comSetBaudRate(com, bbs->bpsrate)) {
				if (!bbs->hidepopups)
					uifcmsg("Cannot Set Baud Rate", "`Cannot Set Baud Rate`\n\n"
					    "Cannot open the specified serial device.\n");
				conn_api.terminate = true;
				comClose(com);
				return -1;
			}
		}
		if (!comSetParity(com, bbs->parity != SYNCTERM_PARITY_NONE, bbs->parity == SYNCTERM_PARITY_ODD)) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Set Parity", "`Cannot Set Parity`\n\n"
				    "Cannot open the specified serial device.\n");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		if (!comSetBits(com, bbs->data_bits, bbs->stop_bits)) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Set Data Bits", "`Cannot Set Data Bits`\n\n"
				    "Cannot open the specified serial device.\n");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		if (!comSetFlowControl(com, bbs->flow_control)) {
			conn_api.close();
			if (!bbs->hidepopups) {
				uifcmsg("Failed to set Flow Control", "`Failed to set Flow Control`\n\n"
				    "SyncTERM was unable to set flow control.\n");
			}
			return -1;
		}
		if (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)
			comLowerRTS(com);
		if (!comRaiseDTR(com)) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Raise DTR", "`Cannot Raise DTR`\n\n"
				    "comRaiseDTR() returned an error.\n");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
	}
	else {
		if ((com = comOpen(settings.mdm.device_name)) == COM_HANDLE_INVALID) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Open Modem", "`Cannot Open Modem`\n\n"
				    "Cannot open the specified modem device.\n");
			conn_api.terminate = true;
			return -1;
		}
		if (settings.mdm.com_rate) {
			if (!comSetBaudRate(com, settings.mdm.com_rate)) {
				if (!bbs->hidepopups)
					uifcmsg("Cannot Set Baud Rate", "`Cannot Set Baud Rate`\n\n"
					    "Cannot open the specified modem device.\n");
				conn_api.terminate = true;
				comClose(com);
				return -1;
			}
		}
		if (!comSetParity(com, bbs->parity != SYNCTERM_PARITY_NONE, bbs->parity == SYNCTERM_PARITY_ODD)) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Set Parity", "`Cannot Set Parity`\n\n"
				    "Cannot open the specified serial device.\n");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		if (!comSetBits(com, bbs->data_bits, bbs->stop_bits)) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Set Data Bits", "`Cannot Set Data Bits`\n\n"
				    "Cannot open the specified serial device.\n");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		if (!comSetFlowControl(com, bbs->flow_control)) {
			conn_api.close();
			if (!bbs->hidepopups) {
				uifcmsg("Failed to set Flow Control", "`Failed to set Flow Control`\n\n"
				    "SyncTERM was unable to set flow control.\n");
			}
			return -1;
		}

		if (!comRaiseDTR(com)) {
			if (!bbs->hidepopups)
				uifcmsg("Cannot Raise DTR", "`Cannot Raise DTR`\n\n"
				    "comRaiseDTR() returned an error.\n");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}

                /* drain keyboard input to avoid accidental cancel */
		while (kbhit())
			getch();

		/* Drain modem output buffer */
		while (comReadByte(com, (uchar*)respbuf))
			;
		respbuf[0] = 0;

		if (!bbs->hidepopups)
			uifc.pop("Initializing...");

		comWriteString(com, settings.mdm.init_string);
		comWriteString(com, "\r");

                /* Wait for "OK" */
		while (1) {
			if ((ret = modem_response(respbuf, sizeof(respbuf), 5)) != 0) {
				modem_close();
				if (!bbs->hidepopups) {
					uifc.pop(NULL);
					if (ret < 0) {
						uifcmsg("Modem Not Responding", "`Modem Not Responding`\n\n"
						    "The modem did not respond to the initializtion string\n"
						    "Check your init string and phone number.\n");
					}
				}
				conn_api.terminate = true;
				return -1;
			}
			if (strstr(respbuf, settings.mdm.init_string)) /* Echo is on */
				continue;
			break;
		}

		if (!strstr(respbuf, "OK")) {
			modem_close();
			if (!bbs->hidepopups) {
				uifc.pop(NULL);
				uifcmsg(respbuf, "`Initialization Error`\n\n"
				    "The modem did not respond favorably to your initialization string.\n");
			}
			conn_api.terminate = true;
			return -1;
		}

		if (!bbs->hidepopups) {
			uifc.pop(NULL);
			uifc.pop("Dialing...");
		}
		comWriteString(com, settings.mdm.dial_string);
		comWriteString(com, bbs->addr);
		comWriteString(com, "\r");

                /* Wait for "CONNECT" */
		while (1) {
			if ((ret = modem_response(respbuf, sizeof(respbuf), 60)) != 0) {
				modem_close();
				if (!bbs->hidepopups) {
					uifc.pop(NULL);
					if (ret < 0)
						uifcmsg(respbuf, "`No Answer`\n\n"
						    "The modem did not connect within 60 seconds.\n");
				}
				conn_api.terminate = true;
				return -1;
			}
			if (strstr(respbuf, bbs->addr)) /* Dial command echoed */
				continue;
			break;
		}

		if (!strstr(respbuf, "CONNECT")) {
			modem_close();
			if (!bbs->hidepopups) {
				uifc.pop(NULL);
				uifcmsg(respbuf, "`Connection Failed`\n\n"
				    "SyncTERM was unable to establish a connection.\n");
			}
			conn_api.terminate = true;
			return -1;
		}

		if (!bbs->hidepopups) {
			uifc.pop(NULL);
			uifc.pop(respbuf);
			SLEEP(1000);
			uifc.pop(NULL);
		}
	}

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		conn_api.close();
		return -1;
	}
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	if (!(conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		FREE_AND_NULL(conn_api.rd_buf);
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;

	if ((bbs->conn_type == CONN_TYPE_SERIAL) || (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)) {
		_beginthread(modem_output_thread, 0, (void *)-1);
		_beginthread(modem_input_thread, 0, (void *)-1);
	}
	else {
		_beginthread(modem_output_thread, 0, NULL);
		_beginthread(modem_input_thread, 0, NULL);
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	return 0;
}

int
serial_close(void)
{
	conn_api.terminate = true;

	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1)
		SLEEP(1);
	comClose(com);
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	return 0;
}

int
modem_close(void)
{
	int64_t start;
	char   garbage[1024];
	COM_HANDLE oldcom;

	conn_api.terminate = true;

	if ((comGetModemStatus(com) & COM_DCD) == 0) /* DCD already low */
		goto CLOSEIT;

        /* TODO:  We need a drain function */
	SLEEP(500);

	if (!comLowerDTR(com))
		goto CLOSEIT;

	start = xp_fast_timer64();
	oldcom = com;
	com = COM_HANDLE_INVALID;
	while (xp_fast_timer64() - start <= 10) {
		if ((comGetModemStatus(com) & COM_DCD) == 0)
			goto CLOSEIT;
		SLEEP(1000);
	}
	com = oldcom;

CLOSEIT:
	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	comClose(com);

	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	return 0;
}
