/* scfgsrch.h - In-SCFG searchable index of menu options. */

#ifndef SCFGSRCH_H
#define SCFGSRCH_H

typedef struct {
	const char* label;   /* Menu option label, e.g. "Internal Code" */
	const char* menu;    /* Menu the option lives in, e.g. "Online Program" */
	const char* path;    /* Full menu path from "Configure" to the option */
} scfg_option_t;

/* Interactive: prompt for a substring, list every matching option with the
 * menu it lives in, let the sysop drill into one to see its source location.
 * Returns when the sysop ESCs the prompt or enters an empty query. */
void scfg_option_search(void);

#endif /* SCFGSRCH_H */
