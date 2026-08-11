/* Copyright (C), 2007 by Stephen Hurd */

#include <stdbool.h>
#include <stdlib.h>

#include "bbslist.h"
#include "ciolib.h"
#include "comio.h"
#include "conn.h"
#include "conn_log.h"
#include "gen_defs.h"
#include "host_ui.h"
#include "modem.h"
#include "sockwrap.h"
#include "syncterm.h"

static _Atomic(COM_HANDLE) com = COM_HANDLE_INVALID;
static bool seven_bits = false;
static const char *modem_log_source = "Serial";

static void
modem_report(struct bbslist *bbs, const char *title,
	const char *log_message, const char *display_message)
{
	conn_logf(modem_log_source, LOG_ERR, "%s", log_message);
	if (!bbs->hidepopups)
		conn_log_alert(title, display_message);
}

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
			assert_pthread_mutex_lock(&(conn_inbuf.write_mutex));
			conn_buf_wait_free(&conn_inbuf, 1, 1000);
			buffered = conn_buf_put(&conn_inbuf, conn_api.rd_buf, bufsz);
			memmove(conn_api.rd_buf, &conn_api.rd_buf[buffered], bufsz - buffered);
			bufsz -= buffered;
			assert_pthread_mutex_unlock(&(conn_inbuf.write_mutex));
		}
		if (args == NULL) {
			if ((comGetModemStatus(com) & COM_DCD) == 0) {
				conn_logf(modem_log_source, LOG_INFO, "carrier detect dropped");
				break;
			}
		}
		else if (monitor_dsr) {
			if ((comGetModemStatus(com) & COM_DSR) == 0) {
				conn_logf(modem_log_source, LOG_INFO, "data-set-ready dropped");
				break;
			}
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
	size_t i;
	int  ret;
	bool monitor_dsr = true;

	SetThreadName("Modem Output");
	conn_api.output_thread_running = 1;
	if (args != NULL) {
		if ((comGetModemStatus(com) & COM_DSR) == 0)
			monitor_dsr = false;
	}
	while (com != COM_HANDLE_INVALID && !conn_api.terminate) {
		assert_pthread_mutex_lock(&(conn_outbuf.read_mutex));
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			assert_pthread_mutex_unlock(&(conn_outbuf.read_mutex));
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
					conn_logf(modem_log_source, LOG_ERR,
					    "serial write failed");
				if (ret < 0)
					break;
			}
		}
		else {
			assert_pthread_mutex_unlock(&(conn_outbuf.read_mutex));
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
		if (quitting)
			return 1;
                /* Abort with keystroke */
		if (kbhit()) {
			(void)syncterm_getkey();
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
	int  fc;
	char respbuf[1024];

	modem_log_source = bbs->conn_type == CONN_TYPE_MODEM ? "Modem"
	    : bbs->conn_type == CONN_TYPE_SERIAL_NORTS ? "Serial-3-Wire"
	    : "Serial";
	seven_bits = (bbs->data_bits == 7);

	if ((bbs->conn_type == CONN_TYPE_SERIAL) || (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)) {
		conn_logf(modem_log_source, LOG_INFO,
		    "opening serial interface (device omitted)");
		com = comOpen(bbs->addr);
		if (com == COM_HANDLE_INVALID) {
			modem_report(bbs, "Cannot Open Port",
			    "cannot open serial device",
			    "Cannot open the specified serial device.");
			conn_api.terminate = true;
			return -1;
		}
		if (bbs->bpsrate) {
			if (!comSetBaudRate(com, bbs->bpsrate)) {
				modem_report(bbs, "Cannot Set Baud Rate",
				    "cannot set serial baud rate",
				    "Cannot set the serial device baud rate.");
				conn_api.terminate = true;
				comClose(com);
				return -1;
			}
		}
		if (!comSetParity(com, bbs->parity != SYNCTERM_PARITY_NONE, bbs->parity == SYNCTERM_PARITY_ODD)) {
			modem_report(bbs, "Cannot Set Parity",
			    "cannot set serial parity",
			    "Cannot set parity on the serial device.");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		if (!comSetBits(com, bbs->data_bits, bbs->stop_bits)) {
			modem_report(bbs, "Cannot Set Data Bits",
			    "cannot set serial data and stop bits",
			    "Cannot set data and stop bits on the serial device.");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		/* 3-wire serial has no RTS/CTS lines wired; asking the
		 * tty layer to flow-control on them would block all output
		 * as soon as the kernel sees CTS low (which it always is,
		 * since the line isn't connected).  Mask RTS/CTS out for
		 * NORTS regardless of what the user picked in the bbslist
		 * UI. */
		fc = bbs->flow_control;
		if (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)
			fc &= ~COM_FLOW_CONTROL_RTS_CTS;
		if (!comSetFlowControl(com, fc)) {
			conn_api.close();
			modem_report(bbs, "Failed to Set Flow Control",
			    "cannot set serial flow control",
			    "SyncTERM was unable to set flow control.");
			return -1;
		}
		if (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)
			comLowerRTS(com);
		if (!comRaiseDTR(com)) {
			modem_report(bbs, "Cannot Raise DTR",
			    "cannot raise DTR", "comRaiseDTR() returned an error.");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
	}
	else {
		conn_logf(modem_log_source, LOG_INFO,
		    "opening modem (device and phone number omitted)");
		com = comOpen(settings.mdm.device_name);
		if (com == COM_HANDLE_INVALID) {
			modem_report(bbs, "Cannot Open Modem",
			    "cannot open modem device",
			    "Cannot open the specified modem device.");
			conn_api.terminate = true;
			return -1;
		}
		if (settings.mdm.com_rate) {
			if (!comSetBaudRate(com, settings.mdm.com_rate)) {
				modem_report(bbs, "Cannot Set Baud Rate",
				    "cannot set modem baud rate",
				    "Cannot set the modem device baud rate.");
				conn_api.terminate = true;
				comClose(com);
				return -1;
			}
		}
		if (!comSetParity(com, bbs->parity != SYNCTERM_PARITY_NONE, bbs->parity == SYNCTERM_PARITY_ODD)) {
			modem_report(bbs, "Cannot Set Parity",
			    "cannot set modem parity",
			    "Cannot set parity on the modem device.");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		if (!comSetBits(com, bbs->data_bits, bbs->stop_bits)) {
			modem_report(bbs, "Cannot Set Data Bits",
			    "cannot set modem data and stop bits",
			    "Cannot set data and stop bits on the modem device.");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}
		if (!comSetFlowControl(com, bbs->flow_control)) {
			conn_api.close();
			modem_report(bbs, "Failed to Set Flow Control",
			    "cannot set modem flow control",
			    "SyncTERM was unable to set flow control.");
			return -1;
		}

		if (!comRaiseDTR(com)) {
			modem_report(bbs, "Cannot Raise DTR",
			    "cannot raise DTR", "comRaiseDTR() returned an error.");
			conn_api.terminate = true;
			comClose(com);
			return -1;
		}

		/* drain keyboard input to avoid accidental cancel */
		while (kbhit()) {
			int ch = syncterm_getkey();
			if (ch == CIO_KEY_MOUSE)
				getmouse(NULL);
			if (quitting)
				break;
		}

		/* Drain modem output buffer */
		while (comReadByte(com, (uchar*)respbuf))
			;
		respbuf[0] = 0;

		if (!bbs->hidepopups)
			host_ui_status("Initializing...");

		conn_logf(modem_log_source, LOG_INFO,
		    "sending initialization command (contents omitted)");
		comWriteString(com, settings.mdm.init_string);
		comWriteString(com, "\r");

                /* Wait for "OK" */
		while (1) {
			if ((ret = modem_response(respbuf, sizeof(respbuf), 5)) != 0) {
				modem_close();
				if (!bbs->hidepopups)
					host_ui_status(NULL);
				if (ret < 0)
					modem_report(bbs, "Modem Not Responding",
					    "initialization response timed out",
					    "The modem did not respond to the initialization string.\n"
					    "Check the initialization string and phone number.");
				else
					conn_logf(modem_log_source, LOG_INFO,
					    "initialization cancelled by user");
				conn_api.terminate = true;
				return -1;
			}
			if (strstr(respbuf, settings.mdm.init_string)) /* Echo is on */
				continue;
			break;
		}

		if (!strstr(respbuf, "OK")) {
			conn_logf(modem_log_source, LOG_ERR,
			    "modem rejected initialization command (response omitted)");
			modem_close();
			if (!bbs->hidepopups) {
				host_ui_status(NULL);
				conn_log_alert("Modem Initialization Error", respbuf);
			}
			conn_api.terminate = true;
			return -1;
		}
		conn_logf(modem_log_source, LOG_INFO,
		    "modem accepted initialization command");

		if (!bbs->hidepopups) {
			host_ui_status(NULL);
			host_ui_status("Dialing...");
		}
		comWriteString(com, settings.mdm.dial_string);
		comWriteString(com, bbs->addr);
		comWriteString(com, "\r");
		conn_logf(modem_log_source, LOG_INFO,
		    "dial command sent (command and phone number omitted)");

                /* Wait for "CONNECT" */
		while (1) {
			if ((ret = modem_response(respbuf, sizeof(respbuf), 60)) != 0) {
				modem_close();
				if (!bbs->hidepopups)
					host_ui_status(NULL);
				if (ret < 0)
					modem_report(bbs, "No Answer",
					    "dial response timed out", respbuf);
				else
					conn_logf(modem_log_source, LOG_INFO,
					    "dial cancelled by user");
				conn_api.terminate = true;
				return -1;
			}
			if (strstr(respbuf, bbs->addr)) /* Dial command echoed */
				continue;
			break;
		}

		if (!strstr(respbuf, "CONNECT")) {
			conn_logf(modem_log_source, LOG_ERR,
			    "modem rejected dial request (response omitted)");
			modem_close();
			if (!bbs->hidepopups) {
				host_ui_status(NULL);
				conn_log_alert("Connection Failed", respbuf);
			}
			conn_api.terminate = true;
			return -1;
		}
		conn_logf(modem_log_source, LOG_INFO, "carrier established");

		if (!bbs->hidepopups) {
			host_ui_status(NULL);
			host_ui_status(respbuf);
			SLEEP(1000);
			host_ui_status(NULL);
		}
	}

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		modem_report(bbs, "Connection Error",
		    "failed to allocate input buffer",
		    "Unable to allocate the connection input buffer.");
		conn_api.close();
		return -1;
	}
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		modem_report(bbs, "Connection Error",
		    "failed to allocate output buffer",
		    "Unable to allocate the connection output buffer.");
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		modem_report(bbs, "Connection Error",
		    "failed to allocate receive workspace",
		    "Unable to allocate the connection receive workspace.");
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	if (!(conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		modem_report(bbs, "Connection Error",
		    "failed to allocate send workspace",
		    "Unable to allocate the connection send workspace.");
		conn_api.close();
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		FREE_AND_NULL(conn_api.rd_buf);
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;
	if (bbs->conn_type != CONN_TYPE_MODEM)
		conn_logf(modem_log_source, LOG_INFO,
		    "serial link configured baud=%lu data-bits=%d stop-bits=%d parity=%d flow=0x%X",
		    (unsigned long)bbs->bpsrate, bbs->data_bits, bbs->stop_bits,
		    bbs->parity, fc);

	if ((bbs->conn_type == CONN_TYPE_SERIAL) || (bbs->conn_type == CONN_TYPE_SERIAL_NORTS)) {
		_beginthread(modem_output_thread, 0, (void *)-1);
		_beginthread(modem_input_thread, 0, (void *)-1);
	}
	else {
		_beginthread(modem_output_thread, 0, NULL);
		_beginthread(modem_input_thread, 0, NULL);
	}

	if (!bbs->hidepopups)
		host_ui_status(NULL);
	conn_logf(modem_log_source, LOG_INFO, "session established");

	return 0;
}

int
serial_close(void)
{
	conn_api.terminate = true;
	conn_logf(modem_log_source, LOG_DEBUG, "closing serial interface");

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
	conn_logf(modem_log_source, LOG_DEBUG, "closing modem and lowering DTR");

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
		if ((comGetModemStatus(oldcom) & COM_DCD) == 0)
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
