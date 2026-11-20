/* Exercise the native C++ callbacks installed by both Botan MAC modules.
 * Registration/vector/transport tests remain in their ordinary executables. */
#include <cstdio>
#include <cstring>
#include <new>
#include "deucessh.h"
#include "dssh_test_botan_mac.h"

struct dssh_mac_ctx;
extern "C" int dssh_botan_hmac256_init(const uint8_t *, dssh_mac_ctx **);
extern "C" int dssh_botan_hmac512_init(const uint8_t *, dssh_mac_ctx **);
extern "C" int dssh_botan_hmac256_generate(const uint8_t *, size_t, uint8_t *, dssh_mac_ctx *);
extern "C" int dssh_botan_hmac512_generate(const uint8_t *, size_t, uint8_t *, dssh_mac_ctx *);
extern "C" void dssh_botan_hmac256_cleanup(dssh_mac_ctx *);
extern "C" void dssh_botan_hmac512_cleanup(dssh_mac_ctx *);

static thread_local bool fail_nothrow;
static thread_local unsigned nothrow_calls;
static thread_local unsigned nothrow_failures;

/* Isolate failure of the module's wrapper allocation. Native provider/factory
 * allocations still run normally; this replacement exists only in this test. */
void *
operator new(std::size_t size, const std::nothrow_t&) noexcept
{
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
	size_t length;
	decltype(&dssh_botan_hmac256_init) init;
	decltype(&dssh_botan_hmac256_generate) generate;
	decltype(&dssh_botan_hmac256_cleanup) cleanup;
};

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
reuse_control(const module& mod, bool observed)
{
	uint8_t keys[2][64] = {};
	std::memset(keys[0], 0x0b, 20);
	std::memset(keys[1], 0x42, sizeof(keys[1]));
	const uint8_t data[] = "Hi There";
	uint8_t answers[2][64] = {};
	for (unsigned slot = 0; slot < 2; slot++) {
		auto ref = Botan::MessageAuthenticationCode::create_or_throw(mod.name);
		ref->set_key(keys[slot], mod.length);
		ref->update(data, sizeof(data) - 1);
		ref->final(answers[slot]);
	}
	if (observed)
		dssh_test_botan_mac_begin(dssh_botan_mac_fault::none, false);
	dssh_mac_ctx *ctx[2] = {};
	bool valid = true;
	for (unsigned slot = 0; slot < 2; slot++) {
		int ret = mod.init(keys[slot], &ctx[slot]);
		valid = ret == 0 && ctx[slot] != nullptr && valid;
	}
	unsigned allocations = nothrow_calls;
	for (unsigned repeat = 0; repeat < 4; repeat++) {
		for (unsigned slot = 0; slot < 2; slot++) {
			if (ctx[slot] == nullptr)
				continue;
			uint8_t frame[80];
			std::memset(frame, 0xa5, sizeof(frame));
			int ret = mod.generate(data, sizeof(data) - 1, frame + 8, ctx[slot]);
			valid = ret == 0 && guarded(frame, mod.length)
			    && std::memcmp(frame + 8, answers[slot], mod.length) == 0 && valid;
		}
	}
	valid = allocations == nothrow_calls && valid;
	if (observed) {
		auto stats = dssh_test_botan_mac_stats();
		valid = stats.factories == 2 && stats.created == 2 && stats.live == 2
		    && stats.key_specs == 2 && stats.key_schedules == 2 && stats.sizes == 8
		    && stats.updates == 8 && stats.finals == 8 && valid;
	}
	for (auto value : ctx)
		mod.cleanup(value);
	mod.cleanup(nullptr);
	if (observed) {
		auto stats = dssh_test_botan_mac_stats();
		valid = stats.live == 0 && stats.destroyed == 2 && valid;
		dssh_test_botan_mac_end();
	}
	return valid;
}

static bool
init_failure(const module& mod, dssh_botan_mac_fault stage, bool allocation, bool wrapper)
{
	dssh_test_botan_mac_begin(dssh_botan_mac_fault::none, false);
	uint8_t key[64] = {};
	dssh_mac_ctx *original = nullptr;
	bool valid = mod.init(key, &original) == 0 && original != nullptr;
	dssh_mac_ctx *candidate = original;
	dssh_test_botan_mac_fault_set(stage, allocation);
	unsigned before = nothrow_failures;
	fail_nothrow = wrapper;
	int ret = mod.init(key, &candidate);
	fail_nothrow = false;
	int expected = wrapper ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT;
	auto stats = dssh_test_botan_mac_stats();
	unsigned created = stage == dssh_botan_mac_fault::factory || stage == dssh_botan_mac_fault::unavailable ? 1 : 2;
	valid = ret == expected && candidate == original && stats.factories == 2
	    && stats.created == created && stats.destroyed == created - 1 && stats.live == 1
	    && nothrow_failures == before + (wrapper ? 1u : 0u) && valid;
	dssh_test_botan_mac_fault_set(dssh_botan_mac_fault::none, false);
	if (candidate != original)
		mod.cleanup(candidate);
	mod.cleanup(original);
	stats = dssh_test_botan_mac_stats();
	valid = stats.live == 0 && stats.destroyed == stats.created && valid;
	dssh_test_botan_mac_end();
	return valid;
}

static bool
generate_failure(const module& mod, dssh_botan_mac_fault stage, bool allocation)
{
	dssh_test_botan_mac_begin(dssh_botan_mac_fault::none, false);
	uint8_t key[64] = {};
	dssh_mac_ctx *ctx = nullptr;
	bool valid = mod.init(key, &ctx) == 0 && ctx != nullptr;
	dssh_test_botan_mac_fault_set(stage, allocation);
	const uint8_t data[] = { 1, 2, 3 };
	uint8_t frame[80];
	std::memset(frame, 0xa5, sizeof(frame));
	int ret = mod.generate(data, sizeof(data), frame + 8, ctx);
	auto stats = dssh_test_botan_mac_stats();
	valid = ret == DSSH_ERROR_INIT && guarded(frame, mod.length) && stats.live == 1
	    && stats.factories == 1 && stats.key_schedules == 1 && stats.updates == 1
	    && stats.sizes == (stage == dssh_botan_mac_fault::update ? 0u : 1u)
	    && stats.finals == (stage == dssh_botan_mac_fault::final ? 1u : 0u) && valid;
	/* A transport failure retires the context. No retry on a partially mutated
 * native object, or preservation of the tag area after final, is promised. */
	mod.cleanup(ctx);
	stats = dssh_test_botan_mac_stats();
	valid = stats.live == 0 && stats.created == 1 && stats.destroyed == 1 && valid;
	dssh_test_botan_mac_end();
	return valid;
}

static bool
null_control(const module& mod)
{
	dssh_test_botan_mac_begin(dssh_botan_mac_fault::none, false);
	uint8_t frame[80];
	std::memset(frame, 0xa5, sizeof(frame));
	int ret = mod.generate(nullptr, 0, frame + 8, nullptr);
	mod.cleanup(nullptr);
	auto stats = dssh_test_botan_mac_stats();
	bool valid = ret == DSSH_ERROR_INIT && guarded(frame, 0) && stats.factories == 0
	    && stats.created == 0 && stats.updates == 0 && stats.finals == 0;
	dssh_test_botan_mac_end();
	return valid;
}

static void
record(const module& mod, const char *label, bool valid, unsigned& passed, unsigned& failed,
    const char *kind = "control")
{
	std::printf("Botan MAC %s %s %s: %s\n", mod.name, label, kind, valid ? "passed" : "failed");
	if (valid)
		passed++;
	else
		failed++;
}

int
main()
{
	const module modules[] = {
		{ "HMAC(SHA-256)", 32, dssh_botan_hmac256_init, dssh_botan_hmac256_generate, dssh_botan_hmac256_cleanup },
		{ "HMAC(SHA-512)", 64, dssh_botan_hmac512_init, dssh_botan_hmac512_generate, dssh_botan_hmac512_cleanup },
	};
	struct stage {
		dssh_botan_mac_fault value;
		const char *name;
	};
	const stage init_stages[] = {
		{ dssh_botan_mac_fault::factory, "factory" },
		{ dssh_botan_mac_fault::key_spec, "key-spec" },
		{ dssh_botan_mac_fault::key_schedule, "key-schedule" },
	};
	const stage generate_stages[] = {
		{ dssh_botan_mac_fault::update, "update" },
		{ dssh_botan_mac_fault::size, "output-length" },
		{ dssh_botan_mac_fault::final, "final" },
	};
	unsigned passed = 0;
	unsigned failed = 0;
	try {
		for (const auto& mod : modules) {
			record(mod, "ordinary-reuse", reuse_control(mod, false), passed, failed);
			record(mod, "observed-reuse", reuse_control(mod, true), passed, failed);
			record(mod, "null-context", null_control(mod), passed, failed);
			record(mod, "wrapper-allocation", init_failure(mod, dssh_botan_mac_fault::none, false, true), passed, failed);
			record(mod, "unavailable-native", init_failure(mod, dssh_botan_mac_fault::unavailable, false, false), passed, failed);
			for (auto stage : init_stages) {
				for (unsigned kind = 0; kind < 2; kind++)
					record(mod, stage.name, init_failure(mod, stage.value, kind != 0, false), passed, failed,
					    kind == 0 ? "exception" : "bad_alloc");
			}
			for (auto stage : generate_stages) {
				for (unsigned kind = 0; kind < 2; kind++)
					record(mod, stage.name, generate_failure(mod, stage.value, kind != 0), passed, failed,
					    kind == 0 ? "exception" : "bad_alloc");
			}
		}
	}
	catch (...) {
		std::fprintf(stderr, "Unexpected exception escaped the native MAC test\n");
		return 1;
	}
	std::printf("Botan MAC boundaries: %u passed, %u failed\n", passed, failed);
	return failed == 0 && passed == 34 ? 0 : 1;
}
