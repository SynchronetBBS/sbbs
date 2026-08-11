#include "ssh_log.h"

#include "gen_defs.h"
#include "protocol_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct text_builder {
	char   *data;
	size_t  len;
	size_t  cap;
};

static bool
builder_reserve(struct text_builder *b, size_t extra)
{
	if (extra > SIZE_MAX - b->len - 1)
		return false;
	size_t need = b->len + extra + 1;
	if (need <= b->cap)
		return true;
	size_t cap = b->cap == 0 ? 256 : b->cap;
	while (cap < need) {
		if (cap > SIZE_MAX / 2) {
			cap = need;
			break;
		}
		cap *= 2;
	}
	char *next = realloc(b->data, cap);
	if (next == NULL)
		return false;
	b->data = next;
	b->cap = cap;
	return true;
}

static bool
builder_bytes(struct text_builder *b, const char *s, size_t len)
{
	if (!builder_reserve(b, len))
		return false;
	memcpy(b->data + b->len, s, len);
	b->len += len;
	b->data[b->len] = '\0';
	return true;
}

static bool
builder_string(struct text_builder *b, const char *s)
{
	return builder_bytes(b, s, strlen(s));
}

static bool
builder_printf(struct text_builder *b, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	va_list copy;
	va_copy(copy, ap);
	int len = vsnprintf(NULL, 0, fmt, copy);
	va_end(copy);
	if (len < 0 || !builder_reserve(b, (size_t)len)) {
		va_end(ap);
		return false;
	}
	vsnprintf(b->data + b->len, b->cap - b->len, fmt, ap);
	va_end(ap);
	b->len += (size_t)len;
	return true;
}

static bool
builder_safe_bytes(struct text_builder *b, const uint8_t *s, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		unsigned char ch = s[i];
		if (ch >= 0x20 && ch <= 0x7e) {
			if (!builder_bytes(b, (const char *)&ch, 1))
				return false;
		}
		else if (!builder_printf(b, "\\x%02X", ch))
			return false;
	}
	return true;
}

static const char *
level_name(dssh_log_level level)
{
	switch (level) {
		case DSSH_LOG_ERROR: return "ERROR";
		case DSSH_LOG_WARNING: return "WARNING";
		case DSSH_LOG_DEBUG: return "DEBUG";
	}
	return "UNKNOWN";
}

static const char *
source_name(dssh_log_source source)
{
	switch (source) {
		case DSSH_LOG_SOURCE_LIBRARY: return "library";
		case DSSH_LOG_SOURCE_PEER_DEBUG: return "peer-debug";
		case DSSH_LOG_SOURCE_PEER_DISCONNECT: return "peer-disconnect";
	}
	return "unknown";
}

dssh_log_level
ssh_log_level_from_config(int level)
{
	if (level <= LOG_ERR)
		return DSSH_LOG_ERROR;
	if (level < LOG_DEBUG)
		return DSSH_LOG_WARNING;
	return DSSH_LOG_DEBUG;
}

bool
ssh_log_append(const struct dssh_log_record *record, FILE *fp)
{
	if (record == NULL)
		return false;
	struct text_builder b = {0};
	if (!builder_printf(&b, "SSH %s %s: ", level_name(record->level),
	    source_name(record->source)) ||
	    !builder_safe_bytes(&b, record->message, record->message_len))
		goto fail;
	if (record->error_code != DSSH_ERROR_NONE &&
	    !builder_printf(&b, " [error=%d: %s]", record->error_code,
	    dssh_strerror(record->error_code)))
		goto fail;
	if (record->ssh_reason_code != 0 &&
	    !builder_printf(&b, " [reason=%u]", record->ssh_reason_code))
		goto fail;
	if (record->language_len != 0 &&
	    (!builder_string(&b, " [language=") ||
	     !builder_safe_bytes(&b, record->language, record->language_len) ||
	     !builder_string(&b, "]")))
		goto fail;
	if (record->always_display && !builder_string(&b, " [always-display]"))
		goto fail;
	if (record->truncated && !builder_string(&b, " [record-truncated]"))
		goto fail;
	if (!builder_string(&b, "\n"))
		goto fail;
	int level = record->level == DSSH_LOG_ERROR ? LOG_ERR
	    : record->level == DSSH_LOG_WARNING ? LOG_WARNING : LOG_DEBUG;
	bool retained = protocol_log_append(level, b.data, b.len, fp);
	free(b.data);
	return retained;

fail:
	free(b.data);
	return false;
}
