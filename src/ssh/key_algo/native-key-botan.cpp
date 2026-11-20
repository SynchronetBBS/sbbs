#include <botan/pkcs8.h>
#include <botan/secmem.h>
#include <memory>
#include <new>
#include <span>

#include "native-key-botan.h"
#include "deucessh.h"

namespace {
struct key_factory {
	Botan::secure_vector<uint8_t> encoding;
	~key_factory() noexcept
	{
		if (!encoding.empty())
			dssh_cleanse(encoding.data(), encoding.size());
	}
};

extern "C" {
static int
clone_key(void *opaque, void **out)
{
	key_factory *factory = static_cast<key_factory *>(opaque);
	try {
		std::span<const uint8_t> input(factory->encoding.data(), factory->encoding.size());
		auto key = Botan::PKCS8::load_key(input);
		if (key == nullptr)
			return DSSH_ERROR_INIT;
		*out = key.release();
		return 0;
	}
	catch (const std::bad_alloc &) {
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static void
destroy_key(void *opaque)
{
	Botan::Private_Key *key = static_cast<Botan::Private_Key *>(opaque);
	delete key;
}

static void
destroy_factory(void *opaque)
{
	key_factory *factory = static_cast<key_factory *>(opaque);
	delete factory;
}
}
}

DSSH_PRIVATE int
dssh_botan_key_pool_create(struct dssh_native_key_pool **out,
    std::unique_ptr<Botan::Private_Key> &key) noexcept
{
	if (out == nullptr || *out != nullptr || key == nullptr)
		return DSSH_ERROR_INVALID;
	try {
		auto factory = std::make_unique<key_factory>();
		factory->encoding = Botan::PKCS8::BER_encode(*key);
		if (factory->encoding.empty())
			return DSSH_ERROR_INIT;
		int ret = dssh_native_key_pool_create(out, factory.get(), key.get(),
		    clone_key, destroy_key, destroy_factory);
		if (ret < 0)
			return ret;
		factory.release();
		key.release();
		return 0;
	}
	catch (const std::bad_alloc &) {
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}
