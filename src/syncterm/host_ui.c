#include "host_ui.h"

#include "wren_host.h"
#include "wren_menu_host.h"

#include <stdio.h>

bool
host_ui_alert(const char *title, const char *message)
{
	if (wren_host_alert(title, message) ||
	    wren_menu_host_alert(title, message))
		return true;
	fprintf(stderr, "%s%s%s\n", title == NULL ? "SyncTERM" : title,
	    message == NULL || message[0] == 0 ? "" : ": ",
	    message == NULL ? "" : message);
	return false;
}

bool
host_ui_confirm(const char *title, const char *message)
{
	bool answer = false;
	if (!wren_menu_host_confirm(title, message, &answer)) {
		fprintf(stderr, "%s: %s\n", title == NULL ? "Confirm" : title,
		    message == NULL ? "" : message);
		return false;
	}
	return answer;
}

int
host_ui_prompt(const char *title, const char *message, char *value,
    size_t value_size, size_t max_len, bool masked)
{
	if (value == NULL || value_size == 0)
		return -1;
	int result = wren_menu_host_prompt(title, message, value, max_len,
	    masked, value, value_size);
	return result < -1 ? -1 : result;
}

int
host_ui_choice(const char *title, const char *const *options, size_t count,
    int current)
{
	return host_ui_choice_message(title, "", options, count, current);
}

int
host_ui_choice_message(const char *title, const char *message,
    const char *const *options, size_t count, int current)
{
	int result = wren_menu_host_choice(title, message, options, count,
	    current);
	return result < -1 ? -1 : result;
}

bool
host_ui_status(const char *message)
{
	if (message == NULL) {
		host_ui_status_clear();
		return true;
	}
	const char *lines[] = { message };
	return host_ui_status_lines("Status", lines, 1);
}

bool
host_ui_status_lines(const char *title, const char *const *lines,
    size_t count)
{
	return wren_menu_host_status(title, lines, count);
}

void
host_ui_status_clear(void)
{
	wren_menu_host_status_clear();
}
