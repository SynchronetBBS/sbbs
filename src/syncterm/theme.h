#ifndef SYNCTERM_THEME_H
#define SYNCTERM_THEME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct syncterm_settings;
struct syncterm_theme_document;

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

enum syncterm_theme_style_field {
	SYNCTERM_THEME_STYLE_FONT,
	SYNCTERM_THEME_STYLE_LEGACY_ATTR,
	SYNCTERM_THEME_STYLE_FOREGROUND,
	SYNCTERM_THEME_STYLE_BACKGROUND,
	SYNCTERM_THEME_STYLE_FIELD_COUNT
};

enum syncterm_theme_metadata_field {
	SYNCTERM_THEME_METADATA_NAME,
	SYNCTERM_THEME_METADATA_AUTHOR,
	SYNCTERM_THEME_METADATA_DESCRIPTION,
	SYNCTERM_THEME_METADATA_VERSION,
	SYNCTERM_THEME_METADATA_FIELD_COUNT
};

enum syncterm_theme_glyph_field {
	SYNCTERM_THEME_GLYPH_CP437,
	SYNCTERM_THEME_GLYPH_ASCII,
	SYNCTERM_THEME_GLYPH_FIELD_COUNT
};

enum syncterm_theme_value_mode {
	SYNCTERM_THEME_VALUE_DEFAULT,
	SYNCTERM_THEME_VALUE_INHERIT,
	SYNCTERM_THEME_VALUE_EXPLICIT
};

enum syncterm_theme_document_error {
	SYNCTERM_THEME_DOCUMENT_OK,
	SYNCTERM_THEME_DOCUMENT_INVALID,
	SYNCTERM_THEME_DOCUMENT_CONFLICT,
	SYNCTERM_THEME_DOCUMENT_EXISTS,
	SYNCTERM_THEME_DOCUMENT_READ_ONLY,
	SYNCTERM_THEME_DOCUMENT_IO
};

struct syncterm_theme_document_style {
	const char *role;
	bool        builtin;
	unsigned    present;
	int         values[SYNCTERM_THEME_STYLE_FIELD_COUNT];
};

struct syncterm_theme_document_glyph {
	const char *name;
	bool        builtin;
	unsigned    present;
	uint8_t     values[SYNCTERM_THEME_GLYPH_FIELD_COUNT];
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

struct syncterm_theme_document *syncterm_theme_document_new(char *error,
    size_t error_size);
struct syncterm_theme_document *syncterm_theme_document_open(
    const char *filename, char *error, size_t error_size);
void syncterm_theme_document_free(struct syncterm_theme_document *document);

const char *syncterm_theme_document_filename(
    const struct syncterm_theme_document *document);
const char *syncterm_theme_document_metadata(
    const struct syncterm_theme_document *document, unsigned field);
bool syncterm_theme_document_set_metadata(
    struct syncterm_theme_document *document, unsigned field,
    const char *value, char *error, size_t error_size);

size_t syncterm_theme_document_style_count(
    const struct syncterm_theme_document *document);
bool syncterm_theme_document_style(
    const struct syncterm_theme_document *document, size_t index,
    struct syncterm_theme_document_style *result);
bool syncterm_theme_document_set_style(
    struct syncterm_theme_document *document, const char *role,
    unsigned field, enum syncterm_theme_value_mode mode, int value,
    char *error, size_t error_size);
bool syncterm_theme_document_add_style(
    struct syncterm_theme_document *document, const char *role,
    char *error, size_t error_size);
bool syncterm_theme_document_remove_style(
    struct syncterm_theme_document *document, const char *role,
    char *error, size_t error_size);

size_t syncterm_theme_document_glyph_count(
    const struct syncterm_theme_document *document);
bool syncterm_theme_document_glyph(
    const struct syncterm_theme_document *document, size_t index,
    struct syncterm_theme_document_glyph *result);
bool syncterm_theme_document_set_glyph(
    struct syncterm_theme_document *document, const char *name,
    unsigned field, enum syncterm_theme_value_mode mode, int value,
    char *error, size_t error_size);
bool syncterm_theme_document_add_glyph(
    struct syncterm_theme_document *document, const char *name, int cp437,
    int ascii, char *error, size_t error_size);
bool syncterm_theme_document_remove_glyph(
    struct syncterm_theme_document *document, const char *name,
    char *error, size_t error_size);

const struct syncterm_theme *syncterm_theme_document_preview(
    const struct syncterm_theme_document *document);
bool syncterm_theme_document_dirty(
    const struct syncterm_theme_document *document);
bool syncterm_theme_document_can_undo(
    const struct syncterm_theme_document *document);
bool syncterm_theme_document_can_redo(
    const struct syncterm_theme_document *document);
bool syncterm_theme_document_undo(struct syncterm_theme_document *document);
bool syncterm_theme_document_redo(struct syncterm_theme_document *document);
bool syncterm_theme_document_import(struct syncterm_theme_document *document,
    const char *filename, char *error, size_t error_size);
enum syncterm_theme_document_error syncterm_theme_document_save(
    struct syncterm_theme_document *document, const char *filename,
    bool force, char *error, size_t error_size);

#endif
