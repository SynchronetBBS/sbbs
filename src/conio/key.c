/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <eventwrap.h>
#include <genwrap.h>
#include <threadwrap.h>
#include <link_list.h>

#include "ciolib.h"

#define CIOKEY_MAX_EVDEV 1024

struct ciokey_state {
	bool enabled;
	bool held[CIOKEY_MAX_EVDEV];
	bool wake_posted;
	link_list_t events;
	pthread_mutex_t mutex;
	xpevent_t wake_event;
};

static struct ciokey_state state;
static pthread_once_t ciokey_initialized = PTHREAD_ONCE_INIT;

static void
ciokey_init(void)
{
	memset(&state, 0, sizeof(state));
	listInit(&state.events, 0);
	assert_pthread_mutex_init(&state.mutex, NULL);
	state.wake_event = CreateEvent(NULL, FALSE, FALSE, NULL);
}

static void
post_wake_locked(void)
{
	if (!state.wake_posted) {
		state.wake_posted = true;
		if (state.wake_event != NULL)
			SetEvent(state.wake_event);
	}
}

static void
queue_event_locked(uint16_t evdev, bool pressed)
{
	struct ciolib_key_event *event;

	event = malloc(sizeof(*event));
	if (event != NULL) {
		event->evdev = evdev;
		event->pressed = pressed;
		if (listPushNode(&state.events, event) != NULL)
			post_wake_locked();
		else
			free(event);
	}
}

void
ciokey_gotevent(uint16_t evdev, bool pressed)
{
	pthread_once(&ciokey_initialized, ciokey_init);
	if (evdev >= CIOKEY_MAX_EVDEV)
		return;
	assert_pthread_mutex_lock(&state.mutex);
	if (state.held[evdev] == pressed) {
		assert_pthread_mutex_unlock(&state.mutex);
		return;
	}
	state.held[evdev] = pressed;
	if (!state.enabled) {
		assert_pthread_mutex_unlock(&state.mutex);
		return;
	}
	queue_event_locked(evdev, pressed);
	assert_pthread_mutex_unlock(&state.mutex);
}

void
ciokey_synthesize(uint16_t evdev, bool pressed)
{
	pthread_once(&ciokey_initialized, ciokey_init);
	if (evdev >= CIOKEY_MAX_EVDEV)
		return;
	assert_pthread_mutex_lock(&state.mutex);
	if (state.held[evdev] != pressed) {
		state.held[evdev] = pressed;
		queue_event_locked(evdev, pressed);
	}
	assert_pthread_mutex_unlock(&state.mutex);
}

bool
ciokey_getevent(struct ciolib_key_event *event)
{
	struct ciolib_key_event *queued;

	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	queued = listShiftNode(&state.events);
	if (queued == NULL) {
		assert_pthread_mutex_unlock(&state.mutex);
		return false;
	}
	if (event != NULL)
		*event = *queued;
	free(queued);
	if (listCountNodes(&state.events) == 0) {
		state.wake_posted = false;
		if (state.wake_event != NULL)
			ResetEvent(state.wake_event);
	}
	assert_pthread_mutex_unlock(&state.mutex);
	return true;
}

bool
ciokey_pending(void)
{
	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	bool ret = listCountNodes(&state.events) > 0;
	assert_pthread_mutex_unlock(&state.mutex);
	return ret;
}

size_t
ciokey_pressed(uint16_t *keys, size_t max)
{
	size_t count = 0;

	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	for (size_t i = 0; i < CIOKEY_MAX_EVDEV; i++) {
		if (!state.held[i])
			continue;
		if (keys != NULL && count < max)
			keys[count] = (uint16_t)i;
		count++;
	}
	assert_pthread_mutex_unlock(&state.mutex);
	return count;
}

void
ciokey_clear_events(void)
{
	struct ciolib_key_event *event;

	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	while ((event = listShiftNode(&state.events)) != NULL)
		free(event);
	state.wake_posted = false;
	if (state.wake_event != NULL)
		ResetEvent(state.wake_event);
	assert_pthread_mutex_unlock(&state.mutex);
}

void
ciokey_reset(void)
{
	struct ciolib_key_event *event;

	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	while ((event = listShiftNode(&state.events)) != NULL)
		free(event);
	memset(state.held, 0, sizeof(state.held));
	state.wake_posted = false;
	if (state.wake_event != NULL)
		ResetEvent(state.wake_event);
	assert_pthread_mutex_unlock(&state.mutex);
}

void
ciokey_focus_lost(void)
{
	struct ciolib_key_event *event;

	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	while ((event = listShiftNode(&state.events)) != NULL)
		free(event);
	memset(state.held, 0, sizeof(state.held));
	state.wake_posted = false;
	if (state.enabled)
		post_wake_locked();
	assert_pthread_mutex_unlock(&state.mutex);
}

bool
ciokey_setenabled(bool enabled)
{
	bool old;

	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	old = state.enabled;
	state.enabled = enabled;
	if (!enabled) {
		struct ciolib_key_event *event;
		while ((event = listShiftNode(&state.events)) != NULL)
			free(event);
		state.wake_posted = false;
		if (state.wake_event != NULL)
			ResetEvent(state.wake_event);
	}
	assert_pthread_mutex_unlock(&state.mutex);
	return old;
}

bool
ciokey_enabled(void)
{
	bool ret;

	pthread_once(&ciokey_initialized, ciokey_init);
	assert_pthread_mutex_lock(&state.mutex);
	ret = state.enabled;
	assert_pthread_mutex_unlock(&state.mutex);
	return ret;
}

int
ciokey_trywait(void)
{
	pthread_once(&ciokey_initialized, ciokey_init);
	if (state.wake_event == NULL)
		return 0;
	return WaitForEvent(state.wake_event, 0) == WAIT_OBJECT_0;
}

int
ciokey_wait(void)
{
	pthread_once(&ciokey_initialized, ciokey_init);
	if (state.wake_event == NULL)
		return 0;
	return WaitForEvent(state.wake_event, INFINITE) == WAIT_OBJECT_0;
}
