#define WIN32_LEAN_AND_MEAN
#include <limits.h>
#include <stdbool.h>
#include <windows.h>
#include <wincon.h>

#if NTDDI_VERSION >= 0x0A000006

#include "bbslist.h"
#include "conn.h"
#include "conn_log.h"
#include "fonts.h"
#include "gen_defs.h"
#include "host_ui.h"
#include "window.h"

HANDLE inputRead, inputWrite, outputRead, outputWrite;
PROCESS_INFORMATION pi;
HPCON cpty;
enum ciolib_codepage codepage;

static void
conpty_error(struct bbslist *bbs, const char *message)
{
	conn_logf("Shell", LOG_ERR, "%s", message);
	if (!bbs->hidepopups)
		conn_log_alert("Shell Error", message);
}

static size_t
get_utf8_span(const uint8_t *b, size_t sz)
{
	size_t ret = 0;
	const uint8_t *last = &b[sz - 1];

	while (b <= last) {
		if ((*b & 0x80) == 0) {
			ret++;
			b++;
		}
		else if ((*b & 0xe0) == 0xc0) {
			b += 2;
			if ((b - 1) <= last)
				ret += 2;
		}
		else if ((*b & 0xf0) == 0xe0) {
			b += 3;
			if ((b - 1) <= last)
				ret += 3;
		}
		else if ((*b & 0xf8) == 0xf0) {
			b += 4;
			if ((b - 1) <= last)
				ret += 4;
		}
		else
			return SIZE_MAX;
	}
	return ret;
}

static void
conpty_input_thread(void *args)
{
	DWORD  rd;
	size_t buffered;
	size_t buffer;
	DWORD  ec;
	size_t fill = 0;
	size_t utf8_span = 0;

	SetThreadName("PTY Input");
	conn_api.input_thread_running = 1;
	while (!conn_api.terminate) {
		if (GetExitCodeProcess(pi.hProcess, &ec)) {
			if (ec != STILL_ACTIVE) {
				conn_logf("Shell", LOG_INFO, "child exit status=%lu",
				    (unsigned long)ec);
				break;
			}
		}
		else {
			conn_logf("Shell", LOG_ERR,
			    "GetExitCodeProcess failed windows-error=%lu",
			    (unsigned long)GetLastError());
			break;
		}
		if (!ReadFile(outputRead, conn_api.rd_buf + fill, conn_api.rd_buf_size - fill, &rd, NULL)) {
			if (!conn_api.terminate)
				conn_logf("Shell", LOG_ERR,
				    "ConPTY read failed windows-error=%lu",
				    (unsigned long)GetLastError());
			break;
		}
		fill += rd;
		utf8_span = get_utf8_span(conn_api.rd_buf, fill);
		if (utf8_span == SIZE_MAX)
			break;
		size_t sz;
		char *cps = utf8_to_cp(codepage, conn_api.rd_buf, '?', utf8_span, &sz);
		if (cps == NULL)
			break;
		buffered = 0;
		while (!conn_api.terminate && buffered < sz) {
			assert_pthread_mutex_lock(&(conn_inbuf.write_mutex));
			buffer = conn_buf_wait_free(&conn_inbuf, sz - buffered, 100);
			buffered += conn_buf_put(&conn_inbuf, cps + buffered, buffer);
			assert_pthread_mutex_unlock(&(conn_inbuf.write_mutex));
		}
		fill -= utf8_span;
		if (fill)
			memmove(conn_api.rd_buf, &conn_api.rd_buf[utf8_span], fill);
		free(cps);
	}
	conn_api.terminate = true;
	conn_api.input_thread_running = 2;
}

static void
conpty_output_thread(void *args)
{
	int   wr;
	DWORD ret;
	DWORD ec;

	SetThreadName("PTY Output");
	conn_api.output_thread_running = 1;
	while (!conn_api.terminate) {
		if (GetExitCodeProcess(pi.hProcess, &ec)) {
			if (ec != STILL_ACTIVE)
				break;
		}
		else {
			break;
		}
		assert_pthread_mutex_lock(&(conn_outbuf.read_mutex));
		ret = 0;
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			assert_pthread_mutex_unlock(&(conn_outbuf.read_mutex));
			size_t sz;
			uint8_t *utf = cp_to_utf8(codepage, conn_api.wr_buf, wr, &sz);
			if (utf == NULL)
				break;
			size_t sent = 0;
			while (!conn_api.terminate && sent < sz) {
				if (!WriteFile(inputWrite, utf + sent, sz - sent, &ret, NULL)) {
					conn_logf("Shell", LOG_ERR,
					    "ConPTY write failed windows-error=%lu",
					    (unsigned long)GetLastError());
					conn_api.terminate = true;
					break;
				}
				sent += ret;
			}
			free(utf);
		}
		else {
			assert_pthread_mutex_unlock(&(conn_outbuf.read_mutex));
		}
	}
	conn_api.terminate = true;
	conn_api.output_thread_running = 2;
}

int conpty_connect(struct bbslist *bbs)
{
	HANDLE heap = GetProcessHeap();
	conn_logf("Shell", LOG_INFO,
	    "starting local shell session (command omitted)");

	int w, h;
	get_term_win_size(&w, &h, NULL, NULL, &bbs->nostatus);
	codepage = conio_fontdata[find_font_id(bbs->font)].cp;

	COORD size = {
		.X = w,
		.Y = h
	};
	STARTUPINFOEXA si = {
		.StartupInfo = {
			.cb = sizeof(STARTUPINFOEXA)
		}
	};
	SIZE_T sz = 0;
	// "Note  This initial call will return an error by design. This is expected behavior."
	InitializeProcThreadAttributeList(NULL, 1, 0, &sz);
	si.lpAttributeList = HeapAlloc(heap, 0, sz);
	if (si.lpAttributeList == NULL) {
		conpty_error(bbs, "HeapAlloc failed");
		return -1;
	}

	char *cmd = bbs->addr;
	if (cmd[0] == 0)
		cmd = getenv("ComSpec");
	if (cmd == NULL)  {
		conpty_error(bbs, "No command shell found");
		return -1;
	}
	if (!CreatePipe(&inputRead, &inputWrite, NULL, 0)) {
		conpty_error(bbs, "CreatePipe (input) failed");
		return -1;
	}
	if (!CreatePipe(&outputRead, &outputWrite, NULL, 0)) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		conpty_error(bbs, "CreatePipe (output) failed");
		return -1;
	}
	if (FAILED(CreatePseudoConsole(size, inputRead, outputWrite, 0, &cpty))) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		conpty_error(bbs, "CreatePseudoConsole failed");
		return -1;
	}
	if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &sz)) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		conpty_error(bbs, "InitializeProcThreadAttributeList failed");
		return -1;
	}

	if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, cpty, sizeof(cpty), NULL, NULL)) {
		DeleteProcThreadAttributeList(si.lpAttributeList);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		conpty_error(bbs, "UpdateProcThreadAttribute failed");
		return -1;
	}

	if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi)) {
		DeleteProcThreadAttributeList(si.lpAttributeList);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		conpty_error(bbs, "CreateProcessA failed");
		return -1;
	}
	DeleteProcThreadAttributeList(si.lpAttributeList);
	HeapFree(heap, 0, si.lpAttributeList);
	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		conpty_error(bbs, "create_conn_buf (input) failed");
		return -1;
	}
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		conpty_error(bbs, "create_conn_buf (output) failed");
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		conpty_error(bbs, "malloc (input) failed");
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	if (!(conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		free(conn_api.rd_buf);
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		conpty_error(bbs, "malloc (output) failed");
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;

	_beginthread(conpty_output_thread, 0, NULL);
	_beginthread(conpty_input_thread, 0, NULL);
	conn_logf("Shell", LOG_INFO, "local shell session established");

	return 0;
}

/* Resize the child console via ResizePseudoConsole.  Pixel args are
 * ignored — conpty is cell-based.  No-op on any failure. */
void
conpty_send_window_change(int text_cols, int text_rows,
    int pixel_cols, int pixel_rows)
{
	(void)pixel_cols;
	(void)pixel_rows;
	if (text_cols <= 0 || text_rows <= 0)
		return;
	if (text_cols > SHRT_MAX || text_rows > SHRT_MAX)
		return;

	COORD size;
	size.X = (SHORT)text_cols;
	size.Y = (SHORT)text_rows;
	(void)ResizePseudoConsole(cpty, size);
}

int
conpty_close(void)
{
	char garbage[1024];
	DWORD ret;

	bool local_close = !conn_api.terminate;
	conn_api.terminate = true;
	conn_logf("Shell", local_close ? LOG_DEBUG : LOG_INFO,
	    local_close ? "local shell teardown requested"
	                : "closing after child or ConPTY teardown");
	TerminateProcess(pi.hProcess, 0);
	WaitForSingleObject(pi.hProcess, 1000);
	ClosePseudoConsole(cpty);
	WriteFile(outputWrite, "Die", 3, &ret, NULL);
	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	CloseHandle(inputRead);
	CloseHandle(inputWrite);
	CloseHandle(outputRead);
	CloseHandle(outputWrite);
	return 0;
}

#endif
