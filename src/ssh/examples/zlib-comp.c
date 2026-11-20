/* Optional external compression module example, not part of libdeucessh.
 * Build this file in the application with zlib and public DeuceSSH headers.
 * Z_PARTIAL_FLUSH follows RFC 4253 section 6.2. */
#include <limits.h>
#ifdef DSSH_EXAMPLE_ZLIB_TESTING
#include <stdatomic.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "deucessh-comp.h"
#include "deucessh.h"
#include "zlib-comp.h"
#define ZLIB_CONST
#include <zlib.h>

union zlib_allocation_header {
	max_align_t alignment;
	size_t length;
};

struct zlib_context {
	z_stream stream;
	bool compress;
	bool initialized;
};

#ifdef DSSH_EXAMPLE_ZLIB_TESTING
static atomic_uint zlib_allocations;
static atomic_uint zlib_live;
static atomic_uint zlib_cleanup_errors;
static atomic_uint zlib_resets;
static atomic_int zlib_alloc_countdown = ATOMIC_VAR_INIT(-1);
#define OBSERVE_ADD(counter, value) atomic_fetch_add(&(counter), (value))
#define OBSERVE_SUB(counter, value) atomic_fetch_sub(&(counter), (value))
#else
#define OBSERVE_ADD(counter, value) ((void)0)
#define OBSERVE_SUB(counter, value) ((void)0)
#endif

static void *
zlib_calloc(size_t count, size_t size)
{
#ifdef DSSH_EXAMPLE_ZLIB_TESTING
	int remaining = atomic_load(&zlib_alloc_countdown);
	while (remaining >= 0) {
		if (remaining == 0)
			return NULL;
		if (atomic_compare_exchange_weak(&zlib_alloc_countdown, &remaining, remaining - 1))
			break;
	}
#endif
	return calloc(count, size);
}

static voidpf
zlib_allocate(voidpf opaque, uInt items, uInt size)
{
	(void)opaque;
#if UINT_MAX > SIZE_MAX
	if (items > SIZE_MAX || size > SIZE_MAX)
		return NULL;
#endif
	size_t count = (size_t)items;
	size_t item_size = (size_t)size;
	if (item_size != 0 && count > (SIZE_MAX - sizeof(union zlib_allocation_header)) / item_size)
		return NULL;
	size_t bytes = count * item_size;
	union zlib_allocation_header *header = zlib_calloc(1, sizeof(*header) + bytes);
	if (header == NULL)
		return NULL;
	header->length = bytes;
	OBSERVE_ADD(zlib_allocations, 1);
	OBSERVE_ADD(zlib_live, 1);
	return header + 1;
}

static void
zlib_release(voidpf opaque, voidpf allocation)
{
	(void)opaque;
	if (allocation == NULL)
		return;
	union zlib_allocation_header *header = (union zlib_allocation_header *)allocation;
	header--;
	size_t length = header->length;
	dssh_cleanse(header, sizeof(*header) + length);
	free(header);
	OBSERVE_SUB(zlib_live, 1);
}

static int
zlib_init(bool compress, dssh_comp_ctx **ctx)
{
	if (ctx == NULL)
		return DSSH_ERROR_INVALID;
	struct zlib_context *state = (struct zlib_context *)*ctx;
	int status;
	if (state != NULL) {
		if (!state->initialized || state->compress != compress)
			return DSSH_ERROR_INVALID;
		status = compress ? deflateReset(&state->stream) : inflateReset(&state->stream);
		if (status == Z_OK)
			OBSERVE_ADD(zlib_resets, 1);
	}
	else {
		state = zlib_calloc(1, sizeof(*state));
		if (state == NULL)
			return DSSH_ERROR_ALLOC;
		*state = (struct zlib_context){.compress = compress};
		dssh_comp_ctx *opaque = (dssh_comp_ctx *)state;
		*ctx = opaque;
		OBSERVE_ADD(zlib_allocations, 1);
		OBSERVE_ADD(zlib_live, 1);
		state->stream.zalloc = zlib_allocate;
		state->stream.zfree = zlib_release;
		/* Fix window/memory parameters to match the bound below. */
		status = compress ? deflateInit2(&state->stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
		    15, 8, Z_DEFAULT_STRATEGY) : inflateInit(&state->stream);
		state->initialized = status == Z_OK;
	}
	if (status != Z_OK)
		return status == Z_MEM_ERROR ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT;
	return 0;
}

static int
zlib_bound(size_t input_len, size_t *output_bound, dssh_comp_ctx *ctx)
{
	struct zlib_context *state = (struct zlib_context *)ctx;
	if (state == NULL || !state->compress)
		return DSSH_ERROR_INVALID;
	/* Default zlib bound plus 64 bytes for the packet flush and prior bit
	 * alignment. Unlike deflateBound's Z_FINISH guarantee, our bound must
	 * include Z_PARTIAL_FLUSH and be stable across the stream lifetime. */
	size_t overhead = 77;
	size_t term = input_len / 4096;
	if (term > SIZE_MAX - overhead)
		return DSSH_ERROR_TOOLONG;
	overhead += term;
	term = input_len / 16384;
	if (term > SIZE_MAX - overhead)
		return DSSH_ERROR_TOOLONG;
	overhead += term;
#if SIZE_MAX >= UINT32_C(33554432)
	term = input_len / UINT32_C(33554432);
	if (term > SIZE_MAX - overhead)
		return DSSH_ERROR_TOOLONG;
	overhead += term;
#endif
	if (input_len > SIZE_MAX - overhead)
		return DSSH_ERROR_TOOLONG;
	*output_bound = input_len + overhead;
	return 0;
}

static uInt
zlib_chunk(size_t remaining)
{
#if SIZE_MAX > UINT_MAX
	if (remaining > UINT_MAX)
		remaining = UINT_MAX;
#endif
	uInt count = (uInt)remaining;
	return count;
}

static int
zlib_transform(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_capacity,
    size_t *output_len, dssh_comp_ctx *ctx)
{
	struct zlib_context *state = (struct zlib_context *)ctx;
	if (state == NULL || !state->initialized)
		return DSSH_ERROR_INVALID;
	z_stream *stream = &state->stream;
	size_t supplied = 0;
	size_t produced_total = 0;
	int result = DSSH_ERROR_INIT;
	uint8_t overflow_byte;
	bool probing = false;
	for (;;) {
		if (stream->avail_in == 0 && supplied < input_len) {
			stream->next_in = &input[supplied];
			stream->avail_in = zlib_chunk(input_len - supplied);
			size_t count = (size_t)stream->avail_in;
			supplied += count;
		}
		if (stream->avail_out == 0) {
			probing = produced_total == output_capacity;
			stream->next_out = probing ? &overflow_byte : &output[produced_total];
			stream->avail_out = probing ? 1 : zlib_chunk(output_capacity - produced_total);
		}
		uInt before_in = stream->avail_in;
		uInt before_out = stream->avail_out;
		bool final_input = supplied == input_len;
		int flush = final_input ? Z_PARTIAL_FLUSH : Z_NO_FLUSH;
		int status = state->compress ? deflate(stream, flush) : inflate(stream, Z_SYNC_FLUSH);
		if (stream->avail_in > before_in || stream->avail_out > before_out) {
			result = DSSH_ERROR_INVALID;
			break;
		}
		/* The differences fit the size_t span used to initialize avail_*. */
		size_t produced = (size_t)(before_out - stream->avail_out);
		if (probing && produced != 0) {
			result = DSSH_ERROR_TOOLONG;
			break;
		}
		produced_total += produced;
		if (status != Z_OK && status != Z_BUF_ERROR) {
			result = status == Z_MEM_ERROR ? DSSH_ERROR_ALLOC : DSSH_ERROR_PARSE;
			break;
		}
		if (final_input && stream->avail_in == 0 && stream->avail_out > 0) {
			*output_len = produced_total;
			result = 0;
			break;
		}
		if (stream->avail_in == before_in && stream->avail_out == before_out) {
			result = DSSH_ERROR_PARSE;
			break;
		}
	}
	/* Do not retain the library's borrowed spans in the module context. */
	stream->next_in = Z_NULL;
	stream->next_out = Z_NULL;
	stream->avail_in = 0;
	stream->avail_out = 0;
	dssh_cleanse(&overflow_byte, sizeof(overflow_byte));
	return result;
}

static void
zlib_cleanup(dssh_comp_ctx *ctx)
{
	struct zlib_context *state = (struct zlib_context *)ctx;
	if (state == NULL)
		return;
	if (state->initialized) {
		int status = state->compress ? deflateEnd(&state->stream) : inflateEnd(&state->stream);
		/* SSH discards an unfinished stream on rekey/close. */
		if (status != Z_OK && !(state->compress && status == Z_DATA_ERROR))
			OBSERVE_ADD(zlib_cleanup_errors, 1);
	}
	dssh_cleanse(state, sizeof(*state));
	free(state);
	OBSERVE_SUB(zlib_live, 1);
}

int
dssh_example_register_zlib_comp(void)
{
	static const char name[] = "zlib";
	if (sizeof(struct dssh_comp_s) > SIZE_MAX - sizeof(name))
		return DSSH_ERROR_ALLOC;
	dssh_comp comp = malloc(sizeof(*comp) + sizeof(name));
	if (comp == NULL)
		return DSSH_ERROR_ALLOC;
	comp->next = NULL;
	comp->init = zlib_init;
	comp->bound = zlib_bound;
	comp->compress = zlib_transform;
	comp->uncompress = zlib_transform;
	comp->cleanup = zlib_cleanup;
	memcpy(comp->name, name, sizeof(name));
	int ret = dssh_transport_register_comp(comp);
	if (ret < 0)
		free(comp);
	return ret;
}

#ifdef DSSH_EXAMPLE_ZLIB_TESTING
void
dssh_example_zlib_stats(unsigned *allocated, unsigned *remaining, unsigned *errors)
{
	*allocated = atomic_load(&zlib_allocations);
	*remaining = atomic_load(&zlib_live);
	*errors = atomic_load(&zlib_cleanup_errors);
}

unsigned
dssh_example_zlib_resets(void)
{
	return atomic_load(&zlib_resets);
}

void
dssh_example_zlib_fail_after(int count)
{
	atomic_store(&zlib_alloc_countdown, count);
}
#endif
