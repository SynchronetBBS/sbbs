/* sntrup761.h -- Streamlined NTRU Prime 761 KEM API */
#ifndef SNTRUP761_H
#define SNTRUP761_H

#include "deucessh-portable.h"

/* Type aliases expected by the SUPERCOP reference implementation */
#include <stdint.h>
typedef int8_t   crypto_int8;
typedef uint8_t  crypto_uint8;
typedef int16_t  crypto_int16;
typedef uint16_t crypto_uint16;
typedef int32_t  crypto_int32;
typedef uint32_t crypto_uint32;
typedef int64_t  crypto_int64;
typedef uint64_t crypto_uint64;

#define crypto_kem_sntrup761_PUBLICKEYBYTES  1158
#define crypto_kem_sntrup761_SECRETKEYBYTES  1763
#define crypto_kem_sntrup761_CIPHERTEXTBYTES 1039
#define crypto_kem_sntrup761_BYTES           32

#ifdef __cplusplus
extern "C" {
#endif

DSSH_PRIVATE int crypto_kem_sntrup761_keypair(unsigned char *pk, unsigned char *sk);
DSSH_PRIVATE int crypto_kem_sntrup761_enc(unsigned char *c, unsigned char *k, const unsigned char *pk);
DSSH_PRIVATE int crypto_kem_sntrup761_dec(unsigned char *k, const unsigned char *c, const unsigned char *sk);

#ifdef __cplusplus
}
#endif

#endif /* SNTRUP761_H */
