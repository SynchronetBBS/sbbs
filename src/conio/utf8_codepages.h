#ifndef UTF8_CODEPAGES_H
#define UTF8_CODEPAGES_H

enum ciolib_codepage {
	CIOLIB_CP437,
	CIOLIB_CP1251,
	CIOLIB_KOI8_R,
	CIOLIB_ISO_8859_2,
	CIOLIB_ISO_8859_4,
	CIOLIB_CP_COUNT
};

uint8_t *cp_to_utf8(enum ciolib_codepage cp, const char *cpstr, size_t buflen, size_t *outlen);
char *utf8_to_cp(enum ciolib_codepage cp, const uint8_t *utf8str, char unmapped, size_t buflen, size_t *outlen);
uint8_t cp_from_unicode_cp(enum ciolib_codepage cp, uint32_t cpoint, char unmapped);
uint8_t cp_from_unicode_cp_ext(enum ciolib_codepage cp, uint32_t cpoint, char unmapped);

#endif
