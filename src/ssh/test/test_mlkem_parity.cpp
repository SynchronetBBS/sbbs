#include <botan/ml_kem.h>
#include <botan/pubkey.h>
#include <botan/system_rng.h>

#include <array>
#include <cstring>

extern "C" {
#include "dssh_test.h"
#include "deucessh-crypto.h"
#include "kex/mlkem768.h"
}

static int
test_native_key_vendor_encapsulation(void)
{
	Botan::ML_KEM_PrivateKey key(Botan::system_rng(), Botan::ML_KEM_Mode::ML_KEM_768);
	auto public_key = key.raw_public_key_bits();
	ASSERT_EQ_U(public_key.size(), crypto_kem_mlkem768_PUBLICKEYBYTES);
	std::array<uint8_t, crypto_kem_mlkem768_CIPHERTEXTBYTES> ciphertext;
	std::array<uint8_t, crypto_kem_mlkem768_BYTES> secret;
	ASSERT_EQ(crypto_kem_mlkem768_enc(ciphertext.data(), secret.data(), public_key.data()), 0);
	Botan::PK_KEM_Decryptor decryptor(key, Botan::system_rng(), "Raw");
	auto recovered = decryptor.decrypt(ciphertext, secret.size());
	bool equal = recovered.size() == secret.size()
	    && dssh_crypto_memcmp(secret.data(), recovered.data(), secret.size()) == 0;
	dssh_cleanse(secret.data(), secret.size());
	ASSERT_TRUE(equal);
	return TEST_PASS;
}

static int
test_vendor_key_native_encapsulation(void)
{
	std::array<uint8_t, crypto_kem_mlkem768_PUBLICKEYBYTES> public_key;
	Botan::secure_vector<uint8_t> private_key(crypto_kem_mlkem768_SECRETKEYBYTES);
	ASSERT_EQ(crypto_kem_mlkem768_keypair(public_key.data(), private_key.data()), 0);
	Botan::ML_KEM_PublicKey key(public_key, Botan::ML_KEM_Mode::ML_KEM_768);
	Botan::PK_KEM_Encryptor encryptor(key, "Raw");
	auto encapsulated = encryptor.encrypt(Botan::system_rng(), crypto_kem_mlkem768_BYTES);
	std::array<uint8_t, crypto_kem_mlkem768_BYTES> recovered;
	ASSERT_EQ(crypto_kem_mlkem768_dec(recovered.data(), encapsulated.encapsulated_shared_key().data(),
	    private_key.data()), 0);
	bool equal = encapsulated.shared_key().size() == recovered.size()
	    && dssh_crypto_memcmp(recovered.data(), encapsulated.shared_key().data(), recovered.size()) == 0;
	dssh_cleanse(recovered.data(), recovered.size());
	ASSERT_TRUE(equal);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "native_key_vendor_encapsulation", test_native_key_vendor_encapsulation },
	{ "vendor_key_native_encapsulation", test_vendor_key_native_encapsulation },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
