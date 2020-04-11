#ifndef UTF8_CODEPAGES_H
#define UTF8_CODEPAGES_H

uint8_t *cp437_to_utf8(const char *cp437str, size_t buflen, size_t *outlen);
char *utf8_to_cp437(const uint8_t *utf8str, char unmapped);

#endif
