#include <array>
#include <cstdio>
#include <cstring>
#include "deucessh.h"
#include "deucessh-crypto.h"
#include "dssh_test_botan_base64.h"

static bool
encode_case(size_t length, dssh_botan_base64_fault fault, unsigned call)
{
	std::array<uint8_t, 385> input{};
	std::array<char, 532> frame;
	frame.fill('!');
	dssh_test_botan_base64_begin(fault, call);
	bool escaped = false;
	int result = 77;
	try {
		result = dssh_base64_encode(input.data(), length, frame.data() + 8, 516 + 1);
	}
	catch (...) {
		escaped = true;
	}
	unsigned calls = dssh_botan_base64_calls;
	dssh_test_botan_base64_begin(dssh_botan_base64_fault::none, 0);
	size_t extent = 4 * ((length + 2) / 3); /* length <= 385 */
	int encoded_length = static_cast<int>(extent); /* extent <= 516 */
	bool success = fault == dssh_botan_base64_fault::none;
	int expected = success ? encoded_length : DSSH_ERROR_INIT;
	unsigned wanted_calls = length == 0 ? 0 : length <= 192 ? 1 : 3;
	bool valid = !escaped && result == expected && calls == (success ? wanted_calls : call);
	for (size_t i = 0; i < frame.size(); i++) {
		if ((i < 8 || i >= 8 + extent + 1) && frame[i] != '!')
			valid = false;
	}
	for (uint8_t byte : input) {
		if (byte != 0)
			valid = false;
	}
	if (success) {
		for (size_t i = 0; i < extent; i++) {
			bool padding = length % 3 != 0 && i >= extent - (3 - length % 3);
			if (frame[8 + i] != (padding ? '=' : 'A'))
				valid = false;
		}
		if (frame[8 + extent] != '\0')
			valid = false;
	}
	if (!valid)
		std::fprintf(stderr, "base64 native result=%d expected=%d calls=%u wanted=%u escaped=%d\n",
		    result, expected, calls, success ? wanted_calls : call, escaped);
	return valid;
}

int main(void)
{
	unsigned cases = 0, failures = 0;
	const dssh_botan_base64_fault faults[] = {
		dssh_botan_base64_fault::zero_count, dssh_botan_base64_fault::short_count,
		dssh_botan_base64_fault::long_count, dssh_botan_base64_fault::maximal_count,
		dssh_botan_base64_fault::short_input, dssh_botan_base64_fault::long_input,
		dssh_botan_base64_fault::exception, dssh_botan_base64_fault::allocation
	};
	for (auto fault : faults) {
		for (unsigned call = 1; call <= 3; call++) {
			if (!encode_case(385, fault, call))
				failures++;
			cases++;
		}
	}
	for (size_t length : {size_t(0), size_t(1), size_t(385)}) {
		if (!encode_case(length, dssh_botan_base64_fault::none, 0))
			failures++;
		cases++;
	}
	for (unsigned mode = 0; mode < 4; mode++) {
		uint8_t input[3] = {1, 2, 3};
		char output[5] = {'!', '!', '!', '!', '!'};
		dssh_test_botan_base64_begin(dssh_botan_base64_fault::exception, 1);
		int result = dssh_base64_encode(mode == 0 ? nullptr : input,
		    mode == 3 ? SIZE_MAX : sizeof(input), mode == 1 ? nullptr : output,
		    mode == 2 ? 4 : sizeof(output));
		int expected = mode < 2 ? DSSH_ERROR_INVALID : DSSH_ERROR_TOOLONG;
		if (result != expected || dssh_botan_base64_calls != 0
		    || std::memcmp(output, "!!!!!", sizeof(output)) != 0)
			failures++;
		cases++;
		dssh_test_botan_base64_begin(dssh_botan_base64_fault::none, 0);
	}
	std::printf("base64_native cases=%u failures=%u\n", cases, failures);
	return failures == 0 ? 0 : 1;
}
