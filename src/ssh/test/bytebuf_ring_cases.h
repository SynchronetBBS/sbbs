/* Real, modest allocated buffers exercise both production callers of the ring
 * helper. Each allocation is reused across all positions and transfer sizes. */
static int
test_bytebuf_ring_positions(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	const size_t capacities[] = {1, 3, 7, 16, 31, 64};
	uint8_t data[64];
	uint8_t output[66];
	uint32_t checks = 0;
	for (size_t i = 0; i < sizeof(data); i++) {
		uint8_t value = (uint8_t)i;
		data[i] = value;
	}
	for (size_t c = 0; c < sizeof(capacities) / sizeof(capacities[0]); c++) {
		struct dssh_bytebuf buffer = {0};
		size_t capacity = capacities[c];
		size_t position = 0;
		size_t count = 0;
		int ret = chan_unit_bytebuf_init(&fixture, &buffer, capacity);
		bool valid = ret == 0;
		if (!valid)
			goto retire;
		for (position = 0; position < capacity; position++) {
			for (count = 0; count <= capacity; count++) {
				/* All operands are at most 128; this independent modulo oracle
				 * is representable even when size_t is only 16 bits. */
				size_t advance = (position + capacity - buffer.tail) % capacity;
				valid = bytebuf_write(&buffer, data, advance) == advance;
				if (!valid)
					goto retire;
				valid = bytebuf_read(&buffer, output, advance, 0) == advance;
				valid = valid && buffer.head == position && buffer.tail == position
				    && bytebuf_available(&buffer) == 0;
				if (!valid)
					goto retire;
				memset(output, 0xa5, sizeof(output));
				valid = bytebuf_write(&buffer, data, count) == count;
				valid = valid && buffer.tail == (position + count) % capacity
				    && buffer.head == position && bytebuf_available(&buffer) == count
				    && bytebuf_free_space(&buffer) == capacity - count;
				if (!valid)
					goto retire;
				if (count == capacity) {
					valid = bytebuf_write(&buffer, data, 1) == 0;
					if (!valid)
						goto retire;
				}
				size_t first = count / 2;
				valid = bytebuf_read(&buffer, output + 1, first, 0) == first;
				valid = valid && buffer.head == (position + first) % capacity
				    && bytebuf_available(&buffer) == count - first;
				if (!valid)
					goto retire;
				valid = bytebuf_read(&buffer, output + 1 + first, count - first, 0)
				    == count - first;
				valid = valid && buffer.head == buffer.tail && buffer.used == 0
				    && bytebuf_free_space(&buffer) == capacity
				    && bytebuf_read(&buffer, output, 1, 0) == 0;
				valid = valid && memcmp(output + 1, data, count) == 0
				    && output[0] == 0xa5;
				for (size_t i = count + 1; i < sizeof(output); i++)
					valid = valid && output[i] == 0xa5;
				if (!valid)
					goto retire;
				/* The six capacities give fewer than 2^16 complete schedules. */
				checks++;
			}
		}
retire:
		chan_unit_bytebuf_free(&fixture, &buffer);
		if (!valid) {
			fprintf(stderr, "bytebuf ring capacity=%zu position=%zu count=%zu init=%d\n",
			    capacity, position, count, ret);
			return chan_unit_finish(&fixture, TEST_FAIL, false);
		}
	}
	int printed = printf("bytebuf_ring schedules=%" PRIu32 " capacities=6 retired=1\n", checks);
	return chan_unit_finish(&fixture, checks != 0 && printed >= 0 ? TEST_PASS : TEST_FAIL, false);
}
