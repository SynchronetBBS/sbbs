/* Private native key construction/borrowing; no installed C++ surface. */
#ifndef DSSH_NATIVE_KEY_BOTAN_H
#define DSSH_NATIVE_KEY_BOTAN_H

#include <memory>
#include <botan/pk_keys.h>
#include "native-key-pool.h"

DSSH_PRIVATE int dssh_botan_key_pool_create(struct dssh_native_key_pool **out,
    std::unique_ptr<Botan::Private_Key> &key) noexcept;

class dssh_botan_key_guard {
	struct dssh_native_key_result result = {};
public:
	dssh_botan_key_guard() = default;
	dssh_botan_key_guard(const dssh_botan_key_guard &) = delete;
	dssh_botan_key_guard &operator=(const dssh_botan_key_guard &) = delete;
	~dssh_botan_key_guard() noexcept
	{
		dssh_result_release(&result.lease);
	}
	int acquire(struct dssh_native_key_pool *pool) noexcept
	{
		return dssh_native_key_acquire(pool, &result);
	}
	Botan::Private_Key *get() const noexcept
	{
		Botan::Private_Key *key = static_cast<Botan::Private_Key *>(result.value);
		return key;
	}
};

#endif
