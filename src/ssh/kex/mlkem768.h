/* ML-KEM-768 (FIPS 203) key encapsulation mechanism.
 * Implementation from libcrux (Cryspen), MIT-licensed. */

#ifndef MLKEM768_H
#define MLKEM768_H

#include "deucessh-portable.h"

#define crypto_kem_mlkem768_PUBLICKEYBYTES  1184
#define crypto_kem_mlkem768_SECRETKEYBYTES  2400
#define crypto_kem_mlkem768_CIPHERTEXTBYTES 1088
#define crypto_kem_mlkem768_BYTES           32

/* Generate a keypair.
 * pk must be PUBLICKEYBYTES, sk must be SECRETKEYBYTES.
 * Returns 0 on success, -1 on error. */
DSSH_PRIVATE int crypto_kem_mlkem768_keypair(unsigned char *pk, unsigned char *sk);

/* Encapsulate: encrypt a shared secret to the given public key.
 * ct must be CIPHERTEXTBYTES, ss must be BYTES.
 * Returns 0 on success, -1 on error (incl. invalid pk). */
DSSH_PRIVATE int crypto_kem_mlkem768_enc(unsigned char *ct, unsigned char *ss,
    const unsigned char *pk);

/* Decapsulate: recover shared secret from ciphertext + secret key.
 * ss must be BYTES.
 * Returns 0 on success. */
DSSH_PRIVATE int crypto_kem_mlkem768_dec(unsigned char *ss, const unsigned char *ct,
    const unsigned char *sk);

#endif /* MLKEM768_H */
