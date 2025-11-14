#ifndef BASE64_HEADER
#define BASE64_HEADER

#include <stdint.h>

void b64_decode(uint8_t *target, size_t tlen, const char *source);
char * b64_encode(const uint8_t *source, size_t slen);

#endif
