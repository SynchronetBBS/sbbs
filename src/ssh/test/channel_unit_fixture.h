/* Assertion cleanup for unshared channel unit fixtures. No allocation, thread
 * or copied production implementation is needed for the ownership registry. */
#ifndef DSSH_CHANNEL_UNIT_FIXTURE_H
#define DSSH_CHANNEL_UNIT_FIXTURE_H

enum chan_unit_kind {
	CHAN_UNIT_BUFFER,
	CHAN_UNIT_EVENTS,
	CHAN_UNIT_PARAMS,
	CHAN_UNIT_TEMP,
};

struct chan_unit_entry {
	enum chan_unit_kind kind;
	void *address;
};

struct chan_unit_report {
	size_t registered;
	size_t retired;
	size_t temporary_frees;
	bool assertion;
	bool valid;
};

struct chan_unit_fixture {
	struct chan_unit_fixture *previous;
	/* Existing cases need at most two simultaneous owners. Controls use all
	 * four kinds together; a fifth registration is a fixture bug. */
	struct chan_unit_entry entries[4];
	size_t count;
	size_t registered;
	size_t retired;
	size_t temporary_frees;
	const char *name;
	struct chan_unit_report *report;
	dssh_test_free_observer observer;
	void *observer_data;
	void *(*allocate)(size_t);
	bool finished;
};

static _Thread_local struct chan_unit_fixture *chan_unit_active;

static void
chan_unit_begin(struct chan_unit_fixture *fixture, const char *name)
{
	*fixture = (struct chan_unit_fixture){.previous = chan_unit_active,
	    .name = name, .allocate = malloc};
	chan_unit_active = fixture;
}

static void
chan_unit_register(struct chan_unit_fixture *fixture, enum chan_unit_kind kind,
    void *address)
{
	if (chan_unit_active != fixture || fixture->finished || address == NULL
	    || fixture->count == sizeof(fixture->entries) / sizeof(fixture->entries[0])
	    || fixture->registered == SIZE_MAX)
		abort();
	for (size_t i = 0; i < fixture->count; i++) {
		if (fixture->entries[i].address == address)
			abort();
	}
	fixture->entries[fixture->count++] = (struct chan_unit_entry){kind, address};
	fixture->registered++;
}

static void
chan_unit_retire(struct chan_unit_fixture *fixture, size_t index)
{
	if (index >= fixture->count || fixture->retired >= fixture->registered)
		abort();
	struct chan_unit_entry entry = fixture->entries[index];
	fixture->count--;
	fixture->entries[index] = fixture->entries[fixture->count];
	fixture->entries[fixture->count] = (struct chan_unit_entry){0};
	bool valid = false;
	switch (entry.kind) {
		case CHAN_UNIT_BUFFER: {
			struct dssh_bytebuf *buffer = entry.address;
			bytebuf_free(buffer);
			valid = buffer->data == NULL && buffer->capacity == 0
			    && buffer->head == 0 && buffer->tail == 0 && buffer->used == 0;
			break;
		}
		case CHAN_UNIT_EVENTS: {
			struct dssh_event_queue *queue = entry.address;
			event_queue_free(queue);
			valid = queue->entries == NULL && queue->capacity == 0
			    && queue->count == 0 && queue->text_capacity == 0
			    && queue->frozen.storage == NULL && !queue->has_frozen;
			break;
		}
		case CHAN_UNIT_PARAMS: {
			struct dssh_chan_params *params = entry.address;
			dssh_chan_params_free(params);
			valid = params->term == NULL && params->command == NULL
			    && params->subsystem == NULL && params->env == NULL
			    && params->env_count == 0 && params->env_capacity == 0
			    && params->modes == NULL && params->mode_count == 0
			    && params->mode_capacity == 0;
			break;
		}
		case CHAN_UNIT_TEMP: {
			uint8_t **bytes = entry.address;
			if (*bytes != NULL) {
				if (fixture->observer != NULL)
					fixture->observer(*bytes, fixture->observer_data);
				free(*bytes);
				fixture->temporary_frees++;
			}
			*bytes = NULL;
			valid = true;
			break;
		}
	}
	if (!valid)
		abort();
	fixture->retired++;
}

static void
chan_unit_release(struct chan_unit_fixture *fixture, enum chan_unit_kind kind,
    void *address)
{
	if (chan_unit_active != fixture || fixture->finished)
		abort();
	for (size_t i = 0; i < fixture->count; i++) {
		if (fixture->entries[i].kind == kind && fixture->entries[i].address == address) {
			chan_unit_retire(fixture, i);
			return;
		}
	}
	abort();
}

static int
chan_unit_finish(struct chan_unit_fixture *fixture, int result, bool assertion)
{
	if (fixture->finished || chan_unit_active != fixture)
		abort();
	/* Assertions may fail while an allocation target is armed. Clear injection
	 * first, then restore the fixture's optional pre-free observer. */
	dssh_test_alloc_reset();
	dssh_test_alloc_observe_frees(fixture->observer, fixture->observer_data);
	while (fixture->count != 0)
		chan_unit_retire(fixture, fixture->count - 1);
	dssh_test_alloc_observe_frees(NULL, NULL);
	fixture->finished = true;
	chan_unit_active = fixture->previous;
	bool valid = fixture->registered == fixture->retired;
	if (fixture->report != NULL) {
		*fixture->report = (struct chan_unit_report){
			.registered = fixture->registered, .retired = fixture->retired,
			.temporary_frees = fixture->temporary_frees,
			.assertion = assertion, .valid = valid};
	}
	int printed = printf("chan_unit_owner test=%s registered=%zu retired=%zu temporary=%zu assertion=%u valid=%u\n",
	    fixture->name, fixture->registered, fixture->retired, fixture->temporary_frees,
	    assertion ? 1U : 0U, valid ? 1U : 0U);
	return printed >= 0 && valid ? result : TEST_FAIL;
}

void
dssh_test_failure_cleanup(void)
{
	if (chan_unit_active != NULL) {
		int result = chan_unit_finish(chan_unit_active, TEST_FAIL, true);
		(void)result; /* The assertion already returns TEST_FAIL. */
	}
	else
		dssh_test_alloc_reset();
}

static int
chan_unit_bytebuf_init(struct chan_unit_fixture *fixture, struct dssh_bytebuf *buffer,
    size_t capacity)
{
	*buffer = (struct dssh_bytebuf){0};
	chan_unit_register(fixture, CHAN_UNIT_BUFFER, buffer);
	return bytebuf_init(buffer, capacity);
}

static void
chan_unit_bytebuf_free(struct chan_unit_fixture *fixture, struct dssh_bytebuf *buffer)
{
	chan_unit_release(fixture, CHAN_UNIT_BUFFER, buffer);
}

static int
chan_unit_event_init(struct chan_unit_fixture *fixture, struct dssh_event_queue *queue,
    size_t max_events, size_t packet_limit)
{
	*queue = (struct dssh_event_queue){0};
	chan_unit_register(fixture, CHAN_UNIT_EVENTS, queue);
	return event_queue_init(queue, max_events, packet_limit);
}

static void
chan_unit_event_free(struct chan_unit_fixture *fixture, struct dssh_event_queue *queue)
{
	chan_unit_release(fixture, CHAN_UNIT_EVENTS, queue);
}

static int
chan_unit_params_init(struct chan_unit_fixture *fixture, struct dssh_chan_params *params,
    enum dssh_chan_type type)
{
	if (params == NULL)
		return dssh_chan_params_init(NULL, type);
	*params = (struct dssh_chan_params){0};
	chan_unit_register(fixture, CHAN_UNIT_PARAMS, params);
	return dssh_chan_params_init(params, type);
}

static void
chan_unit_params_free(struct chan_unit_fixture *fixture, struct dssh_chan_params *params)
{
	if (params == NULL)
		dssh_chan_params_free(NULL);
	else
		chan_unit_release(fixture, CHAN_UNIT_PARAMS, params);
}

static int
chan_unit_alloc(struct chan_unit_fixture *fixture, uint8_t **bytes, size_t size)
{
	*bytes = NULL;
	chan_unit_register(fixture, CHAN_UNIT_TEMP, bytes);
	*bytes = fixture->allocate(size);
	return *bytes != NULL ? 0 : DSSH_ERROR_ALLOC;
}

static void
chan_unit_free(struct chan_unit_fixture *fixture, uint8_t **bytes)
{
	chan_unit_release(fixture, CHAN_UNIT_TEMP, bytes);
}

#endif
