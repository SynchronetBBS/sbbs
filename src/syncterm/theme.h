#ifndef SYNCTERM_THEME_H
#define SYNCTERM_THEME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct syncterm_settings;

struct syncterm_theme_style {
	char   *role;
	int     font;
	int     legacy_attr;
	int     foreground;
	int     background;
};

struct syncterm_theme_glyph {
	char   *name;
	uint8_t cp437;
	uint8_t ascii;
};

struct syncterm_theme {
	char                         *filename;
	char                         *name;
	char                         *author;
	char                         *description;
	char                         *version;
	struct syncterm_theme_style  *styles;
	size_t                        style_count;
	struct syncterm_theme_glyph  *glyphs;
	size_t                        glyph_count;
};

struct syncterm_theme_catalog_entry {
	char                  *filename;
	char                  *name;
	char                  *author;
	char                  *description;
	char                  *version;
	char                  *error;
	struct syncterm_theme *theme;
};

bool syncterm_theme_init(const struct syncterm_settings *settings);
void syncterm_theme_shutdown(void);

const struct syncterm_theme *syncterm_theme_active(void);
const struct syncterm_theme *syncterm_theme_default(void);
uint64_t syncterm_theme_generation(void);

bool syncterm_theme_prepare(const struct syncterm_settings *settings,
    bool force_reload, struct syncterm_theme **result, char *error,
    size_t error_size);
void syncterm_theme_install(struct syncterm_theme *theme);
void syncterm_theme_free(struct syncterm_theme *theme);

bool syncterm_theme_catalog_refresh(const char *selected_filename);
size_t syncterm_theme_catalog_count(void);
const struct syncterm_theme_catalog_entry *syncterm_theme_catalog_entry(
    size_t index);
const char *syncterm_theme_preview(const char *filename);
void syncterm_theme_cancel_preview(void);
bool syncterm_theme_prepare_catalog_selection(const char *filename,
    struct syncterm_theme **result, char *error, size_t error_size);

bool syncterm_theme_valid_filename(const char *filename);
bool syncterm_theme_load_path(const char *path, const char *filename,
    struct syncterm_theme **result, char *error, size_t error_size);

#endif
