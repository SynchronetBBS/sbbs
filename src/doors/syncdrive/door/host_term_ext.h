/* host_term_ext.h -- door-side host calls beyond tdport's host.h, used by
 * main() and by the patched tdport/src/game/flow_scores.c. */
#ifndef SYNCDRIVE_HOST_TERM_EXT_H_
#define SYNCDRIVE_HOST_TERM_EXT_H_

#include <stdbool.h>

const char *host_player_name(void);
void        host_set_player_name(const char *alias);
void        host_set_data_dir(const char *dir);
void        host_scores_lock(void);
void        host_scores_unlock(void);
void        host_set_held_keys_allowed(bool on);
void        host_set_keyscript(const char *s);

#endif
