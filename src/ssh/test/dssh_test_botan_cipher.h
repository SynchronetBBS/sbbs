#ifndef DSSH_TEST_BOTAN_CIPHER_H
#define DSSH_TEST_BOTAN_CIPHER_H

#include <botan/cipher_mode.h>
#include <memory>
#include <string_view>

enum class dssh_botan_cipher_fault {
	none, factory, unavailable, key_spec, key_schedule, start, process,
	zero_output, short_output, long_output, max_output,
};
struct dssh_botan_cipher_stats {
	unsigned factories;
	unsigned key_specs;
	unsigned key_schedules;
	unsigned starts;
	unsigned processes;
	unsigned created;
	unsigned destroyed;
	unsigned live;
	size_t key_bytes;
	size_t iv_bytes;
	size_t native_written;
	size_t reported_written;
	Botan::Cipher_Dir last_direction;
};
void dssh_test_botan_cipher_begin(dssh_botan_cipher_fault fault, bool allocation);
void dssh_test_botan_cipher_fault_set(dssh_botan_cipher_fault fault, bool allocation);
dssh_botan_cipher_stats dssh_test_botan_cipher_stats();
void dssh_test_botan_cipher_end();
std::unique_ptr<Botan::Cipher_Mode> dssh_test_botan_cipher_create(std::string_view name, Botan::Cipher_Dir direction);

#endif
