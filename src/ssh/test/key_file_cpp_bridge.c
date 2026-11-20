/* Inspect the actual C module type; native C++ fixtures never guess its layout. */
#include <string.h>
#include "deucessh-key-algo.h"
#include "dssh_test_internal.h"
#include "key_file_cpp_alloc.h"

bool
dssh_test_key_ed_public(uint8_t *out, size_t capacity, size_t *length)
{
	dssh_key_algo key = find_key_algo("ssh-ed25519");
	const uint8_t *blob = NULL;
	size_t count = 0;
	if (key == NULL || key->pubkey == NULL || key->pubkey(&blob, &count, key->ctx) != 0
	    || blob == NULL || count == 0 || count > capacity)
		return false;
	memcpy(out, blob, count);
	*length = count;
	return true;
}

bool
dssh_test_key_ed_signable(const uint8_t *public_blob, size_t public_length)
{
	dssh_key_algo key = find_key_algo("ssh-ed25519");
	if (key == NULL || key->sign == NULL || key->verify == NULL)
		return false;
	const uint8_t message[] = {'v', 'a', 'l', 'i', 'd'};
	struct dssh_bytes_result signature = {0};
	int ret = key->sign(&signature, message, sizeof(message), key->ctx);
	bool valid = ret == 0 && signature.bytes.data != NULL && signature.bytes.len > 0
	    && signature.lease.release != NULL;
	if (valid)
		valid = key->verify(public_blob, public_length, signature.bytes.data,
		    signature.bytes.len, message, sizeof(message)) == 0;
	dssh_result_release(&signature.lease);
	return valid;
}
