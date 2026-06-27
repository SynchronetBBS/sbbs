/*
 * syncduke_log.c -- optional file-based debug log for the door.
 *
 * Independent of the BBS's stderr / syslog / OutputDebugString capture, so a
 * hangup, clean exit, or crash can be root-caused by reading a plain file --
 * exactly the cases where the door vanishes back to the BBS with nothing logged.
 *
 * Disabled unless a destination is configured (so it costs nothing by default):
 *   - env  SYNCDUKE_LOG=<path>            (highest precedence; handy for tests)
 *   - syncduke.ini  [debug] log = <path>  (via syncduke_log_set_path(), from config.c)
 * <path> may be a file, or a directory (trailing / or \) -> "<dir>/syncduke.log".
 * A relative path lands in the process CWD, which for a door is the per-user
 * -home dir, so each player gets their own log.
 *
 * Lines are stamped with milliseconds since the log opened and flushed per line,
 * so a crash/hangup still leaves the tail on disk. syncduke_log_init() also
 * installs a last-resort crash handler that records the fault (code + address)
 * to the same file before the process dies -- the only postmortem we get, since
 * WER minidumps are not configured here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <stdint.h>
#include <signal.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>   /* RtlCaptureStackBackTrace, GetModuleHandleEx -- no dbghelp
                          * (the vendored Game/src/DbgHelp.h shadows the SDK header) */
#else
  #include <unistd.h>
#endif

#include "syncduke.h"

static FILE              *g_log;
static int                g_tried;          /* attempted to open (success or not) */
static char               g_cfg_path[512];  /* path from syncduke.ini [debug] log */
static unsigned long long g_start_ms;

static unsigned long long sd_now_ms(void)
{
#ifdef _WIN32
	return (unsigned long long)GetTickCount();   /* 32-bit ms; fine for session-elapsed */
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (unsigned long long)t.tv_sec * 1000ULL + (unsigned long long)t.tv_nsec / 1000000ULL;
#endif
}

void syncduke_log_set_path(const char *path)
{
	if (path != NULL && *path != '\0' && !g_tried) {
		strncpy(g_cfg_path, path, sizeof(g_cfg_path) - 1);
		g_cfg_path[sizeof(g_cfg_path) - 1] = '\0';
	}
}

static void syncduke_log_open(void)
{
	const char *p;
	char        buf[600];
	size_t      n;

	g_tried = 1;
	p = getenv("SYNCDUKE_LOG");
	if ((p == NULL || *p == '\0') && g_cfg_path[0] != '\0')
		p = g_cfg_path;
	if (p == NULL || *p == '\0')
		return;                          /* logging disabled */

	n = strlen(p);
	if (n > 0 && (p[n - 1] == '/' || p[n - 1] == '\\')) {   /* a directory */
		snprintf(buf, sizeof buf, "%ssyncduke.log", p);
		p = buf;
	}
	g_log = fopen(p, "a");
	if (g_log != NULL) {
		time_t     t  = time(NULL);
		struct tm *lt = localtime(&t);
		char       ts[32];
		strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", lt);
		g_start_ms = sd_now_ms();
		fprintf(g_log, "\n===== SyncDuke log opened %s (pid %ld) =====\n",
		        ts, (long)
#ifdef _WIN32
		        GetCurrentProcessId()
#else
		        getpid()
#endif
		        );
		fflush(g_log);
	}
}

void syncduke_log(const char *fmt, ...)
{
	va_list ap;

	if (!g_tried)
		syncduke_log_open();
	if (g_log == NULL)
		return;
	fprintf(g_log, "[+%6llums] ", sd_now_ms() - g_start_ms);
	va_start(ap, fmt);
	vfprintf(g_log, fmt, ap);
	va_end(ap);
	fputc('\n', g_log);
	fflush(g_log);   /* per line: a crash/hangup still leaves the tail on disk */
}

/* All exits except _exit() (used by syncduke_hangup, which logs its own reason)
 * pass through here: the engine's Error()/exit() quit paths, the time-limit exit. */
static void syncduke_log_atexit(void)
{
	syncduke_log("process exit (atexit)");
}

#ifdef _WIN32
/* Log one address as module+RVA (offset from the module's load base), resolvable
 * offline against syncduke.map. No dbghelp -- the vendored Game/src/DbgHelp.h would
 * shadow the SDK header on this case-insensitive filesystem. */
static void sd_log_addr(void *addr)
{
	HMODULE hmod = NULL;
	char    mod[64] = "?";
	DWORD64 base = 0;

	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
	                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                       (LPCSTR)addr, &hmod) && hmod != NULL) {
		char path[MAX_PATH];
		base = (DWORD64)(uintptr_t)hmod;
		if (GetModuleFileNameA(hmod, path, sizeof path)) {
			char *b = strrchr(path, '\\');
			strncpy(mod, b ? b + 1 : path, sizeof mod - 1);
			mod[sizeof mod - 1] = '\0';
		}
	}
	syncduke_log("    %s+0x%llx", mod, (unsigned long long)((DWORD64)(uintptr_t)addr - base));
}

/* RtlCaptureStackBackTrace lives in ntdll; resolve it at runtime to avoid the import-
 * lib / calling-convention decoration mismatch (WIN32_LEAN_AND_MEAN omits its prototype). */
typedef USHORT (WINAPI *sd_capture_fn)(ULONG, ULONG, PVOID *, PULONG);

static void sd_log_backtrace(int skip)
{
	static sd_capture_fn capture;
	void  *frames[24];
	USHORT n = 0, i;

	if (capture == NULL) {
		HMODULE nt = GetModuleHandleA("ntdll.dll");
		if (nt != NULL)
			capture = (sd_capture_fn)GetProcAddress(nt, "RtlCaptureStackBackTrace");
	}
	if (capture != NULL)
		n = capture((ULONG)(skip + 1), 24, frames, NULL);
	syncduke_log("  backtrace (%u frames, module+RVA -> resolve via syncduke.map):", (unsigned)n);
	for (i = 0; i < n; i++)
		sd_log_addr(frames[i]);
}

static LONG WINAPI syncduke_crash_filter(EXCEPTION_POINTERS *ep)
{
	if (ep != NULL && ep->ExceptionRecord != NULL) {
		syncduke_log("*** CRASH: exception 0x%08lx at %p ***",
		             (unsigned long)ep->ExceptionRecord->ExceptionCode,
		             ep->ExceptionRecord->ExceptionAddress);
		sd_log_addr(ep->ExceptionRecord->ExceptionAddress);
	} else
		syncduke_log("*** CRASH: unhandled exception ***");
	sd_log_backtrace(0);
	return EXCEPTION_EXECUTE_HANDLER;   /* let the process terminate */
}

/* First-chance backstop: log a fatal exception even if something later replaced our
 * unhandled-exception filter, then let it propagate (CONTINUE_SEARCH). */
static LONG WINAPI syncduke_veh(EXCEPTION_POINTERS *ep)
{
	DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0;
	if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
	    code == EXCEPTION_PRIV_INSTRUCTION  || code == EXCEPTION_STACK_OVERFLOW ||
	    code == EXCEPTION_IN_PAGE_ERROR) {
		syncduke_log("*** VEH: fatal exception 0x%08lx at %p ***",
		             (unsigned long)code, ep->ExceptionRecord->ExceptionAddress);
		sd_log_addr(ep->ExceptionRecord->ExceptionAddress);
		sd_log_backtrace(0);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

/* CRT fast-fails (c0000409) bypass the unhandled-exception filter entirely: an invalid
 * argument to a CRT function (e.g. setvbuf, a bad fd), a pure-virtual call, or abort().
 * Log a backtrace; the invalid-parameter handler then RETURNS so the offending CRT call
 * fails gracefully instead of killing the door. */
static void sd_invalid_param(const wchar_t *e, const wchar_t *f, const wchar_t *file,
                             unsigned int line, uintptr_t res)
{
	(void)e; (void)f; (void)file; (void)line; (void)res;
	syncduke_log("*** CRT INVALID PARAMETER (fast-fail trapped) ***");
	sd_log_backtrace(1);
}

static void sd_purecall(void)
{
	syncduke_log("*** CRT PURE-VIRTUAL CALL ***");
	sd_log_backtrace(1);
}

static void sd_abort_signal(int sig)
{
	syncduke_log("*** abort/signal %d ***", sig);
	sd_log_backtrace(1);
}
#else
static void syncduke_crash_signal(int sig)
{
	syncduke_log("*** CRASH: signal %d ***", sig);
	signal(sig, SIG_DFL);
	raise(sig);                          /* re-raise for a core dump / default action */
}
#endif

void syncduke_log_init(void)
{
	atexit(syncduke_log_atexit);
#ifdef _WIN32
	SetUnhandledExceptionFilter(syncduke_crash_filter);
	AddVectoredExceptionHandler(0, syncduke_veh);   /* runs after others; backstop only */
	_set_invalid_parameter_handler(sd_invalid_param);
	_set_purecall_handler(sd_purecall);
	signal(SIGABRT, sd_abort_signal);
	signal(SIGFPE,  sd_abort_signal);
	signal(SIGILL,  sd_abort_signal);
#else
	signal(SIGSEGV, syncduke_crash_signal);
	signal(SIGABRT, syncduke_crash_signal);
	signal(SIGBUS,  syncduke_crash_signal);
	signal(SIGFPE,  syncduke_crash_signal);
#endif
}
