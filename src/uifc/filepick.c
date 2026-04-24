#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "dirwrap.h"
#include "genwrap.h"
#include "uifc.h"
#include "ciolib.h"

#include "filepick.h"

/* ------------------------------------------------------------------ */
/* Internal types                                                     */
/* ------------------------------------------------------------------ */

enum fp_mode {
	FP_MODE_PICK,    /* single file */
	FP_MODE_DIRSEL,  /* single directory */
	FP_MODE_MULTI    /* zero or more files, possibly cross-directory */
};

enum {
	FIELD_DIR = 0,
	FIELD_FILE,
	FIELD_MASK,
	FIELD_PATH,
	FIELD_EDIT,    /* "Edit (F2)" button — MULTI mode only */
	FIELD_OK,
	FIELD_CANCEL,
	FIELD_COUNT
};

struct fp_entry {
	char *display;   /* what api->list() shows; first char is the selection marker */
	char *name;      /* raw filename, as returned by glob (no marker prefix) */
	int   is_dir;
};

struct fp_state {
	uifcapi_t   *api;
	enum fp_mode mode;
	int          opts;
	const char  *title;

	char         path [MAX_PATH * 4 + 1];   /* current directory, trailing slash */
	char         mask [MAX_PATH * 4 + 1];   /* current glob mask */
	char         entry[MAX_PATH * 8 + 1];   /* CURRENT_PATH input field text */
	char        *prev_path;                 /* fallback for unreadable directory */

	struct fp_entry *dirs;
	size_t           dir_count;
	char           **dir_display;
	int              dir_cur, dir_bar;

	struct fp_entry *files;
	size_t           file_count;
	char           **file_display;
	int              file_cur, file_bar;

	char           **selected;              /* MULTI: full paths of tagged files */
	size_t           sel_count;
	size_t           sel_cap;

	int              field;
	int              at_root;
	int              reread;
	int              finished;
	int              retval;
	int              initial_paint;     /* paint both panes once after a refresh */
	int              last_active_pane;  /* tracks focus transitions: FIELD_DIR/FIELD_FILE/-1 */

	/* Cached layout (recomputed once at startup) */
	int              width, height;
	int              list_width, list_height;
	int              row_lists;     /* top row of list panes (relative to SCRN_TOP) */
	int              row_info;      /* file-info pane top */
	int              row_mask;
	int              row_path;
	int              row_buttons;

	/* Saved console state for cleanup */
	int              saved_hold;
	int              saved_x, saved_y;
	uchar            saved_lbclr;
	char            *saved_edit_item;
};

/* Forward declarations for helpers called before their definition. */
static void fp_clamp_one(int count, int list_height, int *cur, int *bar);
static int  fp_activate_ok(struct fp_state *s, struct file_pick *fp, int back_to_field);

/* ------------------------------------------------------------------ */
/* Selection marker                                                   */
/* ------------------------------------------------------------------ */

static char fp_sel_marker(const struct fp_state *s)
{
	if (s->api->chars && s->api->chars->right_arrow)
		return s->api->chars->right_arrow;
	return '*';
}

static void fp_set_marker(struct fp_state *s, struct fp_entry *e, int selected)
{
	if (e->display == NULL)
		return;
	e->display[0] = selected ? fp_sel_marker(s) : ' ';
}

/* ------------------------------------------------------------------ */
/* Selection list (MULTI mode)                                        */
/* ------------------------------------------------------------------ */

static int sel_index_of(const struct fp_state *s, const char *full_path)
{
	size_t i;

	for (i = 0; i < s->sel_count; i++) {
		if (strcmp(s->selected[i], full_path) == 0)
			return (int)i;
	}
	return -1;
}

static int sel_grow(struct fp_state *s)
{
	size_t new_cap = s->sel_cap ? s->sel_cap * 2 : 16;
	char **n = realloc(s->selected, sizeof(char *) * (new_cap + 1));

	if (n == NULL)
		return -1;
	s->selected = n;
	s->sel_cap = new_cap;
	return 0;
}

static int sel_add(struct fp_state *s, const char *full_path)
{
	char *dup;

	if (sel_index_of(s, full_path) >= 0)
		return 0;
	if (s->sel_count + 1 > s->sel_cap) {
		if (sel_grow(s) < 0)
			return -1;
	}
	dup = strdup(full_path);
	if (dup == NULL)
		return -1;
	s->selected[s->sel_count++] = dup;
	s->selected[s->sel_count] = NULL;
	return 0;
}

static void sel_remove_at(struct fp_state *s, size_t idx)
{
	if (idx >= s->sel_count)
		return;
	free(s->selected[idx]);
	memmove(&s->selected[idx], &s->selected[idx + 1],
	    (s->sel_count - idx - 1) * sizeof(char *));
	s->sel_count--;
	s->selected[s->sel_count] = NULL;
}

static int sel_remove(struct fp_state *s, const char *full_path)
{
	int idx = sel_index_of(s, full_path);

	if (idx < 0)
		return -1;
	sel_remove_at(s, (size_t)idx);
	return 0;
}

/* Refresh the selection markers on the visible file list.  Called after
 * any change to s->selected or after a directory refresh. */
static void fp_refresh_markers(struct fp_state *s)
{
	char full[MAX_PATH * 8 + 1];
	size_t i;

	if (s->mode != FP_MODE_MULTI || s->files == NULL)
		return;
	for (i = 0; i < s->file_count; i++) {
		snprintf(full, sizeof(full), "%s%s", s->path, s->files[i].name);
		fp_set_marker(s, &s->files[i], sel_index_of(s, full) >= 0);
	}
}

/* Tag every file in the currently-visible file pane.  Directories
 * (which live in s->dirs) are never selectable, so we only iterate
 * s->files.  sel_add() is idempotent, so this is safe to invoke when
 * some or all entries are already tagged. */
static void fp_select_all(struct fp_state *s)
{
	char full[MAX_PATH * 8 + 1];
	size_t i;

	if (s->mode != FP_MODE_MULTI || s->files == NULL)
		return;
	for (i = 0; i < s->file_count; i++) {
		snprintf(full, sizeof(full), "%s%s", s->path, s->files[i].name);
		sel_add(s, full);
	}
	fp_refresh_markers(s);
}

/* ------------------------------------------------------------------ */
/* Sort comparators                                                   */
/* ------------------------------------------------------------------ */

static int cmp_entry_ci(const void *a, const void *b)
{
	const struct fp_entry *ea = a;
	const struct fp_entry *eb = b;
	int rc = stricmp(ea->name, eb->name);

	if (rc != 0)
		return rc;
	return strcmp(ea->name, eb->name);
}

static int cmp_entry_cs(const void *a, const void *b)
{
	const struct fp_entry *ea = a;
	const struct fp_entry *eb = b;

	return strcmp(ea->name, eb->name);
}

/* ------------------------------------------------------------------ */
/* Glob with optional hidden-file inclusion                           */
/* ------------------------------------------------------------------ */

/* Append the gl_pathv of src into dst.  Both must be valid glob_t.
 * Frees src->gl_pathv after transferring ownership of strdup'd entries. */
static int glob_merge(glob_t *dst, glob_t *src)
{
	size_t i;
	size_t need = dst->gl_pathc + src->gl_pathc + 1;
	char **n = realloc(dst->gl_pathv, need * sizeof(char *));

	if (n == NULL)
		return -1;
	dst->gl_pathv = n;
	for (i = 0; i < src->gl_pathc; i++) {
		const char *base = strrchr(src->gl_pathv[i], '/');
		const char *base2 = strrchr(src->gl_pathv[i], '\\');
		const char *bn = base > base2 ? base + 1 : (base2 ? base2 + 1 : src->gl_pathv[i]);

		/* Filter out . and .. (and the slash-marked variants) returned by `.*` */
		if (strcmp(bn, ".") == 0 || strcmp(bn, "..") == 0
		    || strcmp(bn, "./") == 0 || strcmp(bn, "../") == 0
		    || strcmp(bn, ".\\") == 0 || strcmp(bn, "..\\") == 0) {
			continue;
		}
		dst->gl_pathv[dst->gl_pathc++] = strdup(src->gl_pathv[i]);
	}
	dst->gl_pathv[dst->gl_pathc] = NULL;
	return 0;
}

static int fp_glob_call(bool case_sensitive, const char *pattern, int flags, glob_t *out)
{
	if (case_sensitive)
		return glob(pattern, flags, NULL, out);
	return globi(pattern, flags, NULL, out);
}

static int fp_do_glob(const struct fp_state *s, const char *mask, int dir_glob, glob_t *out)
{
	char pattern[MAX_PATH * 4 + 2];
	int  rc;
	int  flags = dir_glob ? GLOB_MARK : 0;
	bool case_sensitive = (s->opts & UIFC_FP_MSKCASE) != 0;

	memset(out, 0, sizeof(*out));
	snprintf(pattern, sizeof(pattern), "%s%s", s->path, mask);
	rc = fp_glob_call(case_sensitive, pattern, flags, out);
	if (rc != 0 && rc != GLOB_NOMATCH) {
		out->gl_pathc = 0;
		return -1;
	}

	if (s->opts & UIFC_FP_SHOWHIDDEN) {
		glob_t hidden;

		memset(&hidden, 0, sizeof(hidden));
		snprintf(pattern, sizeof(pattern), "%s.%s", s->path, mask);
		if (fp_glob_call(case_sensitive, pattern, flags, &hidden) == 0)
			glob_merge(out, &hidden);
		globfree(&hidden);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Build fp_entry arrays                                              */
/* ------------------------------------------------------------------ */

static void fp_entry_free(struct fp_entry *e)
{
	if (e == NULL)
		return;
	FREE_AND_NULL(e->display);
	FREE_AND_NULL(e->name);
}

static void fp_entries_free(struct fp_entry **arr, size_t *count)
{
	size_t i;

	if (*arr == NULL) {
		*count = 0;
		return;
	}
	for (i = 0; i < *count; i++)
		fp_entry_free(&(*arr)[i]);
	free(*arr);
	*arr = NULL;
	*count = 0;
}

static void fp_display_free(char ***disp)
{
	if (*disp == NULL)
		return;
	free(*disp);
	*disp = NULL;
}

/* Build the NULL/empty-terminated char** view for api->list(). */
static char **fp_build_display(struct fp_entry *entries, size_t count)
{
	char **arr = malloc((count + 2) * sizeof(char *));
	size_t i;

	if (arr == NULL)
		return NULL;
	for (i = 0; i < count; i++)
		arr[i] = entries[i].display;
	arr[count] = "";
	arr[count + 1] = NULL;
	return arr;
}

/* Allocate one fp_entry from a glob path.  display gets a leading-space prefix
 * so that the multi-select marker can later flip it without resizing. */
static int fp_entry_init(struct fp_entry *e, const char *bare_name, int is_dir)
{
	size_t len = strlen(bare_name);

	e->name = strdup(bare_name);
	e->display = malloc(len + 2);
	if (e->name == NULL || e->display == NULL) {
		fp_entry_free(e);
		return -1;
	}
	e->display[0] = ' ';
	memcpy(e->display + 1, bare_name, len + 1);
	e->is_dir = is_dir;
	return 0;
}

/* Take a glob_t and split into our fp_entry array(s).  When dir_glob is true
 * we emit only directory entries (and the ".." sentinel if not at root); when
 * false we emit only file entries. */
static int fp_entries_from_glob(struct fp_state *s, glob_t *gl, int dir_glob,
                                struct fp_entry **out_arr, size_t *out_count)
{
	struct fp_entry *arr;
	size_t cap;
	size_t n = 0;
	size_t i;
	size_t sort_start;

	cap = gl->gl_pathc + 2;
	arr = calloc(cap, sizeof(*arr));
	if (arr == NULL)
		goto fail;

	if (dir_glob && !s->at_root) {
		if (fp_entry_init(&arr[n], "..", 1) < 0)
			goto fail;
		n++;
	}

	for (i = 0; i < gl->gl_pathc; i++) {
		const char *p = gl->gl_pathv[i];
		const char *bn;
		int isd = isdir(p);
		size_t plen;
		char trimmed[MAX_PATH + 1];

		if (dir_glob && !isd)
			continue;
		if (!dir_glob && isd)
			continue;

		bn = getfname(p);
		if (bn == NULL || *bn == 0) {
			/* GLOB_MARK appended a slash; strip it for display/storage. */
			SAFECOPY(trimmed, p);
			plen = strlen(trimmed);
			while (plen > 0 && (trimmed[plen - 1] == '/' || trimmed[plen - 1] == '\\'))
				trimmed[--plen] = 0;
			bn = getfname(trimmed);
			if (bn == NULL || *bn == 0)
				bn = trimmed;
		}

		if (fp_entry_init(&arr[n], bn, isd) < 0)
			goto fail;
		n++;
	}

	/* Sort entries by name; pin ".." to the front when present. */
	sort_start = (dir_glob && !s->at_root && n > 0
	              && strcmp(arr[0].name, "..") == 0) ? 1 : 0;
	if (n > sort_start) {
		qsort(arr + sort_start, n - sort_start, sizeof(struct fp_entry),
		    (s->opts & UIFC_FP_UNIXSORT) ? cmp_entry_cs : cmp_entry_ci);
	}

	*out_arr = arr;
	*out_count = n;
	return 0;

fail:
	if (arr) {
		for (i = 0; i < n; i++)
			fp_entry_free(&arr[i]);
		free(arr);
	}
	*out_arr = NULL;
	*out_count = 0;
	return -1;
}

/* ------------------------------------------------------------------ */
/* Win32 root (drive list)                                            */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
static int fp_build_drive_list(struct fp_state *s, struct fp_entry **out_arr, size_t *out_count)
{
	struct fp_entry *arr;
	size_t n = 0;
	unsigned long drives = _getdrives();
	int j;
	char path[4];

	arr = calloc('Z' - 'A' + 2, sizeof(*arr));
	if (arr == NULL)
		return -1;

	strcpy(path, "A:\\");
	for (j = 0; j <= 'Z' - 'A'; j++) {
		if (drives & (1ul << j)) {
			path[0] = (char)('A' + j);
			if (fp_entry_init(&arr[n], path, 1) < 0) {
				while (n > 0)
					fp_entry_free(&arr[--n]);
				free(arr);
				return -1;
			}
			n++;
		}
	}
	(void)s;
	*out_arr = arr;
	*out_count = n;
	return 0;
}
#endif

/* ------------------------------------------------------------------ */
/* Path normalization                                                 */
/* ------------------------------------------------------------------ */

static void fp_update_at_root(struct fp_state *s)
{
#ifdef __unix__
	s->at_root = (s->path[0] == '/' && s->path[1] == 0);
#elif defined(_WIN32)
	s->at_root = (strcmp(s->path, "\\\\?\\") == 0);
#else
	s->at_root = 0;
#endif
}

static void fp_normalize_path(struct fp_state *s)
{
	char tmp[sizeof(s->path)];

	SAFECOPY(tmp, s->path);

#ifdef _WIN32
	/* "C:\..\" at top level → drive root marker.  Keep \\?\ intact. */
	if (tmp[0] == 0
	    || (tmp[5] == 0 && tmp[3] == '.' && tmp[4] == '.'
	        && tmp[1] == ':' && IS_PATH_DELIM(tmp[2]))) {
		strcpy(s->path, "\\\\?\\");
	}
	else if (strncmp(tmp, "\\\\?\\", 4) == 0 && tmp[4]) {
		strcpy(s->path, tmp + 4);
	}
	else
#endif
	{
		FULLPATH(s->path, tmp, sizeof(s->path));
	}

#ifdef __unix__
	if (s->path[0] == 0) {
		s->path[0] = '/';
		s->path[1] = 0;
	}
#endif
	backslash(s->path);
	fp_update_at_root(s);
}

/* ------------------------------------------------------------------ */
/* Truncated field rendering                                          */
/* ------------------------------------------------------------------ */

/* Render a string into a fixed-width field at (x,y).  When the string is
 * too long to fit, drop characters from one end and replace them with
 * "...", which is drawn in alt_attr so the reader can see at a glance
 * that the ellipsis is rendering chrome rather than literal data.
 *
 * One trailing column is always reserved as padding so the text never
 * butts up against the right border.
 *
 * trunc_right=1 → keep prefix, "..." at the end (good for masks).
 * trunc_right=0 → keep suffix, "..." at the start (good for paths). */
static void draw_truncated_field(uifcapi_t *api, int x, int y, int field_width,
                                 const char *full, int trunc_right,
                                 int normal_attr, int alt_attr)
{
	char   shown[MAX_PATH * 8 + 4];
	size_t len = strlen(full);
	int    max_chars = field_width - 1;
	int    truncated = 0;

	if (max_chars < 0)
		max_chars = 0;
	if ((int)len > max_chars) {
		truncated = 1;
		if (max_chars <= 3) {
			/* Field too narrow for "...": just clip to whichever end
			 * the caller cares about, no ellipsis. */
			if (trunc_right)
				memcpy(shown, full, (size_t)max_chars);
			else
				memcpy(shown, full + (len - max_chars), (size_t)max_chars);
			shown[max_chars] = 0;
			truncated = 0;
		}
		else if (trunc_right) {
			int keep = max_chars - 3;
			memcpy(shown, full, (size_t)keep);
			memcpy(shown + keep, "...", 4);
		}
		else {
			int keep = max_chars - 3;
			memcpy(shown, "...", 3);
			memcpy(shown + 3, full + (len - keep), (size_t)keep + 1);
		}
	}
	else {
		memcpy(shown, full, len + 1);
	}

	api->printf(x, y, normal_attr, "%-*s", field_width, shown);
	if (truncated) {
		if (trunc_right)
			api->printf(x + max_chars - 3, y, alt_attr, "...");
		else
			api->printf(x, y, alt_attr, "...");
	}
}

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */

static void fp_compute_layout(struct fp_state *s)
{
	uifcapi_t *api = s->api;
	int        has_path = (s->opts & UIFC_FP_ALLOWENTRY) ? 1 : 0;
	int        width = SCRN_RIGHT - SCRN_LEFT + 1;

	if (!(width % 2))
		width--;
	s->width  = width;
	s->height = s->api->scrn_len - 3;

	s->list_width = (width - 3) / 2;

	/* Row layout (relative to SCRN_TOP), bottom-up:
	 *   height-1   bottom border
	 *   height-2   buttons
	 *   height-3   button separator
	 *   height-4   path field            (if has_path)
	 *   height-5   mask field
	 *   height-6   info-pane line 2
	 *   height-7   info-pane line 1
	 *   height-8   info / fields separator
	 *   3..n       list panes
	 *
	 * The info pane and the mask/path pane share one bordered region with
	 * no separator between them.
	 */
	s->row_buttons = s->height - 2;
	s->row_path    = has_path ? s->height - 4 : -1;
	s->row_mask    = has_path ? s->height - 5 : s->height - 4;
	s->row_info    = s->row_mask - 2;            /* line 1 */
	s->row_lists   = 3;
	s->list_height = s->row_info - 1 - s->row_lists;
	if (s->list_height < 3)
		s->list_height = 3;
	s->api->list_height = s->list_height;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                            */
/* ------------------------------------------------------------------ */

static void fp_draw_chrome(struct fp_state *s)
{
	uifcapi_t *api = s->api;
	struct vmem_cell lbuf[512];
	struct vmem_cell shade[512];
	int  i, j;
	int  width  = s->width;
	int  height = s->height;
	int  attr_hb = api->hclr | (api->bclr << 4);
	int  attr_lb = api->lclr | (api->bclr << 4);

	/* Top border with mouse buttons */
	i = 0;
	set_vmem(&lbuf[i++], '\xc9', attr_hb, 0);
	for (j = 1; j < width - 1; j++)
		set_vmem(&lbuf[i++], '\xcd', attr_hb, 0);
	if (api->mode & UIFC_MOUSE && width > 6) {
		set_vmem(&lbuf[1], '[', attr_hb, 0);
		set_vmem(&lbuf[2], '\xfe', attr_lb, 0);
		set_vmem(&lbuf[3], ']', attr_hb, 0);
		set_vmem(&lbuf[4], '[', attr_hb, 0);
		set_vmem(&lbuf[5], '?', attr_lb, 0);
		set_vmem(&lbuf[6], ']', attr_hb, 0);
		api->buttony   = SCRN_TOP;
		api->exitstart = SCRN_LEFT + 1;
		api->exitend   = SCRN_LEFT + 3;
		api->helpstart = SCRN_LEFT + 4;
		api->helpend   = SCRN_LEFT + 6;
	}
	set_vmem(&lbuf[i++], '\xbb', attr_hb, 0);
	vmem_puttext(SCRN_LEFT, SCRN_TOP, SCRN_LEFT + width - 1, SCRN_TOP, lbuf);

	/* Bottom border */
	for (j = 1; j < 7; j++)
		lbuf[j].ch = '\xcd';
	set_vmem_attr(&lbuf[2], attr_hb);
	set_vmem_attr(&lbuf[5], attr_hb);
	lbuf[0].ch = '\xc8';
	lbuf[width - 1].ch = '\xbc';
	vmem_puttext(SCRN_LEFT, SCRN_TOP + height - 1,
	    SCRN_LEFT + width - 1, SCRN_TOP + height - 1, lbuf);

	/* Title bar separator (T-up junction at split) */
	lbuf[0].ch = '\xcc';
	lbuf[width - 1].ch = '\xb9';
	lbuf[(width - 1) / 2].ch = '\xcb';
	vmem_puttext(SCRN_LEFT, SCRN_TOP + 2,
	    SCRN_LEFT + width - 1, SCRN_TOP + 2, lbuf);

	/* List/info separator (T-up junction closes the list divider) */
	lbuf[(width - 1) / 2].ch = '\xca';
	vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_info - 1,
	    SCRN_LEFT + width - 1, SCRN_TOP + s->row_info - 1, lbuf);

	/* Input / button separator (between Mask/Path and OK/Cancel) */
	lbuf[(width - 1) / 2].ch = '\xcd';
	vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_buttons - 1,
	    SCRN_LEFT + width - 1, SCRN_TOP + s->row_buttons - 1, lbuf);

	/* Build a generic interior row (vertical bars on each side, blank fill) */
	lbuf[0].ch = '\xba';
	lbuf[width - 1].ch = '\xba';
	for (j = 1; j < width - 1; j++)
		lbuf[j].ch = ' ';

	/* Title row */
	vmem_puttext(SCRN_LEFT, SCRN_TOP + 1,
	    SCRN_LEFT + width - 1, SCRN_TOP + 1, lbuf);

	/* Info-pane rows (2 lines) */
	vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_info,
	    SCRN_LEFT + width - 1, SCRN_TOP + s->row_info, lbuf);
	vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_info + 1,
	    SCRN_LEFT + width - 1, SCRN_TOP + s->row_info + 1, lbuf);

	/* Mask row */
	vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_mask,
	    SCRN_LEFT + width - 1, SCRN_TOP + s->row_mask, lbuf);

	/* Path row (only if shown) */
	if (s->row_path >= 0) {
		vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_path,
		    SCRN_LEFT + width - 1, SCRN_TOP + s->row_path, lbuf);
	}

	/* Buttons row */
	vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_buttons,
	    SCRN_LEFT + width - 1, SCRN_TOP + s->row_buttons, lbuf);

	/* Pane center divider for list rows */
	lbuf[(width - 1) / 2].ch = '\xba';
	for (j = 0; j < s->list_height; j++) {
		vmem_puttext(SCRN_LEFT, SCRN_TOP + s->row_lists + j,
		    SCRN_LEFT + width - 1, SCRN_TOP + s->row_lists + j, lbuf);
	}

	/* Drop shadow for blue background */
	if (api->bclr == BLUE) {
		vmem_gettext(SCRN_LEFT + width, SCRN_TOP + 1,
		    SCRN_LEFT + width + 1, SCRN_TOP + height - 1, shade);
		for (j = 0; j < 512; j++)
			set_vmem_attr(&shade[j], DARKGRAY);
		vmem_puttext(SCRN_LEFT + width, SCRN_TOP + 1,
		    SCRN_LEFT + width + 1, SCRN_TOP + height - 1, shade);
		vmem_gettext(SCRN_LEFT + 2, SCRN_TOP + height,
		    SCRN_LEFT + width + 1, SCRN_TOP + height, shade);
		for (j = 0; j < width; j++)
			set_vmem_attr(&shade[j], DARKGRAY);
		vmem_puttext(SCRN_LEFT + 2, SCRN_TOP + height,
		    SCRN_LEFT + width + 1, SCRN_TOP + height, shade);
	}
}

static void fp_draw_title(struct fp_state *s)
{
	uifcapi_t *api = s->api;
	int        i = (int)strlen(s->title);
	int        w = s->width - 4;

	if (i > w)
		i = w;
	api->printf(SCRN_LEFT + 2, SCRN_TOP + 1,
	    api->hclr | (api->bclr << 4),
	    "%*s%-*s", (w - i) / 2, "", i, s->title);
}

static void fp_draw_field_labels(struct fp_state *s)
{
	uifcapi_t *api = s->api;
	int        attr = api->hclr | (api->bclr << 4);

	api->printf(SCRN_LEFT + 2, SCRN_TOP + s->row_mask, attr, "Mask: ");
	if (s->row_path >= 0)
		api->printf(SCRN_LEFT + 2, SCRN_TOP + s->row_path, attr, "Path: ");
}

/* Compute screen-column rectangles for the footer buttons.  MULTI mode
 * has three buttons ([Edit (F2)], [OK], [Cancel]); other modes have
 * just [OK] and [Cancel].  Used by both the renderer and the mouse
 * hit-test so they always agree. */
static void fp_button_rects(const struct fp_state *s,
                            const char **edit_label_out, int *edit_x_out, int *edit_len_out,
                            const char **ok_label_out,   int *ok_x_out,   int *ok_len_out,
                            const char **cancel_label_out, int *cancel_x_out, int *cancel_len_out)
{
	uifcapi_t  *api = s->api;
	const char *edit_label   = "[ Review ]";
	const char *ok_label     = (s->mode == FP_MODE_DIRSEL) ? "[ Select ]" : "[   OK   ]";
	const char *cancel_label = "[ Cancel ]";
	int         gap = 4;
	int         ok_len = (int)strlen(ok_label);
	int         cancel_len = (int)strlen(cancel_label);
	int         edit_len = (int)strlen(edit_label);
	int         has_edit = (s->mode == FP_MODE_MULTI);
	int         total;
	int         x_edit = 0;
	int         x_ok;
	int         x_cancel;

	if (has_edit)
		total = edit_len + gap + ok_len + gap + cancel_len;
	else
		total = ok_len + gap + cancel_len;

	if (has_edit) {
		x_edit   = SCRN_LEFT + (s->width - total) / 2;
		x_ok     = x_edit + edit_len + gap;
	}
	else {
		x_ok     = SCRN_LEFT + (s->width - total) / 2;
	}
	x_cancel = x_ok + ok_len + gap;

	if (edit_label_out)   *edit_label_out   = has_edit ? edit_label : "";
	if (edit_x_out)       *edit_x_out       = x_edit;
	if (edit_len_out)     *edit_len_out     = has_edit ? edit_len : 0;
	if (ok_label_out)     *ok_label_out     = ok_label;
	if (ok_x_out)         *ok_x_out         = x_ok;
	if (ok_len_out)       *ok_len_out       = ok_len;
	if (cancel_label_out) *cancel_label_out = cancel_label;
	if (cancel_x_out)     *cancel_x_out     = x_cancel;
	if (cancel_len_out)   *cancel_len_out   = cancel_len;
}

static void fp_draw_buttons(struct fp_state *s)
{
	uifcapi_t  *api = s->api;
	int         normal = api->hclr | (api->bclr << 4);
	int         active = api->lbclr;
	const char *edit_label, *ok_label, *cancel_label;
	int         x_edit, x_ok, x_cancel;
	int         edit_len;

	fp_button_rects(s,
	    &edit_label, &x_edit, &edit_len,
	    &ok_label,   &x_ok,   NULL,
	    &cancel_label, &x_cancel, NULL);
	if (edit_len > 0) {
		api->printf(x_edit, SCRN_TOP + s->row_buttons,
		    s->field == FIELD_EDIT ? active : normal, "%s", edit_label);
	}
	api->printf(x_ok, SCRN_TOP + s->row_buttons,
	    s->field == FIELD_OK ? active : normal, "%s", ok_label);
	api->printf(x_cancel, SCRN_TOP + s->row_buttons,
	    s->field == FIELD_CANCEL ? active : normal, "%s", cancel_label);
}

static void fp_draw_info(struct fp_state *s)
{
	uifcapi_t  *api = s->api;
	int         attr = api->lclr | (api->bclr << 4);
	int         w = s->width - 4;
	const char *bare = NULL;
	char        full[MAX_PATH * 8 + 1];
	struct stat st;
	int         have_stat = 0;
	int         is_dir = 0;
	char        line1[300];
	char        line2[300];
	int         selected = 0;

	if (s->field == FIELD_DIR && s->dirs && s->dir_count > 0
	    && s->dir_cur < (int)s->dir_count) {
		bare = s->dirs[s->dir_cur].name;
		is_dir = 1;
	}
	else if (s->files && s->file_count > 0 && s->file_cur < (int)s->file_count) {
		bare = s->files[s->file_cur].name;
		is_dir = s->files[s->file_cur].is_dir;
	}

	if (bare == NULL) {
		api->printf(SCRN_LEFT + 2, SCRN_TOP + s->row_info,     attr, "%-*s", w, "");
		api->printf(SCRN_LEFT + 2, SCRN_TOP + s->row_info + 1, attr, "%-*s", w, "");
		return;
	}

	snprintf(full, sizeof(full), "%s%s", s->path, bare);
	if (stat(full, &st) == 0)
		have_stat = 1;

	if (s->mode == FP_MODE_MULTI && !is_dir)
		selected = sel_index_of(s, full) >= 0;

	snprintf(line1, sizeof(line1), "%s%s",
	    is_dir ? "Dir:  " : "File: ", bare);
	if ((int)strlen(line1) > w) {
		line1[w] = 0;
	}

	if (have_stat) {
		struct tm *tm = localtime(&st.st_mtime);
		char       tbuf[32];

		if (tm)
			strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm);
		else
			SAFECOPY(tbuf, "");

		{
			/* Fixed-width size column.  "directory" is 9 chars; the
			 * widest possible "<num><suffix>iB" output is also 8
			 * chars (e.g. "999.9PiB"), so a 9-char left-aligned
			 * field keeps the date column from jumping as the
			 * cursor moves between files and directories. */
			char szbuf[16];
			char szwithB[20];

			if (is_dir) {
				SAFECOPY(szwithB, "directory");
			}
			else {
				byte_estimate_to_str((uint64_t)st.st_size, szbuf, sizeof(szbuf),
				    /* unit: */ 1, /* precision: */ 1);
				/* If byte_estimate_to_str picked a binary suffix
				 * (KMGTP), turn it into the IEC form (KiB...);
				 * otherwise this is a plain byte count, so
				 * append a bare "B". */
				size_t l = strlen(szbuf);
				if (l > 0 && strchr("KMGTP", szbuf[l - 1]) != NULL)
					snprintf(szwithB, sizeof(szwithB), "%siB", szbuf);
				else
					snprintf(szwithB, sizeof(szwithB), "%sB", szbuf);
			}
			snprintf(line2, sizeof(line2),
			    "      %-9s  %s%s",
			    szwithB, tbuf,
			    selected ? "  [selected]" : "");
		}
	}
	else {
		snprintf(line2, sizeof(line2), "      (no info)%s",
		    selected ? "  [selected]" : "");
	}

	api->printf(SCRN_LEFT + 2, SCRN_TOP + s->row_info,     attr, "%-*.*s", w, w, line1);
	api->printf(SCRN_LEFT + 2, SCRN_TOP + s->row_info + 1, attr, "%-*.*s", w, w, line2);
}

/* ------------------------------------------------------------------ */
/* Mouse routing                                                      */
/* ------------------------------------------------------------------ */

static int fp_mouse_to_field(struct fp_state *s)
{
	uifcapi_t         *api = s->api;
	struct mouse_event mevnt;
	int                newfield = s->field;

	if (getmouse(&mevnt) != 0)
		return s->field;
	(void)api;  /* Used only via SCRN_LEFT/SCRN_RIGHT macros */

	/* For cross-field clicks on a list pane, move focus and slide the
	 * lightbar to the clicked row, but do NOT re-inject the mouse
	 * event — the click acts purely as a focus transition.  A second
	 * click on that pane (now focused) goes straight through api->list
	 * and activates the item in the usual way, so double-click-to-open
	 * falls out naturally.
	 *
	 * For cross-field clicks on a text field, just move focus; the
	 * next iteration's getstrxy enters via the K_EDIT select-all path
	 * the same way a Tab in would. */
	if (mevnt.endx >= SCRN_LEFT + 1
	    && mevnt.endx <= SCRN_LEFT + s->list_width
	    && mevnt.endy >= SCRN_TOP + s->row_lists
	    && mevnt.endy <= SCRN_TOP + s->row_lists + s->list_height - 1) {
		int delta = (mevnt.starty - SCRN_TOP - s->row_lists) - s->dir_bar;
		newfield = FIELD_DIR;
		s->dir_bar += delta;
		s->dir_cur += delta;
		fp_clamp_one((int)s->dir_count, s->list_height, &s->dir_cur, &s->dir_bar);
	}
	else if (mevnt.endx >= SCRN_LEFT + 1 + s->list_width + 1
	         && mevnt.endx <= SCRN_LEFT + 1 + s->list_width * 2
	         && mevnt.endy >= SCRN_TOP + s->row_lists
	         && mevnt.endy <= SCRN_TOP + s->row_lists + s->list_height - 1) {
		int delta = (mevnt.starty - SCRN_TOP - s->row_lists) - s->file_bar;
		newfield = FIELD_FILE;
		s->file_bar += delta;
		s->file_cur += delta;
		fp_clamp_one((int)s->file_count, s->list_height, &s->file_cur, &s->file_bar);
	}
	/* Mask field */
	else if (!(s->opts & UIFC_FP_MSKNOCHG)
	         && mevnt.endx >= SCRN_LEFT + 1 && mevnt.endx <= SCRN_LEFT + s->width - 2
	         && mevnt.endy == SCRN_TOP + s->row_mask) {
		newfield = FIELD_MASK;
	}
	/* Path field */
	else if (s->row_path >= 0 && (s->opts & UIFC_FP_ALLOWENTRY)
	         && mevnt.endx >= SCRN_LEFT + 1 && mevnt.endx <= SCRN_LEFT + s->width - 2
	         && mevnt.endy == SCRN_TOP + s->row_path) {
		newfield = FIELD_PATH;
	}
	/* Button row.  A click on the actual cells of any footer button
	 * both focuses AND activates it (we unget an Enter so the button
	 * handler fires its action on the next iteration).  A click
	 * elsewhere on the row just moves focus to the nearest button. */
	else if (mevnt.endy == SCRN_TOP + s->row_buttons) {
		int x_edit, edit_len, x_ok, ok_len, x_cancel, cancel_len;

		fp_button_rects(s,
		    NULL, &x_edit, &edit_len,
		    NULL, &x_ok,   &ok_len,
		    NULL, &x_cancel, &cancel_len);
		if (edit_len > 0
		    && mevnt.endx >= x_edit && mevnt.endx < x_edit + edit_len) {
			newfield = FIELD_EDIT;
			ungetch('\r');
		}
		else if (mevnt.endx >= x_ok && mevnt.endx < x_ok + ok_len) {
			newfield = FIELD_OK;
			ungetch('\r');
		}
		else if (mevnt.endx >= x_cancel && mevnt.endx < x_cancel + cancel_len) {
			newfield = FIELD_CANCEL;
			ungetch('\r');
		}
		else if (edit_len > 0 && mevnt.endx < x_ok) {
			newfield = FIELD_EDIT;
		}
		else if (mevnt.endx < x_cancel) {
			newfield = FIELD_OK;
		}
		else {
			newfield = FIELD_CANCEL;
		}
	}

	return newfield;
}

/* ------------------------------------------------------------------ */
/* List refresh                                                       */
/* ------------------------------------------------------------------ */

static int fp_refresh_lists(struct fp_state *s)
{
	glob_t fgl, dgl;
	int    rc;

	fp_normalize_path(s);

	fp_entries_free(&s->dirs, &s->dir_count);
	fp_entries_free(&s->files, &s->file_count);
	fp_display_free(&s->dir_display);
	fp_display_free(&s->file_display);

#ifdef _WIN32
	if (s->at_root) {
		if (fp_build_drive_list(s, &s->dirs, &s->dir_count) < 0)
			return -1;
	}
	else
#endif
	{
		if (fp_do_glob(s, "*", 1, &dgl) < 0 && !isdir(s->path)) {
			if (s->prev_path == NULL)
				return -1;
			s->api->msg("Cannot read directory!");
			SAFECOPY(s->path, s->prev_path);
			FREE_AND_NULL(s->prev_path);
			fp_normalize_path(s);
			memset(&dgl, 0, sizeof(dgl));
			if (fp_do_glob(s, "*", 1, &dgl) < 0)
				return -1;
		}
		rc = fp_entries_from_glob(s, &dgl, 1, &s->dirs, &s->dir_count);
		globfree(&dgl);
		if (rc < 0)
			return -1;
	}

	if (fp_do_glob(s, s->mask, 0, &fgl) < 0)
		memset(&fgl, 0, sizeof(fgl));
	rc = fp_entries_from_glob(s, &fgl, 0, &s->files, &s->file_count);
	globfree(&fgl);
	if (rc < 0)
		return -1;

	s->dir_display  = fp_build_display(s->dirs, s->dir_count);
	s->file_display = fp_build_display(s->files, s->file_count);
	if (s->dir_display == NULL || s->file_display == NULL)
		return -1;

	s->dir_cur = s->dir_bar = 0;
	s->file_cur = s->file_bar = 0;
	fp_refresh_markers(s);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Single-file commit                                                 */
/* ------------------------------------------------------------------ */

static int fp_commit_single(struct fp_state *s, struct file_pick *fp, const char *full)
{
	fp->selected = malloc(sizeof(char *));
	if (fp->selected == NULL)
		return -1;
	fp->selected[0] = strdup(full);
	if (fp->selected[0] == NULL) {
		FREE_AND_NULL(fp->selected);
		return -1;
	}
	fp->files = 1;
	return 0;
}

/* Commit the multi-select buffer into fp.  Empties s->selected (transferred). */
static int fp_commit_multi(struct fp_state *s, struct file_pick *fp)
{
	if (s->sel_count == 0)
		return -1;
	fp->files = (int)s->sel_count;
	fp->selected = s->selected;
	s->selected = NULL;
	s->sel_count = s->sel_cap = 0;
	return 0;
}

/* ------------------------------------------------------------------ */
/* OVERPROMPT / CREATPROMPT                                           */
/* ------------------------------------------------------------------ */

static int fp_confirm_finish(struct fp_state *s, const char *full)
{
	char *YesNo[] = {"Yes", "No", ""};
	int   i;

	if ((s->opts & UIFC_FP_OVERPROMPT) && fexist(full)) {
		if (s->api->list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL,
		        "File exists, overwrite?", YesNo) != 0) {
			if (s->api->exit_flags & UIFC_XF_QUIT)
				return -1;
			return 0;
		}
	}
	if ((s->opts & UIFC_FP_CREATPROMPT) && !fexist(full)) {
		if (s->api->list(WIN_MID | WIN_SAV, 0, 0, 0, &i, NULL,
		        "File does not exist, create?", YesNo) != 0) {
			if (s->api->exit_flags & UIFC_XF_QUIT)
				return -1;
			return 0;
		}
	}
	return 1;
}

/* ------------------------------------------------------------------ */
/* Multi-select review pane                                           */
/* ------------------------------------------------------------------ */

static char **fp_build_sel_view(struct fp_state *s)
{
	char **arr = malloc((s->sel_count + 2) * sizeof(char *));
	size_t i;

	if (arr == NULL)
		return NULL;
	for (i = 0; i < s->sel_count; i++)
		arr[i] = s->selected[i];
	arr[s->sel_count] = "";
	arr[s->sel_count + 1] = NULL;
	return arr;
}

static void fp_open_review(struct fp_state *s)
{
	int    cur = 0, bar = 0;
	int    rc;
	char **view;

	if (s->sel_count == 0) {
		s->api->msg("No files have been selected yet.");
		return;
	}

	for (;;) {
		size_t idx;

		view = fp_build_sel_view(s);
		if (view == NULL)
			return;
		rc = s->api->list(WIN_MID | WIN_SAV | WIN_DEL,
		    0, 0, 0, &cur, &bar, "Esc/click removes", view);
		free(view);

		if (rc == -1)
			return;                     /* Esc */
		if ((rc & MSK_ON) == MSK_DEL)
			idx = (size_t)(rc & MSK_OFF);
		else if (rc >= 0)
			idx = (size_t)rc;            /* Click or Enter on an item */
		else
			return;

		sel_remove_at(s, idx);
		if (cur >= (int)s->sel_count)
			cur = (s->sel_count == 0) ? 0 : (int)s->sel_count - 1;
		fp_refresh_markers(s);
		if (s->sel_count == 0)
			return;
	}
}

/* ------------------------------------------------------------------ */
/* Field handlers                                                     */
/*                                                                    */
/* Each returns the next field to focus, or one of the FP_RC_* values:*/
/*   FP_RC_FINISH (-2)   commit current selection and exit            */
/*   FP_RC_CANCEL (-3)   cancel and exit                              */
/*   FP_RC_REREAD (-4)   refresh lists in outer loop                  */
/* ------------------------------------------------------------------ */

#define FP_RC_FINISH (-2)
#define FP_RC_CANCEL (-3)
#define FP_RC_REREAD (-4)

static int fp_next_field(const struct fp_state *s, int field, int delta)
{
	int n = field;

	for (;;) {
		n += delta;
		if (n < 0)
			n = FIELD_COUNT - 1;
		if (n >= FIELD_COUNT)
			n = 0;
		if (n == FIELD_MASK && (s->opts & UIFC_FP_MSKNOCHG))
			continue;
		if (n == FIELD_PATH && !(s->opts & UIFC_FP_ALLOWENTRY))
			continue;
		if (n == FIELD_EDIT && s->mode != FP_MODE_MULTI)
			continue;
		return n;
	}
}

/* Handle ENTER on a directory entry. */
static int fp_enter_dir(struct fp_state *s)
{
	const char *name = s->dirs[s->dir_cur].name;

	FREE_AND_NULL(s->prev_path);
	s->prev_path = strdup(s->path);
	if (s->prev_path == NULL)
		return FIELD_DIR;
	if (strcmp(name, "..") == 0) {
		/* Walk up: strip trailing component. */
		size_t len = strlen(s->path);

		if (len > 0 && (s->path[len - 1] == '/' || s->path[len - 1] == '\\'))
			s->path[len - 1] = 0;
		while ((len = strlen(s->path)) > 0
		       && s->path[len - 1] != '/' && s->path[len - 1] != '\\')
			s->path[len - 1] = 0;
	}
	else {
		strcat(s->path, name);
	}
	return FP_RC_REREAD;
}

static int fp_handle_dir_field(struct fp_state *s, struct file_pick *fp)
{
	int i;
	uifc_winmode_t flags = WIN_DYN | WIN_NOBRDR | WIN_FIXEDHEIGHT
	    | WIN_EXTKEYS | WIN_UNGETMOUSE | WIN_REDRAW;

	/* In MULTI mode, WIN_EDIT makes F2 internally route to the edit
	 * action AND draws the F2 hint in the status bar (the hint text
	 * itself comes from api->edit_item, which we override to
	 * "Review  " in fp_state_init). */
	if (s->mode == FP_MODE_MULTI)
		flags |= WIN_EDIT;

	i = s->api->list(flags, 1, s->row_lists, s->list_width,
	    &s->dir_cur, &s->dir_bar, NULL, s->dir_display);

	if (i == -1)
		return FP_RC_CANCEL;
	if (i == UIFC_EXTKEY('\t') || i == UIFC_EXTKEY(CIO_KEY_RIGHT))
		return fp_next_field(s, FIELD_DIR, +1);
	if (i == UIFC_EXTKEY(CIO_KEY_BACKTAB) || i == UIFC_EXTKEY(CIO_KEY_LEFT))
		return fp_next_field(s, FIELD_DIR, -1);
	if (i == UIFC_EXTKEY(CIO_KEY_MOUSE))
		return fp_mouse_to_field(s);
	if (s->mode == FP_MODE_MULTI && (i & MSK_ON) == MSK_EDIT) {
		fp_open_review(s);
		return FIELD_DIR;
	}
	if (s->mode == FP_MODE_MULTI && i == UIFC_EXTKEY(CTRL_A)) {
		fp_select_all(s);
		return FIELD_DIR;
	}
	if (i == UIFC_EXTKEY('\n'))
		return fp_activate_ok(s, fp, FIELD_DIR);
	if (i >= 0) {
		if (s->dir_count == 0)
			return FIELD_DIR;
		if (i >= (int)s->dir_count)
			i = (int)s->dir_count - 1;
		s->dir_cur = i;
		fp_clamp_one((int)s->dir_count, s->list_height, &s->dir_cur, &s->dir_bar);
		return fp_enter_dir(s);
	}

	(void)fp;
	return FIELD_DIR;
}

static int fp_handle_file_field(struct fp_state *s, struct file_pick *fp)
{
	int i;
	uifc_winmode_t flags = WIN_DYN | WIN_NOBRDR | WIN_FIXEDHEIGHT
	    | WIN_EXTKEYS | WIN_UNGETMOUSE | WIN_REDRAW;

	if (s->mode == FP_MODE_MULTI)
		flags |= WIN_EDIT;

	i = s->api->list(flags, 1 + s->list_width + 1, s->row_lists,
	    s->list_width, &s->file_cur, &s->file_bar, NULL, s->file_display);

	if (i == -1)
		return FP_RC_CANCEL;
	if (i == UIFC_EXTKEY('\t') || i == UIFC_EXTKEY(CIO_KEY_RIGHT))
		return fp_next_field(s, FIELD_FILE, +1);
	if (i == UIFC_EXTKEY(CIO_KEY_BACKTAB) || i == UIFC_EXTKEY(CIO_KEY_LEFT))
		return fp_next_field(s, FIELD_FILE, -1);
	if (i == UIFC_EXTKEY(CIO_KEY_MOUSE))
		return fp_mouse_to_field(s);
	if (s->mode == FP_MODE_MULTI && (i & MSK_ON) == MSK_EDIT) {
		fp_open_review(s);
		return FIELD_FILE;
	}
	if (s->mode == FP_MODE_MULTI && i == UIFC_EXTKEY(CTRL_A)) {
		fp_select_all(s);
		return FIELD_FILE;
	}
	if (i == UIFC_EXTKEY('\n'))
		return fp_activate_ok(s, fp, FIELD_FILE);
	/* In MULTI mode, treat Space exactly like Enter: toggle the
	 * currently-highlighted file's selection marker. */
	if (s->mode == FP_MODE_MULTI && i == UIFC_EXTKEY(' '))
		i = s->file_cur;
	if (i >= 0) {
		char             full[MAX_PATH * 8 + 1];
		struct fp_entry *e;

		if (s->file_count == 0)
			return FIELD_FILE;
		/* Mouse clicks below the last item return an out-of-range
		 * index — clamp to the last valid entry instead of indexing
		 * past the array.  Also clamp file_bar, because uifc's
		 * redraw path computes the first visible item as
		 * (*cur - *bar) and a stale bar yields a negative index
		 * which crashes strlen(). */
		if (i >= (int)s->file_count)
			i = (int)s->file_count - 1;
		s->file_cur = i;
		fp_clamp_one((int)s->file_count, s->list_height, &s->file_cur, &s->file_bar);
		e = &s->files[s->file_cur];
		i = s->file_cur;

		/* In directory-selection mode the file pane is informational
		 * only — activating a file is a no-op so the user can't
		 * accidentally pick one. */
		if (s->mode == FP_MODE_DIRSEL)
			return FIELD_FILE;

		snprintf(full, sizeof(full), "%s%s", s->path, e->name);
		if (s->mode == FP_MODE_MULTI) {
			int already = sel_index_of(s, full) >= 0;
			if (already)
				sel_remove(s, full);
			else
				sel_add(s, full);
			fp_set_marker(s, e, !already);
			return FIELD_FILE;
		}
		/* Repaint the pane so the cursor is visibly on the chosen
		 * row before we commit and exit (api->list returns *cur
		 * after a click but doesn't repaint the highlight first). */
		s->api->list(WIN_NOBRDR | WIN_FIXEDHEIGHT | WIN_IMM | WIN_REDRAW,
		    1 + s->list_width + 1, s->row_lists, s->list_width,
		    &s->file_cur, &s->file_bar, NULL, s->file_display);
		if (fp_commit_single(s, fp, full) < 0) {
			s->retval = -1;
			return FP_RC_CANCEL;
		}
		s->retval = 1;
		return FP_RC_FINISH;
	}
	return FIELD_FILE;
}

static int fp_handle_mask_field(struct fp_state *s)
{
	uifcapi_t *api = s->api;
	char       prev[sizeof(s->mask)];
	int        key;
	int        dest_field = FIELD_MASK;
	long       kmode = K_EDIT | K_TABEXIT | K_MOUSEEXIT;

	/* K_DEUCEEXIT makes getstrxy() return on F2 / Up / Down too, so
	 * the F2-opens-review shortcut works even while the user is
	 * typing in this field.  Only useful in MULTI mode — in other
	 * modes F2 has no binding. */
	if (s->mode == FP_MODE_MULTI)
		kmode |= K_DEUCEEXIT;

	SAFECOPY(prev, s->mask);
	api->getstrxy(SCRN_LEFT + 8, SCRN_TOP + s->row_mask, s->width - 9,
	    s->mask, sizeof(s->mask) - 1, kmode, &key);

	if (key == ESC || key == CIO_KEY_QUIT
	    || (s->api->exit_flags & UIFC_XF_QUIT))
		return FP_RC_CANCEL;
	if (key == CIO_KEY_MOUSE)
		return fp_mouse_to_field(s);
	if (s->mode == FP_MODE_MULTI && key == CIO_KEY_F(2)) {
		fp_open_review(s);
		return FIELD_MASK;
	}

	if (key == '\t' || key == CIO_KEY_DOWN)
		dest_field = fp_next_field(s, FIELD_MASK, +1);
	else if (key == CIO_KEY_BACKTAB || key == CIO_KEY_UP)
		dest_field = fp_next_field(s, FIELD_MASK, -1);

	if (strcmp(s->mask, prev) != 0) {
		if (s->mask[0] == 0)
			SAFECOPY(s->mask, "*");
		s->field = dest_field;
		return FP_RC_REREAD;
	}
	return dest_field;
}

static int fp_handle_path_field(struct fp_state *s, struct file_pick *fp)
{
	uifcapi_t *api = s->api;
	char       drive[3], tdir[MAX_PATH + 1], fname[MAX_PATH + 1], ext[MAX_PATH + 1];
	int        key;
	char       committed[sizeof(s->entry)];
	int        dest_field = FIELD_PATH;   /* where focus lands when we're done */

	{
		long kmode = K_EDIT | K_TABEXIT | K_MOUSEEXIT;

		if (s->mode == FP_MODE_MULTI)
			kmode |= K_DEUCEEXIT;
		api->getstrxy(SCRN_LEFT + 8, SCRN_TOP + s->row_path, s->width - 9,
		    s->entry, sizeof(s->entry) - 1, kmode, &key);
	}

	if (key == ESC)
		return FP_RC_CANCEL;
	if (key == CIO_KEY_MOUSE)
		return fp_mouse_to_field(s);
	if (s->mode == FP_MODE_MULTI && key == CIO_KEY_F(2)) {
		fp_open_review(s);
		return FIELD_PATH;
	}

	/* Tab/Backtab always moves focus, even if the entered path also
	 * triggers a navigation refresh below.  Up/Down act the same as
	 * Backtab/Tab for keyboard-only navigation. */
	if (key == '\t' || key == CIO_KEY_DOWN)
		dest_field = fp_next_field(s, FIELD_PATH, +1);
	else if (key == CIO_KEY_BACKTAB || key == CIO_KEY_UP)
		dest_field = fp_next_field(s, FIELD_PATH, -1);

	SAFECOPY(committed, s->entry);
	if ((s->opts & (UIFC_FP_FILEEXIST | UIFC_FP_PATHEXIST)) && !fexist(committed)) {
#ifdef _WIN32
		if (committed[0])
#endif
		{
			api->msg("No such path/file!");
			if (api->exit_flags & UIFC_XF_QUIT)
				return FP_RC_CANCEL;
			return FIELD_PATH;
		}
	}

	if (isdir(committed) && committed[0])
		backslash(committed);
	_splitpath(committed, drive, tdir, fname, ext);

	{
		char dir_only[sizeof(s->path)];
		snprintf(dir_only, sizeof(dir_only), "%s%s", drive, tdir);
		if (!isdir(dir_only)) {
#ifdef _WIN32
			if (committed[0] && strcmp(committed, s->mask) != 0)
#endif
			{
				api->msg("No such path!");
				if (api->exit_flags & UIFC_XF_QUIT)
					return FP_RC_CANCEL;
				return FIELD_PATH;
			}
		}
	}

	/* If the basename contains wildcards or the user typed a directory and
	 * pressed enter, treat it as a navigation/mask change. */
	int has_wild = (strchr(fname, '*') || strchr(fname, '?')
	                || strchr(ext, '*') || strchr(ext, '?'));
	int wants_finish = (key == '\r' || key == '\n');

	if (has_wild
	    || ((isdir(committed) || committed[0] == 0)
	        && !(s->opts & UIFC_FP_DIRSEL) && wants_finish)
	    || (!isdir(committed) && !wants_finish)) {
		if (s->opts & UIFC_FP_MSKNOCHG) {
			api->msg("File mask cannot be changed");
			if (api->exit_flags & UIFC_XF_QUIT)
				return FP_RC_CANCEL;
			return FIELD_PATH;
		}
		if (!isdir(committed)) {
			snprintf(s->mask, sizeof(s->mask), "%s%s", fname, ext);
			if (s->mask[0] == 0)
				SAFECOPY(s->mask, "*");
		}
		snprintf(s->path, sizeof(s->path), "%s%s", drive, tdir);
		s->field = dest_field;
		return FP_RC_REREAD;
	}

	/* Plain path navigation: directory navigation or file selection. */
	if (isdir(committed)) {
		if (s->mode == FP_MODE_DIRSEL && wants_finish) {
			if (fp_commit_single(s, fp, committed) < 0) {
				s->retval = -1;
				return FP_RC_CANCEL;
			}
			s->retval = 1;
			return FP_RC_FINISH;
		}
		SAFECOPY(s->path, committed);
		s->field = dest_field;
		return FP_RC_REREAD;
	}

	if (wants_finish && s->mode != FP_MODE_DIRSEL) {
		if (fp_commit_single(s, fp, committed) < 0) {
			s->retval = -1;
			return FP_RC_CANCEL;
		}
		s->retval = 1;
		return FP_RC_FINISH;
	}

	return dest_field;
}

/* Invoke the OK/Select action as if the user had activated the OK
 * footer button.  Used both by the button handler's normal activation
 * path and by the Ctrl+Enter shortcut from other fields.  Returns a
 * value suitable to return from a field handler.  The caller's current
 * field is passed as back_to_field so we can stay there on no-op cases
 * (e.g. MULTI with nothing selected). */
static int fp_activate_ok(struct fp_state *s, struct file_pick *fp, int back_to_field)
{
	uifcapi_t *api = s->api;

	if (s->mode == FP_MODE_MULTI) {
		if (s->sel_count == 0) {
			api->msg("No files have been selected.");
			return back_to_field;
		}
		s->retval = (int)s->sel_count;
		if (fp_commit_multi(s, fp) < 0) {
			s->retval = -1;
			return FP_RC_CANCEL;
		}
		return FP_RC_FINISH;
	}
	if (s->mode == FP_MODE_DIRSEL) {
		if (fp_commit_single(s, fp, s->path) < 0) {
			s->retval = -1;
			return FP_RC_CANCEL;
		}
		s->retval = 1;
		return FP_RC_FINISH;
	}
	/* PICK: commit the highlighted file. */
	if (s->files && s->file_cur < (int)s->file_count) {
		char full[MAX_PATH * 8 + 1];

		snprintf(full, sizeof(full), "%s%s",
		    s->path, s->files[s->file_cur].name);
		if (fp_commit_single(s, fp, full) < 0) {
			s->retval = -1;
			return FP_RC_CANCEL;
		}
		s->retval = 1;
		return FP_RC_FINISH;
	}
	api->msg("No file is highlighted.");
	return back_to_field;
}

static int fp_handle_buttons(struct fp_state *s, struct file_pick *fp)
{
	uifcapi_t *api = s->api;
	int        ch;

	fp_draw_buttons(s);
	(void)api;  /* Used in nested case bodies via api->msg() */

	/* conio extended keys arrive as a 0/0xE0 prefix byte followed by
	 * the scancode in a second getch().  Combine them so our switch
	 * sees the proper CIO_KEY_* value (Backtab, arrows, etc.). */
	ch = getch();
	if (ch == 0 || ch == 0xe0) {
		ch |= (getch() << 8);
		if (ch == CIO_KEY_LITERAL_E0)
			ch = 0xe0;
	}

	switch (ch) {
		case ESC:
			return FP_RC_CANCEL;
		case '\t':
			return fp_next_field(s, s->field, +1);
		case CIO_KEY_BACKTAB:
			return fp_next_field(s, s->field, -1);
		case CIO_KEY_LEFT:
			if (s->field == FIELD_CANCEL)
				return FIELD_OK;
			return fp_next_field(s, s->field, -1);
		case CIO_KEY_RIGHT:
			if (s->field == FIELD_OK)
				return FIELD_CANCEL;
			return fp_next_field(s, s->field, +1);
		case CIO_KEY_MOUSE:
			return fp_mouse_to_field(s);
		case CIO_KEY_F(2):
			if (s->mode == FP_MODE_MULTI) {
				fp_open_review(s);
				return s->field;
			}
			return s->field;
		case '\n':
			/* Ctrl+Enter is an OK-from-anywhere shortcut. */
			return fp_activate_ok(s, fp, s->field);
		case '\r':
		case ' ':
			if (s->field == FIELD_CANCEL)
				return FP_RC_CANCEL;
			if (s->field == FIELD_EDIT) {
				fp_open_review(s);
				return FIELD_EDIT;
			}
			return fp_activate_ok(s, fp, s->field);
	}
	return s->field;
}

/* ------------------------------------------------------------------ */
/* Inactive-list redraw                                               */
/* ------------------------------------------------------------------ */

/* Paint list panes only when their drawn state has actually changed:
 *   - initial_paint: both panes painted (fresh data after a refresh)
 *   - focus moved away from a pane: that pane gets repainted as inactive
 *
 * The currently-focused pane is otherwise driven by its handler's WIN_DYN
 * call, so we deliberately do NOT paint it here every iteration — that
 * would compete with WIN_DYN's own update and produce visible flicker. */
static void fp_draw_lists(struct fp_state *s)
{
	uifcapi_t     *api = s->api;
	int            active_now = (s->field == FIELD_DIR || s->field == FIELD_FILE)
	                            ? s->field : -1;
	int            draw_dir   = 0;
	int            draw_file  = 0;
	uchar          saved      = api->lbclr;
	uifc_winmode_t flags      = WIN_NOBRDR | WIN_FIXEDHEIGHT | WIN_IMM | WIN_REDRAW;

	/* Include WIN_EDIT so api->list's internal bottomline() refresh
	 * keeps the F2 hint visible (otherwise a WIN_IMM pass here erases
	 * the hint we just drew). */
	if (s->mode == FP_MODE_MULTI)
		flags |= WIN_EDIT;

	if (s->initial_paint) {
		draw_dir = draw_file = 1;
	}
	else if (active_now != s->last_active_pane) {
		/* The pane that just lost focus needs to be repainted as inactive. */
		if (s->last_active_pane == FIELD_DIR)
			draw_dir = 1;
		else if (s->last_active_pane == FIELD_FILE)
			draw_file = 1;
	}

	api->lbclr = api->lclr | (api->bclr << 4);
	if (draw_dir && s->dir_display) {
		api->list(flags, 1, s->row_lists, s->list_width,
		    &s->dir_cur, &s->dir_bar, NULL, s->dir_display);
	}
	if (draw_file && s->file_display) {
		api->list(flags, 1 + s->list_width + 1, s->row_lists, s->list_width,
		    &s->file_cur, &s->file_bar, NULL, s->file_display);
	}
	api->lbclr = saved;

	s->initial_paint    = 0;
	s->last_active_pane = active_now;
}

/* ------------------------------------------------------------------ */
/* Top-level browser loop                                             */
/* ------------------------------------------------------------------ */

/* Constrain cursor (absolute item index) and bar (cursor's visible row
 * within the pane) to valid ranges.  api->list can return out-of-range
 * values after a mouse click on empty pane space — letting them reach
 * the next api->list crashes uifc because it indexes the option array
 * with (*cur - *bar). */
static void fp_clamp_one(int count, int list_height, int *cur, int *bar)
{
	if (count <= 0)
		return;
	if (*cur < 0)
		*cur = 0;
	if (*cur >= count)
		*cur = count - 1;
	if (*bar < 0)
		*bar = 0;
	if (*bar > *cur)
		*bar = *cur;
	if (list_height > 0 && *bar >= list_height)
		*bar = list_height - 1;
}

static void fp_clamp_cursors(struct fp_state *s)
{
	fp_clamp_one((int)s->dir_count,  s->list_height, &s->dir_cur,  &s->dir_bar);
	fp_clamp_one((int)s->file_count, s->list_height, &s->file_cur, &s->file_bar);
}

static int fp_run_browser(struct fp_state *s, struct file_pick *fp)
{
	uifcapi_t *api = s->api;
	int        next;

	for (;;) {
		hold_update = TRUE;
		if (fp_refresh_lists(s) < 0) {
			s->retval = -1;
			return -1;
		}
		s->initial_paint = 1;
		{
			char shown[sizeof(s->path)];
			SAFECOPY(shown, s->path);
#ifdef _WIN32
			if (strncmp(shown, "\\\\?\\", 4) == 0)
				memmove(shown, shown + 4, strlen(shown + 3));
#endif
			snprintf(s->entry, sizeof(s->entry), "%s%s", shown, s->mask);
		}
		s->reread = 0;

		while (!s->reread && !s->finished) {
			int text_attr = api->lclr | (api->bclr << 4);
			int alt_attr  = api->cclr | (api->bclr << 4);

			fp_clamp_cursors(s);
			hold_update = TRUE;
			draw_truncated_field(api, SCRN_LEFT + 8, SCRN_TOP + s->row_mask,
			    s->width - 9, s->mask, /*trunc_right=*/1,
			    text_attr, alt_attr);
			if (s->row_path >= 0) {
				char shown[sizeof(s->path)];

				SAFECOPY(shown, s->path);
#ifdef _WIN32
				if (strncmp(shown, "\\\\?\\", 4) == 0)
					memmove(shown, shown + 4, strlen(shown + 3));
#endif
				draw_truncated_field(api, SCRN_LEFT + 8, SCRN_TOP + s->row_path,
				    s->width - 9, shown, /*trunc_right=*/0,
				    text_attr, alt_attr);
			}

			/* Keep the F2 hint on the status line even when focus
			 * leaves the list panes.  api->list() refreshes it on
			 * its own but getstrxy()/getch() don't. */
			if (s->mode == FP_MODE_MULTI && api->bottomline != NULL)
				api->bottomline(WIN_EDIT);

			fp_draw_buttons(s);
			fp_draw_lists(s);
			fp_draw_info(s);
			hold_update = FALSE;

			switch (s->field) {
				case FIELD_DIR:    next = fp_handle_dir_field(s, fp); break;
				case FIELD_FILE:   next = fp_handle_file_field(s, fp); break;
				case FIELD_MASK:   next = fp_handle_mask_field(s);     break;
				case FIELD_PATH:   next = fp_handle_path_field(s, fp); break;
				case FIELD_EDIT:
				case FIELD_OK:
				case FIELD_CANCEL: next = fp_handle_buttons(s, fp);    break;
				default:           next = FIELD_DIR; break;
			}

			if (next == FP_RC_CANCEL) {
				return 0;
			}
			if (next == FP_RC_FINISH) {
				/* PICK mode honours OVERPROMPT/CREATPROMPT */
				if (s->mode == FP_MODE_PICK && fp->files == 1) {
					int ok = fp_confirm_finish(s, fp->selected[0]);
					if (ok < 0)
						return -1;
					if (ok == 0) {
						filepick_free(fp);
						s->retval = 0;
						s->field = FIELD_FILE;
						continue;
					}
				}
				s->finished = 1;
				continue;
			}
			if (next == FP_RC_REREAD) {
				s->reread = 1;
				continue;
			}
			s->field = next;
		}
		if (s->finished)
			break;
	}
	return s->retval;
}

/* ------------------------------------------------------------------ */
/* State lifecycle                                                    */
/* ------------------------------------------------------------------ */

static int fp_state_init(struct fp_state *s, uifcapi_t *api, enum fp_mode mode,
                         const char *title, const char *dir, const char *mask, int opts)
{
	memset(s, 0, sizeof(*s));
	s->api  = api;
	s->mode = mode;
	s->opts = opts;
	if (mode == FP_MODE_DIRSEL)
		s->opts |= UIFC_FP_DIRSEL;
	s->title = title ? title : "";

	{
		const char *src = (dir == NULL || dir[0] == 0) ? "." : dir;
		FULLPATH(s->path, src, sizeof(s->path));
	}
	backslash(s->path);

	if (mask == NULL || mask[0] == 0)
		SAFECOPY(s->mask, "*");
	else
		SAFECOPY(s->mask, mask);

	s->saved_hold      = hold_update;
	s->saved_x         = wherex();
	s->saved_y         = wherey();
	s->saved_lbclr     = api->lbclr;
	s->saved_edit_item = api->edit_item;
	if (mode == FP_MODE_MULTI)
		api->edit_item = "Review  ";

	s->field = (mode == FP_MODE_DIRSEL) ? FIELD_DIR : FIELD_FILE;
	s->last_active_pane = -1;
	return 0;
}

static void fp_state_destroy(struct fp_state *s)
{
	size_t i;

	fp_entries_free(&s->dirs, &s->dir_count);
	fp_entries_free(&s->files, &s->file_count);
	fp_display_free(&s->dir_display);
	fp_display_free(&s->file_display);
	FREE_AND_NULL(s->prev_path);

	if (s->selected) {
		for (i = 0; i < s->sel_count; i++)
			free(s->selected[i]);
		free(s->selected);
		s->selected = NULL;
		s->sel_count = s->sel_cap = 0;
	}

	hold_update = s->saved_hold;
	gotoxy(s->saved_x, s->saved_y);
	s->api->lbclr     = s->saved_lbclr;
	s->api->edit_item = s->saved_edit_item;
}

/* ------------------------------------------------------------------ */
/* Public entry points                                                */
/* ------------------------------------------------------------------ */

static int fp_run(uifcapi_t *api, const char *title, struct file_pick *fp,
                  const char *dir, const char *mask, int opts, enum fp_mode mode)
{
	struct fp_state s;
	int             rc;

	if (fp == NULL || api == NULL)
		return -1;
	fp->files = 0;
	fp->selected = NULL;

	if (mode == FP_MODE_MULTI
	    && (opts & (UIFC_FP_ALLOWENTRY | UIFC_FP_OVERPROMPT | UIFC_FP_CREATPROMPT)))
		return -1;

	if (fp_state_init(&s, api, mode, title, dir, mask, opts) < 0)
		return -1;

	hold_update = TRUE;
	fp_compute_layout(&s);
	fp_draw_chrome(&s);
	fp_draw_title(&s);
	fp_draw_field_labels(&s);

	rc = fp_run_browser(&s, fp);

	fp_state_destroy(&s);
	return rc;
}

int filepick(uifcapi_t *api, const char *title, struct file_pick *fp,
             const char *initial_dir, const char *default_mask, int opts)
{
	enum fp_mode mode = (opts & UIFC_FP_DIRSEL) ? FP_MODE_DIRSEL : FP_MODE_PICK;
	return fp_run(api, title, fp, initial_dir, default_mask, opts, mode);
}

int filepick_multi(uifcapi_t *api, const char *title, struct file_pick *fp,
                   const char *initial_dir, const char *default_mask, int opts)
{
	return fp_run(api, title, fp, initial_dir, default_mask, opts, FP_MODE_MULTI);
}

int filepick_free(struct file_pick *fp)
{
	int i;

	if (fp == NULL)
		return -1;
	for (i = 0; i < fp->files; i++)
		FREE_AND_NULL(fp->selected[i]);
	FREE_AND_NULL(fp->selected);
	fp->files = 0;
	return 0;
}
