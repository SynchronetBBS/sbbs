#include "theme.h"

#include "syncterm.h"

#include <ctype.h>
#include <dirwrap.h>
#include <errno.h>
#include <glob.h>
#include <ini_file.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	theme->name = dup_string(source->name);
	theme->author = dup_string(source->author);
	theme->description = dup_string(source->description);
	theme->version = dup_string(source->version);
	if ((source->filename != NULL && theme->filename == NULL) ||
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
	if (settings->theme_file[0] != '\0') {
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
	    committed_theme->filename != NULL ? committed_theme->filename : ""))
		return true;
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
	struct syncterm_theme *theme = clone_theme(entry->theme);
	if (theme == NULL) {
		snprintf(error, sizeof(error), "out of memory");
		return error;
	}
	syncterm_theme_free(preview_theme);
	preview_theme = theme;
	theme_generation++;
	return NULL;
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
	if (preview_theme != NULL && preview_theme->filename != NULL &&
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
