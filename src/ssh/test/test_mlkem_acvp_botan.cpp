#include <botan/exceptn.h>
#include <botan/ml_kem.h>
#include <botan/pubkey.h>
#include <botan/rng.h>
#include <botan/system_rng.h>
#include <botan/version.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

#include "mlkem_acvp.h"

/* Test-only finite entropy: the algorithm must consume exactly the supplied
 * NIST d||z or m. This does not replace any production RNG. */
class Vector_RNG final : public Botan::RandomNumberGenerator {
public:
	explicit Vector_RNG(std::span<const uint8_t> bytes) : m_bytes(bytes) {}
	bool accepts_input() const override { return false; }
	std::string name() const override { return "NIST ACVP fixed bytes"; }
	void clear() override { m_offset = m_bytes.size(); }
	bool is_seeded() const override { return m_offset < m_bytes.size(); }
	bool consumed() const { return m_offset == m_bytes.size(); }
private:
	void fill_bytes_with_input(std::span<uint8_t> output, std::span<const uint8_t> input) override
	{
		(void)input;
		if (output.size() > m_bytes.size() - m_offset)
			throw std::runtime_error("ACVP entropy exhausted");
		if (!output.empty())
			std::memcpy(output.data(), m_bytes.data() + m_offset, output.size());
		m_offset += output.size();
	}
	std::span<const uint8_t> m_bytes;
	size_t m_offset = 0;
};

template<typename Container>
static bool
equal(const Container& actual, const uint8_t *expected, size_t length)
{
	return actual.size() == length && (length == 0 || std::memcmp(actual.data(), expected, length) == 0);
}

static Botan::secure_vector<uint8_t>
expanded(const Botan::ML_KEM_PrivateKey& key)
{
#if BOTAN_VERSION_CODE >= BOTAN_VERSION_CODE_FOR(3, 14, 0)
	return key.formatted_raw_private_key_bits(Botan::MlPrivateKeyFormat::Expanded);
#else
	/* The advertised provider floor exposes this spelling of the same
	 * expanded-key export. No library or installed header uses this helper. */
	return key.private_key_bits_with_format(Botan::MlPrivateKeyFormat::Expanded);
#endif
}

extern "C" {
static bool
evaluate(const struct dssh_acvp_case *test, const struct dssh_acvp_decoded *bytes)
{
	try {
		const auto mode = Botan::ML_KEM_Mode::ML_KEM_768;
		if (test->kind == DSSH_ACVP_KEYGEN || test->kind == DSSH_ACVP_DECAP_SEED) {
			std::array<uint8_t, 64> seed;
			std::memcpy(seed.data(), bytes->d, 32);
			std::memcpy(seed.data() + 32, bytes->z, 32);
			Vector_RNG rng(seed);
			Botan::ML_KEM_PrivateKey key(rng, mode);
			bool matched = rng.consumed() && equal(key.raw_public_key_bits(), bytes->ek, sizeof(bytes->ek))
			    && equal(expanded(key), bytes->dk, sizeof(bytes->dk));
			if (test->kind == DSSH_ACVP_KEYGEN)
				return matched;
			Botan::PK_KEM_Decryptor dec(key, Botan::system_rng(), "Raw");
			auto secret = dec.decrypt(std::span<const uint8_t>(bytes->c, sizeof(bytes->c)), sizeof(bytes->k));
			return matched && equal(secret, bytes->k, sizeof(bytes->k));
		}
		if (test->kind == DSSH_ACVP_ENCAP) {
			Botan::ML_KEM_PublicKey key(std::span<const uint8_t>(bytes->ek, sizeof(bytes->ek)), mode);
			Botan::PK_KEM_Encryptor enc(key, "Raw");
			Vector_RNG rng(std::span<const uint8_t>(bytes->m, sizeof(bytes->m)));
			auto result = enc.encrypt(rng, sizeof(bytes->k));
			return rng.consumed() && equal(result.encapsulated_shared_key(), bytes->c, sizeof(bytes->c))
			    && equal(result.shared_key(), bytes->k, sizeof(bytes->k));
		}
		if (test->kind == DSSH_ACVP_DECAP) {
			Botan::ML_KEM_PrivateKey key(std::span<const uint8_t>(bytes->dk, sizeof(bytes->dk)), mode);
			Botan::PK_KEM_Decryptor dec(key, Botan::system_rng(), "Raw");
			auto secret = dec.decrypt(std::span<const uint8_t>(bytes->c, sizeof(bytes->c)), sizeof(bytes->k));
			return equal(secret, bytes->k, sizeof(bytes->k));
		}
		if (test->kind == DSSH_ACVP_PUBLIC_CHECK) {
			try {
				Botan::ML_KEM_PublicKey key(std::span<const uint8_t>(bytes->ek, sizeof(bytes->ek)), mode);
				return test->valid;
			}
			catch (const Botan::Decoding_Error&) {
				return !test->valid;
			}
		}
		if (test->kind == DSSH_ACVP_PRIVATE_CHECK) {
			try {
				Botan::ML_KEM_PrivateKey key(std::span<const uint8_t>(bytes->dk, sizeof(bytes->dk)), mode);
				return test->valid;
			}
			catch (const Botan::Decoding_Error&) {
				return !test->valid;
			}
			catch (const Botan::Invalid_Argument&) {
				/* Botan 3.13 uses this for an inconsistent stored public-key hash. */
				return !test->valid;
			}
		}
	}
	catch (const std::exception& error) {
		std::fprintf(stderr, "Botan ACVP operation failed: %s\n", error.what());
	}
	catch (...) {
		std::fprintf(stderr, "Botan ACVP operation failed with an unknown exception\n");
	}
	return false;
}

}

int
main(int argc, char **argv)
{
	return dssh_acvp_run(argc, argv, "Botan", evaluate);
}
