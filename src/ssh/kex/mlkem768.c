/* ML-KEM-768 (FIPS 203) wrapper around libcrux.
 * Provides a simple byte-array API with OpenSSL RAND_bytes. */

#include <openssl/rand.h>
#include <string.h>

#include "mlkem768.h"
#ifdef DSSH_TESTING
#include "ssh-internal.h"
#endif

#include "libcrux_mlkem768_sha3.h"

int
crypto_kem_mlkem768_keypair(unsigned char *pk, unsigned char *sk)
{
	uint8_t rnd[LIBCRUX_ML_KEM_KEY_PAIR_PRNG_LEN];

	if (RAND_bytes(rnd, sizeof(rnd)) != 1)
		return -1;

	libcrux_ml_kem_mlkem768_MlKem768KeyPair kp =
	    libcrux_ml_kem_mlkem768_portable_generate_key_pair(rnd);

	memcpy(pk, kp.pk.value, crypto_kem_mlkem768_PUBLICKEYBYTES);
	memcpy(sk, kp.sk.value, crypto_kem_mlkem768_SECRETKEYBYTES);

	OPENSSL_cleanse(&kp, sizeof(kp));
	OPENSSL_cleanse(rnd, sizeof(rnd));
	return 0;
}

int
crypto_kem_mlkem768_enc(unsigned char *ct, unsigned char *ss,
    const unsigned char *pk)
{
	libcrux_ml_kem_types_MlKemPublicKey_15 mlkem_pk;

	memcpy(mlkem_pk.value, pk, crypto_kem_mlkem768_PUBLICKEYBYTES);

	if (!libcrux_ml_kem_mlkem768_portable_validate_public_key(&mlkem_pk))
		return -1;

	uint8_t rnd[LIBCRUX_ML_KEM_ENC_PRNG_LEN];

	if (RAND_bytes(rnd, sizeof(rnd)) != 1)
		return -1;

	tuple_3c enc =
	    libcrux_ml_kem_mlkem768_portable_encapsulate(&mlkem_pk, rnd);

	memcpy(ct, enc.fst.value, crypto_kem_mlkem768_CIPHERTEXTBYTES);
	memcpy(ss, enc.snd, crypto_kem_mlkem768_BYTES);

	OPENSSL_cleanse(&enc, sizeof(enc));
	OPENSSL_cleanse(rnd, sizeof(rnd));
	return 0;
}

int
crypto_kem_mlkem768_dec(unsigned char *ss, const unsigned char *ct,
    const unsigned char *sk)
{
	libcrux_ml_kem_types_MlKemPrivateKey_55 mlkem_sk;
	libcrux_ml_kem_mlkem768_MlKem768Ciphertext mlkem_ct;

	memcpy(mlkem_sk.value, sk, crypto_kem_mlkem768_SECRETKEYBYTES);
	memcpy(mlkem_ct.value, ct, crypto_kem_mlkem768_CIPHERTEXTBYTES);

	libcrux_ml_kem_mlkem768_portable_decapsulate(&mlkem_sk, &mlkem_ct, ss);

	OPENSSL_cleanse(&mlkem_sk, sizeof(mlkem_sk));
	return 0;
}
