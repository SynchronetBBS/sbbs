#include <cstdio>
#include <cstring>
#include <new>
#include "deucessh.h"
#include "deucessh-crypto.h"
#include "dssh_test_botan_hash.h"

/* Replace only the standard nonthrowing allocation entry in this executable.
 * Ordinary factory/provider allocations retain their normal behavior. */
static thread_local bool fail_nothrow;
static thread_local unsigned nothrow_failures;

void *
operator new(std::size_t size, const std::nothrow_t&) noexcept
{
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

static bool
context_allocation_failure(const char *name)
{
	dssh_test_botan_hash_begin(dssh_botan_hash_fault::none, false);
	dssh_hash_ctx *ctx = nullptr;
	size_t length = 17;
	nothrow_failures = 0;
	fail_nothrow = true;
	int ret = dssh_hash_init(&ctx, name, &length);
	fail_nothrow = false;
	auto stats = dssh_test_botan_hash_stats();
	bool valid = ret == DSSH_ERROR_ALLOC && ctx == nullptr && length == 17
	    && nothrow_failures == 1 && stats.factories == 1 && stats.sizes == 1 && stats.live == 0;
	dssh_hash_free(ctx);
	dssh_test_botan_hash_end();
	if (!valid)
		std::fprintf(stderr, "Botan context allocation: result=%d failures=%u factories=%u sizes=%u live=%u\n",
		    ret, nothrow_failures, stats.factories, stats.sizes, stats.live);
	return valid;
}

static bool
frame_guard(const uint8_t *frame, size_t length)
{
	for (size_t i = 0; i < 96; i++) {
		if ((i < 8 || i >= 8 + length) && frame[i] != 0xa5)
			return false;
	}
	return true;
}

static bool
init_failure(const char *name, dssh_botan_hash_fault stage, bool allocation)
{
	dssh_test_botan_hash_begin(dssh_botan_hash_fault::none, false);
	dssh_hash_ctx *original = nullptr;
	size_t length = 0;
	bool valid = dssh_hash_init(&original, name, &length) == 0;
	dssh_hash_ctx *candidate = original;
	size_t sentinel = 17;
	dssh_test_botan_hash_fault_set(stage, allocation);
	int ret = 77;
	bool escaped = false;
	try {
		ret = dssh_hash_init(&candidate, name, &sentinel);
	}
	catch (...) {
		escaped = true;
	}
	int expected = allocation ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT;
	valid = valid && !escaped && ret == expected && candidate == original && sentinel == 17;
	dssh_test_botan_hash_fault_set(dssh_botan_hash_fault::none, false);
	if (candidate != original)
		dssh_hash_free(candidate);
	dssh_hash_free(original);
	valid = valid && dssh_test_botan_hash_stats().live == 0;
	if (!valid)
		std::fprintf(stderr, "Botan init fault: result=%d expected=%d escaped=%d\n", ret, expected, escaped);
	dssh_test_botan_hash_end();
	return valid;
}

static bool
oneshot_failure(const char *name, dssh_botan_hash_fault stage, bool allocation)
{
	dssh_test_botan_hash_begin(stage, allocation);
	const uint8_t abc[] = { 'a', 'b', 'c' };
	uint8_t frame[96];
	std::memset(frame, 0xa5, sizeof(frame));
	int ret = 77;
	bool escaped = false;
	try {
		ret = dssh_hash_oneshot(name, abc, sizeof(abc), frame + 8, 64);
	}
	catch (...) {
		escaped = true;
	}
	int expected = allocation ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT;
	auto stats = dssh_test_botan_hash_stats();
	bool valid = !escaped && ret == expected && stats.live == 0
	    && frame_guard(frame, std::strcmp(name, "SHA-256") == 0 ? 32 : 64);
	if (stage == dssh_botan_hash_fault::zero_size)
		valid = valid && stats.updates == 0 && stats.finals == 0;
	if (!valid)
		std::fprintf(stderr, "Botan oneshot fault: result=%d expected=%d escaped=%d live=%u\n",
		    ret, expected, escaped, stats.live);
	dssh_test_botan_hash_end();
	return valid;
}

static bool
state_failure(const char *name, dssh_botan_hash_fault stage, bool allocation)
{
	dssh_test_botan_hash_begin(dssh_botan_hash_fault::none, false);
	dssh_hash_ctx *ctx = nullptr;
	size_t length = 0;
	bool valid = dssh_hash_init(&ctx, name, &length) == 0;
	dssh_test_botan_hash_fault_set(stage, allocation);
	const uint8_t input = 7;
	uint8_t frame[96];
	std::memset(frame, 0xa5, sizeof(frame));
	int ret = 77;
	bool escaped = false;
	try {
		ret = stage == dssh_botan_hash_fault::update ? dssh_hash_update(ctx, &input, 1)
		    : dssh_hash_final(ctx, frame + 8, 64);
	}
	catch (...) {
		escaped = true;
	}
	int expected = allocation ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT;
	auto before = dssh_test_botan_hash_stats();
	valid = valid && !escaped && ret == expected && before.live == 0 && frame_guard(frame, length);
	dssh_test_botan_hash_fault_set(dssh_botan_hash_fault::none, false);
	valid = dssh_hash_update(ctx, &input, 1) == DSSH_ERROR_INVALID && valid;
	valid = dssh_hash_final(ctx, frame + 8, 64) == DSSH_ERROR_INVALID && valid;
	auto after = dssh_test_botan_hash_stats();
	valid = valid && before.sizes == after.sizes && before.updates == after.updates
	    && before.finals == after.finals;
	dssh_hash_free(ctx);
	valid = valid && dssh_test_botan_hash_stats().live == 0;
	if (!valid)
		std::fprintf(stderr, "Botan state fault: result=%d expected=%d escaped=%d live=%u\n",
		    ret, expected, escaped, before.live);
	dssh_test_botan_hash_end();
	return valid;
}

static bool
reuse_control(const char *name)
{
	dssh_test_botan_hash_begin(dssh_botan_hash_fault::none, false);
	dssh_hash_ctx *ctx = nullptr;
	size_t length = 0;
	bool valid = dssh_hash_init(&ctx, name, &length) == 0;
	uint8_t frame[96];
	for (unsigned repeat = 0; repeat < 4 && valid; repeat++) {
		std::memset(frame, 0xa5, sizeof(frame));
		auto before = dssh_test_botan_hash_stats();
		valid = dssh_hash_update(ctx, nullptr, 1) == DSSH_ERROR_INVALID
		    && dssh_hash_final(ctx, frame + 8, length - 1) == DSSH_ERROR_INVALID;
		auto rejected = dssh_test_botan_hash_stats();
		valid = valid && before.sizes == rejected.sizes && before.updates == rejected.updates
		    && before.finals == rejected.finals && dssh_hash_final(ctx, frame + 8, length) == 0
		    && frame_guard(frame, length);
	}
	auto stats = dssh_test_botan_hash_stats();
	valid = valid && stats.factories == 1 && stats.live == 1 && stats.finals == 4;
	dssh_hash_free(ctx);
	valid = valid && dssh_test_botan_hash_stats().live == 0;
	dssh_test_botan_hash_end();
	return valid;
}

int
main()
{
	const char *names[] = { "SHA-256", "SHA-512" };
	const dssh_botan_hash_fault stages[] = { dssh_botan_hash_fault::factory, dssh_botan_hash_fault::size,
	    dssh_botan_hash_fault::zero_size, dssh_botan_hash_fault::update, dssh_botan_hash_fault::final };
	bool valid = true;
	unsigned cases = 0;
	for (const char *name : names) {
		bool allocation = context_allocation_failure(name);
		valid = valid && allocation;
		cases++;
		bool control = reuse_control(name);
		valid = valid && control;
		cases++;
		for (auto stage : stages) {
			for (unsigned kind = 0; kind < 2; kind++) {
				if (stage == dssh_botan_hash_fault::zero_size && kind != 0)
					continue;
				if (stage == dssh_botan_hash_fault::factory || stage == dssh_botan_hash_fault::size
				    || stage == dssh_botan_hash_fault::zero_size) {
					bool matched = init_failure(name, stage, kind != 0);
					valid = valid && matched;
					cases++;
				}
				bool matched = oneshot_failure(name, stage, kind != 0);
				valid = valid && matched;
				cases++;
				if (stage == dssh_botan_hash_fault::update || stage == dssh_botan_hash_fault::final
				    || stage == dssh_botan_hash_fault::size) {
					matched = state_failure(name, stage, kind != 0);
					valid = valid && matched;
					cases++;
				}
			}
		}
	}
	std::printf("Botan hash boundaries: %u invocations, %s\n", cases, valid ? "passed" : "failed");
	return valid ? 0 : 1;
}
