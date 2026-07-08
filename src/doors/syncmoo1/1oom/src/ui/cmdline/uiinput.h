#ifndef INC_1OOM_UIINPUT_H
#define INC_1OOM_UIINPUT_H

#include "types.h"

struct input_list_s {
    int value;
    const char *key;
    const char *str;
    const char *display;
};

extern const struct input_list_s il_yes_no[];

struct input_list_dyn_s {
    const struct input_list_s *list;
    void *ctx;
    const char *(*get_display)(void *ctx, const struct input_list_s *l);
    bool (*is_ok)(void *ctx, const struct input_list_s *l);
};

struct input_cmd_s;

extern bool ui_input_match_input(const char *in, const char *key, const char *str);
extern void ui_input_wait_enter(void);
extern char *ui_input_line(const char *prompt);
extern char *ui_input_line_len_trim(const char *prompt, int maxlen);
extern int ui_input_list(const char *title, const char *prompt, const struct input_list_s *list);
extern int ui_input_list_dynamic(const char *title, const char *prompt, const struct input_list_dyn_s *listdyn);
extern int ui_input_tokenize(char *inputbuf, const struct input_cmd_s * const *cmdsptr);

#endif
