#include <new>
#include "deucessh.h"
#include "key_algo/key-context-botan.h"

struct dssh_key_algo_ctx;
extern "C" dssh_key_algo_ctx *
test_empty_key_context(void)
{
	auto *context = new (std::nothrow) dssh_botan_key_context{};
	auto *opaque = reinterpret_cast<dssh_key_algo_ctx *>(context);
	return opaque;
}

/* Warm the actual native owners before a zero-allocation signature round.
 * Holding all leases at once removes dependence on worker scheduling. */
extern "C" int
test_warm_native_key_context(dssh_key_algo_ctx *opaque, unsigned int slots)
{
	if (opaque == nullptr || slots == 0 || slots > 8)
		return DSSH_ERROR_INVALID;
	auto *context = reinterpret_cast<dssh_botan_key_context *>(opaque);
	dssh_botan_key_guard owners[8];
	for (unsigned int i = 0; i < slots; i++) {
		int ret = owners[i].acquire(context->keys);
		if (ret < 0)
			return ret;
	}
	return 0;
}
