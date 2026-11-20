/* Actual native module callbacks; no C flexible-array descriptor is included
 * from C++. Registered NIST vectors remain in test_algo_enc.c. */
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "deucessh.h"
#include "dssh_test_botan_cipher.h"

struct dssh_enc_ctx;
extern "C" int dssh_botan_aes128cbc_init(const uint8_t *, const uint8_t *, bool, dssh_enc_ctx **);
extern "C" int dssh_botan_aes256ctr_init(const uint8_t *, const uint8_t *, bool, dssh_enc_ctx **);
extern "C" int dssh_botan_aes128cbc_crypt(uint8_t *, size_t, dssh_enc_ctx *);
extern "C" int dssh_botan_aes256ctr_crypt(uint8_t *, size_t, dssh_enc_ctx *);
extern "C" void dssh_botan_aes128cbc_cleanup(dssh_enc_ctx *);
extern "C" void dssh_botan_aes256ctr_cleanup(dssh_enc_ctx *);

static thread_local bool fail_nothrow;
static thread_local unsigned nothrow_calls;
static thread_local unsigned nothrow_failures;

void *
operator new(std::size_t size, const std::nothrow_t&) noexcept
{
	if (nothrow_calls == UINT_MAX || nothrow_failures == UINT_MAX)
		std::abort();
	nothrow_calls++;
	if (fail_nothrow) {
		fail_nothrow = false;
		nothrow_failures++;
		return nullptr;
	}
	try {
		return ::operator new(size);
	}
	catch (...) {
		return nullptr;
	}
}

void
operator delete(void *ptr, const std::nothrow_t&) noexcept
{
	::operator delete(ptr);
}

struct module {
	const char *name;
	size_t key_length;
	bool cbc;
	decltype(&dssh_botan_aes128cbc_init) init;
	decltype(&dssh_botan_aes128cbc_crypt) crypt;
	decltype(&dssh_botan_aes128cbc_cleanup) cleanup;
};

static Botan::Cipher_Dir
direction(const module& mod, bool encrypt)
{
	return mod.cbc && !encrypt ? Botan::Cipher_Dir::Decryption : Botan::Cipher_Dir::Encryption;
}

static bool
guarded(const uint8_t *frame, size_t length)
{
	for (size_t i = 0; i < 80; i++) {
		if ((i < 8 || i >= 8 + length) && frame[i] != 0xa5)
			return false;
	}
	return true;
}

static bool
reference(const module& mod, bool encrypt, const uint8_t *key, const uint8_t *iv,
    const uint8_t *input, size_t length, uint8_t *output)
{
	auto cipher = Botan::Cipher_Mode::create_or_throw(mod.name, direction(mod, encrypt));
	cipher->set_key(key, mod.key_length);
	cipher->start(iv, 16);
	std::memcpy(output, input, length);
	return cipher->process(output, length) == length;
}

static bool
reuse_control(const module& mod, bool encrypt, bool observed)
{
	uint8_t keys[2][32];
	uint8_t ivs[2][16];
	std::memset(keys[0], 0x11, sizeof(keys[0]));
	std::memset(keys[1], 0x22, sizeof(keys[1]));
	std::memset(ivs[0], 0x60, sizeof(ivs[0]));
	std::memset(ivs[1], 0x70, sizeof(ivs[1]));
	uint8_t input[64];
	for (size_t i = 0; i < sizeof(input); i++) {
		uint8_t value = (uint8_t)i; /* i is below 64. */
		input[i] = value;
	}
	uint8_t expected[2][64];
	bool valid = true;
	for (unsigned slot = 0; slot < 2; slot++)
		valid = reference(mod, encrypt, keys[slot], ivs[slot], input, sizeof(input), expected[slot]) && valid;
	if (observed)
		dssh_test_botan_cipher_begin(dssh_botan_cipher_fault::none, false);
	dssh_enc_ctx *ctx[2] = {};
	uint8_t frames[2][80];
	for (unsigned slot = 0; slot < 2; slot++) {
		std::memset(frames[slot], 0xa5, sizeof(frames[slot]));
		std::memcpy(frames[slot] + 8, input, sizeof(input));
		int ret = mod.init(keys[slot], ivs[slot], encrypt, &ctx[slot]);
		valid = ret == 0 && ctx[slot] != nullptr && valid;
	}
	valid = ctx[0] != ctx[1] && valid;
	const size_t cbc_parts[] = { 16, 16, 32 };
	const size_t ctr_parts[] = { 1, 15, 17, 31 };
	const size_t *parts = mod.cbc ? cbc_parts : ctr_parts;
	size_t part_count = mod.cbc ? 3 : 4;
	size_t offset = 0;
	unsigned allocations = nothrow_calls;
	for (size_t part = 0; part < part_count; part++) {
		for (unsigned slot = 0; slot < 2; slot++) {
			if (ctx[slot] != nullptr) {
				int ret = mod.crypt(frames[slot] + 8 + offset, parts[part], ctx[slot]);
				valid = ret == 0 && valid;
			}
		}
		offset += parts[part]; /* The fixed partitions sum to 64. */
	}
	for (unsigned slot = 0; slot < 2; slot++)
		valid = guarded(frames[slot], 64) && std::memcmp(frames[slot] + 8, expected[slot], 64) == 0 && valid;
	valid = nothrow_calls == allocations && valid;
	if (observed) {
		auto stats = dssh_test_botan_cipher_stats();
		valid = stats.factories == 2 && stats.key_specs == 2 && stats.key_schedules == 2
		    && stats.starts == 2 && stats.processes == (mod.cbc ? 6u : 8u)
		    && stats.created == 2 && stats.live == 2 && stats.key_bytes == mod.key_length
		    && stats.iv_bytes == 16 && stats.last_direction == direction(mod, encrypt) && valid;
	}
	for (auto value : ctx)
		mod.cleanup(value);
	if (observed) {
		auto stats = dssh_test_botan_cipher_stats();
		valid = stats.live == 0 && stats.destroyed == 2 && valid;
		dssh_test_botan_cipher_end();
	}
	return valid;
}

static bool
init_failure(const module& mod, bool encrypt, dssh_botan_cipher_fault stage, bool allocation, bool wrapper)
{
	dssh_test_botan_cipher_begin(dssh_botan_cipher_fault::none, false);
	uint8_t key[32] = {};
	uint8_t iv[16] = {};
	dssh_enc_ctx *original = nullptr;
	bool valid = mod.init(key, iv, encrypt, &original) == 0 && original != nullptr;
	dssh_enc_ctx *candidate = original;
	dssh_test_botan_cipher_fault_set(stage, allocation);
	unsigned failures = nothrow_failures;
	fail_nothrow = wrapper;
	int ret = mod.init(key, iv, encrypt, &candidate);
	fail_nothrow = false;
	bool factory = stage == dssh_botan_cipher_fault::factory || stage == dssh_botan_cipher_fault::unavailable;
	unsigned created = factory ? 1 : 2;
	auto stats = dssh_test_botan_cipher_stats();
	valid = ret == (wrapper ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT) && candidate == original
	    && stats.factories == 2 && stats.created == created && stats.destroyed == created - 1 && stats.live == 1
	    && stats.key_specs == (factory ? 1u : 2u)
	    && stats.key_schedules == (factory || stage == dssh_botan_cipher_fault::key_spec ? 1u : 2u)
	    && stats.starts == (wrapper || stage == dssh_botan_cipher_fault::start ? 2u : 1u)
	    && stats.processes == 0 && stats.last_direction == direction(mod, encrypt)
	    && nothrow_failures == failures + (wrapper ? 1u : 0u) && valid;
	dssh_test_botan_cipher_fault_set(dssh_botan_cipher_fault::none, false);
	if (candidate != original)
		mod.cleanup(candidate);
	mod.cleanup(original);
	stats = dssh_test_botan_cipher_stats();
	valid = stats.live == 0 && stats.destroyed == stats.created && valid;
	dssh_test_botan_cipher_end();
	return valid;
}

static bool
process_case(const module& mod, bool encrypt, dssh_botan_cipher_fault stage, bool allocation)
{
	uint8_t key[32] = {};
	uint8_t iv[16] = {};
	uint8_t input[16];
	std::memset(input, 0x44, sizeof(input));
	uint8_t expected[16];
	bool valid = reference(mod, encrypt, key, iv, input, sizeof(input), expected);
	dssh_test_botan_cipher_begin(dssh_botan_cipher_fault::none, false);
	dssh_enc_ctx *ctx = nullptr;
	valid = mod.init(key, iv, encrypt, &ctx) == 0 && ctx != nullptr && valid;
	dssh_test_botan_cipher_fault_set(stage, allocation);
	uint8_t frame[80];
	std::memset(frame, 0xa5, sizeof(frame));
	std::memcpy(frame + 8, input, sizeof(input));
	int ret = mod.crypt(frame + 8, sizeof(input), ctx);
	auto stats = dssh_test_botan_cipher_stats();
	valid = ret == (stage == dssh_botan_cipher_fault::none ? 0 : DSSH_ERROR_INIT)
	    && guarded(frame, sizeof(input)) && std::memcmp(frame + 8, expected, sizeof(expected)) == 0
	    && stats.native_written == sizeof(input) && stats.processes == 1
	    && stats.factories == 1 && stats.key_schedules == 1 && stats.starts == 1 && stats.live == 1 && valid;
	if (stage != dssh_botan_cipher_fault::none && stage != dssh_botan_cipher_fault::process)
		valid = stats.reported_written != sizeof(input) && valid;
	/* No retry is promised after changed native state or inconsistent output.
 * The packet layer's terminal/retirement controls are exercised separately. */
	mod.cleanup(ctx);
	stats = dssh_test_botan_cipher_stats();
	valid = stats.live == 0 && stats.created == 1 && stats.destroyed == 1 && valid;
	dssh_test_botan_cipher_end();
	return valid;
}

static bool
parameter_control(const module& mod, bool encrypt)
{
	uint8_t key[32] = {};
	uint8_t iv[16] = {};
	uint8_t input[64];
	std::memset(input, 0x44, sizeof(input));
	uint8_t expected[64];
	bool valid = reference(mod, encrypt, key, iv, input, sizeof(input), expected);
	dssh_test_botan_cipher_begin(dssh_botan_cipher_fault::none, false);
	uint8_t frame[80];
	std::memset(frame, 0xa5, sizeof(frame));
	valid = mod.crypt(frame + 8, 0, nullptr) == DSSH_ERROR_INIT && guarded(frame, 0) && valid;
	mod.cleanup(nullptr);
	dssh_enc_ctx *ctx = nullptr;
	valid = mod.init(key, iv, encrypt, &ctx) == 0 && ctx != nullptr && valid;
	std::memcpy(frame + 8, input, sizeof(input));
	if (mod.cbc) {
		const size_t invalid[] = { 1, 15, 17, 63, SIZE_MAX };
		for (size_t length : invalid) {
			int ret = mod.crypt(frame + 8, length, ctx);
			valid = ret == DSSH_ERROR_INVALID && guarded(frame, 64)
			    && std::memcmp(frame + 8, input, sizeof(input)) == 0 && valid;
		}
	}
	valid = dssh_test_botan_cipher_stats().processes == 0 && valid;
	int ret = mod.crypt(frame + 8, 0, ctx);
	valid = ret == 0 && guarded(frame, 64) && std::memcmp(frame + 8, input, sizeof(input)) == 0 && valid;
	ret = mod.crypt(frame + 8, sizeof(input), ctx);
	auto stats = dssh_test_botan_cipher_stats();
	valid = ret == 0 && guarded(frame, 64) && std::memcmp(frame + 8, expected, sizeof(expected)) == 0
	    && stats.processes == 2 && stats.factories == 1 && stats.key_schedules == 1 && stats.starts == 1 && valid;
	mod.cleanup(ctx);
	valid = dssh_test_botan_cipher_stats().live == 0 && valid;
	dssh_test_botan_cipher_end();
	return valid;
}

static void
record(const module& mod, bool encrypt, const char *label, bool valid,
    unsigned& passed, unsigned& failed, const char *kind = "control")
{
	std::printf("Botan cipher %s %s %s %s: %s\n", mod.name, encrypt ? "encrypt" : "decrypt",
	    label, kind, valid ? "passed" : "failed");
	if (valid)
		passed++;
	else
		failed++;
}

int
main()
{
	const module modules[] = {
		{ "AES-128/CBC/NoPadding", 16, true, dssh_botan_aes128cbc_init, dssh_botan_aes128cbc_crypt, dssh_botan_aes128cbc_cleanup },
		{ "AES-256/CTR-BE", 32, false, dssh_botan_aes256ctr_init, dssh_botan_aes256ctr_crypt, dssh_botan_aes256ctr_cleanup },
	};
	struct stage {
		dssh_botan_cipher_fault value;
		const char *name;
	};
	const stage init_stages[] = {
		{ dssh_botan_cipher_fault::factory, "factory" },
		{ dssh_botan_cipher_fault::key_spec, "key-spec" },
		{ dssh_botan_cipher_fault::key_schedule, "key-schedule" },
		{ dssh_botan_cipher_fault::start, "start" },
	};
	const stage output_stages[] = {
		{ dssh_botan_cipher_fault::none, "exact-output" },
		{ dssh_botan_cipher_fault::zero_output, "zero-output" },
		{ dssh_botan_cipher_fault::short_output, "short-output" },
		{ dssh_botan_cipher_fault::long_output, "long-output" },
		{ dssh_botan_cipher_fault::max_output, "max-output" },
	};
	unsigned passed = 0;
	unsigned failed = 0;
	try {
		for (const auto& mod : modules) {
			for (unsigned role = 0; role < 2; role++) {
				bool encrypt = role == 0;
				record(mod, encrypt, "ordinary-reuse", reuse_control(mod, encrypt, false), passed, failed);
				record(mod, encrypt, "observed-reuse", reuse_control(mod, encrypt, true), passed, failed);
				record(mod, encrypt, "parameter-guards", parameter_control(mod, encrypt), passed, failed);
				record(mod, encrypt, "wrapper-allocation", init_failure(mod, encrypt, dssh_botan_cipher_fault::none, false, true), passed, failed);
				record(mod, encrypt, "unavailable-native", init_failure(mod, encrypt, dssh_botan_cipher_fault::unavailable, false, false), passed, failed);
				for (auto stage : init_stages) {
					for (unsigned kind = 0; kind < 2; kind++)
						record(mod, encrypt, stage.name, init_failure(mod, encrypt, stage.value, kind != 0, false),
						    passed, failed, kind == 0 ? "exception" : "bad_alloc");
				}
				for (unsigned kind = 0; kind < 2; kind++)
					record(mod, encrypt, "process", process_case(mod, encrypt, dssh_botan_cipher_fault::process, kind != 0),
					    passed, failed, kind == 0 ? "exception" : "bad_alloc");
				for (auto stage : output_stages)
					record(mod, encrypt, stage.name, process_case(mod, encrypt, stage.value, false), passed, failed);
			}
		}
	}
	catch (...) {
		std::fprintf(stderr, "Unexpected exception escaped the native cipher test\n");
		return 1;
	}
	std::printf("Botan cipher boundaries: %u passed, %u failed\n", passed, failed);
	return failed == 0 && passed == 80 ? 0 : 1;
}
