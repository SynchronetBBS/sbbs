#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "gameapi.h"
#include "hw.h"
#include "lib.h"
#include "log.h"
#include "main.h"
#include "options.h"
#include "os.h"
#include "types.h"
#include "ui.h"
#include "util.h"
#include "util_cstr.h"

/* -------------------------------------------------------------------------- */

static const struct cfg_module_s {
    const char *str;
    const struct cfg_items_s *items;
    bool const * const cond;
} cfg_items_tbl[] = {
    { "opt", opt_cfg_items, 0 },
    { "opta", opt_cfg_items_audio, &ui_use_audio },
    { "game", game_cfg_items, 0 },
    { "hw", hw_cfg_items, 0 },
    { "hwx", hw_cfg_items_extra, 0 },
    { "ui", ui_cfg_items, 0 },
    { 0, 0, 0 }
};

struct cfg_parse_s {
    const struct cfg_module_s *module;
    const struct cfg_items_s *item;
};

/* -------------------------------------------------------------------------- */

static void cfg_parse_init(struct cfg_parse_s *ctx)
{
    ctx->module = 0;
    ctx->item = 0;
}

static int cfg_parse_line(char *line, int lnum, struct cfg_parse_s *ctx)
{
    const struct cfg_module_s *module;
    const struct cfg_items_s *item;
    char *p, *iname, *ival;
    p = strchr(line, '.');
    if (!p) {
        log_error("Cfg: . missing on line %i\n", lnum);
        return -1;
    }
    *p = '\0';
    iname = ++p;
    p = strstr(iname, " =");
    if (!p) {
        log_error("Cfg: = missing on line %i\n", lnum);
        return -1;
    }
    if (p < (iname + 2)) {
        log_error("Cfg: item name missing on line %i\n", lnum);
        return -1;
    }
    *p = '\0';
    p += 2;
    ival = *p ? (p + 1) : p;
    if (1
      && ctx->module && ctx->module->str
      && ctx->item && ctx->item->name
      && (strcmp(ctx->module->str, line) == 0)
      && (strcmp(ctx->item->name, iname) == 0)
    ) {
        module = ctx->module;
        item = ctx->item;
    } else {
        module = 0;
        for (const struct cfg_module_s *m = cfg_items_tbl; m->str; ++m) {
            if ((strcmp(m->str, line) == 0) && ((!m->cond) || *m->cond)) {
                module = m;
                break;
            }
        }
        if (!module) {
            log_error("Cfg: unknown module on line %i\n", lnum);
            return -1;
        }
again:
        item = 0;
        for (const struct cfg_items_s *i = module->items; i->name; ++i) {
            if (strcmp(i->name, iname) == 0) {
                item = i;
                break;
            }
        }
        if (!item) {
            if (module->items == opt_cfg_items) { /* HACK handle audio option opt -> opta move */
                ++module;
                goto again;
            }
            log_warning("Cfg: ignoring unknown item '%s.%s' on line %i\n", line, iname, lnum);
            return 0;
        }
    }
    ctx->module = module;
    {
        const struct cfg_items_s *i;
        for (i = item + 1; i->name && (i->type == CFG_TYPE_COMMENT); ++i);
        if (!i->name) {
            ++ctx->module;
            i = ctx->module->items;
        }
        ctx->item = i;
    }
    switch (item->type) {
        case CFG_TYPE_INT:
            {
                uint32_t v;
                if (!util_parse_number(ival, &v)) {
                    log_error("Cfg: invalid value on line %i\n", lnum);
                    return -1;
                }
                if (item->check && (!item->check((void *)(intptr_t)(int)v))) {
                    log_warning("Cfg: item failed check on line %i\n", lnum);
                    return -1;
                }
                *((int *)item->var) = (int)v;
            }
            break;
        case CFG_TYPE_STR:
            if (ival[0] == '\0') {
                lib_free(*((char **)item->var));
                *((char **)item->var) = 0;
            } else if (ival[0] == '"') {
                ++ival;
                if (util_cstr_parse_in_place(ival) < 0) {
                    log_warning("Cfg: invalid value on line %i\n", lnum);
                    return -1;
                }
                if (item->check && (!item->check(ival))) {
                    log_warning("Cfg: item failed check on line %i\n", lnum);
                    return -1;
                }
                lib_free(*((char **)item->var));
                *((char **)item->var) = lib_stralloc(ival);
            } else {
                log_error("Cfg: invalid value on line %i\n", lnum);
                return -1;
            }
            break;
        case CFG_TYPE_BOOL:
            if (strcmp(ival, "true") == 0) {
                *((bool *)item->var) = true;
            } else if (strcmp(ival, "false") == 0) {
                *((bool *)item->var) = false;
            } else {
                log_error("Cfg: invalid value on line %i\n", lnum);
                return -1;
            }
            break;
        case CFG_TYPE_COMMENT:
            /* should never happen */
            break;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

char *cfg_cfgname(void)
{
    const char *path = os_get_path_user();
    char namebuf[128];
    char *s;
    if (!os_get_fname_cfg(namebuf, sizeof(namebuf), idstr_main, idstr_ui, idstr_hw)) {
        lib_sprintf(namebuf, sizeof(namebuf), "1oom_config_%s_%s_%s.txt", idstr_main, idstr_ui, idstr_hw);
    }
    s = util_concat(path, FSDEV_DIR_SEP_STR, namebuf, NULL);
    return s;
}

int cfg_load(const char *filename)
{
#define BUFSIZE (FSDEV_PATH_MAX + 64)
    FILE *fd;
    char buf[BUFSIZE];
    int len, lnum = 0;
    struct cfg_parse_s ctx[1];
    log_message("Cfg: loading configuration from '%s'\n", filename);
    fd = fopen(filename, "r");
    if (!fd) {
        log_error("Cfg: failed to open file '%s'\n", filename);
        return -1;
    }
    cfg_parse_init(ctx);
    while ((len = util_get_line(buf, BUFSIZE, fd)) >= 0) {
        ++lnum;
        if ((len == 0) || (buf[0] == '#')) {
            continue;
        }
        if (cfg_parse_line(buf, lnum, ctx) < 0) {
            fclose(fd);
            return -1;
        }
    }
    fclose(fd);
    return 0;
#undef BUFSIZE
}

static int cfg_clear(const char *filename)
{
    FILE *fd;
    if (os_make_path_for(filename)) {
        log_error("Cfg: failed to create path for '%s'\n", filename);
        return -1;
    }
    fd = fopen(filename, "w");
    if (!fd) {
        log_error("Cfg: failed to create file '%s'\n", filename);
        return -1;
    }
    if (fd) {
        fclose(fd);
    }
    return 0;
}

int cfg_save(const char *filename)
{
    if (opt_cfg_reset) {
        return cfg_clear(filename);
    }
    FILE *fd;
    log_message("Cfg: saving configuration to '%s'\n", filename);
    if (os_make_path_for(filename)) {
        log_error("Cfg: failed to create path for '%s'\n", filename);
        return -1;
    }
    fd = fopen(filename, "w+");
    if (!fd) {
        log_error("Cfg: failed to create file '%s'\n", filename);
        return -1;
    }
    if (fprintf(fd, "# 1oom configuration file\n") < 1) {
        goto fail;
    }
    for (const struct cfg_module_s *module = cfg_items_tbl; module->str; ++module) {
        if (module->cond && !(*module->cond)) {
            continue;
        }
        for (const struct cfg_items_s *item = module->items; item->name; ++item) {
            switch (item->type) {
                case CFG_TYPE_INT:
                    if (fprintf(fd, "%s.%s = %i\n", module->str, item->name, *((const int *)item->var)) < 1) {
                        goto fail;
                    }
                    break;
                case CFG_TYPE_STR:
                    if (fprintf(fd, "%s.%s = ", module->str, item->name) < 1) {
                        goto fail;
                    }
                    if (*((const char **)item->var) != 0) {
                        if (fprintf(fd, "\"") < 1) {
                            goto fail;
                        }
                        if (util_cstr_out(fd, *((const char **)item->var)) < 0) {
                            goto fail;
                        }
                        if (fprintf(fd, "\"") < 1) {
                            goto fail;
                        }
                    }
                    if (fprintf(fd, "\n") < 1) {
                        goto fail;
                    }
                    break;
                case CFG_TYPE_BOOL:
                    if (fprintf(fd, "%s.%s = %s\n", module->str, item->name, (*((const bool *)item->var)) ? "true" : "false") < 1) {
                        goto fail;
                    }
                    break;
                case CFG_TYPE_COMMENT:
                    if (fprintf(fd, "# %s\n", item->name) < 1) {
                        goto fail;
                    }
                    break;
            }
        }
    }
    if (fd) {
        fclose(fd);
    }
    return 0;
fail:
    log_error("Cfg: writing to '%s' failed\n", filename);
    if (fd) {
        fclose(fd);
    }
    return -1;
}
