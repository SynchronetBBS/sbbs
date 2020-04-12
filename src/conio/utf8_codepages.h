#ifndef UTF8_CODEPAGES_H
#define UTF8_CODEPAGES_H

struct cp437map {
	uint32_t	unicode;
	uint8_t		cp437;
};

// Sorted by unicode codepoint...
extern struct cp437map cp437_table[160];
extern uint32_t cp437_unicode_table[128];
extern uint32_t cp437_ext_table[32];

uint8_t *cp437_to_utf8(const char *cp437str, size_t buflen, size_t *outlen);
char *utf8_to_cp437(const uint8_t *utf8str, char unmapped);
uint8_t cp437_from_unicode_cp(uint32_t cp, char unmapped);
uint8_t cp437_from_unicode_cp_ext(uint32_t cp, char unmapped);

#endif
