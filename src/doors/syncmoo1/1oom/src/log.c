#include "config.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"
#ifdef FEATURE_MODEBUG
#include "options.h"
#endif
#include "hw.h"

/* ------------------------------------------------------------------------- */

#define MAX_MSG_LEN (1024 * 2)

static char msgbuf[MAX_MSG_LEN] = "";

static FILE *log_fd = NULL;

/* ------------------------------------------------------------------------- */

static void log_file_write(const char *msg)
{
    if (fputs(msg, log_fd) < 0) {
        log_file_close();
        log_error("Log: writing failed!\n");
    } else if (fflush(log_fd) != 0) {
        log_file_close();
        log_error("Log: flush failed!\n");
    }
}

/* ------------------------------------------------------------------------- */

int log_file_open(const char *filename)
{
    if (filename && (filename[0] != '\0')) {
        log_fd = fopen(filename, "w");
        if (!log_fd) {
            log_error("Log: opening %s failed!\n", filename);
            return -1;
        }
    }
    return 0;
}

void log_file_close(void)
{
    if (log_fd) {
        fclose(log_fd);
        log_fd = NULL;
    }
}

void log_message_direct(const char *msg)
{
    if (log_fd) {
        log_file_write(msg);
    }
    hw_log_message(msg);
}

void log_warning_direct(const char *msg)
{
    if (log_fd) {
        log_file_write(msg);
    }
    hw_log_warning(msg);
}

void log_error_direct(const char *msg)
{
    if (log_fd) {
        log_file_write(msg);
    }
    hw_log_error(msg);
}

void log_message(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    vsnprintf(msgbuf, MAX_MSG_LEN, format, ap);
    va_end(ap);

    log_message_direct(msgbuf);
}

void log_warning(const char *format, ...)
{
    va_list ap;
    int len;

    len = snprintf(msgbuf, MAX_MSG_LEN, "warning: ");

    va_start(ap, format);
    vsnprintf(&msgbuf[len], MAX_MSG_LEN - len, format, ap);
    va_end(ap);

    log_warning_direct(msgbuf);
}

void log_error(const char *format, ...)
{
    va_list ap;
    int len;

    len = snprintf(msgbuf, MAX_MSG_LEN, "error: ");

    va_start(ap, format);
    vsnprintf(&msgbuf[len], MAX_MSG_LEN - len, format, ap);
    va_end(ap);

    log_error_direct(msgbuf);
}

void log_fatal_and_die(const char *format, ...)
{
    va_list ap;
    int len;

    len = snprintf(msgbuf, MAX_MSG_LEN, "FATAL: ");

    va_start(ap, format);
    vsnprintf(&msgbuf[len], MAX_MSG_LEN - len, format, ap);
    va_end(ap);

    log_error_direct(msgbuf);
    raise(SIGINT);
    exit(EXIT_FAILURE);
}

#ifdef FEATURE_MODEBUG
static char dbgmsgbuf[MAX_MSG_LEN] = "";

void log_debug(int level, const char *format, ...)
{
    va_list ap;

    if (opt_modebug < level) {
        return;
    }

    va_start(ap, format);
    vsnprintf(dbgmsgbuf, MAX_MSG_LEN, format, ap);
    va_end(ap);

    fflush(stdout);
    fputs(dbgmsgbuf, stderr);
    fflush(stderr);
}
#endif
