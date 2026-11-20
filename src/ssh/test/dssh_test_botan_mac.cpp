/* Inactive calls use the real Botan factory with no observer allocation.
 * Active objects delegate to a real keyed MAC and can throw after native
 * mutation, so the caller must retire them through its ordinary cleanup. */
#include "dssh_test_botan_mac.h"
#include <climits>
#include <new>
#include <stdexcept>
#include <utility>

static thread_local bool enabled;
static thread_local bool allocation_failure;
static thread_local dssh_botan_mac_fault fault;
static thread_local dssh_botan_mac_stats stats;

static void
count(unsigned& value)
{
	if (value == UINT_MAX)
		throw std::overflow_error("Botan MAC observer counter exhausted");
	value++;
}

static void
fail_if(dssh_botan_mac_fault stage)
{
	if (fault != stage)
		return;
	if (allocation_failure)
		throw std::bad_alloc();
	throw std::runtime_error("injected Botan MAC failure");
}

class observed_mac final : public Botan::MessageAuthenticationCode {
	public:
		explicit observed_mac(std::unique_ptr<Botan::MessageAuthenticationCode> value) : inner(std::move(value))
		{
			count(stats.created);
			count(stats.live);
		}
		~observed_mac() override
		{
			/* Counters are bounded by the number of successful constructors. */
			stats.destroyed++;
			stats.live--;
		}
		size_t output_length() const override
		{
			count(stats.sizes);
			fail_if(dssh_botan_mac_fault::size);
			return inner->output_length();
		}
		Botan::Key_Length_Specification key_spec() const override
		{
			count(stats.key_specs);
			fail_if(dssh_botan_mac_fault::key_spec);
			return inner->key_spec();
		}
		bool has_keying_material() const override
		{
			return inner->has_keying_material();
		}
		void clear() override
		{
			inner->clear();
		}
		std::string name() const override
		{
			return inner->name();
		}
		std::unique_ptr<Botan::MessageAuthenticationCode> new_object() const override
		{
			return inner->new_object();
		}
	private:
		void key_schedule(std::span<const uint8_t> key) override
		{
			count(stats.key_schedules);
			inner->set_key(key);
			fail_if(dssh_botan_mac_fault::key_schedule);
		}
		void start_msg(std::span<const uint8_t> nonce) override
		{
			inner->start(nonce);
		}
		void add_data(std::span<const uint8_t> input) override
		{
			count(stats.updates);
			inner->update(input);
			fail_if(dssh_botan_mac_fault::update);
		}
		void final_result(std::span<uint8_t> output) override
		{
			count(stats.finals);
			inner->final(output);
			fail_if(dssh_botan_mac_fault::final);
		}
		std::unique_ptr<Botan::MessageAuthenticationCode> inner;
};

void
dssh_test_botan_mac_begin(dssh_botan_mac_fault stage, bool allocation)
{
	if (stats.live != 0)
		throw std::logic_error("Botan MAC observer still owns native objects");
	stats = {};
	enabled = true;
	fault = stage;
	allocation_failure = allocation;
}

void
dssh_test_botan_mac_fault_set(dssh_botan_mac_fault stage, bool allocation)
{
	fault = stage;
	allocation_failure = allocation;
}

dssh_botan_mac_stats
dssh_test_botan_mac_stats()
{
	return stats;
}

void
dssh_test_botan_mac_end()
{
	if (stats.live != 0)
		throw std::logic_error("Botan MAC observer still owns native objects");
	enabled = false;
	fault = dssh_botan_mac_fault::none;
}

std::unique_ptr<Botan::MessageAuthenticationCode>
dssh_test_botan_mac_create(std::string_view name)
{
	if (!enabled)
		return Botan::MessageAuthenticationCode::create_or_throw(name);
	count(stats.factories);
	fail_if(dssh_botan_mac_fault::factory);
	if (fault == dssh_botan_mac_fault::unavailable)
		return Botan::MessageAuthenticationCode::create_or_throw("DeuceSSH-unavailable-HMAC");
	return std::make_unique<observed_mac>(Botan::MessageAuthenticationCode::create_or_throw(name));
}
