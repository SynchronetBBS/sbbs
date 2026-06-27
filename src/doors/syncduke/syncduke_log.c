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

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <signal.h>
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
static LONG WINAPI syncduke_crash_filter(EXCEPTION_POINTERS *ep)
{
	if (ep != NULL && ep->ExceptionRecord != NULL)
		syncduke_log("*** CRASH: exception 0x%08lx at %p ***",
		             (unsigned long)ep->ExceptionRecord->ExceptionCode,
		             ep->ExceptionRecord->ExceptionAddress);
	else
		syncduke_log("*** CRASH: unhandled exception ***");
	return EXCEPTION_EXECUTE_HANDLER;   /* let the process terminate */
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
#else
	signal(SIGSEGV, syncduke_crash_signal);
	signal(SIGABRT, syncduke_crash_signal);
	signal(SIGBUS,  syncduke_crash_signal);
	signal(SIGFPE,  syncduke_crash_signal);
#endif
}
