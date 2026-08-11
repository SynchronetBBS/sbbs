#ifndef SYNCTERM_SSH_LOG_H
#define SYNCTERM_SSH_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <deucessh.h>

#define SSH_LOG_MAX_SIZE (1024U * 1024U)

bool ssh_log_init(void);
void ssh_log_cleanup(void);
void ssh_log_reset(void);

dssh_log_level ssh_log_level_from_config(int level);

/*
 * Format and retain one DeuceSSH diagnostic, and mirror it to fp when
 * non-NULL.
 */
bool ssh_log_append(const struct dssh_log_record *record, FILE *fp);

/* Caller owns the returned snapshot/help string. */
char *ssh_log_snapshot(size_t *length);
char *ssh_log_build_help(const char *details);

#endif
