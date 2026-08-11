#include "ssh_log.h"

#include "gen_defs.h"
#include "threadwrap.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SSH_LOG_TRUNCATION_MARKER "SSH WARNING local: [log truncated at 1 MiB]\n"

struct text_builder {
	char   *data;
	size_t  len;
	size_t  cap;
};

static pthread_mutex_t ssh_log_mutex;
static bool            ssh_log_initialized;
static char           *ssh_log_data;
static size_t          ssh_log_length;
static size_t          ssh_log_capacity;
static bool            ssh_log_truncated;

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

/* Make length-delimited, possibly peer-controlled bytes safe for logs. */
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
		case DSSH_LOG_ERROR:
			return "ERROR";
		case DSSH_LOG_WARNING:
			return "WARNING";
		case DSSH_LOG_DEBUG:
			return "DEBUG";
	}
	return "UNKNOWN";
}

static const char *
source_name(dssh_log_source source)
{
	switch (source) {
		case DSSH_LOG_SOURCE_LIBRARY:
			return "library";
		case DSSH_LOG_SOURCE_PEER_DEBUG:
			return "peer-debug";
		case DSSH_LOG_SOURCE_PEER_DISCONNECT:
			return "peer-disconnect";
	}
	return "unknown";
}

static char *
format_record(const struct dssh_log_record *record, size_t *length)
{
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
	if (record->language_len != 0) {
		if (!builder_string(&b, " [language=") ||
		    !builder_safe_bytes(&b, record->language, record->language_len) ||
		    !builder_string(&b, "]"))
			goto fail;
	}
	if (record->always_display && !builder_string(&b, " [always-display]"))
		goto fail;
	if (record->truncated && !builder_string(&b, " [record-truncated]"))
		goto fail;
	if (!builder_string(&b, "\n"))
		goto fail;
	if (length != NULL)
		*length = b.len;
	return b.data;

fail:
	free(b.data);
	return NULL;
}

bool
ssh_log_init(void)
{
	if (ssh_log_initialized)
		return true;
	if (pthread_mutex_init(&ssh_log_mutex, NULL) != 0)
		return false;
	ssh_log_initialized = true;
	return true;
}

void
ssh_log_cleanup(void)
{
	if (!ssh_log_initialized)
		return;
	assert_pthread_mutex_lock(&ssh_log_mutex);
	free(ssh_log_data);
	ssh_log_data = NULL;
	ssh_log_length = 0;
	ssh_log_capacity = 0;
	ssh_log_truncated = false;
	assert_pthread_mutex_unlock(&ssh_log_mutex);
	pthread_mutex_destroy(&ssh_log_mutex);
	ssh_log_initialized = false;
}

void
ssh_log_reset(void)
{
	if (!ssh_log_initialized && !ssh_log_init())
		return;
	assert_pthread_mutex_lock(&ssh_log_mutex);
	ssh_log_length = 0;
	ssh_log_truncated = false;
	if (ssh_log_data != NULL)
		ssh_log_data[0] = '\0';
	assert_pthread_mutex_unlock(&ssh_log_mutex);
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

static bool
append_locked(const char *record, size_t record_len)
{
	const size_t marker_len = sizeof(SSH_LOG_TRUNCATION_MARKER) - 1;
	if (ssh_log_truncated)
		return false;
	if (record_len > SSH_LOG_MAX_SIZE - ssh_log_length ||
	    SSH_LOG_MAX_SIZE - ssh_log_length - record_len < marker_len) {
		ssh_log_truncated = true;
		if (marker_len <= SSH_LOG_MAX_SIZE - ssh_log_length) {
			size_t need = ssh_log_length + marker_len + 1;
			if (need > ssh_log_capacity) {
				char *next = realloc(ssh_log_data, need);
				if (next == NULL)
					return false;
				ssh_log_data = next;
				ssh_log_capacity = need;
			}
			memcpy(ssh_log_data + ssh_log_length,
			    SSH_LOG_TRUNCATION_MARKER, marker_len + 1);
			ssh_log_length += marker_len;
		}
		return false;
	}
	size_t need = ssh_log_length + record_len + 1;
	if (need > ssh_log_capacity) {
		size_t cap = ssh_log_capacity == 0 ? 4096 : ssh_log_capacity;
		while (cap < need && cap < SSH_LOG_MAX_SIZE + 1)
			cap *= 2;
		if (cap > SSH_LOG_MAX_SIZE + 1)
			cap = SSH_LOG_MAX_SIZE + 1;
		char *next = realloc(ssh_log_data, cap);
		if (next == NULL)
			return false;
		ssh_log_data = next;
		ssh_log_capacity = cap;
	}
	memcpy(ssh_log_data + ssh_log_length, record, record_len);
	ssh_log_length += record_len;
	ssh_log_data[ssh_log_length] = '\0';
	return true;
}

bool
ssh_log_append(const struct dssh_log_record *record, FILE *fp)
{
	if (record == NULL || (!ssh_log_initialized && !ssh_log_init()))
		return false;
	size_t record_len = 0;
	char *line = format_record(record, &record_len);
	if (line == NULL)
		return false;
	assert_pthread_mutex_lock(&ssh_log_mutex);
	bool retained = append_locked(line, record_len);
	if (fp != NULL)
		(void)fwrite(line, 1, record_len, fp);
	assert_pthread_mutex_unlock(&ssh_log_mutex);
	free(line);
	return retained;
}

char *
ssh_log_snapshot(size_t *length)
{
	if (length != NULL)
		*length = 0;
	if (!ssh_log_initialized)
		return NULL;
	assert_pthread_mutex_lock(&ssh_log_mutex);
	char *ret = NULL;
	if (ssh_log_length != 0) {
		ret = malloc(ssh_log_length + 1);
		if (ret != NULL) {
			memcpy(ret, ssh_log_data, ssh_log_length + 1);
			if (length != NULL)
				*length = ssh_log_length;
		}
	}
	assert_pthread_mutex_unlock(&ssh_log_mutex);
	return ret;
}

static bool
builder_markdown_text(struct text_builder *b, const char *text, bool hard_breaks)
{
	if (text == NULL)
		return true;
	for (const unsigned char *p = (const unsigned char *)text; *p != 0; p++) {
		if (*p == '\r')
			continue;
		if (*p == '\n') {
			if (!builder_string(b, hard_breaks ? "  \n" : "\n"))
				return false;
			continue;
		}
		if ((*p == '\\' || *p == '*' || *p == '`') &&
		    !builder_string(b, "\\"))
			return false;
		if (!builder_bytes(b, (const char *)p, 1))
			return false;
	}
	return true;
}

char *
ssh_log_build_help(const char *details)
{
	size_t log_len = 0;
	char *snapshot = ssh_log_snapshot(&log_len);
	struct text_builder b = {0};
	if (details != NULL && details[0] != '\0') {
		if (!builder_string(&b, "# Connection Details\n\n") ||
		    !builder_markdown_text(&b, details, true))
			goto fail;
	}
	if (snapshot != NULL) {
		if (b.len != 0 && !builder_string(&b, "\n\n"))
			goto fail;
		if (!builder_string(&b, "# SSH Session Log\n\n"))
			goto fail;
		const char *line = snapshot;
		while (*line != '\0') {
			const char *end = strchr(line, '\n');
			size_t len = end == NULL ? strlen(line) : (size_t)(end - line);
			if (!builder_string(&b, "- "))
				goto fail;
			char saved = line[len];
			((char *)line)[len] = '\0';
			bool ok = builder_markdown_text(&b, line, false);
			((char *)line)[len] = saved;
			if (!ok || !builder_string(&b, "\n"))
				goto fail;
			if (end == NULL)
				break;
			line = end + 1;
		}
	}
	free(snapshot);
	if (b.len == 0) {
		free(b.data);
		return NULL;
	}
	return b.data;

fail:
	free(snapshot);
	free(b.data);
	return NULL;
}
