#ifndef SHA256_H
#define SHA256_H

/*
 * SHA-256 in C
 * Reference implementation of FIPS PUB 180-4.
 * 100% Public Domain.
 */

#include <stddef.h>     /* size_t */
#include <stdint.h>     /* uint32_t / uint64_t / uint8_t */

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

typedef struct {
	uint32_t state[8];
	uint64_t count;     /* total bytes fed in */
	uint8_t  buffer[SHA256_BLOCK_SIZE];
} SHA256_CTX;

#ifdef __cplusplus
extern "C" {
#endif

void SHA256Init(SHA256_CTX *ctx);
void SHA256Update(SHA256_CTX *ctx, const void *data, size_t len);
void SHA256Final(SHA256_CTX *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

void SHA256_calc(uint8_t *hash_out, const void *data, size_t len);
char *SHA256_hex(char *to, const uint8_t digest[SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SHA256_H */
