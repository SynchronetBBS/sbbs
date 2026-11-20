#ifndef DSSH_TEST_BOTAN_HASH_H
#define DSSH_TEST_BOTAN_HASH_H

#include <botan/hash.h>
#include <memory>
#include <string_view>

enum class dssh_botan_hash_fault { none, factory, size, zero_size, update, final };
struct dssh_botan_hash_stats {
	unsigned factories;
	unsigned sizes;
	unsigned updates;
	unsigned finals;
	unsigned live;
};
void dssh_test_botan_hash_begin(dssh_botan_hash_fault fault, bool allocation);
void dssh_test_botan_hash_fault_set(dssh_botan_hash_fault fault, bool allocation);
dssh_botan_hash_stats dssh_test_botan_hash_stats();
void dssh_test_botan_hash_end();
std::unique_ptr<Botan::HashFunction> dssh_test_botan_hash_create(std::string_view name);

#endif
