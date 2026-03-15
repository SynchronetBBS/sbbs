/*
 * scripteng.h -- Shared script engine for qtest/gtest
 *
 * Provides line-based script consumption and hook callbacks for
 * driving game code via scripted input files.
 */
#ifndef THE_CLANS__SCRIPTENG___H
#define THE_CLANS__SCRIPTENG___H

#include <stdbool.h>
#include <stdint.h>

/* Script file management */
bool script_open(const char *path);
void script_close(void);
bool script_is_active(void);
int  script_line_number(void);
void script_set_tool_name(const char *name);

/* Script consumption */
bool        script_read_line(char *buf, size_t sz);
const char *script_consume(const char *expected_type);
void        script_expect_end(void);

/* Hook callbacks for Console/Video/Input hook setters */
char    hook_get_answer(const char *szAllowableChars);
char    hook_get_key(void);
void    hook_dos_get_str(char *InputStr, int16_t MaxChars, bool HiBit);
long    hook_dos_get_long(const char *Prompt, long DefaultVal, long Maximum);
int16_t hook_get_string_choice(const char **apszChoices,
                               int16_t NumChoices, bool AllowBlank);

/* Install all hooks at once (convenience) */
void script_install_hooks(void);

#endif
