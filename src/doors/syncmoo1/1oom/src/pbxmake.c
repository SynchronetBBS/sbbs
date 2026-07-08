#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pbx.h"
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

struct patch_item_s {
    struct patch_item_s *next;
    uint16_t itemtype;
    uint16_t itemindex;
    uint32_t itemoffs;
    bool add_offset;
    const char *itemid;
    enum {
        PARAM_FILENAME,
        PARAM_DATA_STR,
        PARAM_DATA_NUM,
    } param_type;
    char *fname;    /* or data (str) */
    char *fullname; /* or data (num) */
    uint32_t len;
    uint32_t pad;
    uint32_t offs;
};

/* -------------------------------------------------------------------------- */

static bool get_file_len(const char *filename, uint32_t *len_out)
{
    FILE *fd = NULL;
    uint32_t len = 0;
    if ((fd = fopen(filename, "rb")) == NULL) {
        perror(filename);
        goto fail;
    }
    if (fseek(fd, 0, SEEK_END) != 0) {
        perror(filename);
        goto fail;
    }
    len = ftell(fd);
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    *len_out = len;
    return true;
fail:
    *len_out = 0;
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    return false;
}

static bool copy_file(const char *filename, uint32_t len_total, FILE *fd_out)
{
#define BUFSIZE 4096
    FILE *fd_in = NULL;
    uint8_t *data = NULL;
    bool flag_ok = false;
    if ((fd_in = fopen(filename, "rb")) == NULL) {
        perror(filename);
        goto fail;
    }
    data = lib_malloc(BUFSIZE);
    while (len_total) {
        uint32_t len;
        len = (len_total > BUFSIZE) ? BUFSIZE : len_total;
        if (fread(data, len, 1, fd_in) < 1) {
            log_error("%s: read error\n", filename);
            goto fail;
        }
        if (fwrite(data, len, 1, fd_out) < 1) {
            log_error("write error\n");
            goto fail;
        }
        len_total -= len;
    }
    flag_ok = true;
fail:
    lib_free(data);
    data = NULL;
    if (fd_in) {
        fclose(fd_in);
        fd_in = NULL;
    }
    return flag_ok;
#undef BUFSIZE
}

static bool isnl(char c)
{
    return (c == '\r') || (c == '\n');
}

static bool isws(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static bool iswsnotnl(char c)
{
    return (c == ' ') || (c == '\t');
}

static int skipws(const char *str, int i, int len)
{
    while ((i < len) && isws(str[i])) {
        ++i;
    }
    return i;
}

static int skipwsnotnl(const char *str, int i, int len)
{
    while ((i < len) && iswsnotnl(str[i])) {
        ++i;
    }
    return i;
}

static int findsep(char *str, int i, int len)
{
    while ((i < len) && (str[i] != SEP_CHAR) && !isnl(str[i])) {
        ++i;
    }
    if ((i >= len) || isnl(str[i]) || (str[i] != SEP_CHAR)) {
        log_error("partial line near byte %i\n", i);
        return -1;
    }
    str[i++] = '\0';
    return skipwsnotnl(str, i, len);
}

static int make_pbx(const char *filename_in, const char *filename_out)
{
    struct patch_item_s *patches = NULL, *tail = NULL;
    int i, num_patches = 0;
    uint32_t len = 0;
    char *dir = NULL, *fullname = NULL;
    char *listfiledata = (char *)util_file_load(filename_in, &len);
    if (!listfiledata) {
        return 2;
    }
    util_fname_split(filename_in, &dir, NULL);
    i = 0;
    while ((i < len) && (listfiledata[i] != '\0')) {
        i = skipws(listfiledata, i, len);
        if (listfiledata[i] == '\0') {
            break;
        } else if (listfiledata[i] == '#') {
            while ((i < len) && !isnl(listfiledata[i])) {
                ++i;
            }
        } else {
            struct patch_item_s *n;
            int positemtype, positemid, positemindex, positemoffs, posparam;
            uint32_t v, flen;
            bool need_index, need_id, need_offs;
            pbx_item_type_t pbxitype;
            uint16_t item_index;
            uint32_t item_offs;
            const char *itemid;
            int32_t *numtbl;
            int numnum;

            positemtype = i;
            if ((i = findsep(listfiledata, i, len)) < 0) {
                return 3;
            }
            if (!util_parse_number(&listfiledata[positemtype], &v)) {
                log_error("error: invalid type number '%s' at %i\n", &listfiledata[positemtype], positemtype);
                return 3;
            }

            switch (v) {
                case 0: pbxitype = PBX_ITEM_TYPE_NAME; need_index = false; need_id = false; need_offs = false; itemid = "PBX NAME"; break;
                case 1: pbxitype = PBX_ITEM_TYPE_DESC; need_index = false; need_id = false; need_offs = false; itemid = "PBX DESC"; break;
                case 2: pbxitype = PBX_ITEM_TYPE_LBXP; need_index = true; need_id = true; need_offs = false; itemid = NULL; break;
                case 3: pbxitype = PBX_ITEM_TYPE_STRP; need_index = true; need_id = true; need_offs = false; itemid = NULL; break;
                case 4: pbxitype = PBX_ITEM_TYPE_NUMP; need_index = true; need_id = true; need_offs = false; itemid = NULL; break;
                case 5: pbxitype = PBX_ITEM_TYPE_LBXO; need_index = true; need_id = true; need_offs = true; itemid = NULL; break;
                default:
                    log_error("error: invalid type number %u at %i\n", v, positemtype);
                    return 3;
            }
            if (need_id) {
                int ilen;
                positemid = i;
                if ((i = findsep(listfiledata, i, len)) < 0) {
                    return 3;
                }
                util_trim_whitespace(&listfiledata[positemid], len - positemid);
                itemid = &listfiledata[positemid];
                ilen = strlen(itemid);
                if (ilen > PBX_ITEM_ID_LEN) {
                    log_error("error: too long item id string '%s' (%i > %i) at %i\n", itemid, ilen, PBX_ITEM_ID_LEN, positemid);
                    return 3;
                }
            }

            if (need_index) {
                positemindex = i;
                if ((i = findsep(listfiledata, i, len)) < 0) {
                    return 3;
                }
                util_trim_whitespace(&listfiledata[positemindex], len - positemindex);
                if (!util_parse_number(&listfiledata[positemindex], &v)) {
                    log_error("error: invalid number '%s' at %i\n", &listfiledata[positemindex], i);
                    return 3;
                }
                item_index = v;
            } else {
                item_index = 0;
            }

            if (need_offs) {
                positemoffs = i;
                if ((i = findsep(listfiledata, i, len)) < 0) {
                    return 3;
                }
                util_trim_whitespace(&listfiledata[positemoffs], len - positemoffs);
                if (!util_parse_number(&listfiledata[positemoffs], &v)) {
                    log_error("error: invalid number '%s' at %i\n", &listfiledata[positemoffs], i);
                    return 3;
                }
                item_offs = v;
            } else {
                item_offs = 0;
            }

            posparam = i;
            while ((i < len) && !isnl(listfiledata[i])) {
                ++i;
            }
            if ((i >= len) || !isnl(listfiledata[i])) {
                log_error("error: partial line near byte %i\n", i);
                return 3;
            }
            listfiledata[i] = '\0';
            ++i;
            util_trim_whitespace(&listfiledata[posparam], len - posparam);

            if (fullname) {
                lib_free(fullname);
                fullname = NULL;
            }

            n = lib_malloc(sizeof(struct patch_item_s));
            n->itemtype = pbxitype;
            n->itemid = itemid;
            n->itemindex = item_index;
            n->itemoffs = item_offs;
            n->add_offset = need_offs;
            n->next = NULL;
            n->fname = &listfiledata[posparam];

            if (n->fname[0] == '"') {
                n->param_type = PARAM_DATA_STR;
                fullname = NULL;
                if (util_cstr_parse(n->fname, n->fname, &flen) < 0) {
                    return 4;
                }
            } else if ((numtbl = util_parse_numbers(n->fname, SEP_CHAR, &numnum)) != NULL) {
                uint8_t *data;
                n->param_type = PARAM_DATA_NUM;
                flen = numnum * 4/*sizeof(int32_t)*/;
                data = lib_malloc(flen);
                fullname = (char *)data;
                for (int j = 0; j < numnum; ++j) {
                    SET_LE_32(data, numtbl[j]);
                    data += 4/*sizeof(int32_t)*/;
                }
                lib_free(numtbl);
                numtbl = NULL;
            } else {
                n->param_type = PARAM_FILENAME;
                if (dir && *dir) {
                    fullname = util_concat(dir, FSDEV_DIR_SEP_STR, n->fname, NULL);
                } else {
                    fullname = lib_stralloc(n->fname);
                }
                if (!get_file_len(fullname, &flen)) {
                    return 4;
                }
            }
            n->fullname = fullname;
            fullname = NULL;
            n->len = flen;
            n->pad = "\0\03\02\01"[flen & 0x3];
            if (tail == NULL) {
                patches = n;
            } else {
                tail->next = n;
            }
            tail = n;
            ++num_patches;
        }
    }

    if (num_patches) {
        const uint8_t pad[3] = { 0, 0, 0 };
        uint8_t buf[PBX_ITEM_HEADER_LEN + 4];
        FILE *fd;
        struct patch_item_s *p, *q;
        uint32_t offs = PBX_HEADER_LEN + (num_patches + 1) * 4;
        p = patches;
        while (p) {
            p->offs = offs;
            offs += PBX_ITEM_HEADER_LEN + p->len + p->pad;
            if (p->add_offset) {
                offs += 4;
            }
            p = p->next;
        }
        len = offs;
        fd = fopen(filename_out, "wb");
        if (!fd) {
            perror(filename_out);
            return 4;
        }
        memset(buf, 0, sizeof(buf));
        memcpy(buf, PBX_MAGIC, 8);
        SET_LE_32(&buf[PBX_OFFS_VERSION], PBX_VERSION);
        SET_LE_32(&buf[PBX_OFFS_ITEMS], num_patches);
        if (fwrite(buf, PBX_HEADER_LEN, 1, fd) != 1) {
            perror(filename_out);
            fclose(fd);
            return 3;
        }
        p = patches;
        while (p) {
            SET_LE_32(buf, p->offs);
            if (fwrite(buf, 4, 1, fd) != 1) {
                perror(filename_out);
                fclose(fd);
                return 3;
            }
            p = p->next;
        }
        SET_LE_32(buf, len);
        if (fwrite(buf, 4, 1, fd) != 1) {
            perror(filename_out);
            fclose(fd);
            return 3;
        }
        p = patches;
        i = 0;
        while (p) {
            uint32_t hlen, tlen;
            memset(buf, 0, sizeof(buf));
            hlen = PBX_ITEM_HEADER_LEN;
            tlen = p->len;
            if (p->add_offset) {
                hlen += 4;
                tlen += 4;
                SET_LE_32(&buf[PBX_OFFS_ITEM_OFFS], p->itemoffs);
            }
            SET_LE_32(&buf[PBX_OFFS_ITEM_LEN], tlen);
            SET_LE_16(&buf[PBX_OFFS_ITEM_TYPE], p->itemtype);
            SET_LE_16(&buf[PBX_OFFS_ITEM_INDEX], p->itemindex);
            lib_strcpy((char *)&buf[PBX_OFFS_ITEM_ID], p->itemid, sizeof(buf) - PBX_OFFS_ITEM_ID);
            if (0
              || (fwrite(buf, hlen, 1, fd) != 1)
              || ((p->param_type == PARAM_FILENAME) && (!copy_file(p->fullname, p->len, fd)))
              || ((p->param_type == PARAM_DATA_STR) && (fwrite(p->fname, p->len, 1, fd) != 1))
              || ((p->param_type == PARAM_DATA_NUM) && (fwrite(p->fullname, p->len, 1, fd) != 1))
              || ((p->pad != 0) && (fwrite(pad, p->pad, 1, fd) != 1))
            ) {
                log_error("writing file '%s'", filename_out);
                fclose(fd);
                return 3;
            }
            LOG_DEBUG((0, "%i offs 0x%x: type %i '%s' index %i offset 0x%x %s '%s' len %i+%i+%i+%i\n", i, p->offs, p->itemtype, p->itemid, p->itemindex, p->itemoffs, (p->param_type == PARAM_FILENAME) ? "file" : "data", p->fname, PBX_ITEM_HEADER_LEN, p->add_offset ? 4 : 0, p->len, p->pad));
            p = p->next;
            ++i;
        }
        fclose(fd);
        p = patches;
        q = NULL;
        while (p) {
            q = p->next;
            lib_free(p->fullname);
            lib_free(p);
            p = q;
        }
    }

    lib_free(listfiledata);
    lib_free(dir);
    return 0;
}

/* -------------------------------------------------------------------------- */

int main_1oom(int argc, char **argv)
{
    const char *filename_in;
    const char *filename_out;
    int i;
    if (argc != 3) {
        fprintf(stderr, "Usage: 1oom_pbxmake PBXIN.TXT OUT.PBX\nPBXIN.TXT format:\n"
                        "# comment\n"
                        "0,\"pbx name\"\n"
                        "1,\"pbx description\"\n"
                        "2,filename.lbx,itemnumber,replacementfile.bin\n"
                        "5,filename.lbx,itemnumber,offset,partialdata.bin\n"
                        "3,string_id,itemnumber,\"new text\"\n"
                        "4,number_id,itemnumber,value[,value]*\n"
               );
        return 1;
    }
    i = 1;
    filename_in = argv[i++];
    filename_out = argv[i++];
    return make_pbx(filename_in, filename_out);
}
