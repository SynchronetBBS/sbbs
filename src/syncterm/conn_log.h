#ifndef SYNCTERM_CONN_LOG_H
#define SYNCTERM_CONN_LOG_H

#include <stdbool.h>

void conn_logf(const char *source, int level, const char *format, ...);
void conn_log_alert(const char *title, const char *message);
bool conn_type_has_diagnostics(int conn_type);

#endif
