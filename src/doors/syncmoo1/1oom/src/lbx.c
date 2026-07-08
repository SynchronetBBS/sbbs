#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lbx.h"
#include "bits.h"
#include "lbxgfx.h"
#include "lib.h"
#include "log.h"
#include "os.h"
#include "types.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

#define LBX_HEADER_LEN  0x200

struct lbxpatch_s {
    struct lbxpatch_s *next;
    uint8_t *data;
    uint32_t len;
    uint32_t offs;
    uint16_t i;
};

struct lbx_s {
    enum {
        LBX_MODE_NONE = 0,
        LBX_MODE_FILE_OPEN,
        LBX_MODE_MEMORY
    } mode;
    FILE *fd;
    uint32_t len;
    uint16_t entries;
    uint16_t type;
    uint8_t *header;
    uint8_t *data;
    int num_in_use;
    struct lbxpatch_s *patches;
    struct lbxpatch_s *overwrites;
};

static const struct {
    const char *filename;
    bool in_memory;     /* load whole file to memory */
} lbxinfo[LBXFILE_NUM] = {
    { "backgrnd.lbx", true },
    { "colonies.lbx", true },
    { "council.lbx", true },
    { "design.lbx", true },
    { "diplomat.lbx", true },
    { "embassy.lbx", true },
    { "eventmsg.lbx", true },
    { "firing.lbx", true },
    { "fonts.lbx", true },
    { "help.lbx", true },
    { "intro.lbx", false },
    { "intro2.lbx", false },
    { "introsnd.lbx", false },
    { "landing.lbx", true },
    { "missile.lbx", true },
    { "music.lbx", true },
    { "names.lbx", true },
    { "nebula.lbx", true },
    { "newscast.lbx", true },
    { "planets.lbx", true },
    { "research.lbx", true },
    { "screens.lbx", true },
    { "ships.lbx", true },
    { "ships2.lbx", true },
    { "soundfx.lbx", true },
    { "space.lbx", true },
    { "spies.lbx", true },
    { "starmap.lbx", true },
    { "starview.lbx", true },
    { "techno.lbx", true },
    { "v11.lbx", true },
    { "vortex.lbx", true },
    { "winlose.lbx", false }
};

static struct lbx_s lbxtbl[LBXFILE_NUM] = { 0 };

static char entrynamebuf[32 + 1]; /* HACK */

/* -------------------------------------------------------------------------- */

static FILE *lbxfile_try_fopen(const char *path, const char *filename)
{
    FILE *fd;
    char *fname;
    if (path) {
        fname = util_concat(path, FSDEV_DIR_SEP_STR, filename, NULL);
    }
    fd = fopen(path ? fname : filename, "rb");
    if (path) {
        lib_free(fname);
        fname = NULL;
    }
    if (!fd) {
        char buf[32];
        const char *p = filename;
        char *q = buf;
        while (*p) {
            *q++ = toupper(*p++);
        }
        *q = 0;
        if (path) {
            fname = util_concat(path, FSDEV_DIR_SEP_STR, buf, NULL);
        }
        fd = fopen(path ? fname : buf, "rb");
        if (path) {
            lib_free(fname);
            fname = NULL;
        }
    }
    return fd;
}

static int lbxfile_open(struct lbx_s *p, const char *filename)
{
    uint16_t v;

    p->fd = lbxfile_try_fopen(os_get_path_data(), filename);

    if (!p->fd) {
        log_error("opening file '%s'!\n", filename);
        goto fail;
    }

    p->header = lib_malloc(LBX_HEADER_LEN);

    if (fread(p->header, LBX_HEADER_LEN, 1, p->fd) < 1) {
        log_error("reading header of file '%s'!\n", filename);
        goto fail;
    }

    v = GET_LE_16(&p->header[0]);
    p->entries = v;
    v = GET_LE_16(&p->header[2]);
    if (v != 0xfead) {
        log_error("file '%s' has wrong signature 0x%04x!\n", filename, v);
        goto fail;
    }

    v = GET_LE_16(&p->header[4]);
    if (v != 0) {
        log_error("file '%s' has nonzero at offs 4: 0x%04x!\n", filename, v);
    }

    p->type = GET_LE_16(&p->header[6]);

    if (fseek(p->fd, 0, SEEK_END)) {
        log_error("error seeking file '%s'\n", filename);
        goto fail;
    }

    p->len = ftell(p->fd);
    rewind(p->fd);
    p->mode = LBX_MODE_FILE_OPEN;
    p->num_in_use = 0;
    return 0;
fail:
    if (p->fd) {
        fclose(p->fd);
        p->fd = NULL;
    }
    lib_free(p->header);
    p->header = NULL;
    return -1;
}

static void lbxfile_close(lbxfile_e i)
{
    switch (lbxtbl[i].mode) {
        case LBX_MODE_FILE_OPEN:
            fclose(lbxtbl[i].fd);
            lbxtbl[i].fd = NULL;
            lib_free(lbxtbl[i].header);
            lbxtbl[i].header = NULL;
            break;
        case LBX_MODE_MEMORY:
            lib_free(lbxtbl[i].data);
            lbxtbl[i].data = NULL;
            lbxtbl[i].header = NULL;
            break;
        case LBX_MODE_NONE:
            break;
    }
    lbxtbl[i].mode = LBX_MODE_NONE;
}

static int lbxfile_load(lbxfile_e i)
{
    if (lbxfile_open(&lbxtbl[i], lbxinfo[i].filename)) {
        goto fail;
    }
    if (lbxinfo[i].in_memory) {
        lbxtbl[i].data = lib_malloc(lbxtbl[i].len);
        if (fread(lbxtbl[i].data, lbxtbl[i].len, 1, lbxtbl[i].fd) < 1) {
            log_error("problem reading file %s\n", lbxinfo[i].filename);
            lib_free(lbxtbl[i].data);
            lbxtbl[i].data = NULL;
            goto fail;
        }
        fclose(lbxtbl[i].fd);
        lbxtbl[i].fd = NULL;
        lib_free(lbxtbl[i].header);
        lbxtbl[i].header = lbxtbl[i].data;
        lbxtbl[i].mode = LBX_MODE_MEMORY;
    }
    return 0;
    fail:
    exit(EXIT_FAILURE);
    return 1;
}

static uint8_t *lbx_extract(struct lbx_s *p, uint16_t i, uint32_t *len_ptr, const char *filename)
{
    if (!p) {
        return NULL;
    }
    if (i >= p->entries) {
        log_error("LBX: %s invalid id %i >= %i\n", filename, i, p->entries);
        return NULL;
    }
    uint32_t offs0, offs1, len;
    offs0 = GET_LE_32(&p->header[8 + i * 4]);
    offs1 = GET_LE_32(&p->header[8 + i * 4 + 4]);
    len = offs1 - offs0;
    if (len_ptr) {
        *len_ptr = len;
    }
    if (p->mode == LBX_MODE_FILE_OPEN) {
        if (fseek(p->fd, offs0, SEEK_SET)) {
            log_error("LBX: problem seeking %s to %i\n", filename, offs0);
            return NULL;
        }
        uint8_t *d = lib_malloc(len);
        if (fread(d, len, 1, p->fd) < 1) {
            log_error("LBX: problem reading file %s\n", filename);
            return NULL;
        }
        return d;
    } else if (p->mode == LBX_MODE_MEMORY) {
        return &p->data[offs0];
    } else {
        log_error("LBX: extract without open\n");
        return NULL;
    }
}

static uint8_t *lbxfile_get_patch(struct lbx_s *lbx, uint16_t i, uint32_t *len_ptr)
{
    struct lbxpatch_s *p;
    if (!lbx) {
        return NULL;
    }
    p = lbx->patches;
    while (p) {
        if (p->i == i) {
            if (len_ptr) {
                *len_ptr = p->len;
            }
            return p->data;
        }
        p = p->next;
    }
    return NULL;
}

static bool lbxfile_apply_overwrites(struct lbx_s *lbx, uint16_t i, uint8_t *data, uint32_t len, const char *lbxname)
{
    struct lbxpatch_s *p;
    if (!lbx) {
        return false;
    }
    p = lbx->overwrites;
    while (p) {
        if (p->i == i) {
            if ((p->offs >= len) || ((p->offs + p->len) > len)) {
                log_fatal_and_die("LBX: PBX overwrite for %s item %i offs 0x%x len 0x%x exceeds item len 0x%x!\n", lbxname, i, p->offs, p->len, len);
                return false;
            }
            memcpy(&data[p->offs], p->data, p->len);
        }
        p = p->next;
    }
    return true;
}

static uint8_t *lbxfile_item_get_do(lbxfile_e file_id, uint16_t entry_id, uint32_t *len_ptr)
{
    uint8_t *p = NULL;
    struct lbx_s *lbx = &lbxtbl[file_id];
    uint32_t len;

    p = lbxfile_get_patch(lbx, entry_id, &len);

    if (p == NULL) {
        if (lbx->mode == LBX_MODE_NONE) {
            if (lbxfile_load(file_id)) {
                goto fail;
            }
        }

        p = lbx_extract(lbx, entry_id, &len, lbxinfo[file_id].filename);

        if (p == NULL) {
            goto fail;
        }
    }

    if (!lbxfile_apply_overwrites(lbx, entry_id, p, len, lbxinfo[file_id].filename)) {
        goto fail;
    }

    if (len_ptr) {
        *len_ptr = len;
    }

    ++lbxtbl[file_id].num_in_use;
    return p;

fail:
    return NULL;
}

static void lbxfile_shutdown_patch_list(struct lbxpatch_s *p)
{
    struct lbxpatch_s *pn;
    while (p) {
        pn = p->next;
        p->next = NULL;
        lib_free(p->data);
        p->data = NULL;
        lib_free(p);
        p = pn;
    }
}

static void lbxfile_shutdown_patches(struct lbx_s *lbx)
{
    struct lbxpatch_s *p;
    if (!lbx) {
        return;
    }
    p = lbx->patches;
    lbx->patches = NULL;
    lbxfile_shutdown_patch_list(p);
    p = lbx->overwrites;
    lbx->overwrites = NULL;
    lbxfile_shutdown_patch_list(p);
}

/* -------------------------------------------------------------------------- */

int lbxfile_init(void)
{
    memset(lbxtbl, 0, sizeof(lbxtbl));
    return 0;
}

void lbxfile_shutdown(void)
{
    for (lbxfile_e i = LBXFILE_BACKGRND; i < LBXFILE_NUM; ++i) {
        lbxfile_close(i);
        lbxfile_shutdown_patches(&lbxtbl[i]);
    }
}

int lbxfile_find_dir(void)
{
    const char **paths = os_get_paths_data();
    const char **pp = paths;
    log_message_direct("LBX: seeking data directory\n");
    while (*pp) {
        FILE *fd;
        log_message("LBX: trying '%s'\n", *pp);
        if ((fd = lbxfile_try_fopen(*pp, "fonts.lbx"))) {
            fclose(fd);
            break;
        }
        ++pp;
    }
    if (!*pp) {
        log_error("could not find the LBX files! Try running with -data path_to_moo1.\n");
        return -1;
    } else {
        log_message("LBX: found data directory at '%s'\n", *pp);
        os_set_path_data(*pp);
    }
    return 0;
}

uint8_t *lbxfile_item_get(lbxfile_e file_id, uint16_t entry_id)
{
    return lbxfile_item_get_do(file_id, entry_id, 0);
}

uint8_t *lbxfile_item_get_with_len(lbxfile_e file_id, uint16_t entry_id, uint32_t *len_ptr)
{
    return lbxfile_item_get_do(file_id, entry_id, len_ptr);
}

void lbxfile_item_release(lbxfile_e file_id, uint8_t *ptr)
{
    if (lbxtbl[file_id].num_in_use <= 0) {
        log_error("%s: release without use\n", lbxinfo[file_id].filename);
        return;
    }
    if (lbxtbl[file_id].mode == LBX_MODE_FILE_OPEN) {
        lib_free(ptr);
    } else if (lbxtbl[file_id].mode == LBX_MODE_MEMORY) {
        if (lbxtbl[file_id].type == LBX_TYPE_GFX) {
            /* restore gfx data to original state */
            lbxgfx_set_frame_0(ptr);
            lbxgfx_set_epage(ptr, 0);
        }
    }
    --lbxtbl[file_id].num_in_use;
    if (lbxtbl[file_id].num_in_use <= 0) {
        lbxfile_close(file_id);
    }
}

void lbxfile_item_release_file(lbxfile_e file_id)
{
    lbxfile_close(file_id);
}

bool lbxfile_exists(lbxfile_e file_id)
{
    if (lbxtbl[file_id].mode != LBX_MODE_NONE) {
        return true;
    } else {
        FILE *fd = NULL;
        bool found;
        fd = lbxfile_try_fopen(os_get_path_data(), lbxinfo[file_id].filename);
        found = (fd != NULL);
        if (fd) {
            fclose(fd);
            fd = NULL;
        }
        return found;
    }
}

const char *lbxfile_name(lbxfile_e file_id)
{
    return lbxinfo[file_id].filename;
}

lbxfile_e lbxfile_id(const char *filename)
{
    for (lbxfile_e j = LBXFILE_BACKGRND; j < LBXFILE_NUM; ++j) {
        if (strcmp(lbxfile_name(j), filename) == 0) {
            return j;
        }
    }
    return LBXFILE_NUM;
}

int lbxfile_type(lbxfile_e file_id)
{
    if ((lbxtbl[file_id].mode == LBX_MODE_NONE) && (lbxfile_load(file_id))) {
        goto fail;
    }
    return lbxtbl[file_id].type;
    fail:
    return -1;
}

int lbxfile_num_items(lbxfile_e file_id)
{
    if ((lbxtbl[file_id].mode == LBX_MODE_NONE) && (lbxfile_load(file_id))) {
        goto fail;
    }
    return lbxtbl[file_id].entries;
    fail:
    return -1;
}

const char *lbxfile_item_name(lbxfile_e file_id, uint16_t entry_id)
{
    if ((lbxtbl[file_id].mode == LBX_MODE_NONE) && (lbxfile_load(file_id))) {
        goto fail;
    }
    {
        struct lbx_s *p;
        p = &lbxtbl[file_id];
        if (p->mode == LBX_MODE_FILE_OPEN) {
            if (fseek(p->fd, 0x200 + entry_id * 32, SEEK_SET)) {
                log_error("problem seeking to %i (of %i)\n", 0x200 + entry_id * 32, p->len);
                goto fail;
            }
            if (fread(entrynamebuf, 32, 1, p->fd) < 1) {
                log_error("problem reading file\n");
                goto fail;
            }
            return entrynamebuf;
        } else {
            return (const char *)&p->data[0x200 + entry_id * 32];
        }
    }
    fail:
    return NULL;
}

int lbxfile_item_len(lbxfile_e file_id, uint16_t entry_id)
{
    if ((lbxtbl[file_id].mode == LBX_MODE_NONE) && (lbxfile_load(file_id))) {
        return -1;
    }
    {
        uint32_t offs0, offs1, len;
        struct lbx_s *p;
        p = &lbxtbl[file_id];
        offs0 = GET_LE_32(&p->header[8 + entry_id * 4]);
        offs1 = GET_LE_32(&p->header[8 + entry_id * 4 + 4]);
        len = offs1 - offs0;
        return len;
    }
}

int lbxfile_item_offs(lbxfile_e file_id, uint16_t entry_id)
{
    if ((lbxtbl[file_id].mode == LBX_MODE_NONE) && (lbxfile_load(file_id))) {
        return -1;
    }
    {
        uint32_t offs0;
        struct lbx_s *p;
        p = &lbxtbl[file_id];
        offs0 = GET_LE_32(&p->header[8 + entry_id * 4]);
        return offs0;
    }
}

void lbxfile_add_patch(lbxfile_e file_id, uint16_t i, uint8_t *data, uint32_t len, const char *patchfilename)
{
    struct lbxpatch_s *p, *pnew, *q = NULL;
    struct lbx_s *lbx = &lbxtbl[file_id];
    pnew = lib_malloc(sizeof(struct lbxpatch_s));
    pnew->next = NULL;
    pnew->data = data;
    pnew->len = len;
    pnew->i = i;
    p = lbx->patches;
    while (p && (i > p->i)) {
        q = p;
        p = p->next;
    }
    if (p && (i == p->i)) {
        log_warning("replacing patch on %s item %i with one from '%s'\n", lbxinfo[file_id].filename, i, patchfilename);
        if (!q) {
            lbx->patches = pnew;
            pnew->next = p->next;
        } else {
            q->next = pnew;
            pnew->next = p->next;
        }
        lib_free(p);
    } else {
        if (!q) {
            lbx->patches = pnew;
            pnew->next = p;
        } else {
            q->next = pnew;
            pnew->next = p;
        }
    }
}

void lbxfile_add_overwrite(lbxfile_e file_id, uint16_t i, uint32_t itemoffs, uint8_t *data, uint32_t len, const char *patchfilename)
{
    struct lbxpatch_s *p, *pnew, *q = NULL;
    struct lbx_s *lbx = &lbxtbl[file_id];
    pnew = lib_malloc(sizeof(struct lbxpatch_s));
    pnew->next = NULL;
    pnew->data = data;
    pnew->len = len;
    pnew->offs = itemoffs;
    pnew->i = i;
    p = lbx->overwrites;
    while (p && (i >= p->i)) {
        q = p;
        p = p->next;
    }
    if (!q) {
        lbx->overwrites = pnew;
        pnew->next = p;
    } else {
        q->next = pnew;
        pnew->next = p;
    }
}
