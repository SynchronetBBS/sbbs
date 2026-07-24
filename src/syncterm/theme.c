#include "theme.h"

#include "syncterm.h"

#include <ctype.h>
#include <dirwrap.h>
#include <errno.h>
#include <ini_file.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <str_list.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#define fsync _commit
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include <dirwrap.h>
#define THEME_INHERIT (-1)

struct style_template {
	const char *role;
	int         font;
	int         legacy_attr;
	int32_t     foreground;
	int32_t     background;
};

struct glyph_template {
	const char *name;
	uint8_t     cp437;
	uint8_t     ascii;
};

#define S(role, font, attr, fg, bg) \
	{ role, font, attr, fg, bg }

static const struct style_template default_styles[] = {
	S("default", 0, 0x1f, 0xffffff, 0x0000a8),
	S("default.inactive", THEME_INHERIT, 0x3f, 0xffffff, 0x00aaaa),
	S("frame", THEME_INHERIT, 0x1e, 0xffff55, THEME_INHERIT),
	S("frame.inactive", THEME_INHERIT, 0x3f, 0xffffff, 0x00aaaa),
	S("title", THEME_INHERIT, 0x1e, 0xffff55, THEME_INHERIT),
	S("title.inactive", THEME_INHERIT, 0x3f, 0xffffff, 0x00aaaa),
	S("menu.item", THEME_INHERIT, 0x1f, THEME_INHERIT, THEME_INHERIT),
	S("menu.item.focused", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("menu.item.disabled", THEME_INHERIT, 0x18, 0x808080, THEME_INHERIT),
	S("list.item", THEME_INHERIT, 0x1f, THEME_INHERIT, THEME_INHERIT),
	S("list.item.focused", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("list.item.disabled", THEME_INHERIT, 0x18, 0x808080, THEME_INHERIT),
	S("list.item.focused.inactive", THEME_INHERIT, 0x3e, 0xffff55, 0x00aaaa),
	S("input", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("input.focused", THEME_INHERIT, 0x70, 0x000000, 0xffffff),
	S("button", THEME_INHERIT, 0x1f, THEME_INHERIT, THEME_INHERIT),
	S("button.hotkey", THEME_INHERIT, 0x1e, 0xffff55, THEME_INHERIT),
	S("button.focused", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("button.focused.hotkey", THEME_INHERIT, 0x78, 0x555555, THEME_INHERIT),
	S("statusbar", THEME_INHERIT, 0x30, 0x000000, 0x00aaaa),
	S("checkbox", THEME_INHERIT, 0x1f, THEME_INHERIT, THEME_INHERIT),
	S("checkbox.focused", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("radio.item", THEME_INHERIT, 0x1f, THEME_INHERIT, THEME_INHERIT),
	S("radio.item.focused", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("spinbox", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("spinbox.focused", THEME_INHERIT, 0x70, 0x000000, 0xffffff),
	S("menubar", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("menubar.item", THEME_INHERIT, 0x70, 0x000000, 0xaaaaaa),
	S("menubar.item.focused", THEME_INHERIT, 0x07, 0xaaaaaa, 0x000000),
	S("scrollbar.track", THEME_INHERIT, 0x17, 0xaaaaaa, 0x0000a8),
	S("scrollbar.thumb", THEME_INHERIT, 0x1f, 0xffffff, 0x0000a8),
	S("progress.fill", THEME_INHERIT, 0x1b, 0x55ffff, 0x0000a8),
	S("logview.info", THEME_INHERIT, 0x1f, 0xffffff, 0x0000a8),
	S("logview.notice", THEME_INHERIT, 0x1e, 0xffff55, 0x0000a8),
	S("logview.warning", THEME_INHERIT, 0x1d, 0xff55ff, 0x0000a8),
	S("logview.error", THEME_INHERIT, 0x1c, 0xff5555, 0x0000a8),
	S("help", THEME_INHERIT, 0x1f, 0xffffff, 0x0000a8),
	S("help.text", THEME_INHERIT, THEME_INHERIT, THEME_INHERIT, THEME_INHERIT),
	S("help.bold", THEME_INHERIT, 0x1e, 0xffff55, THEME_INHERIT),
	S("help.code", THEME_INHERIT, 0x1e, 0xffff55, THEME_INHERIT),
	S("help.heading.1", THEME_INHERIT, 0x31, 0x0000a8, 0x00aaaa),
	S("help.heading.2", THEME_INHERIT, 0x31, 0x0000a8, 0x00aaaa),
	S("help.heading.3", THEME_INHERIT, 0x31, 0x0000a8, 0x00aaaa),
	S("help.bullet", THEME_INHERIT, 0x1f, 0xffffff, THEME_INHERIT),
	S("popup", THEME_INHERIT, 0x4e, 0xffff55, 0xaa0000),
	S("popup.frame", THEME_INHERIT, 0x4f, 0xffffff, 0xaa0000),
	S("popup.frame.inactive", THEME_INHERIT, 0x47, 0xaaaaaa, THEME_INHERIT),
};

#undef S

#define G(name, cp437, ascii) { name, cp437, ascii }

static const struct glyph_template default_glyphs[] = {
	G("background", 0x20, 0x20),
	G("frame.display.topLeft", 0xda, '+'),
	G("frame.display.top", 0xc4, '-'),
	G("frame.display.topRight", 0xbf, '+'),
	G("frame.display.left", 0xb3, '|'),
	G("frame.display.right", 0xb3, '|'),
	G("frame.display.bottomLeft", 0xc0, '+'),
	G("frame.display.bottom", 0xc4, '-'),
	G("frame.display.bottomRight", 0xd9, '+'),
	G("frame.display.tee.left", 0xc3, '+'),
	G("frame.display.tee.right", 0xb4, '+'),
	G("frame.display.title.left", 0xb4, '-'),
	G("frame.display.title.right", 0xc3, '-'),
	G("frame.display.tee.top", 0xc2, '+'),
	G("frame.display.tee.bottom", 0xc1, '+'),
	G("frame.display.cross", 0xc5, '+'),
	G("frame.display.separator", 0xc4, '-'),
	G("frame.control.topLeft", 0xc9, '+'),
	G("frame.control.top", 0xcd, '='),
	G("frame.control.topRight", 0xbb, '+'),
	G("frame.control.left", 0xba, '|'),
	G("frame.control.right", 0xba, '|'),
	G("frame.control.bottomLeft", 0xc8, '+'),
	G("frame.control.bottom", 0xcd, '='),
	G("frame.control.bottomRight", 0xbc, '+'),
	G("frame.control.tee.left", 0xcc, '+'),
	G("frame.control.tee.right", 0xb9, '+'),
	G("frame.control.title.left", 0xb5, '='),
	G("frame.control.title.right", 0xc6, '='),
	G("frame.control.tee.top", 0xcb, '+'),
	G("frame.control.tee.bottom", 0xca, '+'),
	G("frame.control.cross", 0xce, '+'),
	G("frame.control.separator", 0xcd, '='),
	G("scrollbar.track", 0xb0, ':'),
	G("scrollbar.thumb", 0xdb, '#'),
	G("scrollbar.up", 0x1e, '^'),
	G("scrollbar.down", 0x1f, 'v'),
	G("scrollbar.separator", 0xb3, '|'),
	G("arrow.up", 0x18, '^'),
	G("arrow.down", 0x19, 'v'),
	G("arrow.right", 0x10, '>'),
	G("arrow.left", 0x11, '<'),
	G("focus.left", 0x10, '>'),
	G("focus.right", 0x11, '<'),
	G("check.on", 0xfb, 'X'),
	G("check.off", 0x20, 0x20),
	G("radio.on", 0x07, '*'),
	G("radio.off", 0x09, 'o'),
	G("tag.on", 0xaf, '>'),
	G("tag.off", 0x20, 0x20),
};

#undef G

static struct syncterm_theme *default_theme;
static struct syncterm_theme *committed_theme;
static struct syncterm_theme *preview_theme;
static uint64_t theme_generation = 1;

static struct syncterm_theme_catalog_entry *catalog;
static size_t catalog_count;

struct theme_document_style {
	char    *role;
	bool     builtin;
	bool     created;
	bool     deleted;
	unsigned present;
	unsigned modified;
	int      values[SYNCTERM_THEME_STYLE_FIELD_COUNT];
};

struct theme_document_glyph {
	char    *name;
	bool     builtin;
	bool     created;
	bool     deleted;
	unsigned present;
	unsigned modified;
	uint8_t  values[SYNCTERM_THEME_GLYPH_FIELD_COUNT];
};

struct theme_document_snapshot {
	struct theme_document_style *styles;
	size_t                       style_count;
	struct theme_document_glyph *glyphs;
	size_t                       glyph_count;
	str_list_t                    imported_sections;
	bool                          replace_definitions;
};

enum theme_history_kind {
	THEME_HISTORY_METADATA,
	THEME_HISTORY_STYLE,
	THEME_HISTORY_GLYPH,
	THEME_HISTORY_IMPORT
};

struct theme_string_state {
	char *value;
	bool  present;
	bool  modified;
};

struct theme_history_entry {
	enum theme_history_kind kind;
	unsigned                field;
	char                   *name;
	bool                    old_exists;
	bool                    new_exists;
	union {
		struct {
			struct theme_string_state old_state;
			struct theme_string_state new_state;
		} metadata;
		struct {
			struct theme_document_style old_state;
			struct theme_document_style new_state;
		} style;
		struct {
			struct theme_document_glyph old_state;
			struct theme_document_glyph new_state;
		} glyph;
		struct {
			struct theme_document_snapshot old_state;
			struct theme_document_snapshot new_state;
		} import;
	} data;
};

struct syncterm_theme_document {
	char                         *filename;
	str_list_t                    source;
	char                         *metadata[SYNCTERM_THEME_METADATA_FIELD_COUNT];
	unsigned                      metadata_present;
	unsigned                      metadata_modified;
	struct theme_document_style  *styles;
	size_t                        style_count;
	struct theme_document_glyph  *glyphs;
	size_t                        glyph_count;
	str_list_t                    imported_sections;
	bool                          replace_definitions;
	struct syncterm_theme        *preview;
	struct theme_history_entry   *history;
	size_t                        history_count;
	size_t                        history_position;
};

static void
set_error(char *error, size_t error_size, const char *format, ...)
{
	if (error == NULL || error_size == 0)
		return;
	va_list ap;
	va_start(ap, format);
	vsnprintf(error, error_size, format, ap);
	va_end(ap);
}

static char *
dup_string(const char *value)
{
	if (value == NULL)
		return NULL;
	return strdup(value);
}

void
syncterm_theme_free(struct syncterm_theme *theme)
{
	if (theme == NULL)
		return;
	free(theme->filename);
	free(theme->package);
	free(theme->name);
	free(theme->author);
	free(theme->description);
	free(theme->version);
	for (size_t i = 0; i < theme->style_count; i++)
		free(theme->styles[i].role);
	free(theme->styles);
	for (size_t i = 0; i < theme->glyph_count; i++)
		free(theme->glyphs[i].name);
	free(theme->glyphs);
	free(theme);
}

static struct syncterm_theme *
clone_theme(const struct syncterm_theme *source)
{
	struct syncterm_theme *theme = calloc(1, sizeof(*theme));
	if (theme == NULL)
		return NULL;
	theme->filename = dup_string(source->filename);
	theme->package = dup_string(source->package);
	theme->name = dup_string(source->name);
	theme->author = dup_string(source->author);
	theme->description = dup_string(source->description);
	theme->version = dup_string(source->version);
	if ((source->filename != NULL && theme->filename == NULL) ||
	    (source->package != NULL && theme->package == NULL) ||
	    (source->name != NULL && theme->name == NULL) ||
	    (source->author != NULL && theme->author == NULL) ||
	    (source->description != NULL && theme->description == NULL) ||
	    (source->version != NULL && theme->version == NULL))
		goto error;

	if (source->style_count > 0) {
		theme->styles = calloc(source->style_count, sizeof(*theme->styles));
		if (theme->styles == NULL)
			goto error;
		for (size_t i = 0; i < source->style_count; i++) {
			theme->styles[i] = source->styles[i];
			theme->styles[i].role = dup_string(source->styles[i].role);
			if (theme->styles[i].role == NULL)
				goto error;
			theme->style_count++;
		}
	}
	if (source->glyph_count > 0) {
		theme->glyphs = calloc(source->glyph_count, sizeof(*theme->glyphs));
		if (theme->glyphs == NULL)
			goto error;
		for (size_t i = 0; i < source->glyph_count; i++) {
			theme->glyphs[i] = source->glyphs[i];
			theme->glyphs[i].name = dup_string(source->glyphs[i].name);
			if (theme->glyphs[i].name == NULL)
				goto error;
			theme->glyph_count++;
		}
	}
	return theme;

error:
	syncterm_theme_free(theme);
	return NULL;
}

static struct syncterm_theme_style *
find_style(struct syncterm_theme *theme, const char *role)
{
	for (size_t i = 0; i < theme->style_count; i++) {
		if (strcmp(theme->styles[i].role, role) == 0)
			return &theme->styles[i];
	}
	return NULL;
}

static struct syncterm_theme_glyph *
find_glyph(struct syncterm_theme *theme, const char *name)
{
	for (size_t i = 0; i < theme->glyph_count; i++) {
		if (strcmp(theme->glyphs[i].name, name) == 0)
			return &theme->glyphs[i];
	}
	return NULL;
}

static struct syncterm_theme_style *
add_style(struct syncterm_theme *theme, const char *role)
{
	struct syncterm_theme_style *styles = realloc(theme->styles,
	    (theme->style_count + 1) * sizeof(*styles));
	if (styles == NULL)
		return NULL;
	theme->styles = styles;
	struct syncterm_theme_style *style = &styles[theme->style_count];
	style->role = strdup(role);
	if (style->role == NULL)
		return NULL;
	style->font = THEME_INHERIT;
	style->legacy_attr = THEME_INHERIT;
	style->foreground = THEME_INHERIT;
	style->background = THEME_INHERIT;
	theme->style_count++;
	return style;
}

static struct syncterm_theme_glyph *
add_glyph(struct syncterm_theme *theme, const char *name)
{
	struct syncterm_theme_glyph *glyphs = realloc(theme->glyphs,
	    (theme->glyph_count + 1) * sizeof(*glyphs));
	if (glyphs == NULL)
		return NULL;
	theme->glyphs = glyphs;
	struct syncterm_theme_glyph *glyph = &glyphs[theme->glyph_count];
	glyph->name = strdup(name);
	if (glyph->name == NULL)
		return NULL;
	glyph->cp437 = 0;
	glyph->ascii = 0;
	theme->glyph_count++;
	return glyph;
}

static bool
replace_string(char **dest, const char *source)
{
	char *copy = dup_string(source);
	if (source != NULL && copy == NULL)
		return false;
	free(*dest);
	*dest = copy;
	return true;
}

static struct syncterm_theme *
build_default_theme(void)
{
	struct syncterm_theme *theme = calloc(1, sizeof(*theme));
	if (theme == NULL)
		return NULL;
	theme->name = strdup("SyncTERM Default");
	if (theme->name == NULL)
		goto error;
	theme->styles = calloc(sizeof(default_styles) / sizeof(default_styles[0]),
	    sizeof(*theme->styles));
	if (theme->styles == NULL)
		goto error;
	for (size_t i = 0; i < sizeof(default_styles) / sizeof(default_styles[0]); i++) {
		theme->styles[i].role = strdup(default_styles[i].role);
		if (theme->styles[i].role == NULL)
			goto error;
		theme->styles[i].font = default_styles[i].font;
		theme->styles[i].legacy_attr = default_styles[i].legacy_attr;
		theme->styles[i].foreground = default_styles[i].foreground;
		theme->styles[i].background = default_styles[i].background;
		theme->style_count++;
	}
	theme->glyphs = calloc(sizeof(default_glyphs) / sizeof(default_glyphs[0]),
	    sizeof(*theme->glyphs));
	if (theme->glyphs == NULL)
		goto error;
	for (size_t i = 0; i < sizeof(default_glyphs) / sizeof(default_glyphs[0]); i++) {
		theme->glyphs[i].name = strdup(default_glyphs[i].name);
		if (theme->glyphs[i].name == NULL)
			goto error;
		theme->glyphs[i].cp437 = default_glyphs[i].cp437;
		theme->glyphs[i].ascii = default_glyphs[i].ascii;
		theme->glyph_count++;
	}
	return theme;

error:
	syncterm_theme_free(theme);
	return NULL;
}

static int
classic_color(unsigned value, unsigned sentinel, int fallback)
{
	return value == sentinel ? fallback : (int)value;
}

static bool
set_complete_style(struct syncterm_theme *theme, const char *role,
    int foreground, int background)
{
	static const int rgb[] = {
		0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
		0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
		0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
		0xff5555, 0xff55ff, 0xffff55, 0xffffff,
	};
	struct syncterm_theme_style *style = find_style(theme, role);
	if (style == NULL)
		style = add_style(theme, role);
	if (style == NULL)
		return false;
	style->font = 0;
	style->legacy_attr = (background << 4) | foreground;
	style->foreground = rgb[foreground];
	style->background = rgb[background];
	return true;
}

static struct syncterm_theme *
build_classic_theme(const struct syncterm_settings *settings)
{
	struct syncterm_theme *theme = clone_theme(default_theme);
	if (theme == NULL)
		return NULL;
	if (!replace_string(&theme->filename, "") ||
	    !replace_string(&theme->name, "Classic Theme") ||
	    !replace_string(&theme->author, "SyncTERM") ||
	    !replace_string(&theme->description,
	    "The built-in SyncTERM theme using the configured Classic colours"))
		goto error;

	int frame = classic_color(settings->theme_frame_color, 16, 14);
	int text = classic_color(settings->theme_text_color, 16, 15);
	int background = classic_color(settings->theme_background_color, 8, 1);
	int inverse = classic_color(settings->theme_inverse_color, 8, 3);
	int lightbar = classic_color(settings->theme_lightbar_color, 16, 1);
	int lightbar_background = classic_color(
	    settings->theme_lightbar_background_color, 8, 7);

	static const char *normal[] = {
		"default", "menu.item", "list.item", "button", "checkbox",
		"radio.item", "help", "help.bullet",
	};
	static const char *inactive_roles[] = {
		"default.inactive", "frame.inactive", "title.inactive",
	};
	static const char *selected[] = {
		"menu.item.focused", "list.item.focused", "input",
		"input.focused", "button.focused", "button.focused.hotkey",
		"checkbox.focused", "radio.item.focused", "spinbox",
		"spinbox.focused",
	};
	for (size_t i = 0; i < sizeof(normal) / sizeof(normal[0]); i++) {
		if (!set_complete_style(theme, normal[i], text, background))
			goto error;
	}
	for (size_t i = 0; i < sizeof(inactive_roles) / sizeof(inactive_roles[0]); i++) {
		if (!set_complete_style(theme, inactive_roles[i], text, inverse))
			goto error;
	}
	for (size_t i = 0; i < sizeof(selected) / sizeof(selected[0]); i++) {
		if (!set_complete_style(theme, selected[i], lightbar,
		    lightbar_background))
			goto error;
	}
	if (!set_complete_style(theme, "list.item.focused.inactive", frame,
	    inverse) || !set_complete_style(theme, "frame", frame, background) ||
	    !set_complete_style(theme, "title", frame, background) ||
	    !set_complete_style(theme, "button.hotkey", frame, background) ||
	    !set_complete_style(theme, "statusbar", 0, inverse) ||
	    !set_complete_style(theme, "classic.backdrop", inverse, background) ||
	    !set_complete_style(theme, "classic.header", background, inverse) ||
	    !set_complete_style(theme, "classic.console", 14, inverse) ||
	    !set_complete_style(theme, "classic.console.error", 12, inverse) ||
	    !set_complete_style(theme, "classic.comment", text, inverse) ||
	    !set_complete_style(theme, "classic.hint", 0, inverse) ||
	    !set_complete_style(theme, "classic.hotkey", background, inverse) ||
	    !set_complete_style(theme, "help.bold", frame, background) ||
	    !set_complete_style(theme, "help.code", frame, background) ||
	    !set_complete_style(theme, "help.heading.1", background, inverse) ||
	    !set_complete_style(theme, "help.heading.2", background, inverse) ||
	    !set_complete_style(theme, "help.heading.3", background, inverse))
		goto error;
	return theme;

error:
	syncterm_theme_free(theme);
	return NULL;
}

static bool
parse_number(const char *value, long minimum, long maximum, bool inherit,
    int *result)
{
	if (inherit && stricmp(value, "inherit") == 0) {
		*result = THEME_INHERIT;
		return true;
	}
	errno = 0;
	char *end;
	long parsed = strtol(value, &end, 0);
	while (isspace((unsigned char)*end))
		end++;
	if (errno != 0 || end == value || *end != '\0' || parsed < minimum ||
	    parsed > maximum)
		return false;
	*result = (int)parsed;
	return true;
}

static bool
parse_metadata(str_list_t ini, struct syncterm_theme *theme, char *error,
    size_t error_size)
{
	named_string_t **values = iniGetNamedStringList(ini, ROOT_SECTION);
	bool seen_name = false;
	bool seen_author = false;
	bool seen_description = false;
	bool seen_version = false;
	if (values != NULL) {
		for (size_t i = 0; values[i] != NULL; i++) {
			char **destination = NULL;
			bool *seen = NULL;
			if (stricmp(values[i]->name, "Name") == 0) {
				destination = &theme->name;
				seen = &seen_name;
			}
			else if (stricmp(values[i]->name, "Author") == 0) {
				destination = &theme->author;
				seen = &seen_author;
			}
			else if (stricmp(values[i]->name, "Description") == 0) {
				destination = &theme->description;
				seen = &seen_description;
			}
			else if (stricmp(values[i]->name, "Version") == 0) {
				destination = &theme->version;
				seen = &seen_version;
			}
			if (destination == NULL)
				continue;
			if (*seen) {
				set_error(error, error_size, "duplicate metadata key: %s",
				    values[i]->name);
				iniFreeNamedStringList(values);
				return false;
			}
			*seen = true;
			if (!replace_string(destination, values[i]->value)) {
				set_error(error, error_size, "out of memory");
				iniFreeNamedStringList(values);
				return false;
			}
		}
		iniFreeNamedStringList(values);
	}
	if (!seen_name || theme->name == NULL || theme->name[0] == '\0') {
		set_error(error, error_size, "missing required Name metadata");
		return false;
	}
	return true;
}

static bool
parse_style_section(str_list_t ini, const char *section,
    struct syncterm_theme *theme, char *error, size_t error_size)
{
	const char *role = section + strlen("Style:");
	if (*role == '\0') {
		set_error(error, error_size, "empty Style role");
		return false;
	}
	struct syncterm_theme_style *style = find_style(theme, role);
	if (style == NULL)
		style = add_style(theme, role);
	if (style == NULL) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	named_string_t **values = iniGetNamedStringList(ini, section);
	unsigned seen = 0;
	for (size_t i = 0; values != NULL && values[i] != NULL; i++) {
		unsigned bit = 0;
		int *destination = NULL;
		long maximum = 0;
		if (stricmp(values[i]->name, "Font") == 0) {
			bit = 1u << 0;
			destination = &style->font;
			maximum = 255;
		}
		else if (stricmp(values[i]->name, "LegacyAttr") == 0) {
			bit = 1u << 1;
			destination = &style->legacy_attr;
			maximum = 255;
		}
		else if (stricmp(values[i]->name, "Foreground") == 0) {
			bit = 1u << 2;
			destination = &style->foreground;
			maximum = 0xffffff;
		}
		else if (stricmp(values[i]->name, "Background") == 0) {
			bit = 1u << 3;
			destination = &style->background;
			maximum = 0xffffff;
		}
		if (destination == NULL)
			continue;
		if ((seen & bit) != 0) {
			set_error(error, error_size, "duplicate %s in [%s]",
			    values[i]->name, section);
			iniFreeNamedStringList(values);
			return false;
		}
		seen |= bit;
		int parsed;
		if (!parse_number(values[i]->value, 0, maximum, true, &parsed)) {
			set_error(error, error_size, "invalid %s in [%s]",
			    values[i]->name, section);
			iniFreeNamedStringList(values);
			return false;
		}
		*destination = parsed;
	}
	iniFreeNamedStringList(values);
	return true;
}

static bool
parse_glyph_section(str_list_t ini, const char *section,
    struct syncterm_theme *theme, char *error, size_t error_size)
{
	const char *name = section + strlen("Glyph:");
	if (*name == '\0') {
		set_error(error, error_size, "empty Glyph name");
		return false;
	}
	struct syncterm_theme_glyph *glyph = find_glyph(theme, name);
	bool is_new = glyph == NULL;
	if (glyph == NULL)
		glyph = add_glyph(theme, name);
	if (glyph == NULL) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	named_string_t **values = iniGetNamedStringList(ini, section);
	unsigned seen = 0;
	for (size_t i = 0; values != NULL && values[i] != NULL; i++) {
		unsigned bit = 0;
		uint8_t *destination = NULL;
		long minimum = 0;
		long maximum = 255;
		if (stricmp(values[i]->name, "CP437") == 0) {
			bit = 1u << 0;
			destination = &glyph->cp437;
		}
		else if (stricmp(values[i]->name, "ASCII") == 0) {
			bit = 1u << 1;
			destination = &glyph->ascii;
			minimum = 0x20;
			maximum = 0x7e;
		}
		if (destination == NULL)
			continue;
		if ((seen & bit) != 0) {
			set_error(error, error_size, "duplicate %s in [%s]",
			    values[i]->name, section);
			iniFreeNamedStringList(values);
			return false;
		}
		seen |= bit;
		int parsed;
		if (!parse_number(values[i]->value, minimum, maximum, false,
		    &parsed)) {
			set_error(error, error_size, "invalid %s in [%s]",
			    values[i]->name, section);
			iniFreeNamedStringList(values);
			return false;
		}
		*destination = (uint8_t)parsed;
	}
	iniFreeNamedStringList(values);
	if (is_new && seen != 3) {
		set_error(error, error_size,
		    "new glyph [%s] requires CP437 and ASCII", section);
		return false;
	}
	return true;
}

static bool
default_style_complete(const struct syncterm_theme *theme)
{
	for (size_t i = 0; i < theme->style_count; i++) {
		const struct syncterm_theme_style *style = &theme->styles[i];
		if (strcmp(style->role, "default") != 0)
			continue;
		return style->font != THEME_INHERIT &&
		    style->legacy_attr != THEME_INHERIT &&
		    style->foreground != THEME_INHERIT &&
		    style->background != THEME_INHERIT;
	}
	return false;
}

bool
syncterm_theme_load_path(const char *path, const char *filename,
    struct syncterm_theme **result, char *error, size_t error_size)
{
	*result = NULL;
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		set_error(error, error_size, "unable to open: %s", strerror(errno));
		return false;
	}
	str_list_t ini = iniReadFile(fp);
	bool close_ok = fclose(fp) == 0;
	if (ini == NULL || !close_ok) {
		strListFree(&ini);
		set_error(error, error_size, "unable to read theme file");
		return false;
	}
	struct syncterm_theme *theme = clone_theme(default_theme);
	if (theme == NULL) {
		strListFree(&ini);
		set_error(error, error_size, "out of memory");
		return false;
	}
	free(theme->filename);
	theme->filename = strdup(filename);
	free(theme->name);
	theme->name = NULL;
	free(theme->author);
	theme->author = NULL;
	free(theme->description);
	theme->description = NULL;
	free(theme->version);
	theme->version = NULL;
	if (theme->filename == NULL ||
	    !parse_metadata(ini, theme, error, error_size))
		goto failed;

	str_list_t sections = iniGetSectionListWithDupes(ini, NULL);
	for (size_t i = 0; sections != NULL && sections[i] != NULL; i++) {
		bool recognized = strnicmp(sections[i], "Style:", 6) == 0 ||
		    strnicmp(sections[i], "Glyph:", 6) == 0;
		if (!recognized)
			continue;
		for (size_t j = 0; j < i; j++) {
			if (stricmp(sections[i], sections[j]) == 0) {
				set_error(error, error_size, "duplicate section [%s]",
				    sections[i]);
				strListFree(&sections);
				goto failed;
			}
		}
		bool ok = strnicmp(sections[i], "Style:", 6) == 0 ?
		    parse_style_section(ini, sections[i], theme, error, error_size) :
		    parse_glyph_section(ini, sections[i], theme, error, error_size);
		if (!ok) {
			strListFree(&sections);
			goto failed;
		}
	}
	strListFree(&sections);
	strListFree(&ini);
	if (!default_style_complete(theme)) {
		set_error(error, error_size, "the default style must be complete");
		syncterm_theme_free(theme);
		return false;
	}
	*result = theme;
	return true;

failed:
	strListFree(&ini);
	syncterm_theme_free(theme);
	return false;
}

bool
syncterm_theme_valid_filename(const char *filename)
{
	if (filename == NULL || filename[0] == '\0')
		return false;
	if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 ||
	    strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL)
		return false;
	const char *extension = strrchr(filename, '.');
	return extension != NULL && stricmp(extension, ".ini") == 0;
}

bool
syncterm_theme_valid_package(const char *package)
{
	static const char prefix[] = SYNCTERM_THEME_REPOSITORY "/";
	if (package == NULL || strncmp(package, prefix, sizeof(prefix) - 1) != 0)
		return false;
	const char *id = package + sizeof(prefix) - 1;
	if (id[0] == '\0' || strcmp(id, ".") == 0 || strcmp(id, "..") == 0)
		return false;
	for (const unsigned char *p = (const unsigned char *)id; *p; p++) {
		if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') &&
		    *p != '.' && *p != '_' && *p != '-')
			return false;
	}
	return true;
}

bool
syncterm_theme_package_path(const char *package, char *path,
    size_t path_size)
{
	if (!syncterm_theme_valid_package(package) || path == NULL || path_size == 0)
		return false;
	char cache[MAX_PATH + 1];
	if (get_syncterm_filename(cache, sizeof(cache),
	    SYNCTERM_PATH_SYSTEM_CACHE, false) == NULL)
		return false;
	const char *id = strchr(package, '/') + 1;
	return snprintf(path, path_size, "%s/themes/%s/%s.ini", cache,
	    SYNCTERM_THEME_REPOSITORY, id) < (int)path_size;
}

static bool
load_theme_file(const char *filename, struct syncterm_theme **result,
    char *error, size_t error_size)
{
	if (!syncterm_theme_valid_filename(filename)) {
		set_error(error, error_size, "invalid theme filename");
		return false;
	}
	char directory[MAX_PATH + 1];
	if (get_syncterm_filename(directory, sizeof(directory),
	    SYNCTERM_PATH_THEMES, false) == NULL) {
		set_error(error, error_size, "themes directory is unavailable");
		return false;
	}
	char path[MAX_PATH + 1];
	if (snprintf(path, sizeof(path), "%s%s", directory, filename) >=
	    (int)sizeof(path)) {
		set_error(error, error_size, "theme path is too long");
		return false;
	}
	return syncterm_theme_load_path(path, filename, result, error, error_size);
}

bool
syncterm_theme_load_package(const char *package,
    struct syncterm_theme **result, char *error, size_t error_size)
{
	char path[MAX_PATH + 1];
	if (!syncterm_theme_package_path(package, path, sizeof(path))) {
		set_error(error, error_size, "invalid theme package");
		return false;
	}
	struct syncterm_theme *theme = NULL;
	if (!syncterm_theme_load_path(path, "", &theme, error, error_size))
		return false;
	theme->package = strdup(package);
	if (theme->package == NULL) {
		set_error(error, error_size, "out of memory");
		syncterm_theme_free(theme);
		return false;
	}
	*result = theme;
	return true;
}

static bool
same_filename(const char *left, const char *right)
{
#ifdef _WIN32
	return stricmp(left, right) == 0;
#else
	return strcmp(left, right) == 0;
#endif
}

bool
syncterm_theme_init(const struct syncterm_settings *settings)
{
	default_theme = build_default_theme();
	if (default_theme == NULL)
		return false;
	committed_theme = build_classic_theme(settings);
	if (committed_theme == NULL) {
		syncterm_theme_shutdown();
		return false;
	}
	if (settings->theme_package[0] != '\0') {
		struct syncterm_theme *selected;
		char error[256];
		if (syncterm_theme_load_package(settings->theme_package, &selected,
		    error, sizeof(error))) {
			syncterm_theme_free(committed_theme);
			committed_theme = selected;
		}
	}
	else if (settings->theme_file[0] != '\0') {
		struct syncterm_theme *selected;
		char error[256];
		if (load_theme_file(settings->theme_file, &selected, error,
		    sizeof(error))) {
			syncterm_theme_free(committed_theme);
			committed_theme = selected;
		}
	}
	return true;
}

const struct syncterm_theme *
syncterm_theme_active(void)
{
	return preview_theme != NULL ? preview_theme : committed_theme;
}

const struct syncterm_theme *
syncterm_theme_default(void)
{
	return default_theme;
}

uint64_t
syncterm_theme_generation(void)
{
	return theme_generation;
}

bool
syncterm_theme_prepare(const struct syncterm_settings *settings,
    bool force_reload, struct syncterm_theme **result, char *error,
    size_t error_size)
{
	*result = NULL;
	if (!force_reload && committed_theme != NULL &&
	    same_filename(settings->theme_file,
	    committed_theme->filename != NULL ? committed_theme->filename : "") &&
	    strcmp(settings->theme_package,
	    committed_theme->package != NULL ? committed_theme->package : "") == 0)
		return true;
	if (settings->theme_package[0] != '\0')
		return syncterm_theme_load_package(settings->theme_package, result,
		    error, error_size);
	if (settings->theme_file[0] == '\0') {
		*result = build_classic_theme(settings);
		if (*result == NULL) {
			set_error(error, error_size, "out of memory");
			return false;
		}
		return true;
	}
	return load_theme_file(settings->theme_file, result, error, error_size);
}

void
syncterm_theme_install(struct syncterm_theme *theme)
{
	if (theme == NULL)
		return;
	syncterm_theme_free(committed_theme);
	committed_theme = theme;
	syncterm_theme_free(preview_theme);
	preview_theme = NULL;
	theme_generation++;
}

static void
free_catalog(void)
{
	for (size_t i = 0; i < catalog_count; i++) {
		free(catalog[i].filename);
		free(catalog[i].name);
		free(catalog[i].author);
		free(catalog[i].description);
		free(catalog[i].version);
		free(catalog[i].error);
		syncterm_theme_free(catalog[i].theme);
	}
	free(catalog);
	catalog = NULL;
	catalog_count = 0;
}

void
syncterm_theme_shutdown(void)
{
	free_catalog();
	syncterm_theme_free(preview_theme);
	preview_theme = NULL;
	syncterm_theme_free(committed_theme);
	committed_theme = NULL;
	syncterm_theme_free(default_theme);
	default_theme = NULL;
}

static bool
append_catalog(const char *filename, const char *name, const char *author,
    const char *description, const char *version, const char *error,
    struct syncterm_theme *theme)
{
	struct syncterm_theme_catalog_entry *entries = realloc(catalog,
	    (catalog_count + 1) * sizeof(*entries));
	if (entries == NULL)
		return false;
	catalog = entries;
	struct syncterm_theme_catalog_entry *entry = &catalog[catalog_count];
	memset(entry, 0, sizeof(*entry));
	entry->filename = strdup(filename != NULL ? filename : "");
	entry->name = strdup(name != NULL ? name : filename);
	entry->author = dup_string(author);
	entry->description = dup_string(description);
	entry->version = dup_string(version);
	entry->error = dup_string(error);
	entry->theme = theme;
	if (entry->filename == NULL || entry->name == NULL ||
	    (author != NULL && entry->author == NULL) ||
	    (description != NULL && entry->description == NULL) ||
	    (version != NULL && entry->version == NULL) ||
	    (error != NULL && entry->error == NULL)) {
		free(entry->filename);
		free(entry->name);
		free(entry->author);
		free(entry->description);
		free(entry->version);
		free(entry->error);
		syncterm_theme_free(theme);
		memset(entry, 0, sizeof(*entry));
		return false;
	}
	catalog_count++;
	return true;
}

static int
catalog_compare(const void *a, const void *b)
{
	const struct syncterm_theme_catalog_entry *left = a;
	const struct syncterm_theme_catalog_entry *right = b;
	if ((left->error == NULL) != (right->error == NULL))
		return left->error == NULL ? -1 : 1;
	int result = stricmp(left->name, right->name);
	if (result != 0)
		return result;
	return stricmp(left->filename, right->filename);
}

static const struct syncterm_theme_catalog_entry *
find_catalog_entry(const char *filename)
{
	for (size_t i = 0; i < catalog_count; i++) {
		if (same_filename(catalog[i].filename, filename))
			return &catalog[i];
	}
	return NULL;
}

bool
syncterm_theme_catalog_refresh(const char *selected_filename)
{
	syncterm_theme_cancel_preview();
	free_catalog();
	struct syncterm_theme *classic = build_classic_theme(&settings);
	if (classic == NULL || !append_catalog("", "Classic Theme", "SyncTERM",
	    "The built-in theme using the configured Classic colours", NULL,
	    NULL, classic))
		return false;

	char directory[MAX_PATH + 1];
	if (get_syncterm_filename(directory, sizeof(directory),
	    SYNCTERM_PATH_THEMES, false) == NULL) {
		return append_catalog("themes", "Themes directory", NULL, NULL,
		    NULL, "the themes directory is unavailable", NULL);
	}
	char pattern[MAX_PATH + 1];
	if (snprintf(pattern, sizeof(pattern), "%s*", directory) >=
	    (int)sizeof(pattern))
		return false;
	glob_t files = {0};
	if (glob(pattern, 0, NULL, &files) == 0) {
		for (size_t i = 0; i < files.gl_pathc; i++) {
			if (isdir(files.gl_pathv[i]))
				continue;
			const char *filename = getfname(files.gl_pathv[i]);
			if (!syncterm_theme_valid_filename(filename))
				continue;
			struct syncterm_theme *theme;
			char error[256] = "";
			if (syncterm_theme_load_path(files.gl_pathv[i], filename, &theme,
			    error, sizeof(error))) {
				if (!append_catalog(filename, theme->name, theme->author,
				    theme->description, theme->version, NULL, theme)) {
					globfree(&files);
					return false;
				}
			}
			else if (!append_catalog(filename, filename, NULL, NULL, NULL,
			    error, NULL)) {
				globfree(&files);
				return false;
			}
		}
		globfree(&files);
	}
	if (selected_filename != NULL && selected_filename[0] != '\0' &&
	    find_catalog_entry(selected_filename) == NULL &&
	    !append_catalog(selected_filename, selected_filename, NULL, NULL,
	    NULL, "the configured theme file does not exist", NULL))
		return false;
	if (catalog_count > 2)
		qsort(catalog + 1, catalog_count - 1, sizeof(*catalog),
		    catalog_compare);
	return true;
}

size_t
syncterm_theme_catalog_count(void)
{
	return catalog_count;
}

const struct syncterm_theme_catalog_entry *
syncterm_theme_catalog_entry(size_t index)
{
	return index < catalog_count ? &catalog[index] : NULL;
}

const char *
syncterm_theme_preview_loaded(const struct syncterm_theme *source)
{
	static char error[256];
	if (source == NULL) {
		snprintf(error, sizeof(error), "theme is unavailable");
		return error;
	}
	struct syncterm_theme *theme = clone_theme(source);
	if (theme == NULL) {
		snprintf(error, sizeof(error), "out of memory");
		return error;
	}
	syncterm_theme_free(preview_theme);
	preview_theme = theme;
	theme_generation++;
	return NULL;
}

const char *
syncterm_theme_preview(const char *filename)
{
	static char error[256];
	const struct syncterm_theme_catalog_entry *entry =
	    find_catalog_entry(filename != NULL ? filename : "");
	if (entry == NULL) {
		snprintf(error, sizeof(error), "theme is not in the current catalog");
		return error;
	}
	if (entry->error != NULL)
		return entry->error;
	return syncterm_theme_preview_loaded(entry->theme);
}

void
syncterm_theme_cancel_preview(void)
{
	if (preview_theme == NULL)
		return;
	syncterm_theme_free(preview_theme);
	preview_theme = NULL;
	theme_generation++;
}

bool
syncterm_theme_prepare_catalog_selection(const char *filename,
    struct syncterm_theme **result, char *error, size_t error_size)
{
	*result = NULL;
	if (filename == NULL)
		filename = "";
	if (preview_theme != NULL && preview_theme->package == NULL &&
	    preview_theme->filename != NULL &&
	    same_filename(preview_theme->filename, filename)) {
		*result = clone_theme(preview_theme);
		if (*result != NULL)
			return true;
		set_error(error, error_size, "out of memory");
		return false;
	}
	const struct syncterm_theme_catalog_entry *entry =
	    find_catalog_entry(filename);
	if (entry == NULL) {
		set_error(error, error_size, "theme is not in the current catalog");
		return false;
	}
	if (entry->error != NULL) {
		set_error(error, error_size, "%s", entry->error);
		return false;
	}
	*result = clone_theme(entry->theme);
	if (*result == NULL) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

static const char *theme_metadata_keys[] = {
	"Name", "Author", "Description", "Version"
};

static const char *theme_style_keys[] = {
	"Font", "LegacyAttr", "Foreground", "Background"
};

static const char *theme_glyph_keys[] = {
	"CP437", "ASCII"
};

static void
free_document_style(struct theme_document_style *style)
{
	free(style->role);
	memset(style, 0, sizeof(*style));
}

static void
free_document_glyph(struct theme_document_glyph *glyph)
{
	free(glyph->name);
	memset(glyph, 0, sizeof(*glyph));
}

static bool
copy_document_style(struct theme_document_style *destination,
    const struct theme_document_style *source)
{
	*destination = *source;
	destination->role = strdup(source->role);
	return destination->role != NULL;
}

static bool
copy_document_glyph(struct theme_document_glyph *destination,
    const struct theme_document_glyph *source)
{
	*destination = *source;
	destination->name = strdup(source->name);
	return destination->name != NULL;
}

static void
free_document_arrays(struct theme_document_style *styles, size_t style_count,
    struct theme_document_glyph *glyphs, size_t glyph_count)
{
	for (size_t i = 0; i < style_count; i++)
		free_document_style(&styles[i]);
	free(styles);
	for (size_t i = 0; i < glyph_count; i++)
		free_document_glyph(&glyphs[i]);
	free(glyphs);
}

static bool
copy_document_arrays(const struct theme_document_style *source_styles,
    size_t source_style_count, const struct theme_document_glyph *source_glyphs,
    size_t source_glyph_count, struct theme_document_style **styles,
    size_t *style_count, struct theme_document_glyph **glyphs,
    size_t *glyph_count)
{
	*styles = NULL;
	*style_count = 0;
	*glyphs = NULL;
	*glyph_count = 0;
	if (source_style_count > 0) {
		*styles = calloc(source_style_count, sizeof(**styles));
		if (*styles == NULL)
			return false;
		for (size_t i = 0; i < source_style_count; i++) {
			if (!copy_document_style(&(*styles)[i], &source_styles[i]))
				goto failed;
			(*style_count)++;
		}
	}
	if (source_glyph_count > 0) {
		*glyphs = calloc(source_glyph_count, sizeof(**glyphs));
		if (*glyphs == NULL)
			goto failed;
		for (size_t i = 0; i < source_glyph_count; i++) {
			if (!copy_document_glyph(&(*glyphs)[i], &source_glyphs[i]))
				goto failed;
			(*glyph_count)++;
		}
	}
	return true;

failed:
	free_document_arrays(*styles, *style_count, *glyphs, *glyph_count);
	*styles = NULL;
	*style_count = 0;
	*glyphs = NULL;
	*glyph_count = 0;
	return false;
}

static struct theme_document_style *
document_find_style(struct syncterm_theme_document *document,
    const char *role)
{
	for (size_t i = 0; i < document->style_count; i++) {
		if (strcmp(document->styles[i].role, role) == 0)
			return &document->styles[i];
	}
	return NULL;
}

static struct theme_document_glyph *
document_find_glyph(struct syncterm_theme_document *document,
    const char *name)
{
	for (size_t i = 0; i < document->glyph_count; i++) {
		if (strcmp(document->glyphs[i].name, name) == 0)
			return &document->glyphs[i];
	}
	return NULL;
}

static bool
valid_custom_name(const char *name)
{
	if (name == NULL || name[0] == '\0')
		return false;
	bool segment = false;
	for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
		if (*p == '.') {
			if (!segment)
				return false;
			segment = false;
			continue;
		}
		if (!isalnum(*p) && *p != '_' && *p != '-')
			return false;
		segment = true;
	}
	return segment;
}

static bool
document_rebuild_preview(struct syncterm_theme_document *document,
    char *error, size_t error_size)
{
	struct syncterm_theme *theme = clone_theme(default_theme);
	if (theme == NULL) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	if (!replace_string(&theme->filename,
	    document->filename != NULL ? document->filename : "") ||
	    !replace_string(&theme->name,
	    document->metadata[SYNCTERM_THEME_METADATA_NAME])) {
		set_error(error, error_size, "out of memory");
		goto failed;
	}
	free(theme->author);
	theme->author = dup_string(document->metadata[SYNCTERM_THEME_METADATA_AUTHOR]);
	free(theme->description);
	theme->description = dup_string(
	    document->metadata[SYNCTERM_THEME_METADATA_DESCRIPTION]);
	free(theme->version);
	theme->version = dup_string(
	    document->metadata[SYNCTERM_THEME_METADATA_VERSION]);
	if ((document->metadata[SYNCTERM_THEME_METADATA_AUTHOR] != NULL &&
	    theme->author == NULL) ||
	    (document->metadata[SYNCTERM_THEME_METADATA_DESCRIPTION] != NULL &&
	    theme->description == NULL) ||
	    (document->metadata[SYNCTERM_THEME_METADATA_VERSION] != NULL &&
	    theme->version == NULL)) {
		set_error(error, error_size, "out of memory");
		goto failed;
	}
	for (size_t i = 0; i < document->style_count; i++) {
		const struct theme_document_style *source = &document->styles[i];
		if (source->deleted)
			continue;
		struct syncterm_theme_style *style = find_style(theme, source->role);
		if (style == NULL)
			style = add_style(theme, source->role);
		if (style == NULL) {
			set_error(error, error_size, "out of memory");
			goto failed;
		}
		style->font = source->values[SYNCTERM_THEME_STYLE_FONT];
		style->legacy_attr =
		    source->values[SYNCTERM_THEME_STYLE_LEGACY_ATTR];
		style->foreground =
		    source->values[SYNCTERM_THEME_STYLE_FOREGROUND];
		style->background =
		    source->values[SYNCTERM_THEME_STYLE_BACKGROUND];
	}
	for (size_t i = 0; i < document->glyph_count; i++) {
		const struct theme_document_glyph *source = &document->glyphs[i];
		if (source->deleted)
			continue;
		struct syncterm_theme_glyph *glyph = find_glyph(theme, source->name);
		if (glyph == NULL)
			glyph = add_glyph(theme, source->name);
		if (glyph == NULL) {
			set_error(error, error_size, "out of memory");
			goto failed;
		}
		glyph->cp437 = source->values[SYNCTERM_THEME_GLYPH_CP437];
		glyph->ascii = source->values[SYNCTERM_THEME_GLYPH_ASCII];
	}
	if (!default_style_complete(theme)) {
		set_error(error, error_size, "the default style must be complete");
		goto failed;
	}
	syncterm_theme_free(document->preview);
	document->preview = theme;
	return true;

failed:
	syncterm_theme_free(theme);
	return false;
}

static bool
theme_filename_path(const char *filename, char *path, size_t path_size,
    char *error, size_t error_size)
{
	if (!syncterm_theme_valid_filename(filename)) {
		set_error(error, error_size, "invalid theme filename");
		return false;
	}
	char directory[MAX_PATH + 1];
	if (get_syncterm_filename(directory, sizeof(directory),
	    SYNCTERM_PATH_THEMES, false) == NULL) {
		set_error(error, error_size, "themes directory is unavailable");
		return false;
	}
	if (snprintf(path, path_size, "%s%s", directory, filename) >=
	    (int)path_size) {
		set_error(error, error_size, "theme path is too long");
		return false;
	}
	return true;
}

bool
syncterm_theme_delete(const char *filename, char *error, size_t error_size)
{
	if (safe_mode) {
		set_error(error, error_size, "themes cannot be deleted in safe mode");
		return false;
	}
	char path[MAX_PATH + 1];
	if (!theme_filename_path(filename, path, sizeof(path), error, error_size))
		return false;
	if (remove(path) != 0) {
		set_error(error, error_size, "unable to delete theme: %s",
		    strerror(errno));
		return false;
	}
	return true;
}

static str_list_t
read_theme_source(const char *path, char *error, size_t error_size)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		set_error(error, error_size, "unable to open: %s", strerror(errno));
		return NULL;
	}
	str_list_t source = iniReadFile(fp);
	bool close_ok = fclose(fp) == 0;
	if (source == NULL || !close_ok) {
		strListFree(&source);
		set_error(error, error_size, "unable to read theme file");
		return NULL;
	}
	return source;
}

static bool
document_populate(struct syncterm_theme_document *document,
    const struct syncterm_theme *theme, str_list_t source, bool all_present,
    char *error, size_t error_size)
{
	document->styles = calloc(theme->style_count, sizeof(*document->styles));
	document->glyphs = calloc(theme->glyph_count, sizeof(*document->glyphs));
	if ((theme->style_count > 0 && document->styles == NULL) ||
	    (theme->glyph_count > 0 && document->glyphs == NULL)) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	for (size_t i = 0; i < theme->style_count; i++) {
		const struct syncterm_theme_style *input = &theme->styles[i];
		struct theme_document_style *style = &document->styles[i];
		style->role = strdup(input->role);
		if (style->role == NULL) {
			set_error(error, error_size, "out of memory");
			return false;
		}
		style->builtin = find_style(default_theme, input->role) != NULL;
		style->values[SYNCTERM_THEME_STYLE_FONT] = input->font;
		style->values[SYNCTERM_THEME_STYLE_LEGACY_ATTR] = input->legacy_attr;
		style->values[SYNCTERM_THEME_STYLE_FOREGROUND] = input->foreground;
		style->values[SYNCTERM_THEME_STYLE_BACKGROUND] = input->background;
		char section[INI_MAX_VALUE_LEN];
		snprintf(section, sizeof(section), "Style:%s", input->role);
		for (unsigned field = 0; field < SYNCTERM_THEME_STYLE_FIELD_COUNT;
		    field++) {
			if (all_present || (source != NULL &&
			    iniKeyExists(source, section, theme_style_keys[field])))
				style->present |= 1u << field;
		}
		document->style_count++;
	}
	for (size_t i = 0; i < theme->glyph_count; i++) {
		const struct syncterm_theme_glyph *input = &theme->glyphs[i];
		struct theme_document_glyph *glyph = &document->glyphs[i];
		glyph->name = strdup(input->name);
		if (glyph->name == NULL) {
			set_error(error, error_size, "out of memory");
			return false;
		}
		glyph->builtin = find_glyph(default_theme, input->name) != NULL;
		glyph->values[SYNCTERM_THEME_GLYPH_CP437] = input->cp437;
		glyph->values[SYNCTERM_THEME_GLYPH_ASCII] = input->ascii;
		char section[INI_MAX_VALUE_LEN];
		snprintf(section, sizeof(section), "Glyph:%s", input->name);
		for (unsigned field = 0; field < SYNCTERM_THEME_GLYPH_FIELD_COUNT;
		    field++) {
			if (all_present || (source != NULL &&
			    iniKeyExists(source, section, theme_glyph_keys[field])))
				glyph->present |= 1u << field;
		}
		document->glyph_count++;
	}
	return true;
}

static void
free_document_snapshot(struct theme_document_snapshot *snapshot)
{
	free_document_arrays(snapshot->styles, snapshot->style_count,
	    snapshot->glyphs, snapshot->glyph_count);
	strListFree(&snapshot->imported_sections);
	memset(snapshot, 0, sizeof(*snapshot));
}

static bool
copy_document_snapshot(struct theme_document_snapshot *destination,
    const struct syncterm_theme_document *source)
{
	memset(destination, 0, sizeof(*destination));
	if (!copy_document_arrays(source->styles, source->style_count,
	    source->glyphs, source->glyph_count, &destination->styles,
	    &destination->style_count, &destination->glyphs,
	    &destination->glyph_count))
		return false;
	if (source->imported_sections != NULL) {
		destination->imported_sections = strListDup(
		    source->imported_sections);
		if (destination->imported_sections == NULL) {
			free_document_snapshot(destination);
			return false;
		}
	}
	destination->replace_definitions = source->replace_definitions;
	return true;
}

static void
free_string_state(struct theme_string_state *state)
{
	free(state->value);
	memset(state, 0, sizeof(*state));
}

static void
free_history_entry(struct theme_history_entry *entry)
{
	free(entry->name);
	switch (entry->kind) {
		case THEME_HISTORY_METADATA:
			free_string_state(&entry->data.metadata.old_state);
			free_string_state(&entry->data.metadata.new_state);
			break;
		case THEME_HISTORY_STYLE:
			if (entry->old_exists)
				free_document_style(&entry->data.style.old_state);
			if (entry->new_exists)
				free_document_style(&entry->data.style.new_state);
			break;
		case THEME_HISTORY_GLYPH:
			if (entry->old_exists)
				free_document_glyph(&entry->data.glyph.old_state);
			if (entry->new_exists)
				free_document_glyph(&entry->data.glyph.new_state);
			break;
		case THEME_HISTORY_IMPORT:
			free_document_snapshot(&entry->data.import.old_state);
			free_document_snapshot(&entry->data.import.new_state);
			break;
	}
	memset(entry, 0, sizeof(*entry));
}

static void
document_clear_history(struct syncterm_theme_document *document)
{
	for (size_t i = 0; i < document->history_count; i++)
		free_history_entry(&document->history[i]);
	free(document->history);
	document->history = NULL;
	document->history_count = 0;
	document->history_position = 0;
}

static bool
document_push_history(struct syncterm_theme_document *document,
    struct theme_history_entry *entry)
{
	for (size_t i = document->history_position; i < document->history_count;
	    i++)
		free_history_entry(&document->history[i]);
	document->history_count = document->history_position;
	struct theme_history_entry *history = realloc(document->history,
	    (document->history_count + 1) * sizeof(*history));
	if (history == NULL)
		return false;
	document->history = history;
	document->history[document->history_count] = *entry;
	memset(entry, 0, sizeof(*entry));
	document->history_count++;
	document->history_position = document->history_count;
	return true;
}

static bool
copy_string_state(struct theme_string_state *destination, const char *value,
    bool present, bool modified)
{
	memset(destination, 0, sizeof(*destination));
	destination->value = dup_string(value);
	if (value != NULL && destination->value == NULL)
		return false;
	destination->present = present;
	destination->modified = modified;
	return true;
}

static bool
document_set_metadata_state(struct syncterm_theme_document *document,
    unsigned field, const struct theme_string_state *state)
{
	char *value = dup_string(state->value);
	if (state->value != NULL && value == NULL)
		return false;
	free(document->metadata[field]);
	document->metadata[field] = value;
	if (state->present)
		document->metadata_present |= 1u << field;
	else
		document->metadata_present &= ~(1u << field);
	if (state->modified)
		document->metadata_modified |= 1u << field;
	else
		document->metadata_modified &= ~(1u << field);
	return true;
}

static size_t
document_style_index(const struct syncterm_theme_document *document,
    const char *role)
{
	for (size_t i = 0; i < document->style_count; i++) {
		if (strcmp(document->styles[i].role, role) == 0)
			return i;
	}
	return SIZE_MAX;
}

static size_t
document_glyph_index(const struct syncterm_theme_document *document,
    const char *name)
{
	for (size_t i = 0; i < document->glyph_count; i++) {
		if (strcmp(document->glyphs[i].name, name) == 0)
			return i;
	}
	return SIZE_MAX;
}

static bool
document_apply_style_state(struct syncterm_theme_document *document,
    const char *role, bool exists, const struct theme_document_style *state)
{
	size_t index = document_style_index(document, role);
	if (!exists) {
		if (index == SIZE_MAX)
			return true;
		free_document_style(&document->styles[index]);
		memmove(&document->styles[index], &document->styles[index + 1],
		    (document->style_count - index - 1) * sizeof(*document->styles));
		document->style_count--;
		return true;
	}
	struct theme_document_style copy;
	if (!copy_document_style(&copy, state))
		return false;
	if (index != SIZE_MAX) {
		free_document_style(&document->styles[index]);
		document->styles[index] = copy;
		return true;
	}
	struct theme_document_style *styles = realloc(document->styles,
	    (document->style_count + 1) * sizeof(*styles));
	if (styles == NULL) {
		free_document_style(&copy);
		return false;
	}
	document->styles = styles;
	document->styles[document->style_count++] = copy;
	return true;
}

static bool
document_apply_glyph_state(struct syncterm_theme_document *document,
    const char *name, bool exists, const struct theme_document_glyph *state)
{
	size_t index = document_glyph_index(document, name);
	if (!exists) {
		if (index == SIZE_MAX)
			return true;
		free_document_glyph(&document->glyphs[index]);
		memmove(&document->glyphs[index], &document->glyphs[index + 1],
		    (document->glyph_count - index - 1) * sizeof(*document->glyphs));
		document->glyph_count--;
		return true;
	}
	struct theme_document_glyph copy;
	if (!copy_document_glyph(&copy, state))
		return false;
	if (index != SIZE_MAX) {
		free_document_glyph(&document->glyphs[index]);
		document->glyphs[index] = copy;
		return true;
	}
	struct theme_document_glyph *glyphs = realloc(document->glyphs,
	    (document->glyph_count + 1) * sizeof(*glyphs));
	if (glyphs == NULL) {
		free_document_glyph(&copy);
		return false;
	}
	document->glyphs = glyphs;
	document->glyphs[document->glyph_count++] = copy;
	return true;
}

static bool
document_apply_snapshot(struct syncterm_theme_document *document,
    const struct theme_document_snapshot *snapshot)
{
	struct theme_document_style *styles;
	struct theme_document_glyph *glyphs;
	size_t style_count;
	size_t glyph_count;
	if (!copy_document_arrays(snapshot->styles, snapshot->style_count,
	    snapshot->glyphs, snapshot->glyph_count, &styles, &style_count,
	    &glyphs, &glyph_count))
		return false;
	str_list_t imported = NULL;
	if (snapshot->imported_sections != NULL) {
		imported = strListDup(snapshot->imported_sections);
		if (imported == NULL) {
			free_document_arrays(styles, style_count, glyphs, glyph_count);
			return false;
		}
	}
	free_document_arrays(document->styles, document->style_count,
	    document->glyphs, document->glyph_count);
	strListFree(&document->imported_sections);
	document->styles = styles;
	document->style_count = style_count;
	document->glyphs = glyphs;
	document->glyph_count = glyph_count;
	document->imported_sections = imported;
	document->replace_definitions = snapshot->replace_definitions;
	return true;
}

static bool
document_apply_history(struct syncterm_theme_document *document,
    const struct theme_history_entry *entry, bool forward)
{
	bool ok;
	switch (entry->kind) {
		case THEME_HISTORY_METADATA:
			ok = document_set_metadata_state(document, entry->field,
			    forward ? &entry->data.metadata.new_state :
			    &entry->data.metadata.old_state);
			break;
		case THEME_HISTORY_STYLE:
			ok = document_apply_style_state(document, entry->name,
			    forward ? entry->new_exists : entry->old_exists,
			    forward ? &entry->data.style.new_state :
			    &entry->data.style.old_state);
			break;
		case THEME_HISTORY_GLYPH:
			ok = document_apply_glyph_state(document, entry->name,
			    forward ? entry->new_exists : entry->old_exists,
			    forward ? &entry->data.glyph.new_state :
			    &entry->data.glyph.old_state);
			break;
		case THEME_HISTORY_IMPORT:
			ok = document_apply_snapshot(document,
			    forward ? &entry->data.import.new_state :
			    &entry->data.import.old_state);
			break;
		default:
			return false;
	}
	if (!ok)
		return false;
	return document_rebuild_preview(document, NULL, 0);
}

static bool
document_name_exists(const struct syncterm_theme_document *document,
    const char *name, bool style)
{
	if (style) {
		for (size_t i = 0; i < document->style_count; i++) {
			if (!document->styles[i].deleted &&
			    stricmp(document->styles[i].role, name) == 0)
				return true;
		}
	}
	else {
		for (size_t i = 0; i < document->glyph_count; i++) {
			if (!document->glyphs[i].deleted &&
			    stricmp(document->glyphs[i].name, name) == 0)
				return true;
		}
	}
	return false;
}

static bool
line_is_section(const char *line, char *section, size_t section_size)
{
	while (isspace((unsigned char)*line))
		line++;
	if (*line++ != '[')
		return false;
	const char *end = strchr(line, ']');
	if (end == NULL)
		return false;
	size_t length = (size_t)(end - line);
	if (length == 0 || length >= section_size)
		return false;
	memcpy(section, line, length);
	section[length] = '\0';
	return true;
}

static bool
definition_section(const char *section)
{
	return strnicmp(section, "Style:", 6) == 0 ||
	    strnicmp(section, "Glyph:", 6) == 0;
}

static bool
filter_definition_sections(str_list_t source, str_list_t *definitions,
    str_list_t *remainder)
{
	*definitions = strListInit();
	*remainder = strListInit();
	if (*definitions == NULL || *remainder == NULL)
		goto failed;
	bool copy_definition = false;
	for (size_t i = 0; source != NULL && source[i] != NULL; i++) {
		char section[INI_MAX_VALUE_LEN];
		if (line_is_section(source[i], section, sizeof(section)))
			copy_definition = definition_section(section);
		str_list_t *destination = copy_definition ? definitions : remainder;
		if (strListPush(destination, source[i]) == NULL)
			goto failed;
	}
	return true;

failed:
	strListFree(definitions);
	strListFree(remainder);
	return false;
}

static bool
source_lists_equal(str_list_t left, str_list_t right)
{
	size_t left_count = strListCount(left);
	if (left_count != strListCount(right))
		return false;
	for (size_t i = 0; i < left_count; i++) {
		if (strcmp(left[i], right[i]) != 0)
			return false;
	}
	return true;
}

struct syncterm_theme_document *
syncterm_theme_document_new(char *error, size_t error_size)
{
	if (default_theme == NULL) {
		set_error(error, error_size, "themes are not initialized");
		return NULL;
	}
	struct syncterm_theme_document *document = calloc(1, sizeof(*document));
	if (document == NULL) {
		set_error(error, error_size, "out of memory");
		return NULL;
	}
	document->filename = strdup("");
	document->source = strListInit();
	document->metadata[SYNCTERM_THEME_METADATA_NAME] = strdup("New Theme");
	document->metadata_present = 1u << SYNCTERM_THEME_METADATA_NAME;
	document->metadata_modified = 1u << SYNCTERM_THEME_METADATA_NAME;
	if (document->filename == NULL || document->source == NULL ||
	    document->metadata[SYNCTERM_THEME_METADATA_NAME] == NULL ||
	    !document_populate(document, default_theme, NULL, false, error,
	    error_size) || !document_rebuild_preview(document, error, error_size)) {
		syncterm_theme_document_free(document);
		return NULL;
	}
	return document;
}

static struct syncterm_theme_document *
theme_document_open_path(const char *path, const char *filename, char *error,
    size_t error_size)
{
	str_list_t source = read_theme_source(path, error, error_size);
	if (source == NULL)
		return NULL;
	struct syncterm_theme *theme = NULL;
	if (!syncterm_theme_load_path(path, filename, &theme, error, error_size)) {
		strListFree(&source);
		return NULL;
	}
	struct syncterm_theme_document *document = calloc(1, sizeof(*document));
	if (document == NULL) {
		set_error(error, error_size, "out of memory");
		strListFree(&source);
		syncterm_theme_free(theme);
		return NULL;
	}
	document->filename = strdup(filename);
	document->source = source;
	for (unsigned field = 0; field < SYNCTERM_THEME_METADATA_FIELD_COUNT;
	    field++) {
		const char *value = NULL;
		switch (field) {
			case SYNCTERM_THEME_METADATA_NAME:
				value = theme->name;
				break;
			case SYNCTERM_THEME_METADATA_AUTHOR:
				value = theme->author;
				break;
			case SYNCTERM_THEME_METADATA_DESCRIPTION:
				value = theme->description;
				break;
			case SYNCTERM_THEME_METADATA_VERSION:
				value = theme->version;
				break;
		}
		document->metadata[field] = dup_string(value);
		if (value != NULL && document->metadata[field] == NULL)
			goto failed;
		if (iniKeyExists(source, ROOT_SECTION, theme_metadata_keys[field]))
			document->metadata_present |= 1u << field;
	}
	if (document->filename == NULL || !document_populate(document, theme,
	    source, false, error, error_size))
		goto failed;
	document->preview = theme;
	return document;

failed:
	set_error(error, error_size, "out of memory");
	syncterm_theme_free(theme);
	syncterm_theme_document_free(document);
	return NULL;
}

struct syncterm_theme_document *
syncterm_theme_document_open(const char *filename, char *error,
    size_t error_size)
{
	char path[MAX_PATH + 1];
	if (!theme_filename_path(filename, path, sizeof(path), error, error_size))
		return NULL;
	return theme_document_open_path(path, filename, error, error_size);
}

struct syncterm_theme_document *
syncterm_theme_document_copy_path(const char *path, char *error,
    size_t error_size)
{
	if (path == NULL || path[0] == '\0') {
		set_error(error, error_size, "invalid theme path");
		return NULL;
	}
	return theme_document_open_path(path, "", error, error_size);
}

void
syncterm_theme_document_free(struct syncterm_theme_document *document)
{
	if (document == NULL)
		return;
	free(document->filename);
	strListFree(&document->source);
	for (unsigned i = 0; i < SYNCTERM_THEME_METADATA_FIELD_COUNT; i++)
		free(document->metadata[i]);
	free_document_arrays(document->styles, document->style_count,
	    document->glyphs, document->glyph_count);
	strListFree(&document->imported_sections);
	syncterm_theme_free(document->preview);
	document_clear_history(document);
	free(document);
}

const char *
syncterm_theme_document_filename(
    const struct syncterm_theme_document *document)
{
	return document != NULL ? document->filename : NULL;
}

const char *
syncterm_theme_document_metadata(
    const struct syncterm_theme_document *document, unsigned field)
{
	if (document == NULL || field >= SYNCTERM_THEME_METADATA_FIELD_COUNT)
		return NULL;
	return document->metadata[field];
}

bool
syncterm_theme_document_set_metadata(
    struct syncterm_theme_document *document, unsigned field,
    const char *value, char *error, size_t error_size)
{
	if (document == NULL || field >= SYNCTERM_THEME_METADATA_FIELD_COUNT) {
		set_error(error, error_size, "invalid metadata field");
		return false;
	}
	if (value == NULL)
		value = "";
	bool present = value[0] != '\0';
	if (field == SYNCTERM_THEME_METADATA_NAME && !present) {
		set_error(error, error_size, "Name cannot be empty");
		return false;
	}
	const char *new_value = present ? value : NULL;
	bool old_present = (document->metadata_present & (1u << field)) != 0;
	if (old_present == present &&
	    ((new_value == NULL && document->metadata[field] == NULL) ||
	    (new_value != NULL && document->metadata[field] != NULL &&
	    strcmp(new_value, document->metadata[field]) == 0)))
		return true;
	struct theme_history_entry entry = {
		.kind = THEME_HISTORY_METADATA,
		.field = field,
	};
	if (!copy_string_state(&entry.data.metadata.old_state,
	    document->metadata[field], old_present,
	    (document->metadata_modified & (1u << field)) != 0) ||
	    !copy_string_state(&entry.data.metadata.new_state, new_value, present,
	    true)) {
		free_history_entry(&entry);
		set_error(error, error_size, "out of memory");
		return false;
	}
	if (!document_set_metadata_state(document, field,
	    &entry.data.metadata.new_state) ||
	    !document_rebuild_preview(document, error, error_size)) {
		document_set_metadata_state(document, field,
		    &entry.data.metadata.old_state);
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		if (error != NULL && error[0] == '\0')
			set_error(error, error_size, "out of memory");
		return false;
	}
	if (!document_push_history(document, &entry)) {
		document_set_metadata_state(document, field,
		    &entry.data.metadata.old_state);
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

size_t
syncterm_theme_document_style_count(
    const struct syncterm_theme_document *document)
{
	if (document == NULL)
		return 0;
	size_t count = 0;
	for (size_t i = 0; i < document->style_count; i++) {
		if (!document->styles[i].deleted)
			count++;
	}
	return count;
}

bool
syncterm_theme_document_style(
    const struct syncterm_theme_document *document, size_t index,
    struct syncterm_theme_document_style *result)
{
	if (document == NULL || result == NULL)
		return false;
	for (size_t i = 0; i < document->style_count; i++) {
		const struct theme_document_style *style = &document->styles[i];
		if (style->deleted)
			continue;
		if (index-- != 0)
			continue;
		result->role = style->role;
		result->builtin = style->builtin;
		result->present = style->present;
		memcpy(result->values, style->values, sizeof(result->values));
		return true;
	}
	return false;
}

static int
style_default_value(const char *role, unsigned field)
{
	const struct syncterm_theme_style *style = find_style(default_theme, role);
	if (style == NULL)
		return THEME_INHERIT;
	switch (field) {
		case SYNCTERM_THEME_STYLE_FONT:
			return style->font;
		case SYNCTERM_THEME_STYLE_LEGACY_ATTR:
			return style->legacy_attr;
		case SYNCTERM_THEME_STYLE_FOREGROUND:
			return style->foreground;
		case SYNCTERM_THEME_STYLE_BACKGROUND:
			return style->background;
	}
	return THEME_INHERIT;
}

static bool
prepare_style_history(struct theme_history_entry *entry, const char *role,
    bool old_exists, const struct theme_document_style *old_state,
    bool new_exists, const struct theme_document_style *new_state)
{
	memset(entry, 0, sizeof(*entry));
	entry->kind = THEME_HISTORY_STYLE;
	entry->name = strdup(role);
	entry->old_exists = old_exists;
	entry->new_exists = new_exists;
	if (entry->name == NULL ||
	    (old_exists && !copy_document_style(&entry->data.style.old_state,
	    old_state)) ||
	    (new_exists && !copy_document_style(&entry->data.style.new_state,
	    new_state))) {
		free_history_entry(entry);
		return false;
	}
	return true;
}

bool
syncterm_theme_document_set_style(
    struct syncterm_theme_document *document, const char *role,
    unsigned field, enum syncterm_theme_value_mode mode, int value,
    char *error, size_t error_size)
{
	struct theme_document_style *style = document_find_style(document, role);
	if (style == NULL || style->deleted ||
	    field >= SYNCTERM_THEME_STYLE_FIELD_COUNT ||
	    mode > SYNCTERM_THEME_VALUE_EXPLICIT) {
		set_error(error, error_size, "invalid style field");
		return false;
	}
	int new_value;
	bool present;
	if (mode == SYNCTERM_THEME_VALUE_DEFAULT) {
		new_value = style->builtin ? style_default_value(role, field) :
		    THEME_INHERIT;
		present = false;
	}
	else if (mode == SYNCTERM_THEME_VALUE_INHERIT) {
		if (strcmp(role, "default") == 0) {
			set_error(error, error_size,
			    "the default style cannot inherit fields");
			return false;
		}
		new_value = THEME_INHERIT;
		present = true;
	}
	else {
		int maximum = field < SYNCTERM_THEME_STYLE_FOREGROUND ? 255 :
		    0xffffff;
		if (value < 0 || value > maximum) {
			set_error(error, error_size, "style value is out of range");
			return false;
		}
		new_value = value;
		present = true;
	}
	bool old_present = (style->present & (1u << field)) != 0;
	if (style->values[field] == new_value && old_present == present)
		return true;
	struct theme_document_style changed = *style;
	changed.values[field] = new_value;
	changed.modified |= 1u << field;
	if (present)
		changed.present |= 1u << field;
	else
		changed.present &= ~(1u << field);
	struct theme_history_entry entry;
	if (!prepare_style_history(&entry, role, true, style, true, &changed)) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	struct theme_document_style old = *style;
	*style = changed;
	if (!document_rebuild_preview(document, error, error_size) ||
	    !document_push_history(document, &entry)) {
		*style = old;
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		if (error != NULL && error[0] == '\0')
			set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

bool
syncterm_theme_document_add_style(
    struct syncterm_theme_document *document, const char *role,
    char *error, size_t error_size)
{
	if (document == NULL || !valid_custom_name(role)) {
		set_error(error, error_size, "invalid style name");
		return false;
	}
	if (document_name_exists(document, role, true)) {
		set_error(error, error_size, "style already exists");
		return false;
	}
	struct theme_document_style style = {
		.role = (char *)role,
		.created = true,
	};
	for (unsigned field = 0; field < SYNCTERM_THEME_STYLE_FIELD_COUNT;
	    field++)
		style.values[field] = THEME_INHERIT;
	struct theme_history_entry entry;
	if (!prepare_style_history(&entry, role, false, NULL, true, &style) ||
	    !document_apply_style_state(document, role, true, &style) ||
	    !document_rebuild_preview(document, error, error_size) ||
	    !document_push_history(document, &entry)) {
		document_apply_style_state(document, role, false, NULL);
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		if (error != NULL && error[0] == '\0')
			set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

bool
syncterm_theme_document_remove_style(
    struct syncterm_theme_document *document, const char *role,
    char *error, size_t error_size)
{
	struct theme_document_style *style = document_find_style(document, role);
	if (style == NULL || style->deleted) {
		set_error(error, error_size, "style does not exist");
		return false;
	}
	if (style->builtin) {
		set_error(error, error_size, "built-in styles cannot be removed");
		return false;
	}
	struct theme_document_style changed = *style;
	changed.deleted = true;
	struct theme_history_entry entry;
	if (!prepare_style_history(&entry, role, true, style, true, &changed)) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	style->deleted = true;
	if (!document_rebuild_preview(document, error, error_size) ||
	    !document_push_history(document, &entry)) {
		style->deleted = false;
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		if (error != NULL && error[0] == '\0')
			set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

size_t
syncterm_theme_document_glyph_count(
    const struct syncterm_theme_document *document)
{
	if (document == NULL)
		return 0;
	size_t count = 0;
	for (size_t i = 0; i < document->glyph_count; i++) {
		if (!document->glyphs[i].deleted)
			count++;
	}
	return count;
}

bool
syncterm_theme_document_glyph(
    const struct syncterm_theme_document *document, size_t index,
    struct syncterm_theme_document_glyph *result)
{
	if (document == NULL || result == NULL)
		return false;
	for (size_t i = 0; i < document->glyph_count; i++) {
		const struct theme_document_glyph *glyph = &document->glyphs[i];
		if (glyph->deleted)
			continue;
		if (index-- != 0)
			continue;
		result->name = glyph->name;
		result->builtin = glyph->builtin;
		result->present = glyph->present;
		memcpy(result->values, glyph->values, sizeof(result->values));
		return true;
	}
	return false;
}

static int
glyph_default_value(const char *name, unsigned field)
{
	const struct syncterm_theme_glyph *glyph = find_glyph(default_theme, name);
	if (glyph == NULL)
		return 0;
	return field == SYNCTERM_THEME_GLYPH_CP437 ? glyph->cp437 : glyph->ascii;
}

static bool
prepare_glyph_history(struct theme_history_entry *entry, const char *name,
    bool old_exists, const struct theme_document_glyph *old_state,
    bool new_exists, const struct theme_document_glyph *new_state)
{
	memset(entry, 0, sizeof(*entry));
	entry->kind = THEME_HISTORY_GLYPH;
	entry->name = strdup(name);
	entry->old_exists = old_exists;
	entry->new_exists = new_exists;
	if (entry->name == NULL ||
	    (old_exists && !copy_document_glyph(&entry->data.glyph.old_state,
	    old_state)) ||
	    (new_exists && !copy_document_glyph(&entry->data.glyph.new_state,
	    new_state))) {
		free_history_entry(entry);
		return false;
	}
	return true;
}

bool
syncterm_theme_document_set_glyph(
    struct syncterm_theme_document *document, const char *name,
    unsigned field, enum syncterm_theme_value_mode mode, int value,
    char *error, size_t error_size)
{
	struct theme_document_glyph *glyph = document_find_glyph(document, name);
	if (glyph == NULL || glyph->deleted ||
	    field >= SYNCTERM_THEME_GLYPH_FIELD_COUNT ||
	    (mode != SYNCTERM_THEME_VALUE_DEFAULT &&
	    mode != SYNCTERM_THEME_VALUE_EXPLICIT)) {
		set_error(error, error_size, "invalid glyph field");
		return false;
	}
	if (mode == SYNCTERM_THEME_VALUE_DEFAULT && !glyph->builtin) {
		set_error(error, error_size,
		    "custom glyph fields cannot be omitted");
		return false;
	}
	int minimum = field == SYNCTERM_THEME_GLYPH_ASCII ? 0x20 : 0;
	int maximum = field == SYNCTERM_THEME_GLYPH_ASCII ? 0x7e : 0xff;
	if (mode == SYNCTERM_THEME_VALUE_EXPLICIT &&
	    (value < minimum || value > maximum)) {
		set_error(error, error_size, "glyph value is out of range");
		return false;
	}
	int new_value = mode == SYNCTERM_THEME_VALUE_DEFAULT ?
	    glyph_default_value(name, field) : value;
	bool present = mode == SYNCTERM_THEME_VALUE_EXPLICIT;
	bool old_present = (glyph->present & (1u << field)) != 0;
	if (glyph->values[field] == new_value && old_present == present)
		return true;
	struct theme_document_glyph changed = *glyph;
	changed.values[field] = (uint8_t)new_value;
	changed.modified |= 1u << field;
	if (present)
		changed.present |= 1u << field;
	else
		changed.present &= ~(1u << field);
	struct theme_history_entry entry;
	if (!prepare_glyph_history(&entry, name, true, glyph, true, &changed)) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	struct theme_document_glyph old = *glyph;
	*glyph = changed;
	if (!document_rebuild_preview(document, error, error_size) ||
	    !document_push_history(document, &entry)) {
		*glyph = old;
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		if (error != NULL && error[0] == '\0')
			set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

bool
syncterm_theme_document_add_glyph(
    struct syncterm_theme_document *document, const char *name, int cp437,
    int ascii, char *error, size_t error_size)
{
	if (document == NULL || !valid_custom_name(name)) {
		set_error(error, error_size, "invalid glyph name");
		return false;
	}
	if (document_name_exists(document, name, false)) {
		set_error(error, error_size, "glyph already exists");
		return false;
	}
	if (cp437 < 0 || cp437 > 0xff || ascii < 0x20 || ascii > 0x7e) {
		set_error(error, error_size, "glyph value is out of range");
		return false;
	}
	struct theme_document_glyph glyph = {
		.name = (char *)name,
		.created = true,
		.present = (1u << SYNCTERM_THEME_GLYPH_FIELD_COUNT) - 1,
		.modified = (1u << SYNCTERM_THEME_GLYPH_FIELD_COUNT) - 1,
		.values = {(uint8_t)cp437, (uint8_t)ascii},
	};
	struct theme_history_entry entry;
	if (!prepare_glyph_history(&entry, name, false, NULL, true, &glyph) ||
	    !document_apply_glyph_state(document, name, true, &glyph) ||
	    !document_rebuild_preview(document, error, error_size) ||
	    !document_push_history(document, &entry)) {
		document_apply_glyph_state(document, name, false, NULL);
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		if (error != NULL && error[0] == '\0')
			set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

bool
syncterm_theme_document_remove_glyph(
    struct syncterm_theme_document *document, const char *name,
    char *error, size_t error_size)
{
	struct theme_document_glyph *glyph = document_find_glyph(document, name);
	if (glyph == NULL || glyph->deleted) {
		set_error(error, error_size, "glyph does not exist");
		return false;
	}
	if (glyph->builtin) {
		set_error(error, error_size, "built-in glyphs cannot be removed");
		return false;
	}
	struct theme_document_glyph changed = *glyph;
	changed.deleted = true;
	struct theme_history_entry entry;
	if (!prepare_glyph_history(&entry, name, true, glyph, true, &changed)) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	glyph->deleted = true;
	if (!document_rebuild_preview(document, error, error_size) ||
	    !document_push_history(document, &entry)) {
		glyph->deleted = false;
		document_rebuild_preview(document, NULL, 0);
		free_history_entry(&entry);
		if (error != NULL && error[0] == '\0')
			set_error(error, error_size, "out of memory");
		return false;
	}
	return true;
}

const struct syncterm_theme *
syncterm_theme_document_preview(
    const struct syncterm_theme_document *document)
{
	return document != NULL ? document->preview : NULL;
}

bool
syncterm_theme_document_dirty(
    const struct syncterm_theme_document *document)
{
	return document != NULL && (document->filename[0] == '\0' ||
	    document->history_position != 0);
}

bool
syncterm_theme_document_can_undo(
    const struct syncterm_theme_document *document)
{
	return document != NULL && document->history_position > 0;
}

bool
syncterm_theme_document_can_redo(
    const struct syncterm_theme_document *document)
{
	return document != NULL &&
	    document->history_position < document->history_count;
}

bool
syncterm_theme_document_undo(struct syncterm_theme_document *document)
{
	if (!syncterm_theme_document_can_undo(document))
		return false;
	const struct theme_history_entry *entry =
	    &document->history[document->history_position - 1];
	if (!document_apply_history(document, entry, false))
		return false;
	document->history_position--;
	return true;
}

bool
syncterm_theme_document_redo(struct syncterm_theme_document *document)
{
	if (!syncterm_theme_document_can_redo(document))
		return false;
	const struct theme_history_entry *entry =
	    &document->history[document->history_position];
	if (!document_apply_history(document, entry, true))
		return false;
	document->history_position++;
	return true;
}

bool
syncterm_theme_document_import(struct syncterm_theme_document *document,
    const char *filename, char *error, size_t error_size)
{
	if (document == NULL) {
		set_error(error, error_size, "invalid theme document");
		return false;
	}
	struct theme_history_entry entry = {.kind = THEME_HISTORY_IMPORT};
	if (!copy_document_snapshot(&entry.data.import.old_state, document)) {
		set_error(error, error_size, "out of memory");
		return false;
	}
	struct syncterm_theme_document imported = {0};
	if (filename == NULL || filename[0] == '\0') {
		struct syncterm_theme *classic = build_classic_theme(&settings);
		if (classic == NULL || !document_populate(&imported, classic, NULL,
		    true, error, error_size)) {
			syncterm_theme_free(classic);
			goto failed;
		}
		for (size_t i = 0; i < imported.style_count; i++)
			imported.styles[i].modified =
			    (1u << SYNCTERM_THEME_STYLE_FIELD_COUNT) - 1;
		for (size_t i = 0; i < imported.glyph_count; i++)
			imported.glyphs[i].modified =
			    (1u << SYNCTERM_THEME_GLYPH_FIELD_COUNT) - 1;
		syncterm_theme_free(classic);
		imported.imported_sections = strListInit();
		if (imported.imported_sections == NULL)
			goto failed;
	}
	else {
		struct syncterm_theme_document *source =
		    syncterm_theme_document_open(filename, error, error_size);
		if (source == NULL)
			goto failed;
		if (!copy_document_arrays(source->styles, source->style_count,
		    source->glyphs, source->glyph_count, &imported.styles,
		    &imported.style_count, &imported.glyphs,
		    &imported.glyph_count)) {
			syncterm_theme_document_free(source);
			goto failed;
		}
		str_list_t remainder = NULL;
		if (!filter_definition_sections(source->source,
		    &imported.imported_sections, &remainder)) {
			syncterm_theme_document_free(source);
			goto failed;
		}
		strListFree(&remainder);
		syncterm_theme_document_free(source);
	}
	imported.replace_definitions = true;
	if (!copy_document_snapshot(&entry.data.import.new_state, &imported) ||
	    !document_apply_snapshot(document, &entry.data.import.new_state) ||
	    !document_rebuild_preview(document, error, error_size) ||
	    !document_push_history(document, &entry)) {
		document_apply_snapshot(document, &entry.data.import.old_state);
		document_rebuild_preview(document, NULL, 0);
		goto failed;
	}
	free_document_arrays(imported.styles, imported.style_count,
	    imported.glyphs, imported.glyph_count);
	strListFree(&imported.imported_sections);
	return true;

failed:
	free_document_arrays(imported.styles, imported.style_count,
	    imported.glyphs, imported.glyph_count);
	strListFree(&imported.imported_sections);
	free_history_entry(&entry);
	if (error != NULL && error[0] == '\0')
		set_error(error, error_size, "out of memory");
	return false;
}

static bool
document_write_metadata(struct syncterm_theme_document *document,
    str_list_t *output)
{
	for (unsigned field = 0; field < SYNCTERM_THEME_METADATA_FIELD_COUNT;
	    field++) {
		unsigned bit = 1u << field;
		if ((document->metadata_modified & bit) == 0)
			continue;
		if ((document->metadata_present & bit) != 0) {
			if (iniSetString(output, ROOT_SECTION,
			    theme_metadata_keys[field], document->metadata[field],
			    NULL) == NULL)
				return false;
		}
		else
			iniRemoveKey(output, ROOT_SECTION, theme_metadata_keys[field]);
	}
	return true;
}

static bool
document_write_style(struct theme_document_style *style, str_list_t *output)
{
	char section[INI_MAX_VALUE_LEN];
	if (snprintf(section, sizeof(section), "Style:%s", style->role) >=
	    (int)sizeof(section))
		return false;
	if (style->deleted) {
		iniRemoveSection(output, section);
		return true;
	}
	if (style->created && !iniSectionExists(*output, section)) {
		iniAddSection(output, section, NULL);
		if (!iniSectionExists(*output, section))
			return false;
	}
	for (unsigned field = 0; field < SYNCTERM_THEME_STYLE_FIELD_COUNT;
	    field++) {
		unsigned bit = 1u << field;
		if ((style->modified & bit) == 0)
			continue;
		if ((style->present & bit) == 0) {
			iniRemoveKey(output, section, theme_style_keys[field]);
			continue;
		}
		char value[32];
		if (style->values[field] == THEME_INHERIT)
			strcpy(value, "inherit");
		else if (field >= SYNCTERM_THEME_STYLE_FOREGROUND)
			snprintf(value, sizeof(value), "0x%06x",
			    (unsigned)style->values[field]);
		else
			snprintf(value, sizeof(value), "%d", style->values[field]);
		if (iniSetString(output, section, theme_style_keys[field], value,
		    NULL) == NULL)
			return false;
	}
	return true;
}

static bool
document_write_glyph(struct theme_document_glyph *glyph, str_list_t *output)
{
	char section[INI_MAX_VALUE_LEN];
	if (snprintf(section, sizeof(section), "Glyph:%s", glyph->name) >=
	    (int)sizeof(section))
		return false;
	if (glyph->deleted) {
		iniRemoveSection(output, section);
		return true;
	}
	if (glyph->created && !iniSectionExists(*output, section)) {
		iniAddSection(output, section, NULL);
		if (!iniSectionExists(*output, section))
			return false;
	}
	for (unsigned field = 0; field < SYNCTERM_THEME_GLYPH_FIELD_COUNT;
	    field++) {
		unsigned bit = 1u << field;
		if ((glyph->modified & bit) == 0)
			continue;
		if ((glyph->present & bit) == 0) {
			iniRemoveKey(output, section, theme_glyph_keys[field]);
			continue;
		}
		if (iniSetInteger(output, section, theme_glyph_keys[field],
		    glyph->values[field], NULL) == NULL)
			return false;
	}
	return true;
}

static str_list_t
document_build_source(struct syncterm_theme_document *document, char *error,
    size_t error_size)
{
	str_list_t output = strListDup(document->source);
	if (output == NULL) {
		set_error(error, error_size, "out of memory");
		return NULL;
	}
	if (document->replace_definitions) {
		str_list_t discarded = NULL;
		str_list_t remainder = NULL;
		if (!filter_definition_sections(output, &discarded, &remainder)) {
			strListFree(&output);
			set_error(error, error_size, "out of memory");
			return NULL;
		}
		strListFree(&output);
		strListFree(&discarded);
		output = remainder;
		size_t count = strListCount(output);
		size_t added = strListCount(document->imported_sections);
		strListAppendList(&output, document->imported_sections);
		if (strListCount(output) != count + added) {
			strListFree(&output);
			set_error(error, error_size, "out of memory");
			return NULL;
		}
	}
	if (!document_write_metadata(document, &output))
		goto failed;
	for (size_t i = 0; i < document->style_count; i++) {
		if (!document_write_style(&document->styles[i], &output))
			goto failed;
	}
	for (size_t i = 0; i < document->glyph_count; i++) {
		if (!document_write_glyph(&document->glyphs[i], &output))
			goto failed;
	}
	return output;

failed:
	strListFree(&output);
	set_error(error, error_size, "out of memory while updating theme source");
	return NULL;
}

static FILE *
open_theme_temporary(const char *path, char *temporary,
    size_t temporary_size)
{
	for (unsigned attempt = 0; attempt < 100; attempt++) {
		if (snprintf(temporary, temporary_size, "%s.tmp.%ld.%u", path,
		    (long)getpid(), attempt) >= (int)temporary_size)
			return NULL;
		FILE *fp = fopen(temporary, "wbx");
		if (fp != NULL)
			return fp;
		if (errno != EEXIST)
			return NULL;
	}
	errno = EEXIST;
	return NULL;
}

static bool
replace_theme_file(const char *temporary, const char *path)
{
#ifdef _WIN32
	return MoveFileExA(temporary, path,
	    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(temporary, path) == 0;
#endif
}

static void
document_finish_save(struct syncterm_theme_document *document,
    char *filename, str_list_t source)
{
	free(document->filename);
	document->filename = filename;
	strListFree(&document->source);
	document->source = source;
	document->metadata_modified = 0;
	strListFree(&document->imported_sections);
	document->replace_definitions = false;
	for (size_t i = 0; i < document->style_count;) {
		if (document->styles[i].deleted) {
			free_document_style(&document->styles[i]);
			memmove(&document->styles[i], &document->styles[i + 1],
			    (document->style_count - i - 1) *
			    sizeof(*document->styles));
			document->style_count--;
			continue;
		}
		document->styles[i].created = false;
		document->styles[i].modified = 0;
		i++;
	}
	for (size_t i = 0; i < document->glyph_count;) {
		if (document->glyphs[i].deleted) {
			free_document_glyph(&document->glyphs[i]);
			memmove(&document->glyphs[i], &document->glyphs[i + 1],
			    (document->glyph_count - i - 1) *
			    sizeof(*document->glyphs));
			document->glyph_count--;
			continue;
		}
		document->glyphs[i].created = false;
		document->glyphs[i].modified = 0;
		i++;
	}
	document_clear_history(document);
	if (document->preview != NULL)
		replace_string(&document->preview->filename, filename);
}

enum syncterm_theme_document_error
syncterm_theme_document_save(struct syncterm_theme_document *document,
    const char *filename, bool force, char *error, size_t error_size)
{
	if (safe_mode) {
		set_error(error, error_size, "themes cannot be saved in safe mode");
		return SYNCTERM_THEME_DOCUMENT_READ_ONLY;
	}
	if (document == NULL) {
		set_error(error, error_size, "invalid theme document");
		return SYNCTERM_THEME_DOCUMENT_INVALID;
	}
	if (filename == NULL || filename[0] == '\0')
		filename = document->filename;
	char path[MAX_PATH + 1];
	if (!theme_filename_path(filename, path, sizeof(path), error, error_size))
		return SYNCTERM_THEME_DOCUMENT_INVALID;
	bool same = document->filename[0] != '\0' &&
	    same_filename(document->filename, filename);
	if (same && !force) {
		char read_error[256] = "";
		str_list_t current = read_theme_source(path, read_error,
		    sizeof(read_error));
		bool matches = current != NULL &&
		    source_lists_equal(document->source, current);
		strListFree(&current);
		if (!matches) {
			set_error(error, error_size,
			    "the theme file changed after it was opened");
			return SYNCTERM_THEME_DOCUMENT_CONFLICT;
		}
	}
	else if (!same && !force) {
		FILE *existing = fopen(path, "rb");
		if (existing != NULL) {
			fclose(existing);
			set_error(error, error_size, "the theme file already exists");
			return SYNCTERM_THEME_DOCUMENT_EXISTS;
		}
		if (errno != ENOENT) {
			set_error(error, error_size, "unable to check theme file: %s",
			    strerror(errno));
			return SYNCTERM_THEME_DOCUMENT_IO;
		}
	}
	str_list_t output = document_build_source(document, error, error_size);
	if (output == NULL)
		return SYNCTERM_THEME_DOCUMENT_IO;
	char *saved_filename = strdup(filename);
	if (saved_filename == NULL) {
		set_error(error, error_size, "out of memory");
		strListFree(&output);
		return SYNCTERM_THEME_DOCUMENT_IO;
	}
	char temporary[MAX_PATH + 64];
	FILE *fp = open_theme_temporary(path, temporary, sizeof(temporary));
	if (fp == NULL) {
		set_error(error, error_size, "unable to create temporary file: %s",
		    strerror(errno));
		free(saved_filename);
		strListFree(&output);
		return SYNCTERM_THEME_DOCUMENT_IO;
	}
	bool wrote = iniWriteFile(fp, output) && fflush(fp) == 0 &&
	    fsync(fileno(fp)) == 0;
	if (fclose(fp) != 0)
		wrote = false;
	if (!wrote) {
		set_error(error, error_size, "unable to write temporary theme file");
		remove(temporary);
		free(saved_filename);
		strListFree(&output);
		return SYNCTERM_THEME_DOCUMENT_IO;
	}
	struct syncterm_theme *validated = NULL;
	if (!syncterm_theme_load_path(temporary, filename, &validated, error,
	    error_size)) {
		remove(temporary);
		free(saved_filename);
		strListFree(&output);
		return SYNCTERM_THEME_DOCUMENT_INVALID;
	}
	if (!replace_theme_file(temporary, path)) {
		set_error(error, error_size, "unable to replace theme file: %s",
		    strerror(errno));
		remove(temporary);
		syncterm_theme_free(validated);
		free(saved_filename);
		strListFree(&output);
		return SYNCTERM_THEME_DOCUMENT_IO;
	}
	bool selected = same_filename(settings.theme_file, filename);
	document_finish_save(document, saved_filename, output);
	if (selected)
		syncterm_theme_install(validated);
	else
		syncterm_theme_free(validated);
	return SYNCTERM_THEME_DOCUMENT_OK;
}
