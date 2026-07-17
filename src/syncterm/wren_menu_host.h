#ifndef WREN_MENU_HOST_H
#define WREN_MENU_HOST_H

#include <stdbool.h>
#include <stddef.h>

struct bbslist;

/* Persistent trusted menu VM.  It is initialized once after conio and the
 * settings model are ready, parked while a connection is active, and freed
 * during application shutdown. */
bool wren_menu_host_init(void);
void wren_menu_host_shutdown(void);
bool wren_menu_host_active(void);

/* Run the persistent menu controller.  In selection mode, a successful BBS
 * choice is returned as a host-owned copy with the same lifetime contract as
 * show_bbslist(); connected mode is edit-only and always returns NULL. */
struct bbslist *wren_menu_host_run(const char *current, bool connected);

/* C-owned UI rendered by the trusted menu VM.  Blocking calls establish
 * trusted input boundaries and restore the prior screen before returning.
 * prompt/choice return -2 when the bridge fails and -1 when cancelled. */
bool wren_menu_host_alert(const char *title, const char *message);
bool wren_menu_host_confirm(const char *title, const char *message,
                            bool *answer);
int wren_menu_host_prompt(const char *title, const char *message,
                          const char *initial, size_t max_len, bool masked,
                          char *result, size_t result_size);
int wren_menu_host_choice(const char *title, const char *message,
                          const char *const *options, size_t count,
                          int current);

/* Nonblocking progress owns a saved screen until clear.  It never clears
 * input, allowing connection setup to keep observing abort keystrokes. */
bool wren_menu_host_status(const char *title,
                           const char *const *lines, size_t count);
void wren_menu_host_status_clear(void);

#endif
