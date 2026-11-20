#ifndef DSSH_TEST_BOTAN_MAC_H
#define DSSH_TEST_BOTAN_MAC_H

#include <botan/mac.h>
#include <memory>
#include <string_view>

enum class dssh_botan_mac_fault { none, factory, unavailable, key_spec, key_schedule, size, update, final };
struct dssh_botan_mac_stats {
	unsigned factories;
	unsigned key_specs;
	unsigned key_schedules;
	unsigned sizes;
	unsigned updates;
	unsigned finals;
	unsigned created;
	unsigned destroyed;
	unsigned live;
};
void dssh_test_botan_mac_begin(dssh_botan_mac_fault fault, bool allocation);
void dssh_test_botan_mac_fault_set(dssh_botan_mac_fault fault, bool allocation);
dssh_botan_mac_stats dssh_test_botan_mac_stats();
void dssh_test_botan_mac_end();
std::unique_ptr<Botan::MessageAuthenticationCode> dssh_test_botan_mac_create(std::string_view name);

#endif
