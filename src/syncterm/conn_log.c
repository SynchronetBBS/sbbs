#include "conn_log.h"

#include "conn.h"
#include "gen_defs.h"
#include "host_ui.h"
#include "protocol_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern FILE *log_fp;

void
conn_logf(const char *source, int level, const char *format, ...)
{
	if (source == NULL || !protocol_log_enabled(level))
		return;
	char message[768];
	va_list ap;
	va_start(ap, format);
	int message_len = vsnprintf(message, sizeof(message), format, ap);
	va_end(ap);
	if (message_len < 0)
		return;
	if ((size_t)message_len >= sizeof(message))
		message_len = sizeof(message) - 1;
	const char *level_name = level <= LOG_ERR ? "Error"
	    : level <= LOG_WARNING ? "Warning"
	    : level <= LOG_INFO ? "Info" : "Debug";
	char line[sizeof(message) * 4 + 64];
	int prefix_len = snprintf(line, sizeof(line), "%s %s: ", source,
	    level_name);
	if (prefix_len < 0 || (size_t)prefix_len >= sizeof(line))
		return;
	size_t used = (size_t)prefix_len;
	for (int i = 0; i < message_len; i++) {
		unsigned char ch = (unsigned char)message[i];
		if (ch >= 0x20 && ch <= 0x7e)
			line[used++] = (char)ch;
		else
			used += (size_t)snprintf(line + used, sizeof(line) - used,
			    "\\x%02X", ch);
	}
	line[used++] = '\n';
	(void)protocol_log_append(level, line, used, log_fp);
}

void
conn_log_alert(const char *title, const char *message)
{
	char *help = protocol_log_build_help(message);
	if (help != NULL) {
		host_ui_alert_help(title, message, help);
		free(help);
	}
	else
		host_ui_alert(title, message);
}

bool
conn_type_has_diagnostics(int conn_type)
{
	return conn_type > CONN_TYPE_UNKNOWN && conn_type < CONN_TYPE_TERMINATOR;
}
