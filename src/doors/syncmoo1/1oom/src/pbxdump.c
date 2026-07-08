#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pbx.h"
#include "lbx.h"
#include "lib.h"
#include "log.h"
#include "os.h"
#include "types.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

#ifdef FEATURE_MODEBUG
int opt_modebug = 0;
#endif

bool game_num_patch(const char *numid, const int32_t *patchnums, int first, int num)
{
    return false;
}

bool game_str_patch(const char *strid, const char *patchstr, int i)
{
    return false;
}

lbxfile_e lbxfile_id(const char *filename)
{
    return LBXFILE_NUM;
}

void lbxfile_add_patch(lbxfile_e file_id, uint16_t i, uint8_t *data, uint32_t len, const char *patchfilename)
{
}

void lbxfile_add_overwrite(lbxfile_e file_id, uint16_t i, uint32_t itemoffs, uint8_t *data, uint32_t len, const char *patchfilename)
{
}

/* -------------------------------------------------------------------------- */

struct pbxdump_s {
    FILE *fd;
    const char *pbxin;
    const char *prefix;
    const char *prefix_file;
    bool flag_write;
};

/* -------------------------------------------------------------------------- */

static int pbxdump_write(const char *prefix, const char *suffix, bool flag_write, const uint8_t *data, uint32_t len)
{
    if (flag_write) {
        char *fname = util_concat(prefix, suffix, NULL);
        if (util_file_save(fname, data, len) < 0) {
            log_error("writing '%s'!\n", fname);
            lib_free(fname);
            return -1;
        }
        lib_free(fname);
    }
    return 0;
}

static int pbx_dump_textish(struct pbxdump_s *d, const char *filename, int pbxi, const uint8_t *str, uint32_t len, const char *pbxtext, const char *suffix)
{
    bool can_print;
    if (len < 10) {
        can_print = true;
    } else if (len > 200) {
        can_print = false;
    } else {
        int n = 0;
        for (uint32_t i = 0; i < len; ++i) {
            char c;
            c = str[i];
            n += ((c < 0x20) || (c > 0x7e)) ? -1 : 1;
        }
        can_print = (n > 2);
    }
    if (can_print) {
        if (fprintf(d->fd, "%s,\"", pbxtext) < 0) {
            goto fail;
        }
        for (uint32_t i = 0; i < len; ++i) {
            char c, c0;
            c = str[i];
            switch (c) {
                case '\\':
                case '"':
                    c0 = '\\';
                    break;
                case '\n':
                    c0 = '\\';
                    c = 'n';
                    break;
                case '\r':
                    c0 = '\\';
                    c = 'r';
                    break;
                default:
                    c0 = 0;
                    if ((c < 0x20) || (c > 0x7e)) {
                        const char hexchars[0x10] = "0123456789abcdef";
                        char buf[4];
                        buf[0] = '\\';
                        buf[1] = 'x';
                        buf[2] = hexchars[(c >> 4) & 0xf];
                        buf[3] = hexchars[c & 0xf];
                        c = 0;
                        if (fwrite(buf, 4, 1, d->fd) < 1) {
                            goto fail;
                        }
                    }
                    break;
            }
            if (0
              || (c0 && (fputc(c0, d->fd) == EOF))
              || (c && (fputc(c, d->fd) == EOF))
            ) {
                goto fail;
            }
        }
        if (fprintf(d->fd, "\"\n") < 0) {
            goto fail;
        }
        return 0;
    } else {
        if (fprintf(d->fd, "%s,%s%s\n", pbxtext, d->prefix_file, suffix) < 0) {
            goto fail;
        }
        return pbxdump_write(d->prefix, suffix, d->flag_write, (const uint8_t *)str, len);
    }
fail:
    log_error("writing PBXIN file %s\n", d->pbxin);
    return -1;
}

static int pbx_cb_name(void *ctx, const char *filename, int pbxi, uint8_t *str, uint32_t len)
{
    return pbx_dump_textish(ctx, filename, pbxi, str, len, "0", "_name.txt");
}

static int pbx_cb_desc(void *ctx, const char *filename, int pbxi, uint8_t *str, uint32_t len)
{
    return pbx_dump_textish(ctx, filename, pbxi, str, len, "1", "_desc.txt");
}

static int pbx_cb_lbxp(void *ctx, const char *filename, int pbxi, const char *id, uint16_t itemi, uint8_t *data, uint32_t len)
{
    char pbxtext[32];
    char suffix[40];
    char *p = suffix;
    *p++ = '_';
    for (const char *q = id; *q != '.';) {
        *p++ = *q++;
    }
    lib_sprintf(p, sizeof(suffix), "_%03u.bin", itemi);
    lib_sprintf(pbxtext, sizeof(pbxtext), "2,%s,%u", id, itemi);
    return pbx_dump_textish(ctx, filename, pbxi, data, len, pbxtext, suffix);
}

static int pbx_cb_lbxo(void *ctx, const char *filename, int pbxi, const char *id, uint16_t itemi, uint8_t *data, uint32_t len, uint32_t itemoffs)
{
    char pbxtext[32];
    char suffix[40];
    char *p = suffix;
    *p++ = '_';
    for (const char *q = id; *q != '.';) {
        *p++ = *q++;
    }
    lib_sprintf(p, sizeof(suffix), "_%03u.bin", itemi);
    lib_sprintf(pbxtext, sizeof(pbxtext), "5,%s,%u,0x%x", id, itemi, itemoffs);
    return pbx_dump_textish(ctx, filename, pbxi, data, len, pbxtext, suffix);
}

static bool pbx_cb_strp(void *ctx, const char *filename, int pbxi, const char *id, const uint8_t *patchstr, int itemi, uint32_t len)
{
    char pbxtext[32];
    char suffix[40];
    lib_sprintf(pbxtext, sizeof(pbxtext), "3,%s,%i", id, itemi);
    lib_sprintf(suffix, sizeof(suffix), "_%s_%i.txt", id, itemi);
    return (pbx_dump_textish(ctx, filename, pbxi, patchstr, len, pbxtext, suffix) == 0);
}

static bool pbx_cb_nump(void *ctx, const char *filename, int pbxi, const char *id, const int32_t *patchnums, int first, int num)
{
    struct pbxdump_s *d = ctx;
    if (fprintf(d->fd, "4,%s,%i", id, first) < 0) {
        return false;
    }
    for (int i = 0; i < num; ++i) {
        if (fprintf(d->fd, ",%i", patchnums[i]) < 0) {
            return false;
        }
    }
    if (fprintf(d->fd, "\n") < 0) {
        return -1;
    }
    return true;
}

static int dump_pbx(const char *filename, const char *prefix_in, bool flag_write)
{
    struct pbx_add_cbs cbs;
    struct pbxdump_s ctx;
    char *fname_pbxin = NULL, *dir = NULL, *fnam = NULL, *prefix = NULL;
    int res;
    cbs.name = pbx_cb_name;
    cbs.desc = pbx_cb_desc;
    cbs.lbxp = pbx_cb_lbxp;
    cbs.lbxo = pbx_cb_lbxo;
    cbs.strp = pbx_cb_strp;
    cbs.nump = pbx_cb_nump;
    ctx.flag_write = flag_write;
    ctx.prefix = prefix = lib_stralloc(prefix_in ? prefix_in : filename);
    {
        char *p;
        p = strrchr(prefix, '.');
        if (p && (strcasecmp(p + 1, "pbx") == 0) && ((*(p + 4)) == '\0')) {
            *p = '\0';
        }
    }
    util_fname_split(ctx.prefix, &dir, &fnam);
    if (dir) {
        if (flag_write && os_make_path(dir)) {
            log_error("creating path '%s'\n", dir);
            return -1;
        }
        fname_pbxin = util_concat(dir, FSDEV_DIR_SEP_STR, fnam, ".pbxin", NULL);
        ctx.prefix_file = fnam;
    } else {
        ctx.prefix_file = ctx.prefix;
        fname_pbxin = util_concat(ctx.prefix, ".pbxin", NULL);
    }
    if (flag_write) {
        log_message("PBXDUMP: writing to '%s'\n", fname_pbxin);
        ctx.fd = fopen(fname_pbxin, "w+");
        ctx.pbxin = fname_pbxin;
    } else {
        log_message("PBXDUMP: dry run, would use prefix '%s'\n", ctx.prefix);
        ctx.fd = stdout;
        ctx.pbxin = "(stdout)";
    }
    res = pbx_add_file(filename, &cbs, &ctx);
    if (flag_write) {
        fclose(ctx.fd);
        ctx.fd = NULL;
    }
    lib_free(fname_pbxin);
    lib_free(dir);
    lib_free(fnam);
    lib_free(prefix);
    return res;
}

/* -------------------------------------------------------------------------- */

static void show_usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    1oom_pbxdump [OPTIONS] IN.PBX [OUTPREFIX]\n"
                    "Options:\n"
                    "    -w       Write files\n"
           );
}

/* -------------------------------------------------------------------------- */

int main_1oom(int argc, char **argv)
{
    const char *filename_in;
    const char *prefix_out = NULL;
    bool flag_write = false;
    int i;
    i = 1;
    if (i >= argc) {
        show_usage();
        return 1;
    }
    while (argv[i][0] == '-') {
        if (argv[i][2] != '\0') {
            show_usage();
            return 1;
        }
        switch (argv[i][1]) {
            case 'w':
                flag_write = true;
                break;
            default:
                show_usage();
                return 1;
        }
        ++i;
    }
    if (i >= argc) {
        show_usage();
        return 1;
    }
    filename_in = argv[i++];
    if (i < argc) {
        prefix_out = argv[i++];
        if (i < argc) {
            show_usage();
            return 1;
        }
    }
    return dump_pbx(filename_in, prefix_out, flag_write);
}
