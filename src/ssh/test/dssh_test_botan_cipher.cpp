/* Serial, thread-local boundary controls. Disabled calls return the actual
 * factory result directly; enabled objects delegate all operations to Botan. */
#include "dssh_test_botan_cipher.h"
#include <climits>
#include <new>
#include <stdexcept>
#include <utility>

static thread_local bool enabled;
static thread_local bool allocation_failure;
static thread_local dssh_botan_cipher_fault fault;
static thread_local dssh_botan_cipher_stats stats;

static void
count(unsigned& value)
{
	if (value == UINT_MAX)
		throw std::overflow_error("Botan cipher observer counter exhausted");
	value++;
}

static void
fail_if(dssh_botan_cipher_fault stage)
{
	if (fault != stage)
		return;
	if (allocation_failure)
		throw std::bad_alloc();
	throw std::runtime_error("injected Botan cipher failure");
}

class observed_cipher final : public Botan::Cipher_Mode {
	public:
		explicit observed_cipher(std::unique_ptr<Botan::Cipher_Mode> value) : inner(std::move(value))
		{
			count(stats.created);
			count(stats.live);
		}
		~observed_cipher() override
		{
			/* These counters are bounded by completed constructors. Controls
			 * cannot reset their record while any observed context is live. */
			stats.destroyed++;
			stats.live--;
		}
		Botan::Key_Length_Specification key_spec() const override
		{
			count(stats.key_specs);
			fail_if(dssh_botan_cipher_fault::key_spec);
			return inner->key_spec();
		}
		bool has_keying_material() const override
		{
			return inner->has_keying_material();
		}
		std::string name() const override
		{
			return inner->name();
		}
		void clear() override
		{
			inner->clear();
		}
		void reset() override
		{
			inner->reset();
		}
		size_t output_length(size_t length) const override
		{
			return inner->output_length(length);
		}
		size_t update_granularity() const override
		{
			return inner->update_granularity();
		}
		size_t ideal_granularity() const override
		{
			return inner->ideal_granularity();
		}
		size_t minimum_final_size() const override
		{
			return inner->minimum_final_size();
		}
		size_t default_nonce_length() const override
		{
			return inner->default_nonce_length();
		}
		bool valid_nonce_length(size_t length) const override
		{
			return inner->valid_nonce_length(length);
		}
		bool requires_entire_message() const override
		{
			return inner->requires_entire_message();
		}
		size_t tag_size() const override
		{
			return inner->tag_size();
		}
		std::string provider() const override
		{
			return inner->provider();
		}
	private:
		void key_schedule(std::span<const uint8_t> key) override
		{
			count(stats.key_schedules);
			stats.key_bytes = key.size();
			inner->set_key(key);
			fail_if(dssh_botan_cipher_fault::key_schedule);
		}
		void start_msg(const uint8_t *nonce, size_t length) override
		{
			count(stats.starts);
			stats.iv_bytes = length;
			inner->start(nonce, length);
			fail_if(dssh_botan_cipher_fault::start);
		}
		size_t process_msg(uint8_t *message, size_t length) override
		{
			count(stats.processes);
			size_t written = inner->process(message, length);
			stats.native_written = written;
			stats.reported_written = written;
			fail_if(dssh_botan_cipher_fault::process);
			if (fault == dssh_botan_cipher_fault::zero_output)
				stats.reported_written = 0;
			else if (fault == dssh_botan_cipher_fault::short_output) {
				if (written == 0)
					throw std::logic_error("Cannot shorten an empty cipher result");
				stats.reported_written = written - 1;
			}
			else if (fault == dssh_botan_cipher_fault::long_output) {
				if (written == SIZE_MAX)
					throw std::overflow_error("Cannot extend maximum cipher result");
				stats.reported_written = written + 1;
			}
			else if (fault == dssh_botan_cipher_fault::max_output)
				stats.reported_written = SIZE_MAX;
			return stats.reported_written;
		}
		void finish_msg(Botan::secure_vector<uint8_t>& buffer, size_t offset) override
		{
			inner->finish(buffer, offset);
		}
		std::unique_ptr<Botan::Cipher_Mode> inner;
};

void
dssh_test_botan_cipher_begin(dssh_botan_cipher_fault stage, bool allocation)
{
	if (stats.live != 0)
		throw std::logic_error("Botan cipher observer still owns native objects");
	stats = {};
	enabled = true;
	fault = stage;
	allocation_failure = allocation;
}

void
dssh_test_botan_cipher_fault_set(dssh_botan_cipher_fault stage, bool allocation)
{
	fault = stage;
	allocation_failure = allocation;
}

dssh_botan_cipher_stats
dssh_test_botan_cipher_stats()
{
	return stats;
}

void
dssh_test_botan_cipher_end()
{
	if (stats.live != 0)
		throw std::logic_error("Botan cipher observer still owns native objects");
	enabled = false;
	fault = dssh_botan_cipher_fault::none;
}

std::unique_ptr<Botan::Cipher_Mode>
dssh_test_botan_cipher_create(std::string_view name, Botan::Cipher_Dir direction)
{
	if (!enabled)
		return Botan::Cipher_Mode::create_or_throw(name, direction);
	count(stats.factories);
	stats.last_direction = direction;
	fail_if(dssh_botan_cipher_fault::factory);
	if (fault == dssh_botan_cipher_fault::unavailable)
		return Botan::Cipher_Mode::create_or_throw("DeuceSSH-unavailable-cipher", direction);
	return std::make_unique<observed_cipher>(Botan::Cipher_Mode::create_or_throw(name, direction));
}
