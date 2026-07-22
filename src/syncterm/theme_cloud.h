#ifndef SYNCTERM_THEME_CLOUD_H
#define SYNCTERM_THEME_CLOUD_H

#include <stdbool.h>
#include <stddef.h>

#include "theme.h"

#ifndef SYNCTERM_THEME_MANIFEST_URI
#define SYNCTERM_THEME_MANIFEST_URI \
	"http://syncterm.bbsdev.net/themes/manifest.lst"
#endif

struct syncterm_cloud_theme_entry {
	char                  *package;
	char                  *name;
	char                  *author;
	char                  *description;
	char                  *version;
	char                  *uri;
	char                  *sha256;
	bool                   cached;
	bool                   update_available;
	char                  *error;
	struct syncterm_theme *theme;
};

bool syncterm_cloud_themes_refresh(bool fetch,
    const char *selected_package, char *error, size_t error_size);
bool syncterm_cloud_themes_available(void);
size_t syncterm_cloud_theme_count(void);
const struct syncterm_cloud_theme_entry *syncterm_cloud_theme_entry(
    size_t index);
bool syncterm_cloud_theme_cache(const char *package, char *error,
    size_t error_size);
bool syncterm_cloud_theme_delete(const char *package, char *error,
    size_t error_size);
const char *syncterm_cloud_theme_preview(const char *package);
struct syncterm_theme_document *syncterm_cloud_theme_copy(
    const char *package, char *error, size_t error_size);
void syncterm_cloud_themes_shutdown(void);

#endif
