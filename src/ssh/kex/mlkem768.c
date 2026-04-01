/* ML-KEM-768 (FIPS 203) wrapper around libcrux.
 * Provides a simple byte-array API with backend-neutral crypto. */

#include <stdint.h>
#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static inline uint64_t
lcx_bswap64(uint64_t x)
{
	return (x & 0xFF) << 56
	    | (x >> 8 & 0xFF) << 48
	    | (x >> 16 & 0xFF) << 40
	    | (x >> 24 & 0xFF) << 32
	    | (x >> 32 & 0xFF) << 24
	    | (x >> 40 & 0xFF) << 16
	    | (x >> 48 & 0xFF) << 8
	    | (x >> 56 & 0xFF);
}

static inline uint32_t
lcx_bswap32(uint32_t x)
{
	return (x & 0xFF) << 24
	    | (x >> 8 & 0xFF) << 16
	    | (x >> 16 & 0xFF) << 8
	    | (x >> 24 & 0xFF);
}

#define lcx_htole64(x) lcx_bswap64(x)
#define lcx_le64toh(x) lcx_bswap64(x)
#define lcx_le32toh(x) lcx_bswap32(x)
#else
#define lcx_htole64(x) (x)
#define lcx_le64toh(x) (x)
#define lcx_le32toh(x) (x)
#endif

#include "deucessh-crypto.h"
#include "deucessh.h"
#include "mlkem768.h"
#ifdef DSSH_TESTING
#include "ssh-internal.h"
#endif

#include "libcrux_mlkem768_sha3.h"

DSSH_PRIVATE int
crypto_kem_mlkem768_keypair(unsigned char *pk, unsigned char *sk)
{
	uint8_t rnd[LIBCRUX_ML_KEM_KEY_PAIR_PRNG_LEN];

	if (dssh_random(rnd, sizeof(rnd)) != 0)
		return -1;

	libcrux_ml_kem_mlkem768_MlKem768KeyPair kp =
	    libcrux_ml_kem_mlkem768_portable_generate_key_pair(rnd);

	memcpy(pk, kp.pk.value, crypto_kem_mlkem768_PUBLICKEYBYTES);
	memcpy(sk, kp.sk.value, crypto_kem_mlkem768_SECRETKEYBYTES);

	dssh_cleanse(&kp, sizeof(kp));
	dssh_cleanse(rnd, sizeof(rnd));
	return 0;
}

DSSH_PRIVATE int
crypto_kem_mlkem768_enc(unsigned char *ct, unsigned char *ss,
    const unsigned char *pk)
{
	libcrux_ml_kem_types_MlKemPublicKey_15 mlkem_pk;

	memcpy(mlkem_pk.value, pk, crypto_kem_mlkem768_PUBLICKEYBYTES);

	if (!libcrux_ml_kem_mlkem768_portable_validate_public_key(&mlkem_pk))
		return -1;

	uint8_t rnd[LIBCRUX_ML_KEM_ENC_PRNG_LEN];

	if (dssh_random(rnd, sizeof(rnd)) != 0)
		return -1;

	tuple_3c enc =
	    libcrux_ml_kem_mlkem768_portable_encapsulate(&mlkem_pk, rnd);

	memcpy(ct, enc.fst.value, crypto_kem_mlkem768_CIPHERTEXTBYTES);
	memcpy(ss, enc.snd, crypto_kem_mlkem768_BYTES);

	dssh_cleanse(&enc, sizeof(enc));
	dssh_cleanse(rnd, sizeof(rnd));
	return 0;
}

DSSH_PRIVATE int
crypto_kem_mlkem768_dec(unsigned char *ss, const unsigned char *ct,
    const unsigned char *sk)
{
	libcrux_ml_kem_types_MlKemPrivateKey_55 mlkem_sk;
	libcrux_ml_kem_mlkem768_MlKem768Ciphertext mlkem_ct;

	memcpy(mlkem_sk.value, sk, crypto_kem_mlkem768_SECRETKEYBYTES);
	memcpy(mlkem_ct.value, ct, crypto_kem_mlkem768_CIPHERTEXTBYTES);

	libcrux_ml_kem_mlkem768_portable_decapsulate(&mlkem_sk, &mlkem_ct, ss);

	dssh_cleanse(&mlkem_sk, sizeof(mlkem_sk));
	return 0;
}
