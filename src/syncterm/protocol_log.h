#ifndef SYNCTERM_PROTOCOL_LOG_H
#define SYNCTERM_PROTOCOL_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define PROTOCOL_LOG_MAX_SIZE (1024U * 1024U)

bool protocol_log_init(void);
void protocol_log_cleanup(void);
void protocol_log_reset(int level);
bool protocol_log_enabled(int level);

/* The supplied line must already be display-safe and include its newline. */
bool protocol_log_append(int level, const char *line, size_t length, FILE *fp);

/* Caller owns the returned snapshot/help string. */
char *protocol_log_snapshot(size_t *length);
char *protocol_log_build_help(const char *details);

#endif
