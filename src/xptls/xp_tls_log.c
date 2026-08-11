#include "xp_tls_internal.h"

#include <stdio.h>
#include <string.h>

void
xp_tls_log_emit(const struct xp_tls_logger *logger,
	enum xp_tls_log_level level, enum xp_tls_log_source source,
	int error_code, unsigned long native_code, bool fatal,
	const char *message)
{
	if (logger == NULL || logger->callback == NULL ||
	    level > logger->level)
		return;
	if (message == NULL)
		message = "";
	const struct xp_tls_log_record record = {
		.level = level,
		.source = source,
		.backend = logger->backend,
		.error_code = error_code,
		.native_code = native_code,
		.fatal = fatal,
		.message = message,
		.message_len = strlen(message),
	};
	logger->callback(&record, logger->arg);
}

void
xp_tls_log_emitf(const struct xp_tls_logger *logger,
	enum xp_tls_log_level level, enum xp_tls_log_source source,
	int error_code, unsigned long native_code, bool fatal,
	const char *format, ...)
{
	if (logger == NULL || logger->callback == NULL ||
	    level > logger->level)
		return;
	char message[512];
	va_list ap;
	va_start(ap, format);
	vsnprintf(message, sizeof(message), format, ap);
	va_end(ap);
	xp_tls_log_emit(logger, level, source, error_code, native_code,
	    fatal, message);
}
