#ifndef DSSH_TEST_BOTAN_BASE64_H
#define DSSH_TEST_BOTAN_BASE64_H

#include <botan/base64.h>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>

enum class dssh_botan_base64_fault {
	none, zero_count, short_count, long_count, maximal_count,
	short_input, long_input, exception, allocation
};
inline thread_local dssh_botan_base64_fault dssh_botan_base64_fault_value;
inline thread_local unsigned dssh_botan_base64_fail_call;
inline thread_local unsigned dssh_botan_base64_calls;

inline void
dssh_test_botan_base64_begin(dssh_botan_base64_fault fault, unsigned call)
{
	dssh_botan_base64_fault_value = fault;
	dssh_botan_base64_fail_call = call;
	dssh_botan_base64_calls = 0;
}

/* Always perform the native encoding before altering only its reported result
 * or throwing. Production calls the native overload directly. */
inline size_t
dssh_test_botan_base64_encode(char *out, const uint8_t *in, size_t length,
    size_t& consumed, bool final)
{
	size_t count = Botan::base64_encode(out, in, length, consumed, final);
	if (dssh_botan_base64_calls < UINT_MAX)
		dssh_botan_base64_calls++;
	if (dssh_botan_base64_calls != dssh_botan_base64_fail_call)
		return count;
	switch (dssh_botan_base64_fault_value) {
		case dssh_botan_base64_fault::none:
			break;
		case dssh_botan_base64_fault::zero_count:
			return 0;
		case dssh_botan_base64_fault::short_count:
			return count == 0 ? 0 : count - 1;
		case dssh_botan_base64_fault::long_count:
			return count == SIZE_MAX ? SIZE_MAX : count + 1;
		case dssh_botan_base64_fault::maximal_count:
			return SIZE_MAX;
		case dssh_botan_base64_fault::short_input:
			consumed = consumed == 0 ? 0 : consumed - 1;
			break;
		case dssh_botan_base64_fault::long_input:
			consumed = consumed == SIZE_MAX ? SIZE_MAX : consumed + 1;
			break;
		case dssh_botan_base64_fault::exception:
			throw std::runtime_error("Base64 native boundary control");
		case dssh_botan_base64_fault::allocation:
			throw std::bad_alloc();
	}
	return count;
}

#endif
