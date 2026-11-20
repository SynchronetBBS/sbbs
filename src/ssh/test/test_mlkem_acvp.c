/* Deterministic official answers for the actual selected bundled ML-KEM roots. */
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "mlkem_acvp.h"
#ifdef DSSH_TEST_MLKEM_RETIREMENT
static void observe_clear(const char *, const char *, const void *, size_t);
#define DSSH_MLKEM_CLEAR_OBSERVER observe_clear
#endif
#include "kex/libcrux_mlkem768_sha3.h"
#ifdef DSSH_TEST_MLKEM_RETIREMENT
#include "mlkem_hash_retirement.h"
#endif

enum output_control {
	OUTPUT_CONTROL_NONE,
	OUTPUT_CONTROL_KEYGEN_GUARD,
	OUTPUT_CONTROL_ENCAP_GUARD,
	OUTPUT_CONTROL_KEYGEN_INPUT,
	OUTPUT_CONTROL_ENCAP_INPUT,
	OUTPUT_CONTROL_PUBLIC_INPUT
};

static enum output_control output_control;
static uint64_t output_keygens;
static uint64_t output_encapsulations;
static uint64_t output_guards;
static uint64_t output_inputs;
static bool output_valid = true;

static bool
check_output_guards(const uint8_t before[16], const uint8_t after[16])
{
	bool valid = true;
	for (size_t i = 0; i < 16; i++) {
		if (before[i] != 0xa5 || after[i] != 0x5a)
			valid = false;
	}
	output_guards += 2;
	return valid;
}

static bool
evaluate_primitive(const struct dssh_acvp_case *test, const struct dssh_acvp_decoded *bytes)
{
	uint8_t seed[64];
	memcpy(seed, bytes->d, 32);
	memcpy(seed + 32, bytes->z, 32);
	if (test->kind == DSSH_ACVP_KEYGEN || test->kind == DSSH_ACVP_DECAP_SEED) {
		struct {
			uint8_t before[16];
			libcrux_ml_kem_mlkem768_MlKem768KeyPair value;
			uint8_t after[16];
		} key;
		memset(&key, 0xa5, sizeof(key));
		memset(key.after, 0x5a, sizeof(key.after));
		libcrux_ml_kem_mlkem768_portable_generate_key_pair(seed, &key.value);
		if (output_control == OUTPUT_CONTROL_KEYGEN_GUARD)
			key.after[0] = 0;
		if (output_control == OUTPUT_CONTROL_KEYGEN_INPUT)
			seed[0] ^= 1;
		bool guarded = check_output_guards(key.before, key.after);
		bool first_preserved = memcmp(seed, bytes->d, 32) == 0;
		bool last_preserved = memcmp(seed + 32, bytes->z, 32) == 0;
		bool preserved = first_preserved && last_preserved;
		output_keygens++;
		output_inputs++;
		output_valid = guarded && preserved && output_valid;
		bool equal = memcmp(key.value.pk.value, bytes->ek, sizeof(bytes->ek)) == 0
		    && memcmp(key.value.sk.value, bytes->dk, sizeof(bytes->dk)) == 0
		    && guarded && preserved;
		if (test->kind == DSSH_ACVP_KEYGEN)
			return equal;
		libcrux_ml_kem_mlkem768_MlKem768Ciphertext ciphertext;
		memcpy(ciphertext.value, bytes->c, sizeof(bytes->c));
		uint8_t secret[32];
		libcrux_ml_kem_mlkem768_portable_decapsulate(&key.value.sk, &ciphertext, secret);
		return equal && memcmp(secret, bytes->k, sizeof(secret)) == 0;
	}
	if (test->kind == DSSH_ACVP_ENCAP || test->kind == DSSH_ACVP_PUBLIC_CHECK) {
		libcrux_ml_kem_types_MlKemPublicKey_15 key;
		memcpy(key.value, bytes->ek, sizeof(bytes->ek));
		bool admitted = libcrux_ml_kem_mlkem768_portable_validate_public_key(&key);
		if (test->kind == DSSH_ACVP_PUBLIC_CHECK)
			return admitted == test->valid;
		if (!admitted)
			return false;
		uint8_t randomness[32];
		memcpy(randomness, bytes->m, sizeof(randomness));
		struct {
			uint8_t before[16];
			tuple_3c value;
			uint8_t after[16];
		} result;
		memset(&result, 0xa5, sizeof(result));
		memset(result.after, 0x5a, sizeof(result.after));
		libcrux_ml_kem_mlkem768_portable_encapsulate(&key, randomness, &result.value);
		if (output_control == OUTPUT_CONTROL_ENCAP_GUARD)
			result.after[0] = 0;
		if (output_control == OUTPUT_CONTROL_ENCAP_INPUT)
			randomness[0] ^= 1;
		if (output_control == OUTPUT_CONTROL_PUBLIC_INPUT)
			key.value[0] ^= 1;
		bool guarded = check_output_guards(result.before, result.after);
		bool randomness_preserved = memcmp(randomness, bytes->m, sizeof(randomness)) == 0;
		bool key_preserved = memcmp(key.value, bytes->ek, sizeof(key.value)) == 0;
		bool preserved = randomness_preserved && key_preserved;
		output_encapsulations++;
		output_inputs += 2;
		output_valid = guarded && preserved && output_valid;
		return memcmp(result.value.fst.value, bytes->c, sizeof(bytes->c)) == 0
		    && memcmp(result.value.snd, bytes->k, sizeof(bytes->k)) == 0
		    && guarded && preserved;
	}
	libcrux_ml_kem_types_MlKemPrivateKey_55 key;
	libcrux_ml_kem_mlkem768_MlKem768Ciphertext ciphertext = {0};
	memcpy(key.value, bytes->dk, sizeof(bytes->dk));
	if (test->kind == DSSH_ACVP_PRIVATE_CHECK)
		return libcrux_ml_kem_mlkem768_portable_validate_private_key(&key, &ciphertext) == test->valid;
	if (test->kind != DSSH_ACVP_DECAP)
		return false;
	memcpy(ciphertext.value, bytes->c, sizeof(bytes->c));
	uint8_t secret[32];
	libcrux_ml_kem_mlkem768_portable_decapsulate(&key, &ciphertext, secret);
	return memcmp(secret, bytes->k, sizeof(secret)) == 0;
}

#ifdef DSSH_TEST_MLKEM_RETIREMENT
/* Match actual live owner sites, not just buffer sizes. Logical returned
 * values and deeper polynomial/serialization owners have separate acceptance work. */
static const struct {
	const char *function;
	const char *owner;
	size_t bytes;
	unsigned counts[5]; /* keygen, encap, decap, public check, private check */
} clear_sites[] = {
	{"libcrux_ml_kem_utils_into_padded_array_ea0", "out", 1120, {0, 0, 1}},
	{"libcrux_ml_kem_utils_into_padded_array_ea", "out", 64, {0, 0, 1}},
	{"libcrux_ml_kem_constant_time_ops_select_ct", "out", 32, {0, 0, 1}},
	{"libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time", "ret0", 32, {0, 0, 1}},
	{"libcrux_ml_kem_variant_kdf_d8_41", "out", 32, {0, 1, 2}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "decrypted", 32, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "to_hash0", 64, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "hashed", 64, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "to_hash", 1120, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "implicit_rejection_shared_secret0", 32, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "copy_of_decrypted", 32, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "expected_ciphertext", 1088, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "implicit_rejection_shared_secret", 32, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "shared_secret", 32, {0, 0, 1}},
	{"libcrux_ml_kem_ind_cca_decapsulate_70", "ret0", 32, {0, 0, 1}},
	{"libcrux_ml_kem_variant_entropy_preprocess_d8_63", "out", 32, {0, 1, 0}},
	{"libcrux_ml_kem_ind_cca_encapsulate_cd", "to_hash", 64, {0, 1, 0}},
	{"libcrux_ml_kem_ind_cca_encapsulate_cd", "hashed", 64, {0, 1, 0}},
	{"libcrux_ml_kem_variant_cpa_keygen_seed_d8_0e", "seed", 33, {1, 0, 0}},
	{"libcrux_ml_kem_variant_cpa_keygen_seed_d8_0e", "ret0", 64, {1, 0, 0}},
	{"libcrux_ml_kem_ind_cca_serialize_kem_secret_key_48", "out", 2400, {1, 0, 0}},
	{"libcrux_ml_kem_ind_cca_generate_keypair_8c", "uu____0", sizeof(libcrux_ml_kem_utils_extraction_helper_Keypair768), {1, 0, 0}},
	{"libcrux_ml_kem_ind_cpa_generate_keypair_fc", "hashed", sizeof(uint8_t[64]), {1, 0, 0}},
	{"libcrux_ml_kem_ind_cpa_generate_keypair_fc", "prf_input", sizeof(uint8_t[33]), {1, 0, 0}},
	{"libcrux_ml_kem_ind_cpa_generate_keypair_fc", "secret_sample", sizeof(tuple_b0), {1, 0, 0}},
	{"libcrux_ml_kem_ind_cpa_generate_keypair_fc", "error_sample", sizeof(tuple_b0), {1, 0, 0}},
	{"libcrux_ml_kem_ind_cpa_generate_keypair_fc", "t_as_ntt", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0[3]), {1, 0, 0}},
	{"libcrux_ml_kem_ind_cpa_sample_vector_cbd_then_ntt_fc", "prf_inputs", sizeof(uint8_t[3][33]), {2, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_sample_vector_cbd_then_ntt_fc", "prf_outputs", sizeof(uint8_t[3][128]), {2, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_sample_ring_element_cbd_ac", "prf_inputs", sizeof(uint8_t[3][33]), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_sample_ring_element_cbd_ac", "prf_outputs", sizeof(uint8_t[3][128]), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "prf_input", sizeof(uint8_t[33]), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "prf_output", sizeof(uint8_t[128]), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "r_sample", sizeof(tuple_b0), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "error_sample", sizeof(tuple_b0), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "error_2", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "u", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0[3]), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "message_as_ring_element", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_encrypt_60", "v", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {0, 1, 1}},
	{"libcrux_ml_kem_ind_cpa_decrypt_unpacked_6d", "message", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {0, 0, 1}},
	{"libcrux_ml_kem_ind_cpa_decrypt_43", "secret_key_unpacked", sizeof(libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_f8), {0, 0, 1}},
	{"libcrux_ml_kem_serialize_compress_then_serialize_message_aa", "coefficient", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 0, 1}},
	{"libcrux_ml_kem_serialize_compress_then_serialize_4_9a", "coefficient", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 1, 1}},
	{"libcrux_ml_kem_matrix_compute_As_plus_e_60", "product", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {1, 0, 0, 0, 0}},
	{"libcrux_ml_kem_matrix_compute_vector_u_59", "product", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {0, 1, 1, 0, 0}},
	{"libcrux_ml_kem_matrix_compute_ring_element_v_54", "product", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {0, 1, 1, 0, 0}},
	{"libcrux_ml_kem_matrix_compute_message_b3", "product", sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_f0), {0, 0, 1, 0, 0}},
	{"libcrux_ml_kem_polynomial_add_standard_error_reduce_89_03", "coefficient_normal_form", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {3, 0, 0, 0, 0}},
	{"libcrux_ml_kem_polynomial_add_error_reduce_89_38", "coefficient_normal_form", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 3, 3, 0, 0}},
	{"libcrux_ml_kem_polynomial_add_message_error_reduce_89_ea", "coefficient_normal_form", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 1, 1, 0, 0}},
	{"libcrux_ml_kem_polynomial_add_message_error_reduce_89_ea", "tmp", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 1, 1, 0, 0}},
	{"libcrux_ml_kem_polynomial_subtract_reduce_89_d4", "coefficient_normal_form", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 0, 1, 0, 0}},
	{"libcrux_ml_kem_sampling_sample_from_binomial_distribution_2_85", "sampled_i16s", sizeof(int16_t[256]), {6, 7, 7, 0, 0}},
	{"libcrux_ml_kem_serialize_compress_then_serialize_10_fc", "coefficient", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 3, 3, 0, 0}},
	{"libcrux_ml_kem_serialize_serialize_uncompressed_ring_element_5b", "coefficient", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {6, 3, 0, 3, 0}},
	{"libcrux_ml_kem_ntt_ntt_at_layer_4_plus_51", "t", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {18, 9, 21, 0, 0}},
	{"libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_4_plus_6e", "a_minus_b", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {0, 16, 20, 0, 0}},
	{"libcrux_ml_kem_ntt_ntt_at_layer_7_f4", "t", sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector), {6, 3, 3, 0, 0}},
	{"libcrux_ml_kem_vector_portable_serialize_serialize_4", "encoded", sizeof(uint8_t_x4), {0, 16, 16, 0, 0}},
	{"libcrux_ml_kem_vector_portable_serialize_serialize_10", "encoded", sizeof(uint8_t_x5), {0, 48, 48, 0, 0}},
	{"libcrux_ml_kem_vector_portable_serialize_serialize_12", "encoded", sizeof(uint8_t_x3), {96, 48, 0, 48, 0}},
	{"libcrux_ml_kem_vector_portable_serialize_deserialize_4", "decoded", sizeof(int16_t_x8), {0, 0, 16, 0, 0}},
	{"libcrux_ml_kem_vector_portable_serialize_deserialize_10", "decoded", sizeof(int16_t_x8), {0, 0, 48, 0, 0}},
	{"libcrux_ml_kem_vector_portable_serialize_deserialize_12", "decoded", sizeof(int16_t_x2), {0, 96, 96, 48, 0}},
};

struct clear_observation {
	uint32_t calls[sizeof(clear_sites) / sizeof(clear_sites[0])];
	struct dssh_hash_wipes hash;
	bool valid;
};

/* This runner is single-threaded; publish only during evaluate_primitive. */
static struct clear_observation *active_observation;
static size_t observed_cases;
static uint64_t observed_clears;
static uint64_t observed_hash_clears;
static bool observed_hash_valid = true;

static void
observe_clear(const char *function, const char *owner, const void *buffer, size_t bytes)
{
	struct clear_observation *observation = active_observation;
	if (observation == NULL)
		return;
	if (dssh_hash_wipe_record(&observation->hash, function, owner, buffer, bytes))
		return;
	for (size_t i = 0; i < sizeof(clear_sites) / sizeof(clear_sites[0]); i++) {
		if (strcmp(function, clear_sites[i].function) != 0
		    || strcmp(owner, clear_sites[i].owner) != 0)
			continue;
		if (bytes != clear_sites[i].bytes || buffer == NULL
		    || observation->calls[i] == UINT32_MAX) {
			observation->valid = false;
			return;
		}
		const unsigned char *octets = buffer;
		for (size_t j = 0; j < bytes; j++) {
			if (octets[j] != 0)
				observation->valid = false;
		}
		observation->calls[i]++;
		return;
	}
	observation->valid = false;
}

static bool
check_observer(void)
{
	uint8_t buffer[32] = {0};
	bool valid = true;
	for (unsigned scenario = 0; scenario < 5; scenario++) {
		struct clear_observation observation = {.hash = {.valid = true}, .valid = true};
		active_observation = &observation;
		buffer[0] = scenario == 1 ? 1 : 0;
		const char *function = scenario == 3 ? "unknown" : clear_sites[4].function;
		size_t bytes = scenario == 2 ? sizeof(buffer) - 1 : sizeof(buffer);
		if (scenario == 4)
			observation.calls[4] = UINT32_MAX;
		observe_clear(function, clear_sites[4].owner, buffer, bytes);
		active_observation = NULL;
		if (observation.valid != (scenario == 0))
			valid = false;
	}
	if (printf("ML-KEM retirement observer: controls=5 valid=%u\n", valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static bool
evaluate(const struct dssh_acvp_case *test, const struct dssh_acvp_decoded *bytes)
{
	struct clear_observation observation = {.hash = {.valid = true}, .valid = true};
	active_observation = &observation;
	bool matched = evaluate_primitive(test, bytes);
	active_observation = NULL;
	bool keygen = test->kind == DSSH_ACVP_KEYGEN || test->kind == DSSH_ACVP_DECAP_SEED;
	bool encap = test->kind == DSSH_ACVP_ENCAP;
	bool decap = test->kind == DSSH_ACVP_DECAP || test->kind == DSSH_ACVP_DECAP_SEED;
	bool public_check = test->kind == DSSH_ACVP_PUBLIC_CHECK;
	bool private_check = test->kind == DSSH_ACVP_PRIVATE_CHECK;
	for (size_t i = 0; i < sizeof(clear_sites) / sizeof(clear_sites[0]); i++) {
		unsigned expected = (keygen ? clear_sites[i].counts[0] : 0U)
		    + (encap ? clear_sites[i].counts[1] : 0U)
		    + (decap ? clear_sites[i].counts[2] : 0U)
		    + (public_check ? clear_sites[i].counts[3] : 0U)
		    + (private_check ? clear_sites[i].counts[4] : 0U);
		if (observation.calls[i] != expected)
			observation.valid = false;
		/* 64 uint32_t counters across 145 finite cases sum to less than 2^46. */
		observed_clears += observation.calls[i];
	}
	observed_cases++;
	struct dssh_hash_wipes expected_hash = {.valid = true};
	uint32_t generations = keygen ? 1U : 0U;
	uint32_t encapsulations = encap ? 1U : 0U;
	uint32_t decapsulations = decap ? 1U : 0U;
	uint32_t private_checks = test->kind == DSSH_ACVP_PRIVATE_CHECK ? 1U : 0U;
	expected_hash.calls[DSSH_HASH_FINAL72] = generations + encapsulations + decapsulations;
	expected_hash.calls[DSSH_HASH_FINAL136_SHA] = generations + encapsulations + private_checks;
	expected_hash.calls[DSSH_HASH_FINAL136_SHAKE] = 6U * generations + 7U * encapsulations + 8U * decapsulations;
	expected_hash.calls[DSSH_HASH_FINAL168] = 9U * (generations + encapsulations + decapsulations);
	expected_hash.calls[DSSH_HASH_STATE72] = expected_hash.calls[DSSH_HASH_FINAL72];
	expected_hash.calls[DSSH_HASH_STATE136_SHA] = expected_hash.calls[DSSH_HASH_FINAL136_SHA];
	expected_hash.calls[DSSH_HASH_STATE136_SHAKE] = expected_hash.calls[DSSH_HASH_FINAL136_SHAKE];
	expected_hash.calls[DSSH_HASH_FIRST_LAST72] = expected_hash.calls[DSSH_HASH_FINAL72];
	expected_hash.calls[DSSH_HASH_FIRST_LAST136] = expected_hash.calls[DSSH_HASH_FINAL136_SHA]
	    + expected_hash.calls[DSSH_HASH_FINAL136_SHAKE];
	/* Each matrix row absorbs three lanes and squeezes three initial blocks.
	 * Every further sampler block advances all three lanes. Its number is
	 * public-input dependent; direct hash fixtures check exact permutations. */
	uint32_t minimum = 43U * generations + 44U * encapsulations + 44U * decapsulations + 9U * private_checks;
	uint32_t core = observation.hash.calls[DSSH_HASH_C];
	if (core < minimum || (core - minimum) % 3U != 0
	    || (expected_hash.calls[DSSH_HASH_FINAL168] == 0 && core != minimum))
		observation.hash.valid = false;
	expected_hash.calls[DSSH_HASH_C] = core;
	expected_hash.calls[DSSH_HASH_T] = core;
	expected_hash.calls[DSSH_HASH_OLD] = core;
	/* At most145 observations, each a sum of14 uint32_t counters. */
	observed_hash_clears += dssh_hash_wipe_total(&observation.hash);
	if (!dssh_hash_wipe_match(&observation.hash, &expected_hash)) {
		observed_hash_valid = false;
		observation.valid = false;
	}
	if (!observation.valid
	    && fprintf(stderr, "ML-KEM owner retirement mismatch: kind=%d group=%u case=%u\n",
	        test->kind, test->group, test->id) < 0)
		return false;
	return matched && observation.valid;
}
#else
#define evaluate evaluate_primitive
#endif

int
main(int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "--negative-output-keygen") == 0)
			output_control = OUTPUT_CONTROL_KEYGEN_GUARD;
		else if (strcmp(argv[1], "--negative-output-encap") == 0)
			output_control = OUTPUT_CONTROL_ENCAP_GUARD;
		else if (strcmp(argv[1], "--negative-input-keygen") == 0)
			output_control = OUTPUT_CONTROL_KEYGEN_INPUT;
		else if (strcmp(argv[1], "--negative-input-encap") == 0)
			output_control = OUTPUT_CONTROL_ENCAP_INPUT;
		else if (strcmp(argv[1], "--negative-input-public") == 0)
			output_control = OUTPUT_CONTROL_PUBLIC_INPUT;
		if (output_control != OUTPUT_CONTROL_NONE)
			argc = 1;
	}
	int ret;
	#ifdef DSSH_TEST_MLKEM_RETIREMENT
	if (!check_observer() || !dssh_hash_wipe_controls())
		return 2;
	ret = dssh_acvp_run(argc, argv, "bundled retirement", evaluate);
	if (printf("ML-KEM retirement: cases=%zu sites=%zu clears=%" PRIu64 " active=%u\n",
	    observed_cases, sizeof(clear_sites) / sizeof(clear_sites[0]), observed_clears,
	    active_observation != NULL ? 1U : 0U) < 0)
		return 2;
	if (printf("ML-KEM hash retirement in KEM: cases=%zu sites=14 clears=%" PRIu64
	    " active=%u valid=%u\n", observed_cases, observed_hash_clears,
	    active_observation != NULL ? 1U : 0U, observed_hash_valid ? 1U : 0U) < 0)
		return 2;
#else
	ret = dssh_acvp_run(argc, argv, "bundled", evaluate);
#endif
	if (printf("ML-KEM caller outputs: keygen=%" PRIu64 " encap=%" PRIu64
	    " guards=%" PRIu64 " inputs=%" PRIu64 " valid=%u\n", output_keygens,
	    output_encapsulations, output_guards, output_inputs, output_valid ? 1U : 0U) < 0
	    || fflush(stdout) != 0)
		return 2;
	return ret;
}
