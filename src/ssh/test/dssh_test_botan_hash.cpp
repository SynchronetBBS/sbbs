/* Native, typed hash adapter for helper exception and lifetime tests.
 * Inactive calls use the real Botan factory without an adapter allocation. */
#include "dssh_test_botan_hash.h"
#include <climits>
#include <new>
#include <stdexcept>
#include <utility>

static thread_local bool enabled;
static thread_local bool allocation_failure;
static thread_local dssh_botan_hash_fault fault;
static thread_local dssh_botan_hash_stats stats;

static void
count(unsigned& value)
{
	if (value < UINT_MAX)
		value++;
}

static void
fail_if(dssh_botan_hash_fault stage)
{
	if (fault != stage)
		return;
	if (allocation_failure)
		throw std::bad_alloc();
	throw std::runtime_error("injected Botan hash failure");
}

class observed_hash final : public Botan::HashFunction {
	public:
		explicit observed_hash(std::unique_ptr<Botan::HashFunction> value) : inner(std::move(value))
		{
			count(stats.live);
		}
		~observed_hash() override
		{
			if (stats.live > 0)
				stats.live--;
		}
		size_t output_length() const override
		{
			count(stats.sizes);
			fail_if(dssh_botan_hash_fault::size);
			return fault == dssh_botan_hash_fault::zero_size ? 0 : inner->output_length();
		}
		void clear() override
		{
			inner->clear();
		}
		std::string name() const override
		{
			return inner->name();
		}
		std::unique_ptr<Botan::HashFunction> new_object() const override
		{
			return inner->new_object();
		}
		std::unique_ptr<Botan::HashFunction> copy_state() const override
		{
			return inner->copy_state();
		}
	private:
		void add_data(std::span<const uint8_t> input) override
		{
			count(stats.updates);
			/* Exercise failures after the real native context has changed. */
			inner->update(input);
			fail_if(dssh_botan_hash_fault::update);
		}
		void final_result(std::span<uint8_t> output) override
		{
			count(stats.finals);
			inner->final(output);
			fail_if(dssh_botan_hash_fault::final);
		}
		std::unique_ptr<Botan::HashFunction> inner;
};

void
dssh_test_botan_hash_begin(dssh_botan_hash_fault stage, bool allocation)
{
	stats = {};
	enabled = true;
	fault = stage;
	allocation_failure = allocation;
}

void
dssh_test_botan_hash_fault_set(dssh_botan_hash_fault stage, bool allocation)
{
	fault = stage;
	allocation_failure = allocation;
}

dssh_botan_hash_stats
dssh_test_botan_hash_stats()
{
	return stats;
}

void
dssh_test_botan_hash_end()
{
	enabled = false;
	fault = dssh_botan_hash_fault::none;
}

std::unique_ptr<Botan::HashFunction>
dssh_test_botan_hash_create(std::string_view name)
{
	if (!enabled)
		return Botan::HashFunction::create_or_throw(name);
	count(stats.factories);
	fail_if(dssh_botan_hash_fault::factory);
	return std::make_unique<observed_hash>(Botan::HashFunction::create_or_throw(name));
}
