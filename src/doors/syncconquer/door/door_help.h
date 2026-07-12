/*
 * door_help.h -- Ctrl-K key-bindings help card. A door-side overlay (the
 * engine has no in-game key-list screen), toggled on/off and painted over the
 * frame by door_io_present() while active -- the same treatment as the Ctrl-S
 * stats strip, not the time-limited who's-online banner in door_node.c.
 */
#ifndef DOOR_HELP_H_
#define DOOR_HELP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Ctrl-K: flip the help card on/off. */
void door_help_toggle(void);

/* True while the card is showing (door_io.c bypasses its static-frame dedupe
 * and repaints the card on the deduped path, so it survives a still scene). */
int door_help_active(void);

/* Paint the card centered in a cols x rows text grid. No-ops when inactive. */
void door_help_draw(int cols, int rows);

#ifdef __cplusplus
}
#endif

#endif /* DOOR_HELP_H_ */
