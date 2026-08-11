#ifndef SYNCTERM_HOST_UI_H
#define SYNCTERM_HOST_UI_H

#include <stdbool.h>
#include <stddef.h>

bool host_ui_alert(const char *title, const char *message);
bool host_ui_alert_help(const char *title, const char *message,
                        const char *help_text);
bool host_ui_help(const char *title, const char *body);
bool host_ui_confirm(const char *title, const char *message);
int host_ui_prompt(const char *title, const char *message,
                   char *value, size_t value_size, size_t max_len,
                   bool masked);
int host_ui_choice_message(const char *title, const char *message,
                           const char *const *options, size_t count,
                           int current);
bool host_ui_status(const char *message);
bool host_ui_status_lines(const char *title, const char *const *lines,
                          size_t count);
void host_ui_status_clear(void);

#endif
