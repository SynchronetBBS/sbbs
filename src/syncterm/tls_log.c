#include "tls_log.h"

#include "gen_defs.h"
#include "protocol_log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern FILE *log_fp;

static enum xp_tls_log_level
tls_log_level_from_config(int level)
{
	if (level <= LOG_ERR)
		return XP_TLS_LOG_ERROR;
	if (level < LOG_DEBUG)
		return XP_TLS_LOG_WARNING;
	return XP_TLS_LOG_DEBUG;
}

static void
tls_diagnostic_cb(const struct xp_tls_log_record *record, void *arg)
{
	(void)arg;
	if (record == NULL)
		return;
	const char *level_name;
	int level;
	switch (record->level) {
		case XP_TLS_LOG_ERROR:
			level = LOG_ERR;
			level_name = "Error";
			break;
		case XP_TLS_LOG_WARNING:
			level = LOG_WARNING;
			level_name = "Warning";
			break;
		default:
			level = LOG_DEBUG;
			level_name = "Debug";
			break;
	}
	static const char *const sources[] = {
		"library", "backend", "peer-alert", "local-alert"
	};
	const char *source = (unsigned)record->source <
	    sizeof(sources) / sizeof(sources[0])
	    ? sources[record->source] : "unknown";
	if (record->message_len > (SIZE_MAX - 384) / 4)
		return;
	size_t capacity = record->message_len * 4 + 384;
	char *line = malloc(capacity);
	if (line == NULL)
		return;
	int written = snprintf(line, capacity, "TLS %s %s %s: ", level_name,
	    record->backend == NULL ? "xptls" : record->backend, source);
	if (written < 0 || (size_t)written >= capacity) {
		free(line);
		return;
	}
	size_t used = (size_t)written;
	const unsigned char *message = record->message;
	for (size_t i = 0; i < record->message_len; i++) {
		unsigned char ch = message == NULL ? 0 : message[i];
		if (ch >= 0x20 && ch <= 0x7e)
			line[used++] = (char)ch;
		else
			used += (size_t)snprintf(line + used, capacity - used,
			    "\\x%02X", ch);
	}
	if (record->error_code != 0)
		used += (size_t)snprintf(line + used, capacity - used,
		    " [error=%d]", record->error_code);
	if (record->native_code != 0)
		used += (size_t)snprintf(line + used, capacity - used,
		    " [native=%lu]", record->native_code);
	if (record->fatal)
		used += (size_t)snprintf(line + used, capacity - used, " [fatal]");
	line[used++] = '\n';
	line[used] = '\0';
	(void)protocol_log_append(level, line, used, log_fp);
	free(line);
}

void
syncterm_tls_log_configure(struct xp_tls_client_config *config, int level)
{
	if (config == NULL)
		return;
	config->log_cb = tls_diagnostic_cb;
	config->log_level = tls_log_level_from_config(level);
}
