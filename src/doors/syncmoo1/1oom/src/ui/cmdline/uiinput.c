#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "uiinput.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uidefs.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_INPUT 4

/* -------------------------------------------------------------------------- */

const struct input_list_s il_yes_no[] = {
    { 1, "Y", NULL, "Yes" },
    { 0, "N", NULL, "No" },
    { 0, NULL, NULL, NULL }
};

/* -------------------------------------------------------------------------- */

static char *skip_whitespace(char *str)
{
    char c = *str;
    while ((c == ' ') || (c == '\t')) {
        c = *++str;
    }
    return str;
}

static char *skip_and_end_token(char *str)
{
    char c = *str;
    if (c == '"') {
        c = *++str;
        while ((c != '"') && (c != '\n') && (c != '\r') && (c != '\0')) {
            if (c == '\\') {
                c = *++str;
            }
            c = *++str;
        }
        if (c != '"') {
            log_error("Input: missing terminating '\"'\n");
            return NULL;
        }
    } else {
        while ((c != ' ') && (c != '\t') && (c != '\n') && (c != '\r') && (c != '\0')) {
            c = *++str;
        }
    }
    *str = '\0';
    if (c != '\0') {
        ++str;
    }
    return str;
}

/* -------------------------------------------------------------------------- */

bool ui_input_match_input(const char *in, const char *key, const char *str)
{
    int len = strlen(in);
    if (len == 0) {
        return false;
    }
    if (key && (in[1] == 0) && (tolower(in[0]) == tolower(key[0]))) {
        return true;
    }
    if (str) {
        int i;
        for (i = 0; (i < len) && str[i]; ++i) {
            if (str[i] != tolower(in[i])) {
                return false;
            }
        }
        if (str[i] == 0) {
            return true;
        }
    }
    return false;
}

void ui_input_wait_enter(void)
{
    ui_input_line("(press enter)");
}

#ifdef HAVE_READLINE
char *ui_input_line(const char *prompt)
{
    static char *line = NULL;
    lib_free(line);
    line = readline(prompt);
    if ((line != 0) && (*line != 0)) {
        add_history(line);
    } else if (line == NULL) {
        log_message("(end of input)\n");
        exit(0);
    }
    return line;
}
#else
char *ui_input_line(const char *prompt)
{
    static char line[1024];
    line[sizeof(line) - 1] = 0;
    fputs(prompt, stdout);
    fflush(stdout);
    if (fgets(line, sizeof(line) - 1, stdin)) {
        char *p = line;
        char c;
        while (((c = *p) != 0) && (c != '\n') && (c != '\r')) {
            ++p;
        }
        *p = 0;
        return line;
    } else {
        log_message("(end of input)\n");
        exit(0);
    }
}
#endif

char *ui_input_line_len_trim(const char *prompt, int maxlen)
{
    char *line;
    int len;
    line = ui_input_line(prompt);
    util_trim_whitespace(line, 1024);
    len = strlen(line);
    if (len > maxlen) {
        line[maxlen - 1] = 0;
    }
    return line;
}

int ui_input_list(const char *title, const char *prompt, const struct input_list_s *list)
{
    const struct input_list_dyn_s listdyn = { list, 0, 0, 0 };
    return ui_input_list_dynamic(title, prompt, &listdyn);
}

int ui_input_list_dynamic(const char *title, const char *prompt, const struct input_list_dyn_s *ld)
{
    const struct input_list_s *l;
    char *in;
    int num_shortcuts = 0;
    char **shortcuts = NULL;
    const struct input_list_s *list = ld->list;

    l = list;
    while (ld->get_display ? ld->get_display(ld->ctx, l) : l->display) {
        ++num_shortcuts;
        ++l;
    }
    shortcuts = lib_malloc(num_shortcuts * sizeof(char *));
    l = list;
    for (int i = 0; i < num_shortcuts; ++i, ++l) {
        const char *s;
        char *p;
        char c;
        s = ld->get_display ? ld->get_display(ld->ctx, l) : l->display;
        if (*s == '(') { ++s; }
        p = shortcuts[i] = lib_stralloc(s);
        while ((c = *p) != '\0') {
            *p++ = tolower(c);
        }
    }
    for (int i = 0; i < num_shortcuts; ++i) {
        int len;
        bool is_unique;
        is_unique = false;
        len = 0;
        while (!is_unique && shortcuts[i][++len]) {
            is_unique = true;
            for (int j = 0; j < num_shortcuts; ++j) {
                if ((i != j) && (strncmp(shortcuts[i], shortcuts[j], len) == 0)) {
                    is_unique = false;
                    break;
                }
            }
            if (is_unique && (len == 1)) {
                char c;
                c = toupper(shortcuts[i][0]);
                for (int j = 0; j < num_shortcuts; ++j) {
                    if ((i != j) && list[j].key && (list[j].key[0] == c)) {
                        is_unique = false;
                        break;
                    }
                }
            }
        }
        shortcuts[i][len] = '\0';
    }

    while (1) {
        int i;
        putchar('\n');
        if (title) {
            puts(title);
        }
        l = list;
        while ((l->str) || (l->key)) {
            const char *s;
            bool is_ok;
            if (ld->get_display) {
                s = ld->get_display(ld->ctx, l);
                is_ok = ld->is_ok(ld->ctx, l);
            } else {
                s = l->display;
                is_ok = true;
            }
            if (s) {
                printf("  %s) ", l->key);
                if (!is_ok) {
                    putchar('(');
                }
                fputs(s, stdout);
                if (!is_ok) {
                    putchar(')');
                }
                putchar('\n');
            }
            ++l;
        }
        in = ui_input_line(prompt);
        l = list;
        i = 0;
        while ((l->str) || (l->key)) {
            if (ui_input_match_input(in, l->key, l->str ? l->str : shortcuts[i])) {
                if (ld->is_ok && (!ld->is_ok(ld->ctx, l))) {
                    break;
                }
                for (int j = 0; j < num_shortcuts; ++j) {
                    lib_free(shortcuts[j]);
                }
                lib_free(shortcuts);
                return l->value;
            }
            ++l;
            ++i;
        }
        puts("???");
    }
}

int ui_input_tokenize(char *inputbuf, const struct input_cmd_s * const *cmdsptr)
{
    int num = 0;
    ui_data.input.num = 0;
    {
        char *p = inputbuf;
        p = skip_whitespace(p);
        if (*p == '#') {
            return 0;
        }
        while ((*p != '\0') && (*p != '\n') && (*p != '\r')) {
            ui_data.input.tok[num].str = p;
            ui_data.input.tok[num].data.ptr = 0;
            ui_data.input.tok[num].type = INPUT_TOKEN_UNKNOWN;
            p = skip_and_end_token(p);
            if ((p != NULL) && (num < UI_INPUT_TOKEN_MAX)) {
                ++num;
            } else {
                return -1;
            }
            p = skip_whitespace(p);
        }
    }
    for (int i = 0; i < num; ++i) {
        const char *p;
        p = ui_data.input.tok[i].str;
        if (p[0] != '"') {
            int v;
            if (util_parse_signed_number(p, &v)) {
                ui_data.input.tok[i].type = ((p[0] == '+') || (p[0] == '-')) ? INPUT_TOKEN_RELNUMBER : INPUT_TOKEN_NUMBER;
                ui_data.input.tok[i].data.num = v;
            }
        } else {
            ui_data.input.tok[i].str = p + 1;
        }
        LOG_DEBUG((DEBUGLEVEL_INPUT, "%s: tok %i t %i '%s' %i\n", __func__, i, ui_data.input.tok[i].type, ui_data.input.tok[i].str, ui_data.input.tok[i].data.num));
    }
    if ((num > 0) && cmdsptr && (ui_data.input.tok[0].type == INPUT_TOKEN_UNKNOWN)) {
        const struct input_cmd_s *cmds;
        const char *p;
        p = ui_data.input.tok[0].str;
        while ((cmds = *cmdsptr++) != NULL) {
            while (cmds->str_cmd) {
                if (cmds->handle && (strcmp(cmds->str_cmd, p) == 0)) {
                    if (((num - 1) < cmds->num_param_min) || ((num - 1) > cmds->num_param_max)) {
                        log_error("Input: wrong number of parameters %i for '%s' (min %i, max %i)\n", num - 1, cmds->str_cmd, cmds->num_param_min, cmds->num_param_max);
                        num = 0;
                    }
                    ui_data.input.tok[0].type = INPUT_TOKEN_COMMAND;
                    ui_data.input.tok[0].data.cmd = cmds;
                    goto done;
                }
                ++cmds;
            }
        }
    }
done:
    ui_data.input.num = num;
    return 0;
}
