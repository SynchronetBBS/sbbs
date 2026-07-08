#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"
#include "lib.h"
#include "log.h"
#include "os.h"
#include "types.h"
#include "util.h"
#include "util_cstr.h"

/* -------------------------------------------------------------------------- */

#ifdef FEATURE_MODEBUG
int opt_modebug = 0;
#endif

#define SEP_CHAR    ','

#define LBX_HEADER_LEN  0x200
#define LBX_DESC_LEN  0x20
#define LBX_MAX_ENTRIES ((LBX_HEADER_LEN - 4 - 4) / 4)

static struct lbxedit_s {
    uint8_t *data;
    uint32_t len;
    const char *filename_lbx;
    uint8_t type;
    uint8_t entries;
    struct lbxedit_item_s {
        uint32_t start;
        uint32_t len;
        uint8_t *data;
        char desc[LBX_DESC_LEN + 1];
    } item[LBX_MAX_ENTRIES + 1];
} lbxedit_ctx;

/* -------------------------------------------------------------------------- */

static void lbxedit_init(struct lbxedit_s *ctx)
{
    ctx->data = 0;
}

static void lbxedit_shutdown(struct lbxedit_s *ctx)
{
    if (ctx->data) {
        lib_free(ctx->data);
        ctx->data = 0;
    }
}

static bool lbxedit_load_lbx(struct lbxedit_s *ctx, const char *filename)
{
    uint32_t len;
    uint16_t v16;
    uint8_t num;
    uint8_t *data;
    ctx->filename_lbx = filename;
    data = util_file_load(filename, &len);
    if (!data) {
        return false;
    }
    ctx->data = data;
    if (len < LBX_HEADER_LEN) {
        log_error("file '%s' shorter than LBX header (%u < %u)\n", filename, len, LBX_HEADER_LEN);
        return false;
    }
    ctx->len = len;
    v16 = GET_LE_16(&data[2]);
    if (v16 != 0xfead) {
        log_error("file '%s' has wrong signature 0x%04x!\n", filename, v16);
        return false;
    }
    v16 = GET_LE_16(&data[6]);
    ctx->type = v16;
    v16 = GET_LE_16(data);
    if (v16 > LBX_MAX_ENTRIES) {
        log_error("file '%s' has too many entries  (%u < %u)\n", filename, v16, LBX_MAX_ENTRIES);
        return false;
    }
    ctx->entries = num = v16;
    for (uint8_t i = 0; i < num; ++i) {
        memcpy(ctx->item[i].desc, (const char *)&(ctx->data[LBX_HEADER_LEN + LBX_DESC_LEN * i]), LBX_DESC_LEN);
        ctx->item[i].desc[LBX_DESC_LEN] = '\0';
    }
    for (uint8_t i = 0; i < num; ++i) {
        uint32_t offs0, offs1;
        offs0 = GET_LE_32(&data[8 + i * 4]);
        offs1 = GET_LE_32(&data[8 + i * 4 + 4]);
        if ((offs0 > offs1) || (offs1 > len) || (offs0 < LBX_HEADER_LEN)) {
            log_error("file '%s' item %i invalid offsets 0x%x, 0x%x (len 0x%x)\n", filename, i, offs0, offs1, len);
            return false;
        }
        if (offs0 < (LBX_HEADER_LEN + LBX_DESC_LEN * num)) {
            uint8_t j;
            j = (offs0 - LBX_HEADER_LEN) / 0x20;
            log_warning("file '%s' item %i starts early at 0x%x (expected >= 0x%x), invalidating name/desc of items %i..%i\n", filename, i, offs0, LBX_HEADER_LEN + LBX_DESC_LEN * num, j, num - 1);
            for (; j < num; ++j) {
                memset(ctx->item[j].desc, 0, LBX_DESC_LEN);
            }
        }
        ctx->item[i].start = offs0;
        ctx->item[i].len = offs1 - offs0;
        ctx->item[i].data = &(ctx->data[offs0]);
    }
    {
        uint32_t offs0 = GET_LE_32(&data[8 + num * 4]);
        ctx->item[num].start = offs0;
        ctx->item[num].len = 0;
        ctx->item[num].data = 0;
    }
    return true;
}

static int lbxedit_dump_desc(FILE *fd, const char *str)
{
    int len;
    if (fprintf(fd, "\"") < 0) {
        return -1;
    }
    for (len = LBX_DESC_LEN - 1; (len >= 0) && (str[len] == '\0'); --len);
    ++len;
    for (int i = 0; i < len; ++i) {
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
                    if (fwrite(buf, 4, 1, fd) < 1) {
                        return -1;
                    }
                }
                break;
        }
        if (0
          || (c0 && (fputc(c0, fd) == EOF))
          || (c && (fputc(c, fd) == EOF))
        ) {
            return -1;
        }
    }
    if (fprintf(fd, "\"") < 0) {
        return -1;
    }
    return 0;
}

static int lbxedit_do_dump(struct lbxedit_s *ctx, const char *filename_lbx, const char *prefix_out, bool flag_write)
{
    FILE *fd = 0;
    char *fname_lbxin = NULL, *dir = NULL, *fnam = NULL, *prefix = NULL, *prefix_file = NULL, *lbxin = NULL;
    int res = -1;
    if (!lbxedit_load_lbx(ctx, filename_lbx)) {
        return -1;
    }
    prefix = lib_stralloc(prefix_out ? prefix_out : filename_lbx);
    {
        char *p;
        p = strrchr(prefix, '.');
        if (p && (strcasecmp(p + 1, "lbx") == 0) && ((*(p + 4)) == '\0')) {
            *p = '\0';
        }
    }
    util_fname_split(prefix, &dir, &fnam);
    if (dir) {
        if (flag_write && os_make_path(dir)) {
            log_error("creating path '%s'\n", dir);
            return -1;
        }
        fname_lbxin = util_concat(dir, FSDEV_DIR_SEP_STR, fnam, ".lbxin", NULL);
        prefix_file = fnam;
    } else {
        prefix_file = prefix;
        fname_lbxin = util_concat(prefix, ".lbxin", NULL);
    }
    if (flag_write) {
        lbxin = fname_lbxin;
        fd = fopen(fname_lbxin, "w+");
        if (!fd) {
            log_error("opening '%s' for writing\n", lbxin);
            goto fail;
        }
    } else {
        fd = stdout;
        lbxin = "(stdout)";
    }
    if (fprintf(fd, "# 1oom_lbxedit dump of '%s', %u entries, len 0x%x\nt%i\n", filename_lbx, ctx->entries, ctx->len, ctx->type) < 0) {
        log_error("writing to '%s'\n", lbxin);
        goto fail;
    }
    for (uint8_t i = 0; i < ctx->entries; ++i) {
        char buf[FSDEV_PATH_MAX];
        const struct lbxedit_item_s *p = &(ctx->item[i]);
        uint32_t len;
        len = p->len;
        if (len == 0) {
            lib_sprintf(buf, sizeof(buf), "0");
        } else {
            lib_sprintf(buf, sizeof(buf), "%s_%03i.bin", prefix_file, i);
        }
        if (0
          || (fprintf(fd, "# %i 0x%x..0x%x (0x%x)\n", i, p->start, p->start + len, len) < 0)
          || (lbxedit_dump_desc(fd, p->desc) < 0)
          || (fprintf(fd, "%c%s\n", SEP_CHAR, buf) < 0)
        ) {
            log_error("writing to '%s'\n", lbxin);
            goto fail;
        }
        if (flag_write && len) {
            lib_sprintf(buf, sizeof(buf), "%s_%03i.bin", prefix, i);
            if (util_file_save(buf, p->data, len) < 0) {
                log_error("writing '%s'!\n", buf);
                goto fail;
            }
        }
    }
    res = 0;
fail:
    if (flag_write && fd) {
        fclose(fd);
        fd = NULL;
    }
    lib_free(fname_lbxin);
    lib_free(dir);
    lib_free(fnam);
    lib_free(prefix);
    return res;
}

static int lbxedit_do_extract(struct lbxedit_s *ctx, const char *filename_lbx, const char *filename_out, bool flag_write, int itemnum)
{
    if (!lbxedit_load_lbx(ctx, filename_lbx)) {
        return -1;
    }
    if ((itemnum < 0) || (itemnum >= ctx->entries)) {
        fprintf(stderr, "invalid itemnum %i, must be 0 <= N < %u\n", itemnum, ctx->entries);
        return -1;
    }
    if (flag_write) {
        const struct lbxedit_item_s *p = &(ctx->item[itemnum]);
        if (util_file_save(filename_out, p->data, p->len) < 0) {
            log_error("writing '%s'!\n", filename_out);
            return -1;
        }
    }
    return 0;
}

static bool isnl(char c)
{
    return (c == '\r') || (c == '\n');
}

static bool isws(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static int skipws(const char *str, int i, int len)
{
    while ((i < len) && isws(str[i])) {
        ++i;
    }
    return i;
}

static int lbxedit_parsedesc(char *str, int i, int len, char *dst)
{
    uint32_t dlen;
    int slen;
    memset(dst, 0, LBX_DESC_LEN + 1);
    if (str[i] != '\"') {
        log_error("missing start '\"' at %i\n", i);
        return -1;
    }
    if ((slen = util_cstr_parse(&(str[i]), &(str[i]), &dlen)) < 0) {
        return -1;
    }
    if (dlen > LBX_DESC_LEN) {
        log_error("desc too long (%i > %i)\n", dlen, LBX_DESC_LEN);
        return -1;
    }
    memcpy(dst, &(str[i]), dlen);
    i += slen;
    if (str[i] != SEP_CHAR) {
        log_error("missing '%c' after '\"' at %i\n", SEP_CHAR, i);
        return -1;
    }
    return ++i;
}

static int lbxedit_do_make(struct lbxedit_s *ctx, const char *filename_lbxin, const char *filename_lbx, bool flag_write)
{
    FILE *fd = NULL;
    int i, num_items = 0, res = -1;
    uint32_t len = 0;
    char *dir = NULL;
    char *listfiledata = (char *)util_file_load(filename_lbxin, &len);
    if (!listfiledata) {
        return -1;
    }
    util_fname_split(filename_lbxin, &dir, NULL);
    i = 0;
    ctx->data = NULL;
    ctx->type = 0;
    while ((i < len) && (listfiledata[i] != '\0')) {
        i = skipws(listfiledata, i, len);
        if (listfiledata[i] == '\0') {
            break;
        } else if (listfiledata[i] == '#') {
        } else if (listfiledata[i] == 't') {
            ctx->type = listfiledata[++i] - '0';
        } else {
            struct lbxedit_item_s *p;
            if (num_items >= LBX_MAX_ENTRIES) {
                log_error("too manu entries (max %i)\n", LBX_MAX_ENTRIES);
                goto fail;
            }
            p = &(ctx->item[num_items]);
            if ((i = lbxedit_parsedesc(listfiledata, i, len, p->desc)) < 0) {
                goto fail;
            }
            {
                const char *fname;
                char *fullname;
                uint32_t l;
                uint8_t *data;
                fname = (char *)&(listfiledata[i]);
                while ((i < len) && !isnl(listfiledata[i])) {
                    ++i;
                }
                if ((i >= len) || !isnl(listfiledata[i])) {
                    log_error("partial line near byte %i\n", i);
                    return -1;
                }
                listfiledata[i] = '\0';
                ++i;
                if (strcmp(fname, "0") == 0) {
                    data = NULL;
                    l = 0;
                } else {
                    if (dir && *dir) {
                        fullname = util_concat(dir, FSDEV_DIR_SEP_STR, fname, NULL);
                    } else {
                        fullname = lib_stralloc(fname);
                    }
                    data = util_file_load(fullname, &l);
                    lib_free(fullname);
                    if (!data) {
                        goto fail;
                    }
                }
                p->data = data;
                p->len = l;
            }
            ++num_items;
        }
        while ((i < len) && !isnl(listfiledata[i])) {
            ++i;
        }
    }
    /* make header */
    {
        uint32_t offs;
        offs = LBX_HEADER_LEN + LBX_DESC_LEN * num_items;
        ctx->data = lib_malloc(offs);
        ctx->len = offs;
        SET_LE_16(&ctx->data[0], num_items);
        SET_LE_16(&ctx->data[2], 0xfead);
        SET_LE_16(&ctx->data[6], ctx->type);
        for (int i = 0; i < num_items; ++i) {
            struct lbxedit_item_s *p = &(ctx->item[i]);
            p->start = offs;
            SET_LE_32(&ctx->data[8 + i * 4], offs);
            offs += p->len;
            memcpy(&(ctx->data[LBX_HEADER_LEN + i * LBX_DESC_LEN]), p->desc, LBX_DESC_LEN);
        }
        SET_LE_32(&ctx->data[8 + num_items * 4], offs);
    }
    /* save lbx */
    if (flag_write) {
        fd = fopen(filename_lbx, "wb");
        if (0
          || (!fd)
          || (fwrite(ctx->data, ctx->len, 1, fd) != 1)
        ) {
            perror(filename_lbx);
            goto fail;
        }
        for (int i = 0; i < num_items; ++i) {
            struct lbxedit_item_s *p = &(ctx->item[i]);
            if (p->len && (fwrite(p->data, p->len, 1, fd) != 1)) {
                perror(filename_lbx);
                goto fail;
            }
        }
    }
    res = 0;
fail:
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    lib_free(listfiledata);
    listfiledata = NULL;
    lib_free(ctx->data);
    ctx->data = NULL;
    for (int i = 0; i < num_items; ++i) {
        struct lbxedit_item_s *p = &(ctx->item[i]);
        if (p->data) {
            lib_free(p->data);
            p->data = NULL;
        }
    }
    return res;
}

static int lbxedit_do_replace(struct lbxedit_s *ctx, const char *filename_lbx_in, const char *filename_lbx_out, const char *filename_in, bool flag_write, int itemnum)
{
    if (!lbxedit_load_lbx(ctx, filename_lbx_in)) {
        return -1;
    }
    if ((itemnum < 0) || (itemnum >= ctx->entries)) {
        fprintf(stderr, "invalid itemnum %i, must be 0 <= N < %u\n", itemnum, ctx->entries);
        return -1;
    }
    {
        struct lbxedit_item_s *p = &(ctx->item[itemnum]);
        uint8_t *data = NULL, *lbxdata = NULL;
        uint32_t lenold, lennew = 0, o1old;
        int lendiff;
        if (strcmp(filename_in, "0") != 0) {
            data = util_file_load(filename_in, &lennew);
            if (!data) {
                return false;
            }
        }
        lenold = p->len;
        lendiff = lennew - lenold;
        p->data = data;
        p->len = lennew;
        o1old = p[1].start;
        for (int i = itemnum + 1; i <= ctx->entries; ++i) {
            struct lbxedit_item_s *q = &(ctx->item[i]);
            uint32_t offs;
            offs = q->start;
            offs += lendiff;
            q->start = offs;
            SET_LE_32(&ctx->data[8 + i * 4], offs);
        }
        lbxdata = lib_malloc(ctx->len + lendiff);
        {
            uint32_t pos = 0, len;
            len = p->start;
            memcpy(&lbxdata[pos], ctx->data, len);
            pos += len;
            len = lennew;
            memcpy(&lbxdata[pos], data, len);
            pos += len;
            len = ctx->len - o1old;
            if (len) {
                memcpy(&lbxdata[pos], &(ctx->data[o1old]), len);
                pos += len;
            }
            ctx->len = pos;
        }
        lib_free(data);
        lib_free(ctx->data);
        ctx->data = lbxdata;
        ctx->filename_lbx = filename_lbx_out;
        if (flag_write) {
            if (util_file_save(ctx->filename_lbx, ctx->data, ctx->len) < 0) {
                log_error("writing '%s'!\n", ctx->filename_lbx);
                return -1;
            }
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

static void show_usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    1oom_lbxedit [OPTIONS] d IN.LBX [OUTPREFIX]\n"
                    "    1oom_lbxedit [OPTIONS] m LBXIN.TXT OUT.LBX\n"
                    "    1oom_lbxedit [OPTIONS] e IN.LBX OUT.BIN ITEMNUM\n"
                    "    1oom_lbxedit [OPTIONS] r FILE.LBX IN.BIN|0 ITEMNUM\n"
                    "Commands:\n"
                    "    d   Dump LBX file to LBXIN + bin files\n"
                    "    m   Make LBX file from LBXIN + bin files\n"
                    "    e   Extract item from LBX file\n"
                    "    r   Replace item in LBX file\n"
                    "Options:\n"
                    "    -w          (d, r) Write files\n"
                    "    -o OUT.LBX  (r) Set output LBX filename\n"
           );
}

/* -------------------------------------------------------------------------- */

int main_1oom(int argc, char **argv)
{
    const char *filename_in;
    const char *filename_out = NULL, *filename_lbx_out = NULL;
    bool flag_write = false;
    char cmd;
    int i, res = 1;
    lbxedit_init(&lbxedit_ctx);
    i = 1;
    if (i >= argc) {
        goto fail;
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
            case 'o':
                if (++i >= argc) {
                    goto fail;
                }
                filename_lbx_out = argv[i];
                break;
            default:
                goto fail;
        }
        ++i;
    }
    if ((i + 1) >= argc) {
        goto fail;
    }
    {
        const char *cmd_str;
        cmd_str = argv[i++];
        if ((cmd_str[1] != '\0') || (strchr("edmr", cmd_str[0]) == 0)) {
            goto fail;
        }
        cmd = cmd_str[0];
    }
    filename_in = argv[i++];
    if (i < argc) {
        filename_out = argv[i++];
    }
    switch (cmd) {
        case 'd':
            if (i < argc) {
                goto fail;
            }
            res = lbxedit_do_dump(&lbxedit_ctx, filename_in, filename_out, flag_write);
            break;
        case 'm':
            if (!filename_out) {
                fprintf(stderr, "output filename missing\n");
                goto fail;
            }
            if (i < argc) {
                goto fail;
            }
            res = lbxedit_do_make(&lbxedit_ctx, filename_in, filename_out, true);
            break;
        case 'e':
            if (!filename_out) {
                fprintf(stderr, "output filename missing\n");
                goto fail;
            }
            if (i >= argc) {
                fprintf(stderr, "item number missing\n");
                goto fail;
            }
            {
                int itemnum;
                itemnum = atoi(argv[i++]);
                if (i < argc) {
                    goto fail;
                }
                res = lbxedit_do_extract(&lbxedit_ctx, filename_in, filename_out, true, itemnum);
            }
            break;
        case 'r':
            if (!filename_out) {
                fprintf(stderr, "input filename missing\n");
                goto fail;
            }
            if (i >= argc) {
                fprintf(stderr, "item number missing\n");
                goto fail;
            }
            if (!filename_lbx_out) {
                filename_lbx_out = filename_in;
            }
            {
                int itemnum;
                itemnum = atoi(argv[i++]);
                if (i < argc) {
                    goto fail;
                }
                res = lbxedit_do_replace(&lbxedit_ctx, filename_in, filename_lbx_out, filename_out, flag_write, itemnum);
            }
            break;
        default:
            break;
    }
fail:
    lbxedit_shutdown(&lbxedit_ctx);
    if (res > 0) {
        show_usage();
    } else if (res < 0) {
        res = -res;
    }
    return res;
}
