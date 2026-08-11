#ifndef SYNCTERM_SSH_LOG_H
#define SYNCTERM_SSH_LOG_H

#include <stdbool.h>
#include <stdio.h>

#include <deucessh.h>

dssh_log_level ssh_log_level_from_config(int level);
bool ssh_log_append(const struct dssh_log_record *record, FILE *fp);

#endif
