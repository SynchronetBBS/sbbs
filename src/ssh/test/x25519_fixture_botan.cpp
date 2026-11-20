#include <botan/x25519.h>
#include <memory>
#include <new>
#include "x25519_fixture.h"

/* Construct the actual provider object consumed by the private adapter table.
 * Production key generation is unchanged; this fixture imports public vectors. */
extern "C" int
test_x25519_import(const uint8_t scalar[32], void **out)
{
	try {
		auto key = std::make_unique<Botan::X25519_PrivateKey>(std::span<const uint8_t>(scalar, 32));
		*out = key.release();
		return 0;
	}
	catch (const std::bad_alloc&) {
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}
