#define WIN32_LEAN_AND_MEAN
#include <stdatomic.h>
#include <windows.h>
#include <wincon.h>

#if NTDDI_VERSION >= 0x0A000006

#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"
#include "window.h"

HANDLE inputRead, inputWrite, outputRead, outputWrite;
PROCESS_INFORMATION pi;
HPCON cpty;

static atomic_bool terminate;

static void
conpty_input_thread(void *args)
{
	DWORD  rd;
	int    buffered;
	size_t buffer;
	int    i;
	DWORD  ec;

	SetThreadName("PTY Input");
	conn_api.input_thread_running = 1;
	while (!terminate && !conn_api.terminate) {
		if (GetExitCodeProcess(pi.hProcess, &ec)) {
			if (ec != STILL_ACTIVE)
				break;
		}
		else {
			break;
		}
		if (!ReadFile(outputRead, conn_api.rd_buf, conn_api.rd_buf_size, &rd, NULL)) {
			break;
		}
		buffered = 0;
		while (!terminate && !conn_api.terminate && buffered < rd) {
			pthread_mutex_lock(&(conn_inbuf.mutex));
			buffer = conn_buf_wait_free(&conn_inbuf, rd - buffered, 100);
			buffered += conn_buf_put(&conn_inbuf, conn_api.rd_buf + buffered, buffer);
			pthread_mutex_unlock(&(conn_inbuf.mutex));
		}
	}
	terminate = true;
	conn_api.input_thread_running = 2;
}

static void
conpty_output_thread(void *args)
{
	int   wr;
	DWORD ret;
	int   sent;
	DWORD ec;

	SetThreadName("PTY Output");
	conn_api.output_thread_running = 1;
	while (!terminate && !conn_api.terminate) {
		if (GetExitCodeProcess(pi.hProcess, &ec)) {
			if (ec != STILL_ACTIVE)
				break;
		}
		else {
			break;
		}
		pthread_mutex_lock(&(conn_outbuf.mutex));
		ret = 0;
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent = 0;
			while (!terminate && !conn_api.terminate && sent < wr) {
				if (!WriteFile(inputWrite, conn_api.wr_buf + sent, wr - sent, &ret, NULL)) {
					terminate = true;
					break;
				}
				sent += ret;
			}
		}
		else {
			pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
	}
	conn_api.output_thread_running = 2;
}

int conpty_connect(struct bbslist *bbs)
{
	HANDLE heap = GetProcessHeap();

	int w, h;
	get_term_win_size(&w, &h, NULL, NULL, &bbs->nostatus);

	COORD size = {
		.X = w,
		.Y = h
	};
	STARTUPINFOEXA si = {
		.StartupInfo = {
			.cb = sizeof(STARTUPINFOEXA)
		}
	};
	size_t sz;
	// "Note  This initial call will return an error by design. This is expected behavior."
	!InitializeProcThreadAttributeList(NULL, 1, 0, &sz);
	si.lpAttributeList = HeapAlloc(heap, 0, sz);
	if (si.lpAttributeList == NULL) {
		uifcmsg("TODO", "HeapAlloc Failed");
		return -1;
	}

	char *cmd = bbs->addr;
	if (cmd[0] == 0)
		cmd = getenv("ComSpec");
	if (cmd == NULL)  {
		uifcmsg("TODO", "cmd Failed");
		return -1;
	}
	if (!CreatePipe(&inputRead, &inputWrite, NULL, 0)) {
		uifcmsg("TODO", "CreatePipe (input) Failed");
		return -1;
	}
	if (!CreatePipe(&outputRead, &outputWrite, NULL, 0)) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		uifcmsg("TODO", "CreatePipe (output) Failed");
		return -1;
	}
	if (FAILED(CreatePseudoConsole(size, inputRead, outputWrite, 0, &cpty))) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		uifcmsg("TODO", "CreatePseudoConsole Failed");
		return -1;
	}
	if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &sz)) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		uifcmsg("TODO", "InitializeProcThreadAttributeList2 Failed");
		return -1;
	}

	if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, cpty, sizeof(cpty), NULL, NULL)) {
		DeleteProcThreadAttributeList(si.lpAttributeList);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		uifcmsg("TODO", "UpdateProcThreadAttribute Failed");
		return -1;
	}

	if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi)) {
		DeleteProcThreadAttributeList(si.lpAttributeList);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		HeapFree(heap, 0, si.lpAttributeList);
		uifcmsg("TODO", "CreateProcessA Failed");
		return -1;
	}
	DeleteProcThreadAttributeList(si.lpAttributeList);
	HeapFree(heap, 0, si.lpAttributeList);
	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		uifcmsg("TODO", "create_conn_buf (input) Failed");
		return -1;
	}
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		uifcmsg("TODO", "create_conn_buf (output) Failed");
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		CloseHandle(inputRead);
		CloseHandle(inputWrite);
		CloseHandle(outputRead);
		CloseHandle(outputWrite);
		uifcmsg("TODO", "malloc (input) Failed");
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
		uifcmsg("TODO", "malloc (output) Failed");
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;

	terminate = false;
	_beginthread(conpty_output_thread, 0, NULL);
	_beginthread(conpty_input_thread, 0, NULL);

	return 0;
}

int
conpty_close(void)
{
	char garbage[1024];
	DWORD ret;

	conn_api.terminate = 1;
	terminate = true;
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
}

#endif
