#include "protocol_log.h"

#include "gen_defs.h"
#include "threadwrap.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PROTOCOL_LOG_TRUNCATION_MARKER \
	"Protocol Warning local: [log truncated at 1 MiB]\n"

struct text_builder {
	char   *data;
	size_t  len;
	size_t  cap;
};

static pthread_mutex_t protocol_log_mutex;
static bool            protocol_log_initialized;
static char           *protocol_log_data;
static size_t          protocol_log_length;
static size_t          protocol_log_capacity;
static bool            protocol_log_truncated;
static int             protocol_log_threshold = LOG_ERR;

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

bool
protocol_log_init(void)
{
	if (protocol_log_initialized)
		return true;
	if (pthread_mutex_init(&protocol_log_mutex, NULL) != 0)
		return false;
	protocol_log_initialized = true;
	return true;
}

void
protocol_log_cleanup(void)
{
	if (!protocol_log_initialized)
		return;
	assert_pthread_mutex_lock(&protocol_log_mutex);
	free(protocol_log_data);
	protocol_log_data = NULL;
	protocol_log_length = 0;
	protocol_log_capacity = 0;
	protocol_log_truncated = false;
	assert_pthread_mutex_unlock(&protocol_log_mutex);
	pthread_mutex_destroy(&protocol_log_mutex);
	protocol_log_initialized = false;
}

void
protocol_log_reset(int level)
{
	if (!protocol_log_initialized && !protocol_log_init())
		return;
	assert_pthread_mutex_lock(&protocol_log_mutex);
	protocol_log_threshold = level;
	protocol_log_length = 0;
	protocol_log_truncated = false;
	if (protocol_log_data != NULL)
		protocol_log_data[0] = '\0';
	assert_pthread_mutex_unlock(&protocol_log_mutex);
}

bool
protocol_log_enabled(int level)
{
	if (!protocol_log_initialized)
		return false;
	assert_pthread_mutex_lock(&protocol_log_mutex);
	bool enabled = level <= protocol_log_threshold;
	assert_pthread_mutex_unlock(&protocol_log_mutex);
	return enabled;
}

static bool
append_locked(const char *record, size_t record_len)
{
	const size_t marker_len = sizeof(PROTOCOL_LOG_TRUNCATION_MARKER) - 1;
	if (protocol_log_truncated)
		return false;
	if (record_len > PROTOCOL_LOG_MAX_SIZE - protocol_log_length ||
	    PROTOCOL_LOG_MAX_SIZE - protocol_log_length - record_len < marker_len) {
		protocol_log_truncated = true;
		if (marker_len <= PROTOCOL_LOG_MAX_SIZE - protocol_log_length) {
			size_t need = protocol_log_length + marker_len + 1;
			if (need > protocol_log_capacity) {
				char *next = realloc(protocol_log_data, need);
				if (next == NULL)
					return false;
				protocol_log_data = next;
				protocol_log_capacity = need;
			}
			memcpy(protocol_log_data + protocol_log_length,
			    PROTOCOL_LOG_TRUNCATION_MARKER, marker_len + 1);
			protocol_log_length += marker_len;
		}
		return false;
	}
	size_t need = protocol_log_length + record_len + 1;
	if (need > protocol_log_capacity) {
		size_t cap = protocol_log_capacity == 0 ? 4096 : protocol_log_capacity;
		while (cap < need && cap < PROTOCOL_LOG_MAX_SIZE + 1)
			cap *= 2;
		if (cap > PROTOCOL_LOG_MAX_SIZE + 1)
			cap = PROTOCOL_LOG_MAX_SIZE + 1;
		char *next = realloc(protocol_log_data, cap);
		if (next == NULL)
			return false;
		protocol_log_data = next;
		protocol_log_capacity = cap;
	}
	memcpy(protocol_log_data + protocol_log_length, record, record_len);
	protocol_log_length += record_len;
	protocol_log_data[protocol_log_length] = '\0';
	return true;
}

bool
protocol_log_append(int level, const char *line, size_t length, FILE *fp)
{
	if (line == NULL || (!protocol_log_initialized && !protocol_log_init()))
		return false;
	assert_pthread_mutex_lock(&protocol_log_mutex);
	if (level > protocol_log_threshold) {
		assert_pthread_mutex_unlock(&protocol_log_mutex);
		return false;
	}
	bool retained = append_locked(line, length);
	if (fp != NULL)
		(void)fwrite(line, 1, length, fp);
	assert_pthread_mutex_unlock(&protocol_log_mutex);
	return retained;
}

char *
protocol_log_snapshot(size_t *length)
{
	if (length != NULL)
		*length = 0;
	if (!protocol_log_initialized)
		return NULL;
	assert_pthread_mutex_lock(&protocol_log_mutex);
	char *ret = NULL;
	if (protocol_log_length != 0) {
		ret = malloc(protocol_log_length + 1);
		if (ret != NULL) {
			memcpy(ret, protocol_log_data, protocol_log_length + 1);
			if (length != NULL)
				*length = protocol_log_length;
		}
	}
	assert_pthread_mutex_unlock(&protocol_log_mutex);
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
protocol_log_build_help(const char *details)
{
	char *snapshot = protocol_log_snapshot(NULL);
	struct text_builder b = {0};
	if (details != NULL && details[0] != '\0') {
		if (!builder_string(&b, "# Connection Details\n\n") ||
		    !builder_markdown_text(&b, details, true))
			goto fail;
	}
	if (snapshot != NULL) {
		if (b.len != 0 && !builder_string(&b, "\n\n"))
			goto fail;
		if (!builder_string(&b, "# Protocol Session Log\n\n"))
			goto fail;
		char *line = snapshot;
		while (*line != '\0') {
			char *end = strchr(line, '\n');
			size_t len = end == NULL ? strlen(line) : (size_t)(end - line);
			if (!builder_string(&b, "- "))
				goto fail;
			char saved = line[len];
			line[len] = '\0';
			bool ok = builder_markdown_text(&b, line, false);
			line[len] = saved;
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
