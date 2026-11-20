/* External-module fixture: public headers only. Every packet grows by nine
 * bytes and carries a directional counter, so actual packet use/order/reset
 * is observable. Contexts reset in place and transforms allocate nothing. */
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-comp.h"
#include "deucessh.h"
#include "test_comp.h"

struct growth_context {
	uint32_t sequence;
	unsigned owner;
	bool compress;
};

static atomic_uint allocations;
static atomic_uint resets;
static atomic_uint cleanups;
static atomic_uint live;
static atomic_uint compressions;
static atomic_uint decompressions;
static atomic_uint bounds;
static atomic_uint owner_errors;
static _Atomic enum dssh_test_comp_stage fault_stage;
static atomic_int fault_result;
static _Atomic(dssh_test_comp_observer) callback_observer;
static atomic_bool replace_on_reset;
static void growth_cleanup(dssh_comp_ctx *ctx, unsigned owner);

void
dssh_test_comp_replace_on_reset(bool replace)
{
	atomic_store(&replace_on_reset, replace);
}

void
dssh_test_comp_observe(dssh_test_comp_observer observer)
{
	atomic_store(&callback_observer, observer);
}

static void
observe(enum dssh_test_comp_stage stage)
{
	dssh_test_comp_observer observer = atomic_load(&callback_observer);
	if (observer != NULL)
		observer(stage);
}

int
dssh_test_comp_reset(void)
{
	if (atomic_load(&live) != 0)
		return DSSH_ERROR_INIT;
	atomic_store(&allocations, 0);
	atomic_store(&resets, 0);
	atomic_store(&cleanups, 0);
	atomic_store(&compressions, 0);
	atomic_store(&decompressions, 0);
	atomic_store(&bounds, 0);
	atomic_store(&owner_errors, 0);
	atomic_store(&fault_stage, DSSH_TEST_COMP_NONE);
	atomic_store(&replace_on_reset, false);
	return 0;
}

void
dssh_test_comp_stats(struct dssh_test_comp_stats *out)
{
	out->allocations = atomic_load(&allocations);
	out->resets = atomic_load(&resets);
	out->cleanups = atomic_load(&cleanups);
	out->live = atomic_load(&live);
	out->compressions = atomic_load(&compressions);
	out->decompressions = atomic_load(&decompressions);
	out->bounds = atomic_load(&bounds);
	out->owner_errors = atomic_load(&owner_errors);
}

void
dssh_test_comp_fail(enum dssh_test_comp_stage stage, int result)
{
	atomic_store(&fault_result, result);
	atomic_store(&fault_stage, stage);
}

static bool
fault(enum dssh_test_comp_stage stage, int *result)
{
	enum dssh_test_comp_stage expected = stage;
	if (!atomic_compare_exchange_strong(&fault_stage, &expected, DSSH_TEST_COMP_NONE))
		return false;
	*result = atomic_load(&fault_result);
	return true;
}

static int
growth_init(bool compress, dssh_comp_ctx **ctx, unsigned owner)
{
	observe(DSSH_TEST_COMP_INIT);
	struct growth_context *state = (struct growth_context *)*ctx;
	if (state == NULL) {
		state = calloc(1, sizeof(*state));
		if (state == NULL)
			return DSSH_ERROR_ALLOC;
		state->owner = owner;
		dssh_comp_ctx *opaque = (dssh_comp_ctx *)state;
		*ctx = opaque;
		atomic_fetch_add(&allocations, 1);
		atomic_fetch_add(&live, 1);
	}
	else {
		if (state->owner != owner) {
			atomic_fetch_add(&owner_errors, 1);
			return DSSH_ERROR_INVALID;
		}
		atomic_fetch_add(&resets, 1);
		if (atomic_load(&replace_on_reset)) {
			struct growth_context *replacement = calloc(1, sizeof(*replacement));
			if (replacement == NULL)
				return DSSH_ERROR_ALLOC;
			replacement->owner = owner;
			atomic_fetch_add(&allocations, 1);
			atomic_fetch_add(&live, 1);
			growth_cleanup(*ctx, owner);
			state = replacement;
			dssh_comp_ctx *opaque = (dssh_comp_ctx *)state;
			*ctx = opaque;
		}
	}
	state->sequence = 0;
	state->compress = compress;
	int result;
	return fault(DSSH_TEST_COMP_INIT, &result) ? result : 0;
}

static int
growth_init_a(bool compress, dssh_comp_ctx **ctx)
{
	return growth_init(compress, ctx, 1);
}

static int
growth_init_b(bool compress, dssh_comp_ctx **ctx)
{
	return growth_init(compress, ctx, 2);
}

static int
growth_bound(size_t input_len, size_t *output_bound, dssh_comp_ctx *ctx)
{
	observe(DSSH_TEST_COMP_BOUND);
	(void)ctx;
	atomic_fetch_add(&bounds, 1);
	int result;
	if (fault(DSSH_TEST_COMP_BOUND, &result))
		return result;
	if (input_len > SIZE_MAX - 9)
		return DSSH_ERROR_TOOLONG;
	*output_bound = input_len + 9;
	return 0;
}

static int
growth_compress(const uint8_t *input, size_t input_len, uint8_t *output,
    size_t output_capacity, size_t *output_len, dssh_comp_ctx *ctx)
{
	observe(DSSH_TEST_COMP_COMPRESS);
	struct growth_context *state = (struct growth_context *)ctx;
	atomic_fetch_add(&compressions, 1);
	if (state == NULL || !state->compress)
		return DSSH_ERROR_INVALID;
	int result;
	if (fault(DSSH_TEST_COMP_COMPRESS, &result)) {
		if (output_capacity > 0)
			output[0] = 0xA5;
		state->sequence++;
		return result;
	}
	if (input_len > SIZE_MAX - 9 || input_len + 9 > output_capacity)
		return DSSH_ERROR_TOOLONG;
#if SIZE_MAX > UINT32_MAX
	if (input_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;
#endif
	uint32_t length = (uint32_t)input_len;
	size_t pos = 0;
	result = dssh_serialize_uint32(state->sequence, output, output_capacity, &pos);
	if (result < 0)
		return result;
	result = dssh_serialize_uint32(length, output, output_capacity, &pos);
	if (result < 0)
		return result;
	output[pos++] = 0xA5;
	if (input_len > 0)
		memcpy(&output[pos], input, input_len);
	*output_len = pos + input_len;
	state->sequence++;
	return 0;
}

static int
growth_uncompress(const uint8_t *input, size_t input_len, uint8_t *output,
    size_t output_capacity, size_t *output_len, dssh_comp_ctx *ctx)
{
	observe(DSSH_TEST_COMP_UNCOMPRESS);
	struct growth_context *state = (struct growth_context *)ctx;
	atomic_fetch_add(&decompressions, 1);
	if (state == NULL || state->compress)
		return DSSH_ERROR_INVALID;
	int result;
	if (fault(DSSH_TEST_COMP_UNCOMPRESS, &result)) {
		if (output_capacity > 0)
			output[0] = 0xA5;
		state->sequence++;
		return result;
	}
	if (input_len < 9 || input[8] != 0xA5)
		return DSSH_ERROR_PARSE;
	uint32_t sequence, wire_length;
	int64_t parsed = dssh_parse_uint32(input, input_len, &sequence);
	if (parsed != 4)
		return DSSH_ERROR_PARSE;
	parsed = dssh_parse_uint32(&input[4], input_len - 4, &wire_length);
	if (parsed != 4 || sequence != state->sequence || wire_length != input_len - 9)
		return DSSH_ERROR_PARSE;
	if (wire_length > output_capacity)
		return DSSH_ERROR_TOOLONG;
	size_t length = (size_t)wire_length;
	if (length > 0)
		memcpy(output, &input[9], length);
	*output_len = length;
	state->sequence++;
	return 0;
}

static void
growth_cleanup(dssh_comp_ctx *ctx, unsigned owner)
{
	observe(DSSH_TEST_COMP_CLEANUP);
	struct growth_context *state = (struct growth_context *)ctx;
	if (state == NULL)
		return;
	if (state->owner != owner)
		atomic_fetch_add(&owner_errors, 1);
	dssh_cleanse(state, sizeof(*state));
	free(state);
	atomic_fetch_add(&cleanups, 1);
	atomic_fetch_sub(&live, 1);
}

static void
growth_cleanup_a(dssh_comp_ctx *ctx)
{
	growth_cleanup(ctx, 1);
}

static void
growth_cleanup_b(dssh_comp_ctx *ctx)
{
	growth_cleanup(ctx, 2);
}

static int
register_growth(unsigned owner)
{
	const char *name = owner == 1 ? "test-growth" : "test-growth-alt";
	size_t length = strlen(name);
	if (sizeof(struct dssh_comp_s) > SIZE_MAX - length - 1)
		return DSSH_ERROR_ALLOC;
	dssh_comp comp = malloc(sizeof(*comp) + length + 1);
	if (comp == NULL)
		return DSSH_ERROR_ALLOC;
	comp->next = NULL;
	comp->init = owner == 1 ? growth_init_a : growth_init_b;
	comp->bound = growth_bound;
	comp->compress = growth_compress;
	comp->uncompress = growth_uncompress;
	comp->cleanup = owner == 1 ? growth_cleanup_a : growth_cleanup_b;
	memcpy(comp->name, name, length + 1);
	int ret = dssh_transport_register_comp(comp);
	if (ret < 0)
		free(comp);
	return ret;
}

int
dssh_test_register_growth_comp(void)
{
	return register_growth(1);
}

int
dssh_test_register_growth_alt(void)
{
	return register_growth(2);
}

int
dssh_test_register_optional_comp(void)
{
	const char *method = getenv("DSSH_TEST_COMPRESSION");
	if (method == NULL || strcmp(method, "none") == 0)
		return 0;
	int ret = dssh_test_comp_reset();
	if (ret < 0)
		return ret;
	if (strcmp(method, "test-growth") == 0)
		return dssh_test_register_growth_comp();
	if (strcmp(method, "zlib") == 0 && dssh_test_zlib_available())
		return dssh_test_register_zlib_comp();
	return DSSH_ERROR_INVALID;
}
